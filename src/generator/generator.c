/*
 * IFJ26 Compiler Implementation
 *
 * File: generator.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */

#include "generator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"

/*==========================================================
 *       nova verze - snad spravna
 *===========================================================
 */


//valmem
void lower_members(AST_NODE *root) {

  for (AST_NODE *fn = root; fn; fn = fn->next) {
    if (fn->left)lower_members(fn->left);
    if (fn->right)lower_members(fn->right);
    if (fn->third)lower_members(fn->third);


    if (fn->type == NODE_CALL && fn->is_method == true && fn->third == NULL && fn->left) {
      if (strcmp(fn->name, "toString") == 0) {
        free(fn->name);
        ast_name(fn, "toStr");
        fn->third = fn->left;
        fn->left = NULL;
        fn->is_method = false;
        fn->has_parens = true;
      }
      else if (strcmp(fn->name,"length")==0) {
        free(fn->name);
        ast_name(fn, "strLength");
        fn->third = fn->left;
        fn->left = NULL;
        fn->is_method = false;
        fn->has_parens = true;
      }
      else if (strcmp(fn->name,"floor")==0) {
        free(fn->name);
        ast_name(fn, "$floor");
        fn->third = fn->left;
        fn->left = NULL;
        fn->is_method = false;
        fn->has_parens = true;
      }
      else if (strcmp(fn->name,"toInt")==0) {
        fn->third = fn->left;
        fn->left = NULL;
        fn->is_method = false;
        fn->has_parens = true;
      }
    }

  }
}


int new_label(Gen *gen) {
  return gen->label_counter++;
}

//writes definition of variables in function right after pushFrame
void print_defvar(AST_NODE *node, Gen *gen) {
  if (node->type==NODE_VAR_DEF) {
    printf("DEFVAR LF@%s$%d\n", node->name,node->id);
  }
  if (node->type==NODE_FOR) {
    printf("DEFVAR LF@%s$%d\n", node->name,node->id);
    printf("DEFVAR LF@for_end$%d\n",node->id);
    printf("DEFVAR LF@for_step$%d\n",node->id);
  }
}
void emit_defvars(Gen *gen, AST_NODE *node) {
  if (!node)return;
  if (node->left != NULL) {
    print_defvar(node->left,gen);
    emit_defvars(gen,node->left);
  }
  if (node->right != NULL) {
    print_defvar(node->right, gen);
    emit_defvars(gen,node->right);
  }
  if (node->third != NULL) {
    print_defvar(node->third, gen);
    emit_defvars(gen,node->third);
  }
  if (node->next!=NULL) {
    print_defvar(node->next, gen);
    emit_defvars(gen,node->next);
  }
}




/*
 *special characters
 */
void emit_string_literal(Gen *gen, char *str) {
  printf("string@");
  for (char *c = str; *c; c++) {
    switch (*c) {
      case 32:
        printf("\\032");
        break;
      case 10:
        printf("\\010");
        break;
      case 9:
        printf("\\009");
        break;
      case 35:
        printf("\\035");
        break;
      case 92:
        printf("\\092");
        break;
      default:
        if (*c<32)printf("\\%03d", *c);
        else putchar(*c);
    }
  }
}

static void defvar_params(AST_NODE *groups) {
  for (AST_NODE *g = groups; g != NULL; g = g->next) {
    for (AST_NODE *p = g->left; p != NULL; p = p->next) {
      printf("DEFVAR LF@%s$%d\n", p->name, p->id);
    }
  }
}

static void pop_param_chain(AST_NODE *param) {
  if (param == NULL) {
    return;
  }
  pop_param_chain(param->next);
  printf("POPS LF@%s$%d\n", param->name, param->id);
}

static void pop_params(AST_NODE *groups) {
  if (groups == NULL) {
    return;
  }
  pop_params(groups->next);
  pop_param_chain(groups->left);
}

static void process_params(AST_NODE *groups) {
  if (groups == NULL) {
    return;
  }
  defvar_params(groups->next);
  pop_params(groups->next);
}

void gen_function(Gen *gen, AST_NODE *fn) {
  gen->return_type = fn->data_type;
  printf("LABEL $f_%s\n", fn->name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");


  /*        Program
 *          /  |  \
 *         /   |   \
 *      func
 */


  //print all variables of this function
  if (fn->left != NULL) {
    print_defvar(fn->left, gen);
  }
  emit_defvars(gen, fn->left);

  //parametry - muze jich byt nekonecno? rekurzivni funkce vyrvarejici LF@tmp
  //=============================

  //mezi promenne cz
  printf("DEFVAR LF@tmp1\n");
  printf("DEFVAR LF@tmp2\n");
  printf("DEFVAR LF@tmp3\n");

  process_params(fn->third);


  //processing block of code
  //TODO zkontrolovat jestli tady bude moct byt left - je to uz druhe left (1. je v program init)
  //TODO znamenalo by to program -> body -> function ale to prece nenamem
  for (AST_NODE *node = fn->left; node != NULL; node = node->next) {
    if (node->next==NULL && fn->data_type != DATA_UNIT && node->type == NODE_EXPR_STMT) {
      gen_expr(gen, node->left);
    }
    else if (node->next == NULL && fn->data_type != DATA_UNIT && node->type == NODE_IF) {
      gen_expr(gen,node);
    }
    else {
      gen_stmt(gen, node);
    }
  }

  if (fn->data_type == DATA_UNIT) printf("PUSHS nil@nil\n");


  printf("POPFRAME\n");
  printf("RETURN\n");
}


void generator_init(Gen *gen, AST_NODE *root) {
  //==================
  //ast_print(root);
  //====================*/
  printf(".IFJcode26\n");

  //finding main function
  AST_NODE *n = root->left;
  while (n) {
    if (n->is_main) printf("CALL $f_%s\n", n->name);
    n = n->next;
  }
  printf("EXIT int@0\n");


  //built ins
  emit_builtins(gen, root);

  //emits all functions
  n = root->left;
  while (n) {
    gen_function(gen,n);
    n = n->next;
  }
}
