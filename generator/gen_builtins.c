/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_builtins.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 *
 * ---------------------------------------------------------------------------
 * Vestavěné funkce (kapitola 4 specifikace) napsal AI: Claude Sonnet 5.5 (Anthropic,
 * claude-sonnet-5-5), 2026-11-18, na zadání Pavla Nováka (xnovakp01).
 * Prompt: "mohl by se je (builtins) vypsat do gen_builtins, podepis se pod to a dej mi postup,
 *          jak to implementovat v těch ostatních souborech"
 * Zrevidoval a odevzdává: Pavel Novák (xnovakp01)
 * Ověřeno testem test_generator (fáze 6) v simulátoru i v ic26int.
 * ---------------------------------------------------------------------------
 *
 * Jak to funguje
 *   Každá vestavěná funkce je v IFJcode26 napsaná jako obyčejná funkce s návěštím $b_<jméno>
 *   a stejnou konvencí jako uživatelské funkce ($f_<jméno>):
 *     - volající vyhodnotí argumenty zleva doprava, takže poslední argument je na zásobníku nahoře
 *     - CALL $b_<jméno>
 *     - funkce si sama udělá CREATEFRAME + PUSHFRAME, vyzvedne argumenty (POPS, od posledního
 *       k prvnímu), a na konci nechá na zásobníku jednu hodnotu (výsledek), POPFRAME a RETURN
 *   Definice vestavěných funkcí se vypisují až ZA EXIT int@0 (jinak by se do nich program propadl).
 *   Vypíše se jen to, co program opravdu volá (zjistí se průchodem AST).
 *
 *   toStr má tři varianty podle typu argumentu ($b_toStr_int, $b_toStr_float, $b_toStr_bool);
 *   pro String se nevolá nic, hodnota na zásobníku už je řetězec.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"

/* ------------------------------------------------------------------------- */
/*  Které vestavěné funkce existují a které program používá                  */
/* ------------------------------------------------------------------------- */

enum {
  B_READLINE, B_READINT, B_READDOUBLE, B_TOSTR, B_TOINT, B_STRLENGTH, B_SUBSTR, B_ORD, B_CHR,
  B_COUNT
};

static const char *const BUILTIN_NAME[B_COUNT] = {"readLine", "readInt", "readDouble", "toStr", "toInt",
                                                  "strLength", "subStr",  "ord",        "chr"};

/* kolikrát se v programu volá každá vestavěná funkce (jen zda vůbec: 0 nebo víc) */
static bool used[B_COUNT];
static bool used_tostr_int, used_tostr_float, used_tostr_bool;

static int builtin_index(const char *name) {
  for (int i = 0; i < B_COUNT; i++)
    if (strcmp(BUILTIN_NAME[i], name) == 0) return i;
  return -1;
}

/* Je to jméno vestavěné funkce? (print se tady nepočítá, ten generuje gen_expr přímo) */
bool is_builtin(const char *name) { return builtin_index(name) >= 0; }

/* Projde celý strom a zapíše si, které vestavěné funkce se volají. */
static void scan(AST_NODE *node) {
  if (!node) return;
  if (node->type == NODE_CALL && node->name) {
    int idx = builtin_index(node->name);
    if (idx >= 0) {
      used[idx] = true;
      if (idx == B_TOSTR && node->third) {
        switch (node->third->data_type) {
          case DATA_INT:
            used_tostr_int = true;
            break;
          case DATA_DOUBLE:
            used_tostr_float = true;
            break;
          case DATA_BOOL:
            used_tostr_bool = true;
            break;
          default:
            break; /* String: žádná funkce není potřeba */
        }
      }
    }
  }
  scan(node->left);
  scan(node->right);
  scan(node->third);
  scan(node->next);
}

/* ------------------------------------------------------------------------- */
/*  Volání                                                                   */
/* ------------------------------------------------------------------------- */

/* Vypíše samotné CALL vestavěné funkce. Argumenty už musí být na zásobníku (gen_expr je vyhodnotil). */
void emit_builtin_call(Gen *gen, AST_NODE *call) {
  (void)gen;
  if (strcmp(call->name, "toStr") == 0) {
    switch (call->third->data_type) {
      case DATA_INT:
        puts("CALL $b_toStr_int");
        break;
      case DATA_DOUBLE:
        puts("CALL $b_toStr_float");
        break;
      case DATA_BOOL:
        puts("CALL $b_toStr_bool");
        break;
      default:
        break; /* String zůstává na zásobníku, jak je */
    }
    return;
  }
  printf("CALL $b_%s\n", call->name);
}

/* ------------------------------------------------------------------------- */
/*  Definice                                                                 */
/* ------------------------------------------------------------------------- */

/* readLine / readInt / readDouble: READ vrátí nil, když je řádek špatný nebo skončil vstup.
 * Podle zadání se pak vrací výchozí hodnota (0, 0.0, ""). */
static void def_read(const char *name, const char *type, const char *default_value) {
  printf("LABEL $b_%s\n", name);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  printf("READ LF@val %s\n", type);
  printf("JUMPIFNEQ $b_%s$ok LF@val nil@nil\n", name);
  printf("MOVE LF@val %s\n", default_value);
  printf("LABEL $b_%s$ok\n", name);
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

/* funkce s jedním argumentem a jednou instrukcí: výsledek = INSTRUKCE(argument) */
static void def_unary(const char *label, const char *instruction) {
  printf("LABEL $b_%s\n", label);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@arg");
  puts("DEFVAR LF@res");
  puts("POPS LF@arg");
  printf("%s LF@res LF@arg\n", instruction);
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

/* toStr(Boolean): "true" nebo "false" */
static void def_tostr_bool(void) {
  puts("LABEL $b_toStr_bool");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@arg");
  puts("DEFVAR LF@res");
  puts("POPS LF@arg");
  puts("MOVE LF@res string@false");
  puts("JUMPIFEQ $b_toStr_bool$end LF@arg bool@false");
  puts("MOVE LF@res string@true");
  puts("LABEL $b_toStr_bool$end");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

/* ord(value, index): kód znaku na pozici index, nebo -1 (prázdný řetězec, index mimo řetězec) */
static void def_ord(void) {
  puts("LABEL $b_ord");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@res");
  puts("POPS LF@idx"); /* poslední argument je nahoře */
  puts("POPS LF@str");
  puts("STRLEN LF@len LF@str");
  puts("LT LF@cond LF@idx int@0");
  puts("JUMPIFEQ $b_ord$bad LF@cond bool@true");
  puts("LT LF@cond LF@idx LF@len");
  puts("JUMPIFEQ $b_ord$ok LF@cond bool@true");
  puts("LABEL $b_ord$bad");
  puts("PUSHS int@-1");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_ord$ok");
  puts("STRI2INT LF@res LF@str LF@idx");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
}

/* subStr(value, start, end): podřetězec od start (včetně) do end (bez); špatné meze = chyba 40 */
static void def_substr(void) {
  puts("LABEL $b_subStr");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@start");
  puts("DEFVAR LF@end");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@res");
  puts("DEFVAR LF@ch");
  puts("DEFVAR LF@i");
  puts("POPS LF@end"); /* poslední argument je nahoře */
  puts("POPS LF@start");
  puts("POPS LF@str");
  puts("STRLEN LF@len LF@str");
  /* start < 0 */
  puts("LT LF@cond LF@start int@0");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  /* end < 0 */
  puts("LT LF@cond LF@end int@0");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  /* start > end */
  puts("GT LF@cond LF@start LF@end");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  /* start >= délka (tedy NOT start < délka) */
  puts("LT LF@cond LF@start LF@len");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@false");
  /* end > délka */
  puts("GT LF@cond LF@end LF@len");
  puts("JUMPIFEQ $b_subStr$err LF@cond bool@true");
  /* cyklus: res = res + str[i] pro i od start do end - 1 */
  puts("MOVE LF@res string@");
  puts("MOVE LF@i LF@start");
  puts("LABEL $b_subStr$loop");
  puts("LT LF@cond LF@i LF@end");
  puts("JUMPIFEQ $b_subStr$done LF@cond bool@false");
  puts("GETCHAR LF@ch LF@str LF@i");
  puts("CONCAT LF@res LF@res LF@ch");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_subStr$loop");
  puts("LABEL $b_subStr$done");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_subStr$err");
  puts("EXIT int@40");
}

/* Vypíše definice všech vestavěných funkcí, které program volá. Volá se z generator_init
 * hned za EXIT int@0 (a před nebo za definicemi uživatelských funkcí, na pořadí nezáleží). */
void emit_builtins(Gen *gen, AST_NODE *root) {
  (void)gen;
  memset(used, 0, sizeof used);
  used_tostr_int = used_tostr_float = used_tostr_bool = false;
  scan(root);

  if (used[B_READLINE]) def_read("readLine", "string", "string@");
  if (used[B_READINT]) def_read("readInt", "int", "int@0");
  if (used[B_READDOUBLE]) def_read("readDouble", "float", "float@0x0p+0");
  if (used_tostr_int) def_unary("toStr_int", "INT2STR");
  if (used_tostr_float) def_unary("toStr_float", "FLOAT2STR"); /* vždy %.2f, např. 2.50 */
  if (used_tostr_bool) def_tostr_bool();
  if (used[B_TOINT]) def_unary("toInt", "FLOAT2INT"); /* osekne k nule */
  if (used[B_STRLENGTH]) def_unary("strLength", "STRLEN");
  if (used[B_CHR]) def_unary("chr", "INT2CHAR"); /* mimo 0 až 127 skončí interpret chybou 58 */
  if (used[B_ORD]) def_ord();
  if (used[B_SUBSTR]) def_substr();
}
