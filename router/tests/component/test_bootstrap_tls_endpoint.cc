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

#include <sstream>  // istringstream
#include <string>
#include <vector>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/string_utils.h"  // join
#include "mysqlrouter/cluster_metadata.h"
#include "router_component_test.h"
#include "router_test_helpers.h"  // init_windows_socket()

using namespace std::chrono_literals;
using namespace std::string_literals;

struct BootstrapTlsEndpointFailParams {
  const std::string test_name;

  const std::vector<std::string> cmdline_args;

  stdx::expected<void, std::string> expected_result;
};

class BootstrapTlsEndpointFail
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTlsEndpointFailParams> {};

TEST_P(BootstrapTlsEndpointFail, check) {
  // launch the router in bootstrap mode
  std::vector<std::string> cmdline_args = {"--bootstrap", "1.2.3.4:5678"};

  for (auto const &cmdline_arg : GetParam().cmdline_args) {
    cmdline_args.push_back(cmdline_arg);
  }

  auto expected_exit_code =
      GetParam().expected_result ? EXIT_SUCCESS : EXIT_FAILURE;

  auto &router = launch_router_for_bootstrap(cmdline_args, expected_exit_code);

  EXPECT_NO_THROW(router.wait_for_exit());

  if (!GetParam().expected_result) {
    // check if the bootstrapping was successful
    EXPECT_THAT(router.get_full_output(),
                ::testing::HasSubstr(GetParam().expected_result.error()));
  }
}

const BootstrapTlsEndpointFailParams bootstrap_tls_endpoint_fail_params[] = {
    // client-ssl-mode
    {"client_ssl_mode_invalid",  // BS_ARGS_BAD_01
     {"--client-ssl-mode", "foo"},
     stdx::unexpected("value 'foo' provided to --client-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,PASSTHROUGH"s)},
    {"client_ssl_mode_empty",  // BS_ARGS_BAD_02
     {"--client-ssl-mode", ""},
     stdx::unexpected("value '' provided to --client-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,PASSTHROUGH"s)},
    {"client_ssl_mode_space",  // BS_ARGS_BAD_03
     {"--client-ssl-mode", " "},
     stdx::unexpected("value ' ' provided to --client-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,PASSTHROUGH"s)},
    {"client_ssl_mode_no_value",  // BS_ARGS_BAD_04
     {"--client-ssl-mode", "--foo"},
     stdx::unexpected(
         "Error: option '--client-ssl-mode' expects a value, got nothing"s)},
    {"client_ssl_mode_as_last_arg",  // BS_ARGS_BAD_05
     {"--client-ssl-mode"},
     stdx::unexpected(
         "Error: option '--client-ssl-mode' expects a value, got nothing"s)},

    // server-ssl-mode
    {"server_ssl_mode_invalid",  // BS_ARGS_BAD_06
     {"--server-ssl-mode", "foo"},
     stdx::unexpected("value 'foo' provided to --server-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,AS_CLIENT"s)},
    {"server_ssl_mode_empty",  // BS_ARGS_BAD_07
     {"--server-ssl-mode", ""},
     stdx::unexpected("value '' provided to --server-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,AS_CLIENT"s)},
    {"server_ssl_mode_space",  // BS_ARGS_BAD_08
     {"--server-ssl-mode", " "},
     stdx::unexpected("value ' ' provided to --server-ssl-mode is not one "
                      "of DISABLED,PREFERRED,REQUIRED,AS_CLIENT"s)},
    {"server_ssl_mode_no_value",  // BS_ARGS_BAD_09
     {"--server-ssl-mode", "--foo"},
     stdx::unexpected(
         "Error: option '--server-ssl-mode' expects a value, got nothing"s)},
    {"server_ssl_mode_as_last_arg",  // BS_ARGS_BAD_10
     {"--server-ssl-mode"},
     stdx::unexpected(
         "Error: option '--server-ssl-mode' expects a value, got nothing"s)},

    // server-ssl-verify
    {"server_ssl_verify_invalid",  // BS_ARGS_BAD_11
     {"--server-ssl-verify", "foo"},
     stdx::unexpected("value 'foo' provided to --server-ssl-verify is not one "
                      "of DISABLED,VERIFY_CA,VERIFY_IDENTITY"s)},
    {"server_ssl_verify_empty",  // BS_ARGS_BAD_12
     {"--server-ssl-verify", ""},
     stdx::unexpected("value '' provided to --server-ssl-verify is not one "
                      "of DISABLED,VERIFY_CA,VERIFY_IDENTITY"s)},
    {"server_ssl_verify_no_value",  // BS_ARGS_BAD_13
     {"--server-ssl-verify", "--foo"},
     stdx::unexpected(
         "Error: option '--server-ssl-verify' expects a value, got nothing"s)},
    {"server_ssl_verify_as_last_arg",  // BS_ARGS_BAD_14
     {"--server-ssl-verify"},
     stdx::unexpected(
         "Error: option '--server-ssl-verify' expects a value, got nothing"s)},

    // client-ssl-cipher
    {"client_ssl_cipher_empty",  // BS_ARGS_EMPTY_Q_01
     {"--client-ssl-cipher", ""},
     stdx::unexpected(
         "Value for option '--client-ssl-cipher' can't be empty"s)},

    // client-ssl-curves
    {"client_ssl_curves_empty",  // BS_ARGS_EMPTY_Q_02
     {"--client-ssl-curves", ""},
     stdx::unexpected(
         "Value for option '--client-ssl-curves' can't be empty"s)},

    // client-ssl-cert
    {"client_ssl_cert_empty",  // BS_ARGS_EMPTY_Q_03
     {"--client-ssl-cert", ""},
     stdx::unexpected("Value for option '--client-ssl-cert' can't be empty"s)},
    {"client_ssl_cert_without_key",
     {"--client-ssl-cert", "foo"},
     stdx::unexpected(
         "If --client-ssl-cert is set, --client-ssl-key can't be empty"s)},

    // client-ssl-key
    {"client_ssl_key_empty",  // BS_ARGS_EMPTY_Q_04
     {"--client-ssl-key", ""},
     stdx::unexpected("Value for option '--client-ssl-key' can't be empty"s)},
    {"client_ssl_key_without_cert",
     {"--client-ssl-key", "foo"},
     stdx::unexpected(
         "If --client-ssl-key is set, --client-ssl-cert can't be empty"s)},

    // client-ssl-dh-params
    {"client_ssl_dh_params_empty",  // BS_ARGS_EMPTY_Q_05
     {"--client-ssl-dh-params", ""},
     stdx::unexpected(
         "Value for option '--client-ssl-dh-params' can't be empty"s)},

    // server-ssl-cipher
    {"server_ssl_cipher_empty",  // BS_ARGS_EMPTY_Q_06
     {"--server-ssl-cipher", ""},
     stdx::unexpected(
         "Value for option '--server-ssl-cipher' can't be empty"s)},

    // server-ssl-curves
    {"server_ssl_curves_empty",  // BS_ARGS_EMPTY_Q_07
     {"--server-ssl-curves", ""},
     stdx::unexpected(
         "Value for option '--server-ssl-curves' can't be empty"s)},

    // server-ssl-ca
    {"server_ssl_ca_empty",  // BS_ARGS_EMPTY_Q_08
     {"--server-ssl-ca", ""},
     stdx::unexpected("Value for option '--server-ssl-ca' can't be empty"s)},

    // server-ssl-capath
    {"server_ssl_capath_empty",  // BS_ARGS_EMPTY_Q_09
     {"--server-ssl-capath", ""},
     stdx::unexpected(
         "Value for option '--server-ssl-capath' can't be empty"s)},

    // server-ssl-crl
    {"server_ssl_crl_empty",  // BS_ARGS_EMPTY_Q_10
     {"--server-ssl-crl", ""},
     stdx::unexpected("Value for option '--server-ssl-crl' can't be empty"s)},

    // server-ssl-crlpath
    {"server_ssl_crlpath_empty",  // BS_ARGS_EMPTY_Q_11
     {"--server-ssl-crlpath", ""},
     stdx::unexpected(
         "Value for option '--server-ssl-crlpath' can't be empty"s)},

};

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapTlsEndpointFail,
    ::testing::ValuesIn(bootstrap_tls_endpoint_fail_params),
    [](auto const &p) { return p.param.test_name; });

//

class BootstrapTlsEndpointWithoutBootstrapFail
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTlsEndpointFailParams> {};

TEST_P(BootstrapTlsEndpointWithoutBootstrapFail, check) {
  // don't set the --bootstrap option.
  std::vector<std::string> cmdline_args;

  for (auto const &cmdline_arg : GetParam().cmdline_args) {
    cmdline_args.push_back(cmdline_arg);
  }

  auto expected_exit_code =
      GetParam().expected_result ? EXIT_SUCCESS : EXIT_FAILURE;

  auto &router = launch_router_for_bootstrap(cmdline_args, expected_exit_code);

  EXPECT_NO_THROW(router.wait_for_exit());

  if (!GetParam().expected_result) {
    // check if the bootstrapping was successful
    EXPECT_THAT(router.get_full_output(),
                ::testing::HasSubstr(GetParam().expected_result.error()));
  }
}

const BootstrapTlsEndpointFailParams bootstrap_tls_endpoint_without_bootstrap_fail_params[] = {
    {"client_ssl_mode",  // BS_NOBS_C_01
     {"--client-ssl-mode", "disabled"},
     stdx::unexpected(
         "Error: Option --client-ssl-mode can only be used together with -B/--bootstrap"s)},
    {"client_ssl_cipher",  // BS_NOBS_C_02
     {"--client-ssl-cipher", "some-valid-cipher"},
     stdx::unexpected(
         "Error: Option --client-ssl-cipher can only be used together with -B/--bootstrap"s)},
    {"client_ssl_curves",  // BS_NOBS_C_03
     {"--client-ssl-curves", "some-valid-curves"},
     stdx::unexpected(
         "Error: Option --client-ssl-curves can only be used together with -B/--bootstrap"s)},
    {"client_ssl_cert_and_key",  // BS_NOBS_C_04
     {"--client-ssl-cert", "some-cert", "--client-ssl-key", "some-key"},
     stdx::unexpected(
         "Error: Option --client-ssl-cert can only be used together with -B/--bootstrap"s)},
    {"client_ssl_key_and_cert",  // BS_NOBS_C_05
     {"--client-ssl-key", "some-key", "--client-ssl-cert", "some-cert"},
     stdx::unexpected(
         "Error: Option --client-ssl-key can only be used together with -B/--bootstrap"s)},
    {"client_ssl_dh_params",  // BS_NOBS_C_06
     {"--client-ssl-dh-params", "some-valid-dh-params"},
     stdx::unexpected(
         "Error: Option --client-ssl-dh-params can only be used together with -B/--bootstrap"s)},
    {"server_ssl_mode",  // BS_NOBS_S_01
     {"--server-ssl-mode", "disabled"},
     stdx::unexpected(
         "Error: Option --server-ssl-mode can only be used together with -B/--bootstrap"s)},
    {"server_ssl_verify",  // BS_NOBS_S_02
     {"--server-ssl-verify", "disabled"},
     stdx::unexpected(
         "Error: Option --server-ssl-verify can only be used together with -B/--bootstrap"s)},
    {"server_ssl_cipher",  // BS_NOBS_S_03
     {"--server-ssl-cipher", "some-valid-ciphers"},
     stdx::unexpected(
         "Error: Option --server-ssl-cipher can only be used together with -B/--bootstrap"s)},
    {"server_ssl_curves",  // BS_NOBS_S_04
     {"--server-ssl-curves", "some-valid-curves"},
     stdx::unexpected(
         "Error: Option --server-ssl-curves can only be used together with -B/--bootstrap"s)},
    {"server_ssl_ca",  // BS_NOBS_S_05
     {"--server-ssl-ca", "some-valid-ca-file.pem"},
     stdx::unexpected(
         "Error: Option --server-ssl-ca can only be used together with -B/--bootstrap"s)},
    {"server_ssl_capath",  // BS_NOBS_S_05
     {"--server-ssl-capath", "some-valid-capath"},
     stdx::unexpected(
         "Error: Option --server-ssl-capath can only be used together with -B/--bootstrap"s)},
    {"server_ssl_crl",  // BS_NOBS_S_05
     {"--server-ssl-crl", "some-valid-crl-file.pem"},
     stdx::unexpected(
         "Error: Option --server-ssl-crl can only be used together with -B/--bootstrap"s)},
    {"server_ssl_crlpath",  // BS_NOBS_S_05
     {"--server-ssl-crlpath", "some-valid-crlpath"},
     stdx::unexpected(
         "Error: Option --server-ssl-crlpath can only be used together with -B/--bootstrap"s)},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapTlsEndpointWithoutBootstrapFail,
    ::testing::ValuesIn(bootstrap_tls_endpoint_without_bootstrap_fail_params),
    [](auto const &p) { return p.param.test_name; });

// successful bootstraps

struct BootstrapTlsEndpointParams {
  const std::string test_name;

  const std::vector<std::string> cmdline_args;

  std::function<void(const std::vector<std::string> &config_lines)> checker;
};

class BootstrapTlsEndpointWithoutCertGeneration
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTlsEndpointParams> {};

TEST_P(BootstrapTlsEndpointWithoutCertGeneration, succeeds) {
  auto cmdline_args = GetParam().cmdline_args;

  std::string server_cert_pem{SSL_TEST_DATA_DIR "server-cert.pem"};
  std::string server_key_pem{SSL_TEST_DATA_DIR "server-key.pem"};

  ASSERT_TRUE(mysql_harness::Path(server_cert_pem).exists());
  ASSERT_TRUE(mysql_harness::Path(server_key_pem).exists());

  // add arguments that skip cert-generation to speed up the tests.
  cmdline_args.emplace_back("--disable-rest");
  cmdline_args.emplace_back("--client-ssl-cert");
  cmdline_args.emplace_back(server_cert_pem);
  cmdline_args.emplace_back("--client-ssl-key");
  cmdline_args.emplace_back(server_key_pem);

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {Config{
          "127.0.0.1",
          port_pool_.get_next_available(),
          port_pool_.get_next_available(),
          "bootstrap_gr.js",
      }},
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"}, 5s, {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

const BootstrapTlsEndpointParams
    bootstrap_tls_endpoint_without_cert_generation_params[] = {
        {"all_defaults",  // BS_VERIFY_DEFAULT_01
         {},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(
               config_file_lines,
               ::testing::IsSupersetOf({
                   "client_ssl_mode=PREFERRED",
                   "server_ssl_mode=PREFERRED",
                   "client_ssl_cert=" SSL_TEST_DATA_DIR "server-cert.pem",
                   "client_ssl_key=" SSL_TEST_DATA_DIR "server-key.pem",
                   "server_ssl_verify=DISABLED",
               }));

           // not specified at command-line, no set in config
           ASSERT_THAT(config_file_lines,
                       ::testing::Not(::testing::Contains(
                           ::testing::HasSubstr("client_ssl_cipher"))));
           ASSERT_THAT(config_file_lines,
                       ::testing::Not(::testing::Contains(
                           ::testing::HasSubstr("client_ssl_curves"))));
           ASSERT_THAT(config_file_lines,
                       ::testing::Not(::testing::Contains(
                           ::testing::HasSubstr("server_ssl_cipher"))));
           ASSERT_THAT(config_file_lines,
                       ::testing::Not(::testing::Contains(
                           ::testing::HasSubstr("server_ssl_curves"))));
         }},
        {"client_ssl_mode_disabled",  // BS_ARGCASE_CM_01, BS_MODES_06
         {"--client-ssl-mode", "disaBLED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=DISABLED",
                                                "server_ssl_mode=PREFERRED"}));
         }},

        {"client_ssl_mode_disabled_disabled",  // BS_MODES_07
         {"--client-ssl-mode", "disaBLED", "--server-ssl-mode", "Disabled"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=DISABLED",
                                                "server_ssl_mode=DISABLED"}));
         }},
        {"client_ssl_mode_disabled_preferred",  // BS_MODES_08
         {"--client-ssl-mode", "disaBLED", "--server-ssl-mode", "pReferred"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=DISABLED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"client_ssl_mode_disabled_required",  // BS_MODES_09
         {"--client-ssl-mode", "disaBLED", "--server-ssl-mode", "Required"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=DISABLED",
                                                "server_ssl_mode=REQUIRED"}));
         }},
        {"client_ssl_mode_disabled_as_client",  // BS_MODES_10
         {"--client-ssl-mode", "disaBLED", "--server-ssl-mode", "as_Client"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=DISABLED",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},

        {"client_ssl_mode_preferred",  // BS_ARGCASE_CM_02, BS_MODES_11
         {"--client-ssl-mode", "prefeRRED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"client_ssl_mode_preferred_disabled",  // BS_MODES_12
         {"--client-ssl-mode", "Preferred", "--server-ssl-mode", "Disabled"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=DISABLED"}));
         }},
        {"client_ssl_mode_preferred_preferred",  // BS_MODES_13
         {"--client-ssl-mode", "Preferred", "--server-ssl-mode", "pReferred"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"client_ssl_mode_preferred_required",  // BS_MODES_14
         {"--client-ssl-mode", "Preferred", "--server-ssl-mode", "Required"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=REQUIRED"}));
         }},
        {"client_ssl_mode_preferred_as_client",  // BS_MODES_15
         {"--client-ssl-mode", "Preferred", "--server-ssl-mode", "as_Client"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},
        {"client_ssl_mode_required",  // BS_ARGCASE_CM_03, BS_MODES_16
         {"--client-ssl-mode", "requIRED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=REQUIRED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"client_ssl_mode_required_disabled",  // BS_MODES_17
         {"--client-ssl-mode", "ReQuired", "--server-ssl-mode", "Disabled"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=REQUIRED",
                                                "server_ssl_mode=DISABLED"}));
         }},
        {"client_ssl_mode_required_preferred",  // BS_MODES_18
         {"--client-ssl-mode", "ReQuired", "--server-ssl-mode", "pReferred"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=REQUIRED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"client_ssl_mode_required_required",  // BS_MODES_19
         {"--client-ssl-mode", "ReQuired", "--server-ssl-mode", "Required"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=REQUIRED",
                                                "server_ssl_mode=REQUIRED"}));
         }},
        {"client_ssl_mode_required_as_client",  // BS_MODES_20
         {"--client-ssl-mode", "ReQuired", "--server-ssl-mode", "as_Client"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=REQUIRED",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},
        {"client_ssl_mode_passthrough",  // BS_ARGCASE_CM_04,
                                         // BS_MODES_21
         {"--client-ssl-mode", "passthrough"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PASSTHROUGH",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},
        // BS_MODES_22, BS_MODES_23, BS_MODES_24 are failure cases below.
        {"client_ssl_mode_passthrough_as_client",  // BS_MODES_25
         {"--client-ssl-mode", "passthrough", "--server-ssl-mode", "as_client"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PASSTHROUGH",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},
        {"server_ssl_mode_disabled",  // BS_ARGCASE_SM_01, BS_MODES_02
         {"--server-ssl-mode", "disabLED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=DISABLED"}));
         }},
        {"server_ssl_mode_preferred",  // BS_ARGCASE_SM_02, BS_MODES_03
         {"--server-ssl-mode", "preferRED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=PREFERRED"}));
         }},
        {"server_ssl_mode_required",  // BS_ARGCASE_SM_03, BS_MODES_04
         {"--server-ssl-mode", "requirED"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=REQUIRED"}));
         }},
        {"server_ssl_mode_as_client",  // BS_ARGCASE_SM_04, BS_MODES_05
         {"--server-ssl-mode", "as_CLient"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({"client_ssl_mode=PREFERRED",
                                                "server_ssl_mode=AS_CLIENT"}));
         }},
        {"server_ssl_verify_disabled",  // BS_ARGCASE_SV_01
         {"--server-ssl-verify", "DIsabled"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines, ::testing::IsSupersetOf({
                                              "client_ssl_mode=PREFERRED",
                                              "server_ssl_mode=PREFERRED",
                                              "server_ssl_verify=DISABLED",
                                          }));
         }},
        {"server_ssl_verify_verify_identity",  // BS_ARGCASE_SV_02
         {"--server-ssl-verify", "verify_identITY", "--server-ssl-ca",
          "some-valid-ca-file.pem"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "server_ssl_verify=VERIFY_IDENTITY",
                       }));
         }},
        {"server_ssl_verify_verify_ca",  // BS_ARGCASE_SV_03
         {"--server-ssl-verify", "verify_CA", "--server-ssl-ca",
          "some-valid-ca-file.pem"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines, ::testing::IsSupersetOf({
                                              "client_ssl_mode=PREFERRED",
                                              "server_ssl_mode=PREFERRED",
                                              "server_ssl_verify=VERIFY_CA",
                                          }));
         }},
        {"client_ssl_cipher",  // BS_ARGS_ARBITRARY_01
         {"--client-ssl-cipher", "some-cipher-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "client_ssl_cipher=some-cipher-string",
                       }));
         }},
        {"client_ssl_curves",  // BS_ARGS_ARBITRARY_02
         {"--client-ssl-curves", "some-curves-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "client_ssl_curves=some-curves-string",
                       }));
         }},
        {"client_ssl_dh_params",  // BS_ARGS_ARBITRARY_05
         {"--client-ssl-dh-params", "some-dh-param-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "client_ssl_dh_params=some-dh-param-string",
                       }));
         }},
        {"server_ssl_cipher",  // BS_ARGS_ARBITRARY_06
         {"--server-ssl-cipher", "some-cipher-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "server_ssl_cipher=some-cipher-string",
                       }));
         }},
        {"server_ssl_curves",  // BS_ARGS_ARBITRARY_07
         {"--server-ssl-curves", "some-curves-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "server_ssl_curves=some-curves-string",
                       }));
         }},
        {"server_ssl_ca",  // BS_ARGS_ARBITRARY_08
         {"--server-ssl-ca", "some-ca-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines, ::testing::IsSupersetOf({
                                              "client_ssl_mode=PREFERRED",
                                              "server_ssl_mode=PREFERRED",
                                              "server_ssl_ca=some-ca-string",
                                          }));
         }},
        {"server_ssl_capath",  // BS_ARGS_ARBITRARY_09
         {"--server-ssl-capath", "some-capath-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "server_ssl_capath=some-capath-string",
                       }));
         }},
        {"server_ssl_crl",  // BS_ARGS_ARBITRARY_10
         {"--server-ssl-crl", "some-crl-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines, ::testing::IsSupersetOf({
                                              "client_ssl_mode=PREFERRED",
                                              "server_ssl_mode=PREFERRED",
                                              "server_ssl_crl=some-crl-string",
                                          }));
         }},
        {"server_ssl_crlpath",  // BS_ARGS_ARBITRARY_11
         {"--server-ssl-crlpath", "some-crlpath-string"},
         [](const std::vector<std::string> &config_file_lines) {
           ASSERT_THAT(config_file_lines,
                       ::testing::IsSupersetOf({
                           "client_ssl_mode=PREFERRED",
                           "server_ssl_mode=PREFERRED",
                           "server_ssl_crlpath=some-crlpath-string",
                       }));
         }},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapTlsEndpointWithoutCertGeneration,
    ::testing::ValuesIn(bootstrap_tls_endpoint_without_cert_generation_params),
    [](auto const &p) { return p.param.test_name; });

