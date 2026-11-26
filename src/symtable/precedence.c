/**
IFJ26 Compiler 
Author: Lucie Alexandrova [xalexal00]
File: precedence.c

Grammar rules:
* E -> i (i is identifier or literal)
* E -> (E)
* E -> - E
* E -> + E
* E -> E * E
* E -> E / E
* E -> E + E
* E -> E - E
* E -> E < E
* E -> E <= E
* E -> E > E
* E -> E >= E
* E -> E == E
* E -> E != E

for boolthen extension:
* E -> !E
* E -> E && E
* E -> E || E

for lists extension:
* E -> E ++ E
*/
#include "precedence.h"
#include "error.h"
#include "parser/parser.h"
#include "scanner.h"
#include "stack.h"
#include "symtable.h"
#include "token.h"
#include <stdlib.h>
#include <string.h>

// clang-format off
const PrecedenceRel PSA_TABLE[PSA_TABLE_SIZE][PSA_TABLE_SIZE] = {
    //            u+      u-      u!      *       /       +       -       <       >       <=      >=      ==      !=      (       )       id      $       ++      &&      ||      to      until   by
    /* u+  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* u-  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* u!  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* *   */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* /   */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* +   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* -   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* <   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* >   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* <=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* >=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* ==  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* !=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* (   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_EQ, REL_LT, REL_ERR,REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT },
    /* )   */ { REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_ERR,REL_GT, REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* id  */ { REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* $   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_LT, REL_ERR,REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT },
    /* ++  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* &&  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* ||  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* to  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_GT },
    /*until*/ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR },
    /* by  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR },
};
// clang-format on

// checks whether the token ends an expression
static bool is_expression_end(Tokens type) {
  return type == SEMICOLON_TOKEN || type == THEN_TOKEN || type == ELSE_TOKEN ||
         type == DO_TOKEN || type == EOL_TOKEN || type == MATCH_TOKEN ||
         type == ARROW_FAT_TOKEN || type == RBRACE_TOKEN || type == EOF_TOKEN;
}

// converts a token to a precedence analysis symbol
static PSASymbol token_to_symbol(Token token, bool expecting_operand) {
  switch (token.type) {
  case PLUS_TOKEN:
    return expecting_operand ? PSA_UN_PLUS : PSA_PLUS;
  case MINUS_TOKEN:
    return expecting_operand ? PSA_UN_MINUS : PSA_MINUS;
  case NOT_TOKEN:
    return PSA_UN_NOT;
  case MUL_TOKEN:
    return PSA_MUL;
  case DIV_TOKEN:
    return PSA_DIV;
  case LESS_TOKEN:
    return PSA_LT;
  case GREAT_TOKEN:
    return PSA_GT;
  case LESS_EQ_TOKEN:
    return PSA_LEQ;
  case GREAT_EQ_TOKEN:
    return PSA_GEQ;
  case EQ_TOKEN:
    return PSA_EQ;
  case NEQ_TOKEN:
    return PSA_NEQ;
  case LPAR_TOKEN:
    return PSA_LPAREN;
  case RPAR_TOKEN:
    return PSA_RPAREN;
  case PLUSPLUS_TOKEN:
    return PSA_CONCAT;
  case AND_TOKEN:
    return PSA_AND;
  case OR_TOKEN:
    return PSA_OR;
  case IDENTIFIER_TOKEN:
    if (!expecting_operand && token.atribute != NULL) {
      if (strcmp(token.atribute, "to") == 0)
        return PSA_TO;
      if (strcmp(token.atribute, "until") == 0)
        return PSA_UNTIL;
      if (strcmp(token.atribute, "by") == 0)
        return PSA_BY;
    }
    return PSA_ID;
  case INT_NUM_TOKEN:
  case DOUBLE_NUM_TOKEN:
  case STRING_WORD_TOKEN:
  case UNIT_LIT_TOKEN:
  case TRUE_TOKEN:
  case FALSE_TOKEN:
  case LIST_TOKEN:
    return PSA_ID;
  case EOF_TOKEN:
    return PSA_DOLLAR;
  default:
    // if the token is not a valid expression operand, exit with an error
    error_exit(ERROR_SYN, token.line, "Invalid token in precedence expression");
    return PSA_DOLLAR;
  }
}

// creates an AST leaf based on the token type
static AST_NODE *create_leaf(Token token) {
  AST_NODE *node;

  switch (token.type) {
  case IDENTIFIER_TOKEN:
    node = ast_ident_create(NULL, token.atribute, NULL);
    break;
  case INT_NUM_TOKEN:
    node = ast_int_literal_create(strtoll(token.atribute, NULL, 10), NULL);
    break;
  case DOUBLE_NUM_TOKEN:
    node = ast_double_literal_create(strtod(token.atribute, NULL), NULL);
    break;
  case STRING_WORD_TOKEN:
    node = ast_string_literal_create(token.atribute, NULL);
    break;
  case TRUE_TOKEN:
  case FALSE_TOKEN:
    node = ast_create(NULL, NULL, NULL, NODE_LITERAL);
    node->data_type = DATA_BOOL;
    node->ival = token.type == TRUE_TOKEN;
    break;
  case UNIT_LIT_TOKEN:
    node = ast_create(NULL, NULL, NULL, NODE_LITERAL);
    node->data_type = DATA_UNIT;
    break;
  case LIST_TOKEN:
    node = ast_ident_create(NULL, "List", NULL);
    break;
  default:
    error_exit(ERROR_SYN, token.line, "Token is not an expression operand");
    return NULL;
  }

  node->line = token.line; // store the line for error reporting
  return node;
}

// checks if the symbol is a unary operator
static bool is_unary_symbol(PSASymbol symbol) {
  return symbol == PSA_UN_PLUS || symbol == PSA_UN_MINUS ||
         symbol == PSA_UN_NOT;
}

// converts a PSA symbol to an OP_TYPE operator
static bool symbol_to_operator(PSASymbol symbol, OP_TYPE *operator) {
  switch (symbol) {
  case PSA_PLUS:
    *operator= OP_ADD;
    return true;
  case PSA_MINUS:
    *operator= OP_SUB;
    return true;
  case PSA_MUL:
    *operator= OP_MUL;
    return true;
  case PSA_DIV:
    *operator= OP_DIV;
    return true;
  case PSA_LT:
    *operator= OP_LT;
    return true;
  case PSA_GT:
    *operator= OP_GT;
    return true;
  case PSA_LEQ:
    *operator= OP_LE;
    return true;
  case PSA_GEQ:
    *operator= OP_GE;
    return true;
  case PSA_EQ:
    *operator= OP_EQ;
    return true;
  case PSA_NEQ:
    *operator= OP_NEQ;
    return true;
  case PSA_CONCAT:
    *operator= OP_ADD;
    return true;
  case PSA_AND:
    *operator= OP_AND;
    return true;
  case PSA_OR:
    *operator= OP_OR;
    return true;
  case PSA_TO:
    *operator= OP_TO;
    return true;
  case PSA_UNTIL:
    *operator= OP_UNTIL;
    return true;
  case PSA_BY:
    *operator= OP_BY;
    return true;
  default:
    return false;
  }
}

// returns the operator for a unary symbol
static OP_TYPE unary_operator(PSASymbol symbol) {
  switch (symbol) {
  case PSA_UN_PLUS:
    return OP_ADD;
  case PSA_UN_MINUS:
    return OP_SUB;
  case PSA_UN_NOT:
    return OP_NOT;
  default:
    return OP_NOT;
  }
}

// reduces the rule on top of the stack to one AST node
static void reduce_stack(Stack *stack, int line) {
  StackSymbol handle[4]; // handle is the right side of the rule
  size_t handle_size = 0;
  StackSymbol symbol;
  bool marker_found = false;

  while (!stack_is_empty(stack)) {
    symbol = stack_pop(stack);
    if (symbol.kind == STACK_MARKER) {
      // the marker ends the rule the reduction applies to
      marker_found = true;
      break;
    }
    if (handle_size == sizeof(handle) / sizeof(handle[0])) {
      error_exit(ERROR_SYN, line, "Expression handle is too long");
      return;
    }
    handle[handle_size++] = symbol;
  }

  if (!marker_found) {
    // without a marker the expression is invalid
    error_exit(ERROR_SYN, line, "Missing precedence marker");
    return;
  }

  AST_NODE *node = NULL;
  if (handle_size == 1 && ((handle[0].kind == STACK_NONTERMINAL) ||
                           (handle[0].kind == STACK_TERMINAL &&
                            (PSASymbol)handle[0].token == PSA_ID))) {
    node = handle[0].node;
  } else if (handle_size == 2 && handle[0].kind == STACK_NONTERMINAL &&
             handle[1].kind == STACK_TERMINAL &&
             is_unary_symbol((PSASymbol)handle[1].token)) {
    node = ast_unop_create(handle[0].node, NULL,
                           unary_operator((PSASymbol)handle[1].token));
  } else if (handle_size == 3 && handle[0].kind == STACK_TERMINAL &&
             (PSASymbol)handle[0].token == PSA_RPAREN &&
             handle[1].kind == STACK_NONTERMINAL &&
             handle[2].kind == STACK_TERMINAL &&
             (PSASymbol)handle[2].token == PSA_LPAREN) {
    node = handle[1].node;
  } else if (handle_size == 3 && handle[0].kind == STACK_NONTERMINAL &&
             handle[1].kind == STACK_TERMINAL &&
             handle[2].kind == STACK_NONTERMINAL) {
    OP_TYPE operator;
    if (symbol_to_operator((PSASymbol)handle[1].token, &operator)) {
      node = ast_binop_create(handle[2].node, handle[0].node, NULL, operator);
    }
  }

  if (node == NULL) {
    error_exit(ERROR_SYN, line, "Invalid expression handle");
    return;
  }

  stack_push(stack, (StackSymbol){
                        .kind = STACK_NONTERMINAL, .token = 0, .node = node});
}

// handles a line break and checks that the continuation is indented
static Token consume_continuation(Token eol) {
  parser_advance();
  Token indent = parser_advance();

  if (indent.type != INDENT_TOKEN) {
    error_exit(ERROR_SYN, eol.line, "Continuation line must be indented");
  }

  return parser_peek();
}

// checks the token type and advances the parser, otherwise reports an error
static Token expect_token(Tokens type) {
  Token tok = parser_peek();
  if (tok.type != type) {
    error_exit(ERROR_SYN, tok.line, "syntax error");
  }
  parser_advance();
  return tok;
}

// pops the operand for a postfix operation from the stack
static AST_NODE *pop_postfix_operand(Stack *stack, int line) {
  StackSymbol top_terminal = stack_top_terminal(stack);
  if (top_terminal.kind == STACK_TERMINAL &&
      (PSASymbol)top_terminal.token == PSA_RPAREN) {
    reduce_stack(stack, line);
  }
  StackSymbol top = stack_pop(stack);
  AST_NODE *node = NULL;
  if (top.kind == STACK_NONTERMINAL) {
    node = top.node;
  } else if (top.kind == STACK_TERMINAL && (PSASymbol)top.token == PSA_ID) {
    node = top.node;
  } else {
    error_exit(ERROR_SYN, line, "expected expression before postfix");
  }
  if (stack_top(stack).kind == STACK_MARKER) {
    stack_pop(stack);
  }
  return node;
}

// counts arguments in a chain
static int count_arg_chain(AST_NODE *arg) {
  int count = 0;
  for (; arg != NULL; arg = arg->next) {
    count++;
  }
  return count;
}

// parses comma-separated call arguments and allows a trailing comma only before
// a line break
static AST_NODE *parse_call_args(void) {
  expect_token(LPAR_TOKEN);
  if (parser_peek().type == RPAR_TOKEN) {
    parser_advance();
    return NULL;
  }
  AST_NODE *params = NULL;
  for (;;) {
    AST_NODE *arg = precedence_parse_arg();
    if (arg == NULL) {
      error_exit(ERROR_SYN, parser_peek().line, "syntax error");
    }
    params = ast_append(arg, params);
    Token t = parser_peek();
    if (t.type == COMMA_TOKEN) {
      parser_advance();
      Token next = parser_peek();
      if (next.type == RPAR_TOKEN) {
        if (!next.check_new_line) {
          error_exit(ERROR_SYN, next.line,
                     "trailing comma without end of line");
        }
        parser_advance();
        return params;
      }
      continue;
    }
    expect_token(RPAR_TOKEN);
    return params;
  }
}

// parses expressions with the precedence table while building the AST on the
// stack
static AST_NODE *precedence_parse_internal(bool in_args) {
  Token input = parser_peek();
  Stack *stack = stack_init();
  bool expecting_operand = true;
  int paren_depth = 0;

  stack_push(stack, (StackSymbol){.kind = STACK_TERMINAL,
                                  .token = (Tokens)PSA_DOLLAR,
                                  .node = NULL});

  while (true) {
    // an indented line break can continue an expression when an operand is
    // still expected
    if (input.type == EOL_TOKEN && expecting_operand) {
      input = consume_continuation(input);
      continue;
    }

    if (is_expression_end(input.type)) {
      break;
    }

    if (in_args && paren_depth == 0 &&
        (input.type == COMMA_TOKEN || input.type == RPAR_TOKEN)) {
      break;
    }

    if (expecting_operand && input.type == LPAR_TOKEN &&
        parser_peek_next().type == RPAR_TOKEN) {
      int line = input.line;
      parser_advance();
      parser_advance();
      AST_NODE *unit = ast_unit_literal_create(NULL);
      unit->line = line;
      stack_push(
          stack,
          (StackSymbol){.kind = STACK_NONTERMINAL, .token = 0, .node = unit});
      expecting_operand = false;
      input = parser_peek();
      continue;
    }

    if (expecting_operand && input.type == IF_TOKEN) {
      AST_NODE *node = parser_parse_if_expr(true);
      stack_push(
          stack,
          (StackSymbol){.kind = STACK_NONTERMINAL, .token = 0, .node = node});
      expecting_operand = false;
      input = parser_peek();
      continue;
    }

    if (expecting_operand && input.type == INTERP_START_TOKEN) {
      AST_NODE *node = parser_parse_interp();
      stack_push(
          stack,
          (StackSymbol){.kind = STACK_NONTERMINAL, .token = 0, .node = node});
      expecting_operand = false;
      input = parser_peek();
      continue;
    }

    // a parenthesis after an operand is a function call, not a grouping
    // expression
    if (!expecting_operand && input.type == LPAR_TOKEN) {
      AST_NODE *callee = pop_postfix_operand(stack, input.line);
      AST_NODE *params = parse_call_args();
      AST_NODE *call = NULL;
      if (callee->type == NODE_IDENT && callee->name != NULL) {
        call = ast_call_create(params, callee->name, false, true, NULL);
        free_AST(callee);
      } else if (callee->type == NODE_CALL && callee->is_method &&
                 !callee->has_parens) {
        callee->third = params;
        callee->has_parens = true;
        call = callee;
      } else if (callee->type == NODE_CALL &&
                 (!callee->is_method || callee->has_parens)) {
        AST_NODE *group =
            ast_create_param_list(params, NULL, count_arg_chain(params));
        if (callee->arg_groups == NULL) {
          AST_NODE *first = ast_create_param_list(
              callee->third, NULL, count_arg_chain(callee->third));
          first->next = group;
          callee->arg_groups = ast_create_params_list_of_list(first, 2);
          callee->third = NULL;
        } else {
          AST_NODE *last = callee->arg_groups->next;
          while (last->next != NULL) {
            last = last->next;
          }
          last->next = group;
          callee->arg_groups->param_groups++;
        }
        callee->has_parens = true;
        call = callee;
      } else {
        call = ast_create(callee, NULL, NULL, NODE_CALL);
        call->third = params;
        call->is_index = true;
        call->has_parens = true;
      }
      call->line = input.line;
      stack_push(
          stack,
          (StackSymbol){.kind = STACK_NONTERMINAL, .token = 0, .node = call});
      input = parser_peek();
      continue;
    }

    // a dot after an operand starts a method call whose receiver is the current
    // AST node
    if (!expecting_operand && input.type == DOT_TOKEN) {
      int line = input.line;
      parser_advance();
      Token member = expect_token(IDENTIFIER_TOKEN);
      AST_NODE *recv = pop_postfix_operand(stack, line);
      AST_NODE *node = ast_method_call_create(NULL, member.atribute, true,
                                              false, recv, NULL);
      node->line = line;
      stack_push(
          stack,
          (StackSymbol){.kind = STACK_NONTERMINAL, .token = 0, .node = node});
      input = parser_peek();
      continue;
    }

    StackSymbol top = stack_top_terminal(stack);
    PSASymbol top_symbol = (PSASymbol)top.token;
    PSASymbol input_symbol = token_to_symbol(input, expecting_operand);

    if (top.kind != STACK_TERMINAL || top_symbol >= PSA_TABLE_SIZE ||
        input_symbol >= PSA_TABLE_SIZE) {
      error_exit(ERROR_SYN, input.line, "Invalid precedence symbol");
      break;
    }

    // the table decides whether to shift the input, reduce a grammar handle, or
    // report an error
    switch (PSA_TABLE[top_symbol][input_symbol]) {
    case REL_LT:
      // Shift operation
      stack_insert_marker(stack);
      stack_push(stack, (StackSymbol){.kind = STACK_TERMINAL,
                                      .token = (Tokens)input_symbol,
                                      .node = input_symbol == PSA_ID
                                                  ? create_leaf(input)
                                                  : NULL});
      expecting_operand =
          input_symbol == PSA_LPAREN || input_symbol == PSA_UN_PLUS ||
          input_symbol == PSA_UN_MINUS || input_symbol == PSA_UN_NOT ||
          input_symbol == PSA_MUL || input_symbol == PSA_DIV ||
          input_symbol == PSA_PLUS || input_symbol == PSA_MINUS ||
          input_symbol == PSA_LT || input_symbol == PSA_GT ||
          input_symbol == PSA_LEQ || input_symbol == PSA_GEQ ||
          input_symbol == PSA_EQ || input_symbol == PSA_NEQ ||
          input_symbol == PSA_CONCAT || input_symbol == PSA_AND ||
          input_symbol == PSA_OR || input_symbol == PSA_TO ||
          input_symbol == PSA_UNTIL || input_symbol == PSA_BY;
      if (input_symbol == PSA_LPAREN) {
        paren_depth++;
      }
      parser_advance();
      input = parser_peek();
      break;
    case REL_GT:
      // Reduce operation
      reduce_stack(stack, input.line);
      break;
    case REL_EQ:
      // Match operation
      stack_push(stack, (StackSymbol){.kind = STACK_TERMINAL,
                                      .token = (Tokens)input_symbol,
                                      .node = NULL});
      expecting_operand = false;
      if (input_symbol == PSA_RPAREN && paren_depth > 0) {
        paren_depth--;
      }
      parser_advance();
      input = parser_peek();
      break;
    case REL_ERR:
      // Error handling
      error_exit(ERROR_SYN, input.line, "Syntax error in precedence parsing");
      stack_dispose(stack);
      return NULL;
    }
  }

  while (stack_top_terminal(stack).token != (Tokens)PSA_DOLLAR) {
    reduce_stack(stack, input.line);
  }

  StackSymbol result = stack_top(stack);
  AST_NODE *root = result.kind == STACK_NONTERMINAL ? result.node : NULL;
  stack_dispose(stack);
  return root;
}

// parses a whole expression with precedence analysis
AST_NODE *precedence_parse(void) { return precedence_parse_internal(false); }

// parses one call argument
AST_NODE *precedence_parse_arg(void) { return precedence_parse_internal(true); }
