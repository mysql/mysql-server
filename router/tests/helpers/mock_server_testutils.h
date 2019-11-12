/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED
#define MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED

#include <chrono>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

// AddressSanitizer gets confused by the default, MemoryPoolAllocator
// Solaris sparc also gets crashes
using JsonAllocator = rapidjson::CrtAllocator;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, JsonDocument::AllocatorType>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

/**
 * Converts the GR mock data to the JSON object.
 *
 * @param gr_id replication group id to set
 * @param gr_node_ports vector with the ports of the nodes that mock server
 * @param primary_id which node is the primary
 *
 * @return JSON object with the GR mock data.
 */
JsonValue mock_GR_metadata_as_json(const std::string &gr_id,
                                   const std::vector<uint16_t> &gr_node_ports,
                                   unsigned primary_id = 0);

/**
 * Sets the metadata returned by the mock server.
 *
 * @param http_port mock server's http port where it services the http requests
 * @param gr_id replication group id to set
 * @param gr_node_ports vector with the ports of the nodes that mock server
 * @param primary_id which node is the primary
 */
void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                       const std::vector<uint16_t> &gr_node_ports,
                       unsigned primary_id = 0);

void set_mock_bootstrap_data(
    uint16_t http_port, const std::string &cluster_name,
    const std::vector<std::pair<std::string, unsigned>> &gr_members_ports);

/**
 * Converts JSON object to string representation.
 */
std::string json_to_string(const JsonValue &json_doc);

#endif  // MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED
