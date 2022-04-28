/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/kqueue_io_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#ifdef HAVE_KQUEUE
#include <sys/socket.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

using namespace std::chrono_literals;

// check state after constructor.
//
// construct doesn't call open()
TEST(KqueueIoService, init) {
  net::kqueue_io_service io_svc;

  EXPECT_FALSE(io_svc.is_open());
}

// calling open again should fail.
TEST(KqueueIoService, open_already_open) {
  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  // pre-condition: construct calls open()
  ASSERT_TRUE(io_svc.is_open());

  EXPECT_EQ(
      io_svc.open(),
      stdx::make_unexpected(make_error_code(net::socket_errc::already_open)));
}

// calling open again should fail.
TEST(KqueueIoService, close) {
  net::kqueue_io_service io_svc;

  // pre-condition: construct calls open()
  ASSERT_TRUE(io_svc.open());
  ASSERT_TRUE(io_svc.is_open());

  EXPECT_TRUE(io_svc.close());

  EXPECT_FALSE(io_svc.is_open());
}

// check add and remove
TEST(KqueueIoService, add_interest) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);

  ASSERT_TRUE(res);

  auto fds = res.value();

  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  // 2nd add is ignored.
  auto poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  SCOPED_TRACE("// remove interest again");
  EXPECT_TRUE(io_svc.queue_remove_fd_interest(fds.first, EVFILT_READ));

  // 2nd add is ignored.
  poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

/**
 * adding multiple filters to the same file-descriptor works.
 */
TEST(KqueueIoService, add_interest_read_and_write) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);

  ASSERT_TRUE(res);

  auto fds = res.value();

  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// adding read-interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// adding write-interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  // EVFILT_WRITE should trigger
  auto poll_res = io_svc.poll_one(0ms);
  ASSERT_TRUE(poll_res);
  EXPECT_EQ(poll_res.value().fd, fds.first);
  EXPECT_EQ(poll_res.value().event, POLLOUT);

  // no more events
  poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.queue_remove_fd_interest(fds.first, EVFILT_READ));

  // no event triggers, but the filter is removed too
  poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.queue_remove_fd_interest(fds.first, EVFILT_WRITE));

  // as the EVFILT_WRITE triggered, we can't remove it anymore.
  poll_res = io_svc.poll_one(0ms);
  ASSERT_TRUE(poll_res);
  EXPECT_EQ(poll_res.value().fd, fds.first);
  EXPECT_EQ(poll_res.value().event, POLLERR);

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

/**
 * kqueue works with changelists:
 *
 * - add a filter twice, doesn't fail
 * - removing a filter twice, fails
 */
TEST(KqueueIoService, add_interest_read_and_read) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);

  auto fds = res.value();

  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// adding read interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// adding read interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  // 2nd add is ignored.
  auto poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.queue_remove_fd_interest(fds.first, EVFILT_READ));

  // removing the read-interest should succeed
  poll_res = io_svc.poll_one(0ms);
  ASSERT_FALSE(poll_res);
  EXPECT_EQ(poll_res.error(), make_error_code(std::errc::timed_out));

  // removing the read-interest a 2nd time should fail
  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.queue_remove_fd_interest(fds.first, EVFILT_READ));

  poll_res = io_svc.poll_one(0ms);
  ASSERT_TRUE(poll_res);
  EXPECT_EQ(poll_res.value().fd, fds.first);
  EXPECT_EQ(poll_res.value().event, POLLERR);

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

TEST(KqueueIoService, remove_fd_interest_from_empty) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);

  ASSERT_TRUE(res);

  auto fds = res.value();

  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  // queues the remove-fd-interest
  io_svc.queue_remove_fd_interest(fds.first, EVFILT_READ);

  // poll one will fail, if there was no EVFILT_READ that could be removed.
  io_svc.poll_one(100ms);

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

// check poll_one properly tracks the oneshot events.
TEST(KqueueIoService, poll_one) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);

  ASSERT_TRUE(res);

  auto fds = res.value();

  net::kqueue_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE(
      "// poll once which should fire, and remove the oneshot interest");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res);
  EXPECT_EQ(poll_res.value().fd, fds.first);
  EXPECT_EQ(poll_res.value().event, POLLOUT);

  SCOPED_TRACE("// poll again which should block");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res,
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));

  SCOPED_TRACE("// add write interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// poll again which should fire");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res);

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

#endif

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
