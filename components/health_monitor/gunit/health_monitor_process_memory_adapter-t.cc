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

#include "../health_monitor_process_memory_mapping.h"

namespace hm {
namespace {

using mysql::system_info::Process_identity;
using mysql::system_info::Process_memory_details;
using mysql::system_info::Process_memory_residency;
using mysql::system_info::Process_memory_snapshot;
using mysql::system_info::Process_page_faults;
using mysql::system_info::System_result;

Process_memory_snapshot complete_snapshot() {
  Process_memory_snapshot snapshot;
  snapshot.identity = System_result<Process_identity>::success({42, "mysqld"});
  snapshot.residency = System_result<Process_memory_residency>::success({100});
  snapshot.details = System_result<Process_memory_details>::success({300, 400});
  snapshot.page_faults = System_result<Process_page_faults>::success({600});
  return snapshot;
}

TEST(HealthMonitorProcessMemoryAdapter, MapsCompleteSnapshot) {
  Process_Memory_Info output;

  ASSERT_TRUE(convert_process_memory_snapshot(complete_snapshot(), output));
  EXPECT_EQ(output.m_name, "mysqld");
  EXPECT_EQ(output.m_pid, 42U);
  EXPECT_EQ(output.m_vmrss_bytes, 100U);
  EXPECT_EQ(output.m_vmdata_bytes, 300U);
  EXPECT_EQ(output.m_vmswap_bytes, 400U);
  EXPECT_EQ(output.m_page_faults, 600U);
  EXPECT_TRUE(output.m_residency_available);
  EXPECT_TRUE(output.m_details_available);
  EXPECT_TRUE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter, KeepsAvailableZeroMeasurements) {
  auto snapshot = complete_snapshot();
  snapshot.residency = System_result<Process_memory_residency>::success({0});
  snapshot.details = System_result<Process_memory_details>::success({0, 0});
  snapshot.page_faults = System_result<Process_page_faults>::success({0});
  Process_Memory_Info output;

  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_vmrss_bytes, 0U);
  EXPECT_EQ(output.m_vmdata_bytes, 0U);
  EXPECT_EQ(output.m_vmswap_bytes, 0U);
  EXPECT_EQ(output.m_page_faults, 0U);
  EXPECT_TRUE(output.m_residency_available);
  EXPECT_TRUE(output.m_details_available);
  EXPECT_TRUE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter,
     RejectsUnavailableRequiredCapabilitiesWithoutMutation) {
  Process_Memory_Info output;
  output.m_name = "existing";

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::unavailable();
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");

  snapshot = complete_snapshot();
  snapshot.residency = System_result<Process_memory_residency>::unavailable();
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");
}

TEST(HealthMonitorProcessMemoryAdapter,
     AcceptsUnavailableOptionalCapabilities) {
  auto snapshot = complete_snapshot();
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();
  Process_Memory_Info output;

  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_vmrss_bytes, 100U);
  EXPECT_TRUE(output.m_residency_available);
  EXPECT_EQ(output.m_vmdata_bytes, 0U);
  EXPECT_EQ(output.m_vmswap_bytes, 0U);
  EXPECT_FALSE(output.m_details_available);
  EXPECT_EQ(output.m_page_faults, 0U);
  EXPECT_FALSE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter, AcceptsFailedOptionalCapabilities) {
  const auto error = std::make_error_code(std::errc::io_error);
  auto snapshot = complete_snapshot();
  snapshot.details = System_result<Process_memory_details>::failure(error);
  snapshot.page_faults = System_result<Process_page_faults>::failure(error);
  Process_Memory_Info output;

  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_TRUE(output.m_residency_available);
  EXPECT_FALSE(output.m_details_available);
  EXPECT_FALSE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter,
     OptionalCapabilityStatesAreIndependent) {
  const auto error = std::make_error_code(std::errc::io_error);
  auto snapshot = complete_snapshot();
  snapshot.details = System_result<Process_memory_details>::failure(error);
  Process_Memory_Info output;

  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_FALSE(output.m_details_available);
  EXPECT_TRUE(output.m_page_faults_available);
  EXPECT_EQ(output.m_page_faults, 600U);

  snapshot = complete_snapshot();
  snapshot.page_faults = System_result<Process_page_faults>::failure(error);
  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_TRUE(output.m_details_available);
  EXPECT_EQ(output.m_vmdata_bytes, 300U);
  EXPECT_EQ(output.m_vmswap_bytes, 400U);
  EXPECT_FALSE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter, DetectsFullyUnavailableSnapshot) {
  Process_memory_snapshot snapshot;
  snapshot.identity = System_result<Process_identity>::unavailable();
  snapshot.residency = System_result<Process_memory_residency>::unavailable();
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();

  EXPECT_TRUE(process_memory_snapshot_is_unavailable(snapshot));

  snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::unavailable();
  EXPECT_FALSE(process_memory_snapshot_is_unavailable(snapshot));
}

TEST(HealthMonitorProcessMemoryAdapter, MacosProcessRowIsAvailable) {
  auto snapshot = complete_snapshot();
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();
  Process_Memory_Info output;

  EXPECT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_FALSE(process_memory_snapshot_is_unavailable(snapshot));
}

TEST(HealthMonitorProcessMemoryAdapter,
     DoesNotClassifyFailedRequiredMacosCapabilityAsUnavailable) {
  const auto error = std::make_error_code(std::errc::io_error);
  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::failure(error);
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();

  EXPECT_FALSE(process_memory_snapshot_is_unavailable(snapshot));

  snapshot = complete_snapshot();
  snapshot.residency = System_result<Process_memory_residency>::failure(error);
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();

  EXPECT_FALSE(process_memory_snapshot_is_unavailable(snapshot));
}

TEST(HealthMonitorProcessMemoryAdapter,
     RejectsFailedRequiredCapabilitiesWithoutMutation) {
  const auto error = std::make_error_code(std::errc::io_error);
  Process_Memory_Info output;
  output.m_name = "existing";

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::failure(error);
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");

  snapshot = complete_snapshot();
  snapshot.residency = System_result<Process_memory_residency>::failure(error);
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");
}

TEST(HealthMonitorProcessMemoryAdapter, RejectsInvalidIdentityWithoutMutation) {
  Process_Memory_Info output;
  output.m_name = "existing";

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({0, "mysqld"});
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");

  snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({42, ""});
  EXPECT_FALSE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "existing");
}

TEST(HealthMonitorProcessMemoryAdapter, RepeatedConversionDoesNotRetainState) {
  Process_Memory_Info output;
  ASSERT_TRUE(convert_process_memory_snapshot(complete_snapshot(), output));
  EXPECT_EQ(output.m_name, "mysqld");
  EXPECT_EQ(output.m_vmrss_bytes, 100U);

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Process_identity>::success({43, "new"});
  snapshot.residency = System_result<Process_memory_residency>::success({0});
  snapshot.details = System_result<Process_memory_details>::success({0, 0});
  snapshot.page_faults = System_result<Process_page_faults>::success({0});
  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));
  EXPECT_EQ(output.m_name, "new");
  EXPECT_EQ(output.m_pid, 43U);
  EXPECT_EQ(output.m_vmrss_bytes, 0U);
  EXPECT_EQ(output.m_vmdata_bytes, 0U);
  EXPECT_EQ(output.m_vmswap_bytes, 0U);
  EXPECT_EQ(output.m_page_faults, 0U);
  EXPECT_TRUE(output.m_residency_available);
  EXPECT_TRUE(output.m_details_available);
  EXPECT_TRUE(output.m_page_faults_available);
}

TEST(HealthMonitorProcessMemoryAdapter,
     RepeatedConversionClearsOptionalAvailabilityAndValues) {
  Process_Memory_Info output;
  ASSERT_TRUE(convert_process_memory_snapshot(complete_snapshot(), output));

  auto snapshot = complete_snapshot();
  snapshot.details = System_result<Process_memory_details>::unavailable();
  snapshot.page_faults = System_result<Process_page_faults>::unavailable();
  ASSERT_TRUE(convert_process_memory_snapshot(snapshot, output));

  EXPECT_EQ(output.m_vmdata_bytes, 0U);
  EXPECT_EQ(output.m_vmswap_bytes, 0U);
  EXPECT_EQ(output.m_page_faults, 0U);
  EXPECT_FALSE(output.m_details_available);
  EXPECT_FALSE(output.m_page_faults_available);
}

TEST(HealthProcessMemoryTableAdapter, MapsCompleteRecord) {
  Process_Memory_Info record;
  record.m_timestamp = 123;
  record.m_name = "mysqld";
  record.m_pid = 42;
  record.m_vmrss_bytes = 100;
  record.m_vmdata_bytes = 300;
  record.m_vmswap_bytes = 400;
  record.m_page_faults = 600;
  record.m_residency_available = true;
  record.m_details_available = true;
  record.m_page_faults_available = true;

  const auto row = make_health_process_memory_row(record);
  EXPECT_EQ(row.m_timestamp, 123);
  EXPECT_EQ(row.m_name, "mysqld");
  EXPECT_EQ(row.m_pid, 42U);
  for (unsigned int index = 3; index <= 6; ++index) {
    EXPECT_FALSE(health_process_memory_column_is_null(row, index));
  }
}

TEST(HealthProcessMemoryTableAdapter, MapsMacosNullCapabilities) {
  Process_Memory_Info record;
  record.m_vmrss_bytes = 100;
  record.m_residency_available = true;

  const auto row = make_health_process_memory_row(record);
  EXPECT_FALSE(health_process_memory_column_is_null(row, 3));
  EXPECT_TRUE(health_process_memory_column_is_null(row, 4));
  EXPECT_TRUE(health_process_memory_column_is_null(row, 5));
  EXPECT_TRUE(health_process_memory_column_is_null(row, 6));
}

TEST(HealthProcessMemoryTableAdapter, KeepsAvailableZeroNonNull) {
  Process_Memory_Info record;
  record.m_residency_available = true;
  record.m_details_available = true;
  record.m_page_faults_available = true;

  const auto row = make_health_process_memory_row(record);
  EXPECT_EQ(row.m_vmrss_bytes, 0U);
  EXPECT_EQ(row.m_vmdata_bytes, 0U);
  EXPECT_EQ(row.m_vmswap_bytes, 0U);
  EXPECT_EQ(row.m_page_faults, 0U);
  for (unsigned int index = 3; index <= 6; ++index) {
    EXPECT_FALSE(health_process_memory_column_is_null(row, index));
  }
}

TEST(HealthProcessMemoryTableAdapter, MapsSyntheticUnavailableResidency) {
  Process_Memory_Info record;
  record.m_details_available = true;
  record.m_page_faults_available = true;

  const auto row = make_health_process_memory_row(record);
  EXPECT_TRUE(health_process_memory_column_is_null(row, 3));
  EXPECT_FALSE(health_process_memory_column_is_null(row, 4));
  EXPECT_FALSE(health_process_memory_column_is_null(row, 5));
  EXPECT_FALSE(health_process_memory_column_is_null(row, 6));
}

TEST(HealthProcessMemoryTableAdapter, ReusedRowResetsAvailability) {
  Process_Memory_Info record;
  record.m_residency_available = true;
  record.m_details_available = true;
  record.m_page_faults_available = true;
  auto row = make_health_process_memory_row(record);

  record = Process_Memory_Info{};
  row = make_health_process_memory_row(record);
  for (unsigned int index = 3; index <= 6; ++index) {
    EXPECT_TRUE(health_process_memory_column_is_null(row, index));
  }
}

TEST(HealthProcessMemoryTableAdapter, RejectsUnknownColumnIndex) {
  const Health_process_memory_row row;
  EXPECT_DEATH_IF_SUPPORTED(health_process_memory_column_is_null(row, 2), "");
}

TEST(HealthProcessMemoryMetricAdapter, DoesNotDeliverUnavailableMetrics) {
  const Process_Memory_Info record;
  unsigned int deliveries = 0;
  const auto delivery = [&deliveries](uint64_t, uint64_t) { ++deliveries; };

  EXPECT_FALSE(deliver_process_memory_metric(
      record, Process_memory_metric::resident_bytes, delivery));
  EXPECT_FALSE(deliver_process_memory_metric(
      record, Process_memory_metric::data_bytes, delivery));
  EXPECT_FALSE(deliver_process_memory_metric(
      record, Process_memory_metric::swap_bytes, delivery));
  EXPECT_FALSE(deliver_process_memory_metric(
      record, Process_memory_metric::page_faults, delivery));
  EXPECT_EQ(deliveries, 0U);
}

TEST(HealthProcessMemoryMetricAdapter, DeliversAvailableZeroWithPid) {
  Process_Memory_Info record;
  record.m_pid = 42;
  record.m_residency_available = true;
  record.m_details_available = true;
  record.m_page_faults_available = true;
  unsigned int deliveries = 0;
  const auto delivery = [&deliveries](const uint64_t value,
                                      const uint64_t pid) {
    ++deliveries;
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(pid, 42U);
  };

  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::resident_bytes, delivery));
  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::data_bytes, delivery));
  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::swap_bytes, delivery));
  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::page_faults, delivery));
  EXPECT_EQ(deliveries, 4U);
}

TEST(HealthProcessMemoryMetricAdapter,
     DeliversCompleteLinuxStyleValuesWithPid) {
  Process_Memory_Info record;
  record.m_pid = 42;
  record.m_vmrss_bytes = 100;
  record.m_vmdata_bytes = 300;
  record.m_vmswap_bytes = 400;
  record.m_page_faults = 600;
  record.m_residency_available = true;
  record.m_details_available = true;
  record.m_page_faults_available = true;

  const auto expect_delivery = [&record](const Process_memory_metric metric,
                                         const uint64_t expected) {
    unsigned int deliveries = 0;
    EXPECT_TRUE(deliver_process_memory_metric(
        record, metric,
        [&deliveries, expected](const uint64_t value, const uint64_t pid) {
          ++deliveries;
          EXPECT_EQ(value, expected);
          EXPECT_EQ(pid, 42U);
        }));
    EXPECT_EQ(deliveries, 1U);
  };

  expect_delivery(Process_memory_metric::resident_bytes, 100);
  expect_delivery(Process_memory_metric::data_bytes, 300);
  expect_delivery(Process_memory_metric::swap_bytes, 400);
  expect_delivery(Process_memory_metric::page_faults, 600);
}

TEST(HealthProcessMemoryMetricAdapter, DeliversDataAndSwapIndependently) {
  Process_Memory_Info record;
  record.m_pid = 77;
  record.m_vmdata_bytes = 11;
  record.m_vmswap_bytes = 22;
  record.m_details_available = true;
  uint64_t data_value = 0;
  uint64_t swap_value = 0;

  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::data_bytes,
      [&data_value](const uint64_t value, const uint64_t pid) {
        data_value = value;
        EXPECT_EQ(pid, 77U);
      }));
  EXPECT_TRUE(deliver_process_memory_metric(
      record, Process_memory_metric::swap_bytes,
      [&swap_value](const uint64_t value, const uint64_t pid) {
        swap_value = value;
        EXPECT_EQ(pid, 77U);
      }));
  EXPECT_EQ(data_value, 11U);
  EXPECT_EQ(swap_value, 22U);
}

}  // namespace
}  // namespace hm
