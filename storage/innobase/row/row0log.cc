/*****************************************************************************

Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

/** @file row/row0log.cc
 Modification log for online index creation and online table rebuild

 Created 2011-05-26 Marko Makela
 *******************************************************/

#include "row0log.h"

#include "my_config.h"

#include <fcntl.h>
#include <algorithm>
#include <map>

#include "data0data.h"
#include "ddl0ddl.h"
#include "handler0alter.h"
#include "lob0lob.h"
#include "log0chkp.h"
#include "que0que.h"
#include "row0ext.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "srv0mon.h"
#include "trx0rec.h"
#include "ut0new.h"
#include "ut0stage.h"

#include "my_dbug.h"

/** Table row modification operations during online table rebuild.
Delete-marked records are not copied to the rebuilt table. */
enum row_tab_op {
  /** Insert a record */
  ROW_T_INSERT = 0x41,
  /** Update a record in place */
  ROW_T_UPDATE,
  /** Delete (purge) a record */
  ROW_T_DELETE
};

/** Index record modification operations during online index creation */
enum row_op {
  /** Insert a record */
  ROW_OP_INSERT = 0x61,
  /** Delete a record */
  ROW_OP_DELETE
};

/** Size of the modification log entry header, in bytes */
/**op, extra_size*/
constexpr uint32_t ROW_LOG_HEADER_SIZE = 2;

/** Log block for modifications during online ALTER TABLE */
struct row_log_buf_t {
  /** File block buffer */
  byte *block;

  /** Buffer for accessing a record that spans two blocks */
  ddl::mrec_buf_t buf;

  /** Current position in blocks */
  ulint blocks;

  /** Current position within block */
  ulint bytes;

  /** logical position, in bytes from the start of the row_log_table log; 0 for
  row_log_online_op() and row_log_apply(). */
  ulonglong total;
};

/** Tracks BLOB allocation during online ALTER TABLE */
class row_log_table_blob_t {
 public:
  /** Constructor (declaring a BLOB freed)
  @param offset_arg row_log_t::tail::total */
#ifdef UNIV_DEBUG
  row_log_table_blob_t(ulonglong offset_arg)
      : old_offset(0), free_offset(offset_arg), offset(BLOB_FREED) {}
#else  /* UNIV_DEBUG */
  row_log_table_blob_t() : offset(BLOB_FREED) {}
#endif /* UNIV_DEBUG */

  /** Declare a BLOB freed again.
  @param offset_arg row_log_t::tail::total */
#ifdef UNIV_DEBUG
  void blob_free(ulonglong offset_arg)
#else  /* UNIV_DEBUG */
  void blob_free()
#endif /* UNIV_DEBUG */
  {
    ut_ad(offset < offset_arg);
    ut_ad(offset != BLOB_FREED);
    ut_d(old_offset = offset);
    ut_d(free_offset = offset_arg);
    offset = BLOB_FREED;
  }
  /** Declare a freed BLOB reused.
  @param offset_arg row_log_t::tail::total */
  void blob_alloc(ulonglong offset_arg) {
    ut_ad(free_offset <= offset_arg);
    ut_d(old_offset = offset);
    offset = offset_arg;
  }
  /** Determine if a BLOB was freed at a given log position
  @param offset_arg row_log_t::head::total after the log record
  @return true if freed */
  bool is_freed(ulonglong offset_arg) const {
    /* This is supposed to be the offset at the end of the
    current log record. */
    ut_ad(offset_arg > 0);
    /* We should never get anywhere close the magic value. */
    ut_ad(offset_arg < BLOB_FREED);
    return (offset_arg < offset);
  }

 private:
  /** Magic value for a freed BLOB */
  static const ulonglong BLOB_FREED = ~0ULL;
#ifdef UNIV_DEBUG
  /** Old offset, in case a page was freed, reused, freed, ... */
  ulonglong old_offset;
  /** Offset of last blob_free() */
  ulonglong free_offset;
#endif /* UNIV_DEBUG */
  /** Byte offset to the log file */
  ulonglong offset;
};

/** @brief Map of off-page column page numbers to 0 or log byte offsets.

If there is no mapping for a page number, it is safe to access.
If a page number maps to 0, it is an off-page column that has been freed.
If a page number maps to a nonzero number, the number is a byte offset
into the index->online_log, indicating that the page is safe to access
when applying log records starting from that offset. */
typedef std::map<
    page_no_t, row_log_table_blob_t, std::less<page_no_t>,
    ut::allocator<std::pair<const page_no_t, row_log_table_blob_t>>>
    page_no_map;

/** @brief Buffer for logging modifications during online index creation

All modifications to an index that is being created will be logged by
row_log_online_op() to this buffer.

All modifications to a table that is being rebuilt will be logged by
row_log_table_delete(), row_log_table_update(), row_log_table_insert()
to this buffer.

When head.blocks == tail.blocks, the reader will access tail.block
directly. When also head.bytes == tail.bytes, both counts will be
reset to 0 and the file will be truncated. */
struct row_log_t {
  /** File descriptor */
  ddl::Unique_os_file_descriptor file;

  /** Mutex protecting error, max_trx and tail */
  ib_mutex_t mutex;

  /** Map of page numbers of off-page columns that have been freed during
  table-rebuilding ALTER TABLE (row_log_table_*); protected by index->lock
  X-latch only */
  page_no_map *blobs;

  /** Table that is being rebuilt, or NULL when this is a secondary index that
  is being created online */
  dict_table_t *table;

  /** Whether the definition of the PRIMARY KEY has remained the same */
  bool same_pk;

  /** Default values of added columns, or NULL */
  const dtuple_t *add_cols;

  /** Mapping of old column numbers to new ones, or NULL if !table */
  const ulint *col_map;

  /** Error that occurred during online table rebuild */
  dberr_t error;

  /** Biggest observed trx_id in row_log_online_op(); protected by mutex and
  index->lock S-latch, or by index->lock X-latch only */
  trx_id_t max_trx;

  /** writer context; protected by mutex and index->lock S-latch, or by
  index->lock X-latch only */
  row_log_buf_t tail;

  /** Reader context; protected by MDL only; modifiable by
  row_log_apply_ops() */
  row_log_buf_t head;

  /** number of non-virtual column in old table */
  size_t n_old_col;

  /** number of virtual column in old table */
  size_t n_old_vcol;

  /** Where to create temporary file during log operation */
  const char *path;
};

/** Create the file or online log if it does not exist.
@param[in,out] log     online rebuild log
@return true if success, false if not */
[[nodiscard]] static bool row_log_tmpfile(row_log_t *log) {
  DBUG_TRACE;
  if (!log->file.is_open()) {
    log->file = ddl::file_create_low(log->path);
    DBUG_EXECUTE_IF("row_log_tmpfile_fail", log->file.close(););
    if (log->file.is_open()) {
      MONITOR_ATOMIC_INC(MONITOR_ALTER_TABLE_LOG_FILES);
    }
  }

  return log->file.is_open();
}

/** Allocate the memory for the log buffer.
@param[in,out]  log_buf Buffer used for log operation
@return true if success, false if not */
[[nodiscard]] static bool row_log_block_allocate(row_log_buf_t &log_buf) {
  DBUG_TRACE;
  if (log_buf.block == nullptr) {
    DBUG_EXECUTE_IF("simulate_row_log_allocation_failure", return false;);

    log_buf.block = static_cast<uint8_t *>(ut::malloc_large_page_withkey(
        ut::make_psi_memory_key(mem_key_row_log_buf), srv_sort_buf_size,
        ut::fallback_to_normal_page_t{}));

    if (log_buf.block == nullptr) {
      return false;
    }
  }
  return true;
}

/** Free the log buffer.
@param[in,out]  log_buf Buffer used for log operation */
static void row_log_block_free(row_log_buf_t &log_buf) {
  DBUG_TRACE;
  ut::free_large_page(log_buf.block, ut::fallback_to_normal_page_t{});
  log_buf.block = nullptr;
}

/** Logs an operation to a secondary index that is (or was) being created. */
void row_log_online_op(
    dict_index_t *index,   /*!< in/out: index, S or X latched */
    const dtuple_t *tuple, /*!< in: index tuple */
    trx_id_t trx_id)       /*!< in: transaction ID for insert,
                           or 0 for delete */
{
  byte *b;
  ulint extra_size;
  ulint size;
  ulint mrec_size;
  ulint avail_size;
  row_log_t *log;

  ut_ad(dtuple_validate(tuple));
  ut_ad(dtuple_get_n_fields(tuple) == dict_index_get_n_fields(index));
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_S) ||
        rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));

  if (index->is_corrupted()) {
    return;
  }

  ut_ad(dict_index_is_online_ddl(index));

  /* Compute the size of the record. This differs from ddl::buf_encode(),
  because here we do not encode extra_size+1 (and reserve 0 as the
  end-of-chunk marker). */

  size = rec_get_serialize_size(index, tuple->fields, tuple->n_fields, nullptr,
                                &extra_size, MAX_ROW_VERSION);
  ut_ad(size >= extra_size);
  ut_ad(size <= sizeof log->tail.buf);

  mrec_size = ROW_LOG_HEADER_SIZE + (extra_size >= 0x80) + size +
              (trx_id ? DATA_TRX_ID_LEN : 0);

  log = index->online_log;
  mutex_enter(&log->mutex);

  if (trx_id > log->max_trx) {
    log->max_trx = trx_id;
  }

  if (!row_log_block_allocate(log->tail)) {
    log->error = DB_OUT_OF_MEMORY;
    goto err_exit;
  }

  UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);

  ut_ad(log->tail.bytes < srv_sort_buf_size);
  avail_size = srv_sort_buf_size - log->tail.bytes;

  if (mrec_size > avail_size) {
    b = log->tail.buf;
  } else {
    b = log->tail.block + log->tail.bytes;
  }

  if (trx_id != 0) {
    *b++ = ROW_OP_INSERT;
    trx_write_trx_id(b, trx_id);
    b += DATA_TRX_ID_LEN;
  } else {
    *b++ = ROW_OP_DELETE;
  }

  if (extra_size < 0x80) {
    *b++ = (byte)extra_size;
  } else {
    ut_ad(extra_size < 0x8000);
    *b++ = (byte)(0x80 | (extra_size >> 8));
    *b++ = (byte)extra_size;
  }

  rec_serialize_dtuple(b + extra_size, index, tuple->fields, tuple->n_fields,
                       nullptr);
  b += size;

  if (mrec_size >= avail_size) {
    dberr_t err;
    IORequest request(IORequest::ROW_LOG | IORequest::WRITE);
    const os_offset_t byte_offset =
        (os_offset_t)log->tail.blocks * srv_sort_buf_size;

    if (byte_offset + srv_sort_buf_size >= srv_online_max_size) {
      goto write_failed;
    }

    if (mrec_size == avail_size) {
      ut_ad(b == &log->tail.block[srv_sort_buf_size]);
    } else {
      ut_ad(b == log->tail.buf + mrec_size);
      memcpy(log->tail.block + log->tail.bytes, log->tail.buf, avail_size);
    }

    UNIV_MEM_ASSERT_RW(log->tail.block, srv_sort_buf_size);

    if (!row_log_tmpfile(log)) {
      log->error = DB_OUT_OF_MEMORY;
      goto err_exit;
    }

    err = os_file_write_int_fd(request, "(modification log)", log->file.get(),
                               log->tail.block, byte_offset, srv_sort_buf_size);

    log->tail.blocks++;
    if (err != DB_SUCCESS) {
    write_failed:
      /* We set the flag directly instead of
      invoking dict_set_corrupted() here,
      because the index is not "public" yet. */
      index->type |= DICT_CORRUPT;
    }
    UNIV_MEM_INVALID(log->tail.block, srv_sort_buf_size);
    memcpy(log->tail.block, log->tail.buf + avail_size, mrec_size - avail_size);
    log->tail.bytes = mrec_size - avail_size;
  } else {
    log->tail.bytes += mrec_size;
    ut_ad(b == log->tail.block + log->tail.bytes);
  }

  UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);
err_exit:
  mutex_exit(&log->mutex);
}

/** Gets the error status of the online index rebuild log.
 @return DB_SUCCESS or error code */
dberr_t row_log_table_get_error(
    const dict_index_t *index) /*!< in: clustered index of a table
                               that is being rebuilt online */
{
  ut_ad(index->is_clustered());
  ut_ad(dict_index_is_online_ddl(index));
  return (index->online_log->error);
}

/** Starts logging an operation to a table that is being rebuilt.
 @return pointer to log, or NULL if no logging is necessary */
[[nodiscard]] static byte *row_log_table_open(
    row_log_t *log, /*!< in/out: online rebuild log */
    ulint size,     /*!< in: size of log record */
    ulint *avail)   /*!< out: available size for log record */
{
  mutex_enter(&log->mutex);

  UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);

  if (log->error != DB_SUCCESS) {
  err_exit:
    mutex_exit(&log->mutex);
    return (nullptr);
  }

  if (!row_log_block_allocate(log->tail)) {
    log->error = DB_OUT_OF_MEMORY;
    goto err_exit;
  }

  ut_ad(log->tail.bytes < srv_sort_buf_size);
  *avail = srv_sort_buf_size - log->tail.bytes;

  if (size > *avail) {
    /* Make sure log->tail.buf is large enough */
    ut_ad(size <= sizeof log->tail.buf);
    return (log->tail.buf);
  } else {
    return (log->tail.block + log->tail.bytes);
  }
}

/** Stops logging an operation to a table that is being rebuilt.
 @param[in,out] log online rebuild log
 @param[in] b end of log record
 @param[in] size size od log record
 @param[in] avail available size for log record */
static void row_log_table_close_func(row_log_t *log,
                                     IF_DEBUG(const byte *b, ) ulint size,
                                     ulint avail) {
  ut_ad(mutex_own(&log->mutex));

  if (size >= avail) {
    dberr_t err;
    IORequest request(IORequest::ROW_LOG | IORequest::WRITE);
    const os_offset_t byte_offset =
        (os_offset_t)log->tail.blocks * srv_sort_buf_size;

    if (byte_offset + srv_sort_buf_size >= srv_online_max_size) {
      goto write_failed;
    }

    if (size == avail) {
      ut_ad(b == &log->tail.block[srv_sort_buf_size]);
    } else {
      ut_ad(b == log->tail.buf + size);
      memcpy(log->tail.block + log->tail.bytes, log->tail.buf, avail);
    }

    UNIV_MEM_ASSERT_RW(log->tail.block, srv_sort_buf_size);

    if (!row_log_tmpfile(log)) {
      log->error = DB_OUT_OF_MEMORY;
      goto err_exit;
    }

    err = os_file_write_int_fd(request, "(modification log)", log->file.get(),
                               log->tail.block, byte_offset, srv_sort_buf_size);

    log->tail.blocks++;
    if (err != DB_SUCCESS) {
    write_failed:
      log->error = DB_ONLINE_LOG_TOO_BIG;
    }
    UNIV_MEM_INVALID(log->tail.block, srv_sort_buf_size);
    memcpy(log->tail.block, log->tail.buf + avail, size - avail);
    log->tail.bytes = size - avail;
  } else {
    log->tail.bytes += size;
    ut_ad(b == log->tail.block + log->tail.bytes);
  }

  log->tail.total += size;
  UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);
err_exit:
  mutex_exit(&log->mutex);
}

static inline void row_log_table_close(row_log_t *log,
                                       const byte *b [[maybe_unused]],
                                       ulint size, ulint avail) {
  row_log_table_close_func(log, IF_DEBUG(b, ) size, avail);
}

/** Check whether a virtual column is indexed in the new table being
created during alter table
@param[in]      index   cluster index
@param[in]      v_no    virtual column number
@return true if it is indexed, else false */
bool row_log_col_is_indexed(const dict_index_t *index, ulint v_no) {
  return (
      dict_table_get_nth_v_col(index->online_log->table, v_no)->m_col.ord_part);
}

/** Logs a delete operation to a table that is being rebuilt.
 This will be merged in row_log_table_apply_delete(). */
void row_log_table_delete(
    const rec_t *rec,       /*!< in: clustered index leaf page record,
                            page X-latched */
    const dtuple_t *ventry, /*!< in: dtuple holding virtual column info */
    dict_index_t *index,    /*!< in/out: clustered index, S-latched
                            or X-latched */
    const ulint *offsets,   /*!< in: rec_get_offsets(rec,index) */
    const byte *sys)        /*!< in: DB_TRX_ID,DB_ROLL_PTR that should
                            be logged, or NULL to use those in rec */
{
  ulint old_pk_extra_size;
  ulint old_pk_size;
  ulint mrec_size;
  ulint avail_size;
  mem_heap_t *heap = nullptr;
  const dtuple_t *old_pk;

  if (index->is_corrupted() || !dict_index_is_online_ddl(index) ||
      index->online_log->error != DB_SUCCESS) {
    return;
  }

  ut_ad(index->is_clustered());
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(rec_offs_n_fields(offsets) == dict_index_get_n_fields(index));
  ut_ad(rec_offs_size(offsets) <= sizeof index->online_log->tail.buf);
  ut_ad(rw_lock_own_flagged(&index->lock,
                            RW_LOCK_FLAG_S | RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX));

  dict_table_t *new_table = index->online_log->table;
  dict_index_t *new_index = new_table->first_index();

  ut_ad(new_index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(new_index));

  /* Create the tuple PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR in new_table. */
  if (index->online_log->same_pk) {
    dtuple_t *tuple;
    ut_ad(new_index->n_uniq == index->n_uniq);

    /* The PRIMARY KEY and DB_TRX_ID,DB_ROLL_PTR are in the first
    fields of the record. */
    heap = mem_heap_create(
        DATA_TRX_ID_LEN + DTUPLE_EST_ALLOC(new_index->n_uniq + 2),
        UT_LOCATION_HERE);
    old_pk = tuple = dtuple_create(heap, new_index->n_uniq + 2);
    dict_index_copy_types(tuple, new_index, tuple->n_fields);
    dtuple_set_n_fields_cmp(tuple, new_index->n_uniq);

    for (ulint i = 0; i < dtuple_get_n_fields(tuple); i++) {
      ulint len;
      const void *field = rec_get_nth_field(nullptr, rec, offsets, i, &len);
      dfield_t *dfield = dtuple_get_nth_field(tuple, i);
      ut_ad(len != UNIV_SQL_NULL);
      ut_ad(!rec_offs_nth_extern(index, offsets, i));
      dfield_set_data(dfield, field, len);
    }

    if (sys) {
      dfield_set_data(dtuple_get_nth_field(tuple, new_index->n_uniq), sys,
                      DATA_TRX_ID_LEN);
      dfield_set_data(dtuple_get_nth_field(tuple, new_index->n_uniq + 1),
                      sys + DATA_TRX_ID_LEN, DATA_ROLL_PTR_LEN);
    }
  } else {
    /* The PRIMARY KEY has changed. Translate the tuple. */
    old_pk = row_log_table_get_pk(rec, index, offsets, nullptr, &heap);

    if (!old_pk) {
      ut_ad(index->online_log->error != DB_SUCCESS);
      if (heap) {
        goto func_exit;
      }
      return;
    }
  }

  ut_ad(DATA_TRX_ID_LEN ==
        dtuple_get_nth_field(old_pk, old_pk->n_fields - 2)->len);
  ut_ad(DATA_ROLL_PTR_LEN ==
        dtuple_get_nth_field(old_pk, old_pk->n_fields - 1)->len);
  old_pk_size =
      rec_get_serialize_size(new_index, old_pk->fields, old_pk->n_fields,
                             nullptr, &old_pk_extra_size, MAX_ROW_VERSION);
  ut_ad(old_pk_extra_size < 0x100);

  /* 2 = 1 (extra_size) + at least 1 byte payload */
  mrec_size = 2 + old_pk_size;

  /* Check if we need to log virtual column data */
  if (ventry->n_v_fields > 0) {
    ulint v_extra;
    mrec_size += rec_get_serialize_size(new_index, nullptr, 0, ventry, &v_extra,
                                        MAX_ROW_VERSION);
  }

  if (byte *b = row_log_table_open(index->online_log, mrec_size, &avail_size)) {
    *b++ = ROW_T_DELETE;
    *b++ = static_cast<byte>(old_pk_extra_size);

    rec_serialize_dtuple(b + old_pk_extra_size, new_index, old_pk->fields,
                         old_pk->n_fields, nullptr);

    b += old_pk_size;

    /* log virtual columns */
    if (ventry->n_v_fields > 0) {
      rec_serialize_dtuple(b, new_index, nullptr, 0, ventry);
      b += mach_read_from_2(b);
    } else if (index->table->n_v_cols) {
      mach_write_to_2(b, 2);
      b += 2;
    }

    row_log_table_close(index->online_log, b, mrec_size, avail_size);
  }

func_exit:
  mem_heap_free(heap);
}

/** Logs an insert or update to a table that is being rebuilt.
@param[in]      rec             clustered index leaf page record in
                                ROW_FORMAT=REDUNDANT, page X-latched
@param[in]      ventry          dtuple holding virtual column info or NULL
@param[in]      o_ventry        old dtuple holding virtual column info or NULL
@param[in,out]  index           clustered index, S-latched or X-latched
@param[in]      insert          true if insert, false if update
@param[in]      old_pk          old PRIMARY KEY value (if !insert and a PRIMARY
                                KEY is being created)
@param[in]      new_index       clustered index of the new table, not latched */
static void row_log_table_low_redundant(const rec_t *rec,
                                        const dtuple_t *ventry,
                                        const dtuple_t *o_ventry,
                                        dict_index_t *index, bool insert,
                                        const dtuple_t *old_pk,
                                        const dict_index_t *new_index) {
  ulint num_v = ventry ? dtuple_get_n_v_fields(ventry) : 0;

  ut_ad(!page_is_comp(page_align(rec)));
  ut_ad(dict_index_get_n_fields(index) == rec_get_n_fields_old(rec, index));
  ut_ad(dict_tf2_is_valid(index->table->flags, index->table->flags2));
  ut_ad(!dict_table_is_comp(index->table)); /* redundant row format */
  ut_ad(new_index->is_clustered());

  uint8_t rec_version = 0;
  bool has_instant_version = false;
  if (rec_old_is_versioned(rec)) {
    ut_ad(index->has_row_versions());
    has_instant_version = true;
    rec_version = rec_get_instant_row_version_old(rec);
  }

  const auto n_total_fields = index->get_n_total_fields();

  mem_heap_t *heap =
      mem_heap_create(DTUPLE_EST_ALLOC(n_total_fields), UT_LOCATION_HERE);
  /* Create tuple with total fields because for inplace update on record in a
  table with INSTANT DROP columns, we keep the records on current version i.e.
  dropped column value may still exists. */
  dtuple_t *tuple = dtuple_create_with_vcol(heap, n_total_fields, num_v);
  dict_index_copy_types(tuple, index, n_total_fields);

  if (num_v) {
    dict_table_copy_v_types(tuple, index->table);
  }

  dtuple_set_n_fields_cmp(tuple, dict_index_get_n_unique(index));

  if (rec_get_1byte_offs_flag(rec)) {
    for (uint16_t i = 0; i < n_total_fields; i++) {
      dfield_t *dfield;
      ulint len;
      const void *field;

      dfield = dtuple_get_nth_field(tuple, i);
      field = rec_get_nth_field_old_instant(rec, i, index, &len);

      dfield_set_data(dfield, field, len);
    }
  } else {
    for (uint16_t i = 0; i < n_total_fields; i++) {
      dfield_t *dfield;
      ulint len;
      const void *field;

      dfield = dtuple_get_nth_field(tuple, i);
      field = rec_get_nth_field_old_instant(rec, i, index, &len);

      dfield_set_data(dfield, field, len);

      /* Fields stored as default value is not stored externally. */
      size_t phy_pos = index->get_field(i)->col->get_col_phy_pos();
      if (phy_pos < rec_get_n_fields_old_raw(rec) &&
          rec_2_is_field_extern(index, rec, i)) {
        dfield_set_ext(dfield);
      }
    }
  }

  ulint extra_size;
  ulint size = rec_get_serialize_size(index, tuple->fields, tuple->n_fields,
                                      ventry, &extra_size, rec_version);

  /* If INSTANT, we need 2 extra bytes to store info bits and version */
  uint32_t additional_size_for_instant =
      index->has_row_versions() ? (has_instant_version ? 2 : 1) : 0;

  extra_size += additional_size_for_instant;

  ulint mrec_size = ROW_LOG_HEADER_SIZE + size + (extra_size >= 0x80) +
                    additional_size_for_instant;

  if (num_v) {
    if (o_ventry) {
      ulint v_extra = 0;
      mrec_size += rec_get_serialize_size(new_index, nullptr, 0, o_ventry,
                                          &v_extra, MAX_ROW_VERSION);
    }
  } else if (index->table->n_v_cols) {
    mrec_size += 2;
  }

  ulint old_pk_size;
  ulint old_pk_extra_size;
  if (insert || index->online_log->same_pk) {
    ut_ad(!old_pk);
    old_pk_extra_size = old_pk_size = 0;
  } else {
    ut_ad(old_pk);
    ut_ad(old_pk->n_fields == 2 + old_pk->n_fields_cmp);
    ut_ad(DATA_TRX_ID_LEN ==
          dtuple_get_nth_field(old_pk, old_pk->n_fields - 2)->len);
    ut_ad(DATA_ROLL_PTR_LEN ==
          dtuple_get_nth_field(old_pk, old_pk->n_fields - 1)->len);

    old_pk_size =
        rec_get_serialize_size(new_index, old_pk->fields, old_pk->n_fields,
                               nullptr, &old_pk_extra_size, MAX_ROW_VERSION);
    ut_ad(old_pk_extra_size < 0x100);
    mrec_size += 1 /*old_pk_extra_size*/ + old_pk_size;
  }

  ulint avail_size;
  if (byte *b = row_log_table_open(index->online_log, mrec_size, &avail_size)) {
    /* Write TYPE in 1 byte */
    *b++ = insert ? ROW_T_INSERT : ROW_T_UPDATE;

    if (old_pk_size) {
      *b++ = static_cast<byte>(old_pk_extra_size);

      rec_serialize_dtuple(b + old_pk_extra_size, new_index, old_pk->fields,
                           old_pk->n_fields, ventry);
      b += old_pk_size;
    }

    /* Write extra size in 1 or 2 bytes */
    if (extra_size < 0x80) {
      *b++ = static_cast<byte>(extra_size);
    } else {
      ut_ad(extra_size < 0x8000);
      *b++ = static_cast<byte>(0x80 | (extra_size >> 8));
      *b++ = static_cast<byte>(extra_size);
    }

    /* Convert and write record in TEMP format */
    rec_serialize_dtuple(b + extra_size, index, tuple->fields, tuple->n_fields,
                         ventry, rec_version);

    /* Write instant metadata in 1 or 2 bytes */
    if (index->has_row_versions()) {
      /* Write info bit in 1 byte to indicate INSTANT */
      byte *temp_rec = b + extra_size;
      rec_set_info_bits_new_temp(temp_rec, rec_get_info_bits(rec, false));
      if (has_instant_version) {
        rec_new_temp_set_versioned(temp_rec, true);
        /* Write the record version in 1 byte */
        byte *temp_version = temp_rec - 2;
        memcpy(temp_version, &rec_version, sizeof(uint8_t));
        b += 1;
      } else {
        rec_new_temp_set_versioned(temp_rec, false);
      }

      /* Move 'b' to past INSTANT metadata */
      b += 1;
    }

    /* Move 'b' to past record data */
    b += size;

    if (num_v) {
      if (o_ventry) {
        rec_serialize_dtuple(b, new_index, nullptr, 0, o_ventry);
        b += mach_read_from_2(b);
      }
    } else if (index->table->n_v_cols) {
      /* The table contains virtual columns, but nothing
      has changed for them, so just mark a 2 bytes length
      field */
      mach_write_to_2(b, 2);
      b += 2;
    }

    row_log_table_close(index->online_log, b, mrec_size, avail_size);
  }

  mem_heap_free(heap);
}

/** Logs an insert or update to a table that is being rebuilt.
@param[in]      rec             clustered index leaf page record, page X-latched
@param[in]      ventry          dtuple holding virtual column info
@param[in]      o_ventry        dtuple holding old virtual column info
@param[in,out]  index           clustered index, S-latched or X-latched
@param[in]      offsets         rec_get_offsets(rec,index)
@param[in]      insert          true if insert, false if update
@param[in]      old_pk          old PRIMARY KEY value (if !insert and a PRIMARY
                                KEY is being created) */
static void row_log_table_low(const rec_t *rec, const dtuple_t *ventry,
                              const dtuple_t *o_ventry, dict_index_t *index,
                              const ulint *offsets, bool insert,
                              const dtuple_t *old_pk) {
  if (index->is_corrupted() || !dict_index_is_online_ddl(index) ||
      index->online_log->error != DB_SUCCESS) {
    return;
  }

  const dict_index_t *new_index = index->online_log->table->first_index();

  ut_ad(index->is_clustered());
  ut_ad(new_index->is_clustered());
  ut_ad(!dict_index_is_online_ddl(new_index));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(rec_offs_n_fields(offsets) == dict_index_get_n_fields(index));
  ut_ad(rec_offs_size(offsets) <= sizeof index->online_log->tail.buf);
  ut_ad(rw_lock_own_flagged(&index->lock,
                            RW_LOCK_FLAG_S | RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX));
  ut_ad(fil_page_get_type(page_align(rec)) == FIL_PAGE_INDEX);
  ut_ad(page_is_leaf(page_align(rec)));
  ut_ad(page_is_comp(page_align(rec)) == rec_offs_comp(offsets));
  /* old_pk=row_log_table_get_pk() [not needed in INSERT] is a prefix
  of the clustered index record (PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR),
  with no information on virtual columns */
  ut_ad(!old_pk || !insert);
  ut_ad(!old_pk || old_pk->n_v_fields == 0);
  ut_ad(!o_ventry || !insert);
  ut_ad(!o_ventry || ventry);

  if (!rec_offs_comp(offsets)) {
    row_log_table_low_redundant(rec, ventry, o_ventry, index, insert, old_pk,
                                new_index);
    return;
  }

  ut_ad(page_is_comp(page_align(rec)));
  ut_ad(rec_get_status(rec) == REC_STATUS_ORDINARY);

  /* Check the instant to decide copying info bit or not */
  ulint omit_size = REC_N_NEW_EXTRA_BYTES -
                    (index->has_row_versions() ? REC_N_TMP_EXTRA_BYTES : 0);

  ulint extra_size = rec_offs_extra_size(offsets) - omit_size;

  ulint mrec_size = ROW_LOG_HEADER_SIZE + (extra_size >= 0x80) +
                    rec_offs_size(offsets) - omit_size;

  if (ventry && ventry->n_v_fields > 0) {
    ulint v_extra = 0;
    uint64_t rec_size = rec_get_serialize_size(new_index, nullptr, 0, ventry,
                                               &v_extra, MAX_ROW_VERSION);

    mrec_size += rec_size;

    /* If there is actually nothing to be logged for new entry, then
    there must be also nothing to do with old entry. In this case,
    make it same with the case below, by only keep 2 bytes length marker */
    if (rec_size > 2 && o_ventry != nullptr) {
      mrec_size += rec_get_serialize_size(new_index, nullptr, 0, o_ventry,
                                          &v_extra, MAX_ROW_VERSION);
    }
  } else if (index->table->n_v_cols) {
    /* Always leave 2 bytes length marker for virtual column
    data logging even if there is none of them is indexed if table
    has virtual columns */
    mrec_size += 2;
  }

  ulint old_pk_size;
  ulint old_pk_extra_size;
  if (insert || index->online_log->same_pk) {
    ut_ad(!old_pk);
    old_pk_extra_size = old_pk_size = 0;
  } else {
    ut_ad(old_pk);
    ut_ad(old_pk->n_fields == 2 + old_pk->n_fields_cmp);
    ut_ad(DATA_TRX_ID_LEN ==
          dtuple_get_nth_field(old_pk, old_pk->n_fields - 2)->len);
    ut_ad(DATA_ROLL_PTR_LEN ==
          dtuple_get_nth_field(old_pk, old_pk->n_fields - 1)->len);

    old_pk_size =
        rec_get_serialize_size(new_index, old_pk->fields, old_pk->n_fields,
                               nullptr, &old_pk_extra_size, MAX_ROW_VERSION);
    ut_ad(old_pk_extra_size < 0x100);
    mrec_size += 1 /*old_pk_extra_size*/ + old_pk_size;
  }

  ulint avail_size;
  if (byte *b = row_log_table_open(index->online_log, mrec_size, &avail_size)) {
    *b++ = insert ? ROW_T_INSERT : ROW_T_UPDATE;

    if (old_pk_size) {
      ut_ad(!insert);

      *b++ = static_cast<byte>(old_pk_extra_size);

      rec_serialize_dtuple(b + old_pk_extra_size, new_index, old_pk->fields,
                           old_pk->n_fields, nullptr);
      b += old_pk_size;
    }

    if (extra_size < 0x80) {
      *b++ = static_cast<byte>(extra_size);
    } else {
      ut_ad(extra_size < 0x8000);
      *b++ = static_cast<byte>(0x80 | (extra_size >> 8));
      *b++ = static_cast<byte>(extra_size);
    }

    memcpy(b, rec - rec_offs_extra_size(offsets), extra_size);
    b += extra_size;
    memcpy(b, rec, rec_offs_data_size(offsets));
    b += rec_offs_data_size(offsets);

    if (ventry && ventry->n_v_fields > 0) {
      uint64_t new_v_size;

      rec_serialize_dtuple(b, new_index, nullptr, 0, ventry);
      new_v_size = mach_read_from_2(b);
      b += new_v_size;

      /* Nothing for new entry to be logged, skip the old one too. */
      if (new_v_size != 2 && o_ventry != nullptr) {
        rec_serialize_dtuple(b, new_index, nullptr, 0, o_ventry);
        b += mach_read_from_2(b);
      }
    } else if (index->table->n_v_cols) {
      /* The table contains virtual columns, but nothing
      has changed for them, so just mark a 2 bytes length
      field */
      mach_write_to_2(b, 2);
      b += 2;
    }

    row_log_table_close(index->online_log, b, mrec_size, avail_size);
  }
}

/** Logs an update to a table that is being rebuilt.
 This will be merged in row_log_table_apply_update(). */
void row_log_table_update(
    const rec_t *rec,          /*!< in: clustered index leaf page record,
                               page X-latched */
    dict_index_t *index,       /*!< in/out: clustered index, S-latched
                               or X-latched */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec,index) */
    const dtuple_t *old_pk,    /*!< in: row_log_table_get_pk()
                               before the update */
    const dtuple_t *new_v_row, /*!< in: dtuple contains the new virtual
                             columns */
    const dtuple_t *old_v_row) /*!< in: dtuple contains the old virtual
                             columns */
{
  row_log_table_low(rec, new_v_row, old_v_row, index, offsets, false, old_pk);
}

/** Gets the old table column of a PRIMARY KEY column.
@param table old table (before ALTER TABLE)
@param col_map mapping of old column numbers to new ones
@param col_no column position in the new table
@return old table column, or NULL if this is an added column */
static const dict_col_t *row_log_table_get_pk_old_col(const dict_table_t *table,
                                                      const ulint *col_map,
                                                      ulint col_no) {
  for (ulint i = 0; i < table->n_cols; i++) {
    if (col_no == col_map[i]) {
      return (table->get_col(i));
    }
  }

  return (nullptr);
}

/** Maps an old table column of a PRIMARY KEY column.
@param[in]      index           index being operated on
@param[in]      ifield          clustered index field in the new table (after
ALTER TABLE)
@param[in,out]  dfield          clustered index tuple field in the new table
@param[in,out]  heap            memory heap for allocating dfield contents
@param[in]      rec             clustered index leaf page record in the old
table
@param[in]      offsets         rec_get_offsets(rec)
@param[in]      i               rec field corresponding to col
@param[in]      page_size       page size of the old table
@param[in]      max_len         maximum length of dfield
@retval DB_INVALID_NULL         if a NULL value is encountered
@retval DB_TOO_BIG_INDEX_COL    if the maximum prefix length is exceeded */
static dberr_t row_log_table_get_pk_col(dict_index_t *index,
                                        const dict_field_t *ifield,
                                        dfield_t *dfield, mem_heap_t *heap,
                                        const rec_t *rec, const ulint *offsets,
                                        ulint i, const page_size_t &page_size,
                                        ulint max_len) {
  const byte *field;
  ulint len;

  field = rec_get_nth_field(nullptr, rec, offsets, i, &len);

  if (len == UNIV_SQL_NULL) {
    return (DB_INVALID_NULL);
  }

  if (rec_offs_nth_extern(index, offsets, i)) {
    ulint field_len = ifield->prefix_len;
    byte *blob_field;

    if (!field_len) {
      field_len = ifield->fixed_len;
      if (!field_len) {
        field_len = max_len + 1;
      }
    }

    blob_field = static_cast<byte *>(mem_heap_alloc(heap, field_len));

    len = lob::btr_copy_externally_stored_field_prefix(
        nullptr, index, blob_field, field_len, page_size, field, false, len);

    if (len >= max_len + 1) {
      return (DB_TOO_BIG_INDEX_COL);
    }

    dfield_set_data(dfield, blob_field, len);
  } else {
    dfield_set_data(dfield, mem_heap_dup(heap, field, len), len);
  }

  return (DB_SUCCESS);
}

/** Constructs the old PRIMARY KEY and DB_TRX_ID,DB_ROLL_PTR
 of a table that is being rebuilt.
 @return tuple of PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR in the rebuilt table,
 or NULL if the PRIMARY KEY definition does not change */
const dtuple_t *row_log_table_get_pk(
    const rec_t *rec,     /*!< in: clustered index leaf page record,
                          page X-latched */
    dict_index_t *index,  /*!< in/out: clustered index, S-latched
                          or X-latched */
    const ulint *offsets, /*!< in: rec_get_offsets(rec,index) */
    byte *sys,            /*!< out: DB_TRX_ID,DB_ROLL_PTR for
                          row_log_table_delete(), or NULL */
    mem_heap_t **heap)    /*!< in/out: memory heap where allocated */
{
  dtuple_t *tuple = nullptr;
  row_log_t *log = index->online_log;

  ut_ad(index->is_clustered());
  ut_ad(dict_index_is_online_ddl(index));
  ut_ad(!dict_index_is_sdi(index));
  ut_ad(!offsets || rec_offs_validate(rec, index, offsets));
  ut_ad(rw_lock_own_flagged(&index->lock,
                            RW_LOCK_FLAG_S | RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX));

  ut_ad(log);
  ut_ad(log->table);

  if (log->same_pk) {
    /* The PRIMARY KEY columns are unchanged. */
    if (sys) {
      /* Store the DB_TRX_ID,DB_ROLL_PTR. */
      ulint trx_id_offs = index->trx_id_offset;

      if (!trx_id_offs) {
        ulint pos = index->get_sys_col_pos(DATA_TRX_ID);
        ulint len;
        ut_ad(pos > 0);

        if (!offsets) {
          offsets = rec_get_offsets(rec, index, nullptr, pos + 1,
                                    UT_LOCATION_HERE, heap);
        }

        trx_id_offs = rec_get_nth_field_offs(index, offsets, pos, &len);
        ut_ad(len == DATA_TRX_ID_LEN);
      }

      memcpy(sys, rec + trx_id_offs, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
    }

    return (nullptr);
  }

  mutex_enter(&log->mutex);

  /* log->error is protected by log->mutex. */
  if (log->error == DB_SUCCESS) {
    dict_table_t *new_table = log->table;
    dict_index_t *new_index = new_table->first_index();
    const ulint new_n_uniq = dict_index_get_n_unique(new_index);

    if (!*heap) {
      ulint size = 0;

      if (!offsets) {
        size += (1 + REC_OFFS_HEADER_SIZE + index->n_fields) * sizeof *offsets;
      }

      for (ulint i = 0; i < new_n_uniq; i++) {
        size += new_index->get_col(i)->get_min_size();
      }

      *heap = mem_heap_create(DTUPLE_EST_ALLOC(new_n_uniq + 2) + size,
                              UT_LOCATION_HERE);
    }

    if (!offsets) {
      offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                                UT_LOCATION_HERE, heap);
    }

    tuple = dtuple_create(*heap, new_n_uniq + 2);
    dict_index_copy_types(tuple, new_index, tuple->n_fields);
    dtuple_set_n_fields_cmp(tuple, new_n_uniq);

    const ulint max_len = DICT_MAX_FIELD_LEN_BY_FORMAT(new_table);

    const page_size_t &page_size = dict_table_page_size(index->table);

    for (ulint new_i = 0; new_i < new_n_uniq; new_i++) {
      dict_field_t *ifield;
      dfield_t *dfield;
      ulint prtype;
      ulint mbminmaxlen;

      ifield = new_index->get_field(new_i);
      dfield = dtuple_get_nth_field(tuple, new_i);

      const ulint col_no = ifield->col->ind;

      if (const dict_col_t *col = row_log_table_get_pk_old_col(
              index->table, log->col_map, col_no)) {
        ulint i = dict_col_get_clust_pos(col, index);

        if (i == ULINT_UNDEFINED) {
          log->error = DB_CORRUPTION;
          ut_d(ut_error);
          ut_o(goto err_exit);
        }

        log->error = row_log_table_get_pk_col(index, ifield, dfield, *heap, rec,
                                              offsets, i, page_size, max_len);

        if (log->error != DB_SUCCESS) {
          ut_o(err_exit:) tuple = nullptr;
          goto func_exit;
        }

        mbminmaxlen = col->mbminmaxlen;
        prtype = col->prtype;
      } else {
        /* No matching column was found in the old
        table, so this must be an added column.
        Copy the default value. */
        ut_ad(log->add_cols);

        dfield_copy(dfield, dtuple_get_nth_field(log->add_cols, col_no));
        mbminmaxlen = dfield->type.mbminmaxlen;
        prtype = dfield->type.prtype;
      }

      ut_ad(!dfield_is_ext(dfield));
      ut_ad(!dfield_is_null(dfield));

      if (ifield->prefix_len) {
        ulint len = dtype_get_at_most_n_mbchars(
            prtype, mbminmaxlen, ifield->prefix_len, dfield_get_len(dfield),
            static_cast<const char *>(dfield_get_data(dfield)));

        ut_ad(len <= dfield_get_len(dfield));
        dfield_set_len(dfield, len);
      }
    }

    const byte *trx_roll = rec + row_get_trx_id_offset(index, offsets);

    /* Copy the fields, because the fields will be updated
    or the record may be moved somewhere else in the B-tree
    as part of the upcoming operation. */
    if (sys) {
      memcpy(sys, trx_roll, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
      trx_roll = sys;
    } else {
      trx_roll = static_cast<const byte *>(
          mem_heap_dup(*heap, trx_roll, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN));
    }

    dfield_set_data(dtuple_get_nth_field(tuple, new_n_uniq), trx_roll,
                    DATA_TRX_ID_LEN);
    dfield_set_data(dtuple_get_nth_field(tuple, new_n_uniq + 1),
                    trx_roll + DATA_TRX_ID_LEN, DATA_ROLL_PTR_LEN);
  }

func_exit:
  mutex_exit(&log->mutex);
  return (tuple);
}

/** Logs an insert to a table that is being rebuilt.
 This will be merged in row_log_table_apply_insert(). */
void row_log_table_insert(
    const rec_t *rec,       /*!< in: clustered index leaf page record,
                            page X-latched */
    const dtuple_t *ventry, /*!< in: dtuple holding virtual column info */
    dict_index_t *index,    /*!< in/out: clustered index, S-latched
                            or X-latched */
    const ulint *offsets)   /*!< in: rec_get_offsets(rec,index) */
{
  row_log_table_low(rec, ventry, nullptr, index, offsets, true, nullptr);
}

/** Notes that a BLOB is being freed during online ALTER TABLE. */
void row_log_table_blob_free(
    dict_index_t *index, /*!< in/out: clustered index, X-latched */
    page_no_t page_no)   /*!< in: starting page number of the BLOB */
{
  DBUG_TRACE;

  ut_ad(index->is_clustered());
  ut_ad(dict_index_is_online_ddl(index));
  ut_ad(rw_lock_own_flagged(&index->lock, RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX));
  ut_ad(page_no != FIL_NULL);

  if (index->online_log->error != DB_SUCCESS) {
    return;
  }

  page_no_map *blobs = index->online_log->blobs;

  if (blobs == nullptr) {
    index->online_log->blobs = blobs =
        ut::new_withkey<page_no_map>(UT_NEW_THIS_FILE_PSI_KEY);
  }

#ifdef UNIV_DEBUG
  const ulonglong log_pos = index->online_log->tail.total;
#else
#define log_pos /* empty */
#endif          /* UNIV_DEBUG */

  const page_no_map::value_type v(page_no, row_log_table_blob_t(log_pos));

  std::pair<page_no_map::iterator, bool> p = blobs->insert(v);

  if (!p.second) {
    /* Update the existing mapping. */
    ut_ad(p.first->first == page_no);
    p.first->second.blob_free(log_pos);
  }
#undef log_pos
}

/** Notes that a BLOB is being allocated during online ALTER TABLE. */
void row_log_table_blob_alloc(
    dict_index_t *index, /*!< in/out: clustered index, X-latched */
    page_no_t page_no)   /*!< in: starting page number of the BLOB */
{
  DBUG_TRACE;

  ut_ad(index->is_clustered());
  ut_ad(dict_index_is_online_ddl(index));

  ut_ad(rw_lock_own_flagged(&index->lock, RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX));

  ut_ad(page_no != FIL_NULL);

  if (index->online_log->error != DB_SUCCESS) {
    return;
  }

  /* Only track allocations if the same page has been freed
  earlier. Double allocation without a free is not allowed. */
  if (page_no_map *blobs = index->online_log->blobs) {
    page_no_map::iterator p = blobs->find(page_no);

    if (p != blobs->end()) {
      ut_ad(p->first == page_no);
      p->second.blob_alloc(index->online_log->tail.total);
    }
  }
}

/** Converts a log record to a table row.
 @return converted row, or NULL if the conversion fails */
[[nodiscard]] static const dtuple_t *row_log_table_apply_convert_mrec(
    const ddl::mrec_t *mrec, /*!< in: merge record */
    dict_index_t *index,     /*!< in: index of mrec */
    const ulint *offsets,    /*!< in: offsets of mrec */
    const row_log_t *log,    /*!< in: rebuild context */
    mem_heap_t *heap,        /*!< in/out: memory heap */
    dberr_t *error)          /*!< out: DB_SUCCESS or
                             DB_MISSING_HISTORY or
                             reason of failure */
{
  dtuple_t *row;
  ulint num_v = dict_table_get_n_v_cols(log->table);

  *error = DB_SUCCESS;

  /* This is based on row_build(). */
  if (log->add_cols) {
    row = dtuple_copy(log->add_cols, heap);
    /* dict_table_copy_types() would set the fields to NULL */
    for (ulint i = 0; i < log->table->get_n_cols(); i++) {
      log->table->get_col(i)->copy_type(
          dfield_get_type(dtuple_get_nth_field(row, i)));
    }
  } else {
    row = dtuple_create_with_vcol(heap, log->table->get_n_cols(), num_v);
    dict_table_copy_types(row, log->table);
  }

  for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
    const dict_field_t *ind_field = index->get_field(i);

    if (ind_field->prefix_len) {
      /* Column prefixes can only occur in key
      fields, which cannot be stored externally. For
      a column prefix, there should also be the full
      field in the clustered index tuple. The row
      tuple comprises full fields, not prefixes. */
      ut_ad(!rec_offs_nth_extern(index, offsets, i));
      continue;
    }

    const dict_col_t *col = ind_field->col;
    if (col->is_instant_dropped()) {
      /* Skip instanly dropped column */
      continue;
    }

    ulint col_no = log->col_map[dict_col_get_no(col)];

    if (col_no == ULINT_UNDEFINED) {
      /* dropped column */
      continue;
    }

    dfield_t *dfield = dtuple_get_nth_field(row, col_no);

    ulint len;
    const byte *data;

    if (rec_offs_nth_extern(index, offsets, i)) {
      ut_ad(rec_offs_any_extern(offsets));
      rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

      if (const page_no_map *blobs = log->blobs) {
        data = rec_get_nth_field(index, mrec, offsets, i, &len);
        ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);

        page_no_t page_no = mach_read_from_4(
            data + len - (BTR_EXTERN_FIELD_REF_SIZE - lob::BTR_EXTERN_PAGE_NO));
        page_no_map::const_iterator p = blobs->find(page_no);
        if (p != blobs->end() && p->second.is_freed(log->head.total)) {
          /* This BLOB has been freed.
          We must not access the row. */
          *error = DB_MISSING_HISTORY;
          dfield_set_data(dfield, data, len);
          dfield_set_ext(dfield);
          goto blob_done;
        }
      }

      /* Reading a LOB of another transaction. The ALTER
      TABLE trx is not the owner of this LOB. So instead
      of passing current trx, a nullptr is passed for trx.*/
      data = lob::btr_rec_copy_externally_stored_field(
          nullptr, index, mrec, offsets, dict_table_page_size(index->table), i,
          &len, nullptr, false, heap);

      ut_a(data);
      dfield_set_data(dfield, data, len);
    blob_done:
      rw_lock_x_unlock(dict_index_get_lock(index));
    } else {
      data = rec_get_nth_field_instant(mrec, offsets, i, index, &len);
      dfield_set_data(dfield, data, len);
    }

    if (len != UNIV_SQL_NULL && col->mtype == DATA_MYSQL && col->len != len &&
        !dict_table_is_comp(log->table)) {
      ut_ad(col->len >= len);
      if (dict_table_is_comp(index->table)) {
        byte *buf = (byte *)mem_heap_alloc(heap, col->len);
        memcpy(buf, dfield->data, len);
        memset(buf + len, 0x20, col->len - len);

        dfield_set_data(dfield, buf, col->len);
      } else {
        /* field length mismatch should not happen
        when rebuilding the redundant row format
        table. */
        *error = DB_CORRUPTION;
        ut_d(ut_error);
        ut_o(return (nullptr));
      }
    }

    /* See if any columns were changed to NULL or NOT NULL. */
    const dict_col_t *new_col = log->table->get_col(col_no);
    ut_ad(new_col->mtype == col->mtype);

    /* Assert that prtype matches except for nullability. */
    ut_ad(!((new_col->prtype ^ col->prtype) & ~DATA_NOT_NULL));
    ut_ad(!((new_col->prtype ^ dfield_get_type(dfield)->prtype) &
            ~DATA_NOT_NULL));

    if (new_col->prtype == col->prtype) {
      continue;
    }

    if ((new_col->prtype & DATA_NOT_NULL) && dfield_is_null(dfield)) {
      /* We got a NULL value for a NOT NULL column. */
      *error = DB_INVALID_NULL;
      return (nullptr);
    }

    /* Adjust the DATA_NOT_NULL flag in the parsed row. */
    dfield_get_type(dfield)->prtype = new_col->prtype;

    ut_ad(new_col->assert_equal(dfield_get_type(dfield)));
  }

  /* read the virtual column data if any */
  if (num_v) {
    byte *b = const_cast<byte *>(mrec) + rec_offs_data_size(offsets);
    trx_undo_read_v_cols(log->table, b, row, false, true,
                         &(log->col_map[log->n_old_col]), heap);
  }

  return (row);
}

/** Tries to insert an entry into a secondary index, which is created for
multi-value field. For each value to be inserted, if a record with exactly
the same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index.
@param[in]      flags           undo logging and locking flags
@param[in]      index           secondary index
@param[in,out]  offsets_heap    memory heap that can be emptied
@param[in,out]  heap            memory heap
@param[in,out]  entry           index entry to insert
@param[in]      trx_id          PAGE_MAX_TRX_ID during row_log_table_apply(),
                                or trx_id when undo log is disabled during
                                alter copy operation or 0
@param[in]      thr             query thread
@retval DB_SUCCESS on success
@retval DB_LOCK_WAIT on lock wait when !(flags & BTR_NO_LOCKING_FLAG)
@retval DB_FAIL if retry with BTR_MODIFY_TREE is needed
@return error code */
[[nodiscard]] static dberr_t apply_insert_multi_value(
    uint32_t flags, dict_index_t *index, mem_heap_t *offsets_heap,
    mem_heap_t *heap, dtuple_t *entry, trx_id_t trx_id, que_thr_t *thr) {
  dberr_t err = DB_SUCCESS;
  Multi_value_entry_builder_insert mv_entry_builder(index, entry);

  for (dtuple_t *mv_entry = mv_entry_builder.begin(); mv_entry != nullptr;
       mv_entry = mv_entry_builder.next()) {
    err =
        row_ins_sec_index_entry_low(flags, BTR_MODIFY_TREE, index, offsets_heap,
                                    heap, mv_entry, trx_id, thr, false);
    if (err != DB_SUCCESS) {
      break;
    }
  }

  return (err);
}

/** Replays an insert operation on a table that was rebuilt.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_log_table_apply_insert_low(
    que_thr_t *thr,           /*!< in: query graph */
    const dtuple_t *row,      /*!< in: table row
                              in the old table definition */
    trx_id_t trx_id,          /*!< in: trx_id of the row */
    mem_heap_t *offsets_heap, /*!< in/out: memory heap
                              that can be emptied */
    mem_heap_t *heap,         /*!< in/out: memory heap */
    ddl::Dup *dup)            /*!< in/out: for reporting
                                     duplicate key errors */
{
  dberr_t error;
  dtuple_t *entry;
  const row_log_t *log = dup->m_index->online_log;
  dict_index_t *index = log->table->first_index();
  ulint n_index = 0;

  ut_ad(dtuple_validate(row));
  ut_ad(trx_id);

  DBUG_PRINT("ib_alter_table",
             ("insert table " IB_ID_FMT "(index " IB_ID_FMT "): %s",
              index->table->id, index->id, rec_printer(row).str().c_str()));

  static const uint32_t flags = (BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG |
                                 BTR_NO_UNDO_LOG_FLAG | BTR_KEEP_SYS_FLAG);

  entry = row_build_index_entry(row, nullptr, index, heap);

  error = row_ins_clust_index_entry_low(flags, BTR_MODIFY_TREE, index,
                                        index->n_uniq, entry, thr, false);

  switch (error) {
    case DB_SUCCESS:
      break;
    case DB_SUCCESS_LOCKED_REC:
      /* The row had already been copied to the table. */
      return (DB_SUCCESS);
    default:
      return (error);
  }

  do {
    n_index++;
    index = index->next();
    if (!index) {
      break;
    }

    if (index->type & DICT_FTS) {
      continue;
    }

    entry = row_build_index_entry(row, nullptr, index, heap);

    if (index->is_multi_value()) {
      error = apply_insert_multi_value(flags, index, offsets_heap, heap, entry,
                                       trx_id, thr);
    } else {
      error = row_ins_sec_index_entry_low(flags, BTR_MODIFY_TREE, index,
                                          offsets_heap, heap, entry, trx_id,
                                          thr, false);
    }

    /* Report correct index name for duplicate key error. */
    if (error == DB_DUPLICATE_KEY) {
      thr_get_trx(thr)->error_key_num = n_index;
    }

  } while (error == DB_SUCCESS);

  return (error);
}

/** Replays an insert operation on a table that was rebuilt.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_log_table_apply_insert(
    que_thr_t *thr,           /*!< in: query graph */
    const ddl::mrec_t *mrec,  /*!< in: record to insert */
    const ulint *offsets,     /*!< in: offsets of mrec */
    mem_heap_t *offsets_heap, /*!< in/out: memory heap
                              that can be emptied */
    mem_heap_t *heap,         /*!< in/out: memory heap */
    ddl::Dup *dup,            /*!< in/out: for reporting
                                     duplicate key errors */
    trx_id_t trx_id)          /*!< in: DB_TRX_ID of mrec */
{
  const row_log_t *log = dup->m_index->online_log;
  dberr_t error;
  const dtuple_t *row = row_log_table_apply_convert_mrec(
      mrec, dup->m_index, offsets, log, heap, &error);

  switch (error) {
    case DB_MISSING_HISTORY:
      ut_ad(log->blobs);
      /* Because some BLOBs are missing, we know that the
      transaction was rolled back later (a rollback of
      an insert can free BLOBs).
      We can simply skip the insert: the subsequent
      ROW_T_DELETE will be ignored, or a ROW_T_UPDATE will
      be interpreted as ROW_T_INSERT. */
      return (DB_SUCCESS);
    case DB_SUCCESS:
      ut_ad(row != nullptr);
      break;
    default:
      ut_d(ut_error);
      [[fallthrough]];
    case DB_INVALID_NULL:
      ut_ad(row == nullptr);
      return (error);
  }

  error =
      row_log_table_apply_insert_low(thr, row, trx_id, offsets_heap, heap, dup);
  if (error != DB_SUCCESS) {
    /* Report the erroneous row using the new
    version of the table. */
    innobase_row_to_mysql(dup->m_table, log->table, row);
  }
  return (error);
}

/** Delete a record from a secondary index.
@param[in]      index   secondary index
@param[in]      entry   entry to delete
@param[in,out]  pcur    B-tree cursor
@return DB_SUCCESS or error code */
[[nodiscard]] static inline dberr_t row_log_table_delete_sec(
    dict_index_t *index, const dtuple_t *entry, btr_pcur_t *pcur) {
  dberr_t error;
  mtr_t mtr;

  ut_ad(!index->is_clustered());

  mtr_start(&mtr);

  pcur->open(index, 0, entry, PAGE_CUR_LE,
             BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &mtr, UT_LOCATION_HERE);
#ifdef UNIV_DEBUG
  switch (pcur->get_btr_cur()->flag) {
    case BTR_CUR_UNSET:
    case BTR_CUR_DELETE_REF:
    case BTR_CUR_DEL_MARK_IBUF:
    case BTR_CUR_DELETE_IBUF:
    case BTR_CUR_INSERT_TO_IBUF:
      /* We did not request buffering. */
      break;
    case BTR_CUR_HASH:
    case BTR_CUR_HASH_NOT_ATTEMPTED:
    case BTR_CUR_HASH_FAIL:
    case BTR_CUR_BINARY:
      goto flag_ok;
  }
  ut_d(ut_error);
flag_ok:
#endif /* UNIV_DEBUG */

  if (page_rec_is_infimum(pcur->get_rec()) ||
      pcur->get_low_match() < index->n_uniq) {
    /* All secondary index entries should be
    found, because new_table is being modified by
    this thread only, and all indexes should be
    updated in sync. */
    mtr_commit(&mtr);
    return (DB_INDEX_CORRUPT);
  }

  btr_cur_pessimistic_delete(&error, false, pcur->get_btr_cur(),
                             BTR_CREATE_FLAG, false, 0, 0, 0, &mtr, pcur,
                             nullptr);
  mtr_commit(&mtr);

  return (error);
}

/** Deletes a record from a multi-value index.
@param[in]      row     row to delete
@param[in]      ext     external data
@param[in]      index   multi-value index
@param[in,out]  pcur    B-tree cursor
@param[in,out]  heap    memory heap
@return DB_SUCCESS or error code */
[[nodiscard]] static inline dberr_t apply_delete_multi_value(
    const dtuple_t *row, row_ext_t *ext, dict_index_t *index, btr_pcur_t *pcur,
    mem_heap_t *heap) {
  dberr_t err = DB_SUCCESS;
  Multi_value_entry_builder_normal mv_entry_builder(row, ext, index, heap, true,
                                                    false);

  ut_ad(index->is_multi_value());

  for (dtuple_t *entry = mv_entry_builder.begin(); entry != nullptr;
       entry = mv_entry_builder.next()) {
    err = row_log_table_delete_sec(index, entry, pcur);
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  return (err);
}

/** Deletes a record from a table that is being rebuilt.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_log_table_apply_delete_low(
    btr_pcur_t *pcur,       /*!< in/out: B-tree cursor,
                            will be trashed */
    const dtuple_t *ventry, /*!< in: dtuple holding
                            virtual column info */
    const ulint *offsets,   /*!< in: offsets on pcur */
    mem_heap_t *heap,       /*!< in/out: memory heap */
    mtr_t *mtr)             /*!< in/out: mini-transaction,
                            will be committed */
{
  dberr_t error;
  row_ext_t *ext = nullptr;
  dtuple_t *row;
  dict_index_t *index = pcur->get_btr_cur()->index;

  ut_ad(index->is_clustered());

  DBUG_PRINT(
      "ib_alter_table",
      ("delete table " IB_ID_FMT "(index " IB_ID_FMT "): %s", index->table->id,
       index->id, rec_printer(pcur->get_rec(), offsets).str().c_str()));

  if (index->next()) {
    /* Build a row template for purging secondary index entries. */
    row = row_build(ROW_COPY_DATA, index, pcur->get_rec(), offsets, nullptr,
                    nullptr, nullptr, &ext, heap);
    if (ventry) {
      dtuple_copy_v_fields(row, ventry);
    }
  } else {
    row = nullptr;
  }

  btr_cur_pessimistic_delete(&error, false, pcur->get_btr_cur(),
                             BTR_CREATE_FLAG, false, 0, 0, 0, mtr, pcur,
                             nullptr);
  mtr_commit(mtr);

  if (error != DB_SUCCESS) {
    return (error);
  }

  while ((index = index->next()) != nullptr) {
    if (index->type & DICT_FTS) {
      continue;
    }

    if (index->is_multi_value()) {
      error = apply_delete_multi_value(row, ext, index, pcur, heap);
      if (error == DB_INDEX_CORRUPT) {
        return (error);
      }
      continue;
    }

    const dtuple_t *entry = row_build_index_entry(row, ext, index, heap);

    if (row_log_table_delete_sec(index, entry, pcur) == DB_INDEX_CORRUPT) {
      return (DB_INDEX_CORRUPT);
    }
  }

  return (error);
}

/** Replays a delete operation on a table that was rebuilt.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_log_table_apply_delete(
    ulint trx_id_col,         /*!< in: position of
                              DB_TRX_ID in the new
                              clustered index */
    const ddl::mrec_t *mrec,  /*!< in: merge record */
    const ulint *moffsets,    /*!< in: offsets of mrec */
    mem_heap_t *offsets_heap, /*!< in/out: memory heap
                              that can be emptied */
    mem_heap_t *heap,         /*!< in/out: memory heap */
    const row_log_t *log)     /*!< in: online log */
{
  dict_table_t *new_table = log->table;
  dict_index_t *index = new_table->first_index();
  dtuple_t *old_pk;
  mtr_t mtr;
  btr_pcur_t pcur;
  ulint *offsets;
  ulint num_v = new_table->n_v_cols;

  ut_ad(rec_offs_n_fields(moffsets) == dict_index_get_n_unique(index) + 2);
  ut_ad(!rec_offs_any_extern(moffsets));

  /* Convert the row to a search tuple. */
  old_pk = dtuple_create_with_vcol(heap, index->n_uniq, num_v);
  dict_index_copy_types(old_pk, index, index->n_uniq);

  if (num_v) {
    dict_table_copy_v_types(old_pk, index->table);
  }

  for (ulint i = 0; i < index->n_uniq; i++) {
    ulint len;
    const void *field;
    field = rec_get_nth_field(nullptr, mrec, moffsets, i, &len);
    ut_ad(rec_field_not_null_not_add_col_def(len));
    dfield_set_data(dtuple_get_nth_field(old_pk, i), field, len);
  }

  mtr_start(&mtr);

  pcur.open(index, 0, old_pk, PAGE_CUR_LE,
            BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &mtr, UT_LOCATION_HERE);
#ifdef UNIV_DEBUG
  switch (pcur.get_btr_cur()->flag) {
    case BTR_CUR_UNSET:
    case BTR_CUR_DELETE_REF:
    case BTR_CUR_DEL_MARK_IBUF:
    case BTR_CUR_DELETE_IBUF:
    case BTR_CUR_INSERT_TO_IBUF:
      /* We did not request buffering. */
      break;
    case BTR_CUR_HASH:
    case BTR_CUR_HASH_NOT_ATTEMPTED:
    case BTR_CUR_HASH_FAIL:
    case BTR_CUR_BINARY:
      goto flag_ok;
  }
  ut_d(ut_error);
flag_ok:
#endif /* UNIV_DEBUG */

  if (page_rec_is_infimum(pcur.get_rec()) ||
      pcur.get_low_match() < index->n_uniq) {
  all_done:
    mtr_commit(&mtr);
    /* The record was not found. All done. */
    /* This should only happen when an earlier
    ROW_T_INSERT was skipped or
    ROW_T_UPDATE was interpreted as ROW_T_DELETE
    due to BLOBs having been freed by rollback. */
    return (DB_SUCCESS);
  }

  offsets = rec_get_offsets(pcur.get_rec(), index, nullptr, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &offsets_heap);
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
  ut_a(!rec_offs_any_null_extern(index, pcur.get_rec(), offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

  /* Only remove the record if DB_TRX_ID,DB_ROLL_PTR match. */

  {
    ulint len;
    const byte *mrec_trx_id =
        rec_get_nth_field(nullptr, mrec, moffsets, trx_id_col, &len);
    ut_ad(len == DATA_TRX_ID_LEN);
    const byte *rec_trx_id =
        rec_get_nth_field(nullptr, pcur.get_rec(), offsets, trx_id_col, &len);
    ut_ad(len == DATA_TRX_ID_LEN);

    ut_ad(rec_get_nth_field(nullptr, mrec, moffsets, trx_id_col + 1, &len) ==
          mrec_trx_id + DATA_TRX_ID_LEN);
    ut_ad(len == DATA_ROLL_PTR_LEN);
    ut_ad(rec_get_nth_field(nullptr, pcur.get_rec(), offsets, trx_id_col + 1,
                            &len) == rec_trx_id + DATA_TRX_ID_LEN);
    ut_ad(len == DATA_ROLL_PTR_LEN);

    if (memcmp(mrec_trx_id, rec_trx_id, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)) {
      /* The ROW_T_DELETE was logged for a different
      PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR.
      This is possible if a ROW_T_INSERT was skipped
      or a ROW_T_UPDATE was interpreted as ROW_T_DELETE
      because some BLOBs were missing due to
      (1) rolling back the initial insert, or
      (2) purging the BLOB for a later ROW_T_DELETE
      (3) purging 'old values' for a later ROW_T_UPDATE
      or ROW_T_DELETE. */
      ut_ad(!log->same_pk);
      goto all_done;
    }
  }

  if (num_v) {
    byte *b = (byte *)mrec + rec_offs_data_size(moffsets);
    trx_undo_read_v_cols(log->table, b, old_pk, false, true,
                         &(log->col_map[log->n_old_col]), heap);
  }

  return (row_log_table_apply_delete_low(&pcur, old_pk, offsets, heap, &mtr));
}

/** Replays an update operation on the multi-value index.
@param[in]      index           multi-value index
@param[in]      n_index         the sequence of the index
@param[in]      old_row         old row of the update
@param[in]      old_ext         old external data of the update
@param[in]      new_row         new row of the update
@param[in]      non_mv_upd      true if any non-multi-value field on the index
                                gets updated too
@param[in]      trx_id          transaction id of the update
@param[in]      thr             query graph
@param[in,out]  offsets_heap    memory heap that can be emptied
@param[in,out]  heap            memory heap
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t apply_update_multi_value(
    dict_index_t *index, uint32_t n_index, const dtuple_t *old_row,
    row_ext_t *old_ext, const dtuple_t *new_row, bool non_mv_upd,
    trx_id_t trx_id, que_thr_t *thr, mem_heap_t *offsets_heap,
    mem_heap_t *heap) {
  btr_pcur_t pcur;
  mtr_t mtr;
  dberr_t error = DB_SUCCESS;

  ut_ad(index->is_multi_value());

  /* Do the same with other secondary index handling in
  row_log_table_apply_update() */
  {
    Multi_value_entry_builder_normal mv_entry_builder(old_row, old_ext, index,
                                                      heap, true, !non_mv_upd);

    for (dtuple_t *entry = mv_entry_builder.begin(); entry != nullptr;
         entry = mv_entry_builder.next()) {
      mtr_start(&mtr);
      if (row_search_index_entry(index, entry, BTR_MODIFY_TREE, &pcur, &mtr) !=
          ROW_FOUND) {
        mtr_commit(&mtr);
        ut_d(ut_error);
        ut_o(return (DB_CORRUPTION));
      }

      btr_cur_pessimistic_delete(&error, false, pcur.get_btr_cur(),
                                 BTR_CREATE_FLAG, false, 0, 0, 0, &mtr, &pcur,
                                 nullptr);

      if (error != DB_SUCCESS) {
        return (error);
      }

      mtr_commit(&mtr);
    }
  }

  {
    Multi_value_entry_builder_normal mv_entry_builder(new_row, nullptr, index,
                                                      heap, true, !non_mv_upd);

    for (dtuple_t *entry = mv_entry_builder.begin(); entry != nullptr;
         entry = mv_entry_builder.next()) {
      error = row_ins_sec_index_entry_low(
          BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG |
              BTR_KEEP_SYS_FLAG,
          BTR_MODIFY_TREE, index, offsets_heap, heap, entry, trx_id, thr,
          false);

      if (error == DB_DUPLICATE_KEY) {
        thr_get_trx(thr)->error_key_num = n_index;
      }

      if (error != DB_SUCCESS) {
        return (error);
      }
    }
  }

  return (error);
}

/** Replays an update operation on a table that was rebuilt.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_log_table_apply_update(
    que_thr_t *thr,           /*!< in: query graph */
    ulint new_trx_id_col,     /*!< in: position of
                              DB_TRX_ID in the new
                              clustered index */
    const ddl::mrec_t *mrec,  /*!< in: new value */
    const ulint *offsets,     /*!< in: offsets of mrec */
    mem_heap_t *offsets_heap, /*!< in/out: memory heap
                              that can be emptied */
    mem_heap_t *heap,         /*!< in/out: memory heap */
    ddl::Dup *dup,            /*!< in/out: for reporting
                                     duplicate key errors */
    trx_id_t trx_id,          /*!< in: DB_TRX_ID of mrec */
    const dtuple_t *old_pk)   /*!< in: PRIMARY KEY and
                              DB_TRX_ID,DB_ROLL_PTR
                              of the old value,
                              or PRIMARY KEY if same_pk */
{
  const row_log_t *log = dup->m_index->online_log;
  const dtuple_t *row;
  dict_index_t *index = log->table->first_index();
  mtr_t mtr;
  btr_pcur_t pcur;
  dberr_t error;
  bool non_mv_upd = true;
  uint32_t n_index = 0;
  trx_t *trx = thr_get_trx(thr);

  ut_ad(dtuple_get_n_fields_cmp(old_pk) == dict_index_get_n_unique(index));
  ut_ad(dtuple_get_n_fields(old_pk) ==
        dict_index_get_n_unique(index) + (log->same_pk ? 0 : 2));

  row = row_log_table_apply_convert_mrec(mrec, dup->m_index, offsets, log, heap,
                                         &error);

  switch (error) {
    case DB_MISSING_HISTORY:
      /* The record contained BLOBs that are now missing. */
      ut_ad(log->blobs);
      /* Whether or not we are updating the PRIMARY KEY, we
      know that there should be a subsequent
      ROW_T_DELETE for rolling back a preceding ROW_T_INSERT,
      overriding this ROW_T_UPDATE record. (*1)

      This allows us to interpret this ROW_T_UPDATE
      as ROW_T_DELETE.

      When applying the subsequent ROW_T_DELETE, no matching
      record will be found. */
      [[fallthrough]];
    case DB_SUCCESS:
      ut_ad(row != nullptr);
      break;
    default:
      ut_d(ut_error);
      [[fallthrough]];
    case DB_INVALID_NULL:
      ut_ad(row == nullptr);
      return (error);
  }

  mtr_start(&mtr);

  pcur.open(index, 0, old_pk, PAGE_CUR_LE, BTR_MODIFY_TREE, &mtr,
            UT_LOCATION_HERE);
#ifdef UNIV_DEBUG
  switch (pcur.get_btr_cur()->flag) {
    case BTR_CUR_UNSET:
    case BTR_CUR_DELETE_REF:
    case BTR_CUR_DEL_MARK_IBUF:
    case BTR_CUR_DELETE_IBUF:
    case BTR_CUR_INSERT_TO_IBUF:
      ut_d(ut_error); /* We did not request buffering. */
    case BTR_CUR_HASH:
    case BTR_CUR_HASH_NOT_ATTEMPTED:
    case BTR_CUR_HASH_FAIL:
    case BTR_CUR_BINARY:
      break;
  }
#endif /* UNIV_DEBUG */

  if (page_rec_is_infimum(pcur.get_rec()) ||
      pcur.get_low_match() < index->n_uniq) {
    /* The record was not found. This should only happen
    when an earlier ROW_T_INSERT or ROW_T_UPDATE was
    diverted because BLOBs were freed when the insert was
    later rolled back. */

    ut_ad(log->blobs);

    if (error == DB_SUCCESS) {
      /* An earlier ROW_T_INSERT could have been
      skipped because of a missing BLOB, like this:

      BEGIN;
      INSERT INTO t SET blob_col='blob value';
      UPDATE t SET blob_col='';
      ROLLBACK;

      This would generate the following records:
      ROW_T_INSERT (referring to 'blob value')
      ROW_T_UPDATE
      ROW_T_UPDATE (referring to 'blob value')
      ROW_T_DELETE
      [ROLLBACK removes the 'blob value']

      The ROW_T_INSERT would have been skipped
      because of a missing BLOB. Now we are
      executing the first ROW_T_UPDATE.
      The second ROW_T_UPDATE (for the ROLLBACK)
      would be interpreted as ROW_T_DELETE, because
      the BLOB would be missing.

      We could probably assume that the transaction
      has been rolled back and simply skip the
      'insert' part of this ROW_T_UPDATE record.
      However, there might be some complex scenario
      that could interfere with such a shortcut.
      So, we will insert the row (and risk
      introducing a bogus duplicate key error
      for the ALTER TABLE), and a subsequent
      ROW_T_UPDATE or ROW_T_DELETE will delete it. */
      mtr_commit(&mtr);
      error = row_log_table_apply_insert_low(thr, row, trx_id, offsets_heap,
                                             heap, dup);
    } else {
      /* Some BLOBs are missing, so we are interpreting
      this ROW_T_UPDATE as ROW_T_DELETE (see *1).
      Because the record was not found, we do nothing. */
      ut_ad(error == DB_MISSING_HISTORY);
      error = DB_SUCCESS;
    func_exit:
      mtr_commit(&mtr);
    }
  func_exit_committed:
    ut_ad(mtr.has_committed());

    if (error != DB_SUCCESS) {
      /* Report the erroneous row using the new
      version of the table. */
      innobase_row_to_mysql(dup->m_table, log->table, row);
    }

    return (error);
  }

  /* Prepare to update (or delete) the record. */
  ulint *cur_offsets =
      rec_get_offsets(pcur.get_rec(), index, nullptr, ULINT_UNDEFINED,
                      UT_LOCATION_HERE, &offsets_heap);

  if (!log->same_pk) {
    /* Only update the record if DB_TRX_ID,DB_ROLL_PTR match what
    was buffered. */
    ulint len;
    const void *rec_trx_id = rec_get_nth_field(
        nullptr, pcur.get_rec(), cur_offsets, index->n_uniq, &len);
    ut_ad(len == DATA_TRX_ID_LEN);
    ut_ad(dtuple_get_nth_field(old_pk, index->n_uniq)->len == DATA_TRX_ID_LEN);
    ut_ad(dtuple_get_nth_field(old_pk, index->n_uniq + 1)->len ==
          DATA_ROLL_PTR_LEN);
    ut_ad(DATA_TRX_ID_LEN +
              static_cast<const char *>(
                  dtuple_get_nth_field(old_pk, index->n_uniq)->data) ==
          dtuple_get_nth_field(old_pk, index->n_uniq + 1)->data);
    if (memcmp(rec_trx_id, dtuple_get_nth_field(old_pk, index->n_uniq)->data,
               DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)) {
      /* The ROW_T_UPDATE was logged for a different
      DB_TRX_ID,DB_ROLL_PTR. This is possible if an
      earlier ROW_T_INSERT or ROW_T_UPDATE was diverted
      because some BLOBs were missing due to rolling
      back the initial insert or due to purging
      the old BLOB values of an update. */
      ut_ad(log->blobs);
      if (error != DB_SUCCESS) {
        ut_ad(error == DB_MISSING_HISTORY);
        /* Some BLOBs are missing, so we are
        interpreting this ROW_T_UPDATE as
        ROW_T_DELETE (see *1).
        Because this is a different row,
        we will do nothing. */
        error = DB_SUCCESS;
      } else {
        /* Because the user record is missing due to
        BLOBs that were missing when processing
        an earlier log record, we should
        interpret the ROW_T_UPDATE as ROW_T_INSERT.
        However, there is a different user record
        with the same PRIMARY KEY value already. */
        error = DB_DUPLICATE_KEY;
      }

      goto func_exit;
    }
  }

  if (error != DB_SUCCESS) {
    ut_ad(error == DB_MISSING_HISTORY);
    ut_ad(log->blobs);
    /* Some BLOBs are missing, so we are interpreting
    this ROW_T_UPDATE as ROW_T_DELETE (see *1). */
    error =
        row_log_table_apply_delete_low(&pcur, old_pk, cur_offsets, heap, &mtr);
    goto func_exit_committed;
  }

  /** It allows to create tuple with virtual column information. */
  dtuple_t *entry = row_build_index_entry_low(row, nullptr, index, heap,
                                              ROW_BUILD_FOR_INSERT);
  upd_t *update = row_upd_build_difference_binary(index, entry, pcur.get_rec(),
                                                  cur_offsets, false, nullptr,
                                                  heap, dup->m_table, &error);
  if (error != DB_SUCCESS) {
    goto func_exit;
  }

  if (!update->n_fields) {
    /* Nothing to do. */
    goto func_exit;
  }

  const bool pk_updated =
      upd_get_nth_field(update, 0)->field_no < new_trx_id_col;

  if (pk_updated || rec_offs_any_extern(cur_offsets)) {
    /* If the record contains any externally stored
    columns, perform the update by delete and insert,
    because we will not write any undo log that would
    allow purge to free any orphaned externally stored
    columns. */

    if (pk_updated && log->same_pk) {
      /* The ROW_T_UPDATE log record should only be
      written when the PRIMARY KEY fields of the
      record did not change in the old table.  We
      can only get a change of PRIMARY KEY columns
      in the rebuilt table if the PRIMARY KEY was
      redefined (!same_pk). */
      error = DB_CORRUPTION;
      ut_d(ut_error);
      ut_o(goto func_exit);
    }

    error =
        row_log_table_apply_delete_low(&pcur, old_pk, cur_offsets, heap, &mtr);
    ut_ad(mtr.has_committed());

    if (error == DB_SUCCESS) {
      error = row_log_table_apply_insert_low(thr, row, trx_id, offsets_heap,
                                             heap, dup);
    }

    goto func_exit_committed;
  }

  dtuple_t *old_row;
  row_ext_t *old_ext;

  if (index->next() != nullptr) {
    /* Construct the row corresponding to the old value of
    the record. */
    old_row = row_build(ROW_COPY_DATA, index, pcur.get_rec(), cur_offsets,
                        nullptr, nullptr, nullptr, &old_ext, heap);
    ut_ad(old_row);

    DBUG_PRINT("ib_alter_table",
               ("update table " IB_ID_FMT "(index " IB_ID_FMT "): %s to %s",
                index->table->id, index->id, rec_printer(old_row).str().c_str(),
                rec_printer(row).str().c_str()));
  } else {
    old_row = nullptr;
    old_ext = nullptr;
  }

  big_rec_t *big_rec;

  error = btr_cur_pessimistic_update(
      BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG |
          BTR_KEEP_SYS_FLAG | BTR_KEEP_POS_FLAG,
      pcur.get_btr_cur(), &cur_offsets, &offsets_heap, heap, &big_rec, update,
      0, thr, 0, 0, &mtr);

  if (big_rec) {
    if (error == DB_SUCCESS) {
      error = lob::btr_store_big_rec_extern_fields(
          trx, &pcur, update, cur_offsets, big_rec, &mtr, lob::OPCODE_UPDATE);
    }

    dtuple_big_rec_free(big_rec);
  }
  bool vfields_copied = false;
  while ((index = index->next()) != nullptr) {
    n_index++;
    if (error != DB_SUCCESS) {
      break;
    }

    if (index->type & DICT_FTS) {
      continue;
    }

    if (!vfields_copied && dict_index_has_virtual(index)) {
      dtuple_copy_v_fields(old_row, old_pk);
      vfields_copied = true;
    }

    if (!row_upd_changes_ord_field_binary(
            index, update, thr, old_row, nullptr,
            (index->is_multi_value() ? &non_mv_upd : nullptr))) {
      continue;
    }

    mtr_commit(&mtr);

    if (index->is_multi_value()) {
      error =
          apply_update_multi_value(index, n_index, old_row, old_ext, row,
                                   non_mv_upd, trx_id, thr, offsets_heap, heap);
      mtr_start(&mtr);
      continue;
    }

    entry = row_build_index_entry(old_row, old_ext, index, heap);
    if (!entry) {
      ut_d(ut_error);
      ut_o(return (DB_CORRUPTION));
    }

    mtr_start(&mtr);

    if (ROW_FOUND !=
        row_search_index_entry(index, entry, BTR_MODIFY_TREE, &pcur, &mtr)) {
      error = DB_CORRUPTION;
      ut_d(ut_error);
      ut_o(break);
    }

    btr_cur_pessimistic_delete(&error, false, pcur.get_btr_cur(),
                               BTR_CREATE_FLAG, false, 0, 0, 0, &mtr, &pcur,
                               nullptr);

    if (error != DB_SUCCESS) {
      break;
    }

    mtr_commit(&mtr);

    entry = row_build_index_entry(row, nullptr, index, heap);
    error = row_ins_sec_index_entry_low(
        BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG |
            BTR_KEEP_SYS_FLAG,
        BTR_MODIFY_TREE, index, offsets_heap, heap, entry, trx_id, thr, false);

    /* Report correct index name for duplicate key error. */
    if (error == DB_DUPLICATE_KEY) {
      thr_get_trx(thr)->error_key_num = n_index;
    }

    mtr_start(&mtr);
  }

  goto func_exit;
}

/** Applies an operation to a table that was rebuilt.
 @return NULL on failure (mrec corruption) or when out of data;
 pointer to next record on success */
[[nodiscard]] static const ddl::mrec_t *row_log_table_apply_op(
    que_thr_t *thr,              /*!< in: query graph */
    ulint trx_id_col,            /*!< in: position of
                                 DB_TRX_ID in old index */
    ulint new_trx_id_col,        /*!< in: position of
                                 DB_TRX_ID in new index */
    ddl::Dup *dup,               /*!< in/out: for reporting
                                        duplicate key errors */
    dberr_t *error,              /*!< out: DB_SUCCESS
                                 or error code */
    mem_heap_t *offsets_heap,    /*!< in/out: memory heap
                                 that can be emptied */
    mem_heap_t *heap,            /*!< in/out: memory heap */
    const ddl::mrec_t *mrec,     /*!< in: merge record */
    const ddl::mrec_t *mrec_end, /*!< in: end of buffer */
    ulint *offsets)              /*!< in/out: work area
                                 for parsing mrec */
{
  row_log_t *log = dup->m_index->online_log;
  dict_index_t *new_index = log->table->first_index();
  ulint extra_size;
  const ddl::mrec_t *next_mrec;
  dtuple_t *old_pk;

  ut_ad(dup->m_index->is_clustered());
  ut_ad(dup->m_index->table != log->table);
  ut_ad(log->head.total <= log->tail.total);

  *error = DB_SUCCESS;

  /* 3 = 1 (op type) + 1 (extra_size) + at least 1 byte payload */
  if (mrec + 3 >= mrec_end) {
    return (nullptr);
  }

  const ddl::mrec_t *const mrec_start = mrec;

  switch (*mrec++) {
    default:
      *error = DB_CORRUPTION;
      ut_d(ut_error);
      ut_o(return (nullptr));
    case ROW_T_INSERT:
      extra_size = *mrec++;

      if (extra_size >= 0x80) {
        /* Read another byte of extra_size. */

        extra_size = (extra_size & 0x7f) << 8;
        extra_size |= *mrec++;
      }

      mrec += extra_size;

      if (mrec > mrec_end) {
        return (nullptr);
      }

      rec_offs_set_n_fields(offsets, dup->m_index->get_n_total_fields());
      rec_deserialize_init_offsets(mrec, dup->m_index, offsets);

      next_mrec = mrec + rec_offs_data_size(offsets);

      if (log->table->n_v_cols) {
        if (next_mrec + 2 > mrec_end) {
          return (nullptr);
        }
        next_mrec += mach_read_from_2(next_mrec);
      }

      if (next_mrec > mrec_end) {
        return (nullptr);
      } else {
        log->head.total += next_mrec - mrec_start;

        ulint len;
        const byte *db_trx_id =
            rec_get_nth_field(nullptr, mrec, offsets, trx_id_col, &len);
        ut_ad(len == DATA_TRX_ID_LEN);
        *error =
            row_log_table_apply_insert(thr, mrec, offsets, offsets_heap, heap,
                                       dup, trx_read_trx_id(db_trx_id));
      }
      break;

    case ROW_T_DELETE:
      /* 1 (extra_size) + at least 1 (payload) */
      if (mrec + 2 >= mrec_end) {
        return (nullptr);
      }

      extra_size = *mrec++;
      ut_ad(mrec < mrec_end);

      /* We assume extra_size < 0x100 for the PRIMARY KEY prefix.
      For fixed-length PRIMARY key columns, it is 0. */
      mrec += extra_size;

      rec_offs_set_n_fields(offsets, new_index->n_uniq + 2);
      rec_deserialize_init_offsets(mrec, new_index, offsets);
      next_mrec = mrec + rec_offs_data_size(offsets);
      if (log->table->n_v_cols) {
        if (next_mrec + 2 > mrec_end) {
          return (nullptr);
        }

        next_mrec += mach_read_from_2(next_mrec);
      }

      if (next_mrec > mrec_end) {
        return (nullptr);
      }

      log->head.total += next_mrec - mrec_start;

      *error = row_log_table_apply_delete(new_trx_id_col, mrec, offsets,
                                          offsets_heap, heap, log);
      break;

    case ROW_T_UPDATE:
      /* Logically, the log entry consists of the
      (PRIMARY KEY,DB_TRX_ID) of the old value (converted
      to the new primary key definition) followed by
      the new value in the old table definition. If the
      definition of the columns belonging to PRIMARY KEY
      is not changed, the log will only contain
      DB_TRX_ID,new_row. */
      ulint num_v = new_index->table->n_v_cols;

      if (dup->m_index->online_log->same_pk) {
        ut_ad(new_index->n_uniq == dup->m_index->n_uniq);

        extra_size = *mrec++;

        if (extra_size >= 0x80) {
          /* Read another byte of extra_size. */

          extra_size = (extra_size & 0x7f) << 8;
          extra_size |= *mrec++;
        }

        mrec += extra_size;

        if (mrec > mrec_end) {
          return (nullptr);
        }

        rec_offs_set_n_fields(offsets, dup->m_index->get_n_total_fields());
        rec_deserialize_init_offsets(mrec, dup->m_index, offsets);

        next_mrec = mrec + rec_offs_data_size(offsets);

        if (next_mrec > mrec_end) {
          return (nullptr);
        }

        old_pk = dtuple_create_with_vcol(heap, new_index->n_uniq, num_v);
        dict_index_copy_types(old_pk, new_index, old_pk->n_fields);
        if (num_v) {
          dict_table_copy_v_types(old_pk, new_index->table);
        }

        /* Copy the PRIMARY KEY fields from mrec to old_pk. */
        for (ulint i = 0; i < new_index->n_uniq; i++) {
          const void *field;
          ulint len;
          dfield_t *dfield;

          ut_ad(!rec_offs_nth_extern(new_index, offsets, i));

          field = rec_get_nth_field(nullptr, mrec, offsets, i, &len);
          ut_ad(rec_field_not_null_not_add_col_def(len));

          dfield = dtuple_get_nth_field(old_pk, i);
          dfield_set_data(dfield, field, len);
        }
      } else {
        /* We assume extra_size < 0x100
        for the PRIMARY KEY prefix. */
        mrec += *mrec + 1;

        if (mrec > mrec_end) {
          return (nullptr);
        }

        /* Get offsets for PRIMARY KEY,
        DB_TRX_ID, DB_ROLL_PTR. */
        rec_offs_set_n_fields(offsets, new_index->n_uniq + 2);
        rec_deserialize_init_offsets(mrec, new_index, offsets);

        next_mrec = mrec + rec_offs_data_size(offsets);
        if (next_mrec + 2 > mrec_end) {
          return (nullptr);
        }

        /* Copy the PRIMARY KEY fields and
        DB_TRX_ID, DB_ROLL_PTR from mrec to old_pk. */
        old_pk = dtuple_create_with_vcol(heap, new_index->n_uniq + 2, num_v);
        dict_index_copy_types(old_pk, new_index, old_pk->n_fields);

        if (num_v) {
          dict_table_copy_v_types(old_pk, new_index->table);
        }

        for (ulint i = 0; i < dict_index_get_n_unique(new_index) + 2; i++) {
          const void *field;
          ulint len;
          dfield_t *dfield;

          ut_ad(!rec_offs_nth_extern(new_index, offsets, i));

          field = rec_get_nth_field(nullptr, mrec, offsets, i, &len);
          ut_ad(rec_field_not_null_not_add_col_def(len));

          dfield = dtuple_get_nth_field(old_pk, i);
          dfield_set_data(dfield, field, len);
        }

        mrec = next_mrec;

        /* Fetch the new value of the row as it was
        in the old table definition. */
        extra_size = *mrec++;

        if (extra_size >= 0x80) {
          /* Read another byte of extra_size. */

          extra_size = (extra_size & 0x7f) << 8;
          extra_size |= *mrec++;
        }

        mrec += extra_size;

        if (mrec > mrec_end) {
          return (nullptr);
        }

        rec_offs_set_n_fields(offsets, dup->m_index->get_n_total_fields());
        rec_deserialize_init_offsets(mrec, dup->m_index, offsets);

        next_mrec = mrec + rec_offs_data_size(offsets);

        if (next_mrec > mrec_end) {
          return (nullptr);
        }
      }

      /* Read virtual column info from log */
      if (num_v) {
        ulint o_v_size = 0;
        ulint n_v_size = 0;
        if (next_mrec + 2 > mrec_end) {
          return (nullptr);
        }
        n_v_size = mach_read_from_2(next_mrec);
        next_mrec += n_v_size;

        if (next_mrec > mrec_end) {
          return (nullptr);
        }

        /* if there is more than 2 bytes length info */
        if (n_v_size > 2 && mrec_end > next_mrec) {
          trx_undo_read_v_cols(log->table, const_cast<byte *>(next_mrec),
                               old_pk, false, true,
                               &(log->col_map[log->n_old_col]), heap);
          o_v_size = mach_read_from_2(next_mrec);
        }

        next_mrec += o_v_size;
        if (next_mrec > mrec_end) {
          return (nullptr);
        }
      }

      ut_ad(next_mrec <= mrec_end);
      log->head.total += next_mrec - mrec_start;
      dtuple_set_n_fields_cmp(old_pk, new_index->n_uniq);

      {
        ulint len;
        const byte *db_trx_id =
            rec_get_nth_field(nullptr, mrec, offsets, trx_id_col, &len);
        ut_ad(len == DATA_TRX_ID_LEN);
        *error = row_log_table_apply_update(thr, new_trx_id_col, mrec, offsets,
                                            offsets_heap, heap, dup,
                                            trx_read_trx_id(db_trx_id), old_pk);
      }

      break;
  }

  ut_ad(log->head.total <= log->tail.total);
  mem_heap_empty(offsets_heap);
  mem_heap_empty(heap);
  return (next_mrec);
}

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Estimate how much an ALTER TABLE progress should be incremented per
one block of log applied.
For the other phases of ALTER TABLE we increment the progress with 1 per
page processed.
@return amount of abstract units to add to work_completed when one block
of log is applied.
*/
inline ulint row_log_progress_inc_per_block() {
  /* We must increment the progress once per page (as in
  univ_page_size, usually 16KiB). One block here is srv_sort_buf_size
  (usually 1MiB). */
  const ulint pages_per_block = std::max(
      static_cast<unsigned long>(srv_sort_buf_size / univ_page_size.physical()),
      1UL);

  /* Multiply by an artificial factor of 6 to even the pace with
  the rest of the ALTER TABLE phases, they process page_size amount
  of data faster. */
  return (pages_per_block * 6);
}

/** Estimate how much work is to be done by the log apply phase
of an ALTER TABLE for this index.
@param[in]      index   index whose log to assess
@return work to be done by log-apply in abstract units
*/
ulint row_log_estimate_work(const dict_index_t *index) {
  if (index == nullptr || index->online_log == nullptr) {
    return (0);
  }

  const row_log_t *l = index->online_log;
  const ulint bytes_left = static_cast<ulint>(l->tail.total - l->head.total);
  const ulint blocks_left = bytes_left / srv_sort_buf_size;

  return (blocks_left * row_log_progress_inc_per_block());
}
#else  /* HAVE_PSI_STAGE_INTERFACE */
inline ulint row_log_progress_inc_per_block() { return (0); }
#endif /* HAVE_PSI_STAGE_INTERFACE */

/** Applies operations to a table was rebuilt.
@param[in]      thr     query graph
@param[in,out]  dup     for reporting duplicate key errors
@param[in,out]  stage   performance schema accounting object, used by
ALTER TABLE. If not NULL, then stage->inc() will be called for each block
of log that is applied.
@return DB_SUCCESS, or error code on failure */
[[nodiscard]] static dberr_t row_log_table_apply_ops(que_thr_t *thr,
                                                     ddl::Dup *dup,
                                                     Alter_stage *stage) {
  dberr_t error;
  const ddl::mrec_t *mrec = nullptr;
  const ddl::mrec_t *next_mrec;
  const ddl::mrec_t *mrec_end = nullptr; /* silence bogus warning */
  const ddl::mrec_t *next_mrec_end;
  mem_heap_t *heap;
  mem_heap_t *offsets_heap;
  ulint *offsets;
  bool has_index_lock;
  dict_index_t *index = const_cast<dict_index_t *>(dup->m_index);
  dict_table_t *new_table = index->online_log->table;
  dict_index_t *new_index = new_table->first_index();
  ulint n_fields = dict_index_get_n_fields(index);
  ulint n_unique = dict_index_get_n_unique(new_index) + 2;
  const ulint i = 1 + REC_OFFS_HEADER_SIZE + std::max(n_fields, n_unique);
  const ulint trx_id_col =
      dict_col_get_clust_pos(index->table->get_sys_col(DATA_TRX_ID), index);
  const ulint new_trx_id_col =
      dict_col_get_clust_pos(new_table->get_sys_col(DATA_TRX_ID), new_index);
  trx_t *trx = thr_get_trx(thr);
  dberr_t err;

  ut_ad(index->is_clustered());
  ut_ad(dict_index_is_online_ddl(index));
  ut_ad(trx->mysql_thd);
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));
  ut_ad(!dict_index_is_online_ddl(new_index));
  ut_ad(trx_id_col > 0);
  ut_ad(trx_id_col != ULINT_UNDEFINED);
  ut_ad(new_trx_id_col > 0);
  ut_ad(new_trx_id_col != ULINT_UNDEFINED);

  UNIV_MEM_INVALID(&mrec_end, sizeof mrec_end);

  offsets = static_cast<ulint *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, i * sizeof *offsets));
  offsets[0] = i;
  offsets[1] = dict_index_get_n_fields(index);

  heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
  offsets_heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
  has_index_lock = true;

next_block:
  ut_ad(has_index_lock);
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));
  ut_ad(index->online_log->head.bytes == 0);

  stage->inc(row_log_progress_inc_per_block());

  if (trx_is_interrupted(trx)) {
    goto interrupted;
  }

  if (index->is_corrupted()) {
    error = DB_INDEX_CORRUPT;
    goto func_exit;
  }

  ut_ad(dict_index_is_online_ddl(index));

  error = index->online_log->error;

  if (error != DB_SUCCESS) {
    goto func_exit;
  }

  if (UNIV_UNLIKELY(index->online_log->head.blocks >
                    index->online_log->tail.blocks)) {
    /* Note: release-only label. */
    ut_o(unexpected_eof:);
    ib::error(ER_IB_MSG_960)
        << "Unexpected end of temporary file for table " << index->table->name;
  corruption:
    error = DB_CORRUPTION;
    goto func_exit;
  }

  if (index->online_log->head.blocks == index->online_log->tail.blocks) {
    if (index->online_log->head.blocks) {
#ifdef HAVE_FTRUNCATE
      /* Truncate the file in order to save space. */
      if (index->online_log->file.is_open() &&
          ftruncate(index->online_log->file.get(), 0) == -1) {
        perror("ftruncate");
      }
#endif /* HAVE_FTRUNCATE */
      index->online_log->head.blocks = index->online_log->tail.blocks = 0;
    }

    next_mrec = index->online_log->tail.block;
    next_mrec_end = next_mrec + index->online_log->tail.bytes;

    if (next_mrec_end == next_mrec) {
      /* End of log reached. */
    all_done:
      ut_ad(has_index_lock);
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->tail.blocks == 0);
      index->online_log->head.bytes = 0;
      index->online_log->tail.bytes = 0;
      error = DB_SUCCESS;
      goto func_exit;
    }
  } else {
    os_offset_t ofs;

    ofs = (os_offset_t)index->online_log->head.blocks * srv_sort_buf_size;

    ut_ad(has_index_lock);
    has_index_lock = false;
    rw_lock_x_unlock(dict_index_get_lock(index));

    log_free_check();

    ut_ad(dict_index_is_online_ddl(index));

    if (!row_log_block_allocate(index->online_log->head)) {
      error = DB_OUT_OF_MEMORY;
      goto func_exit;
    }

    IORequest request(IORequest::READ | IORequest::ROW_LOG);
    ;

    err = os_file_read_no_error_handling_int_fd(
        request, index->online_log->path, index->online_log->file.get(),
        index->online_log->head.block, ofs, srv_sort_buf_size, nullptr);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_961) << "Unable to read temporary file"
                                  " for table "
                               << index->table_name;
      goto corruption;
    }

#ifdef POSIX_FADV_DONTNEED
    /* Each block is read exactly once.  Free up the file cache. */
    posix_fadvise(index->online_log->file.get(), ofs, srv_sort_buf_size,
                  POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

    next_mrec = index->online_log->head.block;
    next_mrec_end = next_mrec + srv_sort_buf_size;
  }

  /* This read is not protected by index->online_log->mutex for
  performance reasons. We will eventually notice any error that
  was flagged by a DML thread. */
  error = index->online_log->error;

  if (error != DB_SUCCESS) {
    goto func_exit;
  }

  if (mrec) {
    /* A partial record was read from the previous block.
    Copy the temporary buffer full, as we do not know the
    length of the record. Parse subsequent records from
    the bigger buffer index->online_log->head.block
    or index->online_log->tail.block. */

    ut_ad(mrec == index->online_log->head.buf);
    ut_ad(mrec_end > mrec);
    ut_ad(mrec_end < (&index->online_log->head.buf)[1]);

    memcpy((ddl::mrec_t *)mrec_end, next_mrec,
           (&index->online_log->head.buf)[1] - mrec_end);
    mrec =
        row_log_table_apply_op(thr, trx_id_col, new_trx_id_col, dup, &error,
                               offsets_heap, heap, index->online_log->head.buf,
                               (&index->online_log->head.buf)[1], offsets);
    if (error != DB_SUCCESS) {
      goto func_exit;
    } else if (UNIV_UNLIKELY(mrec == nullptr)) {
      /* The record was not reassembled properly. */
      goto corruption;
    }
    /* The record was previously found out to be
    truncated. Now that the parse buffer was extended,
    it should proceed beyond the old end of the buffer. */
    ut_a(mrec > mrec_end);

    index->online_log->head.bytes = mrec - mrec_end;
    next_mrec += index->online_log->head.bytes;
  }

  ut_ad(next_mrec <= next_mrec_end);
  /* The following loop must not be parsing the temporary
  buffer, but head.block or tail.block. */

  /* mrec!=NULL means that the next record starts from the
  middle of the block */
  ut_ad((mrec == nullptr) == (index->online_log->head.bytes == 0));

#ifdef UNIV_DEBUG
  if (index->online_log->head.block != nullptr &&
      next_mrec_end == index->online_log->head.block + srv_sort_buf_size) {
    /* If tail.bytes == 0, next_mrec_end can also be at
    the end of tail.block. */
    if (index->online_log->tail.bytes == 0) {
      ut_ad(next_mrec == next_mrec_end);
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->head.bytes == 0);
    } else {
      ut_ad(next_mrec ==
            index->online_log->head.block + index->online_log->head.bytes);
      ut_ad(index->online_log->tail.blocks > index->online_log->head.blocks);
    }
  } else if (next_mrec_end ==
             index->online_log->tail.block + index->online_log->tail.bytes) {
    ut_ad(next_mrec ==
          index->online_log->tail.block + index->online_log->head.bytes);
    ut_ad(index->online_log->tail.blocks == 0);
    ut_ad(index->online_log->head.blocks == 0);
    ut_ad(index->online_log->head.bytes <= index->online_log->tail.bytes);
  } else {
    ut_error;
  }
#endif /* UNIV_DEBUG */

  mrec_end = next_mrec_end;

  while (!trx_is_interrupted(trx)) {
    if (next_mrec == next_mrec_end && has_index_lock) {
      goto all_done;
    }

    mrec = next_mrec;

    ut_ad(mrec < mrec_end);

    if (!has_index_lock) {
      /* We are applying operations from a different
      block than the one that is being written to.
      We do not hold index->lock in order to
      allow other threads to concurrently buffer
      modifications. */
      ut_ad(mrec >= index->online_log->head.block);
      ut_ad(mrec_end == index->online_log->head.block + srv_sort_buf_size);
      ut_ad(index->online_log->head.bytes < srv_sort_buf_size);

      /* Take the opportunity to do a redo log
      checkpoint if needed. */
      log_free_check();
    } else {
      /* We are applying operations from the last block.
      Do not allow other threads to buffer anything,
      so that we can finally catch up and synchronize. */
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(mrec_end ==
            index->online_log->tail.block + index->online_log->tail.bytes);
      ut_ad(mrec >= index->online_log->tail.block);
    }

    /* This read is not protected by index->online_log->mutex
    for performance reasons. We will eventually notice any
    error that was flagged by a DML thread. */
    error = index->online_log->error;

    if (error != DB_SUCCESS) {
      goto func_exit;
    }

    next_mrec =
        row_log_table_apply_op(thr, trx_id_col, new_trx_id_col, dup, &error,
                               offsets_heap, heap, mrec, mrec_end, offsets);

    if (error != DB_SUCCESS) {
      goto func_exit;
    } else if (next_mrec == next_mrec_end) {
      /* The record happened to end on a block boundary.
      Do we have more blocks left? */
      if (has_index_lock) {
        /* The index will be locked while
        applying the last block. */
        goto all_done;
      }

      mrec = nullptr;
    process_next_block:
      rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
      has_index_lock = true;

      index->online_log->head.bytes = 0;
      index->online_log->head.blocks++;
      goto next_block;
    } else if (next_mrec != nullptr) {
      ut_ad(next_mrec < next_mrec_end);
      index->online_log->head.bytes += next_mrec - mrec;
    } else if (has_index_lock) {
      /* When mrec is within tail.block, it should
      be a complete record, because we are holding
      index->lock and thus excluding the writer. */
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(mrec_end ==
            index->online_log->tail.block + index->online_log->tail.bytes);
      ut_d(ut_error);
      ut_o(goto unexpected_eof);
    } else {
      memcpy(index->online_log->head.buf, mrec, mrec_end - mrec);
      mrec_end += index->online_log->head.buf - mrec;
      mrec = index->online_log->head.buf;
      goto process_next_block;
    }
  }

interrupted:
  error = DB_INTERRUPTED;
func_exit:
  if (!has_index_lock) {
    rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
  }

  mem_heap_free(offsets_heap);
  mem_heap_free(heap);
  row_log_block_free(index->online_log->head);
  ut::free(offsets);
  return (error);
}

/** Apply the row_log_table log to a table upon completing rebuild.
@param[in]      thr             query graph
@param[in]      old_table       old table
@param[in,out]  table           MySQL table (for reporting duplicates)
@param[in,out]  stage           performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_log_table() will be called initially and then
stage->inc() will be called for each block of log that is applied.
@return DB_SUCCESS, or error code on failure */
dberr_t row_log_table_apply(que_thr_t *thr, dict_table_t *old_table,
                            struct TABLE *table, Alter_stage *stage) {
  dberr_t error;
  dict_index_t *clust_index;

  thr_get_trx(thr)->error_key_num = 0;
  DBUG_EXECUTE_IF("innodb_trx_duplicates", thr->prebuilt->replace = 1;);

  stage->begin_phase_log_table();

  ut_ad(!rw_lock_own(dict_operation_lock, RW_LOCK_S));
  clust_index = old_table->first_index();

  rw_lock_x_lock(dict_index_get_lock(clust_index), UT_LOCATION_HERE);

  if (!clust_index->online_log) {
    ut_ad(dict_index_get_online_status(clust_index) == ONLINE_INDEX_COMPLETE);
    /* This function should not be called unless
    rebuilding a table online. Build in some fault
    tolerance. */
    error = DB_ERROR;
    ut_d(ut_error);
  } else {
    ddl::Dup dup = {clust_index, table, clust_index->online_log->col_map, 0};

    error = row_log_table_apply_ops(thr, &dup, stage);

    ut_ad(error != DB_SUCCESS || clust_index->online_log->head.total ==
                                     clust_index->online_log->tail.total);
  }

  rw_lock_x_unlock(dict_index_get_lock(clust_index));
  DBUG_EXECUTE_IF("innodb_trx_duplicates", thr->prebuilt->replace = 0;);

  return (error);
}

bool row_log_allocate(dict_index_t *index, dict_table_t *table, bool same_pk,
                      const dtuple_t *add_cols, const ulint *col_map,
                      const char *path) {
  row_log_t *log;
  DBUG_TRACE;

  ut_ad(!dict_index_is_online_ddl(index));
  ut_ad(index->is_clustered() == !!table);
  ut_ad(!table || index->table != table);
  ut_ad(same_pk || table);
  ut_ad(!table || col_map);
  ut_ad(!add_cols || col_map);
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));

  log = ut::new_withkey<row_log_t>(UT_NEW_THIS_FILE_PSI_KEY);

  if (log == nullptr) {
    return false;
  }

  mutex_create(LATCH_ID_INDEX_ONLINE_LOG, &log->mutex);

  log->blobs = nullptr;
  log->table = table;
  log->same_pk = same_pk;
  log->add_cols = add_cols;
  log->col_map = col_map;
  log->error = DB_SUCCESS;
  log->max_trx = 0;
  log->tail.blocks = log->tail.bytes = 0;
  log->tail.total = 0;
  log->tail.block = log->head.block = nullptr;
  log->head.blocks = log->head.bytes = 0;
  log->head.total = 0;
  log->path = path;
  log->n_old_col = index->table->n_cols;
  log->n_old_vcol = index->table->n_v_cols;

  dict_index_set_online_status(index, ONLINE_INDEX_CREATION);
  index->online_log = log;

  /* While we might be holding an exclusive data dictionary lock
  here, in row_log_abort_sec() we will not always be holding it. Use
  atomic operations in both cases. */
  MONITOR_ATOMIC_INC(MONITOR_ONLINE_CREATE_INDEX);

  return true;
}

/** Free the row log for an index that was being created online. */
void row_log_free(row_log_t *&log) /*!< in,own: row log */
{
  MONITOR_ATOMIC_DEC(MONITOR_ONLINE_CREATE_INDEX);

  ut::delete_(log->blobs);
  row_log_block_free(log->tail);
  row_log_block_free(log->head);
  mutex_free(&log->mutex);
  ut::free(log);
  log = nullptr;
}

/** Get the latest transaction ID that has invoked row_log_online_op()
 during online creation.
 @return latest transaction ID, or 0 if nothing was logged */
trx_id_t row_log_get_max_trx(
    dict_index_t *index) /*!< in: index, must be locked */
{
  ut_ad(dict_index_get_online_status(index) == ONLINE_INDEX_CREATION);

  ut_ad((rw_lock_own(dict_index_get_lock(index), RW_LOCK_S) &&
         mutex_own(&index->online_log->mutex)) ||
        rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));

  return (index->online_log->max_trx);
}

/** Applies an operation to a secondary index that was being created. */
static void row_log_apply_op_low(
    dict_index_t *index,      /*!< in/out: index */
    ddl::Dup *dup,            /*!< in/out: for reporting
                                     duplicate key errors */
    dberr_t *error,           /*!< out: DB_SUCCESS or error code */
    mem_heap_t *offsets_heap, /*!< in/out: memory heap for
                              allocating offsets; can be emptied */
    bool has_index_lock,      /*!< in: true if holding index->lock
                              in exclusive mode */
    enum row_op op,           /*!< in: operation being applied */
    trx_id_t trx_id,          /*!< in: transaction identifier */
    const dtuple_t *entry)    /*!< in: row */
{
  mtr_t mtr;
  btr_cur_t cursor;
  ulint *offsets = nullptr;

  ut_ad(!index->is_clustered());

  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X) == has_index_lock);

  ut_ad(!index->is_corrupted());
  ut_ad(trx_id != 0 || op == ROW_OP_DELETE);

  DBUG_PRINT("ib_create_index",
             ("%s %s index " IB_ID_FMT "," TRX_ID_FMT ": %s",
              op == ROW_OP_INSERT ? "insert" : "delete",
              has_index_lock ? "locked" : "unlocked", index->id, trx_id,
              rec_printer(entry).str().c_str()));

  mtr_start(&mtr);

  /* We perform the pessimistic variant of the operations if we
  already hold index->lock exclusively. First, search the
  record. The operation may already have been performed,
  depending on when the row in the clustered index was
  scanned. */
  btr_cur_search_to_nth_level(
      index, 0, entry, PAGE_CUR_LE,
      has_index_lock ? BTR_MODIFY_TREE : BTR_MODIFY_LEAF, &cursor, 0, __FILE__,
      __LINE__, &mtr);

  ut_ad(dict_index_get_n_unique(index) > 0);
  /* This test is somewhat similar to row_ins_must_modify_rec(),
  but not identical for unique secondary indexes. */
  if (cursor.low_match >= dict_index_get_n_unique(index) &&
      !page_rec_is_infimum(btr_cur_get_rec(&cursor))) {
    /* We have a matching record. */
    bool exists = (cursor.low_match == dict_index_get_n_fields(index));
#ifdef UNIV_DEBUG
    rec_t *rec = btr_cur_get_rec(&cursor);
    ut_ad(page_rec_is_user_rec(rec));
    ut_ad(!rec_get_deleted_flag(rec, page_rec_is_comp(rec)));
#endif /* UNIV_DEBUG */

    ut_ad(exists || dict_index_is_unique(index));

    switch (op) {
      case ROW_OP_DELETE:
        if (!exists) {
          /* The existing record matches the
          unique secondary index key, but the
          PRIMARY KEY columns differ. So, this
          exact record does not exist. For
          example, we could detect a duplicate
          key error in some old index before
          logging an ROW_OP_INSERT for our
          index. This ROW_OP_DELETE could have
          been logged for rolling back
          TRX_UNDO_INSERT_REC. */
          goto func_exit;
        }

        if (btr_cur_optimistic_delete(&cursor, BTR_CREATE_FLAG, &mtr)) {
          *error = DB_SUCCESS;
          break;
        }

        if (!has_index_lock) {
          /* This needs a pessimistic operation.
          Lock the index tree exclusively. */
          mtr_commit(&mtr);

          mtr_start(&mtr);

          btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
                                      BTR_MODIFY_TREE, &cursor, 0, __FILE__,
                                      __LINE__, &mtr);

          /* No other thread than the current one
          is allowed to modify the index tree.
          Thus, the record should still exist. */
          ut_ad(cursor.low_match >= dict_index_get_n_fields(index));
          ut_ad(page_rec_is_user_rec(btr_cur_get_rec(&cursor)));
        }

        /* As there are no externally stored fields in
        a secondary index record, the parameter
        rollback=false will be ignored. */

        btr_cur_pessimistic_delete(error, false, &cursor, BTR_CREATE_FLAG,
                                   false, 0, 0, 0, &mtr, nullptr, nullptr);
        break;
      case ROW_OP_INSERT:
        if (exists) {
          /* The record already exists. There
          is nothing to be inserted.
          This could happen when processing
          TRX_UNDO_DEL_MARK_REC in statement
          rollback:

          UPDATE of PRIMARY KEY can lead to
          statement rollback if the updated
          value of the PRIMARY KEY already
          exists. In this case, the UPDATE would
          be mapped to DELETE;INSERT, and we
          only wrote undo log for the DELETE
          part. The duplicate key error would be
          triggered before logging the INSERT
          part.

          Theoretically, we could also get a
          similar situation when a DELETE operation
          is blocked by a FOREIGN KEY constraint. */
          goto func_exit;
        }

        if (dtuple_contains_null(entry)) {
          /* The UNIQUE KEY columns match, but
          there is a NULL value in the key, and
          NULL!=NULL. */
          goto insert_the_rec;
        }

        goto duplicate;
    }
  } else {
    switch (op) {
      rec_t *rec;
      big_rec_t *big_rec;
      case ROW_OP_DELETE:
        /* The record does not exist. For example, we
        could detect a duplicate key error in some old
        index before logging an ROW_OP_INSERT for our
        index. This ROW_OP_DELETE could be logged for
        rolling back TRX_UNDO_INSERT_REC. */
        goto func_exit;
      case ROW_OP_INSERT:
        if (dict_index_is_unique(index) &&
            (cursor.up_match >= dict_index_get_n_unique(index) ||
             cursor.low_match >= dict_index_get_n_unique(index)) &&
            (!index->n_nullable || !dtuple_contains_null(entry))) {
        duplicate:
          /* Duplicate key */
          ut_ad(dict_index_is_unique(index));
          dup->report(entry->fields);
          *error = DB_DUPLICATE_KEY;
          goto func_exit;
        }
      insert_the_rec:
        /* Insert the record. As we are inserting into
        a secondary index, there cannot be externally
        stored columns (!big_rec). */
        *error = btr_cur_optimistic_insert(
            BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG,
            &cursor, &offsets, &offsets_heap, const_cast<dtuple_t *>(entry),
            &rec, &big_rec, nullptr, &mtr);
        ut_ad(!big_rec);
        if (*error != DB_FAIL) {
          break;
        }

        if (!has_index_lock) {
          /* This needs a pessimistic operation.
          Lock the index tree exclusively. */
          mtr_commit(&mtr);

          mtr_start(&mtr);

          btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
                                      BTR_MODIFY_TREE, &cursor, 0, __FILE__,
                                      __LINE__, &mtr);
        }

        /* We already determined that the
        record did not exist. No other thread
        than the current one is allowed to
        modify the index tree. Thus, the
        record should still not exist. */

        *error = btr_cur_pessimistic_insert(
            BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG | BTR_CREATE_FLAG,
            &cursor, &offsets, &offsets_heap, const_cast<dtuple_t *>(entry),
            &rec, &big_rec, nullptr, &mtr);
        ut_ad(!big_rec);
        break;
    }
    mem_heap_empty(offsets_heap);
  }

  if (*error == DB_SUCCESS && trx_id) {
    page_update_max_trx_id(btr_cur_get_block(&cursor),
                           btr_cur_get_page_zip(&cursor), trx_id, &mtr);
  }

func_exit:
  mtr_commit(&mtr);
}

/** Applies an operation to a secondary index that was being created.
 @return NULL on failure (mrec corruption) or when out of data;
 pointer to next record on success */
[[nodiscard]] static const ddl::mrec_t *row_log_apply_op(
    dict_index_t *index,         /*!< in/out: index */
    ddl::Dup *dup,               /*!< in/out: for reporting
                                        duplicate key errors */
    dberr_t *error,              /*!< out: DB_SUCCESS or error code */
    mem_heap_t *offsets_heap,    /*!< in/out: memory heap for
                                 allocating offsets; can be emptied */
    mem_heap_t *heap,            /*!< in/out: memory heap for
                                 allocating data tuples */
    bool has_index_lock,         /*!< in: true if holding index->lock
                                 in exclusive mode */
    const ddl::mrec_t *mrec,     /*!< in: merge record */
    const ddl::mrec_t *mrec_end, /*!< in: end of buffer */
    ulint *offsets)              /*!< in/out: work area for
                                 rec_deserialize_init_offsets() */

{
  enum row_op op;
  ulint extra_size;
  ulint data_size;
  dtuple_t *entry;
  trx_id_t trx_id;

  /* Online index creation is only used for secondary indexes. */
  ut_ad(!index->is_clustered());

  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X) == has_index_lock);

  if (index->is_corrupted()) {
    *error = DB_INDEX_CORRUPT;
    return (nullptr);
  }

  *error = DB_SUCCESS;

  if (mrec + ROW_LOG_HEADER_SIZE >= mrec_end) {
    return (nullptr);
  }

  switch (*mrec) {
    case ROW_OP_INSERT:
      if (ROW_LOG_HEADER_SIZE + DATA_TRX_ID_LEN + mrec >= mrec_end) {
        return (nullptr);
      }

      op = static_cast<enum row_op>(*mrec++);
      trx_id = trx_read_trx_id(mrec);
      mrec += DATA_TRX_ID_LEN;
      break;
    case ROW_OP_DELETE:
      op = static_cast<enum row_op>(*mrec++);
      trx_id = 0;
      break;
    default:
    corrupted:
      *error = DB_CORRUPTION;
      ut_d(ut_error);
      ut_o(return (nullptr));
  }

  extra_size = *mrec++;

  ut_ad(mrec < mrec_end);

  if (extra_size >= 0x80) {
    /* Read another byte of extra_size. */

    extra_size = (extra_size & 0x7f) << 8;
    extra_size |= *mrec++;
  }

  mrec += extra_size;

  if (mrec > mrec_end) {
    return (nullptr);
  }

  rec_deserialize_init_offsets(mrec, index, offsets);

  if (rec_offs_any_extern(offsets)) {
    /* There should never be any externally stored fields
    in a secondary index, which is what online index
    creation is used for. Therefore, the log file must be
    corrupted. */
    goto corrupted;
  }

  data_size = rec_offs_data_size(offsets);

  mrec += data_size;

  if (mrec > mrec_end) {
    return (nullptr);
  }

  entry = row_rec_to_index_entry_low(mrec - data_size, index, offsets, heap);
  /* Online index creation is only implemented for secondary
  indexes, which never contain off-page columns. */
  ut_ad(!entry->has_ext());

  row_log_apply_op_low(index, dup, error, offsets_heap, has_index_lock, op,
                       trx_id, entry);
  return (mrec);
}

/** Applies operations to a secondary index that was being created.
@param[in]      trx     transaction (for checking if the operation was
interrupted)
@param[in,out]  index   index
@param[in,out]  dup     for reporting duplicate key errors
@param[in,out]  stage   performance schema accounting object, used by
ALTER TABLE. If not NULL, then stage->inc() will be called for each block
of log that is applied.
@return DB_SUCCESS, or error code on failure */
static dberr_t row_log_apply_ops(const trx_t *trx, dict_index_t *index,
                                 ddl::Dup *dup, Alter_stage *stage) {
  dberr_t error;
  const ddl::mrec_t *mrec = nullptr;
  const ddl::mrec_t *next_mrec;
  const ddl::mrec_t *mrec_end = nullptr; /* silence bogus warning */
  const ddl::mrec_t *next_mrec_end;
  mem_heap_t *offsets_heap;
  mem_heap_t *heap;
  ulint *offsets;
  bool has_index_lock;
  const ulint i = 1 + REC_OFFS_HEADER_SIZE + dict_index_get_n_fields(index);

  ut_ad(dict_index_is_online_ddl(index));
  ut_ad(!index->is_committed());
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));
  ut_ad(index->online_log);
  UNIV_MEM_INVALID(&mrec_end, sizeof mrec_end);

  offsets = static_cast<ulint *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, i * sizeof *offsets));
  offsets[0] = i;
  offsets[1] = dict_index_get_n_fields(index);

  offsets_heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
  heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
  has_index_lock = true;

next_block:
  ut_ad(has_index_lock);
  ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_X));
  ut_ad(index->online_log->head.bytes == 0);

  stage->inc(row_log_progress_inc_per_block());

  if (trx_is_interrupted(trx)) {
    goto interrupted;
  }

  error = index->online_log->error;
  if (error != DB_SUCCESS) {
    goto func_exit;
  }

  if (index->is_corrupted()) {
    error = DB_INDEX_CORRUPT;
    goto func_exit;
  }

  if (UNIV_UNLIKELY(index->online_log->head.blocks >
                    index->online_log->tail.blocks)) {
    /* Note: release-only label. */
    ut_o(unexpected_eof:);
    ib::error(ER_IB_MSG_962)
        << "Unexpected end of temporary file for index " << index->name;
  corruption:
    error = DB_CORRUPTION;
    goto func_exit;
  }

  if (index->online_log->head.blocks == index->online_log->tail.blocks) {
    if (index->online_log->head.blocks) {
#ifdef HAVE_FTRUNCATE
      /* Truncate the file in order to save space. */
      if (index->online_log->file.is_open() &&
          ftruncate(index->online_log->file.get(), 0) == -1) {
        perror("ftruncate");
      }
#endif /* HAVE_FTRUNCATE */
      index->online_log->head.blocks = index->online_log->tail.blocks = 0;
    }

    next_mrec = index->online_log->tail.block;
    next_mrec_end = next_mrec + index->online_log->tail.bytes;

    if (next_mrec_end == next_mrec) {
      /* End of log reached. */
    all_done:
      ut_ad(has_index_lock);
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->tail.blocks == 0);
      error = DB_SUCCESS;
      goto func_exit;
    }
  } else {
    os_offset_t ofs;

    ofs = (os_offset_t)index->online_log->head.blocks * srv_sort_buf_size;

    ut_ad(has_index_lock);
    has_index_lock = false;
    rw_lock_x_unlock(dict_index_get_lock(index));

    log_free_check();

    if (!row_log_block_allocate(index->online_log->head)) {
      error = DB_OUT_OF_MEMORY;
      goto func_exit;
    }

    IORequest request(IORequest::READ | IORequest::ROW_LOG);
    dberr_t err = os_file_read_no_error_handling_int_fd(
        request, index->online_log->path, index->online_log->file.get(),
        index->online_log->head.block, ofs, srv_sort_buf_size, nullptr);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_963) << "Unable to read temporary file"
                                  " for index "
                               << index->name;
      goto corruption;
    }

#ifdef POSIX_FADV_DONTNEED
    /* Each block is read exactly once.  Free up the file cache. */
    posix_fadvise(index->online_log->file.get(), ofs, srv_sort_buf_size,
                  POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

    next_mrec = index->online_log->head.block;
    next_mrec_end = next_mrec + srv_sort_buf_size;
  }

  if (mrec) {
    /* A partial record was read from the previous block.
    Copy the temporary buffer full, as we do not know the
    length of the record. Parse subsequent records from
    the bigger buffer index->online_log->head.block
    or index->online_log->tail.block. */

    ut_ad(mrec == index->online_log->head.buf);
    ut_ad(mrec_end > mrec);
    ut_ad(mrec_end < (&index->online_log->head.buf)[1]);

    memcpy((ddl::mrec_t *)mrec_end, next_mrec,
           (&index->online_log->head.buf)[1] - mrec_end);
    mrec = row_log_apply_op(index, dup, &error, offsets_heap, heap,
                            has_index_lock, index->online_log->head.buf,
                            (&index->online_log->head.buf)[1], offsets);
    if (error != DB_SUCCESS) {
      goto func_exit;
    } else if (UNIV_UNLIKELY(mrec == nullptr)) {
      /* The record was not reassembled properly. */
      goto corruption;
    }
    /* The record was previously found out to be
    truncated. Now that the parse buffer was extended,
    it should proceed beyond the old end of the buffer. */
    ut_a(mrec > mrec_end);

    index->online_log->head.bytes = mrec - mrec_end;
    next_mrec += index->online_log->head.bytes;
  }

  ut_ad(next_mrec <= next_mrec_end);
  /* The following loop must not be parsing the temporary
  buffer, but head.block or tail.block. */

  /* mrec!=NULL means that the next record starts from the
  middle of the block */
  ut_ad((mrec == nullptr) == (index->online_log->head.bytes == 0));

#ifdef UNIV_DEBUG
  if (index->online_log->head.block != nullptr &&
      next_mrec_end == index->online_log->head.block + srv_sort_buf_size) {
    /* If tail.bytes == 0, next_mrec_end can also be at
    the end of tail.block. */
    if (index->online_log->tail.bytes == 0) {
      ut_ad(next_mrec == next_mrec_end);
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->head.bytes == 0);
    } else {
      ut_ad(next_mrec ==
            index->online_log->head.block + index->online_log->head.bytes);
      ut_ad(index->online_log->tail.blocks > index->online_log->head.blocks);
    }
  } else if (next_mrec_end ==
             index->online_log->tail.block + index->online_log->tail.bytes) {
    ut_ad(next_mrec ==
          index->online_log->tail.block + index->online_log->head.bytes);
    ut_ad(index->online_log->tail.blocks == 0);
    ut_ad(index->online_log->head.blocks == 0);
    ut_ad(index->online_log->head.bytes <= index->online_log->tail.bytes);
  } else {
    ut_error;
  }
#endif /* UNIV_DEBUG */

  mrec_end = next_mrec_end;

  while (!trx_is_interrupted(trx)) {
    mrec = next_mrec;
    ut_ad(mrec < mrec_end);

    if (!has_index_lock) {
      /* We are applying operations from a different
      block than the one that is being written to.
      We do not hold index->lock in order to
      allow other threads to concurrently buffer
      modifications. */
      ut_ad(mrec >= index->online_log->head.block);
      ut_ad(mrec_end == index->online_log->head.block + srv_sort_buf_size);
      ut_ad(index->online_log->head.bytes < srv_sort_buf_size);

      /* Take the opportunity to do a redo log
      checkpoint if needed. */
      log_free_check();
    } else {
      /* We are applying operations from the last block.
      Do not allow other threads to buffer anything,
      so that we can finally catch up and synchronize. */
      ut_ad(index->online_log->head.blocks == 0);
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(mrec_end ==
            index->online_log->tail.block + index->online_log->tail.bytes);
      ut_ad(mrec >= index->online_log->tail.block);
    }

    next_mrec = row_log_apply_op(index, dup, &error, offsets_heap, heap,
                                 has_index_lock, mrec, mrec_end, offsets);

    if (error != DB_SUCCESS) {
      goto func_exit;
    } else if (next_mrec == next_mrec_end) {
      /* The record happened to end on a block boundary.
      Do we have more blocks left? */
      if (has_index_lock) {
        /* The index will be locked while
        applying the last block. */
        goto all_done;
      }

      mrec = nullptr;
    process_next_block:
      rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
      has_index_lock = true;

      index->online_log->head.bytes = 0;
      index->online_log->head.blocks++;
      goto next_block;
    } else if (next_mrec != nullptr) {
      ut_ad(next_mrec < next_mrec_end);
      index->online_log->head.bytes += next_mrec - mrec;
    } else if (has_index_lock) {
      /* When mrec is within tail.block, it should
      be a complete record, because we are holding
      index->lock and thus excluding the writer. */
      ut_ad(index->online_log->tail.blocks == 0);
      ut_ad(mrec_end ==
            index->online_log->tail.block + index->online_log->tail.bytes);
      ut_d(ut_error);
      ut_o(goto unexpected_eof);
    } else {
      memcpy(index->online_log->head.buf, mrec, mrec_end - mrec);
      mrec_end += index->online_log->head.buf - mrec;
      mrec = index->online_log->head.buf;
      goto process_next_block;
    }
  }

interrupted:
  error = DB_INTERRUPTED;
func_exit:
  if (!has_index_lock) {
    rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
  }

  switch (error) {
    case DB_SUCCESS:
      break;
    case DB_INDEX_CORRUPT:
      if (((os_offset_t)index->online_log->tail.blocks + 1) *
              srv_sort_buf_size >=
          srv_online_max_size) {
        /* The log file grew too big. */
        error = DB_ONLINE_LOG_TOO_BIG;
      }
      [[fallthrough]];
    default:
      /* We set the flag directly instead of
      invoking dict_set_corrupted() here,
      because the index is not "public" yet. */
      index->type |= DICT_CORRUPT;
  }

  mem_heap_free(heap);
  mem_heap_free(offsets_heap);
  row_log_block_free(index->online_log->head);
  ut::free(offsets);
  return (error);
}

/** Apply the row log to the index upon completing index creation.
@param[in]      trx     transaction (for checking if the operation was
interrupted)
@param[in,out]  index   secondary index
@param[in,out]  table   MySQL table (for reporting duplicates)
@param[in,out]  stage   performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_log_index() will be called initially and then
stage->inc() will be called for each block of log that is applied.
@return DB_SUCCESS, or error code on failure */
dberr_t row_log_apply(const trx_t *trx, dict_index_t *index,
                      struct TABLE *table, Alter_stage *stage) {
  dberr_t error;
  row_log_t *log;
  ddl::Dup dup = {index, table, nullptr, 0};
  DBUG_TRACE;

  ut_ad(dict_index_is_online_ddl(index));
  ut_ad(!index->is_clustered());

  stage->begin_phase_log_index();

  log_free_check();

  rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

  if (!index->table->is_corrupted()) {
    error = row_log_apply_ops(trx, index, &dup, stage);
  } else {
    error = DB_SUCCESS;
  }

  if (error != DB_SUCCESS) {
    ut_a(!dict_table_is_discarded(index->table));
    /* We set the flag directly instead of
    invoking dict_set_corrupted() here,
    because the index is not "public" yet. */
    index->type |= DICT_CORRUPT;
    index->table->drop_aborted = true;

    dict_index_set_online_status(index, ONLINE_INDEX_ABORTED);
  } else {
    ut_ad(dup.m_n_dup == 0);
    dict_index_set_online_status(index, ONLINE_INDEX_COMPLETE);
  }

  log = index->online_log;
  index->online_log = nullptr;
  rw_lock_x_unlock(dict_index_get_lock(index));

  row_log_free(log);

  return error;
}
