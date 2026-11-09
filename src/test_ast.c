/*
 * Projekt: IFJ26 – překladač jazyka IFJ26 do IFJcode26
 * Soubor:  test_ast.c – kompletní test modulu AST (ast.c / ast.h)
 *
 * ---------------------------------------------------------------------------
 * Tento soubor vygenerovala AI: Claude Sonnet 5.5 (Anthropic, claude-sonnet-5-5), 2026-11-09.
 * Prompt: "tady mas zadání projektu a teď jsem dodělal ast. Chci abys mi vytvořil kompletní
 *          test, úplně na všechno. I na rozšíření, pokud nějaký nepodporuje tak chci abys
 *          vypsal jaký. Plus se tam pod to podepiš, ať je jasné že jsi to dělal ty."
 * Zadal a zrevidoval: TODO – doplň jméno a xlogin člena týmu
 * ---------------------------------------------------------------------------
 *
 * PŘEKLAD (soubor test_ast.c musí ležet vedle ast.c a ast.h):
 *
 *     gcc -std=gnu11 -g -O0 -Wall -Wextra test_ast.c -o test_ast && ./test_ast
 *
 * Nepřekládej ast.c zvlášť a nelinkuj ho – test si ho vloží přes #include "ast.c"
 * (tzv. unity build). Díky tomu jde testovat i static funkce (node_type_name, op_name,
 * child_label, ...) a podvrhnout calloc/strdup/free/exit (sledování úniků, dvojitých free
 * a simulace selhání alokace -> exit(99)).
 *
 * Pokud překlad padá na  #include <_string.h>  (to je jen macOS; jinde neexistuje),
 * přepiš v ast.c na <string.h> (přenositelné) nebo přidej  -I<adresář s _string.h>.
 *
 * POUŽITÍ:
 *     ./test_ast            spustí všechno
 *     ./test_ast G7         jen testy, jejichž skupina/jméno obsahuje "G7" (bez reportu rozšíření)
 *     ./test_ast -q         vypíše jen neúspěchy
 *
 * VÝSLEDKY:
 *     [ OK ]  prošlo
 *     [FAIL]  selhala kontrola          [CRSH]  proces spadl (segfault, ...)
 *     [TIME]  zacyklení (limit 20 s)    [WARN]  "měkký" test – slabina návrhu, nemění návratový kód
 *
 * Každý test běží ve vlastním procesu (fork), takže pád jednoho testu neshodí ostatní.
 * Návratový kód programu: 0 = žádný tvrdý neúspěch, 1 = aspoň jeden FAIL/CRSH/TIME.
 * Na konci se vypíše přehled podpory rozšíření (FUNEXP, BOOLTHEN, ...).
 *
 * Pozn.: ast_if_create / ast_func_def_create / ast_call_create ukládají parametr typu
 * `const AST_NODE *` přes strdup(). GCC 14+ to bere jako chybu (-Wincompatible-pointer-types),
 * a protože zadání vyžaduje překlad gcc 14/15/16, musí se to v ast.c opravit. Aby šel
 * tento test přeložit i před opravou, je výše zmíněná chyba níže jen sníženo na varování.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/* ========================================================================== */
/*  1) Infrastruktura před vložením ast.c                                     */
/* ========================================================================== */

/* --- sdílená paměť: potomek sem píše hlášky o neúspěšných kontrolách --- */
typedef struct {
  size_t len;
  char buf[1 << 16];
} Shm;
static Shm *g_shm;
static int g_failed; /* počet neúspěšných kontrol v aktuálním testu (jen v potomkovi) */

static void failf(const char *fmt, ...) {
  g_failed++;
  if (!g_shm || g_failed > 40) return;
  size_t room = sizeof g_shm->buf - g_shm->len;
  if (room < 2) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(g_shm->buf + g_shm->len, room, fmt, ap);
  va_end(ap);
  if (n > 0) g_shm->len += ((size_t)n < room) ? (size_t)n : room - 1;
}

#define CHECK(c)                                                                         \
  do {                                                                                   \
    if (!(c)) failf("      řádek %d: CHECK(%s)\n", __LINE__, #c);                        \
  } while (0)

#define CHECK_MSG(c, ...)                                                                \
  do {                                                                                   \
    if (!(c)) {                                                                          \
      failf("      řádek %d: ", __LINE__);                                               \
      failf(__VA_ARGS__);                                                                \
      failf("\n");                                                                       \
    }                                                                                    \
  } while (0)

#define CHECK_INT(got, exp)                                                              \
  do {                                                                                   \
    long long g_ = (long long)(got), e_ = (long long)(exp);                              \
    if (g_ != e_) failf("      řádek %d: %s = %lld, čekáno %lld\n", __LINE__, #got, g_, e_); \
  } while (0)

#define CHECK_PTR(got, exp)                                                              \
  do {                                                                                   \
    const void *g_ = (const void *)(got), *e_ = (const void *)(exp);                     \
    if (g_ != e_) failf("      řádek %d: %s = %p, čekáno %p (%s)\n", __LINE__, #got, g_, e_, #exp); \
  } while (0)

#define CHECK_DBL(got, exp)                                                              \
  do {                                                                                   \
    double g_ = (double)(got), e_ = (double)(exp);                                       \
    if (g_ != e_) failf("      řádek %d: %s = %.17g, čekáno %.17g\n", __LINE__, #got, g_, e_); \
  } while (0)

#define CHECK_STR(got, exp)                                                              \
  do {                                                                                   \
    const char *g_ = (got), *e_ = (exp);                                                 \
    if (!((g_ && e_ && strcmp(g_, e_) == 0) || (!g_ && !e_)))                            \
      failf("      řádek %d: %s\n        dostal : %s%s%s\n        čekáno : %s%s%s\n",     \
            __LINE__, #got, g_ ? "\"" : "", g_ ? g_ : "NULL", g_ ? "\"" : "",            \
            e_ ? "\"" : "", e_ ? e_ : "NULL", e_ ? "\"" : "");                          \
  } while (0)

/* --- registr živých alokací (únik, dvojité / neplatné free) + vstřikování selhání --- */
#define REG_SLOTS ((size_t)1 << 22)
static void **g_reg;
static size_t g_live, g_bad_free;
static long g_alloc_calls, g_fail_at;
static int g_fail_hit;

static size_t reg_hash(void *p) {
  return (size_t)(((uintptr_t)p >> 4) * 2654435761u) & (REG_SLOTS - 1);
}
static void reg_add(void *p) {
  if (!g_reg) g_reg = calloc(REG_SLOTS, sizeof *g_reg);
  if (!g_reg) abort();
  size_t i = reg_hash(p);
  while (g_reg[i] != NULL && g_reg[i] != (void *)1) i = (i + 1) & (REG_SLOTS - 1);
  g_reg[i] = p;
  g_live++;
}
static int reg_del(void *p) {
  if (!g_reg) return 0;
  size_t i = reg_hash(p);
  while (g_reg[i] != NULL) {
    if (g_reg[i] == p) {
      g_reg[i] = (void *)1;
      g_live--;
      return 1;
    }
    i = (i + 1) & (REG_SLOTS - 1);
  }
  return 0;
}

static void *t_calloc(size_t n, size_t s) {
  g_alloc_calls++;
  if (g_fail_at && g_alloc_calls == g_fail_at) {
    g_fail_hit = 1;
    return NULL;
  }
  void *p = calloc(n, s);
  if (p) reg_add(p);
  return p;
}
static char *t_strdup(const char *s) {
  g_alloc_calls++;
  if (g_fail_at && g_alloc_calls == g_fail_at) {
    g_fail_hit = 1;
    return NULL;
  }
  size_t l = strlen(s) + 1; /* strdup(NULL) spadne stejně jako v libc */
  char *p = malloc(l);
  if (!p) return NULL;
  memcpy(p, s, l);
  reg_add(p);
  return p;
}
static void t_free(void *p) {
  if (!p) return;
  if (!reg_del(p)) { /* dvojité free nebo free ukazatele, který nepatří nám */
    g_bad_free++;
    return;
  }
  free(p);
}

static jmp_buf g_exit_jmp;
static int g_exit_armed, g_exit_code;
#if defined(__GNUC__)
__attribute__((noreturn))
#endif
static void t_exit(int code) {
  if (g_exit_armed) {
    g_exit_code = code;
    g_exit_armed = 0;
    longjmp(g_exit_jmp, 1);
  }
  _exit(code);
}

/* ========================================================================== */
/*  2) Vložení testovaného modulu                                             */
/* ========================================================================== */

#define calloc(n, s) t_calloc((n), (s))
#define strdup(s) t_strdup((s))
#define free(p) t_free((p))
#define exit(c) t_exit((c))
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wincompatible-pointer-types"
#include "ast.c"
#pragma GCC diagnostic pop
#undef calloc
#undef strdup
#undef free
#undef exit

/* ast_binop_create je v ast.c definována, ale v ast.h NENÍ deklarována (ostatní moduly ji
 * proto nemohou volat). V tomto překladovém celku je vidět, takže ji testujeme normálně. */

/* ========================================================================== */
/*  3) Pomocné funkce testů                                                   */
/* ========================================================================== */

#define ENUM_SCAN 64

static void check_clean(void) {
  if (g_live != 0) failf("      únik paměti: %zu neuvolněných alokací\n", g_live);
  if (g_bad_free != 0) failf("      dvojité / neplatné free: %zu×\n", g_bad_free);
}

/* --- stavění uzlů --- */
static AST_NODE *mk(NODE_TYPE t, AST_NODE *l, AST_NODE *r, AST_NODE *nx) {
  return ast_create(l, r, nx, t);
}
static AST_NODE *named(NODE_TYPE t, const char *name, AST_NODE *l, AST_NODE *r, AST_NODE *nx) {
  AST_NODE *n = ast_create(l, r, nx, t);
  ast_name(n, (char *)name);
  return n;
}
/* literály stavíme ručně (konstruktory pro double/string nenastavují data_type) */
static AST_NODE *lit_i(long long v) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  n->data_type = DATA_INT;
  n->ival = v;
  return n;
}
static AST_NODE *lit_d(double v) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  n->data_type = DATA_DOUBLE;
  n->dval = v;
  return n;
}
static AST_NODE *lit_s(const char *s) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  n->data_type = DATA_STRING;
  n->sval = s ? t_strdup(s) : NULL;
  return n;
}
static AST_NODE *lit_b(int v) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  n->data_type = DATA_BOOL;
  n->ival = v;
  return n;
}
static AST_NODE *lit_unit(void) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  n->data_type = DATA_UNIT;
  return n;
}
static AST_NODE *ID(const char *name) { return ast_ident_create(NULL, (char *)name, NULL); }
static AST_NODE *BIN(OP_TYPE op, AST_NODE *l, AST_NODE *r) { return ast_binop_create(l, r, NULL, op); }
static AST_NODE *UN(OP_TYPE op, AST_NODE *l) { return ast_unop_create(l, NULL, op); }
static AST_NODE *seq(AST_NODE *a, AST_NODE *b) {
  a->next = b;
  return a;
}
/* "ruční" varianty uzlů s else_param (obcházejí chybné konstruktory) */
static AST_NODE *if_manual(AST_NODE *c, AST_NODE *t, AST_NODE *e) {
  AST_NODE *n = mk(NODE_IF, c, t, NULL);
  n->else_param = e;
  return n;
}
static AST_NODE *call_manual(const char *name, AST_NODE *args) {
  AST_NODE *n = named(NODE_CALL, name, NULL, NULL, NULL);
  n->else_param = args;
  return n;
}
static AST_NODE *func_manual(const char *name, AST_NODE *body, AST_NODE *ret, AST_NODE *params) {
  AST_NODE *n = named(NODE_FUNC_DEF, name, body, ret, NULL);
  n->else_param = params;
  return n;
}

/* Konstruktory if/func_def/call uloží else_param přes strdup() -> ukazatel se změní.
 * Pomocná funkce zjistí, jestli se else_param shoduje s očekávaným; nesprávnou kopii uvolní
 * a else_param odpojí (volající pak uvolní `expected` sám). Funguje před i po opravě ast.c. */
static int detach_else(AST_NODE *n, AST_NODE *expected) {
  int ok = (n->else_param == expected);
  if (!ok && n->else_param) t_free((void *)n->else_param);
  n->else_param = NULL;
  return ok;
}

static size_t count_nodes(const AST_NODE *n) {
  size_t c = 0;
  for (; n; n = n->next) {
    c++;
    c += count_nodes(n->left) + count_nodes(n->right) + count_nodes(n->else_param);
  }
  return c;
}
static size_t count_lines(const char *s) {
  size_t c = 0;
  for (; s && *s; s++)
    if (*s == '\n') c++;
  return c;
}
static AST_NODE *build_next_chain(size_t n) {
  AST_NODE *head = NULL;
  for (size_t i = 0; i < n; i++) head = ast_create(NULL, NULL, head, NODE_EXPR_STMT);
  return head;
}
static AST_NODE *build_left_chain(size_t n) {
  AST_NODE *head = NULL;
  for (size_t i = 0; i < n; i++) head = ast_create(head, NULL, NULL, NODE_UNOP);
  return head;
}

/* --- zachycení výstupu print_AST (stderr) a kontrola, že na stdout nic nejde --- */
static char *slurp(FILE *f) {
  fflush(f);
  long n = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : 0;
  if (n < 0) n = 0;
  rewind(f);
  char *s = malloc((size_t)n + 1);
  if (!s) abort();
  size_t r = fread(s, 1, (size_t)n, f);
  s[r] = 0;
  return s;
}
static void capture_print(AST_NODE *n, char **so_out, char **se_out) {
  fflush(stdout);
  fflush(stderr);
  FILE *fo = tmpfile(), *fe = tmpfile();
  int so = dup(1), se = dup(2);
  if (!fo || !fe || so < 0 || se < 0) {
    failf("      zachycení výstupu selhalo (tmpfile/dup)\n");
    *so_out = strdup("");
    *se_out = strdup("");
    return;
  }
  dup2(fileno(fo), 1);
  dup2(fileno(fe), 2);
  print_AST(n);
  fflush(stdout);
  fflush(stderr);
  dup2(so, 1);
  dup2(se, 2);
  close(so);
  close(se);
  *so_out = slurp(fo);
  *se_out = slurp(fe);
  fclose(fo);
  fclose(fe);
}
/* vrátí obsah stderru (volající uvolní free) */
static char *cap(AST_NODE *n) {
  char *so, *se;
  capture_print(n, &so, &se);
  free(so);
  return se;
}
/* odstraní popisky potomků ("lhs: ", "cond: " ...) -> srovnání nezávislé na labelech */
static char *strip_labels(const char *s) {
  char *o = malloc(strlen(s) + 1);
  if (!o) abort();
  size_t k = 0;
  while (*s) {
    const char *p = s;
    while (*p == ' ') p++;
    memcpy(o + k, s, (size_t)(p - s));
    k += (size_t)(p - s);
    const char *q = p;
    while (*q >= 'a' && *q <= 'z') q++;
    if (q > p && q[0] == ':' && q[1] == ' ') p = q + 2;
    while (*p && *p != '\n') o[k++] = *p++;
    if (*p == '\n') o[k++] = *p++;
    s = p;
  }
  o[k] = 0;
  return o;
}

/* --- tabulky očekávání --- */
static const struct {
  OP_TYPE op;
  const char *sym;
} OPS[] = {{OP_ADD, "+"}, {OP_SUB, "-"}, {OP_MUL, "*"},  {OP_DIV, "/"},  {OP_LT, "<"},
           {OP_LE, "<="}, {OP_GT, ">"},  {OP_GE, ">="},  {OP_EQ, "=="},  {OP_NEQ, "!="},
           {OP_AND, "&&"}, {OP_OR, "||"}, {OP_NOT, "!"}};
#define N_OPS (sizeof OPS / sizeof OPS[0])

static const struct {
  NODE_TYPE t;
  const char *name;
  const char *extra; /* co printer připojí za jméno u čerstvého uzlu */
} NODES[] = {{NODE_PROGRAM, "PROGRAM", ""},     {NODE_FUNC_DEF, "FUNC_DEF", ""},
             {NODE_PARAM, "PARAM", ""},         {NODE_BLOCK, "BLOCK", ""},
             {NODE_VAR_DEF, "VAR_DEF", " var"}, {NODE_ASSIGN, "ASSIGN", ""},
             {NODE_IF, "IF", ""},               {NODE_WHILE, "WHILE", ""},
             {NODE_RETURN, "RETURN", ""},       {NODE_CALL, "CALL", ""},
             {NODE_EXPR_STMT, "EXPR_STMT", ""}, {NODE_BINOP, "BINOP", " op=+"},
             {NODE_UNOP, "UNOP", " op=+"},      {NODE_LITERAL, "LITERAL", " Int 0"},
             {NODE_IDENT, "IDENT", ""},         {NODE_FOR, "FOR", ""},
             {NODE_INTERP, "INTERP", ""}};
#define N_NODES (sizeof NODES / sizeof NODES[0])

/* všechny typy uzlů, které printer zná (roste automaticky, když přidáš nový case) */
static int known_nodes[ENUM_SCAN];
static int n_known_nodes;
static void scan_known_nodes(void) {
  n_known_nodes = 0;
  for (int i = 0; i < ENUM_SCAN; i++)
    if (strcmp(node_type_name((NODE_TYPE)i), "???") != 0) known_nodes[n_known_nodes++] = i;
}

/* ========================================================================== */
/*  4) Registrace testů                                                       */
/* ========================================================================== */

typedef struct {
  const char *id, *group, *title;
  void (*fn)(void);
  int soft;
  int line;
} TestCase;
static TestCase g_tests[512];
static int g_ntests;

static void register_test(const char *id, const char *group, const char *title, void (*fn)(void),
                          int soft, int line) {
  if (g_ntests < (int)(sizeof g_tests / sizeof g_tests[0]))
    g_tests[g_ntests++] = (TestCase){id, group, title, fn, soft, line};
}

/* soft = 1: selhání je jen varování (slabina návrhu, ne chyba v dnešním použití) */
#define TEST(name, group, title, soft)                                                   \
  static void name(void);                                                                \
  __attribute__((constructor)) static void reg_##name(void) {                            \
    register_test(#name, group, title, name, soft, __LINE__);                            \
  }                                                                                      \
  static void name(void)

/* ========================================================================== */
/*  G1  ast_create                                                            */
/* ========================================================================== */
#define G1 "G1  ast_create"

TEST(t_create_fields, G1, "nastaví type/left/right/next a vše ostatní vynuluje", 0) {
  AST_NODE *a = mk(NODE_PROGRAM, NULL, NULL, NULL), *b = mk(NODE_PROGRAM, NULL, NULL, NULL),
           *c = mk(NODE_PROGRAM, NULL, NULL, NULL);
  AST_NODE *n = ast_create(a, b, c, NODE_WHILE);
  CHECK(n != NULL);
  CHECK_INT(n->type, NODE_WHILE);
  CHECK_PTR(n->left, a);
  CHECK_PTR(n->right, b);
  CHECK_PTR(n->next, c);
  CHECK_PTR(n->else_param, NULL);
  CHECK_PTR(n->name, NULL);
  CHECK_PTR(n->sval, NULL);
  CHECK_INT(n->is_const, 0);
  CHECK_INT(n->line, 0);
  CHECK_INT(n->ival, 0);
  CHECK_DBL(n->dval, 0.0);
  CHECK_INT(n->data_type, 0);
  CHECK_INT(n->op_type, 0);
  free_AST(n);
  check_clean();
}

TEST(t_create_all_types, G1, "funguje pro všechny známé NODE_* typy", 0) {
  scan_known_nodes();
  CHECK(n_known_nodes >= (int)N_NODES);
  for (int i = 0; i < n_known_nodes; i++) {
    AST_NODE *n = ast_create(NULL, NULL, NULL, (NODE_TYPE)known_nodes[i]);
    CHECK_MSG(n && (int)n->type == known_nodes[i], "typ %d", known_nodes[i]);
    free_AST(n);
  }
  check_clean();
}

TEST(t_create_unique_pointers, G1, "500 uzlů má navzájem různé adresy", 0) {
  AST_NODE *v[500];
  for (int i = 0; i < 500; i++) v[i] = mk(NODE_EXPR_STMT, NULL, NULL, NULL);
  int dup_found = 0;
  for (int i = 0; i < 500 && !dup_found; i++)
    for (int j = i + 1; j < 500; j++)
      if (v[i] == v[j]) {
        dup_found = 1;
        break;
      }
  CHECK(!dup_found);
  for (int i = 0; i < 500; i++) free_AST(v[i]);
  check_clean();
}

/* ========================================================================== */
/*  G2  ast_name                                                              */
/* ========================================================================== */
#define G2 "G2  ast_name"

TEST(t_name_copies_input, G2, "uloží KOPII jména (změna zdroje se neprojeví)", 0) {
  char buf[] = "abc";
  AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
  ast_name(n, buf);
  buf[0] = 'X';
  CHECK_STR(n->name, "abc");
  CHECK(n->name != buf);
  free_AST(n);
  check_clean();
}

TEST(t_name_edge_strings, G2, "prázdné, dlouhé a speciální identifikátory", 0) {
  const char *ids[] = {"", "_", "_x1", "Ab_9", "main", "toStr", "a1b2c3_"};
  for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
    ast_name(n, (char *)ids[i]);
    CHECK_MSG(n->name && strcmp(n->name, ids[i]) == 0, "jméno \"%s\"", ids[i]);
    free_AST(n);
  }
  char *big = malloc(5001);
  memset(big, 'a', 5000);
  big[5000] = 0;
  AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
  ast_name(n, big);
  CHECK(n->name && strlen(n->name) == 5000 && strcmp(n->name, big) == 0);
  free_AST(n);
  free(big);
  check_clean();
}

TEST(t_name_replace_value, G2, "druhé volání ast_name nastaví nové jméno", 0) {
  AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
  ast_name(n, (char *)"prvni");
  ast_name(n, (char *)"druhe");
  CHECK_STR(n->name, "druhe");
  free_AST(n);
}

TEST(t_name_replace_leak, G2, "druhé volání ast_name neunikne se starým jménem", 1) {
  AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
  ast_name(n, (char *)"prvni");
  ast_name(n, (char *)"druhe");
  free_AST(n);
  check_clean(); /* ast_name přepíše n->name bez free() -> 1 neuvolněná alokace */
}

/* ========================================================================== */
/*  G3  konstruktory literálů                                                 */
/* ========================================================================== */
#define G3 "G3  literály"

TEST(t_int_literal_values, G3, "ast_int_literal_create: hodnoty, typ uzlu i datový typ", 0) {
  const int vals[] = {0, 1, -1, 42, 1000000, INT_MAX, INT_MIN};
  for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
    AST_NODE *n = ast_int_literal_create(vals[i], NULL);
    CHECK_MSG(n != NULL, "hodnota %d", vals[i]);
    if (!n) continue;
    CHECK_MSG(n->ival == vals[i], "hodnota %d -> ival %lld", vals[i], n->ival);
    CHECK_INT(n->type, NODE_LITERAL);
    CHECK_INT(n->data_type, DATA_INT);
    CHECK_PTR(n->left, NULL);
    CHECK_PTR(n->right, NULL);
    CHECK_PTR(n->name, NULL);
    free_AST(n);
  }
  check_clean();
}

TEST(t_int_literal_next, G3, "ast_int_literal_create: parametr next se uloží", 0) {
  AST_NODE *b = ast_int_literal_create(2, NULL);
  AST_NODE *a = ast_int_literal_create(1, b);
  CHECK_PTR(a->next, b);
  free_AST(a);
  check_clean();
}

TEST(t_double_literal_type, G3, "ast_double_literal_create nastaví data_type = DATA_DOUBLE", 0) {
  AST_NODE *n = ast_double_literal_create(3.5f, NULL);
  CHECK_INT(n->type, NODE_LITERAL);
  CHECK_INT(n->data_type, DATA_DOUBLE); /* ctor data_type nenastavuje -> zůstane DATA_INT */
  free_AST(n);
  check_clean();
}

TEST(t_double_literal_exact_float_values, G3, "double literály přesně reprezentovatelné ve floatu", 0) {
  const double vals[] = {0.0, 0.5, -2.0, 1024.25, 65536.0};
  for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
    AST_NODE *n = ast_double_literal_create(vals[i], NULL);
    CHECK_MSG(n->dval == vals[i], "hodnota %.17g -> dval %.17g", vals[i], n->dval);
    free_AST(n);
  }
  check_clean();
}

TEST(t_double_literal_precision, G3, "double literál zachová plných 64 bitů (spec: IEEE 754 binary64)", 0) {
  volatile double big = 1e300; /* volatile: bez varování o přetečení při převodu na float */
  const double vals[] = {0.1, 3.14, 123456789.123, 16777217.0, big};
  for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
    AST_NODE *n = ast_double_literal_create(vals[i], NULL);
    CHECK_MSG(n->dval == vals[i], "hodnota %.17g -> dval %.17g (parametr je float, ne double)",
              vals[i], n->dval);
    free_AST(n);
  }
  check_clean();
}

TEST(t_string_literal_copy, G3, "ast_string_literal_create uloží KOPII řetězce", 0) {
  char buf[] = "ahoj";
  AST_NODE *n = ast_string_literal_create(buf, NULL);
  buf[0] = 'X';
  CHECK_INT(n->type, NODE_LITERAL);
  CHECK_STR(n->sval, "ahoj");
  CHECK(n->sval != buf);
  CHECK_PTR(n->name, NULL);
  free_AST(n);
  check_clean();
}

TEST(t_string_literal_type, G3, "ast_string_literal_create nastaví data_type = DATA_STRING", 0) {
  AST_NODE *n = ast_string_literal_create((char *)"x", NULL);
  CHECK_INT(n->data_type, DATA_STRING); /* ctor data_type nenastavuje -> zůstane DATA_INT */
  free_AST(n);
  check_clean();
}

TEST(t_string_literal_edge, G3, "prázdný, speciální a dlouhý řetězec + next", 0) {
  AST_NODE *nx = ast_int_literal_create(1, NULL);
  AST_NODE *e = ast_string_literal_create((char *)"", nx);
  CHECK_STR(e->sval, "");
  CHECK_PTR(e->next, nx);
  free_AST(e);
  AST_NODE *s = ast_string_literal_create((char *)"a\nb\t\"q\"\\\r", NULL);
  CHECK_STR(s->sval, "a\nb\t\"q\"\\\r");
  free_AST(s);
  char *big = malloc(100001);
  memset(big, 'z', 100000);
  big[100000] = 0;
  AST_NODE *l = ast_string_literal_create(big, NULL);
  CHECK(l->sval && strlen(l->sval) == 100000 && strcmp(l->sval, big) == 0);
  free_AST(l);
  free(big);
  check_clean();
}

TEST(t_design_unset_type_is_int, G3, "návrh: nenastavený data_type se nesmí tvářit jako Int", 1) {
  /* DATA_INT == 0, takže calloc-nutý uzel vypadá jako Int. Proto chybějící data_type v
   * ast_double_/ast_string_literal_create nikdo nepozná (vypíše se "LITERAL Int 0").
   * Doporučení: přidat DATA_UNKNOWN (= 0) před DATA_INT. */
  CHECK(DATA_INT != 0);
}

TEST(t_literal_null_string, G3, "ast_string_literal_create(NULL) nespadne na segfault", 1) {
  g_exit_armed = 1;
  g_exit_code = -1;
  if (setjmp(g_exit_jmp) == 0) {
    AST_NODE *n = ast_string_literal_create(NULL, NULL);
    g_exit_armed = 0;
    free_AST(n);
  }
  g_exit_armed = 0;
}

/* ========================================================================== */
/*  G4  ident / binop / unop / var_def                                        */
/* ========================================================================== */
#define G4 "G4  ident, operátory, definice proměnné"

TEST(t_ident_basic, G4, "ast_ident_create: name (kopie), left, next, typ", 0) {
  char nm[] = "foo";
  AST_NODE *l = lit_i(1), *nx = ID("bar");
  AST_NODE *n = ast_ident_create(l, nm, nx);
  nm[0] = 'X';
  CHECK_INT(n->type, NODE_IDENT);
  CHECK_STR(n->name, "foo");
  CHECK_PTR(n->left, l);
  CHECK_PTR(n->right, NULL);
  CHECK_PTR(n->next, nx);
  free_AST(n);
  check_clean();
}

TEST(t_binop_all_ops, G4, "ast_binop_create: všechny operátory, left/right/next", 0) {
  for (size_t i = 0; i < N_OPS; i++) {
    AST_NODE *l = lit_i(1), *r = lit_i(2), *nx = lit_i(3);
    AST_NODE *n = ast_binop_create(l, r, nx, OPS[i].op);
    CHECK_INT(n->type, NODE_BINOP);
    CHECK_INT(n->op_type, OPS[i].op);
    CHECK_PTR(n->left, l);
    CHECK_PTR(n->right, r);
    CHECK_PTR(n->next, nx);
    CHECK_PTR(n->name, NULL);
    free_AST(n);
  }
  check_clean();
}

TEST(t_unop_all_ops, G4, "ast_unop_create: operátor, operand v left, right = NULL", 0) {
  for (size_t i = 0; i < N_OPS; i++) {
    AST_NODE *l = lit_i(1), *nx = lit_i(3);
    AST_NODE *n = ast_unop_create(l, nx, OPS[i].op);
    CHECK_INT(n->type, NODE_UNOP);
    CHECK_INT(n->op_type, OPS[i].op);
    CHECK_PTR(n->left, l);
    CHECK_PTR(n->right, NULL);
    CHECK_PTR(n->next, nx);
    free_AST(n);
  }
  check_clean();
}

TEST(t_var_def_const_flag, G4, "ast_var_def_create: val (is_const) vs var", 0) {
  AST_NODE *a = ast_var_def_create(lit_i(1), NULL, (char *)"a", true);
  AST_NODE *b = ast_var_def_create(lit_i(1), NULL, (char *)"b", false);
  CHECK_INT(a->is_const, 1);
  CHECK_INT(b->is_const, 0);
  free_AST(a);
  free_AST(b);
  check_clean();
}

TEST(t_var_def_slots, G4, "ast_var_def_create: init v left, jméno (kopie), next, right = NULL", 0) {
  char nm[] = "result";
  AST_NODE *init = lit_i(5), *nx = ID("y");
  AST_NODE *n = ast_var_def_create(init, nx, nm, false);
  nm[0] = 'X';
  CHECK_INT(n->type, NODE_VAR_DEF);
  CHECK_STR(n->name, "result");
  CHECK_PTR(n->left, init);
  CHECK_PTR(n->right, NULL);
  CHECK_PTR(n->next, nx);
  free_AST(n);
  check_clean();
}

/* ========================================================================== */
/*  G5  if / while / func_def / call                                          */
/* ========================================================================== */
#define G5 "G5  if, while, funkce, volání"

TEST(t_if_cond_then_slots, G5, "ast_if_create: cond=left, then=right, next, typ", 0) {
  AST_NODE *c = BIN(OP_LT, ID("x"), lit_i(5)), *t = mk(NODE_BLOCK, NULL, NULL, NULL),
           *e = mk(NODE_BLOCK, NULL, NULL, NULL), *nx = ID("n");
  AST_NODE *n = ast_if_create(c, t, e, nx);
  CHECK_INT(n->type, NODE_IF);
  CHECK_PTR(n->left, c);
  CHECK_PTR(n->right, t);
  CHECK_PTR(n->next, nx);
  detach_else(n, e);
  free_AST(e);
  free_AST(n);
  check_clean();
}

TEST(t_if_else_stored_by_pointer, G5, "ast_if_create: else větev se uloží jako ukazatel (ne strdup)", 0) {
  AST_NODE *e = mk(NODE_BLOCK, NULL, NULL, NULL);
  AST_NODE *n = ast_if_create(ID("c"), mk(NODE_BLOCK, NULL, NULL, NULL), e, NULL);
  int ok = detach_else(n, e);
  CHECK_MSG(ok, "else_param != předaný uzel; kód dělá strdup(const AST_NODE*) -> else větev se ztratí");
  free_AST(e);
  free_AST(n);
  check_clean();
}

TEST(t_if_null_else, G5, "ast_if_create(.., NULL, ..) = if bez else (BOOLTHEN) nespadne", 0) {
  AST_NODE *n = ast_if_create(ID("c"), mk(NODE_BLOCK, NULL, NULL, NULL), NULL, NULL);
  CHECK(n != NULL);
  if (n) {
    CHECK_PTR(n->else_param, NULL);
    free_AST(n);
  }
  check_clean();
}

TEST(t_if_else_if_chain, G5, "else if: větev else je další uzel IF", 0) {
  AST_NODE *inner = if_manual(ID("c2"), mk(NODE_BLOCK, NULL, NULL, NULL), NULL);
  AST_NODE *n = ast_if_create(ID("c1"), mk(NODE_BLOCK, NULL, NULL, NULL), inner, NULL);
  int ok = detach_else(n, inner);
  CHECK_MSG(ok, "else_param != vnořený IF");
  free_AST(inner);
  free_AST(n);
  check_clean();
}

TEST(t_while_slots, G5, "ast_while_create: cond=left, tělo=right, next, typ", 0) {
  AST_NODE *c = BIN(OP_GT, ID("i"), lit_i(1)), *b = mk(NODE_BLOCK, NULL, NULL, NULL), *nx = ID("n");
  AST_NODE *n = ast_while_create(c, b, nx);
  CHECK_INT(n->type, NODE_WHILE);
  CHECK_PTR(n->left, c);
  CHECK_PTR(n->right, b);
  CHECK_PTR(n->next, nx);
  CHECK_PTR(n->else_param, NULL);
  CHECK_PTR(n->name, NULL);
  free_AST(n);
  check_clean();
}

TEST(t_func_def_slots, G5, "ast_func_def_create: left=tělo, right=návr. typ, next, jméno (kopie)", 0) {
  char nm[] = "foo";
  AST_NODE *body = mk(NODE_BLOCK, NULL, NULL, NULL), *ret = mk(NODE_PARAM, NULL, NULL, NULL),
           *params = named(NODE_PARAM, "a", NULL, NULL, NULL), *nx = mk(NODE_FUNC_DEF, NULL, NULL, NULL);
  AST_NODE *f = ast_func_def_create(body, ret, params, nx, nm);
  nm[0] = 'X';
  CHECK_INT(f->type, NODE_FUNC_DEF);
  CHECK_STR(f->name, "foo");
  CHECK_PTR(f->left, body);
  CHECK_PTR(f->right, ret);
  CHECK_PTR(f->next, nx);
  detach_else(f, params);
  free_AST(params);
  free_AST(f);
  check_clean();
}

TEST(t_func_def_params_stored_by_pointer, G5, "ast_func_def_create: parametry se uloží jako ukazatel", 0) {
  AST_NODE *params = seq(named(NODE_PARAM, "a", NULL, NULL, NULL), named(NODE_PARAM, "b", NULL, NULL, NULL));
  AST_NODE *f = ast_func_def_create(mk(NODE_BLOCK, NULL, NULL, NULL), NULL, params, NULL, (char *)"f");
  int ok = detach_else(f, params);
  CHECK_MSG(ok, "else_param != seznam parametrů; strdup(const AST_NODE*) zničí jejich seznam");
  free_AST(params);
  free_AST(f);
  check_clean();
}

TEST(t_func_def_null_params, G5, "funkce bez parametrů (vč. povinné @main) – params = NULL nespadne", 0) {
  AST_NODE *f = ast_func_def_create(mk(NODE_BLOCK, NULL, NULL, NULL), NULL, NULL, NULL, (char *)"main");
  CHECK(f != NULL);
  if (f) {
    CHECK_PTR(f->else_param, NULL);
    CHECK_STR(f->name, "main");
    free_AST(f);
  }
  check_clean();
}

TEST(t_call_slots, G5, "ast_call_create: typ, jméno (kopie), left/right/next = NULL", 0) {
  char nm[] = "print";
  AST_NODE *args = lit_i(1);
  AST_NODE *c = ast_call_create(args, nm);
  nm[0] = 'X';
  CHECK_INT(c->type, NODE_CALL);
  CHECK_STR(c->name, "print");
  CHECK_PTR(c->left, NULL);
  CHECK_PTR(c->right, NULL);
  CHECK_PTR(c->next, NULL);
  detach_else(c, args);
  free_AST(args);
  free_AST(c);
  check_clean();
}

TEST(t_call_args_stored_by_pointer, G5, "ast_call_create: argumenty se uloží jako ukazatel", 0) {
  AST_NODE *args = lit_i(1);
  AST_NODE *c = ast_call_create(args, (char *)"f");
  int ok = detach_else(c, args);
  CHECK_MSG(ok, "else_param != argumenty; strdup(const AST_NODE*) je zničí");
  free_AST(args);
  free_AST(c);
  check_clean();
}

TEST(t_call_args_chain, G5, "ast_call_create: víc argumentů zřetězených přes next", 0) {
  AST_NODE *a = lit_i(1), *b = lit_i(2), *d = lit_i(3);
  seq(a, seq(b, d));
  AST_NODE *c = ast_call_create(a, (char *)"subStr");
  int ok = detach_else(c, a);
  CHECK_MSG(ok, "else_param != první argument");
  CHECK(a->next == b && b->next == d && d->next == NULL);
  free_AST(a);
  free_AST(c);
  check_clean();
}

TEST(t_call_null_params, G5, "volání bez argumentů (např. readInt()) – params = NULL nespadne", 0) {
  AST_NODE *c = ast_call_create(NULL, (char *)"readInt");
  CHECK(c != NULL);
  if (c) {
    CHECK_PTR(c->else_param, NULL);
    CHECK_STR(c->name, "readInt");
    free_AST(c);
  }
  check_clean();
}

/* ========================================================================== */
/*  G6  free_AST                                                              */
/* ========================================================================== */
#define G6 "G6  free_AST"

TEST(t_free_null, G6, "free_AST(NULL) nic neudělá", 0) {
  free_AST(NULL);
  check_clean();
}

TEST(t_free_single_with_strings, G6, "uvolní uzel i name a sval", 0) {
  AST_NODE *n = mk(NODE_LITERAL, NULL, NULL, NULL);
  ast_name(n, (char *)"nm");
  n->sval = t_strdup("text");
  CHECK_INT(g_live, 3);
  free_AST(n);
  check_clean();
}

TEST(t_free_full_tree, G6, "uvolní left, right, else_param i next (vč. jejich jmen/řetězců)", 0) {
  AST_NODE *n = named(NODE_IF, "n", named(NODE_IDENT, "l", NULL, NULL, NULL), named(NODE_IDENT, "r", NULL, NULL, NULL),
                      named(NODE_IDENT, "nx", NULL, NULL, NULL));
  n->else_param = named(NODE_IDENT, "e", NULL, NULL, NULL);
  n->left->sval = t_strdup("s1");
  n->else_param->sval = t_strdup("s2");
  n->right->next = named(NODE_IDENT, "r2", lit_s("deep"), NULL, NULL);
  free_AST(n);
  check_clean();
}

TEST(t_free_next_chain_10k, G6, "řetěz next o délce 10 000", 0) {
  AST_NODE *h = build_next_chain(10000);
  CHECK_INT(count_nodes(h), 10000);
  free_AST(h);
  check_clean();
}

TEST(t_free_left_chain_10k, G6, "do hloubky zanořený strom (left) o hloubce 10 000", 0) {
  AST_NODE *h = build_left_chain(10000);
  CHECK_INT(count_nodes(h), 10000);
  free_AST(h);
  check_clean();
}

TEST(t_free_next_chain_1M, G6, "řetěz next o délce 1 000 000 (rekurze -> přetečení zásobníku?)", 1) {
  AST_NODE *h = build_next_chain(1000000);
  free_AST(h);
  check_clean();
}

TEST(t_free_left_chain_1M, G6, "hloubka 1 000 000 přes left (rekurze -> přetečení zásobníku?)", 1) {
  AST_NODE *h = build_left_chain(1000000);
  free_AST(h);
  check_clean();
}

/* ========================================================================== */
/*  G7  print_AST a pomocné popisky                                           */
/* ========================================================================== */
#define G7 "G7  print_AST"

TEST(t_node_type_names, G7, "node_type_name: správné a navzájem různé názvy", 0) {
  for (size_t i = 0; i < N_NODES; i++) CHECK_STR(node_type_name(NODES[i].t), NODES[i].name);
  for (size_t i = 0; i < N_NODES; i++)
    for (size_t j = i + 1; j < N_NODES; j++) CHECK(strcmp(NODES[i].name, NODES[j].name) != 0);
}

TEST(t_op_names, G7, "op_name: správné symboly pro všech 13 operátorů", 0) {
  for (size_t i = 0; i < N_OPS; i++) CHECK_STR(op_name(OPS[i].op), OPS[i].sym);
}

TEST(t_data_type_names, G7, "data_type_name: Int, Double, String, Boolean, Unit", 0) {
  CHECK_STR(data_type_name(DATA_INT), "Int");
  CHECK_STR(data_type_name(DATA_DOUBLE), "Double");
  CHECK_STR(data_type_name(DATA_STRING), "String");
  CHECK_STR(data_type_name(DATA_BOOL), "Boolean");
  CHECK_STR(data_type_name(DATA_UNIT), "Unit");
}

TEST(t_names_invalid_enum, G7, "neplatná hodnota enumu -> \"???\"", 0) {
  CHECK_STR(node_type_name((NODE_TYPE)999), "???");
  CHECK_STR(op_name((OP_TYPE)999), "???");
  CHECK_STR(data_type_name((DATA_TYPE)999), "???");
}

TEST(t_child_labels_consistent, G7, "popisky potomků odpovídají tomu, co do slotů ukládají konstruktory", 0) {
  /* Očekávání vychází z konstruktorů v ast.c (ty volá parser). Pokud slotům změníš význam,
   * uprav tuto tabulku. NULL = slot se nekontroluje. Hledá se podřetězec. */
  static const struct {
    NODE_TYPE t;
    const char *l[3];
  } T[] = {{NODE_IF, {"cond", "then", "else"}},
           {NODE_WHILE, {"cond", "body", NULL}},
           {NODE_FUNC_DEF, {"body", "type", "param"}}, /* ctor: left=tělo, right=návr. typ, else=parametry */
           {NODE_CALL, {NULL, NULL, "arg"}},           /* ctor: argumenty jsou v else_param */
           {NODE_VAR_DEF, {"init", NULL, NULL}},
           {NODE_ASSIGN, {"target", "value", NULL}},
           {NODE_BLOCK, {"stmts", NULL, NULL}},
           {NODE_RETURN, {"value", NULL, NULL}},
           {NODE_BINOP, {"lhs", "rhs", NULL}},
           {NODE_UNOP, {"operand", NULL, NULL}}};
  for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
    for (int w = 0; w < 3; w++) {
      if (!T[i].l[w]) continue;
      const char *got = child_label(T[i].t, w);
      CHECK_MSG(got && strstr(got, T[i].l[w]),
                "%s, slot %d (%s): popisek \"%s\", čekáno obsahuje \"%s\"", node_type_name(T[i].t), w,
                w == 0 ? "left" : w == 1 ? "right" : "else_param", got ? got : "NULL", T[i].l[w]);
    }
}

TEST(t_print_null, G7, "print_AST(NULL) nevypíše nic a nespadne", 0) {
  char *so, *se;
  capture_print(NULL, &so, &se);
  CHECK_STR(so, "");
  CHECK_STR(se, "");
  free(so);
  free(se);
}

TEST(t_print_only_stderr, G7, "print_AST píše jen na stderr (stdout je vyhrazen pro IFJcode26)", 0) {
  AST_NODE *n = BIN(OP_ADD, ID("a"), lit_i(1));
  char *so, *se;
  capture_print(n, &so, &se);
  CHECK_STR(so, "");
  CHECK(strlen(se) > 0);
  free(so);
  free(se);
  free_AST(n);
  check_clean();
}

TEST(t_print_each_node_type, G7, "každý druh uzlu vypíše svůj název (+ výchozí příslušenství)", 0) {
  for (size_t i = 0; i < N_NODES; i++) {
    AST_NODE *n = mk(NODES[i].t, NULL, NULL, NULL);
    char *out = cap(n);
    char want[64];
    snprintf(want, sizeof want, "%s%s\n", NODES[i].name, NODES[i].extra);
    CHECK_STR(out, want);
    free(out);
    free_AST(n);
  }
  check_clean();
}

TEST(t_print_literals, G7, "literály všech datových typů (ruční uzly)", 0) {
  struct {
    AST_NODE *n;
    const char *want;
  } T[] = {{lit_i(-5), "LITERAL Int -5\n"},
           {lit_i(0), "LITERAL Int 0\n"},
           {lit_d(3.5), "LITERAL Double 3.5\n"},
           {lit_d(0.1), "LITERAL Double 0.1\n"},
           {lit_d(1e10), "LITERAL Double 1e+10\n"},
           {lit_b(1), "LITERAL Boolean true\n"},
           {lit_b(0), "LITERAL Boolean false\n"},
           {lit_unit(), "LITERAL Unit ()\n"},
           {lit_s("hi"), "LITERAL String \"hi\"\n"},
           {lit_s(""), "LITERAL String \"\"\n"},
           {lit_s(NULL), "LITERAL String \"\"\n"}};
  for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
    char *out = cap(T[i].n);
    CHECK_STR(out, T[i].want);
    free(out);
    free_AST(T[i].n);
  }
  check_clean();
}

TEST(t_print_string_escapes, G7, "řetězec: \\n \\r \\t \\\" \\\\ se vypíšou jako escape sekvence", 0) {
  AST_NODE *n = lit_s("a\nb\t\"q\"\\\r");
  char *out = cap(n);
  CHECK_STR(out, "LITERAL String \"a\\nb\\t\\\"q\\\"\\\\\\r\"\n");
  CHECK_INT(count_lines(out), 1);
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_int_extremes, G7, "Int literál: krajní hodnoty long long", 0) {
  AST_NODE *a = lit_i(LLONG_MIN), *b = lit_i(LLONG_MAX);
  char *o1 = cap(a), *o2 = cap(b);
  CHECK_STR(o1, "LITERAL Int -9223372036854775808\n");
  CHECK_STR(o2, "LITERAL Int 9223372036854775807\n");
  free(o1);
  free(o2);
  free_AST(a);
  free_AST(b);
  check_clean();
}

TEST(t_print_literal_via_ctors, G7, "literály vytvořené KONSTRUKTORY se vypíšou se správným typem", 0) {
  AST_NODE *i = ast_int_literal_create(42, NULL), *d = ast_double_literal_create(3.5f, NULL),
           *s = ast_string_literal_create((char *)"hi", NULL);
  char *oi = cap(i), *od = cap(d), *os = cap(s);
  CHECK_STR(oi, "LITERAL Int 42\n");
  CHECK_STR(od, "LITERAL Double 3.5\n");
  CHECK_STR(os, "LITERAL String \"hi\"\n");
  free(oi);
  free(od);
  free(os);
  free_AST(i);
  free_AST(d);
  free_AST(s);
  check_clean();
}

TEST(t_print_golden_expr, G7, "výraz a + b * 2 (přesný výstup včetně odsazení)", 0) {
  AST_NODE *n = BIN(OP_ADD, ID("a"), BIN(OP_MUL, ID("b"), lit_i(2)));
  char *out = cap(n);
  CHECK_STR(out,
            "BINOP op=+\n"
            "  lhs: IDENT name=a\n"
            "  rhs: BINOP op=*\n"
            "    lhs: IDENT name=b\n"
            "    rhs: LITERAL Int 2\n");
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_golden_while, G7, "while i > 1 do i = i - 1 (přesný výstup)", 0) {
  AST_NODE *assign = mk(NODE_ASSIGN, ID("i"), BIN(OP_SUB, ID("i"), lit_i(1)), NULL);
  AST_NODE *n = ast_while_create(BIN(OP_GT, ID("i"), lit_i(1)), mk(NODE_BLOCK, assign, NULL, NULL), NULL);
  char *out = cap(n);
  CHECK_STR(out,
            "WHILE\n"
            "  cond: BINOP op=>\n"
            "    lhs: IDENT name=i\n"
            "    rhs: LITERAL Int 1\n"
            "  body: BLOCK\n"
            "    stmts: ASSIGN\n"
            "      target: IDENT name=i\n"
            "      value: BINOP op=-\n"
            "        lhs: IDENT name=i\n"
            "        rhs: LITERAL Int 1\n");
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_golden_if_else, G7, "if x < 5 then return 1 else return 2 (přesný výstup)", 0) {
  AST_NODE *t = mk(NODE_BLOCK, mk(NODE_RETURN, lit_i(1), NULL, NULL), NULL, NULL);
  AST_NODE *e = mk(NODE_BLOCK, mk(NODE_RETURN, lit_i(2), NULL, NULL), NULL, NULL);
  AST_NODE *n = if_manual(BIN(OP_LT, ID("x"), lit_i(5)), t, e);
  char *out = cap(n);
  CHECK_STR(out,
            "IF\n"
            "  cond: BINOP op=<\n"
            "    lhs: IDENT name=x\n"
            "    rhs: LITERAL Int 5\n"
            "  then: BLOCK\n"
            "    stmts: RETURN\n"
            "      value: LITERAL Int 1\n"
            "  else: BLOCK\n"
            "    stmts: RETURN\n"
            "      value: LITERAL Int 2\n");
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_golden_siblings, G7, "příkazy v bloku (řetěz next) mají stejné odsazení i popisek", 0) {
  AST_NODE *a = ast_var_def_create(lit_i(1), NULL, (char *)"a", true);
  AST_NODE *b = ast_var_def_create(lit_i(2), NULL, (char *)"b", false);
  AST_NODE *n = mk(NODE_BLOCK, seq(a, b), NULL, NULL);
  char *out = cap(n);
  CHECK_STR(out,
            "BLOCK\n"
            "  stmts: VAR_DEF name=a val\n"
            "    init: LITERAL Int 1\n"
            "  stmts: VAR_DEF name=b var\n"
            "    init: LITERAL Int 2\n");
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_next_at_top_level, G7, "kořeny zřetězené přes next se vypíšou pod sebou na úrovni 0", 0) {
  AST_NODE *n = seq(ID("a"), seq(ID("b"), ID("c")));
  char *out = cap(n);
  CHECK_STR(out, "IDENT name=a\nIDENT name=b\nIDENT name=c\n");
  free(out);
  free_AST(n);
  check_clean();
}

TEST(t_print_line_suffix, G7, "číslo řádku se vypíše jen pro line > 0", 0) {
  AST_NODE *a = ID("x"), *b = ID("y"), *c = ID("z");
  a->line = 12;
  b->line = 0;
  c->line = -3;
  char *oa = cap(a), *ob = cap(b), *oc = cap(c);
  CHECK_STR(oa, "IDENT name=x  [line 12]\n");
  CHECK_STR(ob, "IDENT name=y\n");
  CHECK_STR(oc, "IDENT name=z\n");
  free(oa);
  free(ob);
  free(oc);
  free_AST(a);
  free_AST(b);
  free_AST(c);
  check_clean();
}

TEST(t_print_name_on_any_node, G7, "jméno se vypíše u jakéhokoli uzlu, který ho má", 0) {
  scan_known_nodes();
  for (int i = 0; i < n_known_nodes; i++) {
    AST_NODE *n = mk((NODE_TYPE)known_nodes[i], NULL, NULL, NULL);
    ast_name(n, (char *)"nm");
    char *out = cap(n);
    CHECK_MSG(strstr(out, " name=nm") != NULL, "uzel %s: výstup \"%s\"", node_type_name(n->type), out);
    free(out);
    free_AST(n);
  }
  check_clean();
}

TEST(t_print_ops_in_binop_unop, G7, "BINOP a UNOP vypíší správný symbol operátoru", 0) {
  for (size_t i = 0; i < N_OPS; i++) {
    AST_NODE *b = mk(NODE_BINOP, NULL, NULL, NULL), *u = mk(NODE_UNOP, NULL, NULL, NULL);
    b->op_type = u->op_type = OPS[i].op;
    char *ob = cap(b), *ou = cap(u), wb[32], wu[32];
    snprintf(wb, sizeof wb, "BINOP op=%s\n", OPS[i].sym);
    snprintf(wu, sizeof wu, "UNOP op=%s\n", OPS[i].sym);
    CHECK_STR(ob, wb);
    CHECK_STR(ou, wu);
    free(ob);
    free(ou);
    free_AST(b);
    free_AST(u);
  }
  check_clean();
}

