/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "log0sys_var_handler.h"  // Sys_var_handler
#include "log0buf.h"              // log_buffer_resize
#include "log0chkp.h"             // log_checkpointing
#include "log0files_governor.h"   // log_files_resize_requested
#include "log0handler.h"          // update_free_check_limit
#include "log0log.h"              // log_write_ahead_resize
#include "log0write.h"            // log_control_writer_threads
#include "my_sys.h"               // my_error
#include "sql/sql_class.h"        // THD object
#include "ut0dbg.h"               // ut_a

namespace ib::redo {

Sys_var_handler::~Sys_var_handler() {}

bool Sys_var_handler::update_var(THD *thd, std::string_view name,
                                 uint64_t new_value) {
  if (name == "innodb_log_buffer_size") {
    return buffer_size_update(new_value);
  }
  if (name == "innodb_redo_log_capacity") {
    return capacity_update(thd, name, new_value);
  }
  if (name == "innodb_log_write_ahead_size") {
    return log_write_ahead_size_update(thd, name, new_value);
  }
  return false;  // Unknown variable
}

bool Sys_var_handler::update_var(THD *, std::string_view name, bool new_value) {
  if (name == "innodb_log_writer_threads") {
    writer_threads_update(new_value);
    return true;
  }
  return false;  // Unknown variable
}

bool Sys_var_handler::buffer_size_update(uint64_t value) {
  if (log_buffer_resize(*log_sys, static_cast<size_t>(value))) {
    return true;
  }
  /* This could happen if we tried to decrease size of the
  log buffer but we had more data in the log buffer than
  the new size. We could have asked for writing the data to
  disk, after x-locking the log buffer, but this could lead
  to deadlock if there was no space in log files and checkpoint
  was required (because checkpoint writes new redo records
  when persisting dd table buffer). That's why we don't ask
  for writing to disk. */
  ib::error(ER_IB_MSG_1256) << "Failed to change size of the log buffer."
                               " Try flushing the log buffer first.";
  return false;
}

void Sys_var_handler::writer_threads_update(bool value) {
  srv_log_writer_threads = value;
  /* pause/resume the log writer threads based on innodb_log_writer_threads
    value. */
  log_control_writer_threads(*log_sys);
}

bool Sys_var_handler::capacity_update(THD *thd, std::string_view var_name,
                                      uint64_t new_value) {
  static const uint64_t MB = 1024 * 1024;
  ut_a(LOG_CAPACITY_MIN <= new_value);
  ut_a(new_value <= LOG_CAPACITY_MAX);
  ut_a(new_value % MB == 0);

  if (srv_read_only_mode) {
    my_error(ER_CANT_CHANGE_SYS_VAR_IN_READ_ONLY_MODE, MYF(0), var_name.data());
    return false;
  }

  srv_redo_log_capacity = new_value;

  if (new_value == srv_redo_log_capacity_used) {
    return false;
  }

  srv_redo_log_capacity_used = new_value;
  ib::info(ER_IB_MSG_LOG_FILES_CAPACITY_CHANGED,
           srv_redo_log_capacity_used / MB);
  /* This blocks until log files governor does :
  log.m_capacity.update(log.m_files, logical_size, checkpoint_age);
  which updates the log_sys->m_capacity.soft_logical_capacity(), which is
  crucial for correct computation of update_free_check_limit() */

  log_files_resize_requested(*log_sys);

  IB_mutex_guard guard{&log_checkpointing->limits_mutex, UT_LOCATION_HERE};
  if (!m_handler.update_free_check_limit()) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
        "Current %s is too small for safety of redo log files."
        " Consider increasing it or decreasing innodb_thread_concurrency.",
        var_name.data());
  }

  return true;
}

bool Sys_var_handler::log_write_ahead_size_update(THD *thd,
                                                  std::string_view var_name,
                                                  uint64_t in_val) {
  uint64_t val = INNODB_LOG_WRITE_AHEAD_SIZE_MIN;

  while (val < in_val) {
    val = val * 2;
  }
  if (val > INNODB_LOG_WRITE_AHEAD_SIZE_MAX) {
    val = INNODB_LOG_WRITE_AHEAD_SIZE_MAX;
  }

  if (val > UNIV_PAGE_SIZE) {
    val = UNIV_PAGE_SIZE;
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                        "%s cannot be set higher than innodb_page_size.",
                        var_name.data());
  } else if (val != in_val) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                        "%s should be set to power of 2,"
                        " in range [%lu," ULINTPF "]",
                        var_name.data(), INNODB_LOG_WRITE_AHEAD_SIZE_MIN,
                        INNODB_LOG_WRITE_AHEAD_SIZE_MAX);
  }

  if (val != in_val) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                        "Setting %s to %" PRIu64 "", var_name.data(), val);
  }
  log_write_ahead_resize(*log_sys, static_cast<size_t>(val));
  return true;
}
}  // namespace ib::redo
