/*
 * IFJ26 Compiler 
 * File: ast.c
 * Author: Pavel Novák [xnovakp01]
 */
#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// copies a string, exits on allocation failure
static char *ast_strdup(const char *ptr) {
  if (!ptr)
    return NULL;
  size_t len = strlen(ptr);
  char *s = malloc(len + 1);
  if (s == NULL) {
    exit(99);
  }
  memcpy(s, ptr, len);
  s[len] = '\0';
  return s;
}

// returns the basic type name for a type key
static const char *ast_type_suffix(DATA_TYPE type) {
  switch (type) {
  case DATA_INT:
    return "Int";
  case DATA_DOUBLE:
    return "Double";
  case DATA_STRING:
    return "String";
  case DATA_BOOL:
    return "Bool";
  case DATA_UNIT:
    return "Unit";
  case DATA_LIST:
    return "List";
  case DATA_UNION:
    return "Union";
  case DATA_CLASS:
    return "Class";
  default:
    return "Unknown";
  }
}

// joins two strings with a separator into a new key
static char *join_type_keys(const char *first, const char *separator,
                            const char *second) {
  size_t len = strlen(first) + strlen(separator) + strlen(second) + 1;
  char *key = malloc(len);
  if (key == NULL) {
    exit(99);
  }
  snprintf(key, len, "%s%s%s", first, separator, second);
  return key;
}

// counts union members
static size_t union_member_count(const AST_NODE *type) {
  if (type->data_type != DATA_UNION) {
    return 1;
  }
  return union_member_count(type->left) + union_member_count(type->right);
}

// collects keys of all union members
static void collect_union_keys(const AST_NODE *type, char **keys,
                               size_t *count) {
  if (type->data_type == DATA_UNION) {
    collect_union_keys(type->left, keys, count);
    collect_union_keys(type->right, keys, count);
  } else {
    keys[(*count)++] = ast_type_key(type);
  }
}

// compares two keys for qsort
static int compare_type_keys(const void *first, const void *second) {
  return strcmp(*(char *const *)first, *(char *const *)second);
}

// builds the canonical type key for comparisons and methods
char *ast_type_key(const AST_NODE *type) {
  if (type == NULL) {
    return ast_strdup("Unknown");
  }
  if (type->data_type == DATA_CLASS && type->name != NULL) {
    return join_type_keys("Class", "$", type->name);
  }
  if (type->data_type == DATA_LIST) {
    char *elem = ast_type_key(type->left);
    char *prefix = join_type_keys("List", "$", elem);
    char *key = join_type_keys(prefix, "&", "");
    free(prefix);
    free(elem);
    return key;
  }
  if (type->data_type == DATA_UNION) {
    char **keys = malloc(union_member_count(type) * sizeof(*keys));
    if (keys == NULL) {
      exit(99);
    }
    size_t count = 0;
    collect_union_keys(type, keys, &count);
    qsort(keys, count, sizeof(*keys), compare_type_keys);
    char *key = ast_strdup("Union");
    for (size_t i = 0; i < count; i++) {
      if (i == 0 || strcmp(keys[i - 1], keys[i]) != 0) {
        char *previous = key;
        key = join_type_keys(previous, "$", keys[i]);
        free(previous);
      }
    }
    for (size_t i = 0; i < count; i++) {
      free(keys[i]);
    }
    free(keys);
    char *closed = join_type_keys(key, "&", "");
    free(key);
    return closed;
  }
  return ast_strdup(ast_type_suffix(type->data_type));
}

// builds a method key from the name and receiver type
char *ast_method_key(const char *name, const AST_NODE *receiver) {
  char *suffix = ast_type_key(receiver);
  char *key = join_type_keys(name, "$", suffix);
  free(suffix);
  return key;
}

// allocates and initializes a generic AST node
AST_NODE *ast_create(AST_NODE *left, AST_NODE *right, AST_NODE *next,
                     NODE_TYPE type) {
  AST_NODE *node = calloc(1, sizeof(AST_NODE));
  if (!node) {
    exit(99);
  }

  node->type = type;
  node->left = left;
  node->right = right;
  node->next = next;

  return node;
}

// sets and copies the node name
void ast_name(AST_NODE *node, char *name) { node->name = ast_strdup(name); }

// creates an int literal node
AST_NODE *ast_int_literal_create(long long value, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_LITERAL);
  node->ival = value;
  node->data_type = DATA_INT;
  return node;
}

// creates a double literal node
AST_NODE *ast_double_literal_create(double value, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_LITERAL);
  node->dval = value;
  node->data_type = DATA_DOUBLE;
  return node;
}
// creates a string literal node
AST_NODE *ast_string_literal_create(char *value, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_LITERAL);
  node->sval = ast_strdup(value);
  node->data_type = DATA_STRING;
  return node;
}

// creates a Unit literal node
AST_NODE *ast_unit_literal_create(AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_LITERAL);
  node->data_type = DATA_UNIT;
  return node;
}

// creates a bool literal node
AST_NODE *ast_bool_literal_create(bool value, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_LITERAL);
  node->data_type = DATA_BOOL;
  if (value)
    node->ival = 1;
  else
    node->ival = 0;
  return node;
}

// creates an assignment node
AST_NODE *ast_assign_create(AST_NODE *var, AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(var, expr, next, NODE_ASSIGN);
  return node;
}

// if nothing is returned then NULL
AST_NODE *ast_return_create(AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(expr, NULL, next, NODE_RETURN);
  return node;
}

// creates a binary operator node
AST_NODE *ast_binop_create(AST_NODE *left, AST_NODE *right_term, AST_NODE *next,
                           OP_TYPE op) {
  AST_NODE *node = ast_create(left, right_term, next, NODE_BINOP);
  node->op_type = op;

  return node;
}

// creates a unary operator node
AST_NODE *ast_unop_create(AST_NODE *term, AST_NODE *next, OP_TYPE op) {
  AST_NODE *node = ast_create(term, NULL, next, NODE_UNOP);
  node->op_type = op;
  return node;
}

// creates an identifier node
AST_NODE *ast_ident_create(AST_NODE *left, char *name, AST_NODE *next) {
  AST_NODE *node = ast_create(left, NULL, next, NODE_IDENT);
  ast_name(node, name);
  return node;
}

// creates a variable definition node
// semantic analysis must check if DATA_NONE
AST_NODE *ast_var_def_create(AST_NODE *term, AST_NODE *next, char *name,
                             bool is_const, DATA_TYPE declared) {
  AST_NODE *node = ast_create(term, NULL, next, NODE_VAR_DEF);
  ast_name(node, name);
  node->is_const = is_const;
  node->data_type = declared;
  return node;
}

// creates an if node
AST_NODE *ast_if_create(AST_NODE *cond, AST_NODE *then, AST_NODE *els,
                        AST_NODE *next) {
  AST_NODE *node = ast_create(cond, then, next, NODE_IF);
  node->third = els;
  return node;
}

// creates a while node
AST_NODE *ast_while_create(AST_NODE *left, AST_NODE *body, AST_NODE *next) {
  AST_NODE *node = ast_create(left, body, next, NODE_WHILE);
  return node;
}

// creates a function definition node
AST_NODE *ast_func_def_create(AST_NODE *body, AST_NODE *params, AST_NODE *next,
                              DATA_TYPE return_type, char *name, bool is_main,
                              bool has_parens) {
  AST_NODE *node = ast_create(body, NULL, next, NODE_FUNC_DEF);
  ast_name(node, name);
  node->third = params;
  node->data_type = return_type;
  node->is_main = is_main;
  node->has_parens = has_parens;
  return node;
}

// creates a function parameter node
AST_NODE *ast_param_create(DATA_TYPE type, char *name, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_PARAM);
  ast_name(node, name);
  node->data_type = type;
  return node;
}

// creates a for node
AST_NODE *ast_for_create(AST_NODE *range, AST_NODE *body, AST_NODE *guards,
                         AST_NODE *next, char *name) {
  AST_NODE *node = ast_create(range, body, next, NODE_FOR);
  ast_name(node, name);
  node->third = guards;
  return node;
}

// creates a range node
AST_NODE *ast_range_create(AST_NODE *from, AST_NODE *to, AST_NODE *step,
                           AST_NODE *next, bool inclusive) {
  AST_NODE *node = ast_create(from, to, next, NODE_RANGE);
  node->third = step;
  node->inclusive = inclusive;
  return node;
}

// creates a function call node
AST_NODE *ast_call_create(AST_NODE *params, char *name, bool is_method,
                          bool has_parens, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_CALL);
  ast_name(node, name);
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

