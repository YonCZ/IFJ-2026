/*
 * IFJ Implimentace prekladace
 * Soubor: error.h
 * Autor: Polina Tihaninova [xtishap00]
 */

#ifndef ERROR_H
#define ERROR_H

// vypsani vsech chyb
typedef enum {
  // lexikalni chyba
  ERROR_LEX = 1,
  // syntakticka chyba
  ERROR_SYN = 2,
  // semanticka chyba -> pouziti nedefinovane funkce, promenne nebo typu
  ERROR_SEM_UNDEF = 3,
  // semanticka chyba -> nepovolena redefinice funkce, promenne, typu
  ERROR_SEM_REDEF = 4,
  // semanticka chyba -> nespravny pocet nebo typ argumentu pri volani funkce
  ERROR_SEM_ARG = 5,
  // semanticka chyba -> typova nekompatibilita nebo odvozeni typu ve vyrazu
  ERROR_SEM_TYPE = 6,
  // semanticka chyba -> pokus o zapis do nemodifikovane promenne nebo parametru
  ERROR_SEM_READ = 7,
  // ostatni semanticke chyby (nepovoleny return, nevycerpavajici match)
  ERROR_SEM_OTHER = 10,
  // interni chyba
  ERROR_INTERNAL = 99
} Error_state;

typedef void (*cleanup)(void);
// volani uklizeni =
void error_cleanup(cleanup st);

// volani vypisu chyb
void error_exit(Error_state code, int line, const char *name, ...);

#endif