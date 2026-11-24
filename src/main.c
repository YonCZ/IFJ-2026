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
#include "parser/semantic.h"

int main(void) {
    AST_NODE *root = parse_program();
    if (root == NULL) {
        scanner_free();
        return ERROR_INTERNAL;
    }
    
    semantic_analyze(root);

    Gen gen = {0};
    generator_init(&gen, root);

    free_AST(root);
    ast_free_type_refs();
    scanner_free();
    return 0;
}