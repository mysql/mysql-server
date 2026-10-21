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

#include <system_error>

#include "../health_monitor_memory_mapping.h"

namespace hm {
namespace {

using mysql::system_info::Host_memory_breakdown;
using mysql::system_info::Host_memory_info;
using mysql::system_info::Host_memory_snapshot;
using mysql::system_info::Swap_capacity_info;
using mysql::system_info::System_result;

Host_memory_snapshot complete_snapshot() {
  Host_memory_snapshot snapshot;
  snapshot.memory = System_result<Host_memory_info>::success({1000, 100, 300});
  snapshot.breakdown =
      System_result<Host_memory_breakdown>::success({50, 150, 100});
  snapshot.swap_capacity =
      System_result<Swap_capacity_info>::success({400, 100});
  return snapshot;
}

TEST(HealthMonitorHostMemoryAdapter, MapsCompleteSnapshot) {
  System_Memory_Info output;

  ASSERT_TRUE(convert_host_memory_snapshot(complete_snapshot(), output));
  EXPECT_EQ(output.m_total_bytes, 1000U);
  EXPECT_EQ(output.m_free_bytes, 100U);
  EXPECT_EQ(output.m_avail_bytes, 300U);
  EXPECT_EQ(output.m_cached_bytes, 150U);
  EXPECT_TRUE(output.m_cache_available);
  EXPECT_EQ(output.m_swap_total_bytes, 400U);
  EXPECT_EQ(output.m_swap_free_bytes, 100U);
  EXPECT_TRUE(output.m_swap_capacity_available);
}

TEST(HealthMonitorHostMemoryAdapter, KeepsAvailableZeroValues) {
  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::success({0, 0, 0});
  snapshot.swap_capacity = System_result<Swap_capacity_info>::success({0, 0});
  System_Memory_Info output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_TRUE(output.m_cache_available);
  EXPECT_EQ(output.m_cached_bytes, 0U);
  EXPECT_TRUE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_total_bytes, 0U);
  EXPECT_EQ(output.m_swap_free_bytes, 0U);
}

TEST(HealthMonitorHostMemoryAdapter, AcceptsUnavailableOptionalCapabilities) {
  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::unavailable();
  snapshot.swap_capacity = System_result<Swap_capacity_info>::unavailable();
  System_Memory_Info output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_FALSE(output.m_cache_available);
  EXPECT_EQ(output.m_cached_bytes, 0U);
  EXPECT_FALSE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_total_bytes, 0U);
}

TEST(HealthMonitorHostMemoryAdapter, AcceptsFailedOptionalCapabilities) {
  auto snapshot = complete_snapshot();
  const auto error = std::make_error_code(std::errc::io_error);
  snapshot.breakdown = System_result<Host_memory_breakdown>::failure(error);
  snapshot.swap_capacity = System_result<Swap_capacity_info>::failure(error);
  System_Memory_Info output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_FALSE(output.m_cache_available);
  EXPECT_FALSE(output.m_swap_capacity_available);
}

TEST(HealthMonitorHostMemoryAdapter,
     RejectsInvalidRequiredMemoryWithoutMutation) {
  System_Memory_Info output;
  output.m_total_bytes = 99;

  auto snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::unavailable();
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_total_bytes, 99U);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::failure(
      std::make_error_code(std::errc::io_error));
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_total_bytes, 99U);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::success({0, 0, 0});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_total_bytes, 99U);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::success({100, 101, 50});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_total_bytes, 99U);
}

TEST(HealthMonitorHostMemoryAdapter, RejectsInvalidSwapWithoutMutation) {
  auto snapshot = complete_snapshot();
  snapshot.swap_capacity = System_result<Swap_capacity_info>::success({1, 2});
  System_Memory_Info output;
  output.m_total_bytes = 99;

  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_total_bytes, 99U);
}

TEST(HealthMonitorHostMemoryAdapter, RepeatedConversionDoesNotRetainState) {
  System_Memory_Info output;
  ASSERT_TRUE(convert_host_memory_snapshot(complete_snapshot(), output));
  EXPECT_TRUE(output.m_cache_available);
  EXPECT_TRUE(output.m_swap_capacity_available);

  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::unavailable();
  snapshot.swap_capacity = System_result<Swap_capacity_info>::unavailable();
  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, output));
  EXPECT_FALSE(output.m_cache_available);
  EXPECT_EQ(output.m_cached_bytes, 0U);
  EXPECT_FALSE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_total_bytes, 0U);
}

TEST(HealthMonitorMemorySample, PublishesHostOnlySampleWithOneTimestamp) {
  Memory_Info sample;
  sample.sys.m_total_bytes = 1000;
  sample.sys.m_avail_bytes = 300;
  sample.proc.m_timestamp = 17;
  sample.m_process_memory_available = false;
  int process_writes = 0;
  int system_writes = 0;
  System_Memory_Info published_system;

  publish_memory_records(
      sample, 123,
      [&process_writes](const Process_Memory_Info &) { ++process_writes; },
      [&system_writes, &published_system](const System_Memory_Info &record) {
        ++system_writes;
        published_system = record;
      });

  EXPECT_EQ(process_writes, 0);
  EXPECT_EQ(system_writes, 1);
  EXPECT_EQ(sample.proc.m_timestamp, 17);
  EXPECT_EQ(sample.sys.m_timestamp, 123);
  EXPECT_EQ(published_system.m_timestamp, 123);
}

TEST(HealthMonitorMemorySample, PublishesBothRecordsWithOneTimestamp) {
  Memory_Info sample;
  sample.m_process_memory_available = true;
  time_t process_timestamp = 0;
  time_t system_timestamp = 0;

  publish_memory_records(
      sample, 456,
      [&process_timestamp](const Process_Memory_Info &record) {
        process_timestamp = record.m_timestamp;
      },
      [&system_timestamp](const System_Memory_Info &record) {
        system_timestamp = record.m_timestamp;
      });

  EXPECT_EQ(process_timestamp, 456);
  EXPECT_EQ(system_timestamp, 456);
}

TEST(HealthSystemMemoryTableAdapter, MapsNullCapabilitiesAndResetsState) {
  System_Memory_Info record;
  record.m_total_bytes = 1000;
  record.m_avail_bytes = 250;
  record.m_cached_bytes = 150;
  record.m_swap_total_bytes = 400;
  record.m_swap_free_bytes = 100;
  record.m_cache_available = true;
  record.m_swap_capacity_available = true;

  auto row = make_health_system_memory_row(record);
  EXPECT_FALSE(health_system_memory_column_is_null(row, 5));
  EXPECT_FALSE(health_system_memory_column_is_null(row, 6));
  EXPECT_FALSE(health_system_memory_column_is_null(row, 7));

  record.m_cached_bytes = 0;
  record.m_swap_total_bytes = 0;
  record.m_swap_free_bytes = 0;
  record.m_cache_available = false;
  record.m_swap_capacity_available = false;
  row = make_health_system_memory_row(record);

  EXPECT_EQ(row.m_cached_bytes, 0U);
  EXPECT_EQ(row.m_swap_total_bytes, 0U);
  EXPECT_EQ(row.m_swap_free_bytes, 0U);
  EXPECT_TRUE(health_system_memory_column_is_null(row, 5));
  EXPECT_TRUE(health_system_memory_column_is_null(row, 6));
  EXPECT_TRUE(health_system_memory_column_is_null(row, 7));
}

}  // namespace
}  // namespace hm
