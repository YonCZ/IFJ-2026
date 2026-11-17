#ifndef TOKEN_H
#define TOKEN_H

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
    AT_TOKEN,       //zavinac @
    COLON_TOKEN,        //dvojtecka :
    SEMICOLON_TOKEN,   //strednik ;
    UNDERSCORE_TOKEN, // _

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
    BOOLEAN_TOKEN,   //boolean

    //identifikatory a literaly 
    INT_NUM_TOKEN,      //skutecne cislo (5)
    DOUBLE_NUM_TOKEN,       //skutecne double cislo (3.5)
    STRING_WORD_TOKEN,       //skutecne slovo (pes)
    IDENTIFIER_TOKEN,       //nazev promenne 

    //struktura
    INDENT_TOKEN,   //vyssi uroven odsazeni
    DEDENT_TOKEN,  //nizsi uroven odsazeni
    EOF_TOKEN,   //konec souboru
    EOL_TOKEN,  //konec radku

    //ROZSIRENI
    FOR_TOKEN,     //for cyklus
    AND_TOKEN,      //&&
    OR_TOKEN,       //||
    NOT_TOKEN,      //!
    TRUE_TOKEN,     //true
    FALSE_TOKEN,      //false
    CASE_TOKEN,     //case
    CLASS_TOKEN,    //class
    MATCH_TOKEN,    //match
    TYPE_TOKEN,     //type 
    EXTENSION_TOKEN,  //extension
    LIST_TOKEN,     //list

    LBRACKET_TOKEN,      //[
    RBRACKET_TOKEN,      //]
    ARROW_FAT_TOKEN,      //=>
    ARROW_LEFT_TOKEN,   //<-
    PIPE_TOKEN        //|
} Tokens;

//tu okomentovat promt ktery jsem pouzivala
typedef struct {
    Tokens type;   //typ tokenu
    char *atribute;  //hodnota textu
    int line;       //hlaseni chyb
} Token;

#endif

