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
    DOUBLE_DOT_STATE,       //DONE
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
    AND_STATE     //DONE 
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
//kontrola ze tam je s
bool interpolated = false;

//funkce na uvolneni stringu 
static void scanner_cleanup(void) {
    ds_free(&str);
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
                //pokud je to konec radku, tak je to EOL_TOKEN
                else if (c == '\n') {
                    token.type = EOL_TOKEN;
                    token_found = true;
                    break;
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
                            interpolated = true;
                            state = STRING_STATE;
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
                        case '+':
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
                        //pokud je to , (carka), tak je to 
                        case ',':
                            token.type = COMMA_TOKEN;
                            token_found = true;
                            break;
                        //pokud je to . (tecka), tak je to bud po kontrole jestli je to pred cislem tak jde do stavu DOUBL_STATE, jinak do DOT_TOKEN
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
                                for (int i = 3; i >= 0; i--) {
                                    in_ungetc(f, next_chars[i]);
                                }
                                token.type = AT_TOKEN;
                                token_found = true;
                            }
                            //jinak nic neni s @, takze je to lexikalni chyba 1
                            else {
                                error_exit(ERROR_LEX, current_line, "Neplatny znak @");
                            }
                            break;
                        }
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
                        //pokud je too ", tak jde to do STRING_STATE (protozto muze byt slovo)
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
                            error_exit(ERROR_LEX, current_line, "Neocekavany znak");
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
                //pokud je to slach / tak je to komentar s // a vede to do LINE_COMMENT_STATE
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
                    error_exit(ERROR_LEX, current_line, "Neukonceny blokovy komentar");
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
                                    error_exit(ERROR_LEX, current_line, "Nepovolene znaky po viceradkovem blokovem komentari");
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
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak u celociselneho literalu");
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
                    error_exit(ERROR_LEX, current_line, "Spatny format cisla s podtrzitkem");
                }
                break;
            
            //DOUBLE_DOT_STATE = stav kde se kontroluje kam a zajakych podminek se muze oznacit jako tecka v double cisle 
            //pokud je po . cislo napr.1.000, tak je to spravny zapis, jinak se vraci zpatky na cislo 
            case DOUBLE_DOT_STATE: //dokoncit tento state 
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = DOUBLE_STATE;
                }
                else {
                    in_ungetc(f, c);
                    in_ungetc(f, '.');
                    token.type = INT_NUM_TOKEN; 
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            
            //DOUBLE_STATE =je to stav kde se resi jestli je to double cislo a jake za tim muzou byt dalsi podminky 
            //pokud je to double, tak zkontrolujeme jestli je to cislo tak je to zase DOUBLE_STATE 
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
                        error_exit(ERROR_LEX, current_line, "Chybny suffix double");
                    }
                    in_ungetc(f, next);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                //to same ale pokud se tam vubec objevi nejake pismeno
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak u double literalu");
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
                    error_exit(ERROR_LEX, current_line, "Spatny format double s podtrzitkem");
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
                    error_exit(ERROR_LEX, current_line, "Ocekavan exponent");
                }
                break;
            
            //EXP_SIGN_STATE = je to stav ve kterym se resi jestli po operandu se objevuje cislo a jde do EXP_DIGITS_STATE,jinak je to lexikalni chyba
            case EXP_SIGN_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Ocekavano cislo v exponentu");
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
                        error_exit(ERROR_LEX, current_line, "Chybny suffix exponentu");
                    }
                    in_ungetc(f, next);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                //pokud je nejak epismeno tak je to lexikalni chyba 1
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak po exponentu");
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
                    error_exit(ERROR_LEX, current_line, "Spatny format exponentu s podtrzitkem");
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
                    error_exit(ERROR_LEX, current_line, "Nulovy literal s neplatnym suffixem");
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
                    error_exit(ERROR_LEX, current_line, "Ocekavan znak & pro operator AND");
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
                        if (interpolated) {
                            token.type = INTERPOLATED_STRING_TOKEN;
                        } 
                        else {
                            token.type = STRING_WORD_TOKEN;
                        }
                        interpolated = false; 
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
                    error_exit(ERROR_LEX, current_line, "Neukonceny retezec");
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
                    error_exit(ERROR_LEX, current_line, "Nepovolena escape sekvence");
                }
                //jinat je to stale nejake slovo ktere se musi nacist 
                state = STRING_STATE;
                break;
            
            //MULTILINE_STRING_STATE = stav ktery resi jake slova se budou vipysovat v ""
            case MULTILINE_STRING_STATE:
                //pokud je to EOF, je to lexikalni chyba 
                if(c == EOF) {
                    error_exit(ERROR_LEX, current_line, "Neukonceny vice radkovy retezec");
                }
                //jinak jestli tam jsou "", tak se to precte jako nejaky vety ci slova (napr. printf("Toto je moje myslenka v uvozovkach");) a je to STRING_WORD_TOKEN
                else if (c == '"') {
                    int next = in_fgetc(f);
                    int nextnext = in_fgetc(f);
                    if (next == '"' && nextnext == '"') {
                        token.type = STRING_WORD_TOKEN;
                        if (interpolated) {
                            token.type = INTERPOLATED_STRING_TOKEN;
                        } 
                        else {
                            token.type = STRING_WORD_TOKEN;
                        }
                        interpolated = false; 
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                    //jinak se to pushne jako dalsi promena ci nazev funkce
                    else {
                        in_ungetc(f, nextnext); 
                        in_ungetc(f, next);
                        ds_push_char(&str, c);
                    }
                }
                //to same ze se pushne porotoze tu taky to znamena ze je to nazev neceho 
                else {
                    ds_push_char(&str, c);
                }
                break;

            //pokud je to nic z toho tak je to interni chyba 99
            default: 
                error_exit(ERROR_INTERNAL, current_line, "Neznamy stav automatu");
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
                        error_exit(ERROR_LEX, current_line, "Neukonceny blokovy komentar");
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
                        error_exit(ERROR_LEX, current_line, "Nepovolene znaky po viceradkovem blokovem komentari");
                    }
                    //jinak vratime znak a zkontrolujeme tab
                    in_ungetc(f, after);
                    if (has_tab) {
                        error_exit(ERROR_LEX, current_line, "Tabulator na neobrazdnem radku");
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
                    error_exit(ERROR_LEX, current_line, "Tabulator na neobrazdnem radku");
                }
                break;
            }
        //pokud zase nic neni z toho tak se vratime
        } 
        else {
            in_ungetc(f, c);
            //pokud je ta, tab, tak je to zase lexikalni chyba 1
            if (has_tab) {
                error_exit(ERROR_LEX, current_line, "Tabulator na neobrazdnem radku");
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
                error_exit(ERROR_INTERNAL, current_line, "Preteceni zasobniku odsazeni");
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
            error_exit(ERROR_LEX, current_line, "Odsazeni neodpovida zadne urovni");
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
    if (token.type ==LPAR_TOKEN || token.type == LBRACKET_TOKEN) {
        parents++;
    }
    //pokud jsou tam zaviralici zavorky tak to naopak zmensime
    else if (token.type == RPAR_TOKEN || token.type == RBRACKET_TOKEN) {
        if (parents > 0) {
            parents--;
        }
    }

    //promt: proc moje kontroly maji chybu pri vypisovani a hlidani vsech eol 
    //pokud z scan_raw budmeme mit konec souboru
    if (token.type == EOF_TOKEN) {
        //jeste zkontrolujeme konec radku
        if (!last_eol) {
            line_start = true;
            token.check_new_line = last_eol;
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
            return get_token(f);
        }
        //to same pokud se tam objevi novy radek tak znova zavolame funkci 
        line_start =true;
        if (last_eol) {
            return get_token(f);
        }
        last_eol =true;
        //jinak vraime tokenyyyy
        return token;
    }
    //pokud ten token neni eol, tak nastavime posledni enter a vraatime to 
    last_eol = false;
    return token;
}
