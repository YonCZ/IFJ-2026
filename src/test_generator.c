/*
 * Projekt: IFJ26 – překladač jazyka IFJ26 do IFJcode26
 * Soubor:  test_generator.c – test generátoru kódu (fáze 0: kostra, fáze 1: konstanty a print)
 *
 * ---------------------------------------------------------------------------
 * Tento soubor vygenerovala AI: Claude Sonnet 5.5 (Anthropic, claude-sonnet-5-5), 2026-11-17.
 * Prompt 1: "Mám hotovou fázi nula, chci, abys vytvořil nějaké testy, se kterými to můžu
 *            vyzkoušet. Nemáme parser ani sémantickou analýzu, chci otestovat jenom můj
 *            generátor kódu s AST. Udělej mi testovací skript."
 * Prompt 2: "vytvoř mi test na fázi 1" (konstanty a print: Int, Double, String, Boolean, ())
 * Zadal a zrevidoval: Pavel Novák (xnovakp01)
 * ---------------------------------------------------------------------------
 *
 * CO TEST DĚLÁ
 *   Postaví ručně malý AST (bez parseru a sémantiky), zavolá generate_program(), zkontroluje
 *   tvar vygenerovaného IFJcode26 a pak spočítá, co by program vypsal, a porovná to s tím,
 *   co vypsat má. Výpis počítá malý simulátor přímo v tomto souboru (umí jen instrukce, které
 *   fáze 0 a 1 potřebují), takže test funguje i bez interpretu ic26int. Pokud ic26int najde
 *   a jde spustit, pustí v něm kód ještě jednou a porovná výstup také.
 *   Každý případ běží ve vlastním procesu, takže pád generátoru shodí jen ten jeden případ.
 *
 * PŘEKLAD (z adresáře src/; ast.c a generátor se linkují normálně, bez unity buildu):
 *
 *     gcc -std=gnu11 -g -O0 -Wall -Wextra -I. test_generator.c ast.c generator.c gen_*.c \
 *         -o test_generator && ./test_generator
 *
 * POUŽITÍ
 *     ./test_generator              všechny případy (fáze 0 i 1)
 *     ./test_generator f1           jen fáze 1 (f0 = jen fáze 0)
 *     ./test_generator f1_string    jen případ s tímto jménem
 *     ./test_generator -v           vypíše vygenerovaný kód i u případů, které prošly
 *     INTERP=cesta/k/ic26int ./test_generator
 *                                   interpret v jiném místě (smí obsahovat přepínače, cesta
 *                                   ke kódu se přidá na konec). Bez INTERP se hledá ./ic26int,
 *                                   ../ic26int a totéž s příponou -linux-x86_64 a v PATH.
 *                                   Interpret, který na tomhle počítači nejde spustit (třeba
 *                                   Linux program na Macu), se přeskočí.
 *
 * CO SE KONTROLUJE (podle specifikace IFJcode26 a plánu generátoru)
 *   Tvar kódu:
 *   - první řádek je .IFJcode26, výstup končí novým řádkem, na řádku je jedna instrukce
 *   - návěští mají povolené znaky (stejná pravidla jako jméno proměnné, tedy bez @)
 *   - právě jedno CALL na funkci s @main, každá funkce z AST má právě jedno LABEL
 *   - CALL a EXIT int@0 jsou před definicemi funkcí (jinak by se do funkce spadlo)
 *   - každé RETURN je hned za POPFRAME a před POPFRAME je PUSHS nil@nil (Unit vrací nil@nil)
 *   Chování (simulátor, a pokud je k dispozici, i ic26int):
 *   - program doběhne s kódem 0 a vypíše přesně to, co má (jediný znak navíc je chyba)
 *   - žádná běhová chyba: proměnná bez DEFVAR, dvojí DEFVAR, prázdný zásobník, chybný rámec
 *   - po skončení programu na zásobníku nezůstane nic navíc (příkaz po sobě nezanechá nic;
 *     jedna hodnota smí zůstat: návratová hodnota main)
 *   Řetězce: u případů s mezerou, novým řádkem, # a \ se navíc kontroluje přesný zápis
 *   literálu v kódu (např. string@a\032b\010).
 *
 * KDE UPRAVIT, KDYŽ SE ZMĚNÍ ROZHRANÍ
 *   make_program() – jak se skládá NODE_PROGRAM (teď: left = první funkce, další přes next)
 *   make_body()    – jak se skládá tělo funkce (teď: NODE_BLOCK, v left první příkaz, další
 *                    příkazy přes next; takhle to staví i test_ast.c)
 *   print_stmt()   – jak vypadá print (teď: NODE_EXPR_STMT → NODE_CALL "print" → argument
 *                    v third)
 *   run_generator() – jak se volá generátor (teď: Gen gen = {0}; generate_program(&gen, root))
 *   CASES[]        – další případy; pro další fáze přidávej nové případy do tohoto souboru
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "generator.h"

/* ------------------------------------------------------------------------- */
/*  Místa, která se upravují při změně rozhraní                              */
/* ------------------------------------------------------------------------- */

static AST_NODE *make_program(AST_NODE *first_function) {
  return ast_create(first_function, NULL, NULL, NODE_PROGRAM);
}

static AST_NODE *make_body(AST_NODE *first_statement) {
  if (!first_statement) return NULL;
  return ast_create(first_statement, NULL, NULL, NODE_BLOCK);
}

/* print(arg); jako příkaz, další příkaz se napojí přes next */
static AST_NODE *print_stmt(AST_NODE *arg, AST_NODE *next) {
  AST_NODE *call = ast_call_create(arg, (char *)"print", false, true, NULL);
  return ast_create(call, NULL, next, NODE_EXPR_STMT);
}

static void run_generator(AST_NODE *program) {
  Gen gen = {0};
  generate_program(&gen, program);
}

