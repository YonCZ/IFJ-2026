/*
 * IFJ26 Compiler
 * File: ast.h
 * Author: Pavel Novák (xnovakp01)
 */

// input: Mám funkce v ast a chci abys mi udělal komentáře takové že budou
// vypadat jako stromy, left right, third_node, next a budou popisovat co se tam
// deje, samozrejme to bude pojmenovane podle parametru. Na ostatni parametry
// nez node se nedivej

#pragma once
#include <stdbool.h>

typedef enum {
  NODE_NONE,
  NODE_PROGRAM,
  NODE_FUNC_DEF,
  NODE_PARAM,
  NODE_BLOCK,

  NODE_VAR_DEF,
  NODE_ASSIGN,
  NODE_IF,
  NODE_WHILE,
  NODE_RETURN,
  NODE_CALL,
  NODE_EXPR_STMT,

  NODE_BINOP,
  NODE_UNOP, //!
  NODE_IDENT,
  NODE_LITERAL,

  // CONTROL EXTENSION
  NODE_FOR,
  NODE_RANGE,

  // INTERPOLADES
  NODE_INTERP,

  // PARAMLIST
  NODE_LIST_OF_LIST,
  NODE_PARAM_LIST,

  // ALGEBDT
  NODE_MATCH,
  NODE_CASE,
  NODE_WILDCARD,
  NODE_TYPE_DEF,
  NODE_CASE_CLASS,

  NODE_TYPE_REF,

  // built ins
  print,
  read_line,
  read_int,
  read_double,
  to_str,
  to_int,
  str_length,
  sub_str,
  ord,
  chr,
  read_boolean
} NODE_TYPE;

typedef struct AST_NODE AST_NODE;

typedef enum {
  DATA_NONE,
  DATA_INT,
  DATA_DOUBLE,
  DATA_STRING,
  DATA_BOOL,
  DATA_UNIT,
  DATA_LIST,
  DATA_UNION,
  DATA_CLASS,
} DATA_TYPE;

typedef enum {
  OP_NONE,
  OP_ADD,
  OP_SUB,
  OP_GT,
  OP_AND,
  OP_OR,
  OP_MUL,
  OP_DIV,
  OP_LT,
  OP_GE,
  OP_EQ,
  OP_NEQ,
  OP_NOT,

  OP_LE,

  OP_TO,
  OP_UNTIL,
  OP_BY,
} OP_TYPE;

struct AST_NODE {
  NODE_TYPE type;
  AST_NODE *left;
  AST_NODE *right;
  AST_NODE *third;
  AST_NODE *next;
  AST_NODE *type_ref;
  AST_NODE *arg_groups;

  bool is_const; // only for NODE_DEF_VAR
  int line;      // error line, PARSER will define it
  bool is_main;

  DATA_TYPE data_type;
  DATA_TYPE elem_type;
  OP_TYPE op_type;

  bool is_extension; // extkw
  bool is_index;
  bool is_ctor;
  bool is_static_call;
  bool is_field;
  bool is_binding;
  bool is_function_ref;
  int field_index;
  bool int_to_double;

  /*
   *is_method=true
   *has_parens=false
   *
   * x.toString
   */
  bool is_method;

  /*
   *is_method=true
   *has_paresn=true
   *
   * x.plus(12)
   */
  bool has_parens;

  // params

  int param_groups;
  int param_cnt;

  char *name;
  bool inclusive;

  int id;

  long long ival;
  double dval;
  char *sval;
};

AST_NODE *ast_create(AST_NODE *left, AST_NODE *right, AST_NODE *next,
                     NODE_TYPE type);
char *ast_type_key(const AST_NODE *type);
char *ast_method_key(const char *name, const AST_NODE *receiver);
void ast_name(AST_NODE *node, char *name);

AST_NODE *ast_int_literal_create(long long value, AST_NODE *next);
AST_NODE *ast_double_literal_create(double value, AST_NODE *next);
AST_NODE *ast_string_literal_create(char *value, AST_NODE *next);
AST_NODE *ast_bool_literal_create(bool value, AST_NODE *next);
AST_NODE *ast_unit_literal_create(AST_NODE *next);

AST_NODE *ast_binop_create(AST_NODE *left_term, AST_NODE *right_term,
                           AST_NODE *next, OP_TYPE op);
/*        BINOP ---next--->
 *        /   \
 *       /     \
 *  left_term  right_term
 */

