/*
 * IFJ Implementace prekladace
 * Soubor: main.c
 * Autor: Polina Tihaninova [xtishap00]
 */

#include "error.h"
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "generator.h"

int main(void) {
    AST_NODE *root = parse_program();
    if (root == NULL) {
        scanner_free();
        return ERROR_INTERNAL;
    }
    
    Gen gen = {0};
    generator_init(&gen, root);

    free_AST(root);
    scanner_free();
    return 0;
}