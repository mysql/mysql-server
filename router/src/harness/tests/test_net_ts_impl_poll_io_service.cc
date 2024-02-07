/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/poll_io_service.h"

#include <chrono>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "scope_guard.h"

#if defined(_WIN32)
#define AF_SOCKETPAIR AF_INET
#else
#define AF_SOCKETPAIR AF_UNIX
#endif

namespace net {
std::ostream &operator<<(std::ostream &os, net::fd_event e) {
  os << "(fd=" << e.fd << ", events=" << std::bitset<32>(e.event) << ")";

  return os;
}
}  // namespace net

// check state after constructor.
//
// construct doesn't call open()
TEST(PollIoService, init) {
  net::poll_io_service io_svc;

  EXPECT_FALSE(io_svc.is_open());
}

// calling open again should fail.
TEST(PollIoService, open_already_open) {
  net::poll_io_service io_svc;

  // after .open() succeeds, .is_open() must return true.
  //
  // pre-condition: construct calls open()
  ASSERT_TRUE(io_svc.open());

  ASSERT_TRUE(io_svc.is_open());

  // calling open again, should fail.
  EXPECT_EQ(io_svc.open(),
            stdx::unexpected(make_error_code(net::socket_errc::already_open)));
}

// calling open again should fail.
TEST(PollIoService, close) {
  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  ASSERT_TRUE(io_svc.is_open());

  EXPECT_TRUE(io_svc.close());

  EXPECT_FALSE(io_svc.is_open());
}

// check add and remove
TEST(PollIoService, add_interest) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN);

  SCOPED_TRACE("// remove interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), 0);

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

// check add twice
TEST(PollIoService, add_interest_read_and_write) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN);

  SCOPED_TRACE("// adding interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after add-write");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN | POLLOUT);

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLOUT);

  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLOUT));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), 0);

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

TEST(PollIoService, add_interest_read_and_read) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// check fd-interest is not found before adding interest");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);  // not registered yet

  SCOPED_TRACE("// adding interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN);

  SCOPED_TRACE("// adding interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after 2nd add-read");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()),
            std::bitset<32>(POLLIN | POLLOUT));

  SCOPED_TRACE("// remove read-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLIN));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()), std::bitset<32>(POLLOUT));

  SCOPED_TRACE("// remove write-interest again");
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLOUT));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(std::bitset<32>(interest_res.value()), std::bitset<32>());

  SCOPED_TRACE("// remove fd completely");
  EXPECT_TRUE(io_svc.remove_fd(fds.first));

  SCOPED_TRACE("// check fd-interest after remove");
  interest_res = io_svc.interest(fds.first);
  ASSERT_FALSE(interest_res);
}

// check remove_fd_interest fails if fd isn't registered yet.
TEST(PollIoService, remove_fd_interest_from_empty) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  EXPECT_EQ(
      io_svc.remove_fd_interest(fds.first, POLLIN),
      stdx::unexpected(make_error_code(std::errc::no_such_file_or_directory)));
}

// check poll_one properly tracks the oneshot events.
TEST(PollIoService, poll_one) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  using namespace std::chrono_literals;

  SCOPED_TRACE(
      "// poll once which should fire, and remove the oneshot interest");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();

  SCOPED_TRACE("// poll again which should block");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res, stdx::unexpected(make_error_code(std::errc::timed_out)));

  SCOPED_TRACE("// add write interest again");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));

  SCOPED_TRACE("// poll again which should fire");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res);
}

// check remove_fd fails if it isn't registered yet.
TEST(PollIoService, remove_fd_from_empty) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());
  EXPECT_EQ(
      io_svc.remove_fd(fds.first),
      stdx::unexpected(make_error_code(std::errc::no_such_file_or_directory)));

  net::impl::socket::close(fds.first);
  net::impl::socket::close(fds.second);
}

/**
 * one FD with multiple events ready at the same time.
 */
