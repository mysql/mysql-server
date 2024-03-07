/*****************************************************************************

Copyright (c) 2017, 2024, Oracle and/or its affiliates.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file log/log0ddl.cc
 DDL log

 Created 12/1/2016 Shaohua Wang
 *******************************************************/

#include <debug_sync.h>
#include "ha_prototypes.h"

#include <current_thd.h>
#include <sql_thd_internal_api.h>

#include <scope_guard.h>
#include "btr0sea.h"
#include "dict0dd.h"
#include "dict0mem.h"
#include "dict0stats.h"
#include "ha_innodb.h"
#include "log0chkp.h"
#include "log0ddl.h"
#include "my_sys.h"
#include "mysql/plugin.h"
#include "mysqld.h"  //get_server_state
#include "pars0pars.h"
#include "que0que.h"
#include "row0ins.h"
#include "row0row.h"
#include "row0sel.h"
#include "trx0trx.h"

/** Object to handle Log_DDL */
Log_DDL *log_ddl = nullptr;

/** Whether replaying DDL log
Note: we should not write DDL log when replaying DDL log. */
thread_local bool thread_local_ddl_log_replay = false;

/** Whether in recover(replay) DDL log in startup. */
bool Log_DDL::s_in_recovery = false;

#ifdef UNIV_DEBUG

/** Used by SET GLOBAL innodb_ddl_log_crash_counter_reset_debug = 1; */
bool innodb_ddl_log_crash_reset_debug;

/** Below counters are only used for four types of DDL log:
1. FREE TREE
2. DELETE SPACE
3. RENAME SPACE
4. DROP
Other RENAME_TABLE and REMOVE CACHE doesn't touch the data files at all,
so would be skipped */

/** Crash injection counter used before writing FREE TREE log */
static uint32_t crash_before_free_tree_log_counter = 1;

/** Crash injection counter used after writing FREE TREE log */
static uint32_t crash_after_free_tree_log_counter = 1;

/** Crash injection counter used after deleting FREE TREE log */
static uint32_t crash_after_free_tree_delete_counter = 1;

/** Crash injection counter used before writing DELETE SPACE log */
static uint32_t crash_before_delete_space_log_counter = 1;

/** Crash injection counter used after writing DELETE SPACE log */
static uint32_t crash_after_delete_space_log_counter = 1;

/** Crash injection counter used after deleting DELETE SPACE log */
static uint32_t crash_after_delete_space_delete_counter = 1;

/** Crash injection counter used before writing RENAME SPACE log */
static uint32_t crash_before_rename_space_log_counter = 1;

/** Crash injection counter used after writing RENAME SPACE log */
static uint32_t crash_after_rename_space_log_counter = 1;

/** Crash injection counter used after deleting RENAME SPACE log */
static uint32_t crash_after_rename_space_delete_counter = 1;

/** Crash injection counter used before writing DROP log */
static uint32_t crash_before_drop_log_counter = 1;

/** Crash injection counter used after writing DROP log */
static uint32_t crash_after_drop_log_counter = 1;

/** Crash injection counter used after any replay */
static uint32_t crash_after_replay_counter = 1;

/** Crash injection counter used before writing ALTER ENCRYPT TABLESPACE log */
static uint32_t crash_before_alter_encrypt_space_log_counter = 1;

/** Crash injection counter used after writing ALTER ENCRYPT TABLESPACE log */
static uint32_t crash_after_alter_encrypt_space_log_counter = 1;

/** Crash injection counter used during post ddl in each step. */
static uint32_t crash_post_ddl_apply_step_counter = 1;

void ddl_log_crash_reset(THD *, SYS_VAR *, void *, const void *save) {
  const bool reset = *static_cast<const bool *>(save);

  innodb_ddl_log_crash_reset_debug = reset;

  if (reset) {
    crash_before_free_tree_log_counter = 1;
    crash_after_free_tree_log_counter = 1;
    crash_after_free_tree_delete_counter = 1;
    crash_before_delete_space_log_counter = 1;
    crash_after_delete_space_log_counter = 1;
    crash_after_delete_space_delete_counter = 1;
    crash_before_rename_space_log_counter = 1;
    crash_after_rename_space_log_counter = 1;
    crash_after_rename_space_delete_counter = 1;
    crash_before_drop_log_counter = 1;
    crash_after_drop_log_counter = 1;
    crash_after_replay_counter = 1;
    crash_post_ddl_apply_step_counter = 1;
  }
}

#endif /* UNIV_DEBUG */

DDL_Record::DDL_Record()
    : m_id(ULINT_UNDEFINED),
      m_thread_id(ULINT_UNDEFINED),
      m_space_id(SPACE_UNKNOWN),
      m_page_no(FIL_NULL),
      m_index_id(ULINT_UNDEFINED),
      m_table_id(ULINT_UNDEFINED),
      m_old_file_path(nullptr),
      m_new_file_path(nullptr),
      m_heap(nullptr),
      m_deletable(true) {}

DDL_Record::~DDL_Record() {
  if (m_heap != nullptr) {
    mem_heap_free(m_heap);
  }
}

void DDL_Record::set_old_file_path(const char *name) {
  ulint len = strlen(name);

  if (m_heap == nullptr) {
    m_heap = mem_heap_create(FN_REFLEN + 1, UT_LOCATION_HERE);
  }

  m_old_file_path = mem_heap_strdupl(m_heap, name, len);
}

void DDL_Record::set_old_file_path(const byte *data, ulint len) {
  if (m_heap == nullptr) {
    m_heap = mem_heap_create(FN_REFLEN + 1, UT_LOCATION_HERE);
  }

  m_old_file_path = static_cast<char *>(mem_heap_dup(m_heap, data, len + 1));
  m_old_file_path[len] = '\0';
}

void DDL_Record::set_new_file_path(const char *name) {
  ulint len = strlen(name);

  if (m_heap == nullptr) {
    m_heap = mem_heap_create(FN_REFLEN + 1, UT_LOCATION_HERE);
  }

  m_new_file_path = mem_heap_strdupl(m_heap, name, len);
}

void DDL_Record::set_new_file_path(const byte *data, ulint len) {
  if (m_heap == nullptr) {
    m_heap = mem_heap_create(FN_REFLEN + 1, UT_LOCATION_HERE);
  }

  m_new_file_path = static_cast<char *>(mem_heap_dup(m_heap, data, len + 1));
  m_new_file_path[len] = '\0';
}

