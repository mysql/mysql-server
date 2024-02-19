/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_ROUTER_REQUIRE_INCLUDED
#define ROUTING_ROUTER_REQUIRE_INCLUDED

#include <optional>
#include <string>

#include "classic_query_sender.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/channel.h"
#include "mysqlrouter/classic_protocol_message.h"

class RouterRequire {
 public:
  struct Attributes {
    std::optional<bool> ssl;
    std::optional<bool> x509;
    std::optional<std::string> issuer;
    std::optional<std::string> subject;
  };

  static stdx::expected<void, classic_protocol::message::server::Error> enforce(
      Channel &client_channel, Attributes attrs);
};

class RouterRequireFetcher {
 public:
  using Result = stdx::expected<RouterRequire::Attributes,
                                classic_protocol::message::server::Error>;

  static void push_processor(MysqlRoutingClassicConnectionBase *connection,
                             Result &result);
};

#endif
