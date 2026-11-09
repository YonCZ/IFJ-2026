/*
 * Projekt IFJ26 - test lexikální analýzy (scanner)
 * Autoři: <jména a loginy členů týmu, kteří soubor upravovali>
 *
 * Kód vygenerovala AI: Claude Sonnet 5.5 (Anthropic).
 * Prompt: <vlož sem přesné znění svého promptu>
 *
 * Použití:
 *   ./test_scanner < vstup.scala   vypíše tokeny (stejně jako loni)
 *   ./test_scanner --test          spustí samokontrolní testy
 *   ./test_scanner --test --base   totéž, ale bez testů rozšíření
 *   ./test_scanner --test -v       nechá scanneru stderr (jeho chybová hlášení)
 *
 * Překlad (doplň své moduly):
 *   gcc -Wall -Wextra -std=c11 -o test_scanner test_scanner.c scanner.c ...
 *
 * Každý testovací případ se spouští v samostatném procesu (fork), takže
 * scanner nemusí mít žádnou funkci pro reset a pád/exit()/nekonečná smyčka
 * jednoho případu neovlivní ostatní.
 */
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "scanner.h"

/* ========================================================================
 * ADAPTÉR - jediné místo, které je potřeba přizpůsobit tvému scanneru.
 * Výchozí hodnoty vychází z loňského vzoru (token_t, get_next_token, ...).
 * ====================================================================== */
typedef token_t tok_t;

#define SCANNER_INIT() ((void)0) /* např. scanner_init(stdin) */
#define NEXT_TOKEN() get_next_token()
#define FREE_TOKEN(t) free_token(t)
#define TOK_TYPE(t) ((int)(t)->type)

/* Exit kód, když NEXT_TOKEN() vrátí NULL. Loni znamenalo NULL chybu alokace
 * (99). Pokud tvůj scanner hlásí lexikální chybu vrácením NULL, dej 1.
 * Hlásí-li ji sám přes exit(1), nech být. */
#define NULL_TOKEN_EXIT 99

/* Textová podoba hodnoty tokenu. Vrací 0, pokud token hodnotu nenese. */
static int token_value_raw(const tok_t *t, char *raw, size_t size) {
  switch (TOK_TYPE(t)) {
  case INT_NUM_TOKEN:
    snprintf(raw, size, "%lld", (long long)t->data.num_int);
    return 1;
  case DOUBLE_NUM_TOKEN:
    snprintf(raw, size, "%.15g", t->data.num_float);
    return 1;
  case STRING_WOR_TOKEN:
  case IDENTIFIER_TOKEN:
    snprintf(raw, size, "%s", t->data.string ? t->data.string : "");
    return 1;
  default:
    return 0;
  }
}
/* ====================================================================== */

/* Jména tokenů pro výpis: N(PLUS) -> "PLUS" (bez přípony _TOKEN). */
#define N(x)                                                                   \
  case x##_TOKEN:                                                              \
    return #x

static const char *token_name(int type) {
  switch (type) {
    N(PLUS); N(MINUS); N(MUL); N(DIV); N(ASSIGN); N(EQ); N(NEQ);
    N(LESS_EQ); N(GREAT_EQ); N(LESS); N(GREAT);
    N(RPAR); N(LPAR); N(DOT); N(COMMA); N(AT); N(COLON); N(SEMICOLON);
    N(DEF); N(IF); N(WHILE); N(DO); N(ELSE); N(THEN); N(RETURN); N(IMPORT);
    N(VAR); N(VAL);
    N(INT); N(STRING); N(DOUBLE); N(UNIT);
    N(INT_NUM); N(DOUBLE_NUM); N(STRING_WOR); N(UNIT_LIT); N(IDENTIFIER);
    N(SLASH_COMM); N(SLASH_COMM_END);
    N(INDENT); N(DETENT); N(EOF); N(EOL);
    N(FOR); N(TO); N(UNTIL); N(BY); N(AND); N(OR); N(NOT); N(TRUE); N(FALSE);
    N(CASE); N(CLASS); N(MATCH); N(TYPE); N(EXTENCION); N(LIST);
    N(PLUSPLUS); N(LBRACKET); N(RBRACKET); N(ARROW_FAT); N(ARROW_LEFT);
    N(ARROW_RIGHT); N(ARROW); N(UNDERSCORE); N(PIPE);
    N(STRING__INTERP); N(STRING__INTERP_PART);
  default:
    return "UNKNOWN";
  }
}

/* Zkopíruje s do out; neviditelné znaky a zpětné lomítko zapíše jako escape,
 * aby výpis vždy zůstal na jednom řádku. */
static void escape_into(const char *s, char *out, size_t size) {
  size_t o = 0;
  for (; *s && o + 5 < size; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch == '\n') {
      out[o++] = '\\';
      out[o++] = 'n';
    } else if (ch == '\r') {
      out[o++] = '\\';
      out[o++] = 'r';
    } else if (ch == '\t') {
      out[o++] = '\\';
      out[o++] = 't';
    } else if (ch == '\\') {
      out[o++] = '\\';
      out[o++] = '\\';
    } else if (ch < 32 || ch > 126) {
      o += (size_t)snprintf(out + o, 5, "\\x%02X", ch);
    } else {
      out[o++] = (char)ch;
    }
  }
  out[o] = '\0';
}

/* Hodnota tokenu v "escapované" podobě; vrací 0, pokud token hodnotu nenese. */
static int token_value(const tok_t *t, char *out, size_t size) {
  char raw[1024];
  if (!token_value_raw(t, raw, sizeof raw))
    return 0;
  escape_into(raw, out, size);
  return 1;
}

/* ========================================================================
 * Režim 1: výpis tokenů ze stdin (jako loňský test)
 * ====================================================================== */
static int dump_mode(void) {
  SCANNER_INIT();
  int count = 0;
  for (;;) {
    tok_t *t = NEXT_TOKEN();
    if (!t) {
      fprintf(stderr, "\nNEXT_TOKEN() vratil NULL (interni chyba?)\n");
      return NULL_TOKEN_EXIT;
    }
    count++;
    char val[1024];
    printf("Token #%-3d: %-20s", count, token_name(TOK_TYPE(t)));
    if (token_value(t, val, sizeof val))
      printf(" | Value: \"%s\"", val);
    printf("\n");
    int is_eof = TOK_TYPE(t) == EOF_TOKEN;
    FREE_TOKEN(t);
    if (is_eof)
      break;
  }
  printf("\nOK: nacteno %d tokenu\n", count);
  return 0;
}

/* ========================================================================
 * Režim 2: samokontrolní testy
 * ====================================================================== */
enum { K_TOKENS, K_OK, K_ERR, K_INFO };
/* K_TOKENS - proběhne bez chyby a tokeny odpovídají očekávání
 * K_OK     - proběhne bez chyby (tokeny se nekontrolují)
 * K_ERR    - scanner musí skončit s kódem 1 (lexikální chyba)
 * K_INFO   - jen vypíše tokeny; pro nejednoznačné případy ze specifikace */
enum { F_NONE = 0, F_STRUCT = 1, F_EXT = 2 };
/* F_STRUCT - porovnávají se i INDENT/DETENT (jinak se ignorují)
 * F_EXT    - test patří k rozšíření (přeskočí se s --base)
 * EOL a EOF se při porovnávání ignorují vždy. */

typedef struct {
  const char *name;
  int kind;
  int flags;
  const char *src;
  const char *const *expect;
} test_case_t;

/* Zkratky pro očekávané tokeny. Řetězec bez ':' porovnává jen typ tokenu,
 * s ':' porovnává i hodnotu. */
#define E_ID(s) "IDENTIFIER:" s
#define E_NUM(s) "INT_NUM:" s
#define E_DBL(s) "DOUBLE_NUM:" s
#define E_STR(s) "STRING_WOR:" s
#define EXPECT(...) ((const char *const[]){__VA_ARGS__, NULL})
#define NOTHING ((const char *const[]){NULL})

#define LEXERR(name, src) {name, K_ERR, F_NONE, src, NOTHING}

static const test_case_t cases[] = {
    /* ---------------------------------------------------------------- */
    /* Základní lexémy                                                   */
    /* ---------------------------------------------------------------- */
    {"prazdny vstup", K_TOKENS, F_NONE, "", NOTHING},
    {"jen komentare a prazdne radky", K_TOKENS, F_NONE,
     "// komentar\n\n/* blok */\n   \n", NOTHING},
    {"vstup bez koncoveho LF", K_TOKENS, F_NONE, "x",
     EXPECT(E_ID("x"))},
    {"prolog", K_TOKENS, F_NONE, "import ifj26.*\n",
     EXPECT("IMPORT", E_ID("ifj26"), "DOT", "MUL")},
    {"klicova slova", K_TOKENS, F_NONE,
     "def if while do else then return import var val Int String Double Unit\n",
     EXPECT("DEF", "IF", "WHILE", "DO", "ELSE", "THEN", "RETURN", "IMPORT",
            "VAR", "VAL", "INT", "STRING", "DOUBLE", "UNIT")},
    {"klicova slova jsou case-sensitive", K_TOKENS, F_NONE,
     "int string double unit Def IF Var Val While\n",
     EXPECT(E_ID("int"), E_ID("string"), E_ID("double"), E_ID("unit"),
            E_ID("Def"), E_ID("IF"), E_ID("Var"), E_ID("Val"),
            E_ID("While"))},
    {"identifikatory zacinajici klicovym slovem", K_TOKENS, F_NONE,
     "iffy define values Integer if_ _if elsewhere main\n",
     EXPECT(E_ID("iffy"), E_ID("define"), E_ID("values"), E_ID("Integer"),
            E_ID("if_"), E_ID("_if"), E_ID("elsewhere"), E_ID("main"))},
    {"identifikatory", K_TOKENS, F_NONE,
     "x _x x1 __ a_b_c X9 _1 CamelCase\n",
     EXPECT(E_ID("x"), E_ID("_x"), E_ID("x1"), E_ID("__"), E_ID("a_b_c"),
            E_ID("X9"), E_ID("_1"), E_ID("CamelCase"))},
    {"operatory", K_TOKENS, F_NONE, "+ - * / = == != < <= > >=\n",
     EXPECT("PLUS", "MINUS", "MUL", "DIV", "ASSIGN", "EQ", "NEQ", "LESS",
            "LESS_EQ", "GREAT", "GREAT_EQ")},
    {"operatory bez mezer (nejdelsi shoda)", K_TOKENS, F_NONE,
     "a<=b>=c==d!=e<f>g\n",
     EXPECT(E_ID("a"), "LESS_EQ", E_ID("b"), "GREAT_EQ", E_ID("c"), "EQ",
            E_ID("d"), "NEQ", E_ID("e"), "LESS", E_ID("f"), "GREAT",
            E_ID("g"))},
    {"mensi nez zaporne cislo", K_TOKENS, F_NONE, "a < -5\n",
     EXPECT(E_ID("a"), "LESS", "MINUS", E_NUM("5"))},
    {"oddelovace", K_TOKENS, F_NONE, "( ) , . : ; @\n",
     EXPECT("LPAR", "RPAR", "COMMA", "DOT", "COLON", "SEMICOLON", "AT")},
    {"@main", K_TOKENS, F_NONE, "@main def run(): Unit =\n",
     EXPECT("AT", E_ID("main"), "DEF", E_ID("run"), "LPAR", "RPAR", "COLON",
            "UNIT", "ASSIGN")},
    {"zaporne cislo je MINUS + cislo", K_TOKENS, F_NONE, "x = -5\n",
     EXPECT(E_ID("x"), "ASSIGN", "MINUS", E_NUM("5"))},
    {"minus bez mezer", K_TOKENS, F_NONE, "a-b\n",
     EXPECT(E_ID("a"), "MINUS", E_ID("b"))},
    {"deleni (ne komentar)", K_TOKENS, F_NONE, "a / b /c\n",
     EXPECT(E_ID("a"), "DIV", E_ID("b"), "DIV", E_ID("c"))},
    {"strednik", K_TOKENS, F_NONE, "a; b\n",
     EXPECT(E_ID("a"), "SEMICOLON", E_ID("b"))},
    {"tabulator mezi tokeny je bily znak", K_TOKENS, F_NONE, "a\tb\n",
     EXPECT(E_ID("a"), E_ID("b"))},

    /* ---------------------------------------------------------------- */
    /* Číselné literály                                                  */
    /* ---------------------------------------------------------------- */
    {"cela cisla", K_TOKENS, F_NONE, "0 7 42 1_000_000\n",
     EXPECT(E_NUM("0"), E_NUM("7"), E_NUM("42"), E_NUM("1000000"))},
    {"desetinna cisla", K_TOKENS, F_NONE,
     "3.14 .5 1e10 1E+5 2.5e-3 1.5d 1.5D 1_0.2_5\n",
     EXPECT(E_DBL("3.14"), E_DBL("0.5"), E_DBL("10000000000"),
            E_DBL("100000"), E_DBL("0.0025"), E_DBL("1.5"), E_DBL("1.5"),
            E_DBL("10.25"))},
    LEXERR("cele cislo s nulou na zacatku", "x = 007\n"),
    LEXERR("exponent bez cislice (1.5e)", "x = 1.5e\n"),
    LEXERR("exponent bez cislice (1e+)", "x = 1e+\n"),

    /* ---------------------------------------------------------------- */
    /* Řetězcové literály                                                */
    /* ---------------------------------------------------------------- */
    {"retezce", K_TOKENS, F_NONE,
     "\"hello\" \"\" \"a b\" \"x // y\" \"/* z */\"\n",
     EXPECT(E_STR("hello"), E_STR(""), E_STR("a b"), E_STR("x // y"),
            E_STR("/* z */"))},
    /* zdroj: "\\ \" \n \r \t"  (hodnota se nekontroluje - zalezi na tom,
     * jestli scanner escape sekvence zpracovava hned) */
    {"escape sekvence", K_TOKENS, F_NONE,
     "\"\\\\ \\\" \\n \\r \\t\"\n", EXPECT("STRING_WOR")},
    {"viceradkovy retezec", K_TOKENS, F_NONE,
     "val x = \"\"\"A\nhoj s\\nvete\n\"\"\"\n",
     EXPECT("VAL", E_ID("x"), "ASSIGN", "STRING_WOR")},
    {"viceradkovy retezec s uvozovkami a // uvnitr", K_TOKENS, F_NONE,
     "\"\"\"say \"hi\" // not comment\n\"\"\" x\n",
     EXPECT("STRING_WOR", E_ID("x"))},
    LEXERR("neplatna escape sekvence", "x = \"\\x\"\n"),
    LEXERR("neukonceny retezec (EOF)", "x = \"abc"),
    LEXERR("konec radku uvnitr retezce", "x = \"ab\ncd\"\n"),
    LEXERR("neukonceny viceradkovy retezec", "x = \"\"\"abc\n"),

    /* ---------------------------------------------------------------- */
    /* Komentáře                                                         */
    /* ---------------------------------------------------------------- */
    {"radkovy komentar", K_TOKENS, F_NONE, "a // b c\nd\n",
     EXPECT(E_ID("a"), E_ID("d"))},
    {"radkovy komentar bez LF na konci", K_TOKENS, F_NONE, "a // b",
     EXPECT(E_ID("a"))},
    {"blokovy komentar", K_TOKENS, F_NONE, "a /* x */ b /**/ c\n",
     EXPECT(E_ID("a"), E_ID("b"), E_ID("c"))},
    {"vnorene blokove komentare", K_TOKENS, F_NONE,
     "a /* x /* y */ z */ b\n", EXPECT(E_ID("a"), E_ID("b"))},
    {"blokovy komentar pres vice radku", K_TOKENS, F_NONE,
     "a /* x\ny */\nb\n", EXPECT(E_ID("a"), E_ID("b"))},
    LEXERR("neukonceny blokovy komentar", "a /* x\n"),
    LEXERR("neukonceny vnoreny blokovy komentar", "a /* x /* y */\n"),
    /* spec: za viceradkovym komentarem nesmi byt dalsi tokeny - je to ale
     * lex. (1) nebo synt. (2) chyba? Overit na foru, tady jen vypis. */
    {"INFO: token za viceradkovym komentarem na stejnem radku", K_INFO,
     F_NONE, "a /* x\n */ b\n", NOTHING},

    /* ---------------------------------------------------------------- */
    /* Neznámé znaky                                                     */
    /* ---------------------------------------------------------------- */
    LEXERR("neznamy znak &", "x = a & b\n"),
    LEXERR("neznamy znak #", "x = # 1\n"),
    LEXERR("neznamy znak {", "def f() = {\n"),
    LEXERR("neznamy znak %", "x = a % b\n"),
    LEXERR("neznamy znak $ mimo retezec", "x = $a\n"),
    LEXERR("neznamy znak ?", "x = a ? b\n"),
    LEXERR("identifikator v obracenych apostrofech", "x = `a`\n"),
    LEXERR("apostrof", "x = 'a'\n"),
    LEXERR("znak mimo ASCII", "x = \xC3\xA9\n"),

    /* ---------------------------------------------------------------- */
    /* Odsazení (INDENT / DETENT)                                        */
    /* ---------------------------------------------------------------- */
    {"odsazeni - jedna uroven", K_TOKENS, F_STRUCT,
     "def f(): Int =\n  1\nf\n",
     EXPECT("DEF", E_ID("f"), "LPAR", "RPAR", "COLON", "INT", "ASSIGN",
            "INDENT", E_NUM("1"), "DETENT", E_ID("f"))},
    {"odsazeni - vicenasobny dedent", K_TOKENS, F_STRUCT,
     "a\n  b\n    c\nd\n",
     EXPECT(E_ID("a"), "INDENT", E_ID("b"), "INDENT", E_ID("c"), "DETENT",
            "DETENT", E_ID("d"))},
    {"odsazeni - prazdne radky a komentare nic nemeni", K_TOKENS, F_STRUCT,
     "a\n  b\n\n      \n// c\n      /* d */\n  c\nd\n",
     EXPECT(E_ID("a"), "INDENT", E_ID("b"), E_ID("c"), "DETENT", E_ID("d"))},
    {"odsazeni - uvnitr zavorek se nic negeneruje", K_TOKENS, F_STRUCT,
     "f(a,\n    b)\nc\n",
     EXPECT(E_ID("f"), "LPAR", E_ID("a"), "COMMA", E_ID("b"), "RPAR",
            E_ID("c"))},
    {"odsazeni - if / else", K_TOKENS, F_STRUCT,
     "if x < 5 then\n  print(\"a\")\nelse\n  ()\nx\n",
     EXPECT("IF", E_ID("x"), "LESS", E_NUM("5"), "THEN", "INDENT",
            E_ID("print"), "LPAR", E_STR("a"), "RPAR", "DETENT", "ELSE",
            "INDENT", "LPAR", "RPAR", "DETENT", E_ID("x"))},
    LEXERR("tabulator na zacatku radku", "a\n\tb\n"),
    LEXERR("dedent na neexistujici uroven", "a\n    b\n  c\n"),

    /* ---------------------------------------------------------------- */
    /* Celý program (příklad ze specifikace)                             */
    /* ---------------------------------------------------------------- */
    {"program: factorial ze specifikace", K_OK, F_NONE,
     "import ifj26.*\n"
     "\n"
     "def factorial(n: Int): Int =\n"
     "  var result = 1\n"
     "  var i: Int = n // uvedeni typu je nepovinne\n"
     "  while i > 1 do\n"
     "    result = result * i\n"
     "    i = i - 1\n"
     "  result // posledni vyraz v tele funkce urci navratovou hodnotu\n"
     "\n"
     "@main def runFoo(): Unit =\n"
     "  val n = readInt()\n"
     "  val result = factorial(n)\n"
     "  print(result)\n"
     "  print(\"\\n\")\n",
     NOTHING},

    /* ---------------------------------------------------------------- */
    /* Rozšíření                                                         */
    /* ---------------------------------------------------------------- */
    {"rozsireni: klicova slova", K_TOKENS, F_EXT,
     "for case class match type extension List\n",
     EXPECT("FOR", "CASE", "CLASS", "MATCH", "TYPE", "EXTENCION", "LIST")},
    {"rozsireni: to until by true false", K_TOKENS, F_EXT,
     "to until by true false\n",
     EXPECT("TO", "UNTIL", "BY", "TRUE", "FALSE")},
    {"rozsireni: operatory a symboly", K_TOKENS, F_EXT,
     "&& || ! ++ [ ] => <- | _\n",
     EXPECT("AND", "OR", "NOT", "PLUSPLUS", "LBRACKET", "RBRACKET",
            "ARROW_FAT", "ARROW_LEFT", "PIPE", "UNDERSCORE")},
    {"rozsireni: operatory bez mezer", K_TOKENS, F_EXT,
     "a=>b a<-b a!=b !c d++e f||g h&&i\n",
     EXPECT(E_ID("a"), "ARROW_FAT", E_ID("b"), E_ID("a"), "ARROW_LEFT",
            E_ID("b"), E_ID("a"), "NEQ", E_ID("b"), "NOT", E_ID("c"),
            E_ID("d"), "PLUSPLUS", E_ID("e"), E_ID("f"), "OR", E_ID("g"),
            E_ID("h"), "AND", E_ID("i"))},
    {"rozsireni: List[Int]", K_TOKENS, F_EXT, "List[Int]\n",
     EXPECT("LIST", "LBRACKET", "INT", "RBRACKET")},
    {"rozsireni: sjednoceni typu", K_TOKENS, F_EXT, "Int | String\n",
     EXPECT("INT", "PIPE", "STRING")},
    {"rozsireni: cyklus for", K_TOKENS, F_EXT, "for i <- 1 to 10 by 2 do\n",
     EXPECT("FOR", E_ID("i"), "ARROW_LEFT", E_NUM("1"), "TO", E_NUM("10"),
            "BY", E_NUM("2"), "DO")},
    /* "5." neni desetinny literal (desetinna cast je povinna), takze
     * "5.toString" ma dat INT DOT IDENT - chce to 2 znaky dopredu. */
    {"rozsireni: tecka za celym cislem (VALMEM)", K_TOKENS, F_EXT,
     "5.toString x.floor.toInt\n",
     EXPECT(E_NUM("5"), "DOT", E_ID("toString"), E_ID("x"), "DOT",
            E_ID("floor"), "DOT", E_ID("toInt"))},
    {"INFO: interpolovany retezec", K_INFO, F_EXT,
     "s\"$name${n + 1} costs $$0\"\n", NOTHING},
    {"INFO: interpolovany viceradkovy retezec", K_INFO, F_EXT,
     "s\"\"\"He said \"hello\", $name.\nSecond line.\"\"\"\n", NOTHING},

    /* ---------------------------------------------------------------- */
    /* Nejednoznačné případy - jen výpis, ať vidíš, co scanner dělá      */
    /* ---------------------------------------------------------------- */
    {"INFO: CRLF konce radku", K_INFO, F_NONE, "a\r\nb\r\n", NOTHING},
    {"INFO: 1_ (podtrzitko na konci cisla)", K_INFO, F_NONE, "x = 1_\n",
     NOTHING},
    {"INFO: 3. (tecka bez desetinne casti)", K_INFO, F_NONE, "x = 3.\n",
     NOTHING},
    {"INFO: 1d (pripona d bez tecky a exponentu)", K_INFO, F_NONE,
     "x = 1d\n", NOTHING},
};

#define MAX_LINES 4096
#define MAX_TOKENS 100000
#define MAX_OUT (1u << 20)

/* Tělo potomka: nasměruje stdin na zdrojový text a vypisuje tokeny do rourky
 * (jeden řádek na token: "JMENO" nebo "JMENO:hodnota"). */
static void child_main(const char *src, int pipe_wr, int verbose) {
  FILE *tmp = tmpfile();
  if (!tmp)
    _exit(98);
  if (fputs(src, tmp) == EOF || fflush(tmp) != 0)
    _exit(98);
  if (lseek(fileno(tmp), 0, SEEK_SET) < 0 || dup2(fileno(tmp), STDIN_FILENO) < 0)
    _exit(98);

  int devnull = open("/dev/null", O_WRONLY);
  if (devnull >= 0) {
    dup2(devnull, STDOUT_FILENO);
    if (!verbose)
      dup2(devnull, STDERR_FILENO);
  }

  FILE *out = fdopen(pipe_wr, "w");
  if (!out)
    _exit(98);
  setvbuf(out, NULL, _IOLBF, 0);

  alarm(3); /* ochrana proti nekonečné smyčce ve scanneru */
  SCANNER_INIT();
  for (int i = 0; i < MAX_TOKENS; i++) {
    tok_t *t = NEXT_TOKEN();
    if (!t) {
      fflush(out);
      _exit(NULL_TOKEN_EXIT);
    }
    int type = TOK_TYPE(t);
    char val[1024];
    if (token_value(t, val, sizeof val))
      fprintf(out, "%s:%s\n", token_name(type), val);
    else
      fprintf(out, "%s\n", token_name(type));
    FREE_TOKEN(t);
    if (type == EOF_TOKEN) {
      fflush(out);
      _exit(0);
    }
  }
  fflush(out);
  _exit(97); /* scanner nikdy nevrátil EOF */
}

typedef struct {
  int exited; /* 1 = skončil normálně (exit), 0 = zabit signálem */
  int code;   /* exit kód */
  int sig;    /* číslo signálu */
  char *out;  /* výstup potomka (řádky s tokeny) */
} run_result_t;

static run_result_t run_case(const char *src, int verbose) {
  run_result_t r = {0, 0, 0, NULL};
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
    child_main(src, fd[1], verbose);
    _exit(0);
  }
  close(fd[1]);

  size_t cap = 4096, len = 0;
  char *buf = malloc(cap);
  if (!buf)
    exit(2);
  for (;;) {
    if (cap - len < 1024) {
      cap *= 2;
      char *nb = realloc(buf, cap);
      if (!nb)
        exit(2);
      buf = nb;
    }
    ssize_t n = read(fd[0], buf + len, cap - len - 1);
    if (n <= 0)
      break;
    len += (size_t)n;
    if (len > MAX_OUT) {
      kill(pid, SIGKILL);
      break;
    }
  }
  buf[len] = '\0';
  close(fd[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status)) {
    r.exited = 1;
    r.code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    r.sig = WTERMSIG(status);
  }
  r.out = buf;
  return r;
}

static void describe(const run_result_t *r, char *buf, size_t size) {
  if (r->exited)
    snprintf(buf, size, "exit %d", r->code);
  else if (r->sig == SIGALRM)
    snprintf(buf, size, "TIMEOUT - nekonecna smycka?");
  else
    snprintf(buf, size, "pad, signal %d", r->sig);
}

static size_t split_lines(char *buf, const char **lines, size_t max) {
  size_t n = 0;
  char *p = buf;
  while (*p && n < max) {
    lines[n++] = p;
    char *nl = strchr(p, '\n');
    if (!nl)
      break;
    *nl = '\0';
    p = nl + 1;
  }
  return n;
}

static int is_ignored(const char *line, int keep_indent) {
  if (!strcmp(line, "EOL") || !strcmp(line, "EOF"))
    return 1;
  if (!keep_indent && (!strcmp(line, "INDENT") || !strcmp(line, "DETENT")))
    return 1;
  return 0;
}

static int line_matches(const char *want, const char *got) {
  if (strchr(want, ':'))
    return strcmp(want, got) == 0;
  size_t n = strcspn(got, ":");
  return strlen(want) == n && strncmp(want, got, n) == 0;
}

static void print_list(const char *label, const char *const *items, size_t n) {
  printf("         %s", label);
  for (size_t i = 0; i < n; i++)
    printf("%s%s", i ? " | " : "", items[i]);
  printf("\n");
}

static int run_tests(int base_only, int verbose) {
  const size_t total = sizeof cases / sizeof cases[0];
  int pass = 0, fail = 0, info = 0, skipped = 0;

  for (size_t i = 0; i < total; i++) {
    const test_case_t *c = &cases[i];
    if (base_only && (c->flags & F_EXT)) {
      skipped++;
      continue;
    }

    run_result_t r = run_case(c->src, verbose);
    const char *lines[MAX_LINES];
    size_t nlines = split_lines(r.out, lines, MAX_LINES);
    char status[64];
    describe(&r, status, sizeof status);

    int ok = 0;
    const char *got[MAX_LINES];
    size_t ngot = 0;
    size_t nexp = 0;

    switch (c->kind) {
    case K_INFO:
      printf("[INFO] %s (%s)\n", c->name, status);
      print_list("tokeny: ", lines, nlines);
      info++;
      free(r.out);
      continue;
    case K_ERR:
      ok = r.exited && r.code == 1;
      break;
    case K_OK:
      ok = r.exited && r.code == 0;
      break;
    default: /* K_TOKENS */
      for (size_t j = 0; j < nlines; j++)
        if (!is_ignored(lines[j], c->flags & F_STRUCT))
          got[ngot++] = lines[j];
      while (c->expect[nexp])
        nexp++;
      ok = r.exited && r.code == 0 && ngot == nexp;
      for (size_t k = 0; ok && k < nexp; k++)
        ok = line_matches(c->expect[k], got[k]);
      break;
    }

    if (ok) {
      printf("[ OK ] %s\n", c->name);
      pass++;
    } else {
      fail++;
      printf("[FAIL] %s (%s)\n", c->name, status);
      if (c->kind == K_ERR) {
        printf("         ocekavana lexikalni chyba (exit 1)\n");
        print_list("tokeny: ", lines, nlines);
      } else if (c->kind == K_OK) {
        printf("         ocekavan exit 0\n");
        print_list("tokeny: ", lines, nlines);
      } else {
        print_list("ocekavano: ", c->expect, nexp);
        print_list("ziskano:   ", got, ngot);
      }
    }
    free(r.out);
  }

  printf("\nVysledek: %d OK, %d FAIL, %d info, %d preskoceno (celkem %zu)\n",
         pass, fail, info, skipped, total);
  return fail ? 1 : 0;
}

int main(int argc, char *argv[]) {
  int do_test = 0, base = 0, verbose = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--test"))
      do_test = 1;
    else if (!strcmp(argv[i], "--base"))
      base = 1;
    else if (!strcmp(argv[i], "-v"))
      verbose = 1;
    else {
      fprintf(stderr, "Pouziti: %s [--test [--base] [-v]]  (bez argumentu: vypis tokenu ze stdin)\n",
              argv[0]);
      return 2;
    }
  }
  return do_test ? run_tests(base, verbose) : dump_mode();
}
