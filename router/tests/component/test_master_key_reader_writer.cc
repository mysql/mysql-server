/*
 Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "dim.h"
#include "keyring/keyring_manager.h"
#include "mock_server_testutils.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/utils.h"  // copy_file
#include "process_manager.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "script_generator.h"
#include "tcp_port_pool.h"

/**
 * @file
 * @brief Component Tests for the master-key-reader and master-key-writer
 */

using namespace std::chrono_literals;

MATCHER_P(FileContentEqual, master_key, "") {
  std::ifstream file(arg);
  std::stringstream file_content;
  file_content << file.rdbuf();
  return file_content.str() == master_key;
}

MATCHER_P(FileContentNotEqual, master_key, "") {
  std::ifstream file(arg);
  std::stringstream file_content;
  file_content << file.rdbuf();
  return file_content.str() != master_key;
}

class MasterKeyReaderWriterTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});
  }

  void write_to_file(const Path &file_path, const std::string &text) {
    std::ofstream master_key_file(file_path.str());
    if (master_key_file.good()) {
      master_key_file << text;
    }
  }

  static std::pair<std::string, std::map<std::string, std::string>>
  metadata_cache_section(uint16_t server_port) {
    return {"metadata_cache:test",
            {{"router_id", "1"},
             {"bootstrap_server_addresses",
              "mysql://localhost:" + std::to_string(server_port)},
             {"user", "mysql_router1_user"},
             {"metadata_cluster", "test"},
             {"ttl", "500"}}};
  }

  static std::string get_metadata_cache_section(unsigned server_port) {
    auto section = metadata_cache_section(server_port);

    return mysql_harness::ConfigBuilder::build_section(section.first,
                                                       section.second);
  }

  static std::pair<std::string, std::map<std::string, std::string>>
  metadata_cache_routing_section(const std::string &role,
                                 const std::string &strategy,
                                 uint16_t router_port) {
    return {"routing:test_default",
            {{"bind_port", std::to_string(router_port)},
             {"destinations", "metadata-cache://test/default?role=" + role},
             {"protocol", "classic"},
             {"routing_strategy", strategy}}};
  }

  static std::string get_metadata_cache_routing_section(
      const std::string &role, const std::string &strategy,
      uint16_t router_port) {
    auto section = metadata_cache_routing_section(role, strategy, router_port);

    return mysql_harness::ConfigBuilder::build_section(section.first,
                                                       section.second) +
           "\n";
  }

  KeyringInfo init_keyring() {
    ScriptGenerator script_generator(ProcessManager::get_origin(),
                                     tmp_dir_.name());

    KeyringInfo keyring_info;
    keyring_info.set_master_key_reader(script_generator.get_reader_script());
    keyring_info.set_master_key_writer(script_generator.get_writer_script());
    keyring_info.set_keyring_file(Path(tmp_dir_.name()).join("keyring").str());

    keyring_info.generate_master_key();
    master_key_ = keyring_info.get_master_key();
    keyring_info.add_router_id_to_env(1);
    keyring_info.write_master_key();
    mysql_harness::init_keyring_with_key(keyring_info.get_keyring_file(),
                                         keyring_info.get_master_key(), true);

    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    return keyring_info;
  }

  std::map<std::string, std::string> get_default_section_map(
      bool assign_fake_reader = false, bool assign_fake_writer = false) {
    ScriptGenerator script_generator(ProcessManager::get_origin(),
                                     tmp_dir_.name());
    std::map<std::string, std::string> default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] =
        Path(tmp_dir_.name()).join("keyring").str();

    if (assign_fake_reader)
      default_section["master_key_reader"] =
          script_generator.get_fake_reader_script();
    else
      default_section["master_key_reader"] =
          script_generator.get_reader_script();

    if (assign_fake_writer)
      default_section["master_key_writer"] =
          script_generator.get_fake_writer_script();
    else
      default_section["master_key_writer"] =
          script_generator.get_writer_script();

    return default_section;
  }

  std::map<std::string, std::string>
  get_incorrect_master_key_default_section_map() {
    ScriptGenerator script_generator(ProcessManager::get_origin(),
                                     tmp_dir_.name());

    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] =
        Path(tmp_dir_.name()).join("keyring").str();
    default_section["master_key_reader"] =
        script_generator.get_reader_incorrect_master_key_script();
    default_section["master_key_writer"] = script_generator.get_writer_script();

    return default_section;
  }

  auto &launch_router(const std::vector<std::string> &params,
                      int expected_exit_code = EXIT_SUCCESS) {
    return ProcessManager::launch_router(
        params, expected_exit_code,
        /*catch_stderr=*/true,
        /*with_sudo=*/false,
        /*wait_for_notify_ready=*/-1s,
        RouterComponentBootstrapTest::kBootstrapOutputResponder);
  }

  TempDirectory tmp_dir_;
  TempDirectory bootstrap_dir_;
  std::string master_key_;
};

/**
 * @test
 *       verify that when bootstrap is launched using --master-key-reader and
 *       --master-key-writer options then master key file is not created.
 */
TEST_F(MasterKeyReaderWriterTest,
       NoMasterKeyFileWhenBootstrapPassWithMasterKeyReader) {
  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "--directory=" + bootstrap_dir_.name(),
      "--force",
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script(),
  });

  // check if the bootstrapping was successful
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS, 30000ms));
  EXPECT_TRUE(router.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path tmp(bootstrap_dir_.name());
  Path master_key_file(tmp.join("mysqlrouter.key").str());

  ASSERT_FALSE(master_key_file.exists());

  Path keyring_file(tmp.join("data").join("keyring").str());
  ASSERT_TRUE(keyring_file.exists());

  Path dir(tmp_dir_.name());
  Path data_file(dir.join("master_key").str());
  ASSERT_TRUE(data_file.exists());
}

/**
 * @test
 *       verify that when bootstrap is launched using --master-key-reader and
 *       --master-key-writer options then generated config file contains
 *       entries for master_key_reader and master_key_writer.
 *       Also, verify that --bootstrap can be specified after --master-key-*
 *       options (all other tests will use it in the beginning).
 */
TEST_F(MasterKeyReaderWriterTest,
       CheckConfigFileWhenBootstrapPassWithMasterKeyReader) {
  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--directory=" + bootstrap_dir_.name(),
      "--force",
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script(),
      "--report-host",
      "dont.query.dns",
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
  });

  // check if the bootstrapping was successful
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS, 30000ms));
  EXPECT_TRUE(
      router.expect_output("MySQL Router configured for the "
                           "InnoDB Cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path tmp(bootstrap_dir_.name());
  Path config_file(tmp.join("mysqlrouter.conf").str());
  ASSERT_TRUE(config_file.exists());

  // read master-key-reader and master-key-writer
  std::string master_key_reader = "", master_key_writer = "";

  std::ifstream file(config_file.str());
  std::istream_iterator<std::string> beg(file), eof;
  std::vector<std::string> lines(beg, eof);

  for (const auto &line : lines) {
    int index = line.find('=');
    if (line.substr(0, index) == "master_key_reader")
      master_key_reader = line.substr(index + 1);
    else if (line.substr(0, index) == "master_key_writer")
      master_key_writer = line.substr(index + 1);
  }

  ASSERT_THAT(master_key_reader,
              testing::Eq(script_generator.get_reader_script()));
  ASSERT_THAT(master_key_writer,
              testing::Eq(script_generator.get_writer_script()));
}

/**
 * @test
 *       verify that when --master-key-reader option is used, but specified
 * reader cannot be executed, then bootstrap fails and appropriate error
 * message is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterTest, BootstrapFailsWhenCannotRunMasterKeyReader) {
  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--report-host",
          "dont.query.dns",
          "--directory=" + bootstrap_dir_.name(),
          "--force",
          "--master-key-reader=" + script_generator.get_fake_reader_script(),
          "--master-key-writer=" + script_generator.get_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
  EXPECT_TRUE(router.expect_output(
      "Error: Cannot fetch master key file using master key reader"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
}

/**
 * @test
 *       verify that when --master-key-writer option is used, but specified
 * master key writer cannot be executed, then bootstrap fails and appropriate
 * error message is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterTest, BootstrapFailsWhenCannotRunMasterKeyWriter) {
  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--report-host",
          "dont.query.dns",
          "--directory=" + bootstrap_dir_.name(),
          "--force",
          "--master-key-reader=" + script_generator.get_reader_script(),
          "--master-key-writer=" + script_generator.get_fake_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
  EXPECT_TRUE(router.expect_output(
      "Error: Cannot write master key file using master key writer"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
}

/**
 * @test
 *       verify that if keyring file already exists and --master-key-reader
 * option is used and bootstrap fails, then original keyring file is restored.
 */
TEST_F(MasterKeyReaderWriterTest, KeyringFileRestoredWhenBootstrapFails) {
  mysql_harness::mkdir(Path(tmp_dir_.name()).join("data").str(), 0777);
  // create keyring file
  Path keyring_path(Path(tmp_dir_.name()).join("data").join("keyring").str());

  write_to_file(keyring_path, "keyring file content");

  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--directory=" + bootstrap_dir_.name(),
          "--force",
          "--master-key-reader=" + script_generator.get_fake_reader_script(),
          "--master-key-writer=" + script_generator.get_fake_writer_script(),
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
  ASSERT_THAT(keyring_path.str(), FileContentEqual("keyring file content"));
}

/**
 * @test
 *       verify that if --master-key-reader option is used and bootstrap
 * fails, then original master key is restored.
 */
TEST_F(MasterKeyReaderWriterTest, MasterKeyRestoredWhenBootstrapFails) {
  // create file that is used by master-key-reader and master-key-writer
  Path master_key_path(Path(tmp_dir_.name()).join("master_key").str());
  write_to_file(master_key_path, "");

  unsigned server_port = port_pool_.get_next_available();
  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--directory=" + bootstrap_dir_.name(),
          "--force",
          "--master-key-reader=" + script_generator.get_reader_script(),
          "--master-key-writer=" + script_generator.get_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
  ASSERT_THAT(master_key_path.str(), FileContentEqual(""));
}

/*
 * @test
 *       verify that if --master-key-reader option is used and original master
 * key is empty and bootstrap passes, then new master key is stored using
 * --master-key-writer.
 */
TEST_F(MasterKeyReaderWriterTest,
       IsNewMasterKeyIfReaderReturnsEmptyKeyAndBootstrapPass) {
  write_to_file(Path(tmp_dir_.name()).join("master_key"), "");

  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "--directory=" + bootstrap_dir_.name(),
      "--force",
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script(),
  });

  // check if the bootstrapping was successful
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS, 30000ms));
  EXPECT_TRUE(
      router.expect_output("MySQL Router configured for the "
                           "InnoDB Cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path dir(tmp_dir_.name());
  Path data_file(dir.join("master_key").str());
  ASSERT_TRUE(data_file.exists());
  ASSERT_THAT(data_file.str(), FileContentNotEqual(""));
}

/*
 * @test
 *       verify that if master key exists and is not empty and bootstrap pass,
 * then original master key is not overridden.
 */
TEST_F(MasterKeyReaderWriterTest,
       DontWriteMasterKeyAtBootstrapIfMasterKeyAlreadyExists) {
  write_to_file(Path(tmp_dir_.name()).join("master_key"), "master key value");

  auto server_port = port_pool_.get_next_available();
  auto &server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap_gr.js").str(), server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "--directory=" + bootstrap_dir_.name(),
      "--force",
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script(),
  });

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router));
  ASSERT_THAT(Path(tmp_dir_.name()).join("master_key").str(),
              FileContentEqual("master key value"));
}

/**
 * @test
 *       verify that when master key returned by master-key-reader is correct,
 *       then launching the router succeeds
 *
 */
TEST_F(MasterKeyReaderWriterTest, ConnectToMetadataServerPass) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // launch server
  /*auto &server =*/launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes.js").str(), server_port);

  // launch the router with metadata-cache configuration
  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section(metadata_cache_section(server_port))
                    .section(metadata_cache_routing_section(
                        "PRIMARY", "round-robin", router_port));

  // set master-key-reader/writer in DEFAULT section
  auto &default_section = writer.sections().at("DEFAULT");

  const auto keyring_info = init_keyring();

  default_section["keyring_path"] = keyring_info.get_keyring_file();
  default_section["master_key_reader"] = keyring_info.get_master_key_reader();
  default_section["master_key_writer"] = keyring_info.get_master_key_writer();

  auto &router =
      router_spawner()
          // ERROR: 1273 Syntax Error at: ROLLBACK ...
          // it will never get READY.
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::RUNNING)
          .spawn({"-c", writer.write()});

  EXPECT_TRUE(wait_log_contains(
      router, "Connected with metadata server running on", 10s));
}

/**
 * @test
 *       verify that when master key returned by master-key-reader is correct
 *       and then launching the router succeeds, then master-key is not
 * written to log files.
 *
 */
TEST_F(MasterKeyReaderWriterTest,
       NoMasterKeyInLogsWhenConnectToMetadataServerPass) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // launch server
  auto &server = launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes.js").str(), server_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server, server_port, 10000ms));

  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section(metadata_cache_section(server_port))
                    .section(metadata_cache_routing_section(
                        "PRIMARY", "round-robin", router_port));

  // set master-key-reader/writer in DEFAULT section
  auto &default_section = writer.sections().at("DEFAULT");

  const auto keyring_info = init_keyring();

  default_section["keyring_path"] = keyring_info.get_keyring_file();
  default_section["master_key_reader"] = keyring_info.get_master_key_reader();
  default_section["master_key_writer"] = keyring_info.get_master_key_writer();

  // launch the router with metadata-cache configuration

  auto &router =
      router_spawner()
          // ERROR: 1273 Syntax Error at: ROLLBACK ...
          // it will never get READY.
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::RUNNING)
          .spawn({"-c", writer.write()});

  // wait a bit for log to generate.
  std::this_thread::sleep_for(1s);

  auto log_lines =
      mysql_harness::split_string(router.get_logfile_content(), '\n');
  EXPECT_THAT(
      log_lines,
      ::testing::Not(::testing::Contains(::testing::HasSubstr(master_key_))));
}

/**
 * @test
 *       verify that when cannot run master-key-reader in order to read master
 * key then launching the router fails.
 */
TEST_F(MasterKeyReaderWriterTest, CannotLaunchRouterWhenNoMasterKeyReader) {
  auto server_port = port_pool_.get_next_available();
  auto router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  init_keyring();

  // launch second metadata server
  auto &server = launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes.js").str(), server_port,
      false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server, server_port));

  auto default_section_map = get_default_section_map(true, true);
  // launch the router with metadata-cache configuration
  TempDirectory conf_dir("conf");
  auto &router = launch_router(
      {
          "-c",
          create_config_file(conf_dir.name(),
                             metadata_cache_section + routing_section,
                             &default_section_map),
      },
      EXIT_FAILURE);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
}

/**
 * @test
 *       verify that when password fetched using --master-key-reader is
 * incorrect, then launching the router fails with appropriate log message.
 */
TEST_F(MasterKeyReaderWriterTest, CannotLaunchRouterWhenMasterKeyIncorrect) {
  auto server_port = port_pool_.get_next_available();
  auto router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  init_keyring();

  // launch second metadata server
  auto &server = launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes.js").str(), server_port,
      false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server, server_port));

  auto incorrect_master_key_default_section_map =
      get_incorrect_master_key_default_section_map();
  // launch the router with metadata-cache configuration
  TempDirectory conf_dir("conf");
  auto &router = launch_router(
      {"-c", create_config_file(conf_dir.name(),
                                metadata_cache_section + routing_section,
                                &incorrect_master_key_default_section_map)},
      EXIT_FAILURE);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));
}
/*
 * These tests are executed only for STANDALONE layout and are not executed
 * for Windows. Bootstrap for layouts different than STANDALONE use
 * directories to which tests don't have access (see install_layout.cmake).
 */
Path g_origin_path;
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class MasterKeyReaderWriterSystemDeploymentTest
    : public MasterKeyReaderWriterTest {
 protected:
  void SetUp() override {
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
    RouterComponentTest::SetUp();
    init_tmp_dir();

    set_mysqlrouter_exec(Path(exec_file_));
  }

  void TearDown() override {
    RouterComponentTest::TearDown();
#ifdef __APPLE__
    unlink(library_link_file_.c_str());
#endif
  }

  void write_to_file(const Path &file_path, const std::string &text) {
    std::ofstream master_key_file(file_path.str());
    if (master_key_file.good()) {
      master_key_file << text;
    }
  }

  /*
   * Create temporary directory that represents system deployment
   * layout for mysql bootstrap. A mysql executable is copied to
   * tmp_dir_/stage/bin/ and then an execution permission is assigned to it.
   *
   * After the test is completed, the whole temporary directory is deleted.
   */
  void init_tmp_dir() {
    mysql_harness::mkdir(tmp_dir_.name() + "/stage", 0700);
    mysql_harness::mkdir(tmp_dir_.name() + "/stage/bin", 0700);
    exec_file_ = tmp_dir_.name() + "/stage/bin/mysqlrouter";
    mysqlrouter::copy_file(get_mysqlrouter_exec().str(), exec_file_);
#ifndef _WIN32
    chmod(exec_file_.c_str(), 0700);
#endif

    // on MacOS we need to create symlink to library_output_directory
    // inside our temp dir as mysqlrouter has @loader_path/../lib
    // hardcoded by MYSQL_ADD_EXECUTABLE
#ifdef __APPLE__
    std::string cur_dir_name = g_origin_path.real_path().dirname().str();
    const std::string library_output_dir =
        cur_dir_name + "/library_output_directory";

    library_link_file_ =
        std::string(Path(tmp_dir_.name()).real_path().str() + "/stage/lib");

    if (symlink(library_output_dir.c_str(), library_link_file_.c_str())) {
      throw std::runtime_error(
          "Could not create symbolic link to library_output_directory: " +
          std::to_string(errno));
    }
#endif
    config_file_ = tmp_dir_.name() + "/stage/mysqlrouter.conf";
  }

  auto &run_server_mock() {
    const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
    server_port_ = port_pool_.get_next_available();

    // launch mock server and wait for it to start accepting connections
    auto &server_mock = launch_mysql_server_mock(json_stmts, server_port_);
    return server_mock;
  }

  std::string exec_file_;
  std::string config_file_;
#ifdef __APPLE__
  std::string library_link_file_;
#endif

  uint16_t server_port_;
};

/**
 * @test
 *      Verify if bootstrap with --master-key-reader and --master-key-writer
 *      and with system deployment layout then master key file
 * (stage/mysqlrouter.key) is not generated.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest, BootstrapPass) {
  auto &server_mock = run_server_mock();
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port_));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
      "--report-host",
      "dont.query.dns",
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script(),
  });

  // check if the bootstrapping was successful
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS));

  EXPECT_TRUE(
      router.expect_output("MySQL Router configured for the "
                           "InnoDB Cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path dir(tmp_dir_.name());
  Path data_file(dir.join("stage").join("mysqlrouter.key").str());
  ASSERT_FALSE(data_file.exists());
}

/**
 * @test
 *       verify if bootstrap with --master-key-reader and with system
 * deployment layout, but specified reader cannot be executed, then bootstrap
 * fails and appropriate error message is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       BootstrapFailsWhenCannotRunMasterKeyReader) {
  auto &server_mock = run_server_mock();
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port_));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--report-host",
          "dont.query.dns",
          "--master-key-reader=" + script_generator.get_fake_reader_script(),
          "--master-key-writer=" + script_generator.get_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  EXPECT_TRUE(router.expect_output(
      "Error: Cannot fetch master key file using master key reader"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
}

/**
 * @test
 *       verify if bootstrap with --master-key-writer and system deployment
 * layout, but specified master key writer cannot be executed, then bootstrap
 * fails and appropriate error message is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       BootstrapFailsWhenCannotRunMasterKeyWriter) {
  auto &server_mock = run_server_mock();
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port_));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--report-host",
          "dont.query.dns",
          "--master-key-reader=" + script_generator.get_reader_script(),
          "--master-key-writer=" + script_generator.get_fake_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  EXPECT_TRUE(router.expect_output(
      "Error: Cannot write master key file using master key writer"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
}

/**
 * @test
 *       verify that if keyring file already exists and bootstrap with
 * --master-key-reader and system deployment layout and bootstrap fails, then
 * original keyring file is restored.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       KeyringFileRestoredWhenBootstrapFails) {
  mysql_harness::mkdir(Path(tmp_dir_.name()).join("stage").join("data").str(),
                       0777);
  // create keyring file
  Path keyring_path(
      Path(tmp_dir_.name()).join("stage").join("data").join("keyring").str());
  // set original keyring file
  write_to_file(keyring_path, "keyring file content");

  auto &server_mock = run_server_mock();
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port_));

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--connect-timeout=1",
          "--master-key-reader=" + script_generator.get_fake_reader_script(),
          "--master-key-writer=" + script_generator.get_fake_writer_script(),
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  ASSERT_THAT(keyring_path.str(), FileContentEqual("keyring file content"));
}

/**
 * @test
 *       verify bootstrap with --master-key-reader and system deployment
 * layout and bootstrap fails, then original master key is restored.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       MasterKeyRestoredWhenBootstrapFails) {
  // create file with master key
  Path master_key_path(Path(tmp_dir_.name()).join("master_key").str());
  write_to_file(master_key_path, "");

  unsigned server_port = port_pool_.get_next_available();
  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   tmp_dir_.name());

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--master-key-reader=" + script_generator.get_reader_script(),
          "--master-key-writer=" + script_generator.get_writer_script(),
      },
      EXIT_FAILURE);

  // check if the bootstrapping failed
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  ASSERT_THAT(master_key_path.str(), FileContentEqual(""));
}

#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
