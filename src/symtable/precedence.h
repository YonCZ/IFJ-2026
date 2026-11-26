/**
IFJ26 Compiler 
Author: Lucie Alexandrova [xalexal00]
File: precedence.h
*/
#ifndef PRECEDENCE_TABLE_H
#define PRECEDENCE_TABLE_H

#include "ast.h"
#include "symtable.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum {
  REL_LT, // <  (Shift / natlačení na zásobník)
  REL_GT, // >  (Reduce / redukce podle pravidla)
  REL_EQ, // =  (Match / spárování závorek)
  REL_ERR //    (Prázdné pole / syntaktická chyba 2)
} PrecedenceRel;

typedef enum {
  PSA_UN_PLUS = 0, // unární +
  PSA_UN_MINUS,    // unární -
  PSA_UN_NOT,      // unární !
  PSA_MUL,         // *
  PSA_DIV,         // /
  PSA_PLUS,        // binární +
  PSA_MINUS,       // binární -
  PSA_LT,          // <
  PSA_GT,          // >
  PSA_LEQ,         // <=
  PSA_GEQ,         // >=
  PSA_EQ,          // ==
  PSA_NEQ,         // !=
  PSA_LPAREN,      // (
  PSA_RPAREN,      // )
  PSA_ID,          // identifikátor nebo literál
  PSA_DOLLAR,      // $ (konec výrazu)
  PSA_CONCAT,      // ++ (konkatenace seznamů)
  PSA_AND,         // &&
  PSA_OR,          // ||
  PSA_TO,          // to
  PSA_UNTIL,       // until
  PSA_BY,          // by
  PSA_TABLE_SIZE   // počet symbolů (23)
} PSASymbol;

AST_NODE *precedence_parse(void);
AST_NODE *precedence_parse_arg(void);

#endif
