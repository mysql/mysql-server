/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates.

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

#include <chrono>
#include <fstream>
#include <functional>
#include <system_error>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"  // AppInfo
#include "mysqlrouter/io_backend.h"
#include "mysqlrouter/io_component.h"

#include "gtest_consoleoutput.h"
#include "mysql_routing.h"
#include "plugin_config.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"
#include "test/helpers.h"  // init_test_logger

// since this function is only meant to be used here (for testing purposes),
// it's not available in the headers.
void validate_socket_info_test_proxy(
    const std::string &err_prefix, const mysql_harness::ConfigSection *section,
    const RoutingPluginConfig &config);

using mysql_harness::Path;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::StrEq;

// define what is available in routing_plugin.cc
extern const mysql_harness::AppInfo *g_app_info;

string g_cwd;
Path g_origin;

class RoutingPluginTests : public ConsoleOutputTest {
  TcpPortPool tcp_port_pool_;

 protected:
  void SetUp() override {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path = std::make_unique<Path>(g_cwd);
    config_path->append("test_routing_plugin.conf");
    cmd = app_mysqlrouter->str() + " -c " + config_path->str();

    bind_address =
        "127.0.0.1:" + std::to_string(tcp_port_pool_.get_next_available());
    destinations =
        "127.0.0.1:" + std::to_string(tcp_port_pool_.get_next_available());
    socket = rundir + "/unix_socket";
    routing_strategy = "round-robin";
    mode = "read-only";
    connect_timeout = "1";
    client_connect_timeout = "9";
    max_connect_errors = "100";
    protocol = "classic";

    IoComponent::get_instance().init(1, IoBackend::preferred());
  }

  bool in_missing(std::vector<std::string> missing, std::string needle) {
    return std::find(missing.begin(), missing.end(), needle) != missing.end();
  }

  void reset_config(std::vector<std::string> missing = {},
                    bool add_break = false) {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << temp_dir->str() << "\n";
      ofs_config << "config_folder = " << temp_dir->str() << "\n";
      ofs_config << "data_folder = " << temp_dir->str() << "\n\n";
      ofs_config << "[routing:tests]\n";

      using ConfigOption = std::pair<std::string, std::string &>;
      std::vector<ConfigOption> routing_config_options{
          {"bind_address", std::ref(bind_address)},
          {"socket", std::ref(socket)},
          {"destinations", std::ref(destinations)},
          {"routing_strategy", std::ref(routing_strategy)},
          {"mode", std::ref(mode)},
          {"connect_timeout", std::ref(connect_timeout)},
          {"client_connect_timeout", std::ref(client_connect_timeout)},
          {"max_connect_errors", std::ref(max_connect_errors)},
          {"protocol", std::ref(protocol)}};
      for (auto &option : routing_config_options) {
        if (!in_missing(missing, option.first)) {
          ofs_config << option.first << " = " << option.second << "\n";
        }
      }

      // Following is an incorrect [routing] entry. If the above is valid, this
      // will make sure Router stops.
      if (add_break) {
        ofs_config << "\n[routing:break]\n";
      }

      ofs_config << "\n";
      ofs_config.close();
    }
  }

  void TearDown() override {
    if (unlink(config_path->c_str()) == -1) {
      if (errno != ENOENT) {
        auto ec = std::error_code{errno, std::generic_category()};
        // File missing is OK.
        std::cerr << "Failed removing " << config_path->str() << ": "
                  << ec.message() << "(" << ec.value() << ")" << std::endl;
      }
    }
    ConsoleOutputTest::TearDown();
  }

  const string plugindir = "path/to/plugindir";
  const string logdir = "/path/to/logdir";
  const string program = "routing_plugin_test";
  const string rundir = "/path/to/rundir";
  const string cfgdir = "/path/to/cfgdir";
  const string datadir = "/path/to/datadir";
  string bind_address;
  string destinations;
  string socket;
  string routing_strategy;
  string mode;
  string connect_timeout;
  string client_connect_timeout;
  string max_connect_errors;
  string protocol;

  std::unique_ptr<Path> config_path;
  std::string cmd;
};

TEST_F(RoutingPluginTests, PluginObject) {
  ASSERT_EQ(harness_plugin_routing.abi_version, 0x0200U);
  ASSERT_EQ(harness_plugin_routing.plugin_version,
            static_cast<uint32_t>(VERSION_NUMBER(0, 0, 1)));
  ASSERT_EQ(harness_plugin_routing.conflicts_length, 0U);
  ASSERT_THAT(harness_plugin_routing.conflicts, IsNull());
  ASSERT_THAT(harness_plugin_routing.deinit, NotNull());
  ASSERT_THAT(harness_plugin_routing.brief,
              StrEq("Routing MySQL connections between MySQL "
                    "clients/connectors and servers"));
}

TEST_F(RoutingPluginTests, InitAppInfo) {
  ASSERT_THAT(g_app_info, IsNull());

  mysql_harness::AppInfo test_app_info{
      program.c_str(), plugindir.c_str(), logdir.c_str(), rundir.c_str(),
      cfgdir.c_str(),  datadir.c_str(),   nullptr};

  mysql_harness::PluginFuncEnv env(&test_app_info, nullptr);
  harness_plugin_routing.init(&env);
  ASSERT_TRUE(env.exit_ok());

  ASSERT_THAT(g_app_info, Not(IsNull()));
  ASSERT_THAT(program.c_str(), StrEq(g_app_info->program));

  harness_plugin_routing.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

TEST_F(RoutingPluginTests, ListeningTcpSocket) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("bind_address", "127.0.0.1:15508");

  EXPECT_NO_THROW({
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
  });
}

#ifndef _WIN32
TEST_F(RoutingPluginTests, ListeningUnixSocket) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("socket", "./socket");  // if this test fails, check if you don't
                                      // have this file hanging around

  EXPECT_NO_THROW({
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
  });
}

TEST_F(RoutingPluginTests, ListeningBothSockets) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("bind_address", "127.0.0.1:15508");
  section.add("socket", "./socket");  // if this test fails, check if you don't
                                      // have this file hanging around

  EXPECT_NO_THROW({
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
  });
}

TEST_F(RoutingPluginTests, TwoUnixSocketsWithoutTcp) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section1 = cfg.add("routing", "test_route1");
  section1.add("destinations", "localhost:1234");
  section1.add("mode", "read-only");
  section1.add("socket", "./socket1");
  mysql_harness::ConfigSection &section2 = cfg.add("routing", "test_route2");
  section2.add("destinations", "localhost:1234");
  section2.add("mode", "read-only");
  section2.add("socket", "./socket2");

  mysql_harness::AppInfo info;
  info.config = &cfg;
  mysql_harness::PluginFuncEnv env(&info, nullptr);

  EXPECT_NO_THROW({ harness_plugin_routing.init(&env); });

  harness_plugin_routing.deinit(&env);
}

TEST_F(RoutingPluginTests, TwoUnixSocketsWithTcp) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section1 = cfg.add("routing", "test_route1");
  section1.add("destinations", "localhost:1234");
  section1.add("mode", "read-only");
  section1.add("bind_address", "127.0.0.1:15501");
  section1.add("socket", "./socket1");
  mysql_harness::ConfigSection &section2 = cfg.add("routing", "test_route2");
  section2.add("destinations", "localhost:1234");
  section2.add("mode", "read-only");
  section2.add("bind_address", "127.0.0.1:15502");
  section2.add("socket", "./socket2");

  mysql_harness::AppInfo info;
  info.config = &cfg;
  mysql_harness::PluginFuncEnv env(&info, nullptr);

  EXPECT_NO_THROW({ harness_plugin_routing.init(&env); });

  harness_plugin_routing.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

static std::string make_string(size_t len, char c = 'a') {
  std::string res(len, c);

  assert(res.length() == len);

  return res;
}

static void test_socket_length(const std::string &socket_name, size_t max_len) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("socket", socket_name);

  if (socket_name.length() <= max_len) {
    EXPECT_NO_THROW({
      RoutingPluginConfig config(&section);
      validate_socket_info_test_proxy("", &section, config);
    });
  } else {
    EXPECT_THROW(RoutingPluginConfig config(&section), std::invalid_argument);
  }
}

TEST_F(RoutingPluginTests, ListeningSocketNameLength) {
#ifndef _WIN32
  const size_t MAX_SOCKET_NAME_LEN = sizeof(sockaddr_un().sun_path) - 1;
#else
  // doesn't really matter
  const size_t MAX_SOCKET_NAME_LEN = 100;
#endif

  std::string socket_name;

  socket_name = make_string(MAX_SOCKET_NAME_LEN - 1);
  test_socket_length(socket_name, MAX_SOCKET_NAME_LEN);

  socket_name = make_string(MAX_SOCKET_NAME_LEN);
  test_socket_length(socket_name, MAX_SOCKET_NAME_LEN);

  socket_name = make_string(MAX_SOCKET_NAME_LEN + 1);
  test_socket_length(socket_name, MAX_SOCKET_NAME_LEN);
}

TEST_F(RoutingPluginTests, TwoNonuniqueUnixSockets) {
  // TODO add after implementing plugin lifecycle (WL9558),
  // use TwoNonuniqueTcpSockets as an example
  // (exception is thrown in plugin start(), in a separate thread)
}

#endif

TEST_F(RoutingPluginTests, TwoNonuniqueTcpSockets) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section1 = cfg.add("routing", "test_route1");
  section1.add("destinations", "localhost:1234");
  section1.add("mode", "read-only");
  section1.add("bind_address", "127.0.0.1:15508");
  mysql_harness::ConfigSection &section2 = cfg.add("routing", "test_route2");
  section2.add("destinations", "localhost:1234");
  section2.add("mode", "read-only");
  section2.add("bind_address", "127.0.0.1:15508");

  mysql_harness::AppInfo info;
  info.config = &cfg;
  mysql_harness::PluginFuncEnv env(&info, nullptr);

  try {
    harness_plugin_routing.init(&env);

    std::exception_ptr e;
    std::tie(std::ignore, e) = env.pop_error();
    if (e) std::rethrow_exception(e);

    FAIL() << "Expected std::invalid_argument to be thrown";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ(
        "in [routing:test_route2]: duplicate IP or name found in bind_address "
        "'127.0.0.1:15508'",
        e.what());
    SUCCEED();
  } catch (...) {
    FAIL() << "Expected std::invalid_argument to be thrown";
  }
  harness_plugin_routing.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

#ifndef _WIN32
TEST_F(RoutingPluginTests, EmptyUnixSocket) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("socket", "");

  // If this not provided, RoutingPluginConfig() will throw with its own error,
  // which will be misleading. This line should not influence throwing/not
  // throwing the right error ("invalid socket ''")
  section.add("bind_address", "127.0.0.1:15508");

  try {
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
    FAIL() << "Expected std::invalid_argument to be thrown";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ("invalid socket ''", e.what());
    SUCCEED();
  } catch (...) {
    FAIL() << "Expected std::invalid_argument to be thrown";
  }
}

TEST_F(RoutingPluginTests, ListeningHostIsInvalid) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "localhost:1234");
  section.add("mode", "read-only");
  section.add("bind_address", "host.that.does.not.exist:15508");

  try {
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
    FAIL() << "Expected std::invalid_argument to be thrown";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ(
        "invalid IP or name in bind_address 'host.that.does.not.exist:15508'",
        e.what());
    SUCCEED();
  } catch (...) {
    FAIL() << "Expected std::invalid_argument to be thrown";
  }
}
#endif

TEST_F(RoutingPluginTests, Ipv6LinkLocal) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "[fe80::3617:ebff:fecb:587e%3]:3306");
  section.add("mode", "read-only");
  section.add("bind_port", "6446");

  try {
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
  } catch (const std::exception &e) {
    FAIL() << "no exception, got: " << e.what();
  }
}

TEST_F(RoutingPluginTests, InvalidIpv6) {
  mysql_harness::Config cfg;
  mysql_harness::ConfigSection &section = cfg.add("routing", "test_route");
  section.add("destinations", "[fe80::3617:ebff:fecb:587e@3]:3306");
  section.add("mode", "read-only");
  section.add("bind_port", "6446");

  try {
    RoutingPluginConfig config(&section);
    validate_socket_info_test_proxy("", &section, config);
    FAIL() << "expected to throw, but succeeded";
  } catch (const std::exception &e) {
    EXPECT_THAT(e.what(), ::testing::HasSubstr(
                              "invalid IPv6 address: illegal character(s)"));
  }
}

struct RoutingConfigParam {
  const char *test_name;

  std::vector<std::pair<const char *, const char *>> entries;

  std::function<void(const RoutingPluginConfig &)> checker;
};

/**
 * check valid routing config constructs.
 */
class RoutingConfigTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<RoutingConfigParam> {
 public:
  void SetUp() override {
    cfg_.clear();

    mysql_harness::ConfigSection &section = cfg_.add("routing", "test_route");
    section.add("destinations", "127.0.0.1:3306");
    section.add("mode", "read-only");
    section.add("bind_port", "6446");
  }
  mysql_harness::Config cfg_{mysql_harness::Config::allow_keys};
};

/**
 * check the option works in the [DEFAULT] section.
 */
TEST_P(RoutingConfigTest, default_option) {
  for (auto const &kv : GetParam().entries) {
    cfg_.set_default(kv.first, kv.second);
  }

  mysql_harness::ConfigSection &section = cfg_.get("routing", "test_route");

  try {
    RoutingPluginConfig config(&section);

    ASSERT_NO_FATAL_FAILURE(GetParam().checker(config));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }
}

/**
 * check the option works in the [routing] section.
 */
TEST_P(RoutingConfigTest, section_option) {
  mysql_harness::ConfigSection &section = cfg_.get("routing", "test_route");
  for (auto const &kv : GetParam().entries) {
    section.add(kv.first, kv.second);
  }

  try {
    RoutingPluginConfig config(&section);

    ASSERT_NO_FATAL_FAILURE(GetParam().checker(config));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }
}

/**
 * check the option works in the [routing] section and broken [DEFAULT] is
 * ignored.
 */
TEST_P(RoutingConfigTest, section_option_with_default) {
  // set the 'key' to some value, just to check it isn't used
  for (auto const &kv : GetParam().entries) {
    cfg_.set_default(kv.first, "some-other-value");
  }

  mysql_harness::ConfigSection &section = cfg_.get("routing", "test_route");
  for (auto const &kv : GetParam().entries) {
    section.add(kv.first, kv.second);
  }

  try {
    RoutingPluginConfig config(&section);

    ASSERT_NO_FATAL_FAILURE(GetParam().checker(config));
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }
}

RoutingConfigParam routing_config_params[] = {
    // server-ssl-mode
    //
    {"server_ssl_mode_default",
     {},
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kAsClient);
     }},
    {"server_ssl_mode_empty",
     {
         {"server_ssl_mode", ""},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kAsClient);
     }},

    {"server_ssl_mode_as_client",
     {
         {"server_ssl_mode", "as_client"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kAsClient);
     }},
    {"server_ssl_mode_as_client_mixed_case",
     {
         {"server_ssl_mode", "as_Client"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kAsClient);
     }},
    {"server_ssl_mode_preferred",
     {
         {"server_ssl_mode", "preferred"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kPreferred);
     }},
    {"server_ssl_mode_preferred_mixed_case",
     {
         {"server_ssl_mode", "PreFerred"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kPreferred);
     }},
    {"server_ssl_mode_disabled",
     {
         {"server_ssl_mode", "disabled"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kDisabled);
     }},
    {"server_ssl_mode_disabled_mixed_case",
     {
         {"server_ssl_mode", "DisAbled"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kDisabled);
     }},
    {"server_ssl_mode_required",
     {
         {"server_ssl_mode", "required"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kRequired);
     }},
    {"server_ssl_mode_required_mixed_case",
     {
         {"server_ssl_mode", "reQuired"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.dest_ssl_mode, SslMode::kRequired);
     }},

    // client-ssl-mode

    {"client_ssl_mode_default",
     {},
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPassthrough);
     }},
    {"client_ssl_mode_empty",
     {
         {"client_ssl_mode", ""},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPassthrough);
     }},

    {"client_ssl_mode_passthrough",
     {
         {"client_ssl_mode", "passthrough"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPassthrough);
     }},
    {"client_ssl_mode_passthrough_mixed_case",
     {
         {"client_ssl_mode", "PassThrough"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPassthrough);
     }},
    {"client_ssl_mode_preferred",
     {
         {"client_ssl_mode", "preferred"},
         {"client_ssl_cert", "some-cert.pem"},
         {"client_ssl_key", "some-key.pem"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPreferred);
     }},
    {"client_ssl_mode_preferred_mixed_case",
     {
         {"client_ssl_mode", "PreFerred"},
         {"client_ssl_cert", "some-cert.pem"},
         {"client_ssl_key", "some-key.pem"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kPreferred);
     }},
    {"client_ssl_mode_disabled",
     {
         {"client_ssl_mode", "disabled"},
         {"client_ssl_cert", "some-cert.pem"},
         {"client_ssl_key", "some-key.pem"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kDisabled);
     }},
    {"client_ssl_mode_disabled_mixed_case",
     {
         {"client_ssl_mode", "DisAbled"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kDisabled);
     }},
    {"client_ssl_mode_required",
     {
         {"client_ssl_mode", "required"},
         {"client_ssl_cert", "some-cert.pem"},
         {"client_ssl_key", "some-key.pem"},
     },
     [](const auto &config) {
       ASSERT_THAT(config.source_ssl_mode, SslMode::kRequired);
     }},
    {"client_ssl_mode_required_mixed_case",
     {
         {"client_ssl_mode", "reQuired"},
         {"client_ssl_cert", "some-cert.pem"},
         {"client_ssl_key", "some-key.pem"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.source_ssl_mode, SslMode::kRequired);
       ASSERT_EQ(config.source_ssl_cert, "some-cert.pem");
       ASSERT_EQ(config.source_ssl_key, "some-key.pem");
     }},

    // server-ssl-verify
    {"server_ssl_verify_default",
     {},
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kDisabled);
     }},
    {"server_ssl_verify_empty",
     {
         {"server_ssl_verify", ""},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kDisabled);
     }},
    {"server_ssl_verify_disabled",
     {
         {"server_ssl_verify", "disabled"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kDisabled);
     }},
    {"server_ssl_verify_disabled_mixed_case",
     {
         {"server_ssl_verify", "dIsabled"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kDisabled);
     }},
    {"server_ssl_verify_verify_ca_with_ca_file",
     {
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", "some-ca.pem"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kVerifyCa);
       ASSERT_EQ(config.dest_ssl_ca_file, "some-ca.pem");
       ASSERT_EQ(config.dest_ssl_ca_dir, "");
     }},
    {"server_ssl_verify_verify_ca_with_capath",
     {
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_capath", "some-capath"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kVerifyCa);
       ASSERT_EQ(config.dest_ssl_ca_file, "");
       ASSERT_EQ(config.dest_ssl_ca_dir, "some-capath");
     }},
    {"server_ssl_verify_verify_ca_mixed_case_with_ca",
     {
         {"server_ssl_verify", "Verify_Ca"},
         {"server_ssl_ca", "some-ca.pem"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kVerifyCa);
       ASSERT_EQ(config.dest_ssl_ca_file, "some-ca.pem");
       ASSERT_EQ(config.dest_ssl_ca_dir, "");
     }},
    {"server_ssl_verify_verify_ca_mixed_case_with_capath",
     {
         {"server_ssl_verify", "Verify_Ca"},
         {"server_ssl_capath", "some-capath"},
     },
     [](const auto &config) {
       ASSERT_EQ(config.dest_ssl_verify, SslVerify::kVerifyCa);
       ASSERT_EQ(config.dest_ssl_ca_file, "");
       ASSERT_EQ(config.dest_ssl_ca_dir, "some-capath");
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingConfigTest,
                         ::testing::ValuesIn(routing_config_params),
                         [](auto const &info) { return info.param.test_name; });

struct RoutingConfigFailParam {
  const char *test_name;

  std::vector<std::pair<const char *, const char *>> entries;

  std::function<void(const std::exception &)> checker;
};

/**
 * check invalid routing config fails.
 */
class RoutingConfigFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<RoutingConfigFailParam> {
 public:
  void SetUp() override {
    cfg_ = mysql_harness::Config{mysql_harness::Config::allow_keys};

    mysql_harness::ConfigSection &section = cfg_.add("routing", "test_route");
    section.add("destinations", "127.0.0.1:3306");
    section.add("mode", "read-only");
    section.add("bind_port", "6446");
  }

 protected:
  mysql_harness::Config cfg_;
};

TEST_P(RoutingConfigFailTest, default_option) {
  for (auto const &kv : GetParam().entries) {
    cfg_.set_default(kv.first, kv.second);
  }

  mysql_harness::ConfigSection &section = cfg_.get("routing", "test_route");
  try {
    RoutingPluginConfig config(&section);

    FAIL() << "expected to fail";
  } catch (const std::exception &e) {
    ASSERT_NO_FATAL_FAILURE(GetParam().checker(e));
  }
}

TEST_P(RoutingConfigFailTest, section_option) {
  mysql_harness::ConfigSection &section = cfg_.get("routing", "test_route");
  for (auto const &kv : GetParam().entries) {
    section.add(kv.first, kv.second);
  }

  try {
    RoutingPluginConfig config(&section);

    FAIL() << "expected to fail";
  } catch (const std::exception &e) {
    ASSERT_NO_FATAL_FAILURE(GetParam().checker(e));
  }
}

RoutingConfigFailParam routing_config_fail_params[] = {
    // server-ssl-mode
    //
    {"server_ssl_mode_unknown",
     {
         {"server_ssl_mode", "unknown"},
     },
     [](const auto &e) {
       ASSERT_STREQ(e.what(),
                    "invalid value 'unknown' for server_ssl_mode. Allowed are: "
                    "DISABLED,PREFERRED,REQUIRED,AS_CLIENT.");
     }},
    // client-ssl-mode
    //
    {"client_ssl_mode_unknown",
     {
         {"client_ssl_mode", "unknown"},
     },
     [](const auto &e) {
       ASSERT_STREQ(e.what(),
                    "invalid value 'unknown' for client_ssl_mode. Allowed are: "
                    "DISABLED,PREFERRED,REQUIRED,PASSTHROUGH.");
     }},
    {"client_ssl_mode_preferred_missing_cert",
     {
         {"client_ssl_mode", "preferred"},
     },
     [](const auto &e) {
       ASSERT_STREQ(
           e.what(),
           "client_ssl_cert must be set, if client_ssl_mode is 'PREFERRED'.");
     }},
    {"client_ssl_mode_required_missing_cert",
     {
         {"client_ssl_mode", "required"},
     },
     [](const auto &e) {
       ASSERT_STREQ(
           e.what(),
           "client_ssl_cert must be set, if client_ssl_mode is 'REQUIRED'.");
     }},
    {"client_ssl_mode_preferred_missing_key",
     {
         {"client_ssl_mode", "preferred"},
         {"client_ssl_cert", "some-cert.pem"},
     },
     [](const auto &e) {
       ASSERT_STREQ(
           e.what(),
           "client_ssl_key must be set, if client_ssl_mode is 'PREFERRED'.");
     }},
    {"client_ssl_mode_required_missing_key",
     {
         {"client_ssl_mode", "required"},
         {"client_ssl_cert", "some-cert.pem"},
     },
     [](const auto &e) {
       ASSERT_STREQ(
           e.what(),
           "client_ssl_key must be set, if client_ssl_mode is 'REQUIRED'.");
     }},

    // server-ssl-verify
    {"server_ssl_verify_unknown",
     {
         {"server_ssl_verify", "unknown"},
     },
     [](const auto &e) {
       ASSERT_STREQ(
           e.what(),
           "invalid value 'unknown' for server_ssl_verify. Allowed are: "
           "DISABLED,VERIFY_CA,VERIFY_IDENTITY.");
     }},
    {"server_ssl_verify_verify_ca_missing_ca",
     {
         {"server_ssl_verify", "verify_ca"},
     },
     [](const auto &e) {
       ASSERT_STREQ(e.what(),
                    "server_ssl_ca or server_ssl_capath must be set, if "
                    "server_ssl_verify is 'VERIFY_CA'.");
     }},
    {"server_ssl_verify_verify_identity_missing_ca",
     {
         {"server_ssl_verify", "verify_identity"},
     },
     [](const auto &e) {
       ASSERT_STREQ(e.what(),
                    "server_ssl_ca or server_ssl_capath must be set, if "
                    "server_ssl_verify is 'VERIFY_IDENTITY'.");
     }},
};

INSTANTIATE_TEST_SUITE_P(Fail, RoutingConfigFailTest,
                         ::testing::ValuesIn(routing_config_fail_params),
                         [](auto const &info) { return info.param.test_name; });

int main(int argc, char *argv[]) {
  init_test_logger();
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
