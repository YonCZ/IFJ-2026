/*
 * IFJ26 Compiler
 * File: gen_expr.c
 * Author: Pavel Novák [xnovakp01]
 */

#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "gen_internal.h"
#include "generator.h"
#include "error.h"

void literal(AST_NODE *node, char *func_name) {
  printf("%s ", func_name);
  switch (node->data_type) {
  case DATA_STRING:
    emit_string_literal(node->sval);
    break;
  case DATA_INT:
    printf("int@%lld", node->ival);
    break;
  case DATA_BOOL:
    printf("bool@%s", node->ival ? "true" : "false");
    break;
  case DATA_DOUBLE:
    printf("float@%a", node->dval);
    break;
  case DATA_UNIT:
    printf("nil@nil");
    break;
  default:
    error_exit(ERROR_INTERNAL, node->line, "Literal write");
  }
  printf("\n");
}

// stores a stack value into the node variable
void pop_to_val(AST_NODE *node) {
  printf("POPS LF@%s$%d\n", node->name, node->id);
}

// pushes the node variable onto the stack
void push_to_val(AST_NODE *node) {
  printf("PUSHS LF@%s$%d\n", node->name, node->id);
}

// generates list construction
static void gen_list_constructor(Gen *gen, AST_NODE *node) {
  size_t count = 0;
  for (AST_NODE *arg = node->third; arg != NULL; arg = arg->next) {
    gen_expr(gen, arg);
    count++;
  }
  AST_NODE *list_type = node->type_ref;
  char *tag = ast_type_key(list_type != NULL ? list_type->left : NULL);
  printf("NEWARRAY LF@tmp2 int@%zu\n", count + 2);
  printf("ARRAYSET LF@tmp2 int@0 string@$list:%s\n", tag);
  free(tag);
  printf("ARRAYSET LF@tmp2 int@1 int@%zu\n", count);
  for (size_t i = count; i > 0; i--) {
    printf("POPS LF@tmp1\n");
    printf("ARRAYSET LF@tmp2 int@%zu LF@tmp1\n", i + 1);
  }
  printf("PUSHS LF@tmp2\n");
}

// generates case class construction
static void gen_case_constructor(Gen *gen, AST_NODE *node) {
  size_t count = 0;
  for (AST_NODE *arg = node->third; arg != NULL; arg = arg->next) {
    gen_expr(gen, arg);
    count++;
  }
  printf("NEWARRAY LF@tmp2 int@%zu\n", count + 1);
  printf("ARRAYSET LF@tmp2 int@0 string@$class:%s\n",
         node->name != NULL ? node->name : "?");
  for (size_t i = count; i > 0; i--) {
    printf("POPS LF@tmp1\n");
    printf("ARRAYSET LF@tmp2 int@%zu LF@tmp1\n", i);
  }
  printf("PUSHS LF@tmp2\n");
}

// generates a list from a range
static void gen_range_list(Gen *gen, AST_NODE *node) {
  AST_NODE *range = node;
  AST_NODE *step = NULL;
  if (node->type == NODE_BINOP && node->op_type == OP_BY) {
    step = node->right;
    range = node->left;
  }
  AST_NODE *from = NULL;
  AST_NODE *to = NULL;
  bool inclusive = false;
  if (range->type == NODE_RANGE) {
    from = range->left;
    to = range->right;
    if (step == NULL) {
      step = range->third;
    }
    inclusive = range->inclusive;
  } else if (range->type == NODE_BINOP) {
    from = range->left;
    to = range->right;
    inclusive = range->op_type == OP_TO;
  }
  if (from != NULL) {
    gen_expr(gen, from);
  } else {
    printf("PUSHS int@0\n");
  }
  if (to != NULL) {
    gen_expr(gen, to);
  } else {
    printf("PUSHS int@0\n");
  }
  if (step != NULL) {
    gen_expr(gen, step);
  } else {
    printf("PUSHS int@1\n");
  }
  printf("PUSHS int@%d\n", inclusive ? 1 : 0);
  printf("CALL $b_list_range\n");
}
// generates call arguments on the stack
static void gen_call_args(Gen *gen, AST_NODE *call) {
  if (call->arg_groups != NULL) {
    for (AST_NODE *group = call->arg_groups->next; group != NULL;
         group = group->next) {
      for (AST_NODE *param = group->left; param != NULL; param = param->next) {
        gen_expr(gen, param);
      }
    }
  } else {
    for (AST_NODE *param = call->third; param != NULL; param = param->next) {
      gen_expr(gen, param);
    }
  }
}

