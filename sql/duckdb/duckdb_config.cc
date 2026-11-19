/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

#include "duckdb_config.h"
#ifdef HAVE_DUCKDB
#include "sql/duckdb/duckdb_query.h"
#endif
#include "sql/handler.h"

#include <ostream>

namespace myduck {
const char *mode_names[] = {"NONE", "ON", NullS};

ulonglong global_memory_limit = 0;

const char *global_duckdb_temp_directory = nullptr;

ulonglong global_max_temp_directory_size = 0;

ulonglong global_max_threads = 0;

ulonglong max_threads_per_query = 1000000;

ulonglong global_mode = 0;

bool global_use_dio = false;

bool global_scheduler_process_partial = true;

bool global_enable_background_checkpoint = false;

ulonglong global_background_checkpoint_full_interval = 0;

ulonglong appender_allocator_flush_threshold = 0;

ulonglong checkpoint_threshold = 268435456;

bool use_double_for_decimal;

ulonglong duckdb_max_threads_per_query_rpl = 1000000;

bool is_disabled(const handlerton &ht) {
  return (ht.db_type == DB_TYPE_DUCKDB && global_mode != DUCKDB_ON);
}

#ifdef HAVE_DUCKDB
bool update_memory_limit(sys_var *sys_var [[maybe_unused]], THD *thd,
                         enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;
  if (global_memory_limit == 0) {
    oss << "RESET memory_limit";
  } else {
    oss << "SET memory_limit = '";
    oss << BytesToHumanReadableString(global_memory_limit);
    oss << "'";
  }

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_max_temp_directory_size(sys_var *sys_var [[maybe_unused]], THD *thd,
                                    enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;

  if (global_max_temp_directory_size == 0) {
    oss << "RESET GLOBAL max_temp_directory_size";
  } else {
    oss << "SET GLOBAL max_temp_directory_size = '";
    oss << BytesToHumanReadableString(global_max_temp_directory_size);
    oss << "'";
  }

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_threads(sys_var *sys_var [[maybe_unused]], THD *thd,
                    enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;
  if (global_max_threads == 0) {
    oss << "RESET GLOBAL threads";
  } else {
    oss << "SET GLOBAL threads = " << global_max_threads;
  }

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_scheduler_process_partial(sys_var *sys_var [[maybe_unused]],
                                      THD *thd,
                                      enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;
  oss << "SET scheduler_process_partial = ";
  if (global_scheduler_process_partial) {
    oss << "true";
  } else {
    oss << "false";
  }

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_enable_background_checkpoint(sys_var *sys_var [[maybe_unused]],
                                         THD *thd,
                                         enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;
  oss << "SET GLOBAL enable_background_checkpoint = ";
  if (global_enable_background_checkpoint) {
    oss << "true";
  } else {
    oss << "false";
  }

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_background_checkpoint_full_interval(sys_var *sys_var
                                                [[maybe_unused]],
                                                THD *thd,
                                                enum_var_type type
                                                [[maybe_unused]]) {
  std::ostringstream oss;
  oss << "SET GLOBAL background_checkpoint_full_interval = "
      << global_background_checkpoint_full_interval;

  return duckdb_query_and_send(thd, oss.str(), false, true);
}

bool update_checkpoint_threshold(sys_var *sys_var [[maybe_unused]], THD *thd,
                                 enum_var_type type [[maybe_unused]]) {
  std::ostringstream oss;
  oss << "SET GLOBAL checkpoint_threshold = '";
  oss << BytesToHumanReadableString(checkpoint_threshold);
  oss << "'";

  return duckdb_query_and_send(thd, oss.str(), false, true);
}
#endif  // HAVE_DUCKDB
}  // namespace myduck
