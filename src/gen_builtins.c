/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_builtins.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"

void lower_members(AST_NODE *fn); /*gen_valmem.c */

enum {
  B_READLINE,
  B_READINT,
  B_READDOUBLE,
  B_TOSTR,
  B_TOINT,
  B_STRLENGTH,
  B_SUBSTR,
  B_ORD,
  B_CHR,
  B_READBOOLEAN,
  B_FLOOR,
  B_READINTMAYBE,
  B_READDOUBLEMAYBE,
  B_COUNT
};

static const char *const BUILTIN_NAME[B_COUNT] = {
    "readLine",       "readInt", "readDouble",
    "toStr",          "toInt",   "strLength",
    "subStr",         "ord",     "chr",
    "readBoolean",    "$floor",  "readIntMaybe",
    "readDoubleMaybe"};

static bool used[B_COUNT];
static bool used_tostr_int, used_tostr_float, used_tostr_bool;

//finds index in enum based on name
static int builtin_index(const char *name) {
  for (int i = 0; i < B_COUNT; i++)
    if (strcmp(BUILTIN_NAME[i], name) == 0)
      return i;
  return -1;
}

bool is_builtin(const char *name) { return builtin_index(name) >= 0; }

//goes through the AST and finds builtin function
static void scan(AST_NODE *node) {
  if (!node)
    return;
  if (node->type == NODE_CALL && node->name) {
    int idx = builtin_index(node->name);
    if (idx >= 0) {
      used[idx] = true;
      if (idx == B_TOSTR && node->third) {
        switch (node->third->data_type) {
        case DATA_INT:
          used_tostr_int = true;
          break;
        case DATA_DOUBLE:
          used_tostr_float = true;
          break;
        case DATA_BOOL:
          used_tostr_bool = true;
          break;
        default:
          break;
        }
      }
    }
  }

  if (node->type == NODE_INTERP) {
    for (AST_NODE *part = node->left; part; part = part->next) {
      switch (part->data_type) {
      case DATA_INT:
        used_tostr_int = true;
        break;
      case DATA_DOUBLE:
        used_tostr_float = true;
        break;
      case DATA_BOOL:
        used_tostr_bool = true;
        break;
      default:
        break;
      }
    }
  }
  scan(node->left);
  scan(node->right);
  scan(node->third);
  scan(node->arg_groups);
  scan(node->next);
}

//print builtins call
void emit_builtin_call(Gen *gen, AST_NODE *call) {
  (void)gen;
  if (strcmp(call->name, "toStr") == 0) {
    if (call->third != NULL && call->third->data_type == DATA_LIST) {
      puts("CALL $b_list_toStr");
      return;
    }
    if (call->third != NULL && call->third->data_type == DATA_UNION) {
      puts("CALL $b_toStr_any");
      return;
    }
    switch (call->third->data_type) {
    case DATA_INT:
      puts("CALL $b_toStr_int");
      break;
    case DATA_DOUBLE:
      puts("CALL $b_toStr_float");
      break;
    case DATA_BOOL:
      puts("CALL $b_toStr_bool");
      break;
    case DATA_UNIT:
      puts("POPS LF@tmp1");
      puts("PUSHS string@()");
      break;
    default:
      break;
    }
    return;
  }
  printf("CALL $b_%s\n", call->name);
}

static void def_read(const char *name, const char *type,
                     const char *default_value) {
  printf("LABEL $b_%s\n", name);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  printf("READ LF@val %s\n", type);
  printf("JUMPIFNEQ $b_%s$ok LF@val nil@nil\n", name);
  printf("MOVE LF@val %s\n", default_value);
  printf("LABEL $b_%s$ok\n", name);
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_readmaybe(const char *name, const char *type) {
  printf("LABEL $b_%s\n", name);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  printf("READ LF@val %s\n", type);
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_readboolean(void) {
  puts("LABEL $b_readBoolean");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("READ LF@val bool");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_unary(const char *label, const char *instruction) {
  printf("LABEL $b_%s\n", label);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@arg");
  puts("DEFVAR LF@res");
  puts("POPS LF@arg");
  printf("%s LF@res LF@arg\n", instruction);
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_floor(void) {
  puts("LABEL $b_$floor");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@x");
  puts("DEFVAR LF@t");
  puts("DEFVAR LF@f");
  puts("DEFVAR LF@c");
  puts("POPS LF@x");
  puts("FLOAT2INT LF@t LF@x");
  puts("INT2FLOAT LF@f LF@t");
  puts("GT LF@c LF@f LF@x");
  puts("JUMPIFEQ $b_$floor$ok LF@c bool@false");
  puts("SUB LF@f LF@f float@0x1p+0");
  puts("LABEL $b_$floor$ok");
  puts("PUSHS LF@f");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_tostr_bool(void) {
  puts("LABEL $b_toStr_bool");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@arg");
  puts("DEFVAR LF@res");
  puts("POPS LF@arg");
  puts("MOVE LF@res string@false");
  puts("JUMPIFEQ $b_toStr_bool$end LF@arg bool@false");
  puts("MOVE LF@res string@true");
  puts("LABEL $b_toStr_bool$end");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

//finds code of char on index
static void def_ord(void) {
  puts("LABEL $b_ord");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@res");
  puts("POPS LF@idx");
  puts("POPS LF@str");
  puts("STRLEN LF@len LF@str");
  puts("LT LF@cond LF@idx int@0");
  puts("JUMPIFEQ $b_ord$bad LF@cond bool@true");
  puts("LT LF@cond LF@idx LF@len");
  puts("JUMPIFEQ $b_ord$ok LF@cond bool@true");
  puts("LABEL $b_ord$bad");
  puts("PUSHS int@-1");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_ord$ok");
  puts("STRI2INT LF@res LF@str LF@idx");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

static void def_substr(void) {
  puts("LABEL $b_subStr");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@start");
  puts("DEFVAR LF@end");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@res");
  puts("DEFVAR LF@ch");
  puts("DEFVAR LF@i");
  puts("POPS LF@end");
  puts("POPS LF@start");
  puts("POPS LF@str");
  puts("STRLEN LF@len LF@str");
  puts("LT LF@cond LF@start int@0");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  puts("LT LF@cond LF@end int@0");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  puts("GT LF@cond LF@start LF@end");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  puts("LT LF@cond LF@start LF@len");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@false");
  puts("GT LF@cond LF@end LF@len");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  puts("MOVE LF@res string@");
  puts("MOVE LF@i LF@start");
  puts("LABEL $b_subStr$loop");
  puts("LT LF@cond LF@i LF@end");
  puts("JUMPIFEQ $b_subStr$done LF@cond bool@false");
  puts("GETCHAR LF@ch LF@str LF@i");
  puts("CONCAT LF@res LF@res LF@ch");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_subStr$loop");
  puts("LABEL $b_subStr$done");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_subStr$err");
  puts("EXIT int@40");
}

//helper functions for lists
static void def_list_helpers(void) {
  puts("LABEL $b_list_get");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@idx");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@idx int@0");
  puts("JUMPIFEQ $b_list_get$err LF@cond bool@true");
  puts("LT LF@cond LF@idx LF@len");
  puts("JUMPIFEQ $b_list_get$err LF@cond bool@false");
  puts("ADD LF@idx LF@idx int@2");
  puts("ARRAYGET LF@val LF@list LF@idx");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_list_get$err");
  puts("EXIT int@40");

  puts("LABEL $b_list_head");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@val");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("JUMPIFNEQ $b_list_head$ok LF@len int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_head$ok");
  puts("ARRAYGET LF@val LF@list int@2");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_length");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("PUSHS LF@len");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_toList");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("POPS LF@list");
  puts("PUSHS LF@list");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_tail");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("JUMPIFNEQ $b_list_tail$ok LF@len int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_tail$ok");
  puts("SUB LF@newlen LF@len int@1");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_tail$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_tail$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@3");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_tail$loop");
  puts("LABEL $b_list_tail$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_drop");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@n");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@n");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@n int@0");
  puts("JUMPIFEQ $b_list_drop$zero LF@cond bool@false");
  puts("MOVE LF@n int@0");
  puts("LABEL $b_list_drop$zero");
  puts("GT LF@cond LF@n LF@len");
  puts("JUMPIFEQ $b_list_drop$clamped LF@cond bool@false");
  puts("MOVE LF@n LF@len");
  puts("LABEL $b_list_drop$clamped");
  puts("SUB LF@newlen LF@len LF@n");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_drop$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_drop$done LF@cond bool@false");
  puts("ADD LF@j LF@i LF@n");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_drop$loop");
  puts("LABEL $b_list_drop$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_take");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@n");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@n");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@n int@0");
  puts("JUMPIFEQ $b_list_take$zero LF@cond bool@false");
  puts("MOVE LF@n int@0");
  puts("LABEL $b_list_take$zero");
  puts("GT LF@cond LF@n LF@len");
  puts("JUMPIFEQ $b_list_take$clamped LF@cond bool@false");
  puts("MOVE LF@n LF@len");
  puts("LABEL $b_list_take$clamped");
  puts("ADD LF@size LF@n int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@n");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_take$loop");
  puts("LT LF@cond LF@i LF@n");
  puts("JUMPIFEQ $b_list_take$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_take$loop");
  puts("LABEL $b_list_take$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_slice");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@from");
  puts("DEFVAR LF@until");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@until");
  puts("POPS LF@from");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@from int@0");
  puts("JUMPIFEQ $b_list_slice$from_zero LF@cond bool@false");
  puts("MOVE LF@from int@0");
  puts("LABEL $b_list_slice$from_zero");
  puts("GT LF@cond LF@from LF@len");
  puts("JUMPIFEQ $b_list_slice$from_max LF@cond bool@false");
  puts("MOVE LF@from LF@len");
  puts("LABEL $b_list_slice$from_max");
  puts("LT LF@cond LF@until int@0");
  puts("JUMPIFEQ $b_list_slice$until_zero LF@cond bool@false");
  puts("MOVE LF@until int@0");
  puts("LABEL $b_list_slice$until_zero");
  puts("GT LF@cond LF@until LF@len");
  puts("JUMPIFEQ $b_list_slice$until_max LF@cond bool@false");
  puts("MOVE LF@until LF@len");
  puts("LABEL $b_list_slice$until_max");
  puts("LT LF@cond LF@until LF@from");
  puts("JUMPIFEQ $b_list_slice$ordered LF@cond bool@false");
  puts("MOVE LF@until LF@from");
  puts("LABEL $b_list_slice$ordered");
  puts("SUB LF@newlen LF@until LF@from");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_slice$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_slice$done LF@cond bool@false");
  puts("ADD LF@j LF@i LF@from");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_slice$loop");
  puts("LABEL $b_list_slice$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_concat");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@a");
  puts("DEFVAR LF@b");
  puts("DEFVAR LF@la");
  puts("DEFVAR LF@lb");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@b");
  puts("POPS LF@a");
  puts("ARRAYGET LF@la LF@a int@1");
  puts("ARRAYGET LF@lb LF@b int@1");
  puts("ADD LF@size LF@la LF@lb");
  puts("ADD LF@size LF@size int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@a int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ADD LF@size LF@la LF@lb");
  puts("ARRAYSET LF@arr int@1 LF@size");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_concat$la");
  puts("LT LF@cond LF@i LF@la");
  puts("JUMPIFEQ $b_list_concat$lb_start LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@a LF@j");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_concat$la");
  puts("LABEL $b_list_concat$lb_start");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_concat$lb");
  puts("LT LF@cond LF@i LF@lb");
  puts("JUMPIFEQ $b_list_concat$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@b LF@j");
  puts("ADD LF@j LF@i LF@la");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_concat$lb");
  puts("LABEL $b_list_concat$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_range");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@start");
  puts("DEFVAR LF@end");
  puts("DEFVAR LF@step");
  puts("DEFVAR LF@inclusive");
  puts("DEFVAR LF@end_eff");
  puts("DEFVAR LF@count");
  puts("DEFVAR LF@num");
  puts("DEFVAR LF@diff");
  puts("DEFVAR LF@absstep");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@cur");
  puts("DEFVAR LF@cond");
  puts("POPS LF@inclusive");
  puts("POPS LF@step");
  puts("POPS LF@end");
  puts("POPS LF@start");
  puts("JUMPIFNEQ $b_list_range$step_ok LF@step int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_range$step_ok");
  puts("MOVE LF@end_eff LF@end");
  puts("GT LF@cond LF@step int@0");
  puts("JUMPIFEQ $b_list_range$positive LF@cond bool@true");
  puts("SUB LF@absstep int@0 LF@step");
  puts("JUMPIFEQ $b_list_range$neg_inc LF@inclusive int@0");
  puts("SUB LF@end_eff LF@end int@1");
  puts("LABEL $b_list_range$neg_inc");
  puts("GT LF@cond LF@start LF@end_eff");
  puts("JUMPIFEQ $b_list_range$empty LF@cond bool@false");
  puts("SUB LF@diff LF@start LF@end_eff");
  puts("ADD LF@num LF@diff LF@absstep");
  puts("SUB LF@num LF@num int@1");
  puts("IDIV LF@count LF@num LF@absstep");
  puts("JUMP $b_list_range$fill");
  puts("LABEL $b_list_range$positive");
  puts("JUMPIFEQ $b_list_range$pos_inc LF@inclusive int@0");
  puts("ADD LF@end_eff LF@end int@1");
  puts("LABEL $b_list_range$pos_inc");
  puts("LT LF@cond LF@start LF@end_eff");
  puts("JUMPIFEQ $b_list_range$empty LF@cond bool@false");
  puts("SUB LF@diff LF@end_eff LF@start");
  puts("ADD LF@num LF@diff LF@step");
  puts("SUB LF@num LF@num int@1");
  puts("IDIV LF@count LF@num LF@step");
  puts("JUMP $b_list_range$fill");
  puts("LABEL $b_list_range$empty");
  puts("MOVE LF@count int@0");
  puts("LABEL $b_list_range$fill");
  puts("ADD LF@size LF@count int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYSET LF@arr int@0 string@$list:Int");
  puts("ARRAYSET LF@arr int@1 LF@count");
  puts("MOVE LF@cur LF@start");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_range$loop");
  puts("LT LF@cond LF@i LF@count");
  puts("JUMPIFEQ $b_list_range$done LF@cond bool@false");
  puts("ADD LF@idx LF@i int@2");
  puts("ARRAYSET LF@arr LF@idx LF@cur");
  puts("ADD LF@cur LF@cur LF@step");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_range$loop");
  puts("LABEL $b_list_range$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_toStr_any");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@t");
  puts("DEFVAR LF@res");
  puts("POPS LF@val");
  puts("TYPE LF@t LF@val");
  puts("JUMPIFEQ $b_toStr_any$int LF@t string@int");
  puts("JUMPIFEQ $b_toStr_any$float LF@t string@float");
  puts("JUMPIFEQ $b_toStr_any$bool LF@t string@bool");
  puts("JUMPIFEQ $b_toStr_any$string LF@t string@string");
  puts("JUMPIFEQ $b_toStr_any$nil LF@t string@nil");
  puts("JUMPIFEQ $b_toStr_any$array LF@t string@array");
  puts("MOVE LF@res string@?");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$int");
  puts("INT2STR LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$float");
  puts("FLOAT2STR LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$bool");
  puts("MOVE LF@res string@false");
  puts("JUMPIFEQ $b_toStr_any$done LF@val bool@false");
  puts("MOVE LF@res string@true");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$string");
  puts("MOVE LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$nil");
  puts("MOVE LF@res string@()");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$array");
  puts("PUSHS LF@val");
  puts("CALL $b_list_toStr");
  puts("POPS LF@res");
  puts("LABEL $b_toStr_any$done");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_toStr");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@res");
  puts("DEFVAR LF@elem");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@idx");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("MOVE LF@res string@List(");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_toStr$loop");
  puts("LT LF@cond LF@i LF@len");
  puts("JUMPIFEQ $b_list_toStr$done LF@cond bool@false");
  puts("JUMPIFEQ $b_list_toStr$nosep LF@i int@0");
  puts("CONCAT LF@res LF@res string@,\\032");
  puts("LABEL $b_list_toStr$nosep");
  puts("ADD LF@idx LF@i int@2");
  puts("ARRAYGET LF@elem LF@list LF@idx");
  puts("PUSHS LF@elem");
  puts("CALL $b_toStr_any");
  puts("POPS LF@str");
  puts("CONCAT LF@res LF@res LF@str");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_toStr$loop");
  puts("LABEL $b_list_toStr$done");
  puts("CONCAT LF@res LF@res string@)");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_print_any");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@t");
  puts("POPS LF@val");
  puts("TYPE LF@t LF@val");
  puts("JUMPIFEQ $b_print_any$nil LF@t string@nil");
  puts("JUMPIFEQ $b_print_any$array LF@t string@array");
  puts("WRITE LF@val");
  puts("JUMP $b_print_any$done");
  puts("LABEL $b_print_any$nil");
  puts("WRITE string@()");
  puts("JUMP $b_print_any$done");
  puts("LABEL $b_print_any$array");
  puts("PUSHS LF@val");
  puts("CALL $b_list_print");
  puts("LABEL $b_print_any$done");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_print");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@text");
  puts("CALL $b_list_toStr");
  puts("POPS LF@text");
  puts("WRITE LF@text");
  puts("POPFRAME");
  puts("RETURN");
}

