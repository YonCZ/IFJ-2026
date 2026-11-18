/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026
*/
#include "symtable.h"
#include "scanner/token.h"
#include "scanner/stack.h"
#include "precedence.h"
#include "parser.h"
#include "scanner/error.h"
#include <stdlib.h>

const PrecedenceRel PSA_TABLE[PSA_TABLE_SIZE][PSA_TABLE_SIZE] = {
    //            u+      u-      u!      *       /       +       -       <       >       <=      >=      ==      !=      (       )       id      $       ++      &&      ||
    /* u+  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* u-  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* u!  */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* *   */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* /   */ { REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* +   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* -   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* <   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* >   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* <=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* >=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* ==  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* !=  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_ERR,REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* (   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_EQ, REL_LT, REL_ERR,REL_LT, REL_LT, REL_LT },
    /* )   */ { REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_ERR,REL_GT, REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT },
    /* id  */ { REL_ERR,REL_ERR,REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_ERR,REL_GT, REL_GT, REL_GT, REL_GT },
    /* $   */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_ERR,REL_LT, REL_ERR,REL_LT, REL_LT, REL_LT },
    /* ++  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT, REL_GT, REL_GT },
    /* &&  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_GT, REL_GT },
    /* ||  */ { REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_LT, REL_GT, REL_LT, REL_GT, REL_LT, REL_LT, REL_GT }
};

static bool is_expression_end(Tokens type, int paren_depth) {
    if (type == EOF_TOKEN) {
        return true;
    }
    if (paren_depth == 0) { //if we're not inside (), the symbol is expr end
        switch (type) {
            case EOL_TOKEN:
            case SEMICOLON_TOKEN:
            case THEN_TOKEN:
            case ELSE_TOKEN:
            case DO_TOKEN:
            case COMMA_TOKEN:
            case RPAR_TOKEN:
            case DETENT_TOKEN:
            case ARROW_FAT_TOKEN:
            case ASSIGN_TOKEN:
                return true;
            default:
                break;
        }
    }
    return false;
}

static bool is_unary_symbol(PSASymbol symbol) {
    return symbol == PSA_UN_PLUS ||
           symbol == PSA_UN_MINUS ||
           symbol == PSA_UN_NOT;
}

static bool expects_next_operand(PSASymbol symbol) {
    return is_unary_symbol(symbol) ||
           symbol == PSA_LPAREN ||
           symbol == PSA_MUL ||
           symbol == PSA_DIV ||
           symbol == PSA_PLUS ||
           symbol == PSA_MINUS ||
           symbol == PSA_LT ||
           symbol == PSA_GT ||
           symbol == PSA_LEQ ||
           symbol == PSA_GEQ ||
           symbol == PSA_EQ ||
           symbol == PSA_NEQ ||
           symbol == PSA_CONCAT ||
           symbol == PSA_AND ||
           symbol == PSA_OR;
}

static PSASymbol token_to_symbol(Token token, bool expecting_operand) {
    switch (token.type) {
        case PLUS_TOKEN:
            return expecting_operand ? PSA_UN_PLUS : PSA_PLUS;
        case MINUS_TOKEN:
            return expecting_operand ? PSA_UN_MINUS : PSA_MINUS;
        case NOT_TOKEN:
            if (!expecting_operand) {
                error_exit(ERROR_SYN, token.line, "Unexpected unary operator '!'");
            }
            return PSA_UN_NOT;
        case MUL_TOKEN: return PSA_MUL;
        case DIV_TOKEN: return PSA_DIV;
        case LESS_TOKEN: return PSA_LT;
        case GREAT_TOKEN: return PSA_GT;
        case LESS_EQ_TOKEN: return PSA_LEQ;
        case GREAT_EQ_TOKEN: return PSA_GEQ;
        case EQ_TOKEN: return PSA_EQ;
        case NEQ_TOKEN: return PSA_NEQ;
        case LPAR_TOKEN: return PSA_LPAREN;
        case RPAR_TOKEN: return PSA_RPAREN;
        case PLUSPLUS_TOKEN: return PSA_CONCAT;
        case AND_TOKEN: return PSA_AND;
        case OR_TOKEN: return PSA_OR;
        case IDENTIFIER_TOKEN:
        case INT_NUM_TOKEN:
        case DOUBLE_NUM_TOKEN:
        case STRING_WOR_TOKEN:
        case TRUE_TOKEN:
        case FALSE_TOKEN:
            return PSA_ID;
        default:
            error_exit(ERROR_SYN, token.line, "Invalid token in precedence expression");
            return PSA_DOLLAR;
    }
}

