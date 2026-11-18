/*
 * IFJ26 Compiler Implementation
 *
 * File: generator.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */

#include "ast.h"
#include "gen_internal.h"
#include "generator.h"
#include <stdio.h>
#include <stdlib.h>

// budu potřebovat konst. pro instukce (move, add ...) ale i pro operatory binop
// atd...
/*
 * frame instructions
 */
void cg_move() {}
void cg_create_frame(){}
void cg_push_frame(){}
void cg_pop_frame(){}
void cg_def_var(){}
void cd_call(){}
void cg_return(){}

/*
 * stack insturctions
 */
void cg_pushs(){}
void cg_pops(){}
void cg_clears(){}

/*
 *Arithemtic, relational, boolean conversion instructions
 */
//frame ins.
void cg_add(){}
void cg_sub(){}
void cg_mul(){}
void cg_div(){}
//stack ins.
void cg_idiv(){}
void cg_adds(){}
void cg_subs(){}
void cg_muls(){}
void cg_divs(){}
void cg_idivs(){}
//relation ins.
void cg_lt(){}
void cg_gt(){}
void cg_eq(){}
//stack realtion ins.
void cg_lts(){}
void cg_gts(){}
void cg_eqs(){}
//logic ins.
void cg_and(){}
void cg_or(){}
void cg_not(){}
//stack logic ins.
void cg_ands(){}
void cg_ors(){}
void cg_nots(){}
//conversion ins.
void cg_int_to_float(){}
void cg_float_to_int(){}
void cg_int_to_char(){}
void cg_str_to_int(){}
void cg_float_to_str(){}
void cg_int_to_str(){}
//stack conversion ins.
void cg_int_to_foat_s(){}
void cg_float_to_int_s(){}
void cg_int_to_char_s(){}
void cg_str_to_int_s(){}
void float_to_str_s(){}
void int_to_str_s(){}

/*
 *I/O ins.
 */
void cg_read(){}
void cg_write(){}

/*
 *strings ins.
 */
void cg_concat(){}
void cg_strlen(){ }
void cg_getchar(){}
void cg_setchar(){}

/*
 *field ins.
 */
void cg_new_array(){}
void cg_array_get(){}
void cg_array_set(){}

/*
 *type ins.
 */
void cg_type(){}
void cg_isint(){}
//stack type ins.
void cg_types(){}
void cg_isints(){}

/*
 *control flow ins.
 */
void cg_label(){}
void cg_jump(){}
void cg_jumg_if_eg(){}
void cg_jump_if_neg(){}
//stakc control flow
void cg_jump_if_eq_s(){}
void cg_jump_if_neq_s(){}
void cg_exit(){}

/*
 *debugging ins.
 */
void cg_break(){}
void cg_dprint(){}





/*==========================================================
 *       nova verze - snad spravna
*===========================================================
 */

//writes definition of variables in function right after pushFrame
void print_defvar(AST_NODE *node) {
  if (node->type==NODE_VAR_DEF) {
    printf("DEFVAR LF@%s$%d\n", node->name,node->id);
  }
}
void emit_defvars(Gen *gen, AST_NODE *node) {
  if (!node)return;
  if (node->left != NULL) {
    print_defvar(node->left);
    emit_defvars(gen,node->left);
  }
  if (node->right != NULL) {
    print_defvar(node->right);
    emit_defvars(gen,node->right);
  }
  if (node->third != NULL) {
    print_defvar(node->third);
    emit_defvars(gen,node->third);
  }
  if (node->next!=NULL) {
    print_defvar(node->next);
    emit_defvars(gen,node->next);
  }
}


int new_label(Gen *gen) {
  return gen->label_counter++;
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

void gen_function(Gen *gen, AST_NODE *fn) {
  printf("LABEL $f_%s\n", fn->name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");


  /*        Program
 *          /  |  \
 *         /   |   \
 *      func
 */


  //print all variables
  emit_defvars(gen, fn);

  //mezi promenne cz
  printf("DEFVAR LF@tmp1\n");
  printf("DEFVAR LF@tmp2\n");
  printf("DEFVAR LF@tmp3\n");

  for (AST_NODE *node = fn->left; node != NULL; node = node->next) {
    gen_stmt(gen,node);
  }


  //return
  printf("PUSHS nil@nil\n");

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

  //emits all functions
  n = root->left;
  while (n) {
    gen_function(gen,n);
    n = n->next;
  }
}
