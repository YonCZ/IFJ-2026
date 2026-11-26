/*
 * IFJ26 Compiler
 * File: dynamic_string.h
 * Author: Polina Tishaninova [xtishap00]
 */

#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H
#include <stddef.h>

typedef struct {
  // data
  char *data;
  // length
  size_t len;
  // capacity
  size_t cap;
} DString;

// the functions are described in dynamic_string
void ds_init(DString *s);
void ds_free(DString *s);
void ds_clear(DString *s);
void ds_push_char(DString *s, char c);
char *ds_dup(const DString *s);

#endif
