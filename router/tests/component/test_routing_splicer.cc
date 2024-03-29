/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <string>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <openssl/opensslv.h>  // OPENSSL_VERSION

#include "config_builder.h"
#include "mysql.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/mysql_session.h"
#include "mysqlxclient.h"
#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xsession.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "router/src/routing/src/ssl_mode.h"
#include "router_component_test.h"  // ProcessManager

using namespace std::string_literals;
using namespace std::chrono_literals;

class SplicerTest : public RouterComponentTest {
 protected:
  TempDirectory conf_dir_;
  const std::string mock_server_host_{"127.0.0.1"s};
  const std::string router_host_{"127.0.0.1"s};

  const std::string valid_ssl_key_{SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  const std::string valid_ssl_cert_{SSL_TEST_DATA_DIR
                                    "/server-cert-sha512.pem"};
};

TEST_F(SplicerTest, ssl_mode_default_passthrough) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();
  launch_mysql_server_mock(mock_file, server_port);

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations",
               mock_server_host_ + ":" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
          })},
      "");
  SCOPED_TRACE("starting router with config:\n" + config);
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));
}

TEST_F(SplicerTest, ssl_mode_default_preferred) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();
  launch_mysql_server_mock(mock_file, server_port);

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations",
               mock_server_host_ + ":" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_key", valid_ssl_key_},
              {"client_ssl_cert", valid_ssl_cert_},
          })},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));
}

/**
 * check metadata-cache handles broken hostnames in metadata.
 *
 * trace file contains a broken hostname "[foobar]" which should trigger a parse
 * error when the metadata is SELECTed.
 */
TEST_F(SplicerTest, invalid_metadata) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  SCOPED_TRACE("// start mock-server with TLS enabled");
  const std::string mock_file =
      get_data_dir().join("metadata_broken_hostname.js").str();
  auto mock_server_args =
      mysql_server_mock_cmdline_args(mock_file, server_port);

  for (const auto &arg :
       std::vector<std::string>{"--ssl-cert"s, valid_ssl_cert_,  //
                                "--ssl-key"s, valid_ssl_key_,    //
                                "--ssl-mode"s, "REQUIRED"s}) {
    mock_server_args.push_back(arg);
  }

  launch_mysql_server_mock(mock_server_args, server_port);

  SCOPED_TRACE("// start router with TLS enabled");
  auto config = mysql_harness::join(
      std::vector<std::string>{
          mysql_harness::ConfigBuilder::build_section(
              "routing",
              {
                  {"bind_port", std::to_string(router_port)},
                  {"destinations",
                   "metadata-cache://somecluster/default?role=PRIMARY"},
                  {"routing_strategy", "round-robin"},
                  {"client_ssl_mode", "required"},
                  {"client_ssl_key", valid_ssl_key_},
                  {"client_ssl_cert", valid_ssl_cert_},
                  {"server_ssl_mode", "required"},
              }),
          mysql_harness::ConfigBuilder::build_section(
              "metadata_cache:somecluster",
              {
                  {"user", "mysql_router1_user"},
                  {"bootstrap_server_addresses",
                   "mysql://127.0.0.1:" + std::to_string(server_port)},
                  {"metadata_cluster", "test"},
              }),
      },
      "");

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir_.name());
  auto conf_file =
      create_config_file(conf_dir_.name(), config, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_SUCCESS, true, false);

  // wait long enough that a 2nd refresh was done to trigger the invalid
  // hostname

  {
    mysqlrouter::MySQLSession sess;

    // first round should succeed.
    try {
      // the router's certs against the corresponding CA
      sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                           "", "");
      sess.connect("127.0.0.1", router_port,
                   "someuser",  // user
                   "somepass",  // pass
                   "",          // socket
                   ""           // schema
      );

      sess.disconnect();
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }

    // ... then try until the starts to fail.
    try {
      for (size_t rounds{};; ++rounds) {
        // guard against infinite loop
        if (rounds == 100) FAIL() << "connect() should have failed by now.";

        sess.connect("127.0.0.1", router_port,
                     "someuser",  // user
                     "somepass",  // pass
                     "",          // socket
                     ""           // schema
        );

        sess.disconnect();

        // wait a bit and retry.
        std::this_thread::sleep_for(100ms);
      }
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      // connect failed eventually.
      // depending on the timing this cand also be "SSL connection aborted"
      // openssl 1.1.1: 2013
      // openssl 1.0.2: 2026
      EXPECT_THAT(e.code(),
                  ::testing::AnyOf(::testing::Eq(2003), ::testing::Eq(2013),
                                   ::testing::Eq(2026)));
    }
  }

  // shutdown and check the log file
  EXPECT_THAT(router.send_clean_shutdown_event(),
              ::testing::Truly([](auto const &v) { return !bool(v); }));
  EXPECT_EQ(EXIT_SUCCESS, router.wait_for_exit());

  SCOPED_TRACE("// check for the expected error-msg");
  EXPECT_THAT(
      router.get_logfile_content(),
      ::testing::HasSubstr("Error parsing host:port in metadata for instance"));
}

struct SplicerFailParam {
  const char *test_name;

  const std::vector<std::pair<std::string, std::string>> cmdline_opts;

  std::function<void(const std::vector<std::string> &)> checker;
};

class SplicerFailParamTest
    : public SplicerTest,
      public ::testing::WithParamInterface<SplicerFailParam> {};

TEST_P(SplicerFailParamTest, fails) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();
  launch_mysql_server_mock(mock_file, server_port);

  std::string mock_server_host{"127.0.0.1"s};

  std::vector<std::pair<std::string, std::string>> cmdline_opts = {
      {"bind_port", std::to_string(router_port)},
      {"destinations", mock_server_host + ":" + std::to_string(server_port)},
      {"routing_strategy", "round-robin"},
  };

  for (const auto &arg : GetParam().cmdline_opts) {
    cmdline_opts.push_back(arg);
  }

  auto config = mysql_harness::join(
      std::vector<std::string>{
          mysql_harness::ConfigBuilder::build_section("routing", cmdline_opts)},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1ms);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_NO_FATAL_FAILURE(GetParam().checker(
      mysql_harness::split_string(router.get_logfile_content(), '\n')));
}

const SplicerFailParam splicer_fail_params[] = {
    {"client_ssl_mode_unknown",
     {
         {"client_ssl_mode", "unknown"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "invalid value 'unknown' for option "
                                     "client_ssl_mode in [routing]")));
     }},
    {"client_ssl_key_no_cert",
     {
         {"client_ssl_key", "unknown"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_cert must be set")));
     }},
    {"client_ssl_preferred_cert_no_key",  // RT2_CERT_KEY_OPERATION_07
     {
         {"client_ssl_mode", "preferred"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_key must be set")));
     }},
    {"client_ssl_mode_required_cert_no_key",  // RT2_CERT_KEY_OPERATION_08
     {
         {"client_ssl_mode", "required"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_key must be set")));
     }},
    {"client_ssl_mode_preferred_key_no_cert",  // RT2_CERT_KEY_OPERATION_11
     {
         {"client_ssl_mode", "preferred"},
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_cert must be set")));
     }},
    {"client_ssl_mode_required_key_no_cert",  // RT2_CERT_KEY_OPERATION_12
     {
         {"client_ssl_mode", "required"},
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_cert must be set")));
     }},
    {"client_ssl_mode_preferred_no_cert_no_key",  // RT2_CERT_KEY_OPERATION_15
     {
         {"client_ssl_mode", "preferred"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_cert must be set")));
     }},
    {"client_ssl_mode_required_no_cert_no_key",  // RT2_CERT_KEY_OPERATION_16
     {
         {"client_ssl_mode", "required"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_cert must be set")));
     }},
    {"client_ssl_key_no_key",
     {
         {"client_ssl_cert", "unknown"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "client_ssl_key must be set")));
     }},

    {"client_ssl_cert_not_exists",  // RT2_ARGS_PATHS_INVALID_01
     {
         // not a valid cert
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/non-exitent-file"},
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "loading client_ssl_cert")));
     }},
    {"client_ssl_key_not_exists",  // RT2_ARGS_PATHS_INVALID_02
     {
         // not a valid cert
         {"client_ssl_key", SSL_TEST_DATA_DIR "/non-exitent-file"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "loading client_ssl_cert")));
     }},
    {"client_ssl_dh_params_not_exists",  // RT2_ARGS_PATHS_INVALID_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_dh_params", SSL_TEST_DATA_DIR "/non-existent-file"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting client_ssl_dh_params failed")));
     }},
    {"client_ssl_curves_unknown",  // RT2_CIPHERS_UNKNOWN_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_curves", "not-a-curve"},
     },
     [](const std::vector<std::string> &output_lines) {
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
       ASSERT_THAT(output_lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "setting client_ssl_curves=not-a-curve failed")));
#else
       ASSERT_THAT(output_lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "setting client_ssl_curves is not supported by the ssl "
                       "library, it should stay unset")));
#endif
     }},
    {"client_ssl_curves_p521r1_and_unknown",  // RT2_CIPHERS_RECOGNISED_06
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_curves", "secp521r1:not-a-curve"},
     },
     [](const std::vector<std::string> &output_lines) {
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr(
               "setting client_ssl_curves=secp521r1:not-a-curve failed")));
#else
       ASSERT_THAT(output_lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "setting client_ssl_curves is not supported by the ssl "
                       "library, it should stay unset")));
#endif
     }},
    {"server_ssl_ca_not_exists",  // RT2_ARGS_PATHS_INVALID_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/non-existent-file"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("setting server_ssl_ca")));
     }},
    {"client_ssl_cipher_quotes",  // RT2_CIPHERS_EMPTY_Q_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_cipher", "''"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting client_ssl_cipher")));
     }},
    {"client_ssl_curves_quotes",  // RT2_CIPHERS_EMPTY_Q_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_curves", "''"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting client_ssl_curves")));
     }},
    {"server_ssl_cipher_quotes",  // RT2_CIPHERS_EMPTY_Q_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_cipher", "''"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting server_ssl_cipher")));
     }},
    {"server_ssl_curves_quotes",  // RT2_CIPHERS_EMPTY_Q_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_curves", "''"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting server_ssl_curves")));
     }},

#if 0
    // some openssl fail with "no match" if one specifies an unknown cipher
    // some others don't.
    //
    // ubuntu 20.04: openssl 1.1.1f: fails
    // ubuntu 18.04: openssl 1.1.1: ignores
    // debian9     : openssl 1.1.0: fails
    // el6         : openssl 1.0.1: fails
    //
    //
    {"client_ssl_cipher_no_match",  // RT2_CIPHERS_UNKNOWN_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_cipher", "-ALL:unknown"},
     },
     [](const std::vector<std::string> &output_lines) {
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
       // openssl 1.0.1 doesn't fail on unknown ciphers.
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting client_ssl_cipher")));
#else
       // unused
       (void)output_lines;
#endif
     }},
#endif
    {"server_ssl_capath_not_exists",  // RT2_ARGS_PATHS_INVALID_05,
                                      // RT2_CAPATH_BAD_03
                                      // RT2_CAPATH_CRLPATH_VALID_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_capath", SSL_TEST_DATA_DIR "/non-existent-file"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("server_ssl_capath")));
     }},
#if 0
    // both may be specified at the same time:
    //
    // - first ssl_ca is checked
    // - then ssl_capath
    {"server_ssl_ca_and_ssl_ca_path",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-sha512.pem2"},
         {"server_ssl_capath", SSL_TEST_DATA_DIR "/non-existent-path"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting server_ssl_capath")));
     }},
#endif
    {"server_ssl_crl_not_exists",  // RT2_ARGS_PATHS_INVALID_07
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-sha512.pem"},
         {"server_ssl_crl", SSL_TEST_DATA_DIR "/non-existent-file"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("setting server_ssl_crl")));
     }},

    {"server_ssl_crlpath_not_exists",  // RT2_ARGS_PATHS_INVALID_08,
                                       // RT2_CRLPATH_BAD_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-sha512.pem"},
         {"server_ssl_crlpath", SSL_TEST_DATA_DIR "/non-existent-file"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("server_ssl_crlpath")));
     }},

#if 0
    // ssl_crl and ssl_crlpath can be specified together.
    {"server_ssl_crl_and_ssl_crl_path",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-sha512.pem"},
         {"server_ssl_crl", SSL_TEST_DATA_DIR "/crl-server-cert.pem"},
         {"server_ssl_crlpath", SSL_TEST_DATA_DIR "/crldir"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("setting server_ssl_crl")));
     }},
#endif
    {"client_ssl_dh_param_wrong_pem",  // RT2_DH_PARAMS_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         // a certificate isn't a DH param PEM file.
         {"client_ssl_dh_params", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::AllOf(
               ::testing::HasSubstr("setting client_ssl_dh_param"),
               ::testing::AnyOf(
                   ::testing::EndsWith("no start line"),
                   ::testing::EndsWith("DECODER routines::unsupported")))));
     }},
    {"server_ssl_curves_unknown",  // RT2_CIPHERS_UNKNOWN_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_curves", "not-a-curve"},
     },
     [](const std::vector<std::string> &output_lines) {
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
       ASSERT_THAT(output_lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "setting server_ssl_curves=not-a-curve failed")));
#else
       ASSERT_THAT(output_lines, ::testing::Contains(::testing::HasSubstr(
                                     "setting server_ssl_curves=not-a-curve is "
                                     "not supported by the ssl "
                                     "library, it should stay unset")));
#endif
     }},
    {"server_crl_bad",  // RT2_CRL_BAD_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"server_ssl_crl", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("setting server_ssl_ca")));
     }},
    {"server_ca_bad",  // RT2_CA_BAD_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"server_ssl_verify", "verify_ca"},
     },
     [](const std::vector<std::string> &output_lines) {
       ASSERT_THAT(
           output_lines,
           ::testing::Contains(::testing::HasSubstr("setting server_ssl_ca")));
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, SplicerFailParamTest,
                         testing::ValuesIn(splicer_fail_params),
                         [](auto &info) {
                           auto p = info.param;
                           return p.test_name;
                         });

//
// tests that start router successfully and make a connection.
//

struct SplicerConnectParam {
  const char *test_name;

  const std::vector<std::pair<std::string, std::string>> cmdline_opts;

  std::function<void(const std::string &, uint16_t)> checker;
};

class SplicerConnectParamTest
    : public SplicerTest,
      public ::testing::WithParamInterface<SplicerConnectParam> {};

TEST_P(SplicerConnectParamTest, check) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();

  auto mock_server_cmdline_args =
      mysql_server_mock_cmdline_args(mock_file, server_port);

  std::string mock_server_prefix{"mock_server::"};

  for (const auto &arg : GetParam().cmdline_opts) {
    if (arg.first.substr(0, mock_server_prefix.size()) == mock_server_prefix) {
      mock_server_cmdline_args.emplace_back(
          arg.first.substr(mock_server_prefix.size()));
      mock_server_cmdline_args.emplace_back(arg.second);
    }
  }
  launch_mysql_server_mock(mock_server_cmdline_args, server_port);

  const std::string destination("localhost:" + std::to_string(server_port));

  std::vector<std::pair<std::string, std::string>> cmdline_opts = {
      {"bind_port", std::to_string(router_port)},
      {"destinations", destination},
      {"routing_strategy", "round-robin"},
  };

  const auto cadir = Path(conf_dir_.name()).join("cadir").str();
  bool need_cadir{false};
  const auto crldir = Path(conf_dir_.name()).join("crldir").str();
  bool need_crldir{false};

  for (const auto &arg : GetParam().cmdline_opts) {
    // skip mock-server specific entries
    if (arg.first.substr(0, mock_server_prefix.size()) == mock_server_prefix)
      continue;

    if (arg.first == "server_ssl_capath") {
      if (arg.second == "@capath@") {
        cmdline_opts.emplace_back(arg.first, cadir);
        need_cadir = true;
      } else if (arg.second == "@capath_noexist@") {
        cmdline_opts.emplace_back(arg.first, cadir);
      } else {
        cmdline_opts.push_back(arg);
      }
    } else if (arg.first == "server_ssl_crlpath") {
      if (arg.second == "@crlpath@") {
        cmdline_opts.emplace_back(arg.first, crldir);
        need_crldir = true;
      } else {
        cmdline_opts.push_back(arg);
      }
    } else {
      cmdline_opts.push_back(arg);
    }
  }

  // build cadir if needed.
  if (need_cadir) {
    ASSERT_EQ(0, mysql_harness::mkdir(cadir, 0770));

    // hashes are generated with `$ openssl rehash .`
    for (const auto &p : {
             // CA of server-cert.pem
             std::make_pair(SSL_TEST_DATA_DIR "/cacert.pem", "820cc7fb.0"),
             // CA of crl-server-cert.pem
             std::make_pair(SSL_TEST_DATA_DIR "/crl-ca-cert.pem", "5df06fcb.0"),
             // CA of crl-server-cert.pem
             std::make_pair(SSL_TEST_DATA_DIR "/ca-sha512.pem", "07c605e0.0"),
         }) {
      std::ifstream ifs(p.first, std::ios::binary);
      std::ofstream ofs(cadir + "/" + p.second, std::ios::binary);

      ASSERT_TRUE(ofs.is_open());
      ASSERT_TRUE(ifs.is_open());

      ofs << ifs.rdbuf();
    }
  }

  // build crldir if needed.
  if (need_crldir) {
    ASSERT_EQ(0, mysql_harness::mkdir(crldir, 0770));

    // hashes are generated with `$ openssl rehash .`
    for (const auto &p : {std::make_pair(
             // cert with serial-number 3 is revoked for crl-ca-cert-CA
             SSL_TEST_DATA_DIR "/crl-client-revoked.crl", "5df06fcb.r0")}) {
      std::ifstream ifs(p.first, std::ios::binary);
      std::ofstream ofs(crldir + "/" + p.second, std::ios::binary);

      ASSERT_TRUE(ofs.is_open());
      ASSERT_TRUE(ifs.is_open());

      ofs << ifs.rdbuf();
    }
  }

  const auto config = mysql_harness::join(
      std::vector<std::string>{
          mysql_harness::ConfigBuilder::build_section("routing", cmdline_opts)},
      "");

  const auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file}, EXIT_SUCCESS,
                /* catch_stderr */ true, /* with_sudo */ false,
                /* wait_for_notify_ready */ 30s);
  EXPECT_TRUE(wait_for_port_ready(router_port));

  EXPECT_NO_FATAL_FAILURE(GetParam().checker(router_host_, router_port));
}

const SplicerConnectParam splicer_connect_params[] = {
    {"client_ssl_mode_disabled_no_key",  // RT2_CERT_KEY_OPERATION_04
     {
         // client_ssl_cert and client_ssl_key are ignored
         // specifying one is not error
         // {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "DISABLED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_mode_passthrough_no_key",  // RT2_CERT_KEY_OPERATION_05
     {
         // client_ssl_cert and client_ssl_key are ignored
         // specifying one is not error
         // {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "PASSTHROUGH"},
         {"server_ssl_mode", "AS_CLIENT"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_mode_disabled_no_cert",  // RT2_CERT_KEY_OPERATION_09
     {
         // client_ssl_cert and client_ssl_key are ignored
         // specifying one is not error
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         // {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "DISABLED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_mode_passthrough_no_cert",  // RT2_CERT_KEY_OPERATION_10
     {
         // client_ssl_cert and client_ssl_key are ignored
         // specifying one is not error
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         // {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "PASSTHROUGH"},
         {"server_ssl_mode", "AS_CLIENT"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_mode_disabled_no_key_no_cert",  // RT2_CERT_KEY_OPERATION_13
     {
         {"client_ssl_mode", "DISABLED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_mode_passthrough_no_key_no_cert",  // RT2_CERT_KEY_OPERATION_14
     {
         {"client_ssl_mode", "PASSTHROUGH"},
         {"server_ssl_mode", "AS_CLIENT"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_cert_validates",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the corresponding CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_VERIFY_CA, "", "",
                              SSL_TEST_DATA_DIR "/ca-sha512.pem", "", "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_cert_wrong_ca_validate_fails",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // the router's certs against the wrong CA
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_VERIFY_CA, "", "",
                              SSL_TEST_DATA_DIR "/cacert.pem", "", "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "connect expected to fail";
       } catch (const std::exception &e) {
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
    {"client_ssl_cipher_default",  // RT2_CIPHERS_EMPTY_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_cipher", ""},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_curves_default",  // RT2_CIPHERS_EMPTY_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_curves", ""},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cipher_default",  // RT2_CIPHERS_EMPTY_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"server_ssl_cipher", ""},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_curves_default",  // RT2_CIPHERS_EMPTY_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"server_ssl_curves", ""},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_cipher_aes128_sha256",  // RT2_CIPHERS_RECOGNISED_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_cipher", "AES128-SHA256"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "TLSv1.2", "",
                              "", "", "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         ASSERT_STREQ(sess.ssl_cipher(), "AES128-SHA256");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_cipher_many",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_cipher", "AES128-SHA:AES128-SHA256"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "TLSv1.2", "",
                              "", "", "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         ASSERT_STREQ(sess.ssl_cipher(), "AES128-SHA256");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_dh_params",  // RT2_DH_PARAMS_05
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_dh_params",
          CMAKE_SOURCE_DIR "/router/tests/component/data/dhparams-2048.pem"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_ssl_dh_params_empty",  // RT2_DH_PARAMS_06
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_dh_params", ""},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"client_fails_ca_cert_revoked",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         // cacert is revoked.
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_VERIFY_CA,  // ssl-mode
                              "",                               // tls-version
                              "",                               // ssl-cipher
                              SSL_TEST_DATA_DIR "/cacert.pem",  // ca
                              "",                               // capath
                              SSL_TEST_DATA_DIR "/crl-ca-cert.pem",  // crl
                              ""                                     // crlpath
         );
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected cert-validation to fail.";
       } catch (const std::exception &e) {
         // as the cacert.pem is revoked, cert-validation will fail.
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
    {"client_fails_crl_server_cert_revoked",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_VERIFY_CA,  // ssl-mode
                              "",                               // tls-version
                              "",                               // ssl-cipher
                              SSL_TEST_DATA_DIR "/cacert.pem",  // ca
                              "",                               // capath
                              SSL_TEST_DATA_DIR "/crl-server-cert.pem",  // crl
                              ""  // crlpath
         );
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected cert-validation to fail.";
       } catch (const std::exception &e) {
         // as the cacert.pem is revoked, cert-validation will fail.
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
};

INSTANTIATE_TEST_SUITE_P(ServerPlain, SplicerConnectParamTest,
                         testing::ValuesIn(splicer_connect_params),
                         [](auto &info) {
                           auto p = info.param;
                           return p.test_name;
                         });

#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
// enable tests for curves with openssl 1.0.2 and later
const SplicerConnectParam splicer_connect_openssl_102_params[] = {
    {"client_ssl_curves_p521r1",  // RT2_CIPHERS_RECOGNISED_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "DISABLED"},
         {"client_ssl_curves", "secp521r1"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "TLSv1.2", "",
                              "", "", "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         // ASSERT_STREQ(sess.ssl_cipher(), "AES128-SHA256");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_curves_p384",  // RT2_CIPHERS_RECOGNISED_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_curves", "secp384r1"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_curves_many",  // RT2_CIPHERS_RECOGNISED_08
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},

         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_curves", "secp384r1:secp521r1"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
};

INSTANTIATE_TEST_SUITE_P(ServerPlainOpenssl102, SplicerConnectParamTest,
                         testing::ValuesIn(splicer_connect_openssl_102_params),
                         [](auto &info) {
                           auto p = info.param;
                           return p.test_name;
                         });
#endif

const SplicerConnectParam splicer_connect_tls_params[] = {
    {"server_tlsv12_only",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"mock_server::--tls-version", "TLSv1.2"},
         {"server_ssl_mode", "REQUIRED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         const auto row = sess.query_one("show status like 'ssl_cipher'");
         ASSERT_EQ(row->size(), 2);

         // some cipher is selected.
         EXPECT_STRNE((*row)[1], "");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cipher_aes128_sha256",  // RT2_CIPHERS_RECOGNISED_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"mock_server::--tls-version", "TLSv1.2"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_cipher", "AES128-SHA256"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         const auto row = sess.query_one("show status like 'ssl_cipher'");
         ASSERT_EQ(row->size(), 2);

         EXPECT_STREQ((*row)[1], "AES128-SHA256");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cipher_many",  // RT2_CIPHERS_RECOGNISED_07
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_cipher", "AES128-SHA256:AES128-SHA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );

         // if server uses TLSv1.3 we can't check the cert :(
         // ASSERT_STREQ(sess.ssl_cipher(), "AES128-SHA256");
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cert_verify_default",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         // cacert doesn't match the server's cert. But we don't verify

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/cacert.pem"},
         {"server_ssl_mode", "REQUIRED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cert_verify_disabled",  // RT2_VERIFY_02
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         // cacert doesn't match the server's cert. But we don't verify

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/cacert.pem"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "DISABLED"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const std::exception &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_cert_verify_ca",  // RT2_CA_CRL_VALID_01,
                                   // RT2_VERIFY_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/cacert.pem"},

         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_ca_verify_ca_wrong_ca",  // RT2_CA_CRL_VALID_02, RT2_VERIFY_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         // server runs with a cert that matches cacert.pem
         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-sha512.pem"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected connect to fail";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         ASSERT_EQ(e.code(), 2026);
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
    {"server_ssl_ca_verify_identity_wrong_identity",  // RT2_VERIFY_05
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/crl-ca-cert.pem"},

         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_IDENTITY"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected connect to fail";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         ASSERT_EQ(e.code(), 2026);
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
    {"server_ssl_ca_verify_identity",  // RT2_VERIFY_06
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         // server is started with server-cert.pem which has CN=localhost
         // which is signed by cacert.pem
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/cacert.pem"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_IDENTITY"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       // the server's cert is using a CN=localhost
       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_ca_verify_identity_alternative_subject",
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},

         {"mock_server::--ssl-cert",
          SSL_TEST_DATA_DIR "server-cert-verify-san.pem"},
         {"mock_server::--ssl-key",
          SSL_TEST_DATA_DIR "server-key-verify-san.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         // server is started with server-cert-verify-san.pem which has
         // SubjectAltName=localhost which is signed by ca-cert-verify-san.pem
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/ca-cert-verify-san.pem"},

         {"client_ssl_mode", "REQUIRED"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_IDENTITY"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       // the server's cert is using a CN=localhost
       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_capath_verify_ca",  // RT2_CAPATH_CRLPATH_VALID_01
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_capath", "@capath@"},  // will be replaced
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_crl_revoke_server_cert",  // RT2_CA_CRL_VALID_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         // revoke the crl-server-revoked-cert.pem
         //
         // crl-server-revoked.crl is revokes cert with serial-id 4.
         //
         // $ openssl crl -in crl-server-revoked.crl -text
         //
         // serial-id 4 of the CA is 'crl-server-revoked-cert.prm'
         //
         // $ openssl x509 -in crl-server-revoked-cert.pem -text
         {"mock_server::--ssl-cert",
          SSL_TEST_DATA_DIR "crl-server-revoked-cert.pem"},
         {"mock_server::--ssl-key",
          SSL_TEST_DATA_DIR "crl-server-revoked-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/crl-ca-cert.pem"},
         {"server_ssl_crl", SSL_TEST_DATA_DIR "/crl-server-revoked.crl"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
         FAIL() << "expected connect to fail";
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         ASSERT_EQ(e.code(), 2026);
         ASSERT_THAT(e.what(),
                     ::testing::HasSubstr("certificate verify failed"));
       }
     }},
    {"server_ssl_crl_revoke_other_cert",  // RT2_CA_CRL_VALID_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         // revoke the an unrelated cert.
         {"server_ssl_crl", SSL_TEST_DATA_DIR "/crl-client-revoked.crl"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/crl-ca-cert.pem"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},

    {"server_ssl_ca_crlpath",  // RT2_CAPATH_CRLPATH_VALID_03
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         {"server_ssl_ca", SSL_TEST_DATA_DIR "/crl-ca-cert.pem"},
         {"server_ssl_crlpath", "@crlpath@"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
    {"server_ssl_capath_crlpath_no_matching_crl",  // RT2_CAPATH_CRLPATH_VALID_04
     {
         {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
         {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
         {"client_ssl_mode", "REQUIRED"},

         {"mock_server::--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
         {"mock_server::--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
         {"mock_server::--ssl-mode", "PREFERRED"},
         // crldir contains a CRL for the client-cert ... make sure we trust the
         // CA that signed the CRL
         {"server_ssl_crlpath", SSL_TEST_DATA_DIR "/crldir"},
         {"server_ssl_capath", "@capath@"},
         {"server_ssl_mode", "REQUIRED"},
         {"server_ssl_verify", "VERIFY_CA"},
     },
     [](const std::string &router_host, uint16_t router_port) {
       mysqlrouter::MySQLSession sess;

       try {
         sess.connect(router_host, router_port,
                      "someuser",  // user
                      "somepass",  // pass
                      "",          // socket
                      ""           // schema
         );
       } catch (const mysqlrouter::MySQLSession::Error &e) {
         FAIL() << e.what();
       }
     }},
};

INSTANTIATE_TEST_SUITE_P(ServerTls, SplicerConnectParamTest,
                         testing::ValuesIn(splicer_connect_tls_params),
                         [](auto &info) {
                           auto p = info.param;
                           return p.test_name;
                         });

/**
 *
 */
TEST_F(SplicerTest, classic_protocol_default_preferred_as_client) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();
  launch_mysql_server_mock(mock_file, server_port);

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations",
               mock_server_host_ + ":" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_key", valid_ssl_key_},
              {"client_ssl_cert", valid_ssl_cert_},
          })},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));

  mysqlrouter::MySQLSession sess;

  sess.connect(router_host_, router_port,
               "someuser",  // user
               "somepass",  // pass
               "",          // socket
               ""           // schema
  );
}

struct SplicerParam {
  SslMode client_ssl_mode;
  SslMode server_ssl_mode;
  mysql_ssl_mode my_ssl_mode;
  mysql_ssl_mode mock_ssl_mode;

  int expected_success;
  bool expect_client_encrypted;
  bool expect_server_encrypted;
};

void PrintTo(const SplicerParam &p, std::ostream *os) {
  *os << mysqlrouter::MySQLSession::ssl_mode_to_string(p.my_ssl_mode)
      << " -> (client-ssl-mode: " << ssl_mode_to_string(p.client_ssl_mode)
      << ", server-ssl-mode: " << ssl_mode_to_string(p.server_ssl_mode)
      << ") expected to " << (p.expected_success == 0 ? "succeed" : "fail");
}

class SplicerParamTest : public SplicerTest,
                         public ::testing::WithParamInterface<SplicerParam> {};

/**
 * classic protocol connections.
 */
TEST_P(SplicerParamTest, classic_protocol) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();

  auto mock_server_cmdline_args =
      mysql_server_mock_cmdline_args(mock_file, server_port);

  // enable SSL support on the mock-server.
  if (GetParam().mock_ssl_mode != mysql_ssl_mode::SSL_MODE_DISABLED) {
    std::initializer_list<std::pair<const char *, const char *>> mock_opts = {
        {"--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
        {"--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
        {"--ssl-mode", "PREFERRED"}};

    for (const auto &arg : mock_opts) {
      mock_server_cmdline_args.emplace_back(arg.first);
      mock_server_cmdline_args.emplace_back(arg.second);
    }
  }

  launch_mysql_server_mock(mock_server_cmdline_args, server_port);

  const std::string destination(mock_server_host_ + ":" +
                                std::to_string(server_port));
  const std::string mock_username = "someuser";
  const std::string mock_password = "somepass";

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", destination},
              {"routing_strategy", "round-robin"},
              {"client_ssl_key", valid_ssl_key_},
              {"client_ssl_cert", valid_ssl_cert_},
              {"client_ssl_mode",
               ssl_mode_to_string(GetParam().client_ssl_mode)},
              {"server_ssl_mode",
               ssl_mode_to_string(GetParam().server_ssl_mode)},
          })},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));

  mysqlrouter::MySQLSession sess;

  sess.set_ssl_options(GetParam().my_ssl_mode,
                       "",  // tls-version
                       "",  // cipher
                       "",  // ca
                       "",  // capath
                       "",  // crl
                       ""   // crlpath
  );

  try {
    SCOPED_TRACE("// connection to router");
    sess.connect(router_host_, router_port,
                 mock_username,  // user
                 mock_password,  // pass
                 "",             // socket
                 ""              // schema
    );

    EXPECT_THAT(GetParam().expected_success, 0)
        << "expected connect to fail, but it succeeded.";

    const bool is_encrypted{sess.ssl_cipher() != nullptr};

    SCOPED_TRACE("// checking connection is (not) encrypted");
    EXPECT_EQ(is_encrypted, GetParam().expect_client_encrypted);

    SCOPED_TRACE("// checking server's ssl_cipher");
    try {
      const auto row = sess.query_one("show status like 'ssl_cipher'");
      ASSERT_TRUE(row) << "<show status like 'ssl_cipher'> returned no row";
      ASSERT_EQ(row->size(), 2);

      if (GetParam().expect_server_encrypted) {
        EXPECT_STRNE((*row)[1], "");
      } else {
        EXPECT_STREQ((*row)[1], "");
      }
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      FAIL() << e.what();
    }

    SCOPED_TRACE("// SELECT <- 15Mbyte");
    try {
      const auto row =
          sess.query_one("select repeat('a', 15 * 1024 * 1024) as a");
      ASSERT_EQ(row->size(), 1);

      EXPECT_EQ((*row)[0], std::string(15L * 1024 * 1024, 'a'));
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      FAIL() << e.what();
    }

    SCOPED_TRACE("// SELECT -> 4k");
    try {
      const auto row = sess.query_one("select length(" +
                                      std::string(4097, 'a') + ") as length");
      ASSERT_EQ(row->size(), 1);

      EXPECT_STREQ((*row)[0], "4097");
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      FAIL() << e.what();
    }

  } catch (const mysqlrouter::MySQLSession::Error &e) {
    auto expected_code = GetParam().expected_success;

    // expected_code is in XError coded

    if (expected_code == 5001) {
      expected_code = 2026;
    } else if (expected_code == 3159) {
      expected_code = 2026;
    }

    EXPECT_THAT(expected_code, e.code()) << e.what();
  }
}

namespace xcl {
std::ostream &operator<<(std::ostream &os, XError const &err) {
  os << err.error() << ": " << err.what();

  return os;
}
}  // namespace xcl

/**
 * check xproto connection works as expected.
 */
TEST_P(SplicerParamTest, xproto) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();
  const auto server_port_x = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();

  auto mock_server_cmdline_args =
      mysql_server_mock_cmdline_args(mock_file, server_port, 0,  // http_port
                                     server_port_x);

  // enable SSL support on the mock-server.
  if (GetParam().mock_ssl_mode != mysql_ssl_mode::SSL_MODE_DISABLED) {
    std::initializer_list<std::pair<const char *, const char *>> mock_opts = {
        {"--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
        {"--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
        {"--ssl-mode", "PREFERRED"}};

    for (const auto &arg : mock_opts) {
      mock_server_cmdline_args.emplace_back(arg.first);
      mock_server_cmdline_args.emplace_back(arg.second);
    }
  }

  launch_mysql_server_mock(mock_server_cmdline_args, server_port);

  const std::string destination(mock_server_host_ + ":" +
                                std::to_string(server_port_x));
  const std::string mock_username = "someuser";
  const std::string mock_password = "somepass";

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", destination},
              {"routing_strategy", "round-robin"},
              {"client_ssl_key", valid_ssl_key_},
              {"client_ssl_cert", valid_ssl_cert_},
              {"client_ssl_mode",
               ssl_mode_to_string(GetParam().client_ssl_mode)},
              {"server_ssl_mode",
               ssl_mode_to_string(GetParam().server_ssl_mode)},
              {"protocol", "x"},
          })},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));

  auto sess = xcl::create_session();
  ASSERT_THAT(
      sess->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                             mysqlrouter::MySQLSession::ssl_mode_to_string(
                                 GetParam().my_ssl_mode)),
      ::testing::Truly([](const xcl::XError &xerr) { return !xerr; }));

  // use a auth-method that works over plaintext, server-side channels
  if (GetParam().client_ssl_mode == SslMode::kPreferred &&
      GetParam().server_ssl_mode == SslMode::kDisabled &&
      GetParam().my_ssl_mode != SSL_MODE_DISABLED) {
    // client is TLS and will default to PLAIN auth, but it will fail on the
    // server side as the server's connection plaintext (and PLAIN is only
    // allowed over secure channels).
    sess->set_mysql_option(xcl::XSession::Mysqlx_option::Authentication_method,
                           "MYSQL41");
  }

  SCOPED_TRACE("// check the TLS capability is announced properly.");
  {
    auto &xproto = sess->get_protocol();
    auto &xconn = xproto.get_connection();

    const auto connect_err =
        xconn.connect(router_host_, router_port, xcl::Internet_protocol::Any);

    ASSERT_EQ(connect_err.error(), 0) << connect_err.what();

    xcl::XError xerr;
    const auto caps = xproto.execute_fetch_capabilities(&xerr);

    ASSERT_EQ(xerr.error(), 0) << xerr.what();
    ASSERT_TRUE(caps);

    bool has_tls_cap{false};
    for (auto const &cap : caps->capabilities()) {
      ASSERT_TRUE(cap.has_name());
      if (cap.name() == "tls") {
        ASSERT_TRUE(cap.has_value());
        ASSERT_TRUE(cap.value().has_scalar());
        ASSERT_TRUE(cap.value().scalar().has_v_bool());
        has_tls_cap = cap.value().scalar().v_bool();
      }
    }

    if (GetParam().client_ssl_mode == SslMode::kDisabled ||
        (GetParam().client_ssl_mode == SslMode::kPassthrough &&
         GetParam().mock_ssl_mode == SSL_MODE_DISABLED)) {
      EXPECT_FALSE(has_tls_cap);
    } else {
      EXPECT_TRUE(has_tls_cap);
    }

    xconn.close();
  }

  ASSERT_THAT(sess->connect(router_host_.c_str(), router_port,
                            mock_username.c_str(), mock_password.c_str(), ""),
              ::testing::Truly([](auto const &err) {
                return err.error() == GetParam().expected_success;
              }))
      << GetParam().expected_success;

  if (GetParam().expected_success == 0) {
    {
      xcl::XError xerr;
      const auto result =
          sess->execute_sql("show status like 'mysqlx_ssl_cipher'", &xerr);
      ASSERT_TRUE(result) << xerr;

      if (!result->has_resultset()) {
        FAIL() << xerr.what();
      } else {
        const auto row = result->get_next_row(&xerr);
        ASSERT_TRUE(row) << xerr;
        std::string field;
        ASSERT_TRUE(row->get_string(1, &field));

        if (GetParam().expect_server_encrypted) {
          EXPECT_NE(field, "");
        } else {
          EXPECT_EQ(field, "");
        }
      }
    }

    {
      xcl::XError xerr;
      const auto result =
          sess->execute_sql("select repeat('a', 15 * 1024 * 1024) as a", &xerr);
      ASSERT_TRUE(result) << xerr;

      const auto row = result->get_next_row();
      ASSERT_NE(row, nullptr);
      std::string field;
      ASSERT_TRUE(row->get_string(0, &field));

      EXPECT_EQ(field, std::string(15 * 1024 * 1024, 'a'));
    }
  }
}

/**
 * compression should fail, if not passthrough.
 */
TEST_P(SplicerParamTest, xproto_compression) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();
  const auto server_port_x = port_pool_.get_next_available();

  const std::string mock_file = get_data_dir().join("tls_endpoint.js").str();

  auto mock_server_cmdline_args =
      mysql_server_mock_cmdline_args(mock_file, server_port, 0,  // http_port
                                     server_port_x);

  // enable SSL support on the mock-server.
  if (GetParam().mock_ssl_mode != mysql_ssl_mode::SSL_MODE_DISABLED) {
    std::initializer_list<std::pair<const char *, const char *>> mock_opts = {
        {"--ssl-cert", SSL_TEST_DATA_DIR "crl-server-cert.pem"},
        {"--ssl-key", SSL_TEST_DATA_DIR "crl-server-key.pem"},
        {"--ssl-mode", "PREFERRED"}};

    for (const auto &arg : mock_opts) {
      mock_server_cmdline_args.emplace_back(arg.first);
      mock_server_cmdline_args.emplace_back(arg.second);
    }
  }

  launch_mysql_server_mock(mock_server_cmdline_args, server_port);

  const std::string destination(mock_server_host_ + ":" +
                                std::to_string(server_port_x));
  const std::string mock_username = "someuser";
  const std::string mock_password = "somepass";

  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", destination},
              {"routing_strategy", "round-robin"},
              {"client_ssl_key", valid_ssl_key_},
              {"client_ssl_cert", valid_ssl_cert_},
              {"client_ssl_mode",
               ssl_mode_to_string(GetParam().client_ssl_mode)},
              {"server_ssl_mode",
               ssl_mode_to_string(GetParam().server_ssl_mode)},
              {"protocol", "x"},
          })},
      "");
  auto conf_file = create_config_file(conf_dir_.name(), config);

  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_ready(router_port));

  auto sess = xcl::create_session();
  ASSERT_THAT(
      sess->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                             mysqlrouter::MySQLSession::ssl_mode_to_string(
                                 GetParam().my_ssl_mode)),
      ::testing::Truly([](const xcl::XError &xerr) { return !xerr; }));

  // use a auth-method that works over plaintext, server-side channels
  if (GetParam().client_ssl_mode == SslMode::kPreferred &&
      GetParam().server_ssl_mode == SslMode::kDisabled &&
      GetParam().my_ssl_mode != SSL_MODE_DISABLED) {
    // client is TLS and will default to PLAIN auth, but it will fail on the
    // server side as the server's connection plaintext (and PLAIN is only
    // allowed over secure channels).
    sess->set_mysql_option(xcl::XSession::Mysqlx_option::Authentication_method,
                           "MYSQL41");
  }

  SCOPED_TRACE("// check the compression capability is announced properly.");
  {
    auto &xproto = sess->get_protocol();
    auto &xconn = xproto.get_connection();

    const auto connect_err =
        xconn.connect(router_host_, router_port, xcl::Internet_protocol::Any);

    ASSERT_EQ(connect_err.error(), 0) << connect_err.what();

    // try to set the compression capability.
    {
      Mysqlx::Connection::CapabilitiesSet caps;

      auto *cap = caps.mutable_capabilities()->add_capabilities();
      cap->mutable_name()->assign("compression");
      auto *cap_value = cap->mutable_value();
      cap_value->set_type(Mysqlx::Datatypes::Any_Type::Any_Type_OBJECT);
      auto *cap_value_obj = cap_value->mutable_obj();
      {
        auto *cap_value_obj_fld = cap_value_obj->add_fld();
        cap_value_obj_fld->mutable_key()->assign("algorithm");
        auto *fld_value = cap_value_obj_fld->mutable_value();
        fld_value->set_type(Mysqlx::Datatypes::Any_Type::Any_Type_SCALAR);
        auto *fld_scalar = fld_value->mutable_scalar();
        fld_scalar->set_type(
            Mysqlx::Datatypes::Scalar_Type::Scalar_Type_V_STRING);
        fld_scalar->mutable_v_string()->mutable_value()->assign(
            "deflate_stream");
      }

      auto xerr = xproto.execute_set_capability(caps);
      // Invalid or unsupported value for 'compression.algorithm'
      EXPECT_EQ(xerr.error(), 5175) << xerr.what();
    }

    xconn.close();
  }
}

const std::array<SplicerParam, 39> splicer_server_plain_params = {{
    // disabled - disabled
    {SslMode::kDisabled, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_00_01
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_10_01
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kDisabled,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},

    // disabled - preferred
    {SslMode::kDisabled, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_00_02,
                                               // RT2_CERT_KEY_OPERATION_01
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_10_02
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kPreferred,  // REQUIRED + kDisabled = fail
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},

    // disabled - required
    {SslMode::kDisabled, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_00_03
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kDisabled, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_10_03
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kDisabled, SslMode::kRequired,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},  // REQUIRED + kDisabled = fail

    // disabled - as-client
    {SslMode::kDisabled, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_00_04
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_10_04
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kAsClient,  // REQUIRED + kDisabled = fail
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},

    // preferred - disabled
    {SslMode::kPreferred, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_00_05
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_10_05
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},
    {SslMode::kPreferred, SslMode::kDisabled,                               //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},

    // preferred - preferred
    {SslMode::kPreferred, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_00_06,
                                                // RT2_CERT_KEY_OPERATION_03
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_10_06
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},
    {SslMode::kPreferred, SslMode::kPreferred,                              //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},

    // preferred - required
    {SslMode::kPreferred, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_00_07
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kPreferred, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_10_07
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kPreferred, SslMode::kRequired,                               //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},

    // preferred - as-client
    {SslMode::kPreferred, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_00_08
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_10_08
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kAsClient,                               //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},

    // required - disabled
    {SslMode::kRequired, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_00_09,
                                              // RT2_CERT_KEY_OPERATION_04
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},                     // client-side fails
    {SslMode::kRequired, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_10_09
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},
    {SslMode::kRequired, SslMode::kDisabled,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},

    // required - preferred
    {SslMode::kRequired, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_00_10
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001,                                                                  //
     false, false},                            // client
    {SslMode::kRequired, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_10_10
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},
    {SslMode::kRequired, SslMode::kPreferred,  // tls <-> plain
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, true, false},

    // required - required
    {SslMode::kRequired, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_00_11
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},                     // DISABLED + kRequired = fail
    {SslMode::kRequired, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_10_11
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kRequired, SslMode::kRequired,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},

    // required - as-client
    {SslMode::kRequired, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_00_12
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},                     // client fails
    {SslMode::kRequired, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_10_12
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},
    {SslMode::kRequired, SslMode::kAsClient,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     3159, false, false},

    // passthrough - as-client
    {SslMode::kPassthrough, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_00_16,
                                                 // RT2_CERT_KEY_OPERATION_02
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPassthrough, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_10_16
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     0, false, false},
    {SslMode::kPassthrough, SslMode::kAsClient,                             //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_DISABLED,  //
     5001, false, false},

}};

INSTANTIATE_TEST_SUITE_P(
    ServerPlain, SplicerParamTest,
    testing::ValuesIn(splicer_server_plain_params), [](auto &info) {
      auto p = info.param;
      return "ssl_mode_"s + ssl_mode_to_string(p.client_ssl_mode) + "_"s +
             ssl_mode_to_string(p.server_ssl_mode) + "_"s +
             mysqlrouter::MySQLSession::ssl_mode_to_string(p.my_ssl_mode) +
             "_"s + (p.expected_success == 0 ? "succeed" : "fail");
    });

const std::array<SplicerParam, 39> splicer_server_tls_params = {{
    // disabled - disabled
    {SslMode::kDisabled, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_01_01
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_11_01
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kDisabled,                                 //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},

    // disabled - preferred
    {SslMode::kDisabled, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_01_02
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kDisabled, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_11_02
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kDisabled, SslMode::kPreferred,  // REQUIRED + kDisabled = fail
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},

    // disabled - required
    {SslMode::kDisabled, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_01_03
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kDisabled, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_11_03
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kDisabled, SslMode::kRequired,                                 //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},  // REQUIRED + kDisabled = fail

    // disabled - as-client
    {SslMode::kDisabled, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_01_04
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_11_04
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kDisabled, SslMode::kAsClient,  // REQUIRED + kDisabled = fail
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},

    // preferred - disabled
    {SslMode::kPreferred, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_01_05
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_11_05
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, false},
    {SslMode::kPreferred, SslMode::kDisabled,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, false},

    // preferred - preferred
    {SslMode::kPreferred, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_01_06
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kPreferred, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_11_06
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kPreferred, SslMode::kPreferred,                               //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // preferred - required
    {SslMode::kPreferred, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_01_07
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, true},
    {SslMode::kPreferred, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_11_07
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kPreferred, SslMode::kRequired,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // preferred - as-client
    {SslMode::kPreferred, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_01_08
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kPreferred, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_11_08
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kPreferred, SslMode::kAsClient,                                //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // required - disabled
    {SslMode::kRequired, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_01_09
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},                     // client-side fails
    {SslMode::kRequired, SslMode::kDisabled,  // RT2_CONN_TYPE_RSLN_11_09
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, false},
    {SslMode::kRequired, SslMode::kDisabled,                                 //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, false},

    // required - preferred
    {SslMode::kRequired, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_01_10
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001,                                                                   //
     false, false},                            // client
    {SslMode::kRequired, SslMode::kPreferred,  // RT2_CONN_TYPE_RSLN_11_10
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kRequired, SslMode::kPreferred,  // tls <-> plain
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // required - required
    {SslMode::kRequired, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_01_11
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},                     // DISABLED + kRequired = fail
    {SslMode::kRequired, SslMode::kRequired,  // RT2_CONN_TYPE_RSLN_11_11
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kRequired, SslMode::kRequired,                                 //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // required - as-client
    {SslMode::kRequired, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_01_12
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     5001, false, false},                     // client fails
    {SslMode::kRequired, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_11_12
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kRequired, SslMode::kAsClient,                                 //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},

    // passthrough - as-client
    {SslMode::kPassthrough, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_01_16
     mysql_ssl_mode::SSL_MODE_DISABLED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, false, false},
    {SslMode::kPassthrough, SslMode::kAsClient,  // RT2_CONN_TYPE_RSLN_11_16
     mysql_ssl_mode::SSL_MODE_PREFERRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
    {SslMode::kPassthrough, SslMode::kAsClient,                              //
     mysql_ssl_mode::SSL_MODE_REQUIRED, mysql_ssl_mode::SSL_MODE_PREFERRED,  //
     0, true, true},
}};

INSTANTIATE_TEST_SUITE_P(
    ServerTls, SplicerParamTest, testing::ValuesIn(splicer_server_tls_params),
    [](auto &info) {
      auto p = info.param;
      return "ssl_mode_"s + ssl_mode_to_string(p.client_ssl_mode) + "_"s +
             ssl_mode_to_string(p.server_ssl_mode) + "_"s +
             mysqlrouter::MySQLSession::ssl_mode_to_string(p.my_ssl_mode) +
             "_"s + (p.expected_success == 0 ? "succeed" : "fail");
    });

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
