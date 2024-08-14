/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <iostream>  // cerr

#include "mysql/harness/net_ts.h"

class Connection {
 public:
  Connection(net::ip::tcp::socket &&conn)
      : conn_{std::move(conn)},
        read_buffer_{net::buffer(read_buffer_storage_)} {}

  void async_read_all() {
    conn_.async_receive(read_buffer_, [this](std::error_code ec,
                                             size_t /* bytes_transferred */) {
      if (ec) {
        if (ec != net::stream_errc::eof) {
          std::cerr << __LINE__ << ": "
                    << "failed: " << ec << std::endl;
        }

        // the socket is closed, we can close too without waiting
        conn_.close();
        return;  // something failed
      }

      //      std::cerr << __LINE__ << ": recv()ed: " <<
      //      bytes_transferred << std::endl;

      // if there is more, read that too until we reach EOF
      async_read_all();

      return;
    });
  }

 private:
  net::ip::tcp::socket conn_;

  std::array<char, 1024 * 1024> read_buffer_storage_;
  net::mutable_buffer read_buffer_;
};

class Drainer {
 public:
  Drainer(net::io_context &io_ctx)
      : io_ctx_{io_ctx}, acceptor_{io_ctx}, ac_{acceptor_, connections_} {}

  stdx::expected<net::ip::tcp::resolver::results_type, std::error_code> resolve(
      const std::string &hostname, const std::string &service) {
    net::ip::tcp::resolver resolver(io_ctx_);
    return resolver.resolve(hostname, service, net::ip::tcp::resolver::passive);
  }

  stdx::expected<void, std::error_code> open(const net::ip::tcp::endpoint &ep) {
#ifdef SOCK_NONBLOCK
    int open_flags = SOCK_NONBLOCK;
#else
    int open_flags = 0;
#endif

    // try to set the non-blocking mode on the socket right from the start,
    // saving a syscall later for ioctl() later
    auto open_res = acceptor_.open(ep.protocol(), open_flags);
    if (!open_res) return open_res;

    acceptor_.set_option(net::socket_base::reuse_address(true));
#ifdef TCP_FASTOPEN
    acceptor_.set_option(net::ip::tcp::fast_open(0));
#endif
    if (!acceptor_.native_non_blocking()) acceptor_.native_non_blocking(true);

    return {};
  }

  stdx::expected<void, std::error_code> bind(const net::ip::tcp::endpoint &ep) {
    return acceptor_.bind(ep);
  }

  stdx::expected<void, std::error_code> listen() {
    return acceptor_.listen(128);
  }

  class Acceptor {
   public:
    Acceptor(net::ip::tcp::acceptor &acceptor,
             std::list<Connection> &connections)
        : acceptor_(acceptor), connections_(connections) {}

    void operator()(std::error_code ec) {
      if (ec) return;
#ifdef SOCK_NONBLOCK
      int open_flags = SOCK_NONBLOCK;
#else
      int open_flags = 0;
#endif

      for (;;) {
        auto res = acceptor_.accept(open_flags);
        if (res) {
          connections_.emplace_back(std::move(*res));
          connections_.back().async_read_all();
        } else if (res.error() == std::errc::operation_would_block) {
          acceptor_.async_wait(net::socket_base::wait_read, std::move(*this));
          return;
        } else {
          // error ... wanna log it?
          return;
        }
      }
    }

   private:
    net::ip::tcp::acceptor &acceptor_;
    std::list<Connection> &connections_;
  };

  void start_accept() { ac_({}); }

 private:
  net::io_context &io_ctx_;

  std::list<Connection> connections_;

  net::ip::tcp::acceptor acceptor_;

  Acceptor ac_;
};

int main() {
  net::impl::socket::init();

  net::io_context io_ctx;

  Drainer drainer(io_ctx);

  auto resolve_res = drainer.resolve("", "3308");
  if (!resolve_res) {
    std::cerr << __LINE__
              << ": resolve() failed: " << resolve_res.error().message()
              << std::endl;
    return EXIT_FAILURE;
  }

  auto open_res = drainer.open(resolve_res->begin()->endpoint());
  if (!open_res) {
    std::cerr << __LINE__ << ": open() failed: " << open_res.error().message()
              << std::endl;
    return EXIT_FAILURE;
  }

  auto bind_res = drainer.bind(resolve_res->begin()->endpoint());
  if (!bind_res) {
    std::cerr << __LINE__ << ": bind() failed: (" << bind_res.error() << ") "
              << bind_res.error().message() << std::endl;
    return EXIT_FAILURE;
  }
  drainer.listen();
  drainer.start_accept();

  io_ctx.run();

  return 0;
}
