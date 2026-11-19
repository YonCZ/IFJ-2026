/**
author: lucie alexandrova
login: xalexal00
part of project ifj2026

this file contains symtable function and structural definitions and declarations based on a hash table.
the hashing algorithm used is the djb2 algorithm.
*/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

#define SYMTABLE_BASE_SIZE 100

#define SYMTABLE_MAX_LOAD 0.75

typedef enum {
    SLOT_EMPTY,
    SLOT_OCCUPIED,
    SLOT_DELETED
} symtable_slot_status_t;

typedef struct {
    char * key;
    void * value;
    size_t insert_order; //order in which the item was inserted into the table
    symtable_slot_status_t status;
} symtable_item_t;

typedef struct {
    symtable_item_t * items;
    size_t size; //current table size (number of empty, occupied, or deleted slots)
    size_t count; //number of items in the table (occupied slots)
} symtable_t;

bool symtableInit(symtable_t * table); //initializes the symbol table and its fields
void symtableFree(symtable_t * table); //frees the symbol table and its items
bool symtableExists(symtable_t * table); //checks whether the symbol table exists

//functions for inserting, searching, and deleting items in the symbol table
bool symtableInsert(symtable_t * table, const char * key, void * value);
void * symtableSearch(symtable_t * table, const char * key);
bool symtableDelete(symtable_t * table, const char * key);
size_t symtableFindIndex(symtable_t * table, const char * key);
bool symtableCheckForDuplicate(symtable_t * table, const char * key);
void symtableUpdateValue(symtable_t * table, const char * key, void * new_value);

void symtablePrint(symtable_t * table); //prints the symbol table contents for debugging

#endif /*symtable_h*/
