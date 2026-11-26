/*
 * IFJ26 Compiler
 * File: dynamic_string.h
 * Author: Polina Tihaninova [xtishap00]
 */

#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H
#include <stddef.h>

typedef struct {
  // data
  char *data;
  // delka
  size_t len;
  // kapacita
  size_t cap;
} DString;

// funkce jsou popsane v dynamic_string
void ds_init(DString *s);
void ds_free(DString *s);
void ds_clear(DString *s);
void ds_push_char(DString *s, char c);
char *ds_dup(const DString *s);

#endif
