#ifndef ERROR_H
#define ERROR_H

typedef enum {
    ERROR_LEX = 1,
    ERROR_SYN = 2,
    ERROR_SEM_UNDEF = 3,
    ERROR_SEM_REDEF = 4,
    ERROR_SEM_ARG = 5,
    ERROR_SEM_TYPE = 6,
    ERROR_SEM_READ = 7,
    ERROR_SEM_OTHER = 10,
    ERROR_INTERNAL = 99
} Error_state;

typedef void (*cleanup)(void);

void error_cleanup(cleanup st);
void error_exit(Error_state code, int line, const char *name, ...);

#endif