/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_expr.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"
//===========================
// puts one value on stack
//===========================


void literal(Gen *gen, AST_NODE *node, char* func_name) {
  printf("%s ", func_name);
  switch (node->data_type) {
    case DATA_STRING:
      emit_string_literal(gen, node->sval);
      break;
    case DATA_INT:
      printf("int@%lld", node->ival);
      break;
    case DATA_BOOL:
      printf("bool@%s", node->ival?"true":"false");
      break;
    case DATA_DOUBLE:
      printf("float@%a", node->dval);
      break;
    case DATA_UNIT:
      printf("string@()");
      break;
    default:
      exit(99);
  }
  printf("\n");
}

void pop_to_val(AST_NODE *node) {
  printf("POPS LF@%s$%d\n",node->name, node->id);
}

void push_to_val(AST_NODE *node) {
  printf("PUSHS LF@%s$%d\n",node->name, node->id);
}

void gen_expr(Gen *gen, AST_NODE *node) {
  switch (node->type) {
    case NODE_CALL:
      /*===============
       *PRINT
       *===============
       *
       *only for constants*/
      if (strcmp(node->name, "print")==0) {
        if (node->third->type == NODE_LITERAL) literal(gen,node->third, "WRITE");
        //print(x)
        else {
          gen_expr(gen, node->third);
          printf("POPS LF@tmp1\n");
          printf("WRITE LF@tmp1\n");
        }
        //only expresion which doesnt return value
        printf("PUSHS nil@nil\n");
      }
      else {
        for (AST_NODE *param = node->third; param != NULL; param = param->next) {
          gen_expr(gen, param);
        }
        printf("CALL $f_%s\n",node->name);
      }
      break;

    case NODE_IDENT:
      push_to_val(node);
      break;

    case NODE_LITERAL:
      literal(gen, node, "PUSHS");
      break;


    case NODE_UNOP:
      switch (node->op_type) {
        case OP_ADD:
          gen_expr(gen, node->left);
          printf("PUSHS int@1\n");
          printf("ADDS\n");
          break;
        case OP_SUB:
          node->data_type == DATA_INT ? printf("PUSHS int@0\n"):printf(" PUSHS float@%a\n", 0.0);
          gen_expr(gen, node->left);
          printf("SUBS\n");
          break;
        case OP_NOT:
          gen_expr(gen, node->left);
          printf("NOTS\n");
          break;
        default:
          break;
      }
      break;
    case NODE_BINOP:
      if ((node->left->data_type==DATA_INT&&node->right->data_type==DATA_DOUBLE)){
        gen_expr(gen, node->left);
        printf("INT2FLOATS\n");
        gen_expr(gen, node->right);
      }
      else if((node->left->data_type==DATA_DOUBLE && node->right->data_type==DATA_INT)) {
        gen_expr(gen, node->left);
        gen_expr(gen, node->right);
        printf("INT2FLOATS\n");
      }
      else {
        gen_expr(gen, node->left);
        gen_expr(gen, node->right);
      }



      switch (node->op_type) {
        case OP_ADD:
          // 1+1 or 'a'+'b'
          if (node->data_type==DATA_INT || node->data_type==DATA_DOUBLE) printf("ADDS\n");
          else if (node->data_type==DATA_STRING) {
            printf("POPS LF@tmp1\n");
            printf("POPS LF@tmp2\n");
            printf("CONCAT LF@tmp3 LF@tmp2 LF@tmp1\n");
            printf("PUSHS LF@tmp3\n");
          }
          break;
        case OP_GT:
          printf("GTS\n");
          break;
        case OP_LT:
          printf("LTS\n");
          break;
        case OP_EQ:
          printf("EQS\n");
          break;
        case OP_LE:
          printf("GTS\n");
          printf("NOTS\n");
          break;
        case OP_GE:
          printf("LTS\n");
          printf("NOTS\n");
          break;
        case OP_DIV:
          node->data_type==DATA_DOUBLE ? printf("DIVS\n"):printf("IDIVS\n");
          break;
        case OP_MUL:
          printf("MULS\n");
          break;
        case OP_SUB:
          printf("SUBS\n");
          break;
        case OP_AND:
          printf("ANDS\n");
          break;
        case OP_OR:
          printf("ORS\n");
          break;
        case OP_NOT:
          printf("NOTS\n");
          break;
        case OP_NEQ:
          printf("EQS\n");
          printf("NOTS\n");
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }
}

