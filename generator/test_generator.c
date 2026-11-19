/*
 * Projekt: IFJ26 – překladač jazyka IFJ26 do IFJcode26
 * Soubor:  test_generator.c – test generátoru kódu (fáze 0: kostra, 1: konstanty a print,
 *                            2: výrazy, 3: proměnné, 4: if a while, 5: funkce,
 *                            6: vestavěné funkce, 7: rozšíření BOOLTHEN, 8: rozšíření CONTROL – cyklus for a if jako výraz)
 *
 * ---------------------------------------------------------------------------
 * Tento soubor vygenerovala AI: Claude Sonnet 5.5 (Anthropic, claude-sonnet-5-5), 2026-11-17.
 * Prompt 1: "Mám hotovou fázi nula, chci, abys vytvořil nějaké testy, se kterými to můžu
 *            vyzkoušet. Nemáme parser ani sémantickou analýzu, chci otestovat jenom můj
 *            generátor kódu s AST. Udělej mi testovací skript."
 * Prompt 2: "vytvoř mi test na fázi 1" (konstanty a print: Int, Double, String, Boolean, ())
 * Prompt 3: "udelej testy pro fazi 3" (proměnné: val, var, přiřazení, použití proměnné)
 * Prompt 4: "vytvor test pro fazi 2" (výrazy: + - * /, řetězce, porovnání, Int a Double)
 * Prompt 5: "udelej test na fazi 4" (if, else if, else, while a jejich kombinace)
 * Prompt 6: "delej test pro fazi 5" (funkce: volání, parametry, return, rekurze)
 * Prompt 7: "chci abys mi builtins udelal tz" (fáze 6: vestavěné funkce, vstup, běhové chyby)
 * Prompt 8: "udelej testy pro booltehn" (fáze 7: Boolean, !, && a || se zkratem, readBoolean)
 * Prompt 9: "udelej test na for loop" (fáze 8: for s to / until / by, stráže, vnořené cykly)
 * Prompt 10: "uprav test pro cele rozsireni control odendej 4 testy ktere ted nevychazi" (fáze 8 je celé
 *            CONTROL: for + if jako výraz; testy se záporným krokem odebrány)
 * Zadal a zrevidoval: Pavel Novák (xnovakp01)
 * ---------------------------------------------------------------------------
 *
 * CO TEST DĚLÁ
 *   Postaví ručně malý AST (bez parseru a sémantiky), zavolá generator_init(), zkontroluje
 *   tvar vygenerovaného IFJcode26 a pak spočítá, co by program vypsal, a porovná to s tím,
 *   co vypsat má. Výpis počítá malý simulátor přímo v tomto souboru (umí instrukce, které
 *   fáze 0 až 6 potřebují), takže test funguje i bez interpretu ic26int. Pokud ic26int najde
 *   a jde spustit, pustí v něm kód ještě jednou a porovná výstup také.
 *   Každý případ běží ve vlastním procesu, takže pád generátoru shodí jen ten jeden případ.
 *
 * PŘEKLAD (z adresáře src/generator/; ast.c a generátor se linkují normálně, bez unity buildu):
 *
 *     gcc -std=gnu11 -g -O0 -Wall -Wextra -I.. test_generator.c ast.c generator.c gen_*.c \
 *         -o test_generator && ./test_generator
 *
 * POUŽITÍ
 *     ./test_generator              všechny případy (fáze 0 až 8)
 *     ./test_generator f8           jen fáze 8 (f0 až f7 = jen ta fáze)
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
 *   Fáze 4 (if a while): tvar uzlů je podle parseru (parse_if_then, parse_while_stmt, parse_block):
 *   IF: left = podmínka, right = větev then, third = větev else (NULL, když chybí), WHILE: left =
 *   podmínka, right = tělo. Větev i tělo jsou řetěz příkazů přes next. U "else if" je third rovnou
 *   jeden uzel NODE_IF (bez řetězu). Jednořádkový zápis (if c then a else b, kde větve jsou holé
 *   výrazy) tenhle test nezkouší.
 *   - podmínka se vyhodnotí na zásobník a skok si hodnotu vezme: PUSHS bool@false + JUMPIFEQS
 *     (simulátor umí JUMPIFEQS, JUMPIFNEQS a taky JUMPIFEQ a JUMPIFNEQ s operandy). Po skoku
 *     nesmí na zásobníku nic zůstat, kontrola na konci programu hlídá i smyčku (kdyby v každém
 *     průchodu něco zůstalo, na konci by na zásobníku byla hromada hodnot)
 *   - každé if a každé while má vlastní návěští a žádné se v celém programu neopakuje, ani mezi
 *     funkcemi (číslo z new_label je jedno počítadlo pro celý program, to stačí)
 *   - každý skok míří na existující návěští, i tam, kam se za běhu nedojde
 *   - DEFVAR proměnných deklarovaných uvnitř větví a těl cyklů je na začátku funkce (v cyklu by se
 *     provedl podruhé a interpret by skončil chybou 52). Vnitřní návěští (if, while) test nepočítá
 *     za začátek další funkce: funkce se pozná podle LABEL, za kterým je hned CREATEFRAME
 *   Fáze 5 (funkce): tvar uzlů je podle komentářů v ast.h (parser výrazy a volání zatím jen přeskakuje,
 *   takže jejich tvar z něj není vidět):
 *   FUNC_DEF: left = tělo (řetěz příkazů přes next), third = parametry (řetěz NODE_PARAM se jménem,
 *   typem a číslem id), data_type = návratový typ. CALL: name, argumenty ve third (řetěz přes next,
 *   zleva doprava), data_type = typ výsledku. Volání jako příkaz: NODE_EXPR_STMT s voláním v left.
 *   RETURN: left = výraz, NULL u prázdného return. Návratová hodnota bez return: poslední příkaz těla
 *   je NODE_EXPR_STMT s výrazem (konvence je ve funkci flast(), uprav ji tam, jestli to máte jinak).
 *   - volající vyhodnotí argumenty zleva doprava (gen_expr na každý), potom CALL $f_jmeno; výsledek
 *     zůstane na zásobníku (výraz nechá jednu hodnotu). Volání jako příkaz výsledek zahodí (POPS)
 *   - volaná funkce: DEFVAR parametrů a lokálních proměnných na začátku, potom POPS parametrů od
 *     posledního k prvnímu (poslední argument je navrchu). Jméno parametru je jmeno$id jako u proměnné
 *   - výsledek funkce je vždy jedna hodnota na zásobníku: u Unit nil@nil, jinak hodnota z return nebo
 *     poslední výraz těla. return uprostřed funkce (i v if nebo while) musí taky nechat jednu hodnotu
 *     a hned POPFRAME a RETURN. Prázdný return vrací nil@nil
 *   - proměnné volaného a volajícího se nemíchají (každá funkce má vlastní rámec), rekurze má v každém
 *     volání vlastní hodnoty
 *   Fáze 7 (rozšíření BOOLTHEN): && je NODE_BINOP s OP_AND, || je NODE_BINOP s OP_OR, ! je
 *   NODE_UNOP s OP_NOT (operand v left); všechny mají data_type DATA_BOOL. Test kontroluje:
 *   - pravdivostní tabulky && a || (všechny čtyři kombinace), !, !!, == a != nad Boolean
 *   - Boolean v proměnných (val, var, přiřazení), parametrech, návratových hodnotách (return i
 *     poslední výraz těla) a v podmínkách if, else if, while (if bez else má třetí potomek NULL)
 *   - ZKRAT: && a || se vyhodnocují zleva doprava a pravý operand se nevyhodnotí, když už je
 *     výsledek jasný. Test to pozná tak, že pravý operand je celočíselné dělení nulou
 *     (b != 0 && 10 / b > 1 s b = 0): bez zkratu skončí program chybou 57 místo vypsání false.
 *     Případy *_funexp používají totéž s voláním funkce s vedlejším účinkem (rozšíření FUNEXP)
 *   - vnořené && a || (zleva i zprava) a víc && a || v jedné funkci: každé návěští jen jednou
 *   - readBoolean: READ bool vrací true jen pro řádek "true", false jen pro "false"
 *   - toStr(Boolean) je "true" nebo "false"
 *   Fáze 6 (vestavěné funkce): uzel NODE_CALL se jménem funkce (readInt, readDouble, readLine, toStr,
 *   toInt, strLength, subStr, ord, chr), argumenty ve third (řetěz přes next), data_type = typ výsledku.
 *   Typ argumentu toStr je v data_type argumentu (Int, Double nebo String). V základním zadání
 *   smí být volání jen jako příkaz nebo vpravo od val, var a =; případy *_nested používají volání
 *   uvnitř jiného volání (rozšíření FUNEXP) a generátor je zvládne sám od sebe: výraz nechá jednu
 *   hodnotu na zásobníku. Test je černá skříňka: neřeší, jak vestavěné funkce vypadají v kódu
 *   (volání $b_... jako u uživatelských funkcí, nebo rozepsané přímo), kontroluje jen to, co program
 *   vypíše a s jakým kódem skončí:
 *   - print vypisuje bez nového řádku, Double hexadecimálně (%a), toStr(Double) vždy jako %.2f
 *   - readInt a readDouble vrací 0, když je řádek chybný nebo je konec vstupu (instrukce READ
 *     v těch případech vrací nil, generátor ho musí převést); okolní bílé znaky se neignorují
 *   - readLine vrací řádek včetně znaku nového řádku, na konci vstupu prázdný řetězec
 *   - subStr(s, start, end) skončí kódem 40, když start < 0, end < 0, start > end, start >= délka
 *     nebo end > délka; ord vrací -1 pro index mimo řetězec nebo prázdný řetězec; chr(code) mimo
 *     0 až 127 skončí chybou interpretu 58; celočíselné dělení nulou skončí kódem 57
 *   - vestavěná funkce se v kódu nesmí definovat víckrát (stejné pravidlo jako u LABEL funkcí) a
 *     její definice jsou až za EXIT; vynechání výsledku volání (volání jako příkaz) nic nezanechá
 *   - na rozdíl od fází 0 až 4 se v téhle fázi nekontroluje, že před POPFRAME je PUSHS nil@nil (funkce
 *     s hodnotou ho mít nesmí), a CALL na jiné funkce než @main je povolený. POPS parametrů hned za
 *     DEFVAR na začátku funkce se nepočítá jako tělo (DEFVAR za ním je pořád v pořádku)
 *
 * KDE UPRAVIT, KDYŽ SE ZMĚNÍ ROZHRANÍ
 *   make_program() – jak se skládá NODE_PROGRAM (teď: left = první funkce, další přes next)
 *   make_body()    – jak se skládá tělo funkce (teď: rovnou první příkaz, další příkazy přes
 *                    next, bez uzlu NODE_BLOCK; takhle to staví parser, viz parse_block)
 *   use(), vdef(), assign() – jak se v AST staví proměnná, deklarace a přiřazení (teď: NODE_IDENT
 *                    se jménem a číslem id, NODE_VAR_DEF s hodnotou v left, NODE_ASSIGN: left =
 *                    proměnná, right = výraz). Jméno proměnné v kódu je jmeno$id, např. LF@x$1
 *   fdef(), par(), fcall(), fcall_stmt(), fret(), flast() – jak se staví funkce, parametr, volání,
 *                    volání jako příkaz, return a poslední výraz těla (fáze 5)
 *   if_stmt(), while_stmt() – jak se staví if a while (ast_if_create: cond, then, else, next;
 *                    ast_while_create: cond, body, next)
 *   print_stmt()   – jak vypadá print (teď: NODE_EXPR_STMT → NODE_CALL "print" → argument
 *                    v third)
 *   run_generator() – jak se volá generátor (teď: Gen gen = {0}; generator_init(&gen, root))
 *   bin(), neg()   – jak se staví binární a unární operátor (BINOP: left, right, op_type;
 *                    UNOP: left = operand, op_type OP_SUB)
 *   CASES[]        – další případy; pro další fáze přidávej nové případy do tohoto souboru
 */

#define _POSIX_C_SOURCE 200809L

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
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

/* if cond then <then> else <els>; další příkaz přes next. Podle parseru je podmínka v left, větev
 * then v right a větev else ve third. Větev je řetěz příkazů přes next (parse_block); u "else if"
 * je third rovnou uzel NODE_IF; když else chybí, je third NULL. */
static AST_NODE *if_stmt(AST_NODE *cond, AST_NODE *then, AST_NODE *els, AST_NODE *next) {
  return ast_if_create(cond, then, els, next);
}

/* while cond do <body>: podmínka v left, tělo (řetěz příkazů) v right */
static AST_NODE *while_stmt(AST_NODE *cond, AST_NODE *body, AST_NODE *next) {
  return ast_while_create(cond, body, next);
}

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

/* fáze 4: if / else if / else a while. Typy uzlů a čísla proměnných jako by je zapsala sémantika. */
#define I DATA_INT
#define D DATA_DOUBLE
#define S DATA_STRING
#define B DATA_BOOL
#define PS(str, nx) print_stmt(lit_s(str), (nx))
#define UI(name, id) use((name), (id), I)

/* i = i + 1 (jméno a číslo proměnné se zadá) */
static AST_NODE *inc(const char *name, int id, AST_NODE *next) {
  return assign(use(name, id, I), add_int(use(name, id, I), lit_i(1)), next);
}

static AST_NODE *build_f4_if_true(void) {
  return main_prints(if_stmt(lit_b(true), PS("A", NULL), NULL, NULL));
}
static AST_NODE *build_f4_if_false(void) { /* větev se přeskočí, za ifem se pokračuje */
  return main_prints(if_stmt(lit_b(false), PS("A", NULL), NULL, PS("Z", NULL)));
}
static AST_NODE *build_f4_if_else_true(void) {
  return main_prints(if_stmt(lit_b(true), PS("A", NULL), PS("B", NULL), NULL));
}
static AST_NODE *build_f4_if_else_false(void) {
  return main_prints(if_stmt(lit_b(false), PS("A", NULL), PS("B", NULL), NULL));
}
static AST_NODE *build_f4_if_cmp(void) { /* podmínka je porovnání, před ifem a za ním je příkaz */
  return main_prints(PS("<", if_stmt(bin(OP_LT, lit_i(1), lit_i(2), B), PS("A", NULL),
                                     PS("B", NULL), PS(">", NULL))));
}
static AST_NODE *build_f4_if_cmp_else(void) {
  return main_prints(PS("<", if_stmt(bin(OP_LT, lit_i(2), lit_i(1), B), PS("A", NULL),
                                     PS("B", NULL), PS(">", NULL))));
}
static AST_NODE *build_f4_if_multi(void) { /* ve větvích je víc příkazů za sebou (řetěz přes next) */
  return main_prints(if_stmt(lit_b(true), print_stmt(lit_i(1), print_stmt(lit_i(2), NULL)),
                             print_stmt(lit_i(3), print_stmt(lit_i(4), NULL)), NULL));
}
static AST_NODE *build_f4_if_multi_else(void) {
  return main_prints(if_stmt(lit_b(false), print_stmt(lit_i(1), print_stmt(lit_i(2), NULL)),
                             print_stmt(lit_i(3), print_stmt(lit_i(4), NULL)), NULL));
}
static AST_NODE *build_f4_if_two_ifs(void) { /* dvě if za sebou: návěští se nesmí opakovat */
  return main_prints(if_stmt(lit_b(true), PS("A", NULL), PS("B", NULL),
                             if_stmt(lit_b(false), PS("C", NULL), PS("D", NULL), NULL)));
}
static AST_NODE *build_f4_if_three(void) { /* tři if bez else za sebou */
  return main_prints(if_stmt(
      lit_b(true), PS("A", NULL), NULL,
      if_stmt(lit_b(true), PS("B", NULL), NULL, if_stmt(lit_b(false), PS("C", NULL), NULL, NULL))));
}
static AST_NODE *build_f4_if_nested(void) { /* if v then větvi */
  return main_prints(if_stmt(lit_b(true), if_stmt(lit_b(false), PS("X", NULL), PS("Y", NULL), NULL),
                             PS("Z", NULL), NULL));
}
static AST_NODE *build_f4_if_nested_else(void) { /* if v else větvi, za ním další příkaz ve větvi */
  return main_prints(if_stmt(
      lit_b(false), PS("X", NULL),
      if_stmt(lit_b(true), PS("Y", NULL), PS("Z", NULL), PS("!", NULL)), NULL));
}
static AST_NODE *build_f4_if_nested_deep(void) { /* tři if do sebe, za vnějším další příkaz */
  return main_prints(if_stmt(
      lit_b(true),
      if_stmt(lit_b(true), if_stmt(lit_b(true), PS("A", NULL), NULL, NULL), NULL, NULL), NULL,
      PS("B", NULL)));
}
static AST_NODE *build_f4_if_nested_both(void) { /* if jak v then, tak v else větvi */
  return main_prints(if_stmt(lit_b(true),
                             if_stmt(lit_b(true), PS("A", NULL), PS("B", NULL), NULL),
                             if_stmt(lit_b(true), PS("C", NULL), PS("D", NULL), NULL), NULL));
}
static AST_NODE *build_f4_else_if(void) { /* if false A else if true B else C */
  return main_prints(if_stmt(lit_b(false), PS("A", NULL),
                             if_stmt(lit_b(true), PS("B", NULL), PS("C", NULL), NULL), NULL));
}
static AST_NODE *build_f4_else_if_last(void) { /* dojde se až na poslední else */
  return main_prints(if_stmt(lit_b(false), PS("A", NULL),
                             if_stmt(lit_b(false), PS("B", NULL), PS("C", NULL), NULL), NULL));
}
static AST_NODE *build_f4_else_if_first(void) { /* splněná první podmínka, else if se přeskočí */
  return main_prints(if_stmt(lit_b(true), PS("A", NULL),
                             if_stmt(lit_b(true), PS("B", NULL), PS("C", NULL), NULL), NULL));
}
static AST_NODE *build_f4_else_if_chain(void) { /* var x = 2; x == 1 / x == 2 / x == 3 / else */
  return main_prints(vdef(
      "x", 1, false, I, lit_i(2),
      if_stmt(bin(OP_EQ, UI("x", 1), lit_i(1), B), PS("A", NULL),
              if_stmt(bin(OP_EQ, UI("x", 1), lit_i(2), B), PS("B", NULL),
                      if_stmt(bin(OP_EQ, UI("x", 1), lit_i(3), B), PS("C", NULL), PS("D", NULL),
                              NULL),
                      NULL),
              NULL)));
}
static AST_NODE *build_f4_else_if_no_else(void) { /* else if bez posledního else, nic neplatí */
  return main_prints(if_stmt(lit_b(false), PS("A", NULL),
                             if_stmt(lit_b(false), PS("B", NULL), NULL, NULL), PS("Z", NULL)));
}
static AST_NODE *if_var_prog(long long v) { /* var x = v; if x > 3 { big } else { small } */
  return main_prints(vdef("x", 1, false, I, lit_i(v),
                          if_stmt(bin(OP_GT, UI("x", 1), lit_i(3), B), PS("big", NULL),
                                  PS("small", NULL), NULL)));
}
static AST_NODE *build_f4_if_var(void) { return if_var_prog(5); }
static AST_NODE *build_f4_if_var_else(void) { return if_var_prog(1); }
static AST_NODE *build_f4_if_assign(void) { /* var x = 1; if true { x = 2 }; print(x) */
  return main_prints(vdef("x", 1, false, I, lit_i(1),
                          if_stmt(lit_b(true), assign(UI("x", 1), lit_i(2), NULL), NULL,
                                  print_stmt(UI("x", 1), NULL))));
}
static AST_NODE *build_f4_if_assign_skip(void) { /* větev se nevykoná, x zůstane 1 */
  return main_prints(vdef("x", 1, false, I, lit_i(1),
                          if_stmt(lit_b(false), assign(UI("x", 1), lit_i(3), NULL), NULL,
                                  print_stmt(UI("x", 1), NULL))));
}
static AST_NODE *build_f4_if_assign_else(void) { /* if 2 < 1 { x = 1 } else { x = 2 } */
  return main_prints(vdef("x", 1, false, I, lit_i(0),
                          if_stmt(bin(OP_LT, lit_i(2), lit_i(1), B), assign(UI("x", 1), lit_i(1), NULL),
                                  assign(UI("x", 1), lit_i(2), NULL), print_stmt(UI("x", 1), NULL))));
}
static AST_NODE *build_f4_if_var_in_branch(void) { /* proměnné deklarované uvnitř větví */
  return main_prints(if_stmt(
      lit_b(true), vdef("y", 1, true, I, lit_i(4), print_stmt(UI("y", 1), NULL)),
      vdef("z", 2, true, I, lit_i(5), print_stmt(UI("z", 2), NULL)), NULL));
}
static AST_NODE *build_f4_if_cond_expr(void) { /* (1 + 2) == 3 a 2 * 3 > 7 */
  return main_prints(if_stmt(
      bin(OP_EQ, bin(OP_ADD, lit_i(1), lit_i(2), I), lit_i(3), B), PS("A", NULL), PS("B", NULL),
      if_stmt(bin(OP_GT, bin(OP_MUL, lit_i(2), lit_i(3), I), lit_i(7), B), PS("C", NULL),
              PS("D", NULL), NULL)));
}
static AST_NODE *build_f4_if_bool_var(void) { /* val b = true; if b */
  return main_prints(vdef("b", 1, true, B, lit_b(true),
                          if_stmt(use("b", 1, B), PS("A", NULL), PS("B", NULL), NULL)));
}
static AST_NODE *build_f4_if_bool_var_cmp(void) { /* val b = 1 < 0; if b */
  return main_prints(vdef("b", 1, true, B, bin(OP_LT, lit_i(1), lit_i(0), B),
                          if_stmt(use("b", 1, B), PS("A", NULL), PS("B", NULL), NULL)));
}
static AST_NODE *build_f4_if_cmp_ops(void) { /* <=, >=, != v podmínce */
  return main_prints(if_stmt(
      bin(OP_LE, lit_i(2), lit_i(2), B), PS("A", NULL), PS("B", NULL),
      if_stmt(bin(OP_GE, lit_i(3), lit_i(4), B), PS("C", NULL), PS("D", NULL),
              if_stmt(bin(OP_NEQ, lit_i(1), lit_i(1), B), PS("E", NULL), PS("F", NULL), NULL))));
}
static AST_NODE *build_f4_if_string_cmp(void) {
  return main_prints(if_stmt(
      bin(OP_LT, lit_s("a"), lit_s("b"), B), PS("A", NULL), PS("B", NULL),
      if_stmt(bin(OP_EQ, lit_s("ab"), lit_s("ac"), B), PS("C", NULL), PS("D", NULL), NULL)));
}
static AST_NODE *build_f4_if_mixed(void) { /* 2 < 2.5: Int a Double v podmínce (INT2FLOATS) */
  return main_prints(if_stmt(bin(OP_LT, lit_i(2), lit_d(2.5), B), PS("A", NULL), PS("B", NULL), NULL));
}
static AST_NODE *build_f4_if_two_functions(void) { /* if i ve druhé funkci: návěští se nesmí shodovat */
  return make_program(
      func("main", true, if_stmt(lit_b(true), PS("x", NULL), NULL, NULL),
           func("helper", false, if_stmt(lit_b(true), PS("y", NULL), PS("z", NULL), NULL), NULL)));
}
static AST_NODE *build_f4_if_helper_var(void) { /* proměnná ve větvi ve druhé funkci (vlastní DEFVAR) */
  return make_program(
      func("main", true, PS("x", NULL),
           func("helper", false,
                vdef("h", 1, false, I, lit_i(1),
                     if_stmt(bin(OP_EQ, UI("h", 1), lit_i(1), B),
                             vdef("k", 2, true, I, lit_i(2), print_stmt(UI("k", 2), NULL)),
                             print_stmt(lit_i(0), NULL), NULL)),
                NULL)));
}

static AST_NODE *build_f4_while_count(void) { /* var i = 1; while i < 4 { print(i); i = i + 1 } */
  return main_prints(vdef("i", 1, false, I, lit_i(1),
                          while_stmt(bin(OP_LT, UI("i", 1), lit_i(4), B),
                                     print_stmt(UI("i", 1), inc("i", 1, NULL)), NULL)));
}
static AST_NODE *build_f4_while_zero(void) { /* tělo se nevykoná ani jednou */
  return main_prints(while_stmt(lit_b(false), PS("X", NULL), PS("ok", NULL)));
}
static AST_NODE *build_f4_while_1_10(void) { /* čísla 1 až 10 */
  return main_prints(vdef("i", 1, false, I, lit_i(1),
                          while_stmt(bin(OP_LE, UI("i", 1), lit_i(10), B),
                                     print_stmt(UI("i", 1), inc("i", 1, NULL)), NULL)));
}
static AST_NODE *build_f4_while_sum(void) { /* s = 1 + 2 + 3 + 4 + 5 */
  return main_prints(vdef(
      "s", 1, false, I, lit_i(0),
      vdef("i", 2, false, I, lit_i(1),
           while_stmt(bin(OP_LE, UI("i", 2), lit_i(5), B),
                      assign(UI("s", 1), add_int(UI("s", 1), UI("i", 2)), inc("i", 2, NULL)),
                      print_stmt(UI("s", 1), NULL)))));
}
static AST_NODE *build_f4_while_countdown(void) { /* n = 3, 2, 1 */
  return main_prints(vdef(
      "n", 1, false, I, lit_i(3),
      while_stmt(bin(OP_GT, UI("n", 1), lit_i(0), B),
                 print_stmt(UI("n", 1),
                            assign(UI("n", 1), bin(OP_SUB, UI("n", 1), lit_i(1), I), NULL)),
                 NULL)));
}
static AST_NODE *build_f4_while_neq(void) { /* while i != 3 */
  return main_prints(vdef("i", 1, false, I, lit_i(0),
                          while_stmt(bin(OP_NEQ, UI("i", 1), lit_i(3), B), inc("i", 1, NULL),
                                     print_stmt(UI("i", 1), NULL))));
}
static AST_NODE *build_f4_while_cond_expr(void) { /* while i * i < 20: i skončí na 5 */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(1),
      while_stmt(bin(OP_LT, bin(OP_MUL, UI("i", 1), UI("i", 1), I), lit_i(20), B), inc("i", 1, NULL),
                 print_stmt(UI("i", 1), NULL))));
}
static AST_NODE *build_f4_while_nested(void) { /* i = 1..2, j = 1..2, print(i * 10 + j) */
  AST_NODE *val = bin(OP_ADD, bin(OP_MUL, UI("i", 1), lit_i(10), I), UI("j", 2), I);
  AST_NODE *inner = while_stmt(bin(OP_LE, UI("j", 2), lit_i(2), B),
                               print_stmt(val, inc("j", 2, NULL)), inc("i", 1, NULL));
  AST_NODE *outer = while_stmt(bin(OP_LE, UI("i", 1), lit_i(2), B),
                               assign(UI("j", 2), lit_i(1), inner), NULL);
  return main_prints(vdef("i", 1, false, I, lit_i(1), vdef("j", 2, false, I, lit_i(1), outer)));
}
static AST_NODE *build_f4_while_if(void) { /* if v těle cyklu: vypíše jen sudá čísla 2 4 6 */
  AST_NODE *even = bin(OP_EQ, bin(OP_MUL, bin(OP_DIV, UI("i", 1), lit_i(2), I), lit_i(2), I),
                       UI("i", 1), B);
  return main_prints(vdef("i", 1, false, I, lit_i(1),
                          while_stmt(bin(OP_LE, UI("i", 1), lit_i(6), B),
                                     if_stmt(even, print_stmt(UI("i", 1), NULL), NULL,
                                             inc("i", 1, NULL)),
                                     NULL)));
}
static AST_NODE *build_f4_if_while(void) { /* while uvnitř then větve, v else větvi příkaz */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(0),
      if_stmt(bin(OP_LT, lit_i(1), lit_i(2), B),
              while_stmt(bin(OP_LT, UI("i", 1), lit_i(3), B), inc("i", 1, NULL), NULL),
              PS("no", NULL), print_stmt(UI("i", 1), NULL))));
}
static AST_NODE *build_f4_while_var_in_body(void) { /* proměnná uvnitř těla: DEFVAR musí být nahoře */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(1),
      while_stmt(bin(OP_LE, UI("i", 1), lit_i(3), B),
                 vdef("t", 2, true, I, bin(OP_MUL, UI("i", 1), lit_i(2), I),
                      print_stmt(UI("t", 2), inc("i", 1, NULL))),
                 NULL)));
}
static AST_NODE *build_f4_while_two(void) { /* dva while za sebou: návěští se nesmí opakovat */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(0),
      while_stmt(bin(OP_LT, UI("i", 1), lit_i(2), B), print_stmt(UI("i", 1), inc("i", 1, NULL)),
                 while_stmt(bin(OP_LT, UI("i", 1), lit_i(4), B),
                            print_stmt(UI("i", 1), inc("i", 1, NULL)), NULL))));
}
static AST_NODE *build_f4_while_around(void) { /* příkaz před cyklem i za ním */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(1),
      PS("<", while_stmt(bin(OP_LT, UI("i", 1), lit_i(3), B),
                         print_stmt(UI("i", 1), inc("i", 1, NULL)), PS(">", NULL)))));
}
static AST_NODE *build_f4_while_string(void) { /* s = s + "a" v cyklu: pomocné proměnné pro CONCAT */
  return main_prints(vdef(
      "s", 1, false, S, lit_s(""),
      vdef("i", 2, false, I, lit_i(0),
           while_stmt(bin(OP_LT, UI("i", 2), lit_i(3), B),
                      assign(use("s", 1, S), bin(OP_ADD, use("s", 1, S), lit_s("a"), S),
                             inc("i", 2, NULL)),
                      print_stmt(use("s", 1, S), NULL)))));
}
static AST_NODE *build_f4_while_helper(void) { /* while v main i v druhé funkci: návěští se nesmí shodovat */
  return make_program(func(
      "main", true,
      vdef("i", 1, false, I, lit_i(0),
           while_stmt(bin(OP_LT, UI("i", 1), lit_i(2), B), inc("i", 1, NULL),
                      print_stmt(UI("i", 1), NULL))),
      func("helper", false,
           vdef("k", 2, false, I, lit_i(0),
                while_stmt(bin(OP_LT, UI("k", 2), lit_i(1), B), inc("k", 2, NULL), NULL)),
           NULL)));
}
static AST_NODE *build_f4_while_in_else(void) { /* proměnná a cyklus uvnitř else větve */
  return main_prints(if_stmt(
      lit_b(false), PS("A", NULL),
      vdef("i", 1, false, I, lit_i(0),
           while_stmt(bin(OP_LT, UI("i", 1), lit_i(2), B), print_stmt(UI("i", 1), inc("i", 1, NULL)),
                      NULL)),
      NULL));
}
static AST_NODE *build_f4_while_if_else(void) { /* if else v těle cyklu: aabb */
  return main_prints(vdef(
      "i", 1, false, I, lit_i(1),
      while_stmt(bin(OP_LE, UI("i", 1), lit_i(4), B),
                 if_stmt(bin(OP_LT, UI("i", 1), lit_i(3), B), PS("a", NULL), PS("b", NULL),
                         inc("i", 1, NULL)),
                 NULL)));
}
static AST_NODE *build_f4_while_double(void) { /* x = 1.0; while x < 3.0 { x = x + 1.0 } */
  AST_NODE *step = assign(use("x", 1, D), bin(OP_ADD, use("x", 1, D), lit_d(1.0), D), NULL);
  return main_prints(vdef("x", 1, false, D, lit_d(1.0),
                          while_stmt(bin(OP_LT, use("x", 1, D), lit_d(3.0), B), step,
                                     print_stmt(use("x", 1, D), NULL))));
}
static AST_NODE *build_f4_while_mixed(void) { /* while i < 2.5, i je Int: v podmínce INT2FLOATS */
  return main_prints(vdef("i", 1, false, I, lit_i(1),
                          while_stmt(bin(OP_LT, UI("i", 1), lit_d(2.5), B), inc("i", 1, NULL),
                                     print_stmt(UI("i", 1), NULL))));
}
#undef PS
#undef UI
#undef I
#undef D
#undef S
#undef B

/* fáze 5: funkce, parametry, volání, return a rekurze. Typy uzlů a čísla proměnných (id) jako by je
 * zapsala sémantika. Čísla jsou v celém programu unikátní: pomocné funkce používají 1 až 9, proměnné
 * v main od 10. Jméno v kódu je jmeno$id. */
#define I DATA_INT
#define D DATA_DOUBLE
#define S DATA_STRING
#define B DATA_BOOL
#define U DATA_UNIT
#define PS(str, nx) print_stmt(lit_s(str), (nx))
#define UI(name, id) use((name), (id), I)

/* parametr: jméno, číslo, typ; další parametr přes next */
static AST_NODE *par(const char *name, int id, DATA_TYPE t, AST_NODE *next) {
  AST_NODE *n = ast_param_create(t, (char *)name, next);
  n->id = id;
  return n;
}

/* definice funkce (ne @main): návratový typ, parametry, tělo (řetěz příkazů); další funkce přes next */
static AST_NODE *fdef(const char *name, DATA_TYPE ret, AST_NODE *params, AST_NODE *body,
                      AST_NODE *next) {
  return ast_func_def_create(make_body(body), params, next, ret, (char *)name, false, true);
}

/* @main funkce (Unit, bez parametrů) */
static AST_NODE *main_fn(AST_NODE *body, AST_NODE *next) { return func("main", true, body, next); }

/* volání funkce: argumenty (řetěz přes next, zleva doprava) a typ výsledku */
static AST_NODE *fcall(const char *name, AST_NODE *args, DATA_TYPE t) {
  AST_NODE *n = ast_call_create(args, (char *)name, false, true, NULL);
  n->data_type = t;
  return n;
}

/* volání jako příkaz: výsledek se zahodí; další příkaz přes next */
static AST_NODE *fcall_stmt(AST_NODE *c, AST_NODE *next) { return ast_create(c, NULL, next, NODE_EXPR_STMT); }

/* return výraz; e == NULL je prázdný return */
static AST_NODE *fret(AST_NODE *e, AST_NODE *next) { return ast_return_create(e, next); }

/* poslední výraz těla funkce = její návratová hodnota bez return */
static AST_NODE *flast(AST_NODE *e, AST_NODE *next) { return ast_create(e, NULL, next, NODE_EXPR_STMT); }

/* argumenty za sebou (zleva doprava) */
static AST_NODE *a2(AST_NODE *a, AST_NODE *b) {
  a->next = b;
  return a;
}
static AST_NODE *a3(AST_NODE *a, AST_NODE *b, AST_NODE *c) {
  a->next = b;
  b->next = c;
  return a;
}
static AST_NODE *a4(AST_NODE *a, AST_NODE *b, AST_NODE *c, AST_NODE *d) {
  a->next = b;
  b->next = c;
  c->next = d;
  return a;
}

/* def hello(): Unit = print("Hi") */
static AST_NODE *hello_fn(AST_NODE *next) { return fdef("hello", U, NULL, PS("Hi", NULL), next); }
/* def show(x: Int): Unit = print(x)   (x má číslo 1) */
static AST_NODE *show_fn(AST_NODE *next) {
  return fdef("show", U, par("x", 1, I, NULL), print_stmt(UI("x", 1), NULL), next);
}
/* def pdiff(a: Int, b: Int): Unit = print(a - b)   (a = 1, b = 2: záleží na pořadí argumentů) */
static AST_NODE *pdiff_fn(AST_NODE *next) {
  return fdef("pdiff", U, par("a", 1, I, par("b", 2, I, NULL)),
              print_stmt(bin(OP_SUB, UI("a", 1), UI("b", 2), I), NULL), next);
}
/* def twice(x: Int): Int = return x * 2 */
static AST_NODE *twice_fn(AST_NODE *next) {
  return fdef("twice", I, par("x", 1, I, NULL), fret(bin(OP_MUL, UI("x", 1), lit_i(2), I), NULL), next);
}

static AST_NODE *build_f5_call_unit(void) {
  return make_program(main_fn(fcall_stmt(fcall("hello", NULL, U), NULL), hello_fn(NULL)));
}
static AST_NODE *build_f5_call_twice(void) {
  return make_program(main_fn(
      fcall_stmt(fcall("hello", NULL, U), fcall_stmt(fcall("hello", NULL, U), NULL)), hello_fn(NULL)));
}
static AST_NODE *build_f5_call_chain(void) { /* main volá a, a volá b */
  return make_program(main_fn(
      fcall_stmt(fcall("a", NULL, U), NULL),
      fdef("a", U, NULL, PS("<", fcall_stmt(fcall("b", NULL, U), PS(">", NULL))),
           fdef("b", U, NULL, PS("b", NULL), NULL))));
}
static AST_NODE *build_f5_call_main_last(void) { /* helper je definovaný před main */
  return make_program(hello_fn(main_fn(fcall_stmt(fcall("hello", NULL, U), NULL), NULL)));
}
static AST_NODE *build_f5_entry_run(void) { /* @main funkce se jmenuje run a volá další */
  return make_program(func("run", true, fcall_stmt(fcall("hello", NULL, U), NULL), hello_fn(NULL)));
}

static AST_NODE *build_f5_param_one(void) {
  return make_program(main_fn(fcall_stmt(fcall("show", lit_i(42), U), NULL), show_fn(NULL)));
}
static AST_NODE *build_f5_param_two(void) { /* pdiff(10, 3) = 7: pořadí argumentů */
  return make_program(
      main_fn(fcall_stmt(fcall("pdiff", a2(lit_i(10), lit_i(3)), U), NULL), pdiff_fn(NULL)));
}
static AST_NODE *build_f5_param_types(void) { /* show3("x", 5, 2.5): tři parametry tří typů */
  AST_NODE *body =
      print_stmt(use("s", 1, S), print_stmt(UI("n", 2), print_stmt(use("d", 3, D), NULL)));
  AST_NODE *f = fdef("show3", U, par("s", 1, S, par("n", 2, I, par("d", 3, D, NULL))), body, NULL);
  return make_program(
      main_fn(fcall_stmt(fcall("show3", a3(lit_s("x"), lit_i(5), lit_d(2.5)), U), NULL), f));
}
static AST_NODE *build_f5_param_expr(void) { /* pdiff(1 + 2, 3 * 4) = -9: argumenty jsou výrazy */
  AST_NODE *args = a2(bin(OP_ADD, lit_i(1), lit_i(2), I), bin(OP_MUL, lit_i(3), lit_i(4), I));
  return make_program(main_fn(fcall_stmt(fcall("pdiff", args, U), NULL), pdiff_fn(NULL)));
}
static AST_NODE *build_f5_param_vars(void) { /* val a = 10; val b = 4; pdiff(a, b) */
  return make_program(main_fn(
      vdef("a", 10, true, I, lit_i(10),
           vdef("b", 11, true, I, lit_i(4),
                fcall_stmt(fcall("pdiff", a2(UI("a", 10), UI("b", 11)), U), NULL))),
      pdiff_fn(NULL)));
}
static AST_NODE *build_f5_param_frames(void) { /* var x = 1; show(5); print(x): proměnná volajícího přežije */
  return make_program(main_fn(vdef("x", 10, false, I, lit_i(1),
                                   fcall_stmt(fcall("show", lit_i(5), U), print_stmt(UI("x", 10), NULL))),
                              show_fn(NULL)));
}
static AST_NODE *build_f5_param_concat(void) { /* greet(name: String): print("Hi " + name) */
  AST_NODE *f = fdef("greet", U, par("name", 1, S, NULL),
                     print_stmt(bin(OP_ADD, lit_s("Hi "), use("name", 1, S), S), NULL), NULL);
  return make_program(main_fn(fcall_stmt(fcall("greet", lit_s("Bob"), U), NULL), f));
}
static AST_NODE *build_f5_param_loop(void) { /* count(n): var i = 1; while i <= n { print(i); i = i + 1 } */
  AST_NODE *body = vdef("i", 2, false, I, lit_i(1),
                        while_stmt(bin(OP_LE, UI("i", 2), UI("n", 1), B),
                                   print_stmt(UI("i", 2), inc("i", 2, NULL)), NULL));
  AST_NODE *f = fdef("count", U, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(fcall_stmt(fcall("count", lit_i(3), U), NULL), f));
}
static AST_NODE *build_f5_param_if(void) { /* sign(n): if n < 0 { neg } else { pos }; sign(-3), sign(2) */
  AST_NODE *body = if_stmt(bin(OP_LT, UI("n", 1), lit_i(0), B), PS("neg", NULL), PS("pos", NULL), NULL);
  AST_NODE *f = fdef("sign", U, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(
      fcall_stmt(fcall("sign", lit_i(-3), U), fcall_stmt(fcall("sign", lit_i(2), U), NULL)), f));
}
static AST_NODE *build_f5_param_four(void) { /* sum4(1, 2, 3, 4) = 1234: čtyři parametry za sebou */
  AST_NODE *x = bin(OP_ADD,
                    bin(OP_ADD,
                        bin(OP_ADD, bin(OP_MUL, UI("a", 1), lit_i(1000), I),
                            bin(OP_MUL, UI("b", 2), lit_i(100), I), I),
                        bin(OP_MUL, UI("c", 3), lit_i(10), I), I),
                    UI("d", 4), I);
  AST_NODE *f = fdef("sum4", U, par("a", 1, I, par("b", 2, I, par("c", 3, I, par("d", 4, I, NULL)))),
                     print_stmt(x, NULL), NULL);
  return make_program(main_fn(
      fcall_stmt(fcall("sum4", a4(lit_i(1), lit_i(2), lit_i(3), lit_i(4)), U), NULL), f));
}
static AST_NODE *build_f5_param_mixed(void) { /* scale(x: Double, k: Int): print(x * k), INT2FLOATS v těle */
  AST_NODE *f = fdef("scale", U, par("x", 1, D, par("k", 2, I, NULL)),
                     print_stmt(bin(OP_MUL, use("x", 1, D), UI("k", 2), D), NULL), NULL);
  return make_program(
      main_fn(fcall_stmt(fcall("scale", a2(lit_d(1.5), lit_i(2)), U), NULL), f));
}

static AST_NODE *build_f5_ret_int(void) { /* val r = twice(21); print(r) */
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("twice", lit_i(21), I), print_stmt(UI("r", 10), NULL)),
      twice_fn(NULL)));
}
static AST_NODE *build_f5_ret_last(void) { /* hodnota z posledního výrazu těla, bez return */
  AST_NODE *f = fdef("twice", I, par("x", 1, I, NULL),
                     flast(bin(OP_MUL, UI("x", 1), lit_i(2), I), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("twice", lit_i(21), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f5_ret_last_call(void) { /* poslední výraz těla je volání jiné funkce */
  AST_NODE *wrapper = fdef("wrapper", I, par("y", 2, I, NULL), flast(fcall("twice", UI("y", 2), I), NULL),
                           twice_fn(NULL));
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("wrapper", lit_i(4), I), print_stmt(UI("r", 10), NULL)), wrapper));
}
static AST_NODE *build_f5_ret_last_after(void) { /* print(v); x + 1: příkaz před posledním výrazem */
  AST_NODE *f = fdef("incr", I, par("x", 1, I, NULL),
                     PS("v", flast(bin(OP_ADD, UI("x", 1), lit_i(1), I), NULL)), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("incr", lit_i(1), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f5_ret_string(void) {
  AST_NODE *f = fdef("greeting", S, par("n", 1, S, NULL),
                     fret(bin(OP_ADD, lit_s("Hi "), use("n", 1, S), S), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, S, fcall("greeting", lit_s("Ann"), S), print_stmt(use("r", 10, S), NULL)), f));
}
static AST_NODE *build_f5_ret_double(void) { /* half(5.0) = 2.5 */
  AST_NODE *f = fdef("half", D, par("x", 1, D, NULL),
                     fret(bin(OP_DIV, use("x", 1, D), lit_d(2.0), D), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, D, fcall("half", lit_d(5.0), D), print_stmt(use("r", 10, D), NULL)), f));
}
static AST_NODE *build_f5_ret_bool(void) { /* isPos(3), isPos(-1) */
  AST_NODE *f = fdef("isPos", B, par("n", 1, I, NULL), fret(bin(OP_GT, UI("n", 1), lit_i(0), B), NULL), NULL);
  return make_program(main_fn(
      vdef("b", 10, true, B, fcall("isPos", lit_i(3), B),
           vdef("c", 11, true, B, fcall("isPos", lit_i(-1), B),
                print_stmt(use("b", 10, B), print_stmt(use("c", 11, B), NULL)))),
      f));
}
static AST_NODE *build_f5_ret_early(void) { /* abs(n): if n < 0 { return -n }; return n */
  AST_NODE *body = if_stmt(bin(OP_LT, UI("n", 1), lit_i(0), B), fret(neg(UI("n", 1), I), NULL), NULL,
                           fret(UI("n", 1), NULL));
  AST_NODE *f = fdef("abs", I, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(
      vdef("a", 10, true, I, fcall("abs", lit_i(-5), I),
           vdef("b", 11, true, I, fcall("abs", lit_i(7), I),
                print_stmt(UI("a", 10), print_stmt(UI("b", 11), NULL)))),
      f));
}
static AST_NODE *build_f5_ret_early_unit(void) { /* prázdný return v Unit funkci */
  AST_NODE *body = if_stmt(bin(OP_GT, UI("n", 1), lit_i(0), B), PS("pos", fret(NULL, NULL)), NULL,
                           PS("neg", NULL));
  AST_NODE *f = fdef("check", U, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(
      fcall_stmt(fcall("check", lit_i(1), U), fcall_stmt(fcall("check", lit_i(-1), U), NULL)), f));
}
static AST_NODE *build_f5_ret_in_loop(void) { /* return uvnitř while uvnitř if: firstOver(50) = 8 */
  AST_NODE *loop = while_stmt(
      bin(OP_LT, UI("i", 2), lit_i(100), B),
      if_stmt(bin(OP_GT, bin(OP_MUL, UI("i", 2), UI("i", 2), I), UI("limit", 1), B), fret(UI("i", 2), NULL),
              NULL, inc("i", 2, NULL)),
      fret(lit_i(0), NULL));
  AST_NODE *f = fdef("firstOver", I, par("limit", 1, I, NULL), vdef("i", 2, false, I, lit_i(1), loop), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("firstOver", lit_i(50), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f5_ret_two_calls(void) { /* a = twice(5), b = twice(6), print(a + b) */
  return make_program(main_fn(
      vdef("a", 10, true, I, fcall("twice", lit_i(5), I),
           vdef("b", 11, true, I, fcall("twice", lit_i(6), I),
                print_stmt(bin(OP_ADD, UI("a", 10), UI("b", 11), I), NULL))),
      twice_fn(NULL)));
}
static AST_NODE *build_f5_ret_discard(void) { /* twice(5) jako příkaz: výsledek se zahodí */
  return make_program(
      main_fn(fcall_stmt(fcall("twice", lit_i(5), I), PS("ok", NULL)), twice_fn(NULL)));
}
static AST_NODE *build_f5_call_in_loop(void) { /* while i <= 3 { show(i); i = i + 1 } */
  return make_program(main_fn(
      vdef("i", 10, false, I, lit_i(1),
           while_stmt(bin(OP_LE, UI("i", 10), lit_i(3), B),
                      fcall_stmt(fcall("show", UI("i", 10), U), inc("i", 10, NULL)), NULL)),
      show_fn(NULL)));
}
static AST_NODE *build_f5_call_in_if(void) { /* volání ve větvích if */
  return make_program(main_fn(
      if_stmt(lit_b(true), fcall_stmt(fcall("show", lit_i(1), U), NULL),
              fcall_stmt(fcall("show", lit_i(2), U), NULL),
              if_stmt(lit_b(false), fcall_stmt(fcall("show", lit_i(3), U), NULL),
                      fcall_stmt(fcall("show", lit_i(4), U), NULL), NULL)),
      show_fn(NULL)));
}

/* def fact(n: Int): Int: if n <= 1 { return 1 }; val m = n - 1; val r = fact(m); return n * r */
static AST_NODE *fact_fn(AST_NODE *next) {
  AST_NODE *body = if_stmt(
      bin(OP_LE, UI("n", 1), lit_i(1), B), fret(lit_i(1), NULL), NULL,
      vdef("m", 2, true, I, bin(OP_SUB, UI("n", 1), lit_i(1), I),
           vdef("r", 3, true, I, fcall("fact", UI("m", 2), I),
                fret(bin(OP_MUL, UI("n", 1), UI("r", 3), I), NULL))));
  return fdef("fact", I, par("n", 1, I, NULL), body, next);
}
static AST_NODE *fact_prog(long long n) {
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("fact", lit_i(n), I), print_stmt(UI("r", 10), NULL)), fact_fn(NULL)));
}
static AST_NODE *build_f5_fact(void) { return fact_prog(5); }
static AST_NODE *build_f5_fact_10(void) { return fact_prog(10); }
static AST_NODE *build_f5_fib(void) { /* fib(10) = 55: dvě rekurzivní volání */
  AST_NODE *body = if_stmt(
      bin(OP_LT, UI("n", 1), lit_i(2), B), fret(UI("n", 1), NULL), NULL,
      vdef("a", 2, true, I, fcall("fib", bin(OP_SUB, UI("n", 1), lit_i(1), I), I),
           vdef("b", 3, true, I, fcall("fib", bin(OP_SUB, UI("n", 1), lit_i(2), I), I),
                fret(bin(OP_ADD, UI("a", 2), UI("b", 3), I), NULL))));
  AST_NODE *f = fdef("fib", I, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("fib", lit_i(10), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f5_sum_rec(void) { /* sumTo(20) = 210: hloubka rekurze 20 */
  AST_NODE *body = if_stmt(
      bin(OP_EQ, UI("n", 1), lit_i(0), B), fret(lit_i(0), NULL), NULL,
      vdef("r", 2, true, I, fcall("sumTo", bin(OP_SUB, UI("n", 1), lit_i(1), I), I),
           fret(bin(OP_ADD, UI("n", 1), UI("r", 2), I), NULL)));
  AST_NODE *f = fdef("sumTo", I, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("sumTo", lit_i(20), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f5_countdown(void) { /* Unit rekurze: countdown(3) vypíše 321 */
  AST_NODE *body = if_stmt(
      bin(OP_GT, UI("n", 1), lit_i(0), B),
      print_stmt(UI("n", 1),
                 fcall_stmt(fcall("countdown", bin(OP_SUB, UI("n", 1), lit_i(1), I), U), NULL)),
      NULL, NULL);
  AST_NODE *f = fdef("countdown", U, par("n", 1, I, NULL), body, NULL);
  return make_program(main_fn(fcall_stmt(fcall("countdown", lit_i(3), U), NULL), f));
}
static AST_NODE *build_f5_mutual(void) { /* isEven volá isOdd a naopak: isEven(4), isEven(3) */
  AST_NODE *odd = fdef(
      "isOdd", B, par("m", 3, I, NULL),
      if_stmt(bin(OP_EQ, UI("m", 3), lit_i(0), B), fret(lit_b(false), NULL), NULL,
              vdef("q", 4, true, B, fcall("isEven", bin(OP_SUB, UI("m", 3), lit_i(1), I), B),
                   fret(use("q", 4, B), NULL))),
      NULL);
  AST_NODE *even = fdef(
      "isEven", B, par("n", 1, I, NULL),
      if_stmt(bin(OP_EQ, UI("n", 1), lit_i(0), B), fret(lit_b(true), NULL), NULL,
              vdef("r", 2, true, B, fcall("isOdd", bin(OP_SUB, UI("n", 1), lit_i(1), I), B),
                   fret(use("r", 2, B), NULL))),
      odd);
  return make_program(main_fn(
      vdef("a", 10, true, B, fcall("isEven", lit_i(4), B),
           vdef("b", 11, true, B, fcall("isEven", lit_i(3), B),
                print_stmt(use("a", 10, B), print_stmt(use("b", 11, B), NULL)))),
      even));
}
#undef PS
#undef UI
#undef I
#undef D
#undef S
#undef B
#undef U

/* fáze 6: vestavěné funkce. Volání je NODE_CALL se jménem funkce a argumenty ve third; v základním
 * zadání smí stát jen jako příkaz nebo vpravo od val a =, proto výsledek většinou ukládám do
 * proměnné (val) a vypisuju až ji. Případy *_nested volají funkci uvnitř jiného volání (FUNEXP). */
#define I DATA_INT
#define D DATA_DOUBLE
#define S DATA_STRING
#define U DATA_UNIT
#define B6(name, args, type) fcall((name), (args), (type))
#define PS(str, nx) print_stmt(lit_s(str), (nx))
#define UI(name, id) use((name), (id), I)
#define US(name, id) use((name), (id), S)
#define UD(name, id) use((name), (id), D)

/* val v = fn(args); print(v); next. Typ proměnné je typ výsledku volání. */
static AST_NODE *val_print(const char *v, int id, AST_NODE *call, DATA_TYPE t, AST_NODE *next) {
  return vdef(v, id, true, t, call, print_stmt(use(v, id, t), next));
}
static AST_NODE *prog6(AST_NODE *body) { return make_program(main_fn(body, NULL)); }

/* strLength */
static AST_NODE *build_f6_strlen(void) {
  return prog6(val_print("n", 10, B6("strLength", lit_s("Ahoj"), I), I, NULL));
}
static AST_NODE *build_f6_strlen_empty(void) {
  return prog6(val_print("n", 10, B6("strLength", lit_s(""), I), I, NULL));
}
static AST_NODE *build_f6_strlen_var(void) {
  return prog6(vdef("s", 10, true, S, lit_s("abcde"),
                    val_print("n", 11, B6("strLength", US("s", 10), I), I, NULL)));
}
static AST_NODE *build_f6_strlen_special(void) { /* mezera a nový řádek se počítají jako znaky */
  return prog6(val_print("n", 10, B6("strLength", lit_s("a b\nc"), I), I, NULL));
}
/* toInt */
static AST_NODE *build_f6_toint(void) {
  return prog6(val_print("a", 10, B6("toInt", lit_d(3.99), I), I,
                         val_print("b", 11, B6("toInt", lit_d(-2.7), I), I, NULL)));
}
static AST_NODE *build_f6_toint_var(void) {
  return prog6(vdef("d", 10, true, D, lit_d(7.5),
                    val_print("n", 11, B6("toInt", UD("d", 10), I), I, NULL)));
}
/* chr, ord */
static AST_NODE *build_f6_chr(void) {
  return prog6(val_print("a", 10, B6("chr", lit_i(72), S), S,
                         val_print("b", 11, B6("chr", lit_i(105), S), S, NULL)));
}
static AST_NODE *build_f6_chr_edges(void) { /* nový řádek (10) a nejvyšší znak (127), strLength nad nimi */
  return prog6(val_print("a", 10, B6("chr", lit_i(10), S), S,
                         val_print("b", 11, B6("strLength", B6("chr", lit_i(127), S), I), I, NULL)));
}
static AST_NODE *build_f6_ord(void) {
  return prog6(val_print("a", 10, B6("ord", a2(lit_s("A"), lit_i(0)), I), I,
                         val_print("b", 11, B6("ord", a2(lit_s("abc"), lit_i(2)), I), I, NULL)));
}
static AST_NODE *build_f6_ord_out(void) { /* index mimo řetězec nebo prázdný řetězec: -1 */
  return prog6(val_print("a", 10, B6("ord", a2(lit_s("abc"), lit_i(3)), I), I,
                         val_print("b", 11, B6("ord", a2(lit_s("abc"), lit_i(-1)), I), I,
                                   val_print("c", 12, B6("ord", a2(lit_s(""), lit_i(0)), I), I, NULL))));
}
static AST_NODE *build_f6_ord_var(void) { /* ord(s, i) s proměnnými */
  return prog6(vdef("s", 10, true, S, lit_s("xyz"),
                    vdef("i", 11, true, I, lit_i(1),
                         val_print("c", 12, B6("ord", a2(US("s", 10), UI("i", 11)), I), I, NULL))));
}
/* subStr */
static AST_NODE *sub3(const char *s, long long a, long long b) {
  return B6("subStr", a3(lit_s(s), lit_i(a), lit_i(b)), S);
}
static AST_NODE *build_f6_substr(void) { return prog6(val_print("r", 10, sub3("abcdef", 1, 4), S, NULL)); }
static AST_NODE *build_f6_substr_full(void) { return prog6(val_print("r", 10, sub3("abc", 0, 3), S, NULL)); }
static AST_NODE *build_f6_substr_last(void) { return prog6(val_print("r", 10, sub3("abc", 2, 3), S, NULL)); }
static AST_NODE *build_f6_substr_empty(void) { /* start == end: prázdný řetězec, ne chyba */
  return prog6(PS("<", val_print("r", 10, sub3("abc", 1, 1), S, PS(">", NULL))));
}
static AST_NODE *build_f6_substr_var(void) {
  return prog6(vdef("s", 10, true, S, lit_s("Hello World"),
                    vdef("a", 11, true, I, lit_i(6),
                         vdef("b", 12, true, I, lit_i(11),
                              val_print("r", 13, B6("subStr", a3(US("s", 10), UI("a", 11), UI("b", 12)), S),
                                        S, NULL)))));
}
static AST_NODE *build_f6_substr_two(void) { /* dvě volání za sebou, každé s jinými mezemi */
  return prog6(val_print("a", 10, sub3("abcdef", 0, 2), S, val_print("b", 11, sub3("abcdef", 3, 6), S, NULL)));
}
/* toStr */
static AST_NODE *build_f6_tostr_int(void) {
  return prog6(val_print("a", 10, B6("toStr", lit_i(42), S), S,
                         val_print("b", 11, B6("toStr", lit_i(-7), S), S, NULL)));
}
static AST_NODE *build_f6_tostr_double(void) { /* %.2f: 2.50 a 3.14, ne hexadecimálně */
  return prog6(val_print("a", 10, B6("toStr", lit_d(2.5), S), S,
                         val_print("b", 11, B6("toStr", lit_d(3.14159), S), S, NULL)));
}
static AST_NODE *build_f6_tostr_double_neg(void) {
  return prog6(val_print("a", 10, B6("toStr", lit_d(-1.5), S), S,
                         val_print("b", 11, B6("toStr", lit_d(0.0), S), S,
                                   val_print("c", 12, B6("toStr", lit_d(100.0), S), S, NULL))));
}
static AST_NODE *build_f6_tostr_string(void) {
  return prog6(val_print("a", 10, B6("toStr", lit_s("abc"), S), S, NULL));
}
static AST_NODE *build_f6_tostr_var(void) { /* toStr(n) a spojení výsledku s textem */
  return prog6(vdef("n", 10, true, I, lit_i(12),
                    vdef("s", 11, true, S, B6("toStr", UI("n", 10), S),
                         vdef("t", 12, true, S, bin(OP_ADD, lit_s("n="), US("s", 11), S),
                              print_stmt(US("t", 12), NULL)))));
}
static AST_NODE *build_f6_tostr_vars(void) { /* toStr nad proměnnými Int, Double i String */
  return prog6(vdef("i", 10, true, I, lit_i(5),
                    vdef("d", 11, true, D, lit_d(0.5),
                         vdef("t", 12, true, S, lit_s("t"),
                              val_print("a", 13, B6("toStr", UI("i", 10), S), S,
                                        val_print("b", 14, B6("toStr", UD("d", 11), S), S,
                                                  val_print("c", 15, B6("toStr", US("t", 12), S), S, NULL)))))));
}
/* víc volání téže i různých funkcí: každá vestavěná funkce se definuje jednou */
static AST_NODE *build_f6_twice_same(void) {
  return prog6(val_print("a", 10, B6("strLength", lit_s("ab"), I), I,
                         val_print("b", 11, B6("toStr", lit_i(1), S), S,
                                   val_print("c", 12, B6("strLength", lit_s("abc"), I), I,
                                             val_print("d", 13, B6("toStr", lit_i(2), S), S, NULL)))));
}
static AST_NODE *build_f6_all(void) { /* všechny funkce bez vstupu v jednom programu */
  return prog6(vdef(
      "s", 10, true, S, B6("subStr", a3(lit_s("hello"), lit_i(1), lit_i(4)), S), /* "ell" */
      val_print("n", 11, B6("strLength", US("s", 10), I), I,                      /* 3 */
                val_print("o", 12, B6("ord", a2(US("s", 10), lit_i(0)), I), I,       /* 101 */
                          val_print("c", 13, B6("chr", UI("o", 12), S), S,            /* "e" */
                                    val_print("t", 14, B6("toStr", UI("n", 11), S), S,   /* "3" */
                                              val_print("d", 15, B6("toInt", lit_d(9.9), I), I, NULL))))))); /* 9 */
}
/* vestavěné funkce uvnitř cyklu a uvnitř uživatelských funkcí */
static AST_NODE *build_f6_loop(void) { /* každý znak "hello" o jedna dál: ifmmp */
  AST_NODE *body = vdef("code", 12, true, I, B6("ord", a2(US("s", 10), UI("i", 11)), I),
                        vdef("nxt", 13, true, I, add_int(UI("code", 12), lit_i(1)),
                             vdef("c", 14, true, S, B6("chr", UI("nxt", 13), S),
                                  print_stmt(US("c", 14), inc("i", 11, NULL)))));
  return prog6(vdef("s", 10, true, S, lit_s("hello"),
                    vdef("n", 15, true, I, B6("strLength", US("s", 10), I),
                         vdef("i", 11, false, I, lit_i(0),
                              while_stmt(bin(OP_LT, UI("i", 11), UI("n", 15), DATA_BOOL), body, NULL)))));
}
static AST_NODE *build_f6_in_func(void) { /* uživatelská funkce volá vestavěnou a vrací její výsledek */
  AST_NODE *f = fdef("lenOf", I, par("s", 1, S, NULL), fret(B6("strLength", US("s", 1), I), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("lenOf", lit_s("abcd"), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f6_in_func_last(void) { /* hodnota z posledního výrazu: vestavěné volání */
  AST_NODE *f = fdef("lenOf", I, par("s", 1, S, NULL), flast(B6("strLength", US("s", 1), I), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("lenOf", lit_s("abcd"), I), print_stmt(UI("r", 10), NULL)), f));
}
static AST_NODE *build_f6_discard(void) { /* vestavěná funkce jako příkaz: výsledek se zahodí */
  return prog6(fcall_stmt(B6("strLength", lit_s("abc"), I), fcall_stmt(B6("chr", lit_i(65), S), PS("ok", NULL))));
}
/* volání uvnitř volání (rozšíření FUNEXP) */
static AST_NODE *build_f6_nested(void) { /* print(toStr(strLength(subStr("abcd", 1, 3)))) = 2 */
  return prog6(print_stmt(B6("toStr", B6("strLength", sub3("abcd", 1, 3), I), S), NULL));
}
static AST_NODE *build_f6_nested_user(void) { /* show(strLength("abc")): vestavěné volání jako argument */
  return make_program(main_fn(fcall_stmt(fcall("show", B6("strLength", lit_s("abc"), I), U), NULL), show_fn(NULL)));
}
static AST_NODE *build_f6_nested_expr(void) { /* strLength("ab") + strLength("cde") * 2 = 8 */
  return prog6(print_stmt(bin(OP_ADD, B6("strLength", lit_s("ab"), I),
                              bin(OP_MUL, B6("strLength", lit_s("cde"), I), lit_i(2), I), I),
                          NULL));
}
static AST_NODE *build_f6_nested_args(void) { /* subStr s argumenty z volání: subStr("abcdef", ord("A", 0) - 64, 4) */
  return prog6(print_stmt(B6("subStr", a3(lit_s("abcdef"),
                                        bin(OP_SUB, B6("ord", a2(lit_s("A"), lit_i(0)), I), lit_i(64), I),
                                        lit_i(4)), S),
                          NULL));
}

/* běhové chyby: výstup před chybou je v expect_out, kód v posledním poli případu */
static AST_NODE *err_sub(const char *s, long long a, long long b) {
  return prog6(PS("a", val_print("r", 10, sub3(s, a, b), S, PS("b", NULL))));
}
static AST_NODE *build_f6_substr_neg_start(void) { return err_sub("abc", -1, 2); }
static AST_NODE *build_f6_substr_neg_end(void) { return err_sub("abc", 1, -1); }
static AST_NODE *build_f6_substr_start_gt_end(void) { return err_sub("abc", 2, 1); }
static AST_NODE *build_f6_substr_start_len(void) { return err_sub("abc", 3, 3); }
static AST_NODE *build_f6_substr_end_len(void) { return err_sub("abc", 0, 4); }
static AST_NODE *build_f6_substr_empty_str(void) { return err_sub("", 0, 0); }
static AST_NODE *build_f6_chr_big(void) {
  return prog6(PS("a", val_print("r", 10, B6("chr", lit_i(128), S), S, PS("b", NULL))));
}
static AST_NODE *build_f6_chr_neg(void) {
  return prog6(PS("a", val_print("r", 10, B6("chr", lit_i(-1), S), S, PS("b", NULL))));
}
static AST_NODE *build_f6_div_zero(void) { /* celočíselné dělení nulou: chyba 57 z interpretu */
  return prog6(PS("a", print_stmt(bin(OP_DIV, lit_i(1), lit_i(0), I), PS("b", NULL))));
}

/* vstup: readInt, readDouble, readLine */
static AST_NODE *build_f6_readint(void) { return prog6(val_print("n", 10, B6("readInt", NULL, I), I, NULL)); }
static AST_NODE *build_f6_readint_calc(void) { /* dvě čísla: 20 + 22 */
  return prog6(vdef("a", 10, true, I, B6("readInt", NULL, I),
                    vdef("b", 11, true, I, B6("readInt", NULL, I),
                         print_stmt(add_int(UI("a", 10), UI("b", 11)), NULL))));
}
static AST_NODE *build_f6_readdouble(void) {
  return prog6(val_print("d", 10, B6("readDouble", NULL, D), D, NULL));
}
static AST_NODE *build_f6_readline(void) { /* dva řádky, každý včetně \n */
  return prog6(val_print("a", 10, B6("readLine", NULL, S), S, val_print("b", 11, B6("readLine", NULL, S), S, NULL)));
}
static AST_NODE *build_f6_readline_brackets(void) { /* < řádek > : vidět, jestli je prázdný */
  return prog6(PS("<", val_print("a", 10, B6("readLine", NULL, S), S, PS(">", NULL))));
}
static AST_NODE *build_f6_read_mixed(void) { /* readInt, readLine, readDouble za sebou */
  return prog6(val_print("n", 10, B6("readInt", NULL, I), I,
                         val_print("l", 11, B6("readLine", NULL, S), S,
                                   val_print("d", 12, B6("readDouble", NULL, D), D, NULL))));
}
static AST_NODE *build_f6_read_loop(void) { /* n = readInt(); sečte dalších n čísel */
  AST_NODE *body = vdef("x", 14, true, I, B6("readInt", NULL, I),
                        assign(UI("sum", 12), add_int(UI("sum", 12), UI("x", 14)), inc("i", 13, NULL)));
  return prog6(vdef("n", 10, true, I, B6("readInt", NULL, I),
                    vdef("sum", 12, false, I, lit_i(0),
                         vdef("i", 13, false, I, lit_i(0),
                              while_stmt(bin(OP_LT, UI("i", 13), UI("n", 10), DATA_BOOL), body,
                                         print_stmt(UI("sum", 12), NULL))))));
}
static AST_NODE *build_f6_read_discard(void) { /* první číslo se zahodí, vypíše se druhé */
  return prog6(fcall_stmt(B6("readInt", NULL, I), val_print("x", 10, B6("readInt", NULL, I), I, NULL)));
}
static AST_NODE *build_f6_read_func(void) { /* čtení uvnitř uživatelské funkce: sum2() */
  AST_NODE *f = fdef("sum2", I, NULL,
                     vdef("a", 1, true, I, B6("readInt", NULL, I),
                          vdef("b", 2, true, I, B6("readInt", NULL, I),
                               fret(add_int(UI("a", 1), UI("b", 2)), NULL))),
                     NULL);
  return make_program(main_fn(
      vdef("r", 10, true, I, fcall("sum2", NULL, I), print_stmt(UI("r", 10), NULL)), f));
}
#undef PS
#undef UI
#undef US
#undef UD
#undef B6
#undef I
#undef D
#undef S
#undef U

/* fáze 7: rozšíření BOOLTHEN. && = BINOP OP_AND, || = BINOP OP_OR, ! = UNOP OP_NOT, vše DATA_BOOL. */
#define I DATA_INT
#define S DATA_STRING
#define B DATA_BOOL
#define U DATA_UNIT
#define BAND(l, r) bin(OP_AND, (l), (r), B)
#define BOR(l, r) bin(OP_OR, (l), (r), B)
#define BNOT(x) not_op((x))
#define UB(name, id) use((name), (id), B)
#define UI(name, id) use((name), (id), I)
#define LT_(l, r) bin(OP_LT, (l), (r), B)
#define GT_(l, r) bin(OP_GT, (l), (r), B)
#define EQ_(l, r) bin(OP_EQ, (l), (r), B)
#define NEQ_(l, r) bin(OP_NEQ, (l), (r), B)
#define PS(str, nx) print_stmt(lit_s(str), (nx))
#define P(e, nx) print_stmt((e), (nx))

static AST_NODE *not_op(AST_NODE *x) {
  AST_NODE *n = ast_unop_create(x, NULL, OP_NOT);
  n->data_type = DATA_BOOL;
  return n;
}
static AST_NODE *prog7(AST_NODE *body) { return make_program(main_fn(body, NULL)); }
/* 10 / b > 1: pravý operand zkratu; pro b = 0 je to dělení nulou (chyba 57), když se vyhodnotí */
static AST_NODE *div_gt(long long a, const char *v, int id, long long limit) {
  return GT_(bin(OP_DIV, lit_i(a), UI(v, id), I), lit_i(limit));
}

/* pravdivostní tabulky */
#define TT(name, op, a, b) \
  static AST_NODE *build_##name(void) { return prog7(P(bin(op, lit_b(a), lit_b(b), B), NULL)); }
TT(f7_and_tt, OP_AND, true, true)
TT(f7_and_tf, OP_AND, true, false)
TT(f7_and_ft, OP_AND, false, true)
TT(f7_and_ff, OP_AND, false, false)
TT(f7_or_tt, OP_OR, true, true)
TT(f7_or_tf, OP_OR, true, false)
TT(f7_or_ft, OP_OR, false, true)
TT(f7_or_ff, OP_OR, false, false)
#undef TT

static AST_NODE *build_f7_not(void) { /* !true, !false, !!true */
  return prog7(P(BNOT(lit_b(true)), P(BNOT(lit_b(false)), P(BNOT(BNOT(lit_b(true))), NULL))));
}
static AST_NODE *build_f7_not_var(void) {
  return prog7(vdef("a", 10, true, B, lit_b(true), P(BNOT(UB("a", 10)), NULL)));
}
static AST_NODE *build_f7_vars(void) { /* val a = true; val b = false; print(a && b); print(a || b) */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(false),
                         P(BAND(UB("a", 10), UB("b", 11)), P(BOR(UB("a", 10), UB("b", 11)), NULL)))));
}
static AST_NODE *build_f7_rel(void) { /* 1 < 2 && 3 < 4 ; 1 < 2 && 4 < 3 */
  return prog7(P(BAND(LT_(lit_i(1), lit_i(2)), LT_(lit_i(3), lit_i(4))),
                 P(BAND(LT_(lit_i(1), lit_i(2)), LT_(lit_i(4), lit_i(3))), NULL)));
}
static AST_NODE *build_f7_prec_or_and(void) { /* true || false && false = true || (false && false) */
  return prog7(P(BOR(lit_b(true), BAND(lit_b(false), lit_b(false))), NULL));
}
static AST_NODE *build_f7_prec_and_or(void) { /* (true || false) && false */
  return prog7(P(BAND(BOR(lit_b(true), lit_b(false)), lit_b(false)), NULL));
}
static AST_NODE *build_f7_prec_not(void) { /* !false && true ; !a && b s a = true */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(true),
                         P(BAND(BNOT(lit_b(false)), lit_b(true)), P(BAND(BNOT(UB("a", 10)), UB("b", 11)), NULL)))));
}
static AST_NODE *build_f7_slide(void) { /* x < 0 && !u s x = -1, u = false (příklad ze slidů) */
  return prog7(vdef("x", 10, true, I, lit_i(-1),
                    vdef("u", 11, true, B, lit_b(false),
                         P(BAND(LT_(UI("x", 10), lit_i(0)), BNOT(UB("u", 11))), NULL))));
}
static AST_NODE *build_f7_eq(void) { /* true == true, true != false, false == true */
  return prog7(P(EQ_(lit_b(true), lit_b(true)),
                 P(NEQ_(lit_b(true), lit_b(false)), P(EQ_(lit_b(false), lit_b(true)), NULL))));
}
static AST_NODE *build_f7_eq_expr(void) { /* (a && b) == b ; (a || b) != b */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(false),
                         P(EQ_(BAND(UB("a", 10), UB("b", 11)), UB("b", 11)),
                           P(NEQ_(BOR(UB("a", 10), UB("b", 11)), UB("b", 11)), NULL)))));
}
static AST_NODE *build_f7_val(void) { /* val ok = a && b; print(ok) */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(true),
                         vdef("ok", 12, true, B, BAND(UB("a", 10), UB("b", 11)), P(UB("ok", 12), NULL)))));
}
static AST_NODE *build_f7_assign(void) { /* var f = true; f = f && false; print(f); f = !f; print(f) */
  return prog7(vdef("f", 10, false, B, lit_b(true),
                    assign(UB("f", 10), BAND(UB("f", 10), lit_b(false)),
                           P(UB("f", 10), assign(UB("f", 10), BNOT(UB("f", 10)), P(UB("f", 10), NULL))))));
}

/* zkrat: pravý operand je dělení nulou, vyhodnotit se nesmí */
static AST_NODE *build_f7_short_and(void) { /* b != 0 && 10 / b > 1, b = 0 */
  return prog7(vdef("b", 10, true, I, lit_i(0),
                    P(BAND(NEQ_(UI("b", 10), lit_i(0)), div_gt(10, "b", 10, 1)), PS("!", NULL))));
}
static AST_NODE *build_f7_short_and_eval(void) { /* totéž s b = 2: pravý operand se vyhodnotí (10 / 2 > 1) */
  return prog7(vdef("b", 10, true, I, lit_i(2),
                    P(BAND(NEQ_(UI("b", 10), lit_i(0)), div_gt(10, "b", 10, 1)), NULL)));
}
static AST_NODE *build_f7_short_or(void) { /* b == 0 || 10 / b > 1, b = 0 */
  return prog7(vdef("b", 10, true, I, lit_i(0),
                    P(BOR(EQ_(UI("b", 10), lit_i(0)), div_gt(10, "b", 10, 1)), PS("!", NULL))));
}
static AST_NODE *build_f7_short_or_eval(void) { /* b == 0 || 3 / b > 1 s b = 5: pravý operand se vyhodnotí, 0 > 1 */
  return prog7(vdef("b", 10, true, I, lit_i(5),
                    P(BOR(EQ_(UI("b", 10), lit_i(0)), div_gt(3, "b", 10, 1)), NULL)));
}
static AST_NODE *build_f7_short_chain(void) { /* (x != 0 && 5/x > 1) && 5/x > 2, x = 0 */
  return prog7(vdef("x", 10, true, I, lit_i(0),
                    P(BAND(BAND(NEQ_(UI("x", 10), lit_i(0)), div_gt(5, "x", 10, 1)), div_gt(5, "x", 10, 2)), NULL)));
}
static AST_NODE *build_f7_short_chain_right(void) { /* x != 0 && (5/x > 1 && 5/x > 2), x = 0 */
  return prog7(vdef("x", 10, true, I, lit_i(0),
                    P(BAND(NEQ_(UI("x", 10), lit_i(0)), BAND(div_gt(5, "x", 10, 1), div_gt(5, "x", 10, 2))), NULL)));
}
static AST_NODE *build_f7_short_or_chain(void) { /* (x == 0 || 5/x > 1) || 5/x > 2, x = 0 */
  return prog7(vdef("x", 10, true, I, lit_i(0),
                    P(BOR(BOR(EQ_(UI("x", 10), lit_i(0)), div_gt(5, "x", 10, 1)), div_gt(5, "x", 10, 2)), NULL)));
}
static AST_NODE *build_f7_short_mixed(void) { /* (x == 0 || 5/x > 1) && !(x != 0 && 5/x > 1), x = 0 */
  AST_NODE *l = BOR(EQ_(UI("x", 10), lit_i(0)), div_gt(5, "x", 10, 1));
  AST_NODE *r = BNOT(BAND(NEQ_(UI("x", 10), lit_i(0)), div_gt(5, "x", 10, 1)));
  return prog7(vdef("x", 10, true, I, lit_i(0), P(BAND(l, r), NULL)));
}
static AST_NODE *build_f7_short_loop(void) { /* while i < 3 && 6 / (3 - i) > 0: pro i = 3 by se dělilo nulou */
  AST_NODE *div = bin(OP_DIV, lit_i(6), bin(OP_SUB, lit_i(3), UI("i", 10), I), I);
  AST_NODE *cond = BAND(LT_(UI("i", 10), lit_i(3)), GT_(div, lit_i(0)));
  return prog7(vdef("i", 10, false, I, lit_i(0),
                    while_stmt(cond, P(UI("i", 10), inc("i", 10, NULL)), PS("|", NULL))));
}
static AST_NODE *build_f7_short_if(void) { /* if x != 0 && 10 / x > 1 then yes else no, x = 0 */
  return prog7(vdef("x", 10, true, I, lit_i(0),
                    if_stmt(BAND(NEQ_(UI("x", 10), lit_i(0)), div_gt(10, "x", 10, 1)), PS("yes", NULL),
                            PS("no", NULL), NULL)));
}
static AST_NODE *build_f7_many(void) { /* víc && a || v jedné funkci: (a && b) || (c && d) || (a && !d) */
  AST_NODE *e = BOR(BOR(BAND(UB("a", 10), UB("b", 11)), BAND(UB("c", 12), UB("d", 13))),
                    BAND(UB("a", 10), BNOT(UB("d", 13))));
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(false),
                         vdef("c", 12, true, B, lit_b(true),
                              vdef("d", 13, true, B, lit_b(false), P(e, P(BAND(UB("a", 10), UB("b", 11)), NULL)))))));
}
static AST_NODE *build_f7_nested_tmp(void) { /* ((a && b) && (c && d)): vnořené výrazy si nesmí přepisovat pomocnou proměnnou */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(true),
                         vdef("c", 12, true, B, lit_b(true),
                              vdef("d", 13, true, B, lit_b(false),
                                   P(BAND(BAND(UB("a", 10), UB("b", 11)), BAND(UB("c", 12), UB("d", 13))),
                                     P(BOR(BOR(UB("d", 13), UB("d", 13)), BAND(UB("a", 10), UB("c", 12))), NULL)))))));
}

/* podmínky */
static AST_NODE *build_f7_if_andor(void) { /* if (a && b) || c then T else F */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("b", 11, true, B, lit_b(false),
                         vdef("c", 12, true, B, lit_b(true),
                              if_stmt(BOR(BAND(UB("a", 10), UB("b", 11)), UB("c", 12)), PS("T", NULL), PS("F", NULL), NULL)))));
}
static AST_NODE *build_f7_while_and(void) { /* while i < 5 && ok: konec, až ok = false (if bez else) */
  AST_NODE *stop = if_stmt(EQ_(UI("i", 10), lit_i(3)), assign(UB("ok", 11), lit_b(false), NULL), NULL, NULL);
  AST_NODE *body = P(UI("i", 10), inc("i", 10, stop));
  return prog7(vdef("i", 10, false, I, lit_i(0),
                    vdef("ok", 11, false, B, lit_b(true),
                         while_stmt(BAND(LT_(UI("i", 10), lit_i(5)), UB("ok", 11)), body, NULL))));
}
static AST_NODE *build_f7_if_noelse(void) { /* if x < 0 then neg (bez else); if x > 5 then big */
  return prog7(vdef("x", 10, true, I, lit_i(-1),
                    if_stmt(LT_(UI("x", 10), lit_i(0)), PS("neg", NULL), NULL,
                            if_stmt(GT_(UI("x", 10), lit_i(5)), PS("big", NULL), NULL, PS("end", NULL)))));
}
static AST_NODE *build_f7_else_if(void) { /* if x < 0 && !u then opt1 else if x == 0 then print(false), x = 0 */
  AST_NODE *second = if_stmt(EQ_(UI("x", 10), lit_i(0)), P(lit_b(false), NULL), NULL, NULL);
  return prog7(vdef("x", 10, true, I, lit_i(0),
                    vdef("u", 11, true, B, lit_b(false),
                         if_stmt(BAND(LT_(UI("x", 10), lit_i(0)), BNOT(UB("u", 11))), PS("opt1", NULL), second, NULL))));
}
static AST_NODE *build_f7_else_if_chain(void) { /* x = 5: neg / zero / pos */
  AST_NODE *e2 = if_stmt(EQ_(UI("x", 10), lit_i(0)), PS("zero", NULL), PS("pos", NULL), NULL);
  return prog7(vdef("x", 10, true, I, lit_i(5), if_stmt(LT_(UI("x", 10), lit_i(0)), PS("neg", NULL), e2, NULL)));
}

/* funkce s Boolean */
static AST_NODE *build_f7_param(void) { /* flip(x: Boolean): Boolean = return !x */
  AST_NODE *f = fdef("flip", B, par("x", 1, B, NULL), fret(BNOT(UB("x", 1)), NULL), NULL);
  return make_program(main_fn(vdef("r", 10, true, B, fcall("flip", lit_b(true), B), P(UB("r", 10), NULL)), f));
}
static AST_NODE *build_f7_param_two(void) { /* both(a, b) = return a && b */
  AST_NODE *f = fdef("both", B, par("a", 1, B, par("b", 2, B, NULL)), fret(BAND(UB("a", 1), UB("b", 2)), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, B, fcall("both", a2(lit_b(true), lit_b(false)), B),
           vdef("s", 11, true, B, fcall("both", a2(lit_b(true), lit_b(true)), B),
                P(UB("r", 10), P(UB("s", 11), NULL)))),
      f));
}
static AST_NODE *build_f7_ret_last(void) { /* isPos(n: Int): Boolean = n > 0 (poslední výraz těla) */
  AST_NODE *f = fdef("isPos", B, par("n", 1, I, NULL), flast(GT_(UI("n", 1), lit_i(0)), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, B, fcall("isPos", lit_i(3), B),
           vdef("s", 11, true, B, fcall("isPos", lit_i(-3), B), P(UB("r", 10), P(UB("s", 11), NULL)))),
      f));
}
static AST_NODE *build_f7_ret_if(void) { /* check(n): if n < 0 then return false else return true */
  AST_NODE *f = fdef("check", B, par("n", 1, I, NULL),
                     if_stmt(LT_(UI("n", 1), lit_i(0)), fret(lit_b(false), NULL), fret(lit_b(true), NULL), NULL), NULL);
  return make_program(main_fn(
      vdef("r", 10, true, B, fcall("check", lit_i(-1), B),
           vdef("s", 11, true, B, fcall("check", lit_i(1), B), P(UB("r", 10), P(UB("s", 11), NULL)))),
      f));
}
static AST_NODE *side_fn(AST_NODE *next) { /* side(): Boolean: vypíše X a vrátí true (vedlejší účinek) */
  return fdef("side", B, NULL, PS("X", fret(lit_b(true), NULL)), next);
}
static AST_NODE *build_f7_short_funexp(void) { /* false && side() nevolá side; true && side() ano */
  return make_program(main_fn(P(BAND(lit_b(false), fcall("side", NULL, B)), P(BAND(lit_b(true), fcall("side", NULL, B)), NULL)),
                              side_fn(NULL)));
}
static AST_NODE *build_f7_short_funexp_or(void) { /* true || side() nevolá side; false || side() ano */
  return make_program(main_fn(P(BOR(lit_b(true), fcall("side", NULL, B)), P(BOR(lit_b(false), fcall("side", NULL, B)), NULL)),
                              side_fn(NULL)));
}
static AST_NODE *build_f7_param_expr(void) { /* flip(a && b): argument je výraz (FUNEXP) */
  AST_NODE *f = fdef("flip", B, par("x", 1, B, NULL), fret(BNOT(UB("x", 1)), NULL), NULL);
  return make_program(main_fn(
      vdef("a", 10, true, B, lit_b(true),
           vdef("b", 11, true, B, lit_b(false), P(fcall("flip", BAND(UB("a", 10), UB("b", 11)), B), NULL))),
      f));
}

/* toStr a readBoolean */
static AST_NODE *build_f7_tostr(void) { /* toStr(a) a toStr(false) */
  return prog7(vdef("a", 10, true, B, lit_b(true),
                    vdef("s", 11, true, S, fcall("toStr", UB("a", 10), S),
                         vdef("t", 12, true, S, fcall("toStr", lit_b(false), S),
                              P(use("s", 11, S), P(use("t", 12, S), NULL))))));
}
static AST_NODE *build_f7_tostr_expr(void) { /* print(toStr(true && false)) (FUNEXP) */
  return prog7(P(fcall("toStr", BAND(lit_b(true), lit_b(false)), S), NULL));
}
static AST_NODE *build_f7_readbool(void) { return prog7(vdef("r", 10, true, B, fcall("readBoolean", NULL, B), P(UB("r", 10), NULL))); }
static AST_NODE *build_f7_readbool_two(void) { /* dva řádky: a && b a a || b */
  return prog7(vdef("a", 10, true, B, fcall("readBoolean", NULL, B),
                    vdef("b", 11, true, B, fcall("readBoolean", NULL, B),
                         P(BAND(UB("a", 10), UB("b", 11)), P(BOR(UB("a", 10), UB("b", 11)), NULL)))));
}
static AST_NODE *build_f7_readbool_if(void) { /* if readBoolean() then yes else no (přes proměnnou) */
  return prog7(vdef("r", 10, true, B, fcall("readBoolean", NULL, B),
                    if_stmt(UB("r", 10), PS("yes", NULL), PS("no", NULL), NULL)));
}
#undef BAND
#undef BOR
#undef BNOT
#undef UB
#undef UI
#undef LT_
#undef GT_
#undef EQ_
#undef NEQ_
#undef PS
#undef P
#undef I
#undef S
#undef B
#undef U

/* fáze 8: rozšíření CONTROL, cyklus for. Tvar uzlu podle ast.h (ast_for_create / ast_range_create):
 *   NODE_FOR:   left = rozsah (NODE_RANGE), right = tělo (řetěz příkazů přes next, může být NULL),
 *               third = stráže (řetěz výrazů přes next, nebo NULL), name = jméno řídicí proměnné,
 *               id = číslo řídicí proměnné od sémantiky (v kódu LF@jmeno$id)
 *   NODE_RANGE: left = od, right = do, third = krok (NULL = krok 1),
 *               inclusive = true pro "to" (konec se bere), false pro "until" (konec se nebere)
 * Řídicí proměnná je Int. Meze a krok se vyhodnotí jednou před cyklem. Krok 0 = chyba 40 za běhu.
 * "until ... by" gramatika nepovoluje (by jde jen za to), proto se netestuje. Záporný krok se netestuje
 * (zadání ho nevyžaduje, říká jen "nenulový krok").
 *
 * if jako výraz: stejný uzel NODE_IF (left = podmínka, right = větev then, third = větev else), jen
 * data_type je typ výsledku. Větev je řetěz příkazů, jehož POSLEDNÍ příkaz dává hodnotu: buď
 * NODE_EXPR_STMT s výrazem v left, nebo vnořený NODE_IF (else if má třetí větev rovnou NODE_IF).
 * Příkazy před posledním se jen provedou. Zadání: else je povinný, větve mají shodný typ (nebo Int a
 * Double: pak má uzel typ Double a Int větev se převede).
 * Čísla proměnných: řídicí proměnné cyklů mají 1 až 9, ostatní proměnné od 10. */
#define I DATA_INT
#define B DATA_BOOL
#define U DATA_UNIT
#define UI(name, id) use((name), (id), I)
#define PS(str, nx) print_stmt(lit_s(str), (nx))
#define P(e, nx) print_stmt((e), (nx))
#define PRI(nx) P(UI("i", 1), (nx)) /* print(i) s číslem 1 */
#define LT_(l, r) bin(OP_LT, (l), (r), B)
#define GT_(l, r) bin(OP_GT, (l), (r), B)
#define EQ_(l, r) bin(OP_EQ, (l), (r), B)
#define NEQ_(l, r) bin(OP_NEQ, (l), (r), B)
#define MUL_(l, r) bin(OP_MUL, (l), (r), I)
#define PLUS(l, r) add_int((l), (r))
#define LI(v) lit_i(v)

static AST_NODE *prog8(AST_NODE *body) { return make_program(main_fn(body, NULL)); }

static AST_NODE *for_stmt(const char *name, int id, AST_NODE *from, AST_NODE *to, AST_NODE *step,
                          bool inclusive, AST_NODE *guards, AST_NODE *body, AST_NODE *next) {
  AST_NODE *range = ast_range_create(from, to, step, NULL, inclusive);
  AST_NODE *n = ast_for_create(range, body, guards, next, (char *)name);
  n->id = id;
  return n;
}
/* for name <- a to b do body */
#define FT(nm, id, a, b, body, nx) for_stmt((nm), (id), (a), (b), NULL, true, NULL, (body), (nx))
/* for name <- a until b do body */
#define FU(nm, id, a, b, body, nx) for_stmt((nm), (id), (a), (b), NULL, false, NULL, (body), (nx))
/* for name <- a to b by step do body */
#define FS(nm, id, a, b, step, body, nx) for_stmt((nm), (id), (a), (b), (step), true, NULL, (body), (nx))
/* for name <- a to b if guards do body (stráže = řetěz výrazů) */
#define FG(nm, id, a, b, g, body, nx) for_stmt((nm), (id), (a), (b), NULL, true, (g), (body), (nx))
static AST_NODE *two_guards(AST_NODE *a, AST_NODE *b) {
  a->next = b;
  return a;
}
#define I1 UI("i", 1)
#define I2 UI("j", 2)

/* --- rozsahy bez kroku a bez stráží --- */
static AST_NODE *build_f8_to(void) { return prog8(FT("i", 1, LI(1), LI(3), PRI(NULL), NULL)); }
static AST_NODE *build_f8_until(void) { return prog8(FU("i", 1, LI(1), LI(4), PRI(NULL), NULL)); }
static AST_NODE *build_f8_to_once(void) { return prog8(FT("i", 1, LI(5), LI(5), PRI(NULL), NULL)); }
static AST_NODE *build_f8_until_empty(void) {
  return prog8(FU("i", 1, LI(3), LI(3), PS("x", NULL), PS(".", NULL)));
}
static AST_NODE *build_f8_to_empty(void) {
  return prog8(FT("i", 1, LI(5), LI(3), PS("x", NULL), PS(".", NULL)));
}
static AST_NODE *build_f8_around(void) { /* příkazy před cyklem a za ním */
  return prog8(PS("<", FT("i", 1, LI(1), LI(2), PRI(NULL), PS(">", NULL))));
}
static AST_NODE *build_f8_neg_lit(void) { /* for i <- -2 to 2 */
  return prog8(FT("i", 1, neg(LI(2), I), LI(2), PRI(NULL), NULL));
}
static AST_NODE *build_f8_expr_bounds(void) { /* for i <- 1 + 1 to 2 * 2 */
  return prog8(FT("i", 1, PLUS(LI(1), LI(1)), MUL_(LI(2), LI(2)), PRI(NULL), NULL));
}
static AST_NODE *build_f8_var_bounds(void) { /* val a = 3; val b = 5; for i <- a to b */
  return prog8(vdef("a", 10, true, I, LI(3), vdef("b", 11, true, I, LI(5),
                    FT("i", 1, UI("a", 10), UI("b", 11), PRI(NULL), NULL))));
}
static AST_NODE *build_f8_empty_body(void) { /* tělo je prázdné (right == NULL), generátor nesmí spadnout */
  return prog8(FT("i", 1, LI(1), LI(3), NULL, PS("x", NULL)));
}
static AST_NODE *build_f8_big(void) { /* var s = 0; for i <- 1 to 100 do s = s + i; print(s) */
  return prog8(vdef("s", 10, false, I, LI(0),
                    FT("i", 1, LI(1), LI(100), assign(UI("s", 10), PLUS(UI("s", 10), I1), NULL),
                       P(UI("s", 10), NULL))));
}

/* --- řídicí proměnná a tělo --- */
static AST_NODE *build_f8_use_i(void) { /* print(i * i) */
  return prog8(FT("i", 1, LI(1), LI(4), P(MUL_(I1, I1), NULL), NULL));
}
static AST_NODE *build_f8_sum(void) { /* var s = 0; for i <- 1 to 4 do s = s + i; print(s) */
  return prog8(vdef("s", 10, false, I, LI(0),
                    FT("i", 1, LI(1), LI(4), assign(UI("s", 10), PLUS(UI("s", 10), I1), NULL),
                       P(UI("s", 10), NULL))));
}
static AST_NODE *build_f8_body_var(void) { /* val d = i * 2 uvnitř těla (DEFVAR nahoře, ne v cyklu) */
  return prog8(FT("i", 1, LI(1), LI(3),
                  vdef("d", 10, true, I, MUL_(I1, LI(2)), P(UI("d", 10), NULL)), NULL));
}
static AST_NODE *build_f8_if_inside(void) { /* for i <- 1 to 4 do if i > 2 then print(i) */
  return prog8(FT("i", 1, LI(1), LI(4), if_stmt(GT_(I1, LI(2)), PRI(NULL), NULL, NULL), NULL));
}
static AST_NODE *build_f8_while_inside(void) { /* v každém kole nová proměnná k = 0 a while do i */
  return prog8(FT("i", 1, LI(1), LI(2),
                  vdef("k", 10, false, I, LI(0),
                       while_stmt(LT_(UI("k", 10), I1),
                                  P(UI("k", 10), assign(UI("k", 10), PLUS(UI("k", 10), LI(1)), NULL)),
                                  NULL)),
                  NULL));
}
static AST_NODE *build_f8_in_while(void) { /* while n < 2 do { for i <- 1 to 2 do print(i); n = n + 1 } */
  return prog8(vdef("n", 10, false, I, LI(0),
                    while_stmt(LT_(UI("n", 10), LI(2)),
                               FT("i", 1, LI(1), LI(2), PRI(NULL),
                                  assign(UI("n", 10), PLUS(UI("n", 10), LI(1)), NULL)),
                               NULL)));
}

/* --- víc cyklů --- */
static AST_NODE *build_f8_nested(void) { /* print(i * 10 + j) */
  return prog8(FT("i", 1, LI(1), LI(2),
                  FT("j", 2, LI(1), LI(2), P(PLUS(MUL_(I1, LI(10)), I2), NULL), NULL), NULL));
}
static AST_NODE *build_f8_nested3(void) { /* 2 x 2 x 2 průchodů */
  return prog8(FT("i", 1, LI(1), LI(2),
                  FT("j", 2, LI(1), LI(2), FT("k", 3, LI(1), LI(2), PS("x", NULL), NULL), NULL),
                  NULL));
}
static AST_NODE *build_f8_nested_dep(void) { /* vnitřní mez závisí na vnější proměnné: j <- 1 to i */
  return prog8(FT("i", 1, LI(1), LI(3),
                  FT("j", 2, LI(1), I1, P(I2, NULL), NULL), NULL));
}
static AST_NODE *build_f8_two_seq(void) { /* dva cykly za sebou: návěští ani DEFVAR se nesmí opakovat */
  return prog8(FT("i", 1, LI(1), LI(2), PRI(NULL),
                  FU("j", 2, LI(1), LI(3), P(I2, NULL), NULL)));
}
static AST_NODE *build_f8_same_name(void) { /* dva cykly se stejným jménem i, jiná čísla (1 a 2) */
  return prog8(FT("i", 1, LI(1), LI(2), PRI(NULL),
                  FT("i", 2, LI(1), LI(2), P(MUL_(UI("i", 2), LI(10)), NULL), NULL)));
}

