/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "http/server/bind.h"

namespace http {
namespace server {

Bind::Bind(io_context *io_context, const std::string &address,
           const uint16_t port)
    : context_{io_context} {
  auto resolve_res = resolver_.resolve(address, std::to_string(port));
  if (!resolve_res) {
    throw std::system_error(resolve_res.error(),
                            "resolving " + address + " failed");
  }

  for (auto const &resolved : resolve_res.value()) {
    auto open_res = socket_.open(resolved.endpoint().protocol());
    if (!open_res) {
      throw std::system_error(open_res.error(), "socket() failed");
    }

    socket_.native_non_blocking(true);
    auto setop_res = socket_.set_option(net::socket_base::reuse_address(true));
    if (!setop_res) {
      throw std::system_error(setop_res.error(),
                              "setsockopt(SO_REUSEADDR) failed");
    }
    setop_res = socket_.set_option(net::socket_base::keep_alive(true));
    if (!setop_res) {
      throw std::system_error(setop_res.error(),
                              "setsockopt(SO_KEEPALIVE) failed");
    }

    auto bind_res = socket_.bind(resolved.endpoint());
    if (!bind_res) {
      std::ostringstream ss;
      ss << "bind(" << resolved.endpoint() << ") failed";

      throw std::system_error(bind_res.error(), ss.str());
    }
    auto listen_res = socket_.listen(128);
    if (!listen_res) {
      throw std::system_error(setop_res.error(), "listen(128) failed");
    }

    //    auto sock_release_res = socket_.release();
    //    if (!sock_release_res) {
    //      throw std::system_error(sock_release_res.error(), "release()
    //      failed");
    //    }

    //    async_accept();
    return;
  }

  throw std::logic_error("No interface bound to socket.");
}

}  // namespace server
}  // namespace http
