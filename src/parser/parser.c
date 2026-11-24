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
#include "scanner/error.h"
#include "scanner/scanner.h"
#include "scanner/token.h"
#include "symtable/precedence.h"

// All tokens of the input, loaded once at the start.
static Token *tokens;
static int ntokens;

// Cursor of the parser, index into tokens.
static int pos;

// Unique id of every declaration in the program.
static int next_id;

// True while parsing branch expressions of an if that sits in call arguments.
static bool expr_in_args;

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
Token parser_peek(void) { return tokens[pos]; }

// Returns the token after the current one, EOF at the end.
Token parser_peek_next(void) {
  if (pos + 1 >= ntokens) {
    return tokens[ntokens - 1];
  }
  return tokens[pos + 1];
}

Token parser_advance(void) { return tokens[pos++]; }

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
    fprintf(stderr, "%4d: type=%-3d line=%-4d attr=%s\n", i, tokens[i].type,
            tokens[i].line, tokens[i].atribute ? tokens[i].atribute : "-");
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
  if (parser_peek().type == DEDENT_TOKEN) {
    parser_advance(); // closes the indentation of the prolog line
  }
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

static AST_NODE *parse_type_ref(void);

// TypeAtom → basic token, List[T] or ID (alias / case class).
static AST_NODE *parse_type_atom_ref(void) {
  Token t = parser_advance();
  switch (t.type) {
  case INT_TOKEN:
    return ast_type_ref_create(DATA_INT, NULL, NULL);
  case DOUBLE_TOKEN:
    return ast_type_ref_create(DATA_DOUBLE, NULL, NULL);
  case STRING_TOKEN:
    return ast_type_ref_create(DATA_STRING, NULL, NULL);
  case UNIT_TOKEN:
    return ast_type_ref_create(DATA_UNIT, NULL, NULL);
  case BOOLEAN_TOKEN:
    return ast_type_ref_create(DATA_BOOL, NULL, NULL);
  case LIST_TOKEN: {
    parser_expect(LBRACKET_TOKEN);
    AST_NODE *elem = parse_type_ref();
    parser_expect(RBRACKET_TOKEN);
    return ast_list_type_ref_create(elem, NULL);
  }
  case IDENTIFIER_TOKEN:
    return ast_type_ref_create(DATA_NONE, t.atribute, NULL);
  default:
    error_exit(ERROR_SYN, t.line, "syntax error");
    return NULL;
  }
}

// Type → TypeAtom (| TypeAtom)*
static AST_NODE *parse_type_ref(void) {
  AST_NODE *type = parse_type_atom_ref();
  while (parser_peek().type == PIPE_TOKEN) {
    parser_advance();
    AST_NODE *right = parse_type_atom_ref();
    type = ast_union_type_ref_create(type, right, NULL);
  }
  return type;
}

// Basic type of a simple type reference, DATA_NONE for List and unions.
static DATA_TYPE type_ref_basic(AST_NODE *type) {
  if (type == NULL) {
    return DATA_NONE;
  }
  return type->data_type;
}

// Param → ID COLON Type
static AST_NODE *parse_param(void) {
  Token id = parser_expect(IDENTIFIER_TOKEN);
  parser_expect(COLON_TOKEN);
  AST_NODE *type = parse_type_ref();
  AST_NODE *node = ast_param_create(type_ref_basic(type), id.atribute, NULL);
  node->type_ref = type;
  node->id = next_id++;
  node->line = id.line;
  return node;
}

static int parse_param_rest(AST_NODE **head, AST_NODE **tail);

// AfterCommaParam → Param ParamRest | ε
// The ε branch is the trailing comma
static int parse_after_comma_param(AST_NODE **head, AST_NODE **tail) {
  Token t = parser_peek();
  if (t.type == IDENTIFIER_TOKEN) {
    chain_append(head, tail, parse_param());
    return 1 + parse_param_rest(head, tail);
  }
  if (t.type == RPAR_TOKEN) {
    if (!t.check_new_line) {
      error_exit(ERROR_SYN, t.line, "trailing comma without end of line");
    }
    return 0;
  }
  error_exit(ERROR_SYN, t.line, "syntax error");
  return 0;
}

// ParamRest → COMMA AfterCommaParam | ε
static int parse_param_rest(AST_NODE **head, AST_NODE **tail) {
  Token t = parser_peek();
  if (t.type == COMMA_TOKEN) {
    parser_advance();
    return parse_after_comma_param(head, tail);
  }
  if (t.type == RPAR_TOKEN) {
    return 0;
  }
  error_exit(ERROR_SYN, t.line, "syntax error");
  return 0;
}

// ParamList → Param ParamRest | ε, returns the number of parameters.
static int parse_param_list(AST_NODE **head, AST_NODE **tail) {
  if (parser_peek().type == RPAR_TOKEN) {
    return 0; // empty group ()
  }
  chain_append(head, tail, parse_param());
  return 1 + parse_param_rest(head, tail);
}

// Params → ParamGroup Params | ε, one or more () groups.
// Returns true when the head had parentheses.
static bool parse_params(AST_NODE **head) {
  *head = NULL;
  if (parser_peek().type != LPAR_TOKEN) {
    return false; // no parentheses at all
  }
  AST_NODE *groups_head = NULL;
  AST_NODE *groups_tail = NULL;
  int count = 0;
  do {
    parser_advance(); // (
    AST_NODE *params = NULL;
    AST_NODE *tail = NULL;
    int cnt = parse_param_list(&params, &tail);
    AST_NODE *group = ast_create_param_list(params, NULL, cnt);
    chain_append(&groups_head, &groups_tail, group);
    count++;
    parser_expect(RPAR_TOKEN);
  } while (parser_peek().type == LPAR_TOKEN);
  *head = ast_create_params_list_of_list(groups_head, count);
  return true;
}

// TypeDef → TYPE ID ASSIGN Type EOL
static AST_NODE *parse_type_def(void) {
  parser_advance();
  Token id = parser_expect(IDENTIFIER_TOKEN);
  parser_expect(ASSIGN_TOKEN);
  AST_NODE *type = parse_type_ref();
  parser_expect(EOL_TOKEN);
  AST_NODE *node = ast_type_def_create(id.atribute, type, NULL);
  node->line = id.line;
  return node;
}

// CaseClassDef → CASE CLASS ID LPAREN ParamList RPAREN EOL
static AST_NODE *parse_case_class(void) {
  Token kw = parser_advance();
  parser_expect(CLASS_TOKEN);
  Token id = parser_expect(IDENTIFIER_TOKEN);
  parser_expect(LPAR_TOKEN);
  AST_NODE *head = NULL;
  AST_NODE *tail = NULL;
  int count = parse_param_list(&head, &tail);
  AST_NODE *params = ast_create_param_list(head, NULL, count);
  parser_expect(RPAR_TOKEN);
  parser_expect(EOL_TOKEN);
  AST_NODE *node = ast_case_class_create(id.atribute, params, NULL);
  node->line = kw.line;
  return node;
}

// RetType → COLON Type | ε
static AST_NODE *parse_ret_type(void) {
  if (parser_peek().type == COLON_TOKEN) {
    parser_advance();
    return parse_type_ref();
  }
  return NULL;
}

// OptEol → EOL | ε
static void parse_opt_eol(void) {
  if (parser_peek().type == EOL_TOKEN) {
    parser_advance();
  }
}

// FIRST(Expr), tokens that can start an expression.
static bool is_expr_start(Tokens t) {
  switch (t) {
  case IDENTIFIER_TOKEN:
  case INT_NUM_TOKEN:
  case DOUBLE_NUM_TOKEN:
  case STRING_WORD_TOKEN:
  case LPAR_TOKEN:
  case PLUS_TOKEN:
  case MINUS_TOKEN:
  case NOT_TOKEN:
  case TRUE_TOKEN:
  case FALSE_TOKEN:
  case IF_TOKEN:
  case INTERP_START_TOKEN:
  case LIST_TOKEN:
    return true;
  default:
    return false;
  }
}

static AST_NODE *parse_interp(void);
static AST_NODE *parse_match_expr(AST_NODE *subject);

// to, until and by are identifiers checked by strcmp.
static bool is_range_word(Token t) {
  return t.type == IDENTIFIER_TOKEN && t.atribute != NULL &&
         (strcmp(t.atribute, "to") == 0 || strcmp(t.atribute, "until") == 0 ||
          strcmp(t.atribute, "by") == 0);
}

// PatArg → ID, literal, _, or negative number (flat, no recursion).
static AST_NODE *parse_pat_arg_node(void) {
  Token t = parser_peek();
  switch (t.type) {
  case INT_NUM_TOKEN:
    parser_advance();
    return ast_int_literal_create(strtoll(t.atribute, NULL, 10), NULL);
  case DOUBLE_NUM_TOKEN:
    parser_advance();
    return ast_double_literal_create(strtod(t.atribute, NULL), NULL);
  case STRING_WORD_TOKEN:
    parser_advance();
    return ast_string_literal_create(t.atribute, NULL);
  case TRUE_TOKEN:
  case FALSE_TOKEN:
    parser_advance();
    return ast_bool_literal_create(t.type == TRUE_TOKEN, NULL);
  case UNDERSCORE_TOKEN:
    parser_advance();
    return ast_wildcard_create(NULL);
  case IDENTIFIER_TOKEN: {
    parser_advance();
    AST_NODE *node = ast_ident_create(NULL, t.atribute, NULL);
    node->id = next_id++;
    node->line = t.line;
    node->is_binding = true;
    return node;
  }
  case MINUS_TOKEN: {
    parser_advance();
    Token num = parser_peek();
    if (num.type == INT_NUM_TOKEN) {
      parser_advance();
      return ast_int_literal_create(-strtoll(num.atribute, NULL, 10), NULL);
    }
    if (num.type == DOUBLE_NUM_TOKEN) {
      parser_advance();
      return ast_double_literal_create(-strtod(num.atribute, NULL), NULL);
    }
    error_exit(ERROR_SYN, num.line, "syntax error");
    return NULL;
  }
  default:
    error_exit(ERROR_SYN, t.line, "syntax error");
    return NULL;
  }
}

// Pattern → literal, _, (), -num, ID : Type, ID ( PatArgList )
static AST_NODE *parse_pattern_node(void) {
  Token t = parser_peek();
  if (t.type == INT_NUM_TOKEN || t.type == DOUBLE_NUM_TOKEN ||
      t.type == STRING_WORD_TOKEN || t.type == TRUE_TOKEN ||
      t.type == FALSE_TOKEN || t.type == UNDERSCORE_TOKEN ||
      t.type == MINUS_TOKEN) {
    return parse_pat_arg_node();
  }
  if (t.type == LPAR_TOKEN) {
    parser_advance();
    parser_expect(RPAR_TOKEN);
    return ast_unit_literal_create(NULL);
  }
  if (t.type == IDENTIFIER_TOKEN) {
    parser_advance();
    Token next = parser_peek();
    if (next.type == COLON_TOKEN) {
      parser_advance();
      AST_NODE *node = ast_ident_create(NULL, t.atribute, NULL);
      node->type_ref = parse_type_ref();
      node->data_type = type_ref_basic(node->type_ref);
      node->id = next_id++;
      node->line = t.line;
      node->is_binding = true;
      return node;
    }
    if (next.type == LPAR_TOKEN) {
      parser_advance();
      AST_NODE *args = NULL;
      if (parser_peek().type != RPAR_TOKEN) {
        args = ast_append(parse_pat_arg_node(), args);
        while (parser_peek().type == COMMA_TOKEN) {
          parser_advance();
          args = ast_append(parse_pat_arg_node(), args);
        }
      }
      parser_expect(RPAR_TOKEN);
      AST_NODE *node = ast_call_create(args, t.atribute, false, true, NULL);
      node->line = t.line;
      return node;
    }
    error_exit(ERROR_SYN, next.line, "syntax error");
  }
  error_exit(ERROR_SYN, t.line, "syntax error");
  return NULL;
}

static void skip_expression(void);
static void check_range_shape(int start);
static AST_NODE *parse_block(void);

static AST_NODE *parse_rhs(void);
static AST_NODE *wrap_expr_stmt(AST_NODE *expr);

