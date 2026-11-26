/*
 * IFJ26 Compiler
 * File: dynamic_string.c
 * Author: Polina Tishaninova [xtishap00]
 */

#include "dynamic_string.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

// function that initializes the string, sets capacity to 32 and length to 0,
// allocates memory with malloc, and if it fails, it is internal error 99
void ds_init(DString *s) {
  // capacity 32
  s->cap = 32;
  // length 0
  s->len = 0;
  s->data = malloc(s->cap);
  if (!s->data) {
    error_exit(ERROR_INTERNAL, 0, "Allocation failed");
  }
  s->data[0] = '\0';
}

// function that frees the data
void ds_free(DString *s) {
  // if there is data, we can free it
  if (s->data) {
    free(s->data);
    // set the pointer to NULL
    s->data = NULL;
  }
  // length and capacity are set to zero
  s->len = 0;
  s->cap = 0;
}

// function that only clears the string, but keeps the memory
void ds_clear(DString *s) {
  s->len = 0;
  if (s->data) {
    // put the terminating character at the first place
    s->data[0] = '\0';
  }
}

// function that adds a char to the end
void ds_push_char(DString *s, char c) {
  // if there is not enough space, we must make the capacity bigger
  if (s->len + 1 >= s->cap) {
    s->cap *= 2;
    // make the memory bigger
    char *new_data = realloc(s->data, s->cap);
    // check realloc, otherwise internal error
    if (!new_data) {
      error_exit(ERROR_INTERNAL, 0, "Reallocation failed");
    }
    s->data = new_data;
  }
  // add the character, increase the length and move the terminating character
  s->data[s->len++] = c;
  s->data[s->len] = '\0';
}

// function that makes a new copy of the string
char *ds_dup(const DString *s) {
  // allocate new space
  char *dup = malloc(s->len + 1);
  // check malloc, otherwise internal error 99
  if (!dup) {
    error_exit(ERROR_INTERNAL, 0, "Dup failed - allocation error");
  }
  // copy the memory to the new place
  memcpy(dup, s->data, s->len + 1);
  return dup;
}
