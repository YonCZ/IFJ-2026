/*
 * IFJ26 Compiler
 * File: scanner.c
 * Author: Polina Tishaninova [xtishap00]
 */

#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynamic_string.h"
#include "error.h"
#include "scanner.h"
#include "token.h"

typedef enum {
  START_STATE, // DONE
  WORD_STATE,  // DONE

  INT_STATE,            // DONE
  INT_UNDERSCORE_STATE, // DONE
  DOUBLE_STATE,         // DONE
  DOUBLE_UNDERSCORE_STATE,
  EXP_START_STATE,
  EXP_SIGN_STATE,   // DONE
  EXP_DIGITS_STATE, // DONE
  EXP_UNDERSCORE_STATE,

  ASSIGN_STATE, // DONE
  LESS_STATE,   // DONE
  GREAT_STATE,  // DONE
  EXCLAM_STATE, // DONE

  SLASH_STATE,        // DONE
  LINE_COMMENT_STATE, // DONE
  BLOCK_COMM_STATE,   // DONE

  STRING_STATE,           // DONE
  STRING_ESCAPE_STATE,    // DONE
  MULTILINE_STRING_STATE, // DONE

  OR_STATE,  // DONE
  AND_STATE, // DONE

  INTERP_TEXT_STATE,   // state for reading text inside an interpolated string
  INTERP_ESCAPE_STATE, // state for escape sequences inside interpolation
  INTERP_DOLLAR_STATE, // state when we find a dollar in interpolation
  INTERP_ID_STATE      // state for reading a variable name after the dollar
} State;

// prompt: potrebuju v c u scanneru udrzovat stav mezi volanimi funkce (zasobnik
// na znaky back_stack, top, current_line a dstring pro retizky). nevim presne
// jak to nejlip ulozit, myslis ze static promenny takhle na zacatku souboru
// jsou v pohode? a bude to spravne navazovat na moje vlastni ungetc a fgetc?
// zkontroluj mi jestli tenhle zapis dává smysl zasobnik pro vraceni znaku
// zpatky, nekdy ungetc to nezvladne
static int back_stack[16];
// current number of characters in the stack
static int top = 0;
// current line
static int current_line = 1;

// string for storing the characters we have read
static DString str;
// checks if the string is initialized
static bool ds_initialized = false;

// prompt: kolega me na Discordu upozornil, ze moje reseni interpolovanych
// retezcu je spatne. scanner vraci jeden INTERPOLATED_STRING_TOKEN se surovym
// obsahem, takze by parser musel $id a ${vyraz} znovu skenovat sam a resit $$.
// navic to nefunguje kdyz je uvnitr ${} dalsi retezec, napr. s"${f("x")}" -
// retezec skonci na prvni vnitrni uvozovce. jak mam scanner upravit, aby s"..."
// rozsekal na tokeny podle jeho zadani: INTERP_START (text od s" po prvni $
// nebo konec), INTERP_MID (text mezi dvema interpolacemi, jen kdyz neni
// prazdny), INTERP_END (posledni textova cast), LBRACE pro ${ a RBRACE pro k ni
// sparovanou }? $id je normalni IDENTIFIER, $$ je jeden dolar, $ pred cislici
// nebo klicovym slovem je lexikalni chyba 1, LF v jednoradkovem s"..." (i
// uvnitr ${}) je chyba 1 a existuje i viceradkova varianta s"""...""" prompt:
// muj prvni navrh (INTERP_BEGIN / INTERP_PART / INTERP_EXPR_BEGIN /
// INTERP_EXPR_END) nesedi s tim co kolega potrebuje, protoze prvni textova cast
// ma byt v atributu INTERP_START a posledni v INTERP_END. navic
// `$klicove_slovo` nehlasil chybu a LF uvnitr ${} v jednoradkovem retezci
// proslo. prepis to tak, aby to byly normalni stavy v jednom switchi a nebyl
// potreba flag end_pending zasobnik pro interpolovane retezce, protoze s"..."
// se muze objevit i uvnitr ${ }; pamatuje si jestli je retezec viceradkovy
// s"""..."""
static bool interp_multi[10];
// top of the stack for interpolation
static int interp_top = 0;
// checks if we are inside an expression in interpolation
static bool interp_expr = false;

// function to free the string
static void scanner_cleanup(void) { ds_free(&str); }

// function to fully free the scanner, including interpolation
void scanner_free(void) {
  if (ds_initialized) {
    ds_free(&str);
    ds_initialized = false;
  }
  interp_top = 0;
  interp_expr = false;
}

// prompt: potrebuju vyresit cteni a vraceni znaku pri skenovani tokenu. kdyz
// nactu znak co uz do aktualniho tokenu nepatri nebo musim vratit vic znaku
// zpatky, protoze mi obyc ungetc nejak nejde a dle tveho pred tim doporuceni
// bych to musela napsat sama. jak mam presne napsat vlastni funkce fgetc a
// ungetc s vyuzitim myho back_stacku, aby to nejdriv bralo znaky ze zasobniku a
// zaroven hlídalo zvysovani/snizovani radku? funkce ktera je pri pouziti dalsi
// promenne nebo znaku
static int in_fgetc(FILE *f) {
  int c;
  // if there is something in the stack, take it from there
  if (top > 0) {
    c = back_stack[--top];
  }
  // otherwise read from the file
  else {
    c = fgetc(f);
  }
  // if it is a new line
  if (c == '\n') {
    current_line++;
  }
  return c;
}

// function used to go back to the previous character
static void in_ungetc(FILE *f, int c) {
  // EOF cannot be returned
  if (c == EOF) {
    return;
  }
  // if it is a new line, we have to decrease the line number
  if (c == '\n') {
    current_line--;
  }
  // store it in our array
  if (top < 16) {
    back_stack[top++] = c;
  } else {
    ungetc(c, f);
  }
}

