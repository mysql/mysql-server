/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <vector>

#include "helper/container/generic.h"
#include "mrs/gtid_manager.h"

using UID = mrs::database::GTIDuuid;
using Gtid = mrs::database::Gtid;
using GtidSet = mrs::database::GtidSet;
using TCPaddress = mysql_harness::TCPAddress;

const auto k_ok = mrs::GtidAction::k_is_on_server;
const auto k_none = mrs::GtidAction::k_not_found;
const auto k_update = mrs::GtidAction::k_needs_update;

class GtidManagerTest : public testing::Test {
 public:
  TCPaddress make_addr(uint8_t id, uint16_t port) {
    using namespace std::string_literals;
    return TCPaddress("127.0.0."s + std::to_string(id), port);
  }

  Gtid make_gtid(int uid, std::string range) {
    UID u;
    memcpy(u.raw, &uid, sizeof(uid));
    Gtid result{u.to_string() + range};
    return result;
  }

  struct Set {
    int uid;
    std::string range;
  };
  std::vector<GtidSet> make_sets(std::vector<Set> sets) {
    std::vector<GtidSet> result;
    for (auto &s : sets) {
      UID u;
      memcpy(u.raw, &s.uid, sizeof(s.uid));
      GtidSet set{u.to_string() + s.range};
      result.push_back(set);
    }

    return result;
  }

  void SetUp() override {
    sut_.configure("{\"gtid\":{\"cache\":{\"enable\":true}}}");
  }

  mrs::GtidManager sut_{};
};

TEST_F(GtidManagerTest, not_cached) {
  const auto k_addr1 = make_addr(1, 1000);
  const auto k_addr2 = make_addr(2, 1000);
  const auto k_addr3 = make_addr(2, 1001);

  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(2, ":2");
  const auto k_gtid3 = make_gtid(1, ":3");

  sut_.reinitialize(k_addr1, {});
  sut_.reinitialize(k_addr2, {});
  sut_.reinitialize(k_addr3, {});

  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid3));

    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid3));

    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid3));
  }
}

TEST_F(GtidManagerTest, insert_on_addr1_other_not_cached) {
  const auto k_addr1 = make_addr(1, 1000);
  const auto k_addr2 = make_addr(2, 1000);
  const auto k_addr3 = make_addr(2, 1001);

  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(2, ":2");
  const auto k_gtid3 = make_gtid(1, ":3");

  sut_.reinitialize(k_addr1, {});
  sut_.reinitialize(k_addr2, {});
  sut_.reinitialize(k_addr3, {});

  sut_.remember(k_addr1, k_gtid1);
  sut_.remember(k_addr1, k_gtid2);
  sut_.remember(k_addr1, k_gtid3);

  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid1));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid2));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid3));

    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid3));

    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid3));
  }
}

TEST_F(GtidManagerTest, insert_on_addr1_and_adde2_other_not_cached) {
  const auto k_addr1 = make_addr(1, 1000);
  const auto k_addr2 = make_addr(2, 1000);
  const auto k_addr3 = make_addr(2, 1001);

  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(2, ":2");
  const auto k_gtid3 = make_gtid(1, ":2");

  sut_.reinitialize(k_addr1, {});
  sut_.reinitialize(k_addr2, {});
  sut_.reinitialize(k_addr3, {});

  sut_.remember(k_addr1, k_gtid1);
  sut_.remember(k_addr1, k_gtid2);
  sut_.remember(k_addr1, k_gtid3);

  sut_.remember(k_addr2, k_gtid1);
  sut_.remember(k_addr2, k_gtid2);
  sut_.remember(k_addr2, k_gtid3);

  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid1));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid2));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid3));

    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr2, k_gtid1));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr2, k_gtid2));
    ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr2, k_gtid3));

    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid1));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid2));
    ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr3, k_gtid3));
  }
}

TEST_F(GtidManagerTest, missing_initialization_is_notified_to_user) {
  const auto k_addr1 = make_addr(1, 1000);
  const auto k_addr2 = make_addr(2, 1000);

  // Out of range on addr2
  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(2, ":2");
  const auto k_gtid3 = make_gtid(2, ":200");

  // First call on server1, announces thats update is required
  ASSERT_EQ(k_update, sut_.is_executed_on_server(k_addr1, k_gtid1));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid1));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid2));

  // First call on server2, announces thats update is required
  ASSERT_EQ(k_update, sut_.is_executed_on_server(k_addr2, k_gtid1));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid1));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid2));

  sut_.reinitialize(
      k_addr1, make_sets({{1, ":1-10:20:100"}, {2, ":1-20:40-100:200-300"}}));
  sut_.reinitialize(k_addr2, {});

  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid1));
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid2));
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid3));

  // Server2 updated done, returning that is not present in the cache
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid2));
}

TEST_F(GtidManagerTest, reinitialize) {
  const auto k_addr1 = make_addr(1, 1000);
  const auto k_addr2 = make_addr(2, 1000);

  // Out of range on addr2
  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(2, ":2");
  const auto k_gtid3 = make_gtid(2, ":200");

  // Out of range on both servers
  const auto k_gtid4 = make_gtid(1, ":200");
  const auto k_gtid5 = make_gtid(2, ":400");

  sut_.reinitialize(
      k_addr1, make_sets({{1, ":1-10:20:100"}, {2, ":1-20:40-100:200-300"}}));
  sut_.reinitialize(k_addr2, {});

  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid1));
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid2));
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid3));

  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid1));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid2));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid3));

  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid4));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid5));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid4));
  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr2, k_gtid5));
}

TEST_F(GtidManagerTest, reinitialize_and_update) {
  const auto k_addr1 = make_addr(1, 1000);

  const auto k_gtid1 = make_gtid(1, ":1");
  const auto k_gtid2 = make_gtid(1, ":2");
  const auto k_gtid3 = make_gtid(1, ":200");

  sut_.reinitialize(k_addr1, make_sets({{1, ":1-10:20:100"}}));

  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid1));
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid2));

  ASSERT_EQ(k_none, sut_.is_executed_on_server(k_addr1, k_gtid3));

  sut_.remember(k_addr1, k_gtid3);
  ASSERT_EQ(k_ok, sut_.is_executed_on_server(k_addr1, k_gtid3));
}
