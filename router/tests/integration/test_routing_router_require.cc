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

#include <algorithm>  // min
#include <charconv>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include "process_wrapper.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "my_rapidjson_size_t.h"

#include <rapidjson/pointer.h>

#include "exit_status.h"
#include "hexify.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/classic_protocol_codec_frame.h"
#include "mysqlrouter/classic_protocol_codec_message.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/http_client.h"
#include "mysqlrouter/utils.h"
#include "mysqlxclient.h"
#include "mysqlxclient/xerror.h"
#include "openssl_version.h"  // ROUTER_OPENSSL_VERSION
#include "process_launcher.h"
#include "process_manager.h"
#include "procs.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "scope_guard.h"
#include "shared_server.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

struct ConnectionParam {
  std::string_view testname;

  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;

  enum class Protocol {
    Classic,
    X,
  };
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

class SharedRouter {
 public:
  SharedRouter(TcpPortPool &port_pool) : port_pool_(port_pool) {}

  integration_tests::Procs &process_manager() { return procs_; }

  static std::vector<std::string> classic_destinations_from_shared_servers(
      std::span<SharedServer *const> servers) {
    std::vector<std::string> dests;
    dests.reserve(servers.size());

    for (const auto &s : servers) {
      dests.push_back(s->server_host() + ":" +
                      std::to_string(s->server_port()));
    }

    return dests;
  }

  static std::vector<std::string> x_destinations_from_shared_servers(
      std::span<SharedServer *const> servers) {
    std::vector<std::string> dests;
    dests.reserve(servers.size());

    for (const auto &s : servers) {
      dests.push_back(s->server_host() + ":" +
                      std::to_string(s->server_mysqlx_port()));
    }

    return dests;
  }

  void spawn_router(const std::vector<std::string> &classic_destinations,
                    const std::vector<std::string> &x_destinations) {
    auto userfile = conf_dir_.file("userfile");
    {
      std::ofstream ofs(userfile);
      // user:pass
      ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
             "YJgciRvb69";
    }

    auto writer = process_manager().config_writer(conf_dir_.name());

    for (auto protocol :
         {ConnectionParam::Protocol::Classic, ConnectionParam::Protocol::X}) {
      for (const auto &param : connection_params) {
        auto port_key = std::make_tuple(param.client_ssl_mode,
                                        param.server_ssl_mode, protocol);
        auto ports_it = ports_.find(port_key);

        const auto port =
            ports_it == ports_.end()
                ? (ports_[port_key] = port_pool_.get_next_available())
                : ports_it->second;

        auto protocol_name =
            (protocol == ConnectionParam::Protocol::Classic ? "classic"s
                                                            : "x"s);
        const auto &destinations =
            (protocol == ConnectionParam::Protocol::Classic
                 ? classic_destinations
                 : x_destinations);

        std::map<std::string, std::string> options{
            {"bind_port", std::to_string(port)},
            {"destinations", mysql_harness::join(destinations, ",")},
            {"protocol", protocol_name},
            {"routing_strategy", "round-robin"},

            {"client_ssl_mode", std::string(param.client_ssl_mode)},
            {"server_ssl_mode", std::string(param.server_ssl_mode)},

            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
            {"connection_sharing", "0"},     //
            {"connect_retry_timeout", "0"},  //
        };

#if !defined(_WIN32)
        options.emplace("socket", socket_path(param, protocol));
#endif

        if (protocol == ConnectionParam::Protocol::Classic) {
          if (param.client_ssl_mode != kPassthrough) {
            options.emplace("router_require_enforce", "1");
            if (param.client_ssl_mode != kDisabled) {
              options.emplace("client_ssl_ca",
                              SSL_TEST_DATA_DIR "/crl-ca-cert.pem");
              options.emplace("client_ssl_crl",
                              SSL_TEST_DATA_DIR "/crl-client-revoked.crl");
            }
          }
        }

        if (!(param.client_ssl_mode == kPassthrough ||
              param.server_ssl_mode == kDisabled ||
              (param.client_ssl_mode == kDisabled &&
               param.server_ssl_mode == kAsClient))) {
          options.emplace("server_ssl_key",
                          SSL_TEST_DATA_DIR "/crl-client-key.pem");
          options.emplace("server_ssl_cert",
                          SSL_TEST_DATA_DIR "/crl-client-cert.pem");
        }

        writer.section(
            "routing:" + protocol_name + "_" + std::string(param.testname),
            options);
      }
    }

    auto bindir = process_manager().get_origin();
    auto builddir = bindir.join("..");

    auto &proc =
        process_manager()
            .spawner(bindir.join("mysqlrouter").str())
            .with_core_dump(true)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
            .spawn({"-c", writer.write()});

    proc.set_logging_path(process_manager().get_logging_dir().str(),
                          "mysqlrouter.log");

    if (!proc.wait_for_sync_point_result()) {
      GTEST_SKIP() << "router failed to start\n" << proc.get_logfile_content();
    }
  }

  [[nodiscard]] auto host() const { return router_host_; }

  [[nodiscard]] uint16_t port(const ConnectionParam &param,
                              ConnectionParam::Protocol protocol) const {
    return ports_.at(std::make_tuple(param.client_ssl_mode,
                                     param.server_ssl_mode, protocol));
  }

  [[nodiscard]] std::string socket_path(
      const ConnectionParam &param, ConnectionParam::Protocol protocol) const {
    return Path(conf_dir_.name())
        .join((protocol == ConnectionParam::Protocol::Classic ? "classic_"s
                                                              : "x_"s) +
              std::string(param.client_ssl_mode) + "_" +
              std::string(param.server_ssl_mode) + ".sock")
        .str();
  }

 private:
  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  TempDirectory conf_dir_;

  static const constexpr char router_host_[] = "127.0.0.1";
  std::map<
      std::tuple<std::string_view, std::string_view, ConnectionParam::Protocol>,
      uint16_t>
      ports_;
};

/* test environment.
 *
 * spawns servers for the tests.
 */
class TestEnv : public ::testing::Environment {
 public:
  static constexpr const int kStartedSharedServers = 1;

  static SharedServer::Account server_requires_none() {
    return SharedServer::caching_sha2_empty_password_account();
  }

  static SharedServer::Account server_requires_ssl_account() {
    return {"server_requires_ssl", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account server_requires_x509_account() {
    return {"server_requires_x509", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account server_requires_x509_issuer_account() {
    return {"server_requires_x509_issuer", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account server_requires_x509_subject_account() {
    return {"server_requires_x509_subject", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_ssl_true_account() {
    return {"router_requires_ssl_true", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_ssl_false_account() {
    return {"router_requires_ssl_false", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_x509_true_account() {
    return {"router_requires_x509_true", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_x509_false_account() {
    return {"router_requires_x509_false", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_x509_issuer_account() {
    return {"router_requires_x509_issuer", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_x509_subject_account() {
    return {"router_requires_x509_subject", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account router_requires_unknown_attribute_account() {
    return {"unknown_attribute", "pass", "caching_sha2_password"};
  }

  static SharedServer::Account other_attribute_account() {
    return {"other_attribute", "pass", "caching_sha2_password"};
  }

  void SetUp() override {
    for (auto &s : shared_servers_) {
      if (s == nullptr) {
        s = new SharedServer(port_pool_);
        s->prepare_datadir();
        s->spawn_server({
            "--ssl-key=" SSL_TEST_DATA_DIR "/crl-server-key.pem",      //
            "--ssl-cert=" SSL_TEST_DATA_DIR "/crl-server-cert.pem",    //
            "--ssl-ca=" SSL_TEST_DATA_DIR "/crl-ca-cert.pem",          //
            "--ssl-crl=" SSL_TEST_DATA_DIR "/crl-client-revoked.crl",  //
        });
        if (s->mysqld_failed_to_start()) {
          GTEST_SKIP() << "mysql-server failed to start.";
        }

        auto cli_res = s->admin_cli();
        ASSERT_NO_ERROR(cli_res);

        auto cli = std::move(*cli_res);

        SharedServer::setup_mysqld_accounts(cli);

        SCOPED_TRACE("// create accounts for the different scenarios");
        {
          auto account = server_requires_ssl_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " REQUIRE SSL";
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = server_requires_x509_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " REQUIRE X509";
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = server_requires_x509_issuer_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " REQUIRE ISSUER "
              "'/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL "
              "test ca certificate'";
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = server_requires_x509_subject_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " REQUIRE SUBJECT "
              "'/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL "
              "test client certificate'";
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_ssl_false_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"router_require":{"ssl":false}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_ssl_true_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"router_require":{"ssl":true}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_x509_false_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"router_require":{"x509":false}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_x509_true_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"router_require":{"x509":true}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_x509_issuer_account();

          std::ostringstream stmt;
          stmt
              << "CREATE USER " << std::quoted(account.username) <<       //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
              << std::quoted(
                     R"({"router_require":{"issuer":"/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL test ca certificate"}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_x509_subject_account();

          std::ostringstream stmt;
          stmt
              << "CREATE USER " << std::quoted(account.username) <<       //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
              << std::quoted(
                     R"({"router_require":{"subject":"/C=IN/ST=Karnataka/L=Bengaluru/O=Oracle/OU=MySQL/CN=MySQL CRL test client certificate"}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = router_requires_unknown_attribute_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"router_require":{"unknown": true}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }

        {
          auto account = other_attribute_account();

          std::ostringstream stmt;
          stmt << "CREATE USER " << std::quoted(account.username) <<      //
              " IDENTIFIED WITH " << std::quoted(account.auth_method) <<  //
              " BY " << std::quoted(account.password) <<                  //
              " ATTRIBUTE "
               << std::quoted(R"({"other":{}, "router_require": {}})");
          ASSERT_NO_ERROR(cli.query(stmt.str())) << stmt.str();
        }
      }
    }
  }

  std::array<SharedServer *, kStartedSharedServers> servers() {
    return shared_servers_;
  }

  TcpPortPool &port_pool() { return port_pool_; }

  void TearDown() override {
    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->shutdown());
    }

    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->process_manager().wait_for_exit());
    }

    for (auto &s : shared_servers_) {
      if (s != nullptr) delete s;

      s = nullptr;
    }

    SharedServer::destroy_statics();
  }

 protected:
  TcpPortPool port_pool_;

  std::array<SharedServer *, kStartedSharedServers> shared_servers_{};
};

TestEnv *test_env{};

/* test-suite with shared routers.
 */
class TestWithSharedRouter {
 public:
  static void SetUpTestSuite(TcpPortPool &port_pool,
                             std::span<SharedServer *const> servers) {
    for (const auto &s : servers) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    if (shared_router_ == nullptr) {
      shared_router_ = new SharedRouter(port_pool);

      SCOPED_TRACE("// spawn router");
      shared_router_->spawn_router(
          SharedRouter::classic_destinations_from_shared_servers(servers),
          SharedRouter::x_destinations_from_shared_servers(servers));
    }
  }

  static void TearDownTestSuite() {
    delete shared_router_;
    shared_router_ = nullptr;
  }

  static SharedRouter *router() { return shared_router_; }

 protected:
  static SharedRouter *shared_router_;
};

SharedRouter *TestWithSharedRouter::shared_router_ = nullptr;

class RouterRequireTestBase : public RouterComponentTest {
 public:
  static constexpr const size_t kNumServers = 1;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers());
  }

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, 1> shared_servers() {
    return test_env->servers();
  }

  SharedRouter *shared_router() const { return TestWithSharedRouter::router(); }

  void SetUp() override {
    for (auto &s : shared_servers()) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      }
    }
  }

  ~RouterRequireTestBase() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }
};

class RouterRequireTest
    : public RouterRequireTestBase,
      public ::testing::WithParamInterface<
          std::tuple<ConnectionParam, ConnectionParam::Protocol>> {
 protected:
  ConnectionParam conn_param() const { return std::get<0>(GetParam()); }

  ConnectionParam::Protocol protocol() const { return std::get<1>(GetParam()); }

  auto router_host() const { return shared_router()->host(); }

  auto router_port() const {
    return shared_router()->port(conn_param(), protocol());
  }
};

TEST_P(RouterRequireTest, client_ssl_ca_no_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_none();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "CR5");
    RecordProperty(
        "Requirement",
        "If a certificate is received from the the client, Router verify the "
        "client's certificate against `client_ssl_ca`, `client_ssl_capath`, "
        "`client_ssl_crl` and `client_ssl_crlpath`");
    RecordProperty("Description", "no cert");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(router_host(), router_port()));
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels may fail if password isn't cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, client_ssl_ca_good_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_none();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "CR5");
    RecordProperty(
        "Requirement",
        "If a certificate is received from the the client, Router verify the "
        "client's certificate against `client_ssl_ca`, `client_ssl_capath`, "
        "`client_ssl_crl` and `client_ssl_crlpath`");
    RecordProperty("Description", "valid client-cert");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));

    ASSERT_NO_ERROR(cli.connect(router_host(), router_port()));
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels may fail if password isn't cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, client_ssl_ca_bad_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_none();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "CR5");
    RecordProperty(
        "Requirement",
        "If a certificate is received from the the client, Router verify the "
        "client's certificate against `client_ssl_ca`, `client_ssl_capath`, "
        "`client_ssl_crl` and `client_ssl_crlpath`");
    RecordProperty("Description", "bad cert");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/client-cert-verify-san.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/client-key-verify-san.pem"));
    auto connect_res = cli.connect(router_host(), router_port());
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_NO_ERROR(connect_res);
    } else {
      ASSERT_ERROR(connect_res);
      // TLSv1.3:
      // Lost connection to MySQL server at 'reading authorization packet',
      // system error: 71
      //
      // TLSv1.2
      // connecting to destination failed with TLS error:
      // error:14094418:SSL routines:ssl3_read_bytes:tlsv1 alert unknown ca
      EXPECT_THAT(connect_res.error().value(), testing::AnyOf(2013, 2026))
          << connect_res.error();
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels may fail if password isn't cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, client_ssl_ca_revoked_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_none();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "CR5");
    RecordProperty(
        "Requirement",
        "If a certificate is received from the the client, Router verify the "
        "client's certificate against `client_ssl_ca`, `client_ssl_capath`, "
        "`client_ssl_crl` and `client_ssl_crlpath`");
    RecordProperty("Description", "revoked cert");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-revoked-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-revoked-key.pem"));
    auto connect_res = cli.connect(router_host(), router_port());
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_NO_ERROR(connect_res);
    } else {
      ASSERT_ERROR(connect_res);
      // TLSv1.3:
      // Lost connection to MySQL server at 'reading authorization packet',
      // system error: 71
      //
      // TLSv1.2
      // connecting to destination failed with TLS error:
      // error:14094418:SSL routines:ssl3_read_bytes:tlsv1 alert unknown ca
      EXPECT_THAT(connect_res.error().value(), testing::AnyOf(2013, 2026))
          << connect_res.error();
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels may fail if password isn't cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, server_requires_none) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_none();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR6");
    RecordProperty(
        "Requirement",
        "If the currently authenticate use no attributes set, Router MUST "
        "assume none of the above requirements shall be enforced.");
    RecordProperty("Description", "CREATE USER ... REQUIRES NONE");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(router_host(), router_port()));
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels may fail if password isn't cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, server_requires_ssl) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_ssl_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR6");
    RecordProperty(
        "Requirement",
        "If the currently authenticate use no attributes set, Router MUST "
        "assume none of the above requirements shall be enforced.");
    RecordProperty("Description", "CREATE USER ... REQUIRES SSL");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled: SSL is required between router and server.
    if (conn_param().server_ssl_mode == kDisabled ||
        (conn_param().client_ssl_mode == kDisabled &&
         conn_param().server_ssl_mode == kAsClient)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kDisabled) {
      // if account isn't cached, DISABLED may fail as router has no public-key
      // for the client.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");
    if (conn_param().client_ssl_mode != kDisabled &&
        conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over DISABLED fail auth.
      ASSERT_EQ(xerr.error(), 1251) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled ||
               conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, server_requires_x509) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_x509_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR6");
    RecordProperty(
        "Requirement",
        "If the currently authenticate use no attributes set, Router MUST "
        "assume none of the above requirements shall be enforced.");
    RecordProperty("Description", "CREATE USER ... REQUIRES X509");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled:    SSL is required between router and server.
    // passthrough: client sent no cert.
    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kPassthrough ||
        (conn_param().client_ssl_mode == kDisabled &&
         conn_param().server_ssl_mode == kAsClient)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kDisabled) {
      // if account isn't cached, DISABLED may fail as router has no public-key
      // for the client.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kPassthrough) {
      // auth failed as no cert.
      ASSERT_EQ(xerr.error(), 1045) << xerr.what();
    } else if (conn_param().client_ssl_mode != kDisabled &&
               conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over DISABLED fail auth.
      ASSERT_EQ(xerr.error(), 1251) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled ||
               conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, server_requires_x509_subject) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_x509_subject_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR6");
    RecordProperty(
        "Requirement",
        "If the currently authenticate use no attributes set, Router MUST "
        "assume none of the above requirements shall be enforced.");
    RecordProperty("Description", "CREATE USER ... REQUIRES SUBJECT");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled:    SSL is required between router and server.
    // passthrough: client sent no cert.
    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kPassthrough ||
        (conn_param().client_ssl_mode == kDisabled &&
         conn_param().server_ssl_mode == kAsClient)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kDisabled) {
      // with DISABLE__REQUIRED it may fail as the account isn't cached yet and
      // DISABLED doesn't has no way to send the router's public-key:
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kPassthrough) {
      // auth failed as no cert.
      ASSERT_EQ(xerr.error(), 1045) << xerr.what();
    } else if (conn_param().client_ssl_mode != kDisabled &&
               conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over DISABLED fail auth.
      ASSERT_EQ(xerr.error(), 1251) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled ||
               conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, server_requires_x509_issuer) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::server_requires_x509_issuer_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR6");
    RecordProperty(
        "Requirement",
        "If the currently authenticate use no attributes set, Router MUST "
        "assume none of the above requirements shall be enforced.");
    RecordProperty("Description", "CREATE USER ... REQUIRES ISSUER ...");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled:    SSL is required between router and server.
    // passthrough: client sent no cert.
    if (conn_param().server_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kPassthrough ||
        (conn_param().client_ssl_mode == kDisabled &&
         conn_param().server_ssl_mode == kAsClient)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kDisabled) {
      // if account isn't cached, DISABLED may fail as router has no public-key
      // for the client.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kPassthrough) {
      // auth failed as no cert.
      ASSERT_EQ(xerr.error(), 1045) << xerr.what();
    } else if (conn_param().client_ssl_mode != kDisabled &&
               conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over DISABLED fail auth.
      ASSERT_EQ(xerr.error(), 1251) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled ||
               conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_ssl_false) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_ssl_false_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR8");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"ssl": false}}')");

    // router_require_enforce: 1
    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());
    if (conn_param().client_ssl_mode == kDisabled) {
      // if account isn't cached, DISABLED may fail as router has no public-key
      // for the client.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_ssl_true) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_ssl_true_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR2");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"ssl": true}}')");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled:    SSL is required between client and router.
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_true_no_client_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_true_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR3");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"x509": true}}' )"
        "without client cert fails");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled: SSL is required between client and router.
    // required: X509 cert required from client
    if (conn_param().client_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kPreferred ||
        conn_param().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_false_no_client_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty("Description",
                 "no client-cert, router_require: x509:false, "
                 "router-cert: set, server require: not set");

  auto account = TestEnv::router_requires_x509_false_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR8");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"x509": false}}' )"
        "without client cert succeeds");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    if (conn_param().client_ssl_mode == kDisabled) {
      // DISABLED may fail to auth as Router has no public-key.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_true_with_client_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_true_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR3");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"x509": true}}' )"
        "with valid client cert succeeds");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled: SSL is required between client and router.
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_issuer_with_wrong_ca) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_issuer_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR5");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"router_require": {"issuer": "..."}}' )"
        "with client not matching the issuer fails");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);

    // with tlsv1.3, the client lib returns the 2013 lost-connection.
    cli.set_option(MysqlClient::TlsVersion("tlsv1.2"));
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/client-cert-verify-san.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/client-key-verify-san.pem"));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled: SSL is required between client and router.
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kRequired ||
               conn_param().client_ssl_mode == kPreferred ||
               conn_param().client_ssl_mode == kPassthrough) {
      // 2026: SSL connection error:
      // error:0A000418:SSL routines::tlsv1 alert unknown ca
      EXPECT_EQ(connect_res.error().value(), 2026) << connect_res.error();
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_issuer_with_server_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_issuer_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-server-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-server-key.pem"));

    auto connect_res = cli.connect(router_host(), router_port());

    // disabled: SSL is required between client and router.
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else {
      RecordProperty("RequirementId", "RR5");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"issuer": "..."}}' )"
          "with client matching the issuer succeeds");

      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_subject_with_client_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_subject_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));

    auto connect_res = cli.connect(router_host(), router_port());

    if (conn_param().client_ssl_mode == kDisabled) {
      // disabled: SSL is required between client and router.
      RecordProperty("RequirementId", "RR2");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"subject": "..."}}' )"
          "with TLS fails");
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kPassthrough) {
      ASSERT_NO_ERROR(connect_res);
    } else {
      RecordProperty("RequirementId", "RR4");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"subject": "..."}}' )"
          "with client not matching the issuer fails");
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_x509_subject_with_wrong_cert) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::router_requires_x509_subject_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    cli.set_option(
        MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-server-cert.pem"));
    cli.set_option(
        MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-server-key.pem"));

    auto connect_res = cli.connect(router_host(), router_port());

    if (conn_param().client_ssl_mode == kDisabled) {
      // disabled: SSL is required between client and router.
      RecordProperty("RequirementId", "RR4");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"subject": "..."}}' )"
          "without TLS fails");

      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kPreferred ||
               conn_param().client_ssl_mode == kRequired) {
      // required: wrong cert
      RecordProperty("RequirementId", "RR4");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"subject": "..."}}' )"
          "with client not matching the issuer fails");

      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else if (conn_param().client_ssl_mode == kPassthrough) {
      // passthrough: router_required_enforce: 0
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_unknown_attribute) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "a unknown attribute in router_require fails auth for this user");

  auto account = TestEnv::router_requires_unknown_attribute_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());

    if (conn_param().client_ssl_mode == kDisabled ||
        conn_param().client_ssl_mode == kPreferred ||
        conn_param().client_ssl_mode == kRequired) {
      // unknown attribute
      RecordProperty("RequirementId", "RR9");
      RecordProperty(
          "Description",
          R"(CREATE USER ... ATTRIBUTE '{"router_require": {"unknown_attribute": "..."}}' )"
          "fails auth");
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    } else {
      // passthrough: router_required_enforce: 0
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, router_requires_other_attribute) {
  RecordProperty("Worklog", "14304");

  auto account = TestEnv::other_attribute_account();

  if (protocol() == ConnectionParam::Protocol::Classic) {
    RecordProperty("RequirementId", "RR8");
    RecordProperty(
        "Description",
        R"(CREATE USER ... ATTRIBUTE '{"unknown_attribute": "..."}' )"
        "succeeds auth");

    MysqlClient cli;
    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    auto connect_res = cli.connect(router_host(), router_port());
    if (conn_param().client_ssl_mode == kDisabled) {
      // if account isn't cached, DISABLED may fail as router has no public-key
      // for the client.
      if (!connect_res) {
        EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      }
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  } else {
    // router_require_enforce: 0
    auto sess = xcl::create_session();

    auto xerr =
        sess->connect(router_host(), router_port(), account.username.c_str(),
                      account.password.c_str(), "");

    if (conn_param().client_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 2510)) << xerr.what();
    } else if (conn_param().server_ssl_mode == kDisabled) {
      // PLAIN over insecure channels fails if the login hasn't been cached.
      ASSERT_THAT(xerr.error(), testing::AnyOf(0, 1251)) << xerr.what();
    } else {
      ASSERT_EQ(xerr.error(), 0) << xerr.what();
    }
  }
}

TEST_P(RouterRequireTest, change_user_to_ssl_false) {
  if (protocol() != ConnectionParam::Protocol::Classic) {
    return;
  }
  RecordProperty("Worklog", "14304");
  RecordProperty("Description",
                 "COM_CHANGE_USER to a user which `router_requires` nothing. "
                 "The initial connection was done with/without TLS.");

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to same user");
  {
    auto account = TestEnv::router_requires_ssl_false_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    ASSERT_NO_ERROR(change_user_res);
  }
}

TEST_P(RouterRequireTest, change_user_to_ssl_true) {
  if (protocol() != ConnectionParam::Protocol::Classic) {
    return;
  }
  RecordProperty("Worklog", "14304");
  RecordProperty("Description",
                 "COM_CHANGE_USER to a user which `router_requires` a TLS. "
                 "The initial connection was done with/without TLS.");

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to ssl:true");
  {
    auto account = TestEnv::router_requires_ssl_true_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(change_user_res);
      EXPECT_EQ(change_user_res.error().value(), 1045);
    } else {
      ASSERT_NO_ERROR(change_user_res);
    }
  }
}

TEST_P(RouterRequireTest, change_user_to_x509_true_without_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "COM_CHANGE_USER to a user which `router_requires` a x509 certificate. "
      "The initial connection was done without cert.");

  if (protocol() != ConnectionParam::Protocol::Classic ||
      conn_param().client_ssl_mode == kPassthrough) {
    return;
  }

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to x509:true");
  {
    auto account = TestEnv::router_requires_x509_true_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    ASSERT_ERROR(change_user_res);
    EXPECT_EQ(change_user_res.error().value(), 1045);
  }
}

TEST_P(RouterRequireTest, change_user_to_x509_true_with_good_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "COM_CHANGE_USER to a user which `router_requires` a x509 certificate. "
      "The initial connection was done with cert.");

  if (protocol() != ConnectionParam::Protocol::Classic ||
      conn_param().client_ssl_mode == kPassthrough) {
    return;
  }

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(
      MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
  cli.set_option(MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to x509:true");
  {
    auto account = TestEnv::router_requires_x509_true_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(change_user_res);
      EXPECT_EQ(change_user_res.error().value(), 1045);
    } else {
      ASSERT_NO_ERROR(change_user_res);
    }
  }
}

TEST_P(RouterRequireTest, change_user_to_x590_issuer) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "COM_CHANGE_USER to a user which `router_requires` a x509 subject. "
      "The initial connection was done with cert whose issuer matches.");

  if (protocol() != ConnectionParam::Protocol::Classic ||
      conn_param().client_ssl_mode == kPassthrough) {
    return;
  }

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(
      MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
  cli.set_option(MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to x509:issuer");
  {
    auto account = TestEnv::router_requires_x509_issuer_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(change_user_res);
      EXPECT_EQ(change_user_res.error().value(), 1045);

      auto ping_res = cli.ping();
      ASSERT_ERROR(ping_res);
    } else {
      ASSERT_NO_ERROR(change_user_res);

      auto ping_res = cli.ping();
      ASSERT_NO_ERROR(ping_res);
    }
  }
}

TEST_P(RouterRequireTest, change_user_to_x509_subject) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "COM_CHANGE_USER to a user which `router_requires` a x509 subject. "
      "The initial connection was done with cert whose subject matches.");

  if (protocol() != ConnectionParam::Protocol::Classic ||
      conn_param().client_ssl_mode == kPassthrough) {
    return;
  }

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(
      MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-client-cert.pem"));
  cli.set_option(MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-client-key.pem"));
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to x509:subject");
  {
    auto account = TestEnv::router_requires_x509_subject_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    if (conn_param().client_ssl_mode == kDisabled) {
      ASSERT_ERROR(change_user_res);
      EXPECT_EQ(change_user_res.error().value(), 1045);
    } else {
      ASSERT_NO_ERROR(change_user_res);
    }
  }
}

TEST_P(RouterRequireTest, change_user_to_x509_subject_with_wrong_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty(
      "Description",
      "COM_CHANGE_USER to a user which `router_requires` a x509 subject, "
      "but the initial connection was with a cert whose subject doesn't match");

  if (protocol() != ConnectionParam::Protocol::Classic ||
      conn_param().client_ssl_mode == kPassthrough) {
    return;
  }

  MysqlClient cli;
  {
    auto account = TestEnv::router_requires_ssl_false_account();
    cli.username(account.username);
    cli.password(account.password);
  }
  cli.set_option(
      MysqlClient::SslCert(SSL_TEST_DATA_DIR "/crl-server-cert.pem"));
  cli.set_option(MysqlClient::SslKey(SSL_TEST_DATA_DIR "/crl-server-key.pem"));
  cli.set_option(MysqlClient::GetServerPublicKey(true));

  auto connect_res = cli.connect(router_host(), router_port());
  ASSERT_NO_ERROR(connect_res);

  SCOPED_TRACE("// change to x509:subject");
  {
    auto account = TestEnv::router_requires_x509_subject_account();

    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    ASSERT_ERROR(change_user_res);
    EXPECT_EQ(change_user_res.error().value(), 1045);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RouterRequireTest,
    ::testing::Combine(::testing::ValuesIn(connection_params),
                       ::testing::Values(ConnectionParam::Protocol::Classic,
                                         ConnectionParam::Protocol::X)),
    [](auto &info) {
      return "via_"s + std::string(std::get<0>(info.param).testname) +
             (std::get<1>(info.param) == ConnectionParam::Protocol::Classic
                  ? "_classic"s
                  : "_x"s);
    });

class RouterRequireConnectionPoolTest : public RouterComponentTest {
 public:
  ProcessManager &process_manager() { return *this; }

  static std::vector<std::string> classic_destinations_from_shared_servers(
      std::span<SharedServer *const> servers) {
    std::vector<std::string> dests;
    dests.reserve(servers.size());

    for (const auto &s : servers) {
      dests.push_back(s->server_host() + ":" +
                      std::to_string(s->server_port()));
    }

    return dests;
  }

  ProcessWrapper &spawn_router(std::span<const std::string> destinations) {
    auto userfile = conf_dir_.file("userfile");
    {
      std::ofstream ofs(userfile);
      // user:pass
      ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
             "YJgciRvb69";
    }

    auto writer = process_manager().config_writer(conf_dir_.name());

    writer.section("connection_pool", {
                                          {"max_idle_server_connections", "1"},
                                      });

    for (auto [ndx, param] :
         stdx::views::enumerate(std::array<ConnectionParam, 4>({
             ConnectionParam{kPreferredPreferred, kPreferred, kPreferred},
             ConnectionParam{kPreferredPreferredNoSslCert, kPreferred,
                             kPreferred},
             ConnectionParam{kPreferredPreferredWrongSslCert, kPreferred,
                             kPreferred},
             ConnectionParam{kPreferredPreferredNotVerifiedSslCert, kPreferred,
                             kPreferred},
         }))) {
      auto port_key = param.testname;
      auto ports_it = ports_.find(port_key);

      const auto port =
          ports_it == ports_.end()
              ? (ports_[port_key] = port_pool_.get_next_available())
              : ports_it->second;

      std::map<std::string, std::string> options{
          {"bind_port", std::to_string(port)},
          {"destinations", mysql_harness::join(destinations, ",")},
          {"protocol", "classic"},
          {"routing_strategy", "round-robin"},

          {"client_ssl_mode", std::string(param.client_ssl_mode)},
          {"server_ssl_mode", std::string(param.server_ssl_mode)},

          {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
          {"connection_sharing", "0"},     //
          {"connect_retry_timeout", "0"},  //
      };

#if !defined(_WIN32)
      options.emplace("socket", socket_path(param));
#endif

      options.emplace("router_require_enforce", "1");
      options.emplace("client_ssl_ca", SSL_TEST_DATA_DIR "/crl-ca-cert.pem");
      options.emplace("client_ssl_crl",
                      SSL_TEST_DATA_DIR "/crl-client-revoked.crl");

      if (ndx == 0) {
        // only the first has ssl-cert between client and server.
        options.emplace("server_ssl_key",
                        SSL_TEST_DATA_DIR "/crl-client-key.pem");
        options.emplace("server_ssl_cert",
                        SSL_TEST_DATA_DIR "/crl-client-cert.pem");
      } else if (ndx == 2) {
        // unexpected client-cert: signed by unknown CA
        options.emplace("server_ssl_key",
                        SSL_TEST_DATA_DIR "/client-key-verify-san.pem");
        options.emplace("server_ssl_cert",
                        SSL_TEST_DATA_DIR "/client-cert-verify-san.pem");
      } else if (ndx == 3) {
        options.emplace("server_ssl_key",
                        SSL_TEST_DATA_DIR "/crl-server-key.pem");
        options.emplace("server_ssl_cert",
                        SSL_TEST_DATA_DIR "/crl-server-cert.pem");
      }

      writer.section("routing:classic_" + std::string(param.testname), options);
    }

    auto bindir = process_manager().get_origin();
    auto builddir = bindir.join("..");

    auto &proc =
        process_manager()
            .spawner(bindir.join("mysqlrouter").str())
            .with_core_dump(true)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
            .spawn({"-c", writer.write()});

    proc.set_logging_path(process_manager().get_logging_dir().str(),
                          "mysqlrouter.log");

    return proc;
  }

  [[nodiscard]] auto host() const { return "127.0.0.1"; }

  [[nodiscard]] uint16_t port(std::string_view testname) const {
    auto it = ports_.find(testname);
    if (it == ports_.end()) {
      for (auto [k, v] : ports_) {
        std::cerr << k << " -> " << v << "\n";
      }
      throw std::runtime_error("port-key not found: " + std::string(testname));
    }

    return it->second;
  }

  [[nodiscard]] std::string socket_path(const ConnectionParam &param) const {
    return Path(conf_dir_.name())
        .join("classic_" + std::string(param.testname) + ".sock")
        .str();
  }

  static constexpr const std::string_view kPreferredPreferred{
      "PREFERRED__PREFERRED"};
  static constexpr const std::string_view kPreferredPreferredNoSslCert{
      "PREFERRED__PREFERRED_no_ssl_cert"};
  static constexpr const std::string_view kPreferredPreferredWrongSslCert{
      "PREFERRED__PREFERRED_wrong_ssl_cert"};
  static constexpr const std::string_view kPreferredPreferredNotVerifiedSslCert{
      "PREFERRED__PREFERRED_not_verified_ssl_cert"};

 private:
  TempDirectory conf_dir_;

  std::map<std::string_view, uint16_t, std::less<>> ports_;
};

TEST_F(RouterRequireConnectionPoolTest, connection_pool_no_cert_and_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty("RequirementId", "SR5");
  RecordProperty(
      "Requirement",
      "If the router takes the server connection from the connection pool, it "
      "MUST ensure that the pooled connection used a client-certificate that "
      "mathed the route's `server_ssl_cert` setting");

  // start router.
  auto &proc = spawn_router(
      classic_destinations_from_shared_servers(test_env->servers()));
  ASSERT_TRUE(proc.wait_for_sync_point_result());

  // add a connection to the pool
  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_ssl_account();
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(host(), port(kPreferredPreferredNoSslCert)));
  };

  SCOPED_TRACE("// wait until connection is in the pool.");

  std::this_thread::sleep_for(100ms);

  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_account();
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(host(), port(kPreferredPreferred)));
  };
}

TEST_F(RouterRequireConnectionPoolTest, connection_pool_cert_and_cert) {
  RecordProperty("Worklog", "14304");
  RecordProperty("RequirementId", "SR5");
  RecordProperty(
      "Requirement",
      "If the router takes the server connection from the connection pool, it "
      "MUST ensure that the pooled connection used a client-certificate that "
      "mathed the route's `server_ssl_cert` setting");

  // start router.
  auto &proc = spawn_router(
      classic_destinations_from_shared_servers(test_env->servers()));
  ASSERT_TRUE(proc.wait_for_sync_point_result());

  // add a connection to the pool
  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_account();
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(host(), port(kPreferredPreferred)));
  };

  SCOPED_TRACE("// wait until connection is in the pool.");

  std::this_thread::sleep_for(100ms);

  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_account();
    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(host(), port(kPreferredPreferred)));
  };
}

TEST_F(RouterRequireConnectionPoolTest, no_server_cert_with_cert_required) {
  RecordProperty("Worklog", "14304");
  RecordProperty("RequirementId", "SR4");

  // start router.
  auto &proc = spawn_router(
      classic_destinations_from_shared_servers(test_env->servers()));
  ASSERT_TRUE(proc.wait_for_sync_point_result());

  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_account();
    cli.username(account.username);
    cli.password(account.password);

    auto connect_res = cli.connect(host(), port(kPreferredPreferredNoSslCert));
    ASSERT_ERROR(connect_res);
    // Access denied for user 'server_requires_x509'@'localhost' (using
    // password: YES)
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
  };
}

TEST_F(RouterRequireConnectionPoolTest,
       wrong_server_cert_with_cert_required_fails) {
  RecordProperty("Worklog", "14304");
  RecordProperty("RequirementId", "SR4");
  RecordProperty(
      "Description",
      "using `server_ssl_cert` that doesn't match the `ssl_ca` of the server "
      "fails the user's auth.");

  // start router.
  auto &proc = spawn_router(
      classic_destinations_from_shared_servers(test_env->servers()));
  ASSERT_TRUE(proc.wait_for_sync_point_result());

  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_issuer_account();
    cli.username(account.username);
    cli.password(account.password);

    auto connect_res =
        cli.connect(host(), port(kPreferredPreferredWrongSslCert));
    ASSERT_ERROR(connect_res);
    // TLSv1.3:
    // Lost connection to MySQL server at 'reading authorization packet',
    // system error: 71
    //
    // TLSv1.2
    // connecting to destination failed with TLS error:
    // error:14094418:SSL routines:ssl3_read_bytes:tlsv1 alert unknown ca
    EXPECT_THAT(connect_res.error().value(), testing::AnyOf(2013, 2026))
        << connect_res.error();
  };
}

TEST_F(RouterRequireConnectionPoolTest,
       not_verified_server_cert_with_cert_required) {
  RecordProperty("Worklog", "14304");
  RecordProperty("RequirementId", "SR4");
  RecordProperty("Description",
                 "using server_ssl_cert that doesn't match the CREATE USER ... "
                 "REQUIRE ISSUER fails the user's auth.");

  // start router.
  auto &proc = spawn_router(
      classic_destinations_from_shared_servers(test_env->servers()));
  ASSERT_TRUE(proc.wait_for_sync_point_result());

  {
    MysqlClient cli;

    auto account = TestEnv::server_requires_x509_subject_account();
    cli.username(account.username);
    cli.password(account.password);

    auto connect_res =
        cli.connect(host(), port(kPreferredPreferredNotVerifiedSslCert));
    ASSERT_ERROR(connect_res);
    // Access denied for user 'server_requires_x509_subject'@'localhost' (using
    // password: YES)
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
  };
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  TlsLibraryContext tls_lib_ctx;

  // env is owned by googletest
  test_env =
      dynamic_cast<TestEnv *>(::testing::AddGlobalTestEnvironment(new TestEnv));

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
