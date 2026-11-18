/*
 * IFJ Implimentace prekladace
 * Soubor: scanner.c
 * Autor: Polina Tihaninova [xtishap00]
 */

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "scanner.h"
#include "error.h"
#include "dynamic_string.h"

typedef enum {
    START_STATE,        //DONE
    WORD_STATE,         //DONE

    INT_STATE,            //DONE
    INT_UNDERSCORE_STATE,      //DONE
    DOUBLE_STATE,       //DONE
    DOUBLE_UNDERSCORE_STATE,
    EXP_START_STATE,
    EXP_SIGN_STATE,       //DONE
    EXP_DIGITS_STATE,      //DONE
    EXP_UNDERSCORE_STATE,

    ASSIGN_STATE,       //DONE
    LESS_STATE,         //DONE
    GREAT_STATE,        //DONE
    EXCLAM_STATE,       //DONE

    SLASH_STATE,        //DONE
    LINE_COMMENT_STATE,     //DONE
    BLOCK_COMM_STATE,       //DONE

    STRING_STATE,       //DONE
    STRING_ESCAPE_STATE,     //DONE
    MULTILINE_STRING_STATE,     //DONE

    ZERO_STATE,          //DONE

    OR_STATE,   //DONE
    AND_STATE,     //DONE

    INTERP_TEXT_STATE,       //stav pro cteni textu uvnitr interpolovaneho retezce
    INTERP_ESCAPE_STATE,     //stav pro escape sekvence uvnitr interpolace
    INTERP_DOLLAR_STATE,     //stav kdyz narazime na dolar v interpolaci
    INTERP_ID_STATE          //stav pro nacitani promenne po dolaru
} State;

//prompt: potrebuju v c u scanneru udrzovat stav mezi volanimi funkce (zasobnik na znaky back_stack, top, current_line a dstring pro retizky). nevim presne jak to nejlip ulozit, myslis ze static promenny takhle na zacatku souboru jsou v pohode? a bude to spravne navazovat na moje vlastni ungetc a fgetc? zkontroluj mi jestli tenhle zapis dává smysl 
//zasobnik pro vraceni znaku zpatky, nekdy ungetc to nezvladne
static int back_stack[16];
//aktualni pocet znaku v zasobniku
static int top = 0;
//aktualni radek 
static int current_line = 1;

//string pro ukladani prectenych znaku 
static DString str;
//hlida jestli je string inicializovany
static bool ds_initialized = false;

//prompt: kolega me na Discordu upozornil, ze moje reseni interpolovanych retezcu je spatne. scanner vraci jeden INTERPOLATED_STRING_TOKEN se surovym obsahem, takze by parser musel $id a ${vyraz} znovu skenovat sam a resit $$. navic to nefunguje kdyz je uvnitr ${} dalsi retezec, napr. s"${f("x")}" - retezec skonci na prvni vnitrni uvozovce. jak mam scanner upravit, aby s"..." rozsekal na tokeny podle jeho zadani: INTERP_START (text od s" po prvni $ nebo konec), INTERP_MID (text mezi dvema interpolacemi, jen kdyz neni prazdny), INTERP_END (posledni textova cast), LBRACE pro ${ a RBRACE pro k ni sparovanou }? $id je normalni IDENTIFIER, $$ je jeden dolar, $ pred cislici nebo klicovym slovem je lexikalni chyba 1, LF v jednoradkovem s"..." (i uvnitr ${}) je chyba 1 a existuje i viceradkova varianta s"""..."""
//prompt: muj prvni navrh (INTERP_BEGIN / INTERP_PART / INTERP_EXPR_BEGIN / INTERP_EXPR_END) nesedi s tim co kolega potrebuje, protoze prvni textova cast ma byt v atributu INTERP_START a posledni v INTERP_END. navic `$klicove_slovo` nehlasil chybu a LF uvnitr ${} v jednoradkovem retezci proslo. prepis to tak, aby to byly normalni stavy v jednom switchi a nebyl potreba flag end_pending
//zasobnik pro interpolovane retezce, protoze s"..." se muze objevit i uvnitr ${ }; pamatuje si jestli je retezec viceradkovy s"""..."""
static bool interp_multi[10];
//vrchol zasobniku pro interpolaci
static int interp_top = 0;
//kontrola jestli jsme uvnitr vyrazu v interpolaci
static bool interp_expr = false;

//funkce na uvolneni stringu 
static void scanner_cleanup(void) {
    ds_free(&str);
}

//funkce pro kompletni uvolneni scanneru vcetne interpolace
void scanner_free(void) {
    if (ds_initialized) {
        ds_free(&str);
        ds_initialized = false;
    }
    interp_top = 0;
    interp_expr = false;
}

//prompt: potrebuju vyresit cteni a vraceni znaku pri skenovani tokenu. kdyz nactu znak co uz do aktualniho tokenu nepatri nebo musim vratit vic znaku zpatky, protoze mi obyc ungetc nejak nejde a dle tveho pred tim doporuceni bych to musela napsat sama. jak mam presne napsat vlastni funkce fgetc a ungetc s vyuzitim myho back_stacku, aby to nejdriv bralo znaky ze zasobniku a zaroven hlídalo zvysovani/snizovani radku?
//funkce ktera je pri pouziti dalsi promenne nebo znaku
static int in_fgetc(FILE *f) {
    int c;
    //pokud mame neco v zasobniku vezmeme odtud
    if (top > 0) {
        c = back_stack[--top];
    }
    //jinak ze cteme ze souboru
    else {
        c = fgetc(f);
    }
    //pokud je novy radek
    if (c == '\n') {
        current_line++;
    }
    return c;
}

//funkce, ktera se pro pouziti abychom mohli se vratit k puvodni casti 
static void in_ungetc (FILE *f, int c) {
    //EOF se neda vratit 
    if (c == EOF) {
        return;
    }
    //pokud mame enter tak musime snizit
    if (c == '\n') {
        current_line--;
    }
    //ukladane do naseho pole
    if (top < 16) {
        back_stack[top++] = c;
    }
    else {
        ungetc(c,f);
    }
}

/*prompt: kdyz lexikalni chyba nastane primo na znaku konce radku (nedokonceny retezec, `1_` na konci radku, `1e` na konci radku...), hlasi se radek o 1 vetsi, 
protoze in_fgetc uz current_line zvysil. oprav hlaseni cisla radku*/
//funkce pro opravu cisla radku pri chybe na konci radku
static int err_line(int c) {
    if (c == '\n') {
        return current_line - 1;
    }
    return current_line;
}

//funkce pro kontrolu, jestli je znak platny pro identifikator
static bool is_id_char(int c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
        return true;
    }
    return false;
}

//prompt: jak mam jenomocne napsat funkci, ktera zkontroluje jestli je dane slovo klicove slovo jazyka a vrati true/false? 
//funkce pro kontrolu klicovych slov
static bool is_keyword(const char *word) {
    if (strcmp(word, "if") == 0 || strcmp(word, "for") == 0 || strcmp(word, "while") == 0 ||
        strcmp(word, "do") == 0 || strcmp(word, "else") == 0 || strcmp(word, "val") == 0 ||
        strcmp(word, "var") == 0 || strcmp(word, "def") == 0 || strcmp(word, "import") == 0 ||
        strcmp(word, "return") == 0 || strcmp(word, "then") == 0 || strcmp(word, "Int") == 0 ||
        strcmp(word, "String") == 0 || strcmp(word, "Double") == 0 || strcmp(word, "Boolean") == 0 ||
        strcmp(word, "Unit") == 0 || strcmp(word, "true") == 0 || strcmp(word, "false") == 0 ||
        strcmp(word, "case") == 0 || strcmp(word, "class") == 0 || strcmp(word, "match") == 0 ||
        strcmp(word, "type") == 0 || strcmp(word, "extension") == 0 || strcmp(word, "List") == 0) {
        return true;
    }
    return false;
}

//hlavni funkce pro cteni tokenu
static Token scan_raw(FILE *f) {
    //pokud nastane problem pri uklizeni a rozsireni buf, tak to hodi cleanup
    if (!ds_initialized) {
        ds_init(&str);
        error_cleanup(scanner_cleanup);
        ds_initialized = true;
    }
    //vzdy uklzime pred novym string
    ds_clear(&str);

    //zacatek state 
    State state = START_STATE;
    //kontrola jestli je viceradkovy
    bool multi_line = false;
    //kontrola jestli to je prvni cast interpolace
    bool first_part = false;
    //pokud jsme v interpolaci ale ne ve vyrazu, tak jdeme cist text
    if (interp_top > 0 && !interp_expr) {
        state = INTERP_TEXT_STATE;
        multi_line = interp_multi[interp_top - 1];
    }
    //promena aktualni promeny 
    int c;
    //struktura ktera se nenacita 0
    Token token = {0};
    token.atribute = NULL;
    //kontrola jestli se nasel token
    bool token_found = false;

    //pocita jestli mame zarovnany uroven pro blokove komentare
    int nesting_level = 0;
    //kontrola jestli je tam uvnitr blokovy kometar 
    bool comm_newline = false;

    while(!token_found) {
        c = in_fgetc(f);
        switch (state) {
            /*START_STATE = hlavni stav konecneho automatu, kde se urcuje, do jakych stavu budou smerovat nebo zdali uz jsou nejake tokeny
            - urcuji se mezery, konec radku nebo souboru, cisla, slova, nebo zakladni znaky jako je napr. *, - , +*/
            case START_STATE:
                //pokud je to mezera
                if (c == ' ' || c == '\t') {
                    break;
                }
                else if (c == '\r') {
                    break;
                }
                //kontrola na spatny konec radku v interpolaci
                else if (c == '\n' && interp_top > 0 && interp_expr && !interp_multi[interp_top - 1]) {
                    error_exit(ERROR_LEX, err_line(c), "Newline inside ${...} of single-line interpolated string");
                }
                //pokud je to konec radku, tak je to EOL_TOKEN
                else if (c == '\n') {
                    token.type = EOL_TOKEN;
                    token_found = true;
                    break;
                }
                //pokud nahle skonci soubor a jsme v interpolaci
                else if (c == EOF && interp_top > 0) {
                    error_exit(ERROR_LEX, current_line, "Unterminated interpolated string literal");
                }
                //pokud je to konec souboru, tak je to EOF_TOKEN
                else if (c == EOF) {
                    token.type = EOF_TOKEN;
                    token_found = true;
                    break;
                }
                //pokud je to cislo, tak jdeme do stavu INT_STATE
                else if (c >= '1' && c <= '9') {
                    state = INT_STATE;
                    ds_push_char(&str, c);
                }
                //pokud je to nula, tak jdeme do stave ZERO_STATE
                else if (c == '0') {
                    state = ZERO_STATE;
                    ds_push_char(&str, c);
                }
                //pokud je to pismeno, tak jde do stavu WORD_STATE
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    if (c == 's') {
                        int next = in_fgetc(f);
                        if (next == '"') {
                            int q2 = in_fgetc(f);
                            if (q2 == '"') {
                                int q3 = in_fgetc(f);
                                if (q3 == '"') {
                                    multi_line = true;
                                }
                                else {
                                    in_ungetc(f, q3);
                                    in_ungetc(f, q2);
                                }
                            }
                            else {
                                in_ungetc(f, q2);
                            }
                            if (interp_top >= 10) {
                                error_exit(ERROR_INTERNAL, current_line, "Interpolation nesting too deep");
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
                }
                else {
                    //zde se urcuje zakladni znaky a jeji rozdeleni do dalsich stavu
                    switch(c) {
                        //pokud je to znak * (nasobeni), tak je to MUL_TOKEN
                        case '*':
                            token.type = MUL_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to znak + (plus), tak je to PLUS_TOKEN nebo PLUSPLUS_TOKEN
                        case '+': {
                            int next = in_fgetc(f);
                            if (next == '+') {
                                token.type = PLUSPLUS_TOKEN;
                                token_found = true;
                            }
                            else {
                                in_ungetc(f, next);
                                token.type = PLUS_TOKEN;
                                token_found = true;
                            }
                            break;
                        }
                        //pokud je to - (minus), tak je to MINUS_TOKEN
                        case '-':
                            token.type = MINUS_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to ), tak je to RPAR_TOKEN
                        case ')':
                            token.type = RPAR_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to (, tak je to LPAR_TOKEN
                        case '(':
                            token.type = LPAR_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to [, tak je to LBRACKET_TOKEN
                        case '[':
                            token.type = LBRACKET_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to ], tak je to RBRACKET_TOKEN
                        case ']':
                            token.type = RBRACKET_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to , (carka), tak je to COMMA_TOKEN
                        case ',':
                            token.type = COMMA_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to . (tecka), tak je to bud po kontrole jestli je to pred cislem tak jde do stavu DOUBLE_STATE, jinak do DOT_TOKEN
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
                        //pokud je to :, tak je to COLON_TOKEN
                        case ':':
                            token.type = COLON_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to ;, tak je to SEMICOLON_TOKEN
                        case ';':
                            token.type = SEMICOLON_TOKEN;
                            token_found = true;
                            break;
                        //reseni pro @
                        case '@': {
                            char next_chars[5] = {0};
                            //pokracujeme podle pismen, protoze po znaku @ musi byt vzdy main (4 pismena)
                            for (int i = 0; i < 4; i++) {
                                next_chars[i] = in_fgetc(f);
                            }
                            //pokud je to main, tak zkontrolujeme dalsi pismena a je to AT_TOKEN
                            if (strcmp(next_chars, "main") == 0) {
                                int after = in_fgetc(f);
                                in_ungetc(f, after);
                                if (is_id_char(after)) {
                                    error_exit(ERROR_LEX, err_line(c), "Invalid annotation, expected @main");
                                }
                                for (int i = 3; i >= 0; i--) {
                                    in_ungetc(f, next_chars[i]);
                                }
                                token.type = AT_TOKEN;
                                token_found = true;
                            }
                            //jinak nic neni s @, takze je to lexikalni chyba 1
                            else {
                                int newlines = 0;
                                for (int i = 0; i < 4; i++) {
                                    if (next_chars[i] == '\n') {
                                        newlines++;
                                    }
                                }
                                error_exit(ERROR_LEX, current_line - newlines, "Invalid character @");
                            }
                            break;
                        }
                        //pokud je to }, tak musime zkontrolovat jestli to neni konec vyrazu v interpolaci
                        case '}':
                            if (interp_top > 0 && interp_expr) {
                                interp_expr = false;
                                token.type = RBRACE_TOKEN;
                                token_found = true;
                            }
                            else {
                                error_exit(ERROR_LEX, err_line(c), "Unexpected character");
                            }
                            break;
                        //pokud je to _, tak jde to do stavu WORD_STATE (protoze muze to byt string_word nebo cislo, ale resi se to potom co to neni slovo)
                        case '_':
                            ds_push_char(&str, '_');
                            state = WORD_STATE;
                            break;
                        //pokud je to |, tak jde to do OR_STATE
                        case '|':
                            state = OR_STATE;
                            break;
                        //pokud je to &, tak jde to do AND_STATE
                        case '&':
                            state = AND_STATE;
                            break;
                        //pokud je to /, tak to jde do SLASH_STATE
                        case '/':
                            state = SLASH_STATE;
                            break;
                        //pokud je to ", tak jde to do STRING_STATE (protozto muze byt slovo)
                        case '"':
                            state = STRING_STATE;
                            break;
                        //pokud je to =, tak to jde do ASSIGN_STATE (protoze muze to byt vetsi nebo rovnase atd)
                        case '=':
                            state = ASSIGN_STATE;
                            break;
                        //pokud je to !, tak to jde do EXCLAM_STATE (jestli to neni !=)
                        case '!':
                            state = EXCLAM_STATE;
                            break;
                        //pokud je to <, tak jde do LESS_STATE
                        case '<':
                            state = LESS_STATE;
                            break;
                        //pokud je to >, tak to jde do GREAT_STATE
                        case '>':
                            state = GREAT_STATE;
                            break;
                        //jinak je to defaultni chyba neboli lexikalni chyba 1
                        default:
                            error_exit(ERROR_LEX, err_line(c), "Unexpected character");
                    }
                }
                break;

            //WORD_STATE = stav, ve kterem se urcuje zda-li existuje nejaky prikaz / klicove slovo / identifikator
            case WORD_STATE:
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                    ds_push_char(&str, c);
                }
                else {
                    in_ungetc(f, c);
                    //urcuji se jake je to slovo a priradi se k tokenu 
                    if(strcmp(str.data, "if") == 0) {
                        token.type = IF_TOKEN;
                    }
                    else if (strcmp(str.data, "for") == 0) {
                        token.type = FOR_TOKEN;
                    }
                    else if (strcmp(str.data, "while") == 0) {
                        token.type = WHILE_TOKEN;
                    }
                    else if (strcmp(str.data, "do") == 0) {
                        token.type = DO_TOKEN;
                    }
                    else if (strcmp(str.data, "else") == 0) {
                        token.type = ELSE_TOKEN;
                    }
                    else if (strcmp(str.data, "val") == 0) {
                        token.type = VAL_TOKEN;
                    }
                    else if (strcmp(str.data, "var") == 0) {
                        token.type = VAR_TOKEN;
                    }
                    else if (strcmp(str.data, "def") == 0) {
                        token.type = DEF_TOKEN;
                    }
                    else if (strcmp(str.data, "import") == 0) {
                        token.type = IMPORT_TOKEN;
                    }
                    else if (strcmp(str.data, "return") == 0) {
                        token.type = RETURN_TOKEN;
                    }
                    else if (strcmp(str.data, "then") == 0) {
                        token.type = THEN_TOKEN;
                    }
                    else if (strcmp(str.data, "Int") == 0) {
                        token.type = INT_TOKEN;
                    }
                    else if (strcmp(str.data, "String") == 0) {
                        token.type = STRING_TOKEN;
                    }
                    else if (strcmp(str.data, "Double") == 0) {
                        token.type = DOUBLE_TOKEN;
                    }
                    else if (strcmp(str.data, "Boolean") == 0) {
                        token.type = BOOLEAN_TOKEN;
                    }
                    else if (strcmp(str.data, "Unit") == 0) {
                        token.type = UNIT_TOKEN;
                    }
                    else if (strcmp(str.data, "true") == 0) {
                        token.type = TRUE_TOKEN;
                    }
                    else if (strcmp(str.data, "false") == 0) {
                        token.type = FALSE_TOKEN;
                    }
                    else if (strcmp(str.data, "case") == 0) {
                        token.type = CASE_TOKEN;
                    }
                    else if (strcmp(str.data, "class") == 0) {
                        token.type = CLASS_TOKEN;
                    }
                    else if (strcmp(str.data, "match") == 0) {
                        token.type = MATCH_TOKEN;
                    }
                    else if (strcmp(str.data, "type") == 0) {
                        token.type = TYPE_TOKEN;
                    }
                    else if (strcmp(str.data, "extension") == 0) {
                        token.type = EXTENSION_TOKEN;
                    }
                    else if (strcmp(str.data, "List") == 0) {
                        token.type = LIST_TOKEN;
                    }
                    else if(strcmp(str.data, "_") == 0) {
                        token.type = UNDERSCORE_TOKEN;
                    }
                    //jinak je to nejaka promena neboli nazev funkce
                    else {
                        token.type = IDENTIFIER_TOKEN;
                        token.atribute = ds_dup(&str);
                    }
                    token_found = true;
                    break;
                }
                break;

            //SLASH_STATE = stav, ktery urcuje zdali muzeme pocitat lomitko jako deleni nebo jako zacatek komentare
            case SLASH_STATE:
                //pokud je to slash / tak je to komentar s // a vede to do LINE_COMMENT_STATE
                if (c == '/') {
                    state = LINE_COMMENT_STATE;
                }
                //pokud je to slash s *, tak je to /* a vede do BLOCK_COMM_STATE
                else if (c == '*') {
                    state = BLOCK_COMM_STATE;
                    nesting_level = 1;
                    comm_newline = false;
                }
                //jinak je to deleni
                else {
                    in_ungetc(f, c);
                    token.type = DIV_TOKEN;
                    token_found = true;
                    break;
                }
                break;

            //BLOCK_COMM_STATE = stav, ktery urcuje zdali tam je hvezdicka ktera bude ukoncovat komentar 
            case BLOCK_COMM_STATE:
                //pokud je to konec souboru tak je to lexikalni chyba
                if(c == EOF) {
                    error_exit(ERROR_LEX, err_line(c), "Unterminated block comment");
                }
                //pokud je to prazdny radek tak zacian novy radek
                else if (c == '\n') {
                    comm_newline = true;
                }
                //pokud to zacina znova slash jinak je to definitivne blockcomment
                else if (c == '/') {
                    int next = in_fgetc(f);
                    if (next == '*') {
                        nesting_level++;
                    }
                    else {
                        in_ungetc(f, next);
                    }
                }
                //pokud je to zase slash ale nepokracuje tak je to konec radku a nebo jde to do START_STATE
                else if (c == '*') {
                    int next = in_fgetc(f);
                    if (next == '/') {
                        nesting_level--;
                        if (nesting_level == 0) {
                            if (comm_newline) {
                                int check;
                                while ((check = in_fgetc(f)) == ' ' || check == '\t');
                                if (check != '\n' && check != '\r' && check != EOF) {
                                    error_exit(ERROR_LEX, err_line(c), "Disallowed characters after multi-line block comment");
                                }
                                in_ungetc(f, check);
                                token.type = EOL_TOKEN;
                                token_found = true;
                                break;
                            }
                            else {
                                state = START_STATE;
                            }
                        }
                    }
                    else {
                        in_ungetc(f, next);
                    }
                }
                break;

            //LINE_COMMENT_STATE = stav, ktery urcuje zdali se ukoncil komentare, jinak prejde na start state
            case LINE_COMMENT_STATE:
                if (c == '\n' || c == EOF) {
                    in_ungetc(f, c);
                    state = START_STATE;
                }
                break;

            //INT_STATE = stav, ktery urcuje cisla a popripade jestli to je int, double nebo obsahuje nejaky znaky 
            case INT_STATE:
                //pokud je to cislo tak to pushuje jako vysledek
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                }
                //pokud je to s teckou tak jde do DOUBLE_STATE
                else if (c == '.') {
                    int next = in_fgetc(f);
                    if (next >= '0' && next <= '9') {
                        ds_push_char(&str, '.');
                        ds_push_char(&str, next);
                        state = DOUBLE_STATE;
                    }
                    //jinak je to INT_NUM_TOKEN, takze je to cislo 
                    else {
                        in_ungetc(f, next);
                        in_ungetc(f, '.');
                        token.type = INT_NUM_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                }
                //pokud je to s _ tak je to INT_UNDERSCORE_STATE napr. 1_000
                else if (c == '_') {
                    state = INT_UNDERSCORE_STATE;
                }
                //pokud je to s e nebo E, tak jde do EXP_START_STATE
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
                //pokud se tam objevi psmeno tak je to lexikalni chyba 1
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, err_line(c), "Unexpected character in integer literal");
                }
                //jinak je to cislo a patri do tokenu INT_NUM_TOKEN
                else {
                    in_ungetc(f, c);
                    token.type = INT_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            //INT_UNDERSCORE_STATE = stav ve kterem se resi jestli je po _ cislo a jestli se da zapsat
            //pokud po _ pokracuje cislo tak je to spravny zapis 1_000, pokud by tam bylo neco jineho tak je to lexikani chyba
            case INT_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = INT_STATE;
                }
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid number format with underscore");
                }
                break;

            //DOUBLE_STATE =je to stav kde se resi jestli je to double cislo a jake za tim muzou byt dalsi podminky 
            case DOUBLE_STATE:
                //pokud je to double, tak zkontrolujeme jestli je to cislo tak je to zase DOUBLE_STATE 
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = DOUBLE_STATE;
                }
                //pokud je to s _tak je to uz ne int, ale DOUBLE_UNDERSCORE_STATE
                else if (c == '_') {
                    state = DOUBLE_UNDERSCORE_STATE;
                }
                //jestli to obsahuje E a nebo e tak jde do EXP_START_STATE
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
                //a pokud obsahuje D a nebo d tak se to zapise, ale pokud tam bude za tim cislo nebo pismeno, tak je to lexikalni chyba
                else if (c == 'd' || c == 'D') {
                    int next = in_fgetc(f);
                    if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9') || next == '_') {
                        error_exit(ERROR_LEX, err_line(c), "Invalid double suffix");
                    }
                    in_ungetc(f, next);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                //to same ale pokud se tam vubec objevi nejake pismeno
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, err_line(c), "Unexpected character in double literal");
                }
                //jinak je to DOUBLE_NUM_TOKEN (double cislo)
                else {
                    in_ungetc(f, c);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            //DOULE_UNDERSCORE_STATE = je to stav ktery resi jestli je tam po _ cislo 
            case DOUBLE_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = DOUBLE_STATE;
                }
                //vyhodi chybu pokud tam je neco jineho
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid double format with underscore");
                }
                break;

            //EXP_START_STATE = stav ktery ma nejaky exp pred cislem tak se urcuje do jakeho stavu patri
            case EXP_START_STATE:
                //pokud je + nebo - tak pujdeme do stavu EXP_SIGN_STATE
                if (c == '+' || c == '-') {
                    ds_push_char(&str, c);
                    state = EXP_SIGN_STATE;
                }
                //pokud je to cislo EXP_DIGITS_STATE
                else if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                //jinak pokud zadny exp neni tak je to lexikalni chyba 1
                else {
                    error_exit(ERROR_LEX, err_line(c), "Exponent expected");
                }
                break;

            //EXP_SIGN_STATE = je to stav ve kterym se resi jestli po operandu se objevuje cislo a jde do EXP_DIGITS_STATE,jinak je to lexikalni chyba
            case EXP_SIGN_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, err_line(c), "Digit expected in exponent");
                }
                break;

            //EXP_DIGITS_STATE = stav ktery resi po operandu cisla a popripade jestli se tam muzou objevit dalsi znaky 
            case EXP_DIGITS_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                }
                //pokud je _tak se jde do EXP_UNEDERSCORE_STATE
                else if (c == '_') {
                    state = EXP_UNDERSCORE_STATE;
                }
                //pokud je tam d, tak je to DOUBLE_NUM_TOKEN, nesmi taky byt po pismena nebo dalsi cisla, jinak lexikalni chyba 1
                else if (c == 'd' || c == 'D') {
                    int next = in_fgetc(f);
                    if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9') || next == '_') {
                        error_exit(ERROR_LEX, err_line(c), "Invalid exponent suffix");
                    }
                    in_ungetc(f, next);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                //pokud je nejak epismeno tak je to lexikalni chyba 1
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, err_line(c), "Unexpected character after exponent");
                }
                //jinak je to cislo a token DOUBLE_NUM_TOKEN
                else {
                    in_ungetc(f, c);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            //EXP_UNDERSCORE_STATE = je to same jako DOUBLE_UNDERSCORE_STATE
            case EXP_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid exponent format with underscore");
                }
                break;

            //ZERO_STATE =, satv ktery resi problematiku s nulou (kvuli 01.0 nebo 000_000)
            case ZERO_STATE:
                //pokud je tam tecka tak se daa zapsat do 0. neco a potom to muze jit do DOUBLE_STATE
                if (c == '.') {
                    int next = in_fgetc(f);
                    if (next >= '0' && next <= '9'){
                        ds_push_char(&str, c);
                        ds_push_char(&str, next);
                        state = DOUBLE_STATE;
                    }
                    //jinak je to INT_NUM_TOKEN protoze je to treba 10
                    else {
                        in_ungetc(f, next);
                        in_ungetc(f, '.');
                        token.type = INT_NUM_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                }
                //pokud po 0 je e tak zase do EXP_START_STATE
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
                //pokud je po 0 cislo nebo pismeno tak nemuze zapsat a je to lexikalni chyba
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                    error_exit(ERROR_LEX, err_line(c), "Zero literal with invalid suffix");
                }
                //jinak je to INT_NUM_TOKEN neboli cislo 
                else {
                    in_ungetc(f, c);
                    token.type = INT_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            //ASSIGN_STATE = urcuje dlasi podminky ktere obsahuji =
            case ASSIGN_STATE:
                //je to EQ_TOKEN ==
                if (c == '=') {
                    token.type = EQ_TOKEN;
                    token_found = true;
                    break;
                }
                //je to ARROW_FAT_TOKEN (=>)
                if (c == '>') {
                    token.type = ARROW_FAT_TOKEN;
                    token_found = true;
                    break;
                }
                //jinak je to =
                else {
                    in_ungetc(f, c);
                    token.type = ASSIGN_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            //LESS_STATE = stav ktery resi < 
            case LESS_STATE:
                //pokud je to <=
                if (c == '=') {
                    token.type = LESS_EQ_TOKEN;
                    token_found = true;
                    break;
                }
                //pokud je to <-
                else if (c == '-') {
                    token.type = ARROW_LEFT_TOKEN;
                    token_found = true;
                    break;
                }
                //jinak je to <
                else {
                    in_ungetc(f, c);
                    token.type = LESS_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            //GREAT_STATE =resi se s >
            case GREAT_STATE:
                //je to >=
                if (c == '=') {
                    token.type = GREAT_EQ_TOKEN;
                    token_found = true;
                    break;
                }
                //jinak je to >
                else {
                    in_ungetc(f, c);
                    token.type = GREAT_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            //EXCLAM_STATE = resi se negace
            case EXCLAM_STATE:
                //pokud je to !=
                if (c == '=') {
                    token.type = NEQ_TOKEN;
                    token_found = true;
                    break;
                }
                //jinak je to NOT_TOKEN
                else {
                    in_ungetc(f, c);
                    token.type = NOT_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            //OR_STATE = stav ktery resi znak |
            case OR_STATE:
                //je to OR_TOKEN ||
                if (c == '|') {
                    token.type = OR_TOKEN;
                    token_found = true;
                    break;
                }
                //jenom PIPE_TOKEN |
                else {
                    in_ungetc(f, c);
                    token.type = PIPE_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            //AND_STATE = stav ktery resi & znak 
            case AND_STATE:
                if (c == '&') {
                    token.type = AND_TOKEN;
                    token_found = true;
                    break;
                }
                //pokud to neni && je to lexikalni chyba 1
                else {
                    error_exit(ERROR_LEX, err_line(c), "Expected '&' character for AND operator");
                }
                break;

            //STRING_STATE = stav ktery resi vsechny pismena ktere nejsou zadne nejake klicove slovo (if, else , for atd)
            case STRING_STATE:
                //pokud tak je "", tak je nejaky slovo nebo veta ktere se vypisuje 
                if (c == '"') {
                    //pokud tam neni slovo tak kontrolujeme dalsi ""
                    if (str.len == 0) {
                        int next_c = in_fgetc(f);
                        //jinak je to promena MULTILINE_STRING_STATE
                        if (next_c == '"') {
                            state = MULTILINE_STRING_STATE;
                        }
                        //pokud je tam neco tak je to STRING_WORD_TOKEN (slovo kter se bude vypisovat)
                        else {
                            in_ungetc(f, next_c);
                            token.type = STRING_WORD_TOKEN;
                            token.atribute = ds_dup(&str);
                            token_found = true;
                        }
                    }
                    //to same pokud tu nejsou "", tak je to promena nebo nazev funkce
                    else {
                        token.type = STRING_WORD_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                    }
                    break;
                }
                //pokud jsou tam \\ tak je konec radku neboli take konec slova
                else if (c == '\\') {
                    state = STRING_ESCAPE_STATE;
                }
                //pokud je tam konec souboru je to lexikalni chyba 1
                else if (c == '\n' || c == '\r' || c == EOF) {
                    error_exit(ERROR_LEX, err_line(c), "Unterminated string literal");
                }
                //jiank se to tam vyoise ze je to nejake slovo
                else {
                    ds_push_char(&str, c);
                }
                break;
            //STRING_ESCAPE_STATE = stav ktery resi prazdne mista 
            case STRING_ESCAPE_STATE:
                if(c == '"') {
                    ds_push_char(&str, '"');
                }
                else if (c == 'n') {
                    ds_push_char(&str, '\n');
                }
                else if (c == 'r') {
                    ds_push_char(&str, '\r');
                }
                else if (c == 't') {
                    ds_push_char(&str, '\t');
                }
                else if (c == '\\') {
                    ds_push_char(&str, '\\');
                }
                //pokud neco z toho se neobjevi tak je to exikalni chyba protoze se radek musi nejak ukoncit lol
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid escape sequence");
                }
                //jinat je to stale nejake slovo ktere se musi nacist 
                state = STRING_STATE;
                break;
            //MULTILINE_STRING_STATE = stav ktery resi jake slova se budou vipysovat v ""
            case MULTILINE_STRING_STATE:
                //pokud je to EOF, je to lexikalni chyba 
                if(c == EOF) {
                    error_exit(ERROR_LEX, err_line(c), "Unterminated multi-line string literal");
                }
                //jinak jestli tam jsou "", tak se to precte jako nejaky vety ci slova
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
                    //to same ze se pushne porotoze tu taky to znamena ze je to nazev neceho 
                    for (int i = 0; i < quotes; i++) {
                        ds_push_char(&str, '"');
                    }
                }
                else {
                    ds_push_char(&str, c);
                }
                break;

            //INTERP_TEXT_STATE = stav ktery resi cteni textu v interpolovanem retezci s"..."
            case INTERP_TEXT_STATE:
                //pokud je to EOF, je to lexikalni chyba
                if (c == EOF) {
                    error_exit(ERROR_LEX, current_line, "Unterminated interpolated string literal");
                }
                //pokud jsou to uvozovky tak to muze byt konec
                else if (c == '"') {
                    int quotes = 1;
                    //pokud tam je vice uvozovek tak se to musi resit jinak je to konec interpolace
                    if (multi_line) {
                        int q;
                        //opakuje se pokud tam jsou ty uvozovky 
                        while ((q = in_fgetc(f)) == '"') {
                            quotes++;
                        }
                        in_ungetc(f, q);
                    }
                    //pokud je to vice uvozovek tak se musi resit jinak je to konec interpolace
                    if (multi_line && quotes < 3) {
                        for (int i = 0; i < quotes; i++) {
                            ds_push_char(&str, '"');
                        }
                    }
                    //pokud ne tak se pokracuje dale a resi se jestli je to konec interpolace nebo ne
                    else {
                        int closing = 1;
                        //zase kontrola jestli je to vice uvozovek a resi se jestli je to konec interpolace nebo ne
                        if (multi_line) {
                            closing = 3;
                            //pokud je to vice uvozovek tak se musi resit jinak je to konec interpolace
                            for (int i = 0; i < quotes - 3; i++) {
                                ds_push_char(&str, '"');
                            }
                        }
                        //pokud je to prvni cast interpolace tak se musi vratit zpet a resit jestli to neni konec
                        if (first_part) {
                            for (int i = 0; i < closing; i++) {
                                in_ungetc(f, '"');
                            }
                            //jinak je to konec a token se musi zapsat jako start
                            token.type = INTERP_START_TOKEN;
                        }
                        //jinak je to konec a token se musi zapsat jako end
                        else {
                            interp_top--;
                            //kontrola jestki je to  konec jinak se zapise jako end token
                            if (interp_top > 0) {
                                interp_expr = true;
                            }
                            token.type = INTERP_END_TOKEN;
                        }
                        //jinak se musi zapsat atribut a token se musi zapsat jako nejaky token
                        token.atribute = ds_dup(&str);
                        token_found = true;
                    }
                }
                //pokud narazime na dolar tak jdeme resit promennou
                else if (c == '$') {
                    state = INTERP_DOLLAR_STATE;
                }
                //pokud narazime na escape sekvenci tak jdeme resit escape sekvence
                else if (c == '\\' && !multi_line) {
                    state = INTERP_ESCAPE_STATE;
                }
                //pokud narazime na konec radku tak je to lexikalni chyba 1
                else if ((c == '\n' || c == '\r') && !multi_line) {
                    error_exit(ERROR_LEX, err_line(c), "Unterminated interpolated string literal");
                }
                //jinak se pushuje znak do retezce
                else {
                    ds_push_char(&str, c);
                }
                break;

            //INTERP_ESCAPE_STATE = stav ktery resi prazdne mista a escape sekvence uvnitr interpolace
            case INTERP_ESCAPE_STATE:
                if (c == '"') {
                    ds_push_char(&str, '"');
                }
                else if (c == 'n') {
                    ds_push_char(&str, '\n');
                }
                else if (c == 'r') {
                    ds_push_char(&str, '\r');
                }
                else if (c == 't') {
                    ds_push_char(&str, '\t');
                }
                else if (c == '\\') {
                    ds_push_char(&str, '\\');
                }
                //pokud nic nenalezne tak je to lexikalni chyba 1 protoze se musi nejak ukoncit radek
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid escape sequence");
                }
                state = INTERP_TEXT_STATE;
                break;

            //INTERP_DOLLAR_STATE = stav ktery se spusti kdyz najdeme $ v interpolaci
            case INTERP_DOLLAR_STATE:
                //pokud je tam dalsi dolar tak se vypise jen jeden
                if (c == '$') {
                    ds_push_char(&str, '$');
                    state = INTERP_TEXT_STATE;
                }
                //pokud je to zavorka nebo pismeno tak jdeme nacitat promennou
                else if (c == '{' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                    //pokud je to prvni cast interpolace tak se musi zapsat jako INTERP_START_TOKEN
                    if (first_part) {
                        in_ungetc(f, c);
                        in_ungetc(f, '$');
                        token.type = INTERP_START_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                    }
                    //pokud je to zavorka tak se musi zapsat jako INTERP_MID_TOKEN
                    else if (str.len > 0) {
                        in_ungetc(f, c);
                        in_ungetc(f, '$');
                        token.type = INTERP_MID_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                    }
                    //pokud je to zavorka tak se musi zapsat jako INTERP_MID_TOKEN
                    else if (c == '{') {
                        interp_expr = true;
                        token.type = LBRACE_TOKEN;
                        token_found = true;
                    }
                    //pokud je to pismeno tak se musi zapsat jako INTERP_ID_STATE
                    else {
                        ds_push_char(&str, c);
                        state = INTERP_ID_STATE;
                    }
                }
                //jinak je to lexikalni chyba 1 protoze se musi nejak ukoncit radek
                else {
                    error_exit(ERROR_LEX, err_line(c), "Invalid '$' in interpolated string");
                }
                break;

            //INTERP_ID_STATE = stav ktery nacita identifikator po $
            case INTERP_ID_STATE:
            //pokud je to pismeno nebo cislo tak se to pushuje do retezce
                if (is_id_char(c)) {
                    ds_push_char(&str, c);
                }
                //pokud je to konec identifikatoru tak se musi zapsat jako IDENTIFIER_TOKEN
                else {
                    in_ungetc(f, c);
                    //kontrola jestli je to _ nebo klicove slovo, pokud ano tak je to lexikalni chyba 1
                    if (strcmp(str.data, "_") == 0 || is_keyword(str.data)) {
                        error_exit(ERROR_LEX, err_line(c), "Invalid identifier after '$' in interpolated string");
                    }
                    token.type = IDENTIFIER_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                }
                break;
            //pokud je to nic z toho tak je to interni chyba 99
            default:
                error_exit(ERROR_INTERNAL, current_line, "Unknown state in fsm");
        }
    }
    //je to konec radku, tak ukonci radek 
    if (token.type == EOL_TOKEN) {
        token.line = current_line - 1;
    }
    //jinak je to pokracuje v radku 
    else {
        token.line = current_line;
    }
    return token;
}