/*prompt: kdyz lexikalni chyba nastane primo na znaku konce radku (nedokonceny
retezec, `1_` na konci radku, `1e` na konci radku...), hlasi se radek o 1 vetsi,
protoze in_fgetc uz current_line zvysil. oprav hlaseni cisla radku*/
// function to fix the line number when the error is at the end of a line
static int err_line(int c) {
  if (c == '\n') {
    return current_line - 1;
  }
  return current_line;
}

// function to check if a character is valid in an identifier
static bool is_id_char(int c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') || c == '_') {
    return true;
  }
  return false;
}

// prompt: jak mam jenomocne napsat funkci, ktera zkontroluje jestli je dane
// slovo klicove slovo jazyka a vrati true/false? funkce pro kontrolu klicovych
// slov
static bool is_keyword(const char *word) {
  if (strcmp(word, "if") == 0 || strcmp(word, "for") == 0 ||
      strcmp(word, "while") == 0 || strcmp(word, "do") == 0 ||
      strcmp(word, "else") == 0 || strcmp(word, "val") == 0 ||
      strcmp(word, "var") == 0 || strcmp(word, "def") == 0 ||
      strcmp(word, "import") == 0 || strcmp(word, "return") == 0 ||
      strcmp(word, "then") == 0 || strcmp(word, "Int") == 0 ||
      strcmp(word, "String") == 0 || strcmp(word, "Double") == 0 ||
      strcmp(word, "Boolean") == 0 || strcmp(word, "Unit") == 0 ||
      strcmp(word, "true") == 0 || strcmp(word, "false") == 0 ||
      strcmp(word, "case") == 0 || strcmp(word, "class") == 0 ||
      strcmp(word, "match") == 0 || strcmp(word, "type") == 0 ||
      strcmp(word, "extension") == 0 || strcmp(word, "List") == 0) {
    return true;
  }
  return false;
}

