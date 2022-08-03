/*****************************************************************************

Copyright (c) 2007, 2022, Oracle and/or its affiliates.

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

/** @file trx/trx0i_s.cc
 INFORMATION SCHEMA innodb_trx, innodb_locks and
 innodb_lock_waits tables fetch code.

 The code below fetches information needed to fill those
 3 dynamic tables and uploads it into a "transactions
 table cache" for later retrieval.

 Created July 17, 2007 Vasil Dimov
 *******************************************************/

/* Found during the build of 5.5.3 on Linux 2.4 and early 2.6 kernels:
   The includes "univ.i" -> "my_global.h" cause a different path
   to be taken further down with pthread functions and types,
   so they must come first.
   From the symptoms, this is related to bug#46587 in the MySQL bug DB.
*/

#include <sql_class.h>
#include <stdio.h>

#include "buf0buf.h"
#include "dict0dict.h"
#include "ha0storage.h"
#include "ha_prototypes.h"
#include "hash0hash.h"
#include "lock0iter.h"
#include "lock0lock.h"
#include "mem0mem.h"
#include "mysql/plugin.h"
#include "page0page.h"
#include "rem0rec.h"
#include "row0row.h"
#include "srv0srv.h"
#include "sync0rw.h"
#include "sync0sync.h"
#include "trx0i_s.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "ut0mem.h"

#include "storage/perfschema/pfs_data_lock.h"
static_assert(sizeof(pk_pos_data_lock::m_engine_lock_id) >
                  TRX_I_S_LOCK_ID_MAX_LEN,
              "pk_pos_data_lock::m_engine_lock_id must be able to hold "
              "engine_lock_id which has TRX_I_S_LOCK_ID_MAX_LEN chars");
static_assert(sizeof(pk_pos_data_lock_wait::m_requesting_engine_lock_id) >
                  TRX_I_S_LOCK_ID_MAX_LEN,
              "pk_pos_data_lock_wait::m_requesting_engine_lock_id must be able "
              "to hold engine_lock_id which has TRX_I_S_LOCK_ID_MAX_LEN chars");
static_assert(sizeof(pk_pos_data_lock_wait::m_blocking_engine_lock_id) >
                  TRX_I_S_LOCK_ID_MAX_LEN,
              "pk_pos_data_lock_wait::m_blocking_engine_lock_id must be able "
              "to hold engine_lock_id which has TRX_I_S_LOCK_ID_MAX_LEN chars");

/** Initial number of rows in the table cache */
constexpr uint32_t TABLE_CACHE_INITIAL_ROWSNUM = 1024;

/** @brief The maximum number of chunks to allocate for a table cache.

The rows of a table cache are stored in a set of chunks. When a new
row is added a new chunk is allocated if necessary. Assuming that the
first one is 1024 rows (TABLE_CACHE_INITIAL_ROWSNUM) and each
subsequent is N/2 where N is the number of rows we have allocated till
now, then 39th chunk would accommodate 1677416425 rows and all chunks
would accommodate 3354832851 rows. */
constexpr uint32_t MEM_CHUNKS_IN_TABLE_CACHE = 39;

/** Memory limit passed to ha_storage_put_memlim().
@param cache hash storage
@return maximum allowed allocation size */
#define MAX_ALLOWED_FOR_STORAGE(cache) (TRX_I_S_MEM_LIMIT - (cache)->mem_allocd)

/** Memory limit in table_cache_create_empty_row().
@param cache hash storage
@return maximum allowed allocation size */
#define MAX_ALLOWED_FOR_ALLOC(cache)         \
  (TRX_I_S_MEM_LIMIT - (cache)->mem_allocd - \
   ha_storage_get_size((cache)->storage))

/** Memory for each table in the intermediate buffer is allocated in
separate chunks. These chunks are considered to be concatenated to
represent one flat array of rows. */
struct i_s_mem_chunk_t {
  ulint offset;      /*!< offset, in number of rows */
  ulint rows_allocd; /*!< the size of this chunk, in number
                     of rows */
  void *base;        /*!< start of the chunk */
};

/** This represents one table's cache. */
struct i_s_table_cache_t {
  ulint rows_used;   /*!< number of used rows */
  ulint rows_allocd; /*!< number of allocated rows */
  ulint row_size;    /*!< size of a single row */
  i_s_mem_chunk_t chunks[MEM_CHUNKS_IN_TABLE_CACHE]; /*!< array of
                                  memory chunks that stores the
                                  rows */
};

/** Initial size of the cache storage */
constexpr uint32_t CACHE_STORAGE_INITIAL_SIZE = 1024;
/** Number of hash cells in the cache storage */
constexpr uint32_t CACHE_STORAGE_HASH_CELLS = 2048;

/** This structure describes the intermediate buffer */
struct trx_i_s_cache_t {
  rw_lock_t *rw_lock; /*!< read-write lock protecting
                      the rest of this structure */
  std::atomic<std::chrono::steady_clock::time_point> last_read{
      std::chrono::steady_clock::time_point{}}; /*!< last time the cache was
                                                   read; */
  static_assert(decltype(last_read)::is_always_lock_free);

  i_s_table_cache_t innodb_trx;   /*!< innodb_trx table */
  i_s_table_cache_t innodb_locks; /*!< innodb_locks table */

  /** storage for external volatile data that may become unavailable when we
  release exclusive global locksys latch or trx_sys->mutex */
  ha_storage_t *storage;

  /** the amount of memory allocated with mem_alloc*() */
  ulint mem_allocd;

  /** this is true if the memory limit was hit and thus the data in the cache is
  truncated */
  bool is_truncated;
};

/** This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
static trx_i_s_cache_t trx_i_s_cache_static;
/** This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
trx_i_s_cache_t *trx_i_s_cache = &trx_i_s_cache_static;

/** For a record lock that is in waiting state retrieves the only bit that
 is set, for a table lock returns ULINT_UNDEFINED.
 @return record number within the heap */
static ulint wait_lock_get_heap_no(const lock_t *lock) /*!< in: lock */
{
  ulint ret;

  switch (lock_get_type(lock)) {
    case LOCK_REC:
      ret = lock_rec_find_set_bit(lock);
      ut_a(ret != ULINT_UNDEFINED);
      break;
    case LOCK_TABLE:
      ret = ULINT_UNDEFINED;
      break;
    default:
      ut_error;
  }

  return (ret);
}

/** Initializes the members of a table cache. */
static void table_cache_init(
    i_s_table_cache_t *table_cache, /*!< out: table cache */
    size_t row_size)                /*!< in: the size of a
                                    row */
{
  ulint i;

  table_cache->rows_used = 0;
  table_cache->rows_allocd = 0;
  table_cache->row_size = row_size;

  for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {
    /* the memory is actually allocated in
    table_cache_create_empty_row() */
    table_cache->chunks[i].base = nullptr;
  }
}

/** Frees a table cache. */
static void table_cache_free(
    i_s_table_cache_t *table_cache) /*!< in/out: table cache */
{
  ulint i;

  for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {
    /* the memory is actually allocated in
    table_cache_create_empty_row() */
    if (table_cache->chunks[i].base) {
      ut::free(table_cache->chunks[i].base);
      table_cache->chunks[i].base = nullptr;
    }
  }
}

/** Returns an empty row from a table cache. The row is allocated if no more
 empty rows are available. The number of used rows is incremented.
 If the memory limit is hit then NULL is returned and nothing is
 allocated.
 @return empty row, or NULL if out of memory */
static void *table_cache_create_empty_row(
    i_s_table_cache_t *table_cache, /*!< in/out: table cache */
    trx_i_s_cache_t *cache)         /*!< in/out: cache to record
                                    how many bytes are
                                    allocated */
{
  ulint i;
  void *row;

  ut_a(table_cache->rows_used <= table_cache->rows_allocd);

  if (table_cache->rows_used == table_cache->rows_allocd) {
    /* rows_used == rows_allocd means that new chunk needs
    to be allocated: either no more empty rows in the
    last allocated chunk or nothing has been allocated yet
    (rows_num == rows_allocd == 0); */

    i_s_mem_chunk_t *chunk;
    ulint req_bytes;
    ulint got_bytes;
    ulint req_rows;
    ulint got_rows;

    /* find the first not allocated chunk */
    for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {
      if (table_cache->chunks[i].base == nullptr) {
        break;
      }
    }

    /* i == MEM_CHUNKS_IN_TABLE_CACHE means that all chunks
    have been allocated :-X */
    ut_a(i < MEM_CHUNKS_IN_TABLE_CACHE);

    /* allocate the chunk we just found */

    if (i == 0) {
      /* first chunk, nothing is allocated yet */
      req_rows = TABLE_CACHE_INITIAL_ROWSNUM;
    } else {
      /* Memory is increased by the formula
      new = old + old / 2; We are trying not to be
      aggressive here (= using the common new = old * 2)
      because the allocated memory will not be freed
      until InnoDB exit (it is reused). So it is better
      to once allocate the memory in more steps, but
      have less unused/wasted memory than to use less
      steps in allocation (which is done once in a
      lifetime) but end up with lots of unused/wasted
      memory. */
      req_rows = table_cache->rows_allocd / 2;
    }
    req_bytes = req_rows * table_cache->row_size;

    if (req_bytes > MAX_ALLOWED_FOR_ALLOC(cache)) {
      return (nullptr);
    }

    chunk = &table_cache->chunks[i];

    got_bytes = req_bytes;
    chunk->base = ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, req_bytes);

    got_rows = got_bytes / table_cache->row_size;

    cache->mem_allocd += got_bytes;

#if 0
                printf("allocating chunk %d req bytes=%lu, got bytes=%lu,"
                       " row size=%lu,"
                       " req rows=%lu, got rows=%lu\n",
                       i, req_bytes, got_bytes,
                       table_cache->row_size,
                       req_rows, got_rows);
#endif

    chunk->rows_allocd = got_rows;

    table_cache->rows_allocd += got_rows;

    /* adjust the offset of the next chunk */
    if (i < MEM_CHUNKS_IN_TABLE_CACHE - 1) {
      table_cache->chunks[i + 1].offset = chunk->offset + chunk->rows_allocd;
    }

    /* return the first empty row in the newly allocated
    chunk */
    row = chunk->base;
  } else {
    char *chunk_start;
    ulint offset;

    /* there is an empty row, no need to allocate new
    chunks */

    /* find the first chunk that contains allocated but
    empty/unused rows */
    for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {
      if (table_cache->chunks[i].offset + table_cache->chunks[i].rows_allocd >
          table_cache->rows_used) {
        break;
      }
    }

    /* i == MEM_CHUNKS_IN_TABLE_CACHE means that all chunks
    are full, but
    table_cache->rows_used != table_cache->rows_allocd means
    exactly the opposite - there are allocated but
    empty/unused rows :-X */
    ut_a(i < MEM_CHUNKS_IN_TABLE_CACHE);

    chunk_start = (char *)table_cache->chunks[i].base;
    offset = table_cache->rows_used - table_cache->chunks[i].offset;

    row = chunk_start + offset * table_cache->row_size;
  }

  table_cache->rows_used++;

  return (row);
}

#ifdef UNIV_DEBUG
/** Validates a row in the locks cache.
 @return true if valid */
static bool i_s_locks_row_validate(
    const i_s_locks_row_t *row) /*!< in: row to validate */
{
  ut_ad(row->lock_immutable_id != 0);
  ut_ad(row->lock_trx_immutable_id != 0);
  ut_ad(row->lock_table_id != 0);

  if (row->lock_space == SPACE_UNKNOWN) {
    ut_ad(row->lock_page == FIL_NULL);
    ut_ad(row->lock_rec == ULINT_UNDEFINED);
  } else {
    ut_ad(row->lock_page != FIL_NULL);
    ut_ad(row->lock_rec != ULINT_UNDEFINED);
  }

  return true;
}
#endif /* UNIV_DEBUG */

/** Fills i_s_trx_row_t object.
 If memory can not be allocated then false is returned.
 @return false if allocation fails */
static bool fill_trx_row(
    i_s_trx_row_t *row,                        /*!< out: result object
                                               that's filled */
    const trx_t *trx,                          /*!< in: transaction to
                                               get data from */
    const i_s_locks_row_t *requested_lock_row, /*!< in: pointer to the
                                            corresponding row in
                                            innodb_locks if trx is
                                            waiting or NULL if trx
                                            is not waiting */
    trx_i_s_cache_t *cache)                    /*!< in/out: cache into
                                               which to copy volatile
                                               strings */
{
  size_t stmt_len;
  const char *s;

  /* We are going to read various trx->lock fields protected by trx->mutex */
  ut_ad(trx_mutex_own(trx));
  /* We are going to read TRX_WEIGHT, lock_number_of_rows_locked() and
  lock_number_of_tables_locked() which requires latching the lock_sys.
  Also, we need it to avoid reading temporary NULL value set to wait_lock by a
  B-tree page reorganization. */
  ut_ad(locksys::owns_exclusive_global_latch());

  row->trx_id = trx_get_id_for_print(trx);
  row->trx_started = trx->start_time.load(std::memory_order_relaxed);
  row->trx_state = trx_get_que_state_str(trx);
  row->requested_lock_row = requested_lock_row;
  ut_ad(requested_lock_row == nullptr ||
        i_s_locks_row_validate(requested_lock_row));

  if (trx->lock.wait_lock != nullptr) {
    ut_a(requested_lock_row != nullptr);
    row->trx_wait_started = trx->lock.wait_started;
  } else {
    ut_a(requested_lock_row == nullptr);
    row->trx_wait_started = {};
  }

  row->trx_weight = static_cast<uintmax_t>(TRX_WEIGHT(trx));
  if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
    row->trx_schedule_weight.second = trx->lock.schedule_weight.load();
    row->trx_schedule_weight.first = true;
  } else {
    row->trx_schedule_weight.first = false;
  }

  if (trx->mysql_thd == nullptr) {
    /* For internal transactions e.g., purge and transactions
    being recovered at startup there is no associated MySQL
    thread data structure. */
    row->trx_mysql_thread_id = 0;
    row->trx_query = nullptr;
    goto thd_done;
  }

  row->trx_mysql_thread_id = thd_get_thread_id(trx->mysql_thd);

  char query[TRX_I_S_TRX_QUERY_MAX_LEN + 1];
  stmt_len = innobase_get_stmt_safe(trx->mysql_thd, query, sizeof(query));

  if (stmt_len > 0) {
    row->trx_query = static_cast<const char *>(ha_storage_put_memlim(
        cache->storage, query, stmt_len + 1, MAX_ALLOWED_FOR_STORAGE(cache)));

    row->trx_query_cs = innobase_get_charset(trx->mysql_thd);

    if (row->trx_query == nullptr) {
      return false;
    }
  } else {
    row->trx_query = nullptr;
  }

thd_done:
  s = trx->op_info;

  if (s != nullptr && s[0] != '\0') {
    TRX_I_S_STRING_COPY(s, row->trx_operation_state,
                        TRX_I_S_TRX_OP_STATE_MAX_LEN, cache);

    if (row->trx_operation_state == nullptr) {
      return false;
    }
  } else {
    row->trx_operation_state = nullptr;
  }

  row->trx_tables_in_use = trx->n_mysql_tables_in_use;

  row->trx_tables_locked = lock_number_of_tables_locked(trx);

  row->trx_lock_structs = UT_LIST_GET_LEN(trx->lock.trx_locks);

  row->trx_lock_memory_bytes = mem_heap_get_size(trx->lock.lock_heap);

  row->trx_rows_locked = lock_number_of_rows_locked(&trx->lock);

  row->trx_rows_modified = trx->undo_no;

  row->trx_concurrency_tickets = trx->n_tickets_to_enter_innodb;

  switch (trx->isolation_level) {
    case TRX_ISO_READ_UNCOMMITTED:
      row->trx_isolation_level = "READ UNCOMMITTED";
      break;
    case TRX_ISO_READ_COMMITTED:
      row->trx_isolation_level = "READ COMMITTED";
      break;
    case TRX_ISO_REPEATABLE_READ:
      row->trx_isolation_level = "REPEATABLE READ";
      break;
    case TRX_ISO_SERIALIZABLE:
      row->trx_isolation_level = "SERIALIZABLE";
      break;
    /* Should not happen as TRX_ISO_READ_COMMITTED is default */
    default:
      row->trx_isolation_level = "UNKNOWN";
  }

  row->trx_unique_checks = trx->check_unique_secondary;

  row->trx_foreign_key_checks = trx->check_foreigns;

  s = trx->detailed_error;

  if (s != nullptr && s[0] != '\0') {
    TRX_I_S_STRING_COPY(s, row->trx_foreign_key_error,
                        TRX_I_S_TRX_FK_ERROR_MAX_LEN, cache);

    if (row->trx_foreign_key_error == nullptr) {
      return false;
    }
  } else {
    row->trx_foreign_key_error = nullptr;
  }

  row->trx_has_search_latch = trx->has_search_latch;

  row->trx_is_read_only = trx->read_only;

  row->trx_is_autocommit_non_locking = trx_is_autocommit_non_locking(trx);

  return true;
}

/** Format the nth field of "rec" and put it in "buf". The result is always
 NUL-terminated. Returns the number of bytes that were written to "buf"
 (including the terminating NUL).
 @return end of the result */
static ulint put_nth_field(
    char *buf,                 /*!< out: buffer */
    ulint buf_size,            /*!< in: buffer size in bytes */
    ulint n,                   /*!< in: number of field */
    const dict_index_t *index, /*!< in: index */
    const rec_t *rec,          /*!< in: record */
    const ulint *offsets)      /*!< in: record offsets, returned
                               by rec_get_offsets() */
{
  const byte *data;
  ulint data_len;
  dict_field_t *dict_field;
  ulint ret;

  ut_ad(rec_offs_validate(rec, nullptr, offsets));

  if (buf_size == 0) {
    return (0);
  }

  ret = 0;

  if (n > 0) {
    /* we must append ", " before the actual data */

    if (buf_size < 3) {
      buf[0] = '\0';
      return (1);
    }

    memcpy(buf, ", ", 3);

    buf += 2;
    buf_size -= 2;
    ret += 2;
  }

  /* now buf_size >= 1 */

  /* Here any field must be part of index key, which should not be
  added instantly, so no default value */
  ut_ad(!rec_offs_nth_default(index, offsets, n));

  data = rec_get_nth_field(index, rec, offsets, n, &data_len);

  dict_field = index->get_field(n);

  ret +=
      row_raw_format((const char *)data, data_len, dict_field, buf, buf_size);

  return (ret);
}

/** Fill performance schema lock data.
Create a string that represents the LOCK_DATA
column, for a given lock record.
@param[out]     lock_data       Lock data string
@param[in]      lock            Lock to inspect
@param[in]      heap_no         Lock heap number
@param[in]      container       Data container to fill
*/
void p_s_fill_lock_data(const char **lock_data, const lock_t *lock,
                        ulint heap_no,
                        PSI_server_data_lock_container *container) {
  ut_a(lock_get_type(lock) == LOCK_REC);

  switch (heap_no) {
    case PAGE_HEAP_NO_INFIMUM:
      *lock_data = "infimum pseudo-record";
      return;
    case PAGE_HEAP_NO_SUPREMUM:
      *lock_data = "supremum pseudo-record";
      return;
  }

  mtr_t mtr;

  const buf_block_t *block;
  const page_t *page;
  const rec_t *rec;
  const dict_index_t *index;
  ulint n_fields;
  char buf[TRX_I_S_LOCK_DATA_MAX_LEN];
  ulint buf_used;
  ulint i;
  Rec_offsets rec_offsets;

  mtr_start(&mtr);

  block = buf_page_try_get(lock_rec_get_page_id(lock), UT_LOCATION_HERE, &mtr);

  if (block == nullptr) {
    *lock_data = nullptr;

    mtr_commit(&mtr);

    return;
  }

  page = reinterpret_cast<const page_t *>(buf_block_get_frame(block));

  rec = page_find_rec_with_heap_no(page, heap_no);

  index = lock_rec_get_index(lock);

  n_fields = dict_index_get_n_unique_in_tree(index);

  ut_a(n_fields > 0);

  const ulint *offsets = rec_offsets.compute(rec, index);

  /* format and store the data */

  buf_used = 0;
  for (i = 0; i < n_fields; i++) {
    buf_used += put_nth_field(buf + buf_used, sizeof(buf) - buf_used, i, index,
                              rec, offsets) -
                1;
  }

  *lock_data = container->cache_string(buf);

  mtr_commit(&mtr);
}

void fill_locks_row(i_s_locks_row_t *row, const lock_t *lock, ulint heap_no) {
  row->lock_immutable_id = lock_get_immutable_id(lock);
  row->lock_trx_immutable_id = lock_get_trx_immutable_id(lock);
  switch (lock_get_type(lock)) {
    case LOCK_REC: {
      const auto page_id = lock_rec_get_page_id(lock);
      row->lock_space = page_id.space();
      row->lock_page = page_id.page_no();
      row->lock_rec = heap_no;

      break;
    }
    case LOCK_TABLE:

      row->lock_space = SPACE_UNKNOWN;
      row->lock_page = FIL_NULL;
      row->lock_rec = ULINT_UNDEFINED;

      break;
    default:
      ut_error;
  }

  row->lock_table_id = lock_get_table_id(lock);

  ut_ad(i_s_locks_row_validate(row));
}

/** Adds new element to the locks cache, enlarging it if necessary.
 Returns a pointer to the added row. If the row is already present then
 no row is added and a pointer to the existing row is returned.
 If row can not be allocated then NULL is returned.
 @return row */
static i_s_locks_row_t *add_lock_to_cache(
    trx_i_s_cache_t *cache, /*!< in/out: cache */
    const lock_t *lock,     /*!< in: the element to add */
    ulint heap_no)          /*!< in: lock's record number
                            or ULINT_UNDEFINED if the lock
                            is a table lock */
{
  i_s_locks_row_t *dst_row;

  dst_row = (i_s_locks_row_t *)table_cache_create_empty_row(
      &cache->innodb_locks, cache);

  /* memory could not be allocated */
  if (dst_row == nullptr) {
    return (nullptr);
  }

  fill_locks_row(dst_row, lock, heap_no);

  ut_ad(i_s_locks_row_validate(dst_row));
  return (dst_row);
}

/** Adds transaction's relevant (important) locks to cache.
 If the transaction is waiting, then the wait lock is added to
 innodb_locks and a pointer to the added row is returned in
 requested_lock_row, otherwise requested_lock_row is set to NULL.
 If rows can not be allocated then false is returned and the value of
 requested_lock_row is undefined.
 @return false if allocation fails */
static bool add_trx_relevant_locks_to_cache(
    trx_i_s_cache_t *cache,               /*!< in/out: cache */
    const trx_t *trx,                     /*!< in: transaction */
    i_s_locks_row_t **requested_lock_row) /*!< out: pointer to the
                               requested lock row, or NULL or
                               undefined */
{
  /* We are about to iterate over locks for various tables/rows so we can not
  narrow the required latch to any specific shard, and thus require exclusive
  access to lock_sys. This is also needed to avoid observing NULL temporarily
  set to wait_lock during B-tree page reorganization. */
  ut_ad(locksys::owns_exclusive_global_latch());

  /* If transaction is waiting we add the wait lock and all locks
  from another transactions that are blocking the wait lock. */
  if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
    const lock_t *curr_lock;
    ulint wait_lock_heap_no;
    i_s_locks_row_t *blocking_lock_row;
    lock_queue_iterator_t iter;
    const lock_t *wait_lock = trx->lock.wait_lock;
    ut_a(wait_lock != nullptr);

    wait_lock_heap_no = wait_lock_get_heap_no(wait_lock);

    /* add the requested lock */
    *requested_lock_row =
        add_lock_to_cache(cache, wait_lock, wait_lock_heap_no);

    /* memory could not be allocated */
    if (*requested_lock_row == nullptr) {
      return false;
    }

    /* then iterate over the locks before the wait lock and
    add the ones that are blocking it */

    lock_queue_iterator_reset(&iter, wait_lock, ULINT_UNDEFINED);
    locksys::Trx_locks_cache wait_lock_cache{};
    for (curr_lock = lock_queue_iterator_get_prev(&iter); curr_lock != nullptr;
         curr_lock = lock_queue_iterator_get_prev(&iter)) {
      if (locksys::has_to_wait(wait_lock, curr_lock, wait_lock_cache)) {
        /* add the lock that is
        blocking trx->lock.wait_lock */
        blocking_lock_row = add_lock_to_cache(cache, curr_lock,
                                              /* heap_no is the same
                                              for the wait and waited
                                              locks */
                                              wait_lock_heap_no);

        /* memory could not be allocated */
        if (blocking_lock_row == nullptr) {
          return false;
        }
      }
    }
  } else {
    *requested_lock_row = nullptr;
  }

  return true;
}

/** Checks if the cache can safely be updated.
 @return true if can be updated */
static bool can_cache_be_updated(trx_i_s_cache_t *cache) /*!< in: cache */
{
  /* Here we read cache->last_read without acquiring its mutex
  because last_read is only updated when a shared rw lock on the
  whole cache is being held (see trx_i_s_cache_end_read()) and
  we are currently holding an exclusive rw lock on the cache.
  So it is not possible for last_read to be updated while we are
  reading it. */

  ut_ad(rw_lock_own(cache->rw_lock, RW_LOCK_X));

  /** The minimum time that a cache must not be updated after it has been
  read for the last time. We use this technique to ensure that SELECTs which
  join several INFORMATION SCHEMA tables read the same version of the cache. */
  constexpr std::chrono::milliseconds cache_min_idle_time{100};

  return std::chrono::steady_clock::now() - cache->last_read.load() >
         cache_min_idle_time;
}

/** Declare a cache empty, preparing it to be filled up. Not all resources
 are freed because they can be reused. */
static void trx_i_s_cache_clear(
    trx_i_s_cache_t *cache) /*!< out: cache to clear */
{
  cache->innodb_trx.rows_used = 0;
  cache->innodb_locks.rows_used = 0;

  ha_storage_empty(&cache->storage);
}

/** Fetches the data needed to fill the 3 INFORMATION SCHEMA tables into the
 table cache buffer. Cache must be locked for write.
@param[in,out]  cache       the cache
@param[in]      trx_list    the list to scan
*/
template <typename Trx_list>
static void fetch_data_into_cache_low(trx_i_s_cache_t *cache,
                                      Trx_list *trx_list) {
  /* We are going to iterate over many different shards of lock_sys so we need
  exclusive access */
  ut_ad(locksys::owns_exclusive_global_latch());
  constexpr bool rw_trx_list =
      std::is_same<Trx_list, decltype(trx_sys->rw_trx_list)>::value;

  static_assert(
      rw_trx_list ||
          std::is_same<Trx_list, decltype(trx_sys->mysql_trx_list)>::value,
      "only rw_trx_list and mysql_trx_list are supported");

  /* Iterate over the transaction list and add each one
  to innodb_trx's cache. We also add all locks that are relevant
  to each transaction into innodb_locks' and innodb_lock_waits'
  caches. */

  for (auto trx : *trx_list) {
    i_s_trx_row_t *trx_row;
    i_s_locks_row_t *requested_lock_row;

    trx_mutex_enter(trx);

    /* Note: Read only transactions that modify temporary
    tables have a transaction ID.

    Note: auto-commit non-locking read-only transactions
    can have trx->state set from NOT_STARTED to ACTIVE and
    then from ACTIVE to NOT_STARTED with neither trx_sys->mutex
    nor trx->mutex acquired. However, as long as these transactions
    are members of mysql_trx_list they are not freed. For such
    transactions "trx_was_started(trx)" might be considered random,
    but whatever is its result, the code below handles that well
    (transaction won't release locks until its trx->mutex is acquired).

    Note: locking read-only transactions can have trx->state set from
    NOT_STARTED to ACTIVE with neither trx_sys->mutex nor trx->mutex
    acquired. However, such transactions need to be marked as COMMITTED
    before trx->state is set to NOT_STARTED and that is protected by the
    trx->mutex. Therefore the assertion assert_trx_nonlocking_or_in_list()
    should hold few lines below (note: the name of the assertion is wrong,
    because it actually checks if the transaction is autocommit nonlocking,
    whereas its name suggests that it only checks if the trx is nonlocking). */
    if (!trx_was_started(trx) ||
        (!rw_trx_list && trx->id != 0 && !trx->read_only)) {
      trx_mutex_exit(trx);
      continue;
    }

    assert_trx_nonlocking_or_in_list(trx);

    ut_ad(trx->in_rw_trx_list == rw_trx_list);

    if (!add_trx_relevant_locks_to_cache(cache, trx, &requested_lock_row)) {
      cache->is_truncated = true;
      trx_mutex_exit(trx);
      return;
    }

    trx_row = reinterpret_cast<i_s_trx_row_t *>(
        table_cache_create_empty_row(&cache->innodb_trx, cache));

    /* memory could not be allocated */
    if (trx_row == nullptr) {
      cache->is_truncated = true;
      trx_mutex_exit(trx);
      return;
    }

    if (!fill_trx_row(trx_row, trx, requested_lock_row, cache)) {
      /* memory could not be allocated */
      --cache->innodb_trx.rows_used;
      cache->is_truncated = true;
      trx_mutex_exit(trx);
      return;
    }

    trx_mutex_exit(trx);
  }
}

/** Fetches the data needed to fill the 3 INFORMATION SCHEMA tables into the
 table cache buffer. Cache must be locked for write. */
static void fetch_data_into_cache(trx_i_s_cache_t *cache) /*!< in/out: cache */
{
  /* We are going to iterate over many different shards of lock_sys so we need
  exclusive access */
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_ad(trx_sys_mutex_own());

  trx_i_s_cache_clear(cache);

  /* Capture the state of the read-write transactions. This includes
  internal transactions too. They are not on mysql_trx_list */
  fetch_data_into_cache_low(cache, &trx_sys->rw_trx_list);

  /* Capture the state of the read-only active transactions */
  fetch_data_into_cache_low(cache, &trx_sys->mysql_trx_list);

  cache->is_truncated = false;
}

/** Update the transactions cache if it has not been read for some time.
 Called from handler/i_s.cc.
 @return 0 - fetched, 1 - not */
int trx_i_s_possibly_fetch_data_into_cache(
    trx_i_s_cache_t *cache) /*!< in/out: cache */
{
  if (!can_cache_be_updated(cache)) {
    return (1);
  }

  {
    /* We need to read trx_sys and record/table lock queues */
    locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};

    trx_sys_mutex_enter();

    fetch_data_into_cache(cache);

    trx_sys_mutex_exit();
  }

  return (0);
}

bool trx_i_s_cache_is_truncated(trx_i_s_cache_t *cache) {
  return (cache->is_truncated);
}

/** Initialize INFORMATION SCHEMA trx related cache. */
void trx_i_s_cache_init(trx_i_s_cache_t *cache) /*!< out: cache to init */
{
  /* The latching is done in the following order:
  acquire trx_i_s_cache_t::rw_lock, X
  acquire locksys exclusive global latch
  acquire trx_sys mutex
  release trx_sys mutex
  release locksys exclusive global latch
  release trx_i_s_cache_t::rw_lock
  acquire trx_i_s_cache_t::rw_lock, S
  release trx_i_s_cache_t::rw_lock */

  cache->rw_lock = static_cast<rw_lock_t *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*cache->rw_lock)));

  rw_lock_create(trx_i_s_cache_lock_key, cache->rw_lock,
                 LATCH_ID_TRX_I_S_CACHE);

  cache->last_read = std::chrono::steady_clock::time_point{};

  table_cache_init(&cache->innodb_trx, sizeof(i_s_trx_row_t));
  table_cache_init(&cache->innodb_locks, sizeof(i_s_locks_row_t));

  cache->storage =
      ha_storage_create(CACHE_STORAGE_INITIAL_SIZE, CACHE_STORAGE_HASH_CELLS);

  cache->mem_allocd = 0;

  cache->is_truncated = false;
}

/** Free the INFORMATION SCHEMA trx related cache. */
void trx_i_s_cache_free(trx_i_s_cache_t *cache) /*!< in, own: cache to free */
{
  rw_lock_free(cache->rw_lock);
  ut::free(cache->rw_lock);
  cache->rw_lock = nullptr;

  ha_storage_free(cache->storage);
  table_cache_free(&cache->innodb_trx);
  table_cache_free(&cache->innodb_locks);
}

/** Issue a shared/read lock on the tables cache. */
void trx_i_s_cache_start_read(trx_i_s_cache_t *cache) /*!< in: cache */
{
  rw_lock_s_lock(cache->rw_lock, UT_LOCATION_HERE);
}

/** Release a shared/read lock on the tables cache. */
void trx_i_s_cache_end_read(trx_i_s_cache_t *cache) /*!< in: cache */
{
  ut_ad(rw_lock_own(cache->rw_lock, RW_LOCK_S));

  /* update cache last read time */
  cache->last_read.store(std::chrono::steady_clock::now());

  rw_lock_s_unlock(cache->rw_lock);
}

/** Issue an exclusive/write lock on the tables cache. */
void trx_i_s_cache_start_write(trx_i_s_cache_t *cache) /*!< in: cache */
{
  rw_lock_x_lock(cache->rw_lock, UT_LOCATION_HERE);
}

/** Release an exclusive/write lock on the tables cache. */
void trx_i_s_cache_end_write(trx_i_s_cache_t *cache) /*!< in: cache */
{
  ut_ad(rw_lock_own(cache->rw_lock, RW_LOCK_X));

  rw_lock_x_unlock(cache->rw_lock);
}

/** Selects a INFORMATION SCHEMA table cache from the whole cache.
 @return table cache */
static i_s_table_cache_t *cache_select_table(
    trx_i_s_cache_t *cache, /*!< in: whole cache */
    enum i_s_table table)   /*!< in: which table */
{
  i_s_table_cache_t *table_cache;

  ut_ad(rw_lock_own(cache->rw_lock, RW_LOCK_S) ||
        rw_lock_own(cache->rw_lock, RW_LOCK_X));

  switch (table) {
    case I_S_INNODB_TRX:
      table_cache = &cache->innodb_trx;
      break;
    default:
      ut_error;
  }

  return (table_cache);
}

/** Retrieves the number of used rows in the cache for a given
 INFORMATION SCHEMA table.
 @return number of rows */
ulint trx_i_s_cache_get_rows_used(trx_i_s_cache_t *cache, /*!< in: cache */
                                  enum i_s_table table) /*!< in: which table */
{
  i_s_table_cache_t *table_cache;

  table_cache = cache_select_table(cache, table);

  return (table_cache->rows_used);
}

/** Retrieves the nth row (zero-based) in the cache for a given
 INFORMATION SCHEMA table.
 @return row */
void *trx_i_s_cache_get_nth_row(trx_i_s_cache_t *cache, /*!< in: cache */
                                enum i_s_table table,   /*!< in: which table */
                                ulint n)                /*!< in: row number */
{
  i_s_table_cache_t *table_cache;
  ulint i;
  void *row;

  table_cache = cache_select_table(cache, table);

  ut_a(n < table_cache->rows_used);

  row = nullptr;

  for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {
    if (table_cache->chunks[i].offset + table_cache->chunks[i].rows_allocd >
        n) {
      row = (char *)table_cache->chunks[i].base +
            (n - table_cache->chunks[i].offset) * table_cache->row_size;
      break;
    }
  }

  ut_a(row != nullptr);

  return (row);
}
constexpr const char *LOCK_RECORD_ID_FORMAT =
    UINT64PF ":" SPACE_ID_PF ":" PAGE_NO_PF ":" ULINTPF ":" UINT64PF;
constexpr const char *LOCK_TABLE_ID_FORMAT = UINT64PF ":" UINT64PF ":" UINT64PF;
/** Crafts a lock id string from a i_s_locks_row_t object. Returns its
 second argument. This function aborts if there is not enough space in
 lock_id. Be sure to provide at least TRX_I_S_LOCK_ID_MAX_LEN + 1 if you
 want to be 100% sure that it will not abort.
 @return resulting lock id */
char *trx_i_s_create_lock_id(
    const i_s_locks_row_t *row, /*!< in: innodb_locks row */
    char *lock_id,              /*!< out: resulting lock_id */
    ulint lock_id_size)         /*!< in: size of the lock id
                           buffer */
{
  int res_len;

  /* please adjust TRX_I_S_LOCK_ID_MAX_LEN if you change this */

  if (row->lock_space != SPACE_UNKNOWN) {
    /* record lock */
    res_len = snprintf(lock_id, lock_id_size, LOCK_RECORD_ID_FORMAT,
                       row->lock_trx_immutable_id, row->lock_space,
                       row->lock_page, row->lock_rec, row->lock_immutable_id);
  } else {
    /* table lock */
    res_len = snprintf(lock_id, lock_id_size, LOCK_TABLE_ID_FORMAT,
                       row->lock_trx_immutable_id, row->lock_table_id,
                       row->lock_immutable_id);
  }

  /* the typecast is safe because snprintf(3) never returns
  negative result */
  ut_a(res_len >= 0);
  ut_a((ulint)res_len < lock_id_size);

  return (lock_id);
}

int trx_i_s_parse_lock_id(const char *lock_id, i_s_locks_row_t *row) {
  if (sscanf(lock_id, LOCK_RECORD_ID_FORMAT, &row->lock_trx_immutable_id,
             &row->lock_space, &row->lock_page, &row->lock_rec,
             &row->lock_immutable_id) == 5) {
    return LOCK_REC;
  }
  if (sscanf(lock_id, LOCK_TABLE_ID_FORMAT, &row->lock_trx_immutable_id,
             &row->lock_table_id, &row->lock_immutable_id) == 3) {
    return LOCK_TABLE;
  }
  static_assert(LOCK_TABLE != 0 && LOCK_REC != 0);
  return 0;
}
