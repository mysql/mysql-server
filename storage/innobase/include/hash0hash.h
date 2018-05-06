/*****************************************************************************

Copyright (c) 1997, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/hash0hash.h
 The simple hash table utility

 Created 5/20/1997 Heikki Tuuri
 *******************************************************/

#ifndef hash0hash_h
#define hash0hash_h

#include <stddef.h>

#include "mem0mem.h"
#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

struct hash_table_t;
struct hash_cell_t;

typedef void *hash_node_t;

/* Fix Bug #13859: symbol collision between imap/mysql */
#define hash_create hash0_create

/* Differnt types of hash_table based on the synchronization
method used for it. */
enum hash_table_sync_t {
  HASH_TABLE_SYNC_NONE = 0, /*!< Don't use any internal
                            synchronization objects for
                            this hash_table. */
  HASH_TABLE_SYNC_MUTEX,    /*!< Use mutexes to control
                            access to this hash_table. */
  HASH_TABLE_SYNC_RW_LOCK   /*!< Use rw_locks to control
                            access to this hash_table. */
};

/** Creates a hash table with >= n array cells. The actual number
 of cells is chosen to be a prime number slightly bigger than n.
 @return own: created table */
hash_table_t *hash_create(ulint n); /*!< in: number of array cells */

#ifndef UNIV_HOTBACKUP
/** Creates a sync object array array to protect a hash table. "::sync_obj"
can be mutexes or rw_locks depening on the type of hash table.
@param[in]	table		hash table
@param[in]	type		HASH_TABLE_SYNC_MUTEX or HASH_TABLE_SYNC_RW_LOCK
@param[in]	id		mutex/rw_lock ID
@param[in]	n_sync_obj	number of sync objects, must be a power of 2*/
void hash_create_sync_obj(hash_table_t *table, hash_table_sync_t type,
                          latch_id_t id, ulint n_sync_obj);
#endif /* !UNIV_HOTBACKUP */

/** Frees a hash table. */
void hash_table_free(hash_table_t *table); /*!< in, own: hash table */

/** Calculates the hash value from a folded value.
@param[in]	fold	folded value
@param[in]	table	hash table
@return hashed value */
UNIV_INLINE
ulint hash_calc_hash(ulint fold, hash_table_t *table);

#ifndef UNIV_HOTBACKUP
/** Assert that the mutex for the table is held */
#define HASH_ASSERT_OWN(TABLE, FOLD)              \
  ut_ad((TABLE)->type != HASH_TABLE_SYNC_MUTEX || \
        (mutex_own(hash_get_mutex((TABLE), FOLD))));
#else /* !UNIV_HOTBACKUP */
#define HASH_ASSERT_OWN(TABLE, FOLD)
#endif /* !UNIV_HOTBACKUP */

/** Inserts a struct to a hash table. */

#define HASH_INSERT(TYPE, NAME, TABLE, FOLD, DATA)                    \
  do {                                                                \
    hash_cell_t *cell3333;                                            \
    TYPE *struct3333;                                                 \
                                                                      \
    HASH_ASSERT_OWN(TABLE, FOLD)                                      \
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

#ifdef UNIV_HASH_DEBUG
#define HASH_ASSERT_VALID(DATA) ut_a((void *)(DATA) != (void *)-1)
#define HASH_INVALIDATE(DATA, NAME) *(void **)(&DATA->NAME) = (void *)-1
#else
#define HASH_ASSERT_VALID(DATA) \
  do {                          \
  } while (0)
#define HASH_INVALIDATE(DATA, NAME) \
  do {                              \
  } while (0)
#endif

/** Deletes a struct from a hash table. */

#define HASH_DELETE(TYPE, NAME, TABLE, FOLD, DATA)                    \
  do {                                                                \
    hash_cell_t *cell3333;                                            \
    TYPE *struct3333;                                                 \
                                                                      \
    HASH_ASSERT_OWN(TABLE, FOLD)                                      \
                                                                      \
    cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE)); \
                                                                      \
    if (cell3333->node == DATA) {                                     \
      HASH_ASSERT_VALID(DATA->NAME);                                  \
      cell3333->node = DATA->NAME;                                    \
    } else {                                                          \
      struct3333 = (TYPE *)cell3333->node;                            \
                                                                      \
      while (struct3333->NAME != DATA) {                              \
        struct3333 = (TYPE *)struct3333->NAME;                        \
        ut_a(struct3333);                                             \
      }                                                               \
                                                                      \
      struct3333->NAME = DATA->NAME;                                  \
    }                                                                 \
    HASH_INVALIDATE(DATA, NAME);                                      \
  } while (0)

/** Gets the first struct in a hash chain, NULL if none. */

#define HASH_GET_FIRST(TABLE, HASH_VAL) \
  (hash_get_nth_cell(TABLE, HASH_VAL)->node)

/** Gets the next struct in a hash chain, NULL if none. */

#define HASH_GET_NEXT(NAME, DATA) ((DATA)->NAME)

/** Looks for a struct in a hash table. */
#define HASH_SEARCH(NAME, TABLE, FOLD, TYPE, DATA, ASSERTION, TEST)    \
  {                                                                    \
    HASH_ASSERT_OWN(TABLE, FOLD)                                       \
                                                                       \
    (DATA) = (TYPE)HASH_GET_FIRST(TABLE, hash_calc_hash(FOLD, TABLE)); \
    HASH_ASSERT_VALID(DATA);                                           \
                                                                       \
    while ((DATA) != NULL) {                                           \
      ASSERTION;                                                       \
      if (TEST) {                                                      \
        break;                                                         \
      } else {                                                         \
        HASH_ASSERT_VALID(HASH_GET_NEXT(NAME, DATA));                  \
        (DATA) = (TYPE)HASH_GET_NEXT(NAME, DATA);                      \
      }                                                                \
    }                                                                  \
  }

/** Looks for an item in all hash buckets. */
#define HASH_SEARCH_ALL(NAME, TABLE, TYPE, DATA, ASSERTION, TEST) \
  do {                                                            \
    ulint i3333;                                                  \
                                                                  \
    for (i3333 = (TABLE)->n_cells; i3333--;) {                    \
      (DATA) = (TYPE)HASH_GET_FIRST(TABLE, i3333);                \
                                                                  \
      while ((DATA) != NULL) {                                    \
        HASH_ASSERT_VALID(DATA);                                  \
        ASSERTION;                                                \
                                                                  \
        if (TEST) {                                               \
          break;                                                  \
        }                                                         \
                                                                  \
        (DATA) = (TYPE)HASH_GET_NEXT(NAME, DATA);                 \
      }                                                           \
                                                                  \
      if ((DATA) != NULL) {                                       \
        break;                                                    \
      }                                                           \
    }                                                             \
  } while (0)

/** Gets the nth cell in a hash table.
@param[in]	table	hash table
@param[in]	n	cell index
@return pointer to cell */
UNIV_INLINE
hash_cell_t *hash_get_nth_cell(hash_table_t *table, ulint n);

/** Clears a hash table so that all the cells become empty. */
UNIV_INLINE
void hash_table_clear(hash_table_t *table); /*!< in/out: hash table */

/** Returns the number of cells in a hash table.
 @return number of cells */
UNIV_INLINE
ulint hash_get_n_cells(hash_table_t *table); /*!< in: table */
/** Deletes a struct which is stored in the heap of the hash table, and compacts
 the heap. The fold value must be stored in the struct NODE in a field named
 'fold'. */

#define HASH_DELETE_AND_COMPACT(TYPE, NAME, TABLE, NODE)                       \
  do {                                                                         \
    TYPE *node111;                                                             \
    TYPE *top_node111;                                                         \
    hash_cell_t *cell111;                                                      \
    ulint fold111;                                                             \
                                                                               \
    fold111 = (NODE)->fold;                                                    \
                                                                               \
    HASH_DELETE(TYPE, NAME, TABLE, fold111, NODE);                             \
                                                                               \
    top_node111 =                                                              \
        (TYPE *)mem_heap_get_top(hash_get_heap(TABLE, fold111), sizeof(TYPE)); \
                                                                               \
    /* If the node to remove is not the top node in the heap, compact the      \
    heap of nodes by moving the top node in the place of NODE. */              \
                                                                               \
    if (NODE != top_node111) {                                                 \
      /* Copy the top node in place of NODE */                                 \
                                                                               \
      *(NODE) = *top_node111;                                                  \
                                                                               \
      cell111 =                                                                \
          hash_get_nth_cell(TABLE, hash_calc_hash(top_node111->fold, TABLE));  \
                                                                               \
      /* Look for the pointer to the top node, to update it */                 \
                                                                               \
      if (cell111->node == top_node111) {                                      \
        /* The top node is the first in the chain */                           \
                                                                               \
        cell111->node = NODE;                                                  \
      } else {                                                                 \
        /* We have to look for the predecessor of the top                      \
        node */                                                                \
        node111 = static_cast<TYPE *>(cell111->node);                          \
                                                                               \
        while (top_node111 != HASH_GET_NEXT(NAME, node111)) {                  \
          node111 = static_cast<TYPE *>(HASH_GET_NEXT(NAME, node111));         \
        }                                                                      \
                                                                               \
        /* Now we have the predecessor node */                                 \
                                                                               \
        node111->NAME = NODE;                                                  \
      }                                                                        \
    }                                                                          \
                                                                               \
    /* Free the space occupied by the top node */                              \
                                                                               \
    mem_heap_free_top(hash_get_heap(TABLE, fold111), sizeof(TYPE));            \
  } while (0)

#ifndef UNIV_HOTBACKUP
/** Move all hash table entries from OLD_TABLE to NEW_TABLE. */

#define HASH_MIGRATE(OLD_TABLE, NEW_TABLE, NODE_TYPE, PTR_NAME, FOLD_FUNC)  \
  do {                                                                      \
    ulint i2222;                                                            \
    ulint cell_count2222;                                                   \
                                                                            \
    cell_count2222 = hash_get_n_cells(OLD_TABLE);                           \
                                                                            \
    for (i2222 = 0; i2222 < cell_count2222; i2222++) {                      \
      NODE_TYPE *node2222 =                                                 \
          static_cast<NODE_TYPE *>(HASH_GET_FIRST((OLD_TABLE), i2222));     \
                                                                            \
      while (node2222) {                                                    \
        NODE_TYPE *next2222 = static_cast<NODE_TYPE *>(node2222->PTR_NAME); \
        ulint fold2222 = FOLD_FUNC(node2222);                               \
                                                                            \
        HASH_INSERT(NODE_TYPE, PTR_NAME, (NEW_TABLE), fold2222, node2222);  \
                                                                            \
        node2222 = next2222;                                                \
      }                                                                     \
    }                                                                       \
  } while (0)

/** Gets the sync object index for a fold value in a hash table.
@param[in]	table	hash table
@param[in]	fold	fold
@return index */
UNIV_INLINE
ulint hash_get_sync_obj_index(hash_table_t *table, ulint fold);

/** Gets the nth heap in a hash table.
@param[in]	table	hash table
@param[in]	i	index of the mutex
@return mem heap */
UNIV_INLINE
mem_heap_t *hash_get_nth_heap(hash_table_t *table, ulint i);

/** Gets the heap for a fold value in a hash table.
@param[in]	table	hash table
@param[in]	fold	fold
@return mem heap */
UNIV_INLINE
mem_heap_t *hash_get_heap(hash_table_t *table, ulint fold);

/** Gets the nth mutex in a hash table.
@param[in]	table	hash table
@param[in]	i	index of the mutex
@return mutex */
UNIV_INLINE
ib_mutex_t *hash_get_nth_mutex(hash_table_t *table, ulint i);

/** Gets the nth rw_lock in a hash table.
@param[in]	table	hash table
@param[in]	i	index of the mutex
@return rw_lock */
UNIV_INLINE
rw_lock_t *hash_get_nth_lock(hash_table_t *table, ulint i);

/** Gets the mutex for a fold value in a hash table.
@param[in]	table	hash table
@param[in]	fold	fold
@return mutex */
UNIV_INLINE
ib_mutex_t *hash_get_mutex(hash_table_t *table, ulint fold);

/** Gets the rw_lock for a fold value in a hash table.
@param[in]	table	hash table
@param[in]	fold	fold
@return rw_lock */
UNIV_INLINE
rw_lock_t *hash_get_lock(hash_table_t *table, ulint fold);

/** If not appropriate rw_lock for a fold value in a hash table,
relock S-lock the another rw_lock until appropriate for a fold value.
@param[in]	hash_lock	latched rw_lock to be confirmed
@param[in]	table		hash table
@param[in]	fold		fold value
@return	latched rw_lock */
UNIV_INLINE
rw_lock_t *hash_lock_s_confirm(rw_lock_t *hash_lock, hash_table_t *table,
                               ulint fold);

/** If not appropriate rw_lock for a fold value in a hash table,
relock X-lock the another rw_lock until appropriate for a fold value.
@param[in]	hash_lock	latched rw_lock to be confirmed
@param[in]	table		hash table
@param[in]	fold		fold value
@return	latched rw_lock */
UNIV_INLINE
rw_lock_t *hash_lock_x_confirm(rw_lock_t *hash_lock, hash_table_t *table,
                               ulint fold);

/** Reserves all the locks of a hash table, in an ascending order. */
void hash_lock_x_all(hash_table_t *table); /*!< in: hash table */
/** Releases all the locks of a hash table, in an ascending order. */
void hash_unlock_x_all(hash_table_t *table); /*!< in: hash table */
/** Releases all but passed in lock of a hash table, */
void hash_unlock_x_all_but(hash_table_t *table,   /*!< in: hash table */
                           rw_lock_t *keep_lock); /*!< in: lock to keep */

#else /* !UNIV_HOTBACKUP */
#define hash_get_heap(table, fold) ((table)->heap)
#define hash_lock_x_all(t) ((void)0)
#define hash_unlock_x_all(t) ((void)0)
#define hash_unlock_x_all_but(t, l) ((void)0)
#endif /* !UNIV_HOTBACKUP */

struct hash_cell_t {
  void *node; /*!< hash chain node, NULL if none */
};

/* The hash table structure */
struct hash_table_t {
  enum hash_table_sync_t type; /*!< type of hash_table. */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
#ifndef UNIV_HOTBACKUP
  ibool adaptive;     /* TRUE if this is the hash
                     table of the adaptive hash
                     index */
#endif                /* !UNIV_HOTBACKUP */
#endif                /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  ulint n_cells;      /* number of cells in the hash table */
  hash_cell_t *cells; /*!< pointer to cell array */
#ifndef UNIV_HOTBACKUP
  ulint n_sync_obj; /* if sync_objs != NULL, then
                 the number of either the number
                 of mutexes or the number of
                 rw_locks depending on the type.
                 Must be a power of 2 */
  union {
    ib_mutex_t *mutexes; /* NULL, or an array of mutexes
                         used to protect segments of the
                         hash table */
    rw_lock_t *rw_locks; /* NULL, or an array of rw_lcoks
                        used to protect segments of the
                        hash table */
  } sync_obj;

  mem_heap_t **heaps; /*!< if this is non-NULL, hash
                      chain nodes for external chaining
                      can be allocated from these memory
                      heaps; there are then n_mutexes
                      many of these heaps */
#endif                /* !UNIV_HOTBACKUP */
  mem_heap_t *heap;
#ifdef UNIV_DEBUG
  ulint magic_n;
#define HASH_TABLE_MAGIC_N 76561114
#endif /* UNIV_DEBUG */
};

#include "hash0hash.ic"

#endif
