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

#ifndef MYSQL_HARNESS_DESTINATION_ENDPOINT_INCLUDED
#define MYSQL_HARNESS_DESTINATION_ENDPOINT_INCLUDED

#include "harness_export.h"

#include <variant>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/local.h"

namespace mysql_harness {

class HARNESS_EXPORT DestinationEndpoint {
 public:
  using TcpType = net::ip::tcp::endpoint;
  using LocalType = local::stream_protocol::endpoint;

  explicit DestinationEndpoint() : ep_(TcpType()) {}
  explicit DestinationEndpoint(TcpType ep) : ep_(std::move(ep)) {}
  explicit DestinationEndpoint(LocalType ep) : ep_(std::move(ep)) {}

  bool is_tcp() const { return std::holds_alternative<TcpType>(ep_); }
  bool is_local() const { return !is_tcp(); }

  TcpType &as_tcp() { return std::get<TcpType>(ep_); }
  const TcpType &as_tcp() const { return std::get<TcpType>(ep_); }

  LocalType &as_local() { return std::get<LocalType>(ep_); }
  const LocalType &as_local() const { return std::get<LocalType>(ep_); }

  std::string str() const;

 private:
  std::variant<TcpType, LocalType> ep_;
};

}  // namespace mysql_harness

#endif