TEST(PollIoService, one_fd_many_events) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));
  SCOPED_TRACE("// add read interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-write");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN | POLLOUT);

  // make sure the 'wait_read' fires too.
  auto write_res = net::impl::socket::write(fds.second, ".", 1);
  ASSERT_TRUE(write_res) << write_res.error();
  EXPECT_EQ(*write_res, 1);

  using namespace std::chrono_literals;

  SCOPED_TRACE("// poll_one() should fire for the 1st event.");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();
  EXPECT_EQ(poll_res->fd, fds.first);

  SCOPED_TRACE("// remove interest on fd.");
  auto remove_res = io_svc.remove_fd(fds.first);
  ASSERT_TRUE(remove_res) << remove_res.error();

  SCOPED_TRACE(
      "// poll_one() should not fire the 2nd time as the fd is removed.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res, stdx::unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * one FD with multiple events ready at the same time.
 *
 * but remove interest along the way.
 */
TEST(PollIoService, one_fd_many_events_removed) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_write));
  SCOPED_TRACE("// add read interest");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_read));

  SCOPED_TRACE("// check fd-interest after add-write");
  auto interest_res = io_svc.interest(fds.first);
  ASSERT_TRUE(interest_res);
  EXPECT_EQ(interest_res.value(), POLLIN | POLLOUT);

  // make sure the 'wait_read' fires too.
  auto write_res = net::impl::socket::write(fds.second, ".", 1);
  ASSERT_TRUE(write_res) << write_res.error();
  EXPECT_EQ(*write_res, 1);

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
  ASSERT_EQ(poll_res, stdx::unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * POLLHUP is sent on 'socket-close' even if no event is waited for.
 */
TEST(PollIoService, hup_without_event_wanted) {
#if defined(__APPLE__) || defined(__sun)
  GTEST_SKIP() << "This platorm does not generate POLLHUP on closed sockets";
#endif

  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add write interest");
  {
    auto add_res =
        io_svc.add_fd_interest(fds.first, net::socket_base::wait_write);
    EXPECT_TRUE(add_res) << add_res.error();
  }

  SCOPED_TRACE("// check fd-interest after add-write");
  {
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res), std::bitset<32>(POLLOUT));
  }

  using namespace std::chrono_literals;

  SCOPED_TRACE("// poll_one() should fire for the 1st event.");
  auto poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();

  SCOPED_TRACE("// fd is still watched, but has no handler");
  {
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res), std::bitset<32>(0));
  }

  SCOPED_TRACE("// shutdown both sides of the socket, but keep it open.");
  {
    auto shutdown_res = net::impl::socket::shutdown(
        fds.first, static_cast<int>(net::socket_base::shutdown_send));
    ASSERT_TRUE(shutdown_res) << shutdown_res.error();
  }

  {
    auto shutdown_res = net::impl::socket::shutdown(
        fds.second, static_cast<int>(net::socket_base::shutdown_send));
    ASSERT_TRUE(shutdown_res) << shutdown_res.error();
  }

  SCOPED_TRACE("// poll_one() should NOT fire with a HUP event (yet).");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_FALSE(poll_res) << poll_res.value();

  {
    auto add_res =
        io_svc.add_fd_interest(fds.first, net::socket_base::wait_error);
    EXPECT_TRUE(add_res) << add_res.error();
  }

  SCOPED_TRACE("// check fd-interest after add-write");
  {
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res),
              std::bitset<32>(POLLHUP | POLLERR));
  }

  SCOPED_TRACE("// poll_one() should fire with a HUP event.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_TRUE(poll_res) << poll_res.error();

  net::fd_event expected_event{fds.first, POLLHUP};
  ASSERT_EQ(*poll_res, expected_event);

  // close the socket as it would trigger a POLLHUP on the next poll_one.
  EXPECT_TRUE(io_svc.remove_fd(fds.first));
  EXPECT_TRUE(net::impl::socket::close(fds.first));

  SCOPED_TRACE("// all events fired.");
  poll_res = io_svc.poll_one(100ms);
  ASSERT_EQ(poll_res, stdx::unexpected(make_error_code(std::errc::timed_out)));
}

/**
 * HUP, add/remove
 */
