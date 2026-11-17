/*
* IFJ: Generator
 * Soubor: gen_stmt.c
 * Autor: Pavel Novák [xnovakp01]
 */

#pragma once
#include <stdio.h>

#include "ast.h"
#include "gen_internal.h"
#include "generator.h"

// stack is unchanged
void gen_stmt(Gen *gen, AST_NODE *node) {
  switch (node->type) {
    case NODE_EXPR_STMT:
      gen_expr(gen, node->left);
      break;

    default:
      break;
  }
}

void gen_block(Gen *gen, AST_NODE *node) {

}