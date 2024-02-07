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

#ifndef ROUTING_CLASSIC_SESSION_TRACKER_INCLUDED
#define ROUTING_CLASSIC_SESSION_TRACKER_INCLUDED

#include <string>
#include <system_error>
#include <utility>  // pair
#include <vector>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_session_track.h"

stdx::expected<std::vector<std::pair<std::string, std::string>>,
               std::error_code>
session_trackers_to_string(net::const_buffer session_trackers,
                           classic_protocol::capabilities::value_type caps);

#endif