TEST(PollIoService, hup_add_remove) {
  auto res = net::impl::socket::socketpair(AF_SOCKETPAIR, SOCK_STREAM, 0);
  ASSERT_TRUE(res) << res.error();
  auto fds = *res;

  Scope_guard guard([&]() {
    net::impl::socket::close(fds.first);
    net::impl::socket::close(fds.second);
  });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add interest for HUP/ERR");
  EXPECT_TRUE(io_svc.add_fd_interest(fds.first, net::socket_base::wait_error));

  SCOPED_TRACE("// check fd-interest after add interest");
  {
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res),
              std::bitset<32>(POLLHUP | POLLERR));
  }

  // ok, and noop
  EXPECT_TRUE(io_svc.remove_fd_interest(fds.first, POLLHUP | POLLERR));

  SCOPED_TRACE("// check fd-interest after add interest");
  {
    // POLLHUP and POLLERR are always active and not added to the interest.
    auto interest_res = io_svc.interest(fds.first);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res), std::bitset<32>(0));
  }
}

/**
 * test how poll() reacts to delayed connect().
 */
TEST(PollIoService, connect_fail) {
  auto proto = net::ip::tcp::v4();

  auto sock_res =
      net::impl::socket::socket(proto.family(), proto.type(), proto.protocol());
  ASSERT_TRUE(sock_res) << sock_res.error();
  auto fd = *sock_res;

  ASSERT_TRUE(net::impl::socket::native_non_blocking(fd, true));

  // port 4 is unassigned.
  net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), 4);
  auto connect_res = net::impl::socket::connect(
      fd, reinterpret_cast<const sockaddr *>(ep.data()), ep.size());
  ASSERT_FALSE(connect_res);
  EXPECT_THAT(
      connect_res.error(),
      ::testing::AnyOf(
          make_error_condition(std::errc::operation_in_progress),  // Unix
          make_error_condition(std::errc::operation_would_block)   // Windows
          ))
      << connect_res.error().message();

  Scope_guard sock_guard([&]() { net::impl::socket::close(fd); });

  net::poll_io_service io_svc;

  ASSERT_TRUE(io_svc.open());

  SCOPED_TRACE("// add interest for OUT");
  EXPECT_TRUE(io_svc.add_fd_interest(fd, net::socket_base::wait_write));

  SCOPED_TRACE("// check fd-interest after add interest");
  {
    auto interest_res = io_svc.interest(fd);
    ASSERT_TRUE(interest_res);
    EXPECT_EQ(std::bitset<32>(*interest_res), std::bitset<32>(POLLOUT));
  }

  // Linux:   POLLOUT|POLLERR|POLLHUP -> POLLOUT, POLLERR, POLLHUP
  // Windows: POLLOUT|POLLERR|POLLHUP -> POLLOUT, POLLERR, POLLHUP
  // MacOS:   POLLHUP                 -> POLLOUT, POLLHUP
  // Solaris: POLLOUT                 -> POLLOUT
  {
    SCOPED_TRACE("// should have POLLOUT");
    auto poll_res = io_svc.poll_one(std::chrono::seconds(10));
    ASSERT_TRUE(poll_res) << poll_res.error();
    EXPECT_EQ(poll_res->fd, fd);
    EXPECT_EQ(poll_res->event, POLLOUT);
  }

#if defined(__linux__) || defined(_WIN32)
  {
    SCOPED_TRACE("// should have POLLERR");
    auto poll_res = io_svc.poll_one(std::chrono::seconds(0));
    ASSERT_TRUE(poll_res) << poll_res.error();
    EXPECT_EQ(poll_res->fd, fd);
    EXPECT_EQ(poll_res->event, POLLERR);
  }
#endif

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
  {
    SCOPED_TRACE("// should have POLLHUP");
    auto poll_res = io_svc.poll_one(std::chrono::seconds(0));
    ASSERT_TRUE(poll_res) << poll_res.error();
    EXPECT_EQ(poll_res->fd, fd);
    EXPECT_EQ(poll_res->event, POLLHUP);
  }
#endif

  SCOPED_TRACE("// get socket error");
  net::socket_base::error opt;
  socklen_t opt_len = opt.size(proto);
  auto opt_res = net::impl::socket::getsockopt(
      fd, opt.level(proto), opt.name(proto), opt.data(proto), &opt_len);
  ASSERT_TRUE(opt_res) << opt_res.error();

  std::error_code opt_ec{opt.value(), std::system_category()};

  EXPECT_EQ(opt_ec, make_error_condition(std::errc::connection_refused));

  SCOPED_TRACE("// no further events");
  {
    auto poll_res = io_svc.poll_one(std::chrono::seconds(0));
    ASSERT_FALSE(poll_res) << std::bitset<16>(poll_res->event);
  }
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
