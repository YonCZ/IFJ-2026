/*
* IFJ: Generator
 * Soubor: gen_internal.h
 * Autor: Pavel Novák [xnovakp01]
 */


typedef struct {
  int label_counter;
  char *function_name;
}Gen;




/*
 *create from function name and label counter
 */
char *new_label(Gen *gen, AST_NODE *node);


void emit(Gen *gen, char *label);


void emit_string_literal(Gen *gem);
