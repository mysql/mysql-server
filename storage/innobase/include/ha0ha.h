/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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
@param[in]      table       hash table
@param[in]      hash_value  hashed value of the searched data
@return pointer to the data of the first hash table node in chain
having the hash number, NULL if not found */
static inline const rec_t *ha_search_and_get_data(hash_table_t *table,
                                                  uint64_t hash_value);

/** Looks for an element when we know the pointer to the data and updates
 the pointer to data if found.
 @param[in,out] table hash table
 @param[in] hash_value hashed value of the searched data
 @param[in] data pointer to the data
 @param[in] new_block block containing new data
 @param[in] new_data new pointer to the data
 @return true if found */
bool ha_search_and_update_if_found_func(hash_table_t *table,
                                        uint64_t hash_value, const rec_t *data,
                                        IF_AHI_DEBUG(buf_block_t *new_block, )
                                            const rec_t *new_data);

/** Looks for an element when we know the pointer to the data and
updates the pointer to data if found.
@param table in/out: hash table
@param hash_value in: hashed value of the searched data
@param data in: pointer to the data
@param new_block in: block containing new_data
@param new_data in: new pointer to the data */
static inline bool ha_search_and_update_if_found(hash_table_t *table,
                                                 uint64_t hash_value,
                                                 const rec_t *data,
                                                 buf_block_t *new_block,
                                                 const rec_t *new_data) {
  return ha_search_and_update_if_found_func(table, hash_value, data,
                                            IF_AHI_DEBUG(new_block, ) new_data);
}
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
hash_table_t *ib_create(size_t n, latch_id_t id, size_t n_sync_obj,
                        uint32_t type);

/** Empties a hash table and frees the memory heaps. */
void ha_clear(hash_table_t *table); /*!< in, own: hash table */

/** Inserts an entry into a hash table. If an entry with the same hash number
 is found, its node is updated to point to the new data, and no new node
 is inserted. If btr_search_enabled is set to false, we will only allow
 updating existing nodes, but no new node is allowed to be added.
 @param[in] table hash table
 @param[in] hash_value hashed value of data; if a node with the same hash value
 already exists, it is updated to point to the same data, and no new node is
 created!
 @param[in] block buffer block containing the data
 @param[in] data data, must not be NULL
 @return true if succeeded, false if no more memory could be allocated */
bool ha_insert_for_hash_func(hash_table_t *table, uint64_t hash_value,
                             IF_AHI_DEBUG(buf_block_t *block, )
                                 const rec_t *data);

/**
Inserts an entry into a hash table. If an entry with the same hash number
is found, its node is updated to point to the new data, and no new node
is inserted.
@param[in] t  hash table
@param[in] f  hashed value of data
@param[in] b  buffer block containing the data
@param[in] d  data, must not be NULL */
static inline void ha_insert_for_hash(hash_table_t *t, uint64_t f,
                                      buf_block_t *b, const rec_t *d) {
  ha_insert_for_hash_func(t, f, IF_AHI_DEBUG(b, ) d);
  MONITOR_ATOMIC_INC(MONITOR_ADAPTIVE_HASH_ROW_ADDED);
}

/** Looks for an element when we know the pointer to the data and deletes it
from the hash table if found.
@param[in]      table       hash table
@param[in]      hash_value  hashed value of the searched data
@param[in]      data        pointer to the data
@return true if found */
static inline bool ha_search_and_delete_if_found(hash_table_t *table,
                                                 uint64_t hash_value,
                                                 const rec_t *data);

#ifndef UNIV_HOTBACKUP

/** Removes from the chain determined by hash value a single node whose data
pointer points to the page given. Note that the node deleted can have a
different hash value.
@param[in] table Hash table
@param[in] hash_value Hash value
@param[in] page Buffer page */
void ha_remove_a_node_to_page(hash_table_t *table, uint64_t hash_value,
                              const page_t *page);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Validates a given range of the cells in hash table.
@param[in] table Hash table
@param[in] start_index Start index
@param[in] end_index End index
@return true if ok */
bool ha_validate(hash_table_t *table, uint64_t start_index, uint64_t end_index);
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */

/** Prints info of a hash table.
@param[in] file File where to print
@param[in] table Hash table */
void ha_print_info(FILE *file, hash_table_t *table);

#endif /* !UNIV_HOTBACKUP */

/** The hash table external chain node */
struct ha_node_t {
  /** hash value for the data  */
  uint64_t hash_value;
  /** next chain node or NULL if none */
  ha_node_t *next;
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  /** buffer block containing the data, or NULL */
  buf_block_t *block;
#endif
  /** pointer to the data */
  const rec_t *data;
};

#include "ha0ha.ic"

#endif
