/*
 * IFJ Implimentace prekladace
 * Soubor: error.h
 * Autor: Polina Tihaninova [xtishap00]
 */

#ifndef ERROR_H
#define ERROR_H

// vypsani vsech chyb
typedef enum {
  ERROR_LEX = 1, // lexikalni chyba
  ERROR_SYN = 2, // syntakticka chyba
  ERROR_SEM_UNDEF =
      3, // semanticka chyba -> pouziti nedefinovane funkce, promenne nebo typu
  ERROR_SEM_REDEF =
      4, // semanticka chyba -> nepovolena redefinice funkce, promenne, typu
  ERROR_SEM_ARG = 5,  // semanticka chyba -> nespravny pocet neb otyp argumntu
                      // pri volani funkce, nepovolene parametry funkce s main
  ERROR_SEM_TYPE = 6, // semanticka chyba -> semanticka chyba typove
                      // kompatibility nebo odvozeni typu ve vyrazu
  ERROR_SEM_READ = 7, // semanticka chyba-> pokus o zapis do nemodifikovane
                      // promenne nebo formlni parametru
  ERROR_SEM_OTHER =
      10, // ostatni semnaticke chyby (nepovoleny return nevycerpavajicii match)
  ERROR_INTERNAL = 99 // interni chyba
} Error_state;

typedef void (*cleanup)(void);
// volani uklizeni =
void error_cleanup(cleanup st);

// volani vypisu chyb
void error_exit(Error_state code, int line, const char *name, ...);

#endif