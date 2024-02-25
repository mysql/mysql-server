/*****************************************************************************

Copyright (c) 2007, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/fts0ast.h
 The FTS query parser (AST) abstract syntax tree routines

 Created 2007/03/16/03 Sunny Bains
 *******************************************************/

#ifndef INNOBASE_FST0AST_H
#define INNOBASE_FST0AST_H

#include "ha_prototypes.h"
#include "mem0mem.h"

/* The type of AST Node */
enum fts_ast_type_t {
  FTS_AST_OPER,               /*!< Operator */
  FTS_AST_NUMB,               /*!< Number */
  FTS_AST_TERM,               /*!< Term (or word) */
  FTS_AST_TEXT,               /*!< Text string */
  FTS_AST_PARSER_PHRASE_LIST, /*!< Phase for plugin parser
                              The difference from text type
                              is that we tokenize text into
                              term list */
  FTS_AST_LIST,               /*!< Expression list */
  FTS_AST_SUBEXP_LIST         /*!< Sub-Expression list */
};

/* The FTS query operators that we support */
enum fts_ast_oper_t {
  FTS_NONE, /*!< No operator */

  FTS_IGNORE, /*!< Ignore rows that contain
              this word */

  FTS_EXIST, /*!< Include rows that contain
             this word */

  FTS_NEGATE, /*!< Include rows that contain
              this word but rank them
              lower*/

  FTS_INCR_RATING, /*!< Increase the rank for this
                   word*/

  FTS_DECR_RATING, /*!< Decrease the rank for this
                   word*/

  FTS_DISTANCE,    /*!< Proximity distance */
  FTS_IGNORE_SKIP, /*!< Transient node operator
                   signifies that this is a
                   FTS_IGNORE node, and ignored in
                   the first pass of
                   fts_ast_visit() */
  FTS_EXIST_SKIP   /*!< Transient node operator
                   signifies that this ia a
                   FTS_EXIST node, and ignored in
                   the first pass of
                   fts_ast_visit() */
};

/* Data types used by the FTS parser */
struct fts_lexer_t;
struct fts_ast_node_t;
struct fts_ast_state_t;
struct fts_ast_string_t;

typedef dberr_t (*fts_ast_callback)(fts_ast_oper_t, fts_ast_node_t *, void *);

/********************************************************************
Parse the string using the lexer setup within state.*/
int fts_parse(
    /* out: 0 on OK, 1 on error */
    fts_ast_state_t *state); /*!< in: ast state instance.*/

/********************************************************************
Create an AST operator node */
extern fts_ast_node_t *fts_ast_create_node_oper(
    void *arg,            /*!< in: ast state */
    fts_ast_oper_t oper); /*!< in: ast operator */
/********************************************************************
Create an AST term node, makes a copy of ptr */
extern fts_ast_node_t *fts_ast_create_node_term(
    void *arg,                    /*!< in: ast state */
    const fts_ast_string_t *ptr); /*!< in: term string */
/********************************************************************
Create an AST text node */
extern fts_ast_node_t *fts_ast_create_node_text(
    void *arg,                    /*!< in: ast state */
    const fts_ast_string_t *ptr); /*!< in: text string */
/********************************************************************
Create an AST expr list node */
extern fts_ast_node_t *fts_ast_create_node_list(
    void *arg,             /*!< in: ast state */
    fts_ast_node_t *expr); /*!< in: ast expr */
/********************************************************************
Create a sub-expression list node. This function takes ownership of
expr and is responsible for deleting it. */
extern fts_ast_node_t *fts_ast_create_node_subexp_list(
    /* out: new node */
    void *arg,             /*!< in: ast state instance */
    fts_ast_node_t *expr); /*!< in: ast expr instance */
/********************************************************************
Set the wildcard attribute of a term.*/
extern void fts_ast_term_set_wildcard(
    fts_ast_node_t *node); /*!< in: term to change */
/********************************************************************
Set the proximity attribute of a text node. */
void fts_ast_text_set_distance(fts_ast_node_t *node, /*!< in/out: text node */
                               ulint distance);      /*!< in: the text proximity
                                                     distance */
/** Free a fts_ast_node_t instance.
 @return next node to free */
fts_ast_node_t *fts_ast_free_node(
    fts_ast_node_t *node); /*!< in: node to free */
/********************************************************************
Add a sub-expression to an AST*/
extern fts_ast_node_t *fts_ast_add_node(
    fts_ast_node_t *list,  /*!< in: list node instance */
    fts_ast_node_t *node); /*!< in: (sub) expr to add */
/********************************************************************
Print the AST node recursively.*/
extern void fts_ast_node_print(
    fts_ast_node_t *node); /*!< in: ast node to print */
/********************************************************************
Free node and expr allocations.*/
extern void fts_ast_state_free(fts_ast_state_t *state); /*!< in: state instance
                                                        to free */
/** Check only union operation involved in the node
@param[in]      node    ast node to check
@return true if the node contains only union else false. */
bool fts_ast_node_check_union(fts_ast_node_t *node);

/** Traverse the AST - in-order traversal.
 @return DB_SUCCESS if all went well */
[[nodiscard]] dberr_t fts_ast_visit(
    fts_ast_oper_t oper,      /*!< in: FTS operator */
    fts_ast_node_t *node,     /*!< in: instance to traverse*/
    fts_ast_callback visitor, /*!< in: callback */
    void *arg,                /*!< in: callback arg */
    bool *has_ignore);        /*!< out: whether we encounter
                             and ignored processing an
                             operator, currently we only
                             ignore FTS_IGNORE operator */
/********************************************************************
Create a lex instance.*/
[[nodiscard]] fts_lexer_t *fts_lexer_create(
    bool boolean_mode, /*!< in: query type */
    const byte *query, /*!< in: query string */
    ulint query_len)   /*!< in: query string len */
    MY_ATTRIBUTE((malloc));
/********************************************************************
Free an fts_lexer_t instance.*/
void fts_lexer_free(fts_lexer_t *fts_lexer); /*!< in: lexer instance to
                                             free */

/**
Create an ast string object, with NUL-terminator, so the string
has one more byte than len
@param[in] str          pointer to string
@param[in] len          length of the string
@return ast string with NUL-terminator */
fts_ast_string_t *fts_ast_string_create(const byte *str, ulint len);

/**
Free an ast string instance
@param[in,out] ast_str  string to free */
void fts_ast_string_free(fts_ast_string_t *ast_str);

/**
Translate ast string of type FTS_AST_NUMB to unsigned long by strtoul
@param[in] ast_str              string to translate
@param[in] base         the base
@return translated number */
ulint fts_ast_string_to_ul(const fts_ast_string_t *ast_str, int base);

/* String of length len.
We always store the string of length len with a terminating '\0',
regardless of there is any 0x00 in the string itself */
struct fts_ast_string_t {
  /*!< Pointer to string. */
  byte *str;

  /*!< Length of the string. */
  ulint len;
};

/* Query term type */
struct fts_ast_term_t {
  fts_ast_string_t *ptr; /*!< Pointer to term string.*/
  bool wildcard;         /*!< true if wild card set.*/
};

/* Query text type */
struct fts_ast_text_t {
  fts_ast_string_t *ptr; /*!< Pointer to text string.*/
  ulint distance;        /*!< > 0 if proximity distance
                         set */
};

/* The list of nodes in an expr list */
struct fts_ast_list_t {
  fts_ast_node_t *head; /*!< Children list head */
  fts_ast_node_t *tail; /*!< Children list tail */
};

/* FTS AST node to store the term, text, operator and sub-expressions.*/
struct fts_ast_node_t {
  fts_ast_type_t type;        /*!< The type of node */
  fts_ast_text_t text;        /*!< Text node */
  fts_ast_term_t term;        /*!< Term node */
  fts_ast_oper_t oper;        /*!< Operator value */
  fts_ast_list_t list;        /*!< Expression list */
  fts_ast_node_t *next;       /*!< Link for expr list */
  fts_ast_node_t *next_alloc; /*!< For tracking allocations */
  bool visited;               /*!< whether this node is
                              already processed */
  trx_t *trx;
  /* Used by plugin parser */
  fts_ast_node_t *up_node; /*!< Direct up node */
  bool go_up;              /*!< Flag if go one level up */
};

/* To track state during parsing */
struct fts_ast_state_t {
  mem_heap_t *heap;     /*!< Heap to use for alloc */
  fts_ast_node_t *root; /*!< If all goes OK, then this
                        will point to the root.*/

  fts_ast_list_t list; /*!< List of nodes allocated */

  fts_lexer_t *lexer;    /*!< Lexer callback + arg */
  CHARSET_INFO *charset; /*!< charset used for
                         tokenization */
  /* Used by plugin parser */
  fts_ast_node_t *cur_node; /*!< Current node into which
                             we add new node */
  int depth;                /*!< Depth of parsing state */
};

/** Create an AST term node, makes a copy of ptr for plugin parser
 @return node */
extern fts_ast_node_t *fts_ast_create_node_term_for_parser(
    /*==========i=====================*/
    void *arg,        /*!< in: ast state */
    const char *ptr,  /*!< in: term string */
    const ulint len); /*!< in: term string length */

/** Create an AST phrase list node for plugin parser
 @return node */
extern fts_ast_node_t *fts_ast_create_node_phrase_list(
    void *arg); /*!< in: ast state */

#ifdef UNIV_DEBUG
const char *fts_ast_node_type_get(fts_ast_type_t type);
#endif /* UNIV_DEBUG */

#endif /* INNOBASE_FSTS0AST_H */
