/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/linux_epoll_io_service.h"

#include <chrono>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#ifdef HAVE_EPOLL
#include <sys/socket.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "scope_guard.h"

namespace net {
std::ostream &operator<<(std::ostream &os, net::fd_event e) {
  os << "(fd=" << e.fd << ", events=" << std::bitset<32>(e.event) << ")";

  return os;
}
}  // namespace net

// check state after constructor.
//
// construct doesn't call open()
TEST(LinuxEpollIoService, init) {
  net::linux_epoll_io_service io_svc;

  EXPECT_FALSE(io_svc.is_open());
}

// calling open again should fail.
TEST(LinuxEpollIoService, open_already_open) {
  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  // pre-condition: construct calls open()
  ASSERT_TRUE(io_svc.is_open());

  EXPECT_EQ(
      io_svc.open(),
      stdx::make_unexpected(make_error_code(net::socket_errc::already_open)));
}

// calling open again should fail.
TEST(LinuxEpollIoService, close) {
  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  ASSERT_TRUE(io_svc.is_open());

  EXPECT_TRUE(io_svc.close());

  EXPECT_FALSE(io_svc.is_open());
}

// check add and remove
TEST(LinuxEpollIoService, add_interest) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// remove interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, EPOLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

// check add twice
TEST(LinuxEpollIoService, add_interest_read_and_write) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// adding interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after add-write");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, EPOLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLOUT | EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, EPOLLOUT));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

TEST(LinuxEpollIoService, add_interest_read_and_read) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLET | EPOLLONESHOT);

  SCOPED_TRACE("// adding interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after 2nd add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()),
            std::bitset<32>(EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT));

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, EPOLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()),
            std::bitset<32>(EPOLLOUT | EPOLLET | EPOLLONESHOT));

  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, EPOLLOUT));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()),
            std::bitset<32>(EPOLLET | EPOLLONESHOT));

  SCOPED_TRACE("// remove fd completely");
  auto remove_res = io_svc.remove_fd(fds.first);
  EXPECT_TRUE(remove_res) << remove_res.error();

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

// check remove_fd_interest fails if fd isn't registered yet.
TEST(LinuxEpollIoService, remove_fd_interest_from_empty) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  EXPECT_EQ(io_svc.remove_fd_interest(fds.first, EPOLLIN),
            stdx::make_unexpected(
                make_error_code(std::errc::no_such_file_or_directory)));
}

// check poll_one properly tracks the oneshot events.
TEST(LinuxEpollIoService, poll_one) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  using namespace std::chrono_literals;

  SCOPED_TRACE(
      "// poll once which should fire, and remove the oneshot interest");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res);

  SCOPED_TRACE("// poll again which should block");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res,
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));

  SCOPED_TRACE("// add write interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// poll again which should fire");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res);
}

// check remove_fd fails if it isn't registered yet.
TEST(LinuxEpollIoService, remove_fd_from_empty) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  EXPECT_EQ(io_svc.remove_fd(fds.first),
            stdx::make_unexpected(
                make_error_code(std::errc::no_such_file_or_directory)));

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

/**
 * one FD with multiple events ready at the same time.
 */
TEST(LinuxEpollIoService, one_fd_many_events) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));
  SCOPED_TRACE("// add read interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-write");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT);

  // make sure the 'wait_read' fires too.
  EXPECT_EQ(::write(fds.second, ".", 1), 1);

  using namespace std::chrono_literals;

  SCOPED_TRACE("// poll_one() should fire for the 1st event.");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  EXPECT_EQ(poll_res->fd, fds.first);

  SCOPED_TRACE("// poll_one() should fire a 2nd time for the other event.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  EXPECT_EQ(poll_res->fd, fds.first);

  SCOPED_TRACE("// all events fired.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res,
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * one FD with multiple events ready at the same time.
 *
 * but remove interest along the way.
 */
TEST(LinuxEpollIoService, one_fd_many_events_removed) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));
  SCOPED_TRACE("// add read interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-write");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT);

  // make sure the 'wait_read' fires too.
  EXPECT_EQ(::write(fds.second, ".", 1), 1);

  using namespace std::chrono_literals;

  SCOPED_TRACE("// poll_one() should fire for the 1st event.");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  EXPECT_EQ(poll_res->fd, fds.first);

  SCOPED_TRACE("// poll_one() should fire a 2nd time for the other event.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  EXPECT_EQ(poll_res->fd, fds.first);

  auto remove_res = io_svc.remove_fd(fds.first);
  ASSERT_TRUE(remove_res) << remove_res.error();

  SCOPED_TRACE(
      "// poll_one() should not fire the 2nd time as the fd is removed.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res,
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * EPOLLHUP is sent on socket-close even if no event is waited for.
 */
TEST(LinuxEpollIoService, hup_without_event_wanted) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after add-write");
  {
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(interest_res.value(), EPOLLOUT | EPOLLET | EPOLLONESHOT);
  }

  using namespace std::chrono_literals;

  SCOPED_TRACE("// poll_one() should fire for the 1st event.");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();

  SCOPED_TRACE("// fd is not watched");
  {
    // known, but no interest.
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);

    EXPECT_EQ(std::bitset<32>(interest_res.value()),
              std::bitset<32>(EPOLLET | EPOLLONESHOT));
  }

  SCOPED_TRACE("// shutdown the socket, but keep it open.");
  net::impl::socket::shutdown(fds.first, SHUT_RDWR);

  SCOPED_TRACE("// poll_one() should not fire with a HUP event (yet).");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_FALSE(poll_res) << poll_res.value();

  auto add_res =
      io_svc.add_fd_interest(fds.first, net::socket_base::wait_error);
  EXPECT_TRUE(add_res) << add_res.error();

  SCOPED_TRACE("// poll_one() should fire with a HUP event.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  net::fd_event expected_event{fds.first, EPOLLHUP};
  ASSERT_EQ(*poll_res, expected_event);

  // close the socket as it would trigger a EPOLLHUP on the next poll_one.
  EXPECT_TRUE(io_svc.remove_fd(fds.first));
  EXPECT_TRUE(net::impl::socket::close(fds.first));

  SCOPED_TRACE("// all events fired.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res,
            stdx::make_unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * HUP, add/remove
 */
TEST(LinuxEpollIoService, hup_add_remove) {
  auto res = net::impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_TRUE(res);
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::linux_epoll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add interest for HUP/ERR");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_error));

  SCOPED_TRACE("// check fd-interest after add interest");
  {
    // EPOLLHUP and EPOLLERR are always active and not added to the interest.
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res),
              std::bitset<32>(EPOLLET | EPOLLONESHOT | EPOLLHUP | EPOLLERR));
  }

  {
    auto del_res = io_svc.remove_fd_interest(fds.first, EPOLLHUP | EPOLLERR);
    ASSERT_TRUE(del_res) << del_res.error();
  }

  SCOPED_TRACE("// check fd-interest after remove interest");
  {
    // known, but no interest.
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(interest_res.value()),
              std::bitset<32>(EPOLLET | EPOLLONESHOT));
  }
}
#endif

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
