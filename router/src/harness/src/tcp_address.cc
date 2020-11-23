/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "tcp_address.h"

#include <sstream>

#include "mysql/harness/net_ts/internet.h"

namespace mysql_harness {

std::string TCPAddress::str() const {
  std::ostringstream os;

  auto make_res = net::ip::make_address_v6(addr_.c_str());
  if (make_res) {
    // looks like a IPv6 address, wrap in []
    os << "[" << addr_ << "]";
  } else {
    os << addr_;
  }

  if (port_ > 0) {
    os << ":" << port_;
  }

  return os.str();
}

}  // namespace mysql_harness
