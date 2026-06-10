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
#include "routing_guidelines_builder.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

class InvalidGuidelinesTest : public RouterComponentTest {
 public:
  InvalidGuidelinesTest() {
    const std::string runtime_json_stmts =
        get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

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
    init_keyring(default_section, get_test_temp_dir_name(),
                 {KeyringEntry{user_, "password", "mysql_test_password"}});

    const auto state_file =
        create_state_file(get_test_temp_dir_name(),
                          create_state_file_content("", "", {server_port_}, 0));
    default_section["dynamic_state"] = state_file;

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

  std::string create_guidelines(const std::string &dest_match) {
    return guidelines_builder::create(
        {{"d1", dest_match}}, {{"r", "TRUE", {{"round-robin", {"d1"}}}}});
  }

  const uint16_t router_port_{port_pool_.get_next_available()};
  const uint16_t server_port_{port_pool_.get_next_available()};
  const uint16_t http_port_{port_pool_.get_next_available()};
  const std::string user_{"mysql_test_user"};
};

TEST_F(InvalidGuidelinesTest, sqrt_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("SQRT('1') = 1"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "SQRT function, expected NUMBER but got STRING in 'SQRT('1')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, number_invalid_param_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NUMBER(3) = 3"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "NUMBER function, expected STRING but got NUMBER in 'NUMBER(3)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, number_invalid_param_count) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NUMBER('3', '2') = 32"), http_port_,
                 server_port_);
  EXPECT_TRUE(
      wait_log_contains(router,
                        escape_regexp("function NUMBER expected 1 argument but "
                                      "got 2 in 'NUMBER('3', '2')'"),
                        5s));
}

TEST_F(InvalidGuidelinesTest, network_invalid_net_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NETWORK(1, 24) = '1.0.0.0'"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("NETWORK function, 1st argument, expected STRING but got "
                    "NUMBER in 'NETWORK(1, 24)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, network_invalid_netmask_type) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NETWORK('1.0.0.1', '16') = '1.0.0.0'"),
                 http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("NETWORK function, 2nd argument, expected NUMBER but got "
                    "STRING in 'NETWORK('1.0.0.1', '16')'"),
      5s));
}

class NetworkInvalidNetmask
    : public InvalidGuidelinesTest,
      public ::testing::WithParamInterface<std::int16_t> {};

TEST_P(NetworkInvalidNetmask, out_of_range) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const auto netmask_str = std::to_string(GetParam());
  set_guidelines(
      create_guidelines("NETWORK('1.0.0.1', " + netmask_str + ") = '0.0.0.0'"),
      http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "NETWORK function invalid netmask value: " + netmask_str, 5s));
}

INSTANTIATE_TEST_SUITE_P(out_of_range, NetworkInvalidNetmask,
                         ::testing::Values(0, -1, 33));

TEST_F(InvalidGuidelinesTest, is_ipv4_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("IS_IPV4(0)"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "IS_IPV4 function, expected STRING but got NUMBER in 'IS_IPV4(0)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, is_ipv6_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("IS_IPV6(0)"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "IS_IPV6 function, expected STRING but got NUMBER in 'IS_IPV6(0)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, substring_index_invalid_first_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("SUBSTRING_INDEX(0, 0, '1')"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("SUBSTRING_INDEX function, 1st argument, expected STRING "
                    "but got NUMBER in 'SUBSTRING_INDEX(0, 0, '1')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, substring_index_invalid_second_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("SUBSTRING_INDEX('aaa', 0, '1')"),
                 http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("SUBSTRING_INDEX function, 2nd argument, expected STRING "
                    "but got NUMBER in 'SUBSTRING_INDEX('aaa', 0, '1')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, substring_index_invalid_third_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("SUBSTRING_INDEX('aaa', 'a', '1')"),
                 http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("SUBSTRING_INDEX function, 3rd argument, expected NUMBER "
                    "but got STRING in 'SUBSTRING_INDEX('aaa', 'a', '1')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, startswith_invalid_first_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("STARTSWITH(TRUE, 'a')"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("STARTSWITH function, 1st argument, expected STRING but "
                    "got BOOLEAN in 'STARTSWITH(TRUE, 'a')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, startswith_invalid_second_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("STARTSWITH('foo', 1)"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("STARTSWITH function, 2nd argument, expected STRING but "
                    "got NUMBER in 'STARTSWITH('foo', 1)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, endswith_invalid_first_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("ENDSWITH(TRUE, 'a')"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("ENDSWITH function, 1st argument, expected STRING but "
                    "got BOOLEAN in 'ENDSWITH(TRUE, 'a')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, endswith_invalid_second_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("ENDSWITH('foo', 1)"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("ENDSWITH function, 2nd argument, expected STRING but "
                    "got NUMBER in 'ENDSWITH('foo', 1)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, contains_invalid_first_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("CONTAINS(TRUE, 'a')"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("CONTAINS function, 1st argument, expected STRING but "
                    "got BOOLEAN in 'CONTAINS(TRUE, 'a')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, contains_invalid_second_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("CONTAINS('123', 12)"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("CONTAINS function, 2nd argument, expected STRING but "
                    "got NUMBER in 'CONTAINS('123', 12)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, resolve_v4_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("RESOLVE_V4(0)"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("RESOLVE_V4 function, expected STRING but "
                    "got NUMBER in 'RESOLVE_V4(0)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, resolve_v6_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("RESOLVE_V6(0)"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("RESOLVE_V6 function, expected STRING but "
                    "got NUMBER in 'RESOLVE_V6(0)'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, regexp_like_invalid_first_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("REGEXP_LIKE(TRUE, '.*')"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("REGEXP_LIKE function, 1st argument, expected STRING but "
                    "got BOOLEAN in 'REGEXP_LIKE(TRUE, '.*')'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, regexp_like_invalid_second_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("REGEXP_LIKE('bar', 1)"), http_port_,
                 server_port_);
  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp("REGEXP_LIKE function, 2nd argument, expected STRING but "
                    "got NUMBER in 'REGEXP_LIKE('bar', 1)'"),
      5s));
}

struct InvalidReturnParam {
  std::string test_name;
  std::string expected_type;
  std::string provided_type;
  std::string match;
};

class InvalidFunctionReturnType
    : public InvalidGuidelinesTest,
      public ::testing::WithParamInterface<InvalidReturnParam> {};

TEST_P(InvalidFunctionReturnType, InvalidFunctionReturnTypeTest) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines(GetParam().match), http_port_, server_port_);

  std::string log_msg = "expected " + GetParam().expected_type + " but got " +
                        GetParam().provided_type + " in '" +
                        escape_regexp(GetParam().match) + "'";
  EXPECT_TRUE(wait_log_contains(router, log_msg, 5s));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFunctionReturnTypeTest, InvalidFunctionReturnType,
    ::testing::Values(
        InvalidReturnParam{"concat", /*expected*/ "STRING",
                           /*got*/ "NUMBER", "CONCAT('1', '2') = 12"},
        InvalidReturnParam{"sqrt", /*expected*/ "NUMBER",
                           /*got*/ "STRING", "SQRT(9) = '3'"},
        InvalidReturnParam{"number", /*expected*/ "NUMBER",
                           /*got*/ "STRING", "NUMBER('3') = '3'"},
        InvalidReturnParam{"network", /*expected*/ "STRING",
                           /*got*/ "NUMBER", "NETWORK('1.0.0.1', 8) = 1"},
        InvalidReturnParam{"is_ipv6", /*expected*/ "BOOLEAN",
                           /*got*/ "STRING", "IS_IPV6('::1') = '?'"},
        InvalidReturnParam{"substring_index", /*expected*/ "STRING",
                           /*got*/ "BOOLEAN",
                           "SUBSTRING_INDEX('aaab', 'b', 1) = TRUE"},
        InvalidReturnParam{"startswith", /*expected*/ "BOOLEAN",
                           /*got*/ "STRING",
                           "STARTSWITH('abc', 'z') = 'FALSE'"},
        InvalidReturnParam{"endswith", /*expected*/ "BOOLEAN",
                           /*got*/ "STRING", "ENDSWITH('abc', 'c') = 'TRUE'"},
        InvalidReturnParam{"contains", /*expected*/ "BOOLEAN",
                           /*got*/ "STRING", "CONTAINS('abc', 'c') = 'TRUE'"},
        InvalidReturnParam{"resolve_v4", /*expected*/ "STRING",
                           /*got*/ "NUMBER",
                           "RESOLVE_V4('www.oracle.com') = 0"},
        InvalidReturnParam{"resolve_v6", /*expected*/ "STRING",
                           /*got*/ "NUMBER",
                           "RESOLVE_V6('www.oracle.com') = 0"},
        InvalidReturnParam{
            "regexp_like",
            /*expected*/ "BOOLEAN",
            /*got*/ "STRING",
            "REGEXP_LIKE('www.oracle.com', 'www.*com') = 'TRUE'"},
        InvalidReturnParam{"addition", /*expected*/ "NUMBER",
                           /*got*/ "STRING", "1 + 23 = '123'"},
        InvalidReturnParam{"subtraction", /*expected*/ "NUMBER",
                           /*got*/ "STRING", "11 - 1 = '10'"},
        InvalidReturnParam{"multiplication", /*expected*/ "NUMBER",
                           /*got*/ "STRING", "2 * 2 = '?'"},
        InvalidReturnParam{"division", /*expected*/ "NUMBER",
                           /*got*/ "BOOLEAN", "2 / 2 = TRUE"}),
    [](const auto &info) { return info.param.test_name; });

struct InvalidParamCountStruct {
  std::string test_name;
  std::uint8_t expected_param_cnt;
  std::uint8_t provided_param_cnt;
  std::string match;
};

class InvalidParamCount
    : public InvalidGuidelinesTest,
      public ::testing::WithParamInterface<InvalidParamCountStruct> {};

TEST_P(InvalidParamCount, InvalidParamCountTest) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines(GetParam().match), http_port_, server_port_);

  std::string arg_str =
      (GetParam().expected_param_cnt == 1) ? "argument" : "arguments";
  std::string got_str = GetParam().provided_param_cnt == 0
                            ? "none"
                            : std::to_string(GetParam().provided_param_cnt);
  std::string log_msg = "expected " +
                        std::to_string(GetParam().expected_param_cnt) + " " +
                        arg_str + " but got " + got_str + " in '" +
                        escape_regexp(GetParam().match) + "'";
  EXPECT_TRUE(wait_log_contains(router, log_msg, 5s));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidParamCountTest, InvalidParamCount,
    ::testing::Values(
        InvalidParamCountStruct{"sqrt_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2, "SQRT(1, 2)"},
        InvalidParamCountStruct{"sqrt_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "SQRT()"},

        InvalidParamCountStruct{"number_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "NUMBER()"},
        InvalidParamCountStruct{"number_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2, "NUMBER('1','2')"},

        InvalidParamCountStruct{"network_0_param", /*expected_param_cnt*/ 2,
                                /*got*/ 0, "NETWORK()"},
        InvalidParamCountStruct{"network_1_param", /*expected_param_cnt*/ 2,
                                /*got*/ 1, "NETWORK('127.0.0.1')"},
        InvalidParamCountStruct{"network_3_param", /*expected_param_cnt*/ 2,
                                /*got*/ 3, "NETWORK('1.1.1.1', 16, TRUE)"},

        InvalidParamCountStruct{"is_ipv4_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "IS_IPV4()"},
        InvalidParamCountStruct{"is_ipv4_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2, "IS_IPV4('192.168.1.1', 16)"},

        InvalidParamCountStruct{"is_ipv6_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "IS_IPV6()"},
        InvalidParamCountStruct{"is_ipv6_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2, "IS_IPV6('f::1', 64)"},

        InvalidParamCountStruct{"substring_index_0_param",
                                /*expected_param_cnt*/ 3,
                                /*got*/ 0, "SUBSTRING_INDEX()"},
        InvalidParamCountStruct{"substring_index_1_param",
                                /*expected_param_cnt*/ 3,
                                /*got*/ 1, "SUBSTRING_INDEX('abc')"},
        InvalidParamCountStruct{"substring_index_2_param",
                                /*expected_param_cnt*/ 3,
                                /*got*/ 2, "SUBSTRING_INDEX('abc', 'b')"},
        InvalidParamCountStruct{"substring_index_4_param",
                                /*expected_param_cnt*/ 3,
                                /*got*/ 4,
                                "SUBSTRING_INDEX('aaaa', 'b', 1, 'a')"},

        InvalidParamCountStruct{"startswith_0_param", /*expected_param_cnt*/ 2,
                                /*got*/ 0, "STARTSWITH()"},
        InvalidParamCountStruct{"startswith_1_param", /*expected_param_cnt*/ 2,
                                /*got*/ 1, "STARTSWITH('foobar')"},
        InvalidParamCountStruct{"startswith_3_param", /*expected_param_cnt*/ 2,
                                /*got*/ 3,
                                "STARTSWITH('foobar', 'foo', 'bar')"},

        InvalidParamCountStruct{"endswith_0_param", /*expected_param_cnt*/ 2,
                                /*got*/ 0, "ENDSWITH()"},
        InvalidParamCountStruct{"endswith_1_param", /*expected_param_cnt*/ 2,
                                /*got*/ 1, "ENDSWITH('xyz')"},
        InvalidParamCountStruct{"endswith_3_param", /*expected_param_cnt*/ 2,
                                /*got*/ 3, "ENDSWITH('xyz', 'z', 1)"},

        InvalidParamCountStruct{"contains_0_param",
                                /*expected_param_cnt*/ 2, /*got*/ 0,
                                "CONTAINS()"},
        InvalidParamCountStruct{"contains_1_param",
                                /*expected_param_cnt*/ 2, /*got*/ 1,
                                "CONTAINS('foo')"},
        InvalidParamCountStruct{"contains_3_param",
                                /*expected_param_cnt*/ 2, /*got*/ 3,
                                "CONTAINS('foobar', 'bar', TRUE)"},

        InvalidParamCountStruct{"resolve_v4_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "RESOLVE_V4()"},
        InvalidParamCountStruct{"resolve_v4_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2,
                                "RESOLVE_V4('www.oracle.com', 'http')"},

        InvalidParamCountStruct{"resolve_v6_0_param", /*expected_param_cnt*/ 1,
                                /*got*/ 0, "RESOLVE_V6()"},
        InvalidParamCountStruct{"resolve_v6_2_param", /*expected_param_cnt*/ 1,
                                /*got*/ 2,
                                "RESOLVE_V6('www.oracle.com', TRUE)"},

        InvalidParamCountStruct{"regexp_like_0_param",
                                /*expected_param_cnt*/ 2, /*got*/ 0,
                                "REGEXP_LIKE()"},
        InvalidParamCountStruct{"regexp_like_1_param",
                                /*expected_param_cnt*/ 2, /*got*/ 1,
                                "REGEXP_LIKE('abc')"},
        InvalidParamCountStruct{"regexp_like_3_param",
                                /*expected_param_cnt*/ 2, /*got*/ 3,
                                "REGEXP_LIKE('abc', '.*', TRUE)"}),

    [](const auto &info) { return info.param.test_name; });

TEST_F(InvalidGuidelinesTest, not_operator_no_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NOT"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("unexpected end of expression in 'NOT'"), 5s));
}

TEST_F(InvalidGuidelinesTest, not_operator_invalid_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("NOT ()"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(router, escape_regexp("syntax error"), 5s));
}

TEST_F(InvalidGuidelinesTest, or_operator_missing_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("TRUE OR"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("unexpected end of expression in 'OR'"), 5s));
}

TEST_F(InvalidGuidelinesTest, and_operator_missing_param) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines(" AND FALSE"), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, escape_regexp("expecting end of expression or error in 'AND'"),
      5s));
}

TEST_F(InvalidGuidelinesTest, mismatched_parenthesis) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines("(() "), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(router, escape_regexp("syntax error"), 5s));
}

class OperatorTypeError : public InvalidGuidelinesTest,
                          public ::testing::WithParamInterface<std::string> {};

TEST_P(OperatorTypeError, OperatorTypeErrorTest) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  set_guidelines(create_guidelines(GetParam()), http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(router, escape_regexp("type error"), 5s));
}

INSTANTIATE_TEST_SUITE_P(OperatorTypeErrorTest, OperatorTypeError,
                         ::testing::Values("1 + TRUE",       //
                                           "'foo' + 'bar'",  //
                                           "'foo' + 1",      //
                                           "FALSE - 0",      //
                                           "'bar' - 'foo'",  //
                                           "'111' - 1",      //
                                           "TRUE * NULL",    //
                                           "0 * 'foo'",      //
                                           "'x' * 'y'",      //
                                           "12 / 'foo'",     //
                                           "'x' / 'y'",      //
                                           "TRUE / 'foo'",   //
                                           "1 > TRUE",       //
                                           "1 > 'foo'",      //
                                           "'x' > 1",        //
                                           "1 >= TRUE",      //
                                           "1 >= 'foo'",     //
                                           "'x' >= 1",       //
                                           "1 < TRUE",       //
                                           "1 < 'foo'",      //
                                           "'x' < 1",        //
                                           "1 <= TRUE",      //
                                           "1 <= 'foo'",     //
                                           "'x' <= 1",       //
                                           "1 = TRUE",       //
                                           "1 = 'foo'",      //
                                           "'x' = 1",        //
                                           "1 <> TRUE",      //
                                           "1 <> 'foo'",     //
                                           "'x' <> 1"));

class SessionVarInInvalidScope
    : public InvalidGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(SessionVarInInvalidScope, session_var_in_destinations_context) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const std::string guideline = guidelines_builder::create(
      {{"d1", GetParam() + "=" + GetParam()}},
      {{"r1", "TRUE", {{"first-available", {"d1"}}}}});
  set_guidelines(guideline, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "session..* may not be used in 'destinations' context", 5s));
}

INSTANTIATE_TEST_SUITE_P(
    SessionVarInInvalidScopeTest, SessionVarInInvalidScope,
    ::testing::Values("$.session.targetIP", "$.session.targetPort",
                      "$.session.sourceIP", "$.session.randomValue",
                      "$.session.user", "$.session.schema"));

class ServerVarInInvalidScope
    : public InvalidGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(ServerVarInInvalidScope, server_var_in_routes_context) {
  auto &router =
      launch_router(get_routing_section(), get_metadata_cache_section());

  const std::string guideline = guidelines_builder::create(
      {{"d1", "TRUE"}},
      {{"r1", GetParam() + "=" + GetParam(), {{"first-available", {"d1"}}}}});
  set_guidelines(guideline, http_port_, server_port_);
  EXPECT_TRUE(wait_log_contains(
      router, "server..* may not be used in 'routes' context", 5s));
}

INSTANTIATE_TEST_SUITE_P(
    ServerVarInInvalidScopeTest, ServerVarInInvalidScope,
    ::testing::Values("$.server.label", "$.server.address", "$.server.port",
                      "$.server.uuid", "$.server.version",
                      "$.server.clusterName", "$.server.clusterSetName",
                      "$.server.isClusterInvalidated", "$.server.memberRole",
                      "$.server.clusterRole"));

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
