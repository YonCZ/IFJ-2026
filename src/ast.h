//
// Created by Pavel Novák on 28.10.2026.
//
#pragma once
#include <stdbool.h>



/*
pomocna vec od clauda
NODE_LITERAL	dtype + ival / fval / sval
NODE_IDENT	    name
NODE_BINOP	    op, left, right
NODE_UNOP	    op, left
NODE_VAR_DEF	name, is_const, left (inicializace)
NODE_ASSIGN	    left (IDENT), right (výraz)
NODE_IF	left    (podmínka), right (then), third (else)
NODE_WHILE      left (podmínka), right (tělo)
NODE_RETURN	    left (výraz nebo NULL)
NODE_CALL	    name, left (první argument, další přes next)
NODE_FUNC_DEF	name, left (parametry přes next), right (BLOCK)
NODE_BLOCK	    left (první příkaz, další přes next)
*/


typedef enum{
  NODE_PROGRAM,   //definition of variables at the start is in *next
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
  NODE_LITERAL,
  NODE_IDENT,

  //CONTROL EXTENSION
  NODE_FOR,


  //INTERPOLADES
  NODE_INTERP,

}NODE_TYPE;

typedef struct AST_NODE AST_NODE;

typedef enum{
  DATA_INT,
  DATA_DOUBLE,
  DATA_STRING,
  DATA_BOOL,
  DATA_UNIT,
} DATA_TYPE;

typedef enum{
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
  AST_NODE *else_param;
  AST_NODE *next;  //only for if (else) and for var decleration at the start fo function

  bool is_const; //only for NODE_DEF_VAR
  int line;  //error line


  DATA_TYPE data_type;
  OP_TYPE op_type;

  char *name;


  long long ival;
  double dval;
  char *sval;
};


AST_NODE *ast_create(AST_NODE *left, AST_NODE *right,AST_NODE *next, NODE_TYPE type);
void ast_name(AST_NODE *node, char *name);

AST_NODE * ast_int_literal_create( long long value,AST_NODE *next);
AST_NODE * ast_double_literal_create(double value,AST_NODE *next);
AST_NODE * ast_string_literal_create(char *value,AST_NODE *next);
AST_NODE * ast_ident_create(AST_NODE *left, char *name,AST_NODE *next);
AST_NODE * ast_binop_create(AST_NODE *lefr, AST_NODE *right, AST_NODE *next, OP_TYPE op);
AST_NODE * ast_unop_create(AST_NODE *left, AST_NODE *next, OP_TYPE op);
AST_NODE * ast_var_def_create(AST_NODE *left,AST_NODE *next, char *name, bool is_const);
AST_NODE * ast_if_create(AST_NODE *cond, AST_NODE *then,  AST_NODE *els,AST_NODE *next);
AST_NODE * ast_while_create(AST_NODE *left, AST_NODE *right,AST_NODE *next);
AST_NODE * ast_func_def_create(AST_NODE *body, AST_NODE *return_type,  AST_NODE *params,AST_NODE *next, char *name );
AST_NODE * ast_call_create( AST_NODE *params, char *name);

//!!!!!! TODO OSTATNI FUNKCE TADY NEMAJI ZASTOUPENI
void free_AST(AST_NODE *node);

void print_AST(AST_NODE *node);
