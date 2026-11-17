/*
* IFJ: AST
 * Soubor: ast.c
 * Autor: Pavel Novák [xnovakp01]
 */

#include "ast.h"

#include <string.h>
#include <stdlib.h>


static char *ast_strdup(const char *ptr) {
  if (!ptr) return NULL;
  size_t len = strlen(ptr);
  char *s = malloc(len + 1);
  if (s == NULL) {
    exit(99);
  }
  memcpy(s, ptr, len);
  s[len] = '\0';
  return s;
}


AST_NODE *ast_create(AST_NODE *left, AST_NODE *right,AST_NODE *next, NODE_TYPE type) {
  AST_NODE *node = calloc(1,sizeof(AST_NODE));
  if (!node) {
    exit(99);
  }

  node->type = type;
  node->left = left;
  node->right = right;
  node->next = next;

  return node;
}

void ast_name(AST_NODE *node, char *name) {
  node->name = ast_strdup(name);
}



AST_NODE * ast_int_literal_create( long long value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->ival = value;
  node->data_type = DATA_INT;
  return node;
}

AST_NODE * ast_double_literal_create(double value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->dval = value;
  node->data_type = DATA_DOUBLE;
  return node;
}
AST_NODE * ast_string_literal_create(char *value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->sval = ast_strdup(value);
  node->data_type = DATA_STRING;
  return node;
}

AST_NODE * ast_unit_literal_create(AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->data_type = DATA_UNIT;
  return node;
}

AST_NODE * ast_bool_literal_create(bool value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->data_type = DATA_BOOL;
  if (value) node->ival = 1;
  else node->ival = 0;
  return node;
}



AST_NODE *ast_assign_create(AST_NODE *var, AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(var, expr,next, NODE_ASSIGN);
  return node;
}

//if nothing is returned then NULL
AST_NODE *ast_return_create(AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(expr,NULL,next, NODE_RETURN);
  return node;
}


AST_NODE * ast_binop_create(AST_NODE *left, AST_NODE *right_term, AST_NODE *next, OP_TYPE op)
{
  AST_NODE *node = ast_create(left, right_term,next, NODE_BINOP);
  node->op_type = op;

  return node;
}


AST_NODE * ast_unop_create(AST_NODE *term, AST_NODE *next, OP_TYPE op) {
  AST_NODE *node = ast_create(term, NULL, next, NODE_UNOP);
  node->op_type = op;
  return node;

}


AST_NODE * ast_var_def_create(AST_NODE *term,AST_NODE *next, char *name, bool is_const, DATA_TYPE declared) { //semantic analyze must check if DATA_NONE
  AST_NODE *node = ast_create(term, NULL, next, NODE_VAR_DEF);
  ast_name(node, name);
  node->is_const = is_const;
  node->data_type = declared;
  return node;
}

AST_NODE * ast_if_create(AST_NODE *cond, AST_NODE *then,  AST_NODE *els,AST_NODE *next) {
  AST_NODE *node = ast_create(cond,then, next, NODE_IF);
  node->third = els;
  return node;

}

AST_NODE * ast_while_create(AST_NODE *left, AST_NODE *body,AST_NODE *next) {
  AST_NODE *node = ast_create(left,body, next, NODE_WHILE);
  return node;
}

AST_NODE * ast_func_def_create(AST_NODE *body,  AST_NODE *params, AST_NODE *next, DATA_TYPE return_type,
                               char *name, bool is_main, bool has_parens ) {
  AST_NODE *node = ast_create(body,NULL, next, NODE_FUNC_DEF);
  ast_name(node, name);
  node->third = params;
  node->data_type = return_type;
  node->is_main = is_main;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_param_create (DATA_TYPE type, char *name, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_PARAM);
  ast_name(node, name);
  node->data_type = type;
  return node;
}

AST_NODE *ast_for_create(AST_NODE *range, AST_NODE *body, AST_NODE *guards, AST_NODE *next, char *name) {
  AST_NODE *node= ast_create(range, body, next, NODE_FOR);
  ast_name(node, name);
  node->third = guards;
  return node;
}

AST_NODE *ast_range_create(AST_NODE *from, AST_NODE *to, AST_NODE *step, AST_NODE *next, bool inclusive) {
  AST_NODE *node = ast_create(from,to, next, NODE_RANGE);
  node->third = step;
  node->inclusive = inclusive;
  return node;

}


AST_NODE * ast_call_create(AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL,next, NODE_CALL);
  ast_name(node, name);
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_method_call_create(AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *reciever, AST_NODE *next) {
  AST_NODE *node = ast_create(reciever,NULL, next, NODE_CALL);
  node->name = name;
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *interpolades(AST_NODE *part, AST_NODE *next){
  AST_NODE *node=ast_create(part,NULL,next, NODE_INTERP);
  return node;
}


AST_NODE *ast_create_extension(AST_NODE *expr, AST_NODE *params,AST_NODE *next, char *name, bool has_parens, bool is_method, DATA_TYPE declared){
  AST_NODE *node = ast_create(expr, NULL, next, NODE_EXPR_STMT);
  node->third = params;
  node->name = name;
  node->is_method = is_method;
  node->data_type = declared;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_create_params_list_of_list(AST_NODE *next_param_list, int list_cnt) {
  AST_NODE *node = ast_create(NULL, NULL, next_param_list, NODE_LIST_OF_LIST);
  node->list_cnt = list_cnt;
  return node;
}

AST_NODE *ast_create_param_list(AST_NODE *params, AST_NODE *next, int param_cnt) {
  AST_NODE *node = ast_create(params,NULL, next, NODE_PARAM_LIST);
  node->list_param_cnt = param_cnt;
  return node;
}

AST_NODE *ast_append(AST_NODE *new_node, AST_NODE *next) {
  if (!new_node) return next;
  if (!next) return new_node;
  if (!next->next) {
    next->next = new_node;
  }
  else {
    ast_append(new_node, next->next);
  }
  return next;
}

void free_AST(AST_NODE *node) {
  if (!node) {return;}
  free_AST(node->left);
  free_AST(node->right);
  free_AST(node->next);
  free_AST(node->third);
  free(node->name);
  free(node->sval);
  free(node);
}


