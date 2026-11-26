/*
 * IFJ Implimentace prekladace
 * Soubor: scanner.h
 * Autor: Polina Tihaninova [xtishap00]
 */

#ifndef SCANNER_H
#define SCANNER_H
#include "token.h"
#include <stdio.h>

// zavola se funkce na scanner
Token get_token(FILE *f);
// uvolneni
void scanner_free();

#endif