/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_stmt.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */


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

    case NODE_VAR_DEF:
      if (!node->left)break;
      gen_expr(gen, node->left);
      pop_to_val(node);
      break;

    case NODE_ASSIGN:
      gen_expr(gen, node->right);
      pop_to_val(node->left);
      break;

    default:
      break;
  }
}

void gen_block(Gen *gen, AST_NODE *node) {

}