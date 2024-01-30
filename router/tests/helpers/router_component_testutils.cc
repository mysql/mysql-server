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

#include "router_component_testutils.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <gmock/gmock.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include <chrono>
#include <fstream>
#include <thread>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/mock_server_rest_client.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION
#include "router_test_helpers.h"

namespace {
// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonSchemaDocument =
    rapidjson::GenericSchemaDocument<JsonValue, rapidjson::CrtAllocator>;
using JsonSchemaValidator =
    rapidjson::GenericSchemaValidator<JsonSchemaDocument>;
}  // namespace

using namespace std::chrono_literals;
using native_handle_type = net::impl::socket::native_handle_type;

std::string create_state_file_content(
    const std::string &cluster_type_specific_id,
    const std::string &clusterset_id,
    const std::vector<uint16_t> &metadata_servers_ports,
    const uint64_t view_id) {
  std::vector<mysql_harness::TCPAddress> metadata_servers;
  for (const auto port : metadata_servers_ports) {
    metadata_servers.emplace_back("127.0.0.1", port);
  }
  return create_state_file_content(metadata_servers, cluster_type_specific_id,
                                   clusterset_id, view_id);
}

std::string create_state_file_content(
    const std::vector<mysql_harness::TCPAddress> &metadata_servers,
    const std::string &cluster_type_specific_id,
    const std::string &clusterset_id, const uint64_t view_id /*= 0*/) {
  std::string metadata_servers_str;
  for (auto [i, metadata_server] : stdx::views::enumerate(metadata_servers)) {
    metadata_servers_str += "\"mysql://" + metadata_server.address() + ":" +
                            std::to_string(metadata_server.port()) + "\"";
    if (i < metadata_servers.size() - 1) metadata_servers_str += ",";
  }
  std::string view_id_str;
  if (view_id > 0) view_id_str = R"(, "view-id":)" + std::to_string(view_id);
  std::string cluster_id;
  if (!cluster_type_specific_id.empty()) {
    cluster_id =
        R"("group-replication-id": ")" + cluster_type_specific_id + R"(",)";
  }
  if (!clusterset_id.empty()) {
    cluster_id += (R"("clusterset-id": ")" + clusterset_id + R"(",)");
  }

  const std::string version = clusterset_id.empty() ? "1.1.0" : "1.0.0";

  // clang-format off
  const std::string result =
    "{"
       R"("version": ")" + version + R"(",)"
       R"("metadata-cache": {)"
         + cluster_id +
         R"("cluster-metadata-servers": [)" + metadata_servers_str + "]"
         + view_id_str +
        "}"
      "}";
  // clang-format on

  return result;
}

#define CHECK_TRUE(expr) \
  if (!(expr)) return false

static bool check_state_file_helper(
    const std::string &state_file_content,
    const mysqlrouter::ClusterType cluster_type,
    const std::string &expected_cluster_type_specific_id,
    const std::vector<uint16_t> expected_cluster_nodes,
    const uint64_t expected_view_id /*= 0*/,
    const std::string node_address /*= "127.0.0.1"*/) {
  JsonDocument json_doc;
  if (json_doc.Parse<0>(state_file_content.c_str()).HasParseError())
    return false;

  const std::string kExpectedVersion =
      cluster_type == mysqlrouter::ClusterType::GR_CS ? "1.1.0" : "1.0.0";

  CHECK_TRUE(json_doc.HasMember("version"));
  CHECK_TRUE(json_doc["version"].IsString());
  CHECK_TRUE(kExpectedVersion == json_doc["version"].GetString());

  CHECK_TRUE(json_doc.HasMember("metadata-cache"));
  CHECK_TRUE(json_doc["metadata-cache"].IsObject());

  auto metadata_cache_section = json_doc["metadata-cache"].GetObject();

  const std::string cluster_type_specific_id_field =
      cluster_type == mysqlrouter::ClusterType::GR_CS ? "clusterset-id"
                                                      : "group-replication-id";

  CHECK_TRUE(
      metadata_cache_section.HasMember(cluster_type_specific_id_field.c_str()));
  CHECK_TRUE(metadata_cache_section[cluster_type_specific_id_field.c_str()]
                 .IsString());
  CHECK_TRUE(expected_cluster_type_specific_id ==
             metadata_cache_section[cluster_type_specific_id_field.c_str()]
                 .GetString());

  if (expected_view_id > 0) {
    CHECK_TRUE(metadata_cache_section.HasMember("view-id"));
    CHECK_TRUE(metadata_cache_section["view-id"].IsInt());
    CHECK_TRUE(expected_view_id ==
               metadata_cache_section["view-id"].GetUint64());
  }

  CHECK_TRUE(metadata_cache_section.HasMember("cluster-metadata-servers"));
  CHECK_TRUE(metadata_cache_section["cluster-metadata-servers"].IsArray());
  auto cluster_nodes =
      metadata_cache_section["cluster-metadata-servers"].GetArray();
  CHECK_TRUE(expected_cluster_nodes.size() == cluster_nodes.Size());
  for (unsigned i = 0; i < cluster_nodes.Size(); ++i) {
    CHECK_TRUE(cluster_nodes[i].IsString());
    const std::string expected_cluster_node =
        "mysql://" + node_address + ":" +
        std::to_string(expected_cluster_nodes[i]);
    CHECK_TRUE(expected_cluster_node == cluster_nodes[i].GetString());
  }

  return true;
}

void check_state_file(const std::string &state_file,
                      const mysqlrouter::ClusterType cluster_type,
                      const std::string &expected_cluster_type_specific_id,
                      const std::vector<uint16_t> expected_cluster_nodes,
                      const uint64_t expected_view_id /*= 0*/,
                      const std::string node_address /*= "127.0.0.1"*/,
                      std::chrono::milliseconds max_wait_time /*= 5000*/) {
  bool result = false;
  std::string state_file_content;
  auto kRetryStep = 50ms;
  if (getenv("WITH_VALGRIND")) {
    max_wait_time *= 10;
    kRetryStep *= 10;
  }
  do {
    state_file_content = get_file_output(state_file);
    result = check_state_file_helper(
        state_file_content, cluster_type, expected_cluster_type_specific_id,
        expected_cluster_nodes, expected_view_id, node_address);
    if (!result) {
      std::this_thread::sleep_for(kRetryStep);
      max_wait_time -= kRetryStep;
    }
  } while ((!result) && (max_wait_time > kRetryStep));

  if (!result) {
    std::string expected_cluster_nodes_str;
    for (size_t i = 0; i < expected_cluster_nodes.size(); ++i) {
      expected_cluster_nodes_str +=
          std::to_string(expected_cluster_nodes[i]) + " ";
    }

    FAIL() << "Unexpected state file content." << std::endl
           << "cluster_type_specific_id: " << expected_cluster_type_specific_id
           << std::endl
           << "expected_cluster_nodes: " << expected_cluster_nodes_str
           << std::endl
           << "expected_view_id: " << expected_view_id << std::endl
           << "node_address: " << node_address << std::endl
           << "state_file_content: " << state_file_content;
  }

  // check that we have write access to the file
  // just append it with an empty line, that will not break it
  EXPECT_NO_THROW({
    std::ofstream ofs(state_file, std::ios::app);
    ofs << "\n";
  });
}

int get_int_field_value(const std::string &json_string,
                        const std::string &field_name) {
  rapidjson::Document json_doc;
  json_doc.Parse(json_string.c_str());
  if (!json_doc.HasMember(field_name.c_str())) {
    // that can mean this has not been set yet
    return 0;
  }

  if (!json_doc[field_name.c_str()].IsInt()) {
    // that can mean this has not been set yet
    return 0;
  }

  return json_doc[field_name.c_str()].GetInt();
}

std::string get_str_field_value(const std::string &json_string,
                                const std::string &field_name) {
  rapidjson::Document json_doc;
  json_doc.Parse(json_string.c_str());
  if (!json_doc.HasMember(field_name.c_str())) {
    // that can mean this has not been set yet
    return "";
  }

  if (!json_doc[field_name.c_str()].IsString()) {
    // that can mean this has not been set yet
    return "";
  }

  return json_doc[field_name.c_str()].GetString();
}

std::string get_json_in_pretty_format(const std::string &json_string) {
  rapidjson::Document json_doc;
  json_doc.Parse(json_string.c_str());

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  json_doc.Accept(writer);

  return buffer.GetString();
}

int get_transaction_count(const std::string &json_string) {
  return get_int_field_value(json_string, "transaction_count");
}

int get_transaction_count(const uint16_t http_port) {
  std::string server_globals =
      MockServerRestClient(http_port).get_globals_as_json_string();

  return get_transaction_count(server_globals);
}

bool wait_for_transaction_count(const uint16_t http_port,
                                const int expected_queries_count,
                                std::chrono::milliseconds timeout) {
  const std::chrono::milliseconds kStep = 20ms;
  do {
    std::string server_globals =
        MockServerRestClient(http_port).get_globals_as_json_string();
    if (get_transaction_count(server_globals) >= expected_queries_count)
      return true;
    std::this_thread::sleep_for(kStep);
    timeout -= kStep;
  } while (timeout > 0ms);

  return false;
}

bool wait_for_transaction_count_increase(const uint16_t http_port,
                                         const int increment_by,
                                         std::chrono::milliseconds timeout) {
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }
  std::string server_globals =
      MockServerRestClient(http_port).get_globals_as_json_string();
  int expected_queries_count =
      get_transaction_count(server_globals) + increment_by;

  return wait_for_transaction_count(http_port, expected_queries_count, timeout);
}

bool wait_connection_dropped(mysqlrouter::MySQLSession &session,
                             std::chrono::milliseconds timeout) {
  auto kStep = 50ms;
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
    kStep *= 5;
  }

  do {
    try {
      session.query_one("select @@port");
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      // we expect "connection failed" or  "lost connection during query"
      if (e.code() == 2003 || e.code() == 2013) return true;

      throw e;
    }

    std::this_thread::sleep_for(kStep);
    timeout -= kStep;
  } while (timeout >= 0ms);

  return false;
}

size_t count_str_occurences(const std::string &s, const std::string &needle) {
  if (needle.length() == 0) return 0;
  size_t result = 0;
  for (size_t pos = s.find(needle); pos != std::string::npos;
       pos = s.find(needle, pos + needle.length())) {
    ++result;
  }
  return result;
}

void make_bad_connection(uint16_t port) {
  net::io_context io_ctx;
  net::ip::tcp::socket sock(io_ctx);

  net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), port);

  auto connect_res = sock.connect(ep);
  if (!connect_res) throw std::system_error(connect_res.error());

  // MySQL protocol handshake phase
  // To simplify code, instead of alternating between reading and writing
  // protocol packets, we write a lot of garbage upfront, and then read
  // whatever Router sends back. Router will read what we wrote in chunks,
  // in between its writes, thinking they're replies to its handshake packets.
  // Eventually it will finish the handshake with error and disconnect.
  std::vector<char> bogus_data(3, 0);
  const auto write_res = net::write(sock, net::buffer(bogus_data));
  if (!write_res) throw std::system_error(write_res.error(), "write() failed");

  sock.shutdown(net::socket_base::shutdown_type::shutdown_send);

  std::array<char, 1024> buf;
  while (net::read(sock, net::buffer(buf))) {
    // read until error.
  }
}

static void validate_json_against_schema(const JsonSchemaDocument &schema,
                                         const JsonDocument &json) {
  JsonSchemaValidator validator(schema);
  if (!json.Accept(validator)) {
    // validation failed - throw an error with info of where the problem is
    rapidjson::StringBuffer sb_schema;
    validator.GetInvalidSchemaPointer().StringifyUriFragment(sb_schema);
    rapidjson::StringBuffer sb_json;
    validator.GetInvalidDocumentPointer().StringifyUriFragment(sb_json);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);

    // Output {"project":"rapidjson","stars":11}
    std::cout << buffer.GetString() << std::endl;
    FAIL() << std::string("Failed schema directive: ") + sb_schema.GetString() +
                  "\nFailed schema keyword:   " +
                  validator.GetInvalidSchemaKeyword() +
                  "\nFailure location in validated document: " +
                  sb_json.GetString() + "\n" + buffer.GetString() + "\n";
  }
}

static void validate_json_against_schema(const std::string &json,
                                         const std::string &schema) {
  // 1. create schema object from string
  JsonDocument schema_json;
  if (schema_json
          .Parse<rapidjson::kParseCommentsFlag>(schema.data(), schema.length())
          .HasParseError()) {
    FAIL() << "Parsing JSON schema failed at offset "
           << std::to_string(schema_json.GetErrorOffset()) << ": "
           << rapidjson::GetParseError_En(schema_json.GetParseError());
    return;
  }
  JsonSchemaDocument schema_doc(schema_json);

  // 2. create json document from string to verify
  JsonDocument verified_json_doc;
  if (verified_json_doc
          .Parse<rapidjson::kParseCommentsFlag>(json.data(), json.length())
          .HasParseError()) {
    FAIL() << "Parsing JSON failed at offset "
           << std::to_string(schema_json.GetErrorOffset()) << ": "
           << rapidjson::GetParseError_En(verified_json_doc.GetParseError());
    return;
  }

  // 3. validate JSON set in the metadata against the schema
  validate_json_against_schema(schema_doc, verified_json_doc);
}

void validate_config_stored_in_md(uint16_t http_port,
                                  const std::string &validation_schema) {
  const auto server_globals =
      MockServerRestClient(http_port).get_globals_as_json_string();
  std::string config_json =
      get_str_field_value(server_globals, "upd_attr_config_json");

  config_json.erase(std::remove(config_json.begin(), config_json.end(), '\\'),
                    config_json.end());

  validate_json_against_schema(config_json, validation_schema);
}

std::string get_config_defaults_and_update_schema(uint16_t http_port) {
  const auto server_globals =
      MockServerRestClient(http_port).get_globals_as_json_string();
  std::string schema = get_str_field_value(
      server_globals, "upd_attr_config_defaults_and_schema_json");

  std::string result = std::regex_replace(schema, std::regex("\\\\n"), "\n");
  result.erase(std::remove(result.begin(), result.end(), '\\'), result.end());

  return result;
}

namespace {
std::string get_config_field(const std::string &json_string,
                             const std::string &field_name) {
  // the expected structure is:
  // "Configuration": { "8.4.0": { "Defaults"

  rapidjson::Document json_doc;
  json_doc.Parse(json_string.c_str());

  if (!json_doc.HasMember("Configuration") ||
      !json_doc["Configuration"].IsObject()) {
    return "";
  }

  auto json_configuration = json_doc["Configuration"].GetObject();
  if (!json_configuration.HasMember(MYSQL_ROUTER_VERSION) ||
      !json_configuration[MYSQL_ROUTER_VERSION].IsObject()) {
    return "";
  }

  auto json_version = json_configuration[MYSQL_ROUTER_VERSION].GetObject();
  if (!json_version.HasMember(field_name.c_str()) ||
      !json_version[field_name.c_str()].IsObject()) {
    return "";
  }

  auto &json_field = json_version[field_name.c_str()];

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  json_field.Accept(writer);
  return buffer.GetString();
}
}  // namespace

std::string get_config_defaults_stored_in_md(uint16_t http_port) {
  const auto config_default_and_update_schema =
      get_config_defaults_and_update_schema(http_port);

  return get_config_field(config_default_and_update_schema, "Defaults");
}

std::string get_config_update_schema_stored_in_md(uint16_t http_port) {
  const auto config_default_and_update_schema =
      get_config_defaults_and_update_schema(http_port);

  return get_config_field(config_default_and_update_schema,
                          "ConfigurationChangesSchema");
}