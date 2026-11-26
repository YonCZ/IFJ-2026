/*
 * IFJ Implimentace prekladace
 * Soubor: parser.h
 * Autor: Tadeas Prikryl [xprikrt00]
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "token.h"

Token parser_peek(void);
Token parser_peek_next(void);
Token parser_advance(void);
AST_NODE *parser_parse_interp(void);
AST_NODE *parser_parse_if_expr(bool in_args);

// Parses the input from stdin, returns the AST root.
// Exits with the error code through error_exit on failure.
// Supported extensions: BOOLTHEN, PARAMLISTS, VALMEM, EXTKW,
// FUNEXP, CONTROL, INTERPOLATEDS.
AST_NODE *parse_program(void);

#endif
