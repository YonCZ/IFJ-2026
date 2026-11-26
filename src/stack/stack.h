/*
 * IFJ26 Compiler
 * File: stack.h
 * Author: Tadeáš Přikryl [xprikrt00]
 */

#ifndef STACK_H
#define STACK_H

#include "scanner/token.h"
#include <stdbool.h>

// Kind of one stack symbol.
typedef enum {
  STACK_TERMINAL,    // Input token.
  STACK_NONTERMINAL, // Reduced expression, holds an AST node.
  STACK_MARKER,      // '<' boundary before a reduction.
  STACK_EMPTY        // Value returned from an empty stack.
} StackKind;

// One symbol on the stack.
typedef struct {
  StackKind kind; // What the symbol is.
  Tokens token;   // Token type if terminal.
  void *node;     // AST node if nonterminal.
} StackSymbol;

// Dynamic stack.
typedef struct {
  StackSymbol *data; // Array of symbols.
  int top;           // Top index, -1 is empty.
  unsigned capacity; // Allocated size.
} Stack;

// Creates a new empty stack.
Stack *stack_init(void);

// Pushes a symbol on top.
void stack_push(Stack *s, StackSymbol sym);

// Removes and returns the top symbol, STACK_EMPTY symbol if empty.
StackSymbol stack_pop(Stack *s);

// Returns the top symbol without removing it, STACK_EMPTY symbol if empty.
StackSymbol stack_top(const Stack *s);

// Finds the topmost terminal, skips nonterminals and markers. STACK_EMPTY
// symbol if none.
StackSymbol stack_top_terminal(const Stack *s);

// Inserts the '<' marker right above the topmost terminal. Does nothing if
// there is no terminal.
void stack_insert_marker(Stack *s);

// Returns true if the stack is empty.
bool stack_is_empty(const Stack *s);

// Frees the stack.
void stack_dispose(Stack *s);

#endif
