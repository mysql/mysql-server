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

#include <string>
#include <system_error>
#include <vector>

#include "../health_monitor_filesystem_sample.h"

namespace hm {
namespace {

using mysql::system_info::Filesystem_identity;
using mysql::system_info::Filesystem_snapshot;
using mysql::system_info::Filesystem_usage;
using mysql::system_info::System_result;

Filesystem_snapshot complete_snapshot(std::string filesystem_id = "8:1",
                                      std::string mount_point = "/data",
                                      std::string mount_source = "/dev/sda1",
                                      std::string filesystem_type = "xfs",
                                      uint64_t capacity = 1000,
                                      uint64_t available = 250) {
  Filesystem_snapshot snapshot;
  snapshot.identity = System_result<Filesystem_identity>::success(
      {std::move(filesystem_id), std::move(mount_point),
       std::move(mount_source), std::move(filesystem_type)});
  snapshot.usage =
      System_result<Filesystem_usage>::success({capacity, available});
  return snapshot;
}

TEST(HealthMonitorFilesystemAdapter, MapsCompleteLinuxStyleSnapshot) {
  Disk_Info output;

  ASSERT_TRUE(convert_filesystem_snapshot(complete_snapshot(), output));
  EXPECT_EQ("8:1", output.filesystem_id);
  EXPECT_EQ("/dev/sda1", output.device_name);
  EXPECT_EQ("/dev/sda1", output.mount_source);
  EXPECT_EQ("/data", output.mount_point);
  EXPECT_EQ("xfs", output.filesystem_type);
  EXPECT_EQ(1000U, output.total_bytes);
  EXPECT_EQ(250U, output.avail_bytes);
}

TEST(HealthMonitorFilesystemAdapter, MapsCompleteMacosStyleSnapshot) {
  Disk_Info output;

  ASSERT_TRUE(convert_filesystem_snapshot(
      complete_snapshot("123", "/System/Volumes/Data", "/dev/disk3s1", "apfs",
                        2000, 900),
      output));
  EXPECT_EQ("123", output.filesystem_id);
  EXPECT_EQ("/dev/disk3s1", output.device_name);
  EXPECT_EQ("/System/Volumes/Data", output.mount_point);
  EXPECT_EQ("apfs", output.filesystem_type);
  EXPECT_EQ(2000U, output.total_bytes);
  EXPECT_EQ(900U, output.avail_bytes);
}

TEST(HealthMonitorFilesystemAdapter, KeepsAvailableZeroSpace) {
  Disk_Info output;

  ASSERT_TRUE(convert_filesystem_snapshot(
      complete_snapshot("8:1", "/data", "/dev/sda1", "xfs", 1000, 0), output));
  EXPECT_EQ(0U, output.avail_bytes);
}

TEST(HealthMonitorFilesystemAdapter,
     RejectsUnavailableAndFailedIdentityWithoutMutation) {
  Disk_Info output;
  output.filesystem_id = "preserved";

  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Filesystem_identity>::unavailable();
  EXPECT_FALSE(convert_filesystem_snapshot(snapshot, output));
  EXPECT_EQ("preserved", output.filesystem_id);

  snapshot = complete_snapshot();
  snapshot.identity = System_result<Filesystem_identity>::failure(
      std::make_error_code(std::errc::io_error));
  EXPECT_FALSE(convert_filesystem_snapshot(snapshot, output));
  EXPECT_EQ("preserved", output.filesystem_id);
}

TEST(HealthMonitorFilesystemAdapter,
     RejectsUnavailableAndFailedUsageWithoutMutation) {
  Disk_Info output;
  output.total_bytes = 99;

  auto snapshot = complete_snapshot();
  snapshot.usage = System_result<Filesystem_usage>::unavailable();
  EXPECT_FALSE(convert_filesystem_snapshot(snapshot, output));
  EXPECT_EQ(99U, output.total_bytes);

  snapshot = complete_snapshot();
  snapshot.usage = System_result<Filesystem_usage>::failure(
      std::make_error_code(std::errc::io_error));
  EXPECT_FALSE(convert_filesystem_snapshot(snapshot, output));
  EXPECT_EQ(99U, output.total_bytes);
}

TEST(HealthMonitorFilesystemAdapter, RejectsEmptyRequiredIdentityStrings) {
  for (int field = 0; field < 4; ++field) {
    auto snapshot = complete_snapshot();
    auto &identity = snapshot.identity.value();
    if (field == 0) identity.filesystem_id.clear();
    if (field == 1) identity.mount_point.clear();
    if (field == 2) identity.mount_source.clear();
    if (field == 3) identity.filesystem_type.clear();
    Disk_Info output;
    output.filesystem_id = "preserved";

    EXPECT_FALSE(convert_filesystem_snapshot(snapshot, output));
    EXPECT_EQ("preserved", output.filesystem_id);
  }
}

TEST(HealthMonitorFilesystemAdapter, RejectsInvalidUsageWithoutMutation) {
  Disk_Info output;
  output.total_bytes = 99;

  EXPECT_FALSE(convert_filesystem_snapshot(
      complete_snapshot("8:1", "/data", "/dev/sda1", "xfs", 0, 0), output));
  EXPECT_EQ(99U, output.total_bytes);
  EXPECT_FALSE(convert_filesystem_snapshot(
      complete_snapshot("8:1", "/data", "/dev/sda1", "xfs", 100, 101), output));
  EXPECT_EQ(99U, output.total_bytes);
}

TEST(HealthMonitorFilesystemAdapter, RepeatedConversionDoesNotRetainState) {
  Disk_Info output;
  ASSERT_TRUE(convert_filesystem_snapshot(complete_snapshot(), output));

  ASSERT_TRUE(convert_filesystem_snapshot(
      complete_snapshot("new", "/new", "new-source", "apfs", 20, 2), output));
  EXPECT_EQ("new", output.filesystem_id);
  EXPECT_EQ("new-source", output.device_name);
  EXPECT_EQ("new-source", output.mount_source);
  EXPECT_EQ("/new", output.mount_point);
  EXPECT_EQ("apfs", output.filesystem_type);
  EXPECT_EQ(20U, output.total_bytes);
  EXPECT_EQ(2U, output.avail_bytes);
}

TEST(HealthMonitorFilesystemAdapter, ClassifiesFilesystemChangesIndependently) {
  Filesystem_sample_state state;
  Disk_Info sample;
  ASSERT_TRUE(convert_filesystem_snapshot(complete_snapshot(), sample));
  EXPECT_EQ(Filesystem_change::initial,
            classify_filesystem_change(state, sample));

  state = {sample.filesystem_id, sample.total_bytes, true};
  EXPECT_EQ(Filesystem_change::unchanged,
            classify_filesystem_change(state, sample));
  sample.total_bytes += 1;
  EXPECT_EQ(Filesystem_change::capacity_changed,
            classify_filesystem_change(state, sample));
  sample.total_bytes = state.capacity_bytes;
  sample.filesystem_id = "different";
  EXPECT_EQ(Filesystem_change::identity_changed,
            classify_filesystem_change(state, sample));
}

TEST(HealthMonitorFilesystemAdapter, FailedSampleDoesNotChangeSuccessfulState) {
  Filesystem_sample_state state{"8:1", 1000, true};
  Disk_Info output;
  output.filesystem_id = "preserved";
  Filesystem_change change = Filesystem_change::unchanged;
  auto snapshot = complete_snapshot();
  snapshot.usage = System_result<Filesystem_usage>::unavailable();

  EXPECT_FALSE(accept_filesystem_snapshot(snapshot, state, output, change));
  EXPECT_EQ("8:1", state.filesystem_id);
  EXPECT_EQ(1000U, state.capacity_bytes);
  EXPECT_TRUE(state.initialized);
  EXPECT_EQ("preserved", output.filesystem_id);
  EXPECT_EQ(Filesystem_change::unchanged, change);
}

TEST(HealthMonitorFilesystemAdapter,
     IdentityChangeResetsBeforePublishingNewSample) {
  Disk_Info sample;
  ASSERT_TRUE(convert_filesystem_snapshot(complete_snapshot(), sample));
  std::vector<std::string> actions;
  time_t stat_timestamp = 0;
  time_t metric_timestamp = 0;

  publish_filesystem_records(
      sample, 123, true, true, Filesystem_change::identity_changed,
      [&actions]() { actions.emplace_back("reset"); },
      [&actions, &stat_timestamp](const Disk_Info &record) {
        actions.emplace_back("stat");
        stat_timestamp = record.timestamp;
      },
      [&actions, &metric_timestamp](const Disk_Info &record) {
        actions.emplace_back("metric");
        metric_timestamp = record.timestamp;
      });

  EXPECT_EQ((std::vector<std::string>{"reset", "stat", "metric"}), actions);
  EXPECT_EQ(123, stat_timestamp);
  EXPECT_EQ(123, metric_timestamp);
}

TEST(HealthMonitorFilesystemAdapter,
     CapacityChangePublishesWithoutResettingSeries) {
  Disk_Info sample;
  ASSERT_TRUE(convert_filesystem_snapshot(complete_snapshot(), sample));
  int reset_count = 0;
  int metric_count = 0;

  publish_filesystem_records(
      sample, 123, false, true, Filesystem_change::capacity_changed,
      [&reset_count]() { ++reset_count; }, {},
      [&metric_count](const Disk_Info &) { ++metric_count; });

  EXPECT_EQ(0, reset_count);
  EXPECT_EQ(1, metric_count);
}

TEST(HealthMonitorFilesystemAdapter, IncompleteSampleIsNotPublished) {
  Filesystem_sample_state state;
  Disk_Info sample;
  Filesystem_change change = Filesystem_change::unchanged;
  int writes = 0;
  auto snapshot = complete_snapshot();
  snapshot.identity = System_result<Filesystem_identity>::unavailable();

  if (accept_filesystem_snapshot(snapshot, state, sample, change)) {
    publish_filesystem_records(
        sample, 123, true, true, change, {},
        [&writes](const Disk_Info &) { ++writes; },
        [&writes](const Disk_Info &) { ++writes; });
  }

  EXPECT_EQ(0, writes);
  EXPECT_FALSE(state.initialized);
}

TEST(HealthMonitorFilesystemAdapter, FailureThenSuccessPublishesCurrentValues) {
  Filesystem_sample_state state;
  Disk_Info sample;
  Filesystem_change change = Filesystem_change::unchanged;
  auto snapshot = complete_snapshot();
  snapshot.usage = System_result<Filesystem_usage>::unavailable();
  EXPECT_FALSE(accept_filesystem_snapshot(snapshot, state, sample, change));

  ASSERT_TRUE(accept_filesystem_snapshot(
      complete_snapshot("new", "/new", "new-source", "apfs", 20, 2), state,
      sample, change));
  Disk_Info published;
  publish_filesystem_records(
      sample, 456, true, false, change, {},
      [&published](const Disk_Info &record) { published = record; }, {});

  EXPECT_EQ("new", published.filesystem_id);
  EXPECT_EQ("new-source", published.device_name);
  EXPECT_EQ(20U, published.total_bytes);
  EXPECT_EQ(2U, published.avail_bytes);
  EXPECT_EQ(456, published.timestamp);
}

}  // namespace
}  // namespace hm