TEST(t_print_idempotent, G7, "print_AST strom nemění; dvojí výpis je shodný", 0) {
  AST_NODE *n = BIN(OP_ADD, ID("a"), BIN(OP_MUL, lit_s("s"), lit_d(1.5)));
  size_t before = count_nodes(n);
  size_t live_before = g_live; /* 5 uzlů + jméno "a" + sval "s" */
  char *o1 = cap(n), *o2 = cap(n);
  CHECK_STR(o1, o2);
  CHECK_INT(count_nodes(n), before);
  CHECK_INT(g_live, live_before); /* výpis nealokuje ani neuvolňuje nic v AST */
  free(o1);
  free(o2);
  free_AST(n);
  check_clean();
}

TEST(t_print_chain_100k, G7, "řetěz next o délce 100 000 -> 100 000 řádků", 0) {
  AST_NODE *h = build_next_chain(100000);
  char *out = cap(h);
  CHECK_INT(count_lines(out), 100000);
  free(out);
  free_AST(h);
  check_clean();
}

TEST(t_print_deep_left_2000, G7, "hloubka 2 000 přes left -> 2 000 řádků", 0) {
  AST_NODE *h = build_left_chain(2000);
  char *out = cap(h);
  CHECK_INT(count_lines(out), 2000);
  free(out);
  free_AST(h);
  check_clean();
}

/* ========================================================================== */
/*  G8  selhání alokace -> exit(99)                                           */
/* ========================================================================== */
#define G8 "G8  selhání alokace (exit 99)"

static AST_NODE g_dummy; /* nulový uzel pro parametr els/params */
typedef AST_NODE *(*MkFn)(void);
static AST_NODE *mk_create(void) { return ast_create(NULL, NULL, NULL, NODE_IF); }
static AST_NODE *mk_int(void) { return ast_int_literal_create(1, NULL); }
static AST_NODE *mk_double(void) { return ast_double_literal_create(1.0f, NULL); }
static AST_NODE *mk_string(void) { return ast_string_literal_create((char *)"s", NULL); }
static AST_NODE *mk_ident(void) { return ast_ident_create(NULL, (char *)"x", NULL); }
static AST_NODE *mk_binop(void) { return ast_binop_create(NULL, NULL, NULL, OP_ADD); }
static AST_NODE *mk_unop(void) { return ast_unop_create(NULL, NULL, OP_NOT); }
static AST_NODE *mk_vardef(void) { return ast_var_def_create(NULL, NULL, (char *)"v", true); }
static AST_NODE *mk_if(void) { return ast_if_create(NULL, NULL, &g_dummy, NULL); }
static AST_NODE *mk_while(void) { return ast_while_create(NULL, NULL, NULL); }
static AST_NODE *mk_func(void) { return ast_func_def_create(NULL, NULL, &g_dummy, NULL, (char *)"f"); }
static AST_NODE *mk_call(void) { return ast_call_create(&g_dummy, (char *)"c"); }

static void oom_sweep(const char *what, MkFn mk_fn) {
  for (long k = 1; k <= 8; k++) {
    g_alloc_calls = 0;
    g_fail_at = k;
    g_fail_hit = 0;
    g_exit_code = -1;
    g_exit_armed = 1;
    AST_NODE *volatile r = NULL;
    if (setjmp(g_exit_jmp) == 0) {
      r = mk_fn();
      g_exit_armed = 0;
    }
    g_exit_armed = 0;
    g_fail_at = 0;
    if (g_fail_hit) {
      CHECK_MSG(g_exit_code == 99, "%s: selhání %ld. alokace -> exit(%d), čekáno exit(99)", what, k,
                g_exit_code);
    } else {
      CHECK_MSG(r != NULL, "%s: bez selhání alokace vrátila NULL", what);
      break;
    }
  }
}

TEST(t_oom_ctors, G8, "každý konstruktor při selhání calloc/strdup skončí exit(99)", 0) {
  oom_sweep("ast_create", mk_create);
  oom_sweep("ast_int_literal_create", mk_int);
  oom_sweep("ast_double_literal_create", mk_double);
  oom_sweep("ast_string_literal_create", mk_string);
  oom_sweep("ast_ident_create", mk_ident);
  oom_sweep("ast_binop_create", mk_binop);
  oom_sweep("ast_unop_create", mk_unop);
  oom_sweep("ast_var_def_create", mk_vardef);
  oom_sweep("ast_if_create", mk_if);
  oom_sweep("ast_while_create", mk_while);
  oom_sweep("ast_func_def_create", mk_func);
  oom_sweep("ast_call_create", mk_call);
}

TEST(t_oom_ast_name, G8, "ast_name při selhání strdup skončí exit(99)", 0) {
  AST_NODE *n = mk(NODE_IDENT, NULL, NULL, NULL);
  g_alloc_calls = 0;
  g_fail_at = 1;
  g_fail_hit = 0;
  g_exit_code = -1;
  g_exit_armed = 1;
  if (setjmp(g_exit_jmp) == 0) {
    ast_name(n, (char *)"x");
    g_exit_armed = 0;
  }
  g_exit_armed = 0;
  g_fail_at = 0;
  CHECK(g_fail_hit);
  CHECK_INT(g_exit_code, 99);
}

/* ========================================================================== */
/*  G9  integrace a náhodné stromy                                            */
/* ========================================================================== */
#define G9 "G9  integrace, fuzz"

/* Program ze zadání (faktoriál + @main). use_ctors = 1: použije konstruktory tak, jak by je
 * volal parser; 0: pole else_param nastaví ručně. Zaznamenává očekávané else_param. */
static struct {
  AST_NODE *node, *expected;
} g_else_chk[16];
static int g_n_else_chk;

static AST_NODE *P_call(int uc, const char *name, AST_NODE *args) {
  AST_NODE *n = uc ? ast_call_create(args, (char *)name) : call_manual(name, args);
  g_else_chk[g_n_else_chk].node = n;
  g_else_chk[g_n_else_chk++].expected = args;
  return n;
}
static AST_NODE *P_func(int uc, const char *name, AST_NODE *body, AST_NODE *ret, AST_NODE *params, AST_NODE *nx) {
  AST_NODE *n = uc ? ast_func_def_create(body, ret, params, nx, (char *)name) : func_manual(name, body, ret, params);
  if (!uc) n->next = nx;
  g_else_chk[g_n_else_chk].node = n;
  g_else_chk[g_n_else_chk++].expected = params;
  return n;
}

static AST_NODE *build_program(int uc) {
  g_n_else_chk = 0;
  /* def factorial(n: Int): Int = ... */
  AST_NODE *loop_body = mk(NODE_BLOCK,
                           seq(mk(NODE_ASSIGN, ID("result"), BIN(OP_MUL, ID("result"), ID("i")), NULL),
                               mk(NODE_ASSIGN, ID("i"), BIN(OP_SUB, ID("i"), lit_i(1)), NULL)),
                           NULL, NULL);
  AST_NODE *stmts = seq(ast_var_def_create(lit_i(1), NULL, (char *)"result", false),
                        seq(ast_var_def_create(ID("n"), NULL, (char *)"i", false),
                            seq(ast_while_create(BIN(OP_GT, ID("i"), lit_i(1)), loop_body, NULL),
                                mk(NODE_EXPR_STMT, ID("result"), NULL, NULL))));
  AST_NODE *params = named(NODE_PARAM, "n", NULL, NULL, NULL);
  params->data_type = DATA_INT;
  AST_NODE *ret = mk(NODE_PARAM, NULL, NULL, NULL);
  ret->data_type = DATA_INT;

  /* @main def runFoo(): Unit = ... */
  AST_NODE *main_stmts =
      seq(ast_var_def_create(P_call(uc, "readInt", NULL), NULL, (char *)"n", true),
          seq(ast_var_def_create(P_call(uc, "factorial", ID("n")), NULL, (char *)"result", true),
              seq(mk(NODE_EXPR_STMT, P_call(uc, "print", ID("result")), NULL, NULL),
                  mk(NODE_EXPR_STMT, P_call(uc, "print", lit_s("\n")), NULL, NULL))));
  AST_NODE *unit = mk(NODE_PARAM, NULL, NULL, NULL);
  unit->data_type = DATA_UNIT;

  AST_NODE *f2 = P_func(uc, "runFoo", mk(NODE_BLOCK, main_stmts, NULL, NULL), unit, NULL, NULL);
  AST_NODE *f1 = P_func(uc, "factorial", mk(NODE_BLOCK, stmts, NULL, NULL), ret, params, f2);
  return mk(NODE_PROGRAM, f1, NULL, NULL);
}

TEST(t_integration_manual_program, G9, "celý program ze zadání (faktoriál + main), pole nastavená ručně", 0) {
  AST_NODE *root = build_program(0);
  size_t nodes = count_nodes(root);
  char *out = cap(root);
  CHECK_MSG(count_lines(out) == nodes, "počet řádků výpisu %zu != počet uzlů %zu", count_lines(out), nodes);
  const char *must[] = {"PROGRAM",
                        "FUNC_DEF name=factorial",
                        "FUNC_DEF name=runFoo",
                        "PARAM name=n",
                        "VAR_DEF name=result var",
                        "VAR_DEF name=i var",
                        "VAR_DEF name=n val",
                        "VAR_DEF name=result val",
                        "WHILE",
                        "BINOP op=>",
                        "BINOP op=*",
                        "BINOP op=-",
                        "ASSIGN",
                        "EXPR_STMT",
                        "CALL name=readInt",
                        "CALL name=factorial",
                        "CALL name=print",
                        "LITERAL String \"\\n\""};
  for (size_t i = 0; i < sizeof must / sizeof must[0]; i++)
    CHECK_MSG(strstr(out, must[i]) != NULL, "ve výpisu chybí \"%s\"", must[i]);
  CHECK(strstr(out, "???") == NULL);
  free(out);
  free_AST(root);
  check_clean();
}

TEST(t_integration_ctors_only, G9, "tentýž program přes konstruktory (jak ho staví parser) = stejný strom", 0) {
  AST_NODE *ref = build_program(0);
  char *want = cap(ref);
  AST_NODE *root = build_program(1);
  int broken = 0;
  for (int i = 0; i < g_n_else_chk; i++)
    if (g_else_chk[i].node->else_param != g_else_chk[i].expected) {
      broken++;
      t_free((void *)g_else_chk[i].node->else_param);
      g_else_chk[i].node->else_param = NULL;
    }
  CHECK_MSG(broken == 0, "%d uzlů (volání/funkce) má poškozený else_param", broken);
  if (!broken) {
    char *got = cap(root);
    char *a = strip_labels(want), *b = strip_labels(got);
    CHECK_STR(b, a);
    free(a);
    free(b);
    free(got);
    free_AST(root);
    free_AST(ref);
    check_clean();
  }
  free(want);
}

static unsigned long long g_rng;
static unsigned rnd(unsigned n) {
  g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (unsigned)((g_rng >> 33) % n);
}
static void rand_str(char *buf, size_t max, int ident) {
  static const char ID_CH[] = "abcXYZ_019";
  static const char ANY_CH[] = "ab cXYZ_019.,;:!?-+*/<>=()[]{}'#$%&|~\n\t\"\\\r";
  size_t len = rnd((unsigned)max);
  for (size_t i = 0; i < len; i++)
    buf[i] = ident ? ID_CH[rnd(sizeof ID_CH - 1)] : ANY_CH[rnd(sizeof ANY_CH - 1)];
  buf[len] = 0;
}
static AST_NODE *rand_tree(int depth, int *budget) {
  if (*budget <= 0 || depth > 10 || rnd(100) < 25) return NULL;
  (*budget)--;
  AST_NODE *n = ast_create(NULL, NULL, NULL, (NODE_TYPE)known_nodes[rnd((unsigned)n_known_nodes)]);
  n->left = rand_tree(depth + 1, budget);
  n->right = rand_tree(depth + 1, budget);
  n->else_param = rand_tree(depth + 1, budget);
  n->next = rand_tree(depth + 1, budget);
  char buf[16];
  if (rnd(2)) {
    rand_str(buf, sizeof buf, 1);
    ast_name(n, buf);
  }
  if (rnd(3) == 0) {
    rand_str(buf, sizeof buf, 0);
    n->sval = t_strdup(buf);
  }
  n->data_type = (DATA_TYPE)rnd(5);
  n->op_type = OPS[rnd((unsigned)N_OPS)].op;
  n->is_const = (bool)rnd(2);
  n->line = (int)rnd(60) - 5;
  n->ival = (long long)rnd(2000000) - 1000000;
  n->dval = (double)rnd(100000) / 8.0;
  return n;
}

TEST(t_fuzz_random_trees, G9, "300 náhodných stromů: 1 řádek na uzel, bez ???, bez úniků", 0) {
  scan_known_nodes();
  for (int seed = 1; seed <= 300; seed++) {
    g_rng = (unsigned long long)seed * 7919ULL;
    int budget = 150;
    AST_NODE *t = rand_tree(0, &budget);
    size_t nodes = count_nodes(t);
    char *so, *se;
    capture_print(t, &so, &se);
    CHECK_MSG(count_lines(se) == nodes, "seed %d: řádků %zu, uzlů %zu", seed, count_lines(se), nodes);
    CHECK_MSG(strstr(se, "???") == NULL, "seed %d: ve výpisu je ???", seed);
    CHECK_MSG(*so == 0, "seed %d: něco na stdout", seed);
    free(so);
    free(se);
    free_AST(t);
    CHECK_MSG(g_live == 0 && g_bad_free == 0, "seed %d: únik %zu / neplatné free %zu", seed, g_live, g_bad_free);
    if (g_failed) break;
  }
}

/* ========================================================================== */
/*  G10  sondy pro rozšíření (podrobný přehled na konci běhu)                 */
/* ========================================================================== */
#define G10 "G10 sondy rozšíření"

/* kolik uzlů a řádků výpisu, žádné ???, žádný únik */
static void probe_tree(AST_NODE *t, const char *const *must, size_t n_must) {
  size_t nodes = count_nodes(t);
  char *out = cap(t);
  CHECK_MSG(count_lines(out) == nodes, "řádků %zu != uzlů %zu", count_lines(out), nodes);
  CHECK(strstr(out, "???") == NULL);
  for (size_t i = 0; i < n_must; i++) CHECK_MSG(strstr(out, must[i]) != NULL, "ve výpisu chybí \"%s\"", must[i]);
  free(out);
  free_AST(t);
  check_clean();
}

TEST(t_ext_funexp_nested_calls, G10, "FUNEXP: 2 * factorial(readInt() + 1) a vnořená volání s více argumenty", 0) {
  AST_NODE *a = ast_var_def_create(
      BIN(OP_MUL, lit_i(2), call_manual("factorial", BIN(OP_ADD, call_manual("readInt", NULL), lit_i(1)))), NULL,
      (char *)"result", true);
  const char *m1[] = {"VAR_DEF name=result val", "CALL name=factorial", "CALL name=readInt", "BINOP op=*"};
  probe_tree(a, m1, 4);
  /* print(toStr(result + strLength(subStr("abcd", 1, 3)))) */
  AST_NODE *sub = call_manual("subStr", seq(lit_s("abcd"), seq(lit_i(1), lit_i(3))));
  AST_NODE *b = call_manual("print", call_manual("toStr", BIN(OP_ADD, ID("result"), call_manual("strLength", sub))));
  const char *m2[] = {"CALL name=print", "CALL name=toStr", "CALL name=strLength", "CALL name=subStr"};
  probe_tree(b, m2, 4);
}

TEST(t_ext_boolthen_ops_and_literals, G10, "BOOLTHEN: !(a < b) && c || d, literály true/false", 0) {
  AST_NODE *t = BIN(OP_OR, BIN(OP_AND, UN(OP_NOT, BIN(OP_LT, ID("a"), ID("b"))), ID("c")), lit_b(0));
  AST_NODE *v = ast_var_def_create(seq(t, lit_b(1)), NULL, (char *)"ok", false);
  const char *m[] = {"UNOP op=!", "BINOP op=&&", "BINOP op=||", "LITERAL Boolean true", "LITERAL Boolean false"};
  probe_tree(v, m, 5);
}

TEST(t_ext_boolthen_else_if_and_short_if, G10, "BOOLTHEN: if bez else a řetěz else if (ruční uzly)", 0) {
  AST_NODE *chain = if_manual(BIN(OP_LT, ID("x"), lit_i(0)), mk(NODE_BLOCK, NULL, NULL, NULL),
                              if_manual(BIN(OP_EQ, ID("x"), lit_i(0)), mk(NODE_BLOCK, NULL, NULL, NULL),
                                        mk(NODE_BLOCK, NULL, NULL, NULL)));
  const char *m[] = {"else: IF", "BINOP op==", "else: BLOCK"};
  probe_tree(chain, m, 3);
  AST_NODE *short_if = if_manual(ID("c"), mk(NODE_BLOCK, NULL, NULL, NULL), NULL);
  char *out = cap(short_if);
  CHECK(strstr(out, "else") == NULL);
  free(out);
  free_AST(short_if);
  check_clean();
}

TEST(t_ext_control_if_expression, G10, "CONTROL: if jako výraz (val absolute = if x < 0 then 0 - x else x)", 0) {
  AST_NODE *e = if_manual(BIN(OP_LT, ID("x"), lit_i(0)), BIN(OP_SUB, lit_i(0), ID("x")), ID("x"));
  AST_NODE *v = ast_var_def_create(e, NULL, (char *)"absolute", true);
  const char *m[] = {"VAR_DEF name=absolute val", "init: IF", "then: BINOP op=-", "else: IDENT name=x"};
  probe_tree(v, m, 4);
}

TEST(t_ext_interp_parts, G10, "INTERPOLATEDS: s\"$name${n + 1} costs $$0\" jako NODE_INTERP s částmi přes next", 0) {
  AST_NODE *parts = seq(ID("name"), seq(BIN(OP_ADD, ID("n"), lit_i(1)), lit_s(" costs $0")));
  AST_NODE *n = mk(NODE_INTERP, parts, NULL, NULL);
  const char *m[] = {"INTERP", "IDENT name=name", "BINOP op=+", "LITERAL String \" costs $0\""};
  probe_tree(n, m, 4);
}

TEST(t_ext_valmem_desugar, G10, "VALMEM: \"abc\".length, x.toString, d.floor.toInt lze přepsat na NODE_CALL", 0) {
  const char *ma[] = {"CALL name=strLength"}, *mb[] = {"CALL name=toStr"}, *mc[] = {"CALL name=toInt", "CALL name=floor"};
  probe_tree(call_manual("strLength", lit_s("abc")), ma, 1);
  probe_tree(call_manual("toStr", ID("x")), mb, 1);
  probe_tree(call_manual("toInt", call_manual("floor", ID("d"))), mc, 2);
}

/* ========================================================================== */
/*  5) Přehled rozšíření                                                      */
/* ========================================================================== */

typedef enum { R_NOTRUN = 0, R_PASS, R_FAIL, R_CRASH, R_TIME, R_WARN } Result;
static Result g_res[512];

static Result result_of(const char *id) {
  for (int i = 0; i < g_ntests; i++)
    if (strcmp(g_tests[i].id, id) == 0) return g_res[i];
  return R_NOTRUN;
}
static int passed(const char *id) { return result_of(id) == R_PASS; }

/* hledání prvku enumu podle jména z printerů (automaticky zachytí i později přidané uzly) */
static int find_node(const char *const *needles) {
  for (int i = 0; i < ENUM_SCAN; i++) {
    const char *nm = node_type_name((NODE_TYPE)i);
    if (!strcmp(nm, "???")) continue;
    for (int k = 0; needles[k]; k++)
      if (strstr(nm, needles[k])) return i;
  }
  return -1;
}
static int find_data(const char *const *needles) {
  for (int i = 0; i < ENUM_SCAN; i++) {
    const char *nm = data_type_name((DATA_TYPE)i);
    if (!strcmp(nm, "???")) continue;
    for (int k = 0; needles[k]; k++)
      if (strstr(nm, needles[k])) return i;
  }
  return -1;
}
static int find_op(const char *sym) {
  for (int i = 0; i < ENUM_SCAN; i++)
    if (!strcmp(op_name((OP_TYPE)i), sym)) return i;
  return -1;
}

static void ext_row(const char *tag, const char *id, const char *text) { printf("  %s %-14s %s\n", tag, id, text); }

static void print_ext_report(void) {
  const char *n_range[] = {"RANGE", "GENERATOR", NULL};
  const char *n_member[] = {"MEMBER", "FIELD", "DOT", "PROP", "SELECT", NULL};
  const char *n_match[] = {"MATCH", "CASE", "PATTERN", NULL};
  const char *n_adt[] = {"CLASS", "UNION", "ALIAS", "TYPEDEF", NULL};
  const char *n_lambda[] = {"LAMBDA", "ANON", "FUNC_TYPE", NULL};
  const char *n_list[] = {"LIST", NULL};
  const char *n_ext[] = {"EXTENSION", "EXT_", NULL};
  const char *n_plist[] = {"PARAM_LIST", "PARAMLIST", "ARG_LIST", "ARGLIST", NULL};
  const char *d_union[] = {"Union", "Class", "Alias", "|", NULL};
  const char *d_func[] = {"Func", "=>", "Lambda", NULL};
  const char *d_list[] = {"List", NULL};
  char buf[512];

  printf("\n================ PODPORA ROZŠÍŘENÍ IFJ26 V AST ================\n");
  printf("  (uzly/typy/operátory se hledají podle názvů ve výpisových funkcích ast.c;\n");
  printf("   když něco přidáš do enumu a do příslušného switch, přehled se aktualizuje sám)\n\n");

  /* FUNEXP */
  {
    int probe = passed("t_ext_funexp_nested_calls");
    int block = !passed("t_call_null_params") || !passed("t_call_args_stored_by_pointer");
    snprintf(buf, sizeof buf, "%s%s",
             probe ? "volání (NODE_CALL) jde vnořit do výrazů a argumentů" : "sonda selhala – volání ve výrazech nefunguje",
             block ? "; BLOKOVÁNO chybami v ast_call_create (viz G5: t_call_*)" : "");
    ext_row(!probe ? "[ NO ]" : block ? "[PART]" : "[ OK ]", "FUNEXP", buf);
  }
  /* BOOLTHEN */
  {
    int probe = passed("t_ext_boolthen_ops_and_literals") && passed("t_ext_boolthen_else_if_and_short_if");
    int block = !passed("t_if_null_else") || !passed("t_if_else_stored_by_pointer");
    snprintf(buf, sizeof buf, "%s%s",
             probe ? "DATA_BOOL, OP_NOT/AND/OR, else-if řetěz i if bez else jsou strukturálně možné (literál true/false se nastaví "
                     "ručně, bool ctor chybí)"
                   : "sonda selhala",
             block ? "; BLOKOVÁNO: ast_if_create(…, NULL, …) padá / else se ztrácí (G5: t_if_*)" : "");
    ext_row(!probe ? "[ NO ]" : block ? "[PART]" : "[ OK ]", "BOOLTHEN", buf);
  }
  /* CONTROL */
  {
    int probe = passed("t_ext_control_if_expression");
    int range = find_node(n_range) >= 0;
    snprintf(buf, sizeof buf, "if jako výraz: %s. for: %s",
             probe ? "ano (NODE_IF kdekoli ve výrazu)" : "sonda selhala",
             range ? "nalezen uzel pro rozsah (neověřeno)"
                   : "NE – NODE_FOR nemá kam uložit řídicí proměnnou + from + to/until + step + stráže + tělo "
                     "(volné jen left/right/else_param); chybí konstruktor i uzel pro rozsah");
    ext_row(!probe ? "[ NO ]" : range ? "[ OK ]" : "[PART]", "CONTROL", buf);
  }
  /* VALMEM */
  {
    int member = find_node(n_member) >= 0;
    int probe = passed("t_ext_valmem_desugar");
    snprintf(buf, sizeof buf, "%s",
             member ? "nalezen uzel pro přístup ke členu (neověřeno)"
                    : "žádný uzel pro výraz.id; jde desugarovat na NODE_CALL (toString→toStr, toInt→toInt, length→strLength), "
                      "ale floor nemá vestavěný protějšek – generátor ho musí doplnit");
    ext_row(member ? "[ OK ]" : probe ? "[PART]" : "[ NO ]", "VALMEM", buf);
  }
  /* ALGEBDT */
  {
    int m = find_node(n_match) >= 0, c = find_node(n_adt) >= 0, d = find_data(d_union) >= 0;
    snprintf(buf, sizeof buf, "%s",
             (m && c && d) ? "nalezeny uzly pro match/třídy i datový typ (neověřeno)"
                           : "chybí: uzly match/case/vzory, case class, type alias, konstruktor datové třídy; DATA_TYPE nezná "
                             "sjednocení (Int|Unit …) ani datovou třídu");
    ext_row((m && c && d) ? "[ OK ]" : "[ NO ]", "ALGEBDT", buf);
  }
  /* INTERPOLATEDS */
  {
    int probe = passed("t_ext_interp_parts");
    snprintf(buf, sizeof buf, "%s",
             probe ? "NODE_INTERP existuje; části (literály/výrazy) jako řetěz left→next. Chybí konstruktor a popisky v printeru"
                   : "sonda selhala");
    ext_row(probe ? "[ OK ]" : "[ NO ]", "INTERPOLATEDS", buf);
  }
  /* LAMBDAS */
  {
    int l = find_node(n_lambda) >= 0, f = find_data(d_func) >= 0;
    snprintf(buf, sizeof buf, "%s",
             (l && f) ? "nalezen uzel lambdy i funkční typ (neověřeno)"
                      : "chybí: uzel lambda funkce, funkční typ (T1,…)=>R v DATA_TYPE, volání hodnoty typu funkce");
    ext_row((l && f) ? "[ OK ]" : "[ NO ]", "LAMBDAS", buf);
  }
  /* LISTS */
  {
    int l = find_node(n_list) >= 0, d = find_data(d_list) >= 0, o = find_op("++") >= 0;
    snprintf(buf, sizeof buf, "%s",
             (l && d && o) ? "nalezeny uzly, typ i operátor ++ (neověřeno)"
                           : "chybí: typ List[T] v DATA_TYPE, uzel List(...)/List.range, operátor ++ v OP_TYPE (jen přes NODE_CALL hack)");
    ext_row((l && d && o) ? "[ OK ]" : "[ NO ]", "LISTS", buf);
  }
  /* EXTKW */
  {
    int e = find_node(n_ext) >= 0;
    snprintf(buf, sizeof buf, "%s",
             e ? "nalezen uzel pro extension (neověřeno)"
               : "chybí: uzel extension a příjemce (value: T) ve FUNC_DEF; nejde rozlišit bezparametrickou metodu (.twice) od metody s ()");
    ext_row(e ? "[ OK ]" : "[ NO ]", "EXTKW", buf);
  }
  /* PARAMLISTS */
  {
    int p = find_node(n_plist) >= 0;
    snprintf(buf, sizeof buf, "%s",
             p ? "nalezen uzel pro seznam parametrů (neověřeno)"
               : "chybí: FUNC_DEF má jediný řetěz parametrů – nejde zapsat víc seznamů, prázdný seznam ani funkci bez seznamu (def f: Int)");
    ext_row(p ? "[ OK ]" : "[ NO ]", "PARAMLISTS", buf);
  }

  printf("\n  Legenda: [ OK ] podporováno, [PART] částečně / blokováno chybou, [ NO ] nepodporováno.\n");
  printf("  Pozn.: u rozšíření, která AST nepodporuje, je potřeba přidat NODE_*/DATA_*/OP_* a case do\n");
  printf("         node_type_name/op_name/data_type_name/child_label; pak se přehled sám přepne.\n");
}

/* ========================================================================== */
/*  6) Spouštěč                                                               */
/* ========================================================================== */

static int cmp_tests(const void *a, const void *b) {
  return ((const TestCase *)a)->line - ((const TestCase *)b)->line;
}

int main(int argc, char **argv) {
  const char *filter = NULL;
  int quiet = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-q")) quiet = 1;
    else filter = argv[i];
  }

  g_shm = mmap(NULL, sizeof *g_shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (g_shm == MAP_FAILED) {
    perror("mmap");
    return 2;
  }
  qsort(g_tests, (size_t)g_ntests, sizeof g_tests[0], cmp_tests);

  int n_ok = 0, n_fail = 0, n_crash = 0, n_time = 0, n_warn = 0, n_run = 0;
  const char *last_group = NULL;

  printf("test_ast – kompletní test AST modulu (IFJ26)\n");
  for (int i = 0; i < g_ntests; i++) {
    TestCase *t = &g_tests[i];
    if (filter && !strstr(t->group, filter) && !strstr(t->id, filter)) continue;
    n_run++;

    g_shm->len = 0;
    g_shm->buf[0] = 0;
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return 2;
    }
    if (pid == 0) {
      alarm(20);
      g_failed = 0;
      t->fn();
      _exit(g_failed ? 1 : 0);
    }
    int st = 0;
    waitpid(pid, &st, 0);

    Result r;
    const char *extra = "";
    char sigbuf[64] = "";
    if (WIFSIGNALED(st)) {
      int sg = WTERMSIG(st);
      r = (sg == SIGALRM) ? R_TIME : R_CRASH;
      snprintf(sigbuf, sizeof sigbuf, "      proces ukončen signálem %d (%s)\n", sg, strsignal(sg));
      extra = sigbuf;
    } else if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
      r = R_PASS;
    } else {
      r = R_FAIL;
    }
    if (r != R_PASS && t->soft) r = R_WARN;
    g_res[i] = r;

    switch (r) {
    case R_PASS: n_ok++; break;
    case R_FAIL: n_fail++; break;
    case R_CRASH: n_crash++; break;
    case R_TIME: n_time++; break;
    case R_WARN: n_warn++; break;
    default: break;
    }

    if (!last_group || strcmp(last_group, t->group) != 0) {
      if (!quiet || r != R_PASS) printf("\n%s\n", t->group);
      last_group = t->group;
    }
    if (r == R_PASS && quiet) continue;
    const char *tag = r == R_PASS ? "[ OK ]" : r == R_FAIL ? "[FAIL]" : r == R_CRASH ? "[CRSH]" : r == R_TIME ? "[TIME]" : "[WARN]";
    printf("  %s %-38s %s\n", tag, t->id, t->title);
    if (r != R_PASS) {
      g_shm->buf[sizeof g_shm->buf - 1] = 0;
      fputs(g_shm->buf, stdout);
      fputs(extra, stdout);
    }
  }

  printf("\n-----------------------------------------------------------------\n");
  printf("Spuštěno %d testů: %d OK, %d FAIL, %d CRSH, %d TIME, %d WARN (měkké)\n", n_run, n_ok, n_fail, n_crash,
         n_time, n_warn);

  if (!filter) print_ext_report();
  int bad = n_fail + n_crash + n_time;
  printf("\nVýsledek: %s\n", bad ? "NEÚSPĚCH" : "ÚSPĚCH");
  return bad ? 1 : 0;
}