//prompt: mam problem v count_indent s blokovyma komentarama /* */ na zacatku radku. vyhazuje mi to spatne indent a dedent tokeny protoze presakuje jenom // komentare. jak mam upravit count_indent aby nacital cele blokove komentare vcetne vnorenych, vynuloval mezery pokud je komentar na samostatnem radku a hodil lexikalni chybu 1 pokud je za viceradkovym komentarem jeste kod na stejnem radku?
//funkce ktera resi INDENT A DEDENT (proste mezery)
static int count_indent(FILE *f) {
    //promena ve ktery pracujeme s mezery
    int spaces = 0;
    //kontrola jestli tab
    bool has_tab = false;
    //aktualni promena
    int c;

    //prracujeme dokud tam neni konec souboru
    while ((c = in_fgetc(f)) != EOF) {
        //kontrola vsech mezer 
        if (c == ' ') {
            spaces++;
        }
        //kontrola vsech tabuatoru (tab)
        else if (c == '\t') {
            has_tab = true;
        }
        //kontrola konce radku
        else if (c == '\n') {
            spaces = 0;
            has_tab = false;
        }
        //je to komentar
        else if (c == '\r') {
        }
        else if (c == '/') {
            int next = in_fgetc(f);
            //pokud je to //, preskocime vsechno 
            if (next == '/') {
                while ((c = in_fgetc(f)) != '\n' && c != EOF);
                if (c == EOF) {
                    return 0;
                }
                //po komentari pocitame vsechno od znova
                spaces = 0;
                has_tab = false;
            }
            //pokud je to /*, tak preskocime blokovy komentar i s vnorenim
            else if (next == '*') {
                int nesting_level = 1;
                bool comm_newline = false;

                //nacitame cely blokovy komentar
                while (nesting_level > 0) {
                    int bc = in_fgetc(f);
                    if (bc == EOF) {
                        error_exit(ERROR_LEX, current_line, "Unterminated block comment");
                    }
                    if (bc == '\n') {
                        comm_newline = true;
                    }
                    else if (bc == '/') {
                        int bc_next = in_fgetc(f);
                        if (bc_next == '*') {
                            nesting_level++;
                        } else {
                            in_ungetc(f, bc_next);
                        }
                    }
                    else if (bc == '*') {
                        int bc_next = in_fgetc(f);
                        if (bc_next == '/') {
                            nesting_level--;
                        } else {
                            in_ungetc(f, bc_next);
                        }
                    }
                }

                //kontrola co je za blokovym komentarem
                int after;
                while ((after = in_fgetc(f)) == ' ' || after == '\t');
                //pokud je tam konec radku nebo souboru tak je to prazdny radek
                if (after == '\n' || after == '\r' || after == EOF) {
                    if (after == EOF) {
                        return 0;
                    }
                    spaces = 0;
                    has_tab = false;
                }
                //jinak je tam jeste nejaky kod
                else {
                    //pokud mel komentar novy radek a za nim je kod tak je to lexikalni chyba 1
                    if (comm_newline) {
                        error_exit(ERROR_LEX, current_line, "Disallowed characters after multi-line block comment");
                    }
                    //jinak vratime znak a zkontrolujeme tab
                    in_ungetc(f, after);
                    if (has_tab) {
                        error_exit(ERROR_LEX, current_line, "Tabulator on a non-empty line");
                    }
                    break;
                }
            }
            //jinak se vratime pred / 
            else {
                in_ungetc(f, next);
                in_ungetc(f, '/');
                //a pokud tam je tab tak je to lexikalni chyba 1
                if (has_tab) {
                    error_exit(ERROR_LEX, current_line, "Tabulator on a non-empty line");
                }
                break;
            }
        }
        //pokud zase nic neni z toho tak se vratime
        else {
            in_ungetc(f, c);
            //pokud je ta, tab, tak je to zase lexikalni chyba 1
            if (has_tab) {
                error_exit(ERROR_LEX, current_line, "Tabulator on a non-empty line");
            }
            break;
        }
    }
    //narazime na konec souboru tak return 0
    if (c == EOF) {
        return 0;
    }
    //jinak pocet mezer vratime
    return spaces;
}

