/*****************************************************************************

Copyright (c) 1994, 2021, Oracle and/or its affiliates.

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

/** @file include/ha0ha.h
 The hash table with external chains

 Created 8/18/1994 Heikki Tuuri
 *******************************************************/

#ifndef ha0ha_h
#define ha0ha_h

#include "univ.i"

#include "buf0types.h"
#include "hash0hash.h"
#include "page0types.h"
#include "rem0types.h"

/** Looks for an element in a hash table.
@param[in]	table	hash table
@param[in]	fold	folded value of the searched data
@return pointer to the data of the first hash table node in chain
having the fold number, NULL if not found */
static inline const rec_t *ha_search_and_get_data(hash_table_t *table,
                                                  ulint fold);

/** Looks for an element when we know the pointer to the data and updates
 the pointer to data if found.
 @return true if found */
ibool ha_search_and_update_if_found_func(
    hash_table_t *table, /*!< in/out: hash table */
    ulint fold,          /*!< in: folded value of the searched data */
    const rec_t *data,   /*!< in: pointer to the data */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    buf_block_t *new_block, /*!< in: block containing new_data */
#endif                      /* UNIV_AHI_DEBUG || UNIV_DEBUG */
    const rec_t *new_data); /*!< in: new pointer to the data */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Looks for an element when we know the pointer to the data and
updates the pointer to data if found.
@param table in/out: hash table
@param fold in: folded value of the searched data
@param data in: pointer to the data
@param new_block in: block containing new_data
@param new_data in: new pointer to the data */
#define ha_search_and_update_if_found(table, fold, data, new_block, new_data) \
  ha_search_and_update_if_found_func(table, fold, data, new_block, new_data)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/** Looks for an element when we know the pointer to the data and
updates the pointer to data if found.
@param table in/out: hash table
@param fold in: folded value of the searched data
@param data in: pointer to the data
@param new_block ignored: block containing new_data
@param new_data in: new pointer to the data */
#define ha_search_and_update_if_found(table, fold, data, new_block, new_data) \
  ha_search_and_update_if_found_func(table, fold, data, new_data)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/** Creates a hash table with at least n array cells.  The actual number
 of cells is chosen to be a prime number slightly bigger than n.
 @param[in] n           number of array cells
 @param[in] id          latch ID
 @param[in] n_sync_obj  Number of sync objects protecting the hash table.
                        Must be a power of 2, or 0.
 @param[in] type        type of datastructure for which the memory heap is going
                        to be used:
                        MEM_HEAP_FOR_BTR_SEARCH or
                        MEM_HEAP_FOR_PAGE_HASH
 @return own: created table */
hash_table_t *ib_create(ulint n, latch_id_t id, ulint n_sync_obj, ulint type);

/** Empties a hash table and frees the memory heaps. */
void ha_clear(hash_table_t *table); /*!< in, own: hash table */

/** Inserts an entry into a hash table. If an entry with the same fold number
 is found, its node is updated to point to the new data, and no new node
 is inserted.
 @return true if succeed, false if no more memory could be allocated */
ibool ha_insert_for_fold_func(
    hash_table_t *table, /*!< in: hash table */
    ulint fold,          /*!< in: folded value of data; if a node with
                         the same fold value already exists, it is
                         updated to point to the same data, and no new
                         node is created! */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    buf_block_t *block, /*!< in: buffer block containing the data */
#endif                  /* UNIV_AHI_DEBUG || UNIV_DEBUG */
    const rec_t *data); /*!< in: data, must not be NULL */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted.
@return true if succeed, false if no more memory could be allocated
@param t in: hash table
@param f in: folded value of data
@param b in: buffer block containing the data
@param d in: data, must not be NULL */
#define ha_insert_for_fold(t, f, b, d)            \
  do {                                            \
    ha_insert_for_fold_func(t, f, b, d);          \
    MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_ADDED); \
  } while (0)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted.
@return true if succeed, false if no more memory could be allocated
@param t in: hash table
@param f in: folded value of data
@param b ignored: buffer block containing the data
@param d in: data, must not be NULL */
#define ha_insert_for_fold(t, f, b, d)            \
  do {                                            \
    ha_insert_for_fold_func(t, f, d);             \
    MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_ADDED); \
  } while (0)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/** Looks for an element when we know the pointer to the data and deletes it
from the hash table if found.
@param[in]	table	hash table
@param[in]	fold	folded value of the searched data
@param[in]	data	pointer to the data
@return true if found */
static inline ibool ha_search_and_delete_if_found(hash_table_t *table,
                                                  ulint fold,
                                                  const rec_t *data);

#ifndef UNIV_HOTBACKUP

/** Removes from the chain determined by fold all nodes whose data pointer
 points to the page given.
@param[in] table Hash table
@param[in] fold Fold value
@param[in] page Buffer page */
void ha_remove_all_nodes_to_page(hash_table_t *table, ulint fold,
                                 const page_t *page);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Validates a given range of the cells in hash table.
 @return true if ok */
ibool ha_validate(hash_table_t *table, /*!< in: hash table */
                  ulint start_index,   /*!< in: start index */
                  ulint end_index);    /*!< in: end index */
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */

/** Prints info of a hash table.
@param[in] file File where to print
@param[in] table Hash table */
void ha_print_info(FILE *file, hash_table_t *table);

#endif /* !UNIV_HOTBACKUP */

/** The hash table external chain node */
struct ha_node_t {
  ulint fold;      /*!< fold value for the data */
  ha_node_t *next; /*!< next chain node or NULL if none */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  buf_block_t *block; /*!< buffer block containing the data, or NULL */
#endif                /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  const rec_t *data;  /*!< pointer to the data */
};

#include "ha0ha.ic"

#endif
