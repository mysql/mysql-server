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
#include "mysql/harness/net_ts/io_context.h"

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

#include "mock_io_service.h"
#include "mock_socket_service.h"

using ::testing::Return;

TEST(NetTS_io_context, construct) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
}

TEST(NetTS_io_context, stop) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  io_ctx.stop();
  EXPECT_TRUE(io_ctx.stopped());
  io_ctx.restart();
  EXPECT_FALSE(io_ctx.stopped());
}

TEST(NetTS_io_context, poll_empty) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  EXPECT_EQ(io_ctx.poll(), 0);
}

TEST(NetTS_io_context, poll_one_empty) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  EXPECT_EQ(io_ctx.poll_one(), 0);
}

TEST(NetTS_io_context, run_empty) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  EXPECT_EQ(io_ctx.run(), 0);
}

TEST(NetTS_io_context, run_one_empty) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  EXPECT_EQ(io_ctx.run_one(), 0);
}

TEST(NetTS_io_context, poll_io_service_remove_invalid_socket) {
  net::poll_io_service io_service;

  EXPECT_EQ(
      io_service.remove_fd(net::impl::socket::kInvalidSocket),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

TEST(NetTS_io_context, poll_io_service_add_invalid_socket) {
  net::poll_io_service io_service;

  EXPECT_EQ(
      io_service.add_fd_interest(net::impl::socket::kInvalidSocket,
                                 net::impl::socket::wait_type::wait_read),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

namespace net {
std::ostream &operator<<(std::ostream &os, net::fd_event fdev) {
  os << "fd=" << fdev.fd << ", event=" << fdev.event;

  return os;
}
}  // namespace net

TEST(NetTS_io_context, poll_io_service_poll_one_empty) {
  net::poll_io_service io_service;

  ASSERT_TRUE(io_service.open());
  using namespace std::chrono_literals;

  EXPECT_EQ(io_service.poll_one(1ms),
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));
}

TEST(NetTS_io_context, work_guard_blocks_run) {
  // prepare the io-service
  auto io_service = std::make_unique<MockIoService>();

  // succeed the open
  EXPECT_CALL(*io_service, open);

  // should result in a poll(-1) as a signal that we wanted block forever
  EXPECT_CALL(*io_service, poll_one(std::chrono::milliseconds(-1)))
      .WillRepeatedly(
          Return(stdx::make_unexpected(make_error_code(std::errc::timed_out))));

  net::io_context io_ctx(std::make_unique<MockSocketService>(),
                         std::move(io_service));

  // work guard is need to trigger the poll_one() as otherwise the run() would
  // just leave as there is no work to do without blocking
  auto work_guard = net::make_work_guard(io_ctx);

  // run should fail
  EXPECT_EQ(io_ctx.run(), 0);
}

TEST(NetTS_io_context, io_service_open_fails) {
  // prepare the io-service
  auto io_service = std::make_unique<MockIoService>();

  EXPECT_CALL(*io_service, open)
      .WillOnce(Return(stdx::make_unexpected(
          make_error_code(std::errc::too_many_files_open))));

  // no call to poll_one

  net::io_context io_ctx(std::make_unique<MockSocketService>(),
                         std::move(io_service));

  EXPECT_EQ(
      io_ctx.open_res(),
      stdx::make_unexpected(make_error_code(std::errc::too_many_files_open)));

  // work guard is need to trigger the poll_one() as otherwise the run() would
  // just leave as there is no work to do without blocking
  auto work_guard = net::make_work_guard(io_ctx);

  // run should fail
  EXPECT_EQ(io_ctx.run(), 0);
}

// net::is_executor_v<> chokes with solaris-ld on
//
//   test_net_ts_executor.cc.o: symbol
//   .XAKwqohC_Jhd06z.net::is_executor_v<net::system_executor>:
//   external symbolic relocation against non-allocatable section .debug_info;
//   cannot be processed at runtime: relocation ignored Undefined
//   first referenced symbol
//
//     .XAKwqohC_Jhd06z.net::is_executor_v<net::system_executor>
//
//   in file
//
//     .../test_net_ts_executor.cc.o
//
// using the long variant is_executor<T>::value instead
static_assert(net::is_executor<net::io_context::executor_type>::value,
              "io_context::executor_type MUST be an executor");

int main(int argc, char *argv[]) {
  net::impl::socket::init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
