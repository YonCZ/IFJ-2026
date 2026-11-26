/*
 * IFJ Implementace prekladace
 * Soubor: main.c
 * Autor: Polina Tihaninova [xtishap00]
 */

#include "ast.h"
#include "error.h"
#include "generator.h"
#include "parser.h"
#include "semantic.h"
#include "scanner.h"

// nacte vstup, spusti scanner, parser, semantiku a generator
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
