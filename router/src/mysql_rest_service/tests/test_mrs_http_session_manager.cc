/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <thread>

#include "mrs/http/session_manager.h"
#include "mrs/interface/universal_id.h"

#include "mysql/harness/utility/container/generic.h"

class SessionManagerTestSuite : public ::testing::Test {
 public:
  std::string get_uuid_from_sut() {
    mrs::UniversalId auth_app_id{1, 1};
    return sut_.new_session(auth_app_id, "session_")->get_session_id();
  }

  std::vector<std::string> get_session_uuids_from_sut(uint64_t no) {
    std::vector<std::string> result;
    while (no > 0) {
      result.push_back(get_uuid_from_sut());
      --no;
    }

    return result;
  }

  mrs::http::SessionManager sut_;
};

MATCHER(IsTxtUuid, "") {
  const std::array<size_t, 4> positions{8, 13, 18, 23};
  if (arg.length() != 36) return false;

  for (size_t i = 0; i < 36; ++i) {
    if (mysql_harness::utility::container::has(positions, i)) {
      if (arg[i] != '-') {
        return false;
      }

      continue;
    }
    if (!isxdigit(arg[i])) {
      return false;
    }
  }

  return true;
}

TEST_F(SessionManagerTestSuite, check_cookie_session_id_format) {
  ASSERT_THAT(get_uuid_from_sut(), IsTxtUuid());
}

TEST_F(SessionManagerTestSuite, check_multiple_ids_if_they_are_unique) {
  const auto uuids = get_session_uuids_from_sut(100);

  for (const auto &uuid : uuids) {
    ASSERT_THAT(uuid, IsTxtUuid());
  }

  for (const auto &uuid : uuids) {
    ASSERT_EQ(1, std::count(uuids.begin(), uuids.end(), uuid));
  }
}

TEST_F(SessionManagerTestSuite, check_multiple_ids_if_they_are_unique_threads) {
  std::vector<std::string> uuids_t1;
  std::vector<std::string> uuids_t2;

  // Some implementations, generate same random sequences in different threads
  // Lets confirm that session-manager handles ids correctly.
  std::thread t1(
      [this, &uuids_t1]() { uuids_t1 = get_session_uuids_from_sut(1000); });
  std::thread t2(
      [this, &uuids_t2]() { uuids_t2 = get_session_uuids_from_sut(1000); });
  t1.join();
  t2.join();

  for (const auto &uuid : uuids_t1) {
    ASSERT_THAT(uuid, IsTxtUuid());
    ASSERT_EQ(1, std::count(uuids_t1.begin(), uuids_t1.end(), uuid));
    ASSERT_EQ(0, std::count(uuids_t2.begin(), uuids_t2.end(), uuid));
  }

  for (const auto &uuid : uuids_t2) {
    ASSERT_THAT(uuid, IsTxtUuid());
    ASSERT_EQ(0, std::count(uuids_t1.begin(), uuids_t1.end(), uuid));
    ASSERT_EQ(1, std::count(uuids_t2.begin(), uuids_t2.end(), uuid));
  }
}
