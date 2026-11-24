/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026
*/
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtable.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum { SYM_VAR, SYM_FUNC, SYM_TYPE, SYM_CLASS } symbol_kind_t;

typedef struct {
  char *name;
  DATA_TYPE type;
  DATA_TYPE elem_type;
  AST_NODE *type_ref;
  int id;
} param_info_t;

/* record stored in void *value in symtable_t */
typedef struct {
  symbol_kind_t kind;
  char *name;
  DATA_TYPE data_type; // variable type or function return type
  DATA_TYPE elem_type; // element type if the variable is a list
  AST_NODE *type_ref;  // full type (list, union, class)
  bool is_const;       // true for val and function parameters, false for var

  /* function fields (kind == SYM_FUNC) */
  bool is_main;         // true for @main
  bool is_defined;      // true after checking the function body
  bool explicit_return; // true when the return type was given in the header
  AST_NODE *visited_from;
  bool is_builtin;      // true for built-in functions
  bool is_extension;    // true for a method from an extension block
  AST_NODE *fn_node;    // definition node for deferred type inference
  bool analyzing;       // type inference is in progress
  int id;               // unique variable id for the generator
  param_info_t *params; // parameter array
  size_t param_count;   // number of parameters
  size_t group_count;   // number of parameter groups
  size_t *group_counts; // parameters per group
} symbol_info_t;

/* stack item for local scopes (for shadowing and blocks) */
typedef struct scope_node {
  symtable_t symtable;
  struct scope_node *parent;
} scope_node_t;

/* semantic analysis context */
typedef struct {
  symtable_t global_symtable; // global function table
  // top of the local block stack (instead of a single symtable*)
  scope_node_t *current_scope;
  symbol_info_t *current_func; // currently analyzed function
  bool in_function;
  bool in_loop;
  bool has_main;
  AST_NODE *expected_type;
} semantic_context_t;

/* main entry point called from main.c on the AST root */
void semantic_analyze(AST_NODE *root);

/* scope management */
void newScope(semantic_context_t *context);
void freeScope(semantic_context_t *context);

/* declaration lookup and checks */
symbol_info_t *findInScope(semantic_context_t *context, const char *name);
bool is_variable_declared(symtable_t *symtable, const char *name);
bool is_function_declared(symtable_t *symtable, const char *name);

#endif // SEMANTIC_H
