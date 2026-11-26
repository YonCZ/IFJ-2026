/*
 * IFJ26 Compiler
 * File: token.h
 * Author: Polina Tihaninova [xtishap00]
 *
 * Prompt: Pomoz mi sjednotit nazvy tokenu a strukturu Token
 * s aktualnim scannerem a parserem. Zachovej moje komentare
 * a rozdeleni tokenu, dopln potrebne tokeny pro interpolaci
 * a zkontroluj, ze jejich nazvy odpovidaji pouziti v projektu.
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>

typedef enum {
  // operators
  PLUS_TOKEN,     // plus +
  MINUS_TOKEN,    // minus -
  MUL_TOKEN,      // multiplication *
  DIV_TOKEN,      // division /
  ASSIGN_TOKEN,   // assignment =
  EQ_TOKEN,       // equal ==
  NEQ_TOKEN,      // not equal !=
  LESS_EQ_TOKEN,  // less or equal <=
  GREAT_EQ_TOKEN, // greater or equal >=
  LESS_TOKEN,     // less <
  GREAT_TOKEN,    // greater >

  // separators and general symbols
  RPAR_TOKEN,      // round bracket ) on the right
  LPAR_TOKEN,      // round bracket ( on the left
  DOT_TOKEN,       // dot .
  COMMA_TOKEN,     // comma ,
  AT_TOKEN,        // at sign @
  COLON_TOKEN,     // colon :
  SEMICOLON_TOKEN, // semicolon ;

  // keywords
  DEF_TOKEN,    // def
  IF_TOKEN,     // condition if
  WHILE_TOKEN,  // loop while
  DO_TOKEN,     // do while loop
  ELSE_TOKEN,   // else part of if
  THEN_TOKEN,   // then part of if
  RETURN_TOKEN, // return
  IMPORT_TOKEN, // import
  VAR_TOKEN,    // variable that can be changed
  VAL_TOKEN,    // variable that cannot be changed

  // types
  INT_TOKEN,     // int
  STRING_TOKEN,  // string
  DOUBLE_TOKEN,  // double
  UNIT_TOKEN,    // unit (the type with no real value)
  BOOLEAN_TOKEN, // boolean

  // identifiers and literals
  INT_NUM_TOKEN,     // real integer number (5)
  DOUBLE_NUM_TOKEN,  // real double number (3.5)
  STRING_WORD_TOKEN, // real text (dog)
  UNIT_LIT_TOKEN,    // value ()
  IDENTIFIER_TOKEN,  // name of a variable

  // comments
  SLASH_COMM_TOKEN,     // comment
  SLASH_COMM_END_TOKEN, // end of a comment

  // structure
  INDENT_TOKEN, // higher indentation level
  DEDENT_TOKEN, // lower indentation level
  EOF_TOKEN,    // end of file
  EOL_TOKEN,    // end of line

  // EXTENSIONS
  FOR_TOKEN,       // for loop
  AND_TOKEN,       //&&
  OR_TOKEN,        //||
  NOT_TOKEN,       //!
  TRUE_TOKEN,      // true
  FALSE_TOKEN,     // false
  CASE_TOKEN,      // case
  CLASS_TOKEN,     // class
  MATCH_TOKEN,     // match
  TYPE_TOKEN,      // type
  EXTENSION_TOKEN, // extension
  LIST_TOKEN,      // list

  PLUSPLUS_TOKEN,
  LBRACKET_TOKEN,   //[
  RBRACKET_TOKEN,   //]
  LBRACE_TOKEN,     //{
  RBRACE_TOKEN,     //}
  ARROW_FAT_TOKEN,  //=>
  ARROW_LEFT_TOKEN, //<-
  ARROW_RIGHT_TOKEN,
  ARROW_TOKEN,
  EXPRESION_TOKEN,    // expression
  UNDERSCORE_TOKEN,   // _
  PIPE_TOKEN,         //|
  INTERP_START_TOKEN, // start of an interpolation
  INTERP_MID_TOKEN,   // middle part of an interpolation
  INTERP_END_TOKEN,   // end of an interpolation
} Tokens;

typedef struct {
  Tokens type;         // type of the token
  char *atribute;      // text value
  int line;            // used for error messages
  bool check_new_line; // information about a new line
} Token;

#endif
