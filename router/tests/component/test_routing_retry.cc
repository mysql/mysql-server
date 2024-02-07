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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "mysql/harness/net_ts/impl/socket.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::chrono_literals;

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

class RoutingRetryTestBase : public RouterComponentTest {
 protected:
  TempDirectory conf_dir_;

  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t router_port_{port_pool_.get_next_available()};
};

struct ConnectionParam {
  std::string testname;

  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;

  [[nodiscard]] bool redundant_combination() const {
    return
        // same as DISABLED|DISABLED
        (client_ssl_mode == kDisabled && server_ssl_mode == kAsClient) ||
        // same as DISABLED|REQUIRED
        (client_ssl_mode == kDisabled && server_ssl_mode == kPreferred) ||
        // same as PREFERRED|PREFERRED
        (client_ssl_mode == kPreferred && server_ssl_mode == kRequired) ||
        // same as REQUIRED|REQUIRED
        (client_ssl_mode == kRequired && server_ssl_mode == kAsClient) ||
        // same as REQUIRED|REQUIRED
        (client_ssl_mode == kRequired && server_ssl_mode == kPreferred);
  }
};

const ConnectionParam connection_params[] = {
    // DISABLED
    {
        "DISABLED__DISABLED",
        kDisabled,  // client_ssl_mode
        kDisabled,  // server_ssl_mode
    },
    {
        "DISABLED__AS_CLIENT",
        kDisabled,
        kAsClient,
    },
    {
        "DISABLED__REQUIRED",
        kDisabled,
        kRequired,
    },
    {
        "DISABLED__PREFERRED",
        kDisabled,
        kPreferred,
    },

    // PASSTHROUGH
    {
        "PASSTHROUGH__AS_CLIENT",
        kPassthrough,
        kAsClient,
    },

    // PREFERRED
    {
        "PREFERRED__DISABLED",
        kPreferred,
        kDisabled,
    },
    {
        "PREFERRED__AS_CLIENT",
        kPreferred,
        kAsClient,
    },
    {
        "PREFERRED__PREFERRED",
        kPreferred,
        kPreferred,
    },
    {
        "PREFERRED__REQUIRED",
        kPreferred,
        kRequired,
    },

    // REQUIRED ...
    {
        "REQUIRED__DISABLED",
        kRequired,
        kDisabled,
    },
    {
        "REQUIRED__AS_CLIENT",
        kRequired,
        kAsClient,
    },
    {
        "REQUIRED__PREFERRED",
        kRequired,
        kPreferred,
    },
    {
        "REQUIRED__REQUIRED",
        kRequired,
        kRequired,
    },
};

class RoutingRetryTest : public RoutingRetryTestBase,
                         public ::testing::WithParamInterface<ConnectionParam> {
};

TEST_P(RoutingRetryTest, retry_at_greeting) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Requirement",
                 "MUST retry if connect fails with transient errors like 1040 "
                 "max-connections-reached.");
  RecordProperty("Description",
                 "Retry the connect when the greeting fails with 1040 "
                 "max-connections-reached.");

  launch_mysql_server_mock(
      mysql_server_mock_cmdline_args(
          get_data_dir().join("max_connections_reached_at_greeting.js").str(),
          server_port_, 0, 0, "", "127.0.0.1", true),
      EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", std::string(GetParam().client_ssl_mode)},
          {"server_ssl_mode", std::string(GetParam().server_ssl_mode)},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_NO_ERROR(connect_res);
  }

  proc.send_clean_shutdown_event();
}

TEST_P(RoutingRetryTest, retry_at_auth) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Requirement",
                 "MUST retry if connect fails with transient errors like 1040 "
                 "max-connections-reached.");
  RecordProperty(
      "Description",
      "Retry the connect when auth fails with 1040 max-connections-reached.");

  auto can_fetch_password = !(GetParam().client_ssl_mode == kDisabled ||
                              GetParam().client_ssl_mode == kPassthrough ||
                              (GetParam().client_ssl_mode == kPreferred &&
                               GetParam().server_ssl_mode == kAsClient));

  launch_mysql_server_mock(
      mysql_server_mock_cmdline_args(
          get_data_dir().join("max_connections_reached_at_auth.js").str(),
          server_port_, 0, 0, "", "127.0.0.1", true),
      EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", std::string(GetParam().client_ssl_mode)},
          {"server_ssl_mode", std::string(GetParam().server_ssl_mode)},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    if (can_fetch_password) {
      ASSERT_NO_ERROR(connect_res);
    } else {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1040);
    }
  }

  proc.send_clean_shutdown_event();
}

INSTANTIATE_TEST_SUITE_P(Spec, RoutingRetryTest,
                         ::testing::ValuesIn(connection_params),
                         [](const auto &info) {
                           return "via_" + info.param.testname;
                         });

class RoutingRetryFailTest : public RoutingRetryTestBase {};

TEST_F(RoutingRetryFailTest, explicit_timeout) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR1.1");
  RecordProperty(
      "Requirement",
      "The connect MUST be retried at max `connect_retry_timeout` seconds.");

  launch_mysql_server_mock(
      get_data_dir().join("handshake_too_many_con_error.js").str(),
      server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "PREFERRED"},
          {"server_ssl_mode", "PREFERRED"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
          {"connect_retry_timeout", "0.5"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto start = std::chrono::steady_clock::now();
    auto connect_res = cli.connect("127.0.0.1", router_port_);
    auto dur = std::chrono::steady_clock::now() - start;
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1040);

    EXPECT_GT(dur, 500ms);
    EXPECT_LT(dur, 1500ms);
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingRetryFailTest, default_timeout) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR1.2");
  RecordProperty("Requirement",
                 "If `connect_retry_timeout` is not specified, it MUST default "
                 "to 2 seconds.");

  launch_mysql_server_mock(
      get_data_dir().join("handshake_too_many_con_error.js").str(),
      server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "PREFERRED"},
          {"server_ssl_mode", "PREFERRED"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto start = std::chrono::steady_clock::now();
    auto connect_res = cli.connect("127.0.0.1", router_port_);
    auto dur = std::chrono::steady_clock::now() - start;
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1040);

    EXPECT_GT(dur, 6s);
    EXPECT_LT(dur, 8s);
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingRetryFailTest, negative_timeout) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If `connect_retry_timeout` is outside the valid range, "
                 "Router MUST fail to start.");
  RecordProperty("Description", "'connect_retry_timeout = -1' fails");

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "PREFERRED"},
          {"server_ssl_mode", "PREFERRED"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
          {"connect_retry_timeout", "-1"},
      });

  auto &proc = router_spawner()
                   .expected_exit_code(EXIT_FAILURE)
                   .wait_for_sync_point(Spawner::SyncPoint::NONE)
                   .wait_for_notify_ready(-1s)
                   .spawn({"-c", writer.write()});

  EXPECT_NO_THROW(proc.wait_for_exit());

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr(
                  "option connect_retry_timeout in [routing:under_test] needs "
                  "value between 0 and 3600 inclusive, was '-1'"));
}

TEST_F(RoutingRetryTest, too_large_timeout) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If `connect_retry_timeout` is outside the valid range, "
                 "Router MUST fail to start.");
  RecordProperty("Description", "'connect_retry_timeout = 3601' fails");

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "PREFERRED"},
          {"server_ssl_mode", "PREFERRED"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
          {"connect_retry_timeout", "3601"},
      });

  auto &proc = router_spawner()
                   .expected_exit_code(EXIT_FAILURE)
                   .wait_for_sync_point(Spawner::SyncPoint::NONE)
                   .wait_for_notify_ready(-1s)
                   .spawn({"-c", writer.write()});

  EXPECT_NO_THROW(proc.wait_for_exit());

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr(
                  "option connect_retry_timeout in [routing:under_test] needs "
                  "value between 0 and 3600 inclusive, was '3601'"));
}

TEST_F(RoutingRetryFailTest, not_a_float) {
  RecordProperty("Worklog", "15721");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If `connect_retry_timeout` is outside the valid range, "
                 "Router MUST fail to start.");
  RecordProperty("Description", "'connect_retry_timeout = abc' fails");

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "PREFERRED"},
          {"server_ssl_mode", "PREFERRED"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
          {"connect_retry_timeout", "abc"},
      });

  auto &proc = router_spawner()
                   .expected_exit_code(EXIT_FAILURE)
                   .wait_for_sync_point(Spawner::SyncPoint::NONE)
                   .wait_for_notify_ready(-1s)
                   .spawn({"-c", writer.write()});

  EXPECT_NO_THROW(proc.wait_for_exit());

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr(
                  "option connect_retry_timeout in [routing:under_test] needs "
                  "value between 0 and 3600 inclusive, was 'abc'"));
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(mysql_harness::Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
