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

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysqlrouter/connection_pool.h"
#include "router_test_helpers.h"  // init_windows_sockets

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/tls_context.h"
#include "stdx_expected_no_error.h"

#include "tcp_port_pool.h"

namespace {

TcpPortPool tcp_port_pool;

stdx::expected<void, std::error_code> socket_is_alive(
    net::impl::socket::native_handle_type native_handle) {
  std::array<net::impl::poll::poll_fd, 1> fds{{{native_handle, POLLIN, 0}}};
  auto poll_res = net::impl::poll::poll(fds.data(), fds.size(),
                                        std::chrono::milliseconds(0));
  if (!poll_res) {
    if (poll_res.error() != std::errc::timed_out) {
      // shouldn't happen, but if it does, ignore the socket.
      return stdx::unexpected(poll_res.error());
    }

    return {};
  }

  // there is data -> Error packet -> server closed the connection.
  return stdx::unexpected(make_error_code(net::stream_errc::eof));
}

stdx::expected<void, std::error_code> socket_is_alive(
    const ConnectionPool::ServerSideConnection &server_conn) {
  return socket_is_alive(server_conn.connection()->native_handle());
}

class PoolAdder {
 public:
  PoolAdder(net::io_context &io_ctx, ConnectionPool &pool,
            net::ip::tcp::endpoint server_ep)
      : io_ctx_(io_ctx), pool_(pool), server_ep_(server_ep) {}

  void add_new_connection() {
    net::ip::tcp::socket sock(io_ctx_);

    auto open_res = sock.open(server_ep_.protocol());
    ASSERT_NO_ERROR(open_res) << open_res.error().message();
    ASSERT_NO_ERROR(sock.native_non_blocking(true));
    auto connect_res = sock.connect(server_ep_);

    if (!connect_res) {
      auto ec = connect_res.error();
      ASSERT_TRUE(ec == std::errc::operation_in_progress ||
                  ec == std::errc::operation_would_block)
          << ec.message();

      auto wait_res = sock.wait(net::socket_base::wait_write);
      ASSERT_NO_ERROR(wait_res);

      net::socket_base::error sock_err;
      auto option_res = sock.get_option(sock_err);
      ASSERT_NO_ERROR(option_res);

      ASSERT_EQ(sock_err.value(), 0);
    }

    ConnectionPool::ServerSideConnection conn(
        std::make_unique<TcpConnection>(std::move(sock), server_ep_),
        SslMode::kPreferred, {});

    ++added_;
    pool_.add(std::move(conn));
  }

  void periodic_add_new_connection() {
    add_timer_.expires_after(std::chrono::milliseconds(100));
    add_timer_.async_wait([&](auto ec) {
      if (ec) {
        return;
      }

      add_new_connection();

      // next round.
      periodic_add_new_connection();
    });
  }

  [[nodiscard]] uint64_t added() const { return added_; }

 private:
  net::io_context &io_ctx_;
  ConnectionPool &pool_;

  net::steady_timer add_timer_{io_ctx_};

  uint64_t added_{};

  net::ip::tcp::endpoint server_ep_;
};

class StashAdder {
 public:
  StashAdder(net::io_context &io_ctx, ConnectionPool &pool,
             net::ip::tcp::endpoint server_ep)
      : io_ctx_(io_ctx), pool_(pool), server_ep_(server_ep) {}

  void add_new_connection() {
    net::ip::tcp::socket sock(io_ctx_);

    auto open_res = sock.open(server_ep_.protocol());
    ASSERT_NO_ERROR(open_res) << open_res.error().message();
    ASSERT_NO_ERROR(sock.native_non_blocking(true));
    auto connect_res = sock.connect(server_ep_);

    if (!connect_res) {
      auto ec = connect_res.error();
      ASSERT_TRUE(ec == std::errc::operation_in_progress ||
                  ec == std::errc::operation_would_block)
          << ec.message();

      auto wait_res = sock.wait(net::socket_base::wait_write);
      ASSERT_NO_ERROR(wait_res);

      net::socket_base::error sock_err;
      auto option_res = sock.get_option(sock_err);
      ASSERT_NO_ERROR(option_res);

      ASSERT_EQ(sock_err.value(), 0);
    }

    ConnectionPool::ServerSideConnection conn(
        std::make_unique<TcpConnection>(std::move(sock), server_ep_),
        SslMode::kPreferred, {});

    ++added_;
    pool_.stash(std::move(conn), nullptr, std::chrono::milliseconds(1000));
  }