/* Funkce bez parametrů, návratový typ Unit. */
static AST_NODE *func(const char *name, bool is_main, AST_NODE *body, AST_NODE *next) {
  return ast_func_def_create(make_body(body), NULL, next, DATA_UNIT, (char *)name, is_main, true);
}

/* Literály */
static AST_NODE *lit_s(const char *s) { return ast_string_literal_create((char *)s, NULL); }
static AST_NODE *lit_i(long long v) { return ast_int_literal_create(v, NULL); }
static AST_NODE *lit_d(double v) { return ast_double_literal_create(v, NULL); }
static AST_NODE *lit_b(bool v) { return ast_bool_literal_create(v, NULL); }
static AST_NODE *lit_u(void) { return ast_unit_literal_create(NULL); }

/* ------------------------------------------------------------------------- */
/*  Případy                                                                  */
/* ------------------------------------------------------------------------- */

/* fáze 0 */
static AST_NODE *build_empty(void) { return make_program(func("main", true, NULL, NULL)); }
static AST_NODE *build_named_run(void) { return make_program(func("run", true, NULL, NULL)); }
static AST_NODE *build_main_first(void) {
  return make_program(func("main", true, NULL, func("helper", false, NULL, NULL)));
}
static AST_NODE *build_main_last(void) {
  return make_program(func("helper", false, NULL, func("main", true, NULL, NULL)));
}
static AST_NODE *build_three(void) {
  return make_program(
      func("alpha", false, NULL, func("main", true, NULL, func("beta", false, NULL, NULL))));
}

/* fáze 1: v main je vždy jen print(...) s jednou konstantou */
static AST_NODE *main_prints(AST_NODE *stmts) { return make_program(func("main", true, stmts, NULL)); }

static AST_NODE *build_f1_string(void) { return main_prints(print_stmt(lit_s("Ahoj\n"), NULL)); }
static AST_NODE *build_f1_string_plain(void) { return main_prints(print_stmt(lit_s("Ahoj"), NULL)); }
static AST_NODE *build_f1_empty_string(void) {
  /* prázdný řetězec mezi dvěma jinými, aby případ neprošel "naprázdno" i bez print */
  return main_prints(print_stmt(lit_s("<"), print_stmt(lit_s(""), print_stmt(lit_s(">"), NULL))));
}
static AST_NODE *build_f1_specials(void) {
  return main_prints(print_stmt(lit_s("a b\n\t#\\c"), NULL));
}
static AST_NODE *build_f1_int(void) { return main_prints(print_stmt(lit_i(42), NULL)); }
static AST_NODE *build_f1_int_zero(void) { return main_prints(print_stmt(lit_i(0), NULL)); }
static AST_NODE *build_f1_int_big(void) {
  return main_prints(print_stmt(lit_i(1234567890123LL), NULL));
}
static AST_NODE *build_f1_double(void) { return main_prints(print_stmt(lit_d(3.5), NULL)); }
static AST_NODE *build_f1_bool_true(void) { return main_prints(print_stmt(lit_b(true), NULL)); }
static AST_NODE *build_f1_bool_false(void) { return main_prints(print_stmt(lit_b(false), NULL)); }
static AST_NODE *build_f1_unit(void) { return main_prints(print_stmt(lit_u(), NULL)); }
static AST_NODE *build_f1_sequence(void) {
  return main_prints(print_stmt(lit_s("A"), print_stmt(lit_i(1), print_stmt(lit_s("B\n"), NULL))));
}
static AST_NODE *build_f1_in_helper(void) {
  /* print není jen v main: i v další funkci musí vzniknout správný kód (volat se nebude) */
  return make_program(func("main", true, print_stmt(lit_s("x"), NULL),
                           func("helper", false, print_stmt(lit_s("y"), NULL), NULL)));
}

typedef struct {
  const char *name;
  int phase;
  const char *entry;      /* funkce s @main */
  const char *others[3];  /* ostatní funkce, ukončeno NULL */
  AST_NODE *(*build)(void);
  const char *desc;
  const char *expect_out; /* co má program vypsat na stdout */
  const char *must[3];    /* slova (tokeny), která se musí v kódu objevit; ukončeno NULL */
} Case;

static const Case CASES[] = {
    {"empty", 0, "main", {NULL}, build_empty, "jen main, prázdné tělo", "", {NULL}},
    {"named_run", 0, "run", {NULL}, build_named_run, "@main funkce se jmenuje run", "", {NULL}},
    {"main_first", 0, "main", {"helper", NULL}, build_main_first, "main a za ní helper", "", {NULL}},
    {"main_last", 0, "main", {"helper", NULL}, build_main_last, "helper a za ní main", "", {NULL}},
    {"three", 0, "main", {"alpha", "beta", NULL}, build_three, "main uprostřed, dvě další funkce", "",
     {NULL}},

    {"f1_string", 1, "main", {NULL}, build_f1_string, "print(\"Ahoj\\n\")", "Ahoj\n",
     {"string@Ahoj\\010", NULL}},
    {"f1_string_plain", 1, "main", {NULL}, build_f1_string_plain, "print(\"Ahoj\") bez nového řádku",
     "Ahoj", {NULL}},
    {"f1_empty_string", 1, "main", {NULL}, build_f1_empty_string,
     "print(\"<\"), print(\"\"), print(\">\"): prázdný řetězec nevypíše nic", "<>", {NULL}},
    {"f1_specials", 1, "main", {NULL}, build_f1_specials,
     "řetězec s mezerou, \\n, tabulátorem, # a zpětným lomítkem", "a b\n\t#\\c",
     {"string@a\\032b\\010\\009\\035\\092c", NULL}},
    {"f1_int", 1, "main", {NULL}, build_f1_int, "print(42)", "42", {NULL}},
    {"f1_int_zero", 1, "main", {NULL}, build_f1_int_zero, "print(0)", "0", {NULL}},
    {"f1_int_big", 1, "main", {NULL}, build_f1_int_big, "print(1234567890123), 64bitové Int",
     "1234567890123", {NULL}},
    {"f1_double", 1, "main", {NULL}, build_f1_double, "print(3.5) vypíše hexadecimálně", "0x1.cp+1",
     {NULL}},
    {"f1_bool_true", 1, "main", {NULL}, build_f1_bool_true, "print(true)", "true", {NULL}},
    {"f1_bool_false", 1, "main", {NULL}, build_f1_bool_false, "print(false)", "false", {NULL}},
    {"f1_unit", 1, "main", {NULL}, build_f1_unit, "print(()) vypíše ()", "()", {NULL}},
    {"f1_sequence", 1, "main", {NULL}, build_f1_sequence, "tři print za sebou", "A1B\n", {NULL}},
    {"f1_in_helper", 1, "main", {"helper", NULL}, build_f1_in_helper,
     "print i v další funkci, která se nevolá", "x", {"string@y", NULL}},
};
#define N_CASES (sizeof CASES / sizeof CASES[0])

/* ------------------------------------------------------------------------- */
/*  Hlášení problémů                                                         */
/* ------------------------------------------------------------------------- */

#define MAX_MSG 24
static char g_msg[MAX_MSG][420];
static int g_nmsg;

static void problem(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void problem(const char *fmt, ...) {
  if (g_nmsg >= MAX_MSG) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g_msg[g_nmsg++], sizeof g_msg[0], fmt, ap);
  va_end(ap);
}

/* ------------------------------------------------------------------------- */
/*  Pomocné funkce                                                           */
/* ------------------------------------------------------------------------- */

static char *read_file(const char *path, size_t *len_out) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)len + 1);
  if (!buf) exit(99);
  size_t got = fread(buf, 1, (size_t)len, f);
  buf[got] = '\0';
  fclose(f);
  if (len_out) *len_out = got;
  return buf;
}

/* Zobrazí text čitelně (\n jako \n atd.). Používá střídavé buffery, jde volat víckrát v jednom printf. */
static const char *esc(const char *s, size_t n) {
  static char buf[4][200];
  static int k;
  char *b = buf[k++ & 3];
  size_t o = 0;
  for (size_t i = 0; i < n && o < 190; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '\n')
      o += (size_t)snprintf(b + o, 200 - o, "\\n");
    else if (c == '\t')
      o += (size_t)snprintf(b + o, 200 - o, "\\t");
    else if (c == '\\')
      o += (size_t)snprintf(b + o, 200 - o, "\\\\");
    else if (c < 32 || c > 126)
      o += (size_t)snprintf(b + o, 200 - o, "\\x%02x", c);
    else
      b[o++] = (char)c;
  }
  if (o >= 190) o += (size_t)snprintf(b + o, 200 - o, "...");
  b[o] = '\0';
  return b;
}

/* Povolené znaky návěští: stejná pravidla jako jméno proměnné (část za @). */
static bool valid_label(const char *s) {
  static const char special[] = "_-$&%*!?";
  if (!s[0]) return false;
  if (!isalpha((unsigned char)s[0]) && !strchr(special, s[0])) return false;
  for (const char *p = s + 1; *p; p++)
    if (!isalnum((unsigned char)*p) && !strchr(special, *p)) return false;
  return true;
}

static bool is_sep(char c) { return c == '\0' || c == '$' || c == '_'; }

/* Obsahuje návěští jméno funkce jako samostatnou část? ($f_main ano, $f_domain ne) */
static bool has_name(const char *label, const char *name) {
  size_t n = strlen(name);
  for (const char *p = label; (p = strstr(p, name)) != NULL; p++) {
    bool left = (p == label) || is_sep(p[-1]);
    bool right = is_sep(p[n]);
    if (left && right) return true;
  }
  return false;
}

static const struct {
  const char *op;
  int args;
} OPS[] = {
    {"CREATEFRAME", 0}, {"PUSHFRAME", 0}, {"POPFRAME", 0}, {"RETURN", 0}, {"CLEARS", 0},
    {"CALL", 1},        {"LABEL", 1},     {"EXIT", 1},     {"PUSHS", 1},  {"POPS", 1},
    {"WRITE", 1},       {"DEFVAR", 1},    {"JUMP", 1},     {"MOVE", 2},
};
#define N_OPS (sizeof OPS / sizeof OPS[0])

static bool ends_with(const char *s, const char *suffix) {
  size_t a = strlen(s), b = strlen(suffix);
  return a > b && strcmp(s + a - b, suffix) == 0;
}

/* Rozdělí řádek na slova (max 8). Vrací počet. Pracuje nad kopií v buf. */
static int split_words(const char *line, char *buf, size_t bufsz, char *words[8]) {
  snprintf(buf, bufsz, "%s", line);
  int n = 0;
  char *save = NULL;
  for (char *t = strtok_r(buf, " \t", &save); t && n < 8; t = strtok_r(NULL, " \t", &save))
    words[n++] = t;
  return n;
}

/* ------------------------------------------------------------------------- */
/*  Simulátor IFJcode26 (jen instrukce pro fáze 0 a 1)                       */
/* ------------------------------------------------------------------------- */

typedef enum { V_NONE, V_INT, V_FLOAT, V_BOOL, V_STR, V_NIL } VType;
typedef struct {
  VType t;
  long long i;
  double d;
  const char *s;
  size_t n;
} Val;

typedef struct {
  char name[64];
  Val v;
  bool def, init;
} Var;
typedef struct {
  Var var[48];
  int n;
} Frame;

typedef struct {
  Frame gf;
  Frame *tf;
  Frame *lf[64];
  int nlf;
  Val st[256];
  int nst;
  int ret[256];
  int nret;
  char *out;
  size_t outn, outcap;
  void *mem[4096];
  int nmem;
  char err[300];
  int exit_code;
} Vm;

static void *vm_alloc(Vm *vm, size_t n) {
  void *p = calloc(1, n ? n : 1);
  if (!p || vm->nmem >= 4096) exit(99);
  vm->mem[vm->nmem++] = p;
  return p;
}

static void vm_fail(Vm *vm, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void vm_fail(Vm *vm, const char *fmt, ...) {
  if (vm->err[0]) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(vm->err, sizeof vm->err, fmt, ap);
  va_end(ap);
}

static void out_put(Vm *vm, const char *p, size_t n) {
  if (vm->outn + n + 1 > vm->outcap) {
    vm->outcap = (vm->outn + n + 1) * 2;
    vm->out = realloc(vm->out, vm->outcap);
    if (!vm->out) exit(99);
  }
  memcpy(vm->out + vm->outn, p, n);
  vm->outn += n;
  vm->out[vm->outn] = '\0';
}

static Var *find_var(Vm *vm, const char *ref, int ln) {
  Frame *f = NULL;
  const char *name = ref + 3;
  if (strncmp(ref, "GF@", 3) == 0)
    f = &vm->gf;
  else if (strncmp(ref, "TF@", 3) == 0)
    f = vm->tf;
  else if (strncmp(ref, "LF@", 3) == 0)
    f = vm->nlf ? vm->lf[vm->nlf - 1] : NULL;
  else {
    vm_fail(vm, "řádek %d: \"%s\" není proměnná ani konstanta", ln, ref);
    return NULL;
  }
  if (!f) {
    vm_fail(vm, "řádek %d: rámec %.2s neexistuje (chyba 55): %s", ln, ref, ref);
    return NULL;
  }
  for (int i = 0; i < f->n; i++)
    if (strcmp(f->var[i].name, name) == 0) return &f->var[i];
  return NULL; /* nenalezeno; volající rozhodne (DEFVAR ji vytvoří, ostatní je chyba 54) */
}

static Var *need_var(Vm *vm, const char *ref, int ln) {
  Var *v = find_var(vm, ref, ln);
  if (!v && !vm->err[0])
    vm_fail(vm, "řádek %d: proměnná %s neexistuje, chybí DEFVAR (chyba 54)", ln, ref);
  return v;
}

/* Přečte operand: konstanta nebo proměnná. */
static bool read_symb(Vm *vm, const char *tok, int ln, Val *out) {
  memset(out, 0, sizeof *out);
  if (strncmp(tok, "int@", 4) == 0) {
    char *end;
    out->t = V_INT;
    out->i = strtoll(tok + 4, &end, 10);
    if (end == tok + 4 || *end) {
      vm_fail(vm, "řádek %d: chybný zápis čísla \"%s\"", ln, tok);
      return false;
    }
    return true;
  }
  if (strncmp(tok, "float@", 6) == 0) {
    char *end;
    out->t = V_FLOAT;
    const char *q = tok + 6;
    if (*q == '+' || *q == '-') q++;
    /* IFJcode26 chce hexadecimální zápis (%a): 0x1.8p+1. Zápis 3.5 nebo 42 ic26int odmítne (chyba 51). */
    bool hex = (q[0] == '0' && (q[1] == 'x' || q[1] == 'X') && (strchr(q, 'p') || strchr(q, 'P')));
    out->d = strtod(tok + 6, &end);
    if (!hex || end == tok + 6 || *end) {
      vm_fail(vm, "řádek %d: desetinné číslo musí být hexadecimálně (printf %%a, např. float@0x1.8p+1), "
                  "ic26int jiný zápis odmítne: \"%s\"", ln, tok);
      return false;
    }
    return true;
  }
  if (strncmp(tok, "bool@", 5) == 0) {
    out->t = V_BOOL;
    if (strcmp(tok + 5, "true") == 0)
      out->i = 1;
    else if (strcmp(tok + 5, "false") == 0)
      out->i = 0;
    else {
      vm_fail(vm, "řádek %d: bool musí být true nebo false: \"%s\"", ln, tok);
      return false;
    }
    return true;
  }
  if (strcmp(tok, "nil@nil") == 0) {
    out->t = V_NIL;
    return true;
  }
  if (strncmp(tok, "string@", 7) == 0) {
    char *buf = vm_alloc(vm, strlen(tok) + 1);
    size_t o = 0;
    for (const char *p = tok + 7; *p; p++) {
      if (*p == '#') {
        vm_fail(vm, "řádek %d: v řetězci je holé # (ic26int by tu řádek uřízl jako komentář), "
                    "musí být \\035: \"%s\"", ln, tok);
        return false;
      }
      if (*p == '\\') {
        if (isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) &&
            isdigit((unsigned char)p[3])) {
          buf[o++] = (char)((p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0'));
          p += 3;
        } else {
          vm_fail(vm, "řádek %d: zpětné lomítko v řetězci musí mít za sebou tři číslice (\\xyz): "
                      "\"%s\"", ln, tok);
          return false;
        }
      } else {
        buf[o++] = *p;
      }
    }
    out->t = V_STR;
    out->s = buf;
    out->n = o;
    return true;
  }
  Var *v = need_var(vm, tok, ln);
  if (!v) return false;
  if (!v->init) {
    vm_fail(vm, "řádek %d: čtení neinicializované proměnné %s (chyba 56)", ln, tok);
    return false;
  }
  *out = v->v;
  return true;
}

static void write_val(Vm *vm, const Val *v) {
  char tmp[64];
  switch (v->t) {
    case V_INT:
      out_put(vm, tmp, (size_t)snprintf(tmp, sizeof tmp, "%lld", v->i));
      break;
    case V_FLOAT:
      out_put(vm, tmp, (size_t)snprintf(tmp, sizeof tmp, "%a", v->d));
      break;
    case V_BOOL:
      out_put(vm, v->i ? "true" : "false", v->i ? 4 : 5);
      break;
    case V_STR:
      out_put(vm, v->s, v->n);
      break;
    case V_NIL:
      out_put(vm, "nil", 3);
      break;
    default:
      break;
  }
}

/* Spustí program (řádky 0..n-1, řádek 0 je hlavička). Při chybě nastaví vm->err. */
static void vm_run(Vm *vm, char **lines, int n) {
  /* návěští */
  const char *lab[128];
  int labline[128], nlab = 0;
  for (int i = 1; i < n; i++) {
    char buf[1024], *w[8];
    if (split_words(lines[i], buf, sizeof buf, w) >= 2 && strcmp(w[0], "LABEL") == 0 && nlab < 128) {
      char *dup = vm_alloc(vm, strlen(w[1]) + 1);
      strcpy(dup, w[1]);
      lab[nlab] = dup;
      labline[nlab++] = i;
    }
  }

  int pc = 1;
  for (int steps = 0; pc < n; steps++) {
    if (steps > 20000) {
      vm_fail(vm, "program se zacyklil (víc než 20000 instrukcí)");
      return;
    }
    char buf[1024], *w[8];
    int nw = split_words(lines[pc], buf, sizeof buf, w);
    int ln = pc + 1;
    pc++;
    if (nw == 0) continue;
    const char *op = w[0];

    if (strcmp(op, "LABEL") == 0) {
      continue;
    } else if (strcmp(op, "CALL") == 0 || strcmp(op, "JUMP") == 0) {
      if (nw < 2) {
        vm_fail(vm, "řádek %d: %s bez návěští", ln, op);
        return;
      }
      int target = -1;
      for (int k = 0; k < nlab; k++)
        if (strcmp(lab[k], w[1]) == 0) target = labline[k];
      if (target < 0) {
        vm_fail(vm, "řádek %d: skok na neexistující návěští \"%s\" (chyba 52)", ln, w[1]);
        return;
      }
      if (strcmp(op, "CALL") == 0) {
        if (vm->nret >= 256) {
          vm_fail(vm, "řádek %d: příliš hluboké volání", ln);
          return;
        }
        vm->ret[vm->nret++] = pc;
      }
      pc = target + 1;
    } else if (strcmp(op, "RETURN") == 0) {
      if (vm->nret == 0) {
        vm_fail(vm, "řádek %d: RETURN bez volání CALL (chyba 56)", ln);
        return;
      }
      pc = vm->ret[--vm->nret];
    } else if (strcmp(op, "CREATEFRAME") == 0) {
      vm->tf = vm_alloc(vm, sizeof(Frame));
    } else if (strcmp(op, "PUSHFRAME") == 0) {
      if (!vm->tf) {
        vm_fail(vm, "řádek %d: PUSHFRAME bez CREATEFRAME (chyba 55)", ln);
        return;
      }
      if (vm->nlf >= 64) {
        vm_fail(vm, "řádek %d: příliš mnoho rámců", ln);
        return;
      }
      vm->lf[vm->nlf++] = vm->tf;
      vm->tf = NULL;
    } else if (strcmp(op, "POPFRAME") == 0) {
      if (vm->nlf == 0) {
        vm_fail(vm, "řádek %d: POPFRAME bez lokálního rámce (chyba 55)", ln);
        return;
      }
      vm->tf = vm->lf[--vm->nlf];
    } else if (strcmp(op, "DEFVAR") == 0 && nw == 2) {
      Var *v = find_var(vm, w[1], ln);
      if (vm->err[0]) return;
      if (v) {
        vm_fail(vm, "řádek %d: proměnná %s je definovaná podruhé (chyba 52)", ln, w[1]);
        return;
      }
      Frame *f = w[1][0] == 'G' ? &vm->gf : w[1][0] == 'T' ? vm->tf : vm->lf[vm->nlf - 1];
      if (f->n >= 48) {
        vm_fail(vm, "řádek %d: příliš mnoho proměnných v rámci", ln);
        return;
      }
      Var *nv = &f->var[f->n++];
      snprintf(nv->name, sizeof nv->name, "%s", w[1] + 3);
      nv->def = true;
    } else if (strcmp(op, "MOVE") == 0 && nw == 3) {
      Val x;
      if (!read_symb(vm, w[2], ln, &x)) return;
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      v->v = x;
      v->init = true;
    } else if (strcmp(op, "PUSHS") == 0 && nw == 2) {
      Val x;
      if (!read_symb(vm, w[1], ln, &x)) return;
      if (vm->nst >= 256) {
        vm_fail(vm, "řádek %d: přeplněný datový zásobník", ln);
        return;
      }
      vm->st[vm->nst++] = x;
    } else if (strcmp(op, "POPS") == 0 && nw == 2) {
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      if (vm->nst == 0) {
        vm_fail(vm, "řádek %d: POPS z prázdného zásobníku (chyba 56)", ln);
        return;
      }
      v->v = vm->st[--vm->nst];
      v->init = true;
    } else if (strcmp(op, "WRITE") == 0 && nw == 2) {
      Val x;
      if (!read_symb(vm, w[1], ln, &x)) return;
      write_val(vm, &x);
    } else if (strcmp(op, "EXIT") == 0 && nw == 2) {
      Val x;
      if (!read_symb(vm, w[1], ln, &x)) return;
      vm->exit_code = (int)x.i;
      return;
    } else {
      vm_fail(vm, "řádek %d: instrukci \"%s\" (s %d operandy) simulátor v testu neumí. Pro další fáze "
                  "ho rozšiř, nebo spusť test s ic26int", ln, op, nw - 1);
      return;
    }
  }
}

static void vm_free(Vm *vm) {
  for (int i = 0; i < vm->nmem; i++) free(vm->mem[i]);
  free(vm->out);
}

/* ------------------------------------------------------------------------- */
/*  Kontrola vygenerovaného kódu                                             */
/* ------------------------------------------------------------------------- */

static void compare_output(const char *who, const Case *c, const char *got, size_t got_n) {
  size_t exp_n = strlen(c->expect_out);
  if (got_n != exp_n || memcmp(got, c->expect_out, exp_n) != 0)
    problem("%s: program vypsal \"%s\", ale má vypsat \"%s\"", who, esc(got, got_n),
            esc(c->expect_out, exp_n));
}

static void check_output(const Case *c, const char *text, size_t len) {
  if (len == 0) {
    problem("generátor nic nevypsal");
    return;
  }

  /* rozdělení na řádky */
  char *copy = strdup(text);
  if (!copy) exit(99);
  size_t cap = 64, n = 0;
  char **lines = malloc(cap * sizeof *lines);
  if (!lines) exit(99);
  for (char *p = copy; *p;) {
    char *nl = strchr(p, '\n');
    if (n == cap) {
      cap *= 2;
      lines = realloc(lines, cap * sizeof *lines);
      if (!lines) exit(99);
    }
    lines[n++] = p;
    if (!nl) break;
    *nl = '\0';
    p = nl + 1;
  }

  if (strcmp(lines[0], ".IFJcode26") != 0) problem("první řádek není .IFJcode26, ale \"%s\"", lines[0]);
  if (text[len - 1] != '\n') problem("výstup nekončí novým řádkem (chybí \\n za poslední instrukcí)");

  /* tvar řádků a sběr LABEL / CALL / EXIT */
  int bad_lines = 0;
  int exit_line = -1, first_label = -1, entry_call = -1;
  const char *label_name[64];
  int label_line[64], n_labels = 0;
  const char *call_target[64];
  int call_line[64], n_calls = 0;

  for (size_t i = 1; i < n; i++) {
    char buf[1024], *w[8];
    int nw = split_words(lines[i], buf, sizeof buf, w);
    if (nw == 0) continue;
    int ln = (int)i + 1;

    bool known = false;
    for (size_t k = 0; k < N_OPS; k++) {
      if (strcmp(w[0], OPS[k].op) == 0) {
        known = true;
        if (nw - 1 != OPS[k].args && bad_lines++ < 3)
          problem("řádek %d: %s má mít operandů %d, ale jich má %d (chybí \\n za předchozí "
                  "instrukcí, nebo je v řetězci holá mezera, ta musí být \\032): \"%s\"", ln,
                  OPS[k].op, OPS[k].args, nw - 1, lines[i]);
      }
    }
    if (!known) {
      for (size_t k = 0; k < N_OPS; k++)
        if (strcmp(OPS[k].op, "JUMP") != 0 && strncmp(w[0], OPS[k].op, strlen(OPS[k].op)) == 0 &&
            bad_lines++ < 3)
          problem("řádek %d: instrukce %s je slepená s další (chybí \\n?): \"%s\"", ln, OPS[k].op,
                  lines[i]);
    }
    for (int t = 1; t < nw; t++)
      for (size_t k = 0; k < N_OPS; k++)
        if (strlen(OPS[k].op) >= 4 && ends_with(w[t], OPS[k].op) && bad_lines++ < 3)
          problem("řádek %d: operand \"%s\" končí názvem instrukce %s (chybí \\n?)", ln, w[t],
                  OPS[k].op);

    if (strcmp(w[0], "LABEL") == 0 && nw == 2 && n_labels < 64) {
      if (!valid_label(w[1]))
        problem("řádek %d: návěští \"%s\" má nepovolené znaky (smí _-$&%%*!? a alfanumerické, "
                "bez @)", ln, w[1]);
      label_name[n_labels] = strdup(w[1]);
      label_line[n_labels++] = ln;
      if (first_label < 0) first_label = ln;
    }
    if (strcmp(w[0], "CALL") == 0 && nw == 2 && n_calls < 64) {
      if (!valid_label(w[1]))
        problem("řádek %d: návěští \"%s\" má nepovolené znaky (smí _-$&%%*!? a alfanumerické, "
                "bez @)", ln, w[1]);
      call_target[n_calls] = strdup(w[1]);
      call_line[n_calls++] = ln;
    }
    if (strcmp(w[0], "EXIT") == 0 && exit_line < 0) {
      exit_line = ln;
      if (nw != 2 || strcmp(w[1], "int@0") != 0) problem("řádek %d: program má končit EXIT int@0", ln);
    }
  }

  /* LABEL: každá funkce právě jednou, žádné návěští dvakrát */
  const char *funcs[5];
  int nf = 0;
  funcs[nf++] = c->entry;
  for (int i = 0; c->others[i]; i++) funcs[nf++] = c->others[i];
  for (int f = 0; f < nf; f++) {
    int cnt = 0;
    for (int i = 0; i < n_labels; i++)
      if (has_name(label_name[i], funcs[f])) cnt++;
    if (cnt == 0) problem("funkce '%s' se nevygenerovala (chybí její LABEL)", funcs[f]);
    if (cnt > 1) problem("funkce '%s' má LABEL víckrát (%dx)", funcs[f], cnt);
  }
  for (int i = 0; i < n_labels; i++)
    for (int j = i + 1; j < n_labels; j++)
      if (strcmp(label_name[i], label_name[j]) == 0) {
        problem("návěští \"%s\" je definované víckrát (řádky %d a %d, interpret: chyba 52)",
                label_name[i], label_line[i], label_line[j]);
        goto labels_done;
      }
labels_done:;

  /* CALL: právě jedno, na vstupní funkci; ostatní funkce nikdo nevolá */
  int entry_calls = 0;
  for (int i = 0; i < n_calls; i++) {
    bool defined = false;
    for (int j = 0; j < n_labels; j++)
      if (strcmp(call_target[i], label_name[j]) == 0) defined = true;
    if (!defined)
      problem("řádek %d: CALL na neexistující návěští \"%s\" (interpret: chyba 52)", call_line[i],
              call_target[i]);
    if (has_name(call_target[i], c->entry)) {
      entry_calls++;
      if (entry_call < 0) entry_call = call_line[i];
    }
    for (int k = 0; c->others[k]; k++)
      if (has_name(call_target[i], c->others[k]))
        problem("řádek %d: CALL na '%s', ale tu funkci nikdo nevolá (volá se jen @main)",
                call_line[i], c->others[k]);
  }
  if (entry_calls == 0) problem("chybí CALL na vstupní funkci '%s' (ta s @main)", c->entry);
  if (entry_calls > 1) problem("vstupní funkce '%s' se volá víckrát (%dx)", c->entry, entry_calls);

  /* pořadí: CALL, EXIT, teprve pak definice funkcí */
  if (exit_line < 0) problem("chybí EXIT int@0 na konci hlavního kódu");
  if (exit_line >= 0 && entry_call >= 0 && entry_call > exit_line)
    problem("CALL (řádek %d) je až za EXIT (řádek %d)", entry_call, exit_line);
  if (exit_line >= 0 && first_label >= 0 && first_label < exit_line)
    problem("LABEL funkce (řádek %d) je před EXIT (řádek %d), program by do funkce spadl bez volání",
            first_label, exit_line);

  /* RETURN: před ním POPFRAME a před tím PUSHS nil@nil */
  for (size_t i = 1; i < n; i++) {
    char buf[1024], *w[8];
    if (split_words(lines[i], buf, sizeof buf, w) == 0 || strcmp(w[0], "RETURN") != 0) continue;
    if (i < 3 || strcmp(lines[i - 1], "POPFRAME") != 0) {
      problem("řádek %zu: RETURN není hned za POPFRAME", i + 1);
      break;
    }
    if (strcmp(lines[i - 2], "PUSHS nil@nil") != 0) {
      problem("řádek %zu: před POPFRAME chybí PUSHS nil@nil (podle plánu funkce Unit vrací nil@nil)",
              i + 1);
      break;
    }
  }

  /* očekávané zápisy v kódu (např. escape sekvence v řetězci) */
  for (int k = 0; c->must[k]; k++) {
    bool found = false;
    for (size_t i = 1; i < n && !found; i++) {
      char buf[1024], *w[8];
      int nw = split_words(lines[i], buf, sizeof buf, w);
      for (int t = 1; t < nw; t++)
        if (strcmp(w[t], c->must[k]) == 0) found = true;
    }
    if (!found) problem("v kódu chybí zápis \"%s\" (špatně přepsaný řetězec?)", c->must[k]);
  }

  /* chování: simulace programu */
  if (g_nmsg == 0) {
    Vm *vm = calloc(1, sizeof *vm);
    if (!vm) exit(99);
    vm_run(vm, lines, (int)n);
    if (vm->err[0]) {
      problem("simulace: %s", vm->err);
    } else {
      if (vm->exit_code != 0) problem("simulace: program skončil s kódem %d (čekalo se 0)", vm->exit_code);
      compare_output("simulace", c, vm->out ? vm->out : "", vm->outn);
      if (vm->nst > 1)
        problem("simulace: na konci programu zůstalo na zásobníku %d hodnot (smí jedna, návrat z "
                "main). Některý příkaz po sobě něco zanechal, třeba print nezahodil hodnotu", vm->nst);
    }
    vm_free(vm);
    free(vm);
  }

  for (int i = 0; i < n_labels; i++) free((void *)label_name[i]);
  for (int i = 0; i < n_calls; i++) free((void *)call_target[i]);
  free(lines);
  free(copy);
}

/* ------------------------------------------------------------------------- */
/*  Interpret                                                                */
/* ------------------------------------------------------------------------- */

/* Když interpret bere argumenty jinak, uprav jen příkaz tady. */
static int interp_exec(const char *interp, const char *codefile, const char *outf, const char *errf) {
  char cmd[1024];
  snprintf(cmd, sizeof cmd, "%s \"%s\" < /dev/null > \"%s\" 2> \"%s\"", interp, codefile, outf, errf);
  int st = system(cmd);
  return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* Jde interpret na tomhle počítači spustit? Zkusí malý program. */
static bool interp_usable(const char *interp) {
  char codef[] = "/tmp/ifj_gen_probe_XXXXXX";
  char outf[] = "/tmp/ifj_gen_pout_XXXXXX";
  char errf[] = "/tmp/ifj_gen_perr_XXXXXX";
  int a = mkstemp(codef), b = mkstemp(outf), d = mkstemp(errf);
  if (a < 0 || b < 0 || d < 0) return false;
  const char prog[] = ".IFJcode26\nEXIT int@0\n";
  if (write(a, prog, sizeof prog - 1) < 0) { /* ignoruj */ }
  close(a);
  close(b);
  close(d);
  bool ok = interp_exec(interp, codef, outf, errf) == 0;
  remove(codef);
  remove(outf);
  remove(errf);
  return ok;
}

static const char *find_interpreter(char *out, size_t sz, bool *explicit_env) {
  *explicit_env = false;
  const char *env = getenv("INTERP");
  if (env && *env) {
    *explicit_env = true;
    snprintf(out, sz, "%s", env);
    return interp_usable(out) ? out : NULL;
  }
  static const char *cands[] = {"./ic26int",
                                "../ic26int",
                                "../../ic26int",
                                "./ic26int-linux-x86_64",
                                "../ic26int-linux-x86_64",
                                "./ic26int-linux-aarch64",
                                "../ic26int-linux-aarch64"};
  for (size_t i = 0; i < sizeof cands / sizeof cands[0]; i++)
    if (access(cands[i], X_OK) == 0 && interp_usable(cands[i])) {
      snprintf(out, sz, "%s", cands[i]);
      return out;
    }
  if (system("command -v ic26int >/dev/null 2>&1") == 0 && interp_usable("ic26int")) {
    snprintf(out, sz, "ic26int");
    return out;
  }
  return NULL;
}

static void run_interpreter(const Case *c, const char *interp, const char *codefile) {
  char outf[] = "/tmp/ifj_gen_out_XXXXXX";
  char errf[] = "/tmp/ifj_gen_err_XXXXXX";
  int fd1 = mkstemp(outf), fd2 = mkstemp(errf);
  if (fd1 < 0 || fd2 < 0) {
    problem("nepodařilo se vytvořit dočasné soubory pro interpret");
    return;
  }
  close(fd1);
  close(fd2);

  int rc = interp_exec(interp, codefile, outf, errf);
  size_t olen = 0, elen = 0;
  char *so = read_file(outf, &olen);
  char *se = read_file(errf, &elen);
  if (rc != 0) {
    problem("ic26int skončil s kódem %d (čekalo se 0). stderr: %s", rc, se ? esc(se, elen) : "");
  } else {
    compare_output("ic26int", c, so ? so : "", olen);
  }
  free(so);
  free(se);
  remove(outf);
  remove(errf);
}

/* ------------------------------------------------------------------------- */
/*  Spuštění jednoho případu v odděleném procesu                             */
/* ------------------------------------------------------------------------- */

static void print_code(const char *text, int max_lines) {
  int shown = 0;
  const char *p = text;
  while (*p && shown < max_lines) {
    const char *nl = strchr(p, '\n');
    int len = nl ? (int)(nl - p) : (int)strlen(p);
    printf("        | %.*s%s\n", len, p, nl ? "" : "   <-- konec bez \\n");
    shown++;
    if (!nl) break;
    p = nl + 1;
  }
  if (*p) printf("        | ...\n");
}

static bool run_case(const Case *c, bool verbose, const char *interp) {
  g_nmsg = 0;
  char codef[] = "/tmp/ifj_gen_code_XXXXXX";
  int fd = mkstemp(codef);
  if (fd < 0) {
    printf("  CHYBA %-16s nepodařilo se vytvořit dočasný soubor\n", c->name);
    return false;
  }
  close(fd);

  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) {
    printf("  CHYBA %-16s fork selhal\n", c->name);
    remove(codef);
    return false;
  }
  if (pid == 0) {
    alarm(5); /* nekonečná smyčka v generátoru nesmí zablokovat test */
    if (!freopen(codef, "w", stdout)) _exit(120);
    AST_NODE *program = c->build();
    run_generator(program);
    fflush(stdout);
    _exit(0);
  }

  int st = 0;
  waitpid(pid, &st, 0);
  size_t len = 0;
  char *text = read_file(codef, &len);
  if (!text) {
    text = strdup("");
    len = 0;
  }

  if (WIFSIGNALED(st)) {
    int sig = WTERMSIG(st);
    if (sig == SIGALRM)
      problem("generátor běží déle než 5 s (nekonečná smyčka?)");
    else
      problem("generátor spadl na signál %d (%s)", sig, strsignal(sig));
  } else if (WIFEXITED(st) && WEXITSTATUS(st) != 0) {
    problem("generátor skončil s kódem %d (čekalo se 0)", WEXITSTATUS(st));
  } else {
    check_output(c, text, len);
    if (g_nmsg == 0 && interp) run_interpreter(c, interp, codef);
  }

  bool ok = (g_nmsg == 0);
  if (ok) {
    printf("  OK    %-16s %s\n", c->name, c->desc);
    if (verbose) print_code(text, 60);
  } else {
    printf("  CHYBA %-16s %s\n", c->name, c->desc);
    for (int i = 0; i < g_nmsg; i++) printf("        - %s\n", g_msg[i]);
    printf("        vygenerovaný kód:\n");
    if (len == 0)
      printf("        | (nic)\n");
    else
      print_code(text, 30);
  }
  free(text);
  remove(codef);
  return ok;
}

static bool selected(const Case *c, const char *only) {
  if (!only) return true;
  if (strcmp(only, c->name) == 0) return true;
  if (only[0] == 'f' && only[1] >= '0' && only[1] <= '9' && only[2] == '\0')
    return c->phase == only[1] - '0';
  return false;
}

int main(int argc, char **argv) {
  bool verbose = false;
  const char *only = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0)
      verbose = true;
    else
      only = argv[i];
  }

  char interp_buf[512];
  bool explicit_env = false;
  const char *interp = find_interpreter(interp_buf, sizeof interp_buf, &explicit_env);

  int total_ok = 0, total_bad = 0, ran = 0;
  int phase_ok[2] = {0, 0}, phase_bad[2] = {0, 0};
  for (int phase = 0; phase <= 1; phase++) {
    bool header = false;
    for (size_t i = 0; i < N_CASES; i++) {
      if (CASES[i].phase != phase || !selected(&CASES[i], only)) continue;
      if (!header) {
        printf("== Fáze %d: %s\n", phase, phase == 0 ? "kostra programu a funkcí" : "konstanty a print");
        header = true;
      }
      ran++;
      if (run_case(&CASES[i], verbose, interp)) {
        phase_ok[phase]++;
        total_ok++;
      } else {
        phase_bad[phase]++;
        total_bad++;
      }
    }
  }
  if (ran == 0) {
    printf("neznámý případ \"%s\". Dostupné: f0, f1 a", only);
    for (size_t i = 0; i < N_CASES; i++) printf(" %s", CASES[i].name);
    printf("\n");
    return 2;
  }

  printf("== Výsledek: OK %d, chyby %d", total_ok, total_bad);
  for (int phase = 0; phase <= 1; phase++)
    if (phase_ok[phase] + phase_bad[phase] > 0)
      printf("   (fáze %d: %d/%d)", phase, phase_ok[phase], phase_ok[phase] + phase_bad[phase]);
  printf("\n");
  if (interp)
    printf("Výstup ověřil i interpret: %s\n", interp);
  else if (explicit_env)
    printf("POZOR: interpret z INTERP jde na tomhle počítači spustit jen s chybou, použil se jen "
           "simulátor v testu.\n");
  else
    printf("Interpret ic26int jsem nenašel nebo nejde spustit (třeba Linux program na Macu). Výstup "
           "ověřil jen simulátor v testu.\n");
  return total_bad == 0 ? 0 : 1;
}
