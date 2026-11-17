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
    size_t index_in_table;
    symtable_slot_status_t status;
} symtable_item_t;

typedef struct {
    symtable_item_t * items;
    size_t size; //current size of the table (number of slots empty, occupied, or deleted)
    size_t count; //number of items in the table (occupied slots)
} symtable_t;

bool symtable_init(symtable_t * table); //starting function, allocates memory for the symbol table and initializes its fields
void symtable_free(symtable_t * table); //ending function, frees the memory allocated for the symbol table and its items
bool symtable_exists(symtable_t * table); //checks if the symtable even exists

//functions for inserting, searching, and deleting items in the symbol table
bool symtable_insert(symtable_t * table, const char * key, void * value, size_t insert_order);
void * symtable_search(symtable_t * table, const char * key);
bool symtable_delete(symtable_t * table, const char * key);
void * symtable_find_index(symtable_t * table, const char * key);
bool symtable_check_for_duplicate(symtable_t * table, const char * key);
void * symtable_get_or_insert(symtable_t * table, const char * key, void * value, size_t insert_order);
void symtable_update_value(symtable_t * table, const char * key, void * new_value);
size_t symtable_size(symtable_t * table); //returns the number of items in the symbol table

void symtable_print(symtable_t * table); //prints the contents of the symbol table for debugging

#endif /* SYMTABLE_H */
