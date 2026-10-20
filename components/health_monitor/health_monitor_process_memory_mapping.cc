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

#include "health_monitor_process_memory_mapping.h"

#include <cassert>

namespace hm {
namespace {

bool is_available(const mysql::system_info::Result_state state) {
  return state == mysql::system_info::Result_state::available;
}

}  // namespace

bool convert_process_memory_snapshot(
    const mysql::system_info::Process_memory_snapshot &snapshot,
    Process_Memory_Info &output) {
  if (!is_available(snapshot.identity.state()) ||
      !is_available(snapshot.residency.state())) {
    return false;
  }

  const auto &identity = snapshot.identity.value();
  if (identity.pid == 0 || identity.name.empty()) return false;

  const auto &residency = snapshot.residency.value();

  Process_Memory_Info converted;
  converted.m_name = identity.name;
  converted.m_pid = identity.pid;
  converted.m_vmrss_bytes = residency.resident_bytes;
  converted.m_residency_available = true;
  if (is_available(snapshot.details.state())) {
    const auto &details = snapshot.details.value();
    converted.m_vmdata_bytes = details.data_bytes;
    converted.m_vmswap_bytes = details.swap_bytes;
    converted.m_details_available = true;
  }
  if (is_available(snapshot.page_faults.state())) {
    converted.m_page_faults = snapshot.page_faults.value().major_faults;
    converted.m_page_faults_available = true;
  }
  output = std::move(converted);
  return true;
}

bool process_memory_snapshot_is_unavailable(
    const mysql::system_info::Process_memory_snapshot &snapshot) {
  using mysql::system_info::Result_state;
  const bool all_unavailable =
      snapshot.identity.state() == Result_state::unavailable &&
      snapshot.residency.state() == Result_state::unavailable &&
      snapshot.details.state() == Result_state::unavailable &&
      snapshot.page_faults.state() == Result_state::unavailable;
  return all_unavailable;
}

Health_process_memory_row make_health_process_memory_row(
    const Process_Memory_Info &record) {
  Health_process_memory_row row;
  row.m_timestamp = record.m_timestamp;
  row.m_name = record.m_name;
  row.m_pid = record.m_pid;
  row.m_vmrss_bytes = record.m_vmrss_bytes;
  row.m_vmdata_bytes = record.m_vmdata_bytes;
  row.m_vmswap_bytes = record.m_vmswap_bytes;
  row.m_page_faults = record.m_page_faults;
  row.m_residency_available = record.m_residency_available;
  row.m_details_available = record.m_details_available;
  row.m_page_faults_available = record.m_page_faults_available;
  return row;
}

bool health_process_memory_column_is_null(const Health_process_memory_row &row,
                                          const unsigned int index) {
  switch (index) {
    case 3:  // VM_RSS
      return !row.m_residency_available;
    case 4:  // VM_DATA
    case 5:  // VM_SWAP
      return !row.m_details_available;
    case 6:  // PAGE_FAULTS
      return !row.m_page_faults_available;
    default:
      assert(false);
      return false;
  }
}

}  // namespace hm
