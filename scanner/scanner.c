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

    OR_STATE,
    AND_STATE
} State;

static int back_stack[16];
static int top = 0;
static int current_line = 1;

static DString str;
static bool ds_initialized = false;

static void scanner_cleanup(void) {
    ds_free(&str);
}

static int in_fgetc(FILE *f) {
    int c;
    if (top > 0) {
        c = back_stack[--top];
    }
    else {
        c = fgetc(f);
    }
    if (c == '\n') {
        current_line++;
    }
    return c;
}

static void in_ungetc (FILE *f, int c) {
    if (c == EOF) {
        return;
    }
    if (c == '\n') {
        current_line--;
    }
    if (top < 16) {
        back_stack[top++] = c;
    }
    else {
        ungetc(c,f);
    }
}

static Token scan_raw(FILE *f) {
    if (!ds_initialized) {
        ds_init(&str);
        error_cleanup(scanner_cleanup);
        ds_initialized = true;
    }
    ds_clear(&str);

    State state = START_STATE;
    int c;
    Token token = {0};
    token.atribute = NULL;
    bool token_found = false;

    int nesting_level = 0;
    bool comm_newline = false;

    while(!token_found) {
        c = in_fgetc(f);
        switch (state) {
            /*START_STATE = hlavni stav konecneho automatu, kde se urcuje, do jakych stavu budou smerovat nebo zdali uz jsou nejake tokeny
            - urcuji se mezery, konec radku nebo souboru, cisla, slova, nebo zakladni znaky jako je napr. *, - , +*/
            case START_STATE:
                if (c == ' ' || c == '\t') { 
                    break;
                }
                else if (c == '\r') {
                    break;
                }
                else if (c == '\n') {
                    token.type = EOL_TOKEN;
                    token_found = true;
                    break;
                }
                else if (c == EOF) {
                    token.type = EOF_TOKEN;
                    token_found = true;
                    break;
                }
                else if (c >= '1' && c <= '9') {
                    state = INT_STATE;
                    ds_push_char(&str, c);
                }
                else if (c == '0') {
                    state = ZERO_STATE;
                    ds_push_char(&str, c);
                }
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    state = WORD_STATE;
                    ds_push_char(&str, c);
                }
                else {
                    //zde se urcuje zakladni znaky a jeji rozdeleni do dalsich stavu
                    switch(c) {
                        case '*':
                            token.type = MUL_TOKEN;
                            token_found = true;
                            break;
                        case '+':
                            token.type = PLUS_TOKEN; 
                            token_found = true;
                            break;
                        case '-':
                            token.type = MINUS_TOKEN;
                            token_found = true;
                            break;
                        case ')':
                            token.type = RPAR_TOKEN;
                            token_found = true;
                            break;
                        case '(':
                            token.type = LPAR_TOKEN;
                            token_found = true;
                            break;
                        case '[':
                            token.type = LBRACKET_TOKEN;
                            token_found = true;
                            break;
                        case ']':
                            token.type = RBRACKET_TOKEN;
                            token_found = true;
                            break;
                        case ',':
                            token.type = COMMA_TOKEN;
                            token_found = true;
                            break;
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
                        case ':':
                            token.type = COLON_TOKEN;
                            token_found = true;
                            break;
                        case ';':
                            token.type = SEMICOLON_TOKEN;
                            token_found = true;
                            break;
                        case '@': {
                            char next_chars[5] = {0};
                            for (int i = 0; i < 4; i++) {
                                next_chars[i] = in_fgetc(f);
                            }

                            if (strcmp(next_chars, "main") == 0) {
                                for (int i = 3; i >= 0; i--) {
                                    in_ungetc(f, next_chars[i]);
                                }
                                token.type = AT_TOKEN;
                                token_found = true;
                            }
                            else {
                                error_exit(ERROR_LEX, current_line, "Neplatny znak @");
                            }
                            break;
                        }
                        case '_':
                            ds_push_char(&str, '_');
                            state = WORD_STATE;
                            break;
                        case '|':
                            state = OR_STATE;
                            break;
                        case '&':
                            state = AND_STATE;
                            break;
                        case '/':
                            state = SLASH_STATE;
                            break;
                        case '"':
                            state = STRING_STATE;
                            break;
                        case '=':
                            state = ASSIGN_STATE;
                            break;
                        case '!':
                            state = EXCLAM_STATE;
                            break;
                        case '<':
                            state = LESS_STATE;
                            break;
                        case '>':
                            state = GREAT_STATE;
                            break;
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
                if (c == '/') {
                    state = LINE_COMMENT_STATE;
                }
                else if (c == '*') {
                    state = BLOCK_COMM_STATE;
                    nesting_level = 1;
                    comm_newline = false;
                }
                else {
                    in_ungetc(f, c);
                    token.type = DIV_TOKEN;
                    token_found = true;
                    break;
                }
                break;

            //BLOCK_COMM_STATE = stav, ktery urcuje zdali tam je hvezdicka ktera bude ukoncovat komentar 
            case BLOCK_COMM_STATE:
                if(c == EOF) {
                    error_exit(ERROR_LEX, current_line, "Neukonceny blokovy komentar");
                }
                else if (c == '\n') {
                    comm_newline = true;
                }
                else if (c == '/') {
                    int next = in_fgetc(f);
                    if (next == '*') {
                        nesting_level++;
                    }
                    else {
                        in_ungetc(f, next);
                    }
                }
                else if (c == '*') {
                    int next = in_fgetc(f);
                    if (next == '/') {
                        nesting_level--;
                        if (nesting_level == 0) {
                            if (comm_newline) {
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
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                }
                else if (c == '.') {
                    int next = in_fgetc(f);
                    if (next >= '0' && next <= '9') {
                        ds_push_char(&str, '.');
                        ds_push_char(&str, next);
                        state = DOUBLE_STATE;
                    }
                    else {
                        in_ungetc(f, next);
                        in_ungetc(f, '.');
                        token.type = INT_NUM_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                }
                else if (c == '_') {
                    state = INT_UNDERSCORE_STATE;
                }
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak u celociselneho literalu");
                }
                else {
                    in_ungetc(f, c);
                    token.type = INT_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            
            case INT_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = INT_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Spatny format cisla s podtrzitkem");
                }
                break;

            case DOUBLE_DOT_STATE: 
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
            
            case DOUBLE_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = DOUBLE_STATE;
                }
                else if (c == '_') {
                    state = DOUBLE_UNDERSCORE_STATE;
                }
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
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
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak u double literalu");
                }
                else {
                    in_ungetc(f, c);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            
            case DOUBLE_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = DOUBLE_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Spatny format double s podtrzitkem");
                }
                break;
            
            case EXP_START_STATE:
                if (c == '+' || c == '-') {
                    ds_push_char(&str, c);
                    state = EXP_SIGN_STATE;
                }
                else if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Ocekavan exponent");
                }
                break;
            
            case EXP_SIGN_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Ocekavano cislo v exponentu");
                }
                break;

            case EXP_DIGITS_STATE:
                if (c >= '0' && c <= '9') {
                    ds_push_char(&str, c);
                }
                else if (c == '_') {
                    state = EXP_UNDERSCORE_STATE;
                }
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
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    error_exit(ERROR_LEX, current_line, "Neocekavany znak po exponentu");
                }
                else {
                    in_ungetc(f, c);
                    token.type = DOUBLE_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;
            
            case EXP_UNDERSCORE_STATE:
                if (c <= '9' && c >= '0') {
                    ds_push_char(&str, c);
                    state = EXP_DIGITS_STATE;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Spatny format exponentu s podtrzitkem");
                }
                break;

            case ZERO_STATE:
                if (c == '.') {
                    int next = in_fgetc(f);
                    if (next >= '0' && next <= '9'){
                        ds_push_char(&str, c);
                        ds_push_char(&str, next);
                        state = DOUBLE_STATE;
                    }
                    else {
                        in_ungetc(f, next);
                        in_ungetc(f, '.');
                        token.type = INT_NUM_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                }
                else if (c == 'e' || c == 'E') {
                    ds_push_char(&str, c);
                    state = EXP_START_STATE;
                }
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                    error_exit(ERROR_LEX, current_line, "Nulovy literal s neplatnym suffixem");
                }
                else {
                    in_ungetc(f, c);
                    token.type = INT_NUM_TOKEN;
                    token.atribute = ds_dup(&str);
                    token_found = true;
                    break;
                }
                break;

            case ASSIGN_STATE:
                if (c == '=') {
                    token.type = EQ_TOKEN;
                    token_found = true;
                    break;
                }
                if (c == '>') {
                    token.type = ARROW_FAT_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    in_ungetc(f, c);
                    token.type = ASSIGN_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            
            case LESS_STATE:
                if (c == '=') {
                    token.type = LESS_EQ_TOKEN;
                    token_found = true;
                    break;
                }
                else if (c == '-') {
                    token.type = ARROW_LEFT_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    in_ungetc(f, c);
                    token.type = LESS_TOKEN;
                    token_found = true;
                    break;
                }
                break;

            case GREAT_STATE:
                if (c == '=') {
                    token.type = GREAT_EQ_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    in_ungetc(f, c);
                    token.type = GREAT_TOKEN;
                    token_found = true;
                    break;
                }
                break;

            case EXCLAM_STATE:
                if (c == '=') {
                    token.type = NEQ_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    in_ungetc(f, c);
                    token.type = NOT_TOKEN;
                    token_found = true;
                    break;
                }
                break;

            case OR_STATE:
                if (c == '|') {
                    token.type = OR_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    in_ungetc(f, c);
                    token.type = PIPE_TOKEN;
                    token_found = true;
                    break;
                }
                break;
            
            case AND_STATE:
                if (c == '&') {
                    token.type = AND_TOKEN;
                    token_found = true;
                    break;
                }
                else {
                    error_exit(ERROR_LEX, current_line, "Ocekavan znak & pro operator AND");
                }
                break;
            
            case STRING_STATE:
                if (c == '"') {
                    if (str.len == 0) {
                        int next_c = in_fgetc(f);
                        if (next_c == '"') {
                            state = MULTILINE_STRING_STATE;
                        } 
                        else {
                            in_ungetc(f, next_c);
                            token.type = STRING_WORD_TOKEN;
                            token.atribute = ds_dup(&str);
                            token_found = true;
                        }
                    } 
                    else {
                        token.type = STRING_WORD_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                    }
                    break;
                }
                else if (c == '\\') {
                    state = STRING_ESCAPE_STATE;
                }
                else if (c == '\n' || c == '\r' || c == EOF) {
                    error_exit(ERROR_LEX, current_line, "Neukonceny retezec");
                }
                else {
                    ds_push_char(&str, c);
                }
                break;

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
                else {
                    error_exit(ERROR_LEX, current_line, "Nepovolena escape sekvence");
                }
                state = STRING_STATE;
                break;
            
            case MULTILINE_STRING_STATE:
                if(c == EOF) {
                    error_exit(ERROR_LEX, current_line, "Neukonceny vice radkovy retezec");
                }
                else if (c == '"') {
                    int c2 = in_fgetc(f);
                    int c3 = in_fgetc(f);
                    if (c2 == '"' && c3 == '"') {
                        token.type = STRING_WORD_TOKEN;
                        token.atribute = ds_dup(&str);
                        token_found = true;
                        break;
                    }
                    else {
                        in_ungetc(f, c3); 
                        in_ungetc(f, c2);
                        ds_push_char(&str, c);
                    }
                }
                else {
                    ds_push_char(&str, c);
                }
                break;

            default: 
                error_exit(ERROR_INTERNAL, current_line, "Neznamy stav automatu");
        }
    }

    if (token.type == EOL_TOKEN) {
        token.line = current_line - 1;
    } else {
        token.line = current_line;
    }
    return token;
}

static int count_indent(FILE *f) {
    int spaces = 0;
    bool has_tab = false;
    int c;
    while ((c = in_fgetc(f)) != EOF) {
        if (c == ' ') {
            spaces++;
        } else if (c == '\t') {
            has_tab = true;
        } else if (c == '\n') {
            spaces = 0;
            has_tab = false;
        } else if (c == '/') {
            int next = in_fgetc(f);
            if (next == '/') {
                while ((c = in_fgetc(f)) != '\n' && c != EOF);
                if (c == EOF) {
                    return 0;
                }
                spaces = 0;
                has_tab = false;
            } else {
                in_ungetc(f, next);
                in_ungetc(f, '/');
                if (has_tab) {
                    error_exit(ERROR_LEX, current_line, "Tabulator na neobrazdnem radku");
                }
                break;
            }
        } else {
            in_ungetc(f, c);
            if (has_tab) {
                error_exit(ERROR_LEX, current_line, "Tabulator na neobrazdnem radku");
            }
            break;
        }
    }
    
    if (c == EOF) {
        return 0;
    }
    return spaces;
}

Token get_token(FILE *f) {
    static int indent_stack[100] = {0};
    static int stack = 0;
    static int detents_reading = 0;
    static bool line_start = true;
    static int parents = 0;
    static bool last_eol = true;

    Token token = {0};

    if (detents_reading > 0) {
        detents_reading--;
        token.type = DEDENT_TOKEN;
        token.line = current_line;
        return token;
    }
    
    if (line_start && parents == 0) {
        line_start = false;
        int spaces = count_indent(f);

        if (spaces > indent_stack[stack]) {
            if (stack >= 99) {
                error_exit(ERROR_INTERNAL, current_line, "Preteceni zasobniku odsazeni");
            }
            stack++;
            indent_stack[stack] = spaces;
            token.type = INDENT_TOKEN;
            token.line = current_line;
            return token;
        }
        else if (spaces < indent_stack[stack]) {
        while(stack > 0 && spaces < indent_stack[stack]) {
            stack--;
            detents_reading++;
        }
        if (indent_stack[stack] != spaces) {
            error_exit(ERROR_LEX, current_line, "Odsazeni neodpovida zadne urovni");
        }
        detents_reading--;
        token.type = DEDENT_TOKEN;
        token.line = current_line;
        return token;
        }
    }

    token = scan_raw(f);

    if (token.type ==LPAR_TOKEN || token.type == LBRACKET_TOKEN) {
        parents++;
    }
    else if (token.type == RPAR_TOKEN || token.type == RBRACKET_TOKEN) {
        if (parents > 0) {
            parents--;
        }
    }

    if (token.type == EOF_TOKEN) {
        if (!last_eol) {
            last_eol = true;
            line_start = true;
            token.type = EOL_TOKEN;
            token.line = current_line;
            return token;
        }

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
    if (token.type == EOL_TOKEN) {
        if (parents > 0) {
            return get_token(f);
        }
        line_start =true;
        if (last_eol) {
            return get_token(f);
        }
        last_eol =true;
        return token;
    }
    last_eol = false;
    return token;
}
