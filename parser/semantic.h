/**
author: lucie alexandrova
login: xalexal00
part of project ifj2026
*/
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include "symtable/symtable.h"
#include "ast/ast.h"

typedef enum {
    SYM_VAR,
    SYM_FUNC
} symbol_kind_t;

typedef struct {
    char *name;
    DATA_TYPE type;
    int id;
} param_info_t;

/*record stored in void *value in symtable_t*/
typedef struct {
    symbol_kind_t kind;
    char *name;
    DATA_TYPE data_type;      //variable type or function return type
    bool is_const;            //true for val and function parameters, false for var

    /*function fields (kind == SYM_FUNC)*/
    bool is_main;             //true for @main
    bool is_defined;          //true after checking the function body
    bool explicit_return;     //true when the return type was given in the header
    bool is_recursive;        //true when the function calls itself
    bool has_return_stmt;     //true when the body contains a return statement
    bool is_builtin;          //true for built-in functions
    int id;                   //unique variable id for the generator
    param_info_t *params;     //parameter array
    size_t param_count;       //number of parameters
} symbol_info_t;

/*stack item for local scopes (for shadowing and blocks)*/
typedef struct scope_node {
    symtable_t symtable;
    int scope_id;
    struct scope_node *parent;
} scope_node_t;

/*semantic analysis context*/
typedef struct {
    symtable_t global_symtable; //global function table
    scope_node_t *current_scope; //top of the local block stack
    symbol_info_t *current_func; //currently analyzed function
    bool in_function;
    bool in_loop;
    int next_scope_id;
    bool has_main;
} semantic_context_t;

/*main entry point called from main.c on the AST root*/
void semantic_analyze(AST_NODE *root);

/*scope management*/
void newScope(semantic_context_t *context);
void freeScope(semantic_context_t *context);

/*declaration lookup and checks*/
symbol_info_t *findInScope(semantic_context_t *context, const char *name);
bool is_variable_declared(symtable_t *symtable, const char *name);
bool is_function_declared(symtable_t *symtable, const char *name);

#endif //semantic_h