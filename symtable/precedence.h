/**
author: lucie alexandrova
login: xalexal00
part of project: ifj2026
*/
#ifndef PRECEDENCE_TABLE_H
#define PRECEDENCE_TABLE_H

#include "ast.h"
#include "token.h"
#include "symtable.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    REL_LT,  //< (shift)
    REL_GT,  //> (reduce)
    REL_EQ,  //= (match)
    REL_ERR  //(empty entry / syntax error 2)
} PrecedenceRel;

typedef enum {
    PSA_UN_PLUS = 0, //unary +
    PSA_UN_MINUS,    //unary -
    PSA_UN_NOT,      //unary !
    PSA_MUL,         //*
    PSA_DIV,         ///
    PSA_PLUS,        //binary +
    PSA_MINUS,       //binary -
    PSA_LT,          //<
    PSA_GT,          //>
    PSA_LEQ,         //<=
    PSA_GEQ,         //>=
    PSA_EQ,          //==
    PSA_NEQ,         //!=
    PSA_LPAREN,      //(
    PSA_RPAREN,      //)
    PSA_ID,          //identifier or literal
    PSA_DOLLAR,      //$ (end of expression)
    PSA_CONCAT,      //++ (list concatenation)
    PSA_AND,         //&&
    PSA_OR,          //||
    PSA_TO,          //to
    PSA_UNTIL,       //until
    PSA_BY,          //by
    PSA_TABLE_SIZE   //number of symbols (23)
} PSASymbol;

AST_NODE *precedence_parse(void);
AST_NODE *precedence_parse_arg(void);

#endif