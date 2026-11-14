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
        tokens[ntokens++] = get_token(f);
        if (tokens[ntokens - 1].type == EOF_TOKEN) {
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

// Param → ID COLON Type
static AST_NODE *parse_param(void) {
    Token id = parser_expect(IDENTIFIER_TOKEN);
    parser_expect(COLON_TOKEN);
    return ast_param_create(parse_type(), id.atribute, NULL);
}

static void parse_param_rest(AST_NODE **head, AST_NODE **tail);

// AfterCommaParam → Param ParamRest | ε
// The ε branch is the trailing comma
static void parse_after_comma_param(AST_NODE **head, AST_NODE **tail) {
    Token t = parser_peek();
    if (t.type == IDENTIFIER_TOKEN) {
        chain_append(head, tail, parse_param());
        parse_param_rest(head, tail);
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
static void parse_param_rest(AST_NODE **head, AST_NODE **tail) {
    Token t = parser_peek();
    if (t.type == COMMA_TOKEN) {
        parser_advance();
        parse_after_comma_param(head, tail);
        return;
    }
    if (t.type == RPAR_TOKEN) {
        return;
    }
    error_exit(ERROR_SYN, t.line, "syntax error");
}

// ParamList → Param ParamRest | ε
static void parse_param_list(AST_NODE **head, AST_NODE **tail) {
    if (parser_peek().type == RPAR_TOKEN) {
        return; // empty group ()
    }
    chain_append(head, tail, parse_param());
    parse_param_rest(head, tail);
}

// Params → ParamGroup Params | ε, one or more () groups.
// Returns true when the head had parentheses
static bool parse_params(AST_NODE **head) {
    AST_NODE *tail = NULL;
    if (parser_peek().type != LPAR_TOKEN) {
        return false; // no parentheses at all
    }
    do {
        parser_advance(); // (
        parse_param_list(head, &tail);
        parser_expect(RPAR_TOKEN);
    } while (parser_peek().type == LPAR_TOKEN);
    // TODO: param group boundaries (list_no, more_lists)
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
static AST_NODE *parse_rhs(void) {
    Token t = parser_peek();
    switch (t.type) {
        case INT_NUM_TOKEN:
            parser_advance();
            return ast_int_literal_create(
                strtoll(t.atribute ? t.atribute : "0", NULL, 10), NULL);
        case DOUBLE_NUM_TOKEN:
            parser_advance();
            return ast_double_literal_create(
                strtod(t.atribute ? t.atribute : "0", NULL), NULL);
        case STRING_WORD_TOKEN:
            parser_advance();
            return ast_string_literal_create(t.atribute, NULL);
        case TRUE_TOKEN:
        case FALSE_TOKEN:
            parser_advance();
            return ast_bool_literal_create(t.type == TRUE_TOKEN, NULL);
        case IDENTIFIER_TOKEN:
            parser_advance();
            return ast_ident_create(NULL, t.atribute, NULL);
        case LPAR_TOKEN:
            if (pos + 1 < ntokens && tokens[pos + 1].type == RPAR_TOKEN) {
                parser_advance();
                parser_advance();
                return ast_unit_literal_create(NULL); // () unit literal
            }
            break;
        default:
            break;
    }
    error_exit(ERROR_SYN, t.line, "syntax error");
    return NULL;
}

// TEMP: block as a list of expression statements.
static AST_NODE *parse_block(void) {
    AST_NODE *head = NULL;
    AST_NODE *tail = NULL;
    int items = 0;
    for (;;) {
        Token t = parser_peek();
        if (t.type == DEDENT_TOKEN || t.type == EOF_TOKEN) {
            break;
        }
        if (t.type == EOL_TOKEN) {
            parser_advance(); // blank lines
            continue;
        }
        chain_append(&head, &tail, parse_rhs());
        parser_expect(EOL_TOKEN);
        items++;
    }
    if (items == 0) {
        error_exit(ERROR_SYN, parser_peek().line, "empty block");
    }
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
    bool has_parens = parse_params(&params);
    DATA_TYPE ret = parse_ret_type();
    parser_expect(ASSIGN_TOKEN);
    AST_NODE *body = parse_body();
    return ast_func_def_create(body, ret, params, NULL, id.atribute,
                               is_main, has_parens);
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
