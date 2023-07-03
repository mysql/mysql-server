/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "context.h"

#include <cstring>
#include <memory>
#include <mutex>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/local.h"
#include "mysqlrouter/routing.h"
#include "protocol/base_protocol.h"

IMPORT_LOG_FUNCTIONS()

void MySQLRoutingContext::increase_info_active_routes() {
  ++info_active_routes_;
}

void MySQLRoutingContext::decrease_info_active_routes() {
  --info_active_routes_;
}

void MySQLRoutingContext::increase_info_handled_routes() {
  ++info_handled_routes_;
}
