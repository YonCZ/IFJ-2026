/*
 * IFJ26 Compiler Implementation
 *
 * File: gen_builtins.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 *
 * ---------------------------------------------------------------------------
 * Vestavěné funkce (kapitola 4 specifikace) napsal AI: Claude Sonnet 5.5
 * (Anthropic, claude-sonnet-5-5), 2026-11-18, na zadání Pavla Nováka
 * (xnovakp01). Prompt: "mohl by se je (builtins) vypsat do gen_builtins,
 * podepis se pod to a dej mi postup, jak to implementovat v těch ostatních
 * souborech" Zrevidoval a odevzdává: Pavel Novák (xnovakp01) Doplnění
 * 2026-11-19 (rozšíření VALMEM, funkce floor a volání lower_members): Claude
 * Sonnet 5.5, prompt "co mam delat pro rozsireni valmem". Ověřeno testem
 * test_generator (fáze 6) v simulátoru i v ic26int.
 * ---------------------------------------------------------------------------
 *
 * Jak to funguje
 *   Každá vestavěná funkce je v IFJcode26 napsaná jako obyčejná funkce s
 * návěštím $b_<jméno> a stejnou konvencí jako uživatelské funkce ($f_<jméno>):
 *     - volající vyhodnotí argumenty zleva doprava, takže poslední argument je
 * na zásobníku nahoře
 *     - CALL $b_<jméno>
 *     - funkce si sama udělá CREATEFRAME + PUSHFRAME, vyzvedne argumenty (POPS,
 * od posledního k prvnímu), a na konci nechá na zásobníku jednu hodnotu
 * (výsledek), POPFRAME a RETURN Definice vestavěných funkcí se vypisují až ZA
 * EXIT int@0 (jinak by se do nich program propadl). Vypíše se jen to, co
 * program opravdu volá (zjistí se průchodem AST).
 *
 *   toStr má tři varianty podle typu argumentu ($b_toStr_int, $b_toStr_float,
 * $b_toStr_bool); pro String se nevolá nic, hodnota na zásobníku už je řetězec.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"

void lower_members(AST_NODE *fn); /*gen_valmem.c */

/* ------------------------------------------------------------------------- */
/*  Které vestavěné funkce existují a které program používá                  */
/* ------------------------------------------------------------------------- */

enum {
  B_READLINE,
  B_READINT,
  B_READDOUBLE,
  B_TOSTR,
  B_TOINT,
  B_STRLENGTH,
  B_SUBSTR,
  B_ORD,
  B_CHR,
  B_READBOOLEAN, /* rozšíření BOOLTHEN */
  B_FLOOR, /* rozšíření VALMEM: člen .floor (interní jméno, aby se nepletlo s
              uživatelskou funkcí) */
  B_READINTMAYBE,
  B_READDOUBLEMAYBE,
  B_COUNT
};

static const char *const BUILTIN_NAME[B_COUNT] = {
    "readLine",       "readInt", "readDouble",
    "toStr",          "toInt",   "strLength",
    "subStr",         "ord",     "chr",
    "readBoolean",    "$floor",  "readIntMaybe",
    "readDoubleMaybe"};

/* kolikrát se v programu volá každá vestavěná funkce (jen zda vůbec: 0 nebo
 * víc) */
static bool used[B_COUNT];
static bool used_tostr_int, used_tostr_float, used_tostr_bool;

static int builtin_index(const char *name) {
  for (int i = 0; i < B_COUNT; i++)
    if (strcmp(BUILTIN_NAME[i], name) == 0)
      return i;
  return -1;
}

/* Je to jméno vestavěné funkce? (print se tady nepočítá, ten generuje gen_expr
 * přímo) */
bool is_builtin(const char *name) { return builtin_index(name) >= 0; }

/* Projde celý strom a zapíše si, které vestavěné funkce se volají. */
static void scan(AST_NODE *node) {
  if (!node)
    return;
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

  if (node->type == NODE_INTERP) {
    for (AST_NODE *part = node->left; part; part = part->next) {
      switch (part->data_type) {
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
        break;
      }
    }
  }
  scan(node->left);
  scan(node->right);
  scan(node->third);
  scan(node->arg_groups);
  scan(node->next);
}

/* ------------------------------------------------------------------------- */
/*  Volání                                                                   */
/* ------------------------------------------------------------------------- */

/* Vypíše samotné CALL vestavěné funkce. Argumenty už musí být na zásobníku
 * (gen_expr je vyhodnotil). */
void emit_builtin_call(Gen *gen, AST_NODE *call) {
  (void)gen;
  if (strcmp(call->name, "toStr") == 0) {
    if (call->third != NULL && call->third->data_type == DATA_LIST) {
      puts("CALL $b_list_toStr");
      return;
    }
    if (call->third != NULL && call->third->data_type == DATA_UNION) {
      puts("CALL $b_toStr_any");
      return;
    }
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
    case DATA_UNIT:
      puts("POPS LF@tmp1");
      puts("PUSHS string@()");
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

/* readLine / readInt / readDouble: READ vrátí nil, když je řádek špatný nebo
 * skončil vstup. Podle zadání se pak vrací výchozí hodnota (0, 0.0, ""). */
static void def_read(const char *name, const char *type,
                     const char *default_value) {
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

static void def_readmaybe(const char *name, const char *type) {
  printf("LABEL $b_%s\n", name);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  printf("READ LF@val %s\n", type);
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

/* readBoolean (rozšíření BOOLTHEN): podle zadání odpovídá chování instrukce
 * READ, takže se výsledek READ nijak neupravuje (na rozdíl od readInt a
 * readDouble se nil nepřevádí na výchozí hodnotu). READ bool vrací true jen pro
 * řádek "true" a false jen pro "false"; cokoli jiného a konec vstupu je nil. */
static void def_readboolean(void) {
  puts("LABEL $b_readBoolean");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("READ LF@val bool");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
}

/* funkce s jedním argumentem a jednou instrukcí: výsledek = INSTRUKCE(argument)
 */
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

/* floor (rozšíření VALMEM, člen Double.floor): největší celé číslo, které není
 * větší než argument, ale jako Double. FLOAT2INT osekává k nule, takže u
 * záporných neceločíselných čísel je výsledek o 1 větší, než má být (-2.5 ->
 * -2); tehdy se 1.0 odečte. */
static void def_floor(void) {
  puts("LABEL $b_$floor");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@x");
  puts("DEFVAR LF@t");
  puts("DEFVAR LF@f");
  puts("DEFVAR LF@c");
  puts("POPS LF@x");
  puts("FLOAT2INT LF@t LF@x");
  puts("INT2FLOAT LF@f LF@t");
  puts("GT LF@c LF@f LF@x");
  puts("JUMPIFEQ $b_$floor$ok LF@c bool@false");
  puts("SUB LF@f LF@f float@0x1p+0");
  puts("LABEL $b_$floor$ok");
  puts("PUSHS LF@f");
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

/* ord(value, index): kód znaku na pozici index, nebo -1 (prázdný řetězec, index
 * mimo řetězec) */
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

/* subStr(value, start, end): podřetězec od start (včetně) do end (bez); špatné
 * meze = chyba 40 */
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

static void def_list_helpers(void) {
  puts("LABEL $b_list_get");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@idx");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@idx int@0");
  puts("JUMPIFEQ $b_list_get$err LF@cond bool@true");
  puts("LT LF@cond LF@idx LF@len");
  puts("JUMPIFEQ $b_list_get$err LF@cond bool@false");
  puts("ADD LF@idx LF@idx int@2");
  puts("ARRAYGET LF@val LF@list LF@idx");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");
  puts("LABEL $b_list_get$err");
  puts("EXIT int@40");

  puts("LABEL $b_list_head");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@val");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("JUMPIFNEQ $b_list_head$ok LF@len int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_head$ok");
  puts("ARRAYGET LF@val LF@list int@2");
  puts("PUSHS LF@val");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_length");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("PUSHS LF@len");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_toList");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("POPS LF@list");
  puts("PUSHS LF@list");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_tail");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("JUMPIFNEQ $b_list_tail$ok LF@len int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_tail$ok");
  puts("SUB LF@newlen LF@len int@1");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_tail$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_tail$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@3");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_tail$loop");
  puts("LABEL $b_list_tail$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_drop");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@n");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@n");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@n int@0");
  puts("JUMPIFEQ $b_list_drop$zero LF@cond bool@false");
  puts("MOVE LF@n int@0");
  puts("LABEL $b_list_drop$zero");
  puts("GT LF@cond LF@n LF@len");
  puts("JUMPIFEQ $b_list_drop$clamped LF@cond bool@false");
  puts("MOVE LF@n LF@len");
  puts("LABEL $b_list_drop$clamped");
  puts("SUB LF@newlen LF@len LF@n");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_drop$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_drop$done LF@cond bool@false");
  puts("ADD LF@j LF@i LF@n");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_drop$loop");
  puts("LABEL $b_list_drop$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_take");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@n");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@n");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@n int@0");
  puts("JUMPIFEQ $b_list_take$zero LF@cond bool@false");
  puts("MOVE LF@n int@0");
  puts("LABEL $b_list_take$zero");
  puts("GT LF@cond LF@n LF@len");
  puts("JUMPIFEQ $b_list_take$clamped LF@cond bool@false");
  puts("MOVE LF@n LF@len");
  puts("LABEL $b_list_take$clamped");
  puts("ADD LF@size LF@n int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@n");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_take$loop");
  puts("LT LF@cond LF@i LF@n");
  puts("JUMPIFEQ $b_list_take$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_take$loop");
  puts("LABEL $b_list_take$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_slice");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@from");
  puts("DEFVAR LF@until");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@newlen");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@until");
  puts("POPS LF@from");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("LT LF@cond LF@from int@0");
  puts("JUMPIFEQ $b_list_slice$from_zero LF@cond bool@false");
  puts("MOVE LF@from int@0");
  puts("LABEL $b_list_slice$from_zero");
  puts("GT LF@cond LF@from LF@len");
  puts("JUMPIFEQ $b_list_slice$from_max LF@cond bool@false");
  puts("MOVE LF@from LF@len");
  puts("LABEL $b_list_slice$from_max");
  puts("LT LF@cond LF@until int@0");
  puts("JUMPIFEQ $b_list_slice$until_zero LF@cond bool@false");
  puts("MOVE LF@until int@0");
  puts("LABEL $b_list_slice$until_zero");
  puts("GT LF@cond LF@until LF@len");
  puts("JUMPIFEQ $b_list_slice$until_max LF@cond bool@false");
  puts("MOVE LF@until LF@len");
  puts("LABEL $b_list_slice$until_max");
  puts("LT LF@cond LF@until LF@from");
  puts("JUMPIFEQ $b_list_slice$ordered LF@cond bool@false");
  puts("MOVE LF@until LF@from");
  puts("LABEL $b_list_slice$ordered");
  puts("SUB LF@newlen LF@until LF@from");
  puts("ADD LF@size LF@newlen int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@list int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ARRAYSET LF@arr int@1 LF@newlen");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_slice$loop");
  puts("LT LF@cond LF@i LF@newlen");
  puts("JUMPIFEQ $b_list_slice$done LF@cond bool@false");
  puts("ADD LF@j LF@i LF@from");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYGET LF@val LF@list LF@j");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_slice$loop");
  puts("LABEL $b_list_slice$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_concat");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@a");
  puts("DEFVAR LF@b");
  puts("DEFVAR LF@la");
  puts("DEFVAR LF@lb");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@tag");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@j");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@cond");
  puts("POPS LF@b");
  puts("POPS LF@a");
  puts("ARRAYGET LF@la LF@a int@1");
  puts("ARRAYGET LF@lb LF@b int@1");
  puts("ADD LF@size LF@la LF@lb");
  puts("ADD LF@size LF@size int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYGET LF@tag LF@a int@0");
  puts("ARRAYSET LF@arr int@0 LF@tag");
  puts("ADD LF@size LF@la LF@lb");
  puts("ARRAYSET LF@arr int@1 LF@size");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_concat$la");
  puts("LT LF@cond LF@i LF@la");
  puts("JUMPIFEQ $b_list_concat$lb_start LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@a LF@j");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_concat$la");
  puts("LABEL $b_list_concat$lb_start");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_concat$lb");
  puts("LT LF@cond LF@i LF@lb");
  puts("JUMPIFEQ $b_list_concat$done LF@cond bool@false");
  puts("ADD LF@j LF@i int@2");
  puts("ARRAYGET LF@val LF@b LF@j");
  puts("ADD LF@j LF@i LF@la");
  puts("ADD LF@j LF@j int@2");
  puts("ARRAYSET LF@arr LF@j LF@val");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_concat$lb");
  puts("LABEL $b_list_concat$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_range");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@start");
  puts("DEFVAR LF@end");
  puts("DEFVAR LF@step");
  puts("DEFVAR LF@inclusive");
  puts("DEFVAR LF@end_eff");
  puts("DEFVAR LF@count");
  puts("DEFVAR LF@num");
  puts("DEFVAR LF@diff");
  puts("DEFVAR LF@absstep");
  puts("DEFVAR LF@size");
  puts("DEFVAR LF@arr");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@idx");
  puts("DEFVAR LF@cur");
  puts("DEFVAR LF@cond");
  puts("POPS LF@inclusive");
  puts("POPS LF@step");
  puts("POPS LF@end");
  puts("POPS LF@start");
  puts("JUMPIFNEQ $b_list_range$step_ok LF@step int@0");
  puts("EXIT int@40");
  puts("LABEL $b_list_range$step_ok");
  puts("MOVE LF@end_eff LF@end");
  puts("GT LF@cond LF@step int@0");
  puts("JUMPIFEQ $b_list_range$positive LF@cond bool@true");
  puts("SUB LF@absstep int@0 LF@step");
  puts("JUMPIFEQ $b_list_range$neg_inc LF@inclusive int@0");
  puts("SUB LF@end_eff LF@end int@1");
  puts("LABEL $b_list_range$neg_inc");
  puts("GT LF@cond LF@start LF@end_eff");
  puts("JUMPIFEQ $b_list_range$empty LF@cond bool@false");
  puts("SUB LF@diff LF@start LF@end_eff");
  puts("ADD LF@num LF@diff LF@absstep");
  puts("SUB LF@num LF@num int@1");
  puts("IDIV LF@count LF@num LF@absstep");
  puts("JUMP $b_list_range$fill");
  puts("LABEL $b_list_range$positive");
  puts("JUMPIFEQ $b_list_range$pos_inc LF@inclusive int@0");
  puts("ADD LF@end_eff LF@end int@1");
  puts("LABEL $b_list_range$pos_inc");
  puts("LT LF@cond LF@start LF@end_eff");
  puts("JUMPIFEQ $b_list_range$empty LF@cond bool@false");
  puts("SUB LF@diff LF@end_eff LF@start");
  puts("ADD LF@num LF@diff LF@step");
  puts("SUB LF@num LF@num int@1");
  puts("IDIV LF@count LF@num LF@step");
  puts("JUMP $b_list_range$fill");
  puts("LABEL $b_list_range$empty");
  puts("MOVE LF@count int@0");
  puts("LABEL $b_list_range$fill");
  puts("ADD LF@size LF@count int@2");
  puts("NEWARRAY LF@arr LF@size");
  puts("ARRAYSET LF@arr int@0 string@$list:Int");
  puts("ARRAYSET LF@arr int@1 LF@count");
  puts("MOVE LF@cur LF@start");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_range$loop");
  puts("LT LF@cond LF@i LF@count");
  puts("JUMPIFEQ $b_list_range$done LF@cond bool@false");
  puts("ADD LF@idx LF@i int@2");
  puts("ARRAYSET LF@arr LF@idx LF@cur");
  puts("ADD LF@cur LF@cur LF@step");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_range$loop");
  puts("LABEL $b_list_range$done");
  puts("PUSHS LF@arr");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_toStr_any");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@t");
  puts("DEFVAR LF@res");
  puts("POPS LF@val");
  puts("TYPE LF@t LF@val");
  puts("JUMPIFEQ $b_toStr_any$int LF@t string@int");
  puts("JUMPIFEQ $b_toStr_any$float LF@t string@float");
  puts("JUMPIFEQ $b_toStr_any$bool LF@t string@bool");
  puts("JUMPIFEQ $b_toStr_any$string LF@t string@string");
  puts("JUMPIFEQ $b_toStr_any$nil LF@t string@nil");
  puts("JUMPIFEQ $b_toStr_any$array LF@t string@array");
  puts("MOVE LF@res string@?");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$int");
  puts("INT2STR LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$float");
  puts("FLOAT2STR LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$bool");
  puts("MOVE LF@res string@false");
  puts("JUMPIFEQ $b_toStr_any$done LF@val bool@false");
  puts("MOVE LF@res string@true");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$string");
  puts("MOVE LF@res LF@val");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$nil");
  puts("MOVE LF@res string@()");
  puts("JUMP $b_toStr_any$done");
  puts("LABEL $b_toStr_any$array");
  puts("PUSHS LF@val");
  puts("CALL $b_list_toStr");
  puts("POPS LF@res");
  puts("LABEL $b_toStr_any$done");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_toStr");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@list");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@res");
  puts("DEFVAR LF@elem");
  puts("DEFVAR LF@str");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@idx");
  puts("POPS LF@list");
  puts("ARRAYGET LF@len LF@list int@1");
  puts("MOVE LF@res string@List(");
  puts("MOVE LF@i int@0");
  puts("LABEL $b_list_toStr$loop");
  puts("LT LF@cond LF@i LF@len");
  puts("JUMPIFEQ $b_list_toStr$done LF@cond bool@false");
  puts("JUMPIFEQ $b_list_toStr$nosep LF@i int@0");
  puts("CONCAT LF@res LF@res string@,\\032");
  puts("LABEL $b_list_toStr$nosep");
  puts("ADD LF@idx LF@i int@2");
  puts("ARRAYGET LF@elem LF@list LF@idx");
  puts("PUSHS LF@elem");
  puts("CALL $b_toStr_any");
  puts("POPS LF@str");
  puts("CONCAT LF@res LF@res LF@str");
  puts("ADD LF@i LF@i int@1");
  puts("JUMP $b_list_toStr$loop");
  puts("LABEL $b_list_toStr$done");
  puts("CONCAT LF@res LF@res string@)");
  puts("PUSHS LF@res");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_print_any");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@val");
  puts("DEFVAR LF@t");
  puts("POPS LF@val");
  puts("TYPE LF@t LF@val");
  puts("JUMPIFEQ $b_print_any$nil LF@t string@nil");
  puts("JUMPIFEQ $b_print_any$array LF@t string@array");
  puts("WRITE LF@val");
  puts("JUMP $b_print_any$done");
  puts("LABEL $b_print_any$nil");
  puts("WRITE string@()");
  puts("JUMP $b_print_any$done");
  puts("LABEL $b_print_any$array");
  puts("PUSHS LF@val");
  puts("CALL $b_list_print");
  puts("LABEL $b_print_any$done");
  puts("POPFRAME");
  puts("RETURN");

  puts("LABEL $b_list_print");
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@text");
  puts("CALL $b_list_toStr");
  puts("POPS LF@text");
  puts("WRITE LF@text");
  puts("POPFRAME");
  puts("RETURN");
}

/* Vypíše definice všech vestavěných funkcí, které program volá. Volá se z
 * generator_init hned za EXIT int@0 (a před nebo za definicemi uživatelských
 * funkcí, na pořadí nezáleží). */
void emit_builtins(Gen *gen, AST_NODE *root) {
  (void)gen;
  memset(used, 0, sizeof used);
  used_tostr_int = used_tostr_float = used_tostr_bool = false;
  lower_members(root); /* VALMEM: x.toString apod. se přepíše na volání
                          vestavěné funkce (gen_valmem.c) */
  scan(root);

  if (used[B_READLINE])
    def_read("readLine", "string", "string@");
  if (used[B_READINT])
    def_read("readInt", "int", "int@0");
  if (used[B_READBOOLEAN])
    def_readboolean();
  if (used[B_READDOUBLE])
    def_read("readDouble", "float", "float@0x0p+0");
  if (used[B_READINTMAYBE])
    def_readmaybe("readIntMaybe", "int");
  if (used[B_READDOUBLEMAYBE])
    def_readmaybe("readDoubleMaybe", "float");
  if (used_tostr_int)
    def_unary("toStr_int", "INT2STR");
  if (used_tostr_float)
    def_unary("toStr_float", "FLOAT2STR"); /* vždy %.2f, např. 2.50 */
  if (used_tostr_bool)
    def_tostr_bool();
  if (used[B_TOINT])
    def_unary("toInt", "FLOAT2INT"); /* osekne k nule */
  if (used[B_FLOOR])
    def_floor();
  if (used[B_STRLENGTH])
    def_unary("strLength", "STRLEN");
  if (used[B_CHR])
    def_unary("chr", "INT2CHAR"); /* mimo 0 až 127 skončí interpret chybou 58 */
  if (used[B_ORD])
    def_ord();
  if (used[B_SUBSTR])
    def_substr();
  def_list_helpers();
}
