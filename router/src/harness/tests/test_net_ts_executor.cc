/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/executor.h"

#include <gtest/gtest.h>

static_assert(net::is_executor<net::system_executor>::value,
              "net::system_executor MUST be an executor");

// a service MUST inherit from execution_context::service
class MockService : public net::execution_context::service {
 public:
  MockService(net::execution_context &owner)
      : net::execution_context::service(owner) {}

  // a service MUST have a key_type that is_same<> itself
  using key_type = MockService;

  void shutdown() noexcept override {}

  // a method to check it works and can be called
  bool ping() const { return true; }
};

/*
 * if a service doesn't exist in the context, make_service creates it
 */
TEST(TestExecutor, make_service) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::make_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));
}

/*
 * make_service throws, if a service of the same type already exists in the
 * context
 */
TEST(TestExecutor, make_service_dup_throws) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::make_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));

  EXPECT_THROW(net::make_service<MockService>(ctx),
               net::service_already_exists);
}

/*
 * has_service returns false if a service doesn't exist
 */
TEST(TestExecutor, has_service_not) {
  net::execution_context ctx;

  EXPECT_FALSE(net::has_service<MockService>(ctx));
}

/*
 * calling use_service if a service doesn't exists, creates it
 */
TEST(TestExecutor, use_service) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::use_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));
}

/*
 * calling use_service if it already exists, doesn't throw
 */
TEST(TestExecutor, use_service_dup_no_throws) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::use_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));

  EXPECT_NO_THROW(net::use_service<MockService>(ctx));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
