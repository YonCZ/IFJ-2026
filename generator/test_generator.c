/*
 * Projekt: IFJ26 – překladač jazyka IFJ26 do IFJcode26
 * Soubor:  test_generator.c – test generátoru kódu (fáze 0: kostra, 1: konstanty a print,
 *                            2: výrazy, 3: proměnné)
 *
 * ---------------------------------------------------------------------------
 * Tento soubor vygenerovala AI: Claude Sonnet 5.5 (Anthropic, claude-sonnet-5-5), 2026-11-17.
 * Prompt 1: "Mám hotovou fázi nula, chci, abys vytvořil nějaké testy, se kterými to můžu
 *            vyzkoušet. Nemáme parser ani sémantickou analýzu, chci otestovat jenom můj
 *            generátor kódu s AST. Udělej mi testovací skript."
 * Prompt 2: "vytvoř mi test na fázi 1" (konstanty a print: Int, Double, String, Boolean, ())
 * Prompt 3: "udelej testy pro fazi 3" (proměnné: val, var, přiřazení, použití proměnné)
 * Prompt 4: "vytvor test pro fazi 2" (výrazy: + - * /, řetězce, porovnání, Int a Double)
 * Zadal a zrevidoval: Pavel Novák (xnovakp01)
 * ---------------------------------------------------------------------------
 *
 * CO TEST DĚLÁ
 *   Postaví ručně malý AST (bez parseru a sémantiky), zavolá generator_init(), zkontroluje
 *   tvar vygenerovaného IFJcode26 a pak spočítá, co by program vypsal, a porovná to s tím,
 *   co vypsat má. Výpis počítá malý simulátor přímo v tomto souboru (umí instrukce, které
 *   fáze 0 až 3 potřebují), takže test funguje i bez interpretu ic26int. Pokud ic26int najde
 *   a jde spustit, pustí v něm kód ještě jednou a porovná výstup také.
 *   Každý případ běží ve vlastním procesu, takže pád generátoru shodí jen ten jeden případ.
 *
 * PŘEKLAD (z adresáře src/generator/; ast.c a generátor se linkují normálně, bez unity buildu):
 *
 *     gcc -std=gnu11 -g -O0 -Wall -Wextra -I.. test_generator.c ast.c generator.c gen_*.c \
 *         -o test_generator && ./test_generator
 *
 * POUŽITÍ
 *     ./test_generator              všechny případy (fáze 0 až 3)
 *     ./test_generator f2           jen fáze 2 (f0, f1, f3 = jen ta fáze)
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
 *   - každá proměnná, která se v kódu funkce použije (LF@x$1), má DEFVAR ve stejné funkci
 *   - všechny DEFVAR funkce jsou na jejím začátku, hned za PUSHFRAME (DEFVAR uprostřed těla by
 *     se v cyklu provedl víckrát a interpret by skončil chybou)
 *   - každé RETURN je hned za POPFRAME a před POPFRAME je PUSHS nil@nil (Unit vrací nil@nil)
 *   Chování (simulátor, a pokud je k dispozici, i ic26int):
 *   - program doběhne s kódem 0 a vypíše přesně to, co má (jediný znak navíc je chyba)
 *   - žádná běhová chyba: proměnná bez DEFVAR, dvojí DEFVAR, prázdný zásobník, chybný rámec
 *   - po skončení programu na zásobníku nezůstane nic navíc (příkaz po sobě nezanechá nic;
 *     jedna hodnota smí zůstat: návratová hodnota main)
 *   Řetězce: u případů s mezerou, novým řádkem, # a \ se navíc kontroluje přesný zápis
 *   literálu v kódu (např. string@a\032b\010).
 *   Fáze 2 (výrazy): uzel BINOP se řídí typy uzlů, jak je zapíše sémantika. Test proto u každého
 *   uzlu nastavuje data_type: u výsledku BINOP i u operandů (literál, proměnná, vnořený BINOP).
 *   Podle toho generátor pozná Int / Double / String:
 *   - + - * nad Int: ADDS SUBS MULS; / nad Int: IDIVS (celočíselně, k nule); nad Double: DIVS
 *   - + nad dvěma String: CONCAT (zásobníková verze neexistuje, je potřeba pomocná proměnná,
 *     její DEFVAR musí být na začátku funkce jako všechny ostatní)
 *   - smíšené Int a Double: Int operand se hned po vyhodnocení převede (INT2FLOATS), jinak by
 *     interpret skončil chybou 53. Levý operand se musí převést dřív, než se vyhodnotí pravý
 *   - < > == : LTS GTS EQS; <= je GTS+NOTS, >= je LTS+NOTS, != je EQS+NOTS
 *   - pořadí: nejdřív levý operand, pak pravý (jinak 10 - 4 vyjde jako 4 - 10)
 *   - výsledek porovnání se v testu vypisuje přes print (v jazyce to bez rozšíření BOOLTHEN nejde,
 *     ale generátoru je to jedno: výraz vždy nechá jednu hodnotu na zásobníku)
 *   - unární mínus: UNOP s OP_SUB (0 - x). Konvence AST pro unární mínus není ve specifikaci;
 *     pokud ji máte jinak, případy f2_neg a f2_neg_double smaž nebo uprav (funkce neg())
 *
 * KDE UPRAVIT, KDYŽ SE ZMĚNÍ ROZHRANÍ
 *   make_program() – jak se skládá NODE_PROGRAM (teď: left = první funkce, další přes next)
 *   make_body()    – jak se skládá tělo funkce (teď: rovnou první příkaz, další příkazy přes
 *                    next, bez uzlu NODE_BLOCK; takhle to staví parser, viz parse_block)
 *   use(), vdef(), assign() – jak se v AST staví proměnná, deklarace a přiřazení (teď: NODE_IDENT
 *                    se jménem a číslem id, NODE_VAR_DEF s hodnotou v left, NODE_ASSIGN: left =
 *                    proměnná, right = výraz). Jméno proměnné v kódu je jmeno$id, např. LF@x$1
 *   print_stmt()   – jak vypadá print (teď: NODE_EXPR_STMT → NODE_CALL "print" → argument
 *                    v third)
 *   run_generator() – jak se volá generátor (teď: Gen gen = {0}; generator_init(&gen, root))
 *   bin(), neg()   – jak se staví binární a unární operátor (BINOP: left, right, op_type;
 *                    UNOP: left = operand, op_type OP_SUB)
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
  /* Tak to staví parser (parse_block): tělo funkce je rovnou řetěz příkazů přes next, bez uzlu
   * NODE_BLOCK. Tedy fn->left je první příkaz. */
  return first_statement;
}

/* print(arg); jako příkaz, další příkaz se napojí přes next */
static AST_NODE *print_stmt(AST_NODE *arg, AST_NODE *next) {
  AST_NODE *call = ast_call_create(arg, (char *)"print", false, true, NULL);
  return ast_create(call, NULL, next, NODE_EXPR_STMT);
}

/* Proměnná v AST: jméno + unikátní číslo (v kódu LF@x$1). data_type by zapsala sémantika. */
static AST_NODE *use(const char *name, int id, DATA_TYPE t) {
  AST_NODE *n = ast_create(NULL, NULL, NULL, NODE_IDENT);
  ast_name(n, (char *)name);
  n->id = id;
  n->data_type = t;
  return n;
}

/* val / var name: t = init; hodnota je v left, další příkaz přes next */
static AST_NODE *vdef(const char *name, int id, bool is_const, DATA_TYPE t, AST_NODE *init,
                      AST_NODE *next) {
  AST_NODE *n = ast_var_def_create(init, next, (char *)name, is_const, t);
  n->id = id;
  return n;
}

/* x = výraz; jako samostatný příkaz */
static AST_NODE *assign(AST_NODE *target, AST_NODE *expr, AST_NODE *next) {
  return ast_assign_create(target, expr, next);
}

/* l + r pro Int (typ by zapsala sémantika) */
static AST_NODE *add_int(AST_NODE *l, AST_NODE *r) {
  AST_NODE *n = ast_binop_create(l, r, NULL, OP_ADD);
  n->data_type = DATA_INT;
  return n;
}

/* l op r s výsledným typem t (typ by zapsala sémantika; typy operandů mají mít uzly samy) */
static AST_NODE *bin(OP_TYPE op, AST_NODE *l, AST_NODE *r, DATA_TYPE t) {
  AST_NODE *n = ast_binop_create(l, r, NULL, op);
  n->data_type = t;
  return n;
}

/* unární mínus: -x. Operand v left, op_type OP_SUB. */
static AST_NODE *neg(AST_NODE *x, DATA_TYPE t) {
  AST_NODE *n = ast_unop_create(x, NULL, OP_SUB);
  n->data_type = t;
  return n;
}

static void run_generator(AST_NODE *program) {
  Gen gen = {0};
  generator_init(&gen, program);
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
static AST_NODE *build_f1_control(void) {
  /* návrat vozíku (13) a znak s kódem 1: všechny znaky 0-32 musí jít jako \\xyz */
  return main_prints(print_stmt(lit_s("a\rb\001c"), NULL));
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

/* fáze 3: proměnné. Všude jen main, typy a čísla proměnných jako by je zapsala sémantika. */
#define I DATA_INT
static AST_NODE *build_f3_var(void) { /* var x = 5; print(x) */
  return main_prints(vdef("x", 1, false, I, lit_i(5), print_stmt(use("x", 1, I), NULL)));
}
static AST_NODE *build_f3_val(void) { /* val x = 7; print(x) */
  return main_prints(vdef("x", 1, true, I, lit_i(7), print_stmt(use("x", 1, I), NULL)));
}
static AST_NODE *build_f3_types(void) { /* String, Double a Boolean v proměnných */
  return main_prints(vdef(
      "s", 1, true, DATA_STRING, lit_s("Ahoj\n"),
      vdef("d", 2, true, DATA_DOUBLE, lit_d(3.5),
           vdef("b", 3, true, DATA_BOOL, lit_b(true),
                print_stmt(use("s", 1, DATA_STRING),
                           print_stmt(use("d", 2, DATA_DOUBLE),
                                      print_stmt(use("b", 3, DATA_BOOL), NULL)))))));
}
static AST_NODE *build_f3_mixed(void) { /* deklarace uprostřed těla; DEFVAR musí být stejně nahoře */
  return main_prints(print_stmt(
      lit_s("x="), vdef("x", 1, false, I, lit_i(5),
                        print_stmt(use("x", 1, I), print_stmt(lit_s("\n"), NULL)))));
}
static AST_NODE *build_f3_two(void) { /* druhá deklarace v řadě se nesmí přeskočit */
  return main_prints(vdef("a", 1, false, I, lit_i(1),
                          vdef("b", 2, false, I, lit_i(2),
                               print_stmt(use("a", 1, I), print_stmt(use("b", 2, I), NULL)))));
}
static AST_NODE *build_f3_many(void) { /* pět deklarací za sebou */
  return main_prints(vdef(
      "a", 1, false, I, lit_i(1),
      vdef("b", 2, false, I, lit_i(2),
           vdef("c", 3, false, I, lit_i(3),
                vdef("d", 4, false, I, lit_i(4),
                     vdef("e", 5, false, I, lit_i(5),
                          print_stmt(use("a", 1, I),
                                     print_stmt(use("b", 2, I),
                                                print_stmt(use("c", 3, I),
                                                           print_stmt(use("d", 4, I),
                                                                      print_stmt(use("e", 5, I),
                                                                                 NULL)))))))))));
}
static AST_NODE *build_f3_same_name(void) { /* stejné jméno, jiné číslo = jiná proměnná (zastínění) */
  return main_prints(vdef("x", 1, false, I, lit_i(1),
                          vdef("x", 2, false, I, lit_i(2),
                               print_stmt(use("x", 1, I), print_stmt(use("x", 2, I), NULL)))));
}
static AST_NODE *build_f3_assign(void) { /* var x = 1; print(x); x = 2; print(x) */
  return main_prints(vdef(
      "x", 1, false, I, lit_i(1),
      print_stmt(use("x", 1, I),
                 assign(use("x", 1, I), lit_i(2), print_stmt(use("x", 1, I), NULL)))));
}
static AST_NODE *build_f3_copy(void) { /* val a = 3; val b = a; print(b) */
  return main_prints(vdef("a", 1, true, I, lit_i(3),
                          vdef("b", 2, true, I, use("a", 1, I), print_stmt(use("b", 2, I), NULL))));
}
static AST_NODE *build_f3_copy_assign(void) { /* b = a; a = 9; hodnota se kopíruje, b zůstane 1 */
  return main_prints(vdef(
      "a", 1, false, I, lit_i(1),
      vdef("b", 2, false, I, lit_i(2),
           assign(use("b", 2, I), use("a", 1, I),
                  assign(use("a", 1, I), lit_i(9),
                         print_stmt(use("a", 1, I), print_stmt(use("b", 2, I), NULL)))))));
}
static AST_NODE *build_f3_add(void) { /* var x = 5; x = x + 1; print(x)  (potřebuje i sčítání z fáze 2) */
  return main_prints(vdef("x", 1, false, I, lit_i(5),
                          assign(use("x", 1, I), add_int(use("x", 1, I), lit_i(1)),
                                 print_stmt(use("x", 1, I), NULL))));
}
static AST_NODE *build_f3_in_helper(void) { /* proměnná i v další funkci, která se nevolá */
  return make_program(
      func("main", true, vdef("m", 1, false, I, lit_i(2), print_stmt(use("m", 1, I), NULL)),
           func("helper", false, vdef("h", 2, false, I, lit_i(1), print_stmt(use("h", 2, I), NULL)),
                NULL)));
}
#undef I

/* fáze 2: výrazy. print(...) vypisuje výsledek; typy uzlů jako by je zapsala sémantika. */
#define I DATA_INT
#define D DATA_DOUBLE
#define S DATA_STRING
#define B DATA_BOOL
#define PR1(e) main_prints(print_stmt((e), NULL))
#define PR2(e1, e2) main_prints(print_stmt((e1), print_stmt((e2), NULL)))
#define PR3(e1, e2, e3) main_prints(print_stmt((e1), print_stmt((e2), print_stmt((e3), NULL))))

/* Int */
static AST_NODE *build_f2_add(void) { return PR1(bin(OP_ADD, lit_i(2), lit_i(3), I)); }
static AST_NODE *build_f2_sub(void) { return PR1(bin(OP_SUB, lit_i(10), lit_i(4), I)); }
static AST_NODE *build_f2_mul(void) { return PR1(bin(OP_MUL, lit_i(6), lit_i(7), I)); }
static AST_NODE *build_f2_div_int(void) { return PR1(bin(OP_DIV, lit_i(7), lit_i(2), I)); }
static AST_NODE *build_f2_div_int_neg(void) { return PR1(bin(OP_DIV, lit_i(-7), lit_i(2), I)); }
/* Double */
static AST_NODE *build_f2_add_double(void) { return PR1(bin(OP_ADD, lit_d(1.5), lit_d(2.0), D)); }
static AST_NODE *build_f2_sub_double(void) { return PR1(bin(OP_SUB, lit_d(6.5), lit_d(2.5), D)); }
static AST_NODE *build_f2_mul_double(void) { return PR1(bin(OP_MUL, lit_d(1.5), lit_d(2.0), D)); }
static AST_NODE *build_f2_div_double(void) { return PR1(bin(OP_DIV, lit_d(9.0), lit_d(2.0), D)); }
/* vnořené výrazy: zásobník musí vyjít */
static AST_NODE *build_f2_nested(void) { /* (2 + 3) * 4 */
  return PR1(bin(OP_MUL, bin(OP_ADD, lit_i(2), lit_i(3), I), lit_i(4), I));
}
static AST_NODE *build_f2_nested_right(void) { /* 2 * (3 + 4) */
  return PR1(bin(OP_MUL, lit_i(2), bin(OP_ADD, lit_i(3), lit_i(4), I), I));
}
static AST_NODE *build_f2_sub_nested(void) { /* 10 - (3 - 2): pravý operand je výraz */
  return PR1(bin(OP_SUB, lit_i(10), bin(OP_SUB, lit_i(3), lit_i(2), I), I));
}
static AST_NODE *build_f2_deep(void) { /* ((1 + 2) * (3 + 4)) - 5 */
  return PR1(bin(OP_SUB,
                 bin(OP_MUL, bin(OP_ADD, lit_i(1), lit_i(2), I), bin(OP_ADD, lit_i(3), lit_i(4), I), I),
                 lit_i(5), I));
}
/* výrazy s proměnnými */
static AST_NODE *build_f2_vars(void) { /* var a = 6; var b = 4; print(a - b) */
  return main_prints(vdef("a", 1, false, I, lit_i(6),
                          vdef("b", 2, false, I, lit_i(4),
                               print_stmt(bin(OP_SUB, use("a", 1, I), use("b", 2, I), I), NULL))));
}
static AST_NODE *build_f2_var_init(void) { /* val a = 2 + 3; print(a): počáteční hodnota je výraz */
  return main_prints(vdef("a", 1, true, I, bin(OP_ADD, lit_i(2), lit_i(3), I),
                          print_stmt(use("a", 1, I), NULL)));
}
static AST_NODE *build_f2_assign_expr(void) { /* var x = 1; x = x * 5 + 2; print(x) */
  return main_prints(vdef(
      "x", 1, false, I, lit_i(1),
      assign(use("x", 1, I),
             bin(OP_ADD, bin(OP_MUL, use("x", 1, I), lit_i(5), I), lit_i(2), I),
             print_stmt(use("x", 1, I), NULL))));
}
static AST_NODE *build_f2_use_twice(void) { /* val a = 4; print(a * a) */
  return main_prints(vdef("a", 1, true, I, lit_i(4),
                          print_stmt(bin(OP_MUL, use("a", 1, I), use("a", 1, I), I), NULL)));
}
/* unární mínus */
static AST_NODE *build_f2_neg(void) { /* val x = 5; print(-x); print(-3) */
  return main_prints(vdef("x", 1, true, I, lit_i(5),
                          print_stmt(neg(use("x", 1, I), I), print_stmt(neg(lit_i(3), I), NULL))));
}
static AST_NODE *build_f2_neg_double(void) { return PR1(neg(lit_d(2.5), D)); }
/* řetězce */
static AST_NODE *build_f2_str(void) { return PR1(bin(OP_ADD, lit_s("ab"), lit_s("cd"), S)); }
static AST_NODE *build_f2_str3(void) { /* ("a" + "b") + "c" */
  return PR1(bin(OP_ADD, bin(OP_ADD, lit_s("a"), lit_s("b"), S), lit_s("c"), S));
}
static AST_NODE *build_f2_str_nested(void) { /* "a" + ("b" + "c"): pomocné proměnné se musí dát znovu použít */
  return PR1(bin(OP_ADD, lit_s("a"), bin(OP_ADD, lit_s("b"), lit_s("c"), S), S));
}
static AST_NODE *build_f2_str_vars(void) { /* val s = "Ahoj "; val t = "svete"; print(s + t) */
  return main_prints(vdef("s", 1, true, S, lit_s("Ahoj "),
                          vdef("t", 2, true, S, lit_s("svete"),
                               print_stmt(bin(OP_ADD, use("s", 1, S), use("t", 2, S), S), NULL))));
}
static AST_NODE *build_f2_str_twice(void) { /* dvě spojení za sebou, DEFVAR pomocných proměnných jen jednou */
  return PR2(bin(OP_ADD, lit_s("ab"), lit_s("cd"), S), bin(OP_ADD, lit_s("e"), lit_s("f"), S));
}
static AST_NODE *build_f2_str_in_helper(void) { /* spojení i v další funkci, která se nevolá */
  return make_program(func(
      "main", true, print_stmt(lit_s("x"), NULL),
      func("helper", false,
           vdef("s", 1, true, S, bin(OP_ADD, lit_s("a"), lit_s("b"), S), print_stmt(use("s", 1, S), NULL)),
           NULL)));
}
/* porovnání: každý případ vypíše dvě až tři hodnoty, aby se poznala i záměna operandů */
static AST_NODE *build_f2_lt(void) {
  return PR2(bin(OP_LT, lit_i(1), lit_i(2), B), bin(OP_LT, lit_i(2), lit_i(1), B));
}
static AST_NODE *build_f2_gt(void) {
  return PR2(bin(OP_GT, lit_i(2), lit_i(1), B), bin(OP_GT, lit_i(1), lit_i(2), B));
}
static AST_NODE *build_f2_eq(void) {
  return PR2(bin(OP_EQ, lit_i(3), lit_i(3), B), bin(OP_EQ, lit_i(3), lit_i(4), B));
}
static AST_NODE *build_f2_neq(void) {
  return PR2(bin(OP_NEQ, lit_i(1), lit_i(2), B), bin(OP_NEQ, lit_i(2), lit_i(2), B));
}
static AST_NODE *build_f2_le(void) {
  return PR3(bin(OP_LE, lit_i(2), lit_i(2), B), bin(OP_LE, lit_i(3), lit_i(2), B),
             bin(OP_LE, lit_i(1), lit_i(2), B));
}
static AST_NODE *build_f2_ge(void) {
  return PR3(bin(OP_GE, lit_i(2), lit_i(2), B), bin(OP_GE, lit_i(1), lit_i(2), B),
             bin(OP_GE, lit_i(3), lit_i(2), B));
}
static AST_NODE *build_f2_cmp_double(void) {
  return PR2(bin(OP_LT, lit_d(1.5), lit_d(2.5), B), bin(OP_LT, lit_d(2.5), lit_d(1.5), B));
}
static AST_NODE *build_f2_cmp_string(void) {
  return PR2(bin(OP_LT, lit_s("a"), lit_s("b"), B), bin(OP_LT, lit_s("b"), lit_s("a"), B));
}
static AST_NODE *build_f2_eq_string(void) {
  return PR2(bin(OP_EQ, lit_s("ab"), lit_s("ab"), B), bin(OP_EQ, lit_s("ab"), lit_s("ac"), B));
}
static AST_NODE *build_f2_cmp_expr(void) { /* (1 + 2) < (2 * 2); (4 * 2) == (3 + 5) */
  return PR2(bin(OP_LT, bin(OP_ADD, lit_i(1), lit_i(2), I), bin(OP_MUL, lit_i(2), lit_i(2), I), B),
             bin(OP_EQ, bin(OP_MUL, lit_i(4), lit_i(2), I), bin(OP_ADD, lit_i(3), lit_i(5), I), B));
}
/* Int a Double dohromady: Int operand se převede na Double hned po vyhodnocení */
static AST_NODE *build_f2_mixed_left(void) { return PR1(bin(OP_ADD, lit_i(1), lit_d(2.5), D)); }
static AST_NODE *build_f2_mixed_right(void) { return PR1(bin(OP_ADD, lit_d(2.5), lit_i(1), D)); }
static AST_NODE *build_f2_mixed_div(void) { /* 7 / 2.0 a 7.0 / 2 */
  return PR2(bin(OP_DIV, lit_i(7), lit_d(2.0), D), bin(OP_DIV, lit_d(7.0), lit_i(2), D));
}
static AST_NODE *build_f2_mixed_cmp(void) { /* 2 < 2.5; 2.5 > 2; 2 == 2.0 */
  return PR3(bin(OP_LT, lit_i(2), lit_d(2.5), B), bin(OP_GT, lit_d(2.5), lit_i(2), B),
             bin(OP_EQ, lit_i(2), lit_d(2.0), B));
}
#undef PR1
#undef PR2
#undef PR3
#undef I
#undef D
#undef S
#undef B

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
    {"f1_control", 1, "main", {NULL}, build_f1_control,
     "řídicí znaky: \\r (kód 13) a kód 1 se píšou jako \\013 a \\001", "a\rb\001c",
     {"string@a\\013b\\001c", NULL}},
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

    {"f2_add", 2, "main", {NULL}, build_f2_add, "print(2 + 3)", "5", {NULL}},
    {"f2_sub", 2, "main", {NULL}, build_f2_sub, "print(10 - 4): levý operand se vyhodnotí první", "6",
     {NULL}},
    {"f2_mul", 2, "main", {NULL}, build_f2_mul, "print(6 * 7)", "42", {NULL}},
    {"f2_div_int", 2, "main", {NULL}, build_f2_div_int, "print(7 / 2): celočíselné dělení Int (IDIVS)",
     "3", {NULL}},
    {"f2_div_int_neg", 2, "main", {NULL}, build_f2_div_int_neg, "print(-7 / 2): zaokrouhlení k nule",
     "-3", {NULL}},
    {"f2_add_double", 2, "main", {NULL}, build_f2_add_double, "print(1.5 + 2.0)", "0x1.cp+1", {NULL}},
    {"f2_sub_double", 2, "main", {NULL}, build_f2_sub_double, "print(6.5 - 2.5)", "0x1p+2", {NULL}},
    {"f2_mul_double", 2, "main", {NULL}, build_f2_mul_double, "print(1.5 * 2.0)", "0x1.8p+1", {NULL}},
    {"f2_div_double", 2, "main", {NULL}, build_f2_div_double, "print(9.0 / 2.0): desetinné dělení (DIVS)",
     "0x1.2p+2", {NULL}},
    {"f2_nested", 2, "main", {NULL}, build_f2_nested, "print((2 + 3) * 4)", "20", {NULL}},
    {"f2_nested_right", 2, "main", {NULL}, build_f2_nested_right, "print(2 * (3 + 4))", "14", {NULL}},
    {"f2_sub_nested", 2, "main", {NULL}, build_f2_sub_nested, "print(10 - (3 - 2)): pravý operand je výraz",
     "9", {NULL}},
    {"f2_deep", 2, "main", {NULL}, build_f2_deep, "print(((1 + 2) * (3 + 4)) - 5)", "16", {NULL}},
    {"f2_vars", 2, "main", {NULL}, build_f2_vars, "var a = 6; var b = 4; print(a - b)", "2",
     {"LF@a$1", "LF@b$2", NULL}},
    {"f2_var_init", 2, "main", {NULL}, build_f2_var_init, "val a = 2 + 3: počáteční hodnota je výraz", "5",
     {NULL}},
    {"f2_assign_expr", 2, "main", {NULL}, build_f2_assign_expr, "var x = 1; x = x * 5 + 2; print(x)", "7",
     {NULL}},
    {"f2_use_twice", 2, "main", {NULL}, build_f2_use_twice, "val a = 4; print(a * a)", "16", {NULL}},
    {"f2_neg", 2, "main", {NULL}, build_f2_neg, "unární mínus: print(-x), print(-3) (UNOP + OP_SUB)", "-5-3",
     {NULL}},
    {"f2_neg_double", 2, "main", {NULL}, build_f2_neg_double, "unární mínus nad Double: print(-2.5)",
     "-0x1.4p+1", {NULL}},
    {"f2_str", 2, "main", {NULL}, build_f2_str, "print(\"ab\" + \"cd\"): spojení řetězců (CONCAT)", "abcd",
     {NULL}},
    {"f2_str3", 2, "main", {NULL}, build_f2_str3, "print((\"a\" + \"b\") + \"c\")", "abc", {NULL}},
    {"f2_str_nested", 2, "main", {NULL}, build_f2_str_nested,
     "print(\"a\" + (\"b\" + \"c\")): pomocné proměnné se použijí podruhé", "abc", {NULL}},
    {"f2_str_vars", 2, "main", {NULL}, build_f2_str_vars, "val s = \"Ahoj \"; val t = \"svete\"; print(s + t)",
     "Ahoj svete", {"string@Ahoj\\032", NULL}},
    {"f2_str_twice", 2, "main", {NULL}, build_f2_str_twice,
     "dvě spojení za sebou (DEFVAR pomocných proměnných jen jednou)", "abcdef", {NULL}},
    {"f2_str_in_helper", 2, "main", {"helper", NULL}, build_f2_str_in_helper,
     "spojení i ve druhé funkci (ta má mít vlastní DEFVAR pomocných proměnných)", "x", {NULL}},
    {"f2_lt", 2, "main", {NULL}, build_f2_lt, "1 < 2 a 2 < 1", "truefalse", {NULL}},
    {"f2_gt", 2, "main", {NULL}, build_f2_gt, "2 > 1 a 1 > 2", "truefalse", {NULL}},
    {"f2_eq", 2, "main", {NULL}, build_f2_eq, "3 == 3 a 3 == 4", "truefalse", {NULL}},
    {"f2_neq", 2, "main", {NULL}, build_f2_neq, "1 != 2 a 2 != 2 (EQS a NOTS)", "truefalse", {NULL}},
    {"f2_le", 2, "main", {NULL}, build_f2_le, "2 <= 2, 3 <= 2, 1 <= 2 (GTS a NOTS)", "truefalsetrue",
     {NULL}},
    {"f2_ge", 2, "main", {NULL}, build_f2_ge, "2 >= 2, 1 >= 2, 3 >= 2 (LTS a NOTS)", "truefalsetrue",
     {NULL}},
    {"f2_cmp_double", 2, "main", {NULL}, build_f2_cmp_double, "porovnání Double: 1.5 < 2.5 a 2.5 < 1.5",
     "truefalse", {NULL}},
    {"f2_cmp_string", 2, "main", {NULL}, build_f2_cmp_string, "porovnání String: \"a\" < \"b\" a \"b\" < \"a\"",
     "truefalse", {NULL}},
    {"f2_eq_string", 2, "main", {NULL}, build_f2_eq_string, "rovnost String: \"ab\" == \"ab\" a \"ab\" == \"ac\"",
     "truefalse", {NULL}},
    {"f2_cmp_expr", 2, "main", {NULL}, build_f2_cmp_expr, "porovnání výrazů: (1 + 2) < (2 * 2), (4 * 2) == (3 + 5)",
     "truetrue", {NULL}},
    {"f2_mixed_left", 2, "main", {NULL}, build_f2_mixed_left,
     "1 + 2.5: levý operand je Int (INT2FLOATS hned po něm)", "0x1.cp+1", {NULL}},
    {"f2_mixed_right", 2, "main", {NULL}, build_f2_mixed_right,
     "2.5 + 1: pravý operand je Int (INT2FLOATS po něm)", "0x1.cp+1", {NULL}},
    {"f2_mixed_div", 2, "main", {NULL}, build_f2_mixed_div, "7 / 2.0 a 7.0 / 2: dělení Int a Double",
     "0x1.cp+10x1.cp+1", {NULL}},
    {"f2_mixed_cmp", 2, "main", {NULL}, build_f2_mixed_cmp, "2 < 2.5, 2.5 > 2, 2 == 2.0: Int a Double",
     "truetruetrue", {NULL}},

    {"f3_var", 3, "main", {NULL}, build_f3_var, "var x = 5; print(x)", "5", {"LF@x$1", NULL}},
    {"f3_val", 3, "main", {NULL}, build_f3_val, "val x = 7; print(x)", "7", {NULL}},
    {"f3_types", 3, "main", {NULL}, build_f3_types, "String, Double a Boolean v proměnných",
     "Ahoj\n0x1.cp+1true", {NULL}},
    {"f3_mixed", 3, "main", {NULL}, build_f3_mixed,
     "print, pak deklarace uprostřed těla, DEFVAR musí být i tak nahoře", "x=5\n", {NULL}},
    {"f3_two", 3, "main", {NULL}, build_f3_two, "dvě deklarace za sebou (druhá se nesmí přeskočit)",
     "12", {"LF@a$1", "LF@b$2", NULL}},
    {"f3_many", 3, "main", {NULL}, build_f3_many, "pět deklarací za sebou", "12345",
     {"LF@c$3", "LF@e$5", NULL}},
    {"f3_same_name", 3, "main", {NULL}, build_f3_same_name,
     "dvě proměnné stejného jména x s čísly 1 a 2 (zastínění)", "12", {"LF@x$1", "LF@x$2", NULL}},
    {"f3_assign", 3, "main", {NULL}, build_f3_assign, "var x = 1; print(x); x = 2; print(x)", "12",
     {NULL}},
    {"f3_copy", 3, "main", {NULL}, build_f3_copy, "val b = a (proměnná jako počáteční hodnota)", "3",
     {NULL}},
    {"f3_copy_assign", 3, "main", {NULL}, build_f3_copy_assign,
     "b = a; a = 9: kopíruje se hodnota, b zůstane 1", "91", {NULL}},
    {"f3_add", 3, "main", {NULL}, build_f3_add, "x = x + 1 (potřebuje i sčítání z fáze 2)", "6",
     {NULL}},
    {"f3_in_helper", 3, "main", {"helper", NULL}, build_f3_in_helper,
     "proměnná i ve druhé funkci (ta má mít vlastní DEFVAR)", "2", {"LF@m$1", "LF@h$2", NULL}},
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
    {"ADDS", 0},        {"SUBS", 0},      {"MULS", 0},
    {"IDIVS", 0},       {"DIVS", 0},      {"LTS", 0},      {"GTS", 0},    {"EQS", 0},
    {"NOTS", 0},        {"INT2FLOATS", 0}, {"CONCAT", 3},
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
/*  Simulátor IFJcode26 (instrukce pro fáze 0 až 3)                          */
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
    } else if ((strcmp(op, "ADDS") == 0 || strcmp(op, "SUBS") == 0 || strcmp(op, "MULS") == 0) &&
               nw == 1) {
      if (vm->nst < 2) {
        vm_fail(vm, "řádek %d: %s potřebuje dvě hodnoty na zásobníku (chyba 56)", ln, op);
        return;
      }
      Val b = vm->st[--vm->nst], a = vm->st[--vm->nst], r;
      memset(&r, 0, sizeof r);
      unsigned long long ua = (unsigned long long)a.i, ub = (unsigned long long)b.i;
      if (a.t == V_INT && b.t == V_INT) {
        r.t = V_INT;
        r.i = (long long)(op[0] == 'A' ? ua + ub : op[0] == 'S' ? ua - ub : ua * ub);
      } else if (a.t == V_FLOAT && b.t == V_FLOAT) {
        r.t = V_FLOAT;
        r.d = op[0] == 'A' ? a.d + b.d : op[0] == 'S' ? a.d - b.d : a.d * b.d;
      } else {
        vm_fail(vm, "řádek %d: %s má operandy různého nebo nevhodného typu (chyba 53)", ln, op);
        return;
      }
      vm->st[vm->nst++] = r;
    } else if ((strcmp(op, "IDIVS") == 0 || strcmp(op, "DIVS") == 0) && nw == 1) {
      if (vm->nst < 2) {
        vm_fail(vm, "řádek %d: %s potřebuje dvě hodnoty na zásobníku (chyba 56)", ln, op);
        return;
      }
      Val b = vm->st[--vm->nst], a = vm->st[--vm->nst], r;
      memset(&r, 0, sizeof r);
      if (op[0] == 'I') {
        if (a.t != V_INT || b.t != V_INT) {
          vm_fail(vm, "řádek %d: IDIVS chce dvě hodnoty typu int, jinak chyba 53 (pro Double je DIVS)", ln);
          return;
        }
        if (b.i == 0) {
          vm_fail(vm, "řádek %d: dělení nulou (chyba 57)", ln);
          return;
        }
        r.t = V_INT;
        r.i = (b.i == -1) ? (long long)(0ULL - (unsigned long long)a.i) : a.i / b.i; /* k nule */
      } else {
        if (a.t != V_FLOAT || b.t != V_FLOAT) {
          vm_fail(vm, "řádek %d: DIVS chce dvě hodnoty typu float, jinak chyba 53 (pro Int je IDIVS, "
                      "pro smíšené Int a Double je třeba před dělením INT2FLOATS)", ln);
          return;
        }
        r.t = V_FLOAT;
        r.d = a.d / b.d;
      }
      vm->st[vm->nst++] = r;
    } else if ((strcmp(op, "LTS") == 0 || strcmp(op, "GTS") == 0 || strcmp(op, "EQS") == 0) && nw == 1) {
      if (vm->nst < 2) {
        vm_fail(vm, "řádek %d: %s potřebuje dvě hodnoty na zásobníku (chyba 56)", ln, op);
        return;
      }
      Val b = vm->st[--vm->nst], a = vm->st[--vm->nst], r;
      memset(&r, 0, sizeof r);
      r.t = V_BOOL;
      bool eq_op = op[0] == 'E';
      if (a.t == V_NIL || b.t == V_NIL) {
        if (!eq_op) {
          vm_fail(vm, "řádek %d: %s s operandem nil (chyba 53)", ln, op);
          return;
        }
        r.i = (a.t == b.t);
      } else if (a.t != b.t) {
        vm_fail(vm, "řádek %d: %s má operandy různého typu (chyba 53). U smíšeného Int a Double je "
                    "třeba Int nejdřív převést (INT2FLOATS)", ln, op);
        return;
      } else {
        int cmp = 0;
        if (a.t == V_INT || a.t == V_BOOL)
          cmp = (a.i > b.i) - (a.i < b.i);
        else if (a.t == V_FLOAT)
          cmp = (a.d > b.d) - (a.d < b.d);
        else if (a.t == V_STR) {
          size_t m = a.n < b.n ? a.n : b.n;
          cmp = memcmp(a.s, b.s, m);
          if (cmp == 0) cmp = (a.n > b.n) - (a.n < b.n);
        }
        r.i = op[0] == 'L' ? cmp < 0 : op[0] == 'G' ? cmp > 0 : cmp == 0;
      }
      vm->st[vm->nst++] = r;
    } else if (strcmp(op, "NOTS") == 0 && nw == 1) {
      if (vm->nst < 1) {
        vm_fail(vm, "řádek %d: NOTS potřebuje hodnotu na zásobníku (chyba 56)", ln);
        return;
      }
      Val *a = &vm->st[vm->nst - 1];
      if (a->t != V_BOOL) {
        vm_fail(vm, "řádek %d: NOTS chce hodnotu typu bool (chyba 53)", ln);
        return;
      }
      a->i = !a->i;
    } else if (strcmp(op, "INT2FLOATS") == 0 && nw == 1) {
      if (vm->nst < 1) {
        vm_fail(vm, "řádek %d: INT2FLOATS potřebuje hodnotu na zásobníku (chyba 56)", ln);
        return;
      }
      Val *a = &vm->st[vm->nst - 1];
      if (a->t != V_INT) {
        vm_fail(vm, "řádek %d: INT2FLOATS chce hodnotu typu int (chyba 53); převádí se jen Int operand, "
                    "Double už převedený je", ln);
        return;
      }
      a->t = V_FLOAT;
      a->d = (double)a->i;
    } else if (strcmp(op, "CONCAT") == 0 && nw == 4) {
      Val x, y;
      if (!read_symb(vm, w[2], ln, &x) || !read_symb(vm, w[3], ln, &y)) return;
      if (x.t != V_STR || y.t != V_STR) {
        vm_fail(vm, "řádek %d: CONCAT chce dva řetězce (chyba 53)", ln);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      char *buf = vm_alloc(vm, x.n + y.n + 1);
      memcpy(buf, x.s, x.n);
      memcpy(buf + x.n, y.s, y.n);
      v->v.t = V_STR;
      v->v.s = buf;
      v->v.n = x.n + y.n;
      v->init = true;
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

  /* proměnné: DEFVAR jen na začátku funkce a každá použitá proměnná má DEFVAR ve stejné funkci */
  {
    char defs[64][64];
    char refs[128][64];
    int ref_line[128];
    int ndefs = 0, nrefs = 0;
    bool in_fn = false, past_defs = false, late_reported = false;
    for (size_t i = 1; i <= n; i++) {
      char buf[1024], *w[8];
      int nw = (i < n) ? split_words(lines[i], buf, sizeof buf, w) : 0;
      bool is_label = nw >= 1 && strcmp(w[0], "LABEL") == 0;
      if (i == n || is_label) {
        if (in_fn) {
          for (int r = 0; r < nrefs; r++) {
            bool found = false;
            for (int d = 0; d < ndefs; d++)
              if (strcmp(refs[r], defs[d]) == 0) found = true;
            if (!found) {
              problem("řádek %d: proměnná %s se používá, ale ve stejné funkci pro ni není DEFVAR "
                      "(interpret: chyba 54)", ref_line[r], refs[r]);
              break;
            }
          }
        }
        in_fn = is_label;
        past_defs = late_reported = false;
        ndefs = nrefs = 0;
        continue;
      }
      if (!in_fn || nw == 0) continue;
      if (strcmp(w[0], "DEFVAR") == 0 && nw == 2) {
        if (past_defs && !late_reported) {
          problem("řádek %d: DEFVAR %s je až za jinou instrukcí těla. Všechny DEFVAR mají být na "
                  "začátku funkce, jinak se v cyklu provedou víckrát (chyba 52)", (int)i + 1, w[1]);
          late_reported = true;
        }
        if (ndefs < 64 && strncmp(w[1], "LF@", 3) == 0) snprintf(defs[ndefs++], 64, "%s", w[1]);
        continue;
      }
      if (strcmp(w[0], "CREATEFRAME") != 0 && strcmp(w[0], "PUSHFRAME") != 0) past_defs = true;
      for (int t = 1; t < nw; t++)
        if (strncmp(w[t], "LF@", 3) == 0 && nrefs < 128) {
          snprintf(refs[nrefs], 64, "%s", w[t]);
          ref_line[nrefs++] = (int)i + 1;
        }
    }
  }

  /* každá funkce musí končit RETURN, i když má prázdné tělo (jinak se spadne do další funkce) */
  {
    int n_ret = 0;
    for (size_t i = 1; i < n; i++)
      if (strcmp(lines[i], "RETURN") == 0) n_ret++;
    if (n_ret < nf)
      problem("funkcí je %d, ale RETURN jen %d. Funkce s prázdným tělem se musí také vrátit "
              "(PUSHS nil@nil, POPFRAME, RETURN), jinak program propadne do další funkce", nf, n_ret);
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
                                "../../ic26int-linux-x86_64",
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
  int phase_ok[4] = {0}, phase_bad[4] = {0};
  static const char *const PHASE_NAME[4] = {"kostra programu a funkcí", "konstanty a print",
                                           "výrazy", "proměnné"};
  for (int phase = 0; phase <= 3; phase++) {
    bool header = false;
    for (size_t i = 0; i < N_CASES; i++) {
      if (CASES[i].phase != phase || !selected(&CASES[i], only)) continue;
      if (!header) {
        printf("== Fáze %d: %s\n", phase, PHASE_NAME[phase]);
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
    printf("neznámý případ \"%s\". Dostupné: f0, f1, f2, f3 a", only);
    for (size_t i = 0; i < N_CASES; i++) printf(" %s", CASES[i].name);
    printf("\n");
    return 2;
  }

  printf("== Výsledek: OK %d, chyby %d", total_ok, total_bad);
  for (int phase = 0; phase <= 3; phase++)
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