std::ostream &DDL_Record::print(std::ostream &out) const {
  ut_ad(m_type >= Log_Type::SMALLEST_LOG);
  ut_ad(m_type <= Log_Type::BIGGEST_LOG);

  bool printed = false;

  out << "[DDL record: ";

  switch (m_type) {
    case Log_Type::FREE_TREE_LOG:
      out << "FREE";
      break;

    case Log_Type::DELETE_SCHEMA_DIRECTORY_LOG:
      out << "DELETE SCHEMA DIRECTORY";
      break;

    case Log_Type::DELETE_SPACE_LOG:
      out << "DELETE SPACE";
      break;
    case Log_Type::RENAME_SPACE_LOG:
      out << "RENAME SPACE";
      break;
    case Log_Type::DROP_LOG:
      out << "DROP";
      break;
    case Log_Type::RENAME_TABLE_LOG:
      out << "RENAME TABLE";
      break;
    case Log_Type::REMOVE_CACHE_LOG:
      out << "REMOVE CACHE";
      break;
    case Log_Type::ALTER_ENCRYPT_TABLESPACE_LOG:
      out << "ALTER ENCRYPT TABLESPACE";
      break;
    case Log_Type::ALTER_UNENCRYPT_TABLESPACE_LOG:
      out << "ALTER UNENCRYPT TABLESPACE";
      break;
    default:
      ut_d(ut_error);
  }

  out << ",";

  if (m_id != ULINT_UNDEFINED) {
    out << " id=" << m_id;
    printed = true;
  }

  if (m_thread_id != ULINT_UNDEFINED) {
    if (printed) {
      out << ",";
    }
    out << " thread_id=" << m_thread_id;
    printed = true;
  }

  if (m_space_id != SPACE_UNKNOWN) {
    if (printed) {
      out << ",";
    }
    out << " space_id=" << m_space_id;
    printed = true;
  }

  if (m_table_id != ULINT_UNDEFINED) {
    if (printed) {
      out << ",";
    }
    out << " table_id=" << m_table_id;
    printed = true;
  }

  if (m_index_id != ULINT_UNDEFINED) {
    if (printed) {
      out << ",";
    }
    out << " index_id=" << m_index_id;
    printed = true;
  }

  if (m_page_no != FIL_NULL) {
    if (printed) {
      out << ",";
    }
    out << " page_no=" << m_page_no;
    printed = true;
  }

  if (m_old_file_path != nullptr) {
    if (printed) {
      out << ",";
    }
    out << " old_file_path=" << m_old_file_path;
    printed = true;
  }

  if (m_new_file_path != nullptr) {
    if (printed) {
      out << ",";
    }
    out << " new_file_path=" << m_new_file_path;
  }

  out << "]";

  return (out);
}

/** Display a DDL record
@param[in,out]  o       output stream
@param[in]      record  DDL record to display
@return the output stream */
std::ostream &operator<<(std::ostream &o, const DDL_Record &record) {
  return (record.print(o));
}

DDL_Log_Table::DDL_Log_Table() : DDL_Log_Table(nullptr) {}

DDL_Log_Table::DDL_Log_Table(trx_t *trx)
    : m_table(dict_sys->ddl_log), m_tuple(nullptr), m_trx(trx), m_thr(nullptr) {
  ut_ad(m_trx == nullptr || m_trx->ddl_operation);
  m_heap = mem_heap_create(100, UT_LOCATION_HERE);
  if (m_trx != nullptr) {
    start_query_thread();
  }
}

DDL_Log_Table::~DDL_Log_Table() {
  stop_query_thread();
  mem_heap_free(m_heap);
}

void DDL_Log_Table::start_query_thread() {
  que_t *graph = static_cast<que_fork_t *>(que_node_get_parent(
      pars_complete_graph_for_exec(nullptr, m_trx, m_heap, nullptr)));
  m_thr = que_fork_start_command(graph);
  ut_ad(m_trx->lock.n_active_thrs == 1);
}

void DDL_Log_Table::stop_query_thread() {
  if (m_thr != nullptr) {
    que_thr_stop_for_mysql_no_error(m_thr, m_trx);
  }
}

void DDL_Log_Table::create_tuple(const DDL_Record &record) {
  const dict_col_t *col;
  dfield_t *dfield;
  byte *buf;

  m_tuple = dtuple_create(m_heap, m_table->get_n_cols());
  dict_table_copy_types(m_tuple, m_table);
  buf = static_cast<byte *>(mem_heap_alloc(m_heap, 8));
  memset(buf, 0xFF, 8);

  col = m_table->get_sys_col(DATA_ROW_ID);
  dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, buf, DATA_ROW_ID_LEN);

  col = m_table->get_sys_col(DATA_ROLL_PTR);
  dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, buf, DATA_ROLL_PTR_LEN);

  buf = static_cast<byte *>(mem_heap_alloc(m_heap, DATA_TRX_ID_LEN));
  mach_write_to_6(buf, m_trx->id);
  col = m_table->get_sys_col(DATA_TRX_ID);
  dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, buf, DATA_TRX_ID_LEN);

  const ulint rec_id = record.get_id();

  if (rec_id != ULINT_UNDEFINED) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_id_col_len));
    mach_write_to_8(buf, rec_id);
    dfield = dtuple_get_nth_field(m_tuple, s_id_col_no);
    dfield_set_data(dfield, buf, s_id_col_len);
  }

  if (record.get_thread_id() != ULINT_UNDEFINED) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_thread_id_col_len));
    mach_write_to_8(buf, record.get_thread_id());
    dfield = dtuple_get_nth_field(m_tuple, s_thread_id_col_no);
    dfield_set_data(dfield, buf, s_thread_id_col_len);
  }

  ut_ad(record.get_type() >= Log_Type::SMALLEST_LOG);
  ut_ad(record.get_type() <= Log_Type::BIGGEST_LOG);
  buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_type_col_len));
  mach_write_to_4(buf,
                  static_cast<typename std::underlying_type<Log_Type>::type>(
                      record.get_type()));
  dfield = dtuple_get_nth_field(m_tuple, s_type_col_no);
  dfield_set_data(dfield, buf, s_type_col_len);

  if (record.get_space_id() != SPACE_UNKNOWN) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_space_id_col_len));
    mach_write_to_4(buf, record.get_space_id());
    dfield = dtuple_get_nth_field(m_tuple, s_space_id_col_no);
    dfield_set_data(dfield, buf, s_space_id_col_len);
  }

  if (record.get_page_no() != FIL_NULL) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_page_no_col_len));
    mach_write_to_4(buf, record.get_page_no());
    dfield = dtuple_get_nth_field(m_tuple, s_page_no_col_no);
    dfield_set_data(dfield, buf, s_page_no_col_len);
  }

  if (record.get_index_id() != ULINT_UNDEFINED) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_index_id_col_len));
    mach_write_to_8(buf, record.get_index_id());
    dfield = dtuple_get_nth_field(m_tuple, s_index_id_col_no);
    dfield_set_data(dfield, buf, s_index_id_col_len);
  }

  if (record.get_table_id() != ULINT_UNDEFINED) {
    buf = static_cast<byte *>(mem_heap_alloc(m_heap, s_table_id_col_len));
    mach_write_to_8(buf, record.get_table_id());
    dfield = dtuple_get_nth_field(m_tuple, s_table_id_col_no);
    dfield_set_data(dfield, buf, s_table_id_col_len);
  }

  if (record.get_old_file_path() != nullptr) {
    ulint m_len = strlen(record.get_old_file_path()) + 1;
    dfield = dtuple_get_nth_field(m_tuple, s_old_file_path_col_no);
    dfield_set_data(dfield, record.get_old_file_path(), m_len);
  }

  if (record.get_new_file_path() != nullptr) {
    ulint m_len = strlen(record.get_new_file_path()) + 1;
    dfield = dtuple_get_nth_field(m_tuple, s_new_file_path_col_no);
    dfield_set_data(dfield, record.get_new_file_path(), m_len);
  }
}

void DDL_Log_Table::create_tuple(ulint id, const dict_index_t *index) {
  ut_ad(id != ULINT_UNDEFINED);

  dfield_t *dfield;
  ulint len;
  ulint table_col_offset;
  ulint index_col_offset;

  m_tuple = dtuple_create(m_heap, 1);
  dict_index_copy_types(m_tuple, index, 1);

  if (index->is_clustered()) {
    len = s_id_col_len;
    table_col_offset = s_id_col_no;
  } else {
    len = s_thread_id_col_len;
    table_col_offset = s_thread_id_col_no;
  }

  index_col_offset = index->get_col_pos(table_col_offset);
  byte *buf = static_cast<byte *>(mem_heap_alloc(m_heap, len));
  mach_write_to_8(buf, id);
  dfield = dtuple_get_nth_field(m_tuple, index_col_offset);
  dfield_set_data(dfield, buf, len);
}

dberr_t DDL_Log_Table::insert(const DDL_Record &record) {
  ut_ad(record.validate());
  dberr_t error;
  dict_index_t *index = m_table->first_index();
  dtuple_t *entry;
  uint32_t flags = BTR_NO_LOCKING_FLAG;
  mem_heap_t *offsets_heap = mem_heap_create(100, UT_LOCATION_HERE);
  static std::atomic<uint64_t> count(0);

  if (count++ % 64 == 0) {
    log_free_check();
  }

  create_tuple(record);
  entry = row_build_index_entry(m_tuple, nullptr, index, m_heap);

#ifdef UNIV_DEBUG
  bool insert = true;
  DBUG_EXECUTE_IF("ddl_log_return_error_from_insert", insert = false;);

  if (insert) {
#endif
    error = row_ins_clust_index_entry_low(flags, BTR_MODIFY_LEAF, index,
                                          index->n_uniq, entry, m_thr, false);
#ifdef UNIV_DEBUG
  } else {
    error = DB_ERROR;
  }
#endif

  if (error == DB_FAIL) {
    error = row_ins_clust_index_entry_low(flags, BTR_MODIFY_TREE, index,
                                          index->n_uniq, entry, m_thr, false);
    ut_ad(error == DB_SUCCESS);
  }

  if (error != DB_SUCCESS) {
    ib::error(ER_IB_ERR_DDL_LOG_INSERT_FAILURE);
    mem_heap_free(offsets_heap);
    return error;
  }

  index = index->next();

  entry = row_build_index_entry(m_tuple, nullptr, index, m_heap);

  error =
      row_ins_sec_index_entry_low(flags, BTR_MODIFY_LEAF, index, offsets_heap,
                                  m_heap, entry, m_trx->id, m_thr, false);

  if (error == DB_FAIL) {
    error =
        row_ins_sec_index_entry_low(flags, BTR_MODIFY_TREE, index, offsets_heap,
                                    m_heap, entry, m_trx->id, m_thr, false);
  }

  mem_heap_free(offsets_heap);
  ut_ad(error == DB_SUCCESS);
  return (error);
}

void DDL_Log_Table::convert_to_ddl_record(bool is_clustered, rec_t *rec,
                                          const ulint *offsets,
                                          DDL_Record &record) {
  if (is_clustered) {
    for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
      const byte *data;
      ulint len;

      if (i == DATA_ROLL_PTR || i == DATA_TRX_ID) {
        continue;
      }

      const dict_index_t *clust_index = m_table->first_index();
      data = rec_get_nth_field(clust_index, rec, offsets, i, &len);

      if (len != UNIV_SQL_NULL) {
        set_field(data, i, len, record);
      }
    }
  } else {
    /* For secondary index, only the ID would be stored */
    record.set_id(parse_id(m_table->first_index()->next(), rec, offsets));
  }
}

ulint DDL_Log_Table::parse_id(const dict_index_t *index, rec_t *rec,
                              const ulint *offsets) {
  ulint len;
  ulint index_offset = index->get_col_pos(s_id_col_no);

  const byte *data = rec_get_nth_field(index, rec, offsets, index_offset, &len);
  ut_ad(len == s_id_col_len);

  return (mach_read_from_8(data));
}

void DDL_Log_Table::set_field(const byte *data, ulint index_offset, ulint len,
                              DDL_Record &record) {
  dict_index_t *index = dict_sys->ddl_log->first_index();
  ulint col_offset = index->get_col_no(index_offset);

  if (col_offset == s_new_file_path_col_no) {
    record.set_new_file_path(data, len);
    return;
  }

  if (col_offset == s_old_file_path_col_no) {
    record.set_old_file_path(data, len);
    return;
  }

  ulint value = fetch_value(data, col_offset);
  switch (col_offset) {
    case s_id_col_no:
      record.set_id(value);
      break;
    case s_thread_id_col_no:
      record.set_thread_id(value);
      break;
    case s_type_col_no:
      record.set_type(static_cast<Log_Type>(value));
      break;
    case s_space_id_col_no:
      record.set_space_id(static_cast<space_id_t>(value));
      break;
    case s_page_no_col_no:
      record.set_page_no(static_cast<page_no_t>(value));
      break;
    case s_index_id_col_no:
      record.set_index_id(value);
      break;
    case s_table_id_col_no:
      record.set_table_id(value);
      break;
    case s_old_file_path_col_no:
    case s_new_file_path_col_no:
    default:
      ut_d(ut_error);
  }
}

ulint DDL_Log_Table::fetch_value(const byte *data, ulint offset) {
  ulint value = 0;
  switch (offset) {
    case s_id_col_no:
    case s_thread_id_col_no:
    case s_index_id_col_no:
    case s_table_id_col_no:
      value = mach_read_from_8(data);
      return (value);
    case s_type_col_no:
    case s_space_id_col_no:
    case s_page_no_col_no:
      value = mach_read_from_4(data);
      return (value);
    case s_new_file_path_col_no:
    case s_old_file_path_col_no:
    default:
      ut_d(ut_error);
      ut_o(break);
  }

  ut_o(return (value));
}

dberr_t DDL_Log_Table::search_all(DDL_Records &records) {
  mtr_t mtr;
  btr_pcur_t pcur;
  rec_t *rec;
  bool move = true;
  ulint *offsets;
  dict_index_t *index = m_table->first_index();
  dberr_t error = DB_SUCCESS;

  mtr_start(&mtr);

  /** Scan the index in decreasing order. */
  pcur.open_at_side(false, index, BTR_SEARCH_LEAF, true, 0, &mtr);

  for (; move == true; move = pcur.move_to_prev(&mtr)) {
    rec = pcur.get_rec();

    if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec)) {
      continue;
    }

    offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    if (rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
      continue;
    }

    DDL_Record *record = ut::new_withkey<DDL_Record>(UT_NEW_THIS_FILE_PSI_KEY);
    convert_to_ddl_record(index->is_clustered(), rec, offsets, *record);
    records.push_back(record);
  }

  pcur.close();
  mtr_commit(&mtr);

  return (error);
}

dberr_t DDL_Log_Table::search(ulint thread_id, DDL_Records &records) {
  dberr_t error;
  DDL_Records records_of_thread_id;

  error = search_by_id(thread_id, m_table->first_index()->next(),
                       records_of_thread_id);
  ut_ad(error == DB_SUCCESS);

  for (auto it = records_of_thread_id.rbegin();
       it != records_of_thread_id.rend(); ++it) {
    error = search_by_id((*it)->get_id(), m_table->first_index(), records);
    ut_ad(error == DB_SUCCESS);
  }

  for (auto record : records_of_thread_id) {
    ut::delete_(record);
  }

  return (error);
}

dberr_t DDL_Log_Table::search_by_id(ulint id, dict_index_t *index,
                                    DDL_Records &records) {
  mtr_t mtr;
  btr_pcur_t pcur;
  rec_t *rec;
  bool move = true;
  ulint *offsets;
  dberr_t error = DB_SUCCESS;

  mtr_start(&mtr);

  create_tuple(id, index);
  pcur.open_no_init(index, m_tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, 0, &mtr,
                    UT_LOCATION_HERE);

  for (; move == true; move = pcur.move_to_next(&mtr)) {
    rec = pcur.get_rec();

    if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec)) {
      continue;
    }

    offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    if (cmp_dtuple_rec(m_tuple, rec, index, offsets) != 0) {
      break;
    }

    if (rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
      continue;
    }

    DDL_Record *record = ut::new_withkey<DDL_Record>(UT_NEW_THIS_FILE_PSI_KEY);
    convert_to_ddl_record(index->is_clustered(), rec, offsets, *record);
    records.push_back(record);
  }

  mtr_commit(&mtr);

  return (error);
}

dberr_t DDL_Log_Table::remove(ulint id) {
  mtr_t mtr;
  dict_index_t *clust_index = m_table->first_index();
  btr_pcur_t pcur;
  ulint *offsets;
  rec_t *rec;
  dict_index_t *index;
  dtuple_t *row;
  btr_cur_t *btr_cur;
  dtuple_t *entry;
  dberr_t err = DB_SUCCESS;
  enum row_search_result search_result;
  ulint flags = BTR_NO_LOCKING_FLAG;
  static uint64_t count = 0;

  if (count++ % 64 == 0) {
    log_free_check();
  }

  create_tuple(id, clust_index);

  mtr_start(&mtr);

  pcur.open(clust_index, 0, m_tuple, PAGE_CUR_LE,
            BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &mtr, UT_LOCATION_HERE);

  btr_cur = pcur.get_btr_cur();

  if (page_rec_is_infimum(pcur.get_rec()) ||
      pcur.get_low_match() < clust_index->n_uniq) {
    pcur.close();
    mtr_commit(&mtr);
    return (DB_SUCCESS);
  }

  offsets = rec_get_offsets(pcur.get_rec(), clust_index, nullptr,
                            ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);

  row = row_build(ROW_COPY_DATA, clust_index, pcur.get_rec(), offsets, nullptr,
                  nullptr, nullptr, nullptr, m_heap);

  rec = btr_cur_get_rec(btr_cur);

  if (!rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
    err = btr_cur_del_mark_set_clust_rec(flags, btr_cur_get_block(btr_cur), rec,
                                         clust_index, offsets, m_thr, m_tuple,
                                         &mtr);
  }

  pcur.close();
  mtr_commit(&mtr);

  if (err != DB_SUCCESS) {
    return (err);
  }

  mtr_start(&mtr);

  index = clust_index->next();
  entry = row_build_index_entry(row, nullptr, index, m_heap);
  search_result = row_search_index_entry(
      index, entry, BTR_MODIFY_LEAF | BTR_DELETE_MARK, &pcur, &mtr);
  btr_cur = pcur.get_btr_cur();

  if (search_result == ROW_NOT_FOUND) {
    pcur.close();
    mtr_commit(&mtr);
    ut_d(ut_error);
    ut_o(return (DB_CORRUPTION));
  }

  rec = btr_cur_get_rec(btr_cur);

  if (!rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
    err = btr_cur_del_mark_set_sec_rec(flags, btr_cur, true, m_thr, &mtr);
  }

  pcur.close();
  mtr_commit(&mtr);

  return (err);
}

dberr_t DDL_Log_Table::remove(const DDL_Records &records) {
  dberr_t ret = DB_SUCCESS;

  for (auto record : records) {
    if (record->get_deletable()) {
      dberr_t err = remove(record->get_id());

      ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);
      if (err != DB_SUCCESS) {
        ret = err;
      }
    }
  }

  return (ret);
}

Log_DDL::Log_DDL() {
  ut_ad(dict_sys->ddl_log != nullptr);
  ut_ad(dict_table_has_autoinc_col(dict_sys->ddl_log));
}

inline uint64_t Log_DDL::next_id() {
  uint64_t autoinc;

  dict_table_autoinc_lock(dict_sys->ddl_log);
  autoinc = dict_table_autoinc_read(dict_sys->ddl_log);
  ++autoinc;
  dict_table_autoinc_update_if_greater(dict_sys->ddl_log, autoinc);
  dict_table_autoinc_unlock(dict_sys->ddl_log);

  return (autoinc);
}

inline bool Log_DDL::skip(const dict_table_t *table, THD *thd) {
  return (recv_recovery_on || thread_local_ddl_log_replay ||
          (table != nullptr && table->is_temporary()) ||
          thd_is_bootstrap_thread(thd));
}

dberr_t Log_DDL::write_delete_schema_directory_log(
    trx_t *trx, const char *schema_directory_path, const bool is_drop_schema) {
  uint64_t id = next_id();
  auto thread_id = thd_get_thread_id(trx->mysql_thd);

  trx->ddl_operation = true;

  if (is_drop_schema) {
    /* Attach to the on-going transaction and insert the log */
    return insert_delete_schema_directory_log(trx, id, thread_id,
                                              schema_directory_path);
  }

  /* Create the new transaction, insert log and commit. */
  dberr_t err;
  err = insert_delete_schema_directory_log(nullptr, id, thread_id,
                                           schema_directory_path);
  if (err != DB_SUCCESS) {
    return err;
  }

  /*
    Delete the above committed log entry using the main transaction (
    one thats's used to update the DD).

    It'll be committed as part of the final commmit on SQL Layer.
  */
  err = delete_by_id(trx, id, false);
  ut_ad(err == DB_SUCCESS);

  return err;
}

dberr_t Log_DDL::insert_delete_schema_directory_log(
    trx_t *trx, uint64_t id, ulint thread_id,
    const char *schema_directory_path) {
  const bool has_dd_trx = (trx != nullptr);

  if (!has_dd_trx) {
    trx = trx_allocate_for_background();
    trx_start_internal(trx, UT_LOCATION_HERE);
    trx->ddl_operation = true;
  }

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_old_file_path(schema_directory_path);
  record.set_type(Log_Type::DELETE_SCHEMA_DIRECTORY_LOG);

  dberr_t error;
  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  if (!has_dd_trx) {
    trx_commit_for_mysql(trx);
    trx_free_for_background(trx);
  }

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_INSERT_DELETE_SCHEMA_DIRECTORY_DDL_LOG)
        << "DDL log insert : " << record;
  }

  return error;
}

dberr_t Log_DDL::replay_delete_schema_directory_log(
    const char *schema_directory_path) {
  if (srv_print_ddl_logs) {
    ib::info(ER_IB_DELETE_SCHEMA_DIR) << schema_directory_path;
  }

  const bool send_intermediate_errors =
      (get_server_state() == SERVER_OPERATING);
  bool directory_deletion_failed = false;
  if (!my_rm_dir_w_symlink(schema_directory_path, true,
                           send_intermediate_errors,
                           directory_deletion_failed)) {
    return DB_SUCCESS;
  }

  if (my_errno() == ENOENT) {
    /*  This is to handle an edge case where CREATE DATABASE crashes just after
    inserting the DDL Log (but before creating the directory).

    In any other case than this, non-existence of the directory would've been
    caught very early in the call-flow. */
    return DB_SUCCESS;
  }

  if (directory_deletion_failed) {
    char errbuf[MYSQL_ERRMSG_SIZE];
    my_error(ER_DB_DROP_RMDIR, MYF(0), schema_directory_path, errno,
             my_strerror(errbuf, MYSQL_ERRMSG_SIZE, errno));
  }

  return DB_ERROR;
}

dberr_t Log_DDL::write_free_tree_log(trx_t *trx, const dict_index_t *index,
                                     bool is_drop_table) {
  ut_ad(trx == thd_to_trx(current_thd));

  if (skip(index->table, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  if (index->type & DICT_FTS) {
    ut_ad(index->page == FIL_NULL);
    return (DB_SUCCESS);
  }

  if (dict_index_get_online_status(index) != ONLINE_INDEX_COMPLETE) {
    /* To skip any previously aborted index. This is because this kind
    of index should be already freed in previous post_ddl. It's improper
    to log it and may free it again later, which may trigger some
    double free page problem. */
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);
  dberr_t err;

  trx->ddl_operation = true;

  DBUG_INJECT_CRASH("ddl_log_crash_before_free_tree_log",
                    crash_before_free_tree_log_counter++);

  if (is_drop_table) {
    /* Drop index case, if committed, will be redo only */
    err = insert_free_tree_log(trx, index, id, thread_id);
    if (err != DB_SUCCESS) {
      return err;
    }

    DBUG_INJECT_CRASH("ddl_log_crash_after_free_tree_log",
                      crash_after_free_tree_log_counter++);
  } else {
    /* This is the case of building index during create table
    scenario. The index will be dropped if ddl is rolled back */
    err = insert_free_tree_log(nullptr, index, id, thread_id);
    if (err != DB_SUCCESS) {
      return err;
    }

    DBUG_INJECT_CRASH("ddl_log_crash_after_free_tree_log",
                      crash_after_free_tree_log_counter++);

    DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_1",
                    srv_inject_too_many_concurrent_trxs = true;);

    /* Delete this operation if the create trx is committed */
    err = delete_by_id(trx, id, false);
    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

    DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_1",
                    srv_inject_too_many_concurrent_trxs = false;);

    DBUG_INJECT_CRASH("ddl_log_crash_after_free_tree_delete",
                      crash_after_free_tree_delete_counter++);
  }

  return (err);
}

dberr_t Log_DDL::insert_free_tree_log(trx_t *trx, const dict_index_t *index,
                                      uint64_t id, ulint thread_id) {
  ut_ad(index->page != FIL_NULL);

  dberr_t error;
  bool has_dd_trx = (trx != nullptr);
  if (!has_dd_trx) {
    trx = trx_allocate_for_background();
    trx_start_internal(trx, UT_LOCATION_HERE);
    trx->ddl_operation = true;
  } else {
    trx_start_if_not_started(trx, true, UT_LOCATION_HERE);
  }

  ut_ad(trx->ddl_operation);

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::FREE_TREE_LOG);
  record.set_space_id(index->space);
  record.set_page_no(index->page);
  record.set_index_id(index->id);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  if (!has_dd_trx) {
    trx_commit_for_mysql(trx);
    trx_free_for_background(trx);
  }

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_647) << "DDL log insert : " << record;
  }

  return (error);
}

dberr_t Log_DDL::write_delete_space_log(trx_t *trx, const dict_table_t *table,
                                        space_id_t space_id,
                                        const char *file_path, bool is_drop,
                                        bool dict_locked) {
  ut_ad(trx == thd_to_trx(current_thd));
  ut_ad(table == nullptr || dict_table_is_file_per_table(table));

  if (skip(table, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);
  dberr_t err;

  trx->ddl_operation = true;

  DBUG_INJECT_CRASH("ddl_log_crash_before_delete_space_log",
                    crash_before_delete_space_log_counter++);

  if (is_drop) {
    err = insert_delete_space_log(trx, id, thread_id, space_id, file_path,
                                  dict_locked);
    if (err != DB_SUCCESS) {
      return err;
    }

    DBUG_INJECT_CRASH("ddl_log_crash_after_delete_space_log",
                      crash_after_delete_space_log_counter++);
  } else {
    err = insert_delete_space_log(nullptr, id, thread_id, space_id, file_path,
                                  dict_locked);
    if (err != DB_SUCCESS) {
      return err;
    }

    DBUG_INJECT_CRASH("ddl_log_crash_after_delete_space_log",
                      crash_after_delete_space_log_counter++);

    DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_2",
                    srv_inject_too_many_concurrent_trxs = true;);

    err = delete_by_id(trx, id, dict_locked);
    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

    DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_2",
                    srv_inject_too_many_concurrent_trxs = false;);

    DBUG_INJECT_CRASH("ddl_log_crash_after_delete_space_delete",
                      crash_after_delete_space_delete_counter++);
  }

  return (err);
}

dberr_t Log_DDL::insert_delete_space_log(trx_t *trx, uint64_t id,
                                         ulint thread_id, space_id_t space_id,
                                         const char *file_path,
                                         bool dict_locked) {
  dberr_t error;
  bool has_dd_trx = (trx != nullptr);

  if (!has_dd_trx) {
    trx = trx_allocate_for_background();
    trx_start_internal(trx, UT_LOCATION_HERE);
    trx->ddl_operation = true;
  } else {
    trx_start_if_not_started(trx, true, UT_LOCATION_HERE);
  }

  ut_ad(trx->ddl_operation);

  if (dict_locked) {
    dict_sys_mutex_exit();
  }

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::DELETE_SPACE_LOG);
  record.set_space_id(space_id);
  record.set_old_file_path(file_path);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  if (dict_locked) {
    dict_sys_mutex_enter();
  }

  if (!has_dd_trx) {
    trx_commit_for_mysql(trx);
    trx_free_for_background(trx);
  }

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_648) << "DDL log insert : " << record;
  }

  return (error);
}

dberr_t Log_DDL::write_rename_space_log(space_id_t space_id,
                                        const char *old_file_path,
                                        const char *new_file_path) {
  /* Missing current_thd, it happens during crash recovery */
  if (!current_thd) {
    return (DB_SUCCESS);
  }

  trx_t *trx = thd_to_trx(current_thd);

  /* This is special case for fil_rename_tablespace during recovery */
  if (trx == nullptr) {
    return (DB_SUCCESS);
  }

  if (skip(nullptr, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);

  trx->ddl_operation = true;

  DBUG_INJECT_CRASH("ddl_log_crash_before_rename_space_log",
                    crash_before_rename_space_log_counter++);

  dberr_t err = insert_rename_space_log(id, thread_id, space_id, old_file_path,
                                        new_file_path);
  if (err != DB_SUCCESS) {
    return err;
  }

  DBUG_INJECT_CRASH("ddl_log_crash_after_rename_space_log",
                    crash_after_rename_space_log_counter++);

  DBUG_EXECUTE_IF("ddl_log_crash_after_rename_space_log_insert",
                  DBUG_SUICIDE(););

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_4",
                  srv_inject_too_many_concurrent_trxs = true;);

  err = delete_by_id(trx, id, true);
  ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_4",
                  srv_inject_too_many_concurrent_trxs = false;);

  DBUG_INJECT_CRASH("ddl_log_crash_after_rename_space_delete",
                    crash_after_rename_space_delete_counter++);

  return (err);
}

dberr_t Log_DDL::insert_rename_space_log(uint64_t id, ulint thread_id,
                                         space_id_t space_id,
                                         const char *old_file_path,
                                         const char *new_file_path) {
  dberr_t error;
  trx_t *trx = trx_allocate_for_background();
  trx_start_internal(trx, UT_LOCATION_HERE);
  trx->ddl_operation = true;

  ut_ad(dict_sys_mutex_own());
  dict_sys_mutex_exit();

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::RENAME_SPACE_LOG);
  record.set_space_id(space_id);
  record.set_old_file_path(old_file_path);
  record.set_new_file_path(new_file_path);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  dict_sys_mutex_enter();

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_649) << "DDL log insert : " << record;
  }

  return (error);
}

DDL_Record *Log_DDL::find_alter_encrypt_record(space_id_t space_id) {
  if (!ts_encrypt_ddl_records.empty()) {
    for (const auto it : ts_encrypt_ddl_records) {
      if (it->get_space_id() == space_id) {
        return it;
      }
    }
  }
  return nullptr;
}

dberr_t Log_DDL::write_alter_encrypt_space_log(space_id_t space_id,
                                               Encryption::Progress type,
                                               DDL_Record *existing_rec) {
  /* Missing current_thd, it happens during crash recovery */
  if (!current_thd) {
    return (DB_SUCCESS);
  }

  trx_t *trx = thd_to_trx(current_thd);

  if (skip(nullptr, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);

  trx->ddl_operation = true;

  DBUG_INJECT_CRASH("ddl_log_crash_before_alter_encrypt_space_log",
                    crash_before_alter_encrypt_space_log_counter++);

  dberr_t err = insert_alter_encrypt_space_log(id, thread_id, space_id, type,
                                               existing_rec);
  if (err != DB_SUCCESS) {
    return err;
  }

  DBUG_INJECT_CRASH("ddl_log_crash_after_alter_encrypt_space_log",
                    crash_after_alter_encrypt_space_log_counter++);

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_startup_error_1",
                  srv_inject_too_many_concurrent_trxs = true;);

  /* This record to be removed with main transaction commit */
  err = delete_by_id(trx, id, false);
  ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_startup_error_1",
                  srv_inject_too_many_concurrent_trxs = false;);

  return (err);
}

dberr_t Log_DDL::insert_alter_encrypt_space_log(uint64_t id, ulint thread_id,
                                                space_id_t space_id,
                                                Encryption::Progress type,
                                                DDL_Record *existing_rec) {
  dberr_t err = DB_SUCCESS;
  trx_t *trx = trx_allocate_for_background();
  trx_start_internal(trx, UT_LOCATION_HERE);
  trx->ddl_operation = true;

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);

  if (type == Encryption::Progress::ENCRYPTION) {
    record.set_type(Log_Type::ALTER_ENCRYPT_TABLESPACE_LOG);
  } else {
    ut_ad(type == Encryption::Progress::DECRYPTION);
    record.set_type(Log_Type::ALTER_UNENCRYPT_TABLESPACE_LOG);
  }
  record.set_space_id(space_id);

  {
    DDL_Log_Table ddl_log(trx);
    if (existing_rec != nullptr) {
      err = ddl_log.remove(existing_rec->get_id());
      ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

      if (err == DB_TOO_MANY_CONCURRENT_TRXS) {
        ib::error(ER_IB_MSG_DDL_LOG_DELETE_BY_ID_TMCT);
      }
    }

    if (err == DB_SUCCESS) {
      err = ddl_log.insert(record);
    }
  }

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  if (err == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_1284) << "DDL log insert : " << record;
  }

  return (err);
}

dberr_t Log_DDL::write_drop_log(trx_t *trx, const table_id_t table_id) {
  if (skip(nullptr, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  trx->ddl_operation = true;

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);

  DBUG_INJECT_CRASH("ddl_log_crash_before_drop_log",
                    crash_before_drop_log_counter++);

  dberr_t err;
  err = insert_drop_log(trx, id, thread_id, table_id);
  if (err != DB_SUCCESS) {
    return err;
  }

  DBUG_INJECT_CRASH("ddl_log_crash_after_drop_log",
                    crash_after_drop_log_counter++);

  return (err);
}

dberr_t Log_DDL::insert_drop_log(trx_t *trx, uint64_t id, ulint thread_id,
                                 const table_id_t table_id) {
  ut_ad(trx->ddl_operation);
  ut_ad(dict_sys_mutex_own());

  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  dict_sys_mutex_exit();

  dberr_t error;
  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::DROP_LOG);
  record.set_table_id(table_id);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  dict_sys_mutex_enter();

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_650) << "DDL log insert : " << record;
  }

  return (error);
}

dberr_t Log_DDL::write_rename_table_log(dict_table_t *table,
                                        const char *old_name,
                                        const char *new_name) {
  trx_t *trx = thd_to_trx(current_thd);

  if (skip(table, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);

  trx->ddl_operation = true;

  dberr_t err =
      insert_rename_table_log(id, thread_id, table->id, old_name, new_name);
  if (err != DB_SUCCESS) {
    return err;
  }

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_5",
                  srv_inject_too_many_concurrent_trxs = true;);

  err = delete_by_id(trx, id, true);
  ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_5",
                  srv_inject_too_many_concurrent_trxs = false;);

  return (err);
}

dberr_t Log_DDL::insert_rename_table_log(uint64_t id, ulint thread_id,
                                         table_id_t table_id,
                                         const char *old_name,
                                         const char *new_name) {
  dberr_t error;
  trx_t *trx = trx_allocate_for_background();
  trx_start_internal(trx, UT_LOCATION_HERE);
  trx->ddl_operation = true;

  ut_ad(dict_sys_mutex_own());
  dict_sys_mutex_exit();

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::RENAME_TABLE_LOG);
  record.set_table_id(table_id);
  record.set_old_file_path(old_name);
  record.set_new_file_path(new_name);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  dict_sys_mutex_enter();

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_651) << "DDL log insert : " << record;
  }

  return (error);
}

dberr_t Log_DDL::write_remove_cache_log(trx_t *trx, dict_table_t *table) {
  ut_ad(trx == thd_to_trx(current_thd));

  if (skip(table, trx->mysql_thd)) {
    return (DB_SUCCESS);
  }

  uint64_t id = next_id();
  ulint thread_id = thd_get_thread_id(trx->mysql_thd);

  trx->ddl_operation = true;

  dberr_t err =
      insert_remove_cache_log(id, thread_id, table->id, table->name.m_name);
  if (err != DB_SUCCESS) {
    return err;
  }

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_3",
                  srv_inject_too_many_concurrent_trxs = true;);

  err = delete_by_id(trx, id, false);
  ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

  DBUG_EXECUTE_IF("DDL_Log_remove_inject_error_3",
                  srv_inject_too_many_concurrent_trxs = false;);

  return (err);
}

dberr_t Log_DDL::insert_remove_cache_log(uint64_t id, ulint thread_id,
                                         table_id_t table_id,
                                         const char *table_name) {
  dberr_t error;
  trx_t *trx = trx_allocate_for_background();
  trx_start_internal(trx, UT_LOCATION_HERE);
  trx->ddl_operation = true;

  DDL_Record record;
  record.set_id(id);
  record.set_thread_id(thread_id);
  record.set_type(Log_Type::REMOVE_CACHE_LOG);
  record.set_table_id(table_id);
  record.set_new_file_path(table_name);

  {
    DDL_Log_Table ddl_log(trx);
    error = ddl_log.insert(record);
  }

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  if (error == DB_SUCCESS && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_652) << "DDL log insert : " << record;
  }

  return (error);
}

dberr_t Log_DDL::delete_by_id(trx_t *trx, uint64_t id, bool dict_locked) {
  dberr_t err;

  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  ut_ad(trx->ddl_operation);

  if (dict_locked) {
    dict_sys_mutex_exit();
  }

  {
    DDL_Log_Table ddl_log(trx);
    err = ddl_log.remove(id);
    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

    if (err == DB_TOO_MANY_CONCURRENT_TRXS) {
      ib::error(ER_IB_MSG_DDL_LOG_DELETE_BY_ID_TMCT);
    }
  }

  if (dict_locked) {
    dict_sys_mutex_enter();
  }

  if (srv_print_ddl_logs && err == DB_SUCCESS) {
    ib::info(ER_IB_MSG_DDL_LOG_DELETE_BY_ID_OK) << "DDL log delete : " << id;
  }

  return (err);
}

dberr_t Log_DDL::replay_all() {
  ut_ad(is_in_recovery());

  DDL_Log_Table ddl_log;
  DDL_Records records;

  dberr_t err = ddl_log.search_all(records);
  ut_ad(err == DB_SUCCESS);

  for (auto record : records) {
    err = log_ddl->replay(*record);
    if (err != DB_SUCCESS) {
      break;
    }

    /* Delete the DDL log immediately after applying. Applying the whole set of
    logs is not idempotent. */
    DDL_Records current_records;
    current_records.push_back(record);

    err = delete_by_ids(current_records);
    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);
    if (err != DB_SUCCESS) {
      break;
    }
  }

  for (auto record : records) {
    /* Skip delete if the record object is already added to
    ts_encrypt_ddl_records. */
    if (record->get_deletable()) {
      ut::delete_(record);
    }
  }
  return (err);
}

dberr_t Log_DDL::replay_by_thread_id(ulint thread_id) {
  DDL_Log_Table ddl_log;
  DDL_Records records;

  dberr_t err = ddl_log.search(thread_id, records);
  ut_ad(err == DB_SUCCESS);

  for (auto record : records) {
    if (record->get_type() == Log_Type::ALTER_ENCRYPT_TABLESPACE_LOG ||
        record->get_type() == Log_Type::ALTER_UNENCRYPT_TABLESPACE_LOG) {
      DDL_Record *rec = find_alter_encrypt_record(record->get_space_id());
      if (rec != nullptr) {
        ut_ad(record->get_id() != rec->get_id());
      }
    } else {
      DBUG_INJECT_CRASH("ddl_log_post_ddl_apply_step",
                        crash_post_ddl_apply_step_counter++);
      log_ddl->replay(*record);
    }
    /* Delete the DDL log immediately after applying. Applying the whole set
    of logs is not idempotent e.g. typically the rollback actions of a DDL
    rebuilding a table are as follows.
    1. Delete the newly created tablespace file t1.ibd
    2. Rename the saved old tablespace file tmp_name.ibd to t1.ibd

    If there is a crash after performing both [1] and [2] before removing the
    log entries, we would try to repeat the actions again post recovery and
    end up deleting the file for the base table. We should remove each log
    entry immediately after applying it. */
    DBUG_INJECT_CRASH("ddl_log_post_ddl_apply_step",
                      crash_post_ddl_apply_step_counter++);

    /* A crash at this point would replay the last ddl log again. It is fine
    as a single ddl log execution for a table/tablespace is idempotent. */
    DDL_Records current_records;
    current_records.push_back(record);
    err = delete_by_ids(current_records);

    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);
    if (err != DB_SUCCESS) {
      /* ER_IB_MSG_DDL_LOG_DELETE_BY_ID_TMCT must have already been logged. */
      break;
    }
  }
  DBUG_INJECT_CRASH("ddl_log_post_ddl_apply_step",
                    crash_post_ddl_apply_step_counter++);
  for (auto record : records) {
    /* Skip delete if the record object is already added to
    ts_encrypt_ddl_records. */
    if (record->get_deletable()) {
      ut::delete_(record);
    }
  }
  return (err);
}

constexpr uint32_t DELETE_IDS_RETRIES_MAX = 10;

dberr_t Log_DDL::delete_by_ids(DDL_Records &records) {
  dberr_t err = DB_SUCCESS;

  if (records.empty()) {
    return (err);
  }

  int t;
  for (t = DELETE_IDS_RETRIES_MAX; t > 0; t--) {
    trx_t *trx;
    trx = trx_allocate_for_background();
    trx_start_if_not_started(trx, true, UT_LOCATION_HERE);
    trx->ddl_operation = true;

    {
      DDL_Log_Table ddl_log(trx);
      err = ddl_log.remove(records);
      ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);
    }

    trx_commit_for_mysql(trx);
    trx_free_for_background(trx);

#ifdef UNIV_DEBUG
    if (srv_inject_too_many_concurrent_trxs) {
      srv_inject_too_many_concurrent_trxs = false;
    }
#endif /* UNIV_DEBUG */

    if (err != DB_TOO_MANY_CONCURRENT_TRXS) {
      break;
    }
  }

  return (err);
}

dberr_t Log_DDL::replay(DDL_Record &record) {
  dberr_t err = DB_SUCCESS;

  if (srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_654) << "DDL log replay : " << record;
  }

  switch (record.get_type()) {
    case Log_Type::FREE_TREE_LOG:
      replay_free_tree_log(record.get_space_id(), record.get_page_no(),
                           record.get_index_id());
      break;

    case Log_Type::DELETE_SCHEMA_DIRECTORY_LOG:
      err = replay_delete_schema_directory_log(record.get_old_file_path());
      break;

    case Log_Type::DELETE_SPACE_LOG:
      replay_delete_space_log(record.get_space_id(),
                              record.get_old_file_path());
      break;

    case Log_Type::RENAME_SPACE_LOG:
      replay_rename_space_log(record.get_space_id(), record.get_old_file_path(),
                              record.get_new_file_path());
      break;

    case Log_Type::DROP_LOG:
      replay_drop_log(record.get_table_id());
      break;

    case Log_Type::RENAME_TABLE_LOG:
      replay_rename_table_log(record.get_old_file_path(),
                              record.get_new_file_path());
      break;

    case Log_Type::REMOVE_CACHE_LOG:
      replay_remove_cache_log(record.get_table_id(),
                              record.get_new_file_path());
      break;

    case Log_Type::ALTER_ENCRYPT_TABLESPACE_LOG:
    case Log_Type::ALTER_UNENCRYPT_TABLESPACE_LOG:
      err = replay_alter_encrypt_space_log(record);
      break;

    default:
      ut_error;
  }

  return (err);
}

void Log_DDL::replay_free_tree_log(space_id_t space_id, page_no_t page_no,
                                   ulint index_id) {
  ut_ad(space_id != SPACE_UNKNOWN);
  ut_ad(page_no != FIL_NULL);

  bool found;
  const page_size_t page_size(fil_space_get_page_size(space_id, &found));

  /* Skip if it is a single table tablespace and the .ibd
  file is missing */
  if (!found) {
    if (srv_print_ddl_logs) {
      ib::info(ER_IB_MSG_655)
          << "DDL log replay : FREE tablespace " << space_id << " is missing.";
    }

    return;
  }

  /* This is required by dropping hash index afterwards. */
  dict_sys_mutex_enter();

  mtr_t mtr;
  mtr_start(&mtr);

  btr_free_if_exists(page_id_t(space_id, page_no), page_size, index_id, &mtr);

  mtr_commit(&mtr);

  dict_sys_mutex_exit();

  DBUG_INJECT_CRASH("ddl_log_crash_after_replay", crash_after_replay_counter++);
}

void Log_DDL::replay_delete_space_log(space_id_t space_id,
                                      const char *file_path) {
  THD *thd = current_thd;

  if (fsp_is_undo_tablespace(space_id)) {
    /* Serialize this delete with all undo tablespace DDLs. */
    mutex_enter(&undo::ddl_mutex);

    /* If this is called during DROP UNDO TABLESPACE, then the undo_space
    is already gone. But if this is called at startup after a crash, that
    memory object might exist. If the crash occurred just before the file
    was deleted, then at startup it was opened in srv_undo_tablespaces_open().
    Then in trx_rsegs_init(), any explicit undo tablespace that did not
    contain any undo logs was set to empty.  That prevented any new undo
    logs to be added during the startup process up till now.  So whether
    we are at runtime or startup, we assert that the undo tablespace is
    empty and delete the undo::Tablespace object if it exists. */
    undo::spaces->x_lock();
    space_id_t space_num = undo::id2num(space_id);
    undo::Tablespace *undo_space = undo::spaces->find(space_num);
    if (undo_space != nullptr) {
      ut_a(undo_space->is_empty());
      undo::spaces->drop(undo_space);
    }
    undo::spaces->x_unlock();
  }

  if (thd != nullptr) {
    /* For general tablespace, MDL on SDI tables is already
    acquired at innobase_drop_tablespace() and for file_per_table
    tablespace, MDL is acquired at row_drop_table_for_mysql() */
    dict_sys_mutex_enter();
    dict_sdi_remove_from_cache(space_id, nullptr, true);
    dict_sys_mutex_exit();
  }

  /* A master key rotation blocks all DDLs using backup_lock, so it is assured
  that during CREATE/DROP TABLE, master key will not change. */

  DBUG_EXECUTE_IF("ddl_log_replay_delete_space_crash_before_drop",
                  DBUG_SUICIDE(););

  /* Update filename with correct partition case, of needed. */
  std::string path_str(file_path);
  std::string space_name;
  fil_update_partition_name(space_id, 0, false, space_name, path_str);
  file_path = path_str.c_str();

  row_drop_tablespace(space_id, file_path);

  /* If this is an undo space_id, allow the undo number for it
  to be reused. */
  if (fsp_is_undo_tablespace(space_id)) {
    undo::spaces->x_lock();
    undo::unuse_space_id(space_id);
    undo::spaces->x_unlock();

    mutex_exit(&undo::ddl_mutex);
  }

  DBUG_INJECT_CRASH("ddl_log_crash_after_replay", crash_after_replay_counter++);
}

void Log_DDL::replay_rename_space_log(space_id_t space_id,
                                      const char *old_file_path,
                                      const char *new_file_path) {
  bool ret;
  page_id_t page_id(space_id, 0);

  std::string space_name;

  /* Update old filename with correct partition case, of needed. */
  std::string old_path(old_file_path);
  fil_update_partition_name(space_id, 0, false, space_name, old_path);
  old_file_path = old_path.c_str();

  /* Update new filename with correct partition case, of needed. */
  std::string new_path(new_file_path);
  fil_update_partition_name(space_id, 0, false, space_name, new_path);
  new_file_path = new_path.c_str();

  ret = fil_op_replay_rename_for_ddl(page_id, old_file_path, new_file_path);

  if (!ret && srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_656) << "DDL log replay : RENAME from " << old_file_path
                            << " to " << new_file_path << " failed";
  }

  DBUG_INJECT_CRASH("ddl_log_crash_after_replay", crash_after_replay_counter++);
}

static dberr_t replace_and_insert(DDL_Record *record) {
  dberr_t err = DB_SUCCESS;
  trx_t *trx = trx_allocate_for_background();
  trx_start_internal(trx, UT_LOCATION_HERE);
  trx->ddl_operation = true;

  /* update the thread_id for the record */
  record->set_thread_id(ULINT_MAX);

  {
    DDL_Log_Table ddl_log(trx);
    /* Remove old record and insert the new record */
    err = ddl_log.remove(record->get_id());
    ut_ad(err == DB_SUCCESS || err == DB_TOO_MANY_CONCURRENT_TRXS);

    if (err == DB_SUCCESS) {
      /* Insert new record entry */
      err = ddl_log.insert(*record);
    }
  }

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  return err;
}

dberr_t Log_DDL::replay_alter_encrypt_space_log(DDL_Record &record) {
  dberr_t error = DB_SUCCESS;
  /* Normal operation, we shouldn't come here during post_ddl */
  ut_ad(is_in_recovery());

  error = replace_and_insert(&record);
  if (error != DB_SUCCESS) {
    return error;
  }
  ut_ad(record.get_thread_id() == ULINT_MAX);

  /* We could have resume encryption execution one by one for each tablespace
  from here by calling SQL API to run the query. But then it would be blocking
  server bootstrap. We need to resume this encryption in BG thread so we need
  to just make a note of this space and operation here and don't do any real
  operation. */
  ts_encrypt_ddl_records.push_back(&record);

  /* Make sure not to delete this record till resume operation finishes.
  This is to make sure that if there is a crash before that, we can resume
  encryption in the next restart. */
  record.set_deletable(false);

  DBUG_INJECT_CRASH("ddl_log_crash_after_replay", crash_after_replay_counter++);
  return error;
}

void Log_DDL::replay_drop_log(const table_id_t table_id) {
  mutex_enter(&dict_persist->mutex);
  ut_d(dberr_t error =) dict_persist->table_buffer->remove(table_id);
  ut_ad(error == DB_SUCCESS);
  mutex_exit(&dict_persist->mutex);

  DBUG_INJECT_CRASH("ddl_log_crash_after_replay", crash_after_replay_counter++);
}

void Log_DDL::replay_rename_table_log(const char *old_name,
                                      const char *new_name) {
  if (is_in_recovery()) {
    if (srv_print_ddl_logs) {
      ib::info(ER_IB_MSG_657) << "DDL log replay : in recovery,"
                              << " skip RENAME TABLE";
    }

    return;
  }

  trx_t *trx;
  trx = trx_allocate_for_background();
  trx->mysql_thd = current_thd;
  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);
  trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

  /* Convert partition table name DDL log, if needed. Required if
  upgrading a crashed database. */
  std::string old_table(old_name);
  dict_name::rebuild(old_table);
  old_name = old_table.c_str();

  std::string new_table(new_name);
  dict_name::rebuild(new_table);
  new_name = new_table.c_str();

  dberr_t err;
  err = row_rename_table_for_mysql(old_name, new_name, nullptr, trx, true);

  dict_table_t *table;
  table = dd_table_open_on_name_in_mem(new_name, true);
  if (table != nullptr) {
    dict_table_ddl_release(table);
    dd_table_close(table, nullptr, nullptr, true);
  }

  row_mysql_unlock_data_dictionary(trx);

  trx_commit_for_mysql(trx);
  trx_free_for_background(trx);

  if (err != DB_SUCCESS) {
    if (srv_print_ddl_logs) {
      ib::info(ER_IB_MSG_658)
          << "DDL log replay : rename table"
          << " in cache from " << old_name << " to " << new_name;
    }
  } else {
    /* TODO: Once we get rid of dict_operation_lock,
    we may consider to do this in row_rename_table_for_mysql,
    so no need to worry this rename here */
    char errstr[512];

    dict_stats_rename_table(old_name, new_name, errstr, sizeof(errstr));
  }
}

void Log_DDL::replay_remove_cache_log(table_id_t table_id,
                                      const char *table_name) {
  if (is_in_recovery()) {
    if (srv_print_ddl_logs) {
      ib::info(ER_IB_MSG_659) << "DDL log replay : in recovery,"
                              << " skip REMOVE CACHE";
    }

    return;
  }

  dict_table_t *table;

  table = dd_table_open_on_id_in_mem(table_id, false);

  /* Convert partition table name DDL log, if needed. Required if
  upgrading a crashed database. */
  std::string table_str(table_name);
  dict_name::rebuild(table_str);
  table_name = table_str.c_str();

  if (table != nullptr) {
    ut_ad(strcmp(table->name.m_name, table_name) == 0);

    dict_sys_mutex_enter();
    dd_table_close(table, nullptr, nullptr, true);
    btr_drop_ahi_for_table(table);
    dict_table_remove_from_cache(table);
    dict_sys_mutex_exit();
  }
}

dberr_t Log_DDL::post_ddl(THD *thd) {
  if (skip(nullptr, thd)) {
    return (DB_SUCCESS);
  }

  if (srv_read_only_mode || srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN) {
    return (DB_SUCCESS);
  }

  DEBUG_SYNC(thd, "innodb_ddl_log_before_enter");

  DBUG_EXECUTE_IF("ddl_log_before_post_ddl", DBUG_SUICIDE(););

  /* If srv_force_recovery > 0, DROP TABLE is allowed, and here only
  DELETE and DROP log can be replayed. */

  ulint thread_id = thd_get_thread_id(thd);

  if (srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_660)
        << "DDL log post ddl : begin for thread id : " << thread_id;
  }

  thread_local_ddl_log_replay = true;

  dberr_t err = replay_by_thread_id(thread_id);

  thread_local_ddl_log_replay = false;

  if (srv_print_ddl_logs) {
    ib::info(ER_IB_MSG_661)
        << "DDL log post ddl : end for thread id : " << thread_id;
  }

  return (err);
}

dberr_t Log_DDL::recover() {
  if (srv_read_only_mode || srv_force_recovery > 0) {
    return (DB_SUCCESS);
  }

  ib::info(ER_IB_MSG_662) << "DDL log recovery : begin";

  thread_local_ddl_log_replay = true;
  s_in_recovery = true;

  dberr_t err = replay_all();

  thread_local_ddl_log_replay = false;
  s_in_recovery = false;

  ib::info(ER_IB_MSG_663) << "DDL log recovery : end";

  return (err);
}