// main function for reading tokens
static Token scan_raw(FILE *f) {
  // if there is a problem and the buffer must be cleaned up, call cleanup
  if (!ds_initialized) {
    ds_init(&str);
    error_cleanup(scanner_cleanup);
    ds_initialized = true;
  }
  // always clear the string before a new token
  ds_clear(&str);

  // start state
  State state = START_STATE;
  // checks if the string is multi-line
  bool multi_line = false;
  // checks if this is the first part of an interpolation
  bool first_part = false;
  // if we are in an interpolation but not in an expression, we read text
  if (interp_top > 0 && !interp_expr) {
    state = INTERP_TEXT_STATE;
    multi_line = interp_multi[interp_top - 1];
  }
  // the current character
  int c;
  // structure initialized with zeros
  Token token = {0};
  token.atribute = NULL;
  // checks if a token was found
  bool token_found = false;

  // counts the nesting level for block comments
  int nesting_level = 0;
  // checks if the block comment contains a new line
  bool comm_newline = false;

  while (!token_found) {
    c = in_fgetc(f);
    switch (state) {
    /*START_STATE = the main state of the finite automaton. It decides which
    states we go to next, or if we already have a token.
    - it handles spaces, end of line or file, numbers, words, and simple
    characters like *, -, +*/
    case START_STATE:
      // if it is a space
      if (c == ' ' || c == '\t') {
        break;
      } else if (c == '\r') {
        break;
      }
      // check for a bad line end inside interpolation
      else if (c == '\n' && interp_top > 0 && interp_expr &&
               !interp_multi[interp_top - 1]) {
        error_exit(ERROR_LEX, err_line(c),
                   "Newline inside ${...} of single-line interpolated string");
      }
      // if it is the end of a line, it is an EOL_TOKEN
      else if (c == '\n') {
        token.type = EOL_TOKEN;
        token_found = true;
        break;
      }
      // if the file suddenly ends and we are in an interpolation
      else if (c == EOF && interp_top > 0) {
        error_exit(ERROR_LEX, current_line,
                   "Unterminated interpolated string literal");
      }
      // if it is the end of the file, it is an EOF_TOKEN
      else if (c == EOF) {
        token.type = EOF_TOKEN;
        token_found = true;
        break;
      }
      // if it is a digit, we go to INT_STATE
      else if (c >= '0' && c <= '9') {
        state = INT_STATE;
        ds_push_char(&str, c);
      }
      // if it is a letter, we go to WORD_STATE
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        if (c == 's') {
          int next = in_fgetc(f);
          if (next == '"') {
            int q2 = in_fgetc(f);
            if (q2 == '"') {
              int q3 = in_fgetc(f);
              if (q3 == '"') {
                multi_line = true;
              } else {
                in_ungetc(f, q3);
                in_ungetc(f, q2);
              }
            } else {
              in_ungetc(f, q2);
            }
            if (interp_top >= 10) {
              error_exit(ERROR_INTERNAL, current_line,
                         "Interpolation nesting too deep");
            }
            interp_multi[interp_top] = multi_line;
            interp_top++;
            interp_expr = false;
            first_part = true;
            state = INTERP_TEXT_STATE;
            break;
          }
          in_ungetc(f, next);
        }
        state = WORD_STATE;
        ds_push_char(&str, c);
      } else {
        // here we decide what to do with simple characters and send them to
        // other states
        switch (c) {
        // if it is * (multiplication), it is MUL_TOKEN
        case '*':
          token.type = MUL_TOKEN;
          token_found = true;
          break;
        // if it is + (plus), it is PLUS_TOKEN or PLUSPLUS_TOKEN
        case '+': {
          int next = in_fgetc(f);
          if (next == '+') {
            token.type = PLUSPLUS_TOKEN;
            token_found = true;
          } else {
            in_ungetc(f, next);
            token.type = PLUS_TOKEN;
            token_found = true;
          }
          break;
        }
        // if it is - (minus), it is MINUS_TOKEN
        case '-':
          token.type = MINUS_TOKEN;
          token_found = true;
          break;
        // if it is ), it is RPAR_TOKEN
        case ')':
          token.type = RPAR_TOKEN;
          token_found = true;
          break;
        // if it is (, it is LPAR_TOKEN
        case '(':
          token.type = LPAR_TOKEN;
          token_found = true;
          break;
        // if it is [, it is LBRACKET_TOKEN
        case '[':
          token.type = LBRACKET_TOKEN;
          token_found = true;
          break;
        // if it is ], it is RBRACKET_TOKEN
        case ']':
          token.type = RBRACKET_TOKEN;
          token_found = true;
          break;
        // if it is , (comma), it is COMMA_TOKEN
        case ',':
          token.type = COMMA_TOKEN;
          token_found = true;
          break;
        // if it is . (dot), we check if a digit follows. If yes, we go to
        // DOUBLE_STATE, otherwise it is DOT_TOKEN
        case '.': {
          int next = in_fgetc(f);
          if (next >= '0' && next <= '9') {
            ds_push_char(&str, '.');
            ds_push_char(&str, next);
            state = DOUBLE_STATE;
          } else {
            in_ungetc(f, next);
            token.type = DOT_TOKEN;
            token_found = true;
          }
          break;
        }
        // if it is :, it is COLON_TOKEN
        case ':':
          token.type = COLON_TOKEN;
          token_found = true;
          break;
        // if it is ;, it is SEMICOLON_TOKEN
        case ';':
          token.type = SEMICOLON_TOKEN;
          token_found = true;
          break;
        // handling of @
        case '@': {
          char next_chars[5] = {0};
          // we read the next letters, because after @ there must always be
          // main (4 letters)
          for (int i = 0; i < 4; i++) {
            next_chars[i] = in_fgetc(f);
          }
          // if it is main, we check the next character and it is AT_TOKEN
          if (strcmp(next_chars, "main") == 0) {
            int after = in_fgetc(f);
            in_ungetc(f, after);
            if (is_id_char(after)) {
              error_exit(ERROR_LEX, err_line(c),
                         "Invalid annotation, expected @main");
            }
            for (int i = 3; i >= 0; i--) {
              in_ungetc(f, next_chars[i]);
            }
            token.type = AT_TOKEN;
            token_found = true;
          }
          // otherwise @ is not valid, so it is lexical error 1
          else {
            int newlines = 0;
            for (int i = 0; i < 4; i++) {
              if (next_chars[i] == '\n') {
                newlines++;
              }
            }
            error_exit(ERROR_LEX, current_line - newlines,
                       "Invalid character @");
          }
          break;
        }
        // if it is }, we must check if it is the end of an expression in
        // interpolation
        case '}':
          if (interp_top > 0 && interp_expr) {
            interp_expr = false;
            token.type = RBRACE_TOKEN;
            token_found = true;
          } else {
            error_exit(ERROR_LEX, err_line(c), "Unexpected character");
          }
          break;
        // if it is _, we go to WORD_STATE (it can be a word or a number, but
        // we decide that after we know it is not a word)
        case '_':
          ds_push_char(&str, '_');
          state = WORD_STATE;
          break;
        // if it is |, we go to OR_STATE
        case '|':
          state = OR_STATE;
          break;
        // if it is &, we go to AND_STATE
        case '&':
          state = AND_STATE;
          break;
        // if it is /, we go to SLASH_STATE
        case '/':
          state = SLASH_STATE;
          break;
        // if it is ", we go to STRING_STATE (because it can be a word)
        case '"':
          state = STRING_STATE;
          break;
        // if it is =, we go to ASSIGN_STATE (because it can be == and so on)
        case '=':
          state = ASSIGN_STATE;
          break;
        // if it is !, we go to EXCLAM_STATE (to check if it is !=)
        case '!':
          state = EXCLAM_STATE;
          break;
        // if it is <, we go to LESS_STATE
        case '<':
          state = LESS_STATE;
          break;
        // if it is >, we go to GREAT_STATE
        case '>':
          state = GREAT_STATE;
          break;
        // otherwise it is the default error, which is lexical error 1
        default:
          error_exit(ERROR_LEX, err_line(c), "Unexpected character");
        }
      }
      break;

    // WORD_STATE = state where we decide if it is a command / keyword /
    // identifier
    case WORD_STATE:
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_') {
        ds_push_char(&str, c);
      } else {
        in_ungetc(f, c);
        // we decide what word it is and assign it to the token
        if (strcmp(str.data, "if") == 0) {
          token.type = IF_TOKEN;
        } else if (strcmp(str.data, "for") == 0) {
          token.type = FOR_TOKEN;
        } else if (strcmp(str.data, "while") == 0) {
          token.type = WHILE_TOKEN;
        } else if (strcmp(str.data, "do") == 0) {
          token.type = DO_TOKEN;
        } else if (strcmp(str.data, "else") == 0) {
          token.type = ELSE_TOKEN;
        } else if (strcmp(str.data, "val") == 0) {
          token.type = VAL_TOKEN;
        } else if (strcmp(str.data, "var") == 0) {
          token.type = VAR_TOKEN;
        } else if (strcmp(str.data, "def") == 0) {
          token.type = DEF_TOKEN;
        } else if (strcmp(str.data, "import") == 0) {
          token.type = IMPORT_TOKEN;
        } else if (strcmp(str.data, "return") == 0) {
          token.type = RETURN_TOKEN;
        } else if (strcmp(str.data, "then") == 0) {
          token.type = THEN_TOKEN;
        } else if (strcmp(str.data, "Int") == 0) {
          token.type = INT_TOKEN;
        } else if (strcmp(str.data, "String") == 0) {
          token.type = STRING_TOKEN;
        } else if (strcmp(str.data, "Double") == 0) {
          token.type = DOUBLE_TOKEN;
        } else if (strcmp(str.data, "Boolean") == 0) {
          token.type = BOOLEAN_TOKEN;
        } else if (strcmp(str.data, "Unit") == 0) {
          token.type = UNIT_TOKEN;
        } else if (strcmp(str.data, "true") == 0) {
          token.type = TRUE_TOKEN;
        } else if (strcmp(str.data, "false") == 0) {
          token.type = FALSE_TOKEN;
        } else if (strcmp(str.data, "case") == 0) {
          token.type = CASE_TOKEN;
        } else if (strcmp(str.data, "class") == 0) {
          token.type = CLASS_TOKEN;
        } else if (strcmp(str.data, "match") == 0) {
          token.type = MATCH_TOKEN;
        } else if (strcmp(str.data, "type") == 0) {
          token.type = TYPE_TOKEN;
        } else if (strcmp(str.data, "extension") == 0) {
          token.type = EXTENSION_TOKEN;
        } else if (strcmp(str.data, "List") == 0) {
          token.type = LIST_TOKEN;
        } else if (strcmp(str.data, "_") == 0) {
          token.type = UNDERSCORE_TOKEN;
        }
        // otherwise it is a variable or a function name
        else {
          token.type = IDENTIFIER_TOKEN;
          token.atribute = ds_dup(&str);
        }
        token_found = true;
        break;
      }
      break;

    // SLASH_STATE = state that decides if the slash is division or the start
    // of a comment
    case SLASH_STATE:
      // if it is another slash, it is a // comment and we go to
      // LINE_COMMENT_STATE
      if (c == '/') {
        state = LINE_COMMENT_STATE;
      }
      // if it is a slash followed by *, it is /* and we go to BLOCK_COMM_STATE
      else if (c == '*') {
        state = BLOCK_COMM_STATE;
        nesting_level = 1;
        comm_newline = false;
      }
      // otherwise it is division
      else {
        in_ungetc(f, c);
        token.type = DIV_TOKEN;
        token_found = true;
        break;
      }
      break;

    // BLOCK_COMM_STATE = state that looks for the star which ends the comment
    case BLOCK_COMM_STATE:
      // if it is the end of the file, it is a lexical error
      if (c == EOF) {
        error_exit(ERROR_LEX, err_line(c), "Unterminated block comment");
      }
      // if it is a new line, we remember that the comment has a new line
      else if (c == '\n') {
        comm_newline = true;
      }
      // if a slash starts again, check if it is a nested block comment
      else if (c == '/') {
        int next = in_fgetc(f);
        if (next == '*') {
          nesting_level++;
        } else {
          in_ungetc(f, next);
        }
      }
      // if it is a star, we check if a slash follows. If the comment ends, we
      // go to START_STATE or return EOL_TOKEN
      else if (c == '*') {
        int next = in_fgetc(f);
        if (next == '/') {
          nesting_level--;
          if (nesting_level == 0) {
            if (comm_newline) {
              int check;
              while ((check = in_fgetc(f)) == ' ' || check == '\t')
                ;
              if (check != '\n' && check != '\r' && check != EOF) {
                error_exit(
                    ERROR_LEX, err_line(c),
                    "Disallowed characters after multi-line block comment");
              }
              in_ungetc(f, check);
              token.type = EOL_TOKEN;
              token_found = true;
              break;
            } else {
              state = START_STATE;
            }
          }
        } else {
          in_ungetc(f, next);
        }
      }
      break;

    // LINE_COMMENT_STATE = state that checks if the comment ended, otherwise
    // it goes to the start state
    case LINE_COMMENT_STATE:
      if (c == '\n' || c == EOF) {
        in_ungetc(f, c);
        state = START_STATE;
      }
      break;

    // INT_STATE = state for numbers. It decides if it is an int, a double, or
    // if it contains some bad characters
    case INT_STATE:
      // if it is a digit, we add it to the result
      if (c <= '9' && c >= '0') {
        ds_push_char(&str, c);
      }
      // if it is a dot, we go to DOUBLE_STATE
      else if (c == '.') {
        int next = in_fgetc(f);
        if (next >= '0' && next <= '9') {
          ds_push_char(&str, '.');
          ds_push_char(&str, next);
          state = DOUBLE_STATE;
        }
        // otherwise it is INT_NUM_TOKEN, so it is a number
        else {
          in_ungetc(f, next);
          in_ungetc(f, '.');
          token.type = INT_NUM_TOKEN;
          token.atribute = ds_dup(&str);
          token_found = true;
          break;
        }
      }
      // if it is _, we go to INT_UNDERSCORE_STATE, e.g. 1_000
      else if (c == '_') {
        state = INT_UNDERSCORE_STATE;
      }
      // if it is e or E, we go to EXP_START_STATE
      else if (c == 'e' || c == 'E') {
        ds_push_char(&str, c);
        state = EXP_START_STATE;
      }
      // if a letter appears, it is lexical error 1
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        error_exit(ERROR_LEX, err_line(c),
                   "Unexpected character in integer literal");
      }
      // otherwise it is a number and belongs to INT_NUM_TOKEN
      else {
        in_ungetc(f, c);
        token.type = INT_NUM_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
        break;
      }
      break;
    // INT_UNDERSCORE_STATE = state that checks if a digit follows after _.
    // If a digit follows, it is a correct format like 1_000. If it is
    // something else, it is a lexical error
    case INT_UNDERSCORE_STATE:
      if (c <= '9' && c >= '0') {
        ds_push_char(&str, c);
        state = INT_STATE;
      } else {
        error_exit(ERROR_LEX, err_line(c),
                   "Invalid number format with underscore");
      }
      break;

    // DOUBLE_STATE = state that checks if it is a double number and what can
    // follow it
    case DOUBLE_STATE:
      // if it is a double, we check for a digit and stay in DOUBLE_STATE
      if (c >= '0' && c <= '9') {
        ds_push_char(&str, c);
        state = DOUBLE_STATE;
      }
      // if it is _, it is not an int anymore but DOUBLE_UNDERSCORE_STATE
      else if (c == '_') {
        state = DOUBLE_UNDERSCORE_STATE;
      }
      // if it contains E or e, we go to EXP_START_STATE
      else if (c == 'e' || c == 'E') {
        ds_push_char(&str, c);
        state = EXP_START_STATE;
      }
      // if it contains D or d, it is accepted, but if a digit or a letter
      // follows, it is a lexical error
      else if (c == 'd' || c == 'D') {
        int next = in_fgetc(f);
        if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
            (next >= '0' && next <= '9') || next == '_') {
          error_exit(ERROR_LEX, err_line(c), "Invalid double suffix");
        }
        in_ungetc(f, next);
        token.type = DOUBLE_NUM_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
        break;
      }
      // the same, but if any letter appears at all
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        error_exit(ERROR_LEX, err_line(c),
                   "Unexpected character in double literal");
      }
      // otherwise it is DOUBLE_NUM_TOKEN (a double number)
      else {
        in_ungetc(f, c);
        token.type = DOUBLE_NUM_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
        break;
      }
      break;
    // DOUBLE_UNDERSCORE_STATE = state that checks if a digit follows after _
    case DOUBLE_UNDERSCORE_STATE:
      if (c <= '9' && c >= '0') {
        ds_push_char(&str, c);
        state = DOUBLE_STATE;
      }
      // it throws an error if there is something else
      else {
        error_exit(ERROR_LEX, err_line(c),
                   "Invalid double format with underscore");
      }
      break;

    // EXP_START_STATE = state where the number has an exponent mark (e or E),
    // so we decide where to go next
    case EXP_START_STATE:
      // if it is + or -, we go to EXP_SIGN_STATE
      if (c == '+' || c == '-') {
        ds_push_char(&str, c);
        state = EXP_SIGN_STATE;
      }
      // if it is a digit, we go to EXP_DIGITS_STATE
      else if (c >= '0' && c <= '9') {
        ds_push_char(&str, c);
        state = EXP_DIGITS_STATE;
      }
      // otherwise there is no exponent, so it is lexical error 1
      else {
        error_exit(ERROR_LEX, err_line(c), "Exponent expected");
      }
      break;

    // EXP_SIGN_STATE = state that checks if a digit follows after the sign and
    // goes to EXP_DIGITS_STATE, otherwise it is a lexical error
    case EXP_SIGN_STATE:
      if (c >= '0' && c <= '9') {
        ds_push_char(&str, c);
        state = EXP_DIGITS_STATE;
      } else {
        error_exit(ERROR_LEX, err_line(c), "Digit expected in exponent");
      }
      break;

    // EXP_DIGITS_STATE = state that reads the exponent digits and checks which
    // other characters can follow
    case EXP_DIGITS_STATE:
      if (c >= '0' && c <= '9') {
        ds_push_char(&str, c);
      }
      // if it is _, we go to EXP_UNDERSCORE_STATE
      else if (c == '_') {
        state = EXP_UNDERSCORE_STATE;
      }
      // if there is d, it is DOUBLE_NUM_TOKEN. No letters or digits can follow
      // it, otherwise it is lexical error 1
      else if (c == 'd' || c == 'D') {
        int next = in_fgetc(f);
        if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
            (next >= '0' && next <= '9') || next == '_') {
          error_exit(ERROR_LEX, err_line(c), "Invalid exponent suffix");
        }
        in_ungetc(f, next);
        token.type = DOUBLE_NUM_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
        break;
      }
      // if there is any other letter, it is lexical error 1
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        error_exit(ERROR_LEX, err_line(c),
                   "Unexpected character after exponent");
      }
      // otherwise it is a number and the token is DOUBLE_NUM_TOKEN
      else {
        in_ungetc(f, c);
        token.type = DOUBLE_NUM_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
        break;
      }
      break;
    // EXP_UNDERSCORE_STATE = works the same as DOUBLE_UNDERSCORE_STATE
    case EXP_UNDERSCORE_STATE:
      if (c <= '9' && c >= '0') {
        ds_push_char(&str, c);
        state = EXP_DIGITS_STATE;
      } else {
        error_exit(ERROR_LEX, err_line(c),
                   "Invalid exponent format with underscore");
      }
      break;

    // ASSIGN_STATE = handles other cases that contain =
    case ASSIGN_STATE:
      // it is EQ_TOKEN ==
      if (c == '=') {
        token.type = EQ_TOKEN;
        token_found = true;
        break;
      }
      // it is ARROW_FAT_TOKEN (=>)
      if (c == '>') {
        token.type = ARROW_FAT_TOKEN;
        token_found = true;
        break;
      }
      // otherwise it is =
      else {
        in_ungetc(f, c);
        token.type = ASSIGN_TOKEN;
        token_found = true;
        break;
      }
      break;
    // LESS_STATE = state that handles <
    case LESS_STATE:
      // if it is <=
      if (c == '=') {
        token.type = LESS_EQ_TOKEN;
        token_found = true;
        break;
      }
      // if it is <-
      else if (c == '-') {
        token.type = ARROW_LEFT_TOKEN;
        token_found = true;
        break;
      }
      // otherwise it is <
      else {
        in_ungetc(f, c);
        token.type = LESS_TOKEN;
        token_found = true;
        break;
      }
      break;
    // GREAT_STATE = handles >
    case GREAT_STATE:
      // it is >=
      if (c == '=') {
        token.type = GREAT_EQ_TOKEN;
        token_found = true;
        break;
      }
      // otherwise it is >
      else {
        in_ungetc(f, c);
        token.type = GREAT_TOKEN;
        token_found = true;
        break;
      }
      break;
    // EXCLAM_STATE = handles negation
    case EXCLAM_STATE:
      // if it is !=
      if (c == '=') {
        token.type = NEQ_TOKEN;
        token_found = true;
        break;
      }
      // otherwise it is NOT_TOKEN
      else {
        in_ungetc(f, c);
        token.type = NOT_TOKEN;
        token_found = true;
        break;
      }
      break;
    // OR_STATE = state that handles the | character
    case OR_STATE:
      // it is OR_TOKEN ||
      if (c == '|') {
        token.type = OR_TOKEN;
        token_found = true;
        break;
      }
      // just PIPE_TOKEN |
      else {
        in_ungetc(f, c);
        token.type = PIPE_TOKEN;
        token_found = true;
        break;
      }
      break;
    // AND_STATE = state that handles the & character
    case AND_STATE:
      if (c == '&') {
        token.type = AND_TOKEN;
        token_found = true;
        break;
      }
      // if it is not &&, it is lexical error 1
      else {
        error_exit(ERROR_LEX, err_line(c),
                   "Expected '&' character for AND operator");
      }
      break;

    // STRING_STATE = state that reads the text of a string literal
    case STRING_STATE:
      // if it is ", the string may end here
      if (c == '"') {
        // if the string is empty, we check for another "
        if (str.len == 0) {
          int next_c = in_fgetc(f);
          // if there is another ", it is a multi-line string
          if (next_c == '"') {
            state = MULTILINE_STRING_STATE;
          }
          // otherwise it is an empty STRING_WORD_TOKEN
          else {
            in_ungetc(f, next_c);
            token.type = STRING_WORD_TOKEN;
            token.atribute = ds_dup(&str);
            token_found = true;
          }
        }
        // if the string is not empty, the closing " ends it
        else {
          token.type = STRING_WORD_TOKEN;
          token.atribute = ds_dup(&str);
          token_found = true;
        }
        break;
      }
      // if there is a backslash, an escape sequence starts
      else if (c == '\\') {
        state = STRING_ESCAPE_STATE;
      }
      // if there is a line end or the end of the file, it is lexical error 1
      else if (c == '\n' || c == '\r' || c == EOF) {
        error_exit(ERROR_LEX, err_line(c), "Unterminated string literal");
      }
      // otherwise we just add the character to the string
      else {
        ds_push_char(&str, c);
      }
      break;
    // STRING_ESCAPE_STATE = state that handles escape sequences
    case STRING_ESCAPE_STATE:
      if (c == '"') {
        ds_push_char(&str, '"');
      } else if (c == 'n') {
        ds_push_char(&str, '\n');
      } else if (c == 'r') {
        ds_push_char(&str, '\r');
      } else if (c == 't') {
        ds_push_char(&str, '\t');
      } else if (c == '\\') {
        ds_push_char(&str, '\\');
      }
      // if none of these appears, it is a lexical error, because the escape
      // sequence is not valid
      else {
        error_exit(ERROR_LEX, err_line(c), "Invalid escape sequence");
      }
      // after that we go back to reading the string
      state = STRING_STATE;
      break;
    // MULTILINE_STRING_STATE = state that reads the text between """ and """
    case MULTILINE_STRING_STATE:
      // if it is EOF, it is a lexical error
      if (c == EOF) {
        error_exit(ERROR_LEX, err_line(c),
                   "Unterminated multi-line string literal");
      }
      // if there is a ", we count how many quotes follow
      else if (c == '"') {
        int quotes = 1;
        int q;
        while ((q = in_fgetc(f)) == '"') {
          quotes++;
        }
        in_ungetc(f, q);
        if (quotes >= 3) {
          for (int i = 0; i < quotes - 3; i++) {
            ds_push_char(&str, '"');
          }
          token.type = STRING_WORD_TOKEN;
          token.atribute = ds_dup(&str);
          token_found = true;
          break;
        }
        // fewer than 3 quotes are just a normal part of the text, so we add
        // them to the string
        for (int i = 0; i < quotes; i++) {
          ds_push_char(&str, '"');
        }
      } else {
        ds_push_char(&str, c);
      }
      break;

    // INTERP_TEXT_STATE = state that reads text in an interpolated string
    // s"..."
    case INTERP_TEXT_STATE:
      // if it is EOF, it is a lexical error
      if (c == EOF) {
        error_exit(ERROR_LEX, current_line,
                   "Unterminated interpolated string literal");
      }
      // if it is a quote, it can be the end
      else if (c == '"') {
        int quotes = 1;
        // in a multi-line string we count the quotes, otherwise one quote is
        // the end of the string
        if (multi_line) {
          int q;
          // repeat while more quotes follow
          while ((q = in_fgetc(f)) == '"') {
            quotes++;
          }
          in_ungetc(f, q);
        }
        // in a multi-line string, fewer than 3 quotes are just text
        if (multi_line && quotes < 3) {
          for (int i = 0; i < quotes; i++) {
            ds_push_char(&str, '"');
          }
        }
        // otherwise we continue and decide if it is the end of the
        // interpolation or not
        else {
          int closing = 1;
          // again check for multi-line and how many quotes close the string
          if (multi_line) {
            closing = 3;
            // extra quotes before the closing ones belong to the text
            for (int i = 0; i < quotes - 3; i++) {
              ds_push_char(&str, '"');
            }
          }
          // if it is the first part, we put the quotes back and the token
          // is INTERP_START_TOKEN
          if (first_part) {
            for (int i = 0; i < closing; i++) {
              in_ungetc(f, '"');
            }
            // the token is written as start
            token.type = INTERP_START_TOKEN;
          }
          // otherwise it is the end and the token is written as end
          else {
            interp_top--;
            // check if we are still inside another interpolation
            if (interp_top > 0) {
              interp_expr = true;
            }
            token.type = INTERP_END_TOKEN;
          }
          // we save the attribute and the token is ready
          token.atribute = ds_dup(&str);
          token_found = true;
        }
      }
      // if we find a dollar, we go to handle a variable
      else if (c == '$') {
        state = INTERP_DOLLAR_STATE;
      }
      // if we find a backslash, we handle an escape sequence
      else if (c == '\\' && !multi_line) {
        state = INTERP_ESCAPE_STATE;
      }
      // if we find a line end, it is lexical error 1
      else if ((c == '\n' || c == '\r') && !multi_line) {
        error_exit(ERROR_LEX, err_line(c),
                   "Unterminated interpolated string literal");
      }
      // otherwise we add the character to the string
      else {
        ds_push_char(&str, c);
      }
      break;

    // INTERP_ESCAPE_STATE = state that handles escape sequences inside
    // interpolation
    case INTERP_ESCAPE_STATE:
      if (c == '"') {
        ds_push_char(&str, '"');
      } else if (c == 'n') {
        ds_push_char(&str, '\n');
      } else if (c == 'r') {
        ds_push_char(&str, '\r');
      } else if (c == 't') {
        ds_push_char(&str, '\t');
      } else if (c == '\\') {
        ds_push_char(&str, '\\');
      }
      // if nothing matches, it is lexical error 1, because the escape
      // sequence is not valid
      else {
        error_exit(ERROR_LEX, err_line(c), "Invalid escape sequence");
      }
      state = INTERP_TEXT_STATE;
      break;

    // INTERP_DOLLAR_STATE = state that starts when we find $ in interpolation
    case INTERP_DOLLAR_STATE:
      // if another dollar follows, we write only one
      if (c == '$') {
        ds_push_char(&str, '$');
        state = INTERP_TEXT_STATE;
      }
      // if it is a brace or a letter, we start reading a variable
      else if (c == '{' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               c == '_') {
        // if it is the first part of the interpolation, it is written as
        // INTERP_START_TOKEN
        if (first_part) {
          in_ungetc(f, c);
          in_ungetc(f, '$');
          token.type = INTERP_START_TOKEN;
          token.atribute = ds_dup(&str);
          token_found = true;
        }
        // if there is some text before, it is written as INTERP_MID_TOKEN
        else if (str.len > 0) {
          in_ungetc(f, c);
          in_ungetc(f, '$');
          token.type = INTERP_MID_TOKEN;
          token.atribute = ds_dup(&str);
          token_found = true;
        }
        // if it is a brace, an expression starts and the token is LBRACE_TOKEN
        else if (c == '{') {
          interp_expr = true;
          token.type = LBRACE_TOKEN;
          token_found = true;
        }
        // if it is a letter, we go to INTERP_ID_STATE
        else {
          ds_push_char(&str, c);
          state = INTERP_ID_STATE;
        }
      }
      // otherwise it is lexical error 1, because $ is not valid here
      else {
        error_exit(ERROR_LEX, err_line(c),
                   "Invalid '$' in interpolated string");
      }
      break;

    // INTERP_ID_STATE = state that reads an identifier after $
    case INTERP_ID_STATE:
      // if it is a letter or a digit, we add it to the string
      if (is_id_char(c)) {
        ds_push_char(&str, c);
      }
      // if it is the end of the identifier, we write it as IDENTIFIER_TOKEN
      else {
        in_ungetc(f, c);
        // check if it is _ or a keyword. If yes, it is lexical error 1
        if (strcmp(str.data, "_") == 0 || is_keyword(str.data)) {
          error_exit(ERROR_LEX, err_line(c),
                     "Invalid identifier after '$' in interpolated string");
        }
        token.type = IDENTIFIER_TOKEN;
        token.atribute = ds_dup(&str);
        token_found = true;
      }
      break;
    // if it is none of these, it is internal error 99
    default:
      error_exit(ERROR_INTERNAL, current_line, "Unknown state in fsm");
    }
  }
  if (token.type == INT_NUM_TOKEN && token.atribute[0] == '0' &&
      token.atribute[strspn(token.atribute, "0")] != '\0') {
    error_exit(ERROR_LEX, err_line(c), "Nonzero integer starts with zero");
  }
  if (token.type == DOUBLE_NUM_TOKEN &&
      !isfinite(strtod(token.atribute, NULL))) {
    error_exit(ERROR_LEX, err_line(c), "Double literal out of range");
  }
  // if it is the end of a line, set the line of the finished line
  if (token.type == EOL_TOKEN) {
    token.line = current_line - 1;
  }
  // otherwise we are still in the same line
  else {
    token.line = current_line;
  }
  return token;
}

// prompt: mam problem v count_indent s blokovyma komentarama /* */ na zacatku
// radku. vyhazuje mi to spatne indent a dedent tokeny protoze presakuje jenom
// // komentare. jak mam upravit count_indent aby nacital cele blokove komentare
// vcetne vnorenych, vynuloval mezery pokud je komentar na samostatnem radku a
// hodil lexikalni chybu 1 pokud je za viceradkovym komentarem jeste kod na
// stejnem radku? funkce ktera resi INDENT A DEDENT (proste mezery)
static int count_indent(FILE *f) {
  // variable where we count the spaces
  int spaces = 0;
  // checks if there is a tab
  bool has_tab = false;
  // the current character
  int c;

  // we work until the end of the file
  while ((c = in_fgetc(f)) != EOF) {
    // check all spaces
    if (c == ' ') {
      spaces++;
    }
    // check all tabs
    else if (c == '\t') {
      has_tab = true;
    }
    // check the end of a line
    else if (c == '\n') {
      spaces = 0;
      has_tab = false;
    }
    // it is a comment
    else if (c == '\r') {
    } else if (c == '/') {
      int next = in_fgetc(f);
      // if it is //, we skip everything
      if (next == '/') {
        while ((c = in_fgetc(f)) != '\n' && c != EOF)
          ;
        if (c == EOF) {
          return 0;
        }
        // after the comment we count everything again
        spaces = 0;
        has_tab = false;
      }
      // if it is /*, we skip the block comment including nested ones
      else if (next == '*') {
        int nesting_level = 1;
        bool comm_newline = false;

        // we read the whole block comment
        while (nesting_level > 0) {
          int bc = in_fgetc(f);
          if (bc == EOF) {
            error_exit(ERROR_LEX, current_line, "Unterminated block comment");
          }
          if (bc == '\n') {
            comm_newline = true;
          } else if (bc == '/') {
            int bc_next = in_fgetc(f);
            if (bc_next == '*') {
              nesting_level++;
            } else {
              in_ungetc(f, bc_next);
            }
          } else if (bc == '*') {
            int bc_next = in_fgetc(f);
            if (bc_next == '/') {
              nesting_level--;
            } else {
              in_ungetc(f, bc_next);
            }
          }
        }

        // check what is after the block comment
        int after;
        while ((after = in_fgetc(f)) == ' ' || after == '\t')
          ;
        // if there is a line end or the end of the file, the line is empty
        if (after == '\n' || after == '\r' || after == EOF) {
          if (after == EOF) {
            return 0;
          }
          spaces = 0;
          has_tab = false;
        }
        // otherwise there is still some code
        else {
          // if the comment had a new line and there is code after it, it is
          // lexical error 1
          if (comm_newline) {
            error_exit(ERROR_LEX, current_line,
                       "Disallowed characters after multi-line block comment");
          }
          // otherwise we put the character back and check for a tab
          in_ungetc(f, after);
          if (has_tab) {
            error_exit(ERROR_LEX, current_line,
                       "Tabulator on a non-empty line");
          }
          break;
        }
      }
      // otherwise we go back before the /
      else {
        in_ungetc(f, next);
        in_ungetc(f, '/');
        // and if there is a tab, it is lexical error 1
        if (has_tab) {
          error_exit(ERROR_LEX, current_line, "Tabulator on a non-empty line");
        }
        break;
      }
    }
    // if it is none of these, we go back
    else {
      in_ungetc(f, c);
      // if there is a tab, it is lexical error 1 again
      if (has_tab) {
        error_exit(ERROR_LEX, current_line, "Tabulator on a non-empty line");
      }
      break;
    }
  }
  // if we reach the end of the file, return 0
  if (c == EOF) {
    return 0;
  }
  // otherwise we return the number of spaces
  return spaces;
}

// prompt:chci mit fsm a indent, dedent v jedne funkci get_token, proc je lepsi
// rozdelit tak, ze scan_raw bude jenom ciste skenovat tokeny z fsm, a nova
// funkce get_token bude hlavni fce, ktery vola count_indent a potom dalsi
// funkci? ukazmi tona nejakem prikladu protoze nerozumim jak mam to vyresti at
// tam nejsou zadne zadrhely hlavni funkce ve ktere ziskavame tokeny a
// zjistujeme co to je
Token get_token(FILE *f) {
  // stack for indentation, it remembers all levels
  static int indent_stack[100] = {0};
  // top of the stack
  static int stack = 0;
  // counts the dedents we still have to send
  static int detents_reading = 0;
  // checks if we are at the start of a line
  static bool line_start = true;
  // counts the open brackets (nesting)
  static int parents = 0;
  // checks if the last token was the end of a line
  static bool last_eol = true;
  static bool newline_skip = false;

  // we start with an empty token
  Token token = {0};

  // if there is a dedent waiting, we send it
  if (detents_reading > 0) {
    detents_reading--;
    token.type = DEDENT_TOKEN;
    token.line = current_line;
    return token;
  }
  // if a line starts and we are not inside any brackets
  if (line_start && parents == 0) {
    line_start = false;
    // we count the spaces
    int spaces = count_indent(f);

    // if there are more spaces than on the previous level, we make an indent
    if (spaces > indent_stack[stack]) {
      // protection against overflow, it is internal error 99
      if (stack >= 99) {
        error_exit(ERROR_INTERNAL, current_line, "Indentation stack overflow");
      }
      // we save the new level
      stack++;
      indent_stack[stack] = spaces;
      token.type = INDENT_TOKEN;
      token.line = current_line;
      return token;
    }
    // if there are fewer spaces than on the previous level, we make a dedent
    else if (spaces < indent_stack[stack]) {
      // we remove levels from the stack
      while (stack > 0 && spaces < indent_stack[stack]) {
        stack--;
        detents_reading++;
      }
      // if the number of spaces does not match, it is lexical error 1
      if (indent_stack[stack] != spaces) {
        error_exit(ERROR_LEX, current_line,
                   "Indentation level does not match any outer level");
      }
      // we send the first dedent
      detents_reading--;
      token.type = DEDENT_TOKEN;
      token.line = current_line;
      return token;
    }
  }

  // we use scan_raw to load the tokens (the characters we scanned)
  token = scan_raw(f);

  // if it is an opening bracket, we increase parents (nesting)
  if (token.type == LPAR_TOKEN || token.type == LBRACKET_TOKEN ||
      token.type == LBRACE_TOKEN) {
    parents++;
  }
  // if it is a closing bracket, we decrease it
  else if (token.type == RPAR_TOKEN || token.type == RBRACKET_TOKEN ||
           token.type == RBRACE_TOKEN) {
    if (parents > 0) {
      parents--;
    }
  }

  // if scan_raw gives us the end of the file
  if (token.type == EOF_TOKEN) {
    // we still check the end of the line
    if (!last_eol) {
      line_start = true;
      last_eol = true;
      token.type = EOL_TOKEN;
      token.line = current_line;
      return token;
    }
    // and we must also close all levels
    if (stack > 0) {
      while (stack > 0) {
        stack--;
        detents_reading++;
      }
      detents_reading--;
      token.type = DEDENT_TOKEN;
      token.line = current_line;
      return token;
    }
    return token;
  }
  // if we get the end of a line
  if (token.type == EOL_TOKEN) {
    // we call the function again if the line end is ignored (inside brackets)
    // and we get a normal token
    if (parents > 0) {
      newline_skip = true;
      return get_token(f);
    }
    // the same if an empty line appears, we call the function again
    line_start = true;
    if (last_eol) {
      return get_token(f);
    }
    last_eol = true;
    // otherwise we return the token
    return token;
  }
  token.check_new_line = newline_skip;
  newline_skip = false;
  // if the token is not EOL, we remember it and return it
  last_eol = false;
  return token;
}
