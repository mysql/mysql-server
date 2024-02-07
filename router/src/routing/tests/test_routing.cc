/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <stdexcept>
#include <system_error>

#include <gmock/gmock.h>  // EXPECT_THAT
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>  // FRIEND_TEST

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql_routing.h"  // Mode
#include "test/helpers.h"   // init_test_logger

using namespace std::chrono_literals;

using ::testing::Eq;
using ::testing::StrEq;

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
  conf.bind_address = mysql_harness::TCPAddress{"0.0.0.0", 7001};
  conf.protocol = Protocol::Type::kXProtocol;
  conf.connect_timeout = 1;
  MySQLRouting routing(conf, io_ctx_);

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

TEST_F(RoutingTests, set_destinations_from_cvs) {
  RoutingConfig conf;
  conf.routing_strategy = routing::RoutingStrategy::kNextAvailable;
  conf.bind_address = mysql_harness::TCPAddress{"0.0.0.0", 7001};
  conf.protocol = Protocol::Type::kXProtocol;
  conf.connect_timeout = 1;
  MySQLRouting routing(conf, io_ctx_);

  // valid address list
  {
    const std::string cvs = "127.0.0.1:2002,127.0.0.1:2004";
    EXPECT_NO_THROW(routing.set_destinations_from_csv(cvs));
  }

  // no address
  {
    const std::string csv = "";
    EXPECT_THROW(routing.set_destinations_from_csv(csv), std::runtime_error);
  }

  // invalid address
  {
    const std::string csv = "127.0.0..2:2222";
    EXPECT_THROW(routing.set_destinations_from_csv(csv), std::runtime_error);
  }

  // let's check if the correct default port gets chosen for
  // the respective protocol
  // we use the trick here setting the expected address also as
  // the binding address for the routing which should make the method throw
  // an exception if these are the same
  {
    const std::string address = "127.0.0.1";
    RoutingConfig conf_classic;
    conf_classic.routing_strategy = routing::RoutingStrategy::kNextAvailable;
    conf_classic.bind_address = mysql_harness::TCPAddress{address, 3306};
    conf_classic.protocol = Protocol::Type::kClassicProtocol;
    conf_classic.connect_timeout = 1;
    MySQLRouting routing_classic(conf_classic, io_ctx_);
    EXPECT_THROW(routing_classic.set_destinations_from_csv("127.0.0.1"),
                 std::runtime_error);
    EXPECT_THROW(routing_classic.set_destinations_from_csv("127.0.0.1:3306"),
                 std::runtime_error);
    EXPECT_NO_THROW(
        routing_classic.set_destinations_from_csv("127.0.0.1:33060"));

    RoutingConfig conf_x;
    conf_x.routing_strategy = routing::RoutingStrategy::kNextAvailable;
    conf_x.bind_address = mysql_harness::TCPAddress{address, 33060};
    conf_x.protocol = Protocol::Type::kXProtocol;
    conf_x.connect_timeout = 1;
    MySQLRouting routing_x(conf_x, io_ctx_);
    EXPECT_THROW(routing_x.set_destinations_from_csv("127.0.0.1"),
                 std::runtime_error);
    EXPECT_THROW(routing_x.set_destinations_from_csv("127.0.0.1:33060"),
                 std::runtime_error);
    EXPECT_NO_THROW(routing_x.set_destinations_from_csv("127.0.0.1:3306"));
  }
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
