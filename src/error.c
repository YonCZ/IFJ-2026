#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "error.h"

#define MAX_CLEANUPS 16

static cleanup cleanups[MAX_CLEANUPS];
static int cleanup_count = 0;

static const char* error_string(Error_state code) {
    switch (code) {
        case ERROR_LEX:
            return "Lexikalni chyba (chyba 1)";
        case ERROR_SYN:
            return "Syntakticka chyba (chyba 2)";
        case ERROR_SEM_UNDEF:
            return "Semanticka chyba - nedefinovany symbol (chyba 3)";
        case ERROR_SEM_REDEF:
            return "Semanticka chyba - redefinice symbolu (chyba 4)";
        case ERROR_SEM_ARG:
            return "Semanticka chyba - spatne argumenty (chyba 5)";
        case ERROR_SEM_TYPE:
            return "Semanticka chyba - typova nekompatibilita (chyba 6)";
        case ERROR_SEM_READ:
            return "Semanticka chyba - spatny zapis do parametru (chyba 7)";
        case ERROR_SEM_OTHER:
            return "Semanticka chyba - dlasi chyby (chyba 10)";
        case ERROR_INTERNAL:
            return "Interni chyba (chyba 99)";
        default: 
            return "Neznama chyba";
    }
}

void error_cleanup(cleanup st) {
    if (cleanup_count < MAX_CLEANUPS) {
        cleanups[cleanup_count++] = st;
    }
}

void error_exit(Error_state code, int line, const char *name, ...) {
    fprintf(stderr, "%s", error_string(code));
    if (line > 0) {
        fprintf(stderr, ", radek %d", line);
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