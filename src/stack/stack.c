/*
 * IFJ Implimentace prekladace
 * Soubor: stack.c
 * Autor: Tadeas Prikryl [xprikrt00]
 *
 * Prompt: "Precti si AGENTS.md, a na zaklade specifikace a zadani souboru,
 * udelej review meho kodu jestli splnuje vse co zadani pozaduje a jestli
 * moje reseni je spravne, bud mi dej psany review pokud to bude dobre
 * a nebo napis testy a ukaz mi kde na nich moje reseni selhava, prover
 * soubory stack.c a stack.h"
 */

#include <stdbool.h>
#include <stdlib.h>

#include "scanner/error.h"
#include "stack.h"

#define STACK_INIT_CAP 16

// Symbol returned when the stack is empty.
static StackSymbol stack_null_symbol(void) {
  return (StackSymbol){.kind = STACK_EMPTY, .token = 0, .node = NULL};
}

// Doubles the array when there is no room for one more symbol.
static void stack_grow(Stack *s) {
  if ((unsigned)(s->top + 1) < s->capacity) {
    return;
  }
  unsigned new_cap = s->capacity * 2;
  StackSymbol *tmp = realloc(s->data, new_cap * sizeof(StackSymbol));
  if (tmp == NULL) {
    error_exit(ERROR_INTERNAL, __LINE__, "stack: allocation failed");
  }
  s->data = tmp;
  s->capacity = new_cap;
}

// Creates a new empty stack.
Stack *stack_init(void) {
  Stack *s = malloc(sizeof(Stack));
  if (s == NULL) {
    error_exit(ERROR_INTERNAL, __LINE__, "stack: allocation failed");
  }
  s->data = malloc(STACK_INIT_CAP * sizeof(StackSymbol));
  if (s->data == NULL) {
    error_exit(ERROR_INTERNAL, __LINE__, "stack: allocation failed");
  }
  s->top = -1;
  s->capacity = STACK_INIT_CAP;
  return s;
}

// Pushes a symbol on top, grows the array if full.
void stack_push(Stack *s, StackSymbol sym) {
  stack_grow(s);
  s->data[++s->top] = sym;
}

// Removes and returns the top symbol, STACK_EMPTY symbol if empty.
StackSymbol stack_pop(Stack *s) {
  if (stack_is_empty(s)) {
    return stack_null_symbol();
  }
  return s->data[s->top--];
}

// Returns the top symbol without removing it, STACK_EMPTY symbol if empty.
StackSymbol stack_top(const Stack *s) {
  if (stack_is_empty(s)) {
    return stack_null_symbol();
  }
  return s->data[s->top];
}

// Finds the topmost terminal, skips nonterminals and markers. STACK_EMPTY
// symbol if none.
StackSymbol stack_top_terminal(const Stack *s) {
  for (int i = s->top; i >= 0; i--) {
    if (s->data[i].kind == STACK_TERMINAL) {
      return s->data[i];
    }
  }
  return stack_null_symbol();
}

// Inserts the '<' marker right above the topmost terminal.
void stack_insert_marker(Stack *s) {
  int i = s->top;
  while (i >= 0 && s->data[i].kind != STACK_TERMINAL) {
    i--;
  }
  if (i < 0) {
    return; // no terminal to anchor the marker to
  }
  stack_grow(s);
  for (int j = s->top; j > i; j--) {
    s->data[j + 1] = s->data[j];
  }
  s->data[i + 1] =
      (StackSymbol){.kind = STACK_MARKER, .token = 0, .node = NULL};
  s->top++;
}

// Returns true if the stack is empty.
bool stack_is_empty(const Stack *s) { return s->top == -1; }

// Frees the stack, not the AST nodes it may hold.
void stack_dispose(Stack *s) {
  if (s == NULL) {
    return;
  }
  free(s->data);
  free(s);
}
