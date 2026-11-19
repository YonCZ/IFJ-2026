/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026

This file contains semantic analysis functions.
This file was developed with the help of the Gemini Hydrogen AI model.
*/

#include "parser/semantic.h"
#include "parser/parser.h"
#include "symtable/symtable.h"
#include "ast/ast.h"
#include "scanner/error.h"
#include <stdlib.h>
#include <string.h>

void newScope(semantic_context_t *context) {
    scope_node_t *new_scope = calloc(1, sizeof(scope_node_t));
    if (new_scope == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for new scope.");
        return;
    }
    symtableInit(&new_scope->symtable);
    new_scope->scope_id = context->next_scope_id++;
    new_scope->parent = context->current_scope;
    context->current_scope = new_scope;
}

void freeScope(semantic_context_t *context) {
    if (context == NULL || context->current_scope == NULL) {
        return;
    }
    scope_node_t *scope = context->current_scope;
    context->current_scope = scope->parent;
    symtableFree(&scope->symtable);
    free(scope);
}

/* Hledá proměnnou od nejvnitřnějšího bloku (current_scope) směrem ven (shadowing) */
symbol_info_t *findInScope(semantic_context_t *context, const char *name) {
    scope_node_t *scope = context->current_scope;
    while (scope != NULL) {
        if (symtableCheckForDuplicate(&scope->symtable, name)) {
            return (symbol_info_t *)symtableSearch(&scope->symtable, name);
        }
        scope = scope->parent;
    }
    return NULL; // Not found in any scope
}

/* Kontroluje existenci proměnné POUZE v aktuálním (nejvrchnějším) bloku */
bool is_variable_declared(symtable_t *symtable, const char *name) {
    if (symtable == NULL || name == NULL) {
        return false;
    }
    return symtableCheckForDuplicate(symtable, name);
}

/* Kontroluje existenci funkce v globální tabulce symbolů */
bool is_function_declared(symtable_t *symtable, const char *name) {
    if (symtable == NULL || name == NULL) {
        return false;
    }
    return symtableCheckForDuplicate(symtable, name);
}

static symbol_info_t *create_var_symbol(const char *name, DATA_TYPE type, bool is_const, int id) {
    symbol_info_t *info = malloc(sizeof(symbol_info_t));
    if (info == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for variable symbol.");
        return NULL;
    }
    info->kind = SYM_VAR;
    info->name = strdup(name);
    info->data_type = type;
    info->is_const = is_const;
    info->id = id; // true for val and function parameters, false for var
    return info;
}

static symbol_info_t *create_func_symbol(const char *name, DATA_TYPE ret_type, bool explicit_ret, bool is_main, bool is_builtin) {
    symbol_info_t *info = calloc(1, sizeof(symbol_info_t));
    if (info == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for function symbol.");
        return NULL;
    }
    info->kind = SYM_FUNC;
    info->name = strdup(name);
    info->data_type = ret_type;
    info->is_const = false; // Functions are not constants
    info->explicit_return = explicit_ret;
    info->is_main = is_main;
    info->is_builtin = is_builtin;
    return info;
}

static void free_symbol(symbol_info_t *info) {
    if (info != NULL) {
        free(info->name);
        free(info);
    }
}

/* Ověří kompatibilitu typů včetně implicitní konverze Int -> Double */
static bool is_type_compatible(DATA_TYPE target, DATA_TYPE source, bool *needs_int_to_double) {
    if (target == DATA_NONE || source == DATA_NONE) {
        return true;
    }
    if (target == DATA_LIST || source == DATA_LIST ||
        target == DATA_UNION || source == DATA_UNION) {
        return true;
    }
    if (target == source) {
        *needs_int_to_double = false;
        return true;
    }
    if (target == DATA_DOUBLE && source == DATA_INT) {
        *needs_int_to_double = true;
        return true;
    }
    return false;
}

//prvni pruchod AST: posbirat hlavicky vsech funkci, odhalit redefinice (chyba 4) a overit @main (chyby 3, 4, 5, 6)
/* Pomocná funkce pro přiřazení parametrů vestavěným funkcím */
static void set_builtin_params(symbol_info_t *fn, size_t count, const DATA_TYPE *types) {
    fn->param_count = count;
    if (count == 0) {
        fn->params = NULL;
        return;
    }
    fn->params = calloc(count, sizeof(param_info_t));
    if (fn->params == NULL) {
        error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for builtin params.");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        fn->params[i].name = NULL;
        fn->params[i].type = types[i];
    }
}

/* Přednaplní global_symtable vestavěnými funkcemi (print, readInt, subStr, ...) */
static void register_builtin_functions(semantic_context_t *context) {
    symbol_info_t *fn;

    // print(value: Any): Unit -> pro Any použijeme speciální kontrolu v analyze_function_call
    fn = create_func_symbol("print", DATA_UNIT, true, false, true);
    set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_UNIT});
    symtableInsert(&context->global_symtable, "print", fn);

    fn = create_func_symbol("readLine", DATA_STRING, true, false, true);
    symtableInsert(&context->global_symtable, "readLine", fn);

    fn = create_func_symbol("readInt", DATA_INT, true, false, true);
    symtableInsert(&context->global_symtable, "readInt", fn);

    fn = create_func_symbol("readDouble", DATA_DOUBLE, true, false, true);
    symtableInsert(&context->global_symtable, "readDouble", fn);

    // toStr(value: Any): String
    fn = create_func_symbol("toStr", DATA_STRING, true, false, true);
    set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_UNIT});
    symtableInsert(&context->global_symtable, "toStr", fn);

    fn = create_func_symbol("toInt", DATA_INT, true, false, true);
    set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_DOUBLE});
    symtableInsert(&context->global_symtable, "toInt", fn);

    fn = create_func_symbol("strLength", DATA_INT, true, false, true);
    set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_STRING});
    symtableInsert(&context->global_symtable, "strLength", fn);

    fn = create_func_symbol("subStr", DATA_STRING, true, false, true);
    set_builtin_params(fn, 3, (DATA_TYPE[]){DATA_STRING, DATA_INT, DATA_INT});
    symtableInsert(&context->global_symtable, "subStr", fn);

    fn = create_func_symbol("ord", DATA_INT, true, false, true);
    set_builtin_params(fn, 2, (DATA_TYPE[]){DATA_STRING, DATA_INT});
    symtableInsert(&context->global_symtable, "ord", fn);

    fn = create_func_symbol("chr", DATA_STRING, true, false, true);
    set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_INT});
    symtableInsert(&context->global_symtable, "chr", fn);

    fn = create_func_symbol("readBoolean", DATA_BOOL, true, false, true);
    symtableInsert(&context->global_symtable, "readBoolean", fn);

    fn = create_func_symbol("readIntMaybe", DATA_NONE, true, false, true);
    symtableInsert(&context->global_symtable, "readIntMaybe", fn);

    fn = create_func_symbol("readDoubleMaybe", DATA_NONE, true, false, true);
    symtableInsert(&context->global_symtable, "readDoubleMaybe", fn);

    fn = create_func_symbol("List", DATA_LIST, true, false, true);
    symtableInsert(&context->global_symtable, "List", fn);
}

/* 1. průchod: posbírá hlavičky všech funkcí, odhalí redefinice (chyba 4) a ověří @main (chyby 3, 4, 5, 6) */
static void collect_global_functions(AST_NODE *root, semantic_context_t *context) {
    AST_NODE *curr = (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;

    while (curr != NULL) {
        if (curr->type == NODE_CASE_CLASS) {
            symbol_info_t *ctor = create_func_symbol(curr->name, DATA_NONE, true, false, false);
            size_t count = curr->third != NULL ? (size_t)curr->third->param_cnt : 0;
            ctor->param_count = count;
            if (count > 0) {
                ctor->params = calloc(count, sizeof(param_info_t));
                if (ctor->params == NULL) {
                    error_exit(ERROR_INTERNAL, curr->line, curr->name, "Memory allocation failed for params.");
                    return;
                }
                size_t idx = 0;
                for (AST_NODE *p = curr->third->left; p != NULL && idx < count; p = p->next, idx++) {
                    ctor->params[idx].name = strdup(p->name);
                    ctor->params[idx].type = p->data_type;
                    ctor->params[idx].id = p->id;
                }
            }
            symtableInsert(&context->global_symtable, curr->name, ctor);
            curr = curr->next;
            continue;
        }
        if (curr->type == NODE_FUNC_DEF) {
            if (is_function_declared(&context->global_symtable, curr->name)) {
                error_exit(ERROR_SEM_REDEF, curr->line, curr->name, "Function redefinition not allowed.");
                return;
            }

            bool explicit_ret = (curr->type_ref != NULL);
            DATA_TYPE ret_type = explicit_ret ? curr->data_type : DATA_UNIT;
            bool is_main = curr->is_main;

            size_t p_count = 0;
            for (AST_NODE *g = curr->third != NULL ? curr->third->next : NULL;
                 g != NULL; g = g->next) {
                p_count += (size_t)g->param_cnt;
            }

            if (is_main) {
                if (context->has_main) {
                    error_exit(ERROR_SEM_REDEF, curr->line, curr->name, "Multiple @main functions.");
                    return;
                }
                if (p_count > 0) {
                    error_exit(ERROR_SEM_ARG, curr->line, curr->name, "@main function cannot have parameters.");
                    return;
                }
                if (!explicit_ret || ret_type != DATA_UNIT) {
                    error_exit(ERROR_SEM_TYPE, curr->line, curr->name, "@main function must return Unit.");
                    return;
                }
                context->has_main = true;
            }

            symbol_info_t *fn_info = create_func_symbol(curr->name, ret_type, explicit_ret, is_main, false);
            fn_info->param_count = p_count;
            if (p_count > 0) {
                fn_info->params = malloc(sizeof(param_info_t) * p_count);
                if (fn_info->params == NULL) {
                    error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for params.");
                    return;
                }
                size_t idx = 0;
                for (AST_NODE *g = curr->third != NULL ? curr->third->next : NULL;
                     g != NULL; g = g->next) {
                    for (AST_NODE *p = g->left; p != NULL; p = p->next, idx++) {
                        fn_info->params[idx].name = strdup(p->name);
                        fn_info->params[idx].type = p->data_type;
                        fn_info->params[idx].id = p->id;
                    }
                }
            }

            symtableInsert(&context->global_symtable, curr->name, fn_info);
        }
        curr = curr->next;
    }

    if (!context->has_main) {
        error_exit(ERROR_SEM_UNDEF, 0, "main", "Missing @main function.");
    }
}

//druhy pruchod ast, samotna kontrola semantiky, odvozovani typu, kontrola kompatibility, kontrola return, kontrola volani funkci (chyby 3, 4, 5, 6, 7, 10)
static DATA_TYPE analyze_expression(AST_NODE *expr, semantic_context_t *context);
static DATA_TYPE analyze_function_call(AST_NODE *call, semantic_context_t *context);
static DATA_TYPE analyze_statement(AST_NODE *stmt, semantic_context_t *context, bool *guaranteed_return);
static DATA_TYPE analyze_block(AST_NODE *block, semantic_context_t *context, bool create_scope, bool *guaranteed_return);

/* Zkontroluje definici funkce, parametry, tělo a odvodí/ověří návratový typ (chyby 4, 6, 10) */
static void analyze_function_def(AST_NODE *func_node, semantic_context_t *context) {
    if (func_node == NULL || func_node->type != NODE_FUNC_DEF) {
        error_exit(ERROR_INTERNAL, 0, "Invalid function definition node.", "Expected NODE_FUNC_DEF");
        return;
    }

    symbol_info_t *fn_info = (symbol_info_t *)symtableSearch(&context->global_symtable, func_node->name);
    if (fn_info == NULL) {
        error_exit(ERROR_INTERNAL, func_node->line, func_node->name, "Function missing in global table.");
        return;
    }

    context->current_func = fn_info;
    context->in_function = true;
    newScope(context);

    // Vložení formálních parametrů do scope funkce jako nemodifikovatelné (is_const = true)
    for (size_t i = 0; i < fn_info->param_count; i++) {
        if (is_variable_declared(&context->current_scope->symtable, fn_info->params[i].name)) {
            error_exit(ERROR_SEM_REDEF, func_node->line, fn_info->params[i].name, "Duplicate parameter name.");
            return;
        }
        symbol_info_t *param_sym = create_var_symbol(fn_info->params[i].name, fn_info->params[i].type,
                                                     true, fn_info->params[i].id);
        symtableInsert(&context->current_scope->symtable, fn_info->params[i].name, param_sym);
    }

    bool guaranteed_return = false;
    DATA_TYPE body_type = DATA_UNIT;

    if (func_node->left != NULL) {
        body_type = analyze_block(func_node->left, context, false, &guaranteed_return);
    }

    bool body_known = (func_node->left != NULL);

    // Kontrola pravidel návratového typu (sekce 2.3 zadání)
    if (!body_known) {
        fn_info->is_defined = true;
        freeScope(context);
        context->current_func = NULL;
        context->in_function = false;
        return;
    }

    if (!fn_info->explicit_return) {
        if (fn_info->has_return_stmt || fn_info->is_recursive) {
            error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name, "Return type must be explicit for recursive functions or functions using return.");
            return;
        }
        if (body_type == DATA_BOOL) {
            error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name, "Function cannot infer Boolean return type.");
            return;
        }
        fn_info->data_type = body_type; // Odvození návratového typu
    } else if (fn_info->data_type != DATA_UNIT) {
        // Explicitní návratový typ jiný než Unit: pokud není zaručen return, musí sedět typ posledního výrazu
        if (!guaranteed_return) {
            bool needs_conv = false;
            if (!is_type_compatible(fn_info->data_type, body_type, &needs_conv)) {
                error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name, "Function body return type mismatch.");
                return;
            }
        }
    }

    fn_info->is_defined = true;
    freeScope(context);
    context->current_func = NULL;
    context->in_function = false;
}

/* Zpracuje val/var definici proměnné (chyba 4 při redefinici v aktuálním bloku, chyba 6 při neshodě typů) */
static void analyze_var_def(AST_NODE *var_node, semantic_context_t *context) {
    if (is_variable_declared(&context->current_scope->symtable, var_node->name)) {
        error_exit(ERROR_SEM_REDEF, var_node->line, var_node->name, "Variable redefinition in the same scope.");
        return;
    }

    if (var_node->left == NULL) {
        DATA_TYPE declared = var_node->type_ref != NULL ? var_node->data_type : DATA_NONE;
        symbol_info_t *var_sym = create_var_symbol(var_node->name, declared, var_node->is_const,
                                                    var_node->id);
        symtableInsert(&context->current_scope->symtable, var_node->name, var_sym);
        return;
    }

    DATA_TYPE init_type = (var_node->left->type == NODE_CALL)
                          ? analyze_function_call(var_node->left, context)
                          : analyze_expression(var_node->left, context);

    DATA_TYPE final_type = init_type;
    if (var_node->type_ref != NULL && var_node->data_type != DATA_NONE) {
        bool needs_conv = false;
        if (!is_type_compatible(var_node->data_type, init_type, &needs_conv)) {
            error_exit(ERROR_SEM_TYPE, var_node->line, var_node->name, "Variable type mismatch.");
            return;
        }
        final_type = var_node->data_type;
    }

    symbol_info_t *var_sym = create_var_symbol(var_node->name, final_type, var_node->is_const,
                                                var_node->id);
    symtableInsert(&context->current_scope->symtable, var_node->name, var_sym);
}

/* Zpracuje přiřazení id = rhs (chyba 3 nedefinovaná, chyba 7 zápis do val/parametru, chyba 6 typ) */
static void analyze_assign(AST_NODE *assign_node, semantic_context_t *context) {
    if (assign_node == NULL || assign_node->type != NODE_ASSIGN) {
        error_exit(ERROR_INTERNAL, 0, "Invalid assignment node.", "Expected NODE_ASSIGN");
        return;
    }
    if (assign_node->left == NULL || assign_node->left->type != NODE_IDENT) {
        error_exit(ERROR_INTERNAL, 0, "Invalid left-hand side in assignment.", "Expected identifier.");
        return;
    }

    symbol_info_t *var_item = findInScope(context, assign_node->left->name);
    if (var_item == NULL) { // 3
        error_exit(ERROR_SEM_UNDEF, assign_node->line, assign_node->left->name, "Assignment to undefined variable.");
        return;
    }
    if (var_item->is_const) { // 7
        error_exit(ERROR_SEM_READ, assign_node->line, assign_node->left->name, "Assignment to constant variable.");
        return;
    }
    assign_node->left->id = var_item->id;

    if (assign_node->right == NULL) {
        return;
    }
    DATA_TYPE rhs_type = (assign_node->right->type == NODE_CALL)
                         ? analyze_function_call(assign_node->right, context)
                         : analyze_expression(assign_node->right, context);

    bool needs_conv = false;
    if (!is_type_compatible(var_item->data_type, rhs_type, &needs_conv)) { // 6
        error_exit(ERROR_SEM_TYPE, assign_node->line, assign_node->left->name, "Type mismatch in assignment.");
        return;
    }
}

/* Zpracuje if-else (podmínka Boolean, oba bloky nové scopes, pravidla returnu) */
static DATA_TYPE analyze_if(AST_NODE *if_node, semantic_context_t *context, bool *guaranteed_return) {
    DATA_TYPE cond_type = analyze_expression(if_node->left, context);
    if (cond_type != DATA_BOOL && cond_type != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, if_node->line, "if", "Condition in if statement must be Boolean.");
        return DATA_UNIT;
    }

    bool then_ret = false;
    bool else_ret = false;

    DATA_TYPE then_type = analyze_block(if_node->right, context, true, &then_ret);
    DATA_TYPE else_type = analyze_block(if_node->third, context, true, &else_ret);

    if (guaranteed_return != NULL) {
        *guaranteed_return = (then_ret && else_ret);
    }

    if (then_type == else_type) {
        return then_type;
    }
    if (then_type == DATA_NONE || else_type == DATA_NONE) {
        return DATA_NONE;
    }
    return DATA_UNIT;
}

/* Zpracuje while (podmínka Boolean, tělo nový scope, výsledek cyklu je Unit) */
static void analyze_while(AST_NODE *while_node, semantic_context_t *context) {
    DATA_TYPE cond_type = analyze_expression(while_node->left, context);
    if (cond_type != DATA_BOOL && cond_type != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, while_node->line, "while", "Condition in while loop must be Boolean.");
        return;
    }

    bool prev_in_loop = context->in_loop;
    context->in_loop = true;

    bool body_ret = false;
    analyze_block(while_node->right, context, true, &body_ret);

    context->in_loop = prev_in_loop;
}

/* Zpracuje příkaz return (kontrola návratového typu, samostatný return jen pro Unit, chyby 6, 10) */
static void analyze_return(AST_NODE *ret_node, semantic_context_t *context) {
    if (!context->in_function || context->current_func == NULL) {
        error_exit(ERROR_SEM_OTHER, ret_node->line, "return", "Return statement outside of function.");
        return;
    }

    context->current_func->has_return_stmt = true;

    if (!context->current_func->explicit_return) {
        error_exit(ERROR_SEM_TYPE, ret_node->line, context->current_func->name, "Return statement requires explicit function return type.");
        return;
    }

    // Samostatný return bez výrazu
    if (ret_node->left == NULL) {
        if (context->current_func->data_type != DATA_UNIT) {
            error_exit(ERROR_SEM_OTHER, ret_node->line, "return", "Bare return allowed only in explicit Unit functions.");
            return;
        }
        return;
    }

    // Return s výrazem nebo voláním funkce
    DATA_TYPE ret_type = (ret_node->left->type == NODE_CALL)
                         ? analyze_function_call(ret_node->left, context)
                         : analyze_expression(ret_node->left, context);

    // U explicitního Unit se hodnota vyhodnotí a zahodí (pravidlo 4 v sekci 2.3)
    if (context->current_func->data_type == DATA_UNIT) {
        return;
    }

    bool needs_conv = false;
    if (!is_type_compatible(context->current_func->data_type, ret_type, &needs_conv)) {
        error_exit(ERROR_SEM_TYPE, ret_node->line, "return", "Return expression type does not match function signature.");
        return;
    }
}

/* Zpracuje volání funkce jako příkaz nebo výraz na pravé straně (chyby 3, 5) */
static DATA_TYPE analyze_function_call(AST_NODE *call, semantic_context_t *context) {
    if (call->is_method) {
        if (call->left != NULL) {
            analyze_expression(call->left, context);
        }
        for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
            analyze_expression(arg, context);
        }
        DATA_TYPE result = DATA_NONE;
        if (strcmp(call->name, "toString") == 0 || strcmp(call->name, "toStr") == 0) {
            result = DATA_STRING;
        } else if (strcmp(call->name, "length") == 0 || strcmp(call->name, "toInt") == 0) {
            result = DATA_INT;
        } else if (strcmp(call->name, "floor") == 0) {
            result = DATA_DOUBLE;
        }
        call->data_type = result;
        return result;
    }

    symbol_info_t *fn = (symbol_info_t *)symtableSearch(&context->global_symtable, call->name);
    if (fn == NULL || fn->kind != SYM_FUNC) {
        error_exit(ERROR_SEM_UNDEF, call->line, call->name, "Call to undefined function.");
        return DATA_UNIT;
    }

    // Označení přímé rekurze (u nepřímé lze doplnit graf volání nebo kontrolovat explicit_return)
    if (context->current_func != NULL && strcmp(context->current_func->name, fn->name) == 0) {
        context->current_func->is_recursive = true;
    }

    // Argumenty jsou v ast.c uložené v call->third zřetězené přes next
    size_t arg_count = 0;
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
        arg_count++;
    }

    if (strcmp(fn->name, "List") == 0) {
        for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
            analyze_expression(arg, context);
        }
        return DATA_LIST;
    }

    if (arg_count != fn->param_count) {
        error_exit(ERROR_SEM_ARG, call->line, call->name, "Wrong number of arguments in function call.");
        return DATA_UNIT;
    }

    bool is_any_builtin = (fn->is_builtin && (strcmp(fn->name, "print") == 0 || strcmp(fn->name, "toStr") == 0));

    size_t idx = 0;
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next, idx++) {
        DATA_TYPE arg_type = analyze_expression(arg, context);

        if (!is_any_builtin) {
            bool needs_conv = false;
            if (!is_type_compatible(fn->params[idx].type, arg_type, &needs_conv)) {
                error_exit(ERROR_SEM_ARG, arg->line, call->name, "Incompatible argument type.");
                return DATA_UNIT;
            }
        }
    }

    call->data_type = fn->data_type;
    return fn->data_type;
}

/* Rekurzivně vyhodnotí datový typ výrazu a označí implicitní konverzi Int -> Double (chyba 6) */
static DATA_TYPE analyze_expression(AST_NODE *expr, semantic_context_t *context) {
    if (expr == NULL) {
        return DATA_UNIT;
    }

    switch (expr->type) {
        case NODE_LITERAL:
            return expr->data_type;

        case NODE_CALL:
            return analyze_function_call(expr, context);

        case NODE_IF: {
            DATA_TYPE cond_t = analyze_expression(expr->left, context);
            if (cond_t != DATA_BOOL && cond_t != DATA_NONE) {
                error_exit(ERROR_SEM_TYPE, expr->line, "if", "Condition in if expression must be Boolean.");
                return DATA_UNIT;
            }
            bool then_ret = false;
            bool else_ret = false;
            DATA_TYPE then_t = analyze_block(expr->right, context, true, &then_ret);
            DATA_TYPE else_t = analyze_block(expr->third, context, true, &else_ret);
            DATA_TYPE result = (then_t == else_t) ? then_t : DATA_NONE;
            expr->data_type = result;
            return result;
        }

        case NODE_RANGE: {
            if (expr->left != NULL) {
                analyze_expression(expr->left, context);
            }
            if (expr->right != NULL) {
                analyze_expression(expr->right, context);
            }
            if (expr->third != NULL) {
                analyze_expression(expr->third, context);
            }
            expr->data_type = DATA_NONE;
            return DATA_NONE;
        }

        case NODE_INTERP:
            for (AST_NODE *part = expr->left; part != NULL; part = part->next) {
                if (part->type != NODE_LITERAL) {
                    analyze_expression(part, context);
                }
            }
            return DATA_STRING;

        case NODE_IDENT: {
            symbol_info_t *var = findInScope(context, expr->name);
            if (var == NULL || var->kind != SYM_VAR) {
                symbol_info_t *fn = (symbol_info_t *)symtableSearch(
                    &context->global_symtable, expr->name);
                if (fn != NULL && fn->kind == SYM_FUNC) {
                    expr->type = NODE_CALL;
                    expr->third = NULL;
                    expr->is_method = false;
                    expr->has_parens = false;
                    expr->data_type = fn->data_type;
                    return fn->data_type;
                }
                error_exit(ERROR_SEM_UNDEF, expr->line, expr->name, "Use of undefined variable.");
                return DATA_UNIT;
            }
            expr->data_type = var->data_type;
            expr->id = var->id;
            return var->data_type;
        }

        case NODE_UNOP: {
            DATA_TYPE op_type = analyze_expression(expr->left, context);
            if (expr->op_type == OP_NOT) {
                if (op_type != DATA_BOOL && op_type != DATA_NONE) {
                    error_exit(ERROR_SEM_TYPE, expr->line, "unary", "Operator ! requires Boolean.");
                    return DATA_UNIT;
                }
                expr->data_type = DATA_BOOL;
                return DATA_BOOL;
            }
            if (op_type == DATA_NONE) {
                return DATA_NONE;
            }
            if (expr->op_type == OP_ADD || expr->op_type == OP_SUB) {
                if (op_type != DATA_INT && op_type != DATA_DOUBLE) {
                    error_exit(ERROR_SEM_TYPE, expr->line, "unary", "Unary +/- requires Int or Double.");
                    return DATA_UNIT;
                }
                expr->data_type = op_type;
                return op_type;
            }
            return DATA_NONE;
        }

        case NODE_BINOP: {
            DATA_TYPE left_t = analyze_expression(expr->left, context);
            DATA_TYPE right_t = analyze_expression(expr->right, context);

            if (left_t == DATA_NONE || right_t == DATA_NONE) {
                return DATA_NONE;
            }

            switch (expr->op_type) {
                case OP_AND:
                case OP_OR:
                    if ((left_t == DATA_BOOL || left_t == DATA_NONE) &&
                        (right_t == DATA_BOOL || right_t == DATA_NONE)) {
                        expr->data_type = DATA_BOOL;
                        return DATA_BOOL;
                    }
                    error_exit(ERROR_SEM_TYPE, expr->line, "binop", "Logical operator requires Boolean.");
                    return DATA_UNIT;

                case OP_ADD:
                    if (left_t == DATA_LIST && right_t == DATA_LIST) {
                        expr->data_type = DATA_LIST;
                        return DATA_LIST;
                    }
                    if (left_t == DATA_STRING && right_t == DATA_STRING) {
                        expr->data_type = DATA_STRING;
                        return DATA_STRING;
                    }
                    /* fall through */
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                    if ((left_t == DATA_INT || left_t == DATA_DOUBLE) &&
                        (right_t == DATA_INT || right_t == DATA_DOUBLE)) {
                        DATA_TYPE res = (left_t == DATA_DOUBLE || right_t == DATA_DOUBLE) ? DATA_DOUBLE : DATA_INT;
                        expr->data_type = res;
                        return res;
                    }
                    error_exit(ERROR_SEM_TYPE, expr->line, "arith", "Invalid operand types for arithmetic operator.");
                    return DATA_UNIT;

                case OP_TO:
                case OP_UNTIL:
                case OP_BY:
                    if (left_t != DATA_INT && left_t != DATA_LIST) {
                        error_exit(ERROR_SEM_TYPE, expr->line, "range", "Range requires Int or List.");
                        return DATA_UNIT;
                    }
                    if (right_t != DATA_INT && right_t != DATA_LIST) {
                        error_exit(ERROR_SEM_TYPE, expr->line, "range", "Range requires Int or List.");
                        return DATA_UNIT;
                    }
                    expr->data_type = DATA_NONE;
                    return DATA_NONE;

                case OP_LT:
                case OP_LE:
                case OP_GT:
                case OP_GE:
                    if (((left_t == DATA_INT || left_t == DATA_DOUBLE) && (right_t == DATA_INT || right_t == DATA_DOUBLE)) ||
                        (left_t == DATA_STRING && right_t == DATA_STRING)) {
                        expr->data_type = DATA_BOOL;
                        return DATA_BOOL;
                    }
                    error_exit(ERROR_SEM_TYPE, expr->line, "rel", "Invalid operand types for relational operator.");
                    return DATA_UNIT;

                case OP_EQ:
                case OP_NEQ:
                    if (left_t == right_t ||
                        ((left_t == DATA_INT || left_t == DATA_DOUBLE) && (right_t == DATA_INT || right_t == DATA_DOUBLE))) {
                        expr->data_type = DATA_BOOL;
                        return DATA_BOOL;
                    }
                    error_exit(ERROR_SEM_TYPE, expr->line, "eq", "Invalid operand types for equality operator.");
                    return DATA_UNIT;

                default:
                    return DATA_NONE;
            }
        }

        default:
            return DATA_NONE;
    }
}

/* Zpracuje jeden příkaz a vrátí jeho výsledný typ */
static DATA_TYPE analyze_statement(AST_NODE *stmt, semantic_context_t *context, bool *guaranteed_return) {
    if (stmt == NULL) {
        return DATA_UNIT;
    }

    switch (stmt->type) {
        case NODE_VAR_DEF:
            analyze_var_def(stmt, context);
            return DATA_UNIT;

        case NODE_ASSIGN:
            analyze_assign(stmt, context);
            return DATA_UNIT;

        case NODE_IF:
            return analyze_if(stmt, context, guaranteed_return);

        case NODE_WHILE:
            analyze_while(stmt, context);
            return DATA_UNIT;

        case NODE_RETURN:
            analyze_return(stmt, context);
            if (guaranteed_return != NULL) {
                *guaranteed_return = true;
            }
            return DATA_UNIT;

        case NODE_CALL:
            return analyze_function_call(stmt, context);

        case NODE_EXPR_STMT:
            return analyze_expression(stmt->left, context);

        case NODE_FOR: {
            if (stmt->left != NULL) {
                analyze_expression(stmt->left, context);
            }
            newScope(context);
            if (stmt->name != NULL) {
                symbol_info_t *var_sym = create_var_symbol(stmt->name, DATA_INT, false, stmt->id);
                symtableInsert(&context->current_scope->symtable, stmt->name, var_sym);
            }
            for (AST_NODE *guard = stmt->third; guard != NULL; guard = guard->next) {
                analyze_expression(guard, context);
            }
            bool body_ret = false;
            analyze_block(stmt->right, context, false, &body_ret);
            freeScope(context);
            return DATA_UNIT;
        }

        case NODE_LITERAL:
        case NODE_IDENT:
        case NODE_BINOP:
        case NODE_UNOP:
            return analyze_expression(stmt, context);

        default:
            return DATA_UNIT;
    }
}

/* Projde řetěz příkazů v bloku (node->next) a vrátí typ posledního příkazu */
static DATA_TYPE analyze_block(AST_NODE *block, semantic_context_t *context, bool create_scope, bool *guaranteed_return) {
    if (block == NULL) {
        return DATA_UNIT;
    }

    if (create_scope) {
        newScope(context);
    }

    DATA_TYPE last_type = DATA_UNIT;
    AST_NODE *stmt = (block->type == NODE_BLOCK) ? block->left : block;

    while (stmt != NULL) {
        bool stmt_returns = false;
        last_type = analyze_statement(stmt, context, &stmt_returns);
        if (stmt_returns && guaranteed_return != NULL) {
            *guaranteed_return = true;
        }
        stmt = stmt->next;
    }

    if (create_scope) {
        freeScope(context);
    }

    return last_type;
}

void semantic_analyze(AST_NODE *root) {
    semantic_context_t context;
    memset(&context, 0, sizeof(semantic_context_t));

    if (!symtableInit(&context.global_symtable)) {
        return;
    }

    register_builtin_functions(&context);
    collect_global_functions(root, &context);

    // 2. průchod: kontrola těl všech funkcí
    AST_NODE *curr = (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
    while (curr != NULL) {
        if (curr->type == NODE_FUNC_DEF) {
            analyze_function_def(curr, &context);
        }
        curr = curr->next;
    }
    

    // Úklid globální tabulky symbolů
    size_t size_of_global_symtable = context.global_symtable.size;
    for (size_t i = 0; i < size_of_global_symtable; i++) {
        if (context.global_symtable.items[i].status == SLOT_OCCUPIED) {
            symbol_info_t *info = (symbol_info_t *)context.global_symtable.items[i].value;
            if (info != NULL) {
                for (size_t p = 0; p < info->param_count; p++) {
                    free(info->params[p].name);
                }
                free(info->params);
                free_symbol(info);
            }
        }
    }
    symtableFree(&context.global_symtable);
}