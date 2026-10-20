/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#pragma once

#include <mysql/components/library_mysys/system_info.h>

#include <ctime>
#include <vector>

#include "health_monitor_system_bits.h"

namespace hm {

/**
  Convert a host CPU snapshot into Health Monitor CPU records.

  CPU times in Cpu_times_snapshot are required. Extended CPU times are optional
  and will be represented as NULL in the table. For example, extended CPU times
  are unavailable on macOS because it has no supported equivalent.

  @param[in] snapshot CPU snapshot to convert
  @param[in] timestamp Timestamp for every converted record
  @param[in,out] output Destination records, unchanged on failure
  @return true on success, false on failure
*/
bool convert_host_cpu_snapshot(
    const mysql::system_info::Host_cpu_snapshot &snapshot, time_t timestamp,
    std::vector<System_CPU_Stats> &output);

}  // namespace hm