//prompt:chci mit fsm a indent, dedent v jedne funkci get_token, proc je lepsi rozdelit tak, ze scan_raw bude jenom ciste skenovat tokeny z fsm, a nova funkce get_token bude hlavni fce, ktery vola count_indent a potom dalsi funkci? ukazmi tona nejakem prikladu protoze nerozumim jak mam to vyresti at tam nejsou zadne zadrhely 
//hlavni funkce ve ktere ziskavame tokeny a zjistujeme co to je 
Token get_token(FILE *f) {
    //zasonik pro mezery, pamatuje si vsechny
    static int indent_stack[100] = {0};
    //vrchol zasobniku 
    static int stack = 0;
    //cte vsechny detend
    static int detents_reading = 0;
    //cte jestli je to zacatek radku
    static bool line_start = true;
    //pocita jeslti se tam zanedbaly zavorky
    static int parents = 0;
    //kontrola jestli je to konec radku 
    static bool last_eol = true;
    static bool newline_skip = false;

    //zaciname tokenem
    Token token = {0};

    //pokud je tam je dedent, tak je posleme 
    if (detents_reading > 0) {
        detents_reading--;
        token.type = DEDENT_TOKEN;
        token.line = current_line;
        return token;
    }
    //pokud zacina radek a zaroven jsou zanedbane zavorky (jsme v nejakych)
    if (line_start && parents == 0) {
        line_start = false;
        //spocitame mezery 
        int spaces = count_indent(f);

        //pokud je mezer vc nez bylo v minule urovni udelame indent
        if (spaces > indent_stack[stack]) {
            //ochrana proti preteceni je to interalni chyba 99
            if (stack >= 99) {
                error_exit(ERROR_INTERNAL, current_line, "Indentation stack overflow");
            }
            //ulozime novou uroven 
            stack++;
            indent_stack[stack] = spaces;
            token.type = INDENT_TOKEN;
            token.line = current_line;
            return token;
        }
        //pokud je mezer min nez minule musime udelat dedent
        else if (spaces < indent_stack[stack]) {
            //odstranujeme urovne ze zasobniku 
            while(stack > 0 && spaces < indent_stack[stack]) {
                stack--;
                detents_reading++;
            }
            //pokud pocet mezer nesedi tak je to lexikalni chyba 1
            if (indent_stack[stack] != spaces) {
                error_exit(ERROR_LEX, current_line, "Indentation level does not match any outer level");
            }
            //ulozime prvni dedent
            detents_reading--;
            token.type = DEDENT_TOKEN;
            token.line = current_line;
            return token;
        }
    }

    //pouzijeme funkci scan_raw abychom mohli nahrat tokeny (neboli proste nase znaky co jsem skenovali)
    token = scan_raw(f);

    //pokud je to oteviraci zavorka tak zvetsime parents (vnoreni)
    if (token.type == LPAR_TOKEN || token.type == LBRACKET_TOKEN || token.type == LBRACE_TOKEN) {
        parents++;
    }
    //pokud jsou tam zaviralici zavorky tak to naopak zmensime
    else if (token.type == RPAR_TOKEN || token.type == RBRACKET_TOKEN || token.type == RBRACE_TOKEN) {
        if (parents > 0) {
            parents--;
        }
    }

    //pokud z scan_raw budmeme mit konec souboru
    if (token.type == EOF_TOKEN) {
        //jeste zkontrolujeme konec radku
        if (!last_eol) {
            line_start = true;
            last_eol = true;
            token.type = EOL_TOKEN;
            token.line = current_line;
            return token;
        }
        //a musime taky uzavrit vsechny urovne
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
    //pokud nam vrati konec raku 
    if (token.type == EOL_TOKEN) {
        //zavolame znovu funkci pokud se konec radku ignoruje a dostaneme normalni uz vec
        if (parents > 0) {
            newline_skip = true;
            return get_token(f);
        }
        //to same pokud se tam objevi novy radek tak znova zavolame funkci 
        line_start = true;
        if (last_eol) {
            return get_token(f);
        }
        last_eol = true;
        //jinak vraime tokenyyyy
        return token;
    }
    token.check_new_line = newline_skip;
    newline_skip = false;
    //pokud ten token neni eol, tak nastavime posledni enter a vraatime to 
    last_eol = false;
    return token;
}
