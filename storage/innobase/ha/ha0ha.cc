/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file ha/ha0ha.cc
 The hash table with external chains

 Created 8/22/1994 Heikki Tuuri
 *************************************************************************/

#include "ha0ha.h"

#include <sys/types.h>

#include "my_inttypes.h"

#ifdef UNIV_DEBUG
#include "buf0buf.h"
#endif /* UNIV_DEBUG */
#include "btr0sea.h"
#include "page0page.h"

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Maximum number of records in a page */
static const ulint MAX_N_POINTERS = UNIV_PAGE_SIZE_MAX / REC_N_NEW_EXTRA_BYTES;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/** Creates a hash table with at least n array cells.  The actual number
 of cells is chosen to be a prime number slightly bigger than n.
 @return own: created table */
hash_table_t *ib_create(ulint n,       /*!< in: number of array cells */
                        latch_id_t id, /*!< in: latch ID */
                        ulint n_sync_obj,
                        /*!< in: number of mutexes to protect the
                        hash table: must be a power of 2, or 0 */
                        ulint type) /*!< in: type of datastructure for which
                                    MEM_HEAP_FOR_PAGE_HASH */
{
  hash_table_t *table;

  ut_a(type == MEM_HEAP_FOR_BTR_SEARCH || type == MEM_HEAP_FOR_PAGE_HASH);

  ut_ad(ut_is_2pow(n_sync_obj));
  table = hash_create(n);

  /* Creating MEM_HEAP_BTR_SEARCH type heaps can potentially fail,
  but in practise it never should in this case, hence the asserts. */

  if (n_sync_obj == 0) {
    table->heap = mem_heap_create_typed(
        ut_min(static_cast<ulint>(4096), MEM_MAX_ALLOC_IN_BUF / 2 -
                                             MEM_BLOCK_HEADER_SIZE -
                                             MEM_SPACE_NEEDED(0)),
        type);
    ut_a(table->heap);

    return (table);
  }

  if (type == MEM_HEAP_FOR_PAGE_HASH) {
    /* We create a hash table protected by rw_locks for
    buf_pool->page_hash. */
    hash_create_sync_obj(table, HASH_TABLE_SYNC_RW_LOCK, id, n_sync_obj);
  } else {
    hash_create_sync_obj(table, HASH_TABLE_SYNC_MUTEX, id, n_sync_obj);
  }

  table->heaps =
      static_cast<mem_heap_t **>(ut_malloc_nokey(n_sync_obj * sizeof(void *)));

  for (ulint i = 0; i < n_sync_obj; i++) {
    table->heaps[i] = mem_heap_create_typed(
        ut_min(static_cast<ulint>(4096), MEM_MAX_ALLOC_IN_BUF / 2 -
                                             MEM_BLOCK_HEADER_SIZE -
                                             MEM_SPACE_NEEDED(0)),
        type);
    ut_a(table->heaps[i]);
  }

  return (table);
}

/** Recreate a hash table with at least n array cells. The actual number
of cells is chosen to be a prime number slightly bigger than n.
The new cells are all cleared. The heaps are recreated.
The sync objects are reused.
@param[in,out]	table	hash table to be resuzed (to be freed later)
@param[in]	n	number of array cells
@return	resized new table */
hash_table_t *ib_recreate(hash_table_t *table, ulint n) {
  /* This function is for only page_hash for now */
  ut_ad(table->type == HASH_TABLE_SYNC_RW_LOCK);
  ut_ad(table->n_sync_obj > 0);

  hash_table_t *new_table = hash_create(n);

  new_table->type = table->type;
  new_table->n_sync_obj = table->n_sync_obj;
  new_table->sync_obj = table->sync_obj;

  for (ulint i = 0; i < table->n_sync_obj; i++) {
    mem_heap_free(table->heaps[i]);
  }
  ut_free(table->heaps);

  new_table->heaps = static_cast<mem_heap_t **>(
      ut_malloc_nokey(new_table->n_sync_obj * sizeof(void *)));

  for (ulint i = 0; i < new_table->n_sync_obj; i++) {
    new_table->heaps[i] = mem_heap_create_typed(
        ut_min(static_cast<ulint>(4096), MEM_MAX_ALLOC_IN_BUF / 2 -
                                             MEM_BLOCK_HEADER_SIZE -
                                             MEM_SPACE_NEEDED(0)),
        MEM_HEAP_FOR_PAGE_HASH);
    ut_a(new_table->heaps[i]);
  }

  return (new_table);
}

/** Empties a hash table and frees the memory heaps. */
void ha_clear(hash_table_t *table) /*!< in, own: hash table */
{
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
  ut_ad(!table->adaptive || btr_search_own_all(RW_LOCK_X));

  for (ulint i = 0; i < table->n_sync_obj; i++) {
    mem_heap_free(table->heaps[i]);
  }

  ut_free(table->heaps);

  switch (table->type) {
    case HASH_TABLE_SYNC_MUTEX:
      for (ulint i = 0; i < table->n_sync_obj; ++i) {
        mutex_destroy(&table->sync_obj.mutexes[i]);
      }
      ut_free(table->sync_obj.mutexes);
      table->sync_obj.mutexes = NULL;
      break;

    case HASH_TABLE_SYNC_RW_LOCK:
      for (ulint i = 0; i < table->n_sync_obj; ++i) {
        rw_lock_free(&table->sync_obj.rw_locks[i]);
      }

      ut_free(table->sync_obj.rw_locks);
      table->sync_obj.rw_locks = NULL;
      break;

    case HASH_TABLE_SYNC_NONE:
      /* do nothing */
      break;
  }

  table->n_sync_obj = 0;
  table->type = HASH_TABLE_SYNC_NONE;

  /* Clear the hash table. */
  ulint n = hash_get_n_cells(table);

  for (ulint i = 0; i < n; i++) {
    hash_get_nth_cell(table, i)->node = NULL;
  }
}

/** Inserts an entry into a hash table. If an entry with the same fold number
 is found, its node is updated to point to the new data, and no new node
 is inserted. If btr_search_enabled is set to FALSE, we will only allow
 updating existing nodes, but no new node is allowed to be added.
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
    const rec_t *data)  /*!< in: data, must not be NULL */
{
  hash_cell_t *cell;
  ha_node_t *node;
  ha_node_t *prev_node;
  ulint hash;

  ut_ad(data);
  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  ut_a(block->frame == page_align(data));
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  hash_assert_can_modify(table, fold);
  ut_ad(btr_search_enabled);

  hash = hash_calc_hash(fold, table);

  cell = hash_get_nth_cell(table, hash);

  prev_node = static_cast<ha_node_t *>(cell->node);

  while (prev_node != NULL) {
    if (prev_node->fold == fold) {
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
      if (table->adaptive) {
        buf_block_t *prev_block = prev_node->block;
        ut_a(prev_block->frame == page_align(prev_node->data));
        ut_a(os_atomic_decrement_ulint(&prev_block->n_pointers, 1) <
             MAX_N_POINTERS);
        ut_a(os_atomic_increment_ulint(&block->n_pointers, 1) < MAX_N_POINTERS);
      }

      prev_node->block = block;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
      prev_node->data = data;

      return (TRUE);
    }

    prev_node = prev_node->next;
  }

  /* We have to allocate a new chain node */

  node = static_cast<ha_node_t *>(
      mem_heap_alloc(hash_get_heap(table, fold), sizeof(ha_node_t)));

  if (node == NULL) {
    /* It was a btr search type memory heap and at the moment
    no more memory could be allocated: return */

    ut_ad(hash_get_heap(table, fold)->type & MEM_HEAP_BTR_SEARCH);

    return (FALSE);
  }

  ha_node_set_data(node, block, data);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  if (table->adaptive) {
    ut_a(os_atomic_increment_ulint(&block->n_pointers, 1) < MAX_N_POINTERS);
  }
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

  node->fold = fold;

  node->next = NULL;

  prev_node = static_cast<ha_node_t *>(cell->node);

  if (prev_node == NULL) {
    cell->node = node;

    return (TRUE);
  }

  while (prev_node->next != NULL) {
    prev_node = prev_node->next;
  }

  prev_node->next = node;

  return (TRUE);
}

#ifdef UNIV_DEBUG
/** Verify if latch corresponding to the hash table is x-latched
@param[in]	table		hash table */
static void ha_btr_search_latch_x_locked(const hash_table_t *table) {
  ulint i;
  for (i = 0; i < btr_ahi_parts; ++i) {
    if (btr_search_sys->hash_tables[i] == table) {
      break;
    }
  }

  ut_ad(i < btr_ahi_parts);
  ut_ad(rw_lock_own(btr_search_latches[i], RW_LOCK_X));
}
#endif /* UNIV_DEBUG */

/** Deletes a hash node. */
void ha_delete_hash_node(hash_table_t *table, /*!< in: hash table */
                         ha_node_t *del_node) /*!< in: node to be deleted */
{
  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
  ut_d(ha_btr_search_latch_x_locked(table));
  ut_ad(btr_search_enabled);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  if (table->adaptive) {
    ut_a(del_node->block->frame = page_align(del_node->data));
    ut_a(os_atomic_decrement_ulint(&del_node->block->n_pointers, 1) <
         MAX_N_POINTERS);
  }
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

  HASH_DELETE_AND_COMPACT(ha_node_t, next, table, del_node);
}

/** Looks for an element when we know the pointer to the data, and updates
 the pointer to data, if found.
 @return true if found */
ibool ha_search_and_update_if_found_func(
    hash_table_t *table, /*!< in/out: hash table */
    ulint fold,          /*!< in: folded value of the searched data */
    const rec_t *data,   /*!< in: pointer to the data */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    buf_block_t *new_block, /*!< in: block containing new_data */
#endif                      /* UNIV_AHI_DEBUG || UNIV_DEBUG */
    const rec_t *new_data)  /*!< in: new pointer to the data */
{
  ha_node_t *node;

  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
  hash_assert_can_modify(table, fold);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  ut_a(new_block->frame == page_align(new_data));
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

  ut_d(ha_btr_search_latch_x_locked(table));

  if (!btr_search_enabled) {
    return (FALSE);
  }

  node = ha_search_with_data(table, fold, data);

  if (node) {
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    if (table->adaptive) {
      ut_a(os_atomic_decrement_ulint(&node->block->n_pointers, 1) <
           MAX_N_POINTERS);
      ut_a(os_atomic_increment_ulint(&new_block->n_pointers, 1) <
           MAX_N_POINTERS);
    }

    node->block = new_block;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
    node->data = new_data;

    return (TRUE);
  }

  return (FALSE);
}

/** Removes from the chain determined by fold all nodes whose data pointer
 points to the page given. */
void ha_remove_all_nodes_to_page(hash_table_t *table, /*!< in: hash table */
                                 ulint fold,          /*!< in: fold value */
                                 const page_t *page)  /*!< in: buffer page */
{
  ha_node_t *node;

  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
  hash_assert_can_modify(table, fold);
  ut_ad(btr_search_enabled);

  node = ha_chain_get_first(table, fold);

  while (node) {
    if (page_align(ha_node_get_data(node)) == page) {
      /* Remove the hash node */

      ha_delete_hash_node(table, node);

      /* Start again from the first node in the chain
      because the deletion may compact the heap of
      nodes and move other nodes! */

      node = ha_chain_get_first(table, fold);
    } else {
      node = ha_chain_get_next(node);
    }
  }
#ifdef UNIV_DEBUG
  /* Check that all nodes really got deleted */

  node = ha_chain_get_first(table, fold);

  while (node) {
    ut_a(page_align(ha_node_get_data(node)) != page);

    node = ha_chain_get_next(node);
  }
#endif /* UNIV_DEBUG */
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Validates a given range of the cells in hash table.
 @return true if ok */
ibool ha_validate(hash_table_t *table, /*!< in: hash table */
                  ulint start_index,   /*!< in: start index */
                  ulint end_index)     /*!< in: end index */
{
  ibool ok = TRUE;
  ulint i;

  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
  ut_a(start_index <= end_index);
  ut_a(start_index < hash_get_n_cells(table));
  ut_a(end_index < hash_get_n_cells(table));

  for (i = start_index; i <= end_index; i++) {
    ha_node_t *node;
    hash_cell_t *cell;

    cell = hash_get_nth_cell(table, i);

    for (node = static_cast<ha_node_t *>(cell->node); node != 0;
         node = node->next) {
      if (hash_calc_hash(node->fold, table) != i) {
        ib::error(ER_IB_MSG_522) << "Hash table node fold value " << node->fold
                                 << " does not match the"
                                    " cell number "
                                 << i << ".";

        ok = FALSE;
      }
    }
  }

  return (ok);
}
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */

/** Prints info of a hash table. */
void ha_print_info(FILE *file,          /*!< in: file where to print */
                   hash_table_t *table) /*!< in: hash table */
{
#ifdef UNIV_DEBUG
/* Some of the code here is disabled for performance reasons in production
builds, see http://bugs.mysql.com/36941 */
#define PRINT_USED_CELLS
#endif /* UNIV_DEBUG */

#ifdef PRINT_USED_CELLS
  hash_cell_t *cell;
  ulint cells = 0;
  ulint i;
#endif /* PRINT_USED_CELLS */
  ulint n_bufs;

  ut_ad(table);
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#ifdef PRINT_USED_CELLS
  for (i = 0; i < hash_get_n_cells(table); i++) {
    cell = hash_get_nth_cell(table, i);

    if (cell->node) {
      cells++;
    }
  }
#endif /* PRINT_USED_CELLS */

  fprintf(file, "Hash table size %lu", (ulong)hash_get_n_cells(table));

#ifdef PRINT_USED_CELLS
  fprintf(file, ", used cells %lu", (ulong)cells);
#endif /* PRINT_USED_CELLS */

  if (table->heaps == NULL && table->heap != NULL) {
    /* This calculation is intended for the adaptive hash
    index: how many buffer frames we have reserved? */

    n_bufs = UT_LIST_GET_LEN(table->heap->base) - 1;

    if (table->heap->free_block) {
      n_bufs++;
    }

    fprintf(file, ", node heap has %lu buffer(s)\n", (ulong)n_bufs);
  }
}