// creates a method call node
AST_NODE *ast_method_call_create(AST_NODE *params, char *name, bool is_method,
                                 bool has_parens, AST_NODE *reciever,
                                 AST_NODE *next) {
  AST_NODE *node = ast_create(reciever, NULL, next, NODE_CALL);
  ast_name(node, name);
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

// creates a list of parameter groups
AST_NODE *ast_create_params_list_of_list(AST_NODE *next_param_list,
                                         int list_cnt) {
  AST_NODE *node = ast_create(NULL, NULL, next_param_list, NODE_LIST_OF_LIST);
  node->param_groups = list_cnt;
  return node;
}

// creates one parameter group
AST_NODE *ast_create_param_list(AST_NODE *params, AST_NODE *next,
                                int param_cnt) {
  AST_NODE *node = ast_create(params, NULL, next, NODE_PARAM_LIST);
  node->param_cnt = param_cnt;
  return node;
}

typedef struct TypeRefAllocation {
  AST_NODE node;
  struct TypeRefAllocation *next;
} TypeRefAllocation;

static TypeRefAllocation *type_refs;

// allocates a type ref in the arena
static AST_NODE *allocate_type_ref(AST_NODE *left, AST_NODE *right,
                                   AST_NODE *next) {
  TypeRefAllocation *entry = calloc(1, sizeof(*entry));
  if (entry == NULL) {
    exit(99);
  }
  entry->node.type = NODE_TYPE_REF;
  entry->node.left = left;
  entry->node.right = right;
  entry->node.next = next;
  entry->next = type_refs;
  type_refs = entry;
  return &entry->node;
}

// frees all type refs from the arena
void ast_free_type_refs(void) {
  while (type_refs != NULL) {
    TypeRefAllocation *entry = type_refs;
    type_refs = entry->next;
    free(entry->node.name);
    free(entry);
  }
}

// creates a type ref for a basic type or class
AST_NODE *ast_type_ref_create(DATA_TYPE base, char *name, AST_NODE *next) {
  AST_NODE *node = allocate_type_ref(NULL, NULL, next);
  node->data_type = base;
  if (name != NULL) {
    ast_name(node, name);
  }
  return node;
}

// creates a list type ref
AST_NODE *ast_list_type_ref_create(AST_NODE *elem, AST_NODE *next) {
  AST_NODE *node = allocate_type_ref(elem, NULL, next);
  node->data_type = DATA_LIST;
  return node;
}

// creates a union type ref
AST_NODE *ast_union_type_ref_create(AST_NODE *left, AST_NODE *right,
                                    AST_NODE *next) {
  AST_NODE *node = allocate_type_ref(left, right, next);
  node->data_type = DATA_UNION;
  return node;
}

// creates a type definition node
AST_NODE *ast_type_def_create(char *name, AST_NODE *type, AST_NODE *next) {
  AST_NODE *node = ast_create(type, NULL, next, NODE_TYPE_DEF);
  ast_name(node, name);
  return node;
}

// creates a case class node
AST_NODE *ast_case_class_create(char *name, AST_NODE *params, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL, NULL, next, NODE_CASE_CLASS);
  node->third = params;
  ast_name(node, name);
  return node;
}

// creates a match node
AST_NODE *ast_match_create(AST_NODE *subject, AST_NODE *cases, AST_NODE *next) {
  return ast_create(subject, cases, next, NODE_MATCH);
}

// creates one match branch node
AST_NODE *ast_case_create(AST_NODE *pattern, AST_NODE *guard, AST_NODE *body,
                          AST_NODE *next) {
  AST_NODE *node = ast_create(pattern, guard, next, NODE_CASE);
  node->third = body;
  return node;
}

// creates a wildcard pattern node
AST_NODE *ast_wildcard_create(AST_NODE *next) {
  return ast_create(NULL, NULL, next, NODE_WILDCARD);
}

// appends a node to the end of a sibling list
AST_NODE *ast_append(AST_NODE *new_node, AST_NODE *next) {
  if (!new_node)
    return next;
  if (!next)
    return new_node;
  if (!next->next) {
    next->next = new_node;
  } else {
    ast_append(new_node, next->next);
  }
  return next;
}

// recursively frees an AST node and its children
void free_AST(AST_NODE *node) {
  if (!node || node->type == NODE_TYPE_REF) {
    return;
  }
  free_AST(node->left);
  free_AST(node->right);
  free_AST(node->next);
  free_AST(node->third);
  free_AST(node->arg_groups);
  free(node->name);
  free(node->sval);
  free(node);
}
