/*
 * IFJ26 Compiler Implementation
 *
 * File: main.c
 * Authors:
 *   Polina Tishaninova (xtishap00) - Leader
 *   Tadeáš Přikryl (xprikrt00)
 *   Pavel Novák (xnovakp01)
 *   Lucie Alexandrová (xalexal00)
 */

#include <stdio.h>

#include "parser/parser.h"
#include "generator/generator.h"

int main(void) {
    AST_NODE *root = parse_program();
    Gen gen = {0};
    generate_program(&gen, root);
    free_AST(root);
    return 0;
}
