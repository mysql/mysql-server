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
using mysql::system_info::Swap_activity_info;
using mysql::system_info::Swap_capacity_info;
using mysql::system_info::System_result;

Host_memory_snapshot complete_snapshot() {
  Host_memory_snapshot snapshot;
  snapshot.memory = System_result<Host_memory_info>::success({1000, 100, 300});
  snapshot.breakdown =
      System_result<Host_memory_breakdown>::success({50, 150, 100});
  snapshot.swap_capacity =
      System_result<Swap_capacity_info>::success({400, 100});
  snapshot.swap_activity =
      System_result<Swap_activity_info>::success({500, 600});
  return snapshot;
}

TEST(HealthMonitorMemoryAdapter, MapsCompleteSnapshot) {
  System_Memory_Stats output;

  ASSERT_TRUE(convert_host_memory_snapshot(complete_snapshot(), 42, output));
  EXPECT_EQ(output.m_timestamp, 42);
  EXPECT_EQ(output.m_total_bytes, 1000U);
  EXPECT_EQ(output.m_free_bytes, 100U);
  EXPECT_EQ(output.m_avail_bytes, 300U);
  EXPECT_EQ(output.m_used_bytes, 600U);
  EXPECT_EQ(output.m_buffers_bytes, 50U);
  EXPECT_EQ(output.m_cached_bytes, 150U);
  EXPECT_EQ(output.m_slab_bytes, 100U);
  EXPECT_TRUE(output.m_breakdown_available);
  EXPECT_EQ(output.m_swap_total_bytes, 400U);
  EXPECT_EQ(output.m_swap_free_bytes, 100U);
  EXPECT_EQ(output.m_swap_used_bytes, 300U);
  EXPECT_TRUE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_in, 500U);
  EXPECT_EQ(output.m_swap_out, 600U);
  EXPECT_TRUE(output.m_swap_activity_available);
}

TEST(HealthMonitorMemoryAdapter, KeepsAvailableZeroValues) {
  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::success({0, 0, 0});
  snapshot.swap_capacity = System_result<Swap_capacity_info>::success({0, 0});
  snapshot.swap_activity = System_result<Swap_activity_info>::success({0, 0});
  System_Memory_Stats output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_TRUE(output.m_breakdown_available);
  EXPECT_EQ(output.m_buffers_bytes, 0U);
  EXPECT_TRUE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_total_bytes, 0U);
  EXPECT_TRUE(output.m_swap_activity_available);
  EXPECT_EQ(output.m_swap_in, 0U);
}

TEST(HealthMonitorMemoryAdapter, AcceptsUnavailableOptionalCapabilities) {
  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::unavailable();
  snapshot.swap_capacity = System_result<Swap_capacity_info>::unavailable();
  snapshot.swap_activity = System_result<Swap_activity_info>::unavailable();
  System_Memory_Stats output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_used_bytes, 700U);
  EXPECT_FALSE(output.m_breakdown_available);
  EXPECT_FALSE(output.m_swap_capacity_available);
  EXPECT_FALSE(output.m_swap_activity_available);
}

TEST(HealthMonitorMemoryAdapter, AcceptsFailedOptionalCapabilities) {
  auto snapshot = complete_snapshot();
  const auto error = std::make_error_code(std::errc::io_error);
  snapshot.breakdown = System_result<Host_memory_breakdown>::failure(error);
  snapshot.swap_capacity = System_result<Swap_capacity_info>::failure(error);
  snapshot.swap_activity = System_result<Swap_activity_info>::failure(error);
  System_Memory_Stats output;

  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_used_bytes, 700U);
  EXPECT_FALSE(output.m_breakdown_available);
  EXPECT_FALSE(output.m_swap_capacity_available);
  EXPECT_FALSE(output.m_swap_activity_available);
}

TEST(HealthMonitorMemoryAdapter, RejectsInvalidRequiredMemoryWithoutMutation) {
  auto snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::unavailable();
  System_Memory_Stats output;
  output.m_timestamp = 99;

  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::failure(
      std::make_error_code(std::errc::io_error));
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::success({100, 101, 50});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::success({0, 0, 0});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);

  snapshot = complete_snapshot();
  snapshot.memory = System_result<Host_memory_info>::success({100, 50, 101});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);
}

TEST(HealthMonitorMemoryAdapter, RejectsUnderflowingAvailableCapabilities) {
  auto snapshot = complete_snapshot();
  snapshot.breakdown =
      System_result<Host_memory_breakdown>::success({500, 500, 500});
  System_Memory_Stats output;
  output.m_timestamp = 99;

  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);

  snapshot = complete_snapshot();
  snapshot.swap_capacity =
      System_result<Swap_capacity_info>::success({100, 101});
  EXPECT_FALSE(convert_host_memory_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.m_timestamp, 99);
}

TEST(HealthMonitorMemoryAdapter, RepeatedConversionDoesNotRetainState) {
  System_Memory_Stats output;
  ASSERT_TRUE(convert_host_memory_snapshot(complete_snapshot(), 1, output));
  EXPECT_TRUE(output.m_breakdown_available);
  EXPECT_TRUE(output.m_swap_capacity_available);
  EXPECT_TRUE(output.m_swap_activity_available);

  auto snapshot = complete_snapshot();
  snapshot.breakdown = System_result<Host_memory_breakdown>::unavailable();
  snapshot.swap_capacity = System_result<Swap_capacity_info>::unavailable();
  snapshot.swap_activity = System_result<Swap_activity_info>::unavailable();
  ASSERT_TRUE(convert_host_memory_snapshot(snapshot, 2, output));
  EXPECT_EQ(output.m_timestamp, 2);
  EXPECT_FALSE(output.m_breakdown_available);
  EXPECT_EQ(output.m_buffers_bytes, 0U);
  EXPECT_FALSE(output.m_swap_capacity_available);
  EXPECT_EQ(output.m_swap_total_bytes, 0U);
  EXPECT_FALSE(output.m_swap_activity_available);
  EXPECT_EQ(output.m_swap_in, 0U);
}

}  // namespace
}  // namespace hm
