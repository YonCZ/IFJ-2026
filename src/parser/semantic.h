/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026
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
    DATA_TYPE elem_type;
    int id;
} param_info_t;

/* Záznam ukládaný do void *value ve tvé symtable_t */
typedef struct {
    symbol_kind_t kind;
    char *name;
    DATA_TYPE data_type;      // U proměnné její typ; u funkce návratový typ
    DATA_TYPE elem_type;      // Typ prvku, pokud je proměnná seznam
    bool is_const;            // true pro val a parametry funkce, false pro var

    /* Položky pro funkce (kind == SYM_FUNC) */
    bool is_main;             // true pro @main
    bool is_defined;          // true po kontrole těla funkce
    bool explicit_return;     // true, pokud byl návratový typ uveden v hlavičce
    AST_NODE *visited_from;
    bool is_builtin;          // true pro vestavěné funkce
    bool is_extension;        // true pro metodu z extension bloku
    AST_NODE *fn_node;        // uzel definice pro odlozene odvozeni typu
    bool analyzing;           // probiha odvozovani typu
    int id;                   // unikátní id proměnné pro generátor
    param_info_t *params;     // Pole parametrů
    size_t param_count;       // Počet parametrů
    size_t group_count;       // Počet skupin parametrů
    size_t *group_counts;     // Počet parametrů v jednotlivých skupinách
} symbol_info_t;

/* Položka zásobníku lokálních rozsahů platnosti (pro shadowing a bloky) */
typedef struct scope_node {
    symtable_t symtable;
    int scope_id;
    struct scope_node *parent;
} scope_node_t;

/* Kontext sémantické analýzy */
typedef struct {
    symtable_t global_symtable; // Globální tabulka funkcí
    scope_node_t *current_scope; // Vrchol zásobníku lokálních bloků (místo jedné symtable*)
    symbol_info_t *current_func; // Právě analyzovaná funkce
    bool in_function;
    bool in_loop;
    int next_scope_id;
    bool has_main;
} semantic_context_t;

/* Hlavní vstupní bod volaný z main.c nad kořenem AST */
void semantic_analyze(AST_NODE *root);

/* Správa rozsahů platnosti (scopes) */
void newScope(semantic_context_t *context);
void freeScope(semantic_context_t *context);

/* Vyhledávání a kontrola deklarací */
symbol_info_t *findInScope(semantic_context_t *context, const char *name);
bool is_variable_declared(symtable_t *symtable, const char *name);
bool is_function_declared(symtable_t *symtable, const char *name);

#endif // SEMANTIC_H