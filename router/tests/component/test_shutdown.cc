/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

/**
 * @file
 * @brief Component Tests to test Router shutdown
 */

#include <chrono>
#include <csignal>
#include <thread>

#include <gmock/gmock.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>

#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"

using ::testing::Eq;
using namespace std::chrono_literals;

class ShutdownTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();

    // Valgrind needs way more time
    if (getenv("WITH_VALGRIND")) {
      wait_for_cache_ready_timeout_ = 5000;
    }
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &other_sections) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir);

    // create tmp conf dir (note that it will be RAII-deleted before router
    // shuts down, but that's ok)
    const std::string conf_file =
        create_config_file(temp_test_dir, other_sections, &default_section);

    // launch the router
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, EXIT_SUCCESS, true, false, -1s);

    return router;
  }

  void delay_sending_handshake(
      const JsonValue &existing_globals,
      const std::vector<uint16_t> cluster_node_http_ports) {
    const std::string kRestGlobalsUri = "/api/v1/mock_server/globals/";
    const std::string kHostname = "127.0.0.1";
    const int kHandshakeSendDelayMs = 10000;

    JsonValue globals;
    JsonAllocator allocator;
    globals.CopyFrom(existing_globals, allocator);
    globals.AddMember("connect_exec_time", kHandshakeSendDelayMs, allocator);
    const auto json_str = json_to_string(globals);

    // tell all the server mocks to delay sending handshake by 10 seconds
    for (auto http_port : cluster_node_http_ports) {
      EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
    }
  }

  int get_delayed_handshakes_count(const uint16_t http_port) {
    const std::string kRestGlobalsUri = "/api/v1/mock_server/globals/";
    const std::string kHostname = "127.0.0.1";
    constexpr char kDelayedHandshakes[] = "delayed_handshakes";

    // GET request

    EXPECT_TRUE(wait_for_rest_endpoint_ready(kRestGlobalsUri, http_port))
        << "wait_for_rest_endpoint_ready() timed out";

    IOContext io_ctx;
    RestClient rest_client(io_ctx, kHostname, http_port);
    HttpRequest req =
        rest_client.request_sync(HttpMethod::Get, kRestGlobalsUri);

    EXPECT_TRUE(req) << "HTTP Request to " << kHostname << ":"
                     << std::to_string(http_port)
                     << " failed (early): " << req.error_msg() << std::endl;
    EXPECT_GT(req.get_response_code(), 0u)
        << "HTTP Request to " << kHostname << ":" << std::to_string(http_port)
        << " failed: " << req.error_msg() << std::endl;
    EXPECT_EQ(req.get_response_code(), 200u);

    auto resp_body = req.get_input_buffer();
    auto resp_body_content = resp_body.pop_front(resp_body.length());

    // parse JSON

    std::string json_payload(resp_body_content.begin(),
                             resp_body_content.end());

    using JsonDocument =
        rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

    JsonDocument json_doc;
    json_doc.Parse(json_payload.c_str());

    EXPECT_TRUE(!json_doc.HasParseError());

    const auto &v = json_doc.GetObject();
    if (v.HasMember(kDelayedHandshakes)) {
      if (!v[kDelayedHandshakes].IsInt())
        throw std::logic_error("field 'delayed_handshakes' is not an integer!");
      return v[kDelayedHandshakes].GetInt();
    } else {
      return 0;
    }
  }

  unsigned wait_for_cache_ready_timeout_ = 1000;
};

/** @test
 * Verify that Router shutdown is quick when connectivity to cluster is flaky
 *
 * NOTE: If one day Router shutdown is quicker than at the time of writing,
 *       kAcceptableShutdownWait could be reduced
 *
 * At the time of writing, the bottleneck is Metadata Cache's refresh thread.
 * After the shutdown signal has been received and shutdown flag raised, all
 * other threads quickly exit while Refresh thread lingers on
 * mysql_real_connect(), trying to connect to a metadata server. Unfortunately
 * there's no simple way to interrupt this call, so the best we can hope for is
 * that the Router shuts down immediately after that call times out. This is
 * the expectation of this test.
 *
 * To simulate flaky connection, we send a special HTTP request to server mocks
 * that will cause them to reply very slowly during MySQL handshake for all new
 * connections. This is enough to cause mysql_real_connect() to block, just
 * like it would on a flaky TCP connection.
 */
TEST_F(ShutdownTest, flaky_connection_to_cluster) {
  // MdC's refresh thread can block up to this many seconds on
  // mysql_real_connect(<metadata server>)
  constexpr std::chrono::milliseconds kConnectTimeout = 2000ms;

  // This is our expectation - the test will pass if Router shuts down within
  // these many seconds. The value should should allow for up do
  // kConnectTimeout to pass, plus maybe some additional time to account for
  // additional CPU cycles needed. But it should not be at 2 * kConnectTimeout
  // or higher, because we want to make sure no more than one metadata server
  // is blocking the shutdown.
  constexpr std::chrono::milliseconds kAcceptableShutdownWait =
      kConnectTimeout +
      kConnectTimeout / 2;  // should be between 1 and 2 * kConnectTimeout

  TempDirectory temp_test_dir;

  const std::vector<uint16_t> cluster_node_ports{
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
  };
  const std::vector<uint16_t> cluster_node_http_ports{
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
      port_pool_.get_next_available(),
  };
  const uint16_t router_port = port_pool_.get_next_available();

  const std::string json_primary_node =
      get_data_dir().join("test_shutdown.js").str();

  // launch cluster
  // NOTE: We reuse the primary's JSON file for all the secondaries just for
  //       convenience. Only the primary is expected to receive queries,
  //       therefore any arbitrary JSON will do for the secondaries.
  std::vector<ProcessWrapper *> cluster_nodes;
  for (size_t i = 0; i < cluster_node_ports.size(); i++) {
    const auto http_port = cluster_node_http_ports[i];
    auto &node =
        launch_mysql_server_mock(json_primary_node, cluster_node_ports[i],
                                 EXIT_SUCCESS, false /*debug_mode*/, http_port);
    cluster_nodes.emplace_back(&node);
    set_mock_metadata(http_port, "gr-id", cluster_node_ports);
  }

  // write Router config
  std::string servers;
  for (unsigned port : cluster_node_ports)
    servers += "mysql://127.0.0.1:" + std::to_string(port) + ",";
  servers.resize(servers.size() - 1);  // trim last ","
  const std::string config =
      /*[DEFAULT]*/
      "connect_timeout = " + std::to_string(kConnectTimeout.count() / 1000) +
      "\n"
      "\n"
      "[metadata_cache:test]\n"
      "router_id=1\n"
      "bootstrap_server_addresses=" +
      servers +
      "\n"
      "user=mysql_router1_user\n"
      "metadata_cluster=test\n"
      "ttl=0.1\n"
      "\n"
      "[routing:test_default]\n"
      "bind_port=" +
      std::to_string(router_port) +
      "\n"
      "destinations=metadata-cache://test/default?role=PRIMARY\n"
      "protocol=classic\n"
      "routing_strategy=round-robin\n"
      "\n";

  // launch the Router
  auto &router = launch_router(temp_test_dir.name(), config);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  // give the Router a chance to initialise metadata-cache module
  // there is currently no easy way to check that
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout_));

  // now let's tell server nodes to delay sending MySQL Protocol handshake on
  // new connections (to simulate them being unreachable)
  auto current_globals = mock_GR_metadata_as_json("gr-id", cluster_node_ports);
  ASSERT_NO_FATAL_FAILURE(
      { delay_sending_handshake(current_globals, cluster_node_http_ports); });

  // wait for a new (slow) Refresh cycle to commence
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline &&
         !get_delayed_handshakes_count(cluster_node_http_ports.front())) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // and tell Router to shutdown and expect it to finish it within
  // kAcceptableShutdownWait seconds
  EXPECT_FALSE(router.send_clean_shutdown_event());
  check_exit_code(router, EXIT_SUCCESS, kAcceptableShutdownWait);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
