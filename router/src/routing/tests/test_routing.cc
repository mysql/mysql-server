/*
  Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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
#include "mysqlrouter/routing.h"

#include <exception>
#include <stdexcept>

#include <gmock/gmock.h>  // EXPECT_THAT
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>  // FRIEND_TEST
#include <rapidjson/error/en.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql_routing.h"         // Mode
#include "mysql_routing_common.h"  // get_routing_thread_name
#include "test/helpers.h"          // init_test_logger

using namespace std::chrono_literals;

class RoutingTests : public ::testing::Test {
 protected:
  net::io_context io_ctx_;
};

TEST_F(RoutingTests, Defaults) {
  ASSERT_EQ(routing::kDefaultWaitTimeout, 0);
  ASSERT_EQ(routing::kDefaultMaxConnections, 0);
  ASSERT_EQ(routing::kDefaultDestinationConnectionTimeout,
            std::chrono::seconds(5));
  ASSERT_EQ(routing::kDefaultBindAddress, "127.0.0.1");
  ASSERT_EQ(routing::kDefaultNetBufferLength, 16384U);
  ASSERT_EQ(routing::kDefaultMaxConnectErrors, 100ULL);
  ASSERT_EQ(routing::kDefaultClientConnectTimeout, std::chrono::seconds(9));
}

TEST_F(RoutingTests, set_destinations_from_uri) {
  using mysqlrouter::URI;

  RoutingConfig conf;
  conf.routing_strategy = routing::RoutingStrategy::kFirstAvailable;
  conf.bind_address = mysql_harness::TcpDestination{"0.0.0.0", 7001};
  conf.protocol = Protocol::Type::kXProtocol;
  conf.connect_timeout = 1;
  MySQLRouting routing(conf, io_ctx_, nullptr);

  // valid metadata-cache uri
  {
    URI uri("metadata-cache://test/default?role=PRIMARY");
    EXPECT_NO_THROW(routing.set_destinations_from_uri(uri));
  }

  // metadata-cache uri, role missing
  {
    URI uri("metadata-cache://test/default");
    try {
      routing.set_destinations_from_uri(uri);
      FAIL() << "Expected std::runtime_error exception";
    } catch (const std::runtime_error &err) {
      EXPECT_EQ(
          err.what(),
          std::string("Missing 'role' in routing destination specification"));
    } catch (...) {
      FAIL() << "Expected std::runtime_error exception";
    }
  }

  // invalid scheme
  {
    URI uri("invalid-scheme://test/default?role=SECONDARY");
    try {
      routing.set_destinations_from_uri(uri);
      FAIL() << "Expected std::runtime_error exception";
    } catch (const std::runtime_error &err) {
      EXPECT_EQ(err.what(),
                std::string("Invalid URI scheme; expecting: 'metadata-cache' "
                            "is: 'invalid-scheme'"));
    } catch (...) {
      FAIL() << "Expected std::runtime_error exception";
    }
  }
}

struct Requirement {
  std::string worklog;
  std::string requirement_id;
  std::string description;
};

struct RoutingDestinationsParam {
  std::string test_name;

  std::optional<Requirement> requirement;

  std::string destinations;

  std::string expected_error_msg;
};

class RoutingDestinationsTest
    : public RoutingTests,
      public ::testing::WithParamInterface<RoutingDestinationsParam> {};

TEST_P(RoutingDestinationsTest, set_destinations) {
  if (GetParam().requirement) {
    auto req = *GetParam().requirement;

    if (!req.worklog.empty()) {
      RecordProperty("Worklog", req.worklog);
    }
    if (!req.requirement_id.empty()) {
      RecordProperty("RequirementId", req.requirement_id);
    }
    if (!req.description.empty()) {
      RecordProperty("Description", req.description);
    }
  }

  RoutingConfig conf;
  conf.routing_strategy = routing::RoutingStrategy::kNextAvailable;
  conf.bind_address = mysql_harness::TcpDestination{"0.0.0.0", 7001};
  conf.protocol = Protocol::Type::kClassicProtocol;
  conf.connect_timeout = 1;
  MySQLRouting routing(conf, io_ctx_, nullptr);

  if (GetParam().expected_error_msg.empty()) {
    EXPECT_NO_THROW(routing.set_destinations(GetParam().destinations));
  } else {
    try {
      routing.set_destinations(GetParam().destinations);
      FAIL() << "expected an exception is thrown";
    } catch (const std::exception &e) {
      EXPECT_EQ(e.what(), GetParam().expected_error_msg);
    }
  }
}

constexpr bool is_win32 =
#ifdef _WIN32
    true;
#else
    false;
#endif

static const RoutingDestinationsParam routing_destinations_params[] = {
    {
        "valid_address_list",

        {},  // no requirement

        "127.0.0.1:2002,127.0.0.1:2004",
        {},
    },

    {
        "empty",

        {},  // no requirement

        "",
        "No destinations available",
    },

    {
        "invalid_address",

        {},  // no requirement

        "127.0.0..2:2222",
        "'127.0.0..2:2222' is invalid: invalid destination address "
        "'127.0.0..2'",
    },

    {
        "local_uri_short",

        Requirement{"16582",  //
                    "FR1",    //
                    "On Unixes routing.destinations MUST support zero-or-more "
                    "local: URIs. On Windows it MUST fail to start."},

        "local:/tmp/foo",
        is_win32 ? "'local:/tmp/foo' is invalid: invalid URI scheme 'local' "
                   "for URI local:/tmp/foo"
                 : "",
    },

    {
        "local_uri_long",

        Requirement{"16582",  //
                    "FR1",    //
                    "On Unixes routing.destinations MUST support zero-or-more "
                    "local: URIs. On Windows it MUST fail to start."},

        "local:///tmp/foo",
        is_win32 ? "'local:///tmp/foo' is invalid: invalid URI scheme 'local' "
                   "for URI local:///tmp/foo"
                 : "",
    },

    {
        "local_uri_hostname_not_empty",

        Requirement{"16582",  //
                    "FR1",    //
                    "If local: URIs in routing.destinations contains a "
                    "hostname, Router MUST fail to start."},

        "local://host/tmp/foo",
        is_win32
            ? "'local://host/tmp/foo' is invalid: invalid URI scheme 'local' "
              "for URI local://host/tmp/foo"
            : "'local://host/tmp/foo' is invalid: local:-URI with a non-empty "
              "//hostname/ part: 'host' in local://host/tmp/foo. "
              "Ensure that local: is followed by either 1 or 3 slashes.",
    },

    {
        "local_uri_username_not_empty",

        Requirement{
            "16582",  //
            "FR1",    //
            "If local: URIs in routing.destinations contains a username, "
            "Router MUST fail to start."},

        "local://username@/tmp/foo",
        is_win32 ? "'local://username@/tmp/foo' is invalid: invalid URI scheme "
                   "'local' for URI local://username@/tmp/foo"
                 : "'local://username@/tmp/foo' is invalid: local:-URI with a "
                   "non-empty username@ part in local://username@/tmp/foo. "
                   "Ensure the URI contains no '@'.",
    },

    {
        "local_uri_password_not_empty",

        Requirement{
            "16582",  //
            "FR1",    //
            "If local: URIs in routing.destinations contains a password, "
            "Router MUST fail to start."},

        "local://:password@/tmp/foo",
        is_win32
            ? "'local://:password@/tmp/foo' is invalid: invalid URI scheme "
              "'local' for URI local://:password@/tmp/foo"
            : "'local://:password@/tmp/foo' is invalid: local:-URI with a "
              "non-empty :password@ part in local://:password@/tmp/foo. "
              "Ensure the URI contains no '@'.",
    },

    {
        "local_uri_path_empty",

        Requirement{
            "16582",  //
            "FR1",    //
            "If local: URIs in routing.destinations contains an empty path, "
            "Router MUST fail to start."},

        "local:",
        is_win32 ? "'local:' is invalid: invalid URI scheme 'local' "
                   "for URI local:"
                 : "'local:' is invalid: local:-URI with an empty /path part "
                   "in local:.",
    },

    {
        "local_uri_query_not_empty",

        Requirement{
            "16582",  //
            "FR1",    //
            "If local: URIs in routing.destinations contains a ?-query, "
            "Router MUST fail to start."},

        "local:///tmp/foo?bar=fuz",
        is_win32 ? "'local:///tmp/foo?bar=fuz' is invalid: invalid URI scheme "
                   "'local' for URI local:///tmp/foo?bar=fuz"
                 : "'local:///tmp/foo?bar=fuz' is invalid: local:-URI with a "
                   "non-empty "
                   "?query part in local:///tmp/foo?bar=fuz. Ensure the URI "
                   "contains no "
                   "'?'.",
    },

};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingDestinationsTest,
                         ::testing::ValuesIn(routing_destinations_params),
                         [](auto &info) { return info.param.test_name; });

TEST_F(RoutingTests, set_destinations_from_cvs_classic_proto) {
  // let's check if the correct default port gets chosen for
  // the respective protocol
  // we use the trick here setting the expected address also as
  // the binding address for the routing which should make the method throw
  // an exception if these are the same

  const std::string address = "127.0.0.1";
  RoutingConfig conf_classic;
  conf_classic.routing_strategy = routing::RoutingStrategy::kNextAvailable;
  conf_classic.bind_address = mysql_harness::TcpDestination{address, 3306};
  conf_classic.protocol = Protocol::Type::kClassicProtocol;
  conf_classic.connect_timeout = 1;
  MySQLRouting routing_classic(conf_classic, io_ctx_, nullptr);
  EXPECT_THROW(routing_classic.set_destinations("127.0.0.1"),
               std::runtime_error);
  EXPECT_THROW(routing_classic.set_destinations("127.0.0.1:3306"),
               std::runtime_error);
  EXPECT_NO_THROW(routing_classic.set_destinations("127.0.0.1:33060"));
}

TEST_F(RoutingTests, set_destinations_from_cvs_x_proto) {
  const std::string address = "127.0.0.1";
  RoutingConfig conf_x;
  conf_x.routing_strategy = routing::RoutingStrategy::kNextAvailable;
  conf_x.bind_address = mysql_harness::TcpDestination{address, 33060};
  conf_x.protocol = Protocol::Type::kXProtocol;
  conf_x.connect_timeout = 1;
  MySQLRouting routing_x(conf_x, io_ctx_, nullptr);

  // default-port for xproto is 33060
  EXPECT_THROW(routing_x.set_destinations("127.0.0.1"), std::runtime_error);
  EXPECT_THROW(routing_x.set_destinations("127.0.0.1:33060"),
               std::runtime_error);
  EXPECT_NO_THROW(routing_x.set_destinations("127.0.0.1:3306"));
}

TEST_F(RoutingTests, set_destinations_from_cvs_unix_socket) {
  RecordProperty("Worklog", "16582");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Description",
                 "On Unixes routing.destinations MUST support zero-or-more "
                 "local: URIs. On Windows it MUST fail to start.");

  const std::string address = "127.0.0.1";

  RoutingConfig conf;
  conf.routing_strategy = routing::RoutingStrategy::kNextAvailable;
  conf.bind_address = mysql_harness::TcpDestination{address, 6644};
  conf.protocol = Protocol::Type::kClassicProtocol;
  conf.connect_timeout = 1;

  MySQLRouting routing(conf, io_ctx_, nullptr);
#ifdef _WIN32
  EXPECT_THROW(routing.set_destinations("127.0.0.1:3306,local:///tmp/foo"),
               std::invalid_argument);
#else
  EXPECT_NO_THROW(routing.set_destinations("127.0.0.1:3306,local:///tmp/foo"));
  EXPECT_NO_THROW(routing.set_destinations("127.0.0.1:3306,local:/tmp/foo"));
#endif
}

TEST_F(RoutingTests, get_routing_thread_name) {
  // config name must begin with "routing" (name of the plugin passed from
  // configuration file)
  EXPECT_STREQ(":parse err", get_routing_thread_name("", "").c_str());
  EXPECT_STREQ(":parse err", get_routing_thread_name("routin", "").c_str());
  EXPECT_STREQ(":parse err", get_routing_thread_name(" routing", "").c_str());
  EXPECT_STREQ("pre:parse err", get_routing_thread_name("", "pre").c_str());
  EXPECT_STREQ("pre:parse err",
               get_routing_thread_name("routin", "pre").c_str());
  EXPECT_STREQ("pre:parse err",
               get_routing_thread_name(" routing", "pre").c_str());

  // normally prefix would never be empty, so the behavior below is not be very
  // meaningful; it should not crash however
  EXPECT_STREQ(":", get_routing_thread_name("routing", "").c_str());
  EXPECT_STREQ(":", get_routing_thread_name("routing:", "").c_str());

  // realistic (but unanticipated) cases - removing everything up to _default_
  // will fail, in which case we fall back of <prefix>:<everything after
  // "routing:">, trimmed to 15 chars
  EXPECT_STREQ(
      "RtS:test_def_ul",
      get_routing_thread_name("routing:test_def_ult_x_ro", "RtS").c_str());
  EXPECT_STREQ(
      "RtS:test_def_ul",
      get_routing_thread_name("routing:test_def_ult_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:", get_routing_thread_name("routing", "RtS").c_str());
  EXPECT_STREQ("RtS:test_x_ro",
               get_routing_thread_name("routing:test_x_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:test_ro",
               get_routing_thread_name("routing:test_ro", "RtS").c_str());

  // real cases
  EXPECT_STREQ(
      "RtS:x_ro",
      get_routing_thread_name("routing:test_default_x_ro", "RtS").c_str());
  EXPECT_STREQ(
      "RtS:ro",
      get_routing_thread_name("routing:test_default_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:", get_routing_thread_name("routing", "RtS").c_str());
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
