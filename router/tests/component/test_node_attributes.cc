/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_config.h"

#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_metadata.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_config.h"
#include "router_test_helpers.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MetadataSchemaVersion;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;
using namespace std::string_literals;

class NodeAttributesTest : public RouterComponentMetadataTest {
 protected:
  // MUST be 'localhost' to verify it works with hostnames and not just IP
  // addresses.
  static constexpr const char *const node_hostname{"localhost"};

  // first node is RW, all others (if any) RO
  void setup_cluster(const size_t nodes_count, const std::string &tracefile,
                     const std::vector<std::string> &nodes_attributes = {},
                     const bool no_primary = false) {
    assert(nodes_count > 0);

    const std::string json_metadata = get_data_dir().join(tracefile).str();

    for (size_t i = 0; i < nodes_count; ++i) {
      // if we are "relaunching" the cluster we want to use the same port as
      // before as router has them in the configuration
      if (node_ports.size() < nodes_count) {
        node_ports.push_back(port_pool_.get_next_available());
        node_http_ports.push_back(port_pool_.get_next_available());
      }

      cluster_nodes.push_back(
          &launch_mysql_server_mock(json_metadata, node_ports[i], EXIT_SUCCESS,
                                    false, node_http_ports[i]));
    }

    for (size_t i = 0; i < nodes_count; ++i) {
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(*cluster_nodes[i], node_ports[i]));
      ASSERT_TRUE(MockServerRestClient(node_http_ports[i])
                      .wait_for_rest_endpoint_ready());

      auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
      if (no_primary && cluster_nodes.size() > 0) {
        cluster_nodes[0].role = "SECONDARY";
      }
      for (auto [i, attr] : stdx::views::enumerate(nodes_attributes)) {
        if (i < cluster_nodes.size()) {
          cluster_nodes[i].attributes = attr;
        }
      }
      auto gr_nodes = classic_ports_to_gr_nodes(node_ports);
      if (no_primary && gr_nodes.size() > 0) {
        gr_nodes[0].member_role = "SECONDARY";
      }
      set_mock_metadata(node_http_ports[i], "uuid", gr_nodes, i, cluster_nodes,
                        0, false, node_hostname);
    }
  }

  void setup_router(ClusterType cluster_type, const std::string &ttl,
                    const bool read_only = false) {
    const std::string metadata_cache_section =
        get_metadata_cache_section(cluster_type, ttl);
    std::string routing_rw_section{""};
    if (!read_only) {
      routing_rw_section = get_metadata_cache_routing_section(
          router_rw_port, "PRIMARY", "first-available", "rw");
      routing_rw_section += get_metadata_cache_routing_section(
          router_rw_x_port, "PRIMARY", "first-available", "x_rw", "x");
    }
    std::string routing_ro_section = get_metadata_cache_routing_section(
        router_ro_port, "SECONDARY", "round-robin", "ro");
    routing_ro_section += get_metadata_cache_routing_section(
        router_ro_x_port, "SECONDARY", "round-robin", "x_ro", "x");

    router = &launch_router(metadata_cache_section,
                            routing_rw_section + routing_ro_section, node_ports,
                            EXIT_SUCCESS,
                            /*wait_for_notify_ready=*/30s);

    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*router, read_only ? router_ro_port : router_rw_port));

    EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  }

  void set_nodes_attributes(const std::vector<std::string> &nodes_attributes,
                            const bool no_primary = false) {
    auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
    if (no_primary && cluster_nodes.size() > 0) {
      cluster_nodes[0].role = "SECONDARY";
    }
    for (auto [i, attr] : stdx::views::enumerate(nodes_attributes)) {
      if (i < cluster_nodes.size()) {
        cluster_nodes[i].attributes = attr;
      }
    }

    auto gr_nodes = classic_ports_to_gr_nodes(node_ports);
    if (no_primary && gr_nodes.size() > 0) {
      gr_nodes[0].member_role = "SECONDARY";
    }
    ASSERT_NO_THROW({
      set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes,
                        0, false, node_hostname);
    });

    try {
      ASSERT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 3));
    } catch (const std::exception &e) {
      FAIL() << "failed waiting for trans' count increase: " << e.what();
    };
  }

  std::vector<uint16_t> node_ports, node_http_ports;
  std::vector<ProcessWrapper *> cluster_nodes;
  ProcessWrapper *router;

  const uint16_t router_rw_port{port_pool_.get_next_available()};
  const uint16_t router_ro_port{port_pool_.get_next_available()};
  const uint16_t router_rw_x_port{port_pool_.get_next_available()};
  const uint16_t router_ro_x_port{port_pool_.get_next_available()};

 private:
  TempDirectory temp_test_dir;
  TempDirectory conf_dir{"conf"};
};

struct NodeAttributesTestParam {
  // mock_server trace file
  std::string tracefile;
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  // the type of the cluster GR or AR
  ClusterType cluster_type;
  // ttl value we want to set (floating point decimal in seconds)
  std::string ttl;

  NodeAttributesTestParam(std::string tracefile_, std::string description_,
                          ClusterType cluster_type_, std::string ttl_ = "0.5")
      : tracefile(std::move(tracefile_)),
        description(std::move(description_)),
        cluster_type(cluster_type_),
        ttl(std::move(ttl_)) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<NodeAttributesTestParam> &info) {
  return info.param.description;
}

// define constexpr for sun-cc
constexpr const char *const NodeAttributesTest::node_hostname;

/**
 * @test Verifies that setting the _hidden tags in the metadata for the node is
 * handled as expected by the Router.
 *
 * WL#13787: TS_FR02_01, TS_FR02_02, TS_FR02_04
 * WL#13327: TS_R2_6
 */
class ClusterNodeAttributesTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

TEST_P(ClusterNodeAttributesTest, RWRONodeHidden) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  try {
    setup_cluster(3, GetParam().tracefile);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  try {
    setup_router(GetParam().cluster_type, GetParam().ttl);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// check if both RO and RW ports are used");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Make rw connection, should be ok");
  try {
    make_new_connection_ok(router_rw_port, node_ports[0]);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Configure first RO node to hidden=true");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({"", R"({"tags" : {"_hidden": true} })", ""}));

  SCOPED_TRACE("// RW and RO ports should be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Configure both RO node to hidden=true");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({"", R"({"tags" : {"_hidden": true} })",
                            R"({"tags" : {"_hidden": true} })"}));

  SCOPED_TRACE("// RO ports should not be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Unhide first RO node");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({"", R"({"tags" : {"_hidden": false} })", ""}));

  SCOPED_TRACE("// RO ports should be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Hide first RO node");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({"", R"({"tags" : {"_hidden": true} })",
                            R"({"tags" : {"_hidden": true} })"}));

  SCOPED_TRACE("// RO ports should not be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Unhide second RO node");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({"", R"({"tags" : {"_hidden": false} })",
                            R"({"tags" : {"_hidden": true} })"}));

  SCOPED_TRACE("// RO ports should be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Unhide first RO node");
  ASSERT_NO_FATAL_FAILURE({
    set_nodes_attributes({"", R"({"tags" : {"_hidden": false} })",
                          R"({"tags" : {"_hidden": false} })"});
  });

  SCOPED_TRACE("// RO ports should be used by the router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE(
      "// Configure RW node to hidden=true, "
      "disconnect_existing_sessions_when_hidden stays default which is "
      "'true'");
  ASSERT_NO_FATAL_FAILURE(set_nodes_attributes(
      {R"({"tags" : {"_hidden": true} })", R"({"tags" : {"_hidden": true} })",
       R"({"tags" : {"_hidden": true} })"}));

  SCOPED_TRACE("// RW port should be open");
  try {
    EXPECT_TRUE(wait_for_port_unused(router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Making new connection should not be possible");
  try {
    verify_new_connection_fails(router_rw_port);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Configure RW node back to hidden=false");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({R"({"tags" : {"_hidden": false} })", "", ""}));

  SCOPED_TRACE("// RW port should be again used by the Router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Making new connection should be possible again");
  try {
    make_new_connection_ok(router_rw_port, node_ports[0]);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Configure RW node again to hidden=true");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({R"({"tags" : {"_hidden": true} })", "", ""}));

  try {
    EXPECT_TRUE(wait_for_port_unused(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Making new connection should not be possible");
  try {
    verify_new_connection_fails(router_rw_port);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  SCOPED_TRACE("// Configure RW node back to hidden=false");
  ASSERT_NO_FATAL_FAILURE(
      set_nodes_attributes({R"({"tags" : {"_hidden": false} })", "", ""}));

  SCOPED_TRACE("// RW port should be again used by the Router");
  try {
    EXPECT_TRUE(wait_for_port_used(router_rw_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_port));
    EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(router_ro_x_port));
    SCOPED_TRACE("// Making new connection should be possible again");
    make_new_connection_ok(router_rw_port, node_ports[0]);
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }
}

TEST_P(ClusterNodeAttributesTest, RWNodeHidden) {
  SCOPED_TRACE("// launch cluster with only 1 RW node");
  setup_cluster(1, GetParam().tracefile);
  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);

  SCOPED_TRACE("// RW socket is listening");
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));
  EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));

  SCOPED_TRACE("// Hide RW node");
  set_nodes_attributes({R"({"tags" : {"_hidden": true} })"});
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));
  EXPECT_TRUE(wait_for_port_unused(router_rw_x_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));

  SCOPED_TRACE("// Unhide RW node");
  set_nodes_attributes({R"({"tags" : {"_hidden": false} })"});
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));
  EXPECT_TRUE(wait_for_port_used(router_rw_x_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_x_port));
}

INSTANTIATE_TEST_SUITE_P(
    ClusterNodeHidden, ClusterNodeAttributesTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "node_hidden_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "node_hidden_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

/**
 * @test Verifies that setting the _disconnect_existing_sessions_when_hidden
 *       tags back and forth in the metadata for the node is handled as expected
 *        by the Router.
 *
 *  TS_FR02_03, TS_FR04_01
 */
class RWNodeHiddenDontDisconnectToggleTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

TEST_P(RWNodeHiddenDontDisconnectToggleTest, RWNodeHiddenDontDisconnectToggle) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  setup_cluster(3, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0]));

  // test tags: {hidden, disconnect}
  {
    SCOPED_TRACE("// Make rw connection, should be ok");
    auto rw_con_1 = make_new_connection_ok(router_rw_port, node_ports[0]);

    SCOPED_TRACE(
        "// Configure the first RW node to hidden=true, "
        "set disconnect_existing_sessions_when_hidden stays default which is "
        "true");
    set_nodes_attributes({R"({"tags" : {"_hidden": true} })", "", ""});

    SCOPED_TRACE("// The connection should get dropped");
    verify_existing_connection_dropped(rw_con_1.get());
  }

  // reset test (clear hidden flag)
  {
    SCOPED_TRACE(
        "// Unhide the node, "
        "set disconnect_existing_sessions_when_hidden to false");
    set_nodes_attributes(
        {R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false} })",
         "", ""});
  }

  // test tags: {hidden}, then {hidden, disconnect}
  {
    // test tags: {hidden}

    SCOPED_TRACE("// Make rw connection, should be ok");
    auto rw_con_2 = make_new_connection_ok(router_rw_port, node_ports[0]);

    SCOPED_TRACE(
        "// Now configure the first RW node to hidden=true, "
        "disconnect_existing_sessions_when_hidden stays false");
    set_nodes_attributes(
        {R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
         "", ""});

    SCOPED_TRACE("// The existing connection should be ok");
    verify_existing_connection_ok(rw_con_2.get(), node_ports[0]);

    // reset test (clear hidden flag); connection should still be alive
    // therefore we can reuse it for the next test
    SCOPED_TRACE("// Set disconnect_existing_sessions_when_hidden=true");
    set_nodes_attributes(
        {R"({"tags" : {"_disconnect_existing_sessions_when_hidden": true} })",
         "", ""});

    // test tags: {hidden, disconnect}

    SCOPED_TRACE("// And also _hidden=true");
    set_nodes_attributes(
        {R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
         "", ""});

    SCOPED_TRACE("// The connection should get dropped");
    verify_existing_connection_dropped(rw_con_2.get());
  }

  // reset test (clear hidden flag)
  {
    SCOPED_TRACE(
        "// Unhide the node and et disconnect_existing_sessions_when_hidden to "
        "false");
    set_nodes_attributes(
        {R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false })",
         "", ""});
  }

  // test tags: {hidden}
  {
    SCOPED_TRACE("// Make rw connection, should be ok");
    auto rw_con_3 = make_new_connection_ok(router_rw_port, node_ports[0]);

    SCOPED_TRACE("// Hide the node again");
    set_nodes_attributes(
        {R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false })",
         "", ""});

    SCOPED_TRACE("// The existing connection should be ok");
    verify_existing_connection_ok(rw_con_3.get(), node_ports[0]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    RWNodeHiddenDontDisconnectToggle, RWNodeHiddenDontDisconnectToggleTest,
    ::testing::Values(
        NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                "rw_hidden_dont_disconnect_toggle_gr_v2",
                                ClusterType::GR_V2, "0.1"),
        NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                "rw_hidden_dont_disconnect_toggle_ar_v2",
                                ClusterType::RS_V2, "0.1")),
    get_test_description);

class RWNodeHideThenDisconnectTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

/**
 * @test Verify _disconnect_existing_sessions_when_hidden also works when
 * applied AFTER hiding
 *
 * TS_FR04_02
 * */
TEST_P(RWNodeHideThenDisconnectTest, RWNodeHideThenDisconnect) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  setup_cluster(3, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);

  SCOPED_TRACE("// Make rw connection, should be ok");
  auto rw_con_1 = make_new_connection_ok(router_rw_port, node_ports[0]);

  SCOPED_TRACE("// Set disconnect_existing_sessions_when_hidden=false");
  set_nodes_attributes(
      {R"({"tags" : {"_disconnect_existing_sessions_when_hidden": false} })",
       "", ""});
  SCOPED_TRACE("// Then also set hidden=true");
  set_nodes_attributes(
      {R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
       "", ""});

  SCOPED_TRACE("// The existing connection should stay ok");
  verify_existing_connection_ok(rw_con_1.get(), node_ports[0]);

  SCOPED_TRACE(
      "// Now disconnect_existing_sessions_when_hidden also gets set to true");
  set_nodes_attributes(
      {R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
       "", ""});

  SCOPED_TRACE("// The existing connection should be disconnected");
  verify_existing_connection_dropped(rw_con_1.get());
}

INSTANTIATE_TEST_SUITE_P(
    RWNodeHideThenDisconnect, RWNodeHideThenDisconnectTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "rw_hide_then_disconnect_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "rw_hide_then_disconnect_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

/**
 * @test Verify _hidden works well with round-robin
 *
 * TS_FR02_05
 */
class RORoundRobinNodeAttributesTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

TEST_P(RORoundRobinNodeAttributesTest, RORoundRobinNodeHidden) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  setup_cluster(3, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);

  SCOPED_TRACE(
      "// Make one rw connection to check it's not affected by the RO being "
      "hidden");
  auto rw_con_1 = make_new_connection_ok(router_rw_port, node_ports[0]);

  SCOPED_TRACE("// Make ro connection, should be ok and go to the first RO");
  auto ro_con_1 = make_new_connection_ok(router_ro_port, node_ports[1]);

  SCOPED_TRACE("// Configure first RO node to be hidden");
  set_nodes_attributes({"", R"({"tags" : {"_hidden": true} })", ""});

  SCOPED_TRACE("// The existing connection should get dropped");
  verify_existing_connection_dropped(ro_con_1.get());

  SCOPED_TRACE(
      "// Make 2 new connections, both should go to the second RO node");
  auto ro_con_2 = make_new_connection_ok(router_ro_port, node_ports[2]);
  auto ro_con_3 = make_new_connection_ok(router_ro_port, node_ports[2]);

  SCOPED_TRACE("// Now hide also the second RO node");
  set_nodes_attributes({"", R"({"tags" : {"_hidden": true} })",
                        R"({"tags" : {"_hidden": true} })"});
  SCOPED_TRACE("// Both connections to that node should get dropped");
  verify_existing_connection_dropped(ro_con_2.get());
  verify_existing_connection_dropped(ro_con_3.get());
  SCOPED_TRACE(
      "// Since both RO nodes are hidden no new connection to RO port should "
      "be possible");
  verify_new_connection_fails(router_ro_port);

  SCOPED_TRACE("// Unhide the first RO node now");
  set_nodes_attributes({"", "", R"({"tags" : {"_hidden": true} })"});

  SCOPED_TRACE(
      "// Make 2 new connections, both should go to the first RO node this "
      "time");
  /*auto ro_con_4 =*/make_new_connection_ok(router_ro_port, node_ports[1]);
  /*auto ro_con_5 =*/make_new_connection_ok(router_ro_port, node_ports[1]);

  SCOPED_TRACE("// Unhide also the second RO node now");
  set_nodes_attributes({"", "", ""});

  SCOPED_TRACE(
      "// Make more connections to the RO port, they should be assinged in a "
      "round robin fashion as no node is hidden");
  /*auto ro_con_6 =*/make_new_connection_ok(router_ro_port, node_ports[1]);
  /*auto ro_con_7 =*/make_new_connection_ok(router_ro_port, node_ports[2]);
  /*auto ro_con_8 =*/make_new_connection_ok(router_ro_port, node_ports[1]);

  SCOPED_TRACE(
      "// RW connection that we made at the beginning should survive all of "
      "that");
  verify_existing_connection_ok(rw_con_1.get(), node_ports[0]);
}

INSTANTIATE_TEST_SUITE_P(
    RORoundRobinNodeHidden, RORoundRobinNodeAttributesTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "ro_round_robin_hidden_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "ro_round_robin_hidden_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class NodesHiddenWithFallbackTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

TEST_P(NodesHiddenWithFallbackTest, PrimaryHidden) {
  using ::ClusterNode;

  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  setup_cluster(3, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type);
  std::string routing_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "round-robin", "rw");
  routing_section += get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");

  launch_router(metadata_cache_section, routing_section, node_ports,
                EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Configure primary node to be hidden");
  set_nodes_attributes({R"({"tags" : {"_hidden": true} })", "", ""});
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  {
    SCOPED_TRACE("// Remove secondary nodes, primary is hidden");
    const std::vector<GRNode> gr_nodes{
        {node_ports[0], "uuid-1", "ONLINE", "PRIMARY"}};
    const std::vector<ClusterNode> cluster_nodes{
        {node_ports[0], "uuid-1", 0, R"({"tags" : {"_hidden": true} })",
         "PRIMARY"}};
    set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes, 0,
                      false, node_hostname);
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  {
    SCOPED_TRACE("// Bring back second secondary node, primary is hidden");
    const std::vector<GRNode> gr_nodes{
        {node_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
        {node_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};
    const std::vector<ClusterNode> cluster_nodes{
        {node_ports[0], "uuid-1", 0, R"({"tags" : {"_hidden": true} })",
         "PRIMARY"},
        {node_ports[2], "uuid-3", 0, "", "SECONDARY"}};

    set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes, 0,
                      false, node_hostname);
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  {
    SCOPED_TRACE("// Unhide primary node");
    const std::vector<GRNode> gr_nodes{
        {node_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
        {node_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};
    const std::vector<ClusterNode> cluster_nodes{
        {node_ports[0], "uuid-1", 0, "", "PRIMARY"},
        {node_ports[2], "uuid-3", 0, "", "SECONDARY"}};

    set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes, 0,
                      false, node_hostname);
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));
}

TEST_P(NodesHiddenWithFallbackTest, SecondaryHidden) {
  using ::ClusterNode;

  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  setup_cluster(3, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type);
  std::string routing_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "round-robin", "rw");
  routing_section += get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");

  launch_router(metadata_cache_section, routing_section, node_ports,
                EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Configure second secondary node to be hidden");
  set_nodes_attributes({"", "", R"({"tags" : {"_hidden": true} })"});
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  {
    SCOPED_TRACE("// Bring down first secondary node");
    const std::vector<GRNode> gr_nodes{
        {node_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
        {node_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};
    const std::vector<ClusterNode> cluster_nodes{
        {node_ports[0], "uuid-1", 0, "", "PRIMARY"},
        {node_ports[2], "uuid-3", 0, R"({"tags" : {"_hidden": true} })",
         "SECONDARY"}};

    set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes, 0,
                      false, node_hostname);
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Unhide second secondary node");
  {
    const std::vector<GRNode> gr_nodes{
        {node_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
        {node_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};
    const std::vector<ClusterNode> cluster_nodes{
        {node_ports[0], "uuid-1", 0, "", "PRIMARY"},
        {node_ports[2], "uuid-3", 0, "", "SECONDARY"}};
    set_mock_metadata(node_http_ports[0], "uuid", gr_nodes, 0, cluster_nodes, 0,
                      false, node_hostname);
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));
}

INSTANTIATE_TEST_SUITE_P(
    NodesHiddenWithFallback, NodesHiddenWithFallbackTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "hidden_with_fallback_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "hidden_with_fallback_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class OneNodeClusterHiddenTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {
 protected:
  void kill_server(ProcessWrapper *server) { EXPECT_NO_THROW(server->kill()); }
};

/**
 * @test Verify _hidden works fine with one node cluster and after the node
 * resurrection
 *
 * WL#13787: TS_FR02_06, TS_FR02_07
 * WL#13327: TS_R2_3
 */
TEST_P(OneNodeClusterHiddenTest, OneRWNodeClusterHidden) {
  SCOPED_TRACE("// launch one node cluster (single RW node)");
  setup_cluster(1, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);

  SCOPED_TRACE("// RW port should be used, RO is unused");
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  SCOPED_TRACE("// Hide the single node that we have");
  set_nodes_attributes({R"({"tags" : {"_hidden": true} })"});

  SCOPED_TRACE("// RW and RO ports are open");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  verify_new_connection_fails(router_rw_port);

  SCOPED_TRACE(
      "// Check that hiding also works after node dissapearing and getting "
      "back");
  kill_server(cluster_nodes[0]);

  SCOPED_TRACE(
      "// Relaunch the node, set the node as hidden from the very start");
  setup_cluster(1, GetParam().tracefile, {R"({"tags" : {"_hidden": true} })"});

  SCOPED_TRACE("// RW and RO ports are open");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  SCOPED_TRACE("// We still should not be able to connect");
  verify_new_connection_fails(router_rw_port);

  SCOPED_TRACE("// Un-hide the node");
  set_nodes_attributes({R"({"tags" : {"_hidden": false} })"});

  SCOPED_TRACE("// RW port should be used, RO is unused");
  EXPECT_TRUE(wait_for_port_used(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  SCOPED_TRACE("// Now we should be able to connect");
  make_new_connection_ok(router_rw_port, node_ports[0]);
}

/**
 * @test Test hiding a node in a single SECONDARY node cluster.
 *
 * WL#13327: TS_R2_4
 */
TEST_P(OneNodeClusterHiddenTest, OneRONodeClusterHidden) {
  SCOPED_TRACE("// launch one node cluster (single RO) node)");
  setup_cluster(1, GetParam().tracefile, {}, /*no_primary*/ true);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl, true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Hide the single node that we have");
  set_nodes_attributes({R"({"tags" : {"_hidden": true} })"},
                       /*no_primary*/ true);

  SCOPED_TRACE("// RW and RO ports are open");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  verify_new_connection_fails(router_rw_port);

  SCOPED_TRACE(
      "// Check that hiding also works after node dissapearing and getting "
      "back");
  kill_server(cluster_nodes[0]);

  SCOPED_TRACE(
      "// Relaunch the node, set the node as hidden from the very start");
  setup_cluster(1, GetParam().tracefile, {R"({"tags" : {"_hidden": true} })"},
                /*no_primary*/ true);

  SCOPED_TRACE("// RW and RO ports are open");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  SCOPED_TRACE("// We still should not be able to connect");
  verify_new_connection_fails(router_rw_port);

  SCOPED_TRACE("// Un-hide the node");
  set_nodes_attributes({R"({"tags" : {"_hidden": false} })"},
                       /*no_primary*/ true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Now we should be able to connect");
  make_new_connection_ok(router_ro_port, node_ports[0]);
}

INSTANTIATE_TEST_SUITE_P(
    OneNodeClusterHidden, OneNodeClusterHiddenTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "one_node_cluster_hidden_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "one_node_cluster_hidden_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class TwoNodesClusterHidden
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {
 protected:
  void kill_server(ProcessWrapper *server) { EXPECT_NO_THROW(server->kill()); }
};

/**
 * @test Test hiding a node in a two SECONDARY nodes cluster.
 *
 * WL#13327: TS_R2_5
 */
TEST_P(TwoNodesClusterHidden, TwoRONodesClusterHidden) {
  SCOPED_TRACE("// launch two nodes cluster (both SECONDARY) nodes)");
  setup_cluster(2, GetParam().tracefile, {}, /*no_primary*/ true);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl, true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Hide one node");
  set_nodes_attributes({R"({"tags" : {"_hidden": true} })", ""},
                       /*no_primary*/ true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Hide the second node as well");
  set_nodes_attributes(
      {R"({"tags" : {"_hidden": true} })", R"({"tags" : {"_hidden": true} })"},
      /*no_primary*/ true);

  SCOPED_TRACE("// RO and RW ports are unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(router_ro_port));

  verify_new_connection_fails(router_rw_port);

  SCOPED_TRACE("// Un-hide one node");
  set_nodes_attributes({R"({"tags" : {"_hidden": false} })", ""},
                       /*no_primary*/ true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));

  SCOPED_TRACE("// Un-hide second node");
  set_nodes_attributes({"", ""}, /*no_primary*/ true);

  SCOPED_TRACE("// RO port should be used, RW is unused");
  EXPECT_TRUE(wait_for_port_unused(router_rw_port));
  EXPECT_TRUE(wait_for_port_used(router_ro_port));
}

INSTANTIATE_TEST_SUITE_P(
    TwoRONodesClusterHidden, TwoNodesClusterHidden,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "one_node_cluster_hidden_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "one_node_cluster_hidden_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class InvalidAttributesTagsTest
    : public NodeAttributesTest,
      public ::testing::WithParamInterface<NodeAttributesTestParam> {};

/**
 * @test Checks that the router logs a proper warning once when the attributes
 * for the node becomes invalid.
 *
 * The test covers the following scenarios from the test plan (plus add some
 * more cases):
 * TS_log_parse_error_01 TS_log_parse_error_02
 */
TEST_P(InvalidAttributesTagsTest, InvalidAttributesTags) {
  SCOPED_TRACE("// launch cluster with 1 RW node");
  setup_cluster(1, GetParam().tracefile);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  setup_router(GetParam().cluster_type, GetParam().ttl);

  SCOPED_TRACE("// Set the node's attributes to invalid JSON");
  set_nodes_attributes({"not a valid json for sure [] (}", ""});

  SCOPED_TRACE("// Check the expected warnings were logged once");
  check_log_contains(
      *router,
      "Error parsing _hidden from attributes JSON string: not a valid JSON "
      "object",
      1);
  check_log_contains(
      *router,
      "Error parsing _disconnect_existing_sessions_when_hidden from "
      "attributes "
      "JSON string: not a valid JSON object",
      1);

  SCOPED_TRACE("// Set the node's attributes.tags to invalid JSON");
  set_nodes_attributes({R"({"tags" : false})"});

  SCOPED_TRACE("// Check the expected warnings were logged once");
  check_log_contains(
      *router,
      "Error parsing _hidden from attributes JSON string: tags - not a valid "
      "JSON object",
      1);
  check_log_contains(
      *router,
      "Error parsing _disconnect_existing_sessions_when_hidden from "
      "attributes "
      "JSON string: tags - not a valid JSON object",
      1);

  SCOPED_TRACE("// Set the attributes.tags to be invalid types");
  set_nodes_attributes(
      {R"({"tags" : { "_hidden" : [], "_disconnect_existing_sessions_when_hidden": "True" }})"});

  SCOPED_TRACE("// Check the expected warnings were logged once");
  check_log_contains(
      *router,
      "Error parsing _hidden from attributes JSON string: tags._hidden not a "
      "boolean",
      1);
  check_log_contains(
      *router,
      "Error parsing _disconnect_existing_sessions_when_hidden from "
      "attributes "
      "JSON string: tags._disconnect_existing_sessions_when_hidden not a "
      "boolean",
      1);

  SCOPED_TRACE(
      "// Now fix both _hidden and _disconnect_existing_sessions_when_hidden "
      "in the metadata");
  set_nodes_attributes(
      {R"({"tags": { "_hidden" : false, "_disconnect_existing_sessions_when_hidden": false } })"});

  SCOPED_TRACE(
      "// Check the expected warnings about the attributes been valid were "
      "logged once");
  check_log_contains(
      *router, "Successfully parsed _hidden from attributes JSON string", 1);
  check_log_contains(
      *router,
      "Successfully parsed _disconnect_existing_sessions_when_hidden from "
      "attributes JSON string",
      1);

  SCOPED_TRACE("// Set the attributes.tags to be invalid types again");
  set_nodes_attributes(
      {R"({"tags" : { "_hidden" : [], "_disconnect_existing_sessions_when_hidden": "True" }})"});

  SCOPED_TRACE("// Check the expected warnings were logged twice");
  check_log_contains(
      *router,
      "Error parsing _hidden from attributes JSON string: tags._hidden not a "
      "boolean",
      2);
  check_log_contains(
      *router,
      "Error parsing _disconnect_existing_sessions_when_hidden from "
      "attributes "
      "JSON string: tags._disconnect_existing_sessions_when_hidden not a "
      "boolean",
      2);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidAttributesTags, InvalidAttributesTagsTest,
    ::testing::Values(NodeAttributesTestParam("metadata_dynamic_nodes_v2_gr.js",
                                              "invalid_attributes_tags_gr_v2",
                                              ClusterType::GR_V2, "0.1"),
                      NodeAttributesTestParam("metadata_dynamic_nodes_v2_ar.js",
                                              "invalid_attributes_tags_ar_v2",
                                              ClusterType::RS_V2, "0.1")),
    get_test_description);

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
