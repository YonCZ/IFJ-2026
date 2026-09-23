/*
 * IFJ26 Compiler Implementation
 *
 * File: parser.h
 * Module: Syntax & Semantic Analyzer (Parser)
 * Author: Tadeáš Přikryl (xprikrt00)
 * Date: September 2026
 */

#ifndef PARSER_H
#define PARSER_H

/* ── Top-level ─────────────────────────────────────────── */
int  parse_program(void);

/* ── Prolog ────────────────────────────────────────────── */
int  parse_prolog(void);

/* ── Function definitions ──────────────────────────────── */
int  parse_function_def(void);
int  parse_param_list(void);

/* ── Statements (LL / recursive descent) ───────────────── */
int  parse_statement(void);
int  parse_var_def(void);
int  parse_assignment(void);
int  parse_call_stmt(void);
int  parse_return(void);
int  parse_if(void);
int  parse_while(void);

/* ── Block / indentation ───────────────────────────────── */
int  parse_block(void);

/* ── Expressions (precedence analysis) ─────────────────── */
int  parse_expression(void);

#endif