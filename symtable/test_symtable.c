/*
 * Projekt IFJ26 - test tabulky symbolů (symtable)
 * Autoři: <jména a loginy členů týmu, kteří soubor upravovali>
 *
 * Kód vygenerovala AI: Claude Sonnet 5.5 (Anthropic).
 * Prompt: <vlož sem přesné znění svého promptu>
 *
 * Test používá jen chování tabulky (vlož / najdi / zruš), nikoli její
 * vnitřní podobu, takže funguje pro vv-BVS i TRP-izp. Všechno, co závisí na
 * tvém rozhraní, je v bloku ADAPTÉR - hledej značky ">>> UPRAV".
 *
 * Použití:
 *   ./test_symtable                všechny testy, každý ve vlastním procesu
 *   ./test_symtable many           jen testy, jejichž jméno obsahuje "many"
 *   ./test_symtable --nofork       bez fork/alarm (pro valgrind)
 *
 * Doporučený postup:
 *   gcc -g -Wall -Wextra -std=c11 -o test_symtable test_symtable.c symtable.c ...
 *   ./test_symtable
 *   valgrind --leak-check=full --error-exitcode=1 ./test_symtable --nofork
 *
 * Volitelné části (zapínají se přes -D při překladu):
 *   -DHAVE_REMOVE    tabulka umí mazat jednotlivé klíče (doplň st_remove)
 *   -DHAVE_COUNT     tabulka umí vrátit počet prvků (doplň st_count)
 *   -DHAVE_HEIGHT    varianta vv-BVS: umíš zjistit výšku stromu (doplň st_height)
 *   -DTEST_NULL      odolnost proti NULL (st_destroy(NULL) apod.)
 *   -DBIG_N=100000   počet prvků ve velkých testech
 *   -DTEST_TIMEOUT=10  limit na jeden test v sekundách
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "symtable.h"

#ifndef BIG_N
#define BIG_N 100000
#endif
#ifndef TEST_TIMEOUT
#define TEST_TIMEOUT 10
#endif

/* ========================================================================
 * ADAPTÉR - přizpůsob svému symtable.h
 *
 * Test potřebuje jen čtyři operace. K položce si ukládá jedno číslo `id`
 * a pak ověřuje, že se vrátilo správné - vyber libovolné int pole, které
 * tvoje položka má (nebo si ho přidej).
 * ====================================================================== */

/* >>> UPRAV 1/4: typ tabulky */
typedef symtable_t ST;

/* >>> UPRAV 2/4: vytvoření a zrušení tabulky */
static ST *st_create(void) { return symtable_init(); }
static void st_destroy(ST *t) { symtable_free(t); }

/* >>> UPRAV 3/4: vložení klíče.
 * Musí vrátit: PUT_OK     - vloženo
 *              PUT_EXISTS - klíč už v tabulce je (to je redefinice -> chyba 4)
 *              PUT_ERR    - chyba (např. paměti)
 * `id` ulož do libovolného int pole položky. */
enum { PUT_OK = 0, PUT_EXISTS = 1, PUT_ERR = -1 };
static int st_put(ST *t, const char *key, int id) {
  symbol_t sym = {0};
  sym.id = id; /* >>> UPRAV: pole tvé struktury položky */
  int rc = symtable_insert(t, key, &sym);
  return rc == 0 ? PUT_OK : (rc == 1 ? PUT_EXISTS : PUT_ERR);
}

/* >>> UPRAV 4/4: vyhledání. Vrací 1 a zapíše *id, pokud klíč existuje; jinak 0 */
static int st_get(ST *t, const char *key, int *id) {
  symbol_t *s = symtable_search(t, key);
  if (!s)
    return 0;
  *id = s->id; /* >>> UPRAV: totéž pole jako výše */
  return 1;
}

#ifdef HAVE_REMOVE
/* >>> UPRAV (volitelné): vrací 1 = smazáno, 0 = klíč v tabulce nebyl */
static int st_remove(ST *t, const char *key) { return symtable_remove(t, key); }
#endif
#ifdef HAVE_COUNT
/* >>> UPRAV (volitelné): počet prvků v tabulce */
static size_t st_count(ST *t) { return symtable_count(t); }
#endif
#ifdef HAVE_HEIGHT
/* >>> UPRAV (volitelné, vv-BVS): výška stromu; prázdný = 0, jediný uzel = 1.
 * Pokud počítáš hrany (jediný uzel = 0), test má rezervu +1. */
static int st_height(ST *t) { return symtable_height(t); }
#endif
/* ====================================================================== */

/* ---- pomocné věci ------------------------------------------------------ */

static FILE *g_report; /* kam se píšou hlášení o selhání */

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(g_report, "    %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond);  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECKF(cond, ...)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(g_report, "    %s:%d: CHECK(%s)\n      ", __FILE__, __LINE__,    \
              #cond);                                                          \
      fprintf(g_report, __VA_ARGS__);                                          \
      fprintf(g_report, "\n");                                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int has(ST *t, const char *key, int want_id) {
  int id;
  return st_get(t, key, &id) && id == want_id;
}

static int absent(ST *t, const char *key) {
  int id;
  return !st_get(t, key, &id);
}

static void mkkey(char *buf, size_t size, int i) {
  snprintf(buf, size, "k%07d", i);
}

static unsigned g_rng = 2463534242u;
static unsigned rnd(void) { /* xorshift32, deterministické */
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

/* 0 = vzestupně, 1 = sestupně, 2 = náhodně (zamíchané) */
static int *make_order(int n, int mode) {
  int *o = malloc((size_t)n * sizeof *o);
  if (!o)
    return NULL;
  for (int i = 0; i < n; i++)
    o[i] = (mode == 1) ? n - 1 - i : i;
  if (mode == 2)
    for (int i = n - 1; i > 0; i--) {
      int j = (int)(rnd() % (unsigned)(i + 1));
      int tmp = o[i];
      o[i] = o[j];
      o[j] = tmp;
    }
  return o;
}

/* Následující permutace v lexikografickém pořadí; 0 = už byla poslední. */
static int next_perm(char *s, size_t n) {
  size_t i = n - 1;
  while (i > 0 && s[i - 1] >= s[i])
    i--;
  if (i == 0)
    return 0;
  size_t j = n - 1;
  while (s[j] <= s[i - 1])
    j--;
  char tmp = s[i - 1];
  s[i - 1] = s[j];
  s[j] = tmp;
  for (size_t l = i, r = n - 1; l < r; l++, r--) {
    tmp = s[l];
    s[l] = s[r];
    s[r] = tmp;
  }
  return 1;
}

/* ========================================================================
 * Základní operace
 * ====================================================================== */

static int t_create_destroy_empty(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  st_destroy(t);
  return 0;
}

static int t_empty_lookup(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(absent(t, "x"));
  CHECK(absent(t, "main"));
  CHECK(absent(t, ""));
  st_destroy(t);
  return 0;
}

static int t_single(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_put(t, "x", 42) == PUT_OK);
  CHECK(has(t, "x", 42));
  CHECK(absent(t, "y"));
  st_destroy(t);
  return 0;
}

static int t_many_identifiers(void) {
  static const char *names[] = {
      "x",      "y",       "result", "i",      "n",      "factorial",
      "runFoo", "readInt", "print",  "_tmp",   "a1",     "A1",
      "z_9",    "main",    "isEven", "isOdd",  "helper", "longerIdentifierName"};
  const int cnt = (int)(sizeof names / sizeof names[0]);
  ST *t = st_create();
  CHECK(t != NULL);
  for (int i = 0; i < cnt; i++)
    CHECKF(st_put(t, names[i], i + 1) == PUT_OK, "put %s", names[i]);
  for (int i = 0; i < cnt; i++)
    CHECKF(has(t, names[i], i + 1), "find %s", names[i]);
  CHECK(absent(t, "nope"));
  CHECK(absent(t, "resul"));
  CHECK(absent(t, "results"));
  CHECK(absent(t, "X"));
  st_destroy(t);
  return 0;
}

static int t_similar_keys(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_put(t, "abc", 1) == PUT_OK);
  CHECK(absent(t, "ab"));   /* prefix */
  CHECK(absent(t, "abcd")); /* rozšíření */
  CHECK(absent(t, "abd"));
  CHECK(absent(t, "ABC"));
  CHECK(absent(t, "cba")); /* anagram */
  CHECK(absent(t, "bc"));
  CHECK(absent(t, ""));
  CHECK(st_put(t, "ab", 2) == PUT_OK);
  CHECK(st_put(t, "abcd", 3) == PUT_OK);
  CHECK(has(t, "abc", 1));
  CHECK(has(t, "ab", 2));
  CHECK(has(t, "abcd", 3));
  st_destroy(t);
  return 0;
}

static int t_case_sensitive(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_put(t, "x", 1) == PUT_OK);
  CHECK(st_put(t, "X", 2) == PUT_OK);
  CHECK(st_put(t, "int", 3) == PUT_OK);
  CHECK(st_put(t, "Int", 4) == PUT_OK);
  CHECK(has(t, "x", 1));
  CHECK(has(t, "X", 2));
  CHECK(has(t, "int", 3));
  CHECK(has(t, "Int", 4));
  st_destroy(t);
  return 0;
}

/* Redefinice musí být rozpoznána (chyba 4) a nesmí přepsat původní položku. */
static int t_duplicate_insert(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_put(t, "x", 1) == PUT_OK);
  CHECK(st_put(t, "x", 2) == PUT_EXISTS);
  CHECK(has(t, "x", 1)); /* původní hodnota zůstala */
  for (int i = 0; i < 500; i++) {
    char key[32];
    mkkey(key, sizeof key, i);
    CHECK(st_put(t, key, i + 100) == PUT_OK);
  }
  CHECK(st_put(t, "x", 3) == PUT_EXISTS); /* i po růstu tabulky */
  CHECK(has(t, "x", 1));
  CHECK(st_put(t, "k0000250", 9) == PUT_EXISTS);
  CHECK(has(t, "k0000250", 350));
  st_destroy(t);
  return 0;
}

/* Scanner po zpracování tokenu uvolní jeho řetězec, takže si tabulka musí
 * klíč zkopírovat. */
static int t_key_is_copied(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  char *k = malloc(16);
  CHECK(k != NULL);
  strcpy(k, "hello");
  CHECK(st_put(t, k, 7) == PUT_OK);
  strcpy(k, "world"); /* přepsání původního bufferu */
  free(k);
  CHECK(has(t, "hello", 7));
  CHECK(absent(t, "world"));
  st_destroy(t);
  return 0;
}

static int t_long_keys(void) {
  enum { LEN = 5000 };
  char *a = malloc(LEN + 1), *b = malloc(LEN + 1), *c = malloc(LEN + 1);
  CHECK(a && b && c);
  memset(a, 'a', LEN);
  a[LEN] = '\0';
  memcpy(b, a, LEN + 1);
  b[LEN - 1] = 'b'; /* liší se jen posledním znakem */
  memcpy(c, a, LEN + 1);
  c[LEN - 1] = '\0'; /* o znak kratší */
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_put(t, a, 1) == PUT_OK);
  CHECK(st_put(t, b, 2) == PUT_OK);
  CHECK(st_put(t, c, 3) == PUT_OK);
  CHECK(has(t, a, 1));
  CHECK(has(t, b, 2));
  CHECK(has(t, c, 3));
  c[LEN - 2] = '\0';
  CHECK(absent(t, c));
  st_destroy(t);
  free(a);
  free(b);
  free(c);
  return 0;
}

static int t_identifier_chars(void) {
  static const char *names[] = {"_",  "_x", "__",   "x_", "a1_B2",
                                "_1", "X9", "i",    "I",  "_I",
                                "a_b", "a__b", "z", "Z_Z_Z"};
  const int cnt = (int)(sizeof names / sizeof names[0]);
  ST *t = st_create();
  CHECK(t != NULL);
  for (int i = 0; i < cnt; i++)
    CHECKF(st_put(t, names[i], i + 1) == PUT_OK, "put %s", names[i]);
  for (int i = 0; i < cnt; i++)
    CHECKF(has(t, names[i], i + 1), "find %s", names[i]);
  st_destroy(t);
  return 0;
}

/* Dvě tabulky nesmějí sdílet žádný stav (typická chyba: static/globální kořen). */
static int t_independent_tables(void) {
  ST *a = st_create();
  ST *b = st_create();
  CHECK(a && b);
  CHECK(st_put(a, "x", 1) == PUT_OK);
  CHECK(st_put(b, "y", 2) == PUT_OK);
  CHECK(absent(b, "x"));
  CHECK(absent(a, "y"));
  CHECK(st_put(b, "x", 3) == PUT_OK); /* v b ještě x není -> ne PUT_EXISTS */
  CHECK(has(a, "x", 1));
  CHECK(has(b, "x", 3));
  st_destroy(a);
  CHECK(has(b, "x", 3)); /* zrušení a se b nedotkne */
  CHECK(has(b, "y", 2));
  for (int i = 0; i < 300; i++) {
    char key[32];
    mkkey(key, sizeof key, i);
    CHECK(st_put(b, key, i) == PUT_OK);
  }
  CHECK(has(b, "x", 3));
  st_destroy(b);
  return 0;
}

static int t_recreate(void) {
  for (int round = 0; round < 200; round++) {
    ST *t = st_create();
    CHECK(t != NULL);
    CHECKF(absent(t, "k0000000"), "round %d: nova tabulka neni prazdna", round);
    for (int i = 0; i < 50; i++) {
      char key[32];
      mkkey(key, sizeof key, i);
      CHECK(st_put(t, key, i + round) == PUT_OK);
    }
    CHECK(has(t, "k0000049", 49 + round));
    st_destroy(t);
  }
  return 0;
}

/* ========================================================================
 * Zátěž a kolize
 * ====================================================================== */

static int load_order(int mode) {
  const int n = BIG_N;
  int *order = make_order(n, mode);
  CHECK(order != NULL);
  ST *t = st_create();
  CHECK(t != NULL);
  char key[32];
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, order[i]);
    int rc = st_put(t, key, order[i] + 1);
    CHECKF(rc == PUT_OK, "put %s vratilo %d (i=%d)", key, rc, i);
  }
#ifdef HAVE_COUNT
  CHECK(st_count(t) == (size_t)n);
#endif
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, i);
    CHECKF(has(t, key, i + 1), "klic %s nenalezen / spatna hodnota", key);
  }
  for (int i = n; i < n + 1000; i++) {
    mkkey(key, sizeof key, i);
    CHECKF(absent(t, key), "klic %s nalezen, ale nebyl vlozen", key);
  }
  st_destroy(t);
  free(order);
  return 0;
}

static int t_many_ascending(void) { return load_order(0); }
static int t_many_descending(void) { return load_order(1); }
static int t_many_random(void) { return load_order(2); }

/* Po každém vložení (u malých n) a pak v mocninách dvou se ověří, že se při
 * růstu / vyvažování / přehashování neztratil žádný dosavadní prvek. */
static int t_growth_integrity(void) {
  const int n = 3000;
  ST *t = st_create();
  CHECK(t != NULL);
  char key[32];
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, i);
    CHECKF(st_put(t, key, i + 1) == PUT_OK, "put %s", key);
    if (i < 64 || (i & (i - 1)) == 0 || i == n - 1)
      for (int j = 0; j <= i; j++) {
        mkkey(key, sizeof key, j);
        CHECKF(has(t, key, j + 1), "po vlozeni %d se ztratil klic %s", i, key);
      }
  }
  st_destroy(t);
  return 0;
}

/* Všechny permutace "abcdefg": každá hashovací funkce závislá jen na součtu,
 * XORu nebo součinu znaků je dostane do stejné přihrádky. */
static int t_collisions_anagrams(void) {
  char s[8] = "abcdefg";
  ST *t = st_create();
  CHECK(t != NULL);
  int cnt = 0;
  do {
    CHECKF(st_put(t, s, cnt + 1) == PUT_OK, "put %s", s);
    cnt++;
  } while (next_perm(s, 7));
  CHECK(cnt == 5040);
  strcpy(s, "abcdefg");
  int idx = 0;
  do {
    idx++;
    CHECKF(has(t, s, idx), "klic %s nenalezen", s);
  } while (next_perm(s, 7));
  CHECK(absent(t, "abcdefgh"));
  CHECK(absent(t, "abcdeff"));
  st_destroy(t);
  return 0;
}

/* Klíče lišící se jen posledním znakem / jen délkou, krátké klíče. */
static int t_collisions_similar(void) {
  static const char alphabet[] = "abcxyzABCXYZ_09";
  const int na = (int)(sizeof alphabet - 1);
  ST *t = st_create();
  CHECK(t != NULL);
  char key[16];
  int cnt = 0;
  for (int i = 0; i < na; i++) { /* délka 1 */
    key[0] = alphabet[i];
    key[1] = '\0';
    CHECKF(st_put(t, key, ++cnt) == PUT_OK, "put %s", key);
  }
  for (int i = 0; i < na; i++) /* délka 2 */
    for (int j = 0; j < na; j++) {
      key[0] = alphabet[i];
      key[1] = alphabet[j];
      key[2] = '\0';
      CHECKF(st_put(t, key, ++cnt) == PUT_OK, "put %s", key);
    }
  for (int i = 0; i < 3000; i++) { /* společný prefix, liší se jen konec */
    char pk[32];
    snprintf(pk, sizeof pk, "prefix_%04d", i);
    CHECKF(st_put(t, pk, ++cnt) == PUT_OK, "put %s", pk);
  }
  cnt = 0;
  for (int i = 0; i < na; i++) {
    key[0] = alphabet[i];
    key[1] = '\0';
    CHECKF(has(t, key, ++cnt), "find %s", key);
  }
  for (int i = 0; i < na; i++)
    for (int j = 0; j < na; j++) {
      key[0] = alphabet[i];
      key[1] = alphabet[j];
      key[2] = '\0';
      CHECKF(has(t, key, ++cnt), "find %s", key);
    }
  for (int i = 0; i < 3000; i++) {
    char pk[32];
    snprintf(pk, sizeof pk, "prefix_%04d", i);
    CHECKF(has(t, pk, ++cnt), "find %s", pk);
  }
  CHECK(absent(t, "prefix_3000"));
  CHECK(absent(t, "prefix_"));
  st_destroy(t);
  return 0;
}

static int t_interleaved(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  char key[32];
  for (int i = 0; i < 2000; i++) {
    mkkey(key, sizeof key, i);
    CHECK(st_put(t, key, i + 1) == PUT_OK);
    mkkey(key, sizeof key, i / 2);
    CHECKF(has(t, key, i / 2 + 1), "find %s", key);
    mkkey(key, sizeof key, i + 1);
    CHECKF(absent(t, key), "find %s (nema byt)", key);
  }
  st_destroy(t);
  return 0;
}

/* ========================================================================
 * Použití v překladači (scope, funkce) - jen přes výše testované operace
 * ====================================================================== */

/* Vyhledání v řetězu rozsahů od nejvnitřnějšího ven. */
static int scope_find(ST **scopes, int depth, const char *key, int *id) {
  for (int d = depth - 1; d >= 0; d--)
    if (st_get(scopes[d], key, id))
      return 1;
  return 0;
}

static int scope_has(ST **scopes, int depth, const char *key, int want) {
  int id;
  return scope_find(scopes, depth, key, &id) && id == want;
}

/* Překrývání (shadowing), viditelnost v podblocích, redefinice ve stejném
 * bloku a zánik proměnných na konci bloku. */
static int t_scope_chain(void) {
  ST *scopes[4] = {NULL, NULL, NULL, NULL};
  int depth = 0;
  scopes[depth++] = st_create();
  CHECK(scopes[0] != NULL);
  CHECK(st_put(scopes[0], "x", 1) == PUT_OK);
  CHECK(st_put(scopes[0], "y", 2) == PUT_OK);

  scopes[depth++] = st_create(); /* vnořený blok */
  CHECK(scopes[1] != NULL);
  CHECK(scope_has(scopes, depth, "x", 1)); /* vidí proměnnou z vnějšku */
  CHECK(st_put(scopes[1], "x", 10) == PUT_OK); /* překrytí je legální */
  CHECK(scope_has(scopes, depth, "x", 10));
  CHECK(scope_has(scopes, depth, "y", 2));
  CHECK(st_put(scopes[1], "x", 11) == PUT_EXISTS); /* redefinice -> chyba 4 */
  CHECK(st_put(scopes[1], "z", 3) == PUT_OK);

  scopes[depth++] = st_create(); /* ještě hlouběji */
  CHECK(scopes[2] != NULL);
  CHECK(scope_has(scopes, depth, "z", 3));
  CHECK(scope_has(scopes, depth, "x", 10));
  CHECK(st_put(scopes[2], "z", 30) == PUT_OK);
  CHECK(scope_has(scopes, depth, "z", 30));

  st_destroy(scopes[--depth]); /* konec nejhlubšího bloku */
  CHECK(scope_has(scopes, depth, "z", 3));
  CHECK(scope_has(scopes, depth, "x", 10));

  st_destroy(scopes[--depth]); /* konec vnořeného bloku */
  int id;
  CHECK(!scope_find(scopes, depth, "z", &id)); /* z už neexistuje -> chyba 3 */
  CHECK(scope_has(scopes, depth, "x", 1));     /* x je zase to původní */

  st_destroy(scopes[--depth]);
  return 0;
}

/* Globální tabulka funkcí: volání před definicí (vzájemná rekurze) vyžaduje,
 * aby šlo všechny funkce vložit dřív, než se hledají. Přetěžování neexistuje. */
static int t_function_table(void) {
  static const char *fns[] = {"factorial", "runFoo", "helper", "isEven", "isOdd"};
  ST *t = st_create();
  CHECK(t != NULL);
  for (int i = 0; i < 5; i++)
    CHECKF(st_put(t, fns[i], i + 1) == PUT_OK, "put %s", fns[i]);
  for (int i = 0; i < 5; i++)
    CHECKF(has(t, fns[i], i + 1), "find %s", fns[i]);
  CHECK(st_put(t, "helper", 99) == PUT_EXISTS); /* redefinice funkce -> chyba 4 */
  CHECK(has(t, "helper", 3));                   /* původní signatura zůstala */
  CHECK(absent(t, "readInt"));                  /* nedefinovaná -> chyba 3 */
  st_destroy(t);
  return 0;
}

/* ========================================================================
 * Volitelné testy
 * ====================================================================== */

#ifdef HAVE_COUNT
static int t_count(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_count(t) == 0);
  for (int i = 0; i < 100; i++) {
    char key[32];
    mkkey(key, sizeof key, i);
    CHECK(st_put(t, key, i) == PUT_OK);
  }
  CHECK(st_count(t) == 100);
  CHECK(st_put(t, "k0000007", 5) == PUT_EXISTS);
  CHECK(st_count(t) == 100); /* duplicita počet nezvyšuje */
  st_destroy(t);
  return 0;
}
#endif

#ifdef HAVE_REMOVE
static int t_remove(void) {
  ST *t = st_create();
  CHECK(t != NULL);
  CHECK(st_remove(t, "x") == 0); /* mazání z prázdné tabulky */
  const int n = 1000;
  char key[32];
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, i);
    CHECK(st_put(t, key, i + 1) == PUT_OK);
  }
  for (int i = 0; i < n; i += 2) { /* smaž sudé */
    mkkey(key, sizeof key, i);
    CHECKF(st_remove(t, key) == 1, "remove %s", key);
  }
  for (int i = 0; i < n; i += 2) {
    mkkey(key, sizeof key, i);
    CHECKF(st_remove(t, key) == 0, "druhe remove %s", key);
  }
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, i);
    if (i % 2 == 0)
      CHECKF(absent(t, key), "%s ma byt smazany", key);
    else
      CHECKF(has(t, key, i + 1), "%s ma zustat", key);
  }
#ifdef HAVE_COUNT
  CHECK(st_count(t) == (size_t)n / 2);
#endif
  for (int i = 0; i < n; i += 2) { /* znovu vlož (v TRP: pohřbené položky) */
    mkkey(key, sizeof key, i);
    CHECKF(st_put(t, key, i + 5000) == PUT_OK, "reinsert %s", key);
  }
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, i);
    CHECKF(has(t, key, i % 2 ? i + 1 : i + 5000), "po reinsertu %s", key);
  }
  st_destroy(t);
  return 0;
}

/* Mazání uprostřed kolizí - u otevřené adresace nesmí přerušit řetěz sond. */
static int t_remove_collisions(void) {
  char s[8] = "abcdefg";
  ST *t = st_create();
  CHECK(t != NULL);
  int cnt = 0;
  do {
    CHECK(st_put(t, s, ++cnt) == PUT_OK);
  } while (next_perm(s, 7));
  strcpy(s, "abcdefg");
  int idx = 0;
  do {
    idx++;
    if (idx % 3 == 0)
      CHECKF(st_remove(t, s) == 1, "remove %s", s);
  } while (next_perm(s, 7));
  strcpy(s, "abcdefg");
  idx = 0;
  do {
    idx++;
    if (idx % 3 == 0)
      CHECKF(absent(t, s), "%s ma byt smazany", s);
    else
      CHECKF(has(t, s, idx), "%s ma zustat", s);
  } while (next_perm(s, 7));
  st_destroy(t);
  return 0;
}
#endif

#ifdef HAVE_HEIGHT
/* Horní mez výšky AVL stromu ~ 1,44 * log2(n + 2); bereme s mírnou rezervou. */
static int height_bound(int n) {
  int bits = 0;
  for (unsigned v = (unsigned)n + 2; v; v >>= 1)
    bits++;
  return (145 * bits) / 100 + 1;
}

static int height_after(int mode) {
  const int n = BIG_N;
  int *order = make_order(n, mode);
  CHECK(order != NULL);
  ST *t = st_create();
  CHECK(t != NULL);
  char key[32];
  for (int i = 0; i < n; i++) {
    mkkey(key, sizeof key, order[i]);
    CHECK(st_put(t, key, order[i] + 1) == PUT_OK);
  }
  int h = st_height(t);
  CHECKF(h <= height_bound(n), "vyska %d, povolena nejvyse %d (n=%d)", h,
         height_bound(n), n);
  st_destroy(t);
  free(order);
  return 0;
}

static int t_height_ascending(void) { return height_after(0); }
static int t_height_descending(void) { return height_after(1); }
static int t_height_random(void) { return height_after(2); }
#endif

#ifdef TEST_NULL
static int t_null_safety(void) {
  st_destroy(NULL); /* jako free(NULL) */
  int id;
  CHECK(!st_get(NULL, "x", &id));
  return 0;
}
#endif

/* ========================================================================
 * Spouštěč
 * ====================================================================== */

typedef struct {
  const char *name;
  int (*fn)(void);
} test_t;

static const test_t tests[] = {
    {"create_destroy_empty", t_create_destroy_empty},
    {"empty_lookup", t_empty_lookup},
    {"single", t_single},
    {"many_identifiers", t_many_identifiers},
    {"similar_keys", t_similar_keys},
    {"case_sensitive", t_case_sensitive},
    {"duplicate_insert", t_duplicate_insert},
    {"key_is_copied", t_key_is_copied},
    {"long_keys", t_long_keys},
    {"identifier_chars", t_identifier_chars},
    {"independent_tables", t_independent_tables},
    {"recreate", t_recreate},
    {"growth_integrity", t_growth_integrity},
    {"interleaved", t_interleaved},
    {"collisions_anagrams", t_collisions_anagrams},
    {"collisions_similar", t_collisions_similar},
    {"many_ascending", t_many_ascending},
    {"many_descending", t_many_descending},
    {"many_random", t_many_random},
    {"scope_chain", t_scope_chain},
    {"function_table", t_function_table},
#ifdef HAVE_COUNT
    {"count", t_count},
#endif
#ifdef HAVE_REMOVE
    {"remove", t_remove},
    {"remove_collisions", t_remove_collisions},
#endif
#ifdef HAVE_HEIGHT
    {"height_ascending", t_height_ascending},
    {"height_descending", t_height_descending},
    {"height_random", t_height_random},
#endif
#ifdef TEST_NULL
    {"null_safety", t_null_safety},
#endif
};

enum { R_PASS, R_FAIL, R_TIMEOUT, R_CRASH };

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* Spustí test v potomkovi; zprávy o selhání přijme rourou a vrátí je v *msg. */
static int run_forked(const test_t *t, char *msg, size_t msg_size, int *sig) {
  int fd[2];
  if (pipe(fd) != 0) {
    perror("pipe");
    exit(2);
  }
  fflush(NULL);
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(2);
  }
  if (pid == 0) {
    close(fd[0]);
    g_report = fdopen(fd[1], "w");
    if (!g_report)
      _exit(98);
    alarm(TEST_TIMEOUT);
    int rc = t->fn();
    fflush(g_report);
    _exit(rc ? 1 : 0);
  }
  close(fd[1]);
  size_t len = 0;
  for (;;) {
    ssize_t n = read(fd[0], msg + len, msg_size - 1 - len);
    if (n <= 0)
      break;
    len += (size_t)n;
    if (len >= msg_size - 1)
      break;
  }
  msg[len] = '\0';
  close(fd[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status))
    return WEXITSTATUS(status) == 0 ? R_PASS : R_FAIL;
  *sig = WTERMSIG(status);
  return *sig == SIGALRM ? R_TIMEOUT : R_CRASH;
}

int main(int argc, char *argv[]) {
  int nofork = 0;
  const char *filter = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--nofork"))
      nofork = 1;
    else
      filter = argv[i];
  }

  const size_t total = sizeof tests / sizeof tests[0];
  int pass = 0, fail = 0, skipped = 0;
  for (size_t i = 0; i < total; i++) {
    const test_t *t = &tests[i];
    if (filter && !strstr(t->name, filter)) {
      skipped++;
      continue;
    }
    char msg[4096] = "";
    int sig = 0;
    int res;
    double t0 = now_ms();
    if (nofork) {
      g_report = stdout;
      res = t->fn() ? R_FAIL : R_PASS;
    } else {
      res = run_forked(t, msg, sizeof msg, &sig);
    }
    double ms = now_ms() - t0;

    switch (res) {
    case R_PASS:
      printf("[ OK ] %-24s %8.1f ms\n", t->name, ms);
      pass++;
      break;
    case R_FAIL:
      printf("[FAIL] %-24s\n%s", t->name, msg);
      fail++;
      break;
    case R_TIMEOUT:
      printf("[FAIL] %-24s TIMEOUT po %d s - nekonecna smycka nebo degenerace "
             "na O(n)?\n",
             t->name, TEST_TIMEOUT);
      fail++;
      break;
    default:
      printf("[FAIL] %-24s PAD (signal %d%s)\n%s", t->name, sig,
             sig == SIGSEGV ? ", SIGSEGV" : "", msg);
      fail++;
      break;
    }
  }
  printf("\nVysledek: %d OK, %d FAIL", pass, fail);
  if (skipped)
    printf(", %d preskoceno", skipped);
  printf("\n");
  return fail ? 1 : 0;
}