/* --- krok --- */
static AST_NODE *build_f8_step(void) { /* 1 to 10 by 3 = 1 4 7 10 (konec se bere) */
  return prog8(FS("i", 1, LI(1), LI(10), LI(3), PRI(NULL), NULL));
}
static AST_NODE *build_f8_step_skip(void) { /* 1 to 10 by 4 = 1 5 9 (10 se nezastaví) */
  return prog8(FS("i", 1, LI(1), LI(10), LI(4), PRI(NULL), NULL));
}
static AST_NODE *build_f8_step_over(void) { /* krok větší než rozsah: jen první hodnota */
  return prog8(FS("i", 1, LI(1), LI(3), LI(10), PRI(NULL), NULL));
}
static AST_NODE *build_f8_step_expr(void) { /* by 1 + 2 */
  return prog8(FS("i", 1, LI(0), LI(8), PLUS(LI(1), LI(2)), PRI(NULL), NULL));
}
static AST_NODE *build_f8_step_var(void) { /* val s = 2; for i <- 1 to 7 by s */
  return prog8(vdef("s", 10, true, I, LI(2),
                    FS("i", 1, LI(1), LI(7), UI("s", 10), PRI(NULL), NULL)));
}
static AST_NODE *build_f8_step_once(void) { /* krok se čte jednou: s se v těle mění, ale krok zůstává 1 */
  return prog8(vdef("s", 10, false, I, LI(1),
                    FS("i", 1, LI(1), LI(9), UI("s", 10),
                       P(I1, assign(UI("s", 10), PLUS(UI("s", 10), LI(1)), NULL)), NULL)));
}
static AST_NODE *build_f8_bounds_once(void) { /* mez se čte jednou: n se v těle zvětšuje, cyklus běží do 3 */
  return prog8(vdef("n", 10, false, I, LI(3),
                    FT("i", 1, LI(1), UI("n", 10),
                       P(I1, assign(UI("n", 10), PLUS(UI("n", 10), LI(1)), NULL)),
                       P(UI("n", 10), NULL))));
}
static AST_NODE *build_f8_zero_var(void) { /* var s = 0; for i <- 1 to 3 by s: chyba 40, "a" se ještě vypíše */
  return prog8(PS("a", vdef("s", 10, false, I, LI(0),
                            FS("i", 1, LI(1), LI(3), UI("s", 10), PRI(NULL), PS("b", NULL)))));
}
static AST_NODE *build_f8_zero_expr(void) { /* krok s - 1 s s = 1 je nula */
  return prog8(vdef("s", 10, false, I, LI(1),
                    FS("i", 1, LI(1), LI(3), bin(OP_SUB, UI("s", 10), LI(1), I), PRI(NULL), NULL)));
}
static AST_NODE *build_f8_zero_empty(void) { /* i když se tělo nespustí (5 to 3), nulový krok je chyba */
  return prog8(vdef("s", 10, false, I, LI(0),
                    FS("i", 1, LI(5), LI(3), UI("s", 10), PS("x", NULL), NULL)));
}


/* --- stráže (if za generátorem; více stráží se skládá jako "a zároveň") --- */
static AST_NODE *build_f8_guard_one(void) { /* if i > 2 */
  return prog8(FG("i", 1, LI(1), LI(6), GT_(I1, LI(2)), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_two(void) { /* if i > 2, if i < 5 */
  return prog8(FG("i", 1, LI(1), LI(6), two_guards(GT_(I1, LI(2)), LT_(I1, LI(5))), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_none(void) { /* if false: tělo se nespustí, cyklus ale doběhne */
  return prog8(FG("i", 1, LI(1), LI(3), lit_b(false), PS("x", NULL), PS(".", NULL)));
}
static AST_NODE *build_f8_guard_last(void) { /* stráž neplatí až pro poslední hodnotu */
  return prog8(FG("i", 1, LI(1), LI(5), LT_(I1, LI(5)), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_and(void) { /* if i > 1 && i < 4 (zkrat z BOOLTHEN ve stráži) */
  return prog8(FG("i", 1, LI(1), LI(5), bin(OP_AND, GT_(I1, LI(1)), LT_(I1, LI(4)), B), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_or(void) { /* if i == 2 || i == 4 */
  return prog8(FG("i", 1, LI(1), LI(5), bin(OP_OR, EQ_(I1, LI(2)), EQ_(I1, LI(4)), B), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_var(void) { /* val skip = 3; if i != skip */
  return prog8(vdef("skip", 10, true, I, LI(3),
                    FG("i", 1, LI(1), LI(5), NEQ_(I1, UI("skip", 10)), PRI(NULL), NULL)));
}
static AST_NODE *build_f8_guard_step(void) { /* for i <- 1 to 10 by 2 if i > 4 */
  return prog8(for_stmt("i", 1, LI(1), LI(10), LI(2), true, GT_(I1, LI(4)), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_spec(void) { /* příklad ze zadání: 0 until 10, if i != 3, if i < 8 */
  return prog8(for_stmt("i", 1, LI(0), LI(10), NULL, false,
                        two_guards(NEQ_(I1, LI(3)), LT_(I1, LI(8))), PRI(NULL), NULL));
}
static AST_NODE *build_f8_guard_call(void) { /* stráž je volání funkce, která při každém zavolání vypíše g */
  AST_NODE *ok = fdef("ok", B, par("x", 1, I, NULL), PS("g", flast(lit_b(true), NULL)), NULL);
  return make_program(main_fn(
      FG("i", 10, LI(1), LI(2), fcall("ok", UI("i", 10), B), P(UI("i", 10), NULL), NULL), ok));
}

/* --- cyklus ve funkcích --- */
static AST_NODE *build_f8_func_sum(void) { /* def sum(n): Int = { var s = 0; for i <- 1 to n do s = s + i; s } */
  AST_NODE *sum = fdef("sum", I, par("n", 1, I, NULL),
                       vdef("s", 2, false, I, LI(0),
                            FT("i", 3, LI(1), UI("n", 1),
                               assign(UI("s", 2), PLUS(UI("s", 2), UI("i", 3)), NULL),
                               flast(UI("s", 2), NULL))),
                       NULL);
  return make_program(main_fn(
      P(fcall("sum", LI(4), I), PS(",", P(fcall("sum", LI(3), I), NULL))), sum));
}
static AST_NODE *build_f8_func_last_for(void) { /* Unit funkce, jejíž poslední příkaz je for */
  AST_NODE *show = fdef("show", U, par("n", 1, I, NULL),
                        FT("i", 2, LI(1), UI("n", 1), P(UI("i", 2), NULL), NULL), NULL);
  return make_program(main_fn(
      fcall_stmt(fcall("show", LI(2), U), PS("|", fcall_stmt(fcall("show", LI(3), U), NULL))), show));
}
static AST_NODE *build_f8_return_in_for(void) { /* return uprostřed cyklu: na zásobníku nesmí nic zůstat */
  AST_NODE *find = fdef("find", I, par("n", 1, I, NULL),
                        FT("i", 2, LI(1), LI(10),
                           if_stmt(EQ_(UI("i", 2), UI("n", 1)), fret(MUL_(UI("i", 2), LI(100)), NULL),
                                   NULL, NULL),
                           flast(LI(0), NULL)),
                        NULL);
  return make_program(main_fn(
      P(fcall("find", LI(3), I), PS(",", P(fcall("find", LI(20), I), NULL))), find));
}

/* --- if jako výraz (val a = if cond then x else y) --- */
#define E(e) flast((e), NULL) /* větev z jediného výrazu */
static AST_NODE *ife(AST_NODE *cond, AST_NODE *then, AST_NODE *els, DATA_TYPE t) {
  AST_NODE *n = ast_if_create(cond, then, els, NULL);
  n->data_type = t;
  return n;
}
static AST_NODE *build_f8_ife_true(void) { /* val a = if true then 1 else 2 */
  return prog8(vdef("a", 10, true, I, ife(lit_b(true), E(LI(1)), E(LI(2)), I), P(UI("a", 10), NULL)));
}
static AST_NODE *build_f8_ife_false(void) {
  return prog8(vdef("a", 10, true, I, ife(lit_b(false), E(LI(1)), E(LI(2)), I), P(UI("a", 10), NULL)));
}
static AST_NODE *abs_prog(long long xv) { /* příklad ze zadání: val abs = if x < 0 then 0 - x else x */
  return prog8(vdef("x", 10, false, I, LI(xv),
                    vdef("abs", 11, true, I,
                         ife(LT_(UI("x", 10), LI(0)), E(bin(OP_SUB, LI(0), UI("x", 10), I)), E(UI("x", 10)), I),
                         P(UI("abs", 11), NULL))));
}
static AST_NODE *build_f8_ife_abs_neg(void) { return abs_prog(-5); }
static AST_NODE *build_f8_ife_abs_pos(void) { return abs_prog(7); }
static AST_NODE *build_f8_ife_arg(void) { /* print(if x > 0 then 1 else 2): if jako argument */
  return prog8(vdef("x", 10, false, I, LI(3),
                    P(ife(GT_(UI("x", 10), LI(0)), E(LI(1)), E(LI(2)), I), NULL)));
}
static AST_NODE *build_f8_ife_str(void) { /* větve typu String */
  return prog8(vdef("s", 10, true, DATA_STRING, ife(lit_b(true), E(lit_s("yes")), E(lit_s("no")), DATA_STRING),
                    P(UI("s", 10), NULL)));
}
static AST_NODE *build_f8_ife_bool(void) { /* větve typu Boolean */
  return prog8(vdef("x", 10, false, I, LI(3),
                    vdef("b", 11, true, B, ife(GT_(UI("x", 10), LI(0)), E(lit_b(true)), E(lit_b(false)), B),
                         P(UI("b", 11), NULL))));
}
static AST_NODE *build_f8_ife_double(void) { /* větve typu Double */
  return prog8(vdef("d", 10, true, DATA_DOUBLE, ife(lit_b(true), E(lit_d(1.5)), E(lit_d(2.5)), DATA_DOUBLE),
                    P(UI("d", 10), NULL)));
}
static AST_NODE *build_f8_ife_mixed(void) { /* then Int, else Double: výsledek Double, 1 se převede na 1.0 */
  return prog8(vdef("d", 10, true, DATA_DOUBLE, ife(lit_b(true), E(LI(1)), E(lit_d(2.5)), DATA_DOUBLE),
                    P(UI("d", 10), NULL)));
}
static AST_NODE *build_f8_ife_mixed_else(void) { /* totéž, vybere se else větev (Double) */
  return prog8(vdef("d", 10, true, DATA_DOUBLE, ife(lit_b(false), E(LI(1)), E(lit_d(2.5)), DATA_DOUBLE),
                    P(UI("d", 10), NULL)));
}
static AST_NODE *build_f8_ife_mixed_rev(void) { /* then Double, else Int: else větev se převede na Double */
  return prog8(vdef("d", 10, true, DATA_DOUBLE, ife(lit_b(false), E(lit_d(2.5)), E(LI(1)), DATA_DOUBLE),
                    P(UI("d", 10), NULL)));
}
static AST_NODE *build_f8_ife_multi(void) { /* v then větvi je víc příkazů: print("a"), pak hodnota 1 */
  return prog8(vdef("r", 10, true, I, ife(lit_b(true), PS("a", E(LI(1))), PS("b", E(LI(2))), I),
                    P(UI("r", 10), NULL)));
}
static AST_NODE *build_f8_ife_multi_else(void) {
  return prog8(vdef("r", 10, true, I, ife(lit_b(false), PS("a", E(LI(1))), PS("b", E(LI(2))), I),
                    P(UI("r", 10), NULL)));
}
static AST_NODE *elseif_prog(long long xv) { /* if x < 0 then 0 else if x == 0 then 1 else 2 */
  return prog8(vdef("x", 10, false, I, LI(xv),
                    vdef("r", 11, true, I,
                         ife(LT_(UI("x", 10), LI(0)), E(LI(0)),
                             ife(EQ_(UI("x", 10), LI(0)), E(LI(1)), E(LI(2)), I), I),
                         P(UI("r", 11), NULL))));
}
static AST_NODE *build_f8_ife_elseif_first(void) { return elseif_prog(-1); }
static AST_NODE *build_f8_ife_elseif_mid(void) { return elseif_prog(0); }
static AST_NODE *build_f8_ife_elseif_last(void) { return elseif_prog(5); }
static AST_NODE *build_f8_ife_operand(void) { /* (if x > 0 then x else 0) + 10 */
  return prog8(vdef("x", 10, false, I, LI(3),
                    P(PLUS(ife(GT_(UI("x", 10), LI(0)), E(UI("x", 10)), E(LI(0)), I), LI(10)), NULL)));
}
static AST_NODE *build_f8_ife_operand_r(void) { /* 100 + if x > 0 then x else 0 (if jako pravý operand) */
  return prog8(vdef("x", 10, false, I, LI(3),
                    P(PLUS(LI(100), ife(GT_(UI("x", 10), LI(0)), E(UI("x", 10)), E(LI(0)), I)), NULL)));
}
static AST_NODE *build_f8_ife_nested(void) { /* if true then (if false then 1 else 2) else 3 */
  return prog8(vdef("r", 10, true, I,
                    ife(lit_b(true), ife(lit_b(false), E(LI(1)), E(LI(2)), I), E(LI(3)), I),
                    P(UI("r", 10), NULL)));
}
static AST_NODE *build_f8_ife_assign(void) { /* y = if x > 0 then 1 else 2 jako přiřazení */
  return prog8(vdef("x", 10, false, I, LI(3),
                    vdef("y", 11, false, I, LI(0),
                         assign(UI("y", 11), ife(GT_(UI("x", 10), LI(0)), E(LI(1)), E(LI(2)), I),
                                P(UI("y", 11), NULL)))));
}
static AST_NODE *build_f8_ife_two(void) { /* dva if výrazy v jedné funkci: návěští se nesmí opakovat */
  return prog8(vdef("a", 10, true, I, ife(lit_b(true), E(LI(1)), E(LI(2)), I),
                    vdef("b", 11, true, I, ife(lit_b(false), E(LI(3)), E(LI(4)), I),
                         P(UI("a", 10), P(UI("b", 11), NULL)))));
}
static AST_NODE *build_f8_ife_branch_var(void) { /* ve větvi je val t = 5 (DEFVAR musí být nahoře) */
  return prog8(vdef("r", 10, true, I,
                    ife(lit_b(true), vdef("t", 11, true, I, LI(5), E(PLUS(UI("t", 11), LI(1)))), E(LI(0)), I),
                    P(UI("r", 10), NULL)));
}
static AST_NODE *build_f8_ife_in_for(void) { /* for i <- 1 to 3 do print(if i > 2 then 1 else 0) */
  return prog8(FT("i", 1, LI(1), LI(4),
                  P(ife(GT_(I1, LI(2)), E(LI(1)), E(LI(0)), I), NULL), NULL));
}
static AST_NODE *build_f8_ife_for_val(void) { /* uvnitř těla: val r = if i == 2 then 20 else i */
  return prog8(FT("i", 1, LI(1), LI(3),
                  vdef("r", 10, true, I, ife(EQ_(I1, LI(2)), E(LI(20)), E(I1), I), P(UI("r", 10), NULL)),
                  NULL));
}
static AST_NODE *build_f8_ife_tail(void) { /* def sign(x): Int = if x < 0 then -1 else 1 (if je poslední příkaz funkce) */
  AST_NODE *sign = fdef("sign", I, par("x", 1, I, NULL),
                        ife(LT_(UI("x", 1), LI(0)), E(neg(LI(1), I)), E(LI(1)), I), NULL);
  return make_program(main_fn(
      P(fcall("sign", neg(LI(4), I), I), PS(",", P(fcall("sign", LI(4), I), NULL))), sign));
}
static AST_NODE *build_f8_ife_return(void) { /* return if x > 0 then 1 else 2 */
  AST_NODE *pick = fdef("pick", I, par("x", 1, I, NULL),
                        fret(ife(GT_(UI("x", 1), LI(0)), E(LI(1)), E(LI(2)), I), NULL), NULL);
  return make_program(main_fn(
      P(fcall("pick", LI(3), I), PS(",", P(fcall("pick", neg(LI(3), I), I), NULL))), pick));
}
static AST_NODE *side_int_fn(void) { /* def side(): Int = { print("S"); 1 } */
  return fdef("side", I, NULL, PS("S", flast(LI(1), NULL)), NULL);
}
static AST_NODE *build_f8_ife_lazy(void) { /* if false then side() else 2: side() se nezavolá, nevypíše S */
  return make_program(main_fn(
      vdef("r", 10, true, I, ife(lit_b(false), E(fcall("side", NULL, I)), E(LI(2)), I), P(UI("r", 10), NULL)),
      side_int_fn()));
}
static AST_NODE *build_f8_ife_lazy_run(void) { /* if true then side() else 2: side() se zavolá (S), pak hodnota 1 */
  return make_program(main_fn(
      vdef("r", 10, true, I, ife(lit_b(true), E(fcall("side", NULL, I)), E(LI(2)), I), P(UI("r", 10), NULL)),
      side_int_fn()));
}
#undef E
#undef LT_
#undef GT_
#undef EQ_
#undef NEQ_
#undef MUL_
#undef PLUS
#undef LI
#undef I1
#undef I2
#undef FT
#undef FU
#undef FS
#undef FG
#undef PRI
#undef PS
#undef P
#undef UI
#undef I
#undef B
#undef U

typedef struct {
  const char *name;
  int phase;
  const char *entry;      /* funkce s @main */
  const char *others[3];  /* ostatní funkce, ukončeno NULL */
  AST_NODE *(*build)(void);
  const char *desc;
  const char *expect_out; /* co má program vypsat na stdout */
  const char *must[4];    /* slova (tokeny), která se musí v kódu objevit; ukončeno NULL */
  const char *input;      /* co program dostane na standardním vstupu (NULL = nic) */
  int exit_code;          /* s jakým kódem má program skončit (0 = bez chyby) */
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

    {"f4_if_true", 4, "main", {NULL}, build_f4_if_true, "if true { A }", "A", {NULL}},
    {"f4_if_false", 4, "main", {NULL}, build_f4_if_false, "if false { A }; Z: větev se přeskočí", "Z", {NULL}},
    {"f4_if_else_true", 4, "main", {NULL}, build_f4_if_else_true, "if true { A } else { B }", "A", {NULL}},
    {"f4_if_else_false", 4, "main", {NULL}, build_f4_if_else_false, "if false { A } else { B }", "B", {NULL}},
    {"f4_if_cmp", 4, "main", {NULL}, build_f4_if_cmp, "<, if 1 < 2 { A } else { B }, >", "<A>", {NULL}},
    {"f4_if_cmp_else", 4, "main", {NULL}, build_f4_if_cmp_else, "<, if 2 < 1 { A } else { B }, >", "<B>", {NULL}},
    {"f4_if_multi", 4, "main", {NULL}, build_f4_if_multi,
     "více příkazů ve větvích: if true { 1 2 } else { 3 4 }", "12", {NULL}},
    {"f4_if_multi_else", 4, "main", {NULL}, build_f4_if_multi_else,
     "více příkazů ve větvích: if false { 1 2 } else { 3 4 }", "34", {NULL}},
    {"f4_if_two_ifs", 4, "main", {NULL}, build_f4_if_two_ifs, "dvě if za sebou (návěští se nesmí opakovat)", "AD",
     {NULL}},
    {"f4_if_three", 4, "main", {NULL}, build_f4_if_three, "tři if bez else za sebou", "AB", {NULL}},
    {"f4_if_nested", 4, "main", {NULL}, build_f4_if_nested, "if uvnitř then větve", "Y", {NULL}},
    {"f4_if_nested_else", 4, "main", {NULL}, build_f4_if_nested_else,
     "if uvnitř else větve a za ním další příkaz ve větvi", "Y!", {NULL}},
    {"f4_if_nested_deep", 4, "main", {NULL}, build_f4_if_nested_deep, "tři if do sebe a za vnějším příkaz", "AB",
     {NULL}},
    {"f4_if_nested_both", 4, "main", {NULL}, build_f4_if_nested_both, "if jak ve then, tak v else větvi", "A",
     {NULL}},
    {"f4_else_if", 4, "main", {NULL}, build_f4_else_if, "if false A else if true B else C", "B", {NULL}},
    {"f4_else_if_last", 4, "main", {NULL}, build_f4_else_if_last, "if false A else if false B else C", "C",
     {NULL}},
    {"f4_else_if_first", 4, "main", {NULL}, build_f4_else_if_first,
     "if true A else if true B else C: else if se přeskočí", "A", {NULL}},
    {"f4_else_if_chain", 4, "main", {NULL}, build_f4_else_if_chain,
     "var x = 2; if x == 1 .. else if x == 2 .. else if x == 3 .. else", "B", {NULL}},
    {"f4_else_if_no_else", 4, "main", {NULL}, build_f4_else_if_no_else,
     "else if bez posledního else, nic neplatí, pokračuje se Z", "Z", {NULL}},
    {"f4_if_var", 4, "main", {NULL}, build_f4_if_var, "var x = 5; if x > 3 { big } else { small }", "big",
     {"LF@x$1", NULL}},
    {"f4_if_var_else", 4, "main", {NULL}, build_f4_if_var_else, "var x = 1; if x > 3 { big } else { small }",
     "small", {NULL}},
    {"f4_if_assign", 4, "main", {NULL}, build_f4_if_assign, "var x = 1; if true { x = 2 }; print(x)", "2",
     {NULL}},
    {"f4_if_assign_skip", 4, "main", {NULL}, build_f4_if_assign_skip,
     "var x = 1; if false { x = 3 }; print(x): zůstane 1", "1", {NULL}},
    {"f4_if_assign_else", 4, "main", {NULL}, build_f4_if_assign_else,
     "if 2 < 1 { x = 1 } else { x = 2 }; print(x)", "2", {NULL}},
    {"f4_if_var_in_branch", 4, "main", {NULL}, build_f4_if_var_in_branch,
     "val y a val z deklarované uvnitř větví (DEFVAR musí být nahoře)", "4", {"LF@y$1", "LF@z$2", NULL}},
    {"f4_if_cond_expr", 4, "main", {NULL}, build_f4_if_cond_expr,
     "podmínky jsou výrazy: (1 + 2) == 3 a 2 * 3 > 7", "AD", {NULL}},
    {"f4_if_bool_var", 4, "main", {NULL}, build_f4_if_bool_var, "val b = true; if b { A } else { B }", "A", {NULL}},
    {"f4_if_bool_var_cmp", 4, "main", {NULL}, build_f4_if_bool_var_cmp, "val b = 1 < 0; if b { A } else { B }", "B",
     {NULL}},
    {"f4_if_cmp_ops", 4, "main", {NULL}, build_f4_if_cmp_ops,
     "<=, >= a != v podmínce (GTS+NOTS, LTS+NOTS, EQS+NOTS)", "ADF", {NULL}},
    {"f4_if_string_cmp", 4, "main", {NULL}, build_f4_if_string_cmp,
     "podmínka nad řetězci: \"a\" < \"b\", \"ab\" == \"ac\"", "AD", {NULL}},
    {"f4_if_mixed", 4, "main", {NULL}, build_f4_if_mixed, "podmínka 2 < 2.5 (Int a Double, INT2FLOATS)", "A",
     {NULL}},
    {"f4_if_two_functions", 4, "main", {"helper", NULL}, build_f4_if_two_functions,
     "if v main i v helper: návěští se nesmí shodovat", "x", {NULL}},
    {"f4_if_helper_var", 4, "main", {"helper", NULL}, build_f4_if_helper_var,
     "proměnná ve větvi ve druhé funkci (ta má mít vlastní DEFVAR)", "x", {"LF@h$1", "LF@k$2", NULL}},

    {"f4_while_count", 4, "main", {NULL}, build_f4_while_count, "var i = 1; while i < 4 { print(i); i = i + 1 }",
     "123", {NULL}},
    {"f4_while_zero", 4, "main", {NULL}, build_f4_while_zero, "while false { X }; ok: tělo se nevykoná", "ok",
     {NULL}},
    {"f4_while_1_10", 4, "main", {NULL}, build_f4_while_1_10, "čísla 1 až 10 cyklem", "12345678910", {NULL}},
    {"f4_while_sum", 4, "main", {NULL}, build_f4_while_sum, "s = 1 + 2 + 3 + 4 + 5 v cyklu", "15", {NULL}},
    {"f4_while_countdown", 4, "main", {NULL}, build_f4_while_countdown, "while n > 0: 3 2 1", "321", {NULL}},
    {"f4_while_neq", 4, "main", {NULL}, build_f4_while_neq, "while i != 3 (EQS a NOTS v podmínce)", "3", {NULL}},
    {"f4_while_cond_expr", 4, "main", {NULL}, build_f4_while_cond_expr, "while i * i < 20: i skončí na 5", "5",
     {NULL}},
    {"f4_while_nested", 4, "main", {NULL}, build_f4_while_nested, "cyklus v cyklu: 11 12 21 22", "11122122",
     {NULL}},
    {"f4_while_if", 4, "main", {NULL}, build_f4_while_if, "if v těle cyklu: jen sudá čísla do 6", "246", {NULL}},
    {"f4_if_while", 4, "main", {NULL}, build_f4_if_while, "while uvnitř then větve", "3", {NULL}},
    {"f4_while_var_in_body", 4, "main", {NULL}, build_f4_while_var_in_body,
     "val t uvnitř těla cyklu (DEFVAR musí být nahoře, ne v cyklu)", "246", {"LF@i$1", "LF@t$2", NULL}},
    {"f4_while_two", 4, "main", {NULL}, build_f4_while_two, "dva while za sebou (návěští se nesmí opakovat)",
     "0123", {NULL}},
    {"f4_while_around", 4, "main", {NULL}, build_f4_while_around, "příkaz před cyklem i za ním", "<12>", {NULL}},
    {"f4_while_string", 4, "main", {NULL}, build_f4_while_string,
     "s = s + \"a\" v cyklu (pomocné proměnné pro CONCAT se použijí víckrát)", "aaa", {NULL}},
    {"f4_while_helper", 4, "main", {"helper", NULL}, build_f4_while_helper,
     "while v main i v helper: návěští se nesmí shodovat", "2", {"LF@i$1", "LF@k$2", NULL}},
    {"f4_while_in_else", 4, "main", {NULL}, build_f4_while_in_else, "proměnná a cyklus uvnitř else větve", "01",
     {"LF@i$1", NULL}},
    {"f4_while_if_else", 4, "main", {NULL}, build_f4_while_if_else, "if else v těle cyklu: aabb", "aabb", {NULL}},
    {"f4_while_double", 4, "main", {NULL}, build_f4_while_double, "x = 1.0; while x < 3.0 { x = x + 1.0 }",
     "0x1.8p+1", {NULL}},
    {"f4_while_mixed", 4, "main", {NULL}, build_f4_while_mixed, "while i < 2.5 (Int a Double v podmínce)", "3",
     {NULL}},

    {"f5_call_unit", 5, "main", {"hello", NULL}, build_f5_call_unit, "main volá hello(): Unit funkce, výsledek se zahodí",
     "Hi", {NULL}},
    {"f5_call_twice", 5, "main", {"hello", NULL}, build_f5_call_twice, "dvě volání téže funkce za sebou", "HiHi", {NULL}},
    {"f5_call_chain", 5, "main", {"a", "b", NULL}, build_f5_call_chain, "main volá a, a volá b", "<b>", {NULL}},
    {"f5_call_main_last", 5, "main", {"hello", NULL}, build_f5_call_main_last, "hello je definovaná před main", "Hi",
     {NULL}},
    {"f5_entry_run", 5, "run", {"hello", NULL}, build_f5_entry_run, "@main funkce se jmenuje run a volá hello", "Hi",
     {NULL}},

    {"f5_param_one", 5, "main", {"show", NULL}, build_f5_param_one, "show(42): jeden parametr", "42",
     {"LF@x$1", NULL}},
    {"f5_param_two", 5, "main", {"pdiff", NULL}, build_f5_param_two, "pdiff(10, 3) = 10 - 3: pořadí argumentů", "7",
     {"LF@a$1", "LF@b$2", NULL}},
    {"f5_param_types", 5, "main", {"show3", NULL}, build_f5_param_types, "show3(\"x\", 5, 2.5): tři parametry tří typů",
     "x50x1.4p+1", {"LF@s$1", "LF@n$2", "LF@d$3"}},
    {"f5_param_expr", 5, "main", {"pdiff", NULL}, build_f5_param_expr, "pdiff(1 + 2, 3 * 4): argumenty jsou výrazy", "-9",
     {NULL}},
    {"f5_param_vars", 5, "main", {"pdiff", NULL}, build_f5_param_vars, "val a = 10; val b = 4; pdiff(a, b)", "6", {NULL}},
    {"f5_param_frames", 5, "main", {"show", NULL}, build_f5_param_frames,
     "var x = 1; show(5); print(x): proměnná volajícího přežije volání", "51", {NULL}},
    {"f5_param_concat", 5, "main", {"greet", NULL}, build_f5_param_concat,
     "greet(\"Bob\"): spojení řetězců s parametrem ve volané funkci", "Hi Bob", {NULL}},
    {"f5_param_loop", 5, "main", {"count", NULL}, build_f5_param_loop,
     "count(3): parametr a lokální proměnná s cyklem", "123", {"LF@n$1", "LF@i$2", NULL}},
    {"f5_param_if", 5, "main", {"sign", NULL}, build_f5_param_if, "sign(-3), sign(2): if nad parametrem", "negpos",
     {NULL}},
    {"f5_param_four", 5, "main", {"sum4", NULL}, build_f5_param_four, "sum4(1, 2, 3, 4) = 1234: čtyři parametry", "1234",
     {NULL}},
    {"f5_param_mixed", 5, "main", {"scale", NULL}, build_f5_param_mixed,
     "scale(1.5, 2): Double a Int parametr, INT2FLOATS ve volané funkci", "0x1.8p+1", {NULL}},

    {"f5_ret_int", 5, "main", {"twice", NULL}, build_f5_ret_int, "val r = twice(21): return x * 2", "42", {NULL}},
    {"f5_ret_last", 5, "main", {"twice", NULL}, build_f5_ret_last, "hodnota z posledního výrazu těla, bez return", "42",
     {NULL}},
    {"f5_ret_last_call", 5, "main", {"wrapper", "twice", NULL}, build_f5_ret_last_call,
     "poslední výraz těla je volání jiné funkce", "8", {NULL}},
    {"f5_ret_last_after", 5, "main", {"incr", NULL}, build_f5_ret_last_after,
     "příkaz print(v) a za ním poslední výraz x + 1", "v2", {NULL}},
    {"f5_ret_string", 5, "main", {"greeting", NULL}, build_f5_ret_string, "funkce vrací String", "Hi Ann", {NULL}},
    {"f5_ret_double", 5, "main", {"half", NULL}, build_f5_ret_double, "funkce vrací Double: half(5.0)", "0x1.4p+1",
     {NULL}},
    {"f5_ret_bool", 5, "main", {"isPos", NULL}, build_f5_ret_bool, "funkce vrací Boolean: isPos(3), isPos(-1)",
     "truefalse", {NULL}},
    {"f5_ret_early", 5, "main", {"abs", NULL}, build_f5_ret_early, "return uvnitř if: abs(-5), abs(7)", "57", {NULL}},
    {"f5_ret_early_unit", 5, "main", {"check", NULL}, build_f5_ret_early_unit,
     "prázdný return v Unit funkci (vrací nil@nil)", "posneg", {NULL}},
    {"f5_ret_in_loop", 5, "main", {"firstOver", NULL}, build_f5_ret_in_loop,
     "return uvnitř while uvnitř if: firstOver(50) = 8", "8", {NULL}},
    {"f5_ret_two_calls", 5, "main", {"twice", NULL}, build_f5_ret_two_calls, "a = twice(5), b = twice(6), print(a + b)",
     "22", {NULL}},
    {"f5_ret_discard", 5, "main", {"twice", NULL}, build_f5_ret_discard,
     "twice(5) jako příkaz: vrácená hodnota se zahodí", "ok", {NULL}},
    {"f5_call_in_loop", 5, "main", {"show", NULL}, build_f5_call_in_loop, "volání uvnitř while: zásobník se nehromadí",
     "123", {NULL}},
    {"f5_call_in_if", 5, "main", {"show", NULL}, build_f5_call_in_if, "volání ve větvích if", "14", {NULL}},

    {"f5_fact", 5, "main", {"fact", NULL}, build_f5_fact, "rekurzivní faktoriál: fact(5)", "120",
     {"LF@n$1", "LF@m$2", "LF@r$3"}},
    {"f5_fact_10", 5, "main", {"fact", NULL}, build_f5_fact_10, "fact(10): hloubka rekurze 10", "3628800", {NULL}},
    {"f5_fib", 5, "main", {"fib", NULL}, build_f5_fib, "fib(10) = 55: dvě rekurzivní volání v těle", "55", {NULL}},
    {"f5_sum_rec", 5, "main", {"sumTo", NULL}, build_f5_sum_rec, "sumTo(20) = 210: hloubka rekurze 20", "210", {NULL}},
    {"f5_countdown", 5, "main", {"countdown", NULL}, build_f5_countdown,
     "rekurze v Unit funkci s voláním jako příkazem: countdown(3)", "321", {NULL}},
    {"f5_mutual", 5, "main", {"isEven", "isOdd", NULL}, build_f5_mutual,
     "vzájemná rekurze isEven a isOdd (volání funkce definované později)", "truefalse", {NULL}},
    {"f6_strlen", 6, "main", {NULL}, build_f6_strlen,
     "strLength(\"Ahoj\") = 4", "4", {NULL}, NULL, 0},
    {"f6_strlen_empty", 6, "main", {NULL}, build_f6_strlen_empty,
     "strLength(\"\") = 0", "0", {NULL}, NULL, 0},
    {"f6_strlen_var", 6, "main", {NULL}, build_f6_strlen_var,
     "strLength nad proměnnou", "5", {NULL}, NULL, 0},
    {"f6_strlen_special", 6, "main", {NULL}, build_f6_strlen_special,
     "strLength(\"a b\\nc\"): mezera a nový řádek se počítají", "5", {NULL}, NULL, 0},
    {"f6_toint", 6, "main", {NULL}, build_f6_toint,
     "toInt(3.99) = 3, toInt(-2.7) = -2 (osekne, nezaokrouhluje)", "3-2", {NULL}, NULL, 0},
    {"f6_toint_var", 6, "main", {NULL}, build_f6_toint_var,
     "toInt nad proměnnou Double: 7.5 -> 7", "7", {NULL}, NULL, 0},
    {"f6_chr", 6, "main", {NULL}, build_f6_chr,
     "chr(72), chr(105) = Hi", "Hi", {NULL}, NULL, 0},
    {"f6_chr_edges", 6, "main", {NULL}, build_f6_chr_edges,
     "chr(10) je nový řádek, chr(127) má délku 1", "\n1", {NULL}, NULL, 0},
    {"f6_ord", 6, "main", {NULL}, build_f6_ord,
     "ord(\"A\", 0) = 65, ord(\"abc\", 2) = 99", "6599", {NULL}, NULL, 0},
    {"f6_ord_out", 6, "main", {NULL}, build_f6_ord_out,
     "ord mimo řetězec nebo nad prázdným řetězcem = -1", "-1-1-1", {NULL}, NULL, 0},
    {"f6_ord_var", 6, "main", {NULL}, build_f6_ord_var,
     "ord(s, i) s proměnnými: ord(\"xyz\", 1) = 121", "121", {NULL}, NULL, 0},
    {"f6_substr", 6, "main", {NULL}, build_f6_substr,
     "subStr(\"abcdef\", 1, 4) = bcd", "bcd", {NULL}, NULL, 0},
    {"f6_substr_full", 6, "main", {NULL}, build_f6_substr_full,
     "subStr(\"abc\", 0, 3): celý řetězec", "abc", {NULL}, NULL, 0},
    {"f6_substr_last", 6, "main", {NULL}, build_f6_substr_last,
     "subStr(\"abc\", 2, 3): poslední znak", "c", {NULL}, NULL, 0},
    {"f6_substr_empty", 6, "main", {NULL}, build_f6_substr_empty,
     "subStr(\"abc\", 1, 1): prázdný řetězec, ne chyba", "<>", {NULL}, NULL, 0},
    {"f6_substr_var", 6, "main", {NULL}, build_f6_substr_var,
     "subStr s proměnnými: Hello World od 6 do 11", "World", {NULL}, NULL, 0},
    {"f6_substr_two", 6, "main", {NULL}, build_f6_substr_two,
     "dvě volání subStr s různými mezemi: ab a def", "abdef", {NULL}, NULL, 0},
    {"f6_tostr_int", 6, "main", {NULL}, build_f6_tostr_int,
     "toStr(42), toStr(-7)", "42-7", {NULL}, NULL, 0},
    {"f6_tostr_double", 6, "main", {NULL}, build_f6_tostr_double,
     "toStr(2.5) = 2.50, toStr(3.14159) = 3.14 (%.2f, ne %a)", "2.503.14", {NULL}, NULL, 0},
    {"f6_tostr_double_neg", 6, "main", {NULL}, build_f6_tostr_double_neg,
     "toStr(-1.5), toStr(0.0), toStr(100.0)", "-1.500.00100.00", {NULL}, NULL, 0},
    {"f6_tostr_string", 6, "main", {NULL}, build_f6_tostr_string,
     "toStr(\"abc\") = abc", "abc", {NULL}, NULL, 0},
    {"f6_tostr_var", 6, "main", {NULL}, build_f6_tostr_var,
     "toStr(n) a spojení s textem: n=12", "n=12", {NULL}, NULL, 0},
    {"f6_tostr_vars", 6, "main", {NULL}, build_f6_tostr_vars,
     "toStr nad proměnnou Int, Double i String", "50.50t", {NULL}, NULL, 0},
    {"f6_twice_same", 6, "main", {NULL}, build_f6_twice_same,
     "stejné vestavěné funkce víckrát: každá se definuje jednou", "2132", {NULL}, NULL, 0},
    {"f6_all", 6, "main", {NULL}, build_f6_all,
     "subStr, strLength, ord, chr, toStr a toInt v jednom programu", "3101e39", {NULL}, NULL, 0},
    {"f6_loop", 6, "main", {NULL}, build_f6_loop,
     "vestavěné funkce v cyklu: každý znak hello o jedna dál", "ifmmp", {NULL}, NULL, 0},
    {"f6_in_func", 6, "main", {NULL}, build_f6_in_func,
     "uživatelská funkce vrací výsledek strLength (return)", "4", {NULL}, NULL, 0},
    {"f6_in_func_last", 6, "main", {NULL}, build_f6_in_func_last,
     "uživatelská funkce vrací strLength jako poslední výraz", "4", {NULL}, NULL, 0},
    {"f6_discard", 6, "main", {NULL}, build_f6_discard,
     "vestavěná funkce jako příkaz: výsledek se zahodí", "ok", {NULL}, NULL, 0},
    {"f6_nested", 6, "main", {NULL}, build_f6_nested,
     "print(toStr(strLength(subStr(\"abcd\", 1, 3)))) = 2 (FUNEXP)", "2", {NULL}, NULL, 0},
    {"f6_nested_user", 6, "main", {"show", NULL}, build_f6_nested_user,
     "show(strLength(\"abc\")): vestavěné volání jako argument (FUNEXP)", "3", {NULL}, NULL, 0},
    {"f6_nested_expr", 6, "main", {NULL}, build_f6_nested_expr,
     "strLength(\"ab\") + strLength(\"cde\") * 2 = 8 (FUNEXP)", "8", {NULL}, NULL, 0},
    {"f6_nested_args", 6, "main", {NULL}, build_f6_nested_args,
     "subStr(\"abcdef\", ord(\"A\", 0) - 64, 4) = bcd (FUNEXP)", "bcd", {NULL}, NULL, 0},
    {"f6_substr_neg_start", 6, "main", {NULL}, build_f6_substr_neg_start,
     "subStr(\"abc\", -1, 2): start < 0 -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_substr_neg_end", 6, "main", {NULL}, build_f6_substr_neg_end,
     "subStr(\"abc\", 1, -1): end < 0 -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_substr_start_gt_end", 6, "main", {NULL}, build_f6_substr_start_gt_end,
     "subStr(\"abc\", 2, 1): start > end -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_substr_start_len", 6, "main", {NULL}, build_f6_substr_start_len,
     "subStr(\"abc\", 3, 3): start >= délka -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_substr_end_len", 6, "main", {NULL}, build_f6_substr_end_len,
     "subStr(\"abc\", 0, 4): end > délka -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_substr_empty_str", 6, "main", {NULL}, build_f6_substr_empty_str,
     "subStr(\"\", 0, 0): start >= délka -> kód 40", "a", {NULL}, NULL, 40},
    {"f6_chr_big", 6, "main", {NULL}, build_f6_chr_big,
     "chr(128): mimo 0 až 127 -> chyba interpretu 58", "a", {NULL}, NULL, 58},
    {"f6_chr_neg", 6, "main", {NULL}, build_f6_chr_neg,
     "chr(-1): mimo 0 až 127 -> chyba interpretu 58", "a", {NULL}, NULL, 58},
    {"f6_div_zero", 6, "main", {NULL}, build_f6_div_zero,
     "1 / 0 u Int: dělení nulou se neošetřuje, interpret skončí kódem 57", "a", {NULL}, NULL, 57},
    {"f6_readint", 6, "main", {NULL}, build_f6_readint,
     "readInt() přečte 42", "42", {NULL}, "42\n", 0},
    {"f6_readint_calc", 6, "main", {NULL}, build_f6_readint_calc,
     "dvě čísla na dvou řádcích: 20 + 22", "42", {NULL}, "20\n22\n", 0},
    {"f6_readint_neg", 6, "main", {NULL}, build_f6_readint,
     "readInt(): záporné číslo -12", "-12", {NULL}, "-12\n", 0},
    {"f6_readint_nonl", 6, "main", {NULL}, build_f6_readint,
     "readInt(): poslední řádek bez nového řádku", "7", {NULL}, "7", 0},
    {"f6_readint_bad", 6, "main", {NULL}, build_f6_readint,
     "readInt() z \"abc\": 0", "0", {NULL}, "abc\n", 0},
    {"f6_readint_eof", 6, "main", {NULL}, build_f6_readint,
     "readInt() na konci vstupu: 0 (READ vrací nil, to se musí změnit)", "0", {NULL}, "", 0},
    {"f6_readint_space", 6, "main", {NULL}, build_f6_readint,
     "readInt() z \" 5\": okolní bílé znaky se neignorují, 0", "0", {NULL}, " 5\n", 0},
    {"f6_readint_real", 6, "main", {NULL}, build_f6_readint,
     "readInt() z \"3.5\": není to Int, 0", "0", {NULL}, "3.5\n", 0},
    {"f6_readdouble", 6, "main", {NULL}, build_f6_readdouble,
     "readDouble() přečte 3.5", "0x1.cp+1", {NULL}, "3.5\n", 0},
    {"f6_readdouble_int", 6, "main", {NULL}, build_f6_readdouble,
     "readDouble() z \"2\": platné číslo, 2.0", "0x1p+1", {NULL}, "2\n", 0},
    {"f6_readdouble_bad", 6, "main", {NULL}, build_f6_readdouble,
     "readDouble() z \"x\": 0.0", "0x0p+0", {NULL}, "x\n", 0},
    {"f6_readdouble_eof", 6, "main", {NULL}, build_f6_readdouble,
     "readDouble() na konci vstupu: 0.0", "0x0p+0", {NULL}, "", 0},
    {"f6_readline", 6, "main", {NULL}, build_f6_readline,
     "dva readLine: řádky včetně nového řádku", "hello\nworld\n", {NULL}, "hello\nworld\n", 0},
    {"f6_readline_eof", 6, "main", {NULL}, build_f6_readline_brackets,
     "readLine() na konci vstupu: prázdný řetězec", "<>", {NULL}, "", 0},
    {"f6_readline_nonl", 6, "main", {NULL}, build_f6_readline_brackets,
     "readLine() posledního řádku bez nového řádku", "<last>", {NULL}, "last", 0},
    {"f6_readline_empty", 6, "main", {NULL}, build_f6_readline_brackets,
     "readLine() z prázdného řádku: jen \\n", "<\n>", {NULL}, "\n", 0},
    {"f6_read_mixed", 6, "main", {NULL}, build_f6_read_mixed,
     "readInt, readLine, readDouble za sebou", "5abc\n0x1.4p+1", {NULL}, "5\nabc\n2.5\n", 0},
    {"f6_read_loop", 6, "main", {NULL}, build_f6_read_loop,
     "readInt v cyklu: n a pak n čísel, součet 60", "60", {NULL}, "3\n10\n20\n30\n", 0},
    {"f6_read_discard", 6, "main", {NULL}, build_f6_read_discard,
     "readInt() jako příkaz zahodí první řádek, vypíše se druhý", "2", {NULL}, "1\n2\n", 0},
    {"f6_read_func", 6, "main", {"sum2", NULL}, build_f6_read_func,
     "čtení uvnitř uživatelské funkce: 3 + 4", "7", {NULL}, "3\n4\n", 0},
    {"f7_and_tt", 7, "main", {NULL}, build_f7_and_tt,
     "print(true && true)", "true", {NULL}, NULL, 0},
    {"f7_and_tf", 7, "main", {NULL}, build_f7_and_tf,
     "print(true && false)", "false", {NULL}, NULL, 0},
    {"f7_and_ft", 7, "main", {NULL}, build_f7_and_ft,
     "print(false && true)", "false", {NULL}, NULL, 0},
    {"f7_and_ff", 7, "main", {NULL}, build_f7_and_ff,
     "print(false && false)", "false", {NULL}, NULL, 0},
    {"f7_or_tt", 7, "main", {NULL}, build_f7_or_tt,
     "print(true || true)", "true", {NULL}, NULL, 0},
    {"f7_or_tf", 7, "main", {NULL}, build_f7_or_tf,
     "print(true || false)", "true", {NULL}, NULL, 0},
    {"f7_or_ft", 7, "main", {NULL}, build_f7_or_ft,
     "print(false || true)", "true", {NULL}, NULL, 0},
    {"f7_or_ff", 7, "main", {NULL}, build_f7_or_ff,
     "print(false || false)", "false", {NULL}, NULL, 0},
    {"f7_not", 7, "main", {NULL}, build_f7_not,
     "!true, !false, !!true", "falsetruetrue", {NULL}, NULL, 0},
    {"f7_not_var", 7, "main", {NULL}, build_f7_not_var,
     "val a = true; print(!a)", "false", {NULL}, NULL, 0},
    {"f7_vars", 7, "main", {NULL}, build_f7_vars,
     "val a = true; val b = false; a && b = false, a || b = true", "falsetrue", {NULL}, NULL, 0},
    {"f7_rel", 7, "main", {NULL}, build_f7_rel,
     "1 < 2 && 3 < 4 = true; 1 < 2 && 4 < 3 = false (&& pod relačními operátory)", "truefalse", {NULL}, NULL, 0},
    {"f7_prec_or_and", 7, "main", {NULL}, build_f7_prec_or_and,
     "true || false && false = true (|| má nižší prioritu než &&)", "true", {NULL}, NULL, 0},
    {"f7_prec_and_or", 7, "main", {NULL}, build_f7_prec_and_or,
     "(true || false) && false = false", "false", {NULL}, NULL, 0},
    {"f7_prec_not", 7, "main", {NULL}, build_f7_prec_not,
     "!false && true = true; !a && b s a = true = false", "truefalse", {NULL}, NULL, 0},
    {"f7_slide", 7, "main", {NULL}, build_f7_slide,
     "x < 0 && !u s x = -1, u = false = true (příklad ze slidů)", "true", {NULL}, NULL, 0},
    {"f7_eq", 7, "main", {NULL}, build_f7_eq,
     "true == true, true != false, false == true", "truetruefalse", {NULL}, NULL, 0},
    {"f7_eq_expr", 7, "main", {NULL}, build_f7_eq_expr,
     "(a && b) == b a (a || b) != b s a = true, b = false", "truetrue", {NULL}, NULL, 0},
    {"f7_val", 7, "main", {NULL}, build_f7_val,
     "val ok = a && b; print(ok)", "true", {NULL}, NULL, 0},
    {"f7_assign", 7, "main", {NULL}, build_f7_assign,
     "var f = true; f = f && false; f = !f", "falsetrue", {NULL}, NULL, 0},
    {"f7_short_and", 7, "main", {NULL}, build_f7_short_and,
     "ZKRAT: b != 0 && 10 / b > 1 s b = 0: bez zkratu chyba 57", "false!", {NULL}, NULL, 0},
    {"f7_short_and_eval", 7, "main", {NULL}, build_f7_short_and_eval,
     "b != 0 && 10 / b > 1 s b = 2: pravý operand se vyhodnotí = true", "true", {NULL}, NULL, 0},
    {"f7_short_or", 7, "main", {NULL}, build_f7_short_or,
     "ZKRAT: b == 0 || 10 / b > 1 s b = 0: bez zkratu chyba 57", "true!", {NULL}, NULL, 0},
    {"f7_short_or_eval", 7, "main", {NULL}, build_f7_short_or_eval,
     "b == 0 || 3 / b > 1 s b = 5: pravý operand se vyhodnotí = false", "false", {NULL}, NULL, 0},
    {"f7_short_chain", 7, "main", {NULL}, build_f7_short_chain,
     "ZKRAT: (x != 0 && 5/x > 1) && 5/x > 2 s x = 0 (vnořené zleva)", "false", {NULL}, NULL, 0},
    {"f7_short_chain_right", 7, "main", {NULL}, build_f7_short_chain_right,
     "ZKRAT: x != 0 && (5/x > 1 && 5/x > 2) s x = 0 (vnořené zprava)", "false", {NULL}, NULL, 0},
    {"f7_short_or_chain", 7, "main", {NULL}, build_f7_short_or_chain,
     "ZKRAT: (x == 0 || 5/x > 1) || 5/x > 2 s x = 0", "true", {NULL}, NULL, 0},
    {"f7_short_mixed", 7, "main", {NULL}, build_f7_short_mixed,
     "ZKRAT: (x == 0 || 5/x > 1) && !(x != 0 && 5/x > 1) s x = 0", "true", {NULL}, NULL, 0},
    {"f7_short_loop", 7, "main", {NULL}, build_f7_short_loop,
     "ZKRAT v while: i < 3 && 6 / (3 - i) > 0 (pro i = 3 by se dělilo nulou)", "012|", {NULL}, NULL, 0},
    {"f7_short_if", 7, "main", {NULL}, build_f7_short_if,
     "ZKRAT v if: x != 0 && 10 / x > 1 s x = 0 = else větev", "no", {NULL}, NULL, 0},
    {"f7_many", 7, "main", {NULL}, build_f7_many,
     "víc && a || v jedné funkci (každé návěští jednou): (a&&b) || (c&&d) || (a&&!d); a && b", "truefalse", {NULL}, NULL, 0},
    {"f7_nested_tmp", 7, "main", {NULL}, build_f7_nested_tmp,
     "((a && b) && (c && d)) a ((d || d) || (a && c)): vnořené výrazy", "falsetrue", {NULL}, NULL, 0},
    {"f7_if_andor", 7, "main", {NULL}, build_f7_if_andor,
     "if (a && b) || c then T else F", "T", {NULL}, NULL, 0},
    {"f7_while_and", 7, "main", {NULL}, build_f7_while_and,
     "while i < 5 && ok, if bez else nastaví ok = false při i == 3", "012", {NULL}, NULL, 0},
    {"f7_if_noelse", 7, "main", {NULL}, build_f7_if_noelse,
     "if bez else: if x < 0 then neg; if x > 5 then big", "negend", {NULL}, NULL, 0},
    {"f7_else_if", 7, "main", {NULL}, build_f7_else_if,
     "if x < 0 && !u then opt1 else if x == 0 then print(false), x = 0", "false", {NULL}, NULL, 0},
    {"f7_else_if_chain", 7, "main", {NULL}, build_f7_else_if_chain,
     "if/else if/else se třemi větvemi: x = 5 = pos", "pos", {NULL}, NULL, 0},
    {"f7_param", 7, "main", {"flip", NULL}, build_f7_param,
     "flip(x: Boolean): Boolean = return !x; flip(true)", "false", {NULL}, NULL, 0},
    {"f7_param_two", 7, "main", {"both", NULL}, build_f7_param_two,
     "both(a, b) = return a && b; both(true, false), both(true, true)", "falsetrue", {NULL}, NULL, 0},
    {"f7_ret_last", 7, "main", {"isPos", NULL}, build_f7_ret_last,
     "isPos(n): Boolean = n > 0 (poslední výraz těla); isPos(3), isPos(-3)", "truefalse", {NULL}, NULL, 0},
    {"f7_ret_if", 7, "main", {"check", NULL}, build_f7_ret_if,
     "check(n): if n < 0 then return false else return true; check(-1), check(1)", "falsetrue", {NULL}, NULL, 0},
    {"f7_short_funexp", 7, "main", {"side", NULL}, build_f7_short_funexp,
     "ZKRAT + FUNEXP: false && side() nevolá side, true && side() ho volá (vypíše X)", "falseXtrue", {NULL}, NULL, 0},
    {"f7_short_funexp_or", 7, "main", {"side", NULL}, build_f7_short_funexp_or,
     "ZKRAT + FUNEXP: true || side() nevolá side, false || side() ho volá", "trueXtrue", {NULL}, NULL, 0},
    {"f7_param_expr", 7, "main", {"flip", NULL}, build_f7_param_expr,
     "FUNEXP: flip(a && b) s a = true, b = false", "true", {NULL}, NULL, 0},
    {"f7_tostr", 7, "main", {NULL}, build_f7_tostr,
     "toStr(a) a toStr(false) = true, false", "truefalse", {NULL}, NULL, 0},
    {"f7_tostr_expr", 7, "main", {NULL}, build_f7_tostr_expr,
     "FUNEXP: print(toStr(true && false))", "false", {NULL}, NULL, 0},
    {"f7_readbool", 7, "main", {NULL}, build_f7_readbool,
     "readBoolean() přečte true", "true", {NULL}, "true\n", 0},
    {"f7_readbool_false", 7, "main", {NULL}, build_f7_readbool,
     "readBoolean() přečte false", "false", {NULL}, "false\n", 0},
    {"f7_readbool_two", 7, "main", {NULL}, build_f7_readbool_two,
     "dva řádky false a true: a && b a a || b", "falsetrue", {NULL}, "false\ntrue\n", 0},
    {"f7_readbool_if", 7, "main", {NULL}, build_f7_readbool_if,
     "readBoolean() v podmínce if (přes proměnnou)", "yes", {NULL}, "true\n", 0},
    {"f8_to", 8, "main", {NULL}, build_f8_to, "for i <- 1 to 3 do print(i)", "123", {"LF@i$1", NULL}, NULL, 0},
    {"f8_until", 8, "main", {NULL}, build_f8_until, "for i <- 1 until 4: konec se nebere", "123", {NULL}, NULL, 0},
    {"f8_to_once", 8, "main", {NULL}, build_f8_to_once, "5 to 5: jeden průchod", "5", {NULL}, NULL, 0},
    {"f8_until_empty", 8, "main", {NULL}, build_f8_until_empty, "3 until 3: nic, cyklus se přeskočí a pokračuje se za ním", ".", {NULL}, NULL, 0},
    {"f8_to_empty", 8, "main", {NULL}, build_f8_to_empty, "5 to 3: od je větší než do, tělo se nespustí", ".", {NULL}, NULL, 0},
    {"f8_around", 8, "main", {NULL}, build_f8_around, "příkaz před cyklem a za ním", "<12>", {NULL}, NULL, 0},
    {"f8_neg_lit", 8, "main", {NULL}, build_f8_neg_lit, "for i <- -2 to 2 (záporný začátek)", "-2-1012", {NULL}, NULL, 0},
    {"f8_expr_bounds", 8, "main", {NULL}, build_f8_expr_bounds, "meze jsou výrazy: 1 + 1 to 2 * 2", "234", {NULL}, NULL, 0},
    {"f8_var_bounds", 8, "main", {NULL}, build_f8_var_bounds, "meze jsou proměnné: val a = 3, val b = 5", "345", {NULL}, NULL, 0},
    {"f8_empty_body", 8, "main", {NULL}, build_f8_empty_body, "prázdné tělo (right == NULL), za cyklem se pokračuje", "x", {NULL}, NULL, 0},
    {"f8_big", 8, "main", {NULL}, build_f8_big, "součet 1 až 100 (sto průchodů)", "5050", {NULL}, NULL, 0},
    {"f8_use_i", 8, "main", {NULL}, build_f8_use_i, "řídicí proměnná ve výrazu: print(i * i)", "14916", {NULL}, NULL, 0},
    {"f8_sum", 8, "main", {NULL}, build_f8_sum, "var s = 0; for i <- 1 to 4 do s = s + i; print(s)", "10", {NULL}, NULL, 0},
    {"f8_body_var", 8, "main", {NULL}, build_f8_body_var, "val d = i * 2 uvnitř těla (DEFVAR musí být nahoře)", "246", {"LF@i$1", "LF@d$10", NULL}, NULL, 0},
    {"f8_if_inside", 8, "main", {NULL}, build_f8_if_inside, "if uvnitř těla cyklu: tiskne jen i > 2", "34", {NULL}, NULL, 0},
    {"f8_while_inside", 8, "main", {NULL}, build_f8_while_inside, "while uvnitř for, v každém kole nové k = 0", "001", {NULL}, NULL, 0},
    {"f8_in_while", 8, "main", {NULL}, build_f8_in_while, "for uvnitř while (vnější cyklus běží dvakrát)", "1212", {NULL}, NULL, 0},
    {"f8_nested", 8, "main", {NULL}, build_f8_nested, "dva vnořené cykly, print(i * 10 + j)", "11122122", {"LF@i$1", "LF@j$2", NULL}, NULL, 0},
    {"f8_nested3", 8, "main", {NULL}, build_f8_nested3, "tři vnořené cykly, 2 x 2 x 2 průchodů", "xxxxxxxx", {NULL}, NULL, 0},
    {"f8_nested_dep", 8, "main", {NULL}, build_f8_nested_dep, "vnitřní mez závisí na vnější proměnné: j <- 1 to i", "112123", {NULL}, NULL, 0},
    {"f8_two_seq", 8, "main", {NULL}, build_f8_two_seq, "dva cykly za sebou (návěští se nesmí opakovat)", "1212", {NULL}, NULL, 0},
    {"f8_same_name", 8, "main", {NULL}, build_f8_same_name, "dva cykly se stejným jménem i a čísly 1 a 2", "121020", {"LF@i$1", "LF@i$2", NULL}, NULL, 0},
    {"f8_step", 8, "main", {NULL}, build_f8_step, "1 to 10 by 3: konec se bere", "14710", {NULL}, NULL, 0},
    {"f8_step_skip", 8, "main", {NULL}, build_f8_step_skip, "1 to 10 by 4: konec se přeskočí", "159", {NULL}, NULL, 0},
    {"f8_step_over", 8, "main", {NULL}, build_f8_step_over, "krok větší než rozsah: jen první hodnota", "1", {NULL}, NULL, 0},
    {"f8_step_expr", 8, "main", {NULL}, build_f8_step_expr, "krok je výraz: 0 to 8 by 1 + 2", "036", {NULL}, NULL, 0},
    {"f8_step_var", 8, "main", {NULL}, build_f8_step_var, "krok je proměnná: val s = 2, 1 to 7 by s", "1357", {NULL}, NULL, 0},
    {"f8_step_once", 8, "main", {NULL}, build_f8_step_once, "krok se vyhodnotí jednou: s se v těle mění, krok zůstává 1", "123456789", {NULL}, NULL, 0},
    {"f8_bounds_once", 8, "main", {NULL}, build_f8_bounds_once, "mez se vyhodnotí jednou: n se v těle zvětšuje, cyklus běží do 3 (pak n je 6)", "1236", {NULL}, NULL, 0},
    {"f8_zero_var", 8, "main", {NULL}, build_f8_zero_var, "krok 0 z proměnné: chyba 40 za běhu", "a", {NULL}, NULL, 40},
    {"f8_zero_expr", 8, "main", {NULL}, build_f8_zero_expr, "krok s - 1 je 0: chyba 40", "", {NULL}, NULL, 40},
    {"f8_zero_empty", 8, "main", {NULL}, build_f8_zero_empty, "5 to 3 by 0: krok 0 je chyba i když by se tělo nespustilo", "", {NULL}, NULL, 40},
    {"f8_guard_one", 8, "main", {NULL}, build_f8_guard_one, "stráž: for i <- 1 to 6 if i > 2", "3456", {NULL}, NULL, 0},
    {"f8_guard_two", 8, "main", {NULL}, build_f8_guard_two, "dvě stráže (obě musí platit): if i > 2, if i < 5", "34", {NULL}, NULL, 0},
    {"f8_guard_none", 8, "main", {NULL}, build_f8_guard_none, "stráž false: tělo se nespustí, cyklus doběhne", ".", {NULL}, NULL, 0},
    {"f8_guard_last", 8, "main", {NULL}, build_f8_guard_last, "stráž neplatí až u poslední hodnoty", "1234", {NULL}, NULL, 0},
    {"f8_guard_and", 8, "main", {NULL}, build_f8_guard_and, "stráž s &&: if i > 1 && i < 4", "23", {NULL}, NULL, 0},
    {"f8_guard_or", 8, "main", {NULL}, build_f8_guard_or, "stráž s ||: if i == 2 || i == 4", "24", {NULL}, NULL, 0},
    {"f8_guard_var", 8, "main", {NULL}, build_f8_guard_var, "stráž s proměnnou: if i != skip", "1245", {NULL}, NULL, 0},
    {"f8_guard_step", 8, "main", {NULL}, build_f8_guard_step, "stráž a krok dohromady: 1 to 10 by 2 if i > 4", "579", {NULL}, NULL, 0},
    {"f8_guard_spec", 8, "main", {NULL}, build_f8_guard_spec, "příklad ze zadání: 0 until 10, if i != 3, if i < 8", "0124567", {NULL}, NULL, 0},
    {"f8_guard_call", 8, "main", {"ok", NULL}, build_f8_guard_call, "stráž je volání funkce (vypíše g při každém kole)", "g1g2", {NULL}, NULL, 0},
    {"f8_func_sum", 8, "main", {"sum", NULL}, build_f8_func_sum, "def sum(n): Int s cyklem a posledním výrazem s", "10,6", {NULL}, NULL, 0},
    {"f8_func_last_for", 8, "main", {"show", NULL}, build_f8_func_last_for, "Unit funkce končící cyklem for", "12|123", {NULL}, NULL, 0},
    {"f8_return_in_for", 8, "main", {"find", NULL}, build_f8_return_in_for, "return uprostřed cyklu, na zásobníku nic nezůstane", "300,0", {NULL}, NULL, 0},
    {"f8_ife_true", 8, "main", {NULL}, build_f8_ife_true, "val a = if true then 1 else 2", "1", {NULL}, NULL, 0},
    {"f8_ife_false", 8, "main", {NULL}, build_f8_ife_false, "val a = if false then 1 else 2", "2", {NULL}, NULL, 0},
    {"f8_ife_abs_neg", 8, "main", {NULL}, build_f8_ife_abs_neg, "příklad ze zadání: abs = if x < 0 then 0 - x else x, x = -5", "5", {NULL}, NULL, 0},
    {"f8_ife_abs_pos", 8, "main", {NULL}, build_f8_ife_abs_pos, "totéž s x = 7 (vybere se else)", "7", {NULL}, NULL, 0},
    {"f8_ife_arg", 8, "main", {NULL}, build_f8_ife_arg, "if jako argument: print(if x > 0 then 1 else 2)", "1", {NULL}, NULL, 0},
    {"f8_ife_str", 8, "main", {NULL}, build_f8_ife_str, "větve typu String", "yes", {NULL}, NULL, 0},
    {"f8_ife_bool", 8, "main", {NULL}, build_f8_ife_bool, "větve typu Boolean", "true", {NULL}, NULL, 0},
    {"f8_ife_double", 8, "main", {NULL}, build_f8_ife_double, "větve typu Double: 1.5", "0x1.8p+0", {NULL}, NULL, 0},
    {"f8_ife_mixed", 8, "main", {NULL}, build_f8_ife_mixed, "then je Int, else je Double: výsledek Double, 1 se převede na 1.0", "0x1p+0", {NULL}, NULL, 0},
    {"f8_ife_mixed_else", 8, "main", {NULL}, build_f8_ife_mixed_else, "totéž, vybere se else větev 2.5", "0x1.4p+1", {NULL}, NULL, 0},
    {"f8_ife_mixed_rev", 8, "main", {NULL}, build_f8_ife_mixed_rev, "then je Double, else je Int: else větev 1 se převede na 1.0", "0x1p+0", {NULL}, NULL, 0},
    {"f8_ife_multi", 8, "main", {NULL}, build_f8_ife_multi, "ve větvi víc příkazů: print(a) a pak hodnota 1", "a1", {NULL}, NULL, 0},
    {"f8_ife_multi_else", 8, "main", {NULL}, build_f8_ife_multi_else, "totéž, vybere se else: print(b) a hodnota 2", "b2", {NULL}, NULL, 0},
    {"f8_ife_elseif_first", 8, "main", {NULL}, build_f8_ife_elseif_first, "else if: x = -1, platí první podmínka", "0", {NULL}, NULL, 0},
    {"f8_ife_elseif_mid", 8, "main", {NULL}, build_f8_ife_elseif_mid, "else if: x = 0, platí druhá podmínka", "1", {NULL}, NULL, 0},
    {"f8_ife_elseif_last", 8, "main", {NULL}, build_f8_ife_elseif_last, "else if: x = 5, platí poslední else", "2", {NULL}, NULL, 0},
    {"f8_ife_operand", 8, "main", {NULL}, build_f8_ife_operand, "if jako levý operand: (if x > 0 then x else 0) + 10", "13", {NULL}, NULL, 0},
    {"f8_ife_operand_r", 8, "main", {NULL}, build_f8_ife_operand_r, "if jako pravý operand: 100 + if x > 0 then x else 0", "103", {NULL}, NULL, 0},
    {"f8_ife_nested", 8, "main", {NULL}, build_f8_ife_nested, "if true then (if false then 1 else 2) else 3", "2", {NULL}, NULL, 0},
    {"f8_ife_assign", 8, "main", {NULL}, build_f8_ife_assign, "y = if x > 0 then 1 else 2 jako přiřazení", "1", {NULL}, NULL, 0},
    {"f8_ife_two", 8, "main", {NULL}, build_f8_ife_two, "dva if výrazy v jedné funkci (návěští se nesmí opakovat)", "14", {NULL}, NULL, 0},
    {"f8_ife_branch_var", 8, "main", {NULL}, build_f8_ife_branch_var, "ve větvi val t = 5 (DEFVAR musí být nahoře)", "6", {"LF@t$11", NULL}, NULL, 0},
    {"f8_ife_in_for", 8, "main", {NULL}, build_f8_ife_in_for, "if výraz v těle cyklu: print(if i > 2 then 1 else 0)", "0011", {NULL}, NULL, 0},
    {"f8_ife_for_val", 8, "main", {NULL}, build_f8_ife_for_val, "val r = if i == 2 then 20 else i uvnitř těla for", "1203", {NULL}, NULL, 0},
    {"f8_ife_tail", 8, "main", {"sign", NULL}, build_f8_ife_tail, "def sign(x): Int = if ... (if je poslední příkaz funkce)", "-1,1", {NULL}, NULL, 0},
    {"f8_ife_return", 8, "main", {"pick", NULL}, build_f8_ife_return, "return if x > 0 then 1 else 2", "1,2", {NULL}, NULL, 0},
    {"f8_ife_lazy", 8, "main", {"side", NULL}, build_f8_ife_lazy, "vyhodnotí se jen vybraná větev: if false then side() else 2 nevypíše S", "2", {NULL}, NULL, 0},
    {"f8_ife_lazy_run", 8, "main", {"side", NULL}, build_f8_ife_lazy_run, "vybraná větev se vyhodnotí: if true then side() else 2 vypíše S", "S1", {NULL}, NULL, 0},
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
    {"CREATEFRAME", 0}, {"PUSHFRAME", 0}, {"POPFRAME", 0}, {"RETURN", 0}, {"CLEARS", 0}, {"BREAK", 0},
    {"CALL", 1},        {"LABEL", 1},     {"EXIT", 1},     {"PUSHS", 1},  {"POPS", 1},
    {"WRITE", 1},       {"DEFVAR", 1},    {"JUMP", 1},     {"DPRINT", 1},
    {"MOVE", 2},        {"INT2FLOAT", 2}, {"FLOAT2INT", 2}, {"INT2CHAR", 2}, {"FLOAT2STR", 2},
    {"INT2STR", 2},     {"READ", 2},      {"STRLEN", 2},   {"NOT", 2},    {"TYPE", 2},
    {"ISINT", 2},       {"NEWARRAY", 2},
    {"ADD", 3},         {"SUB", 3},       {"MUL", 3},      {"DIV", 3},    {"IDIV", 3},
    {"LT", 3},          {"GT", 3},        {"EQ", 3},       {"AND", 3},    {"OR", 3},
    {"STRI2INT", 3},    {"CONCAT", 3},    {"GETCHAR", 3},  {"SETCHAR", 3}, {"ARRAYGET", 3},
    {"ARRAYSET", 3},
    {"ADDS", 0},        {"SUBS", 0},      {"MULS", 0},
    {"IDIVS", 0},       {"DIVS", 0},      {"LTS", 0},      {"GTS", 0},    {"EQS", 0},
    {"NOTS", 0},        {"ANDS", 0},      {"ORS", 0},      {"INT2FLOATS", 0}, {"FLOAT2INTS", 0},
    {"INT2CHARS", 0},   {"STRI2INTS", 0}, {"FLOAT2STRS", 0}, {"INT2STRS", 0}, {"TYPES", 0},
    {"ISINTS", 0},
    {"JUMPIFEQS", 1},   {"JUMPIFNEQS", 1}, {"JUMPIFEQ", 3}, {"JUMPIFNEQ", 3},
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

/* Je první slovo řádku (instrukce) právě `word`? */
static bool first_word_is(const char *line, const char *word) {
  char buf[1024], *w[8];
  return split_words(line, buf, sizeof buf, w) >= 1 && strcmp(w[0], word) == 0;
}

/* ------------------------------------------------------------------------- */
/*  Simulátor IFJcode26 (instrukce pro fáze 0 až 4)                          */
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
  void *mem[150000]; /* 100000 instrukcí = nejvýš tolik alokací */
  int nmem;
  char err[300];
  int exit_code;
  const char *in; /* standardní vstup programu a pozice, odkud se čte dál */
  size_t inpos;
  char rt_msg[300]; /* běhová chyba interpretu (kód 57 nebo 58): program skončí s tímto kódem */
} Vm;

static void vm_fail(Vm *vm, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void *vm_alloc(Vm *vm, size_t n) {
  void *p = calloc(1, n ? n : 1);
  if (!p) exit(99);
  if (vm->nmem >= 150000) { /* program se zacyklil a jen žere paměť: ukončit ho jako chybu, ne celý test */
    vm_fail(vm, "program se zacyklil (spotřeboval příliš paměti)");
    return p; /* nepoznačeno k uvolnění, ale simulace hned skončí */
  }
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

/* Rovnost hodnot jako u JUMPIFEQ / JUMPIFEQS: stejné typy, nebo nil s čímkoli (pak jen nil == nil). */
static bool values_equal(Vm *vm, const Val *a, const Val *b, int ln, const char *op, bool *eq) {
  if (a->t == V_NIL || b->t == V_NIL) {
    *eq = (a->t == b->t);
    return true;
  }
  if (a->t != b->t) {
    vm_fail(vm, "řádek %d: %s porovnává hodnoty různého typu (chyba 53). Podmínka dává Boolean, takže se "
                "má porovnávat s bool@false (ne s číslem nebo řetězcem)", ln, op);
    return false;
  }
  switch (a->t) {
    case V_INT:
    case V_BOOL:
      *eq = a->i == b->i;
      break;
    case V_FLOAT:
      *eq = a->d == b->d;
      break;
    case V_STR:
      *eq = a->n == b->n && memcmp(a->s, b->s, a->n) == 0;
      break;
    default:
      *eq = false;
  }
  return true;
}


/* Běhová chyba interpretu, kterou smí případ očekávat (57, 58): program skončí s tímto kódem. */
static void vm_rt(Vm *vm, int code, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void vm_rt(Vm *vm, int code, const char *fmt, ...) {
  if (vm->rt_msg[0]) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(vm->rt_msg, sizeof vm->rt_msg, fmt, ap);
  va_end(ap);
  vm->exit_code = code;
}

static Val str_val(Vm *vm, const char *p, size_t n) {
  char *buf = vm_alloc(vm, n + 1);
  memcpy(buf, p, n);
  Val v;
  memset(&v, 0, sizeof v);
  v.t = V_STR;
  v.s = buf;
  v.n = n;
  return v;
}

/* Převod jednoho operandu: INT2FLOAT, FLOAT2INT, INT2CHAR, FLOAT2STR, INT2STR (název bez S na konci).
 * Vrací false, když chyba skončila program (vm->err nebo vm->rt_msg je nastavené). */
static bool conv_val(Vm *vm, const char *base, const Val *a, Val *r, int ln) {
  memset(r, 0, sizeof *r);
  char tmp[64];
  if (strcmp(base, "INT2FLOAT") == 0 || strcmp(base, "INT2CHAR") == 0 || strcmp(base, "INT2STR") == 0) {
    if (a->t != V_INT) {
      vm_fail(vm, "řádek %d: %s chce hodnotu typu int (chyba 53)", ln, base);
      return false;
    }
    if (strcmp(base, "INT2FLOAT") == 0) {
      r->t = V_FLOAT;
      r->d = (double)a->i;
    } else if (strcmp(base, "INT2STR") == 0) {
      *r = str_val(vm, tmp, (size_t)snprintf(tmp, sizeof tmp, "%lld", a->i));
      memcpy((char *)r->s, tmp, r->n);
    } else {
      if (a->i < 0 || a->i > 127) {
        vm_rt(vm, 58, "řádek %d: INT2CHAR s hodnotou %lld mimo 0 až 127 (chyba 58)", ln, a->i);
        return false;
      }
      *r = str_val(vm, tmp, 1);
      ((char *)r->s)[0] = (char)a->i;
    }
    return true;
  }
  if (a->t != V_FLOAT) {
    vm_fail(vm, "řádek %d: %s chce hodnotu typu float (chyba 53)", ln, base);
    return false;
  }
  if (strcmp(base, "FLOAT2INT") == 0) {
    r->t = V_INT;
    r->i = (long long)a->d;
  } else {
    *r = str_val(vm, tmp, (size_t)snprintf(tmp, sizeof tmp, "%.2f", a->d));
    memcpy((char *)r->s, tmp, r->n);
  }
  return true;
}

/* LT, GT, EQ: výsledek relace dvou hodnot stejného typu (nil jen u EQ). */
static bool rel_eval(Vm *vm, char kind, const char *opname, const Val *a, const Val *b, int ln, bool *res) {
  if (a->t == V_NIL || b->t == V_NIL) {
    if (kind != 'E') {
      vm_fail(vm, "řádek %d: %s s operandem nil (chyba 53)", ln, opname);
      return false;
    }
    *res = (a->t == b->t);
    return true;
  }
  if (a->t != b->t) {
    vm_fail(vm, "řádek %d: %s má operandy různého typu (chyba 53)", ln, opname);
    return false;
  }
  int cmp = 0;
  if (a->t == V_INT || a->t == V_BOOL)
    cmp = (a->i > b->i) - (a->i < b->i);
  else if (a->t == V_FLOAT)
    cmp = (a->d > b->d) - (a->d < b->d);
  else if (a->t == V_STR) {
    size_t m = a->n < b->n ? a->n : b->n;
    cmp = memcmp(a->s, b->s, m);
    if (cmp == 0) cmp = (a->n > b->n) - (a->n < b->n);
  }
  *res = kind == 'L' ? cmp < 0 : kind == 'G' ? cmp > 0 : cmp == 0;
  return true;
}

/* READ: jeden řádek vstupu podle typu. String vrací řádek včetně \n (na konci vstupu ""), int a float
 * vrací nil při chybném zápisu nebo na konci vstupu (celý řádek musí být platné číslo, bez mezer). */
static bool read_input(Vm *vm, const char *type, Val *r, int ln) {
  memset(r, 0, sizeof *r);
  const char *in = vm->in ? vm->in : "";
  size_t len = strlen(in), st = vm->inpos, e = st;
  while (e < len && in[e] != '\n') e++;
  size_t line_end = e;
  if (e < len) e++; /* včetně \n */
  if (strcmp(type, "string") == 0) {
    *r = str_val(vm, in + st, e - st);
    vm->inpos = e;
    return true;
  }
  if (strcmp(type, "bool") == 0) { /* jen přesně true nebo false (malými písmeny), jinak nil */
    r->t = V_NIL;
    if (st >= len) return true;
    vm->inpos = e;
    size_t bn = line_end - st;
    if (bn == 4 && memcmp(in + st, "true", 4) == 0) {
      r->t = V_BOOL;
      r->i = 1;
    } else if (bn == 5 && memcmp(in + st, "false", 5) == 0) {
      r->t = V_BOOL;
      r->i = 0;
    }
    return true;
  }
  bool is_int = strcmp(type, "int") == 0, is_float = strcmp(type, "float") == 0;
  if (!is_int && !is_float) {
    vm_fail(vm, "řádek %d: READ s typem \"%s\" simulátor v testu neumí", ln, type);
    return false;
  }
  if (st >= len) { /* konec vstupu */
    r->t = V_NIL;
    return true;
  }
  vm->inpos = e;
  r->t = V_NIL;
  size_t n = line_end - st;
  if (n == 0 || n > 60 || isspace((unsigned char)in[st])) return true;
  char tmp[64];
  memcpy(tmp, in + st, n);
  tmp[n] = '\0';
  char *end;
  errno = 0;
  if (is_int) {
    long long v = strtoll(tmp, &end, 10);
    if (*end == '\0' && errno == 0) {
      r->t = V_INT;
      r->i = v;
    }
  } else {
    double d = strtod(tmp, &end);
    if (*end == '\0' && errno == 0 && !isnan(d) && !isinf(d) && !strpbrk(tmp, "nNiI")) {
      r->t = V_FLOAT;
      r->d = d;
    }
  }
  return true;
}

/* Spustí program (řádky 0..n-1, řádek 0 je hlavička). Při chybě nastaví vm->err. */
static void vm_run(Vm *vm, char **lines, int n) {
  /* návěští */
  const char *lab[256];
  int labline[256], nlab = 0;
  for (int i = 1; i < n; i++) {
    char buf[1024], *w[8];
    if (split_words(lines[i], buf, sizeof buf, w) >= 2 && strcmp(w[0], "LABEL") == 0 && nlab < 256) {
      char *dup = vm_alloc(vm, strlen(w[1]) + 1);
      strcpy(dup, w[1]);
      lab[nlab] = dup;
      labline[nlab++] = i;
    }
  }

  int pc = 1;
  for (int steps = 0; pc < n; steps++) {
    if (vm->err[0]) return;
    if (steps > 100000) {
      vm_fail(vm, "program se zacyklil (víc než 100000 instrukcí)");
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
    } else if (((strcmp(op, "JUMPIFEQS") == 0 || strcmp(op, "JUMPIFNEQS") == 0) && nw == 2) ||
               ((strcmp(op, "JUMPIFEQ") == 0 || strcmp(op, "JUMPIFNEQ") == 0) && nw == 4)) {
      bool stack_form = op[strlen(op) - 1] == 'S';
      bool want_eq = op[6] == 'E'; /* JUMPIFEQ.. skočí při rovnosti, JUMPIFNEQ.. při nerovnosti */
      Val a, b;
      if (stack_form) {
        if (vm->nst < 2) {
          vm_fail(vm, "řádek %d: %s potřebuje dvě hodnoty na zásobníku (chyba 56). Před skokem má na "
                      "zásobníku být výsledek podmínky a k němu bool@false", ln, op);
          return;
        }
        b = vm->st[--vm->nst];
        a = vm->st[--vm->nst];
      } else if (!read_symb(vm, w[2], ln, &a) || !read_symb(vm, w[3], ln, &b)) {
        return;
      }
      bool eq = false;
      if (!values_equal(vm, &a, &b, ln, op, &eq)) return;
      if (eq == want_eq) {
        int target = -1;
        for (int k = 0; k < nlab; k++)
          if (strcmp(lab[k], w[1]) == 0) target = labline[k];
        if (target < 0) {
          vm_fail(vm, "řádek %d: skok na neexistující návěští \"%s\" (chyba 52)", ln, w[1]);
          return;
        }
        pc = target + 1;
      }
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
      if (x.t != V_INT) {
        vm_fail(vm, "řádek %d: EXIT chce celé číslo (chyba 53)", ln);
        return;
      }
      if (x.i < 0 || x.i > 49) {
        vm_rt(vm, 57, "řádek %d: EXIT s hodnotou %lld mimo 0 až 49 (chyba 57)", ln, x.i);
        return;
      }
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
          vm_rt(vm, 57, "řádek %d: dělení nulou (chyba 57)", ln);
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
    } else if ((strcmp(op, "ADD") == 0 || strcmp(op, "SUB") == 0 || strcmp(op, "MUL") == 0 ||
                strcmp(op, "DIV") == 0 || strcmp(op, "IDIV") == 0) && nw == 4) {
      Val a, b, r;
      if (!read_symb(vm, w[2], ln, &a) || !read_symb(vm, w[3], ln, &b)) return;
      memset(&r, 0, sizeof r);
      if (op[0] == 'I' || strcmp(op, "DIV") == 0) {
        bool is_idiv = op[0] == 'I';
        if (a.t != (is_idiv ? V_INT : V_FLOAT) || b.t != a.t) {
          vm_fail(vm, "řádek %d: %s chce dva operandy typu %s (chyba 53)", ln, op, is_idiv ? "int" : "float");
          return;
        }
        if ((is_idiv && b.i == 0) || (!is_idiv && b.d == 0.0)) {
          vm_rt(vm, 57, "řádek %d: dělení nulou (chyba 57)", ln);
          return;
        }
        if (is_idiv) {
          r.t = V_INT;
          r.i = (b.i == -1) ? (long long)(0ULL - (unsigned long long)a.i) : a.i / b.i;
        } else {
          r.t = V_FLOAT;
          r.d = a.d / b.d;
        }
      } else if (a.t == V_INT && b.t == V_INT) {
        unsigned long long ua = (unsigned long long)a.i, ub = (unsigned long long)b.i;
        r.t = V_INT;
        r.i = (long long)(op[0] == 'A' ? ua + ub : op[0] == 'S' ? ua - ub : ua * ub);
      } else if (a.t == V_FLOAT && b.t == V_FLOAT) {
        r.t = V_FLOAT;
        r.d = op[0] == 'A' ? a.d + b.d : op[0] == 'S' ? a.d - b.d : a.d * b.d;
      } else {
        vm_fail(vm, "řádek %d: %s má operandy různého nebo nevhodného typu (chyba 53)", ln, op);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      v->v = r;
      v->init = true;
    } else if ((strcmp(op, "LT") == 0 || strcmp(op, "GT") == 0 || strcmp(op, "EQ") == 0) && nw == 4) {
      Val a, b, r;
      bool res = false;
      if (!read_symb(vm, w[2], ln, &a) || !read_symb(vm, w[3], ln, &b)) return;
      if (!rel_eval(vm, op[0], op, &a, &b, ln, &res)) return;
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      memset(&r, 0, sizeof r);
      r.t = V_BOOL;
      r.i = res;
      v->v = r;
      v->init = true;
    } else if ((strcmp(op, "AND") == 0 || strcmp(op, "OR") == 0) && nw == 4) {
      Val a, b, r;
      if (!read_symb(vm, w[2], ln, &a) || !read_symb(vm, w[3], ln, &b)) return;
      if (a.t != V_BOOL || b.t != V_BOOL) {
        vm_fail(vm, "řádek %d: %s chce dva operandy typu bool (chyba 53)", ln, op);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      memset(&r, 0, sizeof r);
      r.t = V_BOOL;
      r.i = op[0] == 'A' ? (a.i && b.i) : (a.i || b.i);
      v->v = r;
      v->init = true;
    } else if (strcmp(op, "NOT") == 0 && nw == 3) {
      Val a, r;
      if (!read_symb(vm, w[2], ln, &a)) return;
      if (a.t != V_BOOL) {
        vm_fail(vm, "řádek %d: NOT chce operand typu bool (chyba 53)", ln);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      memset(&r, 0, sizeof r);
      r.t = V_BOOL;
      r.i = !a.i;
      v->v = r;
      v->init = true;
    } else if ((strcmp(op, "INT2FLOAT") == 0 || strcmp(op, "FLOAT2INT") == 0 || strcmp(op, "INT2CHAR") == 0 ||
                strcmp(op, "FLOAT2STR") == 0 || strcmp(op, "INT2STR") == 0) && nw == 3) {
      Val a, r;
      if (!read_symb(vm, w[2], ln, &a)) return;
      if (!conv_val(vm, op, &a, &r, ln)) return;
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      v->v = r;
      v->init = true;
    } else if ((strcmp(op, "FLOAT2INTS") == 0 || strcmp(op, "INT2CHARS") == 0 || strcmp(op, "FLOAT2STRS") == 0 ||
                strcmp(op, "INT2STRS") == 0) && nw == 1) {
      if (vm->nst < 1) {
        vm_fail(vm, "řádek %d: %s potřebuje hodnotu na zásobníku (chyba 56)", ln, op);
        return;
      }
      char base[16];
      snprintf(base, sizeof base, "%.*s", (int)strlen(op) - 1, op);
      Val r;
      if (!conv_val(vm, base, &vm->st[vm->nst - 1], &r, ln)) return;
      vm->st[vm->nst - 1] = r;
    } else if ((strcmp(op, "ANDS") == 0 || strcmp(op, "ORS") == 0) && nw == 1) {
      if (vm->nst < 2) {
        vm_fail(vm, "řádek %d: %s potřebuje dvě hodnoty na zásobníku (chyba 56)", ln, op);
        return;
      }
      Val b = vm->st[--vm->nst], a = vm->st[--vm->nst], r;
      if (a.t != V_BOOL || b.t != V_BOOL) {
        vm_fail(vm, "řádek %d: %s chce dva operandy typu bool (chyba 53)", ln, op);
        return;
      }
      memset(&r, 0, sizeof r);
      r.t = V_BOOL;
      r.i = op[0] == 'A' ? (a.i && b.i) : (a.i || b.i);
      vm->st[vm->nst++] = r;
    } else if (strcmp(op, "CLEARS") == 0 && nw == 1) {
      vm->nst = 0;
    } else if (strcmp(op, "STRLEN") == 0 && nw == 3) {
      Val a, r;
      if (!read_symb(vm, w[2], ln, &a)) return;
      if (a.t != V_STR) {
        vm_fail(vm, "řádek %d: STRLEN chce řetězec (chyba 53)", ln);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      memset(&r, 0, sizeof r);
      r.t = V_INT;
      r.i = (long long)a.n;
      v->v = r;
      v->init = true;
    } else if ((strcmp(op, "GETCHAR") == 0 || strcmp(op, "STRI2INT") == 0) && nw == 4) {
      Val a, b, r;
      if (!read_symb(vm, w[2], ln, &a) || !read_symb(vm, w[3], ln, &b)) return;
      if (a.t != V_STR || b.t != V_INT) {
        vm_fail(vm, "řádek %d: %s chce řetězec a celé číslo (chyba 53)", ln, op);
        return;
      }
      if (b.i < 0 || b.i >= (long long)a.n) {
        vm_rt(vm, 58, "řádek %d: %s s indexem %lld mimo řetězec délky %zu (chyba 58)", ln, op, b.i, a.n);
        return;
      }
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      if (op[0] == 'G') {
        r = str_val(vm, a.s + b.i, 1);
      } else {
        memset(&r, 0, sizeof r);
        r.t = V_INT;
        r.i = (unsigned char)a.s[b.i];
      }
      v->v = r;
      v->init = true;
    } else if (strcmp(op, "READ") == 0 && nw == 3) {
      Val r;
      if (!read_input(vm, w[2], &r, ln)) return;
      Var *v = need_var(vm, w[1], ln);
      if (!v) return;
      v->v = r;
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
  const char *label_name[128];
  bool label_fn[128]; /* LABEL začátku funkce: hned za ním je CREATEFRAME (ostatní jsou návěští if/while) */
  int label_line[128], n_labels = 0;
  const char *call_target[64];
  int call_line[64], n_calls = 0;
  const char *jump_target[128];
  int jump_line[128], n_jumps = 0;

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

    if (strcmp(w[0], "LABEL") == 0 && nw == 2 && n_labels < 128) {
      if (!valid_label(w[1]))
        problem("řádek %d: návěští \"%s\" má nepovolené znaky (smí _-$&%%*!? a alfanumerické, "
                "bez @)", ln, w[1]);
      label_name[n_labels] = strdup(w[1]);
      label_fn[n_labels] = (i + 1 < n) && first_word_is(lines[i + 1], "CREATEFRAME");
      label_line[n_labels++] = ln;
      if (first_label < 0) first_label = ln;
    }
    if (((strcmp(w[0], "JUMP") == 0 || strcmp(w[0], "JUMPIFEQS") == 0 ||
          strcmp(w[0], "JUMPIFNEQS") == 0) && nw == 2) ||
        ((strcmp(w[0], "JUMPIFEQ") == 0 || strcmp(w[0], "JUMPIFNEQ") == 0) && nw == 4)) {
      if (!valid_label(w[1]))
        problem("řádek %d: návěští \"%s\" má nepovolené znaky (smí _-$&%%*!? a alfanumerické, "
                "bez @)", ln, w[1]);
      if (n_jumps < 128) {
        jump_target[n_jumps] = strdup(w[1]);
        jump_line[n_jumps++] = ln;
      }
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
      if (label_fn[i] && has_name(label_name[i], funcs[f])) cnt++;
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
    for (int k = 0; c->phase < 5 && c->others[k]; k++)
      if (has_name(call_target[i], c->others[k]))
        problem("řádek %d: CALL na '%s', ale tu funkci nikdo nevolá (volá se jen @main)",
                call_line[i], c->others[k]);
  }
  if (entry_calls == 0) problem("chybí CALL na vstupní funkci '%s' (ta s @main)", c->entry);
  if (entry_calls > 1) problem("vstupní funkce '%s' se volá víckrát (%dx)", c->entry, entry_calls);

  /* skoky: každý míří na existující návěští (kontroluje se i skok, který se za běhu nevykoná) */
  for (int i = 0; i < n_jumps; i++) {
    bool defined = false;
    for (int j = 0; j < n_labels; j++)
      if (strcmp(jump_target[i], label_name[j]) == 0) defined = true;
    if (!defined) {
      problem("řádek %d: skok na neexistující návěští \"%s\" (interpret: chyba 52). Návěští u JUMP a "
              "JUMPIFEQS se musí napsat stejně jako u LABEL", jump_line[i], jump_target[i]);
      break;
    }
  }

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
    if (c->phase < 5 && strcmp(lines[i - 2], "PUSHS nil@nil") != 0) {
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
      /* začátek funkce = LABEL a hned za ním CREATEFRAME; návěští if a while uvnitř funkce se
       * nepočítají, jinak by se za nimi braly DEFVAR za cizí a hlásilo by se "chybí DEFVAR" */
      bool is_label = nw >= 1 && strcmp(w[0], "LABEL") == 0 && i + 1 < n &&
                      first_word_is(lines[i + 1], "CREATEFRAME");
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
      /* POPS parametrů na začátku funkce (před tělem) DEFVAR za sebou nevylučuje */
      if (strcmp(w[0], "CREATEFRAME") != 0 && strcmp(w[0], "PUSHFRAME") != 0 &&
          !(strcmp(w[0], "POPS") == 0 && !past_defs))
        past_defs = true;
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
    vm->in = c->input ? c->input : "";
    vm_run(vm, lines, (int)n);
    if (vm->err[0]) {
      problem("simulace: %s", vm->err);
    } else {
      if (vm->exit_code != c->exit_code)
        problem("simulace: program skončil s kódem %d (čekalo se %d)%s%s", vm->exit_code, c->exit_code,
                vm->rt_msg[0] ? ". Chyba interpretu: " : "", vm->rt_msg);
      compare_output("simulace", c, vm->out ? vm->out : "", vm->outn);
      if (c->exit_code == 0 && vm->nst > 1)
        problem("simulace: na konci programu zůstalo na zásobníku %d hodnot (smí jedna, návrat z "
                "main). Některý příkaz po sobě něco zanechal, třeba print nezahodil hodnotu", vm->nst);
    }
    vm_free(vm);
    free(vm);
  }

  for (int i = 0; i < n_labels; i++) free((void *)label_name[i]);
  for (int i = 0; i < n_calls; i++) free((void *)call_target[i]);
  for (int i = 0; i < n_jumps; i++) free((void *)jump_target[i]);
  free(lines);
  free(copy);
}

/* ------------------------------------------------------------------------- */
/*  Interpret                                                                */
/* ------------------------------------------------------------------------- */

/* Když interpret bere argumenty jinak, uprav jen příkaz tady. */
static int interp_exec(const char *interp, const char *codefile, const char *inf, const char *outf,
                       const char *errf) {
  char cmd[1024];
  snprintf(cmd, sizeof cmd, "%s \"%s\" < \"%s\" > \"%s\" 2> \"%s\"", interp, codefile,
           inf ? inf : "/dev/null", outf, errf);
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
  bool ok = interp_exec(interp, codef, NULL, outf, errf) == 0;
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

  char inf[] = "/tmp/ifj_gen_in_XXXXXX";
  bool has_in = false;
  if (c->input && c->input[0]) {
    int fd3 = mkstemp(inf);
    if (fd3 >= 0) {
      if (write(fd3, c->input, strlen(c->input)) < 0) { /* ignoruj */ }
      close(fd3);
      has_in = true;
    }
  }
  int rc = interp_exec(interp, codefile, has_in ? inf : NULL, outf, errf);
  if (has_in) remove(inf);
  size_t olen = 0, elen = 0;
  char *so = read_file(outf, &olen);
  char *se = read_file(errf, &elen);
  if (rc != c->exit_code) {
    problem("ic26int skončil s kódem %d (čekalo se %d). stderr: %s", rc, c->exit_code,
            se ? esc(se, elen) : "");
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
    if (verbose) print_code(text, 400);
  } else {
    printf("  CHYBA %-16s %s\n", c->name, c->desc);
    for (int i = 0; i < g_nmsg; i++) printf("        - %s\n", g_msg[i]);
    printf("        vygenerovaný kód:\n");
    if (len == 0)
      printf("        | (nic)\n");
    else
      print_code(text, 400);
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
  int phase_ok[9] = {0}, phase_bad[9] = {0};
  static const char *const PHASE_NAME[9] = {"kostra programu a funkcí", "konstanty a print",
                                           "výrazy", "proměnné", "if a while", "funkce",
                                           "vestavěné funkce", "rozšíření BOOLTHEN", "rozšíření CONTROL (cyklus for)"};
  for (int phase = 0; phase <= 8; phase++) {
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
    printf("neznámý případ \"%s\". Dostupné: f0, f1, f2, f3, f4, f5, f6, f7, f8 a", only);
    for (size_t i = 0; i < N_CASES; i++) printf(" %s", CASES[i].name);
    printf("\n");
    return 2;
  }

  printf("== Výsledek: OK %d, chyby %d", total_ok, total_bad);
  for (int phase = 0; phase <= 8; phase++)
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
