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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "router_component_test.h"

struct RoutingRequireConfigInvalidParam {
  std::string testname;

  struct Requirement {
    std::string requirement_id;
    std::string requirement;
    std::string description;
  };

  Requirement requirement;

  std::unordered_map<std::string_view, std::string_view> extra_options;

  std::function<void(const std::string_view &)> log_matcher;
};

class RoutingRequireConfigInvalid
    : public RouterComponentTest,
      public ::testing::WithParamInterface<RoutingRequireConfigInvalidParam> {
 public:
  TempDirectory conf_dir_;

  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t router_port_{port_pool_.get_next_available()};
};

TEST_P(RoutingRequireConfigInvalid, router_require_enforce) {
  RecordProperty("Worklog", "14304");

  auto req = GetParam().requirement;

  if (!req.requirement_id.empty()) {
    RecordProperty("RequirementId", req.requirement_id);

    if (!req.requirement.empty()) {
      RecordProperty("Requirement", req.requirement);
    }

    if (!req.description.empty()) {
      RecordProperty("Description", req.description);
    }
  }

  auto writer = config_writer(conf_dir_.name());

  std::map<std::string, std::string> routing_options{
      {"bind_port", std::to_string(router_port_)},
      {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
      {"routing_strategy", "round-robin"},
  };

  for (auto kv : GetParam().extra_options) {
    routing_options.insert(kv);
  }

  writer.section("routing:under_test", routing_options);

  auto &proc =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_FAILURE)
          .spawn({"-c", writer.write()});

  proc.wait_for_exit();

  GetParam().log_matcher(proc.get_logfile_content());
}

static const RoutingRequireConfigInvalidParam routing_sharing_invalid_params[] =
    {
        {"router_require_enforce_negative",
         {
             "RRE4",
             "If `router_require_enforce` in the `[routing]` section is set to "
             "an invalid value, router MUST fail to start.",
             "negative value",
         },
         {
             {"protocol", "classic"},
             {"client_ssl_mode", "DISABLED"},

             {"router_require_enforce", "-1"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "router_require_enforce in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was '-1'"));
         }},
        {"router_require_enforce_too_large",
         {
             "RRE4",
             "If `router_require_enforce` in the `[routing]` section is set to "
             "an invalid value, router MUST fail to start.",
             "too large",
         },
         {
             {"protocol", "classic"},
             {"client_ssl_mode", "DISABLED"},

             {"router_require_enforce", "2"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "router_require_enforce in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was '2'"));
         }},
        {"router_require_enforce_some_string",
         {
             "RRE4",
             "If `router_require_enforce` in the `[routing]` section is set to "
             "an invalid value, router MUST fail to start.",
             "not a number",
         },
         {
             {"protocol", "classic"},
             {"client_ssl_mode", "DISABLED"},

             {"router_require_enforce", "abc"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "router_require_enforce in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was 'abc'"));
         }},
        {"router_require_enforce_float",
         {
             "RRE4",
             "If `router_require_enforce` in the `[routing]` section is set to "
             "an invalid value, router MUST fail to start.",
             "float",
         },
         {
             {"protocol", "classic"},
             {"client_ssl_mode", "DISABLED"},

             {"router_require_enforce", "1.2"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "router_require_enforce in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was '1.2'"));
         }},
        {"router_require_enforce_and_passthrough",
         {
             "RRE2",
             "If `router_require_enforce` in the `[routing]` section is set in "
             "a section that also has `client_ssl_mode=PASSTHROUGH`, router "
             "MUST fail to start.",
             "",
         },
         {
             {"protocol", "classic"},

             {"router_require_enforce", "1"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with router_require_enforce=1"));
         }},
        {"router_require_enforce_and_protocol_x",
         {
             "RRE3",
             "If `router_require_enforce` in the `[routing]` section is set in "
             "a section that also has `protocol=x`, router MUST fail to start.",
             "",
         },
         {
             {"protocol", "x"},

             {"router_require_enforce", "1"},
             {"client_ssl_mode", "DISABLED"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "protocol=x can not be "
                                "combined with router_require_enforce=1"));
         }},

        {"passthrough_and_client_ssl_ca",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_ca",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_ca", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with client_ssl_ca=somevalue"));
         }},

        {"passthrough_and_client_ssl_capath",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_ca",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_capath", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with client_ssl_capath=somevalue"));
         }},

        {"passthrough_and_client_ssl_crl",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_crl",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_crl", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with client_ssl_crl=somevalue"));
         }},

        {"passthrough_and_client_ssl_crlpath",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_crlpath",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_crlpath", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with client_ssl_crlpath=somevalue"));
         }},

        {"disabled_and_client_ssl_ca",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_ca",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_ca", "somevalue"},
             {"client_ssl_mode", "DISABLED"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=DISABLED can not be "
                                "combined with client_ssl_ca=somevalue"));
         }},

        {"disabled_and_client_ssl_capath",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_ca",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_capath", "somevalue"},
             {"client_ssl_mode", "DISABLED"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=DISABLED can not be "
                                "combined with client_ssl_capath=somevalue"));
         }},

        {"disabled_and_client_ssl_crl",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_crl",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_crl", "somevalue"},
             {"client_ssl_mode", "disabled"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=DISABLED can not be "
                                "combined with client_ssl_crl=somevalue"));
         }},

        {"disabled_and_client_ssl_crlpath",
         {
             "CR2",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `client_ssl_mode` being `PASSTHROUGH` or "
             "`DISABLED`, router MUST fail to start.",
             "PASSTHROUGH and client_ssl_crlpath",
         },
         {
             {"protocol", "classic"},

             {"client_ssl_crlpath", "somevalue"},
             {"client_ssl_mode", "DISABLEd"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=DISABLED can not be "
                                "combined with client_ssl_crlpath=somevalue"));
         }},

        {"protocol_x_and_client_ssl_ca",
         {
             "CR3",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `protocol` is `x`, router MUST fail to start.",
             "client_ssl_ca",
         },
         {
             {"protocol", "x"},
             {"client_ssl_cert", "somevalue"},
             {"client_ssl_key", "somevalue"},
             {"client_ssl_mode", "PREFERRED"},

             {"client_ssl_ca", "somevalue"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "protocol=x can not be "
                                "combined with client_ssl_ca=somevalue"));
         }},

        {"protocol_x_and_client_ssl_capath",
         {
             "CR3",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `protocol` is `x`, router MUST fail to start.",
             "client_ssl_capath",
         },
         {
             {"protocol", "x"},
             {"client_ssl_cert", "somevalue"},
             {"client_ssl_key", "somevalue"},
             {"client_ssl_mode", "PREFERRED"},

             {"client_ssl_capath", "somevalue"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "protocol=x can not be "
                                "combined with client_ssl_capath=somevalue"));
         }},
        {"protocol_x_and_client_ssl_crl",
         {
             "CR3",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `protocol` is `x`, router MUST fail to start.",
             "client_ssl_crl",
         },
         {
             {"protocol", "x"},
             {"client_ssl_cert", "somevalue"},
             {"client_ssl_key", "somevalue"},
             {"client_ssl_mode", "PREFERRED"},

             {"client_ssl_crl", "somevalue"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "protocol=x can not be "
                                "combined with client_ssl_crl=somevalue"));
         }},
        {"protocol_x_and_client_ssl_crlpath",
         {
             "CR3",
             "If `client_ssl_ca`, `client_ssl_capath`, `client_ssl_crl` or "
             "`client_ssl_crlpath` are specified in the `[routing]` section "
             "that also has `protocol` is `x`, router MUST fail to start.",
             "client_ssl_crlpath",
         },
         {
             {"protocol", "x"},
             {"client_ssl_cert", "somevalue"},
             {"client_ssl_key", "somevalue"},
             {"client_ssl_mode", "PREFERRED"},

             {"client_ssl_crlpath", "somevalue"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "protocol=x can not be "
                                "combined with client_ssl_crlpath=somevalue"));
         }},

        {"passthrough_and_server_ssl_cert",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "PASSTHROUGH and server_ssl_cert",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_cert", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with server_ssl_cert=somevalue"));
         }},

        {"passthrough_and_server_ssl_key",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "PASSTHROUGH and server_ssl_key",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_key", "somevalue"},
             {"client_ssl_mode", "PASSTHROUGH"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "client_ssl_mode=PASSTHROUGH can not be "
                                "combined with server_ssl_key=somevalue"));
         }},
        {"disabled_and_server_ssl_cert",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "DISABLED and server_ssl_cert",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_cert", "somevalue"},
             {"client_ssl_mode", "disabled"},
             {"server_ssl_mode", "disabled"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "server_ssl_mode=DISABLED can not be "
                                "combined with server_ssl_cert=somevalue"));
         }},

        {"disabled_and_server_ssl_key",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "DISABLED and server_ssl_cert",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_key", "somevalue"},
             {"client_ssl_mode", "disabled"},
             {"server_ssl_mode", "disabled"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "server_ssl_mode=DISABLED can not be "
                                "combined with server_ssl_key=somevalue"));
         }},

        {"disabled_as_client_and_server_ssl_key",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "DISABLED/AS_CLIENT and server_ssl_key",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_key", "somevalue"},
             {"client_ssl_mode", "disabled"},
             {"server_ssl_mode", "as_client"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "server_ssl_mode=DISABLED can not be "
                                "combined with server_ssl_key=somevalue"));
         }},

        {"disabled_as_client_and_server_ssl_cert",
         {
             "SR1",
             "If `server_ssl_key` or `server_ssl_cert` are specified in the "
             "`[routing]` section that also has `client_ssl_mode` as "
             "`PASSTHROUGH` or `server_ssl_mode` as `DISABLED`, router MUST "
             "fail to start.",
             "DISABLED/AS_CLIENT and server_ssl_cert",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_cert", "somevalue"},
             {"client_ssl_mode", "disabled"},
             {"server_ssl_mode", "as_client"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "server_ssl_mode=DISABLED can not be "
                                "combined with server_ssl_cert=somevalue"));
         }},

        {"server_ssl_cert_no_key",
         {
             "SR2",
             "If `server_ssl_key` is set without `server_ssl_cert` is set (and "
             "vice versa), router MUST fail to start.",
             "server_ssl_cert set, no server_ssl_key",
         },
         {
             {"protocol", "classic"},

             {"server_ssl_cert", "somevalue"},
             {"client_ssl_mode", "DISABLED"},
             {"server_ssl_mode", "PREFERRED"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr(
                        "setting server_ssl_key= and server_ssl_cert=somevalue "
                        "failed: Invalid argument"));
         }},

        {"server_ssl_key_no_cert",
         {
             "SR2",
             "If `server_ssl_key` is set without `server_ssl_cert` is set (and "
             "vice versa), router MUST fail to start.",
             "server_ssl_key set, no server_ssl_cert",
         },
         {
             {"protocol", "classic"},
             {"server_ssl_key", "somevalue"},
             {"client_ssl_mode", "DISABLED"},
             {"server_ssl_mode", "PREFERRED"},
         },
         [](const std::string_view &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr(
                        "setting server_ssl_key=somevalue and server_ssl_cert= "
                        "failed: Invalid argument"));
         }},
};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingRequireConfigInvalid,
                         ::testing::ValuesIn(routing_sharing_invalid_params),
                         [](auto &info) { return info.param.testname; });

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(mysql_harness::Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
