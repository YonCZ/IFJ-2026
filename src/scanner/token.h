/*
 * IFJ Implimentace prekladace
 * Soubor: token.h
 * Autor: Polina Tihaninova [xtishap00]
 * 
 * prompt: potrebuju sjednotit nazvy tokenu a polozky struktury
 * Token s aktualnim scannerem a parserem. Zachovej moje rozdeleni tokenu,
 * dopln tokeny interpolace a vysvetli, ktere symboly scanner opravdu vraci.
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>

typedef enum {
    //operator
    PLUS_TOKEN,     //plus +
    MINUS_TOKEN,    //minus - 
    MUL_TOKEN,      //nasobeni *
    DIV_TOKEN,      //deleni /
    ASSIGN_TOKEN,   //prirazeni =
    EQ_TOKEN,       //rovno ==
    NEQ_TOKEN,      //nerovno !=
    LESS_EQ_TOKEN,      //mensi nebo rovno <=
    GREAT_EQ_TOKEN,     //vetsi nebo rovno >=
    LESS_TOKEN,         //mensi <
    GREAT_TOKEN,        //vetsi >

    //oddelovace a symboly obecne
    RPAR_TOKEN,     //kulata zavorka ) zprava
    LPAR_TOKEN,     //kulata zavorka ( zleva
    DOT_TOKEN,      //tecka .
    COMMA_TOKEN,     //carka ,
    AT_TOKEN,       //scanner ho vraci jen po overeni celeho zapisu @main;
                    //potom vrati IDENTIFIER_TOKEN s atributem "main"
    COLON_TOKEN,        //dvojtecka :
    SEMICOLON_TOKEN,   //strednik ;

    //slova
    DEF_TOKEN, //def
    IF_TOKEN,       //podminka if
    WHILE_TOKEN,        //podminka while
    DO_TOKEN,           //podmink do while 
    ELSE_TOKEN,     //podminka if else
    THEN_TOKEN,         //podminka if then
    RETURN_TOKEN,         //vraceni return
    IMPORT_TOKEN,       //import
    VAR_TOKEN,         //modifikovatelna promenna
    VAL_TOKEN,      //nemodifikovatelna promenna

    //typy
    INT_TOKEN,      //int
    STRING_TOKEN,       //string
    DOUBLE_TOKEN,       //double
    UNIT_TOKEN,         //skutecna hodnota 
    BOOLEAN_TOKEN,      //Boolean (rozsireni BOOLTHEN)

    //identifikatory a literaly 
    INT_NUM_TOKEN,      //skutecne cislo (5)
    DOUBLE_NUM_TOKEN,       //skutecne double cislo (3.5)
    STRING_WORD_TOKEN,      //retezec v uvozovkach, atribut je jeho obsah
    UNIT_LIT_TOKEN,         //zatim se nevraci; () je dvojice LPAR/RPAR
    IDENTIFIER_TOKEN,       //nazev promenne 

    //komentare
    SLASH_COMM_TOKEN,       //zatim se nevraci, komentar se preskakuje
    SLASH_COMM_END_TOKEN,   //zatim se nevraci, LF resi EOL

    //struktura
    INDENT_TOKEN,   //vyssi uroven odsazeni
    DEDENT_TOKEN,  //nizsi uroven odsazeni
    EOF_TOKEN,   //konec souboru
    EOL_TOKEN,  //konec radku

    //ROZSIRENI
    FOR_TOKEN,     //for cyklus
    TO_TOKEN,       //scanner zatim vraci "to" jako IDENTIFIER_TOKEN
    UNTIL_TOKEN,    //scanner zatim vraci "until" jako IDENTIFIER_TOKEN
    BY_TOKEN,       //scanner zatim vraci "by" jako IDENTIFIER_TOKEN
    AND_TOKEN,      //&&
    OR_TOKEN,       //||
    NOT_TOKEN,      //!
    TRUE_TOKEN,     //true
    FALSE_TOKEN,      //false
    CASE_TOKEN,     //case
    CLASS_TOKEN,    //class
    MATCH_TOKEN,    //match
    TYPE_TOKEN,
    EXTENSION_TOKEN,  //extension
    LIST_TOKEN,     //list

    PLUSPLUS_TOKEN,     //++
    LBRACKET_TOKEN,      //[
    RBRACKET_TOKEN,      //]
    LBRACE_TOKEN,        //{ po ${ v interpolovanem retezci
    RBRACE_TOKEN,        //} ukoncujici vyraz ${...}
    ARROW_FAT_TOKEN,      //=>
    ARROW_LEFT_TOKEN,   //<-
    ARROW_RIGHT_TOKEN,  //zatim se nevraci; -> neni ve specifikaci IFJ26
    ARROW_TOKEN,        //zatim se nevraci; ponechano jen kvuli starym navrhum
    UNDERSCORE_TOKEN,      //_
    PIPE_TOKEN,      //|
    INTERP_START_TOKEN,    //prvni textovy usek s"...", muze byt i prazdny
    INTERP_MID_TOKEN,      //neprazdny text mezi dvema vlozenymi vyrazy
    INTERP_END_TOKEN,      //posledni textovy usek
} Tokens;

typedef struct {
    Tokens type;   //typ tokenu
    char *atribute;  //alokovana hodnota ID/literalu/interpolace, uvolnuje parser
    int line;        //radek pro hlaseni chyb
    bool check_new_line; //pred tokenem byl LF uvnitr zavorek; napr. koncova carka
} Token;

#endif