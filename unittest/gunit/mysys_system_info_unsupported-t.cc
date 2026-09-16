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

#include <gtest/gtest.h>

#include <string_view>

#include "mysql/components/library_mysys/system_info.h"

namespace mysql::system_info {
namespace {

template <typename T>
void expect_unavailable(const System_result<T> &result) {
  EXPECT_EQ(Result_state::unavailable, result.state());
  EXPECT_FALSE(result.has_value());
}

void expect_unavailable(const Filesystem_snapshot &snapshot) {
  expect_unavailable(snapshot.identity);
  expect_unavailable(snapshot.usage);
}

void expect_unavailable(const Storage_snapshot &snapshot) {
  expect_unavailable(snapshot.inventory);
  expect_unavailable(snapshot.capacities);
  expect_unavailable(snapshot.hierarchy);
  expect_unavailable(snapshot.read_write);
  expect_unavailable(snapshot.flush);
}

void expect_unavailable(const Host_memory_snapshot &snapshot) {
  expect_unavailable(snapshot.memory);
  expect_unavailable(snapshot.breakdown);
  expect_unavailable(snapshot.swap_capacity);
  expect_unavailable(snapshot.swap_activity);
}

void expect_unavailable(const Host_cpu_snapshot &snapshot) {
  expect_unavailable(snapshot.times);
  expect_unavailable(snapshot.extended_times);
}

void expect_unavailable(const Process_memory_snapshot &snapshot) {
  expect_unavailable(snapshot.identity);
  expect_unavailable(snapshot.residency);
  expect_unavailable(snapshot.details);
  expect_unavailable(snapshot.page_faults);
}

void expect_unavailable(const Process_threads_snapshot &snapshot) {
  expect_unavailable(snapshot.identity);
  expect_unavailable(snapshot.threads);
  expect_unavailable(snapshot.runtime);
  expect_unavailable(snapshot.cpu);
  expect_unavailable(snapshot.extended_cpu);
  expect_unavailable(snapshot.scheduler);
  expect_unavailable(snapshot.io);
}

TEST(MysysSystemInfoUnsupported, AllCapabilitiesAreUnavailable) {
  expect_unavailable(query_filesystem(""));
  expect_unavailable(query_filesystem("/path/is/ignored"));
  expect_unavailable(query_storage_devices());
  expect_unavailable(query_host_memory());
  expect_unavailable(query_host_cpu());
  expect_unavailable(query_process_memory());
  expect_unavailable(query_process_threads());
}

TEST(MysysSystemInfoUnsupported, RepeatedQueriesRemainUnavailable) {
  for (int sample = 0; sample < 2; ++sample) {
    expect_unavailable(query_filesystem(std::string_view{}));
    expect_unavailable(query_storage_devices());
    expect_unavailable(query_host_memory());
    expect_unavailable(query_host_cpu());
    expect_unavailable(query_process_memory());
    expect_unavailable(query_process_threads());
  }
}

}  // namespace
}  // namespace mysql::system_info
