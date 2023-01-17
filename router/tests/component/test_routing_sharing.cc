/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <chrono>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "mysql/harness/net_ts/impl/socket.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

#define EXPECT_NO_ERROR(x) EXPECT_TRUE((x)) << (x).error()

#define ASSERT_NO_ERROR(x) ASSERT_TRUE((x)) << (x).error()

using ::testing::ElementsAre;
using clock_type = std::chrono::steady_clock;

using namespace std::chrono_literals;

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

/**
 * convert a multi-resultset into a simple container which can be EXPECTed
 * against.
 */
static std::vector<std::vector<std::vector<std::string>>> result_as_vector(
    const MysqlClient::Statement::Result &results) {
  std::vector<std::vector<std::vector<std::string>>> resultsets;

  for (const auto &result : results) {
    std::vector<std::vector<std::string>> res_;

    const auto field_count = result.field_count();

    for (const auto &row : result.rows()) {
      std::vector<std::string> row_;

      for (unsigned int ndx = 0; ndx < field_count; ++ndx) {
        auto fld = row[ndx];

        row_.emplace_back(fld == nullptr ? "<NULL>" : fld);
      }

      res_.push_back(std::move(row_));
    }
    resultsets.push_back(std::move(res_));
  }

  return resultsets;
}

constexpr const std::string_view events_stmt =
    "SELECT EVENT_NAME, COUNT_STAR FROM "
    "performance_schema.events_statements_summary_by_thread_by_event_"
    "name AS e JOIN performance_schema.threads AS t ON (e.THREAD_ID = "
    "t.THREAD_ID) WHERE t.PROCESSLIST_ID = CONNECTION_ID() AND "
    "COUNT_STAR > 0 ORDER BY EVENT_NAME";

class RoutingSharingConfig : public RouterComponentTest {
 public:
  stdx::expected<int, std::error_code> rest_get_int(
      const std::string &uri, const std::string &pointer) {
    JsonDocument json_doc;

    fetch_json(rest_client_, uri, json_doc);

    // std::cerr << json_doc << "\n";

    if (auto *v = JsonPointer(pointer).Get(json_doc)) {
      if (!v->IsInt()) {
        return stdx::make_unexpected(
            make_error_code(std::errc::invalid_argument));
      }
      return v->GetInt();
    } else {
      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
  }

  stdx::expected<void, std::error_code> wait_for_idle_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res =
          rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                       "/idleServerConnections");
      if (!int_res) return stdx::make_unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        return stdx::make_unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(5ms);
    } while (true);
  }

 protected:
  TempDirectory conf_dir_;

  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t router_port_{port_pool_.get_next_available()};
  uint16_t rest_port_{port_pool_.get_next_available()};

  IOContext rest_io_ctx_;
  RestClient rest_client_{rest_io_ctx_, "127.0.0.1", rest_port_, rest_user_,
                          rest_pass_};

  static constexpr const char rest_user_[] = "user";
  static constexpr const char rest_pass_[] = "pass";
};

TEST_F(RoutingSharingConfig, connection_sharing_not_set) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

/**
 * check that router doesn't try to do session tracking with a server that
 * doesn't support session-trackers.
 *
 * uses
 *
 * - DISABLE|DISABLED and
 * - empty password
 *
 * to satisfy the "can get plaintext password" requirements for sharing.
 */
TEST_F(RoutingSharingConfig, connection_sharing_no_session_tracker_support) {
  launch_mysql_server_mock(get_data_dir().join("no_session_tracker.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"client_ssl_mode", "DISABLED"},
          {"server_ssl_mode", "DISABLED"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_is_zero) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"connection_sharing", "0"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, warn_connection_sharing_needs_connection_pool) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"connection_sharing", "1"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr("connection_sharing=1 has been ignored"));
}

TEST_F(RoutingSharingConfig, warn_connection_sharing_passthrough) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "classic"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"connection_sharing", "1"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr("connection_sharing=1 has been ignored, as "
                                   "client_ssl_mode=PASSTHROUGH"));
}

TEST_F(RoutingSharingConfig, warn_xproto_does_not_support_sharing) {
  auto writer = config_writer(conf_dir_.name());

  writer.section(
      "routing:under_test",
      {
          {"bind_port", std::to_string(router_port_)},
          {"protocol", "x"},
          {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
          {"routing_strategy", "round-robin"},
          {"connection_sharing", "1"},
      });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  proc.send_clean_shutdown_event();

  EXPECT_THAT(proc.get_logfile_content(),
              ::testing::HasSubstr(
                  "connection_sharing=1 has been ignored, as protocol=x"));
}

TEST_F(RoutingSharingConfig, connection_sharing_delay_is_default) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto writer = config_writer(conf_dir_.name());

  writer
      .section("connection_pool",
               {
                   {"max_idle_server_connections", "1"},
               })
      .section("rest_connection_pool",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "some realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", userfile},
               })
      .section("http_server", {{"port", std::to_string(rest_port_)}})
      .section(
          "routing:under_test",
          {
              {"bind_port", std::to_string(router_port_)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
          });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  const auto kDefaultDelay = 1000ms;
  const auto kDelay = kDefaultDelay;
  const auto kJitter = 500ms;

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    const auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;
      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it once
    {
      const auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      const auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      const auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "2")));
    }

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "3")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_delay_is_zero) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto writer = config_writer(conf_dir_.name());

  writer.section("connection_pool", {{"max_idle_server_connections", "1"}})
      .section("rest_connection_pool",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "some realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", userfile},
               })
      .section("http_server", {{"port", std::to_string(rest_port_)}})
      .section(
          "routing:under_test",
          {
              {"bind_port", std::to_string(router_port_)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  const auto kDelay = 0ms;
  const auto kJitter = 500ms;

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;
      EXPECT_GT(wait_time, kDelay);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "2")));
    }

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;
      EXPECT_GT(wait_time, kDelay);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "3")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_delay_is_small) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto writer = config_writer(conf_dir_.name());

  writer
      .section("connection_pool",
               {
                   {"max_idle_server_connections", "1"},
               })
      .section("rest_connection_pool",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "some realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", userfile},
               })
      .section("http_server", {{"port", std::to_string(rest_port_)}})
      .section(
          "routing:under_test",
          {
              {"bind_port", std::to_string(router_port_)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0.1"},
          });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  const auto kDelay = 100ms;
  const auto kJitter = 100ms;

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "2")));
    }

    // run it again.
    //
    // no new set_option.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "2")));
    }

    // wait until the connection enters the pool.
    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "3")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_delay_is_large) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer.section("connection_pool", {{"max_idle_server_connections", "1"}})
      .section(
          "routing:under_test",
          {
              {"bind_port", std::to_string(router_port_)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "120"},
          });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "1")));
    }

    // run it again without waiting to be pooled.
    //
    // as the delay is large, the query will be sent before it is pooled.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_per_route) {
  auto router_without_sharing_port = port_pool_.get_next_available();
  auto server2_port = port_pool_.get_next_available();

  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server2_port, EXIT_SUCCESS);

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto writer = config_writer(conf_dir_.name());
  writer.section("connection_pool", {{"max_idle_server_connections", "1"}})
      .section("rest_connection_pool",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "some realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", userfile},
               })
      .section("http_server", {{"port", std::to_string(rest_port_)}})
      .section(
          "routing:with_sharing",
          {
              {"bind_port", std::to_string(router_port_)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })
      .section(
          "routing:without_sharing",
          {
              {"bind_port", std::to_string(router_without_sharing_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server2_port)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
          });
  auto &proc = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect to with-sharing-port");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_TRUE(connect_res) << connect_res.error();

    EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 2s));

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "2")));
    }

    EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 2s));

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "3")));
    }
  }

  SCOPED_TRACE("// connect to without-sharing-port");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_without_sharing_port);
    ASSERT_TRUE(connect_res) << connect_res.error();

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::IsEmpty());
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_TRUE(query_res) << query_res.error();

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

struct RoutingSharingConfigInvalidParam {
  std::string testname;

  std::map<std::string, std::string> extra_options;

  std::function<void(const std::string &)> log_matcher;
};

class RoutingSharingConfigInvalid
    : public RouterComponentTest,
      public ::testing::WithParamInterface<RoutingSharingConfigInvalidParam> {
 public:
  TempDirectory conf_dir_;

  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t router_port_{port_pool_.get_next_available()};
};

TEST_P(RoutingSharingConfigInvalid, connection_sharing) {
  auto writer = config_writer(conf_dir_.name());

  std::map<std::string, std::string> routing_options{
      {"bind_port", std::to_string(router_port_)},
      {"protocol", "classic"},
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

static const RoutingSharingConfigInvalidParam routing_sharing_invalid_params[] =
    {
        {"connection_sharing_negative",
         {
             {"connection_sharing", "-1"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::HasSubstr(
                           "connection_sharing in [routing:under_test] needs a "
                           "value of either 0, 1, false or true, was '-1'"));
         }},
        {"connection_sharing_too_large",
         {
             {"connection_sharing", "2"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::HasSubstr(
                           "connection_sharing in [routing:under_test] needs a "
                           "value of either 0, 1, false or true, was '2'"));
         }},
        {"connection_sharing_some_string",
         {
             {"connection_sharing", "abc"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::HasSubstr(
                           "connection_sharing in [routing:under_test] needs a "
                           "value of either 0, 1, false or true, was 'abc'"));
         }},
        {"connection_sharing_float",
         {
             {"connection_sharing", "1.2"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::HasSubstr(
                           "connection_sharing in [routing:under_test] needs a "
                           "value of either 0, 1, false or true, was '1.2'"));
         }},
        {"connection_sharing_delay_float",
         {
             {"connection_sharing", "1"},
             {"connection_sharing_delay", "-1"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::AllOf(
                           ::testing::HasSubstr("connection_sharing_delay in "
                                                "[routing:under_test] needs "
                                                "value between 0 and"),
                           ::testing::HasSubstr(", was '-1'")));
         }},
        {"connection_sharing_delay_quotes",
         {
             {"connection_sharing", "1"},
             {"connection_sharing_delay", "''"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::AllOf(
                           ::testing::HasSubstr("connection_sharing_delay in "
                                                "[routing:under_test] needs "
                                                "value between 0 and"),
                           ::testing::HasSubstr(", was ''''")));
         }},
        {"connection_sharing_delay_some_string",
         {
             {"connection_sharing", "1"},
             {"connection_sharing_delay", "abc"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log,
                       ::testing::AllOf(
                           ::testing::HasSubstr("connection_sharing_delay in "
                                                "[routing:under_test] needs "
                                                "value between 0 and"),
                           ::testing::HasSubstr(", was 'abc'")));
         }},
};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingSharingConfigInvalid,
                         ::testing::ValuesIn(routing_sharing_invalid_params),
                         [](auto &info) { return info.param.testname; });

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(mysql_harness::Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
