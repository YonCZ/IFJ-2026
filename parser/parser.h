/*
 * IFJ Implimentace prekladace
 * Soubor: parser.h
 * Autor: Tadeas Prikryl [xprikrt00]
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast/ast.h"
#include "scanner/token.h"

Token parser_peek(void);
Token parser_advance(void);

// Parses the input from stdin, returns the AST root.
// Exits with the error code through error_exit on failure.
// Supported extensions: BOOLTHEN, PARAMLISTS, VALMEM, EXTKW,
// FUNEXP, CONTROL, INTERPOLATEDS.
AST_NODE *parse_program(void);

#endif
