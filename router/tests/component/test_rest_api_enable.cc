/*
 Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include <gtest/gtest-matchers.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/string_utils.h"  // join
#include "mysql/harness/tls_client_context.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/http_client.h"
#include "mysqlrouter/utils.h"
#include "process_wrapper.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

using mysql_harness::utility::string_format;

class TestRestApiEnable : public RouterComponentBootstrapTest {
 public:
  void SetUp() override {
    RouterComponentBootstrapTest::SetUp();

    cluster_node_port = port_pool_.get_next_available();
    cluster_http_port = port_pool_.get_next_available();

    SCOPED_TRACE("// Launch a server mock that will act as our cluster member");
    const auto trace_file = get_data_dir().join("rest_api_enable.js").str();

    ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_node_port, EXIT_SUCCESS, false, cluster_http_port);

    set_globals();
    set_router_accepting_ports();

    custom_port = port_pool_.get_next_available();

    setup_paths();
  }

  ProcessWrapper &do_bootstrap(std::vector<std::string> additional_config,
                               bool will_run_with_created_config = true) {
    std::vector<std::string> cmdline = {
        "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
        "-d",
        temp_test_dir.name(),
        "--conf-set-option=logger.level=DEBUG",
    };

    if (will_run_with_created_config) {
      // since we are launching the Router after the bootstrap we
      // can't allow default ports to be used
      cmdline.insert(cmdline.end(),
                     {"--conf-set-option=routing:bootstrap_rw.bind_port=" +
                          std::to_string(router_port_rw),
                      "--conf-set-option=routing:bootstrap_ro.bind_port=" +
                          std::to_string(router_port_ro),
                      "--conf-set-option=routing:bootstrap_x_rw.bind_port=" +
                          std::to_string(router_port_x_rw),
                      "--conf-set-option=routing:bootstrap_x_ro.bind_port=" +
                          std::to_string(router_port_x_ro)});

      // Let's overwrite the default bind_address to prevent the MacOS firewall
      // from complaining
      cmdline.insert(
          cmdline.end(),
          {"--conf-set-option=DEFAULT.bind_address=127.0.0.1",
           "--conf-set-option=routing:bootstrap_rw.bind_address=127.0.0.1",
           "--conf-set-option=routing:bootstrap_ro.bind_address=127.0.0.1",
           "--conf-set-option=routing:bootstrap_x_rw.bind_address=127.0.0.1",
           "--conf-set-option=routing:bootstrap_x_ro.bind_address=127.0.0.1"});

      if (std::find(additional_config.begin(), additional_config.end(),
                    "--disable-rw-split") == additional_config.end()) {
        // if --disable-rw-split isn't set, set the bind-port and bind-address
        cmdline.insert(
            cmdline.end(),
            {"--conf-set-option=routing:bootstrap_rw_split.bind_port=" +
                 std::to_string(router_port_rw_split),
             "--conf-set-option=routing:bootstrap_rw_split.bind_address="
             "127.0.0.1"});
      }
    }

    std::move(std::begin(additional_config), std::end(additional_config),
              std::back_inserter(cmdline));
    auto &router_bootstrap = launch_router_for_bootstrap(
        cmdline, EXIT_SUCCESS, /*disable_rest=*/false);

    check_exit_code(router_bootstrap, EXIT_SUCCESS);

    EXPECT_TRUE(router_bootstrap.expect_output(
        "MySQL Router configured for the InnoDB Cluster 'mycluster'"));

    return router_bootstrap;
  }

  void assert_rest_config(const mysql_harness::Path &config_path,
                          const bool is_enabled) const {
    auto content = get_file_output(config_path.str());

    std::string rest_api_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[rest_api]",
        },
        "\n");

    std::string http_server_section = mysql_harness::join(
        std::initializer_list<const char *>{
            R"(\[http_server\])",
            R"(port=.+)",
            R"(ssl=1)",
            R"(ssl_cert=.*)",
            R"(ssl_key=.*)",
        },
        "\n");

    std::string http_auth_backend_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[http_auth_backend:default_auth_backend]",
            "backend=metadata_cache",
        },
        "\n");

    std::string http_auth_realm_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[http_auth_realm:default_auth_realm]",
            "backend=default_auth_backend",
            "method=basic",
            "name=default_realm",
        },
        "\n");

    std::string rest_router_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[rest_router]",
            "require_realm=default_auth_realm",
        },
        "\n");

    std::string rest_routing_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[rest_routing]",
            "require_realm=default_auth_realm",
        },
        "\n");

    std::string rest_metadata_cache_section = mysql_harness::join(
        std::initializer_list<const char *>{
            "[rest_metadata_cache]",
            "require_realm=default_auth_realm",
        },
        "\n");

    if (is_enabled) {
      EXPECT_THAT(content, ::testing::AllOf(
                               ::testing::HasSubstr(rest_api_section),
                               ::testing::ContainsRegex(http_server_section),
                               ::testing::HasSubstr(http_auth_backend_section),
                               ::testing::HasSubstr(http_auth_realm_section),
                               ::testing::HasSubstr(rest_router_section),
                               ::testing::HasSubstr(rest_routing_section),
                               ::testing::HasSubstr(rest_metadata_cache_section)

                                   ));
    } else {
      EXPECT_THAT(content,
                  ::testing::Not(::testing::AnyOf(
                      ::testing::HasSubstr(rest_api_section),
                      ::testing::ContainsRegex(http_server_section),
                      ::testing::HasSubstr(http_auth_backend_section),
                      ::testing::HasSubstr(http_auth_realm_section),
                      ::testing::HasSubstr(rest_router_section),
                      ::testing::HasSubstr(rest_routing_section),
                      ::testing::HasSubstr(rest_metadata_cache_section))));
    }
  }

  enum class CertFile { k_ca_key, k_ca_cert, k_router_key, k_router_cert };

  void create_cert_files(const std::vector<CertFile> &files) {
    mysql_harness::mkdir(datadir_path.str().c_str(), 0755);

    for (const auto &cert : files) {
      std::ofstream cert_stream{
          datadir_path.join(cert_filenames.at(cert)).str()};
      ASSERT_TRUE(cert_stream);
      cert_stream << expected_cert_contents.at(cert);
    }
  }

  std::string read_cert(CertFile cert) const {
    return get_file_output(datadir_path.join(cert_filenames.at(cert)).str());
  }

  bool certificate_files_not_modified(
      const std::vector<CertFile> &user_cert_files) const {
    return std::all_of(std::begin(user_cert_files), std::end(user_cert_files),
                       [this](const CertFile &cert) {
                         return read_cert(cert) ==
                                expected_cert_contents.at(cert);
                       });
  }

  bool certificate_files_exists(const std::vector<CertFile> &cert_files) const {
    return std::all_of(
        std::begin(cert_files), std::end(cert_files),
        [this](const CertFile &cert) {
          return mysql_harness::Path{mysql_harness::Path(temp_test_dir.name())
                                         .join("data")
                                         .join(cert_filenames.at(cert))}
              .exists();
        });
  }

  bool certificate_files_not_changed(
      const std::vector<CertFile> &user_cert_files) const {
    // Check if there are no certificate files that were not add by the user
    for (const auto &cert : {CertFile::k_ca_key, CertFile::k_ca_cert,
                             CertFile::k_router_key, CertFile::k_router_cert}) {
      if (std::find(std::begin(user_cert_files), std::end(user_cert_files),
                    cert) == std::end(user_cert_files) &&
          certificate_files_exists({cert})) {
        return false;
      }
    }

    return certificate_files_not_modified(user_cert_files);
  }

  void assert_rest_works(const uint16_t port) {
    const auto uri = "https://" + gr_member_ip + ":" + std::to_string(port) +
                     rest_api_basepath + "/router/status";

    const auto ca_file =
        datadir_path.join(cert_filenames.at(CertFile::k_ca_cert));
    // Try to verify router certificate using a CA certificate only if the
    // latter exists.
    TlsVerify mode = ca_file.exists() ? TlsVerify::PEER : TlsVerify::NONE;

    TlsClientContext tls_ctx(mode);
    tls_ctx.ssl_ca(ca_file.str(), "");

    assert_certificate_common_name(
        CertFile::k_router_cert,
        "CN=MySQL_Router_Auto_Generated_Router_Certificate");
    if (ca_file.exists()) {
      assert_certificate_common_name(
          CertFile::k_ca_cert, "CN=MySQL_Router_Auto_Generated_CA_Certificate");
    }

    IOContext io_ctx;
    RestClient rest_client(io_ctx, std::move(tls_ctx));

    JsonDocument json_doc;
    // We do not care to authenticate, just check if we got a response
    ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                         HttpStatusCode::Unauthorized, json_doc,
                                         kContentTypeHtmlCharset));
  }

  void verify_bootstrap_at_custom_path(const mysql_harness::Path &path) {
    std::vector<std::string> cmdline = {
        "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
        "-d", path.str()};
    auto &router_bootstrap = launch_router_for_bootstrap(
        cmdline, EXIT_SUCCESS, /*disable_rest=*/false);

    check_exit_code(router_bootstrap, EXIT_SUCCESS);

    auto custom_config_path = path.join("mysqlrouter.conf");
    assert_rest_config(custom_config_path, true);

    std::vector<CertFile> cert_files{CertFile::k_ca_key, CertFile::k_ca_cert,
                                     CertFile::k_router_key,
                                     CertFile::k_router_cert};
    EXPECT_TRUE(
        std::all_of(std::begin(cert_files), std::end(cert_files),
                    [this, &path](const CertFile &cert) {
                      return mysql_harness::Path{
                          path.join("data").join(cert_filenames.at(cert))}
                          .exists();
                    }));
  }

  void assert_certificate_common_name(CertFile cert,
                                      const std::string &CN) const {
    auto cert_path =
        mysql_harness::Path{temp_test_dir.name()}.join("data").join(
            cert_filenames.at(cert));
    ASSERT_TRUE(cert_path.exists());
    ASSERT_EQ(CN, get_CN_from_certificate(cert_path.str()));
  }

  std::string get_CN_from_certificate(std::string cert_filename) const {
    using BIO_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;
    using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;

    BIO_ptr input(BIO_new(BIO_s_file()), BIO_free);
    BIO_read_filename(input.get(), const_cast<char *>(cert_filename.c_str()));

    X509_ptr cert(PEM_read_bio_X509_AUX(input.get(), nullptr, nullptr, nullptr),
                  X509_free);

    X509_NAME *subject = X509_get_subject_name(cert.get());
    BIO_ptr output_bio(BIO_new(BIO_s_mem()), BIO_free);
    X509_NAME_print_ex(output_bio.get(), subject, 0, 0);

    std::string result;
    const auto length = BIO_pending(output_bio.get());
    result.resize(length);
    BIO_read(output_bio.get(), &result[0], length);
    return result;
  }

  ProcessWrapper &launch_router(
      const std::vector<std::string> &params, int expected_exit_code /*= 0*/,
      std::chrono::milliseconds wait_for_notify_ready = -1s,
      ProcessWrapper::OutputResponder output_responder =
          RouterComponentBootstrapTest::kBootstrapOutputResponder) {
    return ProcessManager::launch_router(
        params, expected_exit_code,
        /*catch_stderr*/ true, /*with_sudo*/ false, wait_for_notify_ready,
        output_responder);
  }

  TlsLibraryContext m_tls_lib_ctx;
  const std::string gr_member_ip{"127.0.0.1"};
  const std::string cluster_id{"3a0be5af-0022-11e8-9655-0800279e6a88"};

  uint16_t cluster_node_port;
  uint16_t cluster_http_port;
  uint16_t custom_port;
  uint16_t router_port_rw;
  uint16_t router_port_ro;
  uint16_t router_port_rw_split;
  uint16_t router_port_x_rw;
  uint16_t router_port_x_ro;
  ProcessWrapper *cluster_node;

  TempDirectory temp_test_dir;
  mysql_harness::Path config_path;
  mysql_harness::Path datadir_path;

  static const std::string predefined_ca_key;
  static const std::string predefined_ca_cert;
  static const std::string predefined_router_key;
  static const std::string predefined_router_cert;

  const std::map<CertFile, std::string> expected_cert_contents{
      {CertFile::k_ca_key, predefined_ca_key},
      {CertFile::k_ca_cert, predefined_ca_cert},
      {CertFile::k_router_key, predefined_router_key},
      {CertFile::k_router_cert, predefined_router_cert}};

  const std::map<CertFile, std::string> cert_filenames{
      {CertFile::k_ca_key, "ca-key.pem"},
      {CertFile::k_ca_cert, "ca.pem"},
      {CertFile::k_router_key, "router-key.pem"},
      {CertFile::k_router_cert, "router-cert.pem"}};

 protected:
  void set_globals(std::string cluster_id = "") {
    set_mock_metadata(cluster_http_port, cluster_id,
                      classic_ports_to_gr_nodes({cluster_node_port}), 0,
                      {cluster_node_port}, 0 /*view_id*/,
                      false /*error_on_md_query*/);
  }

  void setup_paths() {
    config_path =
        mysql_harness::Path{temp_test_dir.name()}.join("mysqlrouter.conf");
    datadir_path = mysql_harness::Path{temp_test_dir.name()}.join("data");
  }

  void set_router_accepting_ports() {
    router_port_rw = port_pool_.get_next_available();
    router_port_ro = port_pool_.get_next_available();
    router_port_rw_split = port_pool_.get_next_available();
    router_port_x_rw = port_pool_.get_next_available();
    router_port_x_ro = port_pool_.get_next_available();
  }
};

using cert_file_t = TestRestApiEnable::CertFile;

const std::string TestRestApiEnable::predefined_ca_key{
    R"(-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEA2T3oTODA9W45q241vGKEM9CZMzO2IVKcXjuY8GUun4sKdYob
n7bJfZf9/6rHQpaqiXWiVmKM7Aclw1Sq7pM1VADEq/TTc/aalBYHLzspdjLZNSlg
EB8nQAfEFSVsPEecAomUMv6hfCh8z5pAGpfu4QZDGQO/8S0YxNUEXeMESidMTHJY
QSRc470usq9wa6Y4rVHolRHdWigkz5+L1emTkumUUrwqXPf5D/MLWBHtK9K45txF
z8Fd5vGo1SOqdSw3pEQ5O822/SocFHHlkNYPp/qmEJPf6QVPWc/GM4CHKwx5UXQm
9KxYly6v8e0q1kZ0WL6+ekcrGJuqZeTH1N+o2QIDAQABAoIBAQDJpHPd//Q7Gz++
RsLsBEmPyryY0RPp5EMuGIWCBXj8L9PafAHeAo0N3amuyTbBMRZEFwNCyaDiaFP9
9bXfUpZ6TWg/8DThe3HJqJSsm26FvvbsKGZ5MGF/RnYT5rOLVDCUDl2X48/CbdZD
4HpF9OaOygA31Moxs1k9QjgWaWSO6iqxh1kQI7mbO6X5JV870VEqcSK1gb8hZegG
oHkAFReDPidqaKQXEr3tvnz3D+ckgec3O0M5C9itLP5j2nqekp3YkQDG/WD34X/e
Ghz+/GixrIUZGfAiAQwZCxDtVo/iIYgOCWHySPTNH+kV24wcQA1Y/AZFSM6VJEA0
T2kD/EqdAoGBAPj5cit4pWS1uOnXqGEecrnMrCD+agWBtY5hWNDXaRr4hWalQSO9
lN38OJukTzCYNdWZUNN/zEvfbpF9WyaYTyeTp9KuY8fRVj326r+ADO1YMbWIBfXS
kb5HzAj30j6CBeNLSV+04dgkdhOPqipHKTBL/pfD09tNXlHUYZE3QF5jAoGBAN9f
OuVsgG03CdK6t+gt/mLLx/nvUwVDAO8u6sIC6oHVugfkd49uClWM62GK+lMDCbat
OojHkmKT4TnfkTThMFHQ29t5T0l3EkzOS/yjSlMBqN5UXbb/ik3bohrZ3yvxjRw/
fNRFdLarJwvbPMULg2v3VSOyvQJpETYS/CksJ5KTAoGAcxkWP6R5iXI89tW8wJEL
5nsJBAO5TaxmG1lDbuB2dYJ4YTh6QaSN4oWMQd+WwFdNY96JsAy/jD/RZK736YK1
7Qzko4/9Ds3muaShZ0AyObLw4APvBXJ/7+BPIcI3TrBbOnV+iSEc2wgYEfjzaLIX
B33KR6y/Dv3YYan2JOTO/BMCgYAbPikXvCD5sQHAssclSR7Ce+oa4IZ2mNJvWYCG
QwbI6QE0Xzf5xUj7YCGBFwsqvq8bmYsPDZAb9787aLn0Ahb7k4aNAQGbiysvNOXt
nRi+gPBQlWeMnyQGFOhzb+kZGe/E5zVZSlNOyBcOCiIiQiI4M8Utgmos9hWES9J3
TwxQgwKBgQDxSEZTTebnwQHshKwQ+rK4TtCLBrQna1l0//MRkovT6WdOl6GFaVpP
7xpOMKPGdIp/rsVrBrGymP+X1nVg2/5figLuXBOh35TFftIu2jzhY9e2mcK0yUg/
xBH5Q3lqBr8DL9VPrUdE3e5q0RT2pSxTkuLLlpyfTRLJCaNrbzeunQ==
-----END RSA PRIVATE KEY-----)"};
const std::string TestRestApiEnable::predefined_ca_cert{
    R"(-----BEGIN CERTIFICATE-----
MIIC+DCCAeCgAwIBAgIBATANBgkqhkiG9w0BAQsFADA1MTMwMQYDVQQDDCpNeVNR
TF9Sb3V0ZXJfQXV0b19HZW5lcmF0ZWRfQ0FfQ2VydGlmaWNhdGUwHhcNMjAwMzMx
MTQyOTI4WhcNMzAwMzI5MTQyOTI4WjA1MTMwMQYDVQQDDCpNeVNRTF9Sb3V0ZXJf
QXV0b19HZW5lcmF0ZWRfQ0FfQ2VydGlmaWNhdGUwggEiMA0GCSqGSIb3DQEBAQUA
A4IBDwAwggEKAoIBAQDZPehM4MD1bjmrbjW8YoQz0JkzM7YhUpxeO5jwZS6fiwp1
ihuftsl9l/3/qsdClqqJdaJWYozsByXDVKrukzVUAMSr9NNz9pqUFgcvOyl2Mtk1
KWAQHydAB8QVJWw8R5wCiZQy/qF8KHzPmkAal+7hBkMZA7/xLRjE1QRd4wRKJ0xM
clhBJFzjvS6yr3BrpjitUeiVEd1aKCTPn4vV6ZOS6ZRSvCpc9/kP8wtYEe0r0rjm
3EXPwV3m8ajVI6p1LDekRDk7zbb9KhwUceWQ1g+n+qYQk9/pBU9Zz8YzgIcrDHlR
dCb0rFiXLq/x7SrWRnRYvr56RysYm6pl5MfU36jZAgMBAAGjEzARMA8GA1UdEwEB
/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBABR9C4QO8PA9aQWp9x4oAO4a8J0S
OG9xNaE2naMIH7w9/IV0/aMbGh/uSA1gNgGMoWh3FXLlcNfA+gdBgIjwj92WOWI5
K+2kazRuw4/JA7V4280rsE0pysfMZebyr2QpdVMQj93BUevwdmkLBTj2g1c1b1no
SGCB70NN+WLJ7m8Ug1yI12V+r//zVpsBCQD5GvHaLzgyQiT+uAsZlLGka4PovTvD
vdtg4l1Z7x7KYv3cc93gDQ/Mjzidsz22tFyXF6lWeYDrxDc0PA9BXLwS3HHpgb9p
5uWx33fi5CL8fvEvqQ7NmIf/gc3vBhTA7Mep8c56O53TF2AJyEcF1IB/4Cw=
-----END CERTIFICATE-----)"};
const std::string TestRestApiEnable::predefined_router_key{
    R"(-----BEGIN RSA PRIVATE KEY-----
MIIEogIBAAKCAQEAwIR0W80QVKg91toariO65pB7/aoR67WzIomLmxgTVbcT4qpd
Rxj5kzwVyO0TTcwH9XEjsMUtao0+VByZGGYpxDUrsiyqMBqDpNrzY6PSUDCOPULi
UVMUMzxdtpoiKpvJ35DFYYHZOUCaynIzBKbR/JLFlQZ3GZLJVnKhu3b37hSEnSzB
y/ZKYgFQT45V5ejh80BNmW8zHc8/hEXyes474SsJqyFvmhQbzpjMzPxcbU3+a8VB
7WRomLJG+Gm8SKufPlEjqKESyW4V7fMBv6Qsqry4Z5DJ5GVKIzd6vxoAsRetp5H8
7y86ddwivL1Pv4nya1k8mgtJhRTiP863GmL9JwIDAQABAoIBAChLUONp/1IIyLCw
g8cQ+WyKrzj/oLKaHD1NVqgGmP1mzUWy7MUVyB72A4VDgbfVzZCktpioHIJhv7rx
JWYC9Bj6HAQ17wUUd5tIrIqdXkakcxEFb8MfxWmX5/FxP1d1tgISFg37lJC0IfHf
hyghFnBr8+jmKoVywKtUYN+Q3gG5crnlF57zsVzQK4GDyFO7SJ4VugAeWZ+wZvMJ
rOabSeonTmLa8pRXSd5DlFE/jujZsW+bN+KytPNwTxaTCm1EOAuZI1N3A0Hhs+lK
tv4yOTWRHroEXpcDQvRgLUa8I6LlyBL9FOVweT5EUUdOxxvVZlczdcSHhsp2U0fZ
A2aUQ9ECgYEA4ggSArPRJp5ZPtbdQtV4F6RGNpu1by4r+1FeuYI+uOS7j1efFuNU
s35uvGQ5YDX6x4eMe4RlS/7558pgcbYZAcGf0Pxap3A1ifb8NdbqzHCSZMZDpFKJ
MYph2FddfBqPn3urG8oN1z9vDn+Y+9eopx6Rz/hh1COiRre+PyAgsjkCgYEA2gra
QZqJk/Tl7heji/jW1Tgu2TNuXyyI2KxZpXjiN1r/IqUBD1zs3decoA+0S2U+TnnU
o5YrJOvjb6SLNEiBiGH2wChIweQEphyTsNl0KoAbpvBkq2BLLb+5xu4odScuLM09
iKd3OXfnbF9U1/2rPi/yzDRsDSXt5mKtSfsfql8CgYAD4GeOrE7V/rlBHqZE0yxw
G10o6pq+AWi3srmRLO6udR3SY4pS9ispuO1lRcLGJ6bZbTW3mJm0J/dZRltJF/pt
0UhQaUOUw5Pnfdjtg3Ybc4LPP6dBVjkMJHdxIm50BnCYJ6LToy+BlZDuCrow943o
79lIW9YxsTrDQ7t7ka194QKBgH3bh9IYZtNtqA7/vBp+f1tB++DJzCrJpRAUpAZc
uY8kSmLwBaWdiOggnbrSdcqTXRylPDVU6AB+3KBDxUpfk81qZqjSV/T7LifIFQQe
8OvbWJrK5gD6K0r0AUMvk1DUVdXsfllT+QDGEmI+wNWQCflyad6vX7NTMngqe0ZZ
2xRXAoGAZK18grIW9zEdUxQceuPdL6os+zGiJGLe2B7LORdSP6eIwDTh69SS3mJT
dDI/EFuabDmzNi31ThTfB8wa9sE8w1YLIQI8/FvccnmPC4k92kcxYSmocBaQr9tx
NvYxE7VBhdCH6qaCzmWM/dO/4emCQIEe+PMAlC7nPtpp3TWpqDc=
-----END RSA PRIVATE KEY-----)"};
const std::string TestRestApiEnable::predefined_router_cert{
    R"(-----BEGIN CERTIFICATE-----
MIIC+TCCAeGgAwIBAgIBAjANBgkqhkiG9w0BAQsFADA1MTMwMQYDVQQDDCpNeVNR
TF9Sb3V0ZXJfQXV0b19HZW5lcmF0ZWRfQ0FfQ2VydGlmaWNhdGUwHhcNMjAwMzMx
MTQyOTI4WhcNMzAwMzI5MTQyOTI4WjA5MTcwNQYDVQQDDC5NeVNRTF9Sb3V0ZXJf
QXV0b19HZW5lcmF0ZWRfUm91dGVyX0NlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0B
AQEFAAOCAQ8AMIIBCgKCAQEAwIR0W80QVKg91toariO65pB7/aoR67WzIomLmxgT
VbcT4qpdRxj5kzwVyO0TTcwH9XEjsMUtao0+VByZGGYpxDUrsiyqMBqDpNrzY6PS
UDCOPULiUVMUMzxdtpoiKpvJ35DFYYHZOUCaynIzBKbR/JLFlQZ3GZLJVnKhu3b3
7hSEnSzBy/ZKYgFQT45V5ejh80BNmW8zHc8/hEXyes474SsJqyFvmhQbzpjMzPxc
bU3+a8VB7WRomLJG+Gm8SKufPlEjqKESyW4V7fMBv6Qsqry4Z5DJ5GVKIzd6vxoA
sRetp5H87y86ddwivL1Pv4nya1k8mgtJhRTiP863GmL9JwIDAQABoxAwDjAMBgNV
HRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4IBAQBFH+T9AZgTHTCmw9Zhvg8RQlDN
lRqtChv4ww3kwB3thcEbxaal6ERuZjSzoguHvnktZwg5K0gAgeKYMkGOPD2xJrKW
LEEyROqbrsgSSPLBJQqcUQ0Sr9Sh0S4NUL1FUJfjxcJXbAIi4tYKkC2cWAziBbSv
8JXqOCv7hNeCnLIYB1GFYgBZn9oeeqzxT7C+hcOCAjyPzHQzrqS/GCX9AkCpY0zi
iOhZnJao1ZvGZ6lJLf+SG69L5mFqASpxqriBbZasvg+k4yfKA1uN7IukMgWQ4gUl
VeZwMK4Cb8EO7PzsnX2tD6AA5Ums6GhNgYsbJgdq4MdKb3x6YWZ8DpksSIX2
-----END CERTIFICATE-----)"};

/**
 * @test
 * Verify --disable-rest disables REST support. 'mysqlrouter.conf' should not
 * contain lines required to enable REST API and connecting to REST API
 * should fail.
 *
 * WL13906:TS_FR01_01
 * WL13906:TS_FR06_01
 */
TEST_F(TestRestApiEnable, ensure_rest_is_disabled) {
  do_bootstrap({
      "--disable-rest",                    //
      "--client-ssl-mode", "PASSTHROUGH",  //
      "--disable-rw-split",                //
  });

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, false);

  auto &router = ProcessManager::launch_router({"-c", config_path.str()});

  EXPECT_EQ(std::error_code{}, router.send_clean_shutdown_event());
  EXPECT_EQ(0, router.wait_for_exit());

  EXPECT_THAT(router.get_logfile_content(),
              ::testing::Not(::testing::HasSubstr("rest_routing")));
}

/**
 * @test
 * Verify that bootstrap enables REST API by default. 'mysqlrouter.conf' should
 * contain lines required to enable REST API.
 *
 * WL13906:TS_FR03_01
 * WL13906:TS_FR05_01
 */
TEST_F(TestRestApiEnable, ensure_rest_is_configured_by_default) {
  ASSERT_NO_FATAL_FAILURE(
      do_bootstrap({/*default command line arguments*/}, false));

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);
}

/**
 * @test
 * Verify that --https-port sets REST API port. Verify that connecting to REST
 * API on this specific port works as expected.
 *
 * WL13906:TS_FR02_01
 */
TEST_F(TestRestApiEnable, ensure_rest_works_on_custom_port) {
  do_bootstrap({"--https-port", std::to_string(custom_port)});

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);

  ProcessManager::launch_router({"-c", config_path.str()});

  assert_rest_works(custom_port);
}

class UseEdgeHttpsPortValues : public TestRestApiEnable,
                               public ::testing::WithParamInterface<int> {};

/**
 * @test
 * Verify that --https-port sets REST API port for high and low port values.
 * Verify that 'mysqlrouter.conf' contains configuration for the specified
 * port.
 *
 * WL13906:TS_FR02_02
 * WL13906:TS_FR02_03
 */
TEST_P(UseEdgeHttpsPortValues,
       ensure_bootstrap_works_for_edge_https_port_values) {
  do_bootstrap({"--https-port", std::to_string(GetParam())}, false);

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);
}

INSTANTIATE_TEST_SUITE_P(CheckEdgeHttpsPortValues, UseEdgeHttpsPortValues,
                         ::testing::Values(1, 65535));

class EnableWrongHttpsPort : public TestRestApiEnable,
                             public ::testing::WithParamInterface<int> {};

/**
 * @test
 * Verify that --https-port values out of the allowed range cause bootstrap to
 * fail.
 *
 * WL13906:TS_FailReq02_01
 * WL13906:TS_FailReq02_03
 */
TEST_P(EnableWrongHttpsPort, ensure_bootstrap_fails_for_invalid_https_port) {
  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d", temp_test_dir.name(), "--https-port", std::to_string(GetParam())};
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router_bootstrap, EXIT_FAILURE);

  EXPECT_FALSE(router_bootstrap.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'mycluster'"));

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
}

INSTANTIATE_TEST_SUITE_P(CheckWrongHttpsPort, EnableWrongHttpsPort,
                         ::testing::Values(0, 65536));

class OverlappingHttpsPort
    : public TestRestApiEnable,
      public ::testing::WithParamInterface<uint16_t TestRestApiEnable::*> {};

/**
 * @test
 * Verify that bootstrap do not check if --https-port overlaps with other ports.
 * Bootstrap procedure should succeed.
 *
 * WL13906:TS_NFR02_01
 * WL13906:TS_NFR02_02
 */
TEST_P(OverlappingHttpsPort,
       ensure_bootstrap_works_for_overlapping_https_port) {
  do_bootstrap({"--https-port", std::to_string(this->*GetParam())}, false);

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);
}

INSTANTIATE_TEST_SUITE_P(
    CheckOverlappingHttpsPort, OverlappingHttpsPort,
    ::testing::Values(&TestRestApiEnable::router_port_rw,
                      &TestRestApiEnable::cluster_node_port));

/**
 * @test
 * Verify --https-port and --disable-rest are mutually exclusive. Bootstrap must
 * fail and no certificate files should be created.
 *
 * WL13906:TS_FailReq01_01
 */
TEST_F(TestRestApiEnable, bootstrap_conflicting_options) {
  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d",
      temp_test_dir.name(),
      "--https-port",
      std::to_string(custom_port),
      "--disable-rest"};
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router_bootstrap, EXIT_FAILURE);

  EXPECT_FALSE(router_bootstrap.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'mycluster'"));

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
}

class RestApiEnableUserCertificates
    : public TestRestApiEnable,
      public ::testing::WithParamInterface<std::vector<cert_file_t>> {};

/**
 * @test
 * Verify bootstrap behavior when user provides Router certificates to be used
 * for the REST API service. Bootstrap keeps existing Router/CA cert/key files
 * (does not create new, does not delete old, does not overwrite existing), and
 * exits with success. Verify that connecting to REST API works fine.
 *
 * WL13906:TS_FR04_01
 */
TEST_P(RestApiEnableUserCertificates, ensure_rest_works_with_user_certs) {
  create_cert_files(GetParam());
  auto &router_bootstrap =
      do_bootstrap({"--https-port", std::to_string(custom_port)});
  const auto expected_message =
      string_format("- Using existing certificates from the '%s' directory",
                    datadir_path.real_path().c_str());
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr(expected_message));

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_router_key, cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);
  EXPECT_TRUE(certificate_files_not_changed(GetParam()));

  ProcessManager::launch_router({"-c", config_path.str()});

  assert_rest_works(custom_port);
}

INSTANTIATE_TEST_SUITE_P(
    CheckRestApiUserCertificates, RestApiEnableUserCertificates,
    ::testing::Values(std::vector<cert_file_t>{cert_file_t::k_router_key,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_router_key,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_key,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_key,
                                               cert_file_t::k_router_cert}));

class RestApiEnableNotEnoughFiles
    : public TestRestApiEnable,
      public ::testing::WithParamInterface<std::vector<cert_file_t>> {};

/**
 * @test
 * Verify that if the data directory contains some certificate or key files but
 * Router certificate or RSA key file associated with it is missing the
 * bootstrap procedure must fail.
 *
 * WL13906:TS_FR04_01
 */
TEST_P(RestApiEnableNotEnoughFiles, ensure_rest_fail) {
  create_cert_files(GetParam());
  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d", temp_test_dir.name()};
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);
  check_exit_code(router_bootstrap, EXIT_FAILURE);

  const auto &files = GetParam();
  auto has_file = [files](const cert_file_t &file) {
    return std::find(std::begin(files), std::end(files), file) !=
           std::end(files);
  };

  const auto &router_key_filename =
      cert_filenames.at(cert_file_t::k_router_key);
  const auto &router_cert_filename =
      cert_filenames.at(cert_file_t::k_router_cert);

  std::string missing_files;
  if (!has_file(cert_file_t::k_router_key))
    missing_files += router_key_filename;
  if (!missing_files.empty()) missing_files += ", ";
  if (!has_file(cert_file_t::k_router_cert))
    missing_files += router_cert_filename;
  const std::string output = string_format(
      "Error: Missing certificate files in %s: '%s'. Please provide them or "
      "erase the existing certificate files and re-run bootstrap.",
      datadir_path.real_path().c_str(), missing_files.c_str());
  EXPECT_TRUE(router_bootstrap.expect_output(output));
}

INSTANTIATE_TEST_SUITE_P(
    CheckRestApiEnableNotEnoughFiles, RestApiEnableNotEnoughFiles,
    ::testing::Values(std::vector<cert_file_t>{cert_file_t::k_router_key},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_router_key},
                      std::vector<cert_file_t>{cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_key},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_key},
                      std::vector<cert_file_t>{cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_ca_cert,
                                               cert_file_t::k_router_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key},
                      std::vector<cert_file_t>{cert_file_t::k_ca_cert},
                      std::vector<cert_file_t>{cert_file_t::k_ca_key,
                                               cert_file_t::k_ca_cert}));

class RestApiInvalidUserCerts
    : public TestRestApiEnable,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test
 * Verify that bootstrap does not check if user provided certs and keys are
 * valid.
 * Verify that bootstrap succeeds and files are not changed.
 *
 * WL13906:TS_NFR01_01
 * WL13906:TS_NFR01_02
 */
TEST_P(RestApiInvalidUserCerts,
       ensure_rest_fail_for_invalid_user_certificates) {
  mysql_harness::mkdir(datadir_path.str().c_str(), 0755);

  const auto &ca_key_filename = cert_filenames.at(cert_file_t::k_ca_key);
  const auto &ca_cert_filename = cert_filenames.at(cert_file_t::k_ca_cert);
  const auto &router_key_filename =
      cert_filenames.at(cert_file_t::k_router_key);
  const auto &router_cert_filename =
      cert_filenames.at(cert_file_t::k_router_cert);

  {
    std::ofstream ca_key_stream{datadir_path.join(ca_key_filename).str()};
    ca_key_stream << GetParam();
    std::ofstream ca_cert_stream{datadir_path.join(ca_cert_filename).str()};
    ca_cert_stream << GetParam();
    std::ofstream router_key_stream{
        datadir_path.join(router_key_filename).str()};
    router_key_stream << GetParam();
    std::ofstream router_cert_stream{
        datadir_path.join(router_cert_filename).str()};
    router_cert_stream << GetParam();
  }

  auto &router_bootstrap =
      do_bootstrap({"--https-port", std::to_string(custom_port)});
  const auto expected_message =
      string_format("- Using existing certificates from the '%s' directory",
                    datadir_path.real_path().c_str());
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr(expected_message));

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  EXPECT_EQ(get_file_output(datadir_path.join(ca_key_filename).str()),
            GetParam());
  EXPECT_EQ(get_file_output(datadir_path.join(ca_cert_filename).str()),
            GetParam());
  EXPECT_EQ(get_file_output(datadir_path.join(router_key_filename).str()),
            GetParam());
  EXPECT_EQ(get_file_output(datadir_path.join(router_cert_filename).str()),
            GetParam());
  assert_rest_config(config_path, true);

  auto &router = launch_router({"-c", config_path.str()}, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  const std::string log_error =
      "Error: using SSL private key file '" +
      datadir_path.real_path().join(router_key_filename).str() +
      "' or SSL certificate file '" +
      datadir_path.real_path().join(router_cert_filename).str() + "' failed";
  EXPECT_THAT(router.get_logfile_content("mysqlrouter.log",
                                         temp_test_dir.name() + "/log"),
              ::testing::HasSubstr(log_error));
}

INSTANTIATE_TEST_SUITE_P(CheckRestApiInvalidUserCerts, RestApiInvalidUserCerts,
                         ::testing::Values("", "this aint no certificate"));

/**
 * @test
 * Verify certificates and keys can be written to less common filenames.
 * Pass datadir as a relative path.
 *
 * WL13906:TS_Extra_02
 */
TEST_F(TestRestApiEnable, use_custom_datadir_relative_path) {
  auto odd_path = mysql_harness::Path{temp_test_dir.name()}.join(
      "Path with CAPS, punctuation, spaces and ¿ó¿-¿¿¿ii");

  verify_bootstrap_at_custom_path(odd_path);
}

/**
 * @test
 * Verify certificates and keys can be written to less common filenames.
 * Pass datadir as an absolute path.
 *
 * WL13906:TS_Extra_01
 */
TEST_F(TestRestApiEnable, use_custom_datadir_absolute_path) {
  auto odd_path = mysql_harness::Path{temp_test_dir.name()}.real_path().join(
      "Path with CAPS, punctuation, spaces and ¿ó¿-¿¿¿ii");

  verify_bootstrap_at_custom_path(odd_path);
}

/**
 * @test Verify certificates and keys are cleaned up on error.
 *
 * Verify that
 *
 * 1) bootstrap fails and exits with a meaningful error,
 * 2) any key/certificate files created during bootstrap are erased.
 *
 * WL13906:TS_Extra_03
 */
TEST_F(TestRestApiEnable, ensure_certificate_files_cleanup) {
  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d", temp_test_dir.name(), "--strict"};

  // to fail account verification, use a cluster-id which leads to a failed
  // query at bootstrap.
  set_globals("some-garbage");

  // Account verification is done after the certificates are created, therefore
  // we expect the following order of events:
  // 1. Certificates are created
  // 2. Account verification fails due to the '--strict' option and missing
  //    queries in the rest_api_enable.js file.
  // 3. Certificates are cleaned up.
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router_bootstrap, EXIT_FAILURE);
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr("Account verification failed"));

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
}

class TestRestApiEnableBootstrapFailover : public TestRestApiEnable {
 public:
  void SetUp() override {
    RouterComponentTest::SetUp();
    setup_paths();
    set_router_accepting_ports();
  }

  void setup_mocks(const bool failover_successful) {
    std::vector<uint16_t> classic_ports;
    for (auto i = 0; i < k_node_count; ++i) {
      classic_ports.emplace_back(port_pool_.get_next_available());
    }

    gr_nodes = classic_ports_to_gr_nodes(classic_ports);
    cluster_nodes = classic_ports_to_cluster_nodes(classic_ports);

    for (auto i = 0; i < k_node_count; ++i) {
      cluster_http_port = port_pool_.get_next_available();
      auto port = gr_nodes[i].classic_port;

      std::string trace_file;
      if (i == 0 || !failover_successful) {
        trace_file = get_data_dir()
                         .join("bootstrap_failover_super_read_only_1_gr.js")
                         .str();
      } else {
        trace_file = get_data_dir().join("rest_api_enable.js").str();
      }

      mock_servers.emplace_back(
          port, ProcessManager::launch_mysql_server_mock(
                    trace_file, port, EXIT_SUCCESS, false, cluster_http_port));

      auto &mock_server = mock_servers.back().second;
      ASSERT_NO_FATAL_FAILURE(check_port_ready(mock_server, port));
      ASSERT_TRUE(MockServerRestClient(cluster_http_port)
                      .wait_for_rest_endpoint_ready());

      set_mock_metadata(cluster_http_port, cluster_id, gr_nodes, 0,
                        cluster_nodes, 0, false, gr_member_ip, "",
                        metadata_version, cluster_name);
    }

    cluster_node_port = gr_nodes[0].classic_port;
    router_port_rw = port_pool_.get_next_available();
    router_port_ro = port_pool_.get_next_available();
    router_port_x_rw = port_pool_.get_next_available();
    router_port_x_ro = port_pool_.get_next_available();
  }

 private:
  const mysqlrouter::MetadataSchemaVersion metadata_version{2, 2, 0};
  const std::string cluster_name{"mycluster"};
  std::vector<std::pair<uint16_t, ProcessWrapper &>> mock_servers;
  std::vector<GRNode> gr_nodes;
  std::vector<ClusterNode> cluster_nodes;
  static const uint8_t k_node_count{3};
};

/**
 * @test
 * Verify certificates/key generation works fine when failover happens.
 * 'mysqlrouter.conf' should contain lines required to enable REST API and
 * connecting to REST API should work.
 *
 * WL13906:TS_Extra_04
 */
TEST_F(TestRestApiEnableBootstrapFailover,
       ensure_rest_works_after_node_failover) {
  const bool successful_failover = true;
  setup_mocks(successful_failover);
  const auto rest_port = port_pool_.get_next_available();
  auto &router_bootstrap = do_bootstrap(
      {"--conf-set-option=http_server.port=" + std::to_string(rest_port)});
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr("trying to connect to"));

  EXPECT_TRUE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
  assert_rest_config(config_path, true);

  ProcessManager::launch_router({"-c", config_path.str()});

  assert_rest_works(rest_port);
}

/**
 * @test
 * Verify certificates and keys are cleaned up on error after cluster node
 * failover. Verify that 1) bootstrap fails and exits with a meaningful error,
 * 2) any key/certificate files created during bootstrap are erased.
 *
 * WL13906:TS_Extra_05
 */
TEST_F(TestRestApiEnableBootstrapFailover,
       ensure_certificate_files_cleanup_on_error) {
  const bool successful_failover = true;
  setup_mocks(successful_failover);

  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d", temp_test_dir.name(), "--strict"};
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router_bootstrap, EXIT_FAILURE);
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr("trying to connect to"));
  EXPECT_THAT(router_bootstrap.get_full_output(),
              ::testing::HasSubstr("Account verification failed"));

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
}

/**
 * @test
 * Verify no certificate/key files remain after a failed bootstrap due to all
 * nodes being read only.
 *
 * WL13906:TS_Extra_05
 */
TEST_F(TestRestApiEnableBootstrapFailover,
       ensure_certificate_files_cleanup_on_failed_node_failover) {
  const bool successful_failover = false;
  setup_mocks(successful_failover);

  std::vector<std::string> cmdline = {
      "--bootstrap=" + gr_member_ip + ":" + std::to_string(cluster_node_port),
      "-d", temp_test_dir.name()};
  auto &router_bootstrap = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router_bootstrap, EXIT_FAILURE);
  EXPECT_THAT(
      router_bootstrap.get_full_output(),
      ::testing::HasSubstr("Error: no more nodes to fail-over too, giving up"));

  EXPECT_FALSE(certificate_files_exists(
      {cert_file_t::k_ca_key, cert_file_t::k_ca_cert, cert_file_t::k_router_key,
       cert_file_t::k_router_cert}));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
