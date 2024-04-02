/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include <string>

#include "mrs/http/header_accept.h"

using HeaderAccept = mrs::http::HeaderAccept;

TEST(HttpHeaderAccept, accepts_all) {
  HeaderAccept sut{"*/*"};

  ASSERT_TRUE(sut.is_acceptable("application/json"));
  ASSERT_TRUE(sut.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_TRUE(sut.is_acceptable("text/html"));
  ASSERT_TRUE(sut.is_acceptable("application/xhtml+xml"));
  ASSERT_TRUE(sut.is_acceptable("application/xml"));
  ASSERT_TRUE(sut.is_acceptable("custom/x.custom"));
}

TEST(HttpHeaderAccept, accepts_only_one1) {
  HeaderAccept sut1{"application/json"};
  HeaderAccept sut2{"custom/x.custom"};

  ASSERT_TRUE(sut1.is_acceptable("application/json"));
  ASSERT_FALSE(sut1.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_FALSE(sut1.is_acceptable("text/html"));
  ASSERT_FALSE(sut1.is_acceptable("application/xhtml+xml"));
  ASSERT_FALSE(sut1.is_acceptable("application/xml"));
  ASSERT_FALSE(sut1.is_acceptable("custom/x.custom"));

  ASSERT_FALSE(sut2.is_acceptable("application/json"));
  ASSERT_FALSE(sut2.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_FALSE(sut2.is_acceptable("text/html"));
  ASSERT_FALSE(sut2.is_acceptable("application/xhtml+xml"));
  ASSERT_FALSE(sut2.is_acceptable("application/xml"));
  ASSERT_TRUE(sut2.is_acceptable("custom/x.custom"));
}

TEST(HttpHeaderAccept, accepts_only_specific_class) {
  HeaderAccept sut1{"application/*"};
  HeaderAccept sut2{"custom/*"};

  ASSERT_TRUE(sut1.is_acceptable("application/json"));
  ASSERT_TRUE(sut1.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_FALSE(sut1.is_acceptable("text/html"));
  ASSERT_TRUE(sut1.is_acceptable("application/xhtml+xml"));
  ASSERT_TRUE(sut1.is_acceptable("application/xml"));
  ASSERT_FALSE(sut1.is_acceptable("custom/x.custom"));

  ASSERT_FALSE(sut2.is_acceptable("application/json"));
  ASSERT_FALSE(sut2.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_FALSE(sut2.is_acceptable("text/html"));
  ASSERT_FALSE(sut2.is_acceptable("application/xhtml+xml"));
  ASSERT_FALSE(sut2.is_acceptable("application/xml"));
  ASSERT_TRUE(sut2.is_acceptable("custom/x.custom"));
}

TEST(HttpHeaderAccept, accepts_few_specific) {
  HeaderAccept sut1{
      "application/json, application/x.ieee754.client+json, custom/x.custom"};

  ASSERT_TRUE(sut1.is_acceptable("application/json"));
  ASSERT_TRUE(sut1.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_FALSE(sut1.is_acceptable("text/html"));
  ASSERT_FALSE(sut1.is_acceptable("application/xhtml+xml"));
  ASSERT_FALSE(sut1.is_acceptable("application/xml"));
  ASSERT_TRUE(sut1.is_acceptable("custom/x.custom"));
  ASSERT_FALSE(sut1.is_acceptable("custom/x.custo"));
  ASSERT_FALSE(sut1.is_acceptable("custo/x.custom"));
}

TEST(HttpHeaderAccept, accepts_few_all_and_specific_from_real_header) {
  HeaderAccept sut1{
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
      "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"};

  ASSERT_TRUE(sut1.is_acceptable("application/json"));
  ASSERT_TRUE(sut1.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_TRUE(sut1.is_acceptable("text/html"));
  ASSERT_TRUE(sut1.is_acceptable("application/xhtml+xml"));
  ASSERT_TRUE(sut1.is_acceptable("application/xml"));
  ASSERT_TRUE(sut1.is_acceptable("custom/x.custom"));
}

TEST(HttpHeaderAccept, accepts_few_specific_from_real_header) {
  HeaderAccept sut1{
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
      "webp,image/apng;q=0.8,application/signed-exchange;v=b3;q=0.7"};

  ASSERT_FALSE(sut1.is_acceptable("application/json"));
  ASSERT_FALSE(sut1.is_acceptable("application/x.ieee754.client+json"));
  ASSERT_TRUE(sut1.is_acceptable("text/html"));
  ASSERT_TRUE(sut1.is_acceptable("application/xhtml+xml"));
  ASSERT_TRUE(sut1.is_acceptable("application/xml"));
  ASSERT_FALSE(sut1.is_acceptable("custom/x.custom"));
  ASSERT_TRUE(sut1.is_acceptable("image/apng"));
}
