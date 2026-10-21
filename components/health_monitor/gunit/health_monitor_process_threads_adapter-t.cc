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

#include <array>
#include <chrono>
#include <system_error>

#include "../health_monitor_process_stats_mapping.h"

namespace hm {
namespace {

using mysql::system_info::Process_identity;
using mysql::system_info::Process_runtime_info;
using mysql::system_info::Process_threads_snapshot;
using mysql::system_info::System_result;
using mysql::system_info::Thread_cpu_snapshot;
using mysql::system_info::Thread_extended_cpu_snapshot;
using mysql::system_info::Thread_inventory;
using mysql::system_info::Thread_io_snapshot;
using mysql::system_info::Thread_scheduler_snapshot;
using mysql::system_info::Thread_state;

using Conversion_result = Process_threads_conversion_result;

Process_threads_snapshot complete_snapshot() {
  Process_threads_snapshot snapshot;
  snapshot.identity = System_result<Process_identity>::success({42, "mysqld"});
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{42, "mysqld", Thread_state::running},
        {43, "connection", Thread_state::sleeping}}});
  snapshot.runtime =
      System_result<Process_runtime_info>::success({2, 100, 200, 300});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{42, std::chrono::milliseconds{10}, std::chrono::milliseconds{20}},
        {43, std::chrono::milliseconds{30}, std::chrono::milliseconds{40}}}});
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{42, std::chrono::milliseconds{50}, std::chrono::milliseconds{60},
         std::chrono::milliseconds{70}, std::chrono::milliseconds{80}},
        {43, std::chrono::milliseconds{90}, std::chrono::milliseconds{100},
         std::chrono::milliseconds{110}, std::chrono::milliseconds{120}}}});
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::success(
      {{{42, 1, std::chrono::milliseconds{130}},
        {43, 2, std::chrono::milliseconds{140}}}});
  snapshot.io = System_result<Thread_io_snapshot>::success(
      {{{42, 150, 160}, {43, 170, 180}}});
  return snapshot;
}

Process_threads_snapshot macos_snapshot() {
  auto snapshot = complete_snapshot();
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{1001, "worker", Thread_state::running},
        {1002, "connection", Thread_state::sleeping}}});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{1001, std::chrono::milliseconds{10}, std::chrono::milliseconds{20}},
        {1002, std::chrono::milliseconds{30}, std::chrono::milliseconds{40}}}});
  snapshot.runtime = System_result<Process_runtime_info>::unavailable();
  snapshot.extended_cpu =
      System_result<Thread_extended_cpu_snapshot>::unavailable();
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::unavailable();
  snapshot.io = System_result<Thread_io_snapshot>::unavailable();
  return snapshot;
}

TEST(HealthMonitorProcessThreadsAdapter, MapsCompleteSnapshot) {
  std::vector<System_Process_Stats> output;

  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(complete_snapshot(), 99, output));
  ASSERT_EQ(2U, output.size());
  const auto &process = output[0];
  EXPECT_EQ(99, process.m_timestamp);
  EXPECT_EQ(42U, process.m_pid);
  EXPECT_EQ("mysqld", process.m_process_name);
  EXPECT_EQ(42U, process.m_tid);
  EXPECT_EQ("mysqld", process.m_thread_name);
  EXPECT_EQ('R', process.m_state);
  EXPECT_EQ(10U, process.m_utime_ms);
  EXPECT_EQ(20U, process.m_stime_ms);
  EXPECT_EQ(50U, process.m_cutime_ms);
  EXPECT_EQ(60U, process.m_cstime_ms);
  EXPECT_EQ(2U, process.m_num_threads);
  EXPECT_EQ(100U, process.m_vsize_bytes);
  EXPECT_EQ(200U, process.m_rss_bytes);
  EXPECT_EQ(300U, process.m_rsslim_bytes);
  EXPECT_EQ(1U, process.m_processor);
  EXPECT_EQ(130U, process.m_delayacct_blkio_ms);
  EXPECT_EQ(70U, process.m_guest_time_ms);
  EXPECT_EQ(80U, process.m_cguest_time_ms);
  EXPECT_EQ(150U, process.m_read_bytes);
  EXPECT_EQ(160U, process.m_write_bytes);
  EXPECT_TRUE(process.m_cpu_available);
  EXPECT_TRUE(process.m_runtime_available);
  EXPECT_TRUE(process.m_extended_cpu_available);
  EXPECT_TRUE(process.m_scheduler_available);
  EXPECT_TRUE(process.m_io_available);

  const auto &thread = output[1];
  EXPECT_EQ(43U, thread.m_tid);
  EXPECT_EQ('S', thread.m_state);
  EXPECT_EQ(30U, thread.m_utime_ms);
  EXPECT_EQ(40U, thread.m_stime_ms);
  EXPECT_EQ(90U, thread.m_cutime_ms);
  EXPECT_EQ(100U, thread.m_cstime_ms);
  EXPECT_EQ(0U, thread.m_num_threads);
  EXPECT_EQ(0U, thread.m_vsize_bytes);
  EXPECT_EQ(0U, thread.m_rss_bytes);
  EXPECT_EQ(0U, thread.m_rsslim_bytes);
  EXPECT_EQ(2U, thread.m_processor);
  EXPECT_EQ(0U, thread.m_delayacct_blkio_ms);
  EXPECT_EQ(110U, thread.m_guest_time_ms);
  EXPECT_EQ(120U, thread.m_cguest_time_ms);
  EXPECT_EQ(170U, thread.m_read_bytes);
  EXPECT_EQ(180U, thread.m_write_bytes);
  EXPECT_TRUE(thread.m_cpu_available);
  EXPECT_FALSE(thread.m_runtime_available);
  EXPECT_TRUE(thread.m_extended_cpu_available);
  EXPECT_TRUE(thread.m_scheduler_available);
  EXPECT_TRUE(thread.m_io_available);
}

TEST(HealthMonitorProcessThreadsAdapter, KeepsAvailableZeroValues) {
  auto snapshot = complete_snapshot();
  snapshot.runtime = System_result<Process_runtime_info>::success({0, 0, 0, 0});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{42, {}, {}}, {43, {}, {}}}});
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{42, {}, {}, {}, {}}, {43, {}, {}, {}, {}}}});
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::success(
      {{{42, 0, {}}, {43, 0, {}}}});
  snapshot.io =
      System_result<Thread_io_snapshot>::success({{{42, 0, 0}, {43, 0, 0}}});
  std::vector<System_Process_Stats> output;

  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ(0U, output[0].m_num_threads);
  EXPECT_EQ(0U, output[0].m_utime_ms);
  EXPECT_EQ(0U, output[0].m_read_bytes);
  EXPECT_EQ(0U, output[1].m_delayacct_blkio_ms);
}

TEST(HealthMonitorProcessThreadsAdapter,
     MapsMacosStyleSnapshotWithoutProcessLeader) {
  std::vector<System_Process_Stats> output;

  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(macos_snapshot(), 99, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ(1001U, output[0].m_tid);
  EXPECT_EQ(10U, output[0].m_utime_ms);
  EXPECT_EQ(20U, output[0].m_stime_ms);
  EXPECT_EQ(1002U, output[1].m_tid);
  EXPECT_EQ(30U, output[1].m_utime_ms);
  EXPECT_EQ(40U, output[1].m_stime_ms);
  for (const auto &record : output) {
    EXPECT_TRUE(record.m_cpu_available);
    EXPECT_FALSE(record.m_runtime_available);
    EXPECT_FALSE(record.m_extended_cpu_available);
    EXPECT_FALSE(record.m_scheduler_available);
    EXPECT_FALSE(record.m_io_available);
  }
}

TEST(HealthMonitorProcessThreadsAdapter,
     RejectsUnavailableAndFailedRequiredCapabilitiesWithoutMutation) {
  std::vector<System_Process_Stats> output(1);
  output.front().m_pid = 99;

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::unavailable();
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));
  EXPECT_EQ(1U, output.size());
  EXPECT_EQ(99U, output.front().m_pid);

  snapshot = complete_snapshot();
  snapshot.threads = System_result<Thread_inventory>::unavailable();
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));
  snapshot = complete_snapshot();
  snapshot.cpu = System_result<Thread_cpu_snapshot>::unavailable();
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  const auto error = std::make_error_code(std::errc::io_error);
  snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::failure(error);
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.threads = System_result<Thread_inventory>::failure(error);
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.cpu = System_result<Thread_cpu_snapshot>::failure(error);
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));
  EXPECT_EQ(1U, output.size());
  EXPECT_EQ(99U, output.front().m_pid);
}

TEST(HealthMonitorProcessThreadsAdapter,
     OptionalCapabilityStatesRemainIndependent) {
  const auto error = std::make_error_code(std::errc::io_error);
  std::vector<System_Process_Stats> output;
  auto snapshot = complete_snapshot();
  snapshot.runtime = System_result<Process_runtime_info>::unavailable();
  snapshot.extended_cpu =
      System_result<Thread_extended_cpu_snapshot>::unavailable();
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::unavailable();
  snapshot.io = System_result<Thread_io_snapshot>::unavailable();

  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_FALSE(output[0].m_runtime_available);
  EXPECT_FALSE(output[0].m_extended_cpu_available);
  EXPECT_FALSE(output[0].m_scheduler_available);
  EXPECT_FALSE(output[0].m_io_available);

  output.clear();
  snapshot = complete_snapshot();
  snapshot.io = System_result<Thread_io_snapshot>::failure(error);
  ASSERT_EQ(Conversion_result::degraded,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_TRUE(output[0].m_runtime_available);
  EXPECT_TRUE(output[0].m_extended_cpu_available);
  EXPECT_TRUE(output[0].m_scheduler_available);
  EXPECT_FALSE(output[0].m_io_available);
}

TEST(HealthMonitorProcessThreadsAdapter,
     FailedOptionalCapabilitiesDegradeIndependently) {
  const auto error = std::make_error_code(std::errc::io_error);
  std::vector<System_Process_Stats> output;
  auto snapshot = complete_snapshot();
  snapshot.runtime = System_result<Process_runtime_info>::failure(error);
  ASSERT_EQ(Conversion_result::degraded,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_FALSE(output[0].m_runtime_available);
  EXPECT_TRUE(output[0].m_extended_cpu_available);
  EXPECT_TRUE(output[0].m_scheduler_available);
  EXPECT_TRUE(output[0].m_io_available);

  output.clear();
  snapshot = complete_snapshot();
  snapshot.extended_cpu =
      System_result<Thread_extended_cpu_snapshot>::failure(error);
  ASSERT_EQ(Conversion_result::degraded,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_TRUE(output[0].m_runtime_available);
  EXPECT_FALSE(output[0].m_extended_cpu_available);
  EXPECT_TRUE(output[0].m_scheduler_available);
  EXPECT_TRUE(output[0].m_io_available);

  output.clear();
  snapshot = complete_snapshot();
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::failure(error);
  ASSERT_EQ(Conversion_result::degraded,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_TRUE(output[0].m_runtime_available);
  EXPECT_TRUE(output[0].m_extended_cpu_available);
  EXPECT_FALSE(output[0].m_scheduler_available);
  EXPECT_TRUE(output[0].m_io_available);

  output.clear();
  snapshot = complete_snapshot();
  snapshot.io = System_result<Thread_io_snapshot>::failure(error);
  ASSERT_EQ(Conversion_result::degraded,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_TRUE(output[0].m_runtime_available);
  EXPECT_TRUE(output[0].m_extended_cpu_available);
  EXPECT_TRUE(output[0].m_scheduler_available);
  EXPECT_FALSE(output[0].m_io_available);
}

TEST(HealthMonitorProcessThreadsAdapter,
     WarningSuppressionRecoversAfterSuccessfulConversion) {
  bool error_state = false;
  EXPECT_TRUE(update_process_threads_error_state(Conversion_result::degraded,
                                                 error_state));
  EXPECT_TRUE(error_state);
  EXPECT_FALSE(update_process_threads_error_state(Conversion_result::degraded,
                                                  error_state));
  EXPECT_TRUE(error_state);
  EXPECT_FALSE(update_process_threads_error_state(Conversion_result::success,
                                                  error_state));
  EXPECT_FALSE(error_state);
  EXPECT_TRUE(update_process_threads_error_state(Conversion_result::failure,
                                                 error_state));
  EXPECT_TRUE(error_state);
}

TEST(HealthMonitorProcessThreadsAdapter,
     RejectsMisalignedAndInvalidSnapshotsWithoutMutation) {
  std::vector<System_Process_Stats> output(1);
  output.front().m_pid = 99;

  auto snapshot = complete_snapshot();
  snapshot.threads = System_result<Thread_inventory>::success({});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({0, "mysqld"});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({42, ""});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.io = System_result<Thread_io_snapshot>::success({{{42, 1, 1}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{43, std::chrono::milliseconds{1}, std::chrono::milliseconds{1}},
        {42, std::chrono::milliseconds{1}, std::chrono::milliseconds{1}}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{42, "mysqld", Thread_state::running},
        {42, "duplicate", Thread_state::sleeping}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{42, std::chrono::milliseconds{-1}, std::chrono::milliseconds{1}},
        {43, std::chrono::milliseconds{1}, std::chrono::milliseconds{1}}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{42, std::chrono::milliseconds{-1}, {}, {}, {}},
        {43, {}, {}, {}, {}}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));

  snapshot = complete_snapshot();
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::success(
      {{{42, 1, std::chrono::milliseconds{-1}}, {43, 2, {}}}});
  EXPECT_EQ(Conversion_result::failure,
            convert_process_threads_snapshot(snapshot, 1, output));
  EXPECT_EQ(1U, output.size());
  EXPECT_EQ(99U, output.front().m_pid);
}

TEST(HealthMonitorProcessThreadsAdapter,
     RuntimeWithoutProcessLeaderDoesNotCreateOne) {
  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({44, "mysqld"});
  std::vector<System_Process_Stats> output;

  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(snapshot, 1, output));
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ(42U, output[0].m_tid);
  EXPECT_EQ(43U, output[1].m_tid);
  EXPECT_FALSE(output[0].m_runtime_available);
  EXPECT_FALSE(output[1].m_runtime_available);
}

TEST(HealthMonitorProcessThreadsAdapter, MapsAllThreadStates) {
  const std::array<Thread_state, 6> states{
      Thread_state::running, Thread_state::sleeping, Thread_state::waiting,
      Thread_state::stopped, Thread_state::zombie,   Thread_state::unknown};
  const std::array<char, 6> expected{'R', 'S', 'D', 'T', 'Z', '?'};

  for (size_t index = 0; index < states.size(); ++index) {
    auto snapshot = complete_snapshot();
    snapshot.threads.value().threads[0].state = states[index];
    std::vector<System_Process_Stats> output;
    ASSERT_EQ(Conversion_result::success,
              convert_process_threads_snapshot(snapshot, 1, output));
    ASSERT_EQ(2U, output.size());
    EXPECT_EQ(expected[index], output[0].m_state);
  }
}

TEST(HealthMonitorProcessThreadsAdapter, RepeatedConversionDoesNotRetainState) {
  std::vector<System_Process_Stats> output;
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(complete_snapshot(), 1, output));
  ASSERT_EQ(2U, output.size());

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({43, "new"});
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{43, "new", Thread_state::unknown}}});
  snapshot.runtime = System_result<Process_runtime_info>::success({1, 0, 0, 0});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success({{{43, {}, {}}}});
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{43, {}, {}, {}, {}}}});
  snapshot.scheduler =
      System_result<Thread_scheduler_snapshot>::success({{{43, 0, {}}}});
  snapshot.io = System_result<Thread_io_snapshot>::success({{{43, 0, 0}}});
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(snapshot, 2, output));
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(43U, output.back().m_pid);
  EXPECT_EQ("new", output.back().m_process_name);
  EXPECT_EQ(2, output.back().m_timestamp);
  EXPECT_EQ('?', output.back().m_state);
  EXPECT_EQ(0U, output.back().m_read_bytes);
}

TEST(HealthSystemProcessStatsTableAdapter,
     MapsCompleteAndNonleaderRuntimeColumns) {
  std::vector<System_Process_Stats> records;
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(complete_snapshot(), 1, records));
  ASSERT_EQ(2U, records.size());

  const auto leader = make_system_process_stats_row(records[0]);
  for (unsigned int index = 6; index <= 19; ++index) {
    EXPECT_FALSE(system_process_stats_column_is_null(leader, index));
  }

  const auto nonleader = make_system_process_stats_row(records[1]);
  EXPECT_FALSE(system_process_stats_column_is_null(nonleader, 6));
  EXPECT_FALSE(system_process_stats_column_is_null(nonleader, 8));
  for (unsigned int index = 10; index <= 13; ++index) {
    EXPECT_TRUE(system_process_stats_column_is_null(nonleader, index));
  }
  EXPECT_FALSE(system_process_stats_column_is_null(nonleader, 14));
  EXPECT_FALSE(system_process_stats_column_is_null(nonleader, 18));
}

TEST(HealthSystemProcessStatsTableAdapter, MapsMacosOptionalColumnsToNull) {
  std::vector<System_Process_Stats> records;
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(macos_snapshot(), 1, records));

  const auto row = make_system_process_stats_row(records[0]);
  EXPECT_FALSE(system_process_stats_column_is_null(row, 6));
  EXPECT_FALSE(system_process_stats_column_is_null(row, 7));
  for (unsigned int index = 8; index <= 19; ++index) {
    EXPECT_TRUE(system_process_stats_column_is_null(row, index));
  }
}

TEST(HealthSystemProcessStatsTableAdapter, KeepsAvailableZeroNonNull) {
  auto snapshot = complete_snapshot();
  snapshot.runtime = System_result<Process_runtime_info>::success({0, 0, 0, 0});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success(
      {{{42, {}, {}}, {43, {}, {}}}});
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{42, {}, {}, {}, {}}, {43, {}, {}, {}, {}}}});
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::success(
      {{{42, 0, {}}, {43, 0, {}}}});
  snapshot.io =
      System_result<Thread_io_snapshot>::success({{{42, 0, 0}, {43, 0, 0}}});
  std::vector<System_Process_Stats> records;
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(snapshot, 1, records));

  const auto row = make_system_process_stats_row(records[0]);
  for (unsigned int index = 6; index <= 19; ++index) {
    EXPECT_FALSE(system_process_stats_column_is_null(row, index));
  }
}

TEST(HealthSystemProcessStatsTableAdapter, ReusedRowResetsAvailability) {
  System_Process_Stats record;
  record.m_cpu_available = true;
  record.m_runtime_available = true;
  record.m_extended_cpu_available = true;
  record.m_scheduler_available = true;
  record.m_io_available = true;
  auto row = make_system_process_stats_row(record);

  record = System_Process_Stats{};
  row = make_system_process_stats_row(record);
  for (unsigned int index = 6; index <= 19; ++index) {
    EXPECT_TRUE(system_process_stats_column_is_null(row, index));
  }
}

TEST(HealthSystemProcessStatsTableAdapter, RejectsUnknownColumnIndex) {
  const System_process_stats_row row;
  EXPECT_DEATH_IF_SUPPORTED(system_process_stats_column_is_null(row, 20), "");
}

TEST(HealthSystemProcessStatsMetricAdapter,
     DeliversBasicButNotUnavailableMacosMetrics) {
  std::vector<System_Process_Stats> records;
  ASSERT_EQ(Conversion_result::success,
            convert_process_threads_snapshot(macos_snapshot(), 1, records));
  ASSERT_FALSE(records.empty());

  size_t deliveries = 0;
  uint64_t value = 0;
  const auto delivery = [&](const uint64_t measurement) {
    ++deliveries;
    value = measurement;
  };
  EXPECT_TRUE(deliver_process_thread_metric(
      records[0], Process_thread_metric::user_time, delivery));
  EXPECT_EQ(10U, value);
  EXPECT_TRUE(deliver_process_thread_metric(
      records[0], Process_thread_metric::system_time, delivery));
  EXPECT_EQ(20U, value);
  EXPECT_FALSE(deliver_process_thread_metric(
      records[0], Process_thread_metric::child_user_time, delivery));
  EXPECT_FALSE(deliver_process_thread_metric(
      records[0], Process_thread_metric::thread_count, delivery));
  EXPECT_FALSE(deliver_process_thread_metric(
      records[0], Process_thread_metric::processor, delivery));
  EXPECT_FALSE(deliver_process_thread_metric(
      records[0], Process_thread_metric::read_bytes, delivery));
  EXPECT_EQ(2U, deliveries);
}

TEST(HealthSystemProcessStatsMetricAdapter,
     DeliversEachAvailableCapabilityIndependently) {
  System_Process_Stats record;
  record.m_utime_ms = 1;
  record.m_cutime_ms = 2;
  record.m_num_threads = 3;
  record.m_processor = 4;
  record.m_read_bytes = 5;
  record.m_cpu_available = true;
  record.m_extended_cpu_available = true;
  record.m_runtime_available = true;
  record.m_scheduler_available = true;
  record.m_io_available = true;

  uint64_t sum = 0;
  const auto delivery = [&](const uint64_t value) { sum += value; };
  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::user_time, delivery));
  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::child_user_time, delivery));
  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::thread_count, delivery));
  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::processor, delivery));
  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::read_bytes, delivery));
  EXPECT_EQ(15U, sum);
}

TEST(HealthSystemProcessStatsMetricAdapter, DeliversAvailableZero) {
  System_Process_Stats record;
  record.m_runtime_available = true;
  size_t deliveries = 0;

  EXPECT_TRUE(deliver_process_thread_metric(
      record, Process_thread_metric::resident_bytes, [&](const uint64_t value) {
        EXPECT_EQ(0U, value);
        ++deliveries;
      }));
  EXPECT_EQ(1U, deliveries);
}

TEST(HealthSystemProcessStatsMetricAdapter,
     SelectsRuntimeLeaderFromNewestSampleOnly) {
  std::vector<System_Process_Stats> records(4);
  records[0].m_timestamp = 1;
  records[0].m_runtime_available = true;
  records[0].m_num_threads = 10;
  records[1].m_timestamp = 2;
  records[1].m_runtime_available = true;
  records[1].m_num_threads = 20;
  records[2].m_timestamp = 2;
  records[3].m_timestamp = 2;
  System_Process_Stats selected;
  const auto reader = [&](const int index, System_Process_Stats &record) {
    record = records[index];
    return true;
  };

  ASSERT_TRUE(select_latest_process_thread_metric(
      records.size(), Process_thread_metric::thread_count, reader, selected));
  EXPECT_EQ(2, selected.m_timestamp);
  EXPECT_EQ(20U, selected.m_num_threads);

  records[1].m_runtime_available = false;
  selected.m_num_threads = 99;
  EXPECT_FALSE(select_latest_process_thread_metric(
      records.size(), Process_thread_metric::thread_count, reader, selected));
  EXPECT_EQ(99U, selected.m_num_threads);

  records[1].m_runtime_available = true;
  selected.m_num_threads = 99;
  size_t reads = 0;
  const auto failing_reader = [&](const int index,
                                  System_Process_Stats &record) {
    ++reads;
    if (index == 2) return false;
    record = records[index];
    return true;
  };
  EXPECT_FALSE(select_latest_process_thread_metric(
      records.size(), Process_thread_metric::thread_count, failing_reader,
      selected));
  EXPECT_EQ(2U, reads);
  EXPECT_EQ(99U, selected.m_num_threads);
}

}  // namespace
}  // namespace hm
