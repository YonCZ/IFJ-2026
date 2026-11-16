/*
 * IFJ Implimentace prekladace
 * Soubor: parser.c
 * Autor: Tadeas Prikryl [xprikrt00]
 *
 * Prompt: "Precti si AGENTS.md, a na zaklade specifikace a zadani souboru,
 * udelej review meho kodu jestli splnuje vse co zadani pozaduje a jestli
 * moje reseni je spravne, bud mi dej psany review pokud to bude dobre
 * a nebo napis testy a ukaz mi kde na nich moje reseni selhava, prover
 * soubory parser.c a parser.h"
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner/scanner.h"
#include "scanner/token.h"
#include "scanner/error.h"

// All tokens of the input, loaded once at the start.
static Token *tokens;
static int    ntokens;

// Cursor of the parser, index into tokens.
static int pos;

// Loads all tokens from f into the array, including EOF.
static void load_tokens(FILE *f) {
    int cap = 128;
    tokens = malloc(cap * sizeof(Token));
    if (tokens == NULL) {
        error_exit(ERROR_INTERNAL, 0, "parser: allocation failed");
    }
    ntokens = 0;
    for (;;) {
        if (ntokens == cap) {
            cap *= 2;
            Token *tmp = realloc(tokens, cap * sizeof(Token));
            if (tmp == NULL) {
                error_exit(ERROR_INTERNAL, 0, "parser: allocation failed");
            }
            tokens = tmp;
        }
        Token tok = get_token(f);
        tokens[ntokens++] = tok;
        if (tok.type == EOF_TOKEN) {
            break;
        }
    }
}

// Returns the current token without consuming it.
static inline Token parser_peek(void) {
    return tokens[pos];
}

// Returns the current token and moves the cursor forward.
static inline Token parser_advance(void) {
    return tokens[pos++];
}

// Consumes the current token if it has type t, error 2 otherwise.
static inline Token parser_expect(Tokens t) {
    Token tok = parser_peek();
    if (tok.type != t) {
        error_exit(ERROR_SYN, tok.line, "syntax error");
    }
    parser_advance();
    return tok;
}

// Frees all tokens and their attributes. Idempotent.
static void parser_cleanup(void) {
    for (int i = 0; i < ntokens; i++) {
        free(tokens[i].atribute);
    }
    free(tokens);
    tokens = NULL;
    ntokens = 0;
    pos = 0;
}

#ifdef PARSER_DEBUG
// Prints all loaded tokens to stderr.
static void debug_print_tokens(void) {
    for (int i = 0; i < ntokens; i++) {
        fprintf(stderr, "%4d: type=%-3d line=%-4d attr=%s\n",
                i, tokens[i].type, tokens[i].line,
                tokens[i].atribute ? tokens[i].atribute : "-");
    }
}
#endif

// Prolog → IMPORT ID DOT MUL EOL
// "ifj26" check is strcmp on the attribute, the grammar cannot
// express a specific identifier value.
static void parse_prolog(void) {
    parser_expect(IMPORT_TOKEN);
    Token id = parser_expect(IDENTIFIER_TOKEN);
    if (id.atribute == NULL || strcmp(id.atribute, "ifj26") != 0) {
        error_exit(ERROR_SYN, id.line, "expected ifj26");
    }
    parser_expect(DOT_TOKEN);
    parser_expect(MUL_TOKEN);
    parser_expect(EOL_TOKEN);
}


// Appends node to the end of a sibling chain.
static void chain_append(AST_NODE **head, AST_NODE **tail, AST_NODE *node) {
    if (*tail == NULL) {
        *head = node;
    } else {
        (*tail)->next = node;
    }
    *tail = node;
}

// Type → one of the five type tokens, error 2 otherwise.
static DATA_TYPE parse_type(void) {
    Token t = parser_advance();
    switch (t.type) {
        case INT_TOKEN:     return DATA_INT;
        case DOUBLE_TOKEN:  return DATA_DOUBLE;
        case STRING_TOKEN:  return DATA_STRING;
        case UNIT_TOKEN:    return DATA_UNIT;
        case BOOLEAN_TOKEN: return DATA_BOOL;
        default:
            error_exit(ERROR_SYN, t.line, "syntax error");
            return DATA_NONE;
    }
}

// Param → ID COLON Type, group is the index of its parameter list
static AST_NODE *parse_param(int group) {
    Token id = parser_expect(IDENTIFIER_TOKEN);
    parser_expect(COLON_TOKEN);
    AST_NODE *node = ast_param_create(parse_type(), id.atribute, NULL);
    node->list_no = group;
    node->line = id.line;
    return node;
}

static void parse_param_rest(AST_NODE **head, AST_NODE **tail, int group);

// AfterCommaParam → Param ParamRest | ε
// The ε branch is the trailing comma
static void parse_after_comma_param(AST_NODE **head, AST_NODE **tail, int group) {
    Token t = parser_peek();
    if (t.type == IDENTIFIER_TOKEN) {
        chain_append(head, tail, parse_param(group));
        parse_param_rest(head, tail, group);
        return;
    }
    if (t.type == RPAR_TOKEN) {
        if (!t.check_new_line) {
            error_exit(ERROR_SYN, t.line, "trailing comma without end of line");
        }
        return;
    }
    error_exit(ERROR_SYN, t.line, "syntax error");
}

// ParamRest → COMMA AfterCommaParam | ε
static void parse_param_rest(AST_NODE **head, AST_NODE **tail, int group) {
    Token t = parser_peek();
    if (t.type == COMMA_TOKEN) {
        parser_advance();
        parse_after_comma_param(head, tail, group);
        return;
    }
    if (t.type == RPAR_TOKEN) {
        return;
    }
    error_exit(ERROR_SYN, t.line, "syntax error");
}

// ParamList → Param ParamRest | ε
static void parse_param_list(AST_NODE **head, AST_NODE **tail, int group) {
    if (parser_peek().type == RPAR_TOKEN) {
        return; // empty group ()
    }
    chain_append(head, tail, parse_param(group));
    parse_param_rest(head, tail, group);
}

// Params → ParamGroup Params | ε, one or more () groups.
// Returns true when the head had parentheses
// Params → ParamGroup Params | ε, one or more () groups.
// Returns true when the head had parentheses, groups is their count.
static bool parse_params(AST_NODE **head, int *groups) {
    AST_NODE *tail = NULL;
    *groups = 0;
    if (parser_peek().type != LPAR_TOKEN) {
        return false; // no parentheses at all
    }
    int group = 0;
    do {
        parser_advance(); // (
        parse_param_list(head, &tail, group++);
        parser_expect(RPAR_TOKEN);
    } while (parser_peek().type == LPAR_TOKEN);
    *groups = group;
    return true;
}

// RetType → COLON Type | ε
static DATA_TYPE parse_ret_type(void) {
    if (parser_peek().type == COLON_TOKEN) {
        parser_advance();
        return parse_type();
    }
    return DATA_NONE;
}

// OptEol → EOL | ε
static void parse_opt_eol(void) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
    }
}

// TEMP expression substitute: literals, identifiers and the () unit.
// Full expressions are the precedence analysis.
// FIRST(Expr), tokens that can start an expression.
static bool is_expr_start(Tokens t) {
    switch (t) {
        case IDENTIFIER_TOKEN: case INT_NUM_TOKEN: case DOUBLE_NUM_TOKEN:
        case STRING_WORD_TOKEN: case LPAR_TOKEN: case PLUS_TOKEN:
        case MINUS_TOKEN: case NOT_TOKEN: case TRUE_TOKEN: case FALSE_TOKEN:
        case IF_TOKEN: case INTERP_START_TOKEN:
            return true;
        default:
            return false;
    }
}

static AST_NODE *parse_interp(void);

// to, until and by are identifiers checked by strcmp in the for header.
static bool is_range_word(Token t) {
    return t.type == IDENTIFIER_TOKEN && t.atribute != NULL &&
           (strcmp(t.atribute, "to") == 0 ||
            strcmp(t.atribute, "until") == 0 ||
            strcmp(t.atribute, "by") == 0);
}

// Skips tokens of an expression up to a stopping token. Stopping tokens
// stay for the caller to consume. Nested parentheses belong inside the
// expression, only a closing parenthesis at depth zero stops the skip
// (same for a comma, the argument separator). In a for header the range
// words stop it after at least one token. An if expression swallows its
// then and else, they stop only outside of one.
static void skip_expression(bool range) {
    int depth = 0;
    int start = pos;
    int ifs = 0;
    bool prev_dot = false;
    for (;;) {
        Token t = parser_peek();
        switch (t.type) {
            case LPAR_TOKEN:
                depth++;
                parser_advance();
                break;
            case RPAR_TOKEN:
                if (depth == 0) {
                    return;
                }
                depth--;
                parser_advance();
                break;
            case COMMA_TOKEN:
                if (depth == 0) {
                    return;
                }
                parser_advance();
                break;
            case IDENTIFIER_TOKEN:
                // a member name after a dot is not a range word
                if (range && pos > start && depth == 0 && !prev_dot &&
                    is_range_word(t)) {
                    return;
                }
                parser_advance();
                break;
            case IF_TOKEN:
                ifs++;
                parser_advance();
                break;
            case ELSE_TOKEN:
                if (ifs == 0) {
                    return;
                }
                ifs--;
                parser_advance();
                break;
            case THEN_TOKEN:
                if (ifs > 0) {
                    parser_advance();
                    break;
                }
                return;
            case INTERP_START_TOKEN: // interpolated string, parsed whole
                parse_interp();
                break;
            case VAL_TOKEN: case VAR_TOKEN: case WHILE_TOKEN:
            case FOR_TOKEN: case RETURN_TOKEN: case DEF_TOKEN:
                // a statement keyword cannot continue an expression
                if (depth == 0) {
                    return;
                }
                parser_advance();
                break;
            case EOL_TOKEN:
                // an end of line only ends the expression outside
                // parentheses, an unclosed parenthesis keeps going
                if (depth == 0) {
                    return;
                }
                parser_advance();
                break;
            case SEMICOLON_TOKEN:
            case DO_TOKEN: case DEDENT_TOKEN: case EOF_TOKEN:
            case RBRACE_TOKEN:
                return;
            default:
                parser_advance();
        }
        prev_dot = (t.type == DOT_TOKEN);
    }
}

static AST_NODE *parse_interp(void);

// Skips an expression, returns NULL. An interpolated string is parsed
// and its node returned, the rest of the expression is skipped.
static AST_NODE *parse_rhs(void) {
    Token t = parser_peek();
    if (t.type == INTERP_START_TOKEN) {
        AST_NODE *node = parse_interp();
        skip_expression(false);
        return node;
    }
    if (!is_expr_start(t.type)) {
        error_exit(ERROR_SYN, t.line, "syntax error");
    }
    skip_expression(false);
    return NULL;
}

// Skips a for range bound, the range words stop it, returns NULL.
static AST_NODE *parse_rhs_range(void) {
    Token t = parser_peek();
    if (!is_expr_start(t.type)) {
        error_exit(ERROR_SYN, t.line, "syntax error");
    }
    skip_expression(true);
    return NULL;
}

static AST_NODE *parse_rhs(void);

// InterpString → INTERP_START InterpContent
// InterpContent → LBRACE Expr RBRACE InterpContent | ID InterpContent
//               | INTERP_MID InterpContent | INTERP_END
// Segments (text parts, identifiers, expressions) chain in left.
static AST_NODE *parse_interp(void) {
    Token start = parser_expect(INTERP_START_TOKEN);
    AST_NODE *head = NULL;
    AST_NODE *tail = NULL;
    chain_append(&head, &tail, ast_string_literal_create(start.atribute, NULL));
    for (;;) {
        Token t = parser_peek();
        switch (t.type) {
            case LBRACE_TOKEN: // ${expression}
                parser_advance();
                AST_NODE *value = parse_rhs();
                if (value != NULL) {
                    chain_append(&head, &tail, value);
                }
                parser_expect(RBRACE_TOKEN);
                continue;
            case IDENTIFIER_TOKEN: // $identifier
                parser_advance();
                chain_append(&head, &tail,
                             ast_ident_create(NULL, t.atribute, NULL));
                continue;
            case INTERP_MID_TOKEN: // text between two interpolations
                parser_advance();
                chain_append(&head, &tail,
                             ast_string_literal_create(t.atribute, NULL));
                continue;
            case INTERP_END_TOKEN: // last text part
                parser_advance();
                chain_append(&head, &tail,
                             ast_string_literal_create(t.atribute, NULL));
                AST_NODE *node = ast_create(head, NULL, NULL, NODE_INTERP);
                node->line = start.line;
                return node;
            default:
                error_exit(ERROR_SYN, t.line, "syntax error");
        }
    }
}

static void parse_mid_item(AST_NODE **head, AST_NODE **tail);
static void parse_if_stmt(AST_NODE **head, AST_NODE **tail);
static void parse_while_stmt(AST_NODE **head, AST_NODE **tail);
static void parse_for_stmt(AST_NODE **head, AST_NODE **tail);
static AST_NODE *parse_if_else(AST_NODE **head, AST_NODE **tail);
static AST_NODE *parse_block(void);
static AST_NODE *parse_if_branch(AST_NODE **head, AST_NODE **tail);
static void parse_if_then(AST_NODE **head, AST_NODE **tail, AST_NODE *node);

// FIRST(BlockItem), tokens that can start a statement.
static bool is_stmt_start(Tokens t) {
    switch (t) {
        case VAL_TOKEN: case VAR_TOKEN: case RETURN_TOKEN: case IF_TOKEN:
        case WHILE_TOKEN: case FOR_TOKEN: case IDENTIFIER_TOKEN:
        case INT_NUM_TOKEN:
        case DOUBLE_NUM_TOKEN: case STRING_WORD_TOKEN: case LPAR_TOKEN:
        case TRUE_TOKEN: case FALSE_TOKEN: case INTERP_START_TOKEN:
            return true;
        default:
            return false;
    }
}

// StmtIdTail → ASSIGN Rhs | expression from the saved identifier
static AST_NODE *parse_stmt_id_tail(Token id) {
    if (parser_peek().type == ASSIGN_TOKEN) {
        parser_advance();
        AST_NODE *target = ast_ident_create(NULL, id.atribute, NULL);
        AST_NODE *node = ast_assign_create(target, parse_rhs(), NULL);
        node->line = id.line;
        return node;
    }
    // expression statement from the saved identifier
    skip_expression(false);
    return NULL;
}

// ValVarTail → COLON Type ASSIGN Rhs | ASSIGN Rhs
static AST_NODE *parse_val_var_tail(char *name, bool is_const, int line) {
    DATA_TYPE declared = DATA_NONE;
    if (parser_peek().type == COLON_TOKEN) {
        parser_advance();
        declared = parse_type();
    }
    parser_expect(ASSIGN_TOKEN);
    AST_NODE *node = ast_var_def_create(parse_rhs(), NULL, name, is_const, declared);
    node->line = line;
    return node;
}

// ValStmt → VAL ID ValVarTail
static AST_NODE *parse_val_stmt(void) {
    Token kw = parser_advance();
    Token id = parser_expect(IDENTIFIER_TOKEN);
    return parse_val_var_tail(id.atribute, true, kw.line);
}

// VarStmt → VAR ID ValVarTail
static AST_NODE *parse_var_stmt(void) {
    Token kw = parser_advance();
    Token id = parser_expect(IDENTIFIER_TOKEN);
    return parse_val_var_tail(id.atribute, false, kw.line);
}

// ReturnStmt → RETURN RhsOpt
static AST_NODE *parse_return_stmt(void) {
    Token kw = parser_advance();
    Token t = parser_peek();
    AST_NODE *node;
    if (t.type == EOL_TOKEN || t.type == SEMICOLON_TOKEN) {
        node = ast_return_create(NULL, NULL); // bare return
    } else {
        node = ast_return_create(parse_rhs(), NULL);
    }
    node->line = kw.line;
    return node;
}

// SimpleStmt → ValStmt | VarStmt | ReturnStmt | ID StmtIdTail | ExprStmt
static AST_NODE *parse_simple_stmt(void) {
    switch (parser_peek().type) {
        case VAL_TOKEN:
            return parse_val_stmt();
        case VAR_TOKEN:
            return parse_var_stmt();
        case RETURN_TOKEN:
            return parse_return_stmt();
        case IDENTIFIER_TOKEN: {
            Token id = parser_advance();
            return parse_stmt_id_tail(id);
        }
        default:
            return parse_rhs();
    }
}

// LineEnd → EOL | SEMICOLON MidItem
static void parse_line_end(AST_NODE **head, AST_NODE **tail) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
        return;
    }
    parser_expect(SEMICOLON_TOKEN);
    parse_mid_item(head, tail);
}

// MidItem → MidSimple LineEnd | IfStmt | WhileStmt | ForStmt | EOL
static void parse_mid_item(AST_NODE **head, AST_NODE **tail) {
    Token t = parser_peek();
    if (t.type == EOL_TOKEN) {
        parser_advance(); // trailing semicolon
        return;
    }
    if (t.type == IF_TOKEN) {
        parse_if_stmt(head, tail);
        return;
    }
    if (t.type == WHILE_TOKEN) {
        parse_while_stmt(head, tail);
        return;
    }
    if (t.type == FOR_TOKEN) {
        parse_for_stmt(head, tail);
        return;
    }
    AST_NODE *item = parse_simple_stmt();
    if (item != NULL) {
        chain_append(head, tail, item);
    }
    parse_line_end(head, tail);
}

// IfElse → EOL INDENT Block DEDENT | IfStmt | ExprNoIf LineEnd
static AST_NODE *parse_if_else(AST_NODE **head, AST_NODE **tail) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
        parser_expect(INDENT_TOKEN);
        AST_NODE *block = parse_block();
        parser_expect(DEDENT_TOKEN);
        return block;
    }
    if (parser_peek().type == IF_TOKEN) {
        return parse_if_branch(head, tail); // else if chain
    }
    AST_NODE *value = parse_rhs();
    parse_line_end(head, tail);
    return value;
}

// IfStmt in the else branch, the node is returned, not chained.
static AST_NODE *parse_if_branch(AST_NODE **head, AST_NODE **tail) {
    parser_advance();
    AST_NODE *cond = parse_rhs();
    parser_expect(THEN_TOKEN);
    AST_NODE *node = ast_if_create(cond, NULL, NULL, NULL);
    parse_if_then(head, tail, node);
    return node;
}

// IfThen → EOL INDENT Block DEDENT IfTail | Rhs IfElseOpt
static void parse_if_then(AST_NODE **head, AST_NODE **tail, AST_NODE *node) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
        parser_expect(INDENT_TOKEN);
        node->right = parse_block();
        parser_expect(DEDENT_TOKEN);
        if (parser_peek().type == ELSE_TOKEN) {
            parser_advance();
            node->third = parse_if_else(head, tail);
        }
        return;
    }
    node->right = parse_rhs();
    if (parser_peek().type == ELSE_TOKEN) {
        parser_advance();
        node->third = parse_if_else(head, tail);
    } else {
        parse_line_end(head, tail);
    }
}

// IfStmt → IF Expr THEN IfThen
// The node is chained before its semicolon siblings are parsed.
static void parse_if_stmt(AST_NODE **head, AST_NODE **tail) {
    Token kw = parser_advance();
    AST_NODE *cond = parse_rhs();
    parser_expect(THEN_TOKEN);
    AST_NODE *node = ast_if_create(cond, NULL, NULL, NULL);
    node->line = kw.line;
    chain_append(head, tail, node);
    parse_if_then(head, tail, node);
}

// WhileStmt → WHILE Expr DO EOL INDENT Block DEDENT
static void parse_while_stmt(AST_NODE **head, AST_NODE **tail) {
    Token kw = parser_advance();
    AST_NODE *cond = parse_rhs();
    parser_expect(DO_TOKEN);
    parser_expect(EOL_TOKEN);
    parser_expect(INDENT_TOKEN);
    AST_NODE *body = parse_block();
    parser_expect(DEDENT_TOKEN);
    AST_NODE *node = ast_while_create(cond, body, NULL);
    node->line = kw.line;
    chain_append(head, tail, node);
}

// Range → Expr RangeTail, to is inclusive, until is exclusive.
static AST_NODE *parse_range(void) {
    int line = parser_peek().line;
    AST_NODE *from = parse_rhs_range();
    Token t = parser_peek();
    if (!is_range_word(t) || strcmp(t.atribute, "by") == 0) {
        error_exit(ERROR_SYN, t.line, "syntax error"); // expected to or until
    }
    bool inclusive = strcmp(t.atribute, "to") == 0;
    parser_advance();
    AST_NODE *to = parse_rhs_range();
    AST_NODE *step = NULL;
    if (inclusive) { // only the to range has a step
        Token b = parser_peek();
        if (b.type == IDENTIFIER_TOKEN && b.atribute != NULL &&
            strcmp(b.atribute, "by") == 0) {
            parser_advance();
            step = parse_rhs_range();
        }
    }
    AST_NODE *node = ast_range_create(from, to, step, NULL, inclusive);
    node->line = line;
    return node;
}

// ForGuardRest → EOL ForGuardRest | IF Expr EOL ForGuardRest | ε
static AST_NODE *parse_guard_rest(void) {
    AST_NODE *head = NULL;
    AST_NODE *tail = NULL;
    for (;;) {
        Token t = parser_peek();
        if (t.type == EOL_TOKEN) {
            parser_advance();
            continue;
        }
        if (t.type == IF_TOKEN) {
            parser_advance();
            AST_NODE *guard = parse_rhs();
            if (guard != NULL) {
                chain_append(&head, &tail, guard);
            }
            parser_expect(EOL_TOKEN);
            continue;
        }
        break; // DEDENT
    }
    return head;
}

// DoBody → EOL INDENT Block DEDENT | Rhs LineEnd
static void parse_do_body(AST_NODE **head, AST_NODE **tail, AST_NODE *node) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
        parser_expect(INDENT_TOKEN);
        node->right = parse_block();
        parser_expect(DEDENT_TOKEN);
        return;
    }
    node->right = parse_rhs();
    parse_line_end(head, tail);
}

// ForStmt → FOR ForHead
// The node is chained before its semicolon siblings are parsed.
static void parse_for_stmt(AST_NODE **head, AST_NODE **tail) {
    Token kw = parser_advance();
    AST_NODE *node = ast_for_create(NULL, NULL, NULL, NULL, NULL);
    node->line = kw.line;
    chain_append(head, tail, node);
    Token id;
    if (parser_peek().type == EOL_TOKEN) {
        // ForHead → EOL INDENT ForGen ForGuardRest DEDENT DO DoBody
        parser_advance();
        parser_expect(INDENT_TOKEN);
        id = parser_expect(IDENTIFIER_TOKEN); // ForGen: ID <- Range EOL
        parser_expect(ARROW_LEFT_TOKEN);
        node->left = parse_range();
        parser_expect(EOL_TOKEN);
        node->third = parse_guard_rest();
        parser_expect(DEDENT_TOKEN);
        parser_expect(DO_TOKEN);
        ast_name(node, id.atribute);
        parse_do_body(head, tail, node);
        return;
    }
    // ForHead → ForGenInline DO DoBody
    id = parser_expect(IDENTIFIER_TOKEN);
    parser_expect(ARROW_LEFT_TOKEN);
    node->left = parse_range();
    parser_expect(DO_TOKEN);
    ast_name(node, id.atribute);
    parse_do_body(head, tail, node);
}

// BlockItem → SimpleStmt LineEnd | IfStmt | WhileStmt | ForStmt
static void parse_block_item(AST_NODE **head, AST_NODE **tail) {
    Token t = parser_peek();
    switch (t.type) {
        case IF_TOKEN:
            parse_if_stmt(head, tail);
            return;
        case WHILE_TOKEN:
            parse_while_stmt(head, tail);
            return;
        case FOR_TOKEN:
            parse_for_stmt(head, tail);
            return;
        default: {
            AST_NODE *item = parse_simple_stmt();
            if (item != NULL) {
                chain_append(head, tail, item);
            }
            parse_line_end(head, tail);
        }
    }
}

// StmtRest → EOL StmtRest | BlockItem StmtRest | ε
static void parse_stmt_rest(AST_NODE **head, AST_NODE **tail) {
    for (;;) {
        Token t = parser_peek();
        if (t.type == EOL_TOKEN) {
            parser_advance(); // blank lines
            continue;
        }
        if (is_stmt_start(t.type)) {
            parse_block_item(head, tail);
            continue;
        }
        return; // DEDENT or EOF
    }
}

// Block → BlockItem StmtRest
static AST_NODE *parse_block(void) {
    AST_NODE *head = NULL;
    AST_NODE *tail = NULL;
    parse_block_item(&head, &tail);
    parse_stmt_rest(&head, &tail);
    return head;
}

// Body → Rhs EOL | EOL INDENT Block DEDENT
static AST_NODE *parse_body(void) {
    if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
        parser_expect(INDENT_TOKEN);
        AST_NODE *block = parse_block();
        parser_expect(DEDENT_TOKEN);
        return block;
    }
    AST_NODE *rhs = parse_rhs();
    parser_expect(EOL_TOKEN);
    return rhs;
}

// FuncDef → DEF ID Params RetType ASSIGN Body
//         | AT ID OptEol DEF ID Params RetType ASSIGN Body
// The "main" check is strcmp on the attribute.
static AST_NODE *parse_func_def(void) {
    int line = parser_peek().line;
    bool is_main = false;
    if (parser_peek().type == AT_TOKEN) {
        parser_advance();
        Token at = parser_expect(IDENTIFIER_TOKEN);
        if (at.atribute == NULL || strcmp(at.atribute, "main") != 0) {
            error_exit(ERROR_SYN, at.line, "expected main");
        }
        is_main = true;
        parse_opt_eol();
    }
    parser_expect(DEF_TOKEN);
    Token id = parser_expect(IDENTIFIER_TOKEN);
    AST_NODE *params = NULL;
    int groups = 0;
    bool has_parens = parse_params(&params, &groups);
    DATA_TYPE ret = parse_ret_type();
    parser_expect(ASSIGN_TOKEN);
    AST_NODE *body = parse_body();
    AST_NODE *node = ast_func_def_create(body, params, NULL, ret, id.atribute,
                                         is_main, has_parens);
    node->line = line;
    node->more_lists = groups;
    return node;
}

// ExtensionDef → EXTENSION LPAR Param RPAR EOL INDENT ExtDefs DEDENT
// The receiver is stored as the first param of every method.
static void parse_extension_def(AST_NODE **head, AST_NODE **tail) {
    parser_advance();
    parser_expect(LPAR_TOKEN);
    AST_NODE *receiver = parse_param(0);
    parser_expect(RPAR_TOKEN);
    parser_expect(EOL_TOKEN);
    parser_expect(INDENT_TOKEN);
    // ExtDefs → EOL ExtDefs | FuncDef ExtDefs | ε
    for (;;) {
        Token t = parser_peek();
        if (t.type == EOL_TOKEN) {
            parser_advance();
            continue;
        }
        if (t.type == DEF_TOKEN || t.type == AT_TOKEN) {
            AST_NODE *method = parse_func_def();
            method->is_extension = true;
            AST_NODE *recv = ast_param_create(receiver->data_type,
                                              receiver->name, method->third);
            recv->is_method = true;
            recv->line = receiver->line;
            method->third = recv;
            chain_append(head, tail, method);
            continue;
        }
        break; // DEDENT
    }
    parser_expect(DEDENT_TOKEN);
    free_AST(receiver);
}

// FuncDefs → EOL FuncDefs | FuncDef FuncDefs | ε
static AST_NODE *parse_func_defs(void) {
    AST_NODE *head = NULL;
    AST_NODE *tail = NULL;
    for (;;) {
        Token t = parser_peek();
        if (t.type == EOL_TOKEN) {
            parser_advance(); // blank lines between definitions
            continue;
        }
        if (t.type == DEF_TOKEN || t.type == AT_TOKEN) {
            chain_append(&head, &tail, parse_func_def());
            continue;
        }
        if (t.type == EXTENSION_TOKEN) {
            parse_extension_def(&head, &tail);
            continue;
        }
        if (t.type == EOF_TOKEN) {
            parser_advance();
            break;
        }
        error_exit(ERROR_SYN, t.line, "syntax error");
    }
    return head;
}

// Parses the input from stdin, returns the AST root.
AST_NODE *parse_program(void) {
    error_cleanup(parser_cleanup);
    load_tokens(stdin);

#ifdef PARSER_DEBUG
    debug_print_tokens();
#endif

    parse_prolog();
    AST_NODE *funcs = parse_func_defs();

    AST_NODE *root = ast_create(funcs, NULL, NULL, NODE_PROGRAM);
    parser_cleanup();
    return root;
}
