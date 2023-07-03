/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <csignal>  // SIGPIPE
#include <iostream>

#include "mysql/harness/net_ts.h"
#include "mysql/harness/net_ts/impl/file.h"

#ifndef SPLICE_F_MOVE
#define SPLICE_F_MOVE 0
#endif

class Splicer {
 public:
  Splicer(net::ip::tcp::socket &sock_in, net::ip::tcp::socket &sock_out)
      : sock_in_{sock_in}, sock_out_{sock_out} {}

  stdx::expected<void, std::error_code> open() {
    close();

    auto res = net::impl::file::pipe();
    if (!res) return stdx::make_unexpected(res.error());

#if 0
    // get the size of the queue
    std::cerr << __LINE__ << ": "
              << net::impl::file::fcntl(res->at(0),
                                        net::impl::file::get_pipe_size())
                     .value()
              << std::endl;
#endif

    fds_ = *res;

    return {};
  }

  stdx::expected<size_t, std::error_code> read_some(size_t len) {
    auto res = net::impl::socket::splice_to_pipe(
        sock_in_.native_handle(), fds_.second, len, SPLICE_F_MOVE);
    if (res) {
      in_queue_ += *res;
    }

    return res;
  }

  stdx::expected<size_t, std::error_code> write_some(size_t len) {
    auto res = net::impl::socket::splice_from_pipe(
        fds_.first, sock_out_.native_handle(), len, SPLICE_F_MOVE);
    if (res) {
      in_queue_ -= *res;
    }
    return res;
  }

  size_t queued() const noexcept { return in_queue_; }

  bool empty() const noexcept { return in_queue_ == 0; }

  void close() {
    if (fds_.first != net::impl::file::kInvalidHandle)
      net::impl::file::close(fds_.first);
    fds_.first = net::impl::file::kInvalidHandle;
    if (fds_.second != net::impl::file::kInvalidHandle)
      net::impl::file::close(fds_.second);
    fds_.second = net::impl::file::kInvalidHandle;
  }

  ~Splicer() { close(); }

 private:
  net::ip::tcp::socket &sock_in_;
  net::ip::tcp::socket &sock_out_;
  std::pair<net::impl::file::file_handle_type,
            net::impl::file::file_handle_type>
      fds_{net::impl::file::kInvalidHandle, net::impl::file::kInvalidHandle};

  size_t in_queue_{0};
};

