/*
* IFJ: Generator
 * Soubor: gen_expr.c
 * Autor: Pavel Novák [xnovakp01]
 */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen_internal.h"

// puts one value on stack

void write_literal(Gen *gen, AST_NODE *node) {
  printf("WRITE ");
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
void gen_expr(Gen *gen, AST_NODE *node) {
  switch (node->type) {
    case NODE_CALL:
      /*===============
       *PRINT
       *===============
       *
       *only for constants*/
      if (strcmp(node->name, "print")==0) {
        if (node->third->type == NODE_LITERAL) write_literal(gen,node->third);
      }

      //print(x)
      else if (strcmp(node->name,"print")!=0) {
        printf("POPS LF@tmp\n");
        printf("WRITE LF@tmp\n");
      }
      break;
    default:
      break;
  }
}