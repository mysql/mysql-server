/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <fstream>
#include <thread>
#include "gmock/gmock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "mysql/harness/net_ts/impl/socket.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

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
        return stdx::unexpected(make_error_code(std::errc::invalid_argument));
      }
      return v->GetInt();
    } else {
      return stdx::unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
  }

  stdx::expected<void, std::error_code> wait_for_stashed_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res =
          rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                       "/stashedServerConnections");
      if (!int_res) return stdx::unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        return stdx::unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(5ms);
    } while (true);
  }

  stdx::expected<void, std::error_code> wait_for_idle_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res =
          rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                       "/idleServerConnections");
      if (!int_res) return stdx::unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        std::cerr << *int_res << "\n";
        return stdx::unexpected(make_error_code(std::errc::timed_out));
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
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

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
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

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
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

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
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

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
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

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
                   {"max_idle_server_connections", "0"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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

  const auto kDefaultDelay = 0ms;
  const auto kDelay = kDefaultDelay;
  const auto kJitter = 500ms;

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    const auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_NO_ERROR(connect_res);

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;
      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it once
    {
      const auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      const auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      const auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "1")));
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

  writer
      .section("connection_pool",
               {
                   // no pool needed.
                   {"max_idle_server_connections", "0"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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

  const auto kJitter = 500ms;

  {
    SCOPED_TRACE("// [0] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_NO_ERROR(connect_res);

    SCOPED_TRACE("// [0] expect it to be stashed.");
    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      // the connection should be stashed away right away.
      const auto wait_time = clock_type::now() - start;
      EXPECT_LT(wait_time, kJitter);
    }

    SCOPED_TRACE("// [0] query, old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // ... old ...
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // ... after ...
      // SELECT ... // the 'events_stmt'
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE("// [0] expect it to be stashed again, immediately");
    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;
      EXPECT_LT(wait_time, kJitter);
    }

    SCOPED_TRACE("// [0] query again, old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // ... old ...
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // ... new ...
      // SELECT ... // the 'events_stmt'
      // ... after ...
      // SELECT ... // the 'events_stmt'
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE("// [1] new connection, no sharing as sharing-delay is large");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_port_));

    {
      auto query_res = cli2.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // ... old ...
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // SELECT ... // the 'events_stmt'
      // ... new ...
      // SELECT ... // the 'events_stmt'
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // ... after ...
      // SELECT ... // the 'events_stmt'
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "4"),
                              ElementsAre("statement/sql/set_option", "2")));
    }

    SCOPED_TRACE("// [0] got back our old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // ... old ...
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // SELECT ... // the 'events_stmt'
      // ... new ...
      // SELECT ... // the 'events_stmt'
      // SET @@SESSION.session_track...
      // SELECT collation_connection, ...
      // SELECT ... // the 'events_stmt'
      // ... after ...
      // SELECT ... // the 'events_stmt'
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "5"),
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
                   // no pool needed.
                   {"max_idle_server_connections", "0"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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
              {"connection_sharing_delay", "0.1"},  // kDelay
          });

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  const auto kDelay = 100ms;
  const auto kJitter = 100ms;

  SCOPED_TRACE("// connect");
  {
    SCOPED_TRACE("// [0] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_NO_ERROR(connect_res);

    SCOPED_TRACE(
        "// [0] after connect, the connection should be stashed right away.");
    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      EXPECT_GT(wait_time, kDelay - kJitter);
      EXPECT_LT(wait_time, kDelay + kJitter);
    }

    SCOPED_TRACE(
        "// [0] at query the server-side connection should be taken from the "
        "stash, right away.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE(
        "// [0] at query the server-side connection should be taken from the "
        "stash, right away, again.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // if set_option stays at "1" -> same connection.
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    // wait until the connection enters the stash.
    {
      const auto start = clock_type::now();
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

      const auto wait_time = clock_type::now() - start;

      // check the wait is immediate.
      EXPECT_LT(wait_time, kJitter);
    }

    // wait a bit for the sharing-delay to run out.
    //
    // there is no way to check that the time ran out, but it is known
    // when it got stashed from the step before.
    std::this_thread::sleep_for(kDelay + kJitter);

    SCOPED_TRACE("// [1] new connection. sharing as sharing-delay is ran out.");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_port_));

    // run it again without waiting to be pooled.
    {
      auto query_res = cli2.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // set_option increments as the connection comes from the stash and is
      // reset.
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "4"),
                              ElementsAre("statement/sql/set_option", "2")));
    }

    SCOPED_TRACE(
        "// [0] finds no connection on the stash and opens a new connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      // set_option is "1" as it is a new connect. No SELECT was run on it yet.
      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/set_option", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_delay_is_large) {
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS);

  auto writer = config_writer(conf_dir_.name());

  writer
      .section(
          "connection_pool",
          {
              // connection pool isn't needed as the connections are stashed.
              {"max_idle_server_connections", "0"},
          })
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

  {
    SCOPED_TRACE("// [0] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_port_);
    ASSERT_NO_ERROR(connect_res);

    SCOPED_TRACE("// [0] query, old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE("// [0] query, still old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE("// [1] new connection, no sharing as sharing-delay is large");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_port_));

    // run it again without waiting to be pooled.
    {
      auto query_res = cli2.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    SCOPED_TRACE("// [0] got back our old connection.");
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "3"),
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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
    ASSERT_NO_ERROR(connect_res);

    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1"),
                              ElementsAre("statement/sql/set_option", "1")));
    }

    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 10s));

    // run it again.
    //
    // if there is multiplexing, there will be some SET statements.
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "2"),
                              ElementsAre("statement/sql/set_option", "1")));
    }
  }

  SCOPED_TRACE("// connect to without-sharing-port");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    auto connect_res = cli.connect("127.0.0.1", router_without_sharing_port);
    ASSERT_NO_ERROR(connect_res);

    // run it once
    {
      auto query_res = cli.query(events_stmt);
      ASSERT_NO_ERROR(query_res);

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
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ElementsAre(ElementsAre("statement/sql/select", "1")));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, connection_sharing_pool_before_stash) {
  RecordProperty("Description",
                 "Check that connections are first taken from the pool of "
                 "server-side connections without a client-side connection and "
                 "only if it is empty, taken from the stash of server-side "
                 "connections _with_ a client-side connection.");
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
                   // test needs a pool of 2
                   {"max_idle_server_connections", "2"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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

  {
    SCOPED_TRACE("// [0] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
    ASSERT_NO_ERROR(cli.query("SET @block_me = 1"));  // disable sharing.

    SCOPED_TRACE("// [1] new connection");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_port_));
    ASSERT_NO_ERROR(cli2.query("SET @block_me = 1"));  // disable sharing.
  }

  SCOPED_TRACE("// wait until the connections are moved to the pool.");
  EXPECT_NO_ERROR(wait_for_idle_server_connections(2, 1s));

  {
    SCOPED_TRACE("// [1] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // connection was taken from the pool
    EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 1s));
    // and stashed again.
    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));

    // when a new connection is needed, it should be taken
    //
    // * first from the pool
    // * and then from the stash.
    {
      SCOPED_TRACE("// [2] new connection");
      MysqlClient cli2;

      cli.username("foo");
      cli.password("");

      ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_port_));

      // connection was taken from the pool.
      EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));

      // and stashed again.
      EXPECT_NO_ERROR(wait_for_stashed_server_connections(2, 1s));
    }
  }

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, stashed_connection_is_moved_to_pool) {
  RecordProperty("Description",
                 "Check that stashed connections get moved to the pool when "
                 "the client disconnections.");
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
                   // test needs a pool of 1
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
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

  {
    SCOPED_TRACE("// [0] new connection");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));
  }

  SCOPED_TRACE("// wait until the connections are moved to the pool.");
  EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 1s));
  EXPECT_NO_ERROR(wait_for_stashed_server_connections(0, 1s));

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, pooled_ssl_and_non_ssl_dont_mix) {
  RecordProperty(
      "Description",
      "Check that pooled connections SSL and non-SSL don't get shared.");
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS, false, 0, 0, "",
                           "127.0.0.1", 30s, true  // ssl
  );

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto router_without_ssl_port = router_port_;
  auto router_with_ssl_port = port_pool_.get_next_available();

  auto writer = config_writer(conf_dir_.name());

  writer
      .section("connection_pool",
               {
                   // test needs a pool of 2
                   {"max_idle_server_connections", "2"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
      .section(
          "routing:non_ssl",
          {
              {"bind_port", std::to_string(router_without_ssl_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })
      .section(
          "routing:ssl",
          {
              {"bind_port", std::to_string(router_with_ssl_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "REQUIRED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })

      ;

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  {
    SCOPED_TRACE("// [0] new connection (without-ssl)");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_without_ssl_port));

    // run it once
    {
      auto query_res = cli.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::ElementsAre(
                              ::testing::ElementsAre("Ssl_cipher", "")));
    }

    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));
  }

  SCOPED_TRACE("// wait until the connections are moved to the pool.");
  EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 1s));
  EXPECT_NO_ERROR(wait_for_stashed_server_connections(0, 1s));

  {
    SCOPED_TRACE("// [1] new connection (with-ssl)");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_with_ssl_port));

    // run it once
    {
      auto query_res = cli.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ::testing::ElementsAre(::testing::ElementsAre(
                      "Ssl_cipher", ::testing::Not(::testing::IsEmpty()))));
    }

    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));
    EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 1s));  // not used.
  }

  EXPECT_NO_ERROR(wait_for_idle_server_connections(2, 1s));
  EXPECT_NO_ERROR(wait_for_stashed_server_connections(0, 1s));

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, stashed_ssl_and_non_ssl_dont_mix) {
  RecordProperty(
      "Description",
      "Check that stashed connections SSL and non-SSL don't get shared.");
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS, false, 0, 0, "",
                           "127.0.0.1", 30s, true  // ssl
  );

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto router_without_ssl_port = router_port_;
  auto router_with_ssl_port = port_pool_.get_next_available();

  auto writer = config_writer(conf_dir_.name());

  writer
      .section("connection_pool",
               {
                   // test needs a pool of 2
                   {"max_idle_server_connections", "2"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
      .section(
          "routing:non_ssl",
          {
              {"bind_port", std::to_string(router_without_ssl_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "DISABLED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })
      .section(
          "routing:ssl",
          {
              {"bind_port", std::to_string(router_with_ssl_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "REQUIRED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })

      ;

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  {
    SCOPED_TRACE("// [0] new connection (without-ssl)");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_without_ssl_port));

    // run it once
    {
      auto query_res = cli.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result, ::testing::ElementsAre(
                              ::testing::ElementsAre("Ssl_cipher", "")));
    }

    SCOPED_TRACE("// [0] check state before opening connection [1]");
    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));

    SCOPED_TRACE("// [1] new connection (with-ssl)");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_with_ssl_port));

    // run it once
    {
      auto query_res = cli2.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ::testing::ElementsAre(::testing::ElementsAre(
                      "Ssl_cipher", ::testing::Not(::testing::IsEmpty()))));
    }

    SCOPED_TRACE("// [1] check state before closing both connections");
    EXPECT_NO_ERROR(
        wait_for_stashed_server_connections(2, 1s));           // both stashed
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));  // not used.
  }

  EXPECT_NO_ERROR(wait_for_idle_server_connections(2, 1s));
  EXPECT_NO_ERROR(wait_for_stashed_server_connections(0, 1s));

  proc.send_clean_shutdown_event();
}

TEST_F(RoutingSharingConfig, stashed_ssl_and_ssl) {
  RecordProperty("Description",
                 "Check that stashed connections SSL and SSL can share.");
  launch_mysql_server_mock(get_data_dir().join("sharing.js").str(),
                           server_port_, EXIT_SUCCESS, false, 0, 0, "",
                           "127.0.0.1", 30s, true  // ssl
  );

  auto userfile = conf_dir_.file("userfile");
  {
    std::ofstream ofs(userfile);
    // user:pass
    ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
           "YJgciRvb69";
  }

  auto router_with_ssl_port = router_port_;

  auto writer = config_writer(conf_dir_.name());

  writer
      .section("connection_pool",
               {
                   // test needs a pool of 2
                   {"max_idle_server_connections", "2"},
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
      .section("http_server", {{"bind_address", "127.0.0.1"},
                               {"port", std::to_string(rest_port_)}})
      .section(
          "routing:ssl",
          {
              {"bind_port", std::to_string(router_with_ssl_port)},
              {"protocol", "classic"},
              {"destinations", "127.0.0.1:" + std::to_string(server_port_)},
              {"routing_strategy", "round-robin"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "REQUIRED"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
          })

      ;

  auto &proc = router_spawner().spawn({"-c", writer.write()});

  {
    SCOPED_TRACE("// [0] new connection (with-ssl)");
    MysqlClient cli;

    cli.username("foo");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_with_ssl_port));

    // run it once
    {
      auto query_res = cli.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ::testing::ElementsAre(::testing::ElementsAre(
                      "Ssl_cipher", ::testing::Not(::testing::IsEmpty()))));
    }

    SCOPED_TRACE("// [0] check state before opening connection [1]");
    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));

    SCOPED_TRACE("// [1] new connection (with-ssl)");
    MysqlClient cli2;

    cli2.username("foo");
    cli2.password("");

    ASSERT_NO_ERROR(cli2.connect("127.0.0.1", router_with_ssl_port));

    // run it once
    {
      auto query_res = cli2.query("show status like 'ssl_cipher'");
      ASSERT_NO_ERROR(query_res);

      auto results = result_as_vector(*query_res);
      ASSERT_THAT(results, ::testing::SizeIs(1));

      auto result = results.front();
      EXPECT_THAT(result,
                  ::testing::ElementsAre(::testing::ElementsAre(
                      "Ssl_cipher", ::testing::Not(::testing::IsEmpty()))));
    }

    SCOPED_TRACE("// [1] check state before closing both connections");
    EXPECT_NO_ERROR(wait_for_stashed_server_connections(1, 1s));  // stashed
    EXPECT_NO_ERROR(wait_for_idle_server_connections(0, 1s));     // not used.
  }

  EXPECT_NO_ERROR(wait_for_idle_server_connections(1, 1s));
  EXPECT_NO_ERROR(wait_for_stashed_server_connections(0, 1s));

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
