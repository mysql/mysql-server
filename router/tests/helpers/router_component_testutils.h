/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_TESTUTILS_H_
#define _ROUTER_COMPONENT_TESTUTILS_H_

#include <chrono>
#include <string>
#include <vector>

#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "tcp_address.h"

std::string create_state_file_content(
    const std::string &cluster_type_specific_id,
    const std::string &clusterset_id,
    const std::vector<uint16_t> &metadata_servers_ports,
    const uint64_t view_id = 0);

std::string create_state_file_content(
    const std::vector<mysql_harness::TCPAddress> &metadata_servers,
    const std::string &cluster_type_specific_id,
    const std::string &clusterset_id, const uint64_t view_id = 0);

void check_state_file(
    const std::string &state_file, const mysqlrouter::ClusterType cluster_type,
    const std::string &expected_cluster_type_specific_id,
    const std::vector<uint16_t> expected_cluster_nodes,
    const uint64_t expected_view_id = 0,
    const std::string node_address = "127.0.0.1",
    std::chrono::milliseconds max_wait_time = std::chrono::milliseconds(5000));

int get_int_field_value(const std::string &json_string,
                        const std::string &field_name);
std::string get_str_field_value(const std::string &json_string,
                                const std::string &field_name);
std::string get_json_in_pretty_format(const std::string &json_string);

int get_transaction_count(const std::string &json_string);

int get_transaction_count(const uint16_t http_port);

bool wait_for_transaction_count(
    const uint16_t http_port, const int expected_queries_count,
    std::chrono::milliseconds timeout = std::chrono::seconds(30));

bool wait_for_transaction_count_increase(
    const uint16_t http_port, const int increment_by = 1,
    std::chrono::milliseconds timeout = std::chrono::seconds(30));

bool wait_connection_dropped(
    mysqlrouter::MySQLSession &session,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

size_t count_str_occurences(const std::string &s, const std::string &needle);

void make_bad_connection(uint16_t port);

void validate_config_stored_in_md(uint16_t http_port,
                                  const std::string &validation_schema);

std::string get_config_defaults_stored_in_md(uint16_t http_port);
std::string get_config_update_schema_stored_in_md(uint16_t http_port);

#endif  // _ROUTER_COMPONENT_TESTUTILS_H_