//writet defvar for builtins function
void emit_builtins(Gen *gen, AST_NODE *root) {
  (void)gen;
  memset(used, 0, sizeof used);
  used_tostr_int = used_tostr_float = used_tostr_bool = false;
  lower_members(root);
  scan(root);

  if (used[B_READLINE])
    def_read("readLine", "string", "string@");
  if (used[B_READINT])
    def_read("readInt", "int", "int@0");
  if (used[B_READBOOLEAN])
    def_readboolean();
  if (used[B_READDOUBLE])
    def_read("readDouble", "float", "float@0x0p+0");
  if (used[B_READINTMAYBE])
    def_readmaybe("readIntMaybe", "int");
  if (used[B_READDOUBLEMAYBE])
    def_readmaybe("readDoubleMaybe", "float");
  if (used_tostr_int)
    def_unary("toStr_int", "INT2STR");
  if (used_tostr_float)
    def_unary("toStr_float", "FLOAT2STR");
  if (used_tostr_bool)
    def_tostr_bool();
  if (used[B_TOINT])
    def_unary("toInt", "FLOAT2INT");
  if (used[B_FLOOR])
    def_floor();
  if (used[B_STRLENGTH])
    def_unary("strLength", "STRLEN");
  if (used[B_CHR])
    def_unary("chr", "INT2CHAR");
  if (used[B_ORD])
    def_ord();
  if (used[B_SUBSTR])
    def_substr();
  def_list_helpers();
}
