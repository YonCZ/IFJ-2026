/*
* IFJ: Generator
 * Soubor: generator.h
 * Autor: Pavel Novák [xnovakp01]
 */


#include <ast.h>
#include <gen_internal.h>
#ifndef GENERATOR_H
#define GENERATOR_H

// TODO prelozit
/* volajici vyhodnoti zleva do prava
 *

/*
 *main function
 */
void generate_program(Gen *gen);

void gen_function(Gen *gen, AST_NODE *fn);

void emit_defvars(Gen *gen, AST_NODE *body);

#endif