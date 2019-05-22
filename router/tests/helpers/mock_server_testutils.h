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

/**
 * Sets the metadata returned by the mock server.
 *
 * @param http_port mock server's http port where it services the http requests
 * @param gr_id replication group id to set
 * @param gr_node_ports vector with the ports of the nodes that mock server
 * should return
 */
void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                       const std::vector<uint16_t> &gr_node_ports);

#endif  // MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED
