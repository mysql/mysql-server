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

#include "components/library_mysys/my_system_api/system_info_platform.h"

namespace mysql::system_info::internal {

Filesystem_snapshot query_filesystem_platform(std::string_view) { return {}; }

Storage_snapshot query_storage_devices_platform() { return {}; }

Host_memory_snapshot query_host_memory_platform() { return {}; }

Host_cpu_snapshot query_host_cpu_platform() { return {}; }

Process_memory_snapshot query_process_memory_platform() { return {}; }

Process_threads_snapshot query_process_threads_platform() { return {}; }

}  // namespace mysql::system_info::internal
