/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_HOST_CACHE_H_
#define _ROUTER_COMPONENT_HOST_CACHE_H_

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/mysql_session.h"
#include "rest_api_testutils.h"
#include "rest_metadata_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

const std::string k_property_worklog{"Worklog"};
const std::string k_property_test_plan{"TestPlanID"};

class RestHostCacheApiTest : public RestApiComponentTest {
 public:
  std::vector<std::string> get_static_routing_config(
      std::vector<std::string> sections) {
    sections.push_back(get_static_routing_config(
        "undertest", router_port_, "127.0.0.1", mock_server_port_));

    return sections;
  }

  std::string get_static_routing_config(const std::string &name,
                                        uint16_t routing_port,
                                        const std::string &dest_hostname,
                                        uint16_t dest_port) {
    return mysql_harness::ConfigBuilder::build_section(
        "routing:" + name,
        {{"bind_address", "127.0.0.1"},
         {"bind_port", std::to_string(routing_port)},
         {"routing_strategy", "round-robin"},
         {"protocol", "classic"},
         {"client_ssl_mode", "DISABLED"},
         {"server_ssl_mode", "DISABLED"},
         {"destinations", dest_hostname + ":" + std::to_string(dest_port)}});
  }

  void SetUp() override {
    RestApiComponentTest::SetUp();
    mock_server_port_ = port_pool_.get_next_available();
    mock_server_http_port_ = port_pool_.get_next_available();
    router_port_ = port_pool_.get_next_available();
  }

  bool process_logfile_contains(ProcessWrapper &spawn,
                                const std::string &expected_string) {
    auto content = spawn.get_logfile_content();
    return std::string::npos != content.find(expected_string);
  }

  uint64_t process_logfile_count(ProcessWrapper &spawn,
                                 const std::string &expected_string) {
    uint64_t result{0};
    auto content = spawn.get_logfile_content();
    size_t pos = content.find(expected_string);
    while (pos != std::string::npos) {
      ++result;
      pos = content.find(expected_string, pos + expected_string.size());
    }

    return result;
  }

  void launch_server_impl(uint16_t cluster_port, const std::string &json_file,
                          uint16_t http_port) {
    mock_server_spawner().spawn(mock_server_cmdline(json_file)
                                    .port(cluster_port)
                                    .http_port(http_port)
                                    .args());

    std::vector<uint16_t> nodes_ports{cluster_port};

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  }

  auto &launch_router_failure_impl(const std::vector<std::string> &args) {
    return router_spawner()
        .wait_for_sync_point(Spawner::SyncPoint::NONE)
        .expected_exit_code(EXIT_FAILURE)
        .spawn(args);
  }

  void launch_router_impl(const std::vector<std::string> &args) {
    auto &router = launch_router(args);
    ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port_));
  }

  void setup_static_routing_with_rest(
      const std::vector<std::string> &custom_sections = {}) {
    SCOPED_TRACE("// setup");
    const std::string userfile = create_password_file();
    auto config_sections = get_static_routing_config(
        get_restapi_config("rest_host_cache", userfile, true));

    config_sections.insert(config_sections.end(), custom_sections.begin(),
                           custom_sections.end());

    const std::string conf_file{create_config_file(
        conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};

    SCOPED_TRACE("// start mock-server");
    ASSERT_NO_FATAL_FAILURE(launch_server_impl(mock_server_port_, "my_port.js",
                                               mock_server_http_port_));
    SCOPED_TRACE("// start router");
    ASSERT_NO_FATAL_FAILURE(launch_router_impl({"-c", conf_file}));
  }

  template <typename Allocator>
  std::string rapidjson_to_string(
      const rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> &v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
  }

  JsonDocument rest_api_request_json(const std::string &url) {
    IOContext io_ctx;
    RestClient rest_client(io_ctx, k_http_hostname, http_port_,
                           kRestApiUsername, kRestApiPassword);

    JsonDocument json_doc;
    request_json(rest_client, url, http::base::method::Get,
                 http::base::status_code::Ok, json_doc, "application/json");
    return json_doc;
  }

  uint16_t mock_server_port_;
  uint16_t mock_server_http_port_;
  uint16_t router_port_;
  const std::string k_http_hostname = "127.0.0.1";
  const std::string k_http_uri_host_cache_config =
      rest_api_basepath + "/host_cache/config";
  const std::string k_http_uri_host_cache_entries =
      rest_api_basepath + "/host_cache/entries";
  const std::string k_http_uri_host_cache_status =
      rest_api_basepath + "/host_cache/status";
};

#endif  // _ROUTER_COMPONENT_HOST_CACHE_H_
