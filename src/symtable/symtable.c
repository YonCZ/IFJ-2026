/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026
*/

#include "symtable.h"
#include "scanner/error.h"
#include <stdlib.h>
#include <string.h>

#define SYMTABLE_BASE_SIZE 100

// function declarations

static size_t symtableHash(const char *key, size_t table_size) {
  size_t hash = 5381;
  size_t i = 0;
  while (key[i] != '\0') {
    hash = hash * 33 + key[i];
    i++;
  }
  return hash % table_size;
}

bool symtableInit(symtable_t *table) {
  if (symtableExists(table)) {
    return true; // already initialized
  }
  table->size = SYMTABLE_BASE_SIZE;
  table->count = 0;
  table->items =
      (symtable_item_t *)calloc(table->size, sizeof(symtable_item_t));
  if (table->items == NULL) {
    error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for symbol table.",
               "Memory allocation unsuccessful");
    return false; // allocation failed
  }
  for (size_t i = 0; i < table->size; i++) {
    table->items[i].status = SLOT_EMPTY;
  }
  return true; // allocation of empty table successful
}

void symtableFree(symtable_t *table) {
  if (symtableExists(table) == false) {
    return; // table does not exist
  }

  if (table == NULL || table->items == NULL) {
    return; // table is already empty
  }
  // not empty
  for (size_t i = 0; i < table->size; i++) {
    if (table->items[i].status == SLOT_OCCUPIED) {
      free(table->items[i]
               .key); // only empties the table, does not get rid of the value
                      // completely, because it is only a pointer
    }
  }
  free(table->items);
  table->items = NULL;
  table->size = 0;
  table->count = 0;
}

static bool symtableResize(symtable_t *table) {
  if (symtableExists(table) == false) {
    return false; // table does not exist
  }
  size_t new_size = table->size * 2 + 1;
  symtable_item_t *new_items =
      (symtable_item_t *)calloc(new_size, sizeof(symtable_item_t));
  if (new_items == NULL) {
    error_exit(ERROR_INTERNAL, 0,
               "Memory allocation failed for symbol table resize.",
               "Memory allocation unsuccessful");
    return false; // allocation failed
  }
  // if succesful, copy the old items to the new table
  for (size_t i = 0; i < table->size; i++) {
    if (table->items[i].status == SLOT_OCCUPIED) {
      size_t new_index = symtableHash(table->items[i].key, new_size);
      while (new_items[new_index].status != SLOT_EMPTY) {
        new_index = (new_index + 1) % new_size;
      }
      new_items[new_index] = table->items[i];
    }
  }
  free(table->items);
  table->items = new_items;
  table->size = new_size;
  return true;
}

bool symtableExists(symtable_t *table) {
  return table != NULL && table->items != NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// functions for inserting and searching items in the symbol table
void *symtableSearch(symtable_t *table, const char *key) {
  size_t index = symtableFindIndex(table, key);
  if (index == (size_t)-1) {
    return NULL;
  }
  return table->items[index].value;
}

bool symtableCheckForDuplicate(symtable_t *table, const char *key) {
  if (symtableExists(table) == false) {
    return false; // table does not exist, so no duplicates
  }
  return symtableFindIndex(table, key) != (size_t)-1;
}

bool symtableInsert(symtable_t *table, const char *key, void *value) {
  if (symtableExists(table) == false) {
    return false; // table does not exist
  }
  if (symtableCheckForDuplicate(table, key)) {
    return false; // key already exists
  }
  // key does not exist, so we can insert it
  size_t index = symtableHash(key, table->size);
  while (table->items[index].status == SLOT_OCCUPIED) {
    index = (index + 1) %
            table->size; // goes through the table until it finds an empty slot
  }
  table->items[index].key =
      (char *)malloc(strlen(key) + 1); // allocates memory for the key + '\0'
  if (table->items[index].key == NULL) {
    error_exit(ERROR_INTERNAL, 0,
               "Memory allocation failed for symbol table key.",
               "Memory allocation unsuccessful");
    return false; // allocation failed
  }
  strcpy(table->items[index].key, key); // inserts the key into the table
  table->items[index].value = value;
  table->items[index].status = SLOT_OCCUPIED;
  table->count++;
  table->items[index].insert_order =
      table->count; // the order in which the item was inserted into the table

  // check if the table needs to be resized
  if (table->count >= SYMTABLE_MAX_LOAD * table->size) {
    if (!symtableResize(table)) {
      return false; // resize failed
    }
  }
  return true;
}

size_t symtableFindIndex(symtable_t *table, const char *key) {
  if (!symtableExists(table) || key == NULL)
    return (size_t)-1;
  size_t start = symtableHash(key, table->size);
  size_t i = start;
  do {
    if (table->items[i].status == SLOT_EMPTY) {
      return (size_t)-1; // narazili jsme na prazdny slot -> klic v tabulce neni
    }
    if (table->items[i].status == SLOT_OCCUPIED &&
        strcmp(table->items[i].key, key) == 0) {
      return i;
    }
    i = (i + 1) % table->size;
  } while (i != start);
  return (size_t)-1; // did not find the key in the table
}
