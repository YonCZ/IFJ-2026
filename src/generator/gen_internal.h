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
} Gen;

/*
 *create from function name and label counter
 */
int new_label(Gen *gen);

void emit(Gen *gen, char *label);

void emit_string_literal(char *str);

void gen_block_value(Gen *gen, AST_NODE *stmt, DATA_TYPE result);

void push_to_val(AST_NODE *node);
void pop_to_val(AST_NODE *node);

/*=======
 *gen built ins
 *=============
 */
bool is_builtin(const char *name);
void emit_builtin_call(Gen *gen, AST_NODE *call);
void emit_builtins(Gen *gen, AST_NODE *root);