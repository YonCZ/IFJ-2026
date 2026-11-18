/*
* IFJ: Generator
 * Soubor: gen_internal.h
 * Autor: Pavel Novák [xnovakp01]
 */

#pragma once
#include "ast/ast.h"

typedef struct {
  int label_counter;
  char *function_name;
}Gen;




/*
 *create from function name and label counter
 */
int new_label(Gen *gen);


void emit(Gen *gen, char *label);


void emit_string_literal(Gen *gem, char *str);
