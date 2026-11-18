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
#ifndef GENERATOR_H
#define GENERATOR_H

// TODO prelozit
/* volajici vyhodnoti zleva do prava
 *
 *main function
 */
void generator_init(Gen *gen, AST_NODE *root);

void gen_function(Gen *gen, AST_NODE *fn);

void emit_defvars(Gen *gen, AST_NODE *body);

//statements
void gen_stmt(Gen *gen, AST_NODE *node);

//expresions
void gen_expr(Gen *gen, AST_NODE *node);

#endif