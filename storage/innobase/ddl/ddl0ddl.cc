/*****************************************************************************

Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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

/** @file ddl/ddl0ddl.cc
 DDL implementation misc functions.
Created 2020-11-01 by Sunny Bains. */

#include "btr0load.h"
#include "ddl0fts.h"
#include "ddl0impl-cursor.h"
#include "ddl0impl-merge.h"
#include "dict0dd.h"
#include "handler0alter.h"
#include "lock0lock.h"
#include "row0log.h"

/* Ignore posix_fadvise() on those platforms where it does not exist */
#if defined _WIN32
#define posix_fadvise(fd, offset, len, advice) /* nothing */
#endif                                         /* _WIN32 */

/** Whether to disable file system cache */
bool srv_disable_sort_file_cache;

namespace ddl {

/** Note that an index build has failed.
@param[in,out] index            Index that failed to build. */
static void index_build_failed(dict_index_t *index) noexcept;

doc_id_t Fetch_sequence::fetch(const dtuple_t *dtuple) noexcept {
  const auto doc_field =
      dtuple_get_nth_field(dtuple, m_index->table->fts->doc_col);

  const auto doc_id = static_cast<doc_id_t>(
      mach_read_from_8(static_cast<byte *>(dfield_get_data(doc_field))));

  if (doc_id > m_max_doc_id) {
    m_max_doc_id = doc_id;
  }

  return doc_id;
}

void Dup::report(const mrec_t *mrec, const ulint *offsets) noexcept {
  ++m_n_dup;

  if (m_n_dup == 1) {
    auto heap = mem_heap_create(1024, UT_LOCATION_HERE);
    auto dtuple = row_rec_to_index_entry_low(mrec, m_index, offsets, heap);

    /* Report the first duplicate record, but count all duplicate records. */
    innobase_fields_to_mysql(m_table, m_index, dtuple->fields);

    mem_heap_free(heap);
  }
}

void Dup::report(const dfield_t *dfield) noexcept {
  ++m_n_dup;

  if (m_n_dup == 1) {
    /* Report the first duplicate record, but count all duplicate records. */
    innobase_fields_to_mysql(m_table, m_index, dfield);
  }
}

dberr_t pread(os_fd_t fd, void *ptr, size_t len, os_offset_t offset) noexcept {
  IF_ENABLED("ddl_read_failure", return DB_IO_ERROR;)

  IORequest request;

  /* Merge sort pages are never compressed. */
  request.disable_compression();

  auto err = os_file_read_no_error_handling_int_fd(request, "(ddl)", fd, ptr,
                                                   offset, len, nullptr);

#ifdef POSIX_FADV_DONTNEED
  /* Each block is read exactly once.  Free up the file cache. */
  posix_fadvise(fd, offset, len, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

  return err;
}

dberr_t pwrite(os_fd_t fd, void *ptr, size_t len, os_offset_t offset) noexcept {
  IF_ENABLED("ddl_write_failure", return DB_IO_ERROR;)

  IORequest request(IORequest::WRITE);

  request.disable_compression();

  auto err = os_file_write_int_fd(request, "(ddl)", fd, ptr, offset, len);

#ifdef POSIX_FADV_DONTNEED
  /* The block will be needed on the next merge pass,
  but it can be evicted from the file cache meanwhile. */
  posix_fadvise(fd, offset, len, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

  return err;
}

Unique_os_file_descriptor file_create_low(const char *path) noexcept {
  if (path == nullptr) {
    path = innobase_mysql_tmpdir();
  }
#ifdef UNIV_PFS_IO
  /* This temp file open does not go through normal file APIs, add
 instrumentation to register with performance schema */
  Datafile df;

  df.make_filepath(path, "Innodb Merge Temp File", NO_EXT);

  PSI_file_locker *locker{};
  PSI_file_locker_state state;

  locker = PSI_FILE_CALL(get_thread_file_name_locker)(
      &state, innodb_temp_file_key.m_value, PSI_FILE_OPEN, df.filepath(),
      &locker);

  if (locker != nullptr) {
    PSI_FILE_CALL(start_file_open_wait)(locker, __FILE__, __LINE__);
  }

#endif /* UNIV_PFS_IO */

  auto fd = innobase_mysql_tmpfile(path);

#ifdef UNIV_PFS_IO
  if (locker != nullptr) {
    PSI_FILE_CALL(end_file_open_wait_and_bind_to_descriptor)(locker, fd);
  }
#endif /* UNIV_PFS_IO */

  if (fd < 0) {
    ib::error(ER_IB_MSG_967) << "Cannot create temporary merge file";
    return Unique_os_file_descriptor{};
  }

  return Unique_os_file_descriptor{fd};
}

bool file_create(ddl::file_t *file, const char *path) noexcept {
  file->m_size = 0;
  file->m_n_recs = 0;
  file->m_file = ddl::file_create_low(path);

  if (file->m_file.is_open()) {
    if (srv_disable_sort_file_cache) {
      os_file_set_nocache(file->m_file.get(), "ddl0ddl.cc", "sort");
    }
    return true;
  }

  return false;
}

dict_index_t *create_index(trx_t *trx, dict_table_t *table,
                           const Index_defn *index_def,
                           const dict_add_v_col_t *add_v) noexcept {
  const size_t n_fields = index_def->m_n_fields;

  ut_ad(!srv_read_only_mode);

  /* Create the index prototype, using the passed in def, this is not
  a persistent operation. We pass 0 as the space id, and determine at
  a lower level the space id where to row the table. */

  auto index = dict_mem_index_create(table->name.m_name, index_def->m_name, 0,
                                     index_def->m_ind_type, n_fields);

  ut_a(index);

  index->set_committed(index_def->m_rebuild);

  bool has_new_v_col{};

  for (size_t i = 0; i < n_fields; i++) {
    const char *name;
    auto ifield = &index_def->m_fields[i];

    if (ifield->m_is_v_col) {
      if (ifield->m_col_no >= table->n_v_def) {
        ut_ad(ifield->m_col_no < table->n_v_def + add_v->n_v_col);
        ut_ad(ifield->m_col_no >= table->n_v_def);
        name = add_v->v_col_name[ifield->m_col_no - table->n_v_def];

        has_new_v_col = true;
      } else {
        name = dict_table_get_v_col_name(table, ifield->m_col_no);
      }
    } else {
      name = table->get_col_name(ifield->m_col_no);
    }

    index->add_field(name, ifield->m_prefix_len, ifield->m_is_ascending);
  }

  /* Create B-tree */
  dict_sys_mutex_exit();

  dict_build_index_def(table, index, trx);

  auto err = dict_index_add_to_cache_w_vcol(table, index, add_v, index->page,
                                            trx_is_strict(trx));

  if (err != DB_SUCCESS) {
    trx->error_state = err;
    dict_sys_mutex_enter();
    return nullptr;
  }

  index = dict_table_get_index_on_name(table, index_def->m_name,
                                       index_def->m_rebuild);
  ut_ad(index != nullptr);

  err = dict_create_index_tree_in_mem(index, trx);

  dict_sys_mutex_enter();

  if (err != DB_SUCCESS) {
    if ((index->type & DICT_FTS) && table->fts) {
      fts_cache_index_cache_remove(table, index);
    }

    trx->error_state = err;
    return nullptr;
  }

  if (dict_index_is_spatial(index)) {
    index->fill_srid_value(index_def->m_srid, index_def->m_srid_is_valid);
  }

  /* Adjust field name for newly added virtual columns. */
  for (size_t i = 0; i < n_fields; i++) {
    auto ifield = &index_def->m_fields[i];

    if (ifield->m_is_v_col && ifield->m_col_no >= table->n_v_def) {
      ut_ad(ifield->m_col_no < table->n_v_def + add_v->n_v_col);
      ut_ad(ifield->m_col_no >= table->n_v_def);
      dict_field_t *field = index->get_field(i);
      field->name = add_v->v_col_name[ifield->m_col_no - table->n_v_def];
    }
  }

  if (dict_index_is_spatial(index)) {
    index->fill_srid_value(index_def->m_srid, index_def->m_srid_is_valid);
    index->rtr_srs.reset(fetch_srs(index->srid));
  }

  index->parser = index_def->m_parser;
  index->is_ngram = index_def->m_is_ngram;
  index->has_new_v_col = has_new_v_col;

  /* Note the id of the transaction that created this
  index, we use it to restrict readers from accessing
  this index, to ensure read consistency. */
  ut_ad(index->trx_id == trx->id);

  index->table->def_trx_id = trx->id;

  return index;
}

dberr_t drop_table(trx_t *trx, dict_table_t *table) noexcept {
  ut_ad(!srv_read_only_mode);

  /* There must be no open transactions on the table. */
  ut_a(table->get_ref_count() == 0);

  return row_drop_table_for_mysql(table->name.m_name, trx, false, nullptr);
}

dberr_t lock_table(trx_t *trx, dict_table_t *table,
                   enum lock_mode mode) noexcept {
  ut_ad(!srv_read_only_mode);
  ut_ad(mode == LOCK_X || mode == LOCK_S);

  trx->op_info = "setting table lock for creating or dropping index";
  /* Trx for DDL should not be forced to rollback for now */
  trx->in_innodb |= TRX_FORCE_ROLLBACK_DISABLE;

  return lock_table_for_trx(table, trx, mode);
}

static void index_build_failed(dict_index_t *index) noexcept {
  DEBUG_SYNC_C("merge_drop_index_after_abort");

  rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

  dict_index_set_online_status(index, ONLINE_INDEX_ABORTED_DROPPED);

  rw_lock_x_unlock(dict_index_get_lock(index));

  index->table->drop_aborted = true;

  if (dict_index_has_virtual(index)) {
    /* If multi-value index, a virtual column would be created too.
    This is same for a virtual column along with an index on it case.
    In this rollback case, the metadata for this index would be
    inconsistent with metadata of table, because table metadata
    doesn't have the virtual column for this index yet. So set
    this index to be corrupted and any further use of this index
    would be prevented. */
    index->type |= DICT_CORRUPT;
  }
}

/** We will have to drop the secondary indexes later, when the table is
in use, unless the the DDL has already been externalized. Mark the indexes
as incomplete and corrupted, so that other threads will stop using them.
Let dict_table_close() or crash recovery or the next invocation of
prepare_inplace_alter_table() take care of dropping the indexes.
@param[in,out] trx              Transaction
@param[in] table                Table that owns the indexes. */
static void mark_secondary_indexes(trx_t *trx, dict_table_t *table) noexcept {
  auto index = table->first_index();

  while ((index = index->next()) != nullptr) {
    ut_ad(!index->is_clustered());

    switch (dict_index_get_online_status(index)) {
      case ONLINE_INDEX_ABORTED_DROPPED:
        continue;

      case ONLINE_INDEX_COMPLETE:
        if (index->is_committed()) {
          /* Do nothing to already published indexes. */
        } else if (index->type & DICT_FTS) {
          /* Drop a completed FULLTEXT index, due to a timeout during
          MDL upgrade for commit_inplace_alter_table(). Because only
          concurrent reads are allowed (and they are not seeing this
          index yet) we are safe to drop the index. */
          auto prev = UT_LIST_GET_PREV(indexes, index);

          /* At least there should be the clustered index before this one. */
          ut_ad(prev != nullptr);
          ut_a(table->fts);

          fts_drop_index(table, index, trx, nullptr);

          /* Since INNOBASE_SHARE::idx_trans_tbl is shared between all open
          ha_innobase handles to this table, no thread should be accessing
          this dict_index_t object. Also, we should be holding LOCK=SHARED
          MDL on the table even after the MDL upgrade timeout. */

          /* We can remove a DICT_FTS index from the cache, because we do
          not allow ADD FULLTEXT INDEX with LOCK=NONE. If we allowed that,
          we should exclude FTS entries from prebuilt->ins_node->entry_list
          in ins_node_create_entry_list(). */
          dict_index_remove_from_cache(table, index);

          index = prev;

        } else {
          rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

          dict_index_set_online_status(index, ONLINE_INDEX_ABORTED);

          index->type |= DICT_CORRUPT;

          table->drop_aborted = true;

          rw_lock_x_unlock(dict_index_get_lock(index));

          ut_a(table == index->table);

          ddl::index_build_failed(index);
        }
        break;

      case ONLINE_INDEX_CREATION:
        rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
        ut_ad(!index->is_committed());
        row_log_abort_sec(index);
        rw_lock_x_unlock(dict_index_get_lock(index));

        [[fallthrough]];

      case ONLINE_INDEX_ABORTED:
        ut_a(table == index->table);
        ddl::index_build_failed(index);
        break;

      default:
        ut_error;
    }
  }
}

/** Invalidate all row_prebuilt_t::ins_graph that are referring
to this table. That is, force row_get_prebuilt_insert_row() to
rebuild prebuilt->ins_node->entry_list).
@param[in,out] trx              Transaction
@param[in] table                Table that owns the indexes. */
static void drop_secondary_indexes(trx_t *trx, dict_table_t *table) noexcept {
  ut_ad(table->def_trx_id <= trx->id);

  table->def_trx_id = trx->id;

  auto index = table->first_index();
  auto next_index = index->next();

  while ((index = next_index) != nullptr) {
    /* read the next pointer before freeing the index */
    next_index = index->next();

    ut_ad(!index->is_clustered());

    if (!index->is_committed()) {
      /* For FTS index, drop from table->fts along with its aux tables */
      if (index->type & DICT_FTS) {
        ut_a(table->fts);
        fts_drop_index(table, index, trx, nullptr);
      }

      switch (dict_index_get_online_status(index)) {
        case ONLINE_INDEX_CREATION:
          /* This state should only be possible when
          prepare_inplace_alter_table() fails after invoking
          ddl::create_index(). In inplace_alter_table(),
          ddl::Context::cleanup() should never leave the index in
          this state. It would invoke row_log_abort_sec() on failure. */
        case ONLINE_INDEX_COMPLETE:
          /* In these cases, we are able to drop the index
          straight. The DROP INDEX was never deferred. */
          break;
        case ONLINE_INDEX_ABORTED:
        case ONLINE_INDEX_ABORTED_DROPPED:
          break;
      }

      dict_index_remove_from_cache(table, index);
    }
  }

  table->drop_aborted = false;

  ut_d(dict_table_check_for_dup_indexes(table, CHECK_ALL_COMPLETE));
}

void drop_indexes(trx_t *trx, dict_table_t *table, bool locked) noexcept {
  ut_ad(!srv_read_only_mode);
  ut_ad(dict_sys_mutex_own());
  ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

  ut_d(auto index = table->first_index());
  ut_ad(index->is_clustered());
  ut_ad(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);

  /* the caller should have an open handle to the table */
  ut_ad(table->get_ref_count() >= 1);

  /* It is possible that table->n_ref_count > 1 when
  locked=true. In this case, all code that should have an open
  handle to the table be waiting for the next statement to execute,
  or waiting for a meta-data lock.

  A concurrent purge will be prevented by dict_operation_lock. */

  if (!locked && table->get_ref_count() > 1) {
    /* Mark the secondary indexes as aborted/offline. */
    mark_secondary_indexes(trx, table);
  } else {
    drop_secondary_indexes(trx, table);
  }
}

dberr_t Row::build(ddl::Context &ctx, dict_index_t *index, mem_heap_t *heap,
                   size_t type) noexcept {
  ut_ad(rec_offs_any_null_extern(index, m_rec, m_offsets) == nullptr);

  /* Build a row based on the clustered index. */

  m_ptr = row_build_w_add_vcol(type, index, m_rec, m_offsets, ctx.m_new_table,
                               m_add_cols, ctx.m_add_v, ctx.m_col_map, &m_ext,
                               heap);

  if (!ctx.check_null_constraints(m_ptr)) {
    ctx.m_trx->error_key_num = SERVER_CLUSTER_INDEX_ID;
    return DB_INVALID_NULL;
  }

  auto &fts = ctx.m_fts;

  if (fts.m_doc_id != nullptr && fts.m_doc_id->is_generated()) {
    fts.m_doc_id->increment();
  }

  if (ctx.m_add_autoinc != ULINT_UNDEFINED) {
    auto err = ctx.handle_autoinc(m_ptr);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

dberr_t Cursor::finish(dberr_t err) noexcept {
  if (m_ctx.m_fts.m_ptr != nullptr) {
    /* Wait for the FTS parser threads to complete and prepare to insert. */
    return m_ctx.m_fts.m_ptr->scan_finished(err);
  } else {
    return err;
  }
}

}  // namespace ddl
