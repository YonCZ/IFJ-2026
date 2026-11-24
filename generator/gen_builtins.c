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


//all builtin functions
enum {
  B_READLINE, B_READINT, B_READDOUBLE, B_TOSTR, B_TOINT, B_STRLENGTH, B_SUBSTR, B_ORD, B_CHR,
  B_READBOOLEAN,
  B_FLOOR,
  B_COUNT
};

static const char *const BUILTIN_NAME[B_COUNT] = {"readLine", "readInt", "readDouble", "toStr", "toInt",
                                                  "strLength", "subStr",  "ord",        "chr",
                                                  "readBoolean", "$floor"};

static bool used[B_COUNT];
static bool used_tostr_int, used_tostr_float, used_tostr_bool;


static int builtin_index(const char *name) {
  for (int i = 0; i < B_COUNT; i++)
    if (strcmp(BUILTIN_NAME[i], name) == 0) return i;
  return -1;
}

bool is_builtin(const char *name) {
  return builtin_index(name) >= 0;
}

//which  to_str builtins are called
static void scan(AST_NODE *node) {
  if (!node) return;
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
  scan(node->next);
}


//calling fuctions
void builtin_call(Gen *gen, AST_NODE *call) {
  (void)gen;
  if (strcmp(call->name, "toStr") == 0) {
    switch (call->third->data_type) {
      case DATA_INT:
        printf("CALL $b_toStr_int\n");
        break;
      case DATA_DOUBLE:
        printf("CALL $b_toStr_float\n");
        break;
      case DATA_BOOL:
        printf("CALL $b_toStr_bool\n");
        break;
      default:
        break;
    }
    
    return;
  }
  printf("CALL $b_%s\n", call->name);
}


//AI controlled code
static void def_read(const char *name, const char *type, const char *default_value) {
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

//writes definitions, if they exists
void emit_builtins(Gen *gen, AST_NODE *root) {
  (void)gen;
  memset(used, 0, sizeof used);
  used_tostr_int = used_tostr_float = used_tostr_bool = false;
  refactoring(root);
  scan(root);

  if (used[B_READLINE]) def_read("readLine", "string", "string@");
  if (used[B_READINT]) def_read("readInt", "int", "int@0");
  if (used[B_READBOOLEAN]) def_readboolean();
  if (used[B_READDOUBLE]) def_read("readDouble", "float", "float@0x0p+0");
  if (used_tostr_int) def_unary("toStr_int", "INT2STR");
  if (used_tostr_float) def_unary("toStr_float", "FLOAT2STR");
  if (used_tostr_bool) def_tostr_bool();
  if (used[B_TOINT]) def_unary("toInt", "FLOAT2INT");
  if (used[B_FLOOR]) def_floor();
  if (used[B_STRLENGTH]) def_unary("strLength", "STRLEN");
  if (used[B_CHR]) def_unary("chr", "INT2CHAR");
  if (used[B_ORD]) def_ord();
  if (used[B_SUBSTR]) def_substr();
}
