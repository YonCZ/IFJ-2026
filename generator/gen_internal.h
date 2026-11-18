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

typedef struct {
  int label_counter;
  char *function_name;
  DATA_TYPE return_type;
}Gen;




/*
 *create from function name and label counter
 */
int new_label(Gen *gen);


void emit(Gen *gen, char *label);


void emit_string_literal(Gen *gem, char *str);


void push_to_val(AST_NODE *node);
void pop_to_val(AST_NODE *node);