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

#include <string_view>
#include <system_error>
#include <thread>

#include <event2/event.h>  // EVENT__HAVE_OPENSSL
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "dim.h"
#include "gtest_testname.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/http_request.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

namespace {
// a placeholder that will be replaced with a valid value
//
// for "port", the currently assigned port will be used
// for "ssl_cert", a valid certificate
// ... and so on
const std::string_view kPlaceholder{"@good_placeholder@"};
const std::string_view kPlaceholderDatadir{"@placeholder_datadir@"};
const std::string_view kPlaceholderStddataDir{"@placeholder_stddatadir@"};
const std::string_view kPlaceholderHttpBaseDir{"@placeholder_httpbasedir@"};
const std::string_view kSubdirWithSpace{"with space"};
const std::string_view kSubdirWithIndex{"with_index"};
}  // namespace

#if 0
// invalid bind-address must be chosen wisely to
//
// - ensure getaddrinfo() doesn't try to resolve it and lead to timeouts
// - like "-1" leads to timeouts on some platforms.
// - "::::::::::" (10x ":") should be broken enough as it
//   - no digits that could be interpreted as an IPv4 address
//   - no alpnum that could be interpreted as hostname
//   - too many colon to make it a valid IPv6 address (IPv6 has 7)
static const char kInvalidBindAddress[]{"::::::::::"};
static_assert(sizeof(kInvalidBindAddress) > 7 + 1,
              "kInvalidBindAddress is too short");
#endif

using namespace std::chrono_literals;

const std::string kHttpBasedir(kPlaceholderHttpBaseDir);

uint16_t kHttpDefaultPort{8081};

static constexpr const char kSslSupportIsDisabled[]{
    "SSL support disabled at compile-time"};

static constexpr bool is_with_ssl_support() {
  return
#if defined(EVENT__HAVE_OPENSSL)
      true
#else
      false
#endif
      ;
}

static void ParamPrinter(
    const std::vector<std::pair<std::string, std::string>> &fields,
    std::ostream *os) {
  *os << "(";
  bool is_first{true};
  for (const auto &kv : fields) {
    if (is_first) {
      is_first = false;
    } else {
      *os << ", ";
    }
    *os << kv.first << ": ";
    if (kv.second.length() > 32) {
      *os << kv.second.substr(0, 32) << "...";
    } else {
      *os << kv.second;
    }
  }
  *os << ")";
}

// base-class to init RouterComponentTest before we launch_* anything
class HttpServerTestBase : public RouterComponentTest {};

/**
 * parameters of static-files tests.
 */
struct HttpServerPlainParams {
  std::string test_name;
  std::string test_scenario_id;

  std::string http_hostname;

  std::vector<std::pair<std::string, std::string>> http_section;

  bool expected_success;
  std::string stderr_regex;
  std::string errmsg_regex;

  int http_method;
  std::string raw_uri_path;
  std::string raw_uri_query;
  unsigned int status_code;

  friend void PrintTo(const HttpServerPlainParams &p, std::ostream *os) {
    if (p.expected_success) {
      ParamPrinter(
          {
              {"test_scenario_id",
               ::testing::PrintToString(p.test_scenario_id)},
              {"raw_path", ::testing::PrintToString(p.raw_uri_path)},
              {"raw_query", ::testing::PrintToString(p.raw_uri_query)},
              {"status_code", ::testing::PrintToString(p.status_code)},
          },
          os);
    } else {
      ParamPrinter(
          {
              {"test_scenario_id",
               ::testing::PrintToString(p.test_scenario_id)},
              {"stderr_regex", ::testing::PrintToString(p.stderr_regex)},
              {"errlog_regex", ::testing::PrintToString(p.errmsg_regex)},
          },
          os);
    }
  }
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
class HttpServerPlainTest
    : public HttpServerTestBase,
      public ::testing::WithParamInterface<HttpServerPlainParams> {
 public:
  HttpServerPlainTest()
      : http_port_{port_pool_.get_next_available()}, conf_dir_{} {}

  static void SetUpTestCase() {
    auto http_base_path = mysql_harness::Path(http_base_dir_.name());
    auto http_with_space_path = http_base_path.join(kSubdirWithSpace);
    auto http_with_index_path = http_base_path.join(kSubdirWithIndex);

    // create a few files in the http-base-dir
    mysql_harness::mkdir(http_with_space_path.str(), 0700);
    mysql_harness::mkdir(http_with_index_path.str(), 0700);

    {
      std::fstream touch(http_with_space_path.join("index.html").str(),
                         std::ios::out);
      ASSERT_TRUE(touch.is_open());
    }
    {
      std::fstream touch(http_with_index_path.join("index.html").str(),
                         std::ios::out);
      ASSERT_TRUE(touch.is_open());
    }
  }

 protected:
  uint16_t http_port_;
  TempDirectory conf_dir_;
  static TempDirectory http_base_dir_;
};

// init the static
TempDirectory HttpServerPlainTest::http_base_dir_;

/**
 * ensure GET requests for static files work.
 *
 * - start the http-server component
 * - make a client connect to the http-server
 */
TEST_P(HttpServerPlainTest, ensure) {
  std::vector<std::pair<std::string, std::string>> http_section;
  http_section.reserve(GetParam().http_section.size());

  uint16_t http_port = http_port_;
  bool has_port{false};

  // replace the placeholder
  for (auto const &e : GetParam().http_section) {
    std::string value(e.second);

    if (e.first == "port" && e.second == kPlaceholder) {
      value = std::to_string(http_port_);
      has_port = true;
    } else if (e.first == "static_folder") {
      if (e.second.substr(0, kPlaceholderHttpBaseDir.size()) ==
          kPlaceholderHttpBaseDir) {
        auto fp = mysql_harness::Path(http_base_dir_.name());
        {
          auto subpath = e.second.substr(kPlaceholderHttpBaseDir.size());
          if (!subpath.empty()) {
            fp = fp.join(subpath);
          }
        }
        value = fp.real_path().str();
      } else if (e.second.substr(0, kPlaceholderDatadir.size()) ==
                 kPlaceholderDatadir) {
        value = get_data_dir()
                    .join(e.second.substr(kPlaceholderDatadir.size()))
                    .str();
      }
    }
    http_section.emplace_back(e.first, value);
  }

  if (!has_port) {
    http_port = kHttpDefaultPort;
  }

  // Add a DEBUG level to trigger the 'Running' message.
  std::string conf_file{create_config_file(
      conf_dir_.name(),
      mysql_harness::join(
          std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
              "http_server", http_section)},
          ""))};
  ProcessWrapper &http_server{launch_router(
      {"-c", conf_file}, GetParam().expected_success ? 0 : EXIT_FAILURE, true,
      false, GetParam().expected_success ? 5s : -1s)};

  if (GetParam().expected_success) {
    std::string rel_uri = GetParam().raw_uri_path;

    if (!GetParam().raw_uri_query.empty()) {
      rel_uri += "?" + GetParam().raw_uri_query;
    }

    SCOPED_TRACE("// preparing client and connection object");
    IOContext io_ctx;

    RestClient rest_client(io_ctx, GetParam().http_hostname, http_port);

    SCOPED_TRACE("// wait http port connectable");
    try {
      ASSERT_NO_FATAL_FAILURE(check_port_ready(http_server, http_port,
                                               kDefaultPortReadyTimeout,
                                               GetParam().http_hostname))
          << "http-server:\n"
          << http_server.get_current_output();
    } catch (const std::system_error &e) {
      SCOPED_TRACE(
          "// wait_for_port_ready() failed, waiting for process to startup "
          "before we can kill it");
      // if we tried to connect to an address we can't assign (like connect(::1)
      // on a host IPv6 disabled), skip the test

      ASSERT_EQ(e.code(),
                make_error_condition(std::errc::address_not_available));

      // wait a bit to let the process actually startup to not kill it too early
      EXPECT_TRUE(wait_log_contains(http_server, "Running", 1000ms));

      // skip
      return;
    }

    SCOPED_TRACE("// requesting " + rel_uri);
    auto req = rest_client.request_sync(GetParam().http_method, rel_uri);
    ASSERT_TRUE(req) << rest_client.error_msg();
    ASSERT_EQ(req.get_response_code(), GetParam().status_code);
  } else {
    check_exit_code(http_server, EXIT_FAILURE,
                    1000ms);  // assume it finishes in 1s
    EXPECT_THAT(http_server.get_full_output(),
                ::testing::ContainsRegex(GetParam().stderr_regex));
    EXPECT_THAT(http_server.get_logfile_content(),
                ::testing::ContainsRegex(GetParam().errmsg_regex));
  }
}

const std::string localhost_ipv4("127.0.0.1");
const std::string localhost_ipv6("::1");

static const HttpServerPlainParams http_server_static_files_params[]{
    {"bind-address-ipv4-any",
     "WL11891::TS-3",
     localhost_ipv4,
     {
         {"bind_address", "0.0.0.0"},
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "^$",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"bind-address-ipv6-any",
     "WL11891::TS-3",
     localhost_ipv6,
     {
         {"bind_address", "::"},
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "^$",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"bind-address-ipv4-localhost",
     "WL11891::TS-6",
     localhost_ipv4,
     {
         {"bind_address", "127.0.0.1"},
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "^$",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"bind-address-ipv4-localhost-ws",
     "WL11891::TS-7",
     localhost_ipv4,
     {
         {"bind_address", " 127.0.0.1"},
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "^$",
     HttpMethod::Get,
     "/"
     "",
     "",
     404},

    {"bind-address-duplicated",
     "WL11891::TS-9",
     localhost_ipv4,
     {
         {"bind_address", " 127.0.0.1"},
         {"bind_address", " 127.0.0.1"},
         {"port", std::string(kPlaceholder)},
     },
     false,
     "Option 'bind_address'",
     "Could not open",  // config-parse error ...
     HttpMethod::Get,
     "/",
     "",
     404},

    // port

    {"port-non-default",
     "WL11891::TS-10",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"port-invalid",
     "WL11891::TS-12",
     localhost_ipv4,
     {
         {"port", "-1"},
     },
     false,
     "",
     "option port",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"port-duplicated",
     "WL11891::TS-13",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"port", std::string(kPlaceholder)},
     },
     false,
     "Option 'port' already defined",
     "Could not open file",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"port",
     "WL11891::TS-14",
     localhost_ipv4,
     {
         {"bind_address", "127.0.0.1"},
         {"port", std::string(kPlaceholder)},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     404},

    // static_folder

    {"GET, static_folder does not exist",
     "WL11891::TS-16",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", "does-not-exist"},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     "",
     404},

    {"GET, empty static_folder with trailing spaces",
     "WL11891::TS-18",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", " "},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/index.html",
     "",
     404},

    {"GET, empty static_folder",
     "WL11891::TS-18",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", ""},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/index.html",
     "",
     404},

    {"GET, static_folder dirname with spaces",
     "WL11891::TS-18",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir + "/" + std::string(kSubdirWithSpace)},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/index.html",
     "",
     200},

    // methods

    {"TRACE, file-exists",
     "WL11891::TS-20",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Trace,
     "/with_index/index.html",
     "",
     HttpStatusCode::MethodNotAllowed},

    {"POST, file-exists",
     "WL11891::TS-22",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Post,
     "/with_index/index.html",
     "",
     HttpStatusCode::MethodNotAllowed},

    {"GET, file exists",
     "WL11891:TS-23,WL11891::TS-15",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     "",
     200},

    {"GET, file does not exists",
     "WL11891::TS-24",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/does-not-exist",
     "",
     404},

    {"PUT, file-exists",
     "WL11891::TS-25",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Put,
     "/with_index/index.html",
     "",
     HttpStatusCode::MethodNotAllowed},

    {"PATCH, file-exists",
     "WL11891::TS-26",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Patch,
     "/with_index/index.html",
     "",
     HttpStatusCode::MethodNotAllowed},

    {"DELETE, file-exists",
     "WL11891::TS-27",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Delete,
     "/with_index/index.html",
     "",
     HttpStatusCode::MethodNotAllowed},

    // escaping

    {"GET, escaping",
     "WL11891::TS-29",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index%2ehtml",
     "",
     200},

    // index-files

    {"dir, no index-file",
     "",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     403},

    {"not leave root, ..",
     "WL11891::TS-31",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/../with_index/index.html",
     "",
     200},

    {"not leave root, ..%2f",
     "WL11891::TS-31",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/..%2fwith_index/index.html",
     "",
     200},

    {"long-uri",
     "WL11891::TS-32",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     std::string(15 * 1024, 'a'),
     200},

    {"URI parser, double question-mark",
     "",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     "some=?",
     200},

    {"edge-case, special chars",
     "",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index[].html",
     "",
     404},

    // ssl options ignored

    {"file exists, ssl=0, no ssl-params",
     "WL12524::TS_01",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
         {"ssl", "0"},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     "",
     200},

    {"file exists, ssl=0, ssl-params ignored",
     "WL12524::TS_02",
     localhost_ipv4,
     {
         {"port", std::string(kPlaceholder)},
         {"static_folder", kHttpBasedir},
         {"ssl", "0"},
         {"ssl_key", "does-not-exist"},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/with_index/index.html",
     "",
     200},
};

#if 0
// broken-addresses are hard to be preditably failing.
// getaddrinfo() will try to resolve it no matter what.

    {"bind-address-broken",
     "WL11891::TS-8",
     {
         {"bind_address", kInvalidBindAddress},
         {"port", std::string(kPlaceholder)},
     },
     false,
     "getaddrinfo",
     "binding socket failed",
     HttpMethod::Get,
     "/",
     "",
     404},
#endif

const HttpServerPlainParams http_server_static_files_unusable_params[]{
    // works, but can't be run in automated tests as it can't be guaranteed that
    // the default
    // port is not in use by something else
    {"all defaults",
     "WL11891::TS-3",
     localhost_ipv4,
     {},
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/my_port.js",
     "",
     404},
    {"bind-any-port-default",
     "WL11891::TS-5",
     localhost_ipv4,
     {
         {"bind_address", "0.0.0.0"},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     404},
    {"bind-localhost-port-default",
     "WL11891::TS-6",
     localhost_ipv4,
     {
         {"bind_address", "127.0.0.1"},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     404},

    {"port",
     "WL11891::TS-11",
     localhost_ipv4,
     {
         {"port", std::to_string(kHttpDefaultPort)},
     },
     true,
     "^$",
     "",
     HttpMethod::Get,
     "/",
     "",
     404},

};

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpServerPlainTest,
    ::testing::ValuesIn(http_server_static_files_params),
    [](const ::testing::TestParamInfo<HttpServerPlainParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name + " " +
          (info.param.expected_success ? std::to_string(info.param.status_code)
                                       : "fails_to_start"));
    });

// HTTPS tests

const char kServerCertFile[]{"server-cert.pem"};  // 2048 bit
const char kServerKeyFile[]{"server-key.pem"};
const char kServerCertCaFile[]{"cacert.pem"};
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
static const char kServerCertRsa1024File[]{
    "server-sha1-1024-cert.pem"};  // 1024 bit
#endif

#ifdef EVENT__HAVE_OPENSSL
static const char kWrongServerCertCaFile[]{"ca-sha512.pem"};
#endif
const char kDhParams4File[]{"dhparams-4.pem"};
const char kDhParams2048File[]{"dhparams-2048.pem"};

/**
 * params of HTTPS server tests.
 */
struct HttpClientSecureParams {
  std::string test_name;
  std::string test_scenario_id;

  std::string ca_cert_file;

  TlsVersion min_version;
  TlsVersion max_version;

  bool should_succeeed;
  std::string cipher_list;
  std::string untestable_error_msg;

  friend void PrintTo(const HttpClientSecureParams &p, std::ostream *os) {
    ParamPrinter(
        {
            {"test_scenario_id", ::testing::PrintToString(p.test_scenario_id)},
            {"ca_cert_file", ::testing::PrintToString(p.ca_cert_file)},
            {"cipher_list", ::testing::PrintToString(p.cipher_list)},
        },
        os);
  }
};

/**
 * test-suite of HTTPS server tests.
 *
 * ssl_cert_data_dir_ should point to the location of the
 * ${CMAKE_SOURCE_DIR}/mysql-test/sql_data/ as it contains the
 * certificates we need for testing.
 */
class HttpClientSecureTest
    : public HttpServerTestBase,
      public ::testing::WithParamInterface<HttpClientSecureParams> {
 public:
  HttpClientSecureTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        ssl_cert_data_dir_{SSL_TEST_DATA_DIR},
        conf_file_{create_config_file(
            conf_dir_.name(),
            mysql_harness::ConfigBuilder::build_section(
                "http_server",
                {
                    {"port", std::to_string(http_port_)},  // port to listen on
                    {"ssl", "1"},                          // enable SSL
                    {"ssl_cert",
                     ssl_cert_data_dir_.join(kServerCertFile).str()},
                    {"ssl_key", ssl_cert_data_dir_.join(kServerKeyFile).str()},
                }))} {}

 protected:
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  mysql_harness::Path ssl_cert_data_dir_;
  std::string conf_file_;
};

/**
 * pretty printer for TlsVersion for gtest.
 */
std::ostream &operator<<(std::ostream &os, TlsVersion v) {
  switch (v) {
    case TlsVersion::AUTO:
      os << "AUTO";
      break;
    case TlsVersion::SSL_3:
      os << "SSL3";
      break;
    case TlsVersion::TLS_1_0:
      os << "TLS1.0";
      break;
    case TlsVersion::TLS_1_1:
      os << "TLS1.1";
      break;
    case TlsVersion::TLS_1_2:
      os << "TLS1.2";
      break;
    case TlsVersion::TLS_1_3:
      os << "TLS1.3";
      break;
  }

  return os;
}

/**
 * ensure HTTPS requests work against a well configured server.
 *
 * - start the http-server component with TLS enabled
 * - make a client connect to the http-server
 */
TEST_P(HttpClientSecureTest, ensure) {
  std::string ca_cert = GetParam().ca_cert_file;
  bool should_succeeed = GetParam().should_succeeed;
  std::string cipher_list = GetParam().cipher_list;

  SCOPED_TRACE("// preparing client and connection object");
  TlsClientContext tls_ctx;

  // get the libraries min-version
  //
  // ubuntu/debian disable SSLv3 all the time, which makes TLSv1.0 the
  // min-version
  //
  // check if this test can succeed at all
  tls_ctx.version_range(TlsVersion::AUTO, TlsVersion::AUTO);
  auto library_min_version = tls_ctx.min_version();

  if (GetParam().max_version != TlsVersion::AUTO &&
      GetParam().max_version < library_min_version) {
    // the library will not allow us to set that value, assume it will fail
    ASSERT_FALSE(should_succeeed)
        << "test's TLS.max_version " << GetParam().max_version
        << " is less than " << library_min_version
        << " which can't succeed, but test is expected not fail";

    return;
  }

  tls_ctx.version_range(GetParam().min_version, GetParam().max_version);

  // as min-version isn't set, it may either be "AUTO" aka the lowest supported
  // or SSL_3 ... which is the lowest supported (openssl 1.1.0 and before)
  std::set<TlsVersion> allowed{TlsVersion::AUTO, GetParam().min_version,
                               library_min_version};
  EXPECT_THAT(allowed, ::testing::Contains(tls_ctx.min_version()));

  tls_ctx.ssl_ca(ssl_cert_data_dir_.join(ca_cert).str(), "");
  if (!cipher_list.empty()) {
    if (tls_ctx.has_set_cipher_suites()) tls_ctx.cipher_suites("");
    tls_ctx.cipher_list(cipher_list);
  }

  // help debugging why handshake failed.
  tls_ctx.info_callback([](const SSL *ssl, int where, int ret) {
    std::string state;
    // returns the updated options
    auto opts = SSL_get_options(const_cast<SSL *>(ssl));

    if (opts & SSL_OP_NO_SSLv2) {
      std::cerr << __LINE__ << ": no SSLv2" << std::endl;
    }
    if (opts & SSL_OP_NO_SSLv3) {
      std::cerr << __LINE__ << ": no SSLv3" << std::endl;
    }
    if (opts & SSL_OP_NO_TLSv1) {
      std::cerr << __LINE__ << ": no TLSv1.0" << std::endl;
    }
    if (opts & SSL_OP_NO_TLSv1_1) {
      std::cerr << __LINE__ << ": no TLSv1.1" << std::endl;
    }
    if (opts & SSL_OP_NO_TLSv1_2) {
      std::cerr << __LINE__ << ": no TLSv1.2" << std::endl;
    }

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

  HttpUri u;
  u.set_scheme("https");
  u.set_port(http_port_);
  u.set_host(http_hostname_);
  u.set_path("/");

  IOContext io_ctx;

  auto http_client = std::make_unique<HttpsClient>(io_ctx, std::move(tls_ctx),
                                                   u.get_host(), u.get_port());

  RestClient rest_client(std::move(http_client));

  SCOPED_TRACE("// wait http port connectable");

  ProcessWrapper &http_server = launch_router({"-c", conf_file_});

  ASSERT_NO_FATAL_FAILURE(check_port_ready(http_server, http_port_));

  SCOPED_TRACE("// GETing " + u.join());
  auto req = rest_client.request_sync(HttpMethod::Get, u.get_path());
  if (should_succeeed) {
    ASSERT_TRUE(req) << rest_client.error_msg();
    ASSERT_EQ(req.get_response_code(), 404);
  } else {
    ASSERT_FALSE(req) << req.get_response_code();
  }
}

#ifdef EVENT__HAVE_OPENSSL
static const HttpClientSecureParams http_client_secure_params[]{
    //
    {"default-client-cipher", "WL12524::TS_CR_06", kServerCertCaFile,
     TlsVersion::TLS_1_2, TlsVersion::AUTO, true, "", ""},
    {"SSL3", "WL12524::TS_SR1_01", kServerCertCaFile, TlsVersion::SSL_3,
     TlsVersion::SSL_3, false, "", "invalid cipher"},
    {"TLSv1.0", "WL12524::TS_SR1_01", kServerCertCaFile, TlsVersion::TLS_1_0,
     TlsVersion::TLS_1_0, false, "", "invalid cipher"},
    {"TLSv1.1", "WL12524::TS_SR1_01", kServerCertCaFile, TlsVersion::TLS_1_1,
     TlsVersion::TLS_1_1, false, "", "invalid cipher"},
    {"TLSv1.2", "", kServerCertCaFile, TlsVersion::TLS_1_2, TlsVersion::TLS_1_2,
     true, "", ""},
    {"TLSv1.2+ with TLS1.1 cipher", "WL12524::TS_SR2_01", kServerCertCaFile,
     TlsVersion::TLS_1_2, TlsVersion::AUTO, false, "AES128-SHA",
     "invalid cipher"},
    {"wrong ca", "", kWrongServerCertCaFile, TlsVersion::AUTO, TlsVersion::AUTO,
     false, "", "handshake failure"},
};

#if 0
    {"SSL3 cipher", "WL12524::TS_SR2_01", kServerCertCaFile, false,
     "DH-DSS-DES-CBC-SHA", "invalid cipher"},
    {"TLS1.0 cipher", "WL12524::TS_SR2_01", kServerCertCaFile, false,
     "DES-CBC-SHA", "invalid cipher"},
#endif

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpClientSecureTest, ::testing::ValuesIn(http_client_secure_params),
    [](const ::testing::TestParamInfo<HttpClientSecureParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name +
          (info.param.should_succeeed == false ? " fails" : " succeeds"));
    });

#endif

//
// http_server, broken-config, SSL
//

struct HttpServerSecureParams {
  std::string test_name;
  std::string test_scenario_id;

  std::vector<std::pair<std::string, std::string>> http_section;

  bool expected_success;
  std::string errmsg_regex;

  friend void PrintTo(const HttpServerSecureParams &p, std::ostream *os) {
    ParamPrinter(
        {
            {"test_scenario_id", ::testing::PrintToString(p.test_scenario_id)},
            {"errmsg_regex", ::testing::PrintToString(p.errmsg_regex)},
        },
        os);
  }
};

/**
 * config-failures for HTTPS setups.
 *
 */
class HttpServerSecureTest
    : public HttpServerTestBase,
      public ::testing::WithParamInterface<HttpServerSecureParams> {
 public:
  HttpServerSecureTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        ssl_cert_data_dir_{SSL_TEST_DATA_DIR} {}

 protected:
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  mysql_harness::Path ssl_cert_data_dir_;
};

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
constexpr const char kErrmsgRegexWeakSslKey[]{"ca md too weak"};
#else
constexpr const char kErrmsgRegexWeakSslKey[]{"key-size too small"};
#endif

TEST_P(HttpServerSecureTest, ensure) {
  // const size_t placeholder_length = strlen(kPlaceholder);

  std::vector<std::pair<std::string, std::string>> http_section;
  http_section.reserve(GetParam().http_section.size());

  // replace the placeholder
  for (auto const &e : GetParam().http_section) {
    std::string value(e.second);

    if (e.first == "port" && e.second == kPlaceholder) {
      value = std::to_string(http_port_);
    } else if (e.first == "ssl_cert" || e.first == "ssl_key" ||
               e.first == "ssl_dh_param") {
      if (e.second.substr(0, kPlaceholderStddataDir.size()) ==
          kPlaceholderStddataDir) {
        value = ssl_cert_data_dir_
                    .join(e.second.substr(kPlaceholderStddataDir.size()))
                    .str();
      } else if (e.second.substr(0, kPlaceholderDatadir.size()) ==
                 kPlaceholderDatadir) {
        value = get_data_dir()
                    .join(e.second.substr(kPlaceholderDatadir.size()))
                    .str();
      }
    }
    http_section.emplace_back(e.first, value);
  }

  std::string conf_file{create_config_file(
      conf_dir_.name(),
      mysql_harness::ConfigBuilder::build_section("http_server", http_section),
      nullptr, "mysqlrouter.conf", "", false)};

  // timeout for waiting for ready notification is increased to address the case
  // of strong dh params which takes long on overloaded CPUs
  ProcessWrapper &http_server{
      launch_router({"-c", conf_file},
                    GetParam().expected_success ? EXIT_SUCCESS : EXIT_FAILURE,
                    true, false, GetParam().expected_success ? 20s : -1s)};

  if (GetParam().expected_success) {
    HttpUri u;
    u.set_scheme("https");
    u.set_port(http_port_);
    u.set_host(http_hostname_);
    u.set_path("/");

    SCOPED_TRACE("// preparing client and connection object");
    IOContext io_ctx;
    TlsClientContext tls_ctx;

    tls_ctx.ssl_ca(ssl_cert_data_dir_.join(kServerCertCaFile).str(), "");

    auto http_client = std::make_unique<HttpsClient>(
        io_ctx, std::move(tls_ctx), u.get_host(), u.get_port());

    RestClient rest_client(std::move(http_client));

    SCOPED_TRACE("// wait for port ready");
    ASSERT_NO_FATAL_FAILURE(check_port_ready(http_server, http_port_));

    SCOPED_TRACE("// GETing " + u.join());
    auto req = rest_client.request_sync(HttpMethod::Get, u.get_path());
    ASSERT_TRUE(req) << rest_client.error_msg();
    ASSERT_EQ(req.get_response_code(), 404);
  } else {
    SCOPED_TRACE("// wait until process has finished.");
    check_exit_code(http_server, EXIT_FAILURE, 10s);

    SCOPED_TRACE("// stderr should be empty");
    EXPECT_THAT(http_server.get_full_output(), ::testing::IsEmpty());

    // if openssl 1.1.0 is used and it is compiled with
    // "-DOPENSSL_TLS_SECURITY_LEVEL" > 1 we may also get "ee key too small"
    // instead of kErrmsgRegexWeakSslKey.
    const auto errmsg_regex =
        (GetParam().errmsg_regex == kErrmsgRegexWeakSslKey &&
         TlsClientContext().security_level() > 1)
            ? "ee key too small"
            : GetParam().errmsg_regex;

    EXPECT_THAT(http_server.get_logfile_content(),
                ::testing::ContainsRegex(errmsg_regex));
  }
}

constexpr const char kErrmsgRegexNoSslCertKey[]{
    "if ssl=1 is set, ssl_cert and ssl_key must be set too"};

const HttpServerSecureParams http_server_secure_params[]{
    {"ssl, no cert, no key",
     "WL12524::TS_CR_01",
     {
         {"port", std::string(kPlaceholder)},  //
         {"ssl", "1"},                         // enable SSL
     },
     false,
     kErrmsgRegexNoSslCertKey},

    {"ssl_is_hex",
     "",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "0x1"},  // hex-numbers should fail.
         {"ssl_key", std::string(kPlaceholder)},
     },
     false,
     R"(option ssl in \[http_server\] needs value between 0 and 1 inclusive, was '0x1')"},

    {"ssl=1, no cert",
     "WL12524::TS_CR_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},  // enable SSL
         {"ssl_key", std::string(kPlaceholder)},
     },
     false,
     kErrmsgRegexNoSslCertKey},

    {"ssl=1, no key",
     "WL12524::TS_CR_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},  // enable SSL
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
     },
     false,
     kErrmsgRegexNoSslCertKey},
    {"ssl=1, bad cert",
     "WL12524::TS_CR_02",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key", "does-not-exist"},
         {"ssl_cert", "does-not-exist"},
     },
     false,
     "SSL certificate file 'does-not-exist' failed"},

    {"ssl=1, cert, some unacceptable ciphers",
     "WL12524::TS_CR_05",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "AES128-SHA:TLSv1.2"},
     },
     // if SSL support is disabled in libevent, we should see a failure,
     // success otherwise
     is_with_ssl_support(),
     is_with_ssl_support() ? "" : kSslSupportIsDisabled},
    {"ssl=1, cert, only acceptable ciphers",
     "WL12524::TS_CR_07",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "ECDHE-RSA-AES128-SHA256"},
     },
     // if SSL support is disabled in libevent, we should see a failure,
     // success otherwise
     is_with_ssl_support(),
     is_with_ssl_support() ? "" : kSslSupportIsDisabled},
    {"dh_param file does not exist",
     "",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "AES128-SHA256"},
         {"ssl_dh_param", "does-not-exist"},
     },
     false,
     "setting ssl_dh_params failed"},
    {"dh_param file is no PEM",
     "WL12524::TS_CR_08",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_dh_param",
          std::string(kPlaceholderDatadir) + "/" + "my_port.js"},
     },
     false,
     "setting ssl_dh_params failed"},
    {"dh ciphers, default dh-params",
     "WL12524::TS_CR_09",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         // force a DHE cipher that's known to the server
         {"ssl_cipher", "DHE-RSA-AES256-SHA256"},
     },
     // if SSL support is disabled in libevent, we should see a failure,
     // success otherwise
     is_with_ssl_support(),
     is_with_ssl_support() ? "" : kSslSupportIsDisabled},
    {"dh ciphers, strong dh-params",
     "WL12524::TS_SR4_01,WL12524::TS_SR3_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         // force a DHE cipher that's known to the server
         {"ssl_cipher", "DHE-RSA-AES256-SHA256"},
         {"ssl_dh_param",
          std::string(kPlaceholderDatadir) + "/" + kDhParams2048File},
     },
     // if SSL support is disabled in libevent, we should see a failure,
     // success otherwise
     is_with_ssl_support(),
     is_with_ssl_support() ? "" : kSslSupportIsDisabled},
    {"non-dh-cipher, strong dh-params",
     "WL12524::TS_SR4_01,WL12524::TS_SR3_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "AES128-SHA256"},
         {"ssl_dh_param",
          std::string(kPlaceholderDatadir) + "/" + kDhParams2048File},
     },
     // if SSL support is disabled in libevent, we should see a failure,
     // success otherwise
     is_with_ssl_support(),
     is_with_ssl_support() ? "" : kSslSupportIsDisabled},
    {"dh ciphers, weak dh-params",
     "WL12524::TS_SR7_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         // force a DHE cipher that's known to the server
         {"ssl_cipher", "DHE-RSA-AES256-SHA256"},
         {"ssl_dh_param",
          std::string(kPlaceholderDatadir) + "/" + kDhParams4File},
     },
     false,
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
     "modulus too small"
#else
     "key size of DH param"
#endif
    },
};

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpServerSecureTest, ::testing::ValuesIn(http_server_secure_params),
    [](const ::testing::TestParamInfo<HttpServerSecureParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name +
          (info.param.expected_success ? "_works" : "_fails"));
    });

#if (OPENSSL_VERSION_NUMBER < 0x10101000L)
// This fails with OpenSSL 1.1.1 that added TLS1.3 default ciphers that we
// can't disable
const HttpServerSecureParams http_server_secure_params_pre_openssl_111[]{
    {"ssl_1_cert_only_unacceptable_ciphers",
     "WL12524::TS_CR_04",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "AES128-SHA"},
     },
     // connection will fail as ciphers can't be negotiated or libevent may
     // not support SSL
     false,
     is_with_ssl_support() ? "no cipher match" : kSslSupportIsDisabled},
};

INSTANTIATE_TEST_SUITE_P(
    Openssl111, HttpServerSecureTest,
    ::testing::ValuesIn(http_server_secure_params_pre_openssl_111),
    [](const ::testing::TestParamInfo<HttpServerSecureParams> &info) {
      return info.param.test_name +
             (info.param.expected_success ? "_works" : "_fails");
    });
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
// the bitsize of the public key can only be determined with
// openssl 1.0.2 and later
const HttpServerSecureParams http_server_secure_openssl102_plus_params[]{
    {"ssl_cert weak",
     "WL12524::TS_SR6_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertRsa1024File},
     },
     false,
     kErrmsgRegexWeakSslKey},
    {"ecdh cipher",
     "WL12524::TS_SR6_01",
     {
         {"port", std::string(kPlaceholder)},
         {"ssl", "1"},
         {"ssl_key",
          std::string(kPlaceholderStddataDir) + "/" + kServerKeyFile},
         {"ssl_cert",
          std::string(kPlaceholderStddataDir) + "/" + kServerCertFile},
         {"ssl_cipher", "ECDHE"},
         {"ssl_curves", "P-256"},
     },
     true,
     // if openssl 1.1.0 is used and it is compiled with
     // "-DOPENSSL_TLS_SECURITY_LEVEL" > 4 we may also get "ee key too small"
     // here.
     "no-error"},
};

INSTANTIATE_TEST_SUITE_P(
    Openssl102_plus, HttpServerSecureTest,
    ::testing::ValuesIn(http_server_secure_openssl102_plus_params),
    [](const ::testing::TestParamInfo<HttpServerSecureParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name +
          (info.param.expected_success ? "_works" : "_fails"));
    });
#endif

//
// HTTP auth
//

struct HttpServerAuthParams {
  std::string test_name;
  std::string test_scenario_id;

  std::string url;
  unsigned int status_code;
  std::string username;
  std::string password;

  friend void PrintTo(const HttpServerAuthParams &p, std::ostream *os) {
    ParamPrinter(
        {
            {"test_scenario_id", ::testing::PrintToString(p.test_scenario_id)},
            {"url", ::testing::PrintToString(p.url)},
            {"status_code", ::testing::PrintToString(p.status_code)},
            {"username", ::testing::PrintToString(p.username)},
            {"password", ::testing::PrintToString(p.password)},
        },
        os);
  }
};

class HttpServerAuthTest
    : public HttpServerTestBase,
      public ::testing::WithParamInterface<HttpServerAuthParams> {
 public:
  HttpServerAuthTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        passwd_filename_{"passwd"},
        conf_file_{create_config_file(
            conf_dir_.name(),
            mysql_harness::join(
                std::vector<std::string>{
                    mysql_harness::ConfigBuilder::build_section(
                        "http_server", {{"port", std::to_string(http_port_)},
                                        {"require_realm", "secure"}}),
                    mysql_harness::ConfigBuilder::build_section(
                        "http_auth_backend:local",
                        {{"backend", "file"},
                         {"filename", mysql_harness::Path(conf_dir_.name())
                                          .join(passwd_filename_)
                                          .str()}}),
                    mysql_harness::ConfigBuilder::build_section(
                        "http_auth_realm:secure", {{"backend", "local"},
                                                   {"method", "basic"},
                                                   {"name", "API"},
                                                   {"require", "valid-user"}})},
                ""))} {
    std::string pwf_name(
        mysql_harness::Path(conf_dir_.name()).join(passwd_filename_).str());
    std::fstream pwf{pwf_name, pwf.out};

    if (!pwf.is_open())
      throw std::runtime_error(pwf_name +
                               " failed to open: " + std::to_string(errno));
    constexpr const char kPasswdUserTest[]{
        "user:$6$3ieWD5TQkakPm.iT$"  // sha512 and salt
        "4HI5XzmE4UCSOsu14jujlXYNYk2SB6gi2yVoAncaOzynEnTI0Rc9."
        "78jHABgKm2DHr1LHc7Kg9kCVs9/uCOR7/\n"  // password: test
    };

    pwf << kPasswdUserTest;
  }
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  std::string passwd_filename_;
  std::string conf_file_;
};

/**
 * ensure GET requests for static files work.
 *
 * - start the http-server component
 * - make a client connect to the http-server
 */
TEST_P(HttpServerAuthTest, ensure) {
  launch_router({"-c", conf_file_});

  const std::string http_uri = GetParam().url;
  SCOPED_TRACE("// connecting " + http_hostname_ + ":" +
               std::to_string(http_port_) + " for " + http_uri);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_,
                         GetParam().username, GetParam().password);
  auto req = rest_client.request_sync(HttpMethod::Get, http_uri);
  ASSERT_TRUE(req);
  ASSERT_EQ(req.get_response_code(), GetParam().status_code);
}

const HttpServerAuthParams http_server_auth_params[]{
    {"good_creds", "WL12503::TS_2_1", "/", 404, "user", "test"},
    {"wrong_user", "WL12503::TS_2_2", "/", 401, "user", "wrong"},
    {"no_creds", "WL12503::TS_2_3", "/", 401, "", ""},
    {"wrong_password", "WL12503::TS_2_2", "/", 401, "other", "test"},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpServerAuthTest, ::testing::ValuesIn(http_server_auth_params),
    [](const ::testing::TestParamInfo<HttpServerAuthParams> &info) {
      return info.param.test_name +
             (info.param.status_code == 401 ? "_fails_auth" : "_succeeds_auth");
    });

// Fail tests

struct HttpServerAuthFailParams {
  std::string test_name;
  std::string test_scenario_id;

  std::vector<std::string> sections;
  bool check_at_runtime;

  std::string expected_errmsg;

  friend void PrintTo(const HttpServerAuthFailParams &p, std::ostream *os) {
    ParamPrinter(
        {
            {"test_scenario_id", ::testing::PrintToString(p.test_scenario_id)},
            {"expected_errmsg", ::testing::PrintToString(p.expected_errmsg)},
        },
        os);
  }
};

class HttpServerAuthFailTest
    : public HttpServerTestBase,
      public ::testing::WithParamInterface<HttpServerAuthFailParams> {
 public:
  HttpServerAuthFailTest()
      : http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        passwd_filename_{"passwd"} {}

  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  std::string passwd_filename_;
};

/**
 * ensure GET requests for static files work.
 *
 * - start the http-server component
 * - make a client connect to the http-server
 */
TEST_P(HttpServerAuthFailTest, ensure) {
  std::vector<std::string> config_sections{
      mysql_harness::ConfigBuilder::build_section(
          "http_server",
          {
              {"port", std::to_string(http_port_)},
              {"require_realm", "secure"},
          }),
  };
  std::string passwd_filename =
      Path(conf_dir_.name()).join(passwd_filename_).str();
  for (auto section : GetParam().sections) {
    // replace filename @placeholder@
    const std::string placeholder("filename=@placeholder@");
    const size_t pos = section.find(placeholder);
    if (pos != std::string::npos) {
      section.replace(pos, placeholder.length(), "filename=" + passwd_filename);
    }
    config_sections.push_back(section);
  }

  std::fstream pwf{passwd_filename, std::ios::out};

  if (!pwf.is_open()) throw std::runtime_error("hmm");

  constexpr const char kPasswdUserTest[]{
      "user:$6$3ieWD5TQkakPm.iT$"  // sha512 and salt
      "4HI5XzmE4UCSOsu14jujlXYNYk2SB6gi2yVoAncaOzynEnTI0Rc9."
      "78jHABgKm2DHr1LHc7Kg9kCVs9/uCOR7/\n"  // password: test
  };

  pwf << kPasswdUserTest;
  pwf.close();

  std::string config_content = mysql_harness::join(config_sections, "\n");

  std::string conf_file = create_config_file(conf_dir_.name(), config_content);

  ProcessWrapper &http_server{
      launch_router({"-c", conf_file},
                    GetParam().check_at_runtime ? EXIT_SUCCESS : EXIT_FAILURE,
                    true, false, -1s)};

  if (GetParam().check_at_runtime) {
    ASSERT_NO_FATAL_FAILURE(check_port_ready(http_server, http_port_));
    std::string http_uri = "/";
    SCOPED_TRACE("// connecting " + http_hostname_ + ":" +
                 std::to_string(http_port_) + " for " + http_uri);

    IOContext io_ctx;
    RestClient rest_client(io_ctx, http_hostname_, http_port_, "user", "test");
    auto req = rest_client.request_sync(HttpMethod::Get, http_uri);
    ASSERT_TRUE(req);
    ASSERT_EQ(req.get_response_code(), 404);
  } else {
    SCOPED_TRACE("// wait process to exit with with error");
    check_exit_code(http_server, EXIT_FAILURE);
    EXPECT_THAT(http_server.get_logfile_content(),
                ::testing::HasSubstr(GetParam().expected_errmsg));
  }
}

const HttpServerAuthFailParams http_server_auth_fail_params[]{
    {"backend_no_section",
     "",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_backend",
             {{"backend", "file"}, {"filename", "does-not-exists"}}),
     },
     false,
     "The config section [http_auth_backend] requires a name, like "
     "[http_auth_backend:example]"},
    {"backend_file_filename_not_exists",
     "WL12503::TS_FR6_1",
     {mysql_harness::ConfigBuilder::build_section(
          "http_auth_backend:local",
          {
              {"backend", "file"},
              {"filename", "does-not-exists"},
          }),
      mysql_harness::ConfigBuilder::build_section("http_auth_realm:secure",
                                                  {{"backend", "doesnotexist"},
                                                   {"method", "basic"},
                                                   {"name", "API"},
                                                   {"require", "valid-user"}})},
     false,
     "parsing does-not-exists "},
    {"backend_method_unknown",
     "WL12503::TS_FR6_2",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm:secure", {{"backend", "doesnotexist"},
                                        {"method", "unknown"},
                                        {"name", "API"},
                                        {"require", "valid-user"}}),
     },
     false,
     "unsupported authentication method for "},
    {"backend_does_not_exist",
     "WL12503::TS_FR6_1",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm:secure", {{"backend", "doesnotexist"},
                                        {"method", "basic"},
                                        {"name", "API"},
                                        {"require", "valid-user"}}),
     },
     false,
     "The option 'backend=doesnotexist' in [http_auth_realm:secure] does not "
     "match any http_auth_backend. No [http_auth_backend:doesnotexist] "
     "section defined."},
    {"realm_no_section",
     "WL12503::TS_FR6_1",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm", {{"backend", "doesnotexist"},
                                 {"method", "basic"},
                                 {"name", "API"},
                                 {"require", "valid-user"}}),
     },
     false,
     "The config section [http_auth_realm] requires a name, like "
     "[http_auth_realm:example]"},
    {"multiple_backends",
     "WL12503::TS_2_7",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm:secure", {{"backend", "local"},
                                        {"method", "basic"},
                                        {"name", "API"},
                                        {"require", "valid-user"}}),
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_backend:local",
             {
                 {"backend", "file"},
                 {"filename", "@placeholder@"},
             }),
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_backend:someother",
             {
                 {"backend", "file"},
                 {"filename", "@placeholder@"},
             }),
     },
     true,
     ""},
    {"multiple_realms",
     "",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm:secure", {{"backend", "local"},
                                        {"method", "basic"},
                                        {"name", "API"},
                                        {"require", "valid-user"}}),
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_realm:someother", {{"backend", "local"},
                                           {"method", "basic"},
                                           {"name", "SomeOtherApi"},
                                           {"require", "valid-user"}}),
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_backend:local",
             {
                 {"backend", "file"},
                 {"filename", "@placeholder@"},
             }),
     },
     true,
     ""},
    {"realm_does_not_exist", "", {}, false, "unknown authentication realm for"},
    {"wrong_backend_type_doesnot_exist",
     "",
     {
         mysql_harness::ConfigBuilder::build_section(
             "http_auth_backend:local", {{"backend", "doesnotexist"}}),
     },
     false,
     "unknown backend=doesnotexist in section: http_auth_backend"}};

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpServerAuthFailTest,
    ::testing::ValuesIn(http_server_auth_fail_params),
    [](const ::testing::TestParamInfo<HttpServerAuthFailParams> &info) {
      return gtest_sanitize_param_name(
          info.param.test_name +
          (info.param.check_at_runtime ? "_works" : "_fails"));
    });

int main(int argc, char *argv[]) {
  TlsLibraryContext tls_lib_ctx;
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
