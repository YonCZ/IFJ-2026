/*
 * IFJ26 Compiler
 * File: error.h
 * Author: Polina Tihaninova [xtishap00]
 */

#ifndef ERROR_H
#define ERROR_H

// list of all errors
typedef enum {
  // lexical error
  ERROR_LEX = 1,
  // syntax error
  ERROR_SYN = 2,
  // semantic error -> use of an undefined function, variable or type
  ERROR_SEM_UNDEF = 3,
  // semantic error -> not allowed redefinition of a function, variable, type
  ERROR_SEM_REDEF = 4,
  // semantic error -> wrong number or type of arguments in a function call
  ERROR_SEM_ARG = 5,
  // semantic error -> type incompatibility or type inference in an expression
  ERROR_SEM_TYPE = 6,
  // semantic error -> trying to write to a variable or parameter that cannot
  // be changed
  ERROR_SEM_READ = 7,
  // other semantic errors (not allowed return, non-exhaustive match)
  ERROR_SEM_OTHER = 10,
  // internal error
  ERROR_INTERNAL = 99
} Error_state;

typedef void (*cleanup)(void);
// call for cleanup =
void error_cleanup(cleanup st);

// call for printing errors
void error_exit(Error_state code, int line, const char *name, ...);

#endif
