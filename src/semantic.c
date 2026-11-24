/**
Author: Lucie Alexandrova
Login: xalexal00
Part of project IFJ2026

This file contains semantic analysis functions.
This file was developed with the help of the Gemini Hydrogen AI model.
*/

#include "semantic.h"
#include "ast.h"
#include "parser.h"
#include "error.h"
#include "symtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// copies a string, exits on allocation failure
static char *dup_string(const char *src) {
  size_t len = strlen(src) + 1;
  char *copy = malloc(len);
  if (copy == NULL) {
    error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for string.");
  }
  memcpy(copy, src, len);
  return copy;
}

/* initializes a new scope and pushes it onto the scope stack */
void newScope(semantic_context_t *context) {
  // each scope owns a separate symbol table and points to the scope that
  // encloses it
  scope_node_t *new_scope = calloc(1, sizeof(scope_node_t));
  if (new_scope == NULL) {
    error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for new scope.");
    return;
  }
  symtableInit(&new_scope->symtable);
  new_scope->parent = context->current_scope;
  context->current_scope = new_scope;
}

static void free_symbol(symbol_info_t *info);

/* frees the current scope and pops it from the scope stack */
void freeScope(semantic_context_t *context) {
  if (context == NULL || context->current_scope == NULL) {
    return;
  }
  scope_node_t *scope = context->current_scope;
  // restore the parent before releasing the current scope and its symbols
  context->current_scope = scope->parent;
  for (size_t i = 0; i < scope->symtable.size; i++) {
    if (scope->symtable.items[i].status == SLOT_OCCUPIED) {
      free_symbol(scope->symtable.items[i].value);
    }
  }
  symtableFree(&scope->symtable);
  free(scope);
}

/* searches for a variable from the innermost block (current_scope) outward
 * (shadowing) */
symbol_info_t *findInScope(semantic_context_t *context, const char *name) {
  scope_node_t *scope = context->current_scope;
  while (scope != NULL) {
    // searching from the inside out gives inner declarations priority over
    // outer ones
    if (symtableCheckForDuplicate(&scope->symtable, name)) {
      return (symbol_info_t *)symtableSearch(&scope->symtable, name);
    }
    scope = scope->parent;
  }
  return NULL; // Not found in any scope
}

/* checks existence of a variable only in the current scope */
bool is_variable_declared(symtable_t *symtable, const char *name) {
  if (symtable == NULL || name == NULL) {
    return false;
  }
  return symtableCheckForDuplicate(symtable, name);
}

/* checks existence of a function in the global symbol table */
bool is_function_declared(symtable_t *symtable, const char *name) {
  if (symtable == NULL || name == NULL) {
    return false;
  }
  return symtableCheckForDuplicate(symtable, name);
}

// stores the declaration data needed by later lookups and code generation
static symbol_info_t *create_var_symbol(const char *name, DATA_TYPE type,
                                        bool is_const, int id) {
  symbol_info_t *info = calloc(1, sizeof(symbol_info_t));
  if (info == NULL) {
    error_exit(ERROR_INTERNAL, 0,
               "Memory allocation failed for variable symbol.");
    return NULL;
  }
  info->kind = SYM_VAR;
  info->name = dup_string(name);
  info->data_type = type;
  info->is_const = is_const;
  info->id = id; // true for val and function parameters, false for var
  return info;
}

// function symbols start with empty parameter and return-tracking metadata
static symbol_info_t *create_func_symbol(const char *name, DATA_TYPE ret_type,
                                         bool explicit_ret, bool is_main,
                                         bool is_builtin) {
  symbol_info_t *info = calloc(1, sizeof(symbol_info_t));
  if (info == NULL) {
    error_exit(ERROR_INTERNAL, 0,
               "Memory allocation failed for function symbol.");
    return NULL;
  }
  info->kind = SYM_FUNC;
  info->name = dup_string(name);
  info->data_type = ret_type;
  info->is_const = false; // Functions are not constants
  info->explicit_return = explicit_ret;
  info->is_main = is_main;
  info->is_builtin = is_builtin;
  return info;
}

// frees the symbol name and structure
static void free_symbol(symbol_info_t *info) {
  if (info != NULL) {
    free(info->name);
    free(info);
  }
}

// returns the element type of a list, DATA_NONE otherwise
static DATA_TYPE type_ref_elem(AST_NODE *type) {
  if (type == NULL || type->data_type != DATA_LIST || type->left == NULL) {
    return DATA_NONE;
  }
  return type->left->data_type;
}

// creates a type ref for a basic type
static AST_NODE *basic_type_node(DATA_TYPE type) {
  return ast_type_ref_create(type, NULL, NULL);
}

// resolves aliases and class names to concrete type refs
static AST_NODE *resolve_type_ref(AST_NODE *type, semantic_context_t *context) {
  if (type == NULL) {
    return NULL;
  }
  if (type->data_type == DATA_LIST) {
    type->left = resolve_type_ref(type->left, context);
    return type;
  }
  if (type->data_type == DATA_UNION) {
    type->left = resolve_type_ref(type->left, context);
    type->right = resolve_type_ref(type->right, context);
    return type;
  }
  if (type->data_type != DATA_NONE || type->name == NULL) {
    return type;
  }
  symbol_info_t *sym =
      (symbol_info_t *)symtableSearch(&context->global_symtable, type->name);
  if (sym == NULL) {
    error_exit(ERROR_SEM_UNDEF, type->line, type->name, "Undefined type.");
    return type;
  }
  if (sym->kind == SYM_TYPE) {
    if (sym->type_ref == NULL) {
      error_exit(ERROR_SEM_UNDEF, type->line, type->name,
                 "Type used before its definition.");
    }
    return sym->type_ref;
  }
  if (sym->kind == SYM_CLASS) {
    return sym->type_ref;
  }
  error_exit(ERROR_SEM_UNDEF, type->line, type->name, "Name is not a type.");
  return type;
}

// returns the type ref of an expression, deriving it from its data type if
// needed
static AST_NODE *type_ref_of_expr(AST_NODE *expr) {
  if (expr == NULL) {
    return NULL;
  }
  if (expr->type_ref != NULL) {
    return expr->type_ref;
  }
  if (expr->data_type == DATA_LIST) {
    AST_NODE *elem =
        expr->elem_type == DATA_NONE ? NULL : basic_type_node(expr->elem_type);
    return ast_list_type_ref_create(elem, NULL);
  }
  return basic_type_node(expr->data_type);
}

// finds the list member of a union
static AST_NODE *find_list_member(AST_NODE *type) {
  if (type == NULL) {
    return NULL;
  }
  if (type->data_type == DATA_LIST) {
    return type;
  }
  if (type->data_type == DATA_UNION) {
    AST_NODE *left = find_list_member(type->left);
    return left != NULL ? left : find_list_member(type->right);
  }
  return NULL;
}

/* checks type compatibility including Int -> Double conversion, lists, unions
 * and classes */
static bool is_type_compatible_ref(AST_NODE *target, AST_NODE *source,
                                   bool *needs_int_to_double) {
  if (needs_int_to_double != NULL) {
    *needs_int_to_double = false;
  }
  if (target == NULL || source == NULL || target->data_type == DATA_NONE ||
      source->data_type == DATA_NONE) {
    return true;
  }
  if (source->data_type == DATA_UNION) {
    bool left_conv = false;
    bool right_conv = false;
    bool left = is_type_compatible_ref(target, source->left, &left_conv);
    bool right = is_type_compatible_ref(target, source->right, &right_conv);
    return left && right && !left_conv && !right_conv;
  }
  if (target->data_type == DATA_UNION) {
    bool left_conv = false;
    bool right_conv = false;
    bool left = is_type_compatible_ref(target->left, source, &left_conv);
    bool right = is_type_compatible_ref(target->right, source, &right_conv);
    if (needs_int_to_double != NULL) {
      bool exact = (left && !left_conv) || (right && !right_conv);
      *needs_int_to_double =
          !exact && ((left && left_conv) || (right && right_conv));
    }
    return left || right;
  }
  if (target->data_type == DATA_LIST) {
    if (source->data_type != DATA_LIST) {
      return false;
    }
    bool elem_conv = false;
    return is_type_compatible_ref(target->left, source->left, &elem_conv) &&
           !elem_conv;
  }
  if (target->data_type == DATA_CLASS) {
    if (source->data_type != DATA_CLASS) {
      return false;
    }
    if (target->name == NULL || source->name == NULL) {
      return true;
    }
    return strcmp(target->name, source->name) == 0;
  }
  if (target->data_type != source->data_type) {
    if (target->data_type == DATA_DOUBLE && source->data_type == DATA_INT) {
      // implicit Int -> Double widening is recorded for the generator
      if (needs_int_to_double != NULL) {
        *needs_int_to_double = true;
      }
      return true;
    }
    return false;
  }
  return true;
}

// checks compatibility of basic types
static bool is_type_compatible(DATA_TYPE target, DATA_TYPE source,
                               bool *needs_int_to_double) {
  return is_type_compatible_ref(basic_type_node(target),
                                basic_type_node(source), needs_int_to_double);
}

static bool coerce_value_expr(AST_NODE *value, AST_NODE *target,
                              semantic_context_t *context);

// coerces the last expression of a branch to the expected type
static bool coerce_branch_chain(AST_NODE *chain, AST_NODE *target,
                                semantic_context_t *context) {
  if (chain == NULL) {
    return true;
  }
  AST_NODE *last = chain;
  while (last->next != NULL) {
    last = last->next;
  }
  return coerce_value_expr(last, target, context);
}

// checks and coerces the elements of a List constructor
static bool coerce_list_constructor(AST_NODE *call, AST_NODE *target,
                                    semantic_context_t *context) {
  if (target == NULL || target->data_type != DATA_LIST) {
    return false;
  }
  call->type_ref = target;
  call->data_type = DATA_LIST;
  call->elem_type = type_ref_elem(target);
  bool ok = true;
  if (call->arg_groups != NULL) {
    for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next) {
      for (AST_NODE *arg = g->left; arg != NULL; arg = arg->next) {
        if (!coerce_value_expr(arg, target->left, context)) {
          ok = false;
        }
      }
    }
  } else {
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
      if (!coerce_value_expr(arg, target->left, context)) {
        ok = false;
      }
    }
  }
  return ok;
}

// checks an expression against the expected type and marks conversions
static bool coerce_value_expr(AST_NODE *value, AST_NODE *target,
                              semantic_context_t *context) {
  if (value == NULL || target == NULL) {
    return true;
  }
  while (value->type == NODE_EXPR_STMT && value->left != NULL) {
    value = value->left;
  }
  if (value->type == NODE_RETURN) {
    return true;
  }
  if (value->type == NODE_VAR_DEF || value->type == NODE_ASSIGN ||
      value->type == NODE_WHILE || value->type == NODE_FOR) {
    AST_NODE *unit = basic_type_node(DATA_UNIT);
    return is_type_compatible_ref(target, unit, NULL);
  }
  if (value->type == NODE_IF) {
    value->type_ref = target;
    value->data_type = target->data_type;
    bool then_ok = coerce_branch_chain(value->right, target, context);
    bool else_ok = coerce_branch_chain(value->third, target, context);
    return then_ok && else_ok;
  }
  if (value->type == NODE_MATCH) {
    value->type_ref = target;
    value->data_type = target->data_type;
    bool ok = true;
    for (AST_NODE *branch = value->right; branch != NULL;
         branch = branch->next) {
      if (!coerce_branch_chain(branch->third, target, context)) {
        ok = false;
      }
    }
    return ok;
  }
  if (value->type == NODE_CALL && value->name != NULL &&
      strcmp(value->name, "List") == 0 && target->data_type == DATA_LIST) {
    return coerce_list_constructor(value, target, context);
  }
  bool needs_conv = false;
  if (!is_type_compatible_ref(target, type_ref_of_expr(value), &needs_conv)) {
    return false;
  }
  if (needs_conv) {
    value->int_to_double = true;
  }
  return true;
}

// first AST pass: collect all function headers, detect redefinitions (error 4)
// and verify @main (errors 3, 4, 5, 6)
/* helper function for assigning parameters to built-in functions */
static void set_builtin_params(symbol_info_t *fn, size_t count,
                               const DATA_TYPE *types) {
  // built-ins do not need parameter names, but their types are checked like
  // user-defined functions
  fn->param_count = count;
  fn->group_count = 1;
  fn->group_counts = malloc(sizeof(size_t));
  if (fn->group_counts == NULL) {
    error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for group counts.");
    return;
  }
  fn->group_counts[0] = count;
  if (count == 0) {
    fn->params = NULL;
    return;
  }
  fn->params = calloc(count, sizeof(param_info_t));
  if (fn->params == NULL) {
    error_exit(ERROR_INTERNAL, 0,
               "Memory allocation failed for builtin params.");
    return;
  }
  for (size_t i = 0; i < count; i++) {
    // built-in parameter names are intentionally omitted because calls validate
    // only their types
    fn->params[i].name = NULL;
    fn->params[i].type = types[i];
  }
}

/* prepopulates global_symtable with built-in functions (print, readInt, subStr,
 * ...) */
static void register_builtin_functions(semantic_context_t *context) {
  symbol_info_t *fn;

  // print(value: Any): Unit -> Any uses special validation in
  // analyze_function_call
  fn = create_func_symbol("print", DATA_UNIT, true, false, true);
  set_builtin_params(fn, 1, (DATA_TYPE[]){DATA_UNIT});
  symtableInsert(&context->global_symtable, "print", fn);

  fn = create_func_symbol("readLine", DATA_STRING, true, false, true);
  set_builtin_params(fn, 0, NULL);
  symtableInsert(&context->global_symtable, "readLine", fn);

  fn = create_func_symbol("readInt", DATA_INT, true, false, true);
  set_builtin_params(fn, 0, NULL);
  symtableInsert(&context->global_symtable, "readInt", fn);

  fn = create_func_symbol("readDouble", DATA_DOUBLE, true, false, true);
  set_builtin_params(fn, 0, NULL);
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
  set_builtin_params(fn, 0, NULL);
  symtableInsert(&context->global_symtable, "readBoolean", fn);

  fn = create_func_symbol("readIntMaybe", DATA_UNION, true, false, true);
  set_builtin_params(fn, 0, NULL);
  fn->type_ref = ast_union_type_ref_create(basic_type_node(DATA_INT),
                                           basic_type_node(DATA_UNIT), NULL);
  symtableInsert(&context->global_symtable, "readIntMaybe", fn);

  fn = create_func_symbol("readDoubleMaybe", DATA_UNION, true, false, true);
  set_builtin_params(fn, 0, NULL);
  fn->type_ref = ast_union_type_ref_create(basic_type_node(DATA_DOUBLE),
                                           basic_type_node(DATA_UNIT), NULL);
  symtableInsert(&context->global_symtable, "readDoubleMaybe", fn);

  fn = create_func_symbol("List", DATA_LIST, true, false, true);
  fn->group_count = 1;
  fn->group_counts = malloc(sizeof(size_t));
  fn->group_counts[0] = 0;
  symtableInsert(&context->global_symtable, "List", fn);
}

/* pass 0: registers type and class names so aliases and recursive classes can
 * be resolved */
static void register_type_names(AST_NODE *root, semantic_context_t *context) {
  AST_NODE *curr =
      (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
  while (curr != NULL) {
    if (curr->type == NODE_TYPE_DEF) {
      if (symtableCheckForDuplicate(&context->global_symtable, curr->name)) {
        error_exit(ERROR_SEM_REDEF, curr->line, curr->name,
                   "Type redefinition.");
        return;
      }
      symbol_info_t *sym = calloc(1, sizeof(symbol_info_t));
      if (sym == NULL) {
        error_exit(ERROR_INTERNAL, curr->line, curr->name,
                   "Memory allocation failed for type.");
        return;
      }
      sym->kind = SYM_TYPE;
      sym->name = dup_string(curr->name);
      sym->data_type = DATA_NONE;
      symtableInsert(&context->global_symtable, curr->name, sym);
    } else if (curr->type == NODE_CASE_CLASS) {
      // case classes are registered as types with attribute metadata
      if (symtableCheckForDuplicate(&context->global_symtable, curr->name)) {
        error_exit(ERROR_SEM_REDEF, curr->line, curr->name,
                   "Class redefinition.");
        return;
      }
      symbol_info_t *sym = calloc(1, sizeof(symbol_info_t));
      if (sym == NULL) {
        error_exit(ERROR_INTERNAL, curr->line, curr->name,
                   "Memory allocation failed for class.");
        return;
      }
      sym->kind = SYM_CLASS;
      sym->name = dup_string(curr->name);
      sym->data_type = DATA_CLASS;
      sym->type_ref = ast_type_ref_create(DATA_CLASS, curr->name, NULL);
      sym->group_count = 1;
      sym->group_counts = malloc(sizeof(size_t));
      sym->group_counts[0] =
          curr->third != NULL ? (size_t)curr->third->param_cnt : 0;
      size_t count = sym->group_counts[0];
      sym->param_count = count;
      if (count > 0) {
        sym->params = calloc(count, sizeof(param_info_t));
        size_t idx = 0;
        for (AST_NODE *p = curr->third->left; p != NULL && idx < count;
             p = p->next) {
          for (size_t prev = 0; prev < idx; prev++) {
            if (strcmp(sym->params[prev].name, p->name) == 0) {
              error_exit(ERROR_SEM_REDEF, p->line, p->name,
                         "Duplicate class attribute.");
              return;
            }
          }
          sym->params[idx].name = dup_string(p->name);
          sym->params[idx].id = p->id;
          idx++;
        }
      }
      symtableInsert(&context->global_symtable, curr->name, sym);
    }
    curr = curr->next;
  }
}

// resolves type refs of types, classes and functions
static void resolve_program_types(AST_NODE *root, semantic_context_t *context) {
  AST_NODE *curr =
      (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
  while (curr != NULL) {
    if (curr->type == NODE_TYPE_DEF) {
      symbol_info_t *sym = (symbol_info_t *)symtableSearch(
          &context->global_symtable, curr->name);
      AST_NODE *resolved = resolve_type_ref(curr->left, context);
      curr->left = resolved;
      if (sym != NULL) {
        sym->type_ref = resolved;
        sym->data_type = resolved != NULL ? resolved->data_type : DATA_NONE;
      }
    }
    curr = curr->next;
  }
  curr = (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
  while (curr != NULL) {
    if (curr->type == NODE_CASE_CLASS) {
      symbol_info_t *sym = (symbol_info_t *)symtableSearch(
          &context->global_symtable, curr->name);
      if (sym != NULL && curr->third != NULL) {
        size_t idx = 0;
        for (AST_NODE *p = curr->third->left;
             p != NULL && idx < sym->param_count; p = p->next, idx++) {
          p->type_ref = resolve_type_ref(p->type_ref, context);
          if (p->type_ref != NULL) {
            p->data_type = p->type_ref->data_type;
          }
          sym->params[idx].type = p->data_type;
          sym->params[idx].elem_type = type_ref_elem(p->type_ref);
          sym->params[idx].type_ref = p->type_ref;
        }
      }
    }
    curr = curr->next;
  }
  curr = (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
  while (curr != NULL) {
    if (curr->type == NODE_FUNC_DEF) {
      curr->type_ref = resolve_type_ref(curr->type_ref, context);
      curr->data_type =
          curr->type_ref != NULL ? curr->type_ref->data_type : DATA_NONE;
      if (curr->third != NULL) {
        for (AST_NODE *g = curr->third->next; g != NULL; g = g->next) {
          for (AST_NODE *p = g->left; p != NULL; p = p->next) {
            p->type_ref = resolve_type_ref(p->type_ref, context);
            if (p->type_ref != NULL) {
              p->data_type = p->type_ref->data_type;
            }
          }
        }
      }
      if (curr->is_extension && curr->third != NULL &&
          curr->third->next != NULL) {
        AST_NODE *recv = curr->third->next->left;
        char *key = ast_method_key(curr->name, recv->type_ref);
        free(curr->name);
        curr->name = key;
      }
    }
    curr = curr->next;
  }
}

/* first pass: collects all function headers, detects redefinitions (error 4)
 * and verifies @main (errors 3, 4, 5, 6) */
static void collect_global_functions(AST_NODE *root,
                                     semantic_context_t *context) {
  AST_NODE *curr =
      (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;

  while (curr != NULL) {
    if (curr->type == NODE_CASE_CLASS) {
      curr = curr->next;
      continue;
    }
    if (curr->type == NODE_FUNC_DEF) {
      if (is_function_declared(&context->global_symtable, curr->name)) {
        error_exit(ERROR_SEM_REDEF, curr->line, curr->name,
                   "Function redefinition not allowed.");
        return;
      }

      bool explicit_ret = (curr->type_ref != NULL);
      DATA_TYPE ret_type = explicit_ret ? curr->data_type : DATA_NONE;
      DATA_TYPE ret_elem = type_ref_elem(curr->type_ref);
      bool is_main = curr->is_main;

      // parameter groups are flattened because the AST stores each source-level
      // group separately
      size_t p_count = 0;
      // count parameters before allocating one contiguous metadata array
      for (AST_NODE *g = curr->third != NULL ? curr->third->next : NULL;
           g != NULL; g = g->next) {
        p_count += (size_t)g->param_cnt;
      }

      // main must be unique, parameterless, and explicitly return Unit
      if (is_main) {
        if (context->has_main) {
          error_exit(ERROR_SEM_REDEF, curr->line, curr->name,
                     "Multiple @main functions.");
          return;
        }
        if (p_count > 0) {
          error_exit(ERROR_SEM_ARG, curr->line, curr->name,
                     "@main function cannot have parameters.");
          return;
        }
        if (!explicit_ret || ret_type != DATA_UNIT) {
          error_exit(ERROR_SEM_TYPE, curr->line, curr->name,
                     "@main function must return Unit.");
          return;
        }
        context->has_main = true;
      }

      symbol_info_t *fn_info = create_func_symbol(curr->name, ret_type,
                                                  explicit_ret, is_main, false);
      fn_info->param_count = p_count;
      fn_info->elem_type = ret_elem;
      fn_info->type_ref = curr->type_ref;
      fn_info->is_extension = curr->is_extension;
      fn_info->fn_node = curr;

      size_t g_count = 0;
      for (AST_NODE *g = curr->third != NULL ? curr->third->next : NULL;
           g != NULL; g = g->next) {
        g_count++;
      }
      fn_info->group_count = g_count;
      if (g_count > 0) {
        fn_info->group_counts = malloc(sizeof(size_t) * g_count);
        if (fn_info->group_counts == NULL) {
          error_exit(ERROR_INTERNAL, curr->line, curr->name,
                     "Memory allocation failed for group counts.");
          return;
        }
        size_t gi = 0;
        for (AST_NODE *g = curr->third->next; g != NULL; g = g->next, gi++) {
          fn_info->group_counts[gi] = (size_t)g->param_cnt;
        }
      }
      if (p_count > 0) {
        fn_info->params = malloc(sizeof(param_info_t) * p_count);
        if (fn_info->params == NULL) {
          error_exit(ERROR_INTERNAL, 0, "Memory allocation failed for params.");
          return;
        }
        size_t idx = 0;
        // copy parameter names, types, and ids from every parameter group into
        // the function symbol
        for (AST_NODE *g = curr->third != NULL ? curr->third->next : NULL;
             g != NULL; g = g->next) {
          for (AST_NODE *p = g->left; p != NULL; p = p->next, idx++) {
            fn_info->params[idx].name = dup_string(p->name);
            fn_info->params[idx].type = p->data_type;
            fn_info->params[idx].elem_type = type_ref_elem(p->type_ref);
            fn_info->params[idx].type_ref = p->type_ref;
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

// second AST pass: semantic checks, type inference, compatibility checks,
// return checks, and function call checks (errors 3, 4, 5, 6, 7, 10)
static DATA_TYPE analyze_expression(AST_NODE *expr,
                                    semantic_context_t *context);
static DATA_TYPE analyze_function_call(AST_NODE *call,
                                       semantic_context_t *context);
static DATA_TYPE analyze_statement(AST_NODE *stmt, semantic_context_t *context,
                                   bool value_needed, bool *guaranteed_return);
static DATA_TYPE analyze_block(AST_NODE *block, semantic_context_t *context,
                               bool create_scope, bool value_needed,
                               bool *guaranteed_return);

/* checks the function definition, parameters, body, and infers or verifies the
 * return type (errors 4, 6, 10) */
static void analyze_function_def(AST_NODE *func_node,
                                 semantic_context_t *context) {
  if (func_node == NULL || func_node->type != NODE_FUNC_DEF) {
    error_exit(ERROR_INTERNAL, 0, "Invalid function definition node.",
               "Expected NODE_FUNC_DEF");
    return;
  }

  symbol_info_t *fn_info = (symbol_info_t *)symtableSearch(
      &context->global_symtable, func_node->name);
  if (fn_info == NULL) {
    error_exit(ERROR_INTERNAL, func_node->line, func_node->name,
               "Function missing in global table.");
    return;
  }

  if (fn_info->is_defined) {
    return;
  }
  if (fn_info->analyzing) {
    // recursive functions need an explicit return type, otherwise inference
    // would be ambiguous
    error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name,
               "Recursive function needs an explicit return type.");
  }
  fn_info->analyzing = true;
  context->current_func = fn_info;
  context->in_function = true;
  // the function body starts with a fresh scope containing its parameters
  newScope(context);

  // inserts formal parameters into the function scope as immutable values
  // (is_const = true)
  for (size_t i = 0; i < fn_info->param_count; i++) {
    if (is_variable_declared(&context->current_scope->symtable,
                             fn_info->params[i].name)) {
      error_exit(ERROR_SEM_REDEF, func_node->line, fn_info->params[i].name,
                 "Duplicate parameter name.");
      return;
    }
    symbol_info_t *param_sym =
        create_var_symbol(fn_info->params[i].name, fn_info->params[i].type,
                          true, fn_info->params[i].id);
    param_sym->elem_type = fn_info->params[i].elem_type;
    param_sym->type_ref = fn_info->params[i].type_ref;
    symtableInsert(&context->current_scope->symtable, fn_info->params[i].name,
                   param_sym);
  }

  bool guaranteed_return = false;
  DATA_TYPE body_type = DATA_UNIT;

  AST_NODE *prev_expected = context->expected_type;
  if (fn_info->type_ref != NULL && fn_info->data_type != DATA_UNIT) {
    context->expected_type = fn_info->type_ref;
  }
  if (func_node->left != NULL) {
    body_type =
        analyze_block(func_node->left, context, false,
                      fn_info->data_type != DATA_UNIT, &guaranteed_return);
  }
  context->expected_type = prev_expected;

  if (fn_info->type_ref != NULL && fn_info->data_type != DATA_UNIT &&
      !guaranteed_return && func_node->left != NULL) {
    if (!coerce_branch_chain(func_node->left, fn_info->type_ref, context)) {
      error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name,
                 "Function body return type mismatch.");
      return;
    }
  }

  // checks the return type rules (section 2.3 of the specification)
  if (!fn_info->explicit_return) {
    func_node->data_type = body_type;
    // the implicit return type is inferred from the last expression of the body
    fn_info->data_type = body_type; // Odvození návratového typu
    if (func_node->left != NULL &&
        (body_type == DATA_LIST || body_type == DATA_CLASS)) {
      AST_NODE *last = func_node->left;
      while (last->next != NULL) {
        last = last->next;
      }
      AST_NODE *value = last->type == NODE_EXPR_STMT ? last->left : last;
      fn_info->type_ref = type_ref_of_expr(value);
      fn_info->elem_type = type_ref_elem(fn_info->type_ref);
    }
  } else if (fn_info->data_type != DATA_UNIT) {
    // explicit return type other than Unit: without a guaranteed return, the
    // last expression type must match
    if (!guaranteed_return) {
      bool needs_conv = false;
      if (!is_type_compatible(fn_info->data_type, body_type, &needs_conv)) {
        error_exit(ERROR_SEM_TYPE, func_node->line, func_node->name,
                   "Function body return type mismatch.");
        return;
      }
    }
  }

  fn_info->analyzing = false;
  fn_info->is_defined = true;
  freeScope(context);
  context->current_func = NULL;
  context->in_function = false;
}

// infers the return type of a function that has not been analyzed yet
static void infer_return_type(symbol_info_t *fn, semantic_context_t *context) {
  if (fn->fn_node != NULL && !fn->explicit_return && !fn->is_defined) {
    semantic_context_t callee = {.global_symtable = context->global_symtable};
    analyze_function_def(fn->fn_node, &callee);
  }
}

/* processes a val/var variable definition (error 4 for redefinition in the
 * current block, error 6 for a type mismatch) */
static void analyze_var_def(AST_NODE *var_node, semantic_context_t *context) {
  if (is_variable_declared(&context->current_scope->symtable, var_node->name)) {
    error_exit(ERROR_SEM_REDEF, var_node->line, var_node->name,
               "Variable redefinition in the same scope.");
    return;
  }

  AST_NODE *declared_ref = var_node->type_ref;
  if (declared_ref != NULL) {
    declared_ref = resolve_type_ref(declared_ref, context);
    var_node->type_ref = declared_ref;
    var_node->data_type = declared_ref->data_type;
  }

  // a declaration without an initializer keeps its explicit type, or remains
  // unknown until later use
  if (var_node->left == NULL) {
    symbol_info_t *var_sym = create_var_symbol(
        var_node->name, var_node->data_type, var_node->is_const, var_node->id);
    var_sym->elem_type = type_ref_elem(declared_ref);
    var_sym->type_ref = declared_ref;
    symtableInsert(&context->current_scope->symtable, var_node->name, var_sym);
    return;
  }

  AST_NODE *prev_expected = context->expected_type;
  context->expected_type = declared_ref;
  DATA_TYPE init_type = (var_node->left->type == NODE_CALL)
                            ? analyze_function_call(var_node->left, context)
                            : analyze_expression(var_node->left, context);
  context->expected_type = prev_expected;
  DATA_TYPE init_elem = var_node->left->elem_type;

  AST_NODE *final_ref = declared_ref;
  DATA_TYPE final_type = init_type;
  DATA_TYPE final_elem = init_elem;
  // the declared type wins over the inferred initializer type after
  // compatibility is verified
  if (declared_ref != NULL) {
    // the compatibility check also records whether an Int-to-Double conversion
    // is needed
    if (!coerce_value_expr(var_node->left, declared_ref, context)) {
      error_exit(ERROR_SEM_TYPE, var_node->line, var_node->name,
                 "Variable type mismatch.");
      return;
    }
    final_type = declared_ref->data_type;
    final_elem = type_ref_elem(declared_ref);
  } else {
    final_ref = type_ref_of_expr(var_node->left);
    final_type = var_node->left->data_type;
    final_elem = var_node->left->elem_type;
  }

  symbol_info_t *var_sym = create_var_symbol(var_node->name, final_type,
                                             var_node->is_const, var_node->id);
  var_sym->elem_type = final_elem;
  var_sym->type_ref = final_ref;
  symtableInsert(&context->current_scope->symtable, var_node->name, var_sym);
}
/* processes an id = rhs assignment (error 3 undefined, error 7 assignment to
 * val/parameter, error 6 type mismatch) */
static void analyze_assign(AST_NODE *assign_node, semantic_context_t *context) {
  if (assign_node == NULL || assign_node->type != NODE_ASSIGN) {
    error_exit(ERROR_INTERNAL, 0, "Invalid assignment node.",
               "Expected NODE_ASSIGN");
    return;
  }
  if (assign_node->left == NULL || assign_node->left->type != NODE_IDENT) {
    error_exit(ERROR_INTERNAL, 0, "Invalid left-hand side in assignment.",
               "Expected identifier.");
    return;
  }

  symbol_info_t *var_item = findInScope(context, assign_node->left->name);
  if (var_item == NULL) { // 3
    error_exit(ERROR_SEM_UNDEF, assign_node->line, assign_node->left->name,
               "Assignment to undefined variable.");
    return;
  }
  if (var_item->is_const) { // 7
    error_exit(ERROR_SEM_READ, assign_node->line, assign_node->left->name,
               "Assignment to constant variable.");
    return;
  }
  assign_node->left->id = var_item->id;

  if (assign_node->right == NULL) {
    // an assignment without a right-hand side is already valid after the target
    // checks above
    return;
  }
  AST_NODE *prev_expected = context->expected_type;
  context->expected_type = var_item->type_ref;
  (void)((assign_node->right->type == NODE_CALL)
             ? analyze_function_call(assign_node->right, context)
             : analyze_expression(assign_node->right, context));
  context->expected_type = prev_expected;

  // type mismatch (error 6)
  if (!coerce_value_expr(assign_node->right, var_item->type_ref, context)) {
    error_exit(ERROR_SEM_TYPE, assign_node->line, assign_node->left->name,
               "Type mismatch in assignment.");
    return;
  }
}

/* processes if-else (Boolean condition, new scopes for both blocks, return
 * rules) */
static DATA_TYPE analyze_if(AST_NODE *if_node, semantic_context_t *context,
                            bool value_needed, bool *guaranteed_return) {
  DATA_TYPE cond_type = analyze_expression(if_node->left, context);
  if (cond_type != DATA_BOOL && cond_type != DATA_NONE) {
    error_exit(ERROR_SEM_TYPE, if_node->line, "if",
               "Condition in if statement must be Boolean.");
    return DATA_UNIT;
  }

  bool then_ret = false;
  bool else_ret = false;

  // both branches are analyzed in separate scopes
  DATA_TYPE then_type =
      analyze_block(if_node->right, context, true, value_needed, &then_ret);
  DATA_TYPE else_type =
      analyze_block(if_node->third, context, true, value_needed, &else_ret);

  if (guaranteed_return != NULL) {
    // an if statement guarantees a return only when both branches guarantee one
    *guaranteed_return = (then_ret && else_ret);
  }

  AST_NODE *expected = context->expected_type;
  if (value_needed && expected != NULL) {
    bool then_ok = coerce_branch_chain(if_node->right, expected, context);
    bool else_ok = coerce_branch_chain(if_node->third, expected, context);
    if (!then_ok || !else_ok) {
      error_exit(ERROR_SEM_TYPE, if_node->line, "if",
                 "Branches have incompatible types.");
      return DATA_UNIT;
    }
    if_node->type_ref = expected;
    if_node->data_type = expected->data_type;
    return expected->data_type;
  }

  // without an expected type, the result type is derived from both branches
  DATA_TYPE result = DATA_UNIT;
  if (value_needed) {
    if (then_ret || then_type == else_type) {
      result = else_type;
    } else if (else_ret) {
      result = then_type;
    } else if ((then_type == DATA_INT && else_type == DATA_DOUBLE) ||
               (then_type == DATA_DOUBLE && else_type == DATA_INT)) {
      result = DATA_DOUBLE;
    } else if (then_type == DATA_NONE || else_type == DATA_NONE) {
      // an unresolved branch keeps the whole conditional unresolved
      result = DATA_NONE;
    } else {
      error_exit(ERROR_SEM_TYPE, if_node->line, "if",
                 "Branches have incompatible types.");
    }
  }
  if_node->data_type = result;
  if (value_needed &&
      (result == DATA_LIST || result == DATA_CLASS || result == DATA_UNION)) {
    AST_NODE *branch = then_ret ? if_node->third : if_node->right;
    AST_NODE *value = branch;
    if (value != NULL) {
      while (value->next != NULL) {
        value = value->next;
      }
      if (value->type == NODE_EXPR_STMT) {
        value = value->left;
      }
      if (value != NULL) {
        if_node->type_ref = type_ref_of_expr(value);
        if_node->elem_type = value->elem_type;
      }
    }
  }
  return result;
}

/* processes while (Boolean condition, new scope for the body, loop result is
 * Unit) */
static void analyze_while(AST_NODE *while_node, semantic_context_t *context) {
  DATA_TYPE cond_type = analyze_expression(while_node->left, context);
  if (cond_type != DATA_BOOL && cond_type != DATA_NONE) {
    error_exit(ERROR_SEM_TYPE, while_node->line, "while",
               "Condition in while loop must be Boolean.");
    return;
  }

  bool prev_in_loop = context->in_loop;
  context->in_loop = true;

  bool body_ret = false;
  analyze_block(while_node->right, context, true, false, &body_ret);

  // loop analysis must not leak the loop state into the surrounding statement
  context->in_loop = prev_in_loop;
}

/* processes a return statement (return type check, bare return only for Unit,
 * errors 6, 10) */
static void analyze_return(AST_NODE *ret_node, semantic_context_t *context) {
  if (!context->in_function || context->current_func == NULL) {
    error_exit(ERROR_SEM_OTHER, ret_node->line, "return",
               "Return statement outside of function.");
    return;
  }

  if (!context->current_func->explicit_return) {
    error_exit(ERROR_SEM_TYPE, ret_node->line, context->current_func->name,
               "Return statement requires explicit function return type.");
    return;
  }

  // bare return without an expression
  if (ret_node->left == NULL) {
    if (context->current_func->data_type != DATA_UNIT) {
      error_exit(ERROR_SEM_OTHER, ret_node->line, "return",
                 "Bare return allowed only in explicit Unit functions.");
      return;
    }
    return;
  }

  // return with an expression or function call
  AST_NODE *prev_expected = context->expected_type;
  if (context->current_func->data_type != DATA_UNIT) {
    context->expected_type = context->current_func->type_ref;
  }
  (void)((ret_node->left->type == NODE_CALL)
             ? analyze_function_call(ret_node->left, context)
             : analyze_expression(ret_node->left, context));
  context->expected_type = prev_expected;

  // for explicit Unit, the value is evaluated and discarded (rule 4 in
  // section 2.3)
  if (context->current_func->data_type == DATA_UNIT) {
    // Unit functions still analyze the expression so undefined names and
    // invalid calls are detected
    return;
  }

  if (!coerce_value_expr(ret_node->left, context->current_func->type_ref,
                         context)) {
    error_exit(ERROR_SEM_TYPE, ret_node->line, "return",
               "Return expression type does not match function signature.");
    return;
  }
}

/* processes a function call as a statement or right-hand-side expression
 * (errors 3, 5) */
static size_t call_group_count(AST_NODE *call) {
  if (call->arg_groups != NULL) {
    return (size_t)call->arg_groups->param_groups;
  }
  return call->has_parens ? 1 : 0;
}

// counts arguments in a chain
static size_t arg_chain_count(AST_NODE *arg) {
  size_t count = 0;
  for (; arg != NULL; arg = arg->next) {
    count++;
  }
  return count;
}

// splits a call into the part before an index and the indexing itself
static DATA_TYPE split_index_call(AST_NODE *call, size_t keep_groups,
                                  semantic_context_t *context) {
  AST_NODE *base = ast_create(call->left, NULL, NULL, NODE_CALL);
  base->line = call->line;
  base->is_method = call->is_method;
  base->name = dup_string(call->name);

  AST_NODE *index_group =
      call->arg_groups != NULL ? call->arg_groups->next : NULL;
  AST_NODE *direct_index = call->third;
  if (keep_groups != 0) {
    AST_NODE *last = index_group;
    for (size_t i = 1; i < keep_groups; i++) {
      last = last->next;
    }
    index_group = last->next;
    last->next = NULL;
    base->has_parens = true;
    if (keep_groups == 1) {
      AST_NODE *first = call->arg_groups->next;
      base->third = first->left;
      free(first);
    } else {
      base->arg_groups = call->arg_groups;
      base->arg_groups->param_groups = (int)keep_groups;
    }
  }
  if (keep_groups < 2) {
    free(call->arg_groups);
  }

  AST_NODE *previous_expected = context->expected_type;
  context->expected_type = NULL;
  analyze_function_call(base, context);
  do {
    AST_NODE *arg = index_group != NULL ? index_group->left : direct_index;
    AST_NODE *indexed = ast_create(base, NULL, NULL, NODE_CALL);
    indexed->third = arg;
    indexed->is_index = true;
    indexed->line = call->line;
    if (base->data_type != DATA_LIST || arg == NULL || arg->next != NULL) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List indexing needs one index and a List value.");
      return DATA_UNIT;
    }
    DATA_TYPE index_type = analyze_expression(arg, context);
    if (index_type != DATA_INT && index_type != DATA_NONE) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List index must be Int.");
      return DATA_UNIT;
    }
    indexed->type_ref = base->type_ref != NULL ? base->type_ref->left : NULL;
    indexed->data_type = indexed->type_ref != NULL
                             ? indexed->type_ref->data_type
                             : base->elem_type;
    indexed->elem_type = type_ref_elem(indexed->type_ref);
    base = indexed;
    if (index_group != NULL) {
      AST_NODE *done = index_group;
      index_group = index_group->next;
      free(done);
    }
  } while (index_group != NULL);
  context->expected_type = previous_expected;

  AST_NODE *saved_next = call->next;
  free(call->name);
  *call = *base;
  call->next = saved_next;
  free(base);
  return call->data_type;
}

// analyzes an argument and checks its type
static bool analyze_argument(AST_NODE *arg, AST_NODE *type,
                             semantic_context_t *context) {
  AST_NODE *previous_expected = context->expected_type;
  context->expected_type = type;
  analyze_expression(arg, context);
  context->expected_type = previous_expected;
  return coerce_value_expr(arg, type, context);
}

// checks the number and types of list method arguments
static void check_list_method_args(AST_NODE *call, size_t count,
                                   semantic_context_t *context) {
  if (!call->has_parens || arg_chain_count(call->third) != count) {
    error_exit(ERROR_SEM_ARG, call->line, call->name,
               "Wrong number of arguments in List method.");
  }
  for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
    if (!analyze_argument(arg, basic_type_node(DATA_INT), context)) {
      error_exit(ERROR_SEM_ARG, arg->line, call->name,
                 "List method arguments must be Int.");
    }
  }
}

// analyzes a function call, method call, constructor or indexing
static DATA_TYPE analyze_function_call(AST_NODE *call,
                                       semantic_context_t *context) {
  if (call->is_static_call) {
    return call->data_type;
  }
  if (call->is_index && call->left != NULL) {
    DATA_TYPE base_type = analyze_expression(call->left, context);
    if (base_type != DATA_LIST) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "Indexed value is not a List.");
      return DATA_UNIT;
    }
    if (call->third == NULL || call->third->next != NULL) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List index takes one argument.");
      return DATA_UNIT;
    }
    DATA_TYPE index_type = analyze_expression(call->third, context);
    if (index_type != DATA_INT && index_type != DATA_NONE) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List index must be Int.");
      return DATA_UNIT;
    }
    AST_NODE *elem_ref =
        call->left->type_ref != NULL ? call->left->type_ref->left : NULL;
    call->type_ref = elem_ref;
    call->data_type =
        elem_ref != NULL ? elem_ref->data_type : call->left->elem_type;
    call->elem_type = type_ref_elem(elem_ref);
    return call->data_type;
  }
  if (call->is_method && call->name != NULL &&
      strcmp(call->name, "range") == 0 && call->left != NULL &&
      call->left->type == NODE_IDENT && call->left->name != NULL &&
      strcmp(call->left->name, "List") == 0) {
    if (call_group_count(call) > 1) {
      return split_index_call(call, 1, context);
    }
    call->is_static_call = true;
    call->is_method = false;
    size_t args = 0;
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
      DATA_TYPE t = analyze_expression(arg, context);
      if (t != DATA_INT && t != DATA_NONE) {
        error_exit(ERROR_SEM_ARG, arg->line, call->name,
                   "List.range arguments must be Int.");
        return DATA_UNIT;
      }
      args++;
    }
    if (args < 2 || args > 3) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List.range takes 2 or 3 arguments.");
      return DATA_UNIT;
    }
    AST_NODE *elem = basic_type_node(DATA_INT);
    call->type_ref = ast_list_type_ref_create(elem, NULL);
    call->data_type = DATA_LIST;
    call->elem_type = DATA_INT;
    return DATA_LIST;
  }
  if (call->is_method) {
    // method receivers and arguments are analyzed first, then the built-in
    // method result type is selected
    if (strchr(call->name, '$') != NULL) {
      return call->data_type;
    }
    DATA_TYPE recv_type = DATA_NONE;
    DATA_TYPE recv_elem = DATA_NONE;
    if (call->left != NULL) {
      recv_type = analyze_expression(call->left, context);
      recv_elem = call->left->elem_type;
    }
    if (recv_type == DATA_LIST && call->name != NULL &&
        (strcmp(call->name, "filter") == 0 ||
         strcmp(call->name, "takeWhile") == 0 ||
         strcmp(call->name, "dropWhile") == 0)) {
      if (call->arg_groups != NULL) {
        if (call->arg_groups->param_groups > 1) {
          return split_index_call(call, 1, context);
        }
        if (call->arg_groups->next != NULL) {
          call->third = call->arg_groups->next->left;
          call->arg_groups = NULL;
        }
      }
      if (call->third == NULL || call->third->next != NULL) {
        error_exit(ERROR_SEM_ARG, call->line, call->name,
                   "Predicate takes one argument.");
        return DATA_UNIT;
      }
      AST_NODE *pred = call->third;
      symbol_info_t *pfn = pred->type == NODE_IDENT && pred->name != NULL
                               ? (symbol_info_t *)symtableSearch(
                                     &context->global_symtable, pred->name)
                               : NULL;
      if (pfn != NULL && pfn->kind == SYM_FUNC && !pfn->is_extension) {
        infer_return_type(pfn, context);
      }
      if (pfn == NULL || pfn->kind != SYM_FUNC || pfn->is_extension ||
          pfn->param_count != 1 || pfn->data_type != DATA_BOOL) {
        error_exit(ERROR_SEM_TYPE, call->line, call->name,
                   "Predicate must be a Boolean function.");
        return DATA_UNIT;
      }
      pred->is_function_ref = true;
      call->type_ref = call->left->type_ref;
      call->data_type = DATA_LIST;
      call->elem_type = recv_elem;
      return DATA_LIST;
    }
    size_t method_args = 0;
    if (recv_type == DATA_LIST) {
      if (strcmp(call->name, "drop") == 0 || strcmp(call->name, "take") == 0) {
        method_args = 1;
      } else if (strcmp(call->name, "slice") == 0) {
        method_args = 2;
      }
    }
    DATA_TYPE result = DATA_NONE;
    DATA_TYPE result_elem = DATA_NONE;
    if (recv_type == DATA_CLASS && call->left != NULL &&
        call->left->type_ref != NULL && call->left->type_ref->name != NULL) {
      symbol_info_t *cls = (symbol_info_t *)symtableSearch(
          &context->global_symtable, call->left->type_ref->name);
      if (cls != NULL && cls->kind == SYM_CLASS) {
        for (size_t fi = 0; fi < cls->param_count; fi++) {
          if (cls->params[fi].name != NULL &&
              strcmp(cls->params[fi].name, call->name) == 0) {
            if (call->has_parens) {
              return split_index_call(call, 0, context);
            }
            call->is_field = true;
            call->field_index = (int)fi + 1;
            call->data_type = cls->params[fi].type;
            call->type_ref = cls->params[fi].type_ref;
            call->elem_type = cls->params[fi].elem_type;
            return cls->params[fi].type;
          }
        }
      }
    }
    if (recv_type == DATA_LIST) {
      if (strcmp(call->name, "toString") == 0) {
        result = DATA_STRING;
        call->type_ref = basic_type_node(DATA_STRING);
      } else if (strcmp(call->name, "length") == 0) {
        result = DATA_INT;
        call->type_ref = basic_type_node(DATA_INT);
      } else if (strcmp(call->name, "head") == 0) {
        result = recv_elem;
        call->type_ref = call->left != NULL && call->left->type_ref != NULL
                             ? call->left->type_ref->left
                             : NULL;
      } else if (strcmp(call->name, "tail") == 0 ||
                 strcmp(call->name, "drop") == 0 ||
                 strcmp(call->name, "take") == 0 ||
                 strcmp(call->name, "slice") == 0 ||
                 strcmp(call->name, "toList") == 0) {
        result = DATA_LIST;
        result_elem = recv_elem;
        call->type_ref = call->left != NULL ? call->left->type_ref : NULL;
      }
    } else if (strcmp(call->name, "toString") == 0) {
      result = DATA_STRING;
    } else if (strcmp(call->name, "length") == 0 &&
               (recv_type == DATA_STRING || recv_type == DATA_NONE)) {
      result = DATA_INT;
    } else if (strcmp(call->name, "toInt") == 0 &&
               (recv_type == DATA_DOUBLE || recv_type == DATA_NONE)) {
      result = DATA_INT;
    } else if (strcmp(call->name, "floor") == 0 &&
               (recv_type == DATA_DOUBLE || recv_type == DATA_NONE)) {
      result = DATA_DOUBLE;
    }

    // method return types are determined by the supported method name after all
    // arguments are checked
    if (result != DATA_NONE) {
      size_t groups = call_group_count(call);
      size_t expected_groups = method_args == 0 ? 0 : 1;
      if (groups > expected_groups) {
        if (result == DATA_LIST) {
          return split_index_call(call, expected_groups, context);
        }
        error_exit(ERROR_SEM_ARG, call->line, call->name,
                   "This member does not take arguments.");
        return DATA_UNIT;
      }
      if (method_args != 0) {
        check_list_method_args(call, method_args, context);
      }
    }

    if (result == DATA_NONE) {
      char *key = ast_method_key(call->name, type_ref_of_expr(call->left));
      symbol_info_t *ext =
          (symbol_info_t *)symtableSearch(&context->global_symtable, key);
      if (ext == NULL && recv_type == DATA_INT) {
        char *alt_key =
            ast_method_key(call->name, basic_type_node(DATA_DOUBLE));
        symbol_info_t *alt =
            (symbol_info_t *)symtableSearch(&context->global_symtable, alt_key);
        if (alt != NULL && alt->kind == SYM_FUNC && alt->is_extension &&
            alt->param_count > 0 && alt->params[0].type == DATA_DOUBLE) {
          ext = alt;
          free(key);
          key = alt_key;
          alt_key = NULL;
        }
        free(alt_key);
      }
      if (ext != NULL && ext->kind == SYM_FUNC && ext->is_extension) {
        size_t ext_expected_groups =
            ext->group_count > 0 ? ext->group_count - 1 : 0;
        if (call_group_count(call) > ext_expected_groups) {
          free(key);
          return split_index_call(call, ext_expected_groups, context);
        }
        AST_NODE *first_group =
            call->arg_groups != NULL ? call->arg_groups->next : NULL;
        size_t expected_groups =
            ext->group_count > 0 ? ext->group_count - 1 : 0;
        size_t call_groups = 0;
        size_t arg_count = 0;
        if (first_group != NULL) {
          for (AST_NODE *g = first_group; g != NULL; g = g->next) {
            call_groups++;
            arg_count += (size_t)g->param_cnt;
          }
        } else if (call->has_parens) {
          call_groups = 1;
          for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
            arg_count++;
          }
        }
        if (call_groups != expected_groups) {
          error_exit(ERROR_SEM_ARG, call->line, call->name,
                     "Wrong number of argument lists in method call.");
          return DATA_UNIT;
        }
        if (arg_count + 1 != ext->param_count) {
          error_exit(ERROR_SEM_ARG, call->line, call->name,
                     "Wrong number of arguments in method call.");
          return DATA_UNIT;
        }
        if (ext->group_counts != NULL) {
          if (first_group != NULL) {
            size_t gi = 0;
            for (AST_NODE *g = first_group; g != NULL; g = g->next, gi++) {
              if ((size_t)g->param_cnt != ext->group_counts[gi + 1]) {
                error_exit(ERROR_SEM_ARG, call->line, call->name,
                           "Wrong number of arguments in one list.");
                return DATA_UNIT;
              }
            }
          } else if (call->has_parens && expected_groups == 1 &&
                     arg_count != ext->group_counts[1]) {
            error_exit(ERROR_SEM_ARG, call->line, call->name,
                       "Wrong number of arguments in one list.");
            return DATA_UNIT;
          }
        }
        if (!coerce_value_expr(call->left, ext->params[0].type_ref, context)) {
          error_exit(ERROR_SEM_ARG, call->line, call->name,
                     "Incompatible receiver type.");
          return DATA_UNIT;
        }
        size_t idx = 1;
        if (first_group != NULL) {
          for (AST_NODE *g = first_group; g != NULL; g = g->next) {
            for (AST_NODE *arg = g->left; arg != NULL; arg = arg->next, idx++) {
              if (!analyze_argument(arg, ext->params[idx].type_ref, context)) {
                error_exit(ERROR_SEM_ARG, arg->line, call->name,
                           "Incompatible argument type.");
                return DATA_UNIT;
              }
            }
          }
        } else {
          for (AST_NODE *arg = call->third; arg != NULL;
               arg = arg->next, idx++) {
            if (!analyze_argument(arg, ext->params[idx].type_ref, context)) {
              error_exit(ERROR_SEM_ARG, arg->line, call->name,
                         "Incompatible argument type.");
              return DATA_UNIT;
            }
          }
        }
        infer_return_type(ext, context);
        result = ext->data_type;
        result_elem = ext->elem_type;
        call->type_ref = ext->type_ref;
        free(call->name);
        call->name = key;
        key = NULL;
      } else if (recv_type != DATA_NONE) {
        free(key);
        error_exit(ERROR_SEM_UNDEF, call->line, call->name,
                   "Undefined method.");
        return DATA_UNIT;
      }
      free(key);
    }
    call->data_type = result;
    call->elem_type = result_elem;
    return result;
  }

  symbol_info_t *var = findInScope(context, call->name);
  if (var != NULL && var->kind == SYM_VAR) {
    if (var->data_type != DATA_LIST) {
      error_exit(ERROR_SYN, call->line, call->name,
                 "Variable is not callable.");
      return DATA_UNIT;
    }
    if (call_group_count(call) > 1) {
      return split_index_call(call, 1, context);
    }
    size_t index_args = 0;
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
      index_args++;
    }
    if (index_args != 1) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List index takes one argument.");
      return DATA_UNIT;
    }
    DATA_TYPE index_type = analyze_expression(call->third, context);
    if (index_type != DATA_INT && index_type != DATA_NONE) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "List index must be Int.");
      return DATA_UNIT;
    }
    call->is_index = true;
    call->data_type = var->elem_type;
    call->type_ref = var->type_ref != NULL ? var->type_ref->left : NULL;
    call->elem_type = type_ref_elem(call->type_ref);
    call->id = var->id;
    return var->elem_type;
  }

  symbol_info_t *fn =
      (symbol_info_t *)symtableSearch(&context->global_symtable, call->name);
  if (fn != NULL && fn->kind == SYM_CLASS) {
    size_t groups = call->arg_groups != NULL
                        ? (size_t)call->arg_groups->param_groups
                        : (call->has_parens ? 1 : 0);
    size_t args = 0;
    if (call->arg_groups != NULL) {
      for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next) {
        args += (size_t)g->param_cnt;
      }
    } else {
      for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
        args++;
      }
    }
    if (groups != 1 || args != fn->param_count) {
      error_exit(ERROR_SEM_ARG, call->line, call->name,
                 "Wrong number of arguments in constructor.");
      return DATA_UNIT;
    }
    size_t idx = 0;
    if (call->arg_groups != NULL) {
      for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next) {
        for (AST_NODE *arg = g->left; arg != NULL; arg = arg->next, idx++) {
          AST_NODE *prev_expected = context->expected_type;
          context->expected_type = fn->params[idx].type_ref;
          (void)analyze_expression(arg, context);
          context->expected_type = prev_expected;
          if (!coerce_value_expr(arg, fn->params[idx].type_ref, context)) {
            error_exit(ERROR_SEM_ARG, arg->line, call->name,
                       "Incompatible constructor argument.");
            return DATA_UNIT;
          }
        }
      }
    } else {
      for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next, idx++) {
        AST_NODE *prev_expected = context->expected_type;
        context->expected_type = fn->params[idx].type_ref;
        (void)analyze_expression(arg, context);
        context->expected_type = prev_expected;
        if (!coerce_value_expr(arg, fn->params[idx].type_ref, context)) {
          error_exit(ERROR_SEM_ARG, arg->line, call->name,
                     "Incompatible constructor argument.");
          return DATA_UNIT;
        }
      }
    }
    call->is_ctor = true;
    call->data_type = DATA_CLASS;
    call->type_ref = fn->type_ref;
    return DATA_CLASS;
  }
  // ordinary calls resolve through the global function table
  if (fn == NULL || fn->kind != SYM_FUNC || fn->is_extension) {
    error_exit(ERROR_SEM_UNDEF, call->line, call->name,
               "Call to undefined function.");
    return DATA_UNIT;
  }

  if (call_group_count(call) > fn->group_count) {
    return split_index_call(call, fn->group_count, context);
  }

  size_t call_groups = 0;
  size_t arg_count = 0;
  if (call->arg_groups != NULL) {
    for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next) {
      call_groups++;
      arg_count += (size_t)g->param_cnt;
    }
  } else if (call->has_parens) {
    call_groups = 1;
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next) {
      arg_count++;
    }
  }

  // List accepts any number of expressions and therefore bypasses ordinary
  // parameter counting
  if (strcmp(fn->name, "List") == 0) {
    if (call_group_count(call) > 1) {
      return split_index_call(call, 1, context);
    }
    AST_NODE *target = context->expected_type;
    AST_NODE *expected_list = find_list_member(target);
    AST_NODE *expected_elem =
        expected_list != NULL ? expected_list->left : NULL;
    AST_NODE *first = call->third;
    if (first == NULL && expected_elem == NULL) {
      error_exit(ERROR_SEM_TYPE, call->line, call->name,
                 "An empty List needs an expected element type.");
      return DATA_UNIT;
    }

    context->expected_type = expected_elem;
    bool all_numeric = true;
    bool has_double = false;
    for (AST_NODE *arg = first; arg != NULL; arg = arg->next) {
      DATA_TYPE type = analyze_expression(arg, context);
      all_numeric = all_numeric && (type == DATA_INT || type == DATA_DOUBLE);
      has_double = has_double || type == DATA_DOUBLE;
    }
    context->expected_type = target;
    if (expected_elem != NULL && expected_list != NULL &&
        coerce_list_constructor(call, expected_list, context)) {
      return DATA_LIST;
    }

    AST_NODE *elem = all_numeric && has_double ? basic_type_node(DATA_DOUBLE)
                                               : type_ref_of_expr(first);
    AST_NODE *list = ast_list_type_ref_create(elem, NULL);
    if (!coerce_list_constructor(call, list, context)) {
      error_exit(ERROR_SEM_TYPE, call->line, call->name,
                 "List elements must have compatible types.");
      return DATA_UNIT;
    }
    return DATA_LIST;
  }
  if (call_groups != fn->group_count) {
    error_exit(ERROR_SEM_ARG, call->line, call->name,
               "Wrong number of argument groups in function call.");
    return DATA_UNIT;
  }

  if (arg_count != fn->param_count) {
    error_exit(ERROR_SEM_ARG, call->line, call->name,
               "Wrong number of arguments in function call.");
    return DATA_UNIT;
  }

  if (call->arg_groups != NULL && fn->group_counts != NULL) {
    size_t gi = 0;
    for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next, gi++) {
      if ((size_t)g->param_cnt != fn->group_counts[gi]) {
        error_exit(ERROR_SEM_ARG, call->line, call->name,
                   "Wrong number of arguments in one group.");
        return DATA_UNIT;
      }
    }
  }

  // print and toStr accept any expression, while other functions require
  // matching parameter types
  bool is_any_builtin = (fn->is_builtin && (strcmp(fn->name, "print") == 0 ||
                                            strcmp(fn->name, "toStr") == 0));

  size_t idx = 0;
  // argument expressions are checked in source order against the corresponding
  // parameter
  if (call->arg_groups != NULL) {
    for (AST_NODE *g = call->arg_groups->next; g != NULL; g = g->next) {
      for (AST_NODE *arg = g->left; arg != NULL; arg = arg->next, idx++) {
        AST_NODE *target_ref = is_any_builtin ? NULL : fn->params[idx].type_ref;
        if (!is_any_builtin && target_ref == NULL &&
            fn->params[idx].type != DATA_NONE) {
          target_ref = basic_type_node(fn->params[idx].type);
        }
        if (!analyze_argument(arg, target_ref, context)) {
          error_exit(ERROR_SEM_ARG, arg->line, call->name,
                     "Incompatible argument type.");
          return DATA_UNIT;
        }
      }
    }
  } else {
    for (AST_NODE *arg = call->third; arg != NULL; arg = arg->next, idx++) {
      AST_NODE *target_ref = is_any_builtin ? NULL : fn->params[idx].type_ref;
      if (!is_any_builtin && target_ref == NULL &&
          fn->params[idx].type != DATA_NONE) {
        target_ref = basic_type_node(fn->params[idx].type);
      }
      if (!analyze_argument(arg, target_ref, context)) {
        error_exit(ERROR_SEM_ARG, arg->line, call->name,
                   "Incompatible argument type.");
        return DATA_UNIT;
      }
    }
  }

  infer_return_type(fn, context);
  call->data_type = fn->data_type;
  call->elem_type = fn->elem_type;
  call->type_ref = fn->type_ref;
  return fn->data_type;
}

// analyzes a match pattern and introduces its bindings
static DATA_TYPE analyze_pattern(AST_NODE *pat, semantic_context_t *context) {
  if (pat == NULL) {
    return DATA_NONE;
  }
  if (pat->type == NODE_WILDCARD) {
    return DATA_NONE;
  }
  if (pat->type == NODE_LITERAL) {
    pat->type_ref = basic_type_node(pat->data_type);
    return pat->data_type;
  }
  if (pat->type == NODE_IDENT) {
    if (pat->type_ref != NULL) {
      pat->type_ref = resolve_type_ref(pat->type_ref, context);
      pat->data_type =
          pat->type_ref != NULL ? pat->type_ref->data_type : DATA_NONE;
    }
    symbol_info_t *sym =
        create_var_symbol(pat->name, pat->data_type, true, pat->id);
    sym->type_ref = pat->type_ref;
    sym->elem_type = type_ref_elem(pat->type_ref);
    symtableInsert(&context->current_scope->symtable, pat->name, sym);
    return pat->data_type;
  }
  if (pat->type == NODE_CALL) {
    symbol_info_t *ctor =
        (symbol_info_t *)symtableSearch(&context->global_symtable, pat->name);
    if (ctor == NULL || ctor->kind != SYM_CLASS) {
      error_exit(ERROR_SEM_UNDEF, pat->line, pat->name,
                 "Undefined constructor in pattern.");
      return DATA_NONE;
    }
    if (arg_chain_count(pat->third) != ctor->param_count) {
      error_exit(ERROR_SEM_ARG, pat->line, pat->name,
                 "Wrong number of fields in constructor pattern.");
      return DATA_NONE;
    }
    size_t idx = 0;
    for (AST_NODE *arg = pat->third; arg != NULL; arg = arg->next, idx++) {
      if (arg->type == NODE_IDENT) {
        if (is_variable_declared(&context->current_scope->symtable,
                                 arg->name)) {
          error_exit(ERROR_SEM_REDEF, arg->line, arg->name,
                     "Duplicate name in constructor pattern.");
          return DATA_NONE;
        }
        AST_NODE *field_ref = ctor->params[idx].type_ref;
        arg->type_ref = field_ref;
        arg->data_type = field_ref != NULL ? field_ref->data_type : DATA_NONE;
        symbol_info_t *sym =
            create_var_symbol(arg->name, arg->data_type, true, arg->id);
        sym->type_ref = field_ref;
        sym->elem_type = type_ref_elem(field_ref);
        symtableInsert(&context->current_scope->symtable, arg->name, sym);
      }
    }
    pat->type_ref = ctor->type_ref;
    pat->data_type = DATA_CLASS;
    return DATA_CLASS;
  }
  return DATA_NONE;
}

/* recursively evaluates the expression data type and marks implicit Int ->
 * Double conversion (error 6) */
static DATA_TYPE analyze_expression(AST_NODE *expr,
                                    semantic_context_t *context) {
  if (expr == NULL) {
    return DATA_UNIT;
  }

  switch (expr->type) {
  case NODE_LITERAL:
    return expr->data_type;

  case NODE_CALL:
    return analyze_function_call(expr, context);

  case NODE_MATCH: {
    analyze_expression(expr->left, context);
    bool has_wildcard = false;
    AST_NODE *expected = context->expected_type;
    AST_NODE *result_ref = NULL;
    DATA_TYPE result = DATA_NONE;
    for (AST_NODE *branch = expr->right; branch != NULL;
         branch = branch->next) {
      newScope(context);
      analyze_pattern(branch->left, context);
      if (branch->left != NULL && branch->left->type == NODE_WILDCARD &&
          branch->right == NULL) {
        has_wildcard = true;
      }
      if (branch->right != NULL) {
        DATA_TYPE guard_t = analyze_expression(branch->right, context);
        if (guard_t != DATA_BOOL && guard_t != DATA_NONE) {
          error_exit(ERROR_SEM_TYPE, branch->line, "match",
                     "Guard must be Boolean.");
          return DATA_UNIT;
        }
      }
      bool branch_ret = false;
      DATA_TYPE branch_t =
          analyze_block(branch->third, context, false, true, &branch_ret);
      AST_NODE *branch_ref = NULL;
      if (branch->third != NULL) {
        AST_NODE *last = branch->third;
        while (last->next != NULL) {
          last = last->next;
        }
        AST_NODE *value = last->type == NODE_EXPR_STMT ? last->left : last;
        if (value != NULL) {
          branch_ref = type_ref_of_expr(value);
        }
      }
      if (expected != NULL) {
        if (!coerce_branch_chain(branch->third, expected, context)) {
          error_exit(ERROR_SEM_TYPE, branch->line, "match",
                     "Branches have incompatible types.");
          return DATA_UNIT;
        }
        result = expected->data_type;
        result_ref = expected;
      } else if (result_ref == NULL) {
        result = branch_t;
        result_ref = branch_ref;
      } else if (branch_ref != NULL) {
        bool conv = false;
        bool forward = is_type_compatible_ref(result_ref, branch_ref, &conv);
        if (forward) {
          if (conv && branch->third != NULL) {
            AST_NODE *last = branch->third;
            while (last->next != NULL) {
              last = last->next;
            }
            if (last->type == NODE_EXPR_STMT) {
              last = last->left;
            }
            if (last != NULL) {
              last->int_to_double = true;
            }
          }
        } else {
          bool back = false;
          if (is_type_compatible_ref(branch_ref, result_ref, &back)) {
            result = branch_ref->data_type;
            result_ref = branch_ref;
          } else {
            error_exit(ERROR_SEM_TYPE, branch->line, "match",
                       "Branches have incompatible types.");
            return DATA_UNIT;
          }
        }
      }
      freeScope(context);
    }
    if (!has_wildcard) {
      error_exit(ERROR_SEM_OTHER, expr->line, "match", "Nonexhaustive match.");
      return DATA_UNIT;
    }
    expr->data_type = result;
    expr->type_ref = result_ref;
    return result;
  }
  case NODE_IF:
    return analyze_if(expr, context, true, NULL);

  case NODE_RANGE: {
    // range operands are checked recursively, the result is a list of Ints
    DATA_TYPE from_t = DATA_NONE;
    DATA_TYPE to_t = DATA_NONE;
    DATA_TYPE step_t = DATA_NONE;
    if (expr->left != NULL) {
      from_t = analyze_expression(expr->left, context);
    }
    if (expr->right != NULL) {
      to_t = analyze_expression(expr->right, context);
    }
    if (expr->third != NULL) {
      step_t = analyze_expression(expr->third, context);
    }
    if ((from_t != DATA_INT && from_t != DATA_NONE) ||
        (to_t != DATA_INT && to_t != DATA_NONE) ||
        (step_t != DATA_INT && step_t != DATA_NONE)) {
      error_exit(ERROR_SEM_TYPE, expr->line, "range",
                 "Range bounds and step must be Int.");
      return DATA_UNIT;
    }
    if (expr->third != NULL && expr->third->type == NODE_LITERAL &&
        expr->third->data_type == DATA_INT && expr->third->ival == 0) {
      error_exit(ERROR_SEM_OTHER, expr->line, "range",
                 "Range step must not be zero.");
      return DATA_UNIT;
    }
    expr->data_type = DATA_LIST;
    expr->elem_type = DATA_INT;
    return DATA_LIST;
  }

  case NODE_INTERP:
    // literal interpolation parts need no semantic analysis, but embedded
    // expressions do
    for (AST_NODE *part = expr->left; part != NULL; part = part->next) {
      if (part->type != NODE_LITERAL) {
        analyze_expression(part, context);
      }
    }
    expr->data_type = DATA_STRING;
    return DATA_STRING;

  case NODE_IDENT: {
    // an identifier first resolves as a local variable, then as a zero-argument
    // function reference
    symbol_info_t *var = findInScope(context, expr->name);
    if (var == NULL || var->kind != SYM_VAR) {
      symbol_info_t *fn = (symbol_info_t *)symtableSearch(
          &context->global_symtable, expr->name);
      if (fn != NULL && fn->kind == SYM_FUNC) {
        if (fn->group_count != 0) {
          error_exit(ERROR_SEM_ARG, expr->line, expr->name,
                     "Function requires an argument list.");
          return DATA_UNIT;
        }
        // a bare function name is represented as a call with no arguments
        expr->type = NODE_CALL;
        expr->third = NULL;
        expr->is_method = false;
        expr->has_parens = false;
        infer_return_type(fn, context);
        expr->data_type = fn->data_type;
        expr->elem_type = fn->elem_type;
        expr->type_ref = fn->type_ref;
        return fn->data_type;
      }
      error_exit(ERROR_SEM_UNDEF, expr->line, expr->name,
                 "Use of undefined variable.");
      return DATA_UNIT;
    }
    expr->data_type = var->data_type;
    expr->elem_type = var->elem_type;
    expr->type_ref = var->type_ref;
    expr->id = var->id;
    return var->data_type;
  }

  case NODE_UNOP: {
    DATA_TYPE op_type = analyze_expression(expr->left, context);
    // logical negation is Boolean-only; unary arithmetic operators preserve the
    // numeric operand type
    if (expr->op_type == OP_NOT) {
      if (op_type != DATA_BOOL && op_type != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, expr->line, "unary",
                   "Operator ! requires Boolean.");
        return DATA_UNIT;
      }
      expr->data_type = DATA_BOOL;
      return DATA_BOOL;
    }
    if (op_type == DATA_NONE) {
      // unknown operands remain unknown instead of causing a second misleading
      // type error
      return DATA_NONE;
    }
    if (expr->op_type == OP_ADD || expr->op_type == OP_SUB) {
      if (op_type != DATA_INT && op_type != DATA_DOUBLE) {
        error_exit(ERROR_SEM_TYPE, expr->line, "unary",
                   "Unary +/- requires Int or Double.");
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

    // an unknown operand prevents reliable type validation for the whole binary
    // expression
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
      error_exit(ERROR_SEM_TYPE, expr->line, "binop",
                 "Logical operator requires Boolean.");
      return DATA_UNIT;

    case OP_ADD:
      // addition also implements list concatenation and string concatenation
      if (left_t == DATA_LIST && right_t == DATA_LIST) {
        AST_NODE *left_ref = type_ref_of_expr(expr->left);
        AST_NODE *right_ref = type_ref_of_expr(expr->right);
        if (!is_type_compatible_ref(left_ref, right_ref, NULL) ||
            !is_type_compatible_ref(right_ref, left_ref, NULL)) {
          error_exit(ERROR_SEM_TYPE, expr->line, "concat",
                     "List element type mismatch.");
          return DATA_UNIT;
        }
        expr->data_type = DATA_LIST;
        expr->type_ref = left_ref;
        expr->elem_type = type_ref_elem(left_ref);
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
        // mixed numeric operands produce Double; otherwise the narrower common
        // type is preserved
        DATA_TYPE res = (left_t == DATA_DOUBLE || right_t == DATA_DOUBLE)
                            ? DATA_DOUBLE
                            : DATA_INT;
        expr->data_type = res;
        return res;
      }
      error_exit(ERROR_SEM_TYPE, expr->line, "arith",
                 "Invalid operand types for arithmetic operator.");
      return DATA_UNIT;

    case OP_TO:
    case OP_UNTIL:
    case OP_BY:
      // range operators work with Ints and produce a list of Ints
      if (right_t != DATA_INT && right_t != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, expr->line, "range",
                   "Range bound must be Int.");
        return DATA_UNIT;
      }
      if (expr->op_type == OP_BY) {
        if (left_t != DATA_LIST && left_t != DATA_NONE) {
          error_exit(ERROR_SEM_TYPE, expr->line, "range",
                     "Step requires a range.");
          return DATA_UNIT;
        }
        if (expr->right->type == NODE_LITERAL &&
            expr->right->data_type == DATA_INT && expr->right->ival == 0) {
          error_exit(ERROR_SEM_OTHER, expr->line, "range",
                     "Range step must not be zero.");
          return DATA_UNIT;
        }
      } else if (left_t != DATA_INT && left_t != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, expr->line, "range",
                   "Range start must be Int.");
        return DATA_UNIT;
      }
      expr->data_type = DATA_LIST;
      expr->elem_type = DATA_INT;
      return DATA_LIST;

    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
      if (((left_t == DATA_INT || left_t == DATA_DOUBLE) &&
           (right_t == DATA_INT || right_t == DATA_DOUBLE)) ||
          (left_t == DATA_STRING && right_t == DATA_STRING)) {
        expr->data_type = DATA_BOOL;
        return DATA_BOOL;
      }
      error_exit(ERROR_SEM_TYPE, expr->line, "rel",
                 "Invalid operand types for relational operator.");
      return DATA_UNIT;

    case OP_EQ:
    case OP_NEQ:
      if (left_t == DATA_UNION || right_t == DATA_UNION) {
        error_exit(ERROR_SEM_TYPE, expr->line, "equality",
                   "Union values cannot be compared directly.");
        return DATA_UNIT;
      }
      if (left_t == right_t ||
          ((left_t == DATA_INT || left_t == DATA_DOUBLE) &&
           (right_t == DATA_INT || right_t == DATA_DOUBLE))) {
        expr->data_type = DATA_BOOL;
        return DATA_BOOL;
      }
      error_exit(ERROR_SEM_TYPE, expr->line, "eq",
                 "Invalid operand types for equality operator.");
      return DATA_UNIT;

    default:
      return DATA_NONE;
    }
  }

  default:
    return DATA_NONE;
  }
}

/* processes one statement and returns its resulting type */
static DATA_TYPE analyze_statement(AST_NODE *stmt, semantic_context_t *context,
                                   bool value_needed, bool *guaranteed_return) {
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
    return analyze_if(stmt, context, value_needed, guaranteed_return);

  case NODE_WHILE:
    analyze_while(stmt, context);
    return DATA_UNIT;

  case NODE_RETURN:
    analyze_return(stmt, context);
    if (guaranteed_return != NULL) {
      // a return statement terminates the remaining control-flow path
      *guaranteed_return = true;
    }
    return DATA_UNIT;

  case NODE_CALL:
    return analyze_function_call(stmt, context);

  case NODE_EXPR_STMT:
    return analyze_expression(stmt->left, context);

  case NODE_FOR: {
    DATA_TYPE source_type = DATA_NONE;
    DATA_TYPE source_elem = DATA_NONE;
    if (stmt->left != NULL) {
      source_type = analyze_expression(stmt->left, context);
      source_elem = stmt->left->elem_type;
    }
    DATA_TYPE loop_type = DATA_NONE;
    if (source_type == DATA_LIST) {
      loop_type = source_elem;
    } else if (source_type != DATA_NONE) {
      error_exit(ERROR_SEM_TYPE, stmt->line, stmt->name,
                 "For source must be a range or a list.");
      return DATA_UNIT;
    }
    // the loop variable and its body share a dedicated scope that is removed
    // after analysis
    newScope(context);
    AST_NODE *loop_ref = NULL;
    if (stmt->left != NULL && stmt->left->type_ref != NULL &&
        stmt->left->type_ref->data_type == DATA_LIST) {
      loop_ref = stmt->left->type_ref->left;
    } else if (loop_type != DATA_NONE) {
      loop_ref = basic_type_node(loop_type);
    }
    if (stmt->name != NULL) {
      symbol_info_t *var_sym =
          create_var_symbol(stmt->name, loop_type, true, stmt->id);
      var_sym->type_ref = loop_ref;
      var_sym->elem_type = type_ref_elem(loop_ref);
      symtableInsert(&context->current_scope->symtable, stmt->name, var_sym);
    }
    // the guard expressions are evaluated in the loop scope, but their values
    // are discarded
    for (AST_NODE *guard = stmt->third; guard != NULL; guard = guard->next) {
      DATA_TYPE guard_t = analyze_expression(guard, context);
      if (guard_t != DATA_BOOL && guard_t != DATA_NONE) {
        error_exit(ERROR_SEM_TYPE, guard->line, "for",
                   "For guard must be Boolean.");
        return DATA_UNIT;
      }
    }
    bool body_ret = false;
    analyze_block(stmt->right, context, false, false, &body_ret);
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

/* traverses the statement chain in a block (node->next) and returns the last
 * statement type */
static DATA_TYPE analyze_block(AST_NODE *block, semantic_context_t *context,
                               bool create_scope, bool value_needed,
                               bool *guaranteed_return) {
  if (block == NULL) {
    return DATA_UNIT;
  }

  if (create_scope) {
    // the caller keeps the current scope when requested, while nested blocks
    // own their temporary scope
    newScope(context);
  }

  DATA_TYPE last_type = DATA_UNIT;
  AST_NODE *stmt = (block->type == NODE_BLOCK) ? block->left : block;

  // only the last statement type is returned, while any returning statement
  // marks the block as returning
  while (stmt != NULL) {
    bool stmt_returns = false;
    last_type = analyze_statement(
        stmt, context, value_needed && stmt->next == NULL, &stmt_returns);
    if (stmt_returns && guaranteed_return != NULL) {
      *guaranteed_return = true;
    }
    stmt = stmt->next;
  }

  if (create_scope) {
    freeScope(context);
  }

  // the final statement determines the block value used for return-type
  // inference
  return last_type;
}

// checks whether a function calls itself
static bool calls_itself(AST_NODE *node, AST_NODE *origin,
                         semantic_context_t *context) {
  for (; node != NULL; node = node->next) {
    if (node->type == NODE_CALL && !node->is_index) {
      symbol_info_t *fn = (symbol_info_t *)symtableSearch(
          &context->global_symtable, node->name);
      if (fn != NULL && fn->fn_node != NULL &&
          fn->is_extension == node->is_method) {
        if (fn->fn_node == origin) {
          return true;
        }
        if (fn->visited_from != origin) {
          fn->visited_from = origin;
          if (calls_itself(fn->fn_node->left, origin, context)) {
            return true;
          }
        }
      }
      if (node->is_method && node->name != NULL && node->third != NULL &&
          (strcmp(node->name, "filter") == 0 ||
           strcmp(node->name, "takeWhile") == 0 ||
           strcmp(node->name, "dropWhile") == 0)) {
        AST_NODE *pred = node->third;
        if (pred->type == NODE_IDENT && pred->name != NULL) {
          symbol_info_t *pfn = (symbol_info_t *)symtableSearch(
              &context->global_symtable, pred->name);
          if (pfn != NULL && pfn->fn_node != NULL) {
            if (pfn->fn_node == origin) {
              return true;
            }
            if (pfn->visited_from != origin) {
              pfn->visited_from = origin;
              if (calls_itself(pfn->fn_node->left, origin, context)) {
                return true;
              }
            }
          }
        }
      }
    }
    if (calls_itself(node->left, origin, context) ||
        calls_itself(node->right, origin, context) ||
        calls_itself(node->third, origin, context) ||
        calls_itself(node->arg_groups, origin, context)) {
      return true;
    }
  }
  return false;
}

/* main semantic analysis function, performs passes over the AST */
void semantic_analyze(AST_NODE *root) {
  semantic_context_t context;
  // initialize the semantic context to zero
  memset(&context, 0, sizeof(semantic_context_t));

  if (!symtableInit(&context.global_symtable)) {
    // error exit is built into symtableInit, so we just return here
    return;
  }

  register_builtin_functions(&context);
  register_type_names(root, &context);
  resolve_program_types(root, &context);
  // first pass: collects function headers, checks for redefinitions, and
  // verifies @main
  collect_global_functions(root, &context);

  // second pass: checks the bodies of all functions
  AST_NODE *curr =
      (root != NULL && root->type == NODE_PROGRAM) ? root->left : root;
  while (curr != NULL) {
    if (curr->type == NODE_FUNC_DEF) {
      analyze_function_def(curr, &context);
    }
    curr = curr->next;
  }

  for (curr = root->left; curr != NULL; curr = curr->next) {
    if (curr->type == NODE_FUNC_DEF && curr->type_ref == NULL &&
        calls_itself(curr->left, curr, &context)) {
      error_exit(ERROR_SEM_TYPE, curr->line, curr->name,
                 "Recursive function needs an explicit return type.");
    }
  }

  // cleans up the global symbol table
  size_t size_of_global_symtable = context.global_symtable.size;
  for (size_t i = 0; i < size_of_global_symtable; i++) {
    if (context.global_symtable.items[i].status == SLOT_OCCUPIED) {
      symbol_info_t *info =
          (symbol_info_t *)context.global_symtable.items[i].value;
      if (info != NULL) {
        // parameter names and arrays belong to each function symbol and must be
        // released before the symbol itself
        for (size_t p = 0; p < info->param_count; p++) {
          free(info->params[p].name);
        }
        free(info->params);
        free(info->group_counts);
        free_symbol(info);
      }
    }
  }
  symtableFree(&context.global_symtable);
}
