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

#ifndef MYSQL_REST_SERVICE_SUPPORTED_OPTIONS_INCLUDED
#define MYSQL_REST_SERVICE_SUPPORTED_OPTIONS_INCLUDED

#include <array>

static constexpr std::array mysql_rest_service_supported_options{
    "mysql_user",
    "mysql_user_data_access",
    "mysql_read_write_route",
    "mysql_read_only_route",
    "router_id",
    "metadata_refresh_interval",
    "developer",
    "developer_debug_port",
    "wait_for_metadata_schema_access",
    "level"};

#endif /* MYSQL_REST_SERVICE_SUPPORTED_OPTIONS_INCLUDED */
