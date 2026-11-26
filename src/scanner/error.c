/*
 * IFJ26 Compiler
 * File: error.c
 * Author: Polina Tishaninova [xtishap00]
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#define MAX_CLEANUPS 16

static cleanup cleanups[MAX_CLEANUPS];
static int cleanup_count = 0;

// function that returns the text for every error
static const char *error_string(Error_state code) {
  switch (code) {
  case ERROR_LEX:
    return "Lexical error (error 1)";
  case ERROR_SYN:
    return "Syntax error (error 2)";
  case ERROR_SEM_UNDEF:
    return "Semantic error - undefined symbol (error 3)";
  case ERROR_SEM_REDEF:
    return "Semantic error - symbol redefinition (error 4)";
  case ERROR_SEM_ARG:
    return "Semantic error - wrong arguments (error 5)";
  case ERROR_SEM_TYPE:
    return "Semantic error - type incompatibility (error 6)";
  case ERROR_SEM_READ:
    return "Semantic error - wrong write to a parameter (error 7)";
  case ERROR_SEM_OTHER:
    return "Semantic error - other errors (error 10)";
  case ERROR_INTERNAL:
    return "Internal error (error 99)";
  default:
    return "Unknown error";
  }
}

// function that registers a cleanup function
void error_cleanup(cleanup st) {
  if (cleanup_count < MAX_CLEANUPS) {
    cleanups[cleanup_count++] = st;
  }
}

// function that prints the error and exits
void error_exit(Error_state code, int line, const char *name, ...) {
  fprintf(stderr, "%s", error_string(code));
  if (line > 0) {
    fprintf(stderr, ", line %d", line);
  }
  fprintf(stderr, ": ");

  va_list args;
  va_start(args, name);
  vfprintf(stderr, name, args);
  va_end(args);

  fprintf(stderr, "\n");

  int n = cleanup_count;
  cleanup_count = 0;
  for (int i = n - 1; i >= 0; i--) {
    cleanups[i]();
  }
  exit((int)code);
}
