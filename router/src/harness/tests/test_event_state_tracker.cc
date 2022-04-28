/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/event_state_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using mysql_harness::EventStateTracker;

class EventStateTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override { EventStateTracker::instance().clear(); }
};

TEST_F(EventStateTrackerTest, NoTagEventBool) {
  // first call is always returns true as the initial state is not known
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataRefreshOk));

  // second call with the same state
  EXPECT_FALSE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataRefreshOk));

  // another call, state changes now
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      false, EventStateTracker::EventId::MetadataRefreshOk));
}

TEST_F(EventStateTrackerTest, NoTagEventInt) {
  // first call is always returns true as the initial state is not known
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      0, EventStateTracker::EventId::MetadataRefreshOk));

  // second call with a different state
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      1, EventStateTracker::EventId::MetadataRefreshOk));

  // another call, yet another state
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      2, EventStateTracker::EventId::MetadataRefreshOk));

  // no change now
  EXPECT_FALSE(EventStateTracker::instance().state_changed(
      2, EventStateTracker::EventId::MetadataRefreshOk));
}

TEST_F(EventStateTrackerTest, NoTagIndependentEvents) {
  // first call is always returns true as the initial state is not known
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      0, EventStateTracker::EventId::MetadataRefreshOk));

  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      0, EventStateTracker::EventId::GRMemberConnectedOk));

  // both change independently
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      1, EventStateTracker::EventId::MetadataRefreshOk));

  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      1, EventStateTracker::EventId::GRMemberConnectedOk));
}

TEST_F(EventStateTrackerTest, TagEvent) {
  // the same eventId but 2 different tags, each is a separate event so both
  // should return true
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:3306"));

  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:33060"));

  // second call with the same state for each
  EXPECT_FALSE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:3306"));

  EXPECT_FALSE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:33060"));

  // another call, state changes now
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      false, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:33060"));

  // new tag, should return true
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      false, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:6446"));
}

TEST_F(EventStateTrackerTest, RemoveTag) {
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:3306"));
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:33060"));

  EventStateTracker::instance().remove_tag("localhost:3306");

  // after removing we expect the change to be reported for removed tag
  EXPECT_TRUE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:3306"));
  EXPECT_FALSE(EventStateTracker::instance().state_changed(
      true, EventStateTracker::EventId::MetadataServerConnectedOk,
      "localhost:33060"));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
