/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

class InvalidGuidelinesTest : public RouterComponentTest {
 public:
  InvalidGuidelinesTest() {
    mock_server_spawner().spawn(
        mock_server_cmdline("metadata_dynamic_nodes_v2_gr.js")
            .port(server_port_)
            .http_port(http_port_)
            .args());

    set_mock_metadata(http_port_, "", classic_ports_to_gr_nodes({server_port_}),
                      0, {server_port_});
  }

 protected:
  auto &launch_router(const std::string &routing_section,
                      const std::string &metadata_cache_section) {
    auto default_section = get_DEFAULT_defaults();

    const auto state_file =
        create_state_file(get_test_temp_dir_name(),
                          create_state_file_content("", "", {server_port_}, 0));
    default_section["dynamic_state"] = state_file;

    init_keyring(default_section, get_test_temp_dir_name(),
                 {KeyringEntry{user_, "password", "mysql_test_password"}});

    const std::string conf_file = create_config_file(
        get_test_temp_dir_name(), metadata_cache_section + routing_section,
        &default_section);

    return ProcessManager::launch_router({"-c", conf_file});
  }

  std::string get_routing_section(const std::string &role = "PRIMARY",
                                  const std::string &protocol = "classic") {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(router_port_)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", protocol}};

    return mysql_harness::ConfigBuilder::build_section(
        "routing:test_guidelines", options);
  }

  std::string get_metadata_cache_section(
      mysqlrouter::ClusterType cluster_type = mysqlrouter::ClusterType::GR_V2) {
    const std::string cluster_type_str =
        (cluster_type == mysqlrouter::ClusterType::RS_V2) ? "rs" : "gr";

    std::map<std::string, std::string> options{
        {"cluster_type", cluster_type_str},
        {"router_id", "1"},
        {"user", user_},
        {"connect_timeout", "1"},
        {"metadata_cluster", "test"},
        {"ttl", "0.1"}};

    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:bootstrap", options);
  }

  struct TestGuidelinesBuilder {
    auto &add_destinations(
        const std::optional<std::string_view> &destinations_str) {
      if (destinations_str) destinations_str_ = destinations_str.value();
      custom_destinations_ = true;
      return *this;
    }

    auto &add_routes(const std::optional<std::string_view> &routes_str) {
      if (routes_str) routes_str_ = routes_str.value();
      custom_routes_ = true;
      return *this;
    }

    auto &add_version(const std::optional<std::string_view> &version) {
      if (version) version_str_ = version.value();
      custom_version_ = true;
      return *this;
    }

    auto &add_name(const std::optional<std::string_view> &name) {
      if (name) name_str_ = name.value();
      custom_name_ = true;
      return *this;
    }

    auto &add_raw(std::string str) {
      guidelines_ += std::move(str);
      return *this;
    }

    std::string get() {
      if (!custom_version_) version_str_ = default_version;
      guidelines_ += version_str_;

      if (!custom_name_) name_str_ = default_name;
      if (!guidelines_.empty() && !name_str_.empty()) guidelines_ += ",\n";
      guidelines_ += name_str_;

      if (!custom_destinations_) destinations_str_ = default_destinations;
      if (!guidelines_.empty() && !destinations_str_.empty())
        guidelines_ += ",\n";
      guidelines_ += destinations_str_;

      if (!custom_routes_) routes_str_ = default_routes;
      if (!guidelines_.empty() && !routes_str_.empty()) {
        guidelines_ += ",\n";
      }
      guidelines_ += routes_str_;

      return "{\n" + guidelines_ + "\n}";
    }

    std::string guidelines_;
    std::string version_str_;
    std::string name_str_;
    std::string destinations_str_;
    std::string routes_str_;
    bool custom_version_{false};
    bool custom_name_{false};
    bool custom_destinations_{false};
    bool custom_routes_{false};
    const std::string_view default_destinations{R"(
      "destinations": [
        {
          "name": "d1",
          "match": "TRUE"
        }
      ]
    )"};
    const std::string_view default_routes{R"(
      "routes": [
        {
          "name": "r1",
          "match": "TRUE",
          "destinations": [
            {
              "classes": ["d1"],
              "strategy": "round-robin",
              "priority": 0
            }
          ]
        }
      ]
    )"};
    const std::string_view default_version{R"(
      "version": "1.0")"};
    const std::string_view default_name{R"(
      "name": "test_guidelines")"};
  };

  void set_guidelines(std::string_view guidelines, const uint16_t http_port,
                      const uint16_t server_port) {
    auto globals = mock_GR_metadata_as_json(
        "", classic_ports_to_gr_nodes({server_port}), 0, {server_port});
    JsonAllocator allocator;

    globals.AddMember("transaction_count", 0, allocator);
    globals.AddMember(
        "routing_guidelines",
        JsonValue(guidelines.data(), guidelines.size(), allocator), allocator);

    auto globals_str = json_to_string(globals);
    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(globals_str));
    EXPECT_TRUE(wait_for_transaction_count_increase(http_port, 2));
  }

  TestGuidelinesBuilder gb_;
  const uint16_t router_port_{port_pool_.get_next_available()};
  const uint16_t server_port_{port_pool_.get_next_available()};
  const uint16_t http_port_{port_pool_.get_next_available()};
  const std::string user_{"mysql_test_user"};
};

TEST_F(InvalidGuidelinesTest, unknown_guidelines_field) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_raw("\"foobar\": \"baz\",").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      "foobar: Unexpected field, only 'version', 'name', 'destinations', and "
      "'routes' are allowed",
      5s));
}

TEST_F(InvalidGuidelinesTest, missing_version) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_version(std::nullopt).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router, escape_regexp(R"("missing":["version"])"), 5s));
}

TEST_F(InvalidGuidelinesTest, version_type_mismatch) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_version("\"version\": 11").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router, "'version' must be a string value", 5s));
}

TEST_F(InvalidGuidelinesTest, version_no_value) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_version("\"version\": ").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(router, "Invalid value", 5s));
}

TEST_F(InvalidGuidelinesTest, version_empty) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_version("\"version\": \"\"").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      "Invalid routing guidelines version format. Expected <major>.<minor>",
      5s));
}

TEST_F(InvalidGuidelinesTest, name_invalid_value) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_name("\"name\": 11").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(R"("type":{"expected":["string"],"actual":"integer")"),
      5s));
}

TEST_F(InvalidGuidelinesTest, empty_name) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_name("\"name\": \"\"").get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, R"(name: field is expected to be a non empty string)", 5s));
}

TEST_F(InvalidGuidelinesTest, no_destinations) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_destinations(std::nullopt).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "no destination classes defined by the document", 5s));
}

TEST_F(InvalidGuidelinesTest, empty_destinations) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "destinations: field is expected to be a non empty array", 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_type_mismatch) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": 11)";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "destinations: field is expected to be an array", 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_unexpected_value) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "foobar": "baz"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[0].foobar: unexpected field name"),
      5s));
}

TEST_F(InvalidGuidelinesTest, destinations_empty_name) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "",
      "match": "TRUE"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "name: field is expected to be a non empty string", 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_empty_match) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "group1",
      "match": ""
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "match: field is expected to be a non empty string", 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_no_match) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[0]: 'match' field not defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_no_name) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "match": "TRUE"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[0]: 'name' field not defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, destinations_name_type_mismatch) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": 11,
      "match": "TRUE"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("destinations[0].name: field is expected to be a string"),
      5s));
}

TEST_F(InvalidGuidelinesTest, multiple_destinations_name_collision) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1",
      "match": "TRUE"
    },
    {
      "name": "d1",
      "match": "1 > 2"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[1]: 'd1' class was already defined"),
      5s));
}

TEST_F(InvalidGuidelinesTest, destinations_match_type_mismatch) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1",
      "match": []
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("destinations[0].match: field is expected to be a string"),
      5s));
}

TEST_F(InvalidGuidelinesTest, destinations_match_invalid_value) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1",
      "match": "foo"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "destinations[0].match: match does not evaluate to boolean"),
      5s));
}

TEST_F(InvalidGuidelinesTest, destinations_match_unknown_variable) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1",
      "match": "$.foo <> 'bar'"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "destinations[0].match: undefined variable: foo in '$.foo'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, destinations_match_unknown_function) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto dest_str = R"("destinations": [
    {
      "name": "d1",
      "match": "FOO('bar', 'x') <> 11"
    }
  ])";
  const auto &guidelines = gb_.add_destinations(dest_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[0].match: syntax error"), 5s));
}

TEST_F(InvalidGuidelinesTest, no_routes) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto &guidelines = gb_.add_routes(std::nullopt).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router, "no routes defined by the document", 5s));
}

TEST_F(InvalidGuidelinesTest, empty_routes) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "routes: field is expected to be a non empty array", 5s));
}

TEST_F(InvalidGuidelinesTest, routes_empty_name) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "round-robin",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "name: field is expected to be a non empty string", 5s));
}

TEST_F(InvalidGuidelinesTest, routes_no_destinations) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE"
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0]: 'destinations' field not defined"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_destinations_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": "d1"
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].destinations: field is expected to be an array"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_empty_destinations) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": []
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "routes[0].destinations: field is expected to be a non empty array"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_empty_dest_class) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": [],
      "strategy": "round-robin",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "classes: field is expected to be a non empty array", 5s));
}

TEST_F(InvalidGuidelinesTest, routes_dest_class_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": "d1",
      "strategy": "round-robin",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router,
                        escape_regexp("routes[0].destinations[0].classes: "
                                      "field is expected to be an array"),
                        5s));
}

TEST_F(InvalidGuidelinesTest, routes_dest_class_unknown) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1", "__unknown_dest__"],
      "strategy": "round-robin",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      "undefined destination class '__unknown_dest__' found in route 'r1'",
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_dest_class_empty) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1", ""],
      "strategy": "round-robin",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("classes[1]: field is expected to be a non empty string"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_no_strategy) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].destinations[0]: 'strategy' field not defined"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_strategy_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": ["round-robin", "first-available"],
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router,
                        escape_regexp("routes[0].destinations[0].strategy: "
                                      "field is expected to be a string"),
                        5s));
}

TEST_F(InvalidGuidelinesTest, routes_strategy_unsupported) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "foo",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(router,
                                "strategy: unexpected value 'foo', supported "
                                "strategies: round-robin, first-available",
                                5s));
}

TEST_F(InvalidGuidelinesTest, routes_empty_strategy) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "strategy: field is expected to be a non empty string", 5s));
}

TEST_F(InvalidGuidelinesTest, routes_name_missing) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0]: 'name' field not defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, routes_name_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": {},
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0].name: field is expected to be a string"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_no_matching_criteria) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0]: 'match' field not defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, routes_matching_criteria_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": ["TRUE"],
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].match: field is expected to be a string"), 5s));
}

TEST_F(InvalidGuidelinesTest, routes_empty_matching_criteria) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "routes[0].match: field is expected to be a non empty string"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_matching_criteria_syntax_error) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": ">,.'';:[]",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router, R"(routes\[0\].match: syntax error)", 5s));
}

TEST_F(InvalidGuidelinesTest, routes_matching_criteria_unknown_var) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "$.x = ''",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0].match: undefined variable: x in '$.x'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_matching_criteria_unknown_func) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "UNKNOWN() > 3",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[0].match: syntax error"), 5s));
}

TEST_F(InvalidGuidelinesTest, routes_unsupported_field) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }],
    "foo": {}
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      "foo: unexpected field, only 'name', 'connectionSharingAllowed', "
      "'enabled', 'match' and 'destinations' are allowed",
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_enabled_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }],
    "enabled": {}
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].enabled: field is expected to be boolean"), 5s));
}

TEST_F(InvalidGuidelinesTest, routes_sharing_allowed_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }],
    "connectionSharingAllowed": "yes"
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(
      wait_log_contains(router,
                        escape_regexp("routes[0].connectionSharingAllowed: "
                                      "field is expected to be boolean"),
                        5s));
}

TEST_F(InvalidGuidelinesTest, routes_priority_invalid_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": "1"
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].destinations[0].priority: field is expected to "
                    "be a positive integer"),
      5s));
}

TEST_F(InvalidGuidelinesTest, routes_priority_negative_value) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": -8
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("routes[0].destinations[0].priority: field is expected to "
                    "be a positive integer"),
      5s));
}

TEST_F(InvalidGuidelinesTest, multiple_routes_name_collision) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  },
  {
    "name": "r1",
    "match": "$.session.targetIP = '127.0.0.1'",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "round-robin",
      "priority": 1
    }]
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[1]: 'r1' route was already defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, multiple_routes_one_invalid) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto routes_str = R"("routes" : [
  {
    "name": "r1",
    "match": "TRUE",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "first-available",
      "priority": 0
    }]
  },
  {
    "name": "r2",
    "match": "$.session.targetIP = '127.0.0.1'",
    "destinations": [{
      "classes": ["d1"],
      "strategy": "round-robin",
      "priority": 1
    }]
  },
  {
    "name": "empty"
  }
  ])";
  const auto &guidelines = gb_.add_routes(routes_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("routes[2]: 'match' field not defined"), 5s));
}

TEST_F(InvalidGuidelinesTest, multiple_destinations_one_invalid) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto destinations_str = R"("destinations" : [
  {
    "name": "dest0",
    "match": "TRUE"
  },
  {
    "name": "dest1",
    "match": "$.server.address = '127.0.0.1'"
  },
  {
    "name": "empty"
  }
  ])";
  const auto &guidelines = gb_.add_routes(destinations_str).get();
  set_guidelines(guidelines, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("destinations[2]: 'match' field not defined"), 5s));
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
