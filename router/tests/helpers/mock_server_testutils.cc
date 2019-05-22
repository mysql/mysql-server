
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

#include <array>
#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"

// AddressSanitizer gets confused by the default, MemoryPoolAllocator
// Solaris sparc also gets crashes
using JsonAllocator = rapidjson::CrtAllocator;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, JsonDocument::AllocatorType>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

static std::string json_to_string(const JsonValue &json_doc) {
  JsonStringBuffer out_buffer;

  rapidjson::Writer<JsonStringBuffer> out_writer{out_buffer};
  json_doc.Accept(out_writer);
  return out_buffer.GetString();
}

void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                       const std::vector<uint16_t> &gr_node_ports) {
  JsonValue json_doc(rapidjson::kObjectType);
  JsonAllocator allocator;
  json_doc.AddMember("gr_id", JsonValue(gr_id.c_str(), gr_id.length()),
                     allocator);

  JsonValue gr_nodes_json(rapidjson::kArrayType);
  for (auto &gr_node : gr_node_ports) {
    JsonValue node(rapidjson::kArrayType);
    node.PushBack(JsonValue((int)gr_node), allocator);
    node.PushBack(JsonValue("ONLINE", strlen("ONLINE")), allocator);
    gr_nodes_json.PushBack(node, allocator);
  }
  json_doc.AddMember("gr_nodes", gr_nodes_json, allocator);

  const auto json_str = json_to_string(json_doc);

  EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}
