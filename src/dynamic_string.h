#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H
#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} DString;

void ds_init(DString *s);
void ds_free(DString *s);
void ds_clear(DString *s);
void ds_push_char(DString *s, char c);
void ds_push_str(DString *s, const char *str);
char *ds_dup(const DString *s);

#endif