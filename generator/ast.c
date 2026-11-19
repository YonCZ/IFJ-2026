/*
 * IFJ26 Compiler Implementation
 *
 * File: ast.c
 * Module: Generator
 * Author: Pavel Novák (xnovakp01)
 * Date: September 2026
 */
#include "ast.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *ast_strdup(const char *ptr) {
  if (!ptr) return NULL;
  size_t len = strlen(ptr);
  char *s = malloc(len + 1);
  if (s == NULL) {
    exit(99);
  }
  memcpy(s, ptr, len);
  s[len] = '\0';
  return s;
}


AST_NODE *ast_create(AST_NODE *left, AST_NODE *right,AST_NODE *next, NODE_TYPE type) {
  AST_NODE *node = calloc(1,sizeof(AST_NODE));
  if (!node) {
    exit(99);
  }

  node->type = type;
  node->left = left;
  node->right = right;
  node->next = next;

  return node;
}

void ast_name(AST_NODE *node, char *name) {
  node->name = ast_strdup(name);
}



AST_NODE * ast_int_literal_create( long long value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->ival = value;
  node->data_type = DATA_INT;
  return node;
}

AST_NODE * ast_double_literal_create(double value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->dval = value;
  node->data_type = DATA_DOUBLE;
  return node;
}
AST_NODE * ast_string_literal_create(char *value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->sval = ast_strdup(value);
  node->data_type = DATA_STRING;
  return node;
}

AST_NODE * ast_unit_literal_create(AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->data_type = DATA_UNIT;
  return node;
}

AST_NODE * ast_bool_literal_create(bool value,AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_LITERAL);
  node->data_type = DATA_BOOL;
  if (value) node->ival = 1;
  else node->ival = 0;
  return node;
}



AST_NODE *ast_assign_create(AST_NODE *var, AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(var, expr,next, NODE_ASSIGN);
  return node;
}

//if nothing is returned then NULL
AST_NODE *ast_return_create(AST_NODE *expr, AST_NODE *next) {
  AST_NODE *node = ast_create(expr,NULL,next, NODE_RETURN);
  return node;
}


AST_NODE * ast_binop_create(AST_NODE *left, AST_NODE *right_term, AST_NODE *next, OP_TYPE op)
{
  AST_NODE *node = ast_create(left, right_term,next, NODE_BINOP);
  node->op_type = op;

  return node;
}


AST_NODE * ast_unop_create(AST_NODE *term, AST_NODE *next, OP_TYPE op) {
  AST_NODE *node = ast_create(term, NULL, next, NODE_UNOP);
  node->op_type = op;
  return node;

}


AST_NODE * ast_ident_create(AST_NODE *left, char *name, AST_NODE *next) {
  AST_NODE *node = ast_create(left, NULL, next, NODE_IDENT);
  ast_name(node, name);
  return node;
}

AST_NODE * ast_var_def_create(AST_NODE *term,AST_NODE *next, char *name, bool is_const, DATA_TYPE declared) { //semantic analyze must check if DATA_NONE
  AST_NODE *node = ast_create(term, NULL, next, NODE_VAR_DEF);
  ast_name(node, name);
  node->is_const = is_const;
  node->data_type = declared;
  return node;
}

AST_NODE * ast_if_create(AST_NODE *cond, AST_NODE *then,  AST_NODE *els,AST_NODE *next) {
  AST_NODE *node = ast_create(cond,then, next, NODE_IF);
  node->third = els;
  return node;

}

AST_NODE * ast_while_create(AST_NODE *left, AST_NODE *body,AST_NODE *next) {
  AST_NODE *node = ast_create(left,body, next, NODE_WHILE);
  return node;
}

AST_NODE * ast_func_def_create(AST_NODE *body,  AST_NODE *params, AST_NODE *next, DATA_TYPE return_type,
                               char *name, bool is_main, bool has_parens ) {
  AST_NODE *node = ast_create(body,NULL, next, NODE_FUNC_DEF);
  ast_name(node, name);
  node->third = params;
  node->data_type = return_type;
  node->is_main = is_main;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_param_create (DATA_TYPE type, char *name, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL, next, NODE_PARAM);
  ast_name(node, name);
  node->data_type = type;
  return node;
}

AST_NODE *ast_for_create(AST_NODE *range, AST_NODE *body, AST_NODE *guards, AST_NODE *next, char *name) {
  AST_NODE *node= ast_create(range, body, next, NODE_FOR);
  ast_name(node, name);
  node->third = guards;
  return node;
}

AST_NODE *ast_range_create(AST_NODE *from, AST_NODE *to, AST_NODE *step, AST_NODE *next, bool inclusive) {
  AST_NODE *node = ast_create(from,to, next, NODE_RANGE);
  node->third = step;
  node->inclusive = inclusive;
  return node;

}


AST_NODE * ast_call_create(AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *next) {
  AST_NODE *node = ast_create(NULL,NULL,next, NODE_CALL);
  ast_name(node, name);
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_method_call_create(AST_NODE *params, char *name, bool is_method, bool has_parens, AST_NODE *reciever, AST_NODE *next) {
  AST_NODE *node = ast_create(reciever,NULL, next, NODE_CALL);
  ast_name(node, name);
  node->third = params;
  node->is_method = is_method;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *interpolades(AST_NODE *part, AST_NODE *next){
  AST_NODE *node=ast_create(part,NULL,next, NODE_INTERP);
  return node;
}


AST_NODE *ast_create_extension(AST_NODE *expr, AST_NODE *params,AST_NODE *next, char *name, bool has_parens, bool is_method, DATA_TYPE declared){
  AST_NODE *node = ast_create(expr, NULL, next, NODE_EXPR_STMT);
  node->third = params;
  ast_name(node, name);
  node->is_method = is_method;
  node->data_type = declared;
  node->has_parens = has_parens;
  return node;
}

AST_NODE *ast_create_params_list_of_list(AST_NODE *next_param_list, int list_cnt) {
  AST_NODE *node = ast_create(NULL, NULL, next_param_list, NODE_LIST_OF_LIST);
  node->list_cnt = list_cnt;
  return node;
}

AST_NODE *ast_create_param_list(AST_NODE *params, AST_NODE *next, int param_cnt) {
  AST_NODE *node = ast_create(params,NULL, next, NODE_PARAM_LIST);
  node->list_param_cnt = param_cnt;
  return node;
}

AST_NODE *ast_append(AST_NODE *new_node, AST_NODE *next) {
  if (!new_node) return next;
  if (!next) return new_node;
  if (!next->next) {
    next->next = new_node;
  }
  else {
    ast_append(new_node, next->next);
  }
  return next;
}

void free_AST(AST_NODE *node) {
  if (!node) {return;}
  free_AST(node->left);
  free_AST(node->right);
  free_AST(node->next);
  free_AST(node->third);
  free(node->name);
  free(node->sval);
  free(node);
}


//TODO VYMAZAT AI SNOB


/* ---------------------------------------------------------------- nazvy */

static const char *data_type_name(DATA_TYPE t) {
  switch (t) {
    case DATA_INT:    return "Int";
    case DATA_DOUBLE: return "Double";
    case DATA_STRING: return "String";
    case DATA_BOOL:   return "Bool";
    case DATA_UNIT:   return "Unit";
    default:          return "?";
  }
}

static const char *op_name(OP_TYPE op) {
  switch (op) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_GT:  return ">";
    case OP_LT:  return "<";
    case OP_GE:  return ">=";
    case OP_LE:  return "<=";
    case OP_EQ:  return "==";
    case OP_NEQ: return "!=";
    case OP_AND: return "&&";
    case OP_OR:  return "||";
    case OP_NOT: return "!";
    default:     return "?op";
  }
}

static const char *type_name(NODE_TYPE t) {
  switch (t) {
    case NODE_NONE:         return "NONE";
    case NODE_PROGRAM:      return "PROGRAM";
    case NODE_FUNC_DEF:     return "FUNC_DEF";
    case NODE_PARAM:        return "PARAM";
    case NODE_BLOCK:        return "BLOCK";
    case NODE_VAR_DEF:      return "VAR_DEF";
    case NODE_ASSIGN:       return "ASSIGN";
    case NODE_IF:           return "IF";
    case NODE_WHILE:        return "WHILE";
    case NODE_RETURN:       return "RETURN";
    case NODE_CALL:         return "CALL";
    case NODE_EXPR_STMT:    return "EXPR_STMT";
    case NODE_BINOP:        return "BINOP";
    case NODE_UNOP:         return "UNOP";
    case NODE_IDENT:        return "IDENT";
    case NODE_LITERAL:      return "LITERAL";
    case NODE_FOR:          return "FOR";
    case NODE_RANGE:        return "RANGE";
    case NODE_INTERP:       return "INTERP";
    case NODE_LIST_OF_LIST: return "LIST_OF_LIST";
    case NODE_PARAM_LIST:   return "PARAM_LIST";
    default:                return "BUILTIN?";
  }
}

/* Popisky deti: ktery ukazatel (left/right/third) znamena co, podle typu uzlu. */
static const char *slot_label(const AST_NODE *n, int slot) {
  /* slot: 0 = left, 1 = right, 2 = third */
  static const char *def[3] = {"left", "right", "third"};
  const char *l[3] = {def[0], def[1], def[2]};

  switch (n->type) {
    case NODE_PROGRAM:    l[0] = "functions"; break;
    case NODE_FUNC_DEF:   l[0] = "body"; l[2] = "params"; break;
    case NODE_VAR_DEF:    l[0] = "init"; break;
    case NODE_ASSIGN:     l[0] = "var"; l[1] = "value"; break;
    case NODE_IF:         l[0] = "cond"; l[1] = "then"; l[2] = "else"; break;
    case NODE_WHILE:      l[0] = "cond"; l[1] = "body"; break;
    case NODE_RETURN:     l[0] = "value"; break;
    case NODE_CALL:       l[0] = "receiver"; l[2] = "args"; break;
    case NODE_EXPR_STMT:  l[0] = "expr"; l[2] = "params"; break;
    case NODE_UNOP:       l[0] = "operand"; break;
    case NODE_FOR:        l[0] = "range"; l[1] = "body"; l[2] = "guards"; break;
    case NODE_RANGE:      l[0] = "from"; l[1] = "to"; l[2] = "step"; break;
    case NODE_INTERP:     l[0] = "part"; break;
    case NODE_PARAM_LIST: l[0] = "params"; break;
    case NODE_BLOCK:      l[0] = "stmts"; break;
    default: break;
  }
  return l[slot];
}

/* ------------------------------------------------------------- jeden uzel */

static void print_string_value(FILE *out, const char *s) {
  fputc('"', out);
  for (; s && *s; s++) {
    switch (*s) {
      case '\n': fputs("\\n", out); break;
      case '\t': fputs("\\t", out); break;
      case '\r': fputs("\\r", out); break;
      case '"':  fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      default:   fputc(*s, out);
    }
  }
  fputc('"', out);
}

/* Popis uzlu na jeden radek (bez odradkovani). */
static void print_node_text(FILE *out, const AST_NODE *n) {
  switch (n->type) {
    case NODE_FUNC_DEF:
      fprintf(out, "FUNC_DEF %s%s -> %s", n->name ? n->name : "?",
              n->is_main ? " [@main]" : "", data_type_name(n->data_type));
      break;
    case NODE_PARAM:
      fprintf(out, "PARAM %s : %s", n->name ? n->name : "?", data_type_name(n->data_type));
      break;
    case NODE_VAR_DEF:
      fprintf(out, "VAR_DEF %s %s$%d : %s", n->is_const ? "val" : "var",
              n->name ? n->name : "?", n->id, data_type_name(n->data_type));
      break;
    case NODE_IDENT:
      fprintf(out, "IDENT %s$%d", n->name ? n->name : "?", n->id);
      if (n->data_type != DATA_NONE) fprintf(out, " : %s", data_type_name(n->data_type));
      break;
    case NODE_LITERAL:
      fprintf(out, "LITERAL %s ", data_type_name(n->data_type));
      switch (n->data_type) {
        case DATA_INT:    fprintf(out, "%lld", n->ival); break;
        case DATA_DOUBLE: fprintf(out, "%g", n->dval); break;
        case DATA_STRING: print_string_value(out, n->sval); break;
        case DATA_BOOL:   fputs(n->ival ? "true" : "false", out); break;
        case DATA_UNIT:   fputs("()", out); break;
        default:          fputs("?", out);
      }
      break;
    case NODE_BINOP:
    case NODE_UNOP:
      fprintf(out, "%s %s", type_name(n->type), op_name(n->op_type));
      if (n->data_type != DATA_NONE) fprintf(out, " : %s", data_type_name(n->data_type));
      break;
    case NODE_CALL:
      fprintf(out, "CALL %s%s", n->name ? n->name : "?", n->is_method ? " (metoda)" : "");
      if (n->data_type != DATA_NONE) fprintf(out, " : %s", data_type_name(n->data_type));
      break;
    case NODE_RANGE:
      fprintf(out, "RANGE %s", n->inclusive ? "(vcetne konce)" : "(bez konce)");
      break;
    case NODE_FOR:
      fprintf(out, "FOR %s", n->name ? n->name : "?");
      break;
    case NODE_EXPR_STMT:
      fputs("EXPR_STMT", out);
      if (n->name) fprintf(out, " %s", n->name);
      break;
    default:
      fputs(type_name(n->type), out);
  }
}

/* --------------------------------------------------------------- strom */

/* spoji prefix a pokracovani, vysledek je treba uvolnit */
static char *join(const char *a, const char *b) {
  char *r = malloc(strlen(a) + strlen(b) + 1);
  if (!r) exit(99);
  strcpy(r, a);
  strcat(r, b);
  return r;
}

static void print_node(FILE *out, const AST_NODE *n, const char *prefix, bool last, const char *label);

/* Jedna pozice (left/right/third): bud jeden uzel, nebo cely retez pres next. */
static void print_slot(FILE *out, const AST_NODE *s, const char *label, const char *prefix, bool last) {
  if (!s->next) {
    print_node(out, s, prefix, last, label);
    return;
  }
  fprintf(out, "%s%s%s:\n", prefix, last ? "└── " : "├── ", label);
  char *inner = join(prefix, last ? "    " : "│   ");
  for (const AST_NODE *c = s; c; c = c->next) {
    print_node(out, c, inner, c->next == NULL, NULL);
  }
  free(inner);
}

/* Uzel + jeho deti. label je popisek pozice, ve ktere uzel stoji (nebo NULL). */
static void print_node(FILE *out, const AST_NODE *n, const char *prefix, bool last, const char *label) {
  fprintf(out, "%s%s", prefix, last ? "└── " : "├── ");
  if (label) fprintf(out, "%s: ", label);
  print_node_text(out, n);
  fputc('\n', out);

  const AST_NODE *slots[3] = {n->left, n->right, n->third};
  int count = 0, lastslot = -1;
  for (int i = 0; i < 3; i++) {
    if (slots[i]) {
      count++;
      lastslot = i;
    }
  }
  if (count == 0) return;

  char *child_prefix = join(prefix, last ? "    " : "│   ");
  for (int i = 0; i < 3; i++) {
    if (slots[i]) {
      print_slot(out, slots[i], slot_label(n, i), child_prefix, i == lastslot);
    }
  }
  free(child_prefix);
}

/* ------------------------------------------------------------ rozhrani */

void ast_print(const AST_NODE *root) {
  FILE *out = stderr;

  if (!root) {
    fputs("(prazdny AST)\n", out);
    return;
  }

  /* koren: vypsat bez vetve, pak jeho deti; dalsi uzly pres next (kdyby jich bylo vic) */
  for (const AST_NODE *n = root; n; n = n->next) {
    print_node_text(out, n);
    fputc('\n', out);

    const AST_NODE *slots[3] = {n->left, n->right, n->third};
    int lastslot = -1;
    for (int i = 0; i < 3; i++) {
      if (slots[i]) lastslot = i;
    }
    for (int i = 0; i < 3; i++) {
      if (slots[i]) print_slot(out, slots[i], slot_label(n, i), "", i == lastslot);
    }
  }
}

/* Vypise jen jeden uzel na jeden radek (bez deti a bez next).
 * V zavorce je videt, ktere ukazatele jsou vyplnene, napr. VAR_DEF var x$1 : Int  [left, next] */
void ast_print_node(const AST_NODE *node) {
  if (!node) {
    fputs("(NULL)\n", stderr);
    return;
  }
  print_node_text(stderr, node);

  const char *names[4] = {"left", "right", "third", "next"};
  const AST_NODE *ptrs[4] = {node->left, node->right, node->third, node->next};
  const char *sep = "  [";
  for (int i = 0; i < 4; i++) {
    if (ptrs[i]) {
      fprintf(stderr, "%s%s", sep, names[i]);
      sep = ", ";
    }
  }
  if (sep[0] == ',') fputc(']', stderr);
  fputc('\n', stderr);
}
