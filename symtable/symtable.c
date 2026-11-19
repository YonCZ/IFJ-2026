/**
author: lucie alexandrova
login: xalexal00
part of project ifj2026
*/

#include "symtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "scanner/error.h"

//function declarations

static size_t symtableHash(const char * key, size_t table_size){
    size_t hash = 5381;
    size_t i = 0;
    while (key[i] != '\0') {
        hash = hash * 33 + key[i];
        i++;
    }
    return hash % table_size;
}

bool symtableInit(symtable_t * table){
    if (symtableExists(table)) {
        return true; //already initialized
    }
    table->size = SYMTABLE_BASE_SIZE;
    table->count = 0;
    table->items = (symtable_item_t *)calloc(table->size, sizeof(symtable_item_t));
    if (table->items == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for symbol table.", "Memory allocation unsuccessful");
        return false; //allocation failed
    }
    for (size_t i = 0; i < table->size; i++) {
        table->items[i].status = SLOT_EMPTY;
    }
    return true; //allocation of empty table successful
}

void symtableFree(symtable_t * table){
    if (symtableExists(table) == false) {
        return; //table does not exist
    }

    if (table==NULL || table->items == NULL) {
        return; //table is already empty
    }
    //table is not empty
    for (size_t i = 0; i < table->size; i++) { 
        if (table->items[i].status == SLOT_OCCUPIED) {
            free(table->items[i].key); //only empties the table; the value is only a pointer
        }
    }
    free(table->items);
    table->items = NULL;
    table->size = 0;
    table->count = 0;
}

static bool symtableResize(symtable_t * table){
    if (symtableExists(table) == false) {
        return false; //table does not exist
    }
    size_t new_size = table->size * 2 + 1;
    symtable_item_t * new_items = (symtable_item_t *)calloc(new_size, sizeof(symtable_item_t));
    if (new_items == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for symbol table resize.", "Memory allocation unsuccessful");
        return false; //allocation failed
    }
    //if successful, copy the old items to the new table
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

bool symtableExists(symtable_t * table){
    return table != NULL && table->items != NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//functions for inserting, searching, and deleting items in the symbol table
void * symtableSearch(symtable_t * table, const char * key) {
    size_t index = symtableFindIndex(table, key);
    if (index == (size_t)-1) {
        return NULL;
    }
    return table->items[index].value;
}

bool symtableCheckForDuplicate(symtable_t * table, const char * key){
    if (symtableExists(table) == false) {
        return false; //table does not exist, so there are no duplicates
    }
    return symtableFindIndex(table, key) != (size_t)-1;
}

bool symtableInsert(symtable_t * table, const char * key, void * value){
    if (symtableExists(table) == false) {
        return false; //table does not exist
    }
    if (symtableCheckForDuplicate(table, key)) {
        return false; //key already exists
    }
    //key does not exist, so it can be inserted
    size_t index = symtableHash(key, table->size);
    while (table->items[index].status == SLOT_OCCUPIED) {
        index = (index + 1) % table->size; //searches until it finds an empty slot
    }
    table->items[index].key = (char *)malloc(strlen(key) + 1); //allocates memory for the key and '\0'
    if (table->items[index].key == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for symbol table key.", "Memory allocation unsuccessful");
        return false; //allocation failed
    }
    strcpy(table->items[index].key, key); //inserts the key into the table
    table->items[index].value = value;
    table->items[index].status = SLOT_OCCUPIED;
    table->count++;
    table->items[index].insert_order = table->count; //the order in which the item was inserted into the table

    //checks whether the table needs to be resized
    if (table->count >= SYMTABLE_MAX_LOAD * table->size) {
        if (!symtableResize(table)) {
            return false; //resize failed
        }
    }
    return true;
}


size_t symtableFindIndex(symtable_t * table, const char * key){
    if (!symtableExists(table) || key == NULL) return (size_t)-1;
    size_t start = symtableHash(key, table->size);
    size_t i = start;
    do {
        if (table->items[i].status == SLOT_EMPTY) {
            return (size_t)-1; //an empty slot means the key is not in the table
        }
        if (table->items[i].status == SLOT_OCCUPIED && strcmp(table->items[i].key, key) == 0) {
            return i; //the item is found at it's hash index
        }
        i = (i + 1) % table->size; //loop through the entire table
    } while (i != start);
    return (size_t)-1; //the key was not found in the table
}

bool symtableDelete(symtable_t * table, const char * key){
    size_t index = symtableFindIndex(table, key);
    if (index != (size_t)-1) { //if the index is not error (-1), delete the item
            free(table->items[index].key);
            table->items[index].status = SLOT_DELETED;
            table->count--;
            return true; //successfully deleted the item
        }
    return false; //the key was not found in the table
    }

void symtableUpdateValue(symtable_t * table, const char * key, void * new_value){
    size_t i = symtableFindIndex(table, key);
    if (i != (size_t)-1) {
        table->items[i].value = new_value;
    }
}

void symtablePrint(symtable_t * table){
    if (symtableExists(table) == false) {
        printf("Symbol table does not exist.\n");
        return;
    }
    printf("Symbol Table Size: %zu, Count: %zu\n, Table load: %.2f\n", table->size, table->count, (double)table->count / table->size);
    printf("Symbol Table Contents:\n");
    for (size_t i = 0; i < table->size; i++) {
        if (table->items[i].status == SLOT_OCCUPIED) {
            printf("Index: %zu, Key: %s, Value: %p, Insert Order: %zu\n", i, table->items[i].key, table->items[i].value, table->items[i].insert_order);
        }
    }
} //prints the symbol table contents for debugging