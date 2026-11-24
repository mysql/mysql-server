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

#pragma once

#include "my_config.h"
#include "my_inttypes.h"
#include "sql/set_var.h"

class handlerton;

namespace myduck {
extern ulonglong global_mode;
extern const char *mode_names[];
enum enum_modes { DUCKDB_NONE = 0, DUCKDB_ON = 1 };

extern ulonglong global_memory_limit;
extern const char *global_duckdb_temp_directory;
extern ulonglong global_max_temp_directory_size;
extern ulonglong global_max_threads;
extern ulonglong max_threads_per_query;
extern bool global_use_dio;
extern bool global_scheduler_process_partial;
extern bool global_enable_background_checkpoint;
extern ulonglong global_background_checkpoint_full_interval;
extern ulonglong appender_allocator_flush_threshold;
extern ulonglong checkpoint_threshold;

extern bool use_double_for_decimal;

extern ulonglong duckdb_max_threads_per_query_rpl;

inline bool global_mode_on() {
#ifdef HAVE_DUCKDB
  return global_mode == DUCKDB_ON;
#else
  return false;
#endif
}

/** Whether to enable duckdb storage engine */
bool is_disabled(const handlerton &ht);

bool update_memory_limit(sys_var *sys_var [[maybe_unused]], THD *thd,
                         enum_var_type type [[maybe_unused]]);

bool update_max_temp_directory_size(sys_var *sys_var [[maybe_unused]], THD *thd,
                                    enum_var_type type [[maybe_unused]]);

bool update_threads(sys_var *sys_var [[maybe_unused]], THD *thd,
                    enum_var_type type [[maybe_unused]]);

bool update_scheduler_process_partial(sys_var *sys_var [[maybe_unused]],
                                      THD *thd,
                                      enum_var_type type [[maybe_unused]]);

bool update_enable_background_checkpoint(sys_var *sys_var [[maybe_unused]],
                                         THD *thd,
                                         enum_var_type type [[maybe_unused]]);

bool update_background_checkpoint_full_interval(sys_var *sys_var
                                                [[maybe_unused]],
                                                THD *thd,
                                                enum_var_type type
                                                [[maybe_unused]]);

bool update_checkpoint_threshold(sys_var *sys_var [[maybe_unused]], THD *thd,
                                 enum_var_type type [[maybe_unused]]);
}  // namespace myduck
