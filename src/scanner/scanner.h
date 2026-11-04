/*
 * IFJ Implimentace prekladace
 * Soubor: scanner.h
 * Autor: Polina Tihaninova [xtishap00]
 */

#ifndef SCANNER_H
#define SCANNER_H
#include <stdio.h>
#include "token.h"

//zavola se funkce na scanner
Token get_token(FILE *f);

#endif