#include "dynamic_string.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

void ds_init(DString *s) {
    s->cap = 32;
    s->len = 0;
    s->data = malloc(s->cap);
    if (!s->data) {
        error_exit(ERROR_INTERNAL, 0, "Alokace selhala");
    }
    s->data[0] = '\0';
}

void ds_free(DString *s) {
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
    s->cap = 0;
}

void ds_clear(DString *s) {
    s->len = 0;
    if (s->data) {
        s->data[0] = '\0';
    }
}

void ds_push_char(DString *s, char c) {
    if (s->len + 1 >= s->cap) {
        s->cap *= 2;
        char *new_data = realloc(s->data, s->cap);
        if (!new_data) {
            error_exit(ERROR_INTERNAL, 0, "Realokace selhala");
        }
        s->data = new_data;
    }
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
}

void ds_push_str(DString *s, const char *str) {
    while (*str) {
        ds_push_char(s, *str++);
    }
}

char *ds_dup(const DString *s) {
    char *dup = malloc(s->len + 1);
    if (!dup) {
        error_exit(ERROR_INTERNAL, 0, "Dup selhal - chyba alokace");
    }
    memcpy(dup, s->data, s->len + 1);
    return dup;
}
