/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026

This file contains symtable function and structural definitions and declarations based on a hash table.
The hashing algorithm used is the djb2 algorithm.
*/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

#define SYMTABLE_MAX_LOAD 0.75

typedef enum {
    SLOT_EMPTY,
    SLOT_OCCUPIED,
    SLOT_DELETED
} symtable_slot_status_t;

typedef struct {
    char * key;
    void * value;
    size_t insert_order; //the order in which the item was inserted into the table
    symtable_slot_status_t status;
} symtable_item_t;

typedef struct {
    symtable_item_t * items;
    size_t size; //current size of the table (number of slots empty, occupied, or deleted)
    size_t count; //number of items in the table (occupied slots)
} symtable_t;

bool symtableInit(symtable_t * table); //starting function, allocates memory for the symbol table and initializes its fields
void symtableFree(symtable_t * table); //ending function, frees the memory allocated for the symbol table and its items
bool symtableExists(symtable_t * table); //checks if the symtable even exists

//functions for inserting, searching, and deleting items in the symbol table
bool symtableInsert(symtable_t * table, const char * key, void * value);
void * symtableSearch(symtable_t * table, const char * key);
bool symtableDelete(symtable_t * table, const char * key);
size_t symtableFindIndex(symtable_t * table, const char * key);
bool symtableCheckForDuplicate(symtable_t * table, const char * key);
void * symtableGetOrInsert(symtable_t * table, const char * key, void * value);
void symtableUpdateValue(symtable_t * table, const char * key, void * new_value);
size_t symtableSize(symtable_t * table); //returns the number of items in the symbol table

void symtablePrint(symtable_t * table); //prints the contents of the symbol table for debugging

#endif /* SYMTABLE_H */
