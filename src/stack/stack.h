/*
 * Author: Tadeas Prikryl (xprikrt00)
 * IFJ26 Compiler - Stack for precedence analysis
 */

#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include "scanner/token.h"

// One symbol on the stack.
typedef struct {
    bool   is_terminal; // True if terminal.
    Tokens token;       // Token type if terminal.
    void  *node;        // AST node if nonterminal.
} StackSymbol;

// Dynamic stack.
typedef struct {
    StackSymbol *data;     // Array of symbols.
    int          top;      // Top index, -1 is empty.
    unsigned     capacity; // Allocated size.
} Stack;

// Creates a new empty stack.
Stack *stack_init(void);

// Pushes a symbol on top.
void stack_push(Stack *s, StackSymbol sym);

// Removes and returns the top symbol.
StackSymbol stack_pop(Stack *s);

// Returns the top symbol without removing it.
StackSymbol stack_top(const Stack *s);

// Finds the topmost terminal, skips nonterminals.
StackSymbol stack_top_terminal(const Stack *s);

// Returns true if the stack is empty.
bool stack_is_empty(const Stack *s);

// Frees the stack.
void stack_dispose(Stack *s);

#endif