// MatchExpr → EOL INDENT (EOL)* CASE+ DEDENT.
// Each branch: CASE Pattern [IF Expr] FATARROW body.
static AST_NODE *parse_match_expr(AST_NODE *subject) {
  Token kw = parser_advance(); // MATCH
  parser_expect(EOL_TOKEN);
  parser_expect(INDENT_TOKEN);
  AST_NODE *cases = NULL;
  AST_NODE *cases_tail = NULL;
  bool saw_case = false;
  for (;;) {
    Token t = parser_peek();
    if (t.type == EOL_TOKEN) {
      parser_advance();
      continue;
    }
    if (t.type == DEDENT_TOKEN) {
      if (!saw_case) {
        error_exit(ERROR_SYN, t.line, "syntax error");
      }
      parser_advance();
      break;
    }
    Token case_kw = parser_expect(CASE_TOKEN);
    AST_NODE *pattern = parse_pattern_node();
    AST_NODE *guard = NULL;
    if (parser_peek().type == IF_TOKEN) {
      parser_advance();
      guard = parse_rhs();
    }
    parser_expect(ARROW_FAT_TOKEN);
    AST_NODE *body;
    if (parser_peek().type == EOL_TOKEN) {
      parser_advance();
      parser_expect(INDENT_TOKEN);
      body = parse_block();
      parser_expect(DEDENT_TOKEN);
    } else {
      body = wrap_expr_stmt(parse_rhs());
      if (parser_peek().type == EOL_TOKEN) {
        parser_advance();
      }
    }
    AST_NODE *branch = ast_case_create(pattern, guard, body, NULL);
    branch->line = case_kw.line;
    chain_append(&cases, &cases_tail, branch);
    saw_case = true;
  }
  AST_NODE *node = ast_match_create(subject, cases, NULL);
  node->line = kw.line;
  return node;
}

static void skip_expression(void) {
  int depth = 0;
  int ifs = 0;
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
    case MATCH_TOKEN: { // match postfix on an operand
      AST_NODE *node = parse_match_expr(NULL);
      free_AST(node);
      if (depth == 0) {
        return; // expression ends at DEDENT
      }
      break;
    }
    case VAL_TOKEN:
    case VAR_TOKEN:
    case WHILE_TOKEN:
    case FOR_TOKEN:
    case RETURN_TOKEN:
    case DEF_TOKEN:
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
    case DO_TOKEN:
    case DEDENT_TOKEN:
    case EOF_TOKEN:
    case RBRACE_TOKEN:
    case ARROW_FAT_TOKEN: // match guard ends before =>
    case ASSIGN_TOKEN:    // assignment is not an expression
      return;
    default:
      parser_advance();
    }
  }
}

static AST_NODE *parse_interp(void);
static void check_range_shape(int start);

// Skips an expression, returns NULL or the interp node.
// True when the precedence analysis can handle the expression at the cursor.
// Calls, member access, ranges, match, if-expressions and interpolation
// still go through skip_expression.
static bool precedence_can_parse(void) {
  int depth = 0;
  for (int i = pos; i < ntokens; i++) {
    Tokens t = tokens[i].type;
    if (t == ASSIGN_TOKEN) {
      return false;
    }
    if (depth == 0 &&
        (t == EOL_TOKEN || t == SEMICOLON_TOKEN || t == THEN_TOKEN ||
         t == DO_TOKEN || t == ELSE_TOKEN || t == ARROW_FAT_TOKEN ||
         t == COMMA_TOKEN || t == RPAR_TOKEN || t == DEDENT_TOKEN ||
         t == EOF_TOKEN)) {
      return true;
    }
    if (t == LPAR_TOKEN) {
      depth++;
    } else if (t == RPAR_TOKEN) {
      depth--;
    }
  }
  return true;
}

static AST_NODE *parse_rhs(void);
static AST_NODE *wrap_expr_stmt(AST_NODE *expr);

