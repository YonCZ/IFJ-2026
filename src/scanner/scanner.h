/*
 * IFJ26 Compiler
 * File: scanner.h
 * Author: Polina Tihaninova [xtishap00]
 */

#ifndef SCANNER_H
#define SCANNER_H
#include "token.h"
#include <stdio.h>

// calls the scanner function to get the next token
Token get_token(FILE *f);
// frees the memory used by the scanner
void scanner_free();

#endif
