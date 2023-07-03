/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <libmysql/dns_srv_data.h>
#include <unordered_set>

namespace dns_srv_data_unittest {

class dns_srv_data_test : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    data.clear();
    host.clear();
    port = 0;
  }

  Dns_srv_data data;
  std::string host;
  unsigned port;
};

TEST_F(dns_srv_data_test, empty_list) {
  host = "nothost";
  port = 100;
  ASSERT_EQ(data.pop_next(host, port), true);
  ASSERT_STREQ(host.c_str(), "nothost");
  ASSERT_EQ(port, 100);
}

TEST_F(dns_srv_data_test, one_element) {
  data.add("h1", 12, 0, 0);

  ASSERT_EQ(data.pop_next(host, port), false);
  ASSERT_STREQ(host.c_str(), "h1");
  ASSERT_EQ(port, 12);
  host = "h2";
  port = 13;
  ASSERT_EQ(data.pop_next(host, port), true);
}

TEST_F(dns_srv_data_test, different_prio) {
  const std::string low("low");
  const std::string high("high");
  // push low prio first, high prio next
  data.add(low, 13, 2, 0);
  data.add(high, 12, 1, 0);

  // pop and expect high prio
  ASSERT_EQ(data.pop_next(host, port), false);
  ASSERT_STREQ(host.c_str(), high.c_str());
  ASSERT_EQ(port, 12);

  // pop and expect low prio
  ASSERT_EQ(data.pop_next(host, port), false);
  ASSERT_STREQ(host.c_str(), low.c_str());
  ASSERT_EQ(port, 13);

  // expect empty
  ASSERT_EQ(data.pop_next(host, port), true);
}

TEST_F(dns_srv_data_test, different_weight) {
  const std::string low("low");
  const std::string high("high");
  bool got_low = false, got_high = false;
  // push low prio first, high weight next
  data.add(low, 13, 1, 1);
  data.add(high, 12, 1, 2);

  while (!data.pop_next(host, port)) {
    if (host == high && port == 12)
      got_high = true;
    else if (host == low && port == 13)
      got_low = true;
    else
      ASSERT_TRUE(host == high || host == low);
  }
  ASSERT_TRUE(got_high && got_low);
}

TEST_F(dns_srv_data_test, zero_weight) {
  const std::string zero("zero");
  const std::string nonzero("nonzero");
  std::unordered_set<unsigned> s = {12, 13, 14, 15};
  data.add(zero, 13, 1, 0);
  data.add(zero, 12, 1, 0);
  data.add(nonzero, 14, 1, 1);
  data.add(nonzero, 15, 1, 2);

  while (!data.pop_next(host, port)) {
    std::unordered_set<unsigned>::iterator i = s.find(port);
    ASSERT_TRUE(i != s.end());
    s.erase(i);
  }
  ASSERT_TRUE(s.empty());
}

TEST_F(dns_srv_data_test, mixed_weight) {
  const std::string p1("p1");
  const std::string p2("p2");
  std::unordered_set<unsigned> s = {12, 13, 14, 15};
  data.add(p1, 13, 1, 0);
  data.add(p1, 12, 1, 1);
  data.add(p2, 14, 2, 0);
  data.add(p2, 15, 2, 1);

  while (!data.pop_next(host, port)) {
    std::unordered_set<unsigned>::iterator i = s.find(port);
    ASSERT_TRUE(i != s.end());
    if (s.size() > 2)
      ASSERT_STREQ(host.c_str(), p1.c_str());
    else
      ASSERT_STREQ(host.c_str(), p2.c_str());
    s.erase(i);
  }
  ASSERT_TRUE(s.empty());
}

}  // namespace dns_srv_data_unittest
