/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_internal.h
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */

#pragma once
#include "ast.h"
#include <stdbool.h>

typedef struct {
  int label_counter;
  char *function_name;
  DATA_TYPE return_type;
}Gen;




/*
 *create from function name and label counter
 */
int new_label(Gen *gen);


void emit_string_literal(Gen *gem, char *str);

void lower_members(AST_NODE *root);

void push_to_val(AST_NODE *node);
void pop_to_val(AST_NODE *node);

/*=======
 *gen built ins
 *=============
 */
bool is_builtin(const char *name);
void emit_builtin_call(Gen *gen, AST_NODE *call);
void emit_builtins(Gen *gen, AST_NODE *root);