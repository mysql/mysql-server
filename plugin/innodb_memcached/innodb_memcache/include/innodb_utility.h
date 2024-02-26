/***********************************************************************

Copyright (c) 2012, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/
/**************************************************/ /**
 @file innodb_utility.h

 Created 03/15/2011      Jimmy Yang (most macros and defines are adopted
                         from utility files in the InnoDB. Namely
                         ut/ut0lst.h, ut0rnd.h and hash0hash.h etc.)
 *******************************************************/

#ifndef INNODB_UTILITY_H
#define INNODB_UTILITY_H

#include "api0api.h"
#include "config.h"

#define UT_LIST_NODE_T(TYPE)                                      \
  struct {                                                        \
    TYPE *prev; /*!< pointer to the previous node,                \
                NULL if start of list */                          \
    TYPE *next; /*!< pointer to next node, NULL if end of list */ \
  }

#define UT_LIST_BASE_NODE_T(TYPE)                             \
  struct {                                                    \
    int count;   /*!< count of nodes in list */               \
    TYPE *start; /*!< pointer to list start, NULL if empty */ \
    TYPE *end;   /*!< pointer to list end, NULL if empty */   \
  }

/** Some Macros to manipulate the list, extracted from "ut0lst.h" */
#define UT_LIST_INIT(BASE) \
  {                        \
    (BASE).count = 0;      \
    (BASE).start = NULL;   \
    (BASE).end = NULL;     \
  }

#define UT_LIST_ADD_LAST(NAME, BASE, N) \
  {                                     \
    ((BASE).count)++;                   \
    ((N)->NAME).prev = (BASE).end;      \
    ((N)->NAME).next = NULL;            \
    if ((BASE).end != NULL) {           \
      (((BASE).end)->NAME).next = (N);  \
    }                                   \
    (BASE).end = (N);                   \
    if ((BASE).start == NULL) {         \
      (BASE).start = (N);               \
    }                                   \
  }

#define UT_LIST_ADD_FIRST(NAME, BASE, N)     \
  {                                          \
    ((BASE).count)++;                        \
    ((N)->NAME).next = (BASE).start;         \
    ((N)->NAME).prev = NULL;                 \
    if (UNIV_LIKELY((BASE).start != NULL)) { \
      (((BASE).start)->NAME).prev = (N);     \
    }                                        \
    (BASE).start = (N);                      \
    if (UNIV_UNLIKELY((BASE).end == NULL)) { \
      (BASE).end = (N);                      \
    }                                        \
  }

#define UT_LIST_REMOVE_CLEAR(NAME, N) \
  ((N)->NAME.prev = (N)->NAME.next =  \
       reinterpret_cast<decltype((N)->NAME.next)>(-1))

/** Removes a node from a linked list. */
#define UT_LIST_REMOVE(NAME, BASE, N)                     \
  do {                                                    \
    ((BASE).count)--;                                     \
    if (((N)->NAME).next != NULL) {                       \
      ((((N)->NAME).next)->NAME).prev = ((N)->NAME).prev; \
    } else {                                              \
      (BASE).end = ((N)->NAME).prev;                      \
    }                                                     \
    if (((N)->NAME).prev != NULL) {                       \
      ((((N)->NAME).prev)->NAME).next = ((N)->NAME).next; \
    } else {                                              \
      (BASE).start = ((N)->NAME).next;                    \
    }                                                     \
    UT_LIST_REMOVE_CLEAR(NAME, N);                        \
  } while (0)

#define UT_LIST_GET_NEXT(NAME, N) (((N)->NAME).next)

#define UT_LIST_GET_LEN(BASE) (BASE).count

#define UT_LIST_GET_FIRST(BASE) (BASE).start

/*************************************************************/ /**
 Folds a character string ending in the null character.
 Extracted from ut0rnd.h and ut0rnd.ic.
 @return folded value */

uint64_t ut_fold_string(
    /*===========*/
    const char *str); /*!< in: null-terminated string */

typedef struct hash_cell_struct {
  void *node; /*!< hash chain node, NULL if none */
} hash_cell_t;

typedef struct hash_table_struct {
  uint64_t n_cells;   /* number of cells in the hash table */
  hash_cell_t *array; /*!< pointer to cell array */
} hash_table_t;

#define HASH_INSERT(TYPE, NAME, TABLE, FOLD, DATA)                    \
  do {                                                                \
    hash_cell_t *cell3333;                                            \
    TYPE *struct3333;                                                 \
                                                                      \
    (DATA)->NAME = NULL;                                              \
                                                                      \
    cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE)); \
                                                                      \
    if (cell3333->node == NULL) {                                     \
      cell3333->node = DATA;                                          \
    } else {                                                          \
      struct3333 = (TYPE *)cell3333->node;                            \
                                                                      \
      while (struct3333->NAME != NULL) {                              \
        struct3333 = (TYPE *)struct3333->NAME;                        \
      }                                                               \
                                                                      \
      struct3333->NAME = DATA;                                        \
    }                                                                 \
  } while (0)

/*******************************************************************/ /**
 Gets the first struct in a hash chain, NULL if none. */

#define HASH_GET_FIRST(TABLE, HASH_VAL) \
  (hash_get_nth_cell(TABLE, HASH_VAL)->node)

/*******************************************************************/ /**
 Gets the next struct in a hash chain, NULL if none. */

#define HASH_GET_NEXT(NAME, DATA) ((DATA)->NAME)

/********************************************************************/ /**
 Looks for a struct in a hash table. */
#define HASH_SEARCH(NAME, TABLE, FOLD, TYPE, DATA, TEST)               \
  {                                                                    \
    (DATA) = (TYPE)HASH_GET_FIRST(TABLE, hash_calc_hash(FOLD, TABLE)); \
                                                                       \
    while ((DATA) != NULL) {                                           \
      if (TEST) {                                                      \
        break;                                                         \
      } else {                                                         \
        (DATA) = (TYPE)HASH_GET_NEXT(NAME, DATA);                      \
      }                                                                \
    }                                                                  \
  }

/********************************************************************/ /**
 Cleanup items in a hash table */
#define HASH_CLEANUP(TABLE, TYPE)                         \
  do {                                                    \
    uint64_t i;                                           \
    TYPE data;                                            \
                                                          \
    for (i = 0; i < TABLE->n_cells; i++) {                \
      data = (TYPE)HASH_GET_FIRST(TABLE, i);              \
                                                          \
      while (data) {                                      \
        TYPE next_data;                                   \
        next_data = (TYPE)HASH_GET_NEXT(name_hash, data); \
        innodb_config_free(data);                         \
        free(data);                                       \
        data = next_data;                                 \
      }                                                   \
    }                                                     \
                                                          \
    free(TABLE->array);                                   \
    free(TABLE);                                          \
  } while (0)

/************************************************************/ /**
 Gets the nth cell in a hash table.
 @return pointer to cell */
hash_cell_t *hash_get_nth_cell(
    /*==============*/
    hash_table_t *table, /*!< in: hash table */
    uint64_t n);         /*!< in: cell index */

/*************************************************************/ /**
 Creates a hash table with >= n array cells. The actual number
 of cells is chosen to be a prime number slightly bigger than n.
 @return own: created table */
hash_table_t *hash_create(
    /*========*/
    uint64_t n); /*!< in: number of array cells */

/**************************************************************/ /**
 Calculates the hash value from a folded value.
 @return hashed value */
uint64_t hash_calc_hash(
    /*===========*/
    uint64_t fold,        /*!< in: folded value */
    hash_table_t *table); /*!< in: hash table */

#endif /* INNODB_UTILITY_H */
