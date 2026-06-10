/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mysql/harness/destination_socket.h"

namespace mysql_harness {

stdx::expected<void, std::error_code> DestinationSocket::open(
    const mysql_harness::DestinationEndpoint &ep, int flags) {
  auto &io_ctx = get_executor().context();

  if (ep.is_local()) {
    if (!is_local()) {
      sock_ = LocalType{io_ctx};
    }
    return as_local().open(ep.as_local().protocol(), flags);
  }

  if (!is_tcp()) {
    sock_ = TcpType{io_ctx};
  }
  return as_tcp().open(ep.as_tcp().protocol(), flags);
}

stdx::expected<void, std::error_code> DestinationSocket::connect(
    const mysql_harness::DestinationEndpoint &ep) {
  auto &io_ctx = get_executor().context();

  if (ep.is_local()) {
    if (!is_local()) {
      // adjust the underlying socket-type as needed.
      sock_ = LocalType{io_ctx};
    }

    return as_local().connect(ep.as_local());
  }

  if (!is_tcp()) {
    sock_ = TcpType{io_ctx};
  }

  return as_tcp().connect(ep.as_tcp());
}

}  // namespace mysql_harness
