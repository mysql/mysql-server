/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include <ctime>  // time_t

#include "mysqlrouter/http_client.h"

class HttpTimeParsesTest : public ::testing::Test,
                           public ::testing::WithParamInterface<
                               std::tuple<const char *, time_t, const char *>> {
 protected:
  void SetUp() override {}
};

class HttpTimeThrowsTest : public ::testing::Test,
                           public ::testing::WithParamInterface<const char *> {
 protected:
  void SetUp() override {}
};

TEST_P(HttpTimeParsesTest, time_from_rfc5322_fixdate) {
  EXPECT_NO_THROW(
      EXPECT_THAT(time_from_rfc5322_fixdate(std::get<0>(GetParam())),
                  ::testing::Eq(std::get<1>(GetParam()))));

  char date_buf[30];
  EXPECT_NO_THROW(
      EXPECT_THAT(time_to_rfc5322_fixdate(std::get<1>(GetParam()), date_buf,
                                          sizeof(date_buf)),
                  ::testing::Eq(29)));

  // equal, if you ignore whitespace
  EXPECT_NO_THROW(
      EXPECT_THAT(date_buf, ::testing::StrEq(std::get<2>(GetParam()))));
}

INSTANTIATE_TEST_SUITE_P(
    HttpTimeParses, HttpTimeParsesTest,
    ::testing::Values(
        // parses a valid date
        std::make_tuple("Thu, 31 May 2018 15:18:20 GMT",
                        static_cast<time_t>(1527779900),
                        "Thu, 31 May 2018 15:18:20 GMT"),
        // whitespace get ignored
        std::make_tuple("Thu,  31  May  2018  15:18:20  GMT",
                        static_cast<time_t>(1527779900),
                        "Thu, 31 May 2018 15:18:20 GMT"),

        // other date
        std::make_tuple("Thu, 31 May 2018 05:18:20 GMT",
                        static_cast<time_t>(1527743900),
                        "Thu, 31 May 2018 05:18:20 GMT")));

TEST_P(HttpTimeThrowsTest, time_from_rfc5322_fixdate_p) {
  EXPECT_THROW(time_from_rfc5322_fixdate(GetParam()), std::out_of_range);
}

INSTANTIATE_TEST_SUITE_P(HttpTimeThrows, HttpTimeThrowsTest,
                         ::testing::Values(
                             // year too small
                             "Thu, 31 May 1899 15:18:20 GMT",

                             // wrong timezone
                             "Thu, 31 May 2018 5:18:20 UTC",

                             // throws at invalid weekday");
                             "Tho, 31 May 2018 15:18:20 GMT",

                             // throws at invalid month");
                             "Thu, 31 Mai 2018 15:18:20 GMT",

                             // throws at short year");
                             "Thu, 31 May 201 15:18:20 GMT",

                             // throws at long year
                             "Thu, 31 May 20188 15:18:20 GMT"));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
