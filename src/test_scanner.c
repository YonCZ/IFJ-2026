/*
 * Projekt IFJ26 - test lexikální analýzy (scanner)
 * Autoři: <jména a loginy členů týmu, kteří soubor upravovali>
 *
 * Kód vygenerovala AI: Claude Sonnet 5.5 (Anthropic).
 * Prompt 1: moje tokeny vypdadají takto a chci udělat test na lexikalní analýzu tedy scanner. Dám ti sem příklad pro z minulého roku a ty mi udělej na tento rok takovýto test. Tady jsou moje tokeny
 * Prompt 2: tady máš celý projekt a chci abys upravil testy tak aby správně otestovaly scanner
 *
 * Test je napsaný proti rozhraní scanner.h / token.h:
 *   Token get_token(FILE *f);
 *   Token.type     typ tokenu (enum Tokens, *_TOKEN)
 *   Token.atribute text tokenu (jen IDENTIFIER, INT_NUM, DOUBLE_NUM, STRING_WORD)
 *   Token.line     číslo řádku
 *
 * Co se u tokenů kontroluje:
 *   - IDENTIFIER   název
 *   - INT_NUM      číslice bez podtržítek ("1_000" -> "1000")
 *   - DOUBLE_NUM   text literálu bez podtržítek a bez přípony d ("1.5d" -> "1.5",
 *                  ".5" zůstává ".5", exponent se nepřepočítává)
 *   - STRING_WORD  obsah po zpracování escape sekvencí (bez uvozovek)
 *   V hlášení se hodnota vypisuje "escapovaná": LF jako \n, TAB jako \t, \ jako \\.
 *
 * Použití:
 *   ./test_scanner < vstup.scala   vypíše tokeny (včetně čísla řádku)
 *   ./test_scanner --test          spustí samokontrolní testy
 *   ./test_scanner --test --base   totéž, ale bez testů rozšíření
 *   ./test_scanner --test -v       nechá scanneru stderr (jeho chybová hlášení)
 *
 * Překlad (doplň své moduly a -I podle struktury repozitáře):
 *   gcc -g -Wall -Wextra -std=c11 -o test_scanner test_scanner.c scanner.c \
 *       error.c dynamic_string.c
 *
 * Každý testovací případ se spouští v samostatném procesu (fork), takže
 * scanner nemusí mít žádnou funkci pro reset (má static proměnné) a pád,
 * exit() nebo nekonečná smyčka jednoho případu neovlivní ostatní.
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

#include "error.h"
#include "scanner.h"

/* ========================================================================
 * ADAPTÉR - jediné místo, které závisí na rozhraní scanneru.
 * ====================================================================== */
typedef Token tok_t;

#define NEXT_TOKEN(f) get_token(f)
#define FREE_TOKEN(t) free((t).atribute)
#define TOK_LINE(t) ((t).line)

/* Kód, kterým musí skončit proces při lexikální chybě (error_exit). */
#define LEX_ERROR_EXIT ERROR_LEX

/* Textová podoba hodnoty tokenu. Vrací 0, pokud token hodnotu nenese. */
static int token_value_raw(const tok_t *t, const char **raw) {
  switch (t->type) {
  case INT_NUM_TOKEN:
  case DOUBLE_NUM_TOKEN:
  case STRING_WORD_TOKEN:
  case IDENTIFIER_TOKEN:
    *raw = t->atribute ? t->atribute : "(NULL)";
    return 1;
  default:
    return 0;
  }
}
/* ====================================================================== */

/* Jména tokenů pro výpis: N(PLUS) -> "PLUS" (bez přípony _TOKEN).
 * Switch nemá default, takže při přidání tokenu do token.h překladač
 * upozorní (-Wswitch), že tady chybí. */
#define N(x)                                                                   \
  case x##_TOKEN:                                                              \
    return #x

static const char *token_name(Tokens type) {
  switch (type) {
    N(PLUS); N(MINUS); N(MUL); N(DIV); N(ASSIGN); N(EQ); N(NEQ);
    N(LESS_EQ); N(GREAT_EQ); N(LESS); N(GREAT);
    N(RPAR); N(LPAR); N(DOT); N(COMMA); N(AT); N(COLON); N(SEMICOLON);
    N(UNDERSCORE);
    N(DEF); N(IF); N(WHILE); N(DO); N(ELSE); N(THEN); N(RETURN); N(IMPORT);
    N(VAR); N(VAL);
    N(INT); N(STRING); N(DOUBLE); N(UNIT); N(BOOLEAN);
    N(INT_NUM); N(DOUBLE_NUM); N(STRING_WORD); N(IDENTIFIER);
    N(INDENT); N(DEDENT); N(EOF); N(EOL);
    N(FOR); N(AND); N(OR); N(NOT); N(TRUE); N(FALSE);
    N(CASE); N(CLASS); N(MATCH); N(TYPE); N(EXTENSION); N(LIST);
    N(LBRACKET); N(RBRACKET); N(ARROW_FAT); N(ARROW_LEFT); N(PIPE);
  }
  return "UNKNOWN";
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
  const char *raw;
  if (!token_value_raw(t, &raw))
    return 0;
  escape_into(raw, out, size);
  return 1;
}

#define MAX_LINES 4096
#define MAX_TOKENS 100000
#define MAX_OUT (1u << 20)

/* ========================================================================
 * Režim 1: výpis tokenů ze stdin
 * ====================================================================== */
static int dump_mode(void) {
  int count = 0;
  while (count < MAX_TOKENS) {
    tok_t t = NEXT_TOKEN(stdin);
    count++;
    char val[2048];
    printf("Token #%-3d: %-12s", count, token_name(t.type));
    if (token_value(&t, val, sizeof val))
      printf(" | Value: \"%s\"", val);
    printf("  [radek %d]\n", TOK_LINE(t));
    int is_eof = t.type == EOF_TOKEN;
    FREE_TOKEN(t);
    if (is_eof) {
      printf("\nOK: nacteno %d tokenu\n", count);
      return 0;
    }
  }
  printf("\nCHYBA: scanner nevratil EOF_TOKEN ani po %d tokenech\n", MAX_TOKENS);
  return 1;
}

/* ========================================================================
 * Režim 2: samokontrolní testy
 * ====================================================================== */
enum { K_TOKENS, K_LINES, K_OK, K_ERR, K_INFO };
/* K_TOKENS - proběhne bez chyby a tokeny odpovídají očekávání
 * K_LINES  - jako K_TOKENS, ale očekávání je "JMENO@radek" (hodnota se
 *            nekontroluje, kontroluje se Token.line)
 * K_OK     - proběhne bez chyby (tokeny se nekontrolují)
 * K_ERR    - proces musí skončit s kódem ERROR_LEX (lexikální chyba)
 * K_INFO   - jen vypíše tokeny; pro nejednoznačné případy ze specifikace */
enum { F_NONE = 0, F_STRUCT = 1, F_EXT = 2, F_EOL = 4 };
/* F_STRUCT - porovnávají se i INDENT/DEDENT (jinak se ignorují)
 * F_EOL    - porovnává se i EOL (jinak se ignoruje)
 * F_EXT    - test patří k rozšíření (přeskočí se s --base)
 * EOF se při porovnávání ignoruje vždy. */
#define F_FULL (F_STRUCT | F_EOL)

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
#define E_STR(s) "STRING_WORD:" s
#define EXPECT(...) ((const char *const[]){__VA_ARGS__, NULL})
#define NOTHING ((const char *const[]){NULL})

#define LEXERR(name, src) {name, K_ERR, F_NONE, src, NOTHING}

static const test_case_t cases[] = {
    /* ---------------------------------------------------------------- */
    /* Základní lexémy                                                   */
    /* ---------------------------------------------------------------- */
    {"prazdny vstup", K_TOKENS, F_FULL, "", NOTHING},
    {"jen komentare a prazdne radky", K_TOKENS, F_FULL,
     "// komentar\n\n/* blok */\n   \n", NOTHING},
    {"prolog", K_TOKENS, F_NONE, "import ifj26.*\n",
     EXPECT("IMPORT", E_ID("ifj26"), "DOT", "MUL")},
    {"klicova slova", K_TOKENS, F_NONE,
     "def if while do else then return import var val Int String Double Unit "
     "Boolean\n",
     EXPECT("DEF", "IF", "WHILE", "DO", "ELSE", "THEN", "RETURN", "IMPORT",
            "VAR", "VAL", "INT", "STRING", "DOUBLE", "UNIT", "BOOLEAN")},
    {"klicova slova jsou case-sensitive", K_TOKENS, F_NONE,
     "int string double unit boolean Def IF Var Val While Return\n",
     EXPECT(E_ID("int"), E_ID("string"), E_ID("double"), E_ID("unit"),
            E_ID("boolean"), E_ID("Def"), E_ID("IF"), E_ID("Var"),
            E_ID("Val"), E_ID("While"), E_ID("Return"))},
    {"identifikatory zacinajici klicovym slovem", K_TOKENS, F_NONE,
     "iffy define values Integer if_ _if elsewhere main if1 Int2 List_ "
     "trueish\n",
     EXPECT(E_ID("iffy"), E_ID("define"), E_ID("values"), E_ID("Integer"),
            E_ID("if_"), E_ID("_if"), E_ID("elsewhere"), E_ID("main"),
            E_ID("if1"), E_ID("Int2"), E_ID("List_"), E_ID("trueish"))},
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
    {"oddelovace", K_TOKENS, F_NONE, "( ) , . : ;\n",
     EXPECT("LPAR", "RPAR", "COMMA", "DOT", "COLON", "SEMICOLON")},
    {"@main", K_TOKENS, F_NONE, "@main def run(): Unit =\n",
     EXPECT("AT", E_ID("main"), "DEF", E_ID("run"), "LPAR", "RPAR", "COLON",
            "UNIT", "ASSIGN")},
    LEXERR("@ bez main", "@foo\n"),
    LEXERR("@ na konci souboru", "@"),
    {"zaporne cislo je MINUS + cislo", K_TOKENS, F_NONE, "x = -5\n",
     EXPECT(E_ID("x"), "ASSIGN", "MINUS", E_NUM("5"))},
    {"minus bez mezer", K_TOKENS, F_NONE, "a-b\n",
     EXPECT(E_ID("a"), "MINUS", E_ID("b"))},
    {"deleni (ne komentar)", K_TOKENS, F_NONE, "a / b /c d/e\n",
     EXPECT(E_ID("a"), "DIV", E_ID("b"), "DIV", E_ID("c"), E_ID("d"), "DIV",
            E_ID("e"))},
    {"strednik", K_TOKENS, F_NONE, "a; b\n",
     EXPECT(E_ID("a"), "SEMICOLON", E_ID("b"))},
    {"tabulator mezi tokeny je bily znak", K_TOKENS, F_NONE, "a\tb\n",
     EXPECT(E_ID("a"), E_ID("b"))},
    {"hlavicka funkce bez mezer", K_TOKENS, F_NONE, "def f(x:Int):Int=x\n",
     EXPECT("DEF", E_ID("f"), "LPAR", E_ID("x"), "COLON", "INT", "RPAR",
            "COLON", "INT", "ASSIGN", E_ID("x"))},
    {"tecka mezi identifikatory", K_TOKENS, F_NONE, "a.b\n",
     EXPECT(E_ID("a"), "DOT", E_ID("b"))},

    /* ---------------------------------------------------------------- */
    /* Konec řádku (EOL)                                                 */
    /* ---------------------------------------------------------------- */
    {"EOL: za kazdym radkem", K_TOKENS, F_EOL, "a\nb\n",
     EXPECT(E_ID("a"), "EOL", E_ID("b"), "EOL")},
    {"EOL: vstup bez koncoveho LF se ukonci EOL", K_TOKENS, F_EOL, "x",
     EXPECT(E_ID("x"), "EOL")},
    {"EOL: prazdne radky se slevaji", K_TOKENS, F_EOL, "a\n\n\n   \nb\n",
     EXPECT(E_ID("a"), "EOL", E_ID("b"), "EOL")},
    {"EOL: prazdne radky na zacatku souboru", K_TOKENS, F_EOL, "\n\na\n",
     EXPECT(E_ID("a"), "EOL")},
    {"EOL: radkovy komentar", K_TOKENS, F_EOL, "a // c\nb\n",
     EXPECT(E_ID("a"), "EOL", E_ID("b"), "EOL")},
    {"EOL: radek jen s komentarem", K_TOKENS, F_EOL, "a\n// c\nb\n",
     EXPECT(E_ID("a"), "EOL", E_ID("b"), "EOL")},
    {"EOL: viceradkovy blokovy komentar", K_TOKENS, F_EOL,
     "a /* x\ny */\nb\n", EXPECT(E_ID("a"), "EOL", E_ID("b"), "EOL")},
    {"EOL: uvnitr zavorek se negeneruje", K_TOKENS, F_EOL, "f(a,\nb)\nc\n",
     EXPECT(E_ID("f"), "LPAR", E_ID("a"), "COMMA", E_ID("b"), "RPAR", "EOL",
            E_ID("c"), "EOL")},
    {"EOL: viceradkovy retezec je jeden token", K_TOKENS, F_EOL,
     "\"\"\"a\nb\"\"\"\nc\n",
     EXPECT("STRING_WORD", "EOL", E_ID("c"), "EOL")},

    /* ---------------------------------------------------------------- */
    /* Číselné literály                                                  */
    /* ---------------------------------------------------------------- */
    {"cela cisla", K_TOKENS, F_NONE,
     "0 7 42 1_000_000 123456789012345678901234567890\n",
     EXPECT(E_NUM("0"), E_NUM("7"), E_NUM("42"), E_NUM("1000000"),
            E_NUM("123456789012345678901234567890"))},
    {"desetinna cisla", K_TOKENS, F_NONE,
     "3.14 .5 0.5 1e10 1E+5 2.5e-3 1.5d 1.5D 1_0.2_5 0e0 1_0e1_0\n",
     EXPECT(E_DBL("3.14"), E_DBL(".5"), E_DBL("0.5"), E_DBL("1e10"),
            E_DBL("1E+5"), E_DBL("2.5e-3"), E_DBL("1.5"), E_DBL("1.5"),
            E_DBL("10.25"), E_DBL("0e0"), E_DBL("10e10"))},
    {"cislo hned pred operatorem a zavorkou", K_TOKENS, F_NONE,
     "(1+2.5)*3\n",
     EXPECT("LPAR", E_NUM("1"), "PLUS", E_DBL("2.5"), "RPAR", "MUL",
            E_NUM("3"))},
    LEXERR("cele cislo s nulou na zacatku", "x = 007\n"),
    LEXERR("exponent bez cislice (1.5e)", "x = 1.5e\n"),
    LEXERR("exponent bez cislice (1e+)", "x = 1e+\n"),
    LEXERR("cele cislo nasledovane pismenem", "x = 12abc\n"),
    LEXERR("desetinne cislo nasledovane pismenem", "x = 1.5x\n"),
    LEXERR("exponent nasledovany pismenem", "x = 1e5x\n"),

    /* ---------------------------------------------------------------- */
    /* Řetězcové literály                                                */
    /* ---------------------------------------------------------------- */
    {"retezce", K_TOKENS, F_NONE,
     "\"hello\" \"\" \"a b\" \"x // y\" \"/* z */\"\n",
     EXPECT(E_STR("hello"), E_STR(""), E_STR("a b"), E_STR("x // y"),
            E_STR("/* z */"))},
    /* zdroj: "\\ \" \n \r \t"  ->  hodnota: \ " LF CR TAB (odděleno mezerami) */
    {"escape sekvence", K_TOKENS, F_NONE,
     "\"\\\\ \\\" \\n \\r \\t\"\n",
     EXPECT(E_STR("\\\\ \" \\n \\r \\t"))},
    {"uvozovka uvnitr retezce", K_TOKENS, F_NONE, "\"a\\\"b\"\n",
     EXPECT(E_STR("a\"b"))},
    {"viceradkovy retezec", K_TOKENS, F_NONE,
     "val x = \"\"\"A\nhoj\n\"\"\"\n",
     EXPECT("VAL", E_ID("x"), "ASSIGN", E_STR("A\\nhoj\\n"))},
    {"viceradkovy retezec s uvozovkami a // uvnitr", K_TOKENS, F_NONE,
     "\"\"\"say \"hi\" // not comment\n\"\"\" x\n",
     EXPECT(E_STR("say \"hi\" // not comment\\n"), E_ID("x"))},
    {"prazdny viceradkovy retezec", K_TOKENS, F_NONE, "x = \"\"\"\"\"\"\n",
     EXPECT(E_ID("x"), "ASSIGN", E_STR(""))},
    {"dve uvozovky uvnitr viceradkoveho retezce", K_TOKENS, F_NONE,
     "\"\"\"a\"\"b\"\"\"\n", EXPECT(E_STR("a\"\"b"))},
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
    {"radkovy komentar obsahujici /*", K_TOKENS, F_NONE, "a // /* x\nb\n",
     EXPECT(E_ID("a"), E_ID("b"))},
    {"blokovy komentar", K_TOKENS, F_NONE, "a /* x */ b /**/ c\n",
     EXPECT(E_ID("a"), E_ID("b"), E_ID("c"))},
    {"blokovy komentar bez mezer", K_TOKENS, F_NONE, "a/*c*/b\n",
     EXPECT(E_ID("a"), E_ID("b"))},
    {"blokovy komentar obsahujici //", K_TOKENS, F_NONE, "a /* // */ b\n",
     EXPECT(E_ID("a"), E_ID("b"))},
    {"blokovy komentar s hvezdickami", K_TOKENS, F_NONE, "a /*** x ***/ b\n",
     EXPECT(E_ID("a"), E_ID("b"))},
    {"vnorene blokove komentare", K_TOKENS, F_NONE,
     "a /* x /* y */ z */ b\n", EXPECT(E_ID("a"), E_ID("b"))},
    {"blokovy komentar pres vice radku", K_TOKENS, F_NONE,
     "a /* x\ny */\nb\n", EXPECT(E_ID("a"), E_ID("b"))},
    LEXERR("neukonceny blokovy komentar", "a /* x\n"),
    LEXERR("neukonceny vnoreny blokovy komentar", "a /* x /* y */\n"),
    /* spec: za viceradkovym komentarem nesmi byt dalsi tokeny - je to ale
     * lex. (1) nebo synt. (2) chyba? Overit na foru, tady jen vypis. */
    {"INFO: token za viceradkovym komentarem na stejnem radku", K_INFO,
     F_FULL, "a /* x\n */ b\n", NOTHING},

    /* ---------------------------------------------------------------- */
    /* Neznámé znaky                                                     */
    /* ---------------------------------------------------------------- */
    LEXERR("neznamy znak & (jedno)", "x = a & b\n"),
    LEXERR("neznamy znak # ", "x = # 1\n"),
    LEXERR("neznamy znak {", "def f() = {\n"),
    LEXERR("neznamy znak %", "x = a % b\n"),
    LEXERR("neznamy znak $ mimo retezec", "x = $a\n"),
    LEXERR("neznamy znak ?", "x = a ? b\n"),
    LEXERR("identifikator v obracenych apostrofech", "x = `a`\n"),
    LEXERR("apostrof", "x = 'a'\n"),
    LEXERR("znak mimo ASCII", "x = \xC3\xA9\n"),

    /* ---------------------------------------------------------------- */
    /* Odsazení (INDENT / DEDENT) - porovnává se celý tok vč. EOL        */
    /* ---------------------------------------------------------------- */
    {"odsazeni - jedna uroven", K_TOKENS, F_FULL,
     "def f(): Int =\n  1\nf\n",
     EXPECT("DEF", E_ID("f"), "LPAR", "RPAR", "COLON", "INT", "ASSIGN", "EOL",
            "INDENT", E_NUM("1"), "EOL", "DEDENT", E_ID("f"), "EOL")},
    {"odsazeni - vicenasobny dedent", K_TOKENS, F_FULL, "a\n  b\n    c\nd\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "INDENT", E_ID("c"),
            "EOL", "DEDENT", "DEDENT", E_ID("d"), "EOL")},
    {"odsazeni - navrat na strednou uroven", K_TOKENS, F_FULL,
     "a\n  b\n    c\n  d\ne\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "INDENT", E_ID("c"),
            "EOL", "DEDENT", E_ID("d"), "EOL", "DEDENT", E_ID("e"), "EOL")},
    {"odsazeni - vice bloku za sebou", K_TOKENS, F_FULL,
     "a\n  b\nc\n  d\ne\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "DEDENT", E_ID("c"),
            "EOL", "INDENT", E_ID("d"), "EOL", "DEDENT", E_ID("e"), "EOL")},
    {"odsazeni - jiny pocet mezer nez 2", K_TOKENS, F_FULL,
     "a\n    b\n      c\nd\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "INDENT", E_ID("c"),
            "EOL", "DEDENT", "DEDENT", E_ID("d"), "EOL")},
    {"odsazeni - dedent na konci souboru (s LF)", K_TOKENS, F_FULL,
     "a\n  b\n    c\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "INDENT", E_ID("c"),
            "EOL", "DEDENT", "DEDENT")},
    {"odsazeni - dedent na konci souboru (bez LF)", K_TOKENS, F_FULL,
     "a\n  b\n    c",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", "INDENT", E_ID("c"),
            "EOL", "DEDENT", "DEDENT")},
    {"odsazeni - prazdne radky a radkove komentare nic nemeni", K_TOKENS,
     F_FULL, "a\n  b\n\n      \n// c\n  // d\n  c\nd\n",
     EXPECT(E_ID("a"), "EOL", "INDENT", E_ID("b"), "EOL", E_ID("c"), "EOL",
            "DEDENT", E_ID("d"), "EOL")},
    /* Blokovy komentar na vlastnim radku (zde odsazeny o 6 mezer) se nesmi
     * chovat jako kod - nema vytvorit INDENT ani DEDENT. */
    {"odsazeni - blokovy komentar na vlastnim radku nic nemeni", K_TOKENS,
     F_STRUCT, "a\n  b\n      /* d */\n  c\nd\n",
     EXPECT(E_ID("a"), "INDENT", E_ID("b"), E_ID("c"), "DEDENT", E_ID("d"))},
    {"odsazeni - uvnitr zavorek se nic negeneruje", K_TOKENS, F_FULL,
     "f(a,\n    b)\nc\n",
     EXPECT(E_ID("f"), "LPAR", E_ID("a"), "COMMA", E_ID("b"), "RPAR", "EOL",
            E_ID("c"), "EOL")},
    {"odsazeni - if / else", K_TOKENS, F_FULL,
     "if x < 5 then\n  print(\"a\")\nelse\n  ()\nx\n",
     EXPECT("IF", E_ID("x"), "LESS", E_NUM("5"), "THEN", "EOL", "INDENT",
            E_ID("print"), "LPAR", E_STR("a"), "RPAR", "EOL", "DEDENT", "ELSE",
            "EOL", "INDENT", "LPAR", "RPAR", "EOL", "DEDENT", E_ID("x"),
            "EOL")},
    LEXERR("tabulator na zacatku radku", "a\n\tb\n"),
    LEXERR("tabulator za mezerami na zacatku radku", "a\n  \tb\n"),
    LEXERR("dedent na neexistujici uroven", "a\n    b\n  c\n"),

    /* ---------------------------------------------------------------- */
    /* Čísla řádků (Token.line)                                          */
    /* ---------------------------------------------------------------- */
    {"radky - tokeny na peti radcich", K_LINES, F_EOL, "a b\nc\n\nd\n",
     EXPECT("IDENTIFIER@1", "IDENTIFIER@1", "EOL@1", "IDENTIFIER@2", "EOL@2",
            "IDENTIFIER@4", "EOL@4")},
    {"radky - komentare a prazdne radky se pocitaji", K_LINES, F_EOL,
     "// c\n\na\n\n// d\nb\n",
     EXPECT("IDENTIFIER@3", "EOL@3", "IDENTIFIER@6", "EOL@6")},
    {"radky - INDENT a DEDENT", K_LINES, F_FULL, "a\n  b\nc\n",
     EXPECT("IDENTIFIER@1", "EOL@1", "INDENT@2", "IDENTIFIER@2", "EOL@2",
            "DEDENT@3", "IDENTIFIER@3", "EOL@3")},
    {"radky - vstup bez koncoveho LF", K_LINES, F_EOL, "a\nb",
     EXPECT("IDENTIFIER@1", "EOL@1", "IDENTIFIER@2", "EOL@2")},

    /* ---------------------------------------------------------------- */
    /* Celý program (příklad ze specifikace) - celý tok tokenů           */
    /* ---------------------------------------------------------------- */
    {"program: factorial ze specifikace", K_TOKENS, F_FULL,
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
     EXPECT(
         "IMPORT", E_ID("ifj26"), "DOT", "MUL", "EOL",
         "DEF", E_ID("factorial"), "LPAR", E_ID("n"), "COLON", "INT", "RPAR",
         "COLON", "INT", "ASSIGN", "EOL",
         "INDENT", "VAR", E_ID("result"), "ASSIGN", E_NUM("1"), "EOL",
         "VAR", E_ID("i"), "COLON", "INT", "ASSIGN", E_ID("n"), "EOL",
         "WHILE", E_ID("i"), "GREAT", E_NUM("1"), "DO", "EOL",
         "INDENT", E_ID("result"), "ASSIGN", E_ID("result"), "MUL", E_ID("i"),
         "EOL",
         E_ID("i"), "ASSIGN", E_ID("i"), "MINUS", E_NUM("1"), "EOL",
         "DEDENT", E_ID("result"), "EOL",
         "DEDENT", "AT", E_ID("main"), "DEF", E_ID("runFoo"), "LPAR", "RPAR",
         "COLON", "UNIT", "ASSIGN", "EOL",
         "INDENT", "VAL", E_ID("n"), "ASSIGN", E_ID("readInt"), "LPAR", "RPAR",
         "EOL",
         "VAL", E_ID("result"), "ASSIGN", E_ID("factorial"), "LPAR", E_ID("n"),
         "RPAR", "EOL",
         E_ID("print"), "LPAR", E_ID("result"), "RPAR", "EOL",
         E_ID("print"), "LPAR", E_STR("\\n"), "RPAR", "EOL",
         "DEDENT")},

    /* ---------------------------------------------------------------- */
    /* Rozšíření                                                         */
    /* ---------------------------------------------------------------- */
    {"rozsireni: klicova slova", K_TOKENS, F_EXT,
     "for case class match type extension List true false\n",
     EXPECT("FOR", "CASE", "CLASS", "MATCH", "TYPE", "EXTENSION", "LIST",
            "TRUE", "FALSE")},
    {"rozsireni: operatory a symboly", K_TOKENS, F_EXT,
     "&& || ! [ ] => <- | _\n",
     EXPECT("AND", "OR", "NOT", "LBRACKET", "RBRACKET", "ARROW_FAT",
            "ARROW_LEFT", "PIPE", "UNDERSCORE")},
    {"rozsireni: operatory bez mezer", K_TOKENS, F_EXT,
     "a=>b a<-b a!=b !c f||g h&&i\n",
     EXPECT(E_ID("a"), "ARROW_FAT", E_ID("b"), E_ID("a"), "ARROW_LEFT",
            E_ID("b"), E_ID("a"), "NEQ", E_ID("b"), "NOT", E_ID("c"),
            E_ID("f"), "OR", E_ID("g"), E_ID("h"), "AND", E_ID("i"))},
    {"rozsireni: podtrzitko samo a v identifikatorech", K_TOKENS, F_EXT,
     "_ _a a_ __\n",
     EXPECT("UNDERSCORE", E_ID("_a"), E_ID("a_"), E_ID("__"))},
    {"rozsireni: case _ =>", K_TOKENS, F_EXT, "case _ => 0\n",
     EXPECT("CASE", "UNDERSCORE", "ARROW_FAT", E_NUM("0"))},
    {"rozsireni: List[Int]", K_TOKENS, F_EXT, "List[Int]\n",
     EXPECT("LIST", "LBRACKET", "INT", "RBRACKET")},
    {"rozsireni: vnorene List[List[Int]]", K_TOKENS, F_EXT,
     "List[List[Int]]\n",
     EXPECT("LIST", "LBRACKET", "LIST", "LBRACKET", "INT", "RBRACKET",
            "RBRACKET")},
    {"rozsireni: sjednoceni typu", K_TOKENS, F_EXT, "Int | String\n",
     EXPECT("INT", "PIPE", "STRING")},
    /* to / until / by nejsou v token.h, takze to jsou obycejne identifikatory.
     * Pokud je pridate jako klicova slova, upravte i tento test. */
    {"rozsireni: cyklus for (to / by jsou identifikatory)", K_TOKENS, F_EXT,
     "for i <- 1 to 10 by 2 do\n",
     EXPECT("FOR", E_ID("i"), "ARROW_LEFT", E_NUM("1"), E_ID("to"),
            E_NUM("10"), E_ID("by"), E_NUM("2"), "DO")},
    /* "5." neni desetinny literal (desetinna cast je povinna), takze
     * "5.toString" ma dat INT DOT IDENT - chce to 2 znaky dopredu. */
    {"rozsireni: tecka za celym cislem", K_TOKENS, F_EXT,
     "5.toString x.floor.toInt\n",
     EXPECT(E_NUM("5"), "DOT", E_ID("toString"), E_ID("x"), "DOT",
            E_ID("floor"), "DOT", E_ID("toInt"))},
    {"rozsireni: zavorky pres vice radku", K_TOKENS, F_FULL | F_EXT,
     "List[Int,\n  String]\nx\n",
     EXPECT("LIST", "LBRACKET", "INT", "COMMA", "STRING", "RBRACKET", "EOL",
            E_ID("x"), "EOL")},

    /* ---------------------------------------------------------------- */
    /* Nejednoznačné případy - jen výpis, ať vidíš, co scanner dělá      */
    /* ---------------------------------------------------------------- */
    {"INFO: CRLF konce radku", K_INFO, F_FULL, "a\r\nb\r\n", NOTHING},
    {"INFO: 1_ (podtrzitko na konci cisla)", K_INFO, F_NONE, "x = 1_\n",
     NOTHING},
    {"INFO: 1__0 (dve podtrzitka v cisle)", K_INFO, F_NONE, "x = 1__0\n",
     NOTHING},
    {"INFO: 3. (tecka bez desetinne casti)", K_INFO, F_NONE, "x = 3.\n",
     NOTHING},
    {"INFO: 1d (pripona d bez tecky a exponentu)", K_INFO, F_NONE,
     "x = 1d\n", NOTHING},
    {"INFO: viceradkovy retezec s \\n uvnitr (zpracuje se escape?)", K_INFO,
     F_NONE, "x = \"\"\"a\\nb\"\"\"\n", NOTHING},
    {"INFO: viceradkovy retezec koncici uvozovkou", K_INFO, F_NONE,
     "x = \"\"\"a\"\"\"\"\n", NOTHING},
    {"INFO: @mainx", K_INFO, F_NONE, "@mainx\n", NOTHING},
};

/* Tělo potomka: nasměruje scanner na dočasný soubor se zdrojovým textem a
 * vypisuje tokeny do rourky (jeden řádek na token: "JMENO\tradek" nebo
 * "JMENO:hodnota\tradek"). */
static void child_main(const char *src, int pipe_wr, int verbose) {
  FILE *tmp = tmpfile();
  if (!tmp)
    _exit(98);
  if (fputs(src, tmp) == EOF || fflush(tmp) != 0)
    _exit(98);
  rewind(tmp);

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
  for (int i = 0; i < MAX_TOKENS; i++) {
    tok_t t = NEXT_TOKEN(tmp);
    char val[2048];
    if (token_value(&t, val, sizeof val))
      fprintf(out, "%s:%s\t%d\n", token_name(t.type), val, TOK_LINE(t));
    else
      fprintf(out, "%s\t%d\n", token_name(t.type), TOK_LINE(t));
    int is_eof = t.type == EOF_TOKEN;
    FREE_TOKEN(t);
    if (is_eof) {
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

/* Rozdělí výstup potomka na řádky; u každého oddělí číslo řádku zdroje
 * (za tabulátorem) do lnums. */
static size_t split_lines(char *buf, const char **lines, int *lnums,
                          size_t max) {
  size_t n = 0;
  char *p = buf;
  while (*p && n < max) {
    char *nl = strchr(p, '\n');
    if (nl)
      *nl = '\0';
    char *tab = strchr(p, '\t');
    lnums[n] = 0;
    if (tab) {
      *tab = '\0';
      lnums[n] = atoi(tab + 1);
    }
    lines[n++] = p;
    if (!nl)
      break;
    p = nl + 1;
  }
  return n;
}

static int is_ignored(const char *line, int flags) {
  if (!strcmp(line, "EOF"))
    return 1;
  if (!(flags & F_EOL) && !strcmp(line, "EOL"))
    return 1;
  if (!(flags & F_STRUCT) && (!strcmp(line, "INDENT") || !strcmp(line, "DEDENT")))
    return 1;
  return 0;
}

static int line_matches(const char *want, const char *got) {
  if (strchr(want, ':'))
    return strcmp(want, got) == 0;
  size_t n = strcspn(got, ":");
  return strlen(want) == n && strncmp(want, got, n) == 0;
}

/* want = "JMENO@radek"; hodnota tokenu se nekontroluje. */
static int line_matches_ln(const char *want, const char *got, int got_ln) {
  const char *at = strchr(want, '@');
  if (!at)
    return 0;
  size_t n = (size_t)(at - want);
  return strcspn(got, ":") == n && strncmp(want, got, n) == 0 &&
         atoi(at + 1) == got_ln;
}

static void print_list(const char *label, const char *const *items, size_t n) {
  printf("         %s", label);
  for (size_t i = 0; i < n; i++)
    printf("%s%s", i ? " | " : "", items[i]);
  printf("\n");
}

static void print_list_ln(const char *label, const char *const *items,
                          const int *lnums, size_t n) {
  printf("         %s", label);
  for (size_t i = 0; i < n; i++) {
    size_t len = strcspn(items[i], ":");
    printf("%s%.*s@%d", i ? " | " : "", (int)len, items[i], lnums[i]);
  }
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
    int lnums[MAX_LINES];
    size_t nlines = split_lines(r.out, lines, lnums, MAX_LINES);
    char status[64];
    describe(&r, status, sizeof status);

    int ok = 0;
    const char *got[MAX_LINES];
    int gotln[MAX_LINES];
    size_t ngot = 0;
    size_t nexp = 0;

    if (c->kind == K_INFO) {
      printf("[INFO] %s (%s)\n", c->name, status);
      print_list_ln("tokeny: ", lines, lnums, nlines);
      info++;
      free(r.out);
      continue;
    }

    switch (c->kind) {
    case K_ERR:
      ok = r.exited && r.code == LEX_ERROR_EXIT;
      break;
    case K_OK:
      ok = r.exited && r.code == 0;
      break;
    default: /* K_TOKENS, K_LINES */
      for (size_t j = 0; j < nlines; j++)
        if (!is_ignored(lines[j], c->flags)) {
          got[ngot] = lines[j];
          gotln[ngot] = lnums[j];
          ngot++;
        }
      while (c->expect[nexp])
        nexp++;
      ok = r.exited && r.code == 0 && ngot == nexp;
      for (size_t k = 0; ok && k < nexp; k++)
        ok = c->kind == K_LINES ? line_matches_ln(c->expect[k], got[k], gotln[k])
                                : line_matches(c->expect[k], got[k]);
      break;
    }

    if (ok) {
      printf("[ OK ] %s\n", c->name);
      pass++;
    } else {
      fail++;
      printf("[FAIL] %s (%s)\n", c->name, status);
      if (c->kind == K_ERR) {
        printf("         ocekavana lexikalni chyba (exit %d)\n",
             LEX_ERROR_EXIT);
        print_list_ln("tokeny: ", lines, lnums, nlines);
      } else if (c->kind == K_OK) {
        printf("         ocekavan exit 0\n");
        print_list_ln("tokeny: ", lines, lnums, nlines);
      } else if (c->kind == K_LINES) {
        print_list("ocekavano: ", c->expect, nexp);
        print_list_ln("ziskano:   ", got, gotln, ngot);
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