//
// bootstrap tests with certificate generation not suppressed
//

class BootstrapTlsEndpoint
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTlsEndpointParams> {};

TEST_P(BootstrapTlsEndpoint, succeeds) {
  auto cmdline_args = GetParam().cmdline_args;

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"},
      20s,  // 20 seconds as cert-generation may take a while on slow machines
      {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

TEST_P(BootstrapTlsEndpoint, existing_config) {
  auto cmdline_args = GetParam().cmdline_args;

  // create an existing config.
  uint16_t router_port{6446};  // doesn't matter
  uint16_t server_port{3306};  // doesn't matter
  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", "127.0.0.1:" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
          })},
      "");
  SCOPED_TRACE("starting router with config:\n" + config);
  auto conf_file = create_config_file(bootstrap_dir.name(), config);

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"},
      20s,  // 20 seconds as cert-generation may take a while on slow machines
      {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

TEST_P(BootstrapTlsEndpoint, existing_config_with_client_ssl_cert) {
  auto cmdline_args = GetParam().cmdline_args;

  // create an existing config.
  uint16_t router_port{6446};  // doesn't matter
  uint16_t server_port{3306};  // doesn't matter
  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", "127.0.0.1:" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
          })},
      "");
  SCOPED_TRACE("starting router with config:\n" + config);
  auto conf_file = create_config_file(
      bootstrap_dir.name(), config, nullptr, "mysqlrouter.conf",
      mysql_harness::join(
          std::vector<std::string>{
              mysql_harness::ConfigBuilder::build_pair(
                  std::make_pair("client_ssl_cert", "foo")),
          },
          ""));

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"},
      20s,  // 20 seconds as cert-generation may take a while on slow machines
      {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

TEST_P(BootstrapTlsEndpoint, existing_config_with_client_ssl_key) {
  auto cmdline_args = GetParam().cmdline_args;

  // create an existing config.
  uint16_t router_port{6446};  // doesn't matter
  uint16_t server_port{3306};  // doesn't matter
  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", "127.0.0.1:" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
          })},
      "\n");
  SCOPED_TRACE("starting router with config:\n" + config);
  auto conf_file = create_config_file(
      bootstrap_dir.name(), config, nullptr, "mysqlrouter.conf",
      mysql_harness::join(
          std::vector<std::string>{
              mysql_harness::ConfigBuilder::build_pair(
                  std::make_pair("client_ssl_key", "foo")),
          },
          "\n"));

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"},
      20s,  // 20 seconds as cert-generation may take a while on slow machines
      {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

TEST_P(BootstrapTlsEndpoint, existing_config_with_client_ssl_cert_and_key) {
  auto cmdline_args = GetParam().cmdline_args;

  // create an existing config.
  uint16_t router_port{6446};  // doesn't matter
  uint16_t server_port{3306};  // doesn't matter
  auto config = mysql_harness::join(
      std::vector<std::string>{mysql_harness::ConfigBuilder::build_section(
          "routing",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", "127.0.0.1:" + std::to_string(server_port)},
              {"routing_strategy", "round-robin"},
          })},
      "");
  SCOPED_TRACE("starting router with config:\n" + config);
  auto conf_file = create_config_file(
      bootstrap_dir.name(), config, nullptr, "mysqlrouter.conf",
      mysql_harness::join(
          std::vector<std::string>{
              mysql_harness::ConfigBuilder::build_pair(
                  std::make_pair("client_ssl_cert", "foo")),
              mysql_harness::ConfigBuilder::build_pair(
                  std::make_pair("client_ssl_key", "bar")),
          },
          "\n"));

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS,
      {"# MySQL Router configured"},
      20s,  // 20 seconds as cert-generation may take a while on slow machines
      {2, 0, 3}, cmdline_args));

  ASSERT_NE(config_file.size(), 0);

  std::vector<std::string> config_file_lines;
  {
    std::istringstream ss{get_file_output(config_file)};

    for (std::string line; std::getline(ss, line);) {
      config_file_lines.emplace_back(line);
    }
  }

  ASSERT_NO_FATAL_FAILURE(GetParam().checker(config_file_lines));
}

static void check_cert_generated(
    const std::vector<std::string> &config_file_lines,
    const std::string &expected_client_ssl_mode,
    const std::string &expected_server_ssl_mode) {
  ASSERT_THAT(config_file_lines,
              ::testing::IsSupersetOf({
                  "client_ssl_mode=" + expected_client_ssl_mode,
                  "server_ssl_mode=" + expected_server_ssl_mode,
                  "server_ssl_verify=DISABLED"s,
              }));

  // client_ssl_cert=${datadir}/router-cert.pem
  ASSERT_THAT(config_file_lines, ::testing::Contains(::testing::AllOf(
                                     ::testing::StartsWith("client_ssl_cert="),
                                     ::testing::EndsWith("router-cert.pem"))));

  // client_ssl_key=${datadir}/router-key.pem
  ASSERT_THAT(config_file_lines, ::testing::Contains(::testing::AllOf(
                                     ::testing::StartsWith("client_ssl_key="),
                                     ::testing::EndsWith("router-key.pem"))));

  // not specified at command-line, no set in config
  ASSERT_THAT(config_file_lines,
              ::testing::Not(::testing::Contains(
                  ::testing::HasSubstr("client_ssl_cipher"))));
  ASSERT_THAT(config_file_lines,
              ::testing::Not(::testing::Contains(
                  ::testing::HasSubstr("client_ssl_curves"))));
  ASSERT_THAT(config_file_lines,
              ::testing::Not(::testing::Contains(
                  ::testing::HasSubstr("server_ssl_cipher"))));
  ASSERT_THAT(config_file_lines,
              ::testing::Not(::testing::Contains(
                  ::testing::HasSubstr("server_ssl_curves"))));

  // check certs are generated.
  {
    auto prefix = "client_ssl_cert="s;
    auto it = std::find_if(config_file_lines.begin(), config_file_lines.end(),
                           [&prefix](auto const &line) {
                             return (line.size() >= prefix.size() &&
                                     line.substr(0, prefix.size()) == prefix);
                           });

    EXPECT_TRUE(it != config_file_lines.end())
        << prefix + " not found in config-file";

    auto filename = it->substr(prefix.size());
    ASSERT_GT(filename.size(), 0);
    EXPECT_THAT(filename, ::testing::Truly([](auto const &v) {
                  return mysql_harness::Path(v).exists();
                }));
  }
  {
    auto prefix = "client_ssl_key="s;
    auto it = std::find_if(config_file_lines.begin(), config_file_lines.end(),
                           [&prefix](auto const &line) {
                             return (line.size() >= prefix.size() &&
                                     line.substr(0, prefix.size()) == prefix);
                           });

    EXPECT_TRUE(it != config_file_lines.end())
        << prefix + " not found in config-file";

    auto filename = it->substr(prefix.size());
    ASSERT_GT(filename.size(), 0);
    EXPECT_THAT(filename, ::testing::Truly([](auto const &v) {
                  return mysql_harness::Path(v).exists();
                }));
  }
}

static void check_no_cert_generated(
    const std::vector<std::string> &config_file_lines,
    const std::string &expected_client_ssl_mode,
    const std::string &expected_server_ssl_mode) {
  ASSERT_THAT(config_file_lines,
              ::testing::IsSupersetOf({
                  "client_ssl_mode=" + expected_client_ssl_mode,
                  "server_ssl_mode=" + expected_server_ssl_mode,
                  "server_ssl_verify=DISABLED"s,
              }));

  // not specified at command-line, no set in config
  ASSERT_THAT(config_file_lines,
              ::testing::Not(::testing::Contains(::testing::AnyOf(
                  ::testing::StartsWith("client_ssl_cipher"),
                  ::testing::StartsWith("client_ssl_curves"),
                  ::testing::StartsWith("server_ssl_cipher"),
                  ::testing::StartsWith("server_ssl_curves")))));
}

static void check_cert_specified(
    const std::vector<std::string> &config_file_lines,
    const std::string &expected_client_ssl_mode,
    const std::string &expected_server_ssl_mode,
    const std::string &expected_client_cert,
    const std::string &expected_client_key) {
  ASSERT_THAT(config_file_lines,
              ::testing::IsSupersetOf({
                  "client_ssl_mode=" + expected_client_ssl_mode,
                  "server_ssl_mode=" + expected_server_ssl_mode,
                  "client_ssl_cert=" + expected_client_cert,
                  "client_ssl_key=" + expected_client_key,
              }));
  // check certs are NOT generated.
  //
  // certs are only generated if they are left at defaults.
  {
    auto prefix = "client_ssl_cert="s;
    auto it = std::find_if(config_file_lines.begin(), config_file_lines.end(),
                           [&prefix](auto const &line) {
                             return (line.size() >= prefix.size() &&
                                     line.substr(0, prefix.size()) == prefix);
                           });

    EXPECT_TRUE(it != config_file_lines.end())
        << prefix + " not found in config-file";

    auto filename = it->substr(prefix.size());
    ASSERT_GT(filename.size(), 0);
    EXPECT_THAT(filename, ::testing::Not(::testing::Truly([](auto const &v) {
                  return mysql_harness::Path(v).exists();
                })));
  }
  {
    auto prefix = "client_ssl_key="s;
    auto it = std::find_if(config_file_lines.begin(), config_file_lines.end(),
                           [&prefix](auto const &line) {
                             return (line.size() >= prefix.size() &&
                                     line.substr(0, prefix.size()) == prefix);
                           });

    EXPECT_TRUE(it != config_file_lines.end())
        << prefix + " not found in config-file";

    auto filename = it->substr(prefix.size());
    ASSERT_GT(filename.size(), 0);
    EXPECT_THAT(filename, ::testing::Not(::testing::Truly([](auto const &v) {
                  return mysql_harness::Path(v).exists();
                })));
  }
}

const BootstrapTlsEndpointParams bootstrap_tls_endpoint_params[] = {
    {"all_defaults",  // BS_MODES_01
                      // BS_VERIFY_DEFAULT_01
                      // BS_CERT_KEY_MODE_01
                      // BS_CERT_KEY_CONFIG_PERSISTS_01
                      // BS_CERT_KEY_CONFIG_PERSISTS_04
                      // BS_CERT_KEY_CONFIG_PERSISTS_07
                      // BS_CERT_KEY_CONFIG_PERSISTS_10
     {},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_generated(config_file_lines, "PREFERRED", "PREFERRED");
     }},
    {"client_ssl_mode_preferred_cert_gen",  // BS_MODES_02, BS_CERT_KEY_MODE_02
     {"--client-ssl-mode", "PREFERRED"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_generated(config_file_lines, "PREFERRED", "PREFERRED");
     }},
    {"client_ssl_mode_required_cert_gen",  // BS_MODES_03, BS_CERT_KEY_MODE_03,
                                           // BS_CERT_KEY_CONFIG_PERSISTS_02
                                           // BS_CERT_KEY_CONFIG_PERSISTS_05
                                           // BS_CERT_KEY_CONFIG_PERSISTS_08
                                           // BS_CERT_KEY_CONFIG_PERSISTS_11
     {"--client-ssl-mode", "REQUIRED"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_generated(config_file_lines, "REQUIRED", "PREFERRED");
     }},
    {"client_ssl_mode_passthrough_no_cert_gen",  // BS_MODES_04,
                                                 // BS_CERT_KEY_MODE_04
                                                 // BS_CERT_KEY_CONFIG_PERSISTS_03
                                                 // BS_CERT_KEY_CONFIG_PERSISTS_06
                                                 // BS_CERT_KEY_CONFIG_PERSISTS_09
                                                 // BS_CERT_KEY_CONFIG_PERSISTS_12
     {"--client-ssl-mode", "PASSTHROUGH", "--disable-rw-split"},
     [](const std::vector<std::string> &config_file_lines) {
       check_no_cert_generated(config_file_lines, "PASSTHROUGH", "AS_CLIENT");

       // not specified at command-line, no set in config
       ASSERT_THAT(config_file_lines,
                   ::testing::Not(::testing::Contains(::testing::AnyOf(
                       ::testing::StartsWith("client_ssl_cert"),
                       ::testing::StartsWith("client_ssl_key")))));
     }},
    {"client_ssl_mode_disabled_no_cert_gen",  // BS_MODES_05,
                                              // BS_CERT_KEY_MODE_05
     {"--client-ssl-mode", "DISABLED", "--disable-rw-split"},
     [](const std::vector<std::string> &config_file_lines) {
       check_no_cert_generated(config_file_lines, "DISABLED", "PREFERRED");

       // not specified at command-line, no set in config
       ASSERT_THAT(config_file_lines,
                   ::testing::Not(::testing::Contains(::testing::AnyOf(
                       ::testing::StartsWith("client_ssl_cert"),
                       ::testing::StartsWith("client_ssl_key")))));
     }},
    {"client_ssl_mode_passthrough_key_cert_no_cert_gen",  // BS_CERT_KEY_ARGS_07
     {"--client-ssl-mode", "PASSTHROUGH",                 //
      "--client-ssl-key", "bar",                          //
      "--client-ssl-cert", "foo"},
     [](const std::vector<std::string> &config_file_lines) {
       check_no_cert_generated(config_file_lines, "PASSTHROUGH", "AS_CLIENT");
     }},
    {"client_ssl_cert_and_key",  // BS_ARGS_ARBITRARY_03
                                 // BS_ARGS_ARBITRARY_04
                                 // BS_CERT_KEY_ARGS_01
     {"--client-ssl-cert", "some-ssl-cert", "--client-ssl-key", "some-ssl-key"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_specified(config_file_lines, "PREFERRED", "PREFERRED",
                            "some-ssl-cert", "some-ssl-key");
     }},
    {"client_ssl_cert_key_and_mode_disabled",  // BS_CERT_KEY_ARGS_??
     {"--client-ssl-cert", "some-ssl-cert", "--client-ssl-key", "some-ssl-key",
      "--client-ssl-mode", "disabled"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_specified(config_file_lines, "DISABLED", "PREFERRED",
                            "some-ssl-cert", "some-ssl-key");
     }},
    {"client_ssl_cert_key_and_mode_preferred",  // BS_CERT_KEY_ARGS_??
     {"--client-ssl-cert", "some-ssl-cert", "--client-ssl-key", "some-ssl-key",
      "--client-ssl-mode", "preferred"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_specified(config_file_lines, "PREFERRED", "PREFERRED",
                            "some-ssl-cert", "some-ssl-key");
     }},
    {"client_ssl_cert_key_and_mode_required",  // BS_CERT_KEY_ARGS_??
     {"--client-ssl-cert", "some-ssl-cert", "--client-ssl-key", "some-ssl-key",
      "--client-ssl-mode", "REQUIRED"},
     [](const std::vector<std::string> &config_file_lines) {
       check_cert_specified(config_file_lines, "REQUIRED", "PREFERRED",
                            "some-ssl-cert", "some-ssl-key");
     }},

};

INSTANTIATE_TEST_SUITE_P(Spec, BootstrapTlsEndpoint,
                         ::testing::ValuesIn(bootstrap_tls_endpoint_params),
                         [](auto const &p) { return p.param.test_name; });

// failing bootstraps with mocks

struct BootstrapTlsEndpointFailMockParams {
  const std::string test_name;

  const std::vector<std::string> cmdline_args;

  const std::vector<std::string> expected_output_lines;
};

class BootstrapTlsEndpointFailMock
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTlsEndpointFailMockParams> {
};

TEST_P(BootstrapTlsEndpointFailMock, fails) {
  bootstrap_failover(
      {
          Config{
              "127.0.0.1",
              port_pool_.get_next_available(),
              port_pool_.get_next_available(),
              "bootstrap_gr.js",
          },
      },
      mysqlrouter::ClusterType::GR_V2, {}, EXIT_FAILURE,
      GetParam().expected_output_lines, 1s, {2, 0, 3}, GetParam().cmdline_args);
}

const BootstrapTlsEndpointFailMockParams
    bootstrap_tls_endpoint_fail_mock_params[] = {
        {"client_ssl_mode_passthrough_preferred",  // BS_MODES_22
         {"--client-ssl-mode", "passthrough", "--server-ssl-mode", "preferred"},
         {
             "Error: --server-ssl-mode must be AS_CLIENT or not specified, if "
             "--client-ssl-mode is PASSTHROUGH.",
         }},
        {"client_ssl_mode_passthrough_required",  // BS_MODES_23
         {"--client-ssl-mode", "passthrough", "--server-ssl-mode", "required"},
         {
             "Error: --server-ssl-mode must be AS_CLIENT or not specified, if "
             "--client-ssl-mode is PASSTHROUGH.",
         }},
        {"client_ssl_mode_passthrough_disabled",  // BS_MODES_24
         {"--client-ssl-mode", "passthrough", "--server-ssl-mode", "disabled"},
         {
             "Error: --server-ssl-mode must be AS_CLIENT or not specified, if "
             "--client-ssl-mode is PASSTHROUGH.",
         }},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapTlsEndpointFailMock,
    ::testing::ValuesIn(bootstrap_tls_endpoint_fail_mock_params),
    [](auto const &p) { return p.param.test_name; });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
