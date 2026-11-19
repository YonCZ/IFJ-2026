/*
 * IFJ26 Compiler Implementation
 *
 * File: ast.h
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */


//comments made by Claude Sonnet 5.5 high
//input; Mám funkce v ast a chci abys mi udělal komentáře takové že budou vypadat jako stromy, left right, third_node, next a budou popisovat co se tam deje, samozrejme to bude pojmenovane podle parametru. Na ostatni parametry nez node se nedivej

#pragma once
#include <stdbool.h>

typedef enum{
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
  NODE_UNOP,  //!
  NODE_IDENT,
  NODE_LITERAL,

  //CONTROL EXTENSION
  NODE_FOR,
  NODE_RANGE,

  //INTERPOLADES
  NODE_INTERP,

  //PARAMLIST
  NODE_LIST_OF_LIST,
  NODE_PARAM_LIST,

  //built ins
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
}NODE_TYPE;

typedef struct AST_NODE AST_NODE;

typedef enum{
  DATA_NONE,
  DATA_INT,
  DATA_DOUBLE,
  DATA_STRING,
  DATA_BOOL,
  DATA_UNIT,
} DATA_TYPE;

typedef enum{
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

}OP_TYPE;


struct AST_NODE{
  NODE_TYPE type;
  AST_NODE *left;
  AST_NODE *right;
  AST_NODE *third;
  AST_NODE *next;

  bool is_const; //only for NODE_DEF_VAR
  int line;  //error line, PARSER will define it
  bool is_main;

  DATA_TYPE data_type;
  OP_TYPE op_type;

  bool is_extension; //extkw

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


  //params

  int list_no;
  int list_more_lists;

  char *name;
  bool inclusive;

  int id;

  long long ival;
  double dval;
  char *sval;
};


AST_NODE *ast_create(AST_NODE *left, AST_NODE *right,AST_NODE *next, NODE_TYPE type);
/*                   NODE (type) ---next--->
 *                 /      |      \
 *            /           |           \
 *        left          right         NULL
 *       (left)        (right)       (third)
 *
 * Základní konstruktor, ostatní funkce z něj vycházejí. Uzel je vynulovaný (calloc),
 * třetí větev (third) se nastavuje až v konkrétních konstruktorech.
 */
void ast_name(AST_NODE *node, char *name);
/* ast_name zkopíruje jméno do node->name (uzel pak vlastní vlastní kopii) */

AST_NODE * ast_int_literal_create( long long value,AST_NODE *next);
AST_NODE * ast_double_literal_create(double value,AST_NODE *next);
AST_NODE * ast_string_literal_create(char *value,AST_NODE *next);
AST_NODE * ast_bool_literal_create(bool value,AST_NODE *next);
AST_NODE * ast_unit_literal_create(AST_NODE *next);
/*                     LITERAL ---next--->
 *                 /      |      \
 *            /           |           \
 *        NULL          NULL          NULL
 *       (left)        (right)       (third)
 *
 * Literál nemá žádné potomky, jen hodnotu a typ:
 *   Int:    ival, data_type = DATA_INT
 *   Double: dval, data_type = DATA_DOUBLE
 *   String: sval (kopie), data_type = DATA_STRING
 *   Bool:   ival = 1 (true) nebo 0 (false), data_type = DATA_BOOL
 *   Unit:   bez hodnoty, data_type = DATA_UNIT
 */


AST_NODE * ast_binop_create(AST_NODE *left_term, AST_NODE *right_term, AST_NODE *next, OP_TYPE op);
/*                      BINOP ---next--->
 *                 /      |      \
 *            /           |           \
 *      left_term    right_term       NULL
 *       (left)        (right)       (third)
 *
 * op_type = op
 */

AST_NODE * ast_unop_create(AST_NODE *term, AST_NODE *next, OP_TYPE op);
/*                      UNOP ---next--->
 *                 /      |      \
 *            /           |           \
 *        term          NULL          NULL
 *       (left)        (right)       (third)
 *
 * op_type = op
 */

AST_NODE * ast_var_def_create(AST_NODE *term,AST_NODE *next, char *name, bool is_const, DATA_TYPE declared);
/*                     VAR_DEF ---next--->
 *                 /      |      \
 *            /           |           \
 *        term          NULL          NULL
 *       (left)        (right)       (third)
 *
 * name = jméno proměnné, is_const (val = true, var = false),
 * data_type = uvedený typ (DATA_NONE, když není uveden)
 */
AST_NODE * ast_if_create(AST_NODE *cond, AST_NODE *then,  AST_NODE *els,AST_NODE *next);
/*                       IF ---next--->
 *                 /      |      \
 *            /           |           \
 *        cond          then           els
 *       (left)        (right)       (third)
 *
 * els může být NULL (zkrácený if)
 */

AST_NODE * ast_while_create(AST_NODE *cond, AST_NODE *body,AST_NODE *next);
/*                      WHILE ---next--->
 *                 /      |      \
 *            /           |           \
 *        cond          body          NULL
 *       (left)        (right)       (third)
 */

AST_NODE * ast_func_def_create(AST_NODE *body,  AST_NODE *params, AST_NODE *next, DATA_TYPE return_type, char *name, bool is_main, bool has_parens);
/*                    FUNC_DEF ---next--->
 *                 /      |      \
 *            /           |           \
 *        body          NULL         params
 *       (left)        (right)       (third)
 *
 * name, data_type = návratový typ, is_main (@main), has_parens
 */

AST_NODE * ast_ident_create(AST_NODE *left, char *name, AST_NODE *next);
/*                      IDENT ---next--->
 *                 /      |      \
 *            /           |           \
 *        left          NULL          NULL
 *       (left)        (right)       (third)
 *
 * name = jméno identifikátoru
 */


AST_NODE * ast_call_create( AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *next);
/*                      CALL ---next--->
 *                 /      |      \
 *            /           |           \
 *        NULL          NULL         params
 *       (left)        (right)       (third)
 *
 * name = jméno funkce, is_method, has_parens;
 * params je řetěz argumentů spojených přes next
 */

//is_method
//yes - x.plus(12)
//no -  plus(x, 12)
AST_NODE *ast_method_call_create(AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *reciever, AST_NODE *next);
/*                      CALL ---next--->
 *                 /      |      \
 *            /           |           \
 *      reciever        NULL         params
 *       (left)        (right)       (third)
 *
 * reciever = příjemce metody (x v x.plus(12)); může být sám volání,
 * proto lze řetězit: number.floor.toInt
 * name, is_method, has_parens jako u ast_call_create
 */

AST_NODE *ast_param_create (DATA_TYPE type, char *name, AST_NODE *next);
/*                      PARAM ---next--->
 *                 /      |      \
 *            /           |           \
 *        NULL          NULL          NULL
 *       (left)        (right)       (third)
 *
 * name = jméno parametru, data_type = type
 */

AST_NODE *ast_assign_create(AST_NODE *var, AST_NODE *expr, AST_NODE *next);
/*                     ASSIGN ---next--->
 *                 /      |      \
 *            /           |           \
 *         var          expr          NULL
 *       (left)        (right)       (third)
 */

AST_NODE *ast_return_create(AST_NODE *expr, AST_NODE *next);
/*                     RETURN ---next--->
 *                 /      |      \
 *            /           |           \
 *        expr          NULL          NULL
 *       (left)        (right)       (third)
 *
 * expr je NULL u samostatného return (vrací ())
 */

AST_NODE *ast_for_create(AST_NODE *range, AST_NODE *body, AST_NODE *guards, AST_NODE *next, char *name);
/*                       FOR ---next--->
 *                 /      |      \
 *            /           |           \
 *        range         body         guards
 *       (left)        (right)       (third)
 *
 * range je uzel RANGE (viz níže):
 *     RANGE
 *    /  |  \
 *   /   |   \
 * from  to  step
 *
 * name = jméno řídicí proměnné cyklu, guards je řetěz stráží (výrazů) spojených přes next
 */

AST_NODE *ast_range_create(AST_NODE *from, AST_NODE *to, AST_NODE *step, AST_NODE *next, bool inclusive);
/*                      RANGE ---next--->
 *                 /      |      \
 *            /           |           \
 *        from           to           step
 *       (left)        (right)       (third)
 *
 * inclusive = true pro "to", false pro "until"; step je NULL, když "by" chybí
 */

AST_NODE *interpolades(AST_NODE *part, AST_NODE *next);
/*                     INTERP ---next--->
 *                 /      |      \
 *            /           |           \
 *        part          NULL          NULL
 *       (left)        (right)       (third)
 *
 * part je první část interpolovaného řetězce, další části navazují přes next
 * (text, $id nebo ${výraz}); data_type celého řetězce je String
 */

AST_NODE *ast_create_extension(AST_NODE *expr, AST_NODE *params,AST_NODE *next, char *name, bool has_parens, bool is_method, DATA_TYPE declared);
/*                    EXPR_STMT ---next--->
 *                 /      |      \
 *            /           |           \
 *        expr          NULL         params
 *       (left)        (right)       (third)
 *
 * uzel typu NODE_EXPR_STMT (ne FUNC_DEF)
 * name, has_parens, is_method, data_type = declared
 */


AST_NODE *ast_create_params_list_of_list(AST_NODE *next_param_list, int list_cnt);
/*                  LIST_OF_LIST ---next_param_list--->
 *                 /      |      \
 *            /           |           \
 *        NULL          NULL          NULL
 *       (left)        (right)       (third)
 *
 * v next je další seznam parametrů (next_param_list), list_no = list_cnt
 * (spojování seznamů přes ast_append)
 */

AST_NODE *ast_create_param_list(AST_NODE *params, AST_NODE *next, int param_cnt);
/*                   PARAM_LIST ---next--->
 *                 /      |      \
 *            /           |           \
 *       params         NULL          NULL
 *       (left)        (right)       (third)
 *
 * jeden seznam parametrů, další seznam navazuje přes next; list_more_lists = param_cnt
 */



AST_NODE *ast_append(AST_NODE *new_node, AST_NODE *next);
/*   new_node ---next3---> next2 ---next1---> ...
*/





void free_AST(AST_NODE *node);