// returns the type name for pattern matching
static const char *basic_pattern_type(DATA_TYPE type) {
  switch (type) {
  case DATA_INT:
    return "int";
  case DATA_DOUBLE:
    return "float";
  case DATA_STRING:
    return "string";
  case DATA_BOOL:
    return "bool";
  case DATA_UNIT:
    return "nil";
  default:
    return "nil";
  }
}

// generates literal pattern comparison
static void gen_literal_pattern(Gen *gen, AST_NODE *pattern, const char *value,
                                int fail_label) {
  printf("TYPE LF@tmp1 %s\n", value);
  if (pattern->data_type == DATA_INT || pattern->data_type == DATA_DOUBLE) {
    int int_label = new_label(gen);
    int done_label = new_label(gen);
    printf("JUMPIFEQ xMATCH$%d LF@tmp1 string@int\n", int_label);
    printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@float\n", fail_label);
    printf("PUSHS %s\n", value);
    if (pattern->data_type == DATA_INT) {
      printf("PUSHS float@%a\n", (double)pattern->ival);
    } else {
      printf("PUSHS float@%a\n", pattern->dval);
    }
    printf("EQS\n");
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS xMATCH$%d\n", fail_label);
    printf("JUMP xMATCH$%d\n", done_label);
    printf("LABEL xMATCH$%d\n", int_label);
    printf("PUSHS %s\n", value);
    if (pattern->data_type == DATA_INT) {
      printf("PUSHS int@%lld\n", pattern->ival);
    } else {
      printf("INT2FLOATS\n");
      printf("PUSHS float@%a\n", pattern->dval);
    }
    printf("EQS\n");
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS xMATCH$%d\n", fail_label);
    printf("LABEL xMATCH$%d\n", done_label);
    return;
  }
  printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@%s\n", fail_label,
         basic_pattern_type(pattern->data_type));
  if (pattern->data_type == DATA_UNIT) {
    return;
  }
  printf("PUSHS %s\n", value);
  literal(pattern, "PUSHS");
  printf("EQS\n");
  printf("PUSHS bool@false\n");
  printf("JUMPIFEQS xMATCH$%d\n", fail_label);
}
// generates type pattern comparison
static void gen_type_pattern(Gen *gen, AST_NODE *type, const char *value,
                             int fail_label) {
  if (type == NULL) {
    return;
  }
  if (type->data_type == DATA_UNION) {
    int next_label = new_label(gen);
    int success_label = new_label(gen);
    gen_type_pattern(gen, type->left, value, next_label);
    printf("JUMP xMATCH$%d\n", success_label);
    printf("LABEL xMATCH$%d\n", next_label);
    gen_type_pattern(gen, type->right, value, fail_label);
    printf("LABEL xMATCH$%d\n", success_label);
    return;
  }

  printf("TYPE LF@tmp1 %s\n", value);
  if (type->data_type == DATA_LIST || type->data_type == DATA_CLASS) {
    printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@array\n", fail_label);
    printf("ARRAYGET LF@tmp1 %s int@0\n", value);
    if (type->data_type == DATA_LIST) {
      char *tag = ast_type_key(type->left);
      printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@$list:%s\n", fail_label, tag);
      free(tag);
    } else {
      printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@$class:%s\n", fail_label,
             type->name);
    }
    return;
  }
  printf("JUMPIFNEQ xMATCH$%d LF@tmp1 string@%s\n", fail_label,
         basic_pattern_type(type->data_type));
}

// generates one pattern test
static void gen_pattern_test(Gen *gen, AST_NODE *pattern, int match_id,
                             int fail_label) {
  if (pattern == NULL || pattern->type == NODE_WILDCARD) {
    return;
  }
  char subject[32];
  snprintf(subject, sizeof subject, "LF@match$%d", match_id);
  if (pattern->type == NODE_LITERAL) {
    gen_literal_pattern(gen, pattern, subject, fail_label);
  } else if (pattern->type == NODE_IDENT) {
    gen_type_pattern(gen, pattern->type_ref, subject, fail_label);
    if (pattern->is_binding) {
      printf("MOVE LF@%s$%d %s\n", pattern->name, pattern->id, subject);
    }
  } else if (pattern->type == NODE_CALL) {
    gen_type_pattern(gen, pattern->type_ref, subject, fail_label);
    int field = 1;
    for (AST_NODE *arg = pattern->third; arg != NULL;
         arg = arg->next, field++) {
      if (arg->type == NODE_LITERAL) {
        printf("ARRAYGET LF@tmp3 %s int@%d\n", subject, field);
        gen_literal_pattern(gen, arg, "LF@tmp3", fail_label);
      } else if (arg->type == NODE_IDENT && arg->is_binding) {
        printf("ARRAYGET LF@%s$%d %s int@%d\n", arg->name, arg->id, subject,
               field);
      }
    }
  }
}

// generates filter, takeWhile or dropWhile
static void gen_list_predicate(Gen *gen, AST_NODE *node, int kind) {
  int lbl = new_label(gen);
  gen_expr(gen, node->left);
  puts("CREATEFRAME");
  puts("PUSHFRAME");
  puts("DEFVAR LF@src");
  puts("DEFVAR LF@len");
  puts("DEFVAR LF@i");
  puts("DEFVAR LF@dest");
  puts("DEFVAR LF@count");
  puts("DEFVAR LF@state");
  puts("DEFVAR LF@elem");
  puts("DEFVAR LF@cond");
  puts("DEFVAR LF@scratch");
  puts("POPS LF@src");
  puts("ARRAYGET LF@len LF@src int@1");
  puts("ADD LF@scratch LF@len int@2");
  puts("NEWARRAY LF@dest LF@scratch");
  puts("ARRAYGET LF@scratch LF@src int@0");
  puts("ARRAYSET LF@dest int@0 LF@scratch");
  puts("ARRAYSET LF@dest int@1 int@0");
  puts("MOVE LF@i int@0");
  puts("MOVE LF@count int@0");
  puts("MOVE LF@state int@0");
  printf("LABEL lp$%d$LOOP\n", lbl);
  puts("LT LF@scratch LF@i LF@len");
  printf("JUMPIFEQ lp$%d$END LF@scratch bool@false\n", lbl);
  puts("ADD LF@scratch LF@i int@2");
  puts("ARRAYGET LF@elem LF@src LF@scratch");
  if (kind == 2) {
    printf("JUMPIFEQ lp$%d$APPEND LF@state int@1\n", lbl);
  }
  puts("PUSHS LF@elem");
  printf("CALL $f_%s\n", node->third->name);
  puts("POPS LF@cond");
  if (kind == 0) {
    printf("JUMPIFEQ lp$%d$SKIP LF@cond bool@false\n", lbl);
  } else if (kind == 1) {
    printf("JUMPIFEQ lp$%d$END LF@cond bool@false\n", lbl);
  } else {
    printf("JUMPIFEQ lp$%d$SKIP LF@cond bool@true\n", lbl);
    puts("MOVE LF@state int@1");
  }
  printf("LABEL lp$%d$APPEND\n", lbl);
  puts("ADD LF@scratch LF@count int@2");
  puts("ARRAYSET LF@dest LF@scratch LF@elem");
  puts("ADD LF@count LF@count int@1");
  printf("LABEL lp$%d$SKIP\n", lbl);
  puts("ADD LF@i LF@i int@1");
  printf("JUMP lp$%d$LOOP\n", lbl);
  printf("LABEL lp$%d$END\n", lbl);
  puts("ARRAYSET LF@dest int@1 LF@count");
  puts("PUSHS LF@dest");
  puts("POPFRAME");
}
// generates a match expression
static void gen_match(Gen *gen, AST_NODE *node) {
  int match_id = node->field_index;
  int end_label = new_label(gen);
  gen_expr(gen, node->left);
  printf("POPS LF@match$%d\n", match_id);
  for (AST_NODE *branch = node->right; branch != NULL; branch = branch->next) {
    int next_label = new_label(gen);
    gen_pattern_test(gen, branch->left, match_id, next_label);
    if (branch->right != NULL) {
      gen_expr(gen, branch->right);
      printf("PUSHS bool@false\n");
      printf("JUMPIFEQS xMATCH$%d\n", next_label);
    }
    gen_block_value(gen, branch->third, node->data_type);
    printf("JUMP xMATCH$%d$END\n", end_label);
    printf("LABEL xMATCH$%d\n", next_label);
  }
  printf("LABEL xMATCH$%d$END\n", end_label);
}

// generates code for an expression
void gen_expr(Gen *gen, AST_NODE *node) {
  if (node == NULL) {
    return;
  }
  switch (node->type) {
  case NODE_CALL:
    if (node->is_index) {
      if (node->left != NULL) {
        gen_expr(gen, node->left);
      } else {
        printf("PUSHS LF@%s$%d\n", node->name, node->id);
      }
      gen_expr(gen, node->third);
      printf("CALL $b_list_get\n");
      break;
    }
    if (node->is_field) {
      gen_expr(gen, node->left);
      printf("POPS LF@tmp1\n");
      printf("ARRAYGET LF@tmp2 LF@tmp1 int@%d\n", node->field_index);
      printf("PUSHS LF@tmp2\n");
      break;
    }
    if (node->is_ctor) {
      gen_case_constructor(gen, node);
      break;
    }
    if (node->is_static_call) {
      size_t count = 0;
      for (AST_NODE *arg = node->third; arg != NULL; arg = arg->next) {
        gen_expr(gen, arg);
        count++;
      }
      if (count == 2) {
        printf("PUSHS int@1\n");
      }
      printf("PUSHS int@0\n");
      printf("CALL $b_list_range\n");
      break;
    }
    if (node->is_method && node->left != NULL && node->name != NULL &&
        node->third != NULL && node->third->is_function_ref) {
      if (strcmp(node->name, "filter") == 0) {
        gen_list_predicate(gen, node, 0);
        break;
      }
      if (strcmp(node->name, "takeWhile") == 0) {
        gen_list_predicate(gen, node, 1);
        break;
      }
      if (strcmp(node->name, "dropWhile") == 0) {
        gen_list_predicate(gen, node, 2);
        break;
      }
    }
    /*===============
     *PRINT
     *===============
     *
     *only for constants*/
    if (strcmp(node->name, "print") == 0) {
      if (node->third->type == NODE_LITERAL &&
          node->third->data_type == DATA_UNIT) {
        printf("WRITE string@()\n");
      } else if (node->third->type == NODE_LITERAL)
        literal(node->third, "WRITE");
      // print(x)
      else if (node->third->data_type == DATA_LIST) {
        gen_expr(gen, node->third);
        printf("CALL $b_list_print\n");
      } else if (node->third->data_type == DATA_UNION) {
        gen_expr(gen, node->third);
        printf("CALL $b_print_any\n");
      } else {
        gen_expr(gen, node->third);
        printf("POPS LF@tmp1\n");
        if (node->third->data_type == DATA_UNIT) {
          printf("WRITE string@()\n");
        } else {
          printf("WRITE LF@tmp1\n");
        }
      }
      // only expresion which doesnt return value
      printf("PUSHS nil@nil\n");
    } else if (node->name != NULL && strcmp(node->name, "List") == 0 &&
               !node->is_method && node->data_type == DATA_LIST) {
      gen_list_constructor(gen, node);
    } else if (node->is_method && node->name != NULL &&
               strcmp(node->name, "toList") == 0 && node->left != NULL &&
               (node->left->type == NODE_RANGE ||
                (node->left->type == NODE_BINOP &&
                 node->left->data_type == DATA_LIST))) {
      gen_range_list(gen, node->left);
    } else {

      // receiver of an extension method goes first
      if (node->is_method && node->left != NULL) {
        gen_expr(gen, node->left);
      }

      gen_call_args(gen, node);

      if (node->is_method && node->left != NULL &&
          node->left->data_type == DATA_LIST && node->name != NULL) {
        if (strcmp(node->name, "head") == 0) {
          printf("CALL $b_list_head\n");
        } else if (strcmp(node->name, "tail") == 0) {
          printf("CALL $b_list_tail\n");
        } else if (strcmp(node->name, "drop") == 0) {
          printf("CALL $b_list_drop\n");
        } else if (strcmp(node->name, "take") == 0) {
          printf("CALL $b_list_take\n");
        } else if (strcmp(node->name, "slice") == 0) {
          printf("CALL $b_list_slice\n");
        } else if (strcmp(node->name, "length") == 0) {
          printf("CALL $b_list_length\n");
        } else if (strcmp(node->name, "toString") == 0) {
          printf("CALL $b_list_toStr\n");
        } else if (strcmp(node->name, "toList") == 0) {
          printf("CALL $b_list_toList\n");
        } else {
          printf("CALL $f_%s\n", node->name);
        }
      } else if (is_builtin(node->name)) {
        emit_builtin_call(gen, node);
      } else
        printf("CALL $f_%s\n", node->name);
    }
    break;

  case NODE_IDENT:
    push_to_val(node);
    break;

  case NODE_LITERAL:
    literal(node, "PUSHS");
    break;

  case NODE_UNOP:
    switch (node->op_type) {
    case OP_ADD:
      gen_expr(gen, node->left);
      break;
    case OP_SUB:
      gen_expr(gen, node->left);
      if (node->data_type == DATA_DOUBLE) {
        printf("PUSHS float@%a\n", -1.0);
      } else {
        printf("PUSHS int@-1\n");
      }
      printf("MULS\n");
      break;
    case OP_NOT:
      gen_expr(gen, node->left);
      printf("NOTS\n");
      break;
    default:
      break;
    }
    break;
  case NODE_BINOP:
    if (node->op_type == OP_AND) {
      int i = new_label(gen);

      gen_expr(gen, node->left);
      printf("POPS LF@tmp1\n");
      printf("JUMPIFEQ and$end$%d LF@tmp1 bool@false\n", i);

      gen_expr(gen, node->right);
      printf("POPS LF@tmp1\n");

      printf("LABEL and$end$%d\n", i);
      printf("PUSHS LF@tmp1\n");
      break;
    }
    if (node->op_type == OP_OR) {
      int or_i = new_label(gen);

      gen_expr(gen, node->left);
      printf("POPS LF@tmp1\n");
      printf("JUMPIFEQ or$end$%d LF@tmp1 bool@true\n", or_i);

      gen_expr(gen, node->right);
      printf("POPS LF@tmp1\n");

      printf("LABEL or$end$%d\n", or_i);
      printf("PUSHS LF@tmp1\n");
      break;
    }

    if (node->data_type == DATA_LIST &&
        (node->op_type == OP_TO || node->op_type == OP_UNTIL ||
         node->op_type == OP_BY)) {
      gen_range_list(gen, node);
      break;
    }

    /*
     *checking if one is INT and other is FLOAT
     */
    if ((node->left->data_type == DATA_INT &&
         node->right->data_type == DATA_DOUBLE)) {
      gen_expr(gen, node->left);
      printf("INT2FLOATS\n");
      gen_expr(gen, node->right);
    } else if ((node->left->data_type == DATA_DOUBLE &&
                node->right->data_type == DATA_INT)) {
      gen_expr(gen, node->left);
      gen_expr(gen, node->right);
      printf("INT2FLOATS\n");
    } else {
      gen_expr(gen, node->left);
      gen_expr(gen, node->right);
    }

    switch (node->op_type) {
    case OP_ADD:
      if (node->data_type == DATA_LIST) {
        printf("CALL $b_list_concat\n");
      } else if (node->data_type == DATA_INT || node->data_type == DATA_DOUBLE)
        printf("ADDS\n");
      else if (node->data_type == DATA_STRING) {
        printf("POPS LF@tmp1\n");
        printf("POPS LF@tmp2\n");
        printf("CONCAT LF@tmp3 LF@tmp2 LF@tmp1\n");
        printf("PUSHS LF@tmp3\n");
      }
      break;
    case OP_GT:
      printf("GTS\n");
      break;
    case OP_LT:
      printf("LTS\n");
      break;
    case OP_EQ:
      printf("EQS\n");
      break;
    case OP_LE:
      printf("GTS\n");
      printf("NOTS\n");
      break;
    case OP_GE:
      printf("LTS\n");
      printf("NOTS\n");
      break;
    case OP_DIV:
      node->data_type == DATA_DOUBLE ? printf("DIVS\n") : printf("IDIVS\n");
      break;
    case OP_MUL:
      printf("MULS\n");
      break;
    case OP_SUB:
      printf("SUBS\n");
      break;
    case OP_NOT:
      printf("NOTS\n");
      break;
    case OP_NEQ:
      printf("EQS\n");
      printf("NOTS\n");
      break;

    default:
      break;
    }
    break;

  case NODE_INTERP:
    printf("PUSHS string@\n");
    for (AST_NODE *part = node->left; part; part = part->next) {
      gen_expr(gen, part);
      switch (part->data_type) {
      case DATA_INT:
        printf("CALL $b_toStr_int\n");
        break;
      case DATA_DOUBLE:
        printf("CALL $b_toStr_float\n");
        break;
      case DATA_BOOL:
        printf("CALL $b_toStr_bool\n");
        break;
      case DATA_UNIT:
        printf("POPS LF@tmp1\n");
        printf("PUSHS string@()\n");
        break;
      case DATA_LIST:
        printf("CALL $b_list_toStr\n");
        break;
      case DATA_UNION:
        printf("CALL $b_toStr_any\n");
        break;
      default:
        break;
      }
      printf("POPS LF@tmp1\n");
      printf("POPS LF@tmp2\n");
      printf("CONCAT LF@tmp3 LF@tmp2 LF@tmp1\n");
      printf("PUSHS LF@tmp3\n");
    }
    break;

  case NODE_MATCH:
    gen_match(gen, node);
    break;

  case NODE_RANGE:
    gen_range_list(gen, node);
    break;

  case NODE_IF: {
    int i = new_label(gen);

    // if
    gen_expr(gen, node->left);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS xIF$%d$ELSE\n", i);

    gen_block_value(gen, node->right, node->data_type);
    printf("JUMP xIF$%d$END\n", i);
    printf("LABEL xIF$%d$ELSE\n", i);
    gen_block_value(gen, node->third, node->data_type);
    printf("LABEL xIF$%d$END\n", i);
    break;
  }

  default:
    break;
  }
  if (node->int_to_double) {
    printf("INT2FLOATS\n");
  }
}
