/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
#include "ut0rnd.h"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

class hash_table_t;
struct hash_cell_t;

typedef void *hash_node_t;

/* Different types of hash_table based on the synchronization
method used for it. */
enum hash_table_sync_t {
  HASH_TABLE_SYNC_NONE = 0, /*!< Don't use any internal
                            synchronization objects for
                            this hash_table. */
  HASH_TABLE_SYNC_RW_LOCK   /*!< Use rw_locks to control
                            access to this hash_table. */
};

struct hash_cell_t {
  void *node; /*!< hash chain node, NULL if none */
};

#ifndef UNIV_HOTBACKUP

/** Creates a sync object array to protect a hash table.
@param[in]      table           hash table
@param[in]      id              latch ID
@param[in]      n_sync_obj      number of sync objects, must be a power of 2 */
void hash_create_sync_obj(hash_table_t *table, latch_id_t id,
                          size_t n_sync_obj);
#endif /* !UNIV_HOTBACKUP */

/** Calculates the cell index from a hashed value for a specified hash table.
@param[in]      hash_value      hashed value
@param[in]      table           hash table
@return cell index for specified hash table*/
static inline uint64_t hash_calc_cell_id(uint64_t hash_value,
                                         hash_table_t *table);

/** Gets the nth cell in a hash table.
@param[in]      table   hash table
@param[in]	n	cell index
@return pointer to cell */
static inline hash_cell_t *hash_get_nth_cell(hash_table_t *table, size_t n);

/** Inserts a struct to a hash table. */

#define HASH_INSERT(TYPE, NAME, TABLE, HASH_VALUE, DATA)                    \
  do {                                                                      \
    hash_cell_t *cell3333;                                                  \
    TYPE *struct3333;                                                       \
    const uint64_t hash_value3333 = HASH_VALUE;                             \
                                                                            \
    hash_assert_can_modify(TABLE, hash_value3333);                          \
                                                                            \
    (DATA)->NAME = NULL;                                                    \
                                                                            \
    cell3333 =                                                              \
        hash_get_nth_cell(TABLE, hash_calc_cell_id(hash_value3333, TABLE)); \
                                                                            \
    if (cell3333->node == NULL) {                                           \
      cell3333->node = DATA;                                                \
    } else {                                                                \
      struct3333 = (TYPE *)cell3333->node;                                  \
                                                                            \
      while (struct3333->NAME != NULL) {                                    \
        struct3333 = (TYPE *)struct3333->NAME;                              \
      }                                                                     \
                                                                            \
      struct3333->NAME = DATA;                                              \
    }                                                                       \
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

#define HASH_DELETE(TYPE, NAME, TABLE, HASH_VALUE, DATA)                    \
  do {                                                                      \
    hash_cell_t *cell3333;                                                  \
    TYPE *struct3333;                                                       \
    const uint64_t hash_value3333 = HASH_VALUE;                             \
                                                                            \
    hash_assert_can_modify(TABLE, hash_value3333);                          \
                                                                            \
    cell3333 =                                                              \
        hash_get_nth_cell(TABLE, hash_calc_cell_id(hash_value3333, TABLE)); \
                                                                            \
    if (cell3333->node == DATA) {                                           \
      HASH_ASSERT_VALID(DATA->NAME);                                        \
      cell3333->node = DATA->NAME;                                          \
    } else {                                                                \
      struct3333 = (TYPE *)cell3333->node;                                  \
                                                                            \
      while (struct3333->NAME != DATA) {                                    \
        struct3333 = (TYPE *)struct3333->NAME;                              \
        ut_a(struct3333);                                                   \
      }                                                                     \
                                                                            \
      struct3333->NAME = DATA->NAME;                                        \
    }                                                                       \
    HASH_INVALIDATE(DATA, NAME);                                            \
  } while (0)

/** Gets the first struct in a hash chain, NULL if none. */

static inline void *&hash_get_first(hash_table_t *table, size_t cell_id) {
  return hash_get_nth_cell(table, cell_id)->node;
}

/** Gets the next struct in a hash chain, NULL if none. */

#define HASH_GET_NEXT(NAME, DATA) ((DATA)->NAME)

/** Looks for a struct in a hash table. */
#define HASH_SEARCH(NAME, TABLE, HASH_VALUE, TYPE, DATA, ASSERTION, TEST)      \
  {                                                                            \
    const uint64_t hash_value3333 = HASH_VALUE;                                \
                                                                               \
    hash_assert_can_search(TABLE, hash_value3333);                             \
                                                                               \
    (DATA) =                                                                   \
        (TYPE)hash_get_first(TABLE, hash_calc_cell_id(hash_value3333, TABLE)); \
    HASH_ASSERT_VALID(DATA);                                                   \
                                                                               \
    while ((DATA) != NULL) {                                                   \
      ASSERTION;                                                               \
      if (TEST) {                                                              \
        break;                                                                 \
      } else {                                                                 \
        HASH_ASSERT_VALID(HASH_GET_NEXT(NAME, DATA));                          \
        (DATA) = (TYPE)HASH_GET_NEXT(NAME, DATA);                              \
      }                                                                        \
    }                                                                          \
  }

/** Looks for an item in all hash cells. */
#define HASH_SEARCH_ALL(NAME, TABLE, TYPE, DATA, ASSERTION, TEST) \
  do {                                                            \
    size_t i3333;                                                 \
                                                                  \
    for (i3333 = (TABLE)->get_n_cells(); i3333--;) {              \
      (DATA) = (TYPE)hash_get_first(TABLE, i3333);                \
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

/** Clears a hash table so that all the cells become empty. */
static inline void hash_table_clear(
    hash_table_t *table); /*!< in/out: hash table */

/** Returns the number of cells in a hash table.
 @return number of cells */
static inline size_t hash_get_n_cells(hash_table_t *table); /*!< in: table */

/** Deletes a struct which is stored in the heap of the hash table, and compacts
 the heap. The hash value must be stored in the struct NODE in a field named
 'hash_value'. */
#define HASH_DELETE_AND_COMPACT(TYPE, NAME, TABLE, NODE)                  \
  do {                                                                    \
    TYPE *node111;                                                        \
    TYPE *top_node111;                                                    \
    hash_cell_t *cell111;                                                 \
    uint64_t hash_value111;                                               \
                                                                          \
    hash_value111 = (NODE)->hash_value;                                   \
                                                                          \
    HASH_DELETE(TYPE, NAME, TABLE, hash_value111, NODE);                  \
                                                                          \
    top_node111 =                                                         \
        (TYPE *)mem_heap_get_top(hash_get_heap(TABLE), sizeof(TYPE));     \
                                                                          \
    /* If the node to remove is not the top node in the heap, compact the \
    heap of nodes by moving the top node in the place of NODE. */         \
                                                                          \
    if (NODE != top_node111) {                                            \
      /* Copy the top node in place of NODE */                            \
                                                                          \
      *(NODE) = *top_node111;                                             \
                                                                          \
      cell111 = hash_get_nth_cell(                                        \
          TABLE, hash_calc_cell_id(top_node111->hash_value, TABLE));      \
                                                                          \
      /* Look for the pointer to the top node, to update it */            \
                                                                          \
      if (cell111->node == top_node111) {                                 \
        /* The top node is the first in the chain */                      \
                                                                          \
        cell111->node = NODE;                                             \
      } else {                                                            \
        /* We have to look for the predecessor of the top                 \
        node */                                                           \
        node111 = static_cast<TYPE *>(cell111->node);                     \
                                                                          \
        while (top_node111 != HASH_GET_NEXT(NAME, node111)) {             \
          node111 = static_cast<TYPE *>(HASH_GET_NEXT(NAME, node111));    \
        }                                                                 \
                                                                          \
        /* Now we have the predecessor node */                            \
                                                                          \
        node111->NAME = NODE;                                             \
      }                                                                   \
    }                                                                     \
                                                                          \
    /* Free the space occupied by the top node */                         \
                                                                          \
    mem_heap_free_top(hash_get_heap(TABLE), sizeof(TYPE));                \
  } while (0)

#ifndef UNIV_HOTBACKUP
/** Move all hash table entries from OLD_TABLE to NEW_TABLE. */

#define HASH_MIGRATE(OLD_TABLE, NEW_TABLE, NODE_TYPE, PTR_NAME, HASH_FUNC)  \
  do {                                                                      \
    size_t i2222;                                                           \
    size_t cell_count2222;                                                  \
                                                                            \
    cell_count2222 = hash_get_n_cells(OLD_TABLE);                           \
                                                                            \
    for (i2222 = 0; i2222 < cell_count2222; i2222++) {                      \
      NODE_TYPE *node2222 =                                                 \
          static_cast<NODE_TYPE *>(hash_get_first((OLD_TABLE), i2222));     \
                                                                            \
      while (node2222) {                                                    \
        NODE_TYPE *next2222 = static_cast<NODE_TYPE *>(node2222->PTR_NAME); \
        uint64_t hash_value2222 = HASH_FUNC(node2222);                      \
                                                                            \
        HASH_INSERT(NODE_TYPE, PTR_NAME, (NEW_TABLE), hash_value2222,       \
                    node2222);                                              \
                                                                            \
        node2222 = next2222;                                                \
      }                                                                     \
    }                                                                       \
  } while (0)

/** Gets the sync object index for a hash value in a hash table.
@param[in]      table       hash table
@param[in]      hash_value  hash value
@return index */
static inline uint64_t hash_get_sync_obj_index(hash_table_t *table,
                                               uint64_t hash_value);

/** Gets the heap for a hash value in a hash table.
@param[in]      table   hash table
@return mem heap */
static inline mem_heap_t *hash_get_heap(hash_table_t *table);

/** Gets the nth rw_lock in a hash table.
@param[in]      table   hash table
@param[in]      i       index of the rw_lock
@return rw_lock */
static inline rw_lock_t *hash_get_nth_lock(hash_table_t *table, size_t i);

/** Gets the rw_lock for a hash value in a hash table.
@param[in]      table       hash table
@param[in]      hash_value  hash value
@return rw_lock */
static inline rw_lock_t *hash_get_lock(hash_table_t *table,
                                       uint64_t hash_value);

/** If not appropriate rw_lock for a hash value in a hash table,
relock S-lock the another rw_lock until appropriate for a hash value.
@param[in]      hash_lock       latched rw_lock to be confirmed
@param[in]      table           hash table
@param[in]      hash_value      hash value
@return latched rw_lock */
static inline rw_lock_t *hash_lock_s_confirm(rw_lock_t *hash_lock,
                                             hash_table_t *table,
                                             uint64_t hash_value);

/** If not appropriate rw_lock for a hash value in a hash table,
relock X-lock the another rw_lock until appropriate for a hash value.
@param[in]      hash_lock       latched rw_lock to be confirmed
@param[in]      table           hash table
@param[in]      hash_value      hash value
@return latched rw_lock */
static inline rw_lock_t *hash_lock_x_confirm(rw_lock_t *hash_lock,
                                             hash_table_t *table,
                                             uint64_t hash_value);

#ifdef UNIV_DEBUG

/** Verifies that the current thread holds X-latch on all shards.
Assumes type==HASH_TABLE_SYNC_RW_LOCK.
@param[in]  table the table in question
@return true iff the current thread holds X-latch on all shards*/
bool hash_lock_has_all_x(const hash_table_t *table);

#endif /* UNIV_DEBUG */

/** Reserves all the locks of a hash table, in an ascending order. */
void hash_lock_x_all(hash_table_t *table); /*!< in: hash table */

/** Releases all the locks of a hash table, in an ascending order. */
void hash_unlock_x_all(hash_table_t *table); /*!< in: hash table */

/** Releases all but passed in lock of a hash table,
@param[in] table Hash table
@param[in] keep_lock Lock to keep */
void hash_unlock_x_all_but(hash_table_t *table, rw_lock_t *keep_lock);

#else /* !UNIV_HOTBACKUP */
#define hash_get_heap(table) ((table)->heap)
#define hash_lock_x_all(t) ((void)0)
#define hash_unlock_x_all(t) ((void)0)
#define hash_unlock_x_all_but(t, l) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/* The hash table structure */
class hash_table_t {
 public:
  hash_table_t(size_t n) {
    const auto prime = ut::find_prime(n);
    cells = ut::make_unique<hash_cell_t[]>(prime);
    set_n_cells(prime);

    /* Initialize the cell array */
    hash_table_clear(this);
  }
  ~hash_table_t() { ut_ad(magic_n == HASH_TABLE_MAGIC_N); }

  /** Returns number of cells in cells[] array.
   If type==HASH_TABLE_SYNC_RW_LOCK it can be used:
  - without any latches to peek a value, before hash_lock_[sx]_confirm
  - when holding S-latch for at least one n_sync_obj to get the "real" value
  @return value of n_cells
  */
  size_t get_n_cells() { return n_cells.load(std::memory_order_relaxed); }

  /** Returns a helper class for calculating fast modulo n_cells.
   If type==HASH_TABLE_SYNC_RW_LOCK it can be used:
  - without any latches to peek a value, before hash_lock_[sx]_confirm
  - when holding S-latch for at least one n_sync_obj to get the "real" value */
  const ut::fast_modulo_t get_n_cells_fast_modulo() {
    return n_cells_fast_modulo.load();
  }

  /** Sets the number of n_cells, to the provided one.
  If type==HASH_TABLE_SYNC_RW_LOCK it can be used only when holding x-latches on
  all shards.
  @param[in]  n   The new size of cells[] array
  */
  void set_n_cells(size_t n) {
#ifndef UNIV_HOTBACKUP
    ut_ad(type == HASH_TABLE_SYNC_NONE || hash_lock_has_all_x(this));
#endif
    n_cells.store(n);
    n_cells_fast_modulo.store(n);
  }

 public:
  /** Either:
  a) HASH_TABLE_SYNC_NONE in which case n_sync_obj is 0 and rw_locks is nullptr
  or
  b) HASH_TABLE_SYNC_RW_LOCK in which case n_sync_obj > 0 is the number of
  rw_locks elements, each of which protects a disjoint fraction of cells.
  The default type of hash_table is HASH_TABLE_SYNC_NONE i.e.: the caller is
  responsible for access control to the table. */
  enum hash_table_sync_t type = HASH_TABLE_SYNC_NONE;

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
#ifndef UNIV_HOTBACKUP
  /* true if this is the hash table of the adaptive hash index */
  bool adaptive = false;
#endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
 private:
  /** The number of cells in the hash table.
  If type==HASH_TABLE_SYNC_RW_LOCK it is:
  - modified when holding X-latches on all n_sync_obj
  - read
    - without any latches to peek a value, before hash_lock_[sx]_confirm
    - when holding S-latch for at least one n_sync_obj to get the "real" value
  */
  std::atomic<size_t> n_cells;
  /** Utility to calculate the modulo n_cells fast. It is set together with
  n_cells. It can be read without latches in parallel to set_n_cells, and as it
  is a complex object, it is not set atomically. Because of this the
  multi-threaded version is used. */
  ut::mt_fast_modulo_t n_cells_fast_modulo;

 public:
  /** The pointer to the array of cells.
  If type==HASH_TABLE_SYNC_RW_LOCK it is:
  - modified when holding X-latches on all n_sync_obj
  - read when holding an S-latch for at least one n_sync_obj
  */
  ut::unique_ptr<hash_cell_t[]> cells;
#ifndef UNIV_HOTBACKUP
  /** if rw_locks != nullptr, then it's their number (must be a power of two).
  Otherwise, 0. Is zero iff the type is HASH_TABLE_SYNC_NONE. */
  size_t n_sync_obj = 0;
  /** nullptr, or an array of n_sync_obj rw_locks used to protect segments of
  the hash table. Is nullptr iff the type is HASH_TABLE_SYNC_NONE. */
  rw_lock_t *rw_locks = nullptr;

#endif /* !UNIV_HOTBACKUP */
  mem_heap_t *heap = nullptr;
#ifdef UNIV_DEBUG
  static constexpr uint32_t HASH_TABLE_MAGIC_N = 76561114;
  uint32_t magic_n = HASH_TABLE_MAGIC_N;
#endif /* UNIV_DEBUG */
};

#include "hash0hash.ic"

#endif
