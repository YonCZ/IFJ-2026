/**
IFJ26 Compiler 
Author: Lucie Alexandrova [xalexal00]
File: precedence.c

This file contains symtable function and structural definitions and declarations
based on a hash table. The hashing algorithm used is the djb2 algorithm.
*/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

#define SYMTABLE_MAX_LOAD 0.75

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED } symtable_slot_status_t;

typedef struct {
  char *key;
  void *value;
  symtable_slot_status_t status;
} symtable_item_t;

typedef struct {
  symtable_item_t *items;
  // current size of the table (number of slots empty, occupied, or deleted)
  size_t size;
  // number of items in the table (occupied slots)
  size_t count;
} symtable_t;

// starting function, allocates memory for the symbol table and initializes its
// fields
bool symtableInit(symtable_t *table);
// ending function, frees the memory allocated for the symbol table and its
// items
void symtableFree(symtable_t *table);
// checks if the symtable even exists
bool symtableExists(symtable_t *table);

// functions for inserting and searching items in the symbol table
bool symtableInsert(symtable_t *table, const char *key, void *value);
void *symtableSearch(symtable_t *table, const char *key);
size_t symtableFindIndex(symtable_t *table, const char *key);
bool symtableCheckForDuplicate(symtable_t *table, const char *key);

#endif /* SYMTABLE_H */