AST_NODE *ast_unop_create(AST_NODE *term, AST_NODE *next, OP_TYPE op);
/*   UNOP ---next--->
 *     |
 *    term
 */

AST_NODE *ast_var_def_create(AST_NODE *term, AST_NODE *next, char *name,
                             bool is_const, DATA_TYPE declared);
/*   VAR_DEF ---next--->
 *     |
 *    term
 */
AST_NODE *ast_if_create(AST_NODE *cond, AST_NODE *then, AST_NODE *els,
                        AST_NODE *next);
/*             IF ---next--->
 *          /  |  \
 *         /   |   \
 *      cond  then  els
 */

AST_NODE *ast_while_create(AST_NODE *cond, AST_NODE *body, AST_NODE *next);
/*        WHILE ---next--->
 *        /   \
 *       /     \
 *    cond     body
 */

AST_NODE *ast_func_def_create(AST_NODE *body, AST_NODE *params, AST_NODE *next,
                              DATA_TYPE return_type, char *name, bool is_main,
                              bool has_parens);
/*        FUNC_DEF ---next--->
 *        /    \
 *       /      \
 *    body    params
 */

AST_NODE *ast_ident_create(AST_NODE *left, char *name, AST_NODE *next);
/*       identifikator ---next--->
 *        /    \
 *       /      \
 *    literal
 */

AST_NODE *ast_call_create(AST_NODE *params, char *name, bool is_method,
                          bool has_parens, AST_NODE *next);
/*   CALL ---next--->
 *          /  |  \
 *         /   |   \
 *      call params
 */

// is_method
// yes - x.plus(12)
// no -  plus(x, 12)
AST_NODE *ast_method_call_create(AST_NODE *params, char *name, bool is_method,
                                 bool has_parens, AST_NODE *reciever,
                                 AST_NODE *next);
/*   CALL ---next--->
 *          /  |
 *         /   |
 *      call params
 *       /
 *      /
 *     ....
 *     /
 *    /
 *   reciever
 */

AST_NODE *ast_param_create(DATA_TYPE type, char *name, AST_NODE *next);
/*   PARAM ---next--->
 */

AST_NODE *ast_assign_create(AST_NODE *var, AST_NODE *expr, AST_NODE *next);
/*        ASSIGN ---next--->
 *        /    \
 *       /      \
 */

AST_NODE *ast_return_create(AST_NODE *expr, AST_NODE *next);
/*   RETURN ---next--->
 *     |
 *    expr
 */

AST_NODE *ast_for_create(AST_NODE *range, AST_NODE *body, AST_NODE *guards,
                         AST_NODE *next, char *name);
/*             FOR ---next--->
 *          /   |    \
 *         /    |     \
 *     range  body   guards
 *    /  |  \
 *   /   |   \
 * from  to  step
 */

AST_NODE *ast_range_create(AST_NODE *from, AST_NODE *to, AST_NODE *step,
                           AST_NODE *next, bool inclusive);
/*             RANGE ---next--->
 *           /   |   \
 *          /    |    \
 *       from   to    step
 */

AST_NODE *ast_create_params_list_of_list(AST_NODE *next_param_list,
                                         int list_cnt);
/*
 * using ast_append for list linking
 *
 *             third ---> next_param_list--->..
 */

AST_NODE *ast_create_param_list(AST_NODE *params, AST_NODE *next,
                                int param_cnt);
/*             list ---next--->
 *           /   |   \
 *          /    |    \
 *       params
 */

AST_NODE *ast_type_ref_create(DATA_TYPE base, char *name, AST_NODE *next);

AST_NODE *ast_list_type_ref_create(AST_NODE *elem, AST_NODE *next);

AST_NODE *ast_union_type_ref_create(AST_NODE *left, AST_NODE *right,
                                    AST_NODE *next);

AST_NODE *ast_type_def_create(char *name, AST_NODE *type, AST_NODE *next);

AST_NODE *ast_case_class_create(char *name, AST_NODE *params, AST_NODE *next);

AST_NODE *ast_match_create(AST_NODE *subject, AST_NODE *cases, AST_NODE *next);

AST_NODE *ast_case_create(AST_NODE *pattern, AST_NODE *guard, AST_NODE *body,
                          AST_NODE *next);

AST_NODE *ast_wildcard_create(AST_NODE *next);

AST_NODE *ast_append(AST_NODE *new_node, AST_NODE *next);
/*   new_node ---next3---> next2 ---next1---> ...
 */

void free_AST(AST_NODE *node);
void ast_free_type_refs(void);