int main() {
  net::impl::socket::init();

#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  net::io_context io_ctx;
  net::ip::tcp::acceptor acceptor(io_ctx);

  net::ip::tcp::resolver resolver(io_ctx);
  auto resolve_res =
      resolver.resolve("", "3307", net::ip::tcp::resolver::passive);
  if (!resolve_res) {
    std::cerr << __LINE__
              << ": resolve() failed: " << resolve_res.error().message()
              << std::endl;
    return EXIT_FAILURE;
  }
  std::cerr << __LINE__
            << ": resolve()d as: " << resolve_res->begin()->endpoint()
            << std::endl;

  auto open_res = acceptor.open(resolve_res->begin()->endpoint().protocol());
  if (!open_res) {
    std::cerr << __LINE__ << ": open() failed: " << open_res.error().message()
              << std::endl;
    return EXIT_FAILURE;
  }

  auto bind_res = acceptor.bind(resolve_res->begin()->endpoint());
  if (!bind_res) {
    std::cerr << __LINE__ << ": bind() failed: (" << bind_res.error() << ") "
              << bind_res.error().message() << std::endl;
    return EXIT_FAILURE;
  }
  std::cerr << __LINE__ << ": bind() to " << acceptor.local_endpoint().value()
            << std::endl;

  acceptor.set_option(net::socket_base::reuse_address(true));
#ifdef TCP_FASTOPEN
  // acceptor.set_option(net::ip::tcp::fast_open(1));
#endif
  // acceptor.native_non_blocking(true);

  auto listen_res = acceptor.listen(128);
  if (!listen_res) {
    std::cerr << __LINE__ << ": listen() failed: (" << listen_res.error()
              << ") " << listen_res.error().message() << std::endl;
    return EXIT_FAILURE;
  }

  while (true) {
    auto accept_res = acceptor.accept();
    if (!accept_res) {
      std::cerr << __LINE__ << ": " << accept_res.error().message()
                << std::endl;
      break;
    } else {
      // unwrap
      auto client_conn = std::move(*accept_res);
      client_conn.native_non_blocking(true);

      std::cerr << __LINE__ << ": accepted(). "
                << "fd=" << client_conn.native_handle() << " connected "
                << client_conn.local_endpoint().value() << " to "
                << client_conn.remote_endpoint().value() << std::endl;

      auto backend_resolve_res = resolver.resolve("localhost", "3308");
      for (const auto &addr : backend_resolve_res.value()) {
        net::ip::tcp::socket server_conn(io_ctx);
        server_conn.open(addr.endpoint().protocol());
        auto connect_res = server_conn.connect(addr.endpoint());
        if (!connect_res) {
          std::cerr << __LINE__ << ": (" << __func__ << "): connect("
                    << addr.endpoint() << ") failed: (" << connect_res.error()
                    << ") " << connect_res.error().message() << std::endl;

          continue;
        }
        std::cerr << __LINE__ << " (" << __func__ << "): "
                  << "fd=" << server_conn.native_handle() << " connected "
                  << server_conn.local_endpoint().value() << " to "
                  << server_conn.remote_endpoint().value() << std::endl;

        // copy the data from client to server
#if 0
        std::array<char, 1024> buffers;

        auto recv_res = net::read(client_conn, net::buffer(buffers));
        if (!recv_res) {
          std::cerr << __LINE__ << ": recv() failed: (" << recv_res.error()
                    << ") " << recv_res.error().message() << std::endl;
        } else {
          std::cerr << __LINE__ << ": recv()ed: " << recv_res.value()
                    << std::endl;
        }

        auto write_res =
            net::write(server_conn, net::buffer(buffers, recv_res.value()));
        if (!write_res) {
          std::cerr << __LINE__ << ": write() failed: (" << write_res.error()
                    << ") " << write_res.error().message() << std::endl;
        } else {
          std::cerr << __LINE__ << ": write()ed: " << write_res.value()
                    << std::endl;
        }
#else
        Splicer splicer(client_conn, server_conn);
        splicer.open();

        while (client_conn.is_open() ||
               (!splicer.empty() && server_conn.is_open())) {
          if (client_conn.is_open()) {
            auto wait_res = client_conn.wait(net::socket_base::wait_read);
            if (!wait_res) {
              std::cerr << __LINE__ << ": read.wait() failed: ("
                        << wait_res.error() << ") "
                        << wait_res.error().message() << std::endl;
              client_conn.close();
            } else {
              // TODO: what if the ioctl() fails?
              auto bytes_available = client_conn.available().value();
              if (bytes_available == 0) {
                // client is done
                client_conn.close();
              } else {
                auto splice_read_res = splicer.read_some(bytes_available);
                if (!splice_read_res) {
                  std::cerr << __LINE__ << ": read.splice() failed: ("
                            << splice_read_res.error() << ") "
                            << splice_read_res.error().message() << std::endl;

                  client_conn.close();
                } else {
#if 0
                  std::cerr << __LINE__
                            << ": read.spliced()ed: " << splice_read_res.value()
                            << std::endl;
#endif
                }
              }
            }
          }

          if (!splicer.empty()) {
            auto splice_write_res = splicer.write_some(splicer.queued());
            if (!splice_write_res) {
              std::cerr << __LINE__ << ": write.splice() failed: ("
                        << splice_write_res.error() << ") "
                        << splice_write_res.error().message() << std::endl;
              server_conn.close();
            } else {
#if 0
              std::cerr << __LINE__
                        << ": write.spliced()ed: " << splice_write_res.value()
                        << std::endl;
#endif
            }
          }
        }
#endif
      }
    }
  }

  return 0;
}
