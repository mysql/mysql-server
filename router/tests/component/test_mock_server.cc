/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "exit_status.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlxclient.h"
#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xrow.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_config.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

const char *xcl_column_type_to_string(xcl::Column_type type) {
  switch (type) {
    case xcl::Column_type::BIT:
      return "BIT";
    case xcl::Column_type::BYTES:
      return "BYTES";
    case xcl::Column_type::DATETIME:
      return "DATETIME";
    case xcl::Column_type::DECIMAL:
      return "DECIMAL";
    case xcl::Column_type::SET:
      return "SET";
    case xcl::Column_type::ENUM:
      return "ENUM";
    case xcl::Column_type::TIME:
      return "TIME";
    case xcl::Column_type::SINT:
      return "SINT";
    case xcl::Column_type::UINT:
      return "UINT";
    case xcl::Column_type::DOUBLE:
      return "DOUBLE";
    case xcl::Column_type::FLOAT:
      return "FLOAT";
  }

  return "unknown";
}

namespace xcl {
std::ostream &operator<<(std::ostream &os, xcl::Column_type type) {
  os << xcl_column_type_to_string(type);
  return os;
}
}  // namespace xcl

struct MockServerCLITestParam {
  const char *test_name;

  std::vector<std::string> cmdline_args;

  int expected_exit_code;

  std::function<void(const std::string &output)> checker;
};

class MockServerCLITest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MockServerCLITestParam> {};

TEST_P(MockServerCLITest, check) {
  SCOPED_TRACE("// start binary");
  auto &cmd = launch_mysql_server_mock(GetParam().cmdline_args, 0 /* = port */,
                                       GetParam().expected_exit_code, -1s);

  SCOPED_TRACE("// wait for exit");
  check_exit_code(cmd, GetParam().expected_exit_code, 5s);
  SCOPED_TRACE("// checking stdout");

  GetParam().checker(cmd.get_full_output());
}

const MockServerCLITestParam mock_server_cli_test_param[] = {
    {"version",
     // ensure mock-server supports --version.
     // WL#12118::TS_1-3
     {"--version"},
     EXIT_SUCCESS,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr(MYSQL_ROUTER_VERSION));
     }},
    {"help",
     // ensure mock-server supports --help.
     {"--help"},
     EXIT_SUCCESS,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("--version"));
     }},
    {"invalid_port",
     // ensure mock-server fails with --http-port=65536.
     // WL#12118::TS_1-4
     {"--http-port", "65536"},
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("was '65536'"));
     }},
    {"hex_http_port",
     // ensure mock-server fails with --http-port=0xffff.
     {"--http-port", "0xffff",  //
      "--filename", "@filename@"},
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("was '0xffff'"));
     }},
    {"hex_xport",
     // ensure mock-server fails with --xport=0xffff.
     {"--port", "0xffff",  //
      "--filename", "@filename@"},
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("was '0xffff'"));
     }},
    {"hex_port",
     // ensure mock-server fails with --port=0xffff.
     {"--xport", "0xffff",  //
      "--filename", "@filename@"},
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("was '0xffff'"));
     }},
    {"invalid_ssl_mode",
     {"--filename", "@filename@",  //
      "--ssl-mode", "verify_ca"},
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("invalid value 'verify_ca'"));
     }},
    {"ssl_mode_required_no_cert_no_key",
     {
         "--filename", "@filename@",  //
         "--ssl-mode", "required",    //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_cert is empty"));
     }},
    {"ssl_mode_required_no_key",
     {
         "--filename", "@filename@",                 //
         "--ssl-mode", "required",                   //
         "--ssl-cert", "@datadir@/server-cert.pem",  //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_key is empty"));
     }},
    {"ssl_mode_required_no_cert",
     {
         "--filename", "@filename@",               //
         "--ssl-mode", "required",                 //
         "--ssl-key", "@datadir@/server-key.pem",  //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_cert is empty"));
     }},
    {"ssl_mode_preferred_no_cert_no_key",
     {
         "--filename", "@filename@",  //
         "--ssl-mode", "preferred",   //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_cert is empty"));
     }},
    {"ssl_mode_preferred_no_key",
     {
         "--filename", "@filename@",                 //
         "--ssl-mode", "preferred",                  //
         "--ssl-cert", "@datadir@/server-cert.pem",  //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_key is empty"));
     }},
    {"ssl_mode_preferred_no_cert",
     {
         "--filename", "@filename@",               //
         "--ssl-mode", "preferred",                //
         "--ssl-key", "@datadir@/server-key.pem",  //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output, ::testing::HasSubstr("ssl_cert is empty"));
     }},
    {"tls_version_unknown",
     {
         "--filename", "@filename@",                 // required
         "--ssl-mode", "preferred",                  // required
         "--ssl-key", "@datadir@/server-key.pem",    // required
         "--ssl-cert", "@datadir@/server-cert.pem",  // required
         "--tls-version", "sslv3",                   //
     },
     EXIT_FAILURE,
     [](const std::string &output) {
       EXPECT_THAT(output,
                   ::testing::HasSubstr("setting 'tls_version=sslv3' failed"));
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, MockServerCLITest,
                         ::testing::ValuesIn(mock_server_cli_test_param),
                         [](const auto &info) { return info.param.test_name; });

class MockServerCLITestBase : public RouterComponentTest {};

TEST_F(MockServerCLITestBase, classic_many_connections) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();
  auto bind_port = port_pool_.get_next_available();
  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  std::map<std::string, std::string> config{
      {"--module-prefix", get_data_dir().str()},
      {"--filename", get_data_dir().join("my_port.js").str()},
      {"--bind-address", "127.0.0.1"},
      {"--port", std::to_string(bind_port)},
  };

  std::vector<std::string> cmdline_args;

  for (const auto &arg : config) {
    cmdline_args.push_back(arg.first);
    cmdline_args.push_back(arg.second);
  }

  SCOPED_TRACE("// start " + mysql_server_mock_path);
  spawner(mysql_server_mock_path).with_core_dump(true).spawn(cmdline_args);

  // Opening a new connection takes ~12ms on a dev-machine.
  //
  // Spawning 80 connections sequentially takes ~1sec.
  //
  // ideas to improve this time:
  //
  // - use libmysqlclient's async connect code.
  // - make the mock handle connections faster.
  constexpr int kNumConnections{80};
  std::vector<mysqlrouter::MySQLSession> classic_sessions{kNumConnections};

  for (auto &sess : classic_sessions) {
    try {
      sess.connect("127.0.0.1", bind_port, "username", "password", "", "");
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }
  }

  for (auto &sess : classic_sessions) {
    try {
      auto row = sess.query_one("select @@port");
      ASSERT_EQ(row->size(), 1);
      EXPECT_EQ((*row)[0], std::to_string(bind_port));
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }
  }
}

struct MockServerConnectOkTestParam {
  const char *test_name;

  std::vector<std::string> cmdline_args;
};

class MockServerConnectOkTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MockServerConnectOkTestParam> {};

/**
 * replace named placeholders in a string.
 *
 * format: voo@key@bar
 */
static std::string replace_placeholders(
    const std::string &arg, const std::map<std::string, std::string> &config) {
  std::string value;

  auto cur = arg.begin();
  auto end = arg.end();

  do {
    // find pairs of @
    auto first_at = std::find(cur, end, '@');
    if (first_at == end) {
      break;
    }

    auto second_at = std::find(first_at + 1, end, '@');
    if (second_at == end) {
      throw std::runtime_error("expected 2nd @.");
    }

    value += std::string(cur, first_at);

    auto lookup = std::string(first_at + 1, second_at);

    value += config.at(lookup);
    cur = second_at + 1;
  } while (true);

  value += std::string(cur, end);

  return value;
}

static void classic_protocol_connect_ok(
    const std::string &host, uint16_t port,
    const std::string &username = "username",
    const std::string &password = "password") {
  mysqlrouter::MySQLSession sess;

  sess.connect(host, port,
               username,  // user
               password,  // pass
               "",        // socket
               ""         // schema
  );
}

static void classic_protocol_connect_fail(const std::string &host,
                                          uint16_t port,
                                          const std::string &username,
                                          const std::string &password,
                                          int expected_error_code) {
  mysqlrouter::MySQLSession sess;
  try {
    sess.connect(host, port,
                 username,  // user
                 password,  // pass
                 "",        // socket
                 ""         // schema
    );
    FAIL() << "expected to fail";
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    ASSERT_EQ(e.code(), expected_error_code);
  }
}

static void x_protocol_connect_ok(const std::string &host, uint16_t port,
                                  const std::string &username = "username",
                                  const std::string &password = "password") {
  auto sess = xcl::create_session();
  ASSERT_THAT(
      sess->set_mysql_option(
          xcl::XSession::Mysqlx_option::Ssl_mode,
          mysqlrouter::MySQLSession::ssl_mode_to_string(SSL_MODE_PREFERRED)),
      ::testing::Truly([](const xcl::XError &xerr) { return !xerr; }));
  ASSERT_THAT(
      sess->connect(host.c_str(), port, username.c_str(), password.c_str(), ""),
      ::testing::Truly([](auto const &err) { return err.error() == 0; }));
}

TEST_P(MockServerConnectOkTest, classic_protocol) {
  auto bind_port = port_pool_.get_next_available();

  std::map<std::string, std::string> config{
      {"http_port", std::to_string(port_pool_.get_next_available())},
      {"datadir", get_data_dir().str()},
      {"certdir", SSL_TEST_DATA_DIR},
      {"hostname", "127.0.0.1"},
  };

  std::vector<std::string> cmdline_args;

  for (const auto &arg : GetParam().cmdline_args) {
    if (arg.empty()) {
      cmdline_args.push_back(arg);
    } else {
      cmdline_args.push_back(replace_placeholders(arg, config));
    }
  }

  cmdline_args.emplace_back("--bind-address=127.0.0.1");
  cmdline_args.emplace_back("--port");
  cmdline_args.push_back(std::to_string(bind_port));

  SCOPED_TRACE("// start binary");
  launch_mysql_server_mock(cmdline_args, bind_port);

  SCOPED_TRACE("// checking "s + GetParam().test_name);
  classic_protocol_connect_ok(config.at("hostname"), bind_port);
}

TEST_P(MockServerConnectOkTest, x_protocol) {
  auto bind_port = port_pool_.get_next_available();
  auto other_bind_port = port_pool_.get_next_available();

  std::map<std::string, std::string> config{
      {"http_port", std::to_string(port_pool_.get_next_available())},
      {"datadir", get_data_dir().str()},
      {"certdir", SSL_TEST_DATA_DIR},
      {"hostname", "127.0.0.1"},
      {"bind_address", "127.0.0.1"},
  };

  std::vector<std::string> cmdline_args{"--logging-folder",
                                        get_test_temp_dir_name()};

  for (const auto &arg : GetParam().cmdline_args) {
    if (arg.empty()) {
      cmdline_args.push_back(arg);
    } else {
      cmdline_args.push_back(replace_placeholders(arg, config));
    }
  }

  cmdline_args.emplace_back("--bind-address=127.0.0.1");
  // set the classic port even though we don't use it.
  // otherwise it defaults to bind to port 3306 which may lead to "Address
  // already in use"
  cmdline_args.emplace_back("--port");
  cmdline_args.push_back(std::to_string(other_bind_port));

  cmdline_args.emplace_back("--xport");
  cmdline_args.push_back(std::to_string(bind_port));

  SCOPED_TRACE("// start binary");
  launch_mysql_server_mock(cmdline_args, other_bind_port);

  SCOPED_TRACE("// checking "s + GetParam().test_name);
  x_protocol_connect_ok(config.at("hostname"), bind_port);
}

const MockServerConnectOkTestParam mock_server_connect_ok_test_param[] = {
    {"ssl_mode_default",
     {
         "--filename", "@datadir@/my_port.js",  //
         "--module-prefix", "@datadir@",        //
     }},
    {"ssl_mode_disabled",
     {
         "--filename", "@datadir@/my_port.js",  //
         "--module-prefix", "@datadir@",        //
         "--ssl-mode", "disabled",              //
     }},
    {"ssl_mode_disabled_ignored_ssl_key",
     {
         "--filename", "@datadir@/my_port.js",     //
         "--module-prefix", "@datadir@",           //
         "--ssl-mode", "disabled",                 //
         "--ssl-key", "@certdir@/does-not-exist",  //
     }},
    {"ssl_mode_disabled_ignored_ssl_cert",
     {
         "--filename", "@datadir@/my_port.js",      //
         "--module-prefix", "@datadir@",            //
         "--ssl-mode", "disabled",                  //
         "--ssl-cert", "@certdir@/does-not-exist",  //
     }},
    {"ssl_mode_required",
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "required",                   //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
     }},
    {"ssl_mode_preferred",
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
     }},
    {"ssl_cipher_aes128_sha256",
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
         "--ssl-cipher", "AES128-SHA256",            // a known cipher
     }},
    {"ssl_cipher_empty",
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
         "--ssl-cipher", "",                         //
     }},
    {"ssl_ca",
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
         "--ssl-ca", "@certdir@/cacert.pem",         //
     }},
    {"ssl_crl_no_client_cert",
     // if no cert is presented, and it isn't required by the test-file, check
     // the CRL file is not having an impact.
     {
         "--filename", "@datadir@/my_port.js",             //
         "--module-prefix", "@datadir@",                   //
         "--ssl-mode", "preferred",                        //
         "--ssl-key", "@certdir@/server-key.pem",          //
         "--ssl-cert", "@certdir@/server-cert.pem",        //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",          //
         "--ssl-crl", "@certdir@/crl-client-revoked.crl",  //
     }},
    {"auth_with_username",
     {
         "--filename", "@datadir@/mock_server_require_username.js",  //
         "--module-prefix", "@datadir@",                             //
         "--ssl-mode", "preferred",                                  //
         "--ssl-key", "@certdir@/server-key.pem",                    //
         "--ssl-cert", "@certdir@/server-cert.pem",                  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
     }},
    {"auth_with_password",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--ssl-mode", "preferred",                                  //
         "--ssl-key", "@certdir@/server-key.pem",                    //
         "--ssl-cert", "@certdir@/server-cert.pem",                  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
     }},
    {"client_cert_no_cert_required",
     // testfile doesn't require a cert.
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",    //
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, MockServerConnectOkTest,
                         ::testing::ValuesIn(mock_server_connect_ok_test_param),
                         [](const auto &info) { return info.param.test_name; });

// custom connect tests

struct MockServerConnectTestParam {
  const char *test_name;

  std::vector<std::string> cmdline_args;

  std::function<void(const std::map<std::string, std::string> &args)> checker;
};

class MockServerConnectTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MockServerConnectTestParam> {};

TEST_P(MockServerConnectTest, check) {
  auto classic_port = port_pool_.get_next_available();
  std::map<std::string, std::string> config{
      {"http_port", std::to_string(port_pool_.get_next_available())},
      {"port", std::to_string(classic_port)},
      {"xport", std::to_string(port_pool_.get_next_available())},
      {"datadir", get_data_dir().str()},
      {"certdir", SSL_TEST_DATA_DIR},
      {"hostname", "127.0.0.1"},
  };

  std::vector<std::string> cmdline_args{"--logging-folder",
                                        get_test_temp_dir_name()};

  for (const auto &arg : GetParam().cmdline_args) {
    if (arg.empty()) {
      cmdline_args.push_back(arg);
    } else {
      cmdline_args.push_back(replace_placeholders(arg, config));
    }
  }

  SCOPED_TRACE("// start binary");
  launch_mysql_server_mock(cmdline_args, classic_port);

  SCOPED_TRACE("// checking "s + GetParam().test_name);
  GetParam().checker(config);
}

const MockServerConnectTestParam mock_server_connect_test_param[] = {
    {"mysql_native_password_with_password",
     // certificate is required, but no cert is presented
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--port", "@port@",                                         //
         "--bind-address", "127.0.0.1",                              //
         "--verbose",                                                //
     },
     [](const std::map<std::string, std::string> &config) {
       auto host = config.at("hostname");
       auto port = atol(config.at("port").c_str());
       const char username[] = "username";
       const char password[] = "password";

       mysqlrouter::MySQLSession sess;

       const auto opt_res =
           sess.set_option(mysqlrouter::MySQLSession::DefaultAuthentication(
               "mysql_native_password"));
       // set_option() only fails for bad-options, but not bad values.
       //
       // if auth-method-name is invalid, the connect will fail.
       ASSERT_TRUE(opt_res) << opt_res.error().message();

       try {
         sess.connect(host, port,
                      username,  // user
                      password,  // pass
                      "",        // socket
                      ""         // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_cert_verify_cert_no_cert",
     // certificate is required, but no cert is presented
     {
         "--filename", "@datadir@/mock_server_cert_verify_cert.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--ssl-mode", "required",                                   //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
         "--ssl-key", "@certdir@/crl-server-key.pem",                //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",              //
         "--verbose",                                                //
     },
     [](const std::map<std::string, std::string> &config) {
       classic_protocol_connect_fail(config.at("hostname"),
                                     atol(config.at("port").c_str()),
                                     "username", "password", 1045);
     }},
    {"auth_wrong_password",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--ssl-mode", "preferred",                                  //
         "--ssl-key", "@certdir@/server-key.pem",                    //
         "--ssl-cert", "@certdir@/server-cert.pem",                  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
     },
     [](const std::map<std::string, std::string> &config) {
       classic_protocol_connect_fail(config.at("hostname"),
                                     atol(config.at("port").c_str()),
                                     "username", "wrongpass", 1045);
     }},
    {"auth_wrong_username",
     {
         "--filename", "@datadir@/mock_server_require_username.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--ssl-mode", "preferred",                                  //
         "--ssl-key", "@certdir@/server-key.pem",                    //
         "--ssl-cert", "@certdir@/server-cert.pem",                  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
     },
     [](const std::map<std::string, std::string> &config) {
       classic_protocol_connect_fail(config.at("hostname"),
                                     atol(config.at("port").c_str()),
                                     "wronguser", "wrongpass", 1045);
     }},
    {"client_cert_no_cert_required_with_trusted_cert",
     // testfile doesn't require a cert. But we send one anyway.
     {
         "--filename", "@datadir@/my_port.js",       //
         "--module-prefix", "@datadir@",             //
         "--bind-address", "127.0.0.1",              //
         "--port", "@port@",                         //
         "--ssl-mode", "preferred",                  //
         "--ssl-key", "@certdir@/server-key.pem",    //
         "--ssl-cert", "@certdir@/server-cert.pem",  //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",    //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                    "username",  // user
                    "password",  // pass
                    "",          // socket
                    ""           // schema
       );
     }},
    {"client_cert_require_certifiticate",
     {
         "--filename", "@datadir@/mock_server_cert_verify_cert.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--ssl-mode", "required",                                   //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                    //
         "--ssl-key", "@certdir@/crl-server-key.pem",                //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",              //
         "--verbose",                                                //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_cert_verify_issuer",
     // testfile requires that issuer is the one of the crl-ca-cert.pem
     {
         "--filename", "@datadir@/mock_server_cert_verify_issuer.js",  //
         "--module-prefix", "@datadir@",                               //
         "--bind-address", "127.0.0.1",                                //
         "--port", "@port@",                                           //
         "--ssl-mode", "required",                                     //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                      //
         "--ssl-key", "@certdir@/crl-server-key.pem",                  //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                //
         "--verbose",                                                  //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_cert_verify_subject",
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_cert_verify_subject_wrong_subject",
     // present a client cert with the wrong subject
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
         "--verbose",                                                   //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       // present the server cert instead of the client cert.
       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-server-cert.pem", config),
           replace_placeholders("@certdir@/crl-server-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected to fail";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         // access denied
         ASSERT_EQ(e.code(), 1045);
       }
     }},
    {"client_cert_revoked",
     // present a client cert with the wrong subject
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
         "--ssl-crl", "@certdir@/crl-client-revoked.crl",               //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       // present the server cert instead of the client cert.
       sess.set_ssl_cert(replace_placeholders(
                             "@certdir@/crl-client-revoked-cert.pem", config),
                         replace_placeholders(
                             "@certdir@/crl-client-revoked-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected to fail";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         // connection aborted
         //
         // openssl 1.1.1: 2013
         // openssl 1.0.2: 2026
         ASSERT_THAT(e.code(), ::testing::AnyOf(2013, 2026));
       }
     }},
    {"client_cert_revoked_other_cert",
     // present a client cert with the wrong subject
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
         "--ssl-crl", "@certdir@/crl-client-revoked.crl",               //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       // present the server cert instead of the client cert.
       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"tls_version_12",
     // allow only TLSv1.2 on the server and force client to TLSv1.2 too
     //
     // should succeed.
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
         "--tls-version", "TLSv1.2",                                    //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       sess.set_ssl_options(SSL_MODE_REQUIRED, "TLSv1.2",
                            "",  //
                            "",  //
                            "",  //
                            "",  //
                            "");

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},

    {"tls_version_12_wrong_version",
     // allow only TLSv1.2 on the server and force client to TLSv1.1 too
     //
     // should fail
     {
         "--filename", "@datadir@/mock_server_cert_verify_subject.js",  //
         "--module-prefix", "@datadir@",                                //
         "--bind-address", "127.0.0.1",                                 //
         "--port", "@port@",                                            //
         "--ssl-mode", "required",                                      //
         "--ssl-ca", "@certdir@/crl-ca-cert.pem",                       //
         "--ssl-key", "@certdir@/crl-server-key.pem",                   //
         "--ssl-cert", "@certdir@/crl-server-cert.pem",                 //
         "--tls-version", "TLSv1.2",                                    //
     },
     [](const std::map<std::string, std::string> &config) {
       // connect should work.
       mysqlrouter::MySQLSession sess;

       sess.set_ssl_cert(
           replace_placeholders("@certdir@/crl-client-cert.pem", config),
           replace_placeholders("@certdir@/crl-client-key.pem", config));

       // TLSv1.1 may be forbidden in libmysqlclient.
       try {
         sess.set_ssl_options(SSL_MODE_REQUIRED, "TLSv1.1",
                              "",  //
                              "",  //
                              "",  //
                              "",  //
                              "");
       } catch (const std::exception &e) {
         // Error setting TLS_VERSION option for MySQL connection
         GTEST_SKIP() << e.what();
       }

       try {
         sess.connect(config.at("hostname"), atol(config.at("port").c_str()),
                      "username",  // user
                      "password",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected to failed";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         ASSERT_EQ(e.code(), 2026);
       }
     }},
    {"xproto_mysqlsh_select_connection_id",
     {
         "--filename",      "@datadir@/tls_endpoint.js",      //
         "--module-prefix", "@datadir@",                      //
         "--bind-address",  "127.0.0.1",                      //
         "--port",          "@port@",                         //
         "--xport",         "@xport@",                        //
         "--ssl-mode",      "required",                       //
         "--ssl-ca",        "@certdir@/crl-ca-cert.pem",      //
         "--ssl-key",       "@certdir@/crl-server-key.pem",   //
         "--ssl-cert",      "@certdir@/crl-server-cert.pem",  //
         "--tls-version",   "TLSv1.2",                        //
     },
     [](const std::map<std::string, std::string> &config) {
       auto sess = xcl::create_session();
       ASSERT_THAT(
           sess->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                  mysqlrouter::MySQLSession::ssl_mode_to_string(
                                      SSL_MODE_PREFERRED)),
           ::testing::Truly([](const xcl::XError &xerr) { return !xerr; }));
       ASSERT_THAT(
           sess->connect(config.at("hostname").c_str(),
                         atol(config.at("xport").c_str()), "username",
                         "password", ""),
           ::testing::Truly([](auto const &err) { return err.error() == 0; }));
       xcl::XError xerr;
       auto query_result = sess->execute_sql(
           "select "
           "@@lower_case_table_names, @@version, connection_id(), "
           "variable_value "
           "from performance_schema.session_status "
           "where variable_name = 'mysqlx_ssl_cipher'",
           &xerr);

       ASSERT_TRUE(query_result->has_resultset());

       const auto &meta = query_result->get_metadata();
       ASSERT_THAT(meta, ::testing::SizeIs(::testing::Eq(4)));
       EXPECT_EQ(meta[0].type,
                 xcl::Column_type::SINT);  // lower_case_table_names
       EXPECT_EQ(meta[1].type, xcl::Column_type::BYTES);  // version
       EXPECT_EQ(meta[2].type, xcl::Column_type::SINT);   // connection_id
       EXPECT_EQ(meta[3].type, xcl::Column_type::BYTES);  // cipher

       auto row = query_result->get_next_row();
       ASSERT_TRUE(row);
       ASSERT_EQ(row->get_number_of_fields(), 4);

       // check if there something like a version-string in field[1]
       std::string version_string;
       ASSERT_TRUE(row->get_string(1, &version_string));
       ASSERT_THAT(version_string,
                   ::testing::SizeIs(::testing::Gt(5)));  // x.y.z
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, MockServerConnectTest,
                         ::testing::ValuesIn(mock_server_connect_test_param),
                         [](const auto &info) { return info.param.test_name; });

// --core-file

struct MockServerCoreTestParam {
  const char *test_name;

  std::vector<std::string> cmdline_args;

  ExitStatus expected_exit_status_;
};

class MockServerCoreTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MockServerCoreTestParam> {};

TEST_P(MockServerCoreTest, check) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  std::map<std::string, std::string> config{
      {"http_port", std::to_string(port_pool_.get_next_available())},
      {"bind_address", "127.0.0.1"},
      {"port", std::to_string(port_pool_.get_next_available())},
      {"xport", std::to_string(port_pool_.get_next_available())},
      {"datadir", get_data_dir().str()},
      {"certdir", SSL_TEST_DATA_DIR},
      {"hostname", "127.0.0.1"},
  };

  std::vector<std::string> cmdline_args{"--logging-folder",
                                        get_test_temp_dir_name()};

  for (const auto &arg : GetParam().cmdline_args) {
    if (arg.empty()) {
      cmdline_args.push_back(arg);
    } else {
      cmdline_args.push_back(replace_placeholders(arg, config));
    }
  }

  bool wait_for_ready =
      GetParam().expected_exit_status_ == ExitStatus{EXIT_SUCCESS};

  SCOPED_TRACE("// start binary");
  auto &proc = spawner(mysql_server_mock_path)
                   .expected_exit_code(GetParam().expected_exit_status_)
                   .wait_for_notify_ready(wait_for_ready ? 1s : -1s)
                   .spawn(cmdline_args);

  if (!wait_for_ready) {
    EXPECT_NO_THROW(proc.native_wait_for_exit());

    EXPECT_THAT(proc.get_full_output(),
                ::testing::HasSubstr("Value for parameter '--core-file' "
                                     "needs to be one of: ['0', '1']"));
  }
}

const MockServerCoreTestParam mock_server_core_test_param[] = {
    {"core_file_no_arg",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--core-file",                                              //
     },
     ExitStatus{EXIT_SUCCESS}},
    {"core_file_0",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--core-file", "0",                                         //
     },
     ExitStatus{EXIT_SUCCESS}},
    {"core_file_1",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--core-file", "1",                                         //
     },
     ExitStatus{EXIT_SUCCESS}},
    {"core_file_invalid",
     {
         "--filename", "@datadir@/mock_server_require_password.js",  //
         "--module-prefix", "@datadir@",                             //
         "--bind-address", "127.0.0.1",                              //
         "--port", "@port@",                                         //
         "--core-file", "abc",                                       //
     },
     ExitStatus{EXIT_FAILURE}},
};

INSTANTIATE_TEST_SUITE_P(Spec, MockServerCoreTest,
                         ::testing::ValuesIn(mock_server_core_test_param),
                         [](const auto &info) { return info.param.test_name; });

// session-tracker

struct MockServerCommandTestParam {
  const char *test_name;

  std::function<void(MysqlClient &cli)> checker;
};

class MockServerCommandTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MockServerCommandTestParam> {};

TEST_P(MockServerCommandTest, check) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  auto port = port_pool_.get_next_available();
  auto xport = port_pool_.get_next_available();

  SCOPED_TRACE("// start mock-server");
  spawner(mysql_server_mock_path)
      .with_core_dump(true)
      .spawn({
          "--logging-folder",
          get_test_temp_dir_name(),
          "--module-prefix",
          get_data_dir().str(),
          "--bind-address=127.0.0.1",
          "--port",
          std::to_string(port),
          "--xport",
          std::to_string(xport),
          "--filename",
          get_data_dir().join("session_tracker.js").str(),
      });

  MysqlClient cli;
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", port));

  GetParam().checker(cli);
}

const MockServerCommandTestParam mock_server_command_test_param[] = {
    {
        "use_schema",
        [](auto &cli) {
          ASSERT_NO_ERROR(cli.query("USE some_schema"));
          EXPECT_THAT(cli.session_trackers(),
                      ::testing::ElementsAre(std::make_tuple(
                          SESSION_TRACK_SCHEMA, "some_schema")));
        },
    },
    {
        "set_sysvars",
        [](auto &cli) {
          ASSERT_NO_ERROR(cli.query("SET sysvar1=1, sysvar2=2"));
          EXPECT_THAT(
              cli.session_trackers(),
              // key, value, key, value, ...
              ::testing::ElementsAre(
                  std::make_tuple(SESSION_TRACK_SYSTEM_VARIABLES, "sysvar_a"),
                  std::make_tuple(SESSION_TRACK_SYSTEM_VARIABLES, "1"),
                  std::make_tuple(SESSION_TRACK_SYSTEM_VARIABLES, "sysvar_b"),
                  std::make_tuple(SESSION_TRACK_SYSTEM_VARIABLES, "2")));
        },
    },
    {
        "gtid",
        [](auto &cli) {
          ASSERT_NO_ERROR(cli.query("INSERT ..."));
          EXPECT_THAT(cli.session_trackers(),
                      ::testing::ElementsAre(std::make_tuple(
                          SESSION_TRACK_GTIDS,
                          "3E11FA47-71CA-11E1-9E33-C80AA9429562:1")));
        },
    },
    {
        "uservar",
        [](auto &cli) {
          ASSERT_NO_ERROR(cli.query("INSERT @user_var"));
          EXPECT_THAT(
              cli.session_trackers(),
              ::testing::ElementsAre(
                  std::make_tuple(SESSION_TRACK_STATE_CHANGE, "1"),
                  std::make_tuple(SESSION_TRACK_GTIDS,
                                  "3E11FA47-71CA-11E1-9E33-C80AA9429562:2"),
                  std::make_tuple(SESSION_TRACK_TRANSACTION_CHARACTERISTICS,
                                  ""),
                  std::make_tuple(SESSION_TRACK_TRANSACTION_STATE,
                                  "________")));
        },
    },
};

INSTANTIATE_TEST_SUITE_P(Spec, MockServerCommandTest,
                         ::testing::ValuesIn(mock_server_command_test_param),
                         [](const auto &info) { return info.param.test_name; });

int main(int argc, char *argv[]) {
  net::impl::socket::init();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
