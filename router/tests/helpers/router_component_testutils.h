/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_TESTUTILS_H_
#define _ROUTER_COMPONENT_TESTUTILS_H_

#include <chrono>
#include <string>
#include <vector>
#include "mysql_session.h"

std::string create_state_file_content(
    const std::string &replication_goup_id,
    const std::vector<uint16_t> &metadata_servers_ports,
    const unsigned view_id = 0);

void check_state_file(
    const std::string &state_file,
    const std::string &expected_group_replication_id,
    const std::vector<uint16_t> expected_cluster_nodes,
    const unsigned expected_view_id = 0,
    const std::string node_address = "127.0.0.1",
    std::chrono::milliseconds max_wait_time = std::chrono::milliseconds(5000));

int get_int_field_value(const std::string &json_string,
                        const std::string &field_name);

int get_transaction_count(const std::string &json_string);

bool wait_for_transaction_count(
    const uint16_t http_port, const int expected_queries_count,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

bool wait_for_transaction_count_increase(
    const uint16_t http_port, const int increment_by = 1,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

bool wait_connection_dropped(
    mysqlrouter::MySQLSession &session,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

#endif  // _ROUTER_COMPONENT_TESTUTILS_H_
