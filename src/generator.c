/*
* IFJ: Generator
 * Soubor: generator.c
 * Autor: Pavel Novák [xnovakp01]
 */

#include <ast.h>
#include <gen_internal.h>
#include <generator.h>
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


//writes definition of variables in function right after pushFrame
void print_defvar(AST_NODE *node) {
  if (node->type==NODE_VAR_DEF) {
    printf("LF@%s$%d\n", node->name,node->id);
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
  if (node->third) {
    print_defvar(node->third);
    emit_defvars(gen,node->third);
  }

  emit_defvars(gen,node->next);
}

static void generator_recur(Gen *gen, AST_NODE *root);

//pro napojovani funkci pres next
void gen_list(Gen *gen, AST_NODE *node) {
  while (node != NULL) {
    generator_recur(gen,node);
    node = node->next;
  }
}
static void generator_recur(Gen *gen,AST_NODE *root) {
  if (!root)return;
  //TODO
  //kazdy case si deti rozpracuje sam
  switch (root->type) {
    case NODE_NONE:
      break;

    case NODE_PROGRAM:
      /*prolog
      push frame
       */
      break;
    case NODE_FUNC_DEF:
      //counter for multiple while or if
      int counter = 0;

      printf("CREATEFRAME\n");
      printf("PUSHFRAME\n");
      //define variables
      emit_defvars(gen,root);

      //continue on the body of function
      generator_recur(gen,root->left);
      generator_recur(gen,root->right);

      /*
       *createFrame
       pushFrame
       * emit variables
       * left
       * right
       * next
       * popFrame
       * a jestli tam je jeste neco na konec to nevim
       */
      break;

    case NODE_PARAM:
      /*
       *tady budou definice parametru
       *prace s docasnym ramcem

       */
      break;
    case NODE_BLOCK:
      //to netusim proc to tady je
      break;

    case NODE_VAR_DEF:
      /*to je jasne
       *left je kam se to zaradi
       *rigt je hodnota
       */

      break;
    case NODE_ASSIGN:
      break;
    case NODE_IF:
      break;
    case NODE_WHILE:
      break;
    case NODE_RETURN:
      break;
    case NODE_CALL:
      break;
    case NODE_EXPR_STMT:
      break;

    case NODE_BINOP:
      break;
    case NODE_UNOP:
      break;
    case NODE_LITERAL:
      break;

    case NODE_FOR:
      break;
    case NODE_RANGE:
      break;

    case NODE_INTERP:
      break;
    case print:
      break;
    case read_line:
      break;
    case read_int:
      break;
    case read_double:
      break;
    case to_str:
      break;
    case to_int:
      break;
    case str_length:
      break;
    case sub_str:
      break;
    case ord:
      break;
    case chr:
      break;

    default:
      break;
  }
}

/*==========================================================
 *       nova verze - snad spravna
*===========================================================
 */

int new_label(Gen *gen) {
  return gen->label_counter++;
}


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
        putchar(*c);
    }
  }
}

void gen_function(Gen *gen, AST_NODE *fn) {
  printf("LABEL $f_%s\n", fn->name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@tmp\n");
  /*        Program
 *          /  |  \
 *         /   |   \
 *      block
  *     /  |  \
 *     /   |   \
 *   stmt
 */

  //if block is null -> nothing to do
  if (!fn->left)return;

  for (AST_NODE *node = fn->left->left; node != NULL; node = node->next) {
    gen_stmt(gen,node);
  }
  printf("PUSHS nil@nil\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}


void generator_init(Gen *gen, AST_NODE *root) {
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
