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

#include <chrono>
#include <climits>
#include <initializer_list>
#include <system_error>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>  // chmod
#include <unistd.h>    // symlink
#endif

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-typed-test.h>

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_testutils.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "process_manager.h"
#include "random_generator.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "stdx_expected_no_error.h"

using mysql_harness::ConfigBuilder;
using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;

Path g_origin_path;

template <class Iterable>
static std::string make_error_message_regex(
    const Iterable &expected_error_codes) {
  std::string expected_error_regex("(");

  for (auto const &ec : expected_error_codes) {
    if (expected_error_codes.size() > 1) {
      expected_error_regex += "|";
    }
    expected_error_regex += ec.message();
  }

  expected_error_regex += ")";

  return expected_error_regex;
}

class NotifyTest : public RestApiComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
  }

  bool wait_signal_handler_ready(ProcessWrapper &router) {
#ifdef _WIN32
    UNREFERENCED_PARAMETER(router);
    return true;
#else
    // log_reopen service reports readiness after the signal handler is
    // initialized in the current implementation
    return wait_log_contains(router, "ready 'log_reopen'", 5s);
#endif
  }

  std::string create_config_file(
      const std::vector<std::string> &config_file_sections,
      const std::optional<std::string> &state_file = std::nullopt) {
    auto default_section = prepare_config_defaults();
    if (state_file) {
      default_section["dynamic_state"] = *state_file;
    }

    const std::string config_file_content =
        mysql_harness::join(config_file_sections, "");

    return ProcessManager::create_config_file(
        get_test_temp_dir_name(), config_file_content, &default_section);
  }

  auto &launch_router(const std::string &conf_file,
                      bool wait_for_ready_expected_result = true,
                      std::chrono::milliseconds wait_for_ready_timeout = 5s,
                      const std::string &notification_socket_node = "default",
                      bool do_create_notify_socket = true,
                      int expected_exit_code = EXIT_SUCCESS,
                      bool wait_on_notify_socket = true) {
    std::vector<std::pair<std::string, std::string>> env_vars;

    std::string socket_node;
    if (notification_socket_node == "default") {
      socket_node = generate_notify_socket_path(get_test_temp_dir_name());
    } else {
      socket_node = notification_socket_node;
    }

    net::io_context io_ctx;
    wait_socket_t notify_socket{io_ctx};

    if (do_create_notify_socket) {
      EXPECT_NO_ERROR(notify_socket.open());
      EXPECT_NO_ERROR(notify_socket.bind({socket_node})) << socket_node;
    }
    env_vars.emplace_back("NOTIFY_SOCKET", socket_node);

    auto &router =
        launch_router({"-c", conf_file}, env_vars, expected_exit_code);

    if (wait_on_notify_socket) {
      const auto wait_for_ready_result =
          wait_for_notified_ready(notify_socket, wait_for_ready_timeout);

      if (wait_for_ready_expected_result) {
        EXPECT_NO_ERROR(wait_for_ready_result);
      } else {
        EXPECT_ERROR(wait_for_ready_result);
      }
    }

    return router;
  }

 protected:
  std::map<std::string, std::string> prepare_config_defaults() {
    auto default_section = get_DEFAULT_defaults();
    const std::string masterkey_file =
        Path(get_test_temp_dir_name()).join("master.key").str();
    const std::string keyring_file =
        Path(get_test_temp_dir_name()).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    return default_section;
  }

  ProcessWrapper &launch_router(
      const std::vector<std::string> &params,
      const std::vector<std::pair<std::string, std::string>> &env_vars,
      int expected_exit_code,
      ProcessWrapper::OutputResponder output_responder =
          RouterComponentBootstrapTest::kBootstrapOutputResponder) {
    // wait_for_notify_ready is false as we do it manually in those tests
    auto &router =
        launch_command(get_mysqlrouter_exec().str(), params, expected_exit_code,
                       /*catch_stderr*/ true, env_vars, output_responder);
    router.set_logging_path(get_logging_dir().str(), "mysqlrouter.log");

    return router;
  }

  std::string generate_notify_socket_path(const std::string &tmp_dir,
                                          unsigned length = 12) {
    const std::string unique_id =
        mysql_harness::RandomGenerator().generate_identifier(
            length, mysql_harness::RandomGenerator::AlphabetLowercase);

#ifdef _WIN32
    UNREFERENCED_PARAMETER(tmp_dir);
    return R"(\\.\pipe\)" + unique_id;
#else
    Path result(tmp_dir);
    result.append(unique_id);

    return result.str();
#endif
  }
};

/**
 * @test TS_R1_1, TS_R2_1, TS_R5_1
 *
 */
TEST_F(NotifyTest, NotifyReadyBasic) {
  SCOPED_TRACE(
      "// Launch the Router with only keepalive plugin, "
      "wait_for_ready_expected_result=true so the launcher is requested to set "
      "the NOTIFY_SOCKET and wait for the Router to raport it is ready");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  /*auto &router =*/launch_router(create_config_file(config_sections),
                                  /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R5_3
 *
 */
TEST_F(NotifyTest, NotifyReadyNoPlugin) {
  SCOPED_TRACE("// Launch the Router with no plugin configured");

  auto &router = launch_router(create_config_file({}),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 200ms, "default",
                               true, EXIT_FAILURE);

  EXPECT_EQ(EXIT_FAILURE, router.wait_for_exit());

  const auto error_found = router.get_full_output().find(
      "Error: The service is not configured to load or start any plugin.");

  EXPECT_NE(std::string::npos, error_found);
}

/**
 * @test TS_R4_1
 *
 */
TEST_F(NotifyTest, NotifyReadyHttpBackend) {
  SCOPED_TRACE(
      "// Launch the Router with the http_backend, also logger which gets "
      "added to the configuration implicitly by the launch_router method");

  const std::vector<std::string> config_sections{
      ConfigBuilder::build_section("http_auth_backend:somebackend",
                                   {
                                       {"backend", "file"},
                                       {"filename", create_password_file()},
                                   }),
  };

  /*auto &router =*/launch_router(create_config_file(config_sections),
                                  /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R4_2
 *
 */
TEST_F(NotifyTest, NotifyReadyMetadataCache) {
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();

  /*auto &metadata_server = */ mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(md_server_port)
          .http_port(md_server_http_port)
          .args());

  SCOPED_TRACE(
      "// Launch the Router with the routing and metadata_cache configuration");

  std::string nodes = "mysql://localhost:" + std::to_string(md_server_port);

  const std::vector<std::string> config_sections{
      ConfigBuilder::build_section(
          "routing:rw",
          {{"bind_port", std::to_string(port_pool_.get_next_available())},
           {"routing_strategy", "first-available"},
           {"destinations", "metadata-cache://test/default?role=PRIMARY"},
           {"protocol", "classic"}}),
      ConfigBuilder::build_section("metadata_cache",
                                   {
                                       {"cluster_type", "gr"},
                                       {"router_id", "1"},
                                       {"user", "mysql_router1_user"},
                                       {"connect_timeout", "1"},
                                       {"metadata_cluster", "test"},
                                   }),
  };

  const std::string state_file = create_state_file(
      get_test_temp_dir_name(),
      create_state_file_content("uuid", "", {md_server_port}, 0));

  /*auto &router =*/launch_router(
      create_config_file(config_sections, state_file),
      /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R4_3
 *
 */
TEST_F(NotifyTest, NotifyReadyHttpPlugins) {
  SCOPED_TRACE(
      "// Launch the Router with the http_server, http_auth_realm and "
      "http_auth_backend plugins");

  const std::vector<std::string> config_sections{
      ConfigBuilder::build_section(
          "http_server",
          {
              {"bind_address", "127.0.0.1"},
              {"port", std::to_string(port_pool_.get_next_available())},
          }),
      ConfigBuilder::build_section("http_auth_realm:somerealm",
                                   {
                                       {"backend", "somebackend"},
                                       {"method", "basic"},
                                       {"name", "Some Realm"},
                                   }),
      ConfigBuilder::build_section("http_auth_backend:somebackend",
                                   {
                                       {"backend", "file"},
                                       {"filename", create_password_file()},
                                   }),
  };

  /*auto &router =*/launch_router(create_config_file(config_sections),
                                  /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R4_4
 *
 */
TEST_F(NotifyTest, NotifyReadyManyPlugins) {
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();

  /*auto &metadata_server = */ mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(md_server_port)
          .http_port(md_server_http_port)
          .args());

  SCOPED_TRACE("// Launch the Router with multiple plugins");

  std::string nodes = "mysql://localhost:" + std::to_string(md_server_port);

  const std::vector<std::string> config_sections{
      ConfigBuilder::build_section(
          "routing:rw",
          {{"bind_port", std::to_string(port_pool_.get_next_available())},
           {"routing_strategy", "first-available"},
           {"destinations", "metadata-cache://test/default?role=PRIMARY"},
           {"protocol", "classic"}}),
      ConfigBuilder::build_section("metadata_cache",
                                   {
                                       {"cluster_type", "gr"},
                                       {"router_id", "1"},
                                       {"user", "mysql_router1_user"},
                                       {"connect_timeout", "1"},
                                       {"metadata_cluster", "test"},
                                   }),
#ifndef _WIN32
      ConfigBuilder::build_section("syslog", {}),
#else
      ConfigBuilder::build_section("eventlog", {}),
#endif
      ConfigBuilder::build_section("keepalive", {}),
      ConfigBuilder::build_section(
          "http_server",
          {
              {"bind_address", "127.0.0.1"},
              {"port", std::to_string(port_pool_.get_next_available())},
          }),
      ConfigBuilder::build_section("http_auth_realm:somerealm",
                                   {
                                       {"backend", "somebackend"},
                                       {"method", "basic"},
                                       {"name", "Some Realm"},
                                   }),
      ConfigBuilder::build_section("http_auth_backend:somebackend",
                                   {
                                       {"backend", "file"},
                                       {"filename", create_password_file()},
                                   }),
      ConfigBuilder::build_section("rest_api", {}),
      ConfigBuilder::build_section("rest_router",
                                   {{"require_realm", "somerealm"}}),
      ConfigBuilder::build_section("rest_routing",
                                   {{"require_realm", "somerealm"}}),
      ConfigBuilder::build_section("rest_metadata_cache",
                                   {{"require_realm", "somerealm"}}),
  };

  const std::string state_file = create_state_file(
      get_test_temp_dir_name(),
      create_state_file_content("uuid", "", {md_server_port}, 0));

  /*auto &router =*/launch_router(
      create_config_file(config_sections, state_file),
      /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R4_5
 *
 */
TEST_F(NotifyTest, NotifyReadyMetadataCacheNoServer) {
  SCOPED_TRACE(
      "// Launch the Router with the routing and metadata_cache configuration, "
      "we pick a socket where on which there is noone accepting to mimic "
      "unavailable cluster");

  const auto metadata_server_port = port_pool_.get_next_available();

  auto writer =
      config_writer(get_test_temp_dir_name())
          .section(
              "routing:rw",
              {{"bind_port", std::to_string(port_pool_.get_next_available())},
               {"routing_strategy", "first-available"},
               {"destinations", "metadata-cache://test/default?role=PRIMARY"},
               {"protocol", "classic"}})
          .section("metadata_cache", {
                                         {"cluster_type", "gr"},
                                         {"router_id", "1"},
                                         {"user", "mysql_router1_user"},
                                         {"connect_timeout", "1"},
                                         {"metadata_cluster", "test"},
                                     });

  // prepare keyring and state file
  auto &default_section = writer.sections()["DEFAULT"];
  init_keyring(default_section, get_test_temp_dir_name());
  const auto state_file = create_state_file(
      get_test_temp_dir_name(),
      create_state_file_content("uuid", "", {metadata_server_port}, 0));
  default_section["dynamic_state"] = state_file;

  // check that router never becomes READY (within a reasonable time) as
  // metadata-cache fails to connect
  //
  // if we could wait for 'STATUS=running' and then for "not READY=1", the test
  // could be faster. Until then the test needs a later timeout.

  /*auto &router =*/launch_router(writer.write(),
                                  /*wait_for_ready_expected_result*/ false,
                                  /*wait_for_ready_timeout*/ 5s);
}

/**
 * @test TS_R6_1, TS_R7_10, TS_R8_2
 *
 */
TEST_F(NotifyTest, NotifyReadySocketEmpty) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// Notification socket is empty so we should not get ready "
      "notification, still the Router should start and close successfully");
  auto &router = launch_router(create_config_file(config_sections),
                               false,  // wait_for_ready_expected_result
                               500ms,  // wait_for_ready_timeout
                               "",     // notification_socket_node
                               false   // don't bind the socket
  );

  EXPECT_TRUE(wait_log_contains(router,
                                "DEBUG .* NOTIFY_SOCKET is empty, skipping "
                                "sending 'READY=1' notification",
                                2s));
}

/**
 * @test TS_R7_1
 *
 */
TEST_F(NotifyTest, NotifyReadyNonExistingNotifySocket) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// We set the notification socket to some nonexisting socket, error "
      "should get reported but the Router should still start and close as "
      "expected");
  auto &router = launch_router(create_config_file(config_sections),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 500ms,
                               /*notifiication_socket_node*/ "default",
                               /*do_create_notify_socket*/ false,
                               /*expected_exit_code*/ EXIT_SUCCESS);

  const auto expected_error_codes = {
    make_error_code(std::errc::no_such_file_or_directory),
#if defined(_WIN32)
    std::error_code{ERROR_FILE_NOT_FOUND, std::system_category()},
#endif
  };

  EXPECT_TRUE(
      wait_log_contains(router,
                        "WARNING .* sending .* to NOTIFY_SOCKET='.*' "
                        "failed: " +
                            make_error_message_regex(expected_error_codes),
                        2s));
}

class NotifyTestInvalidSocketNameTest
    : public NotifyTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test TS_R7_3, TS_R7_4, TS_R7_12
 *
 */
TEST_P(NotifyTestInvalidSocketNameTest, NotifyTestInvalidSocketName) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// We set the notification socket to some nonexisting socket with some "
      "invalid name, error should get reported but the Router should still "
      "start and close as expected");
  auto &router = launch_router(create_config_file(config_sections),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 500ms,
                               /*notifiication_socket_node*/ GetParam(),
                               /*do_create_notify_socket*/ false,
                               /*expected_exit_code*/ EXIT_SUCCESS);

  const auto expected_error_codes = {
    make_error_code(std::errc::connection_refused),
    make_error_code(std::errc::no_such_file_or_directory),
#if defined(_WIN32)
    std::error_code{ERROR_FILE_NOT_FOUND, std::system_category()},
    std::error_code{ERROR_ACCESS_DENIED, std::system_category()},
#endif
  };

  EXPECT_TRUE(
      wait_log_contains(router,
                        "WARNING .* sending .* to NOTIFY_SOCKET='.*' "
                        "failed: " +
                            make_error_message_regex(expected_error_codes),
                        2s));
}

INSTANTIATE_TEST_SUITE_P(
    NotifyTestInvalidSocketName, NotifyTestInvalidSocketNameTest,
    ::testing::Values(
        "CON", "PRN",
        /*"/AUX", "NUL", "/COM1", */  // those do not cause error on pb2, they
                                      // are successfully used as a pipe names
        "-option", "--option", "./\\.", "@/router/ipc", "@\\path\\", "@/path/",
        "@\\", "@/"));

#ifndef _WIN32

/**
 * @test TS_R7_5
 *
 */
TEST_F(NotifyTest, NotifyReadyNotRelatedSocket) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// We set the notification socket to some existing socket but not one "
      "that anyone is reading from (mimic socket not created by the systemd)");

  const std::string socket_name =
      generate_notify_socket_path(get_test_temp_dir_name());

  net::io_context io_ctx;
  notify_socket_t notify_socket(io_ctx);

  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  auto &router = launch_router(create_config_file(config_sections),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 500ms,
                               /*notification_socket_node*/ socket_name,
                               /*do_create_notify_socket*/ false,
                               /*expected_exit_code*/ EXIT_SUCCESS,
                               /*wait_on_notify_socket*/ false);

  SCOPED_TRACE(
      "// We test a socket ready error scenario so we need to 'manually' wait "
      "for the signal handler to become ready to safely stop the Router");
  EXPECT_TRUE(wait_signal_handler_ready(router));
}

template <class T>
class NotifyReadyNotRelatedSocketNonDatagramTest : public NotifyTest {};

using NotifyReadyNotRelatedSocketNonDatagramTestTypes =
#if !defined(__APPLE__)
    ::testing::Types<local::stream_protocol, local::seqpacket_protocol>;
#else
    // on Mac os trying to create a socket type SOCK_SEQPACKET
    // leads to "Protocol not supported" error
    ::testing::Types<local::stream_protocol>;
#endif
TYPED_TEST_SUITE(NotifyReadyNotRelatedSocketNonDatagramTest,
                 NotifyReadyNotRelatedSocketNonDatagramTestTypes);

/**
 * @test TS_R7_7, TS_R7_8
 *
 */
TYPED_TEST(NotifyReadyNotRelatedSocketNonDatagramTest, check) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// We set the notification socket to some existing socket of type "
      "different than SOCK_DGRAM "
      "that anyone is reading from (mimic socket not created by the systemd)");

  const std::string socket_name =
      this->generate_notify_socket_path(TestFixture::get_test_temp_dir_name());

  net::io_context io_ctx;
  auto notify_socket = typename TypeParam::socket{io_ctx};
  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  auto &router = this->launch_router(this->create_config_file(config_sections),
                                     /*wait_for_ready_expected_result*/ false,
                                     /*wait_for_ready_timeout*/ 500ms,
                                     /*notifiication_socket_node*/ socket_name,
                                     /*do_create_notify_socket*/ false,
                                     /*expected_exit_code*/ EXIT_SUCCESS,
                                     /*wait_on_notify_socket*/ false);

  SCOPED_TRACE(
      "// We test a socket ready error scenario so we need to 'manually' wait "
      "for the signal handler to become ready to safely stop the Router");
  EXPECT_TRUE(this->wait_signal_handler_ready(router));
}

/**
 * @test TS_R7_9
 *
 */
TEST_F(NotifyTest, NotifyTestSocketNameTooLong) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE("// We use very long name for the notify socket name");
  const auto socket_name =
      generate_notify_socket_path(get_test_temp_dir_name(), 260);
  auto &router = launch_router(create_config_file(config_sections),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 500ms,
                               /*notifiication_socket_node*/ socket_name,
                               /*do_create_notify_socket*/ false,
                               /*expected_exit_code*/ EXIT_SUCCESS);

  EXPECT_TRUE(wait_log_contains(
      router,
      "WARNING .* sending .* to NOTIFY_SOCKET='.*' "
      "failed: " +
          make_error_code(std::errc::filename_too_long).message(),
      500ms));
}

/**
 * @test TS_R7_9
 *
 */
TEST_F(NotifyTest, NotifyTestSocketDirNameTooLong) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE("// We use very long name for the notify socket name");
  mysql_harness::Path socket_path{get_test_temp_dir_name()};
  socket_path.append(mysql_harness::RandomGenerator().generate_identifier(
      1025, mysql_harness::RandomGenerator::AlphabetLowercase));
  socket_path.append(mysql_harness::RandomGenerator().generate_identifier(
      12, mysql_harness::RandomGenerator::AlphabetLowercase));
  auto &router =
      launch_router(create_config_file(config_sections),
                    /*wait_for_ready_expected_result*/ false,
                    /*wait_for_ready_timeout*/ 500ms,
                    /*notifiication_socket_node*/ socket_path.c_str(),
                    /*do_create_notify_socket*/ false,
                    /*expected_exit_code*/ EXIT_SUCCESS);

  EXPECT_TRUE(wait_log_contains(
      router,
      "WARNING .* sending 'READY=1' to NOTIFY_SOCKET='" + socket_path.str() +
          "' failed: " +
          make_error_code(std::errc::filename_too_long).message(),
      500ms));
}

/**
 * @test TS_R7_2, TS_R8_3
 *
 */
TEST_F(NotifyTest, NotifyReadyNoSocketAccess) {
  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE(
      "// Let's create notify socket and limit its access to read-only");
  const std::string socket_name =
      generate_notify_socket_path(get_test_temp_dir_name());

  net::io_context io_ctx;
  notify_socket_t notify_socket{io_ctx};
  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  EXPECT_EQ(chmod(socket_name.c_str(), 0100), 0);

  SCOPED_TRACE(
      "// Let's launch the Router passing that NOTIFY_SOCKET as env variable");
  std::vector<std::pair<std::string, std::string>> env_vars;
  env_vars.emplace_back("NOTIFY_SOCKET", socket_name);
  const std::string conf_file = create_config_file(config_sections);
  auto &router = launch_router({"-c=" + conf_file}, env_vars, EXIT_SUCCESS);

  SCOPED_TRACE(
      "// We expect a warning and no notification sent to the socket, the "
      "Router should still exit with SUCCESS");
  EXPECT_FALSE(wait_for_notified_ready(notify_socket, 100ms));

  EXPECT_TRUE(wait_log_contains(
      router,
      "WARNING .* sending .* to NOTIFY_SOCKET='.*' "
      "failed: " +
          make_error_code(std::errc::permission_denied).message(),
      5s));

  SCOPED_TRACE(
      "// We test a socket ready error scenario so we need to 'manually' wait "
      "for the signal handler to become ready to safely stop the Router");
  EXPECT_TRUE(wait_signal_handler_ready(router));

  SCOPED_TRACE(
      "// Check explicitly that stopping the Router is also successfull "
      "despite the NOTIFY_SOCKET is not accessible");
  EXPECT_EQ(EXIT_SUCCESS, router.kill());
}

/**
 * @test TS_R7_11
 *
 */
TEST_F(NotifyTest, NotifyReadySymlink) {
  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  SCOPED_TRACE("// Let's create notify socket and a symbolic link to it");
  const std::string socket_name =
      generate_notify_socket_path(get_test_temp_dir_name());
  const std::string symlink_name =
      generate_notify_socket_path(get_test_temp_dir_name());

  net::io_context io_ctx;
  notify_socket_t notify_socket{io_ctx};

  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  const std::string socket_name_full = Path(socket_name).real_path().str();
  EXPECT_EQ(symlink(socket_name_full.c_str(), symlink_name.c_str()), 0);

  SCOPED_TRACE(
      "// Let's launch the Router passing the symbolic link to the socket as "
      "NOTIFY_SOCKET");
  std::vector<std::pair<std::string, std::string>> env_vars;
  env_vars.emplace_back("NOTIFY_SOCKET", symlink_name);
  const std::string conf_file = create_config_file(config_sections);
  /*auto &router =*/launch_router({"-c=" + conf_file}, env_vars, EXIT_SUCCESS);

  SCOPED_TRACE("// We expect READY notification on the socket");
  EXPECT_TRUE(wait_for_notified_ready(notify_socket, 5s));
}

#endif

/**
 * @test TS_R8_1
 *
 */
TEST_F(NotifyTest, NotifyStoppingBasic) {
  SCOPED_TRACE("// Launch the Router with only keepalive plugin");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {}),
  };

  const std::string socket_name =
      generate_notify_socket_path(get_test_temp_dir_name());

  net::io_context io_ctx;
  wait_socket_t notify_socket{io_ctx};

  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  auto &router = launch_router(create_config_file(config_sections),
                               /*wait_for_ready_expected_result*/ false,
                               /*wait_for_ready_timeout*/ 5s,
                               /*notifiication_socket_node*/ socket_name,
                               /*do_create_notify_socket*/ false,
                               /*expected_exit_code*/ EXIT_SUCCESS,
                               /*wait_on_notify_socket*/ false);

  EXPECT_TRUE(wait_for_notified_ready(notify_socket, 5s));

  stdx::expected<void, std::error_code> stopped_notification_read{
      stdx::unexpected(std::error_code{})};
  auto wait_for_stopped = std::thread([&] {
    stopped_notification_read = wait_for_notified_stopping(notify_socket, 5s);
  });

  ASSERT_THAT(router.kill(), testing::Eq(0));
  wait_for_stopped.join();

  EXPECT_TRUE(wait_log_contains(
      router, "DEBUG .* Using NOTIFY_SOCKET=.* for the 'STOPPING=1", 500ms));

  EXPECT_TRUE(stopped_notification_read);
}

class NotifyBootstrapNotAffectedTest
    : public NotifyTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test TS_R3_1, TS_R3_2, TS_R10_1, TS_R10_2
 *
 */
TEST_P(NotifyBootstrapNotAffectedTest, NotifyBootstrapNotAffected) {
  TempDirectory temp_test_dir;

  SCOPED_TRACE("// Launch our metadata server we bootstrap against");
  const auto metadata_server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();

  /*auto &metadata_server = */ mock_server_spawner().spawn(
      mock_server_cmdline("bootstrap_gr.js")
          .port(metadata_server_port)
          .http_port(http_port)
          .args());

  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({metadata_server_port}), 0,
                    {metadata_server_port});

  SCOPED_TRACE(
      "// Create notification socket and pass it to the Router as env "
      "variable");
  const std::string socket_name =
      generate_notify_socket_path(get_test_temp_dir_name());

  net::io_context io_ctx;
  wait_socket_t notify_socket{io_ctx};

  ASSERT_NO_ERROR(notify_socket.open());
  ASSERT_NO_ERROR(notify_socket.bind({socket_name}));

  SCOPED_TRACE("// Listen for notification while we are bootstrapping");
  stdx::expected<void, std::error_code> ready_notification_read{
      stdx::unexpected(std::error_code{})};
  auto wait_for_stopped = std::thread([&] {
    ready_notification_read =
        wait_for_notified(notify_socket, GetParam(), 300ms);
  });

  SCOPED_TRACE("// Do the bootstrap");
  std::vector<std::pair<std::string, std::string>> env_vars;
  env_vars.emplace_back("NOTIFY_SOCKET", socket_name);

  auto &router = launch_router(
      {"--bootstrap=localhost:" + std::to_string(metadata_server_port),
       "-d=" + temp_test_dir.name(),
       "--conf-set-option=DEFAULT.plugin_folder=" +
           ProcessManager::get_plugin_dir().str(),
       "--report-host=dont.query.dns"},
      env_vars, EXIT_SUCCESS,
      RouterComponentBootstrapTest::kBootstrapOutputResponder);

  SCOPED_TRACE("// Bootstrap should be successful");
  check_exit_code(router, EXIT_SUCCESS);

  SCOPED_TRACE("// No notification should be sent by the Router");
  wait_for_stopped.join();
  ASSERT_EQ(ready_notification_read,
            stdx::unexpected(make_error_code(std::errc::timed_out)));
}

INSTANTIATE_TEST_SUITE_P(
    NotifyBootstrapNotAffected, NotifyBootstrapNotAffectedTest,
    ::testing::Values("READY=1",
                      "STOPPING=1\nSTATUS=Router shutdown in progress\n"));

/**
 * @test TS_R5_5
 *
 */
TEST_F(NotifyTest, NotifyReadyMockServerPlugin) {
  SCOPED_TRACE(
      "// Launch the Router with mock_server  plugin, "
      "wait_for_ready_expected_result=true so the launcher is requested to set "
      "the NOTIFY_SOCKET and wait for the Router to raport it is ready");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section(
          "mock_server:test",
          {
              {"filename", get_data_dir().join("my_port.js").str()},
              {"port", std::to_string(port_pool_.get_next_available())},
          }),
  };

  /*auto &router =*/launch_router(create_config_file(config_sections),
                                  /*wait_for_ready_expected_result*/ true);
}

/**
 * @test TS_R6_2
 *
 */
TEST_F(NotifyTest, NotificationSocketNotSet) {
  SCOPED_TRACE("// Launch the Router when NOTIFY_SOCKET is not set");

  const std::vector<std::string> config_sections{
      // logger section is added implicitly by launch_router
      // ConfigBuilder::build_section("logger", {}),
      ConfigBuilder::build_section("keepalive", {})};

  const std::string conf_file = create_config_file(config_sections);

  auto &router = ProcessManager::launch_router({"-c", conf_file}, EXIT_SUCCESS,
                                               true, false,
                                               /*wait_for_notify_ready=*/-1s);

  SCOPED_TRACE(
      "// We do not use notify socket so we need to 'manually' wait for the "
      "signal handler to become ready to safely stop the Router");
  EXPECT_TRUE(wait_signal_handler_ready(router));

  EXPECT_EQ(EXIT_SUCCESS, router.kill());
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
