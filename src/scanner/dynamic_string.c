/*
 * IFJ Implimentace prekladace
 * Soubor: dynamic_string.c
 * Autor: Polina Tihaninova [xtishap00]
 */

#include "dynamic_string.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

// funkce ktera inicializuje funkce stringu, da kapacitu 32 a delku 0, vyhradi
// pamet pres malloc, kdyz ne tak interni chyba 99
void ds_init(DString *s) {
  // kapacita 32
  s->cap = 32;
  // delka 0
  s->len = 0;
  s->data = malloc(s->cap);
  if (!s->data) {
    error_exit(ERROR_INTERNAL, 0, "Alokace selhala");
  }
  s->data[0] = '\0';
}

// funkce ktera uvolnuje data
void ds_free(DString *s) {
  // pokud tam jsou data tak je muzeme uvolnit
  if (s->data) {
    free(s->data);
    // vynulovani pointeru
    s->data = NULL;
  }
  // jinak delka a kapacita se nastavi jako nula
  s->len = 0;
  s->cap = 0;
}

// funkce ktera jenom vycisti string, ale pamet se ponecha
void ds_clear(DString *s) {
  s->len = 0;
  if (s->data) {
    // strci naprvni misto ukoncovaci znak
    s->data[0] = '\0';
  }
}

// funkce ktera pushuje char nakonec
void ds_push_char(DString *s, char c) {
  // kdyz uz se to tam nevede, musime zvetsit kapacitu
  if (s->len + 1 >= s->cap) {
    s->cap *= 2;
    // zvetseni pameti
    char *new_data = realloc(s->data, s->cap);
    // kontrola reallocu, jinak interni chyba
    if (!new_data) {
      error_exit(ERROR_INTERNAL, 0, "Realokace selhala");
    }
    s->data = new_data;
  }
  // prida se znak a zvets se delka a soupne posunuty ukoncovaci znak
  s->data[s->len++] = c;
  s->data[s->len] = '\0';
}

// funkce ktera vyrobi novou kopii stringu
char *ds_dup(const DString *s) {
  // alokujeme nove misto
  char *dup = malloc(s->len + 1);
  // kontrola mallocu, jinak interni chyba 99
  if (!dup) {
    error_exit(ERROR_INTERNAL, 0, "Dup selhal - chyba alokace");
  }
  // kopiruje se pamet do noveho mista
  memcpy(dup, s->data, s->len + 1);
  return dup;
}
