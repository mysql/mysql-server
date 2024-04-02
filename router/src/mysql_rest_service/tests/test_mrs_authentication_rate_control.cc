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
#include <thread>

#include "mrs/authentication/rate_control_for.h"

using RateControlString = mrs::authentication::RateControlFor<std::string, 1>;
using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;

const uint64_t k_block_after_rate = 10;

class RateControlForTest : public testing::Test {
 public:
  void SetUp() override {}

  RateControlString sut_{k_block_after_rate, seconds{10}, {}};
};

TEST_F(RateControlForTest, different_requests_can_be_accepts_in_any_number) {
  const uint64_t number_of_clients = 10000;
  for (uint64_t i = 0; i < number_of_clients; ++i) {
    ASSERT_TRUE(sut_.allow(std::to_string(i)));
  }

  ASSERT_EQ(number_of_clients, sut_.size());
  sut_.clear();
  ASSERT_EQ(number_of_clients, sut_.size());

  std::this_thread::sleep_for(milliseconds{1500});

  ASSERT_EQ(number_of_clients, sut_.size());
  sut_.clear();
  ASSERT_EQ(0, sut_.size());
}

TEST_F(RateControlForTest, different_requests_under_the_limit) {
  const uint64_t number_of_clients = 1000;
  const uint64_t number_of_request_per_client = k_block_after_rate;
  for (uint64_t i = 0; i < 1000; ++i) {
    for (uint64_t j = 0; j < number_of_request_per_client; ++j) {
      ASSERT_TRUE(sut_.allow(std::to_string(i)));
    }
  }

  ASSERT_EQ(number_of_clients, sut_.size());
  sut_.clear();
  ASSERT_EQ(number_of_clients, sut_.size());

  std::this_thread::sleep_for(milliseconds{1500});
  ASSERT_TRUE(sut_.allow(std::to_string(0)));

  ASSERT_EQ(number_of_clients, sut_.size());
  sut_.clear();
  ASSERT_EQ(1, sut_.size());
}

TEST_F(RateControlForTest, different_requests_keeps_the_rate_under_limit) {
  for (int repeat = 0; repeat < 3; ++repeat) {
    std::this_thread::sleep_for(seconds{1});
    for (uint64_t i = 0; i < 100; ++i) {
      for (uint64_t j = 0; j < k_block_after_rate; ++j) {
        ASSERT_TRUE(sut_.allow(std::to_string(i)));
      }
    }
  }
}

TEST_F(RateControlForTest, block_when_rate_reached) {
  const std::string k_host = "some_host";
  for (uint64_t j = 0; j < k_block_after_rate; ++j) {
    ASSERT_TRUE(sut_.allow(k_host));
  }
  ASSERT_FALSE(sut_.allow(k_host));
}

TEST_F(RateControlForTest, different_requests_block_when_rate_reached) {
  for (uint64_t i = 0; i < 100; ++i) {
    for (uint64_t j = 0; j < k_block_after_rate; ++j) {
      ASSERT_TRUE(sut_.allow(std::to_string(i)));
    }
    ASSERT_FALSE(sut_.allow(std::to_string(i)));
  }
}

TEST_F(RateControlForTest, show_that_host_is_unblocked_after_timeout1) {
  sut_ = RateControlString{k_block_after_rate, seconds{2}, {}};
  const std::string k_host = "some_host";
  for (uint64_t j = 0; j < k_block_after_rate; ++j) {
    ASSERT_TRUE(sut_.allow(k_host));
  }
  ASSERT_FALSE(sut_.allow(k_host));

  std::this_thread::sleep_for(milliseconds{2100});

  ASSERT_TRUE(sut_.allow(k_host));
}

TEST_F(RateControlForTest, show_that_host_is_unblocked_after_timeout2) {
  sut_ = RateControlString{k_block_after_rate, seconds{2}, {}};
  const std::string k_host = "some_host";
  for (uint64_t j = 0; j < k_block_after_rate; ++j) {
    ASSERT_TRUE(sut_.allow(k_host));
  }
  ASSERT_FALSE(sut_.allow(k_host));

  std::this_thread::sleep_for(milliseconds{500});
  // Request in middle, doesn't prolong the 'block timer'.
  ASSERT_FALSE(sut_.allow(k_host));
  std::this_thread::sleep_for(milliseconds{1600});

  ASSERT_TRUE(sut_.allow(k_host));
}

TEST_F(RateControlForTest, empty_config_always_accept_requests) {
  sut_ = RateControlString{{}, seconds{2}, {}};

  const uint64_t number_of_clients = 10000;
  for (uint64_t i = 0; i < number_of_clients; ++i) {
    ASSERT_TRUE(sut_.allow(std::to_string(i)));
  }

  const std::string k_host = "some_host";
  for (uint64_t j = 0; j < 10 * k_block_after_rate; ++j) {
    ASSERT_TRUE(sut_.allow(k_host));
  }
}

TEST_F(RateControlForTest, speed_limit) {
  sut_ = RateControlString{{}, seconds{2}, {milliseconds(500)}};
  const std::string k_host = "some_host";

  ASSERT_TRUE(sut_.allow(k_host));
  for (uint64_t j = 0; j < k_block_after_rate; ++j) {
    ASSERT_FALSE(sut_.allow(k_host));
  }

  for (int i = 0; i < 2; ++i) {
    std::this_thread::sleep_for(milliseconds{510});
    ASSERT_TRUE(sut_.allow(k_host));
    ASSERT_FALSE(sut_.allow(k_host));
  }
}

TEST_F(RateControlForTest,
       different_requests_keeps_the_rate_under_limit_with_speed_limit) {
  bool result = true;
  sut_ = RateControlString{{}, seconds{2}, {milliseconds(50)}};
  for (int repeat = 0; repeat < 3; ++repeat) {
    std::this_thread::sleep_for(seconds{1});
    for (uint64_t i = 0; i < 100; ++i) {
      result = true;
      for (uint64_t j = 0; j < k_block_after_rate; ++j) {
        ASSERT_EQ(result, sut_.allow(std::to_string(i)));
        result = false;
      }
    }
  }
}