static AST_NODE *create_leaf(Token token) {
    AST_NODE *node = NULL;

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
        case STRING_WOR_TOKEN:
            node = ast_string_literal_create(token.atribute, NULL);
            break;
        case TRUE_TOKEN:
        case FALSE_TOKEN:
            node = ast_create(NULL, NULL, NULL, NODE_LITERAL);
            node->data_type = DATA_BOOL;
            node->ival = (token.type == TRUE_TOKEN);
            break;
        default:
            error_exit(ERROR_SYN, token.line, "Token is not an expression operand");
            return NULL;
    }

    node->line = token.line;
    // NEVOLAT free(token.atribute) - pole tokenů uvolňuje parser_cleanup()!
    return node;
}

static bool symbol_to_operator(PSASymbol symbol, OP_TYPE *operator) {
    switch (symbol) {
        case PSA_PLUS:   *operator = OP_ADD; return true;
        case PSA_MINUS:  *operator = OP_SUB; return true;
        case PSA_MUL:    *operator = OP_MUL; return true;
        case PSA_DIV:    *operator = OP_DIV; return true;
        case PSA_LT:     *operator = OP_LT;  return true;
        case PSA_GT:     *operator = OP_GT;  return true;
        case PSA_LEQ:    *operator = OP_LE;  return true;
        case PSA_GEQ:    *operator = OP_GE;  return true;
        case PSA_EQ:     *operator = OP_EQ;  return true;
        case PSA_NEQ:    *operator = OP_NEQ; return true;
        case PSA_CONCAT: *operator = OP_ADD; return true; // TODO: zvážit vlastní OP_CONCAT
        case PSA_AND:    *operator = OP_AND; return true;
        case PSA_OR:     *operator = OP_OR;  return true;
        default: return false;
    }
}

static OP_TYPE unary_operator(PSASymbol symbol) {
    switch (symbol) {
        case PSA_UN_PLUS:  return OP_ADD;
        case PSA_UN_MINUS: return OP_SUB;
        case PSA_UN_NOT:   return OP_NOT;
        default:           return OP_NOT;
    }
}

static void reduce_stack(Stack *stack, int line) {
    StackSymbol handle[4];
    size_t handle_size = 0;
    StackSymbol symbol;
    bool marker_found = false;

    while (!stack_is_empty(stack)) {
        symbol = stack_pop(stack);
        if (symbol.kind == STACK_MARKER) {
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
        error_exit(ERROR_SYN, line, "Missing precedence marker");
        return;
    }

    AST_NODE *node = NULL;
    if (handle_size == 1 &&
        ((handle[0].kind == STACK_NONTERMINAL) ||
         (handle[0].kind == STACK_TERMINAL &&
          (PSASymbol)handle[0].token == PSA_ID))) {
        node = handle[0].node;
    } else if (handle_size == 2 &&
               handle[0].kind == STACK_NONTERMINAL &&
               handle[1].kind == STACK_TERMINAL &&
               is_unary_symbol((PSASymbol)handle[1].token)) {
        node = ast_unop_create(handle[0].node, NULL,
                               unary_operator((PSASymbol)handle[1].token));
    } else if (handle_size == 3 &&
               handle[0].kind == STACK_TERMINAL &&
               (PSASymbol)handle[0].token == PSA_RPAREN &&
               handle[1].kind == STACK_NONTERMINAL &&
               handle[2].kind == STACK_TERMINAL &&
               (PSASymbol)handle[2].token == PSA_LPAREN) {
        node = handle[1].node;
    } else if (handle_size == 3 &&
               handle[0].kind == STACK_NONTERMINAL &&
               handle[1].kind == STACK_TERMINAL &&
               handle[2].kind == STACK_NONTERMINAL) {
        OP_TYPE operator;
        if (symbol_to_operator((PSASymbol)handle[1].token, &operator)) {
            node = ast_binop_create(handle[2].node, handle[0].node,
                                    NULL, operator);
        }
    }

    if (node == NULL) {
        error_exit(ERROR_SYN, line, "Invalid expression handle");
        return;
    }

    node->line = line;
    stack_push(stack, (StackSymbol){
        .kind = STACK_NONTERMINAL,
        .token = 0,
        .node = node
    });
}

AST_NODE *precedence_parse(void) {
    Stack *stack = stack_init();
    int paren_depth = 0;
    bool expecting_operand = true;
    bool saw_any_token = false;

    stack_push(stack, (StackSymbol){
        .kind = STACK_TERMINAL,
        .token = (Tokens)PSA_DOLLAR,
        .node = NULL
    });

    while (true) {

        Token input = parser_peek(); //todo take out of static in parser

        if (is_expression_end(input.type, paren_depth)) {
            break;
        }

        if (input.type == EOL_TOKEN && paren_depth > 0) {
            parser_advance();
            continue;
        }

        saw_any_token = true;
        StackSymbol top = stack_top_terminal(stack);
        PSASymbol top_symbol = (PSASymbol)top.token;
        PSASymbol input_symbol = token_to_symbol(input, expecting_operand);

        if (top.kind != STACK_TERMINAL ||
            top_symbol >= PSA_TABLE_SIZE ||
            input_symbol >= PSA_TABLE_SIZE) {
            error_exit(ERROR_SYN, input.line, "Invalid precedence symbol");
            stack_dispose(stack);
            return NULL;
        }

        switch (PSA_TABLE[top_symbol][input_symbol]) {
            case REL_LT:
                if (input_symbol == PSA_LPAREN) {
                    paren_depth++;
                }
                stack_insert_marker(stack);
                stack_push(stack, (StackSymbol){
                    .kind = STACK_TERMINAL,
                    .token = (Tokens)input_symbol,
                    .node = (input_symbol == PSA_ID) ? create_leaf(input) : NULL
                });
                expecting_operand = expects_next_operand(input_symbol);
                parser_advance();
                break;

            case REL_GT:
                reduce_stack(stack, input.line);
                break;

            case REL_EQ:
                if (input_symbol == PSA_RPAREN) {
                    paren_depth--;
                }
                stack_push(stack, (StackSymbol){
                    .kind = STACK_TERMINAL,
                    .token = (Tokens)input_symbol,
                    .node = NULL
                });
                expecting_operand = false;
                parser_advance();
                break;

            case REL_ERR:
                error_exit(ERROR_SYN, input.line, "Syntax error in precedence parsing");
                stack_dispose(stack);
                return NULL;
        }
    }

    Token end_token = parser_peek();
    if (!saw_any_token || paren_depth != 0) {
        error_exit(ERROR_SYN, end_token.line, "Empty or unclosed expression");
        stack_dispose(stack);
        return NULL;
    }

    while ((PSASymbol)stack_top_terminal(stack).token != PSA_DOLLAR) {
        reduce_stack(stack, end_token.line);
    }

    StackSymbol result = stack_top(stack);
    if (result.kind != STACK_NONTERMINAL || result.node == NULL) {
        error_exit(ERROR_SYN, end_token.line, "Invalid final expression state");
        stack_dispose(stack);
        return NULL;
    }

    AST_NODE *root = result.node;
    stack_dispose(stack);
    return root;
}