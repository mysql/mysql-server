/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/**
 * @file
 *
 * plugin interface of the io-plugin.
 */

#include "mysqlrouter/io_backend.h"
#include "mysqlrouter/io_component.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../src/io_plugin.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logger_plugin.h"  // harness_plugin_logger
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/io_export.h"
#include "mysqlrouter/io_thread.h"
#include "test/helpers.h"  // init_test_logger

TEST(IoPluginTest, no_config_section) {
  // appinfo MUST be provided as get_app_info will assert() otherwise.
  mysql_harness::AppInfo appinfo{};
  mysql_harness::PluginFuncEnv env(&appinfo, nullptr);

  harness_plugin_io.init(&env);
  ASSERT_TRUE(env.exit_ok());
  harness_plugin_io.start(&env);
  ASSERT_TRUE(env.exit_ok());

  harness_plugin_io.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

TEST(IoPluginTest, section_only) {
  mysql_harness::ConfigSection section("io", "foo", nullptr);
  mysql_harness::AppInfo appinfo{};
  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  harness_plugin_io.init(&env);
  ASSERT_TRUE(env.exit_ok());
  harness_plugin_io.start(&env);
  ASSERT_TRUE(env.exit_ok());
  harness_plugin_io.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

TEST(IoPluginTest, section_with_key) {
  mysql_harness::Config config;
  auto &section = config.add("io", "foo");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "[io] section does not expect a key, found 'foo'");
}

/**
 * explicitly setting a unsupported backend errors out.
 *
 * - TS_FR00_05
 */
TEST(IoPluginTest, unknown_backend) {
  // prepare a simple config
  mysql_harness::Config config;
  auto &section = config.add("io");
  section.add("backend", "unknown");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // try to init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "[io] backend 'unknown' is not known. Known backends are: " +
                mysql_harness::join(IoBackend::supported(), ", "));
}

/**
 * explicitly setting a supported backend works.
 *
 * - TS_FR00_03
 * - TS_FR00_04a
 */
TEST(IoPluginTest, explicit_backend) {
  for (const auto &backend : IoBackend::supported()) {
    // prepare a simple config
    mysql_harness::Config config;
    auto &section = config.add("io");
    section.add("backend", backend);

    mysql_harness::AppInfo appinfo{};
    appinfo.config = &config;

    mysql_harness::PluginFuncEnv env(&appinfo, &section);

    // logger needs be initialized before the io-plugin
    harness_plugin_logger.init(&env);

    // try to init the io-plugin
    harness_plugin_io.init(&env);
    ASSERT_TRUE(env.exit_ok());

    auto &io_comp = IoComponent::get_instance();

    EXPECT_EQ(io_comp.io_threads().size(), std::thread::hardware_concurrency());
    EXPECT_EQ(io_comp.backend_name(), backend);

    harness_plugin_io.deinit(&env);
    ASSERT_TRUE(env.exit_ok());
  }
}

/**
 * explicitly setting a supported backend and threads works.
 *
 * - TS_FR00_06
 * - TS_FR00_07
 */
TEST(IoPluginTest, explicit_backend_and_threads) {
  const size_t num_threads{3};
  for (const auto &backend : IoBackend::supported()) {
    // prepare a simple config
    mysql_harness::Config config;
    auto &section = config.add("io");
    section.add("backend", backend);
    section.add("threads", std::to_string(num_threads));

    mysql_harness::AppInfo appinfo{};
    appinfo.config = &config;

    mysql_harness::PluginFuncEnv env(&appinfo, &section);

    // logger needs be initialized before the io-plugin
    harness_plugin_logger.init(&env);

    // try to init the io-plugin
    harness_plugin_io.init(&env);
    ASSERT_TRUE(env.exit_ok());

    auto &io_comp = IoComponent::get_instance();

    EXPECT_EQ(io_comp.io_threads().size(), num_threads);
    EXPECT_EQ(io_comp.backend_name(), backend);

    harness_plugin_io.deinit(&env);
    ASSERT_TRUE(env.exit_ok());
  }
}

/**
 * explicitly setting a non-number value for 'threads' fails.
 *
 * - TS_FR00_08
 */
TEST(IoPluginTest, threads_is_string_fails) {
  // prepare a simple config
  mysql_harness::Config config;
  auto &section = config.add("io");
  section.add("threads", "foo");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // try to init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "option threads in [io] needs value between 0 and 1024 "
            "inclusive, was 'foo'");

  harness_plugin_io.deinit(&env);
}

/**
 * explicitly setting a negative threads fails.
 *
 * - TS_FR00_09
 */
TEST(IoPluginTest, negative_threads) {
  // prepare a simple config
  mysql_harness::Config config;
  auto &section = config.add("io");
  section.add("threads", "-1");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // try to init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "option threads in [io] needs value between 0 and 1024 "
            "inclusive, was '-1'");

  harness_plugin_io.deinit(&env);
}

/**
 * explicitly setting a too large thread-count fails.
 *
 * - TS_FR00_10
 */
TEST(IoPluginTest, too_many_threads) {
  // prepare a simple config
  mysql_harness::Config config;
  auto &section = config.add("io");
  section.add("threads", "1025");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // try to init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "option threads in [io] needs value between 0 and 1024 "
            "inclusive, was '1025'");

  harness_plugin_io.deinit(&env);
}

/**
 * explicitly setting a floating point value for 'threads' fails.
 *
 * - TS_FR00_11
 */
TEST(IoPluginTest, threads_is_double_fails) {
  // prepare a simple config
  mysql_harness::Config config;
  auto &section = config.add("io");
  section.add("threads", "1.2");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, &section);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // try to init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_FALSE(env.exit_ok());
  auto err = env.pop_error();
  EXPECT_EQ(std::get<0>(err),
            "option threads in [io] needs value between 0 and 1024 "
            "inclusive, was '1.2'");

  harness_plugin_io.deinit(&env);
}

/**
 * test plugin behaviour if empty [io] section is provided.
 *
 * - TS_FR00_01
 */
TEST(IoPluginTest, empty_config_section) {
  // prepare a simple config
  mysql_harness::Config config;

  // empty [io] section
  config.add("io");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, nullptr);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_TRUE(env.exit_ok()) << std::get<0>(env.pop_error());

  auto &io_comp = IoComponent::get_instance();

  EXPECT_EQ(io_comp.io_threads().size(), std::thread::hardware_concurrency());
  EXPECT_EQ(io_comp.backend_name(), IoBackend::preferred());

  // start the mainloop, which should exit right away
  harness_plugin_io.start(&env);
  ASSERT_TRUE(env.exit_ok());

  // cleanup
  harness_plugin_io.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

/**
 * test plugin behaviour if no [io] section is provided.
 *
 * - TS_FR00_02
 */
TEST(IoPluginTest, no_io_config_section) {
  // prepare a simple config
  mysql_harness::Config config;

  // empty [io] section
  config.add("io");

  mysql_harness::AppInfo appinfo{};
  appinfo.config = &config;

  mysql_harness::PluginFuncEnv env(&appinfo, nullptr);

  // logger needs be initialized before the io-plugin
  harness_plugin_logger.init(&env);

  // init the io-plugin
  harness_plugin_io.init(&env);
  ASSERT_TRUE(env.exit_ok()) << std::get<0>(env.pop_error());

  auto &io_comp = IoComponent::get_instance();

  EXPECT_EQ(io_comp.io_threads().size(), std::thread::hardware_concurrency());
  EXPECT_EQ(io_comp.backend_name(), IoBackend::preferred());

  // start the mainloop, which should exit right away
  harness_plugin_io.start(&env);
  ASSERT_TRUE(env.exit_ok());

  // cleanup
  harness_plugin_io.deinit(&env);
  ASSERT_TRUE(env.exit_ok());
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
