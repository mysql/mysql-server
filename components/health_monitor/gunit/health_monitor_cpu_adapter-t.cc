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

#include <chrono>
#include <system_error>
#include <vector>

#include "../health_monitor_cpu_mapping.h"

namespace hm {
namespace {

using mysql::system_info::Cpu_extended_snapshot;
using mysql::system_info::Cpu_extended_times;
using mysql::system_info::Cpu_times;
using mysql::system_info::Cpu_times_snapshot;
using mysql::system_info::Host_cpu_snapshot;
using mysql::system_info::System_result;
using namespace std::chrono_literals;

Host_cpu_snapshot complete_snapshot() {
  Host_cpu_snapshot snapshot;
  snapshot.times = System_result<Cpu_times_snapshot>::success(
      {{{"cpu", 1ms, 2ms, 3ms, 4ms}, {"cpu0", 5ms, 6ms, 7ms, 8ms}}});
  snapshot.extended_times = System_result<Cpu_extended_snapshot>::success(
      {{{"cpu", 9ms, 10ms, 11ms, 12ms, 13ms, 14ms},
        {"cpu0", 15ms, 16ms, 17ms, 18ms, 19ms, 20ms}}});
  return snapshot;
}

TEST(HealthMonitorCpuAdapter, MapsCompleteSnapshot) {
  std::vector<System_CPU_Stats> output;
  ASSERT_TRUE(convert_host_cpu_snapshot(complete_snapshot(), 42, output));
  ASSERT_EQ(output.size(), 2U);

  EXPECT_EQ(output[0].m_timestamp, 42);
  EXPECT_EQ(output[0].m_cpu, "cpu");
  EXPECT_EQ(output[0].m_user_ms, 1U);
  EXPECT_EQ(output[0].m_nice_ms, 2U);
  EXPECT_EQ(output[0].m_system_ms, 3U);
  EXPECT_EQ(output[0].m_idle_ms, 4U);
  EXPECT_EQ(output[0].m_iowait_ms, 9U);
  EXPECT_EQ(output[0].m_irq_ms, 10U);
  EXPECT_EQ(output[0].m_softirq_ms, 11U);
  EXPECT_EQ(output[0].m_steal_ms, 12U);
  EXPECT_EQ(output[0].m_guest_ms, 13U);
  EXPECT_EQ(output[0].m_guest_nice_ms, 14U);
  EXPECT_TRUE(output[0].m_extended_times_available);

  EXPECT_EQ(output[1].m_timestamp, 42);
  EXPECT_EQ(output[1].m_cpu, "cpu0");
  EXPECT_EQ(output[1].m_user_ms, 5U);
  EXPECT_EQ(output[1].m_guest_nice_ms, 20U);
  EXPECT_TRUE(output[1].m_extended_times_available);
}

TEST(HealthMonitorCpuAdapter, KeepsAvailableZeroExtendedCounters) {
  auto snapshot = complete_snapshot();
  snapshot.extended_times = System_result<Cpu_extended_snapshot>::success(
      {{{"cpu", 0ms, 0ms, 0ms, 0ms, 0ms, 0ms},
        {"cpu0", 0ms, 0ms, 0ms, 0ms, 0ms, 0ms}}});
  std::vector<System_CPU_Stats> output;

  ASSERT_TRUE(convert_host_cpu_snapshot(snapshot, 1, output));
  ASSERT_EQ(output.size(), 2U);
  EXPECT_TRUE(output[0].m_extended_times_available);
  EXPECT_EQ(output[0].m_iowait_ms, 0U);
  EXPECT_TRUE(output[1].m_extended_times_available);
}

TEST(HealthMonitorCpuAdapter, AcceptsUnavailableExtendedCapability) {
  auto snapshot = complete_snapshot();
  snapshot.extended_times = System_result<Cpu_extended_snapshot>::unavailable();
  std::vector<System_CPU_Stats> output;

  ASSERT_TRUE(convert_host_cpu_snapshot(snapshot, 1, output));
  ASSERT_EQ(output.size(), 2U);
  EXPECT_FALSE(output[0].m_extended_times_available);
  EXPECT_FALSE(output[1].m_extended_times_available);
}

TEST(HealthMonitorCpuAdapter, AcceptsFailedExtendedCapability) {
  auto snapshot = complete_snapshot();
  snapshot.extended_times = System_result<Cpu_extended_snapshot>::failure(
      std::make_error_code(std::errc::io_error));
  std::vector<System_CPU_Stats> output;

  ASSERT_TRUE(convert_host_cpu_snapshot(snapshot, 1, output));
  ASSERT_EQ(output.size(), 2U);
  EXPECT_FALSE(output[0].m_extended_times_available);
}

TEST(HealthMonitorCpuAdapter, RejectsUnavailableRequiredCapability) {
  Host_cpu_snapshot snapshot;
  std::vector<System_CPU_Stats> output(1);

  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorCpuAdapter, RejectsFailedRequiredCapability) {
  Host_cpu_snapshot snapshot;
  snapshot.times = System_result<Cpu_times_snapshot>::failure(
      std::make_error_code(std::errc::io_error));
  std::vector<System_CPU_Stats> output(1);

  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorCpuAdapter, RejectsEmptyRequiredCapability) {
  Host_cpu_snapshot snapshot;
  snapshot.times = System_result<Cpu_times_snapshot>::success({});
  std::vector<System_CPU_Stats> output(1);

  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorCpuAdapter, RejectsMisalignedExtendedCapability) {
  auto snapshot = complete_snapshot();
  snapshot.extended_times.value().cpus[1].cpu = "cpu1";
  std::vector<System_CPU_Stats> output(1);

  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.size(), 1U);

  snapshot = complete_snapshot();
  snapshot.extended_times.value().cpus.pop_back();
  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorCpuAdapter, RejectsNegativeCountersWithoutOutputMutation) {
  auto snapshot = complete_snapshot();
  snapshot.times.value().cpus[1].idle = -1ms;
  std::vector<System_CPU_Stats> output(1);
  output[0].m_cpu = "existing";

  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_cpu, "existing");

  snapshot = complete_snapshot();
  snapshot.extended_times.value().cpus[1].irq = -1ms;
  EXPECT_FALSE(convert_host_cpu_snapshot(snapshot, 1, output));
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_cpu, "existing");
}

TEST(HealthMonitorCpuAdapter, RepeatedConversionDoesNotRetainExtendedState) {
  std::vector<System_CPU_Stats> output;
  ASSERT_TRUE(convert_host_cpu_snapshot(complete_snapshot(), 1, output));
  ASSERT_EQ(output.size(), 2U);
  EXPECT_TRUE(output[0].m_extended_times_available);

  auto snapshot = complete_snapshot();
  snapshot.extended_times = System_result<Cpu_extended_snapshot>::unavailable();
  output.clear();
  ASSERT_TRUE(convert_host_cpu_snapshot(snapshot, 2, output));
  ASSERT_EQ(output.size(), 2U);
  EXPECT_FALSE(output[0].m_extended_times_available);
  EXPECT_EQ(output[0].m_iowait_ms, 0U);
}

}  // namespace
}  // namespace hm
