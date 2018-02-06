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

/** @file ha/hash0hash.cc
 The simple hash table utility

 Created 5/20/1997 Heikki Tuuri
 *******************************************************/

#include "hash0hash.h"

#include "mem0mem.h"
#include "my_inttypes.h"
#include "sync0sync.h"

#ifndef UNIV_HOTBACKUP

/** Reserves all the locks of a hash table, in an ascending order. */
void hash_lock_x_all(hash_table_t *table) /*!< in: hash table */
{
  ut_ad(table->type == HASH_TABLE_SYNC_RW_LOCK);

  for (ulint i = 0; i < table->n_sync_obj; i++) {
    rw_lock_t *lock = table->sync_obj.rw_locks + i;

    ut_ad(!rw_lock_own(lock, RW_LOCK_S));
    ut_ad(!rw_lock_own(lock, RW_LOCK_X));

    rw_lock_x_lock(lock);
  }
}

/** Releases all the locks of a hash table, in an ascending order. */
void hash_unlock_x_all(hash_table_t *table) /*!< in: hash table */
{
  ut_ad(table->type == HASH_TABLE_SYNC_RW_LOCK);

  for (ulint i = 0; i < table->n_sync_obj; i++) {
    rw_lock_t *lock = table->sync_obj.rw_locks + i;

    ut_ad(rw_lock_own(lock, RW_LOCK_X));

    rw_lock_x_unlock(lock);
  }
}

/** Releases all but passed in lock of a hash table, */
void hash_unlock_x_all_but(hash_table_t *table,  /*!< in: hash table */
                           rw_lock_t *keep_lock) /*!< in: lock to keep */
{
  ut_ad(table->type == HASH_TABLE_SYNC_RW_LOCK);

  for (ulint i = 0; i < table->n_sync_obj; i++) {
    rw_lock_t *lock = table->sync_obj.rw_locks + i;

    ut_ad(rw_lock_own(lock, RW_LOCK_X));

    if (keep_lock != lock) {
      rw_lock_x_unlock(lock);
    }
  }
}

#endif /* !UNIV_HOTBACKUP */

/** Creates a hash table with >= n array cells. The actual number of cells is
 chosen to be a prime number slightly bigger than n.
 @return own: created table */
hash_table_t *hash_create(ulint n) /*!< in: number of array cells */
{
  hash_cell_t *array;
  ulint prime;
  hash_table_t *table;

  prime = ut_find_prime(n);

  table = static_cast<hash_table_t *>(ut_malloc_nokey(sizeof(hash_table_t)));

  array =
      static_cast<hash_cell_t *>(ut_malloc_nokey(sizeof(hash_cell_t) * prime));

  /* The default type of hash_table is HASH_TABLE_SYNC_NONE i.e.:
  the caller is responsible for access control to the table. */
  table->type = HASH_TABLE_SYNC_NONE;
  table->cells = array;
  table->n_cells = prime;
#ifndef UNIV_HOTBACKUP
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  table->adaptive = FALSE;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  table->n_sync_obj = 0;
  table->sync_obj.mutexes = NULL;
  table->heaps = NULL;
#endif /* !UNIV_HOTBACKUP */
  table->heap = NULL;
  ut_d(table->magic_n = HASH_TABLE_MAGIC_N);

  /* Initialize the cell array */
  hash_table_clear(table);

  return (table);
}

/** Frees a hash table. */
void hash_table_free(hash_table_t *table) /*!< in, own: hash table */
{
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);

  ut_free(table->cells);
  ut_free(table);
}

#ifndef UNIV_HOTBACKUP
/** Creates a sync object array to protect a hash table. "::sync_obj" can be
mutexes or rw_locks depening on the type of hash table.
@param[in]	table		hash table
@param[in]	type		HASH_TABLE_SYNC_MUTEX or
                                HASH_TABLE_SYNC_RW_LOCK
@param[in]	id		latch ID
@param[in]	n_sync_obj	number of sync objects, must be a power of 2 */
void hash_create_sync_obj(hash_table_t *table, enum hash_table_sync_t type,
                          latch_id_t id, ulint n_sync_obj) {
  ut_a(n_sync_obj > 0);
  ut_a(ut_is_2pow(n_sync_obj));
  ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);

  table->type = type;

  switch (table->type) {
    case HASH_TABLE_SYNC_MUTEX:
      table->sync_obj.mutexes = static_cast<ib_mutex_t *>(
          ut_malloc_nokey(n_sync_obj * sizeof(ib_mutex_t)));

      for (ulint i = 0; i < n_sync_obj; i++) {
        mutex_create(id, table->sync_obj.mutexes + i);
      }

      break;

    case HASH_TABLE_SYNC_RW_LOCK: {
      latch_level_t level = sync_latch_get_level(id);

      ut_a(level != SYNC_UNKNOWN);

      table->sync_obj.rw_locks = static_cast<rw_lock_t *>(
          ut_malloc_nokey(n_sync_obj * sizeof(rw_lock_t)));

      for (ulint i = 0; i < n_sync_obj; i++) {
        rw_lock_create(hash_table_locks_key, table->sync_obj.rw_locks + i,
                       level);
      }

      break;
    }

    case HASH_TABLE_SYNC_NONE:
      ut_error;
  }

  table->n_sync_obj = n_sync_obj;
}
#endif /* !UNIV_HOTBACKUP */
