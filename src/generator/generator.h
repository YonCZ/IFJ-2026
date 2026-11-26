/*
 * IFJ26 Compiler 
 * File: generator.c
 * Author: Pavel Novák [xnovakp01]
 */

#include "ast.h"
#include "gen_internal.h"
#ifndef GENERATOR_H
#define GENERATOR_H

void generator_init(Gen *gen, AST_NODE *root);

void gen_function(Gen *gen, AST_NODE *fn);

void emit_defvars(AST_NODE *body);

// statements
void gen_stmt(Gen *gen, AST_NODE *node);

// expresions
void gen_expr(Gen *gen, AST_NODE *node);

void gen_block(Gen *gen, AST_NODE *node);

#endif
