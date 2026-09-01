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

#include "../health_monitor_storage_mapping.h"

namespace hm {
namespace {

using mysql::system_info::Result_state;
using mysql::system_info::Storage_capacities;
using mysql::system_info::Storage_device_kind;
using mysql::system_info::Storage_flush_snapshot;
using mysql::system_info::Storage_hierarchy;
using mysql::system_info::Storage_inventory;
using mysql::system_info::Storage_read_write_snapshot;
using mysql::system_info::Storage_snapshot;
using mysql::system_info::System_result;
using namespace std::chrono_literals;

Storage_snapshot complete_snapshot() {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"sda", Storage_device_kind::disk},
        {"sda1", Storage_device_kind::partition}}});
  snapshot.capacities = System_result<Storage_capacities>::success(
      {{{"sda1", 400, 512}, {"sda", 1000, 4096}}});
  snapshot.hierarchy =
      System_result<Storage_hierarchy>::success({{{"sda1", "sda"}}});
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success(
      {{{"sda1", 11, 12, 13ms, 14, 15, 16ms}, {"sda", 1, 2, 3ms, 4, 5, 6ms}}});
  snapshot.flush = System_result<Storage_flush_snapshot>::success(
      {{{"sda1", 17, 18ms}, {"sda", 7, 8ms}}});
  return snapshot;
}

TEST(HealthMonitorStorageAdapter, JoinsCompleteSnapshotByDeviceName) {
  std::vector<System_Disk_Stats> output;

  ASSERT_EQ(convert_storage_snapshot(complete_snapshot(), 42, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 2U);

  const auto &disk = output[0];
  EXPECT_EQ(disk.m_timestamp, 42);
  EXPECT_EQ(disk.m_device_name, "sda");
  EXPECT_TRUE(disk.m_device_kind_available);
  EXPECT_FALSE(disk.m_partition);
  EXPECT_TRUE(disk.m_capacity_available);
  EXPECT_EQ(disk.m_total_bytes, 1000U);
  EXPECT_EQ(disk.m_sector_size_bytes, 4096U);
  EXPECT_TRUE(disk.m_hierarchy_available);
  EXPECT_TRUE(disk.m_parent_disk_name.empty());
  EXPECT_TRUE(disk.m_read_write_available);
  EXPECT_EQ(disk.m_reads, 1U);
  EXPECT_EQ(disk.m_read_bytes, 2U);
  EXPECT_EQ(disk.m_read_time_ms, 3U);
  EXPECT_EQ(disk.m_writes, 4U);
  EXPECT_EQ(disk.m_write_bytes, 5U);
  EXPECT_EQ(disk.m_write_time_ms, 6U);
  EXPECT_TRUE(disk.m_flush_available);
  EXPECT_EQ(disk.m_flushes, 7U);
  EXPECT_EQ(disk.m_flush_time_ms, 8U);

  const auto &partition = output[1];
  EXPECT_EQ(partition.m_timestamp, 42);
  EXPECT_EQ(partition.m_device_name, "sda1");
  EXPECT_TRUE(partition.m_device_kind_available);
  EXPECT_TRUE(partition.m_partition);
  EXPECT_TRUE(partition.m_hierarchy_available);
  EXPECT_EQ(partition.m_parent_disk_name, "sda");
  EXPECT_EQ(partition.m_reads, 11U);
  EXPECT_EQ(partition.m_flushes, 17U);
}

TEST(HealthMonitorStorageAdapter, PreservesReadWriteOnlyHotplugDevice) {
  Storage_snapshot snapshot;
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success(
      {{{"dm-9", 1, 2, 3ms, 4, 5, 6ms}}});
  std::vector<System_Disk_Stats> output;

  ASSERT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "dm-9");
  EXPECT_FALSE(output[0].m_device_kind_available);
  EXPECT_FALSE(output[0].m_capacity_available);
  EXPECT_TRUE(output[0].m_read_write_available);
  EXPECT_FALSE(output[0].m_flush_available);
}

TEST(HealthMonitorStorageAdapter, FailedOptionalCapabilitiesPreserveRows) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"sda", Storage_device_kind::disk}}});
  const auto error = std::make_error_code(std::errc::io_error);
  snapshot.capacities = System_result<Storage_capacities>::failure(error);
  snapshot.hierarchy = System_result<Storage_hierarchy>::failure(error);
  snapshot.read_write =
      System_result<Storage_read_write_snapshot>::failure(error);
  snapshot.flush = System_result<Storage_flush_snapshot>::failure(error);
  std::vector<System_Disk_Stats> output;

  ASSERT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_TRUE(output[0].m_device_kind_available);
  EXPECT_FALSE(output[0].m_capacity_available);
  EXPECT_FALSE(output[0].m_hierarchy_available);
  EXPECT_FALSE(output[0].m_read_write_available);
  EXPECT_FALSE(output[0].m_flush_available);
}

TEST(HealthMonitorStorageAdapter, MissingCapabilityRowsRemainUnavailable) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"sda", Storage_device_kind::disk},
        {"sda1", Storage_device_kind::partition}}});
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success(
      {{{"sda", 1, 2, 3ms, 4, 5, 6ms}}});
  snapshot.flush =
      System_result<Storage_flush_snapshot>::success({{{"sda1", 7, 8ms}}});
  std::vector<System_Disk_Stats> output;

  ASSERT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 2U);
  EXPECT_TRUE(output[0].m_read_write_available);
  EXPECT_FALSE(output[0].m_flush_available);
  EXPECT_FALSE(output[1].m_read_write_available);
  EXPECT_TRUE(output[1].m_flush_available);

  const auto disk_row = make_system_disk_table_row(output[0]);
  EXPECT_FALSE(system_disk_table_column_is_null(disk_row, 2));
  EXPECT_TRUE(system_disk_table_column_is_null(disk_row, 8));
  EXPECT_TRUE(system_disk_read_write_metric_eligible(output[0], false));
  EXPECT_FALSE(system_disk_flush_metric_eligible(output[0], false));

  const auto partition_row = make_system_disk_table_row(output[1]);
  EXPECT_TRUE(system_disk_table_column_is_null(partition_row, 2));
  EXPECT_FALSE(system_disk_table_column_is_null(partition_row, 8));
  EXPECT_FALSE(system_disk_read_write_metric_eligible(output[1], true));
  EXPECT_TRUE(system_disk_flush_metric_eligible(output[1], true));
}

TEST(HealthMonitorStorageAdapter, KeepsAvailableZeroValues) {
  auto snapshot = complete_snapshot();
  snapshot.capacities = System_result<Storage_capacities>::success(
      {{{"sda", 0, 512}, {"sda1", 0, 512}}});
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success(
      {{{"sda", 0, 0, 0ms, 0, 0, 0ms}, {"sda1", 0, 0, 0ms, 0, 0, 0ms}}});
  snapshot.flush = System_result<Storage_flush_snapshot>::success(
      {{{"sda", 0, 0ms}, {"sda1", 0, 0ms}}});
  std::vector<System_Disk_Stats> output;

  ASSERT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 2U);
  EXPECT_TRUE(output[0].m_capacity_available);
  EXPECT_EQ(output[0].m_total_bytes, 0U);
  EXPECT_TRUE(output[0].m_read_write_available);
  EXPECT_EQ(output[0].m_reads, 0U);
  EXPECT_TRUE(output[0].m_flush_available);
  EXPECT_EQ(output[0].m_flushes, 0U);
  const auto row = make_system_disk_table_row(output[0]);
  EXPECT_FALSE(system_disk_table_column_is_null(row, 2));
  EXPECT_FALSE(system_disk_table_column_is_null(row, 8));
  EXPECT_TRUE(system_disk_read_write_metric_eligible(output[0], false));
  EXPECT_TRUE(system_disk_flush_metric_eligible(output[0], false));
}

TEST(HealthMonitorStorageAdapter, AllUnavailableReturnsUnavailable) {
  Storage_snapshot snapshot;
  std::vector<System_Disk_Stats> output(1);
  output[0].m_device_name = "existing";

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::unavailable);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "existing");
}

TEST(HealthMonitorStorageAdapter, FailureOnlyReturnsFailed) {
  Storage_snapshot snapshot;
  snapshot.read_write = System_result<Storage_read_write_snapshot>::failure(
      std::make_error_code(std::errc::io_error));
  std::vector<System_Disk_Stats> output(1);

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorStorageAdapter, HierarchyOnlyFailureReturnsUnavailable) {
  Storage_snapshot snapshot;
  snapshot.hierarchy = System_result<Storage_hierarchy>::failure(
      std::make_error_code(std::errc::io_error));
  std::vector<System_Disk_Stats> output(1);
  output[0].m_device_name = "existing";

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::unavailable);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "existing");
}

TEST(HealthMonitorStorageAdapter, RejectsMissingPartitionRelationship) {
  auto snapshot = complete_snapshot();
  snapshot.hierarchy = System_result<Storage_hierarchy>::success({});
  std::vector<System_Disk_Stats> output(1);

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorStorageAdapter, RejectsRelationshipOnDisk) {
  auto snapshot = complete_snapshot();
  snapshot.hierarchy =
      System_result<Storage_hierarchy>::success({{{"sda", "sda1"}}});
  std::vector<System_Disk_Stats> output(1);

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorStorageAdapter, RejectsPartitionParent) {
  auto snapshot = complete_snapshot();
  snapshot.inventory.value().devices.push_back(
      {"sda2", Storage_device_kind::partition});
  snapshot.capacities.value().devices.push_back({"sda2", 100, 512});
  snapshot.read_write.value().devices.push_back({"sda2", 1, 2, 3ms, 4, 5, 6ms});
  snapshot.flush.value().devices.push_back({"sda2", 7, 8ms});
  snapshot.hierarchy = System_result<Storage_hierarchy>::success(
      {{{"sda1", "sda2"}, {"sda2", "sda"}}});
  std::vector<System_Disk_Stats> output(1);

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorStorageAdapter, RejectsZeroSectorSizeWithoutMutation) {
  auto snapshot = complete_snapshot();
  snapshot.capacities.value().devices[0].sector_size_bytes = 0;
  std::vector<System_Disk_Stats> output(1);
  output[0].m_device_name = "existing";

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "existing");
}

TEST(HealthMonitorStorageAdapter, RejectsNegativeDurationsWithoutMutation) {
  auto snapshot = complete_snapshot();
  snapshot.read_write.value().devices[1].write_time = -1ms;
  std::vector<System_Disk_Stats> output(1);
  output[0].m_device_name = "existing";

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "existing");

  snapshot = complete_snapshot();
  snapshot.flush.value().devices[0].flush_time = -1ms;
  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  ASSERT_EQ(output.size(), 1U);
  EXPECT_EQ(output[0].m_device_name, "existing");
}

TEST(HealthMonitorStorageAdapter, RejectsDuplicateAndEmptyDeviceNames) {
  auto snapshot = complete_snapshot();
  snapshot.inventory.value().devices.push_back(
      {"sda", Storage_device_kind::disk});
  std::vector<System_Disk_Stats> output(1);

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);

  snapshot = complete_snapshot();
  snapshot.flush.value().devices[0].device.clear();
  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_EQ(output.size(), 1U);
}

TEST(HealthMonitorStorageAdapter, RejectsInvalidInventoryKind) {
  auto snapshot = complete_snapshot();
  snapshot.inventory.value().devices[0].kind = Storage_device_kind::unknown;
  std::vector<System_Disk_Stats> output;

  EXPECT_EQ(convert_storage_snapshot(snapshot, 1, output),
            Result_state::failed);
  EXPECT_TRUE(output.empty());
}

TEST(HealthMonitorStorageAdapter, FailureThenSuccessDoesNotRetainState) {
  Storage_snapshot failed;
  failed.inventory = System_result<Storage_inventory>::failure(
      std::make_error_code(std::errc::io_error));
  std::vector<System_Disk_Stats> output;

  EXPECT_EQ(convert_storage_snapshot(failed, 1, output), Result_state::failed);
  EXPECT_TRUE(output.empty());
  ASSERT_EQ(convert_storage_snapshot(complete_snapshot(), 2, output),
            Result_state::available);
  ASSERT_EQ(output.size(), 2U);
  EXPECT_EQ(output[0].m_timestamp, 2);
  EXPECT_TRUE(output[0].m_read_write_available);
}

TEST(HealthMonitorStorageAdapter, TableMappingResetsNullableState) {
  System_Disk_Stats record;
  record.m_reads = 1;
  record.m_flushes = 2;
  record.m_read_write_available = true;
  record.m_flush_available = true;

  auto row = make_system_disk_table_row(record);
  for (unsigned int index = 2; index <= 9; ++index) {
    EXPECT_FALSE(system_disk_table_column_is_null(row, index));
  }

  record.m_reads = 0;
  record.m_flushes = 0;
  record.m_read_write_available = false;
  record.m_flush_available = false;
  row = make_system_disk_table_row(record);
  EXPECT_EQ(row.m_reads, 0U);
  EXPECT_EQ(row.m_flushes, 0U);
  for (unsigned int index = 2; index <= 9; ++index) {
    EXPECT_TRUE(system_disk_table_column_is_null(row, index));
  }
}

TEST(HealthMonitorStorageAdapter, MetricEligibilityRequiresKindAndCapability) {
  System_Disk_Stats record;
  record.m_device_kind_available = true;
  record.m_read_write_available = true;
  record.m_flush_available = true;

  EXPECT_TRUE(system_disk_read_write_metric_eligible(record, false));
  EXPECT_FALSE(system_disk_read_write_metric_eligible(record, true));
  EXPECT_TRUE(system_disk_flush_metric_eligible(record, false));

  record.m_partition = true;
  EXPECT_TRUE(system_disk_read_write_metric_eligible(record, true));
  EXPECT_TRUE(system_disk_flush_metric_eligible(record, true));

  record.m_device_kind_available = false;
  EXPECT_FALSE(system_disk_read_write_metric_eligible(record, true));
  EXPECT_FALSE(system_disk_flush_metric_eligible(record, true));

  record.m_device_kind_available = true;
  record.m_read_write_available = false;
  record.m_flush_available = false;
  EXPECT_FALSE(system_disk_read_write_metric_eligible(record, true));
  EXPECT_FALSE(system_disk_flush_metric_eligible(record, true));
}

}  // namespace
}  // namespace hm
