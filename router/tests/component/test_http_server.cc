/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef _WIN32
// ensure windows.h doesn't expose min() nor max()
#define NOMINMAX
#endif

#include <gmock/gmock.h>

#include "config_builder.h"
#include "dim.h"
#include "gtest_testname.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "temp_dir.h"

Path g_origin_path;

// base-class to init RouterComponentTest before we launch_* anything
class HttpServerTestBase : public RouterComponentTest {
 public:
  HttpServerTestBase() {
    set_origin(g_origin_path);

    RouterComponentTest::init();
  }
};

/**
 * parameters of static-files tests.
 */
struct HttpServerStaticFilesParams {
  std::string test_name;
  std::string url;
  unsigned int status_code;
};

/**
 * testsuite of static-file tests.
 *
 * It
 *
 * - reserves port for HTTP server in port-pool
 * - prepares configuration files
 * - starts server
 *
 * After all tests in the suite are done, stops server and cleans up
 */
class HttpServerStaticFilesTest
    : public HttpServerTestBase,
      public ::testing::Test,
      public ::testing::WithParamInterface<HttpServerStaticFilesParams> {
 public:
  HttpServerStaticFilesTest()
      : port_pool_{},
        http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        conf_file_{create_config_file(
            conf_dir_.name(),
            ConfigBuilder::build_section(
                "http_server", {{"port", std::to_string(http_port_)},
                                {"static_folder", get_data_dir().str()}}))},
        http_server_{launch_router({"-c", conf_file_})} {}

 protected:
  TcpPortPool port_pool_;
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  std::string conf_file_;
  CommandHandle http_server_;
};

/**
 * ensure GET requests for static files work.
 *
 * - start the http-server component
 * - make a client connect to the http-server
 */
TEST_P(HttpServerStaticFilesTest, ensure) {
  SCOPED_TRACE("// wait http port connectable");
  ASSERT_TRUE(wait_for_port_ready(http_port_, 1000))
      << http_server_.get_full_output();

  std::string http_uri = GetParam().url;
  SCOPED_TRACE("// connecting " + http_hostname_ + ":" +
               std::to_string(http_port_) + " for " + http_uri);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_);
  auto req = rest_client.request_sync(HttpMethod::Get, http_uri);
  ASSERT_TRUE(req);
  ASSERT_EQ(req.get_response_code(), GetParam().status_code);
}

INSTANTIATE_TEST_CASE_P(
    http_static, HttpServerStaticFilesTest,
    ::testing::Values(
        HttpServerStaticFilesParams{"dir, no index-file", "/", 403},
        HttpServerStaticFilesParams{"file exists", "/my_port.js", 200},
        HttpServerStaticFilesParams{"not leave root", "/../my_port.js", 200}
        // assumes my_root.js is only in datadir
        ),
    [](const ::testing::TestParamInfo<HttpServerStaticFilesParams> &info) {
      return gtest_sanitize_param_name(info.param.test_name + " " +
                                       std::to_string(info.param.status_code));
    });

// HTTPS tests
//
const char kServerCertFile[]{"server-cert.pem"};
const char kServerKeyFile[]{"server-key.pem"};
const char kServerCertCaFile[]{"cacert.pem"};
const char kWrongServerCertCaFile[]{"ca-sha512.pem"};

/**
 * params of HTTPS server tests.
 */
struct HttpsServerParams {
  std::string test_name;
  std::string ca_cert_file;
  bool should_succeeed;
  std::string cipher_list;
  std::string no_idea;

  friend void PrintTo(const HttpsServerParams &params, std::ostream *os) {
    *os << "test-name: '" << params.test_name << "', "
        << "cipher_list: '" << params.cipher_list << "'";
  }
};

/**
 * test-suite of HTTPS server tests.
 *
 * ssl_cert_data_dir_ should point to the location of the
 * ${CMAKE_SOURCE_DIR}/mysql-test/sql_data/ as it contains the
 * certificates we need for testing.
 */
class HttpsServerTest
    : public HttpServerTestBase,
      public ::testing::Test,
      public ::testing::WithParamInterface<HttpsServerParams> {
 public:
  HttpsServerTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        ssl_cert_data_dir_{SSL_TEST_DATA_DIR},
        conf_file_{create_config_file(
            conf_dir_.name(),
            ConfigBuilder::build_section(
                "http_server",
                {
                    {"port", std::to_string(http_port_)},  // port to listen on
                    {"ssl", "1"},                          // enable SSL
                    {"ssl_cert",
                     ssl_cert_data_dir_.join(kServerCertFile).str()},
                    {"ssl_key", ssl_cert_data_dir_.join(kServerKeyFile).str()},
                }))},
        http_server_{launch_router({"-c", conf_file_})} {}

 protected:
  TcpPortPool port_pool_;
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  mysql_harness::Path ssl_cert_data_dir_;
  std::string conf_file_;
  CommandHandle http_server_;
};

/**
 * ensure HTTPS requests work against a well configured server.
 *
 * - start the http-server component with TLS enabled
 * - make a client connect to the http-server
 */
TEST_P(HttpsServerTest, ensure) {
  std::string ca_cert = GetParam().ca_cert_file;
  bool should_succeeed = GetParam().should_succeeed;
  std::string cipher_list = GetParam().cipher_list;

  SCOPED_TRACE("// wait http port connectable");
  ASSERT_TRUE(wait_for_port_ready(http_port_, 1000))
      << get_router_log_output() << http_server_.get_full_output();

  HttpUri u;
  u.set_scheme("https");
  u.set_port(http_port_);
  u.set_host(http_hostname_);
  u.set_path("/");

  SCOPED_TRACE("// preparing client and connection object");
  IOContext io_ctx;
  TlsClientContext tls_ctx;

  // as min-version isn't set, it may either be "AUTO" aka the lowest supported
  // or SSL_3 ... which is the lowest supported (openssl 1.1.0 and before)
  std::set<TlsVersion> allowed{TlsVersion::AUTO, TlsVersion::SSL_3};
  EXPECT_THAT(allowed, ::testing::Contains(tls_ctx.min_version()));

  tls_ctx.ssl_ca(ssl_cert_data_dir_.join(ca_cert).str(), "");
  if (!cipher_list.empty()) {
    if (tls_ctx.has_set_cipher_suites()) tls_ctx.cipher_suites("");
    tls_ctx.cipher_list(cipher_list);
  }

  // help debugging why handshake failed.
  tls_ctx.info_callback([](const SSL *ssl, int where, int ret) {
    std::string state;

    if (where & SSL_CB_LOOP)
      state = "loop";
    else if (where & SSL_CB_EXIT)
      state = "exit";
    else if (where & SSL_CB_READ)
      state = "read";
    else if (where & SSL_CB_WRITE)
      state = "write";
    else if (where & SSL_CB_ALERT)
      state = "alert";
    else if (where & SSL_CB_HANDSHAKE_START)
      state = "handshake";
    else if (where & SSL_CB_HANDSHAKE_DONE)
      state = "handshake-done";

    if (where & SSL_CB_ALERT) {
      std::cerr << __LINE__ << ": (" << SSL_state_string_long(ssl) << ") "
                << state << ": " << SSL_alert_type_string_long(ret)
                << "::" << SSL_alert_desc_string_long(ret) << std::endl;
    } else {
      std::cerr << __LINE__ << ": (" << SSL_state_string_long(ssl) << ") "
                << state << ": " << ret << std::endl;
    }
    if (where & SSL_CB_HANDSHAKE_START) {
      const char *cipher;
      for (int i = 0; (cipher = SSL_get_cipher_list(ssl, i)); i++) {
        std::cerr << __LINE__ << ": available cipher[" << i << "]: " << cipher
                  << std::endl;
      }
    }

    if (where & SSL_CB_HANDSHAKE_DONE) {
      const char *cipher;
      for (int i = 0; (cipher = SSL_get_cipher_list(ssl, i)); i++) {
        std::cerr << __LINE__ << ": available cipher[" << i << "]: " << cipher
                  << std::endl;
      }
    }
  });

  std::unique_ptr<HttpsClient> http_client(
      new HttpsClient(io_ctx, std::move(tls_ctx), u.get_host(), u.get_port()));

  RestClient rest_client(std::move(http_client));

  SCOPED_TRACE("// GETing " + u.join());
  auto req = rest_client.request_sync(HttpMethod::Get, u.get_path());
  if (should_succeeed) {
    ASSERT_TRUE(req) << rest_client.error_msg();
    ASSERT_EQ(req.get_response_code(), 404);
  } else {
    ASSERT_FALSE(req) << req.get_response_code();
  }
}

INSTANTIATE_TEST_CASE_P(
    http_static, HttpsServerTest,
    ::testing::Values(
        //
        HttpsServerParams{"default-client-cipher", kServerCertCaFile, true, "",
                          ""},
        HttpsServerParams{"TLS1.1 cipher", kServerCertCaFile, false,
                          "AES128-SHA", "unknown CA"},
        HttpsServerParams{"wrong ca", kWrongServerCertCaFile, false, "",
                          "handshake failure"}),
    [](const ::testing::TestParamInfo<HttpsServerParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name +
          (info.param.should_succeeed == false ? " fails" : " succeeds"));
    });

//
// http_server, broken-config, SSL
//

struct HttpsServerBrokenConfigParams {
  std::string test_name;
  std::vector<std::pair<std::string, std::string>> http_section;
  std::string errmsg_regex;

  friend void PrintTo(const HttpsServerBrokenConfigParams &params,
                      std::ostream *os) {
    *os << "test-name: '" << params.test_name << "', "
        << "expected-error-msg: '" << params.errmsg_regex << "'";
  }
};

/**
 * config-failures for HTTPS setups.
 *
 */
class HttpsServerBrokenConfigTest
    : public HttpServerTestBase,
      public ::testing::Test,
      public ::testing::WithParamInterface<HttpsServerBrokenConfigParams> {
 public:
  HttpsServerBrokenConfigTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        ssl_cert_data_dir_{SSL_TEST_DATA_DIR} {}

 protected:
  TcpPortPool port_pool_;
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  mysql_harness::Path ssl_cert_data_dir_;
};

// a placeholder that will be replaced with a valid value
//
// for "port", the currently assinged port will be used
// for "ssl_cert", a valid certificate
// ... and so on
const char kPlaceholder[]{"@placeholder@"};

TEST_P(HttpsServerBrokenConfigTest, ensure) {
  std::vector<std::pair<std::string, std::string>> http_section;
  http_section.reserve(GetParam().http_section.size());

  // replace the placeholder
  for (auto const &e : GetParam().http_section) {
    std::string value(e.second);

    if (e.first == "port" && e.second == kPlaceholder) {
      value = std::to_string(http_port_);
    } else if (e.first == "ssl_cert" && e.second == kPlaceholder) {
      value = ssl_cert_data_dir_.join(kServerCertFile).str();
    } else if (e.first == "ssl_key" && e.second == kPlaceholder) {
      value = ssl_cert_data_dir_.join(kServerKeyFile).str();
    }
    http_section.push_back({e.first, value});
  }

  std::string conf_file{create_config_file(
      conf_dir_.name(),
      ConfigBuilder::build_section("http_server", http_section))};
  CommandHandle http_server{launch_router({"-c", conf_file})};

  EXPECT_EQ(EXIT_FAILURE,
            http_server.wait_for_exit(1000));  // assume it finishes in 1s
  EXPECT_EQ("", http_server.get_full_output());
  EXPECT_THAT(get_router_log_output(),
              ::testing::ContainsRegex(GetParam().errmsg_regex));
}

constexpr const char kErrmsgRegexNoSslCertKey[]{
    "if ssl=1 is set, ssl_cert and ssl_key must be set too"};

INSTANTIATE_TEST_CASE_P(
    http_static, HttpsServerBrokenConfigTest,
    ::testing::Values(
        HttpsServerBrokenConfigParams{
            "ssl, no cert, no key",
            {
                {"port", kPlaceholder}, {"ssl", "1"},  // enable SSL
            },
            kErrmsgRegexNoSslCertKey},
        HttpsServerBrokenConfigParams{"ssl and no cert",
                                      {
                                          {"port", kPlaceholder},
                                          {"ssl", "1"},  // enable SSL
                                          {"ssl_key", kPlaceholder},
                                      },
                                      kErrmsgRegexNoSslCertKey},
        HttpsServerBrokenConfigParams{"ssl and no key",
                                      {
                                          {"port", kPlaceholder},
                                          {"ssl", "1"},  // enable SSL
                                          {"ssl_cert", kPlaceholder},
                                      },
                                      kErrmsgRegexNoSslCertKey}),
    [](const ::testing::TestParamInfo<HttpsServerBrokenConfigParams> &info) {
      return gtest_sanitize_param_name(info.param.test_name);
    });

int main(int argc, char *argv[]) {
  TlsLibraryContext tls_lib_ctx;
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
