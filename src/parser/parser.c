/*
 * IFJ Implimentace prekladace
 * Soubor: parser.c
 * Autor: Tadeas Prikryl [xprikrt00]
 *
 * Prompt: "Precti si AGENTS.md, a na zaklade specifikace a zadani souboru,
 * udelej review meho kodu jestli splnuje vse co zadani pozaduje a jestli
 * moje reseni je spravne, bud mi dej psany review pokud to bude dobre
 * a nebo napis testy a ukaz mi kde na nich moje reseni selhava, prover
 * soubory parser.c a parser.h"
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner/scanner.h"
#include "scanner/token.h"
#include "scanner/error.h"

// All tokens of the input, loaded once at the start.
static Token *tokens;
static int    ntokens;

// Cursor of the parser, index into tokens.
static int pos;

// Loads all tokens from f into the array, including EOF.
static void load_tokens(FILE *f) {
    int cap = 128;
    tokens = malloc(cap * sizeof(Token));
    if (tokens == NULL) {
        error_exit(ERROR_INTERNAL, 0, "parser: allocation failed");
    }
    ntokens = 0;
    for (;;) {
        if (ntokens == cap) {
            cap *= 2;
            Token *tmp = realloc(tokens, cap * sizeof(Token));
            if (tmp == NULL) {
                error_exit(ERROR_INTERNAL, 0, "parser: allocation failed");
            }
            tokens = tmp;
        }
        tokens[ntokens++] = get_token(f);
        if (tokens[ntokens - 1].type == EOF_TOKEN) {
            break;
        }
    }
}

// Returns the current token without consuming it.
static inline Token parser_peek(void) {
    return tokens[pos];
}

// Returns the current token and moves the cursor forward.
static inline Token parser_advance(void) {
    return tokens[pos++];
}

// Consumes the current token if it has type t, error 2 otherwise.
static inline Token parser_expect(Tokens t) {
    Token tok = parser_peek();
    if (tok.type != t) {
        error_exit(ERROR_SYN, tok.line, "syntax error");
    }
    parser_advance();
    return tok;
}

// Frees all tokens and their attributes. Idempotent.
static void parser_cleanup(void) {
    for (int i = 0; i < ntokens; i++) {
        free(tokens[i].atribute);
    }
    free(tokens);
    tokens = NULL;
    ntokens = 0;
    pos = 0;
}

#ifdef PARSER_DEBUG
// Prints all loaded tokens to stderr.
static void debug_print_tokens(void) {
    for (int i = 0; i < ntokens; i++) {
        fprintf(stderr, "%4d: type=%-3d line=%-4d attr=%s\n",
                i, tokens[i].type, tokens[i].line,
                tokens[i].atribute ? tokens[i].atribute : "-");
    }
}
#endif

// Prolog → IMPORT ID DOT MUL EOL
// "ifj26" check is strcmp on the attribute, the grammar cannot
// express a specific identifier value.
static void parse_prolog(void) {
    parser_expect(IMPORT_TOKEN);
    Token id = parser_expect(IDENTIFIER_TOKEN);
    if (id.atribute == NULL || strcmp(id.atribute, "ifj26") != 0) {
        error_exit(ERROR_SYN, id.line, "expected ifj26");
    }
    parser_expect(DOT_TOKEN);
    parser_expect(MUL_TOKEN);
    parser_expect(EOL_TOKEN);
}

// Parses the input from stdin, returns the AST root.
AST_NODE *parse_program(void) {
    error_cleanup(parser_cleanup);
    load_tokens(stdin);

#ifdef PARSER_DEBUG
    debug_print_tokens();
#endif

    parse_prolog();
    parser_cleanup();
    return NULL;
}
