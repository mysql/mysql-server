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

#include <chrono>
#include <cstdint>
#include <string_view>
#include <system_error>

#include "mysql/components/library_mysys/system_info.h"

namespace mysql::system_info::internal {

/** Collect filesystem information for the active platform. */
[[nodiscard]] Filesystem_snapshot query_filesystem_platform(
    std::string_view path);

/** Collect storage device information for the active platform. */
[[nodiscard]] Storage_snapshot query_storage_devices_platform();

/** Collect host memory information for the active platform. */
[[nodiscard]] Host_memory_snapshot query_host_memory_platform();

/** Collect host CPU information for the active platform. */
[[nodiscard]] Host_cpu_snapshot query_host_cpu_platform();

/** Collect process memory information for the active platform. */
[[nodiscard]] Process_memory_snapshot query_process_memory_platform();

/** Collect process thread information for the active platform. */
[[nodiscard]] Process_threads_snapshot query_process_threads_platform();

/**
  Convert cumulative CPU ticks to whole milliseconds.

  @param[in] ticks Cumulative CPU ticks
  @param[in] ticks_per_second Number of CPU ticks per second
  @param[out] milliseconds Converted whole milliseconds, unchanged on error
  @retval false Success
  @retval true Error
*/
[[nodiscard]] bool cpu_ticks_to_milliseconds(
    uint64_t ticks, uint64_t ticks_per_second,
    std::chrono::milliseconds &milliseconds);

#ifdef __APPLE__
/** Create an error code in the native Mach error domain. */
[[nodiscard]] std::error_code mach_error_code(int error);
#endif

/**
  Reject invalid available filesystem usage values.

  The function transforms its snapshot argument and returns the transformed
  value. Passing an lvalue preserves the original. Passing a moved value avoids
  a copy.
*/
[[nodiscard]] Filesystem_snapshot validate_filesystem_snapshot(
    Filesystem_snapshot snapshot);

/**
  Reject invalid available storage device values.
*/
[[nodiscard]] Storage_snapshot validate_storage_snapshot(
    Storage_snapshot snapshot);

/**
  Reject invalid available host memory values.
*/
[[nodiscard]] Host_memory_snapshot validate_host_memory_snapshot(
    Host_memory_snapshot snapshot);

/**
  Reject invalid available host CPU values.
*/
[[nodiscard]] Host_cpu_snapshot validate_host_cpu_snapshot(
    Host_cpu_snapshot snapshot);

/**
  Reject invalid available process identity values.
*/
[[nodiscard]] Process_memory_snapshot validate_process_memory_snapshot(
    Process_memory_snapshot snapshot);

/**
  Reject invalid available process thread values.
*/
[[nodiscard]] Process_threads_snapshot validate_process_threads_snapshot(
    Process_threads_snapshot snapshot);

}  // namespace mysql::system_info::internal