static AST_NODE *parse_if_expr(bool in_args) {
  bool prev_in_args = expr_in_args;
  expr_in_args = in_args;
  Token kw = parser_advance();
  AST_NODE *cond = parse_rhs();
  parser_expect(THEN_TOKEN);
  AST_NODE *then_branch;
  if (parser_peek().type == EOL_TOKEN) {
    parser_advance();
    parser_expect(INDENT_TOKEN);
    then_branch = parse_block();
    parser_expect(DEDENT_TOKEN);
  } else {
    then_branch = wrap_expr_stmt(parse_rhs());
  }
  if (parser_peek().type != ELSE_TOKEN) {
    error_exit(ERROR_SYN, parser_peek().line,
               "else is required in an if expression");
  }
  parser_advance();
  AST_NODE *else_branch;
  if (parser_peek().type == EOL_TOKEN) {
    parser_advance();
    parser_expect(INDENT_TOKEN);
    else_branch = parse_block();
    parser_expect(DEDENT_TOKEN);
  } else if (parser_peek().type == IF_TOKEN) {
    else_branch = parse_if_expr(in_args);
  } else {
    else_branch = wrap_expr_stmt(parse_rhs());
  }
  AST_NODE *node = ast_if_create(cond, then_branch, else_branch, NULL);
  node->line = kw.line;
  expr_in_args = prev_in_args;
  return node;
}

static AST_NODE *parse_rhs(void) {
  Token t = parser_peek();
  if (t.type == IF_TOKEN) {
    return parse_if_expr(expr_in_args);
  }
  if (!is_expr_start(t.type)) {
    error_exit(ERROR_SYN, t.line, "syntax error");
  }
  int start = pos;
  AST_NODE *node = NULL;
  if (precedence_can_parse()) {
    node = expr_in_args ? precedence_parse_arg() : precedence_parse();
  } else {
    skip_expression();
  }
  check_range_shape(start);
  while (parser_peek().type == MATCH_TOKEN) {
    node = parse_match_expr(node);
  }
  return node;
}

// Checks range shape on skipped tokens, per parenthesis level.
static void check_range_shape(int start) {
  int dir[16] = {0};
  bool by[16] = {false};
  int depth = 0;
  bool prev_operand = false;
  bool prev_dot = false;
  for (int i = start; i < pos; i++) {
    Token t = tokens[i];
    switch (t.type) {
    case LPAR_TOKEN:
      if (depth < 15) {
        depth++;
        dir[depth] = 0;
        by[depth] = false;
      }
      prev_operand = false;
      prev_dot = false;
      continue;
    case RPAR_TOKEN:
      if (depth > 0) {
        depth--;
      }
      prev_operand = true; // a closed expression is an operand
      prev_dot = false;
      continue;
    case COMMA_TOKEN:
      // each argument has its own range shape
      if (depth < 16) {
        dir[depth] = 0;
        by[depth] = false;
      }
      prev_operand = false;
      prev_dot = false;
      continue;
    case DOT_TOKEN:
      prev_dot = true;
      continue;
    case IDENTIFIER_TOKEN:
      if (!prev_dot && prev_operand && is_range_word(t)) {
        if (strcmp(t.atribute, "by") == 0) {
          if (dir[depth] != 1 || by[depth]) {
            error_exit(ERROR_SYN, t.line, "syntax error");
          }
          by[depth] = true;
        } else {
          if (dir[depth] != 0 || by[depth]) {
            error_exit(ERROR_SYN, t.line, "syntax error");
          }
          dir[depth] = (strcmp(t.atribute, "to") == 0) ? 1 : 2;
        }
        prev_operand = false;
        prev_dot = false;
        continue;
      }
      prev_operand = true;
      prev_dot = false;
      continue;
    case INT_NUM_TOKEN:
    case DOUBLE_NUM_TOKEN:
    case STRING_WORD_TOKEN:
    case TRUE_TOKEN:
    case FALSE_TOKEN:
    case LIST_TOKEN:
      prev_operand = true;
      prev_dot = false;
      continue;
    case MATCH_TOKEN: // skip the whole match block
      i++;            // MATCH
      // find the INDENT that opens the match block
      while (i < pos && tokens[i].type != INDENT_TOKEN) {
        i++;
      }
      if (i < pos) {
        int mdepth = 1; // inside the match block
        i++;
        while (i < pos && mdepth > 0) {
          if (tokens[i].type == INDENT_TOKEN)
            mdepth++;
          else if (tokens[i].type == DEDENT_TOKEN)
            mdepth--;
          i++;
        }
      }
      prev_operand = true; // match result is an operand
      prev_dot = false;
      continue;
    default:
      prev_operand = false; // operators and the rest
      prev_dot = false;
      continue;
    }
  }
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
      chain_append(&head, &tail, ast_ident_create(NULL, t.atribute, NULL));
      continue;
    case INTERP_MID_TOKEN: // text between two interpolations
      parser_advance();
      chain_append(&head, &tail, ast_string_literal_create(t.atribute, NULL));
      continue;
    case INTERP_END_TOKEN: // last text part
      parser_advance();
      chain_append(&head, &tail, ast_string_literal_create(t.atribute, NULL));
      AST_NODE *node = ast_create(head, NULL, NULL, NODE_INTERP);
      node->line = start.line;
      return node;
    default:
      error_exit(ERROR_SYN, t.line, "syntax error");
    }
  }
}

AST_NODE *parser_parse_interp(void) { return parse_interp(); }

AST_NODE *parser_parse_if_expr(bool in_args) { return parse_if_expr(in_args); }

static void parse_mid_item(AST_NODE **head, AST_NODE **tail);
static void parse_if_stmt(AST_NODE **head, AST_NODE **tail);
static void parse_while_stmt(AST_NODE **head, AST_NODE **tail);
static void parse_for_stmt(AST_NODE **head, AST_NODE **tail);
static AST_NODE *parse_block(void);

// FIRST(BlockItem), tokens that can start a statement.
static bool is_stmt_start(Tokens t) {
  switch (t) {
  case VAL_TOKEN:
  case VAR_TOKEN:
  case RETURN_TOKEN:
  case IF_TOKEN:
  case WHILE_TOKEN:
  case FOR_TOKEN:
  case IDENTIFIER_TOKEN:
  case INT_NUM_TOKEN:
  case DOUBLE_NUM_TOKEN:
  case STRING_WORD_TOKEN:
  case LPAR_TOKEN:
  case TRUE_TOKEN:
  case FALSE_TOKEN:
  case INTERP_START_TOKEN:
  case LIST_TOKEN:
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
  int start = pos - 1; // include the saved ID in the range check
  skip_expression();
  check_range_shape(start);
  return NULL;
}

// ValVarTail → COLON Type ASSIGN Rhs | ASSIGN Rhs
static AST_NODE *parse_val_var_tail(char *name, bool is_const, int line) {
  AST_NODE *declared_type = NULL;
  DATA_TYPE declared = DATA_NONE;
  if (parser_peek().type == COLON_TOKEN) {
    parser_advance();
    declared_type = parse_type_ref();
    declared = type_ref_basic(declared_type);
  }
  parser_expect(ASSIGN_TOKEN);
  AST_NODE *node =
      ast_var_def_create(parse_rhs(), NULL, name, is_const, declared);
  node->type_ref = declared_type;
  node->id = next_id++;
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
  if (t.type == EOL_TOKEN || t.type == SEMICOLON_TOKEN ||
      t.type == ELSE_TOKEN || t.type == DEDENT_TOKEN || t.type == RPAR_TOKEN ||
      t.type == COMMA_TOKEN) {
    node = ast_return_create(NULL, NULL); // bare return
  } else {
    node = ast_return_create(parse_rhs(), NULL);
  }
  node->line = kw.line;
  return node;
}

// SimpleStmt → ValStmt | VarStmt | ReturnStmt | ID StmtIdTail | ExprStmt
// Wraps an expression into an expression statement.
static AST_NODE *wrap_expr_stmt(AST_NODE *expr) {
  if (expr == NULL) {
    return NULL;
  }
  AST_NODE *stmt = ast_create(expr, NULL, NULL, NODE_EXPR_STMT);
  stmt->line = expr->line;
  return stmt;
}

static AST_NODE *parse_simple_stmt(void) {
  switch (parser_peek().type) {
  case VAL_TOKEN:
    return parse_val_stmt();
  case VAR_TOKEN:
    return parse_var_stmt();
  case RETURN_TOKEN:
    return parse_return_stmt();
  case IDENTIFIER_TOKEN: {
    if (pos + 1 < ntokens && tokens[pos + 1].type == ASSIGN_TOKEN) {
      Token id = parser_advance();
      return parse_stmt_id_tail(id);
    }
    return wrap_expr_stmt(parse_rhs());
  }
  default:
    return wrap_expr_stmt(parse_rhs());
  }
}

static AST_NODE *parse_if_branch(void);
static AST_NODE *parse_while_body(void);
static AST_NODE *parse_for_body(void);

static AST_NODE *parse_control_body(void) {
  if (parser_peek().type == EOL_TOKEN) {
    parser_advance();
    parser_expect(INDENT_TOKEN);
    AST_NODE *body = parse_block();
    parser_expect(DEDENT_TOKEN);
    return body;
  }
  switch (parser_peek().type) {
  case IF_TOKEN:
    return parse_if_branch();
  case WHILE_TOKEN:
    return parse_while_body();
  case FOR_TOKEN:
    return parse_for_body();
  default:
    return parse_simple_stmt();
  }
}

// LineEnd → EOL | SEMICOLON MidItem
// After a DEDENT (match or break-form if), no terminator needed.
static void parse_line_end(AST_NODE **head, AST_NODE **tail) {
  if (pos > 0 && tokens[pos - 1].type == DEDENT_TOKEN) {
    return; // multi-line statement, no terminator
  }
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

static AST_NODE *parse_if_branch(void) {
  Token kw = parser_advance();
  AST_NODE *cond = parse_rhs();
  parser_expect(THEN_TOKEN);
  AST_NODE *then_branch = parse_control_body();
  AST_NODE *else_branch = NULL;
  if (parser_peek().type == ELSE_TOKEN) {
    parser_advance();
    else_branch = parse_control_body();
  }
  AST_NODE *node = ast_if_create(cond, then_branch, else_branch, NULL);
  node->line = kw.line;
  return node;
}

static void parse_if_stmt(AST_NODE **head, AST_NODE **tail) {
  chain_append(head, tail, parse_if_branch());
  parse_line_end(head, tail);
}

static AST_NODE *parse_while_body(void) {
  Token kw = parser_advance();
  AST_NODE *cond = parse_rhs();
  parser_expect(DO_TOKEN);
  AST_NODE *node = ast_while_create(cond, parse_control_body(), NULL);
  node->line = kw.line;
  return node;
}

static void parse_while_stmt(AST_NODE **head, AST_NODE **tail) {
  chain_append(head, tail, parse_while_body());
  parse_line_end(head, tail);
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

// Converts a range expression (a to b [by c]) into NODE_RANGE.
static AST_NODE *make_range(AST_NODE *expr) {
  if (expr == NULL || expr->type != NODE_BINOP) {
    return expr;
  }
  AST_NODE *step = NULL;
  AST_NODE *range = expr;
  if (expr->op_type == OP_BY) {
    step = expr->right;
    range = expr->left;
    if (range == NULL || range->type != NODE_BINOP) {
      return expr;
    }
  }
  bool inclusive;
  if (range->op_type == OP_TO) {
    inclusive = true;
  } else if (range->op_type == OP_UNTIL) {
    inclusive = false;
  } else {
    return expr;
  }
  AST_NODE *from = range->left;
  AST_NODE *to = range->right;
  range->left = NULL;
  range->right = NULL;
  if (step != NULL) {
    expr->left = NULL;
    expr->right = NULL;
    free_AST(expr);
  }
  free_AST(range);
  return ast_range_create(from, to, step, NULL, inclusive);
}

static AST_NODE *parse_for_body(void) {
  Token kw = parser_advance();
  AST_NODE *node = ast_for_create(NULL, NULL, NULL, NULL, NULL);
  node->id = next_id++;
  node->line = kw.line;
  bool multiline = parser_peek().type == EOL_TOKEN;
  if (multiline) {
    parser_advance();
    parser_expect(INDENT_TOKEN);
  }
  Token id = parser_expect(IDENTIFIER_TOKEN);
  parser_expect(ARROW_LEFT_TOKEN);
  node->left = make_range(parse_rhs());
  if (multiline) {
    parser_expect(EOL_TOKEN);
    node->third = parse_guard_rest();
    parser_expect(DEDENT_TOKEN);
  }
  parser_expect(DO_TOKEN);
  ast_name(node, id.atribute);
  node->right = parse_control_body();
  return node;
}

static void parse_for_stmt(AST_NODE **head, AST_NODE **tail) {
  chain_append(head, tail, parse_for_body());
  parse_line_end(head, tail);
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
    Token st = parser_peek();
    if (st.type == PLUS_TOKEN || st.type == MINUS_TOKEN ||
        st.type == NOT_TOKEN) {
      // logical line cannot start with an operator
      error_exit(ERROR_SYN, st.line, "syntax error");
    }
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
  AST_NODE *rhs = wrap_expr_stmt(parse_rhs());
  if (pos > 0 && tokens[pos - 1].type != DEDENT_TOKEN) {
    parser_expect(EOL_TOKEN);
  }
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
  bool has_parens = parse_params(&params);
  AST_NODE *ret_type = parse_ret_type();
  DATA_TYPE ret = type_ref_basic(ret_type);
  if (is_main) {
    if (params != NULL) {
      int count = 0;
      for (AST_NODE *g = params->next; g != NULL; g = g->next) {
        count += g->param_cnt;
      }
      if (count > 0) {
        error_exit(ERROR_SEM_ARG, id.line, "main must have no parameters");
      }
    }
    if (ret != DATA_NONE && ret != DATA_UNIT) {
      error_exit(ERROR_SEM_TYPE, id.line, "main must return Unit");
    }
  }
  parser_expect(ASSIGN_TOKEN);
  AST_NODE *body = parse_body();
  AST_NODE *node = ast_func_def_create(body, params, NULL, ret, id.atribute,
                                       is_main, has_parens);
  node->type_ref = ret_type;
  node->line = line;
  return node;
}

// ExtensionDef → EXTENSION LPAR Param RPAR EOL INDENT ExtDefs DEDENT
// The receiver is stored as the first param of every method.
static void parse_extension_def(AST_NODE **head, AST_NODE **tail) {
  parser_advance();
  parser_expect(LPAR_TOKEN);
  AST_NODE *receiver = parse_param();
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
      AST_NODE *recv =
          ast_param_create(receiver->data_type, receiver->name, NULL);
      recv->type_ref = receiver->type_ref;
      recv->is_method = true;
      recv->line = receiver->line;
      AST_NODE *recv_group = ast_create_param_list(recv, NULL, 1);
      if (method->third == NULL) {
        method->third = ast_create_params_list_of_list(recv_group, 1);
        method->third->line = receiver->line;
      } else {
        recv_group->next = method->third->next;
        method->third->next = recv_group;
        method->third->param_groups++;
      }
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
    if (t.type == TYPE_TOKEN) {
      chain_append(&head, &tail, parse_type_def());
      continue;
    }
    if (t.type == CASE_TOKEN) {
      chain_append(&head, &tail, parse_case_class());
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

  while (parser_peek().type == INDENT_TOKEN) {
    parser_advance(); // leading spaces before the prolog
  }
  parse_prolog();
  AST_NODE *funcs = parse_func_defs();

  AST_NODE *main_fn = NULL;
  for (AST_NODE *fn = funcs; fn != NULL; fn = fn->next) {
    if (!fn->is_main) {
      continue;
    }
    if (main_fn != NULL) {
      error_exit(ERROR_SEM_REDEF, fn->line, "multiple main functions");
    }
    main_fn = fn;
  }
  if (main_fn == NULL) {
    error_exit(ERROR_SEM_UNDEF, 1, "missing main function");
  }

  for (AST_NODE *fn = funcs; fn != NULL; fn = fn->next) {
    if (fn->is_extension || fn->name == NULL) {
      continue;
    }
    for (AST_NODE *other = fn->next; other != NULL; other = other->next) {
      if (other->is_extension || other->name == NULL) {
        continue;
      }
      if (strcmp(fn->name, other->name) == 0) {
        error_exit(ERROR_SEM_REDEF, other->line, "function redefined");
      }
    }
  }

  AST_NODE *root = ast_create(funcs, NULL, NULL, NODE_PROGRAM);
  parser_cleanup();
  return root;
}
