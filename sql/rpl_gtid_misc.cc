/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_thread.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/psi/mysql_mutex.h"
#include "nulls.h"
#include "sql/rpl_gtid.h"
#include "typelib.h"

struct mysql_mutex_t;

#ifdef MYSQL_SERVER
#include "mysql/thread_type.h"
#include "mysqld_error.h"  // ER_*
#include "sql/binlog.h"
#include "sql/current_thd.h"
#include "sql/rpl_msr.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "storage/perfschema/pfs_instr_class.h"  // gtid_monitoring_getsystime
#endif                                           // ifdef MYSQL_SERVER

#ifndef MYSQL_SERVER
#include "client/mysqlbinlog.h"
#endif

#include "sql/log.h"

using mysql::gtid::Tsid;
using mysql::utils::Return_status;

ulong _gtid_consistency_mode;
const char *gtid_consistency_mode_names[] = {"OFF", "ON", "WARN", NullS};
TYPELIB gtid_consistency_mode_typelib = {
    array_elements(gtid_consistency_mode_names) - 1, "",
    gtid_consistency_mode_names, nullptr};

#ifdef MYSQL_SERVER
enum_gtid_consistency_mode get_gtid_consistency_mode() {
  global_tsid_lock->assert_some_lock();
  return (enum_gtid_consistency_mode)_gtid_consistency_mode;
}
#endif

std::size_t Gtid::skip_whitespace(const char *text, std::size_t pos) {
  DBUG_TRACE;
  while (my_isspace(&my_charset_utf8mb3_general_ci, text[pos])) {
    ++pos;
  }
  return pos;
}

std::tuple<Return_status, rpl_sid, std::size_t> Gtid::parse_sid_str(
    const char *text, std::size_t pos) {
  DBUG_TRACE;
  Return_status status = Return_status::ok;
  rpl_sid sid{};
  sid.clear();
  pos = skip_whitespace(text, pos);
  if (sid.parse(&text[pos], mysql::gtid::Uuid::TEXT_LENGTH) == 0) {
    pos += mysql::gtid::Uuid::TEXT_LENGTH;
    pos = skip_whitespace(text, pos);
  } else {
    DBUG_PRINT("info", ("not a uuid at char %d in '%s'", (int)pos, text));
    status = Return_status::error;
  }
  return std::make_tuple(status, sid, pos);
}

std::pair<Gtid::Tag, std::size_t> Gtid::parse_tag_str(const char *text,
                                                      std::size_t pos) {
  DBUG_TRACE;
  Gtid::Tag tag;
  pos = skip_whitespace(text, pos);
  pos += tag.from_cstring(text + pos);
  pos = skip_whitespace(text, pos);
  return std::make_pair(tag, pos);
}

std::tuple<Return_status, rpl_gno, std::size_t> Gtid::parse_gno_str(
    const char *text, std::size_t pos) {
  DBUG_TRACE;
  auto status = Return_status::ok;
  const char *text_cpy = text + pos;
  rpl_gno gno_var = parse_gno(&text_cpy);
  pos = text_cpy - text;
  if (gno_var > 0) {
    pos = skip_whitespace(text, pos);
    if (text[pos] != '\0') {
      DBUG_PRINT("info", ("expected end of string, found garbage '%.80s' "
                          "at char %d in '%s'",
                          text_cpy, (int)pos, text));
      status = Return_status::error;
      gno_var = 0;
    }
  } else {
    DBUG_PRINT("info",
               ("GNO was zero or invalid (%" PRId64 ") at char %d in '%s'",
                gno_var, (int)pos, text));
    status = Return_status::error;
  }
  return std::make_tuple(status, gno_var, pos);
}

void Gtid::report_parsing_error(const char *text) {
  DBUG_TRACE;
  BINLOG_ERROR(("Malformed GTID specification: %.200s", text),
               (ER_MALFORMED_GTID_SPECIFICATION, MYF(0), text));
}

std::pair<Return_status, std::size_t> Gtid::parse_gtid_separator(
    const char *text, std::size_t pos) {
  DBUG_TRACE;
  auto status = Return_status::ok;
  pos = skip_whitespace(text, pos);
  if (text[pos] != gtid_separator) {
    DBUG_PRINT("info", ("missing colon at char %d in '%s'", (int)pos, text));
    status = Return_status::error;
  } else {
    pos = skip_whitespace(text, pos + 1);
  }
  return std::make_pair(status, pos);
}

std::pair<Return_status, mysql::gtid::Gtid> Gtid::parse_gtid_from_cstring(
    const char *text) {
  DBUG_TRACE;
  auto invalid_gtid = std::make_pair(Return_status::error, mysql::gtid::Gtid());

  auto [status, uuid, pos] = parse_sid_str(text, 0);
  if (status != Return_status::ok) {
    return invalid_gtid;
  }
  std::tie(status, pos) = parse_gtid_separator(text, pos);
  if (status != Return_status::ok) {
    return invalid_gtid;
  }
  mysql::gtid::Tag tag;
  std::tie(tag, pos) = parse_tag_str(text, pos);
  if (tag.is_defined()) {
    std::tie(status, pos) = parse_gtid_separator(text, pos);
    if (status != Return_status::ok) {
      return invalid_gtid;
    }
  }
  rpl_gno gno;
  std::tie(status, gno, pos) = parse_gno_str(text, pos);
  if (status != Return_status::ok) {
    return invalid_gtid;
  }
  return std::make_pair(Return_status::ok,
                        mysql::gtid::Gtid(mysql::gtid::Tsid(uuid, tag), gno));
}

Return_status Gtid::parse(Tsid_map *tsid_map, const char *text) {
  DBUG_TRACE;
  auto [status, parsed_gtid] = parse_gtid_from_cstring(text);
  if (status == Return_status::error) {
    report_parsing_error(text);
    return status;
  }
  rpl_sidno sidno_var = tsid_map->add_tsid(parsed_gtid.get_tsid());
  if (sidno_var <= 0) {
    status = Return_status::error;
  }
  rpl_gno gno_var = parsed_gtid.get_gno();
  if (status == Return_status::ok) {
    sidno = sidno_var;
    gno = gno_var;
  }
  return status;
}

int Gtid::to_string_gno(char *buf) const {
  DBUG_TRACE;
  int id = 0;
  *buf = ':';
  ++id;
  id += format_gno(buf + id, gno);
  return id;
}

int Gtid::to_string(const Tsid &tsid, char *buf) const {
  DBUG_TRACE;
  int id = 0;
  id += tsid.to_string(buf);
  id += to_string_gno(buf + id);
  return id;
}

int Gtid::to_string(const Tsid_map *tsid_map, char *buf, bool need_lock) const {
  DBUG_TRACE;
  int ret;
  if (tsid_map != nullptr) {
    Checkable_rwlock *lock = tsid_map->get_tsid_lock();
    if (lock) {
      if (need_lock)
        lock->rdlock();
      else
        lock->assert_some_lock();
    }
    const auto tsid = tsid_map->sidno_to_tsid(sidno);
    if (lock && need_lock) lock->unlock();
    ret = to_string(tsid, buf);
  } else {
#ifdef NDEBUG
    /*
      NULL is only allowed in debug mode, since the sidno does not
      make sense for users but is useful to include in debug
      printouts.  Therefore, we want to ASSERT(0) in non-debug mode.
      Since there is no ASSERT in non-debug mode, we use abort
      instead.
    */
    my_abort();
#endif
    ret = sprintf(buf, "%d:%" PRId64, sidno, gno);
  }
  return ret;
}

bool Gtid::is_valid(const char *text) {
  DBUG_TRACE;
  Return_status status;
  std::tie(status, std::ignore) = parse_gtid_from_cstring(text);
  return status == Return_status::ok;
}

#ifndef NDEBUG
void check_return_status(enum_return_status status, const char *action,
                         const char *status_name, int allow_unreported) {
  if (status != RETURN_STATUS_OK) {
    assert(allow_unreported || status == RETURN_STATUS_REPORTED_ERROR);
    if (status == RETURN_STATUS_REPORTED_ERROR) {
#if defined(MYSQL_SERVER) && !defined(NDEBUG)
      THD *thd = current_thd;
      /*
        We create a new system THD with 'SYSTEM_THREAD_COMPRESS_GTID_TABLE'
        when initializing gtid state by fetching gtids during server startup,
        so we can check on it before diagnostic area is active and skip the
        assert in this case. We assert that diagnostic area logged the error
        outside server startup since the assert is really useful.
     */
      assert(thd == nullptr ||
             thd->get_stmt_da()->status() == Diagnostics_area::DA_ERROR ||
             (thd->get_stmt_da()->status() == Diagnostics_area::DA_EMPTY &&
              thd->system_thread == SYSTEM_THREAD_COMPRESS_GTID_TABLE));
#endif
    }
    DBUG_PRINT("info", ("%s error %d (%s)", action, status, status_name));
  }
}
#endif  // ! NDEBUG

#ifdef MYSQL_SERVER

rpl_sidno get_sidno_from_global_tsid_map(const Tsid &tsid) {
  DBUG_TRACE;

  global_tsid_lock->rdlock();
  rpl_sidno sidno = global_tsid_map->add_tsid(tsid);
  global_tsid_lock->unlock();

  return sidno;
}

const Tsid &get_tsid_from_global_tsid_map(rpl_sidno sidno) {
  DBUG_TRACE;
  Checkable_rwlock::Guard g(*global_tsid_lock, Checkable_rwlock::READ_LOCK);
  return global_tsid_map->sidno_to_tsid(sidno);
}

rpl_gno get_last_executed_gno(rpl_sidno sidno) {
  DBUG_TRACE;

  global_tsid_lock->rdlock();
  rpl_gno gno = gtid_state->get_last_executed_gno(sidno);
  global_tsid_lock->unlock();

  return gno;
}

Trx_monitoring_info::Trx_monitoring_info() { clear(); }

Trx_monitoring_info::Trx_monitoring_info(const Trx_monitoring_info &info) {
  if ((is_info_set = info.is_info_set)) {
    gtid = info.gtid;
    original_commit_timestamp = info.original_commit_timestamp;
    immediate_commit_timestamp = info.immediate_commit_timestamp;
    start_time = info.start_time;
    end_time = info.end_time;
    skipped = info.skipped;
    last_transient_error_number = info.last_transient_error_number;
    strcpy(last_transient_error_message, info.last_transient_error_message);
    last_transient_error_timestamp = info.last_transient_error_timestamp;
    transaction_retries = info.transaction_retries;
    is_retrying = info.is_retrying;
    compression_type = info.compression_type;
    compressed_bytes = info.compressed_bytes;
    uncompressed_bytes = info.uncompressed_bytes;
  }
}

void Trx_monitoring_info::clear() {
  gtid = {0, 0};
  original_commit_timestamp = 0;
  immediate_commit_timestamp = 0;
  start_time = 0;
  end_time = 0;
  skipped = false;
  is_info_set = false;
  last_transient_error_number = 0;
  last_transient_error_message[0] = '\0';
  last_transient_error_timestamp = 0;
  transaction_retries = 0;
  is_retrying = false;
  compression_type = mysql::binlog::event::compression::type::NONE;
  compressed_bytes = 0;
  uncompressed_bytes = 0;
}

void Trx_monitoring_info::copy_to_ps_table(Tsid_map *tsid_map, char *gtid_arg,
                                           uint *gtid_length_arg,
                                           ulonglong *original_commit_ts_arg,
                                           ulonglong *immediate_commit_ts_arg,
                                           ulonglong *start_time_arg) const {
  assert(tsid_map);
  assert(gtid_arg);
  assert(gtid_length_arg);
  assert(original_commit_ts_arg);
  assert(immediate_commit_ts_arg);
  assert(start_time_arg);

  if (is_info_set) {
    // The trx_monitoring_info is populated
    if (gtid.is_empty()) {
      // The transaction is anonymous
      memcpy(gtid_arg, "ANONYMOUS", 10);
      *gtid_length_arg = 9;
    } else {
      // The GTID is set
      Checkable_rwlock *tsid_lock = tsid_map->get_tsid_lock();
      tsid_lock->rdlock();
      *gtid_length_arg = gtid.to_string(tsid_map, gtid_arg);
      tsid_lock->unlock();
    }
    *original_commit_ts_arg = original_commit_timestamp;
    *immediate_commit_ts_arg = immediate_commit_timestamp;
    *start_time_arg = start_time;
  } else {
    // This monitoring info is not populated, so let's zero the input
    memcpy(gtid_arg, "", 1);
    *gtid_length_arg = 0;
    *original_commit_ts_arg = 0;
    *immediate_commit_ts_arg = 0;
    *start_time_arg = 0;
  }
}

void Trx_monitoring_info::copy_to_ps_table(Tsid_map *tsid_map, char *gtid_arg,
                                           uint *gtid_length_arg,
                                           ulonglong *original_commit_ts_arg,
                                           ulonglong *immediate_commit_ts_arg,
                                           ulonglong *start_time_arg,
                                           ulonglong *end_time_arg) const {
  assert(end_time_arg);

  *end_time_arg = is_info_set ? end_time : 0;
  copy_to_ps_table(tsid_map, gtid_arg, gtid_length_arg, original_commit_ts_arg,
                   immediate_commit_ts_arg, start_time_arg);
}

void Trx_monitoring_info::copy_to_ps_table(
    Tsid_map *tsid_map, char *gtid_arg, uint *gtid_length_arg,
    ulonglong *original_commit_ts_arg, ulonglong *immediate_commit_ts_arg,
    ulonglong *start_time_arg, uint *last_transient_errno_arg,
    char *last_transient_errmsg_arg, uint *last_transient_errmsg_length_arg,
    ulonglong *last_transient_timestamp_arg, ulong *retries_count_arg) const {
  assert(last_transient_errno_arg);
  assert(last_transient_errmsg_arg);
  assert(last_transient_errmsg_length_arg);
  assert(last_transient_timestamp_arg);
  assert(retries_count_arg);

  if (is_info_set) {
    *last_transient_errno_arg = last_transient_error_number;
    strcpy(last_transient_errmsg_arg, last_transient_error_message);
    *last_transient_errmsg_length_arg = strlen(last_transient_error_message);
    *last_transient_timestamp_arg = last_transient_error_timestamp;
    *retries_count_arg = transaction_retries;
  } else {
    *last_transient_errno_arg = 0;
    memcpy(last_transient_errmsg_arg, "", 1);
    *last_transient_errmsg_length_arg = 0;
    *last_transient_timestamp_arg = 0;
    *retries_count_arg = 0;
  }
  copy_to_ps_table(tsid_map, gtid_arg, gtid_length_arg, original_commit_ts_arg,
                   immediate_commit_ts_arg, start_time_arg);
}

void Trx_monitoring_info::copy_to_ps_table(
    Tsid_map *tsid_map, char *gtid_arg, uint *gtid_length_arg,
    ulonglong *original_commit_ts_arg, ulonglong *immediate_commit_ts_arg,
    ulonglong *start_time_arg, ulonglong *end_time_arg,
    uint *last_transient_errno_arg, char *last_transient_errmsg_arg,
    uint *last_transient_errmsg_length_arg,
    ulonglong *last_transient_timestamp_arg, ulong *retries_count_arg) const {
  assert(end_time_arg);

  *end_time_arg = is_info_set ? end_time : 0;
  copy_to_ps_table(tsid_map, gtid_arg, gtid_length_arg, original_commit_ts_arg,
                   immediate_commit_ts_arg, start_time_arg,
                   last_transient_errno_arg, last_transient_errmsg_arg,
                   last_transient_errmsg_length_arg,
                   last_transient_timestamp_arg, retries_count_arg);
}

Gtid_monitoring_info::Gtid_monitoring_info(mysql_mutex_t *atomic_mutex_arg)
    : atomic_mutex(atomic_mutex_arg) {
  processing_trx = new Trx_monitoring_info;
  last_processed_trx = new Trx_monitoring_info;
}

Gtid_monitoring_info::~Gtid_monitoring_info() {
  delete last_processed_trx;
  delete processing_trx;
}

void Gtid_monitoring_info::atomic_lock() {
  if (atomic_mutex == nullptr) {
    bool expected = false;
    while (!atomic_locked.compare_exchange_weak(expected, true)) {
      /*
        On exchange failures, the atomic_locked value (true) is set
        to the expected variable. It needs to be reset again.
      */
      expected = false;
      /*
        All "atomic" operations on this object are based on copying
        variable contents and setting values. They should not take long.
      */
      my_thread_yield();
    }
#ifndef NDEBUG
    assert(!is_locked);
    is_locked = true;
#endif
  } else {
    // If this object is relying on a mutex, just ensure it was acquired.
    mysql_mutex_assert_owner(atomic_mutex)
  }
}

void Gtid_monitoring_info::atomic_unlock() {
  if (atomic_mutex == nullptr) {
#ifndef NDEBUG
    assert(is_locked);
    is_locked = false;
#endif
    atomic_locked = false;
  } else
    mysql_mutex_assert_owner(atomic_mutex)
}

void Gtid_monitoring_info::clear() {
  atomic_lock();
  processing_trx->clear();
  last_processed_trx->clear();
  atomic_unlock();
}

void Gtid_monitoring_info::clear_processing_trx() {
  atomic_lock();
  processing_trx->clear();
  atomic_unlock();
}

void Gtid_monitoring_info::clear_last_processed_trx() {
  atomic_lock();
  last_processed_trx->clear();
  atomic_unlock();
}

void Gtid_monitoring_info::update(mysql::binlog::event::compression::type t,
                                  size_t payload_size,
                                  size_t uncompressed_size) {
  processing_trx->compression_type = t;
  processing_trx->compressed_bytes = payload_size;
  processing_trx->uncompressed_bytes = uncompressed_size;
}

void Gtid_monitoring_info::start(Gtid gtid_arg, ulonglong original_ts_arg,
                                 ulonglong immediate_ts_arg, bool skipped_arg) {
  /**
    When a new transaction starts processing, we reset all the information from
    the previous processing_trx and fetch the current timestamp as the new
    start_time.
  */
  if (!processing_trx->gtid.equals(gtid_arg) || !processing_trx->is_retrying) {
    /* Collect current timestamp before the atomic operation */
    ulonglong start_time = gtid_monitoring_getsystime();

    atomic_lock();
    processing_trx->gtid = gtid_arg;
    processing_trx->original_commit_timestamp = original_ts_arg;
    processing_trx->immediate_commit_timestamp = immediate_ts_arg;
    processing_trx->start_time = start_time;
    processing_trx->end_time = 0;
    processing_trx->skipped = skipped_arg;
    processing_trx->is_info_set = true;
    processing_trx->last_transient_error_number = 0;
    processing_trx->last_transient_error_message[0] = '\0';
    processing_trx->last_transient_error_timestamp = 0;
    processing_trx->transaction_retries = 0;
    processing_trx->compression_type =
        mysql::binlog::event::compression::type::NONE;
    processing_trx->compressed_bytes = 0;
    processing_trx->uncompressed_bytes = 0;
    atomic_unlock();
  } else {
    /**
      If the transaction is being retried, only update the skipped field
      because it determines if the information will be kept after it finishes
      executing.
    */
    atomic_lock();
    processing_trx->skipped = skipped_arg;
    atomic_unlock();
  }
}

void Gtid_monitoring_info::finish() {
  /* Collect current timestamp before the atomic operation */
  ulonglong end_time = gtid_monitoring_getsystime();

  atomic_lock();
  processing_trx->end_time = end_time;
  /*
    We only swap if the transaction was not skipped.

    Notice that only applier thread set the skipped variable to true.
  */
  if (!processing_trx->skipped) std::swap(processing_trx, last_processed_trx);

  processing_trx->clear();
  atomic_unlock();
}

void Gtid_monitoring_info::copy_info_to(
    Trx_monitoring_info *processing_dest,
    Trx_monitoring_info *last_processed_dest) {
  atomic_lock();
  *processing_dest = *processing_trx;
  *last_processed_dest = *last_processed_trx;
  atomic_unlock();
}

void Gtid_monitoring_info::copy_info_to(Gtid_monitoring_info *dest) {
  copy_info_to(dest->processing_trx, dest->last_processed_trx);
}

bool Gtid_monitoring_info::is_processing_trx_set() {
  /*
    This function is only called by threads about to update the monitoring
    information. It should be safe to collect this information without
    acquiring locks.
  */
  return processing_trx->is_info_set;
}

const Gtid *Gtid_monitoring_info::get_processing_trx_gtid() {
  /*
    This function is only called by relay log recovery/queuing.
  */
  assert(atomic_mutex != nullptr);
  mysql_mutex_assert_owner(atomic_mutex);
  return &processing_trx->gtid;
}

void Gtid_monitoring_info::store_transient_error(
    uint transient_errno_arg, const char *transient_err_message_arg,
    ulong trans_retries_arg) {
  ulonglong retry_timestamp = gtid_monitoring_getsystime();
  processing_trx->is_retrying = true;
  atomic_lock();
  processing_trx->transaction_retries = trans_retries_arg;
  processing_trx->last_transient_error_number = transient_errno_arg;
  snprintf(processing_trx->last_transient_error_message,
           sizeof(processing_trx->last_transient_error_message), "%.*s",
           MAX_SLAVE_ERRMSG - 1, transient_err_message_arg);
  processing_trx->last_transient_error_timestamp = retry_timestamp;
  atomic_unlock();
}
#endif  // ifdef MYSQL_SERVER
