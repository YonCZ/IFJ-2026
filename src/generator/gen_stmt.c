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
      printf("POPS LF@tmp1\n");
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

    case NODE_IF: {
      int if_id= new_label(gen);

      //condition
      gen_expr(gen, node->left);
      printf("PUSHS bool@false\n");
      printf("JUMPIFEQS IF$%d$ELSE\n",if_id);


      //then
      gen_block(gen, node->right);
      printf("JUMP IF$%d$END\n",if_id);

      //else
      printf("LABEL IF$%d$ELSE\n",if_id);
      gen_block(gen,node->third);

      //end
      printf("LABEL IF$%d$END\n",if_id);
      break;
    }
      case NODE_WHILE:{
      int while_id = new_label(gen);
      printf("LABEL WHILE$%d\n",while_id);

      gen_expr(gen, node->left);
      printf("PUSHS bool@false\n");
      printf("JUMPIFEQS WHILE$%d$END\n",while_id);

      gen_block(gen, node->right);
      printf("JUMP WHILE$%d\n",while_id);

      printf("LABEL WHILE$%d$END\n",while_id);
      break;
    }

    case NODE_RETURN:
      if (node->left != NULL) {
        gen_expr(gen, node->left);
        if (gen->return_type == DATA_UNIT) {
          printf("POPS LF@tmp1\n");
        } else if (gen->return_type == DATA_DOUBLE && node->left->data_type == DATA_INT) {
          printf("INT2FLOATS\n");
        }
      }
      if (gen->return_type == DATA_UNIT) {
        printf("PUSHS nil@nil\n");
      }
      printf("POPFRAME\n");
      printf("RETURN\n");
      break;

    case NODE_FOR: {
      AST_NODE *range = node->left;
      if (range == NULL) {
        break;
      }

      //assign variables
      gen_expr(gen, range->left);
      printf("POPS LF@%s$%d\n",node->name,node->id);

      gen_expr(gen, range->right);
      printf("POPS LF@for_end$%d\n",node->id);

      if (range->third) {
        gen_expr(gen, range->third);
        printf("POPS LF@for_step$%d\n",node->id);
      }
      else printf("MOVE LF@for_step$%d int@1\n",node->id);

      printf("LABEL for$%d\n",node->id);

      //zero range condition
      printf("JUMPIFNEQ STEP$OK$%d LF@for_step$%d int@0\n", node->id, node->id);
      printf("EXIT int@40\n");
      printf("LABEL STEP$OK$%d\n", node->id);



      printf("LT LF@tmp1 LF@for_step$%d int@0\n", node->id);
      printf("JUMPIFEQ for$%d$NEG LF@tmp1 bool@true\n", node->id);
      if (range->inclusive) {
        printf("GT LF@tmp1 LF@%s$%d LF@for_end$%d\n", node->name, node->id, node->id);
        printf("JUMPIFEQ for$%d$END LF@tmp1 bool@true\n", node->id);
      }
      else {
        printf("LT LF@tmp1 LF@%s$%d LF@for_end$%d\n", node->name, node->id, node->id);
        printf("JUMPIFEQ for$%d$END LF@tmp1 bool@false\n", node->id);
      }
      printf("JUMP for$%d$CHK\n", node->id);
      printf("LABEL for$%d$NEG\n", node->id);
      if (range->inclusive) {
        printf("LT LF@tmp1 LF@%s$%d LF@for_end$%d\n", node->name, node->id, node->id);
        printf("JUMPIFEQ for$%d$END LF@tmp1 bool@true\n", node->id);
      }
      else {
        printf("GT LF@tmp1 LF@%s$%d LF@for_end$%d\n", node->name, node->id, node->id);
        printf("JUMPIFEQ for$%d$END LF@tmp1 bool@false\n", node->id);
      }
      printf("LABEL for$%d$CHK\n", node->id);

      //guards
      for (AST_NODE *guard = node->third; guard != NULL; guard = guard->next) {
        gen_expr(gen, guard);
        printf("PUSHS bool@false\n");
        printf("JUMPIFEQS for$%d$NEXT\n",node->id);
      }
      gen_block(gen, node->right);

      printf("LABEL for$%d$NEXT\n",node->id);
      //i++
      printf("ADD LF@%s$%d LF@%s$%d LF@for_step$%d\n", node->name, node->id, node->name, node->id, node->id);
      printf("JUMP for$%d\n", node->id);

      printf("LABEL for$%d$END\n",node->id);
      break;
    }
    default:
      break;
  }
}

void gen_block(Gen *gen, AST_NODE *fn) {
  for (AST_NODE *node = fn; node != NULL; node = node->next) {
    gen_stmt(gen,node);
  }
}

void gen_block_value(Gen *gen, AST_NODE *stmt, DATA_TYPE result) {
  for (; stmt != NULL; stmt = stmt->next) {
    if (stmt->next == NULL && result != DATA_UNIT &&
        (stmt->type == NODE_EXPR_STMT || stmt->type == NODE_IF)) {
      AST_NODE *expr = stmt->type == NODE_EXPR_STMT ? stmt->left : stmt;
      gen_expr(gen, expr);
      if (result == DATA_DOUBLE && expr->data_type == DATA_INT) {
        printf("INT2FLOATS\n");
      }
      return;
    }
    gen_stmt(gen, stmt);
  }
  printf("PUSHS nil@nil\n");
}