  void periodic_add_new_connection() {
    add_timer_.expires_after(std::chrono::milliseconds(100));
    add_timer_.async_wait([&](auto ec) {
      if (ec) {
        return;
      }

      add_new_connection();

      // next round.
      periodic_add_new_connection();
    });
  }

  [[nodiscard]] uint64_t added() const { return added_; }

 private:
  net::io_context &io_ctx_;
  ConnectionPool &pool_;

  net::steady_timer add_timer_{io_ctx_};

  uint64_t added_{};

  net::ip::tcp::endpoint server_ep_;
};

}  // namespace

/**
 * check that concurrently taking a connection from the pool while the
 * ConnectionPool's "async_idle" handler is running isn't a problem.
 *
 * The test tries to run "pop_if()" in a similar interval as the
 * ConnectionPool's idle_time, trying to unstash when idle_timeout kicks in.
 * With TSAN, this is expected to be clean.
 */
TEST(ConnectionPoolTest, concurrent_pool_access) {
  net::io_context io_ctx;
  ConnectionPool pool(1024, std::chrono::milliseconds(1));

  auto test_run_time = std::chrono::seconds(1);

  std::atomic<uint64_t> popped{0};
  std::atomic<uint64_t> alive{0};
  std::atomic<uint64_t> dead{0};
  std::atomic<bool> is_done{false};

  net::ip::tcp::endpoint server_ep(net::ip::address_v4::loopback(),
                                   tcp_port_pool.get_next_available());

  net::ip::tcp::acceptor listener(io_ctx);

  ASSERT_NO_ERROR(listener.open(net::ip::tcp::v4()));
  auto bind_res = listener.bind(server_ep);
  ASSERT_NO_ERROR(bind_res) << bind_res.error().message();
  ASSERT_NO_ERROR(listener.listen(128));

  std::thread accept_thread([&]() {
    std::vector<net::ip::tcp::socket> client_sockets;

    listener.native_non_blocking(true);

    while (!is_done) {
      // check every 100ms if the test is going down.
      auto poll_time = std::chrono::milliseconds(100);

      std::array<net::impl::poll::poll_fd, 1> fds{
          {{listener.native_handle(), POLLIN, 0}}};
      auto poll_res = net::impl::poll::poll(fds.data(), fds.size(), poll_time);
      if (poll_res) {
        auto accept_res = listener.accept();
        if (accept_res) {
          client_sockets.push_back(std::move(*accept_res));
        }
      }

      // cleanup the closed client connections.

      auto cur = client_sockets.begin();
      for (; cur != client_sockets.end();) {
        if (socket_is_alive(cur->native_handle())) {
          ++cur;
        } else {
          std::array<char, 1024> buf;

          cur->read_some(net::buffer(buf));

          cur = client_sockets.erase(cur);
        }
      }
    }

    // tell the client we want to shutdown.
    for (auto &sock : client_sockets) {
      sock.shutdown(net::socket_base::shutdown_send);
    }

    // wait until the client closed.
    for (auto &sock : client_sockets) {
      std::array<char, 1024> buf;
      sock.read_some(net::buffer(buf));
    }
  });

  std::thread pop_thread([&]() {
    // try to take a connection from the pool while ConnectionPool in the
    // main-thread checks that the socket is still alive.
    while (!is_done) {
      auto pop_res = pool.pop_if(server_ep.address().to_string(),
                                 [](const auto &) { return true; });
      if (pop_res) {
        popped++;
        auto alive_res = socket_is_alive(*pop_res);
        if (alive_res) {
          ++alive;
        } else {
          ++dead;
          auto ec = alive_res.error();
          if (ec != net::stream_errc::eof) {
            // shouldn't happen.
            std::cerr << ec << "\n";
          }
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  PoolAdder adder(io_ctx, pool, server_ep);

  adder.add_new_connection();
  adder.periodic_add_new_connection();

  /* auto run_res = */ io_ctx.run_for(test_run_time);

  is_done = true;

  pop_thread.join();

  accept_thread.join();
}

/**
 * check that concurrently taking a connection from the stash while the
 * ConnectionPool's "async_idle" handler is running isn't a problem.
 *
 * The test tries to run "unstash" in a similar interval as the ConnectionPool's
 * idle_time, trying to unstash when idle_timeout kicks in. With TSAN, this is
 * expected to be clean.
 */
TEST(ConnectionPoolTest, concurrent_stash_access) {
  net::io_context io_ctx;

  ConnectionPool pool(0, std::chrono::milliseconds(1));

  auto test_run_time = std::chrono::seconds(1);

  std::atomic<uint64_t> popped{0};
  std::atomic<uint64_t> alive{0};
  std::atomic<uint64_t> dead{0};
  std::atomic<bool> is_done{false};

  net::ip::tcp::endpoint server_ep(net::ip::address_v4::loopback(),
                                   tcp_port_pool.get_next_available());

  net::ip::tcp::acceptor listener(io_ctx);

  ASSERT_NO_ERROR(listener.open(net::ip::tcp::v4()));
  auto bind_res = listener.bind(server_ep);
  ASSERT_NO_ERROR(bind_res) << bind_res.error().message();
  ASSERT_NO_ERROR(listener.listen(128));

  std::thread accept_thread([&]() {
    std::vector<net::ip::tcp::socket> client_sockets;

    listener.native_non_blocking(true);

    while (!is_done) {
      // check every 100ms if the test is going down.
      auto poll_time = std::chrono::milliseconds(100);

      std::array<net::impl::poll::poll_fd, 1> fds{
          {{listener.native_handle(), POLLIN, 0}}};
      auto poll_res = net::impl::poll::poll(fds.data(), fds.size(), poll_time);
      if (poll_res) {
        auto accept_res = listener.accept();
        if (accept_res) {
          client_sockets.push_back(std::move(*accept_res));
        }
      }

      // cleanup the closed client connections.

      auto cur = client_sockets.begin();

      for (; cur != client_sockets.end();) {
        if (socket_is_alive(cur->native_handle())) {
          ++cur;
        } else {
          std::array<char, 1024> buf;

          cur->read_some(net::buffer(buf));

          cur = client_sockets.erase(cur);
        }
      }
    }

    // tell the client we want to shutdown.
    for (auto &sock : client_sockets) {
      sock.shutdown(net::socket_base::shutdown_send);
    }

    // wait until the client closed.
    for (auto &sock : client_sockets) {
      std::array<char, 1024> buf;
      sock.read_some(net::buffer(buf));
    }
  });

  std::thread pop_thread([&]() {
    // take the connection from the stash again.
    while (!is_done) {
      auto pop_res =
          pool.unstash_mine(server_ep.address().to_string(), nullptr);
      if (pop_res) {
        popped++;
        auto alive_res = socket_is_alive(*pop_res);
        if (alive_res) {
          ++alive;
        } else {
          ++dead;
          auto ec = alive_res.error();
          if (ec != net::stream_errc::eof) {
            std::cerr << ec << "\n";
          }
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  StashAdder adder(io_ctx, pool, server_ep);

  adder.add_new_connection();
  adder.periodic_add_new_connection();

  /* auto run_res = */ io_ctx.run_for(test_run_time);

  is_done = true;

  pop_thread.join();

  accept_thread.join();

  do {
    // wait until everything is properly shut down.
    auto poll_res = io_ctx.poll_one();

    if (poll_res == 0) {
      break;
    }
  } while (true);
}

int main(int argc, char *argv[]) {
  TlsLibraryContext lib_ctx;
  init_windows_sockets();

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
