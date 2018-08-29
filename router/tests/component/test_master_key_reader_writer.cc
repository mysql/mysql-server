/*
 Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dim.h"
#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "mysqlrouter/keyring_info.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "script_generator.h"
#include "tcp_port_pool.h"
#include "utils.h"

Path g_origin_path;

/**
 * @file
 * @brief Component Tests for the master-key-reader and master-key-writer
 */

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

class MasterKeyReaderWriterTest : public RouterComponentTest,
                                  public ::testing::Test {
 protected:
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
    tmp_dir_ = get_tmp_dir();
    bootstrap_dir_ = get_tmp_dir();
    logging_folder = Path(tmp_dir_).join("log").str();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});
  }

  void TearDown() override {
    purge_dir(tmp_dir_);
    purge_dir(bootstrap_dir_);
  }

  void write_to_file(const Path &file_path, const std::string &text) {
    std::ofstream master_key_file(file_path.str());
    if (master_key_file.good()) {
      master_key_file << text;
    }
  }

  std::string get_json_file(const Path &data_dir, unsigned server_port) {
    std::map<std::string, std::string> json_vars = {
        {"PRIMARY_HOST", "127.0.0.1:" + std::to_string(server_port)},
        {"PRIMARY_PORT", std::to_string(server_port)},
    };

    // launch the primary node working also as metadata server
    std::string json_primary_node_template =
        data_dir.join("metadata_1_node.js").str();
    std::string json_primary_node =
        Path(tmp_dir_).join("metadata_1_node.json").str();
    rewrite_js_to_tracefile(json_primary_node_template, json_primary_node,
                            json_vars);
    return json_primary_node;
  }

  std::string get_metadata_cache_section(unsigned server_port) {
    return "[metadata_cache:test]\n"
           "router_id=1\n"
           "bootstrap_server_addresses=mysql://localhost:" +
           std::to_string(server_port) +
           "\n"
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "ttl=500\n\n";
  }

  std::string get_metadata_cache_routing_section(const std::string &role,
                                                 const std::string &strategy,
                                                 unsigned router_port) {
    return "[routing:test_default]\n"
           "bind_port=" +
           std::to_string(router_port) + "\n" +
           "destinations=metadata-cache://test/default?role=" + role + "\n" +
           "protocol=classic\n" + "routing_strategy=" + strategy + "\n";
  }

  KeyringInfo init_keyring() {
    ScriptGenerator script_generator(g_origin_path, tmp_dir_);

    KeyringInfo keyring_info;
    keyring_info.set_master_key_reader(script_generator.get_reader_script());
    keyring_info.set_master_key_writer(script_generator.get_writer_script());
    keyring_info.set_keyring_file(Path(tmp_dir_).join("keyring").str());

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
    ScriptGenerator script_generator(g_origin_path, tmp_dir_);
    std::map<std::string, std::string> default_section = get_DEFAULT_defaults();
    default_section["logging_folder"] = logging_folder;
    default_section["keyring_path"] = Path(tmp_dir_).join("keyring").str();

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
    ScriptGenerator script_generator(g_origin_path, tmp_dir_);

    auto default_section = get_DEFAULT_defaults();
    default_section["logging_folder"] = logging_folder;
    default_section["keyring_path"] = Path(tmp_dir_).join("keyring").str();
    default_section["master_key_reader"] =
        script_generator.get_reader_incorrect_master_key_script();
    default_section["master_key_writer"] = script_generator.get_writer_script();

    return default_section;
  }

  TcpPortPool port_pool_;
  std::string tmp_dir_;
  std::string bootstrap_dir_;
  std::string logging_folder;
  std::string master_key_;
};

/**
 * @test
 *       verify that when bootstrap is launched using --master-key-reader and
 *       --master-key-writer options then master key file is not created.
 */
TEST_F(MasterKeyReaderWriterTest,
       NoMasterKeyFileWhenBootstrapPassWithMasterKeyReader) {
  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host dont.query.dns" + " --directory=" + bootstrap_dir_ +
      " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(30000), 0);
  EXPECT_TRUE(
      router.expect_output("MySQL Router  has now been configured for the "
                           "InnoDB cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path tmp(bootstrap_dir_);
  Path master_key_file(tmp.join("mysqlrouter.key").str());

  ASSERT_FALSE(master_key_file.exists());

  Path keyring_file(tmp.join("data").join("keyring").str());
  ASSERT_TRUE(keyring_file.exists());

  Path dir(tmp_dir_);
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
  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--directory=" + bootstrap_dir_ + " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script() +
      " --report-host dont.query.dns" +
      " --bootstrap=127.0.0.1:" + std::to_string(server_port));

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(30000), 0);
  EXPECT_TRUE(
      router.expect_output("MySQL Router  has now been configured for the "
                           "InnoDB cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path tmp(bootstrap_dir_);
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
 * reader cannot be executed, then bootstrap fails and appropriate error message
 * is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterTest, BootstrapFailsWhenCannotRunMasterKeyReader) {
  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host dont.query.dns" + " --directory=" + bootstrap_dir_ +
      " --force" +
      " --master-key-reader=" + script_generator.get_fake_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_EQ(router.wait_for_exit(), 1);
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
  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host dont.query.dns" + " --directory=" + bootstrap_dir_ +
      " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_fake_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_EQ(router.wait_for_exit(), 1);
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
  mysqlrouter::mkdir(Path(tmp_dir_).join("data").str(), 0777);
  // create keyring file
  Path keyring_path(Path(tmp_dir_).join("data").join("keyring").str());

  write_to_file(keyring_path, "keyring file content");

  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --directory=" + bootstrap_dir_ + " --force" +
      " --master-key-reader=" + script_generator.get_fake_reader_script() +
      " --master-key-writer=" + script_generator.get_fake_writer_script() +
      " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_EQ(router.wait_for_exit(), 1);
  ASSERT_THAT(keyring_path.str(), FileContentEqual("keyring file content"));
}

/**
 * @test
 *       verify that if --master-key-reader option is used and bootstrap fails,
 *       then original master key is restored.
 */
TEST_F(MasterKeyReaderWriterTest, MasterKeyRestoredWhenBootstrapFails) {
  // create file that is used by master-key-reader and master-key-writer
  Path master_key_path(Path(tmp_dir_).join("master_key").str());
  write_to_file(master_key_path, "");

  unsigned server_port = port_pool_.get_next_available();
  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --directory=" + bootstrap_dir_ + " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_EQ(router.wait_for_exit(), 1);
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
  write_to_file(Path(tmp_dir_).join("master_key"), "");

  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host dont.query.dns" + " --directory=" + bootstrap_dir_ +
      " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(30000), 0);
  EXPECT_TRUE(
      router.expect_output("MySQL Router  has now been configured for the "
                           "InnoDB cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path dir(tmp_dir_);
  Path data_file(dir.join("master_key").str());
  ASSERT_TRUE(data_file.exists());
  ASSERT_THAT(data_file.str(), FileContentNotEqual(""));
}

/*
 * @test
 *       verify that if master key exists and is not empty and bootstrap pass,
 * then original master key is not overriden.
 */
TEST_F(MasterKeyReaderWriterTest,
       DontWriteMasterKeyAtBootstrapIfMasterKeyAlreadyExists) {
  write_to_file(Path(tmp_dir_).join("master_key"), "master key value");

  unsigned server_port = port_pool_.get_next_available();
  auto server_mock = launch_mysql_server_mock(
      get_data_dir().join("bootstrap.js").str(), server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host dont.query.dns" + " --directory=" + bootstrap_dir_ +
      " --force" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_EQ(router.wait_for_exit(), 0) << router.get_full_output();
  ASSERT_THAT(Path(tmp_dir_).join("master_key").str(),
              FileContentEqual("master key value"));
}

/**
 * @test
 *       verify that when master key returned by master-key-reader is correct,
 *       then launching the router succeeds
 *
 */
TEST_F(MasterKeyReaderWriterTest, ConnectToMetadataServerPass) {
  unsigned server_port = port_pool_.get_next_available();
  unsigned router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  std::string json_primary_node = get_json_file(get_data_dir(), server_port);
  init_keyring();

  // launch server
  auto server = launch_mysql_server_mock(json_primary_node, server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server.get_full_output();

  auto default_section_map = get_default_section_map();
  // launch the router with metadata-cache configuration
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  auto router = RouterComponentTest::launch_router(
      "-c " + create_config_file(conf_dir,
                                 "[logger]\nlevel = DEBUG\n" +
                                     metadata_cache_section + routing_section,
                                 &default_section_map));

  // in windows waiting for the router's keyring reader takes about 2seconds,
  // and we need to do 3 rounds
  EXPECT_TRUE(wait_for_port_ready(router_port, 10000))
      << router.get_full_output();

  auto matcher = [&](const std::string &line) -> bool {
    return line.find("Connected with metadata server running on") != line.npos;
  };

  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", matcher,
                           std::chrono::milliseconds(10000)))
      << router.get_full_output();
}

/**
 * @test
 *       verify that when master key returned by master-key-reader is correct
 *       and then launching the router succeeds, then master-key is not written
 *       to log files.
 *
 */
TEST_F(MasterKeyReaderWriterTest,
       NoMasterKeyInLogsWhenConnectToMetadataServerPass) {
  unsigned server_port = port_pool_.get_next_available();
  unsigned router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  std::string json_primary_node = get_json_file(get_data_dir(), server_port);
  init_keyring();

  // launch server
  auto server = launch_mysql_server_mock(json_primary_node, server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server.get_full_output();

  auto default_section_map = get_default_section_map();
  // launch the router with metadata-cache configuration
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  auto router = RouterComponentTest::launch_router(
      "-c " + create_config_file(conf_dir,
                                 "[logger]\nlevel = DEBUG\n" +
                                     metadata_cache_section + routing_section,
                                 &default_section_map));

  // in windows waiting for the router's keyring reader takes about 2seconds,
  // and we need to do 3 rounds
  EXPECT_TRUE(wait_for_port_ready(router_port, 10000))
      << router.get_full_output();

  auto matcher = [&, this](const std::string &line) -> bool {
    return line.find(master_key_) != line.npos;
  };

  EXPECT_FALSE(find_in_file(logging_folder + "/mysqlrouter.log", matcher,
                            std::chrono::milliseconds(1000)))
      << router.get_full_output();
}

/**
 * @test
 *       verify that when cannot run master-key-reader in order to read master
 * key then launching the router fails.
 */
TEST_F(MasterKeyReaderWriterTest, CannotLaunchRouterWhenNoMasterKeyReader) {
  unsigned server_port = port_pool_.get_next_available();
  unsigned router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  std::string json_primary_node = get_json_file(get_data_dir(), server_port);
  init_keyring();

  // launch second metadata server
  auto server = launch_mysql_server_mock(json_primary_node, server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server.get_full_output();

  auto default_section_map = get_default_section_map(true, true);
  // launch the router with metadata-cache configuration
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  auto router = RouterComponentTest::launch_router(
      "-c " + create_config_file(conf_dir,
                                 "[logger]\nlevel = DEBUG\n" +
                                     metadata_cache_section + routing_section,
                                 &default_section_map));

  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *       verify that when password fetched using --master-key-reader is
 * incorrect, then launching the router fails with appropriate log message.
 */
TEST_F(MasterKeyReaderWriterTest, CannotLaunchRouterWhenMasterKeyIncorrect) {
  unsigned server_port = port_pool_.get_next_available();
  unsigned router_port = port_pool_.get_next_available();
  std::string metadata_cache_section = get_metadata_cache_section(server_port);
  std::string routing_section =
      get_metadata_cache_routing_section("PRIMARY", "round-robin", router_port);

  std::string json_primary_node = get_json_file(get_data_dir(), server_port);
  init_keyring();

  // launch second metadata server
  auto server = launch_mysql_server_mock(json_primary_node, server_port, false);
  bool server_ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(server_ready) << server.get_full_output();

  auto incorrect_master_key_default_section_map =
      get_incorrect_master_key_default_section_map();
  // launch the router with metadata-cache configuration
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  auto router = RouterComponentTest::launch_router(
      "-c " + create_config_file(conf_dir,
                                 "[logger]\nlevel = DEBUG\n" +
                                     metadata_cache_section + routing_section,
                                 &incorrect_master_key_default_section_map));

  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();
}
/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class MasterKeyReaderWriterSystemDeploymentTest : public RouterComponentTest,
                                                  public ::testing::Test {
 protected:
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
    init_tmp_dir();

    set_mysqlrouter_exec(Path(exec_file_));
  }

  void TearDown() override {
#ifdef __APPLE__
    unlink(library_link_file_.c_str());
#endif
    purge_dir(tmp_dir_);
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
    tmp_dir_ = get_tmp_dir();
    mysqlrouter::mkdir(tmp_dir_ + "/stage", 0700);
    mysqlrouter::mkdir(tmp_dir_ + "/stage/bin", 0700);
    exec_file_ = tmp_dir_ + "/stage/bin/mysqlrouter";
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
        std::string(Path(tmp_dir_).real_path().str() + "/stage/lib");

    if (symlink(library_output_dir.c_str(), library_link_file_.c_str())) {
      throw std::runtime_error(
          "Could not create symbolic link to library_output_directory: " +
          std::to_string(errno));
    }
#endif
    config_file_ = tmp_dir_ + "/stage/mysqlrouter.conf";
  }

  RouterComponentTest::CommandHandle run_server_mock() {
    const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
    server_port_ = port_pool_.get_next_available();

    // launch mock server and wait for it to start accepting connections
    auto server_mock = launch_mysql_server_mock(json_stmts, server_port_);
    EXPECT_TRUE(wait_for_port_ready(server_port_, 1000))
        << "Timed out waiting for mock server port ready\n"
        << server_mock.get_full_output();
    return server_mock;
  }

  TcpPortPool port_pool_;

  std::string tmp_dir_;
  std::string exec_file_;
  std::string config_file_;
#ifdef __APPLE__
  std::string library_link_file_;
#endif

  unsigned server_port_;
};

/**
 * @test
 *      Verify if bootstrap with --master-key-reader and --master-key-writer
 *      and with system deployment layout then master key file
 * (stage/mysqlrouter.key) is not generated.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest, BootstrapPass) {
  auto server_mock = run_server_mock();
  bool server_ready = wait_for_port_ready(server_port_, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
      " --report-host dont.query.dns" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 0))
      << router.get_full_output();

  EXPECT_TRUE(
      router.expect_output("MySQL Router  has now been configured for the "
                           "InnoDB cluster 'mycluster'"))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  Path dir(tmp_dir_);
  Path data_file(dir.join("stage").join("mysqlrouter.key").str());
  ASSERT_FALSE(data_file.exists());
}

/**
 * @test
 *       verify if bootstrap with --master-key-reader and with system deployment
 * layout, but specified reader cannot be executed, then bootstrap fails and
 * appropriate error message is printed to standard output.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       BootstrapFailsWhenCannotRunMasterKeyReader) {
  auto server_mock = run_server_mock();
  bool server_ready = wait_for_port_ready(server_port_, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
      " --report-host dont.query.dns" +
      " --master-key-reader=" + script_generator.get_fake_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

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
  auto server_mock = run_server_mock();
  bool server_ready = wait_for_port_ready(server_port_, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
      " --report-host dont.query.dns" +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_fake_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

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
  mysqlrouter::mkdir(Path(tmp_dir_).join("stage").join("data").str(), 0777);
  // create keyring file
  Path keyring_path(
      Path(tmp_dir_).join("stage").join("data").join("keyring").str());
  // set original keyring file
  write_to_file(keyring_path, "keyring file content");

  auto server_mock = run_server_mock();
  bool server_ready = wait_for_port_ready(server_port_, 1000);
  EXPECT_TRUE(server_ready) << server_mock.get_full_output();

  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
      " --master-key-reader=" + script_generator.get_fake_reader_script() +
      " --master-key-writer=" + script_generator.get_fake_writer_script() +
      " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  ASSERT_THAT(keyring_path.str(), FileContentEqual("keyring file content"));
}

/**
 * @test
 *       verify bootstrap with --master-key-reader and system deployment layout
 *       and bootstrap fails, then original master key is restored.
 */
TEST_F(MasterKeyReaderWriterSystemDeploymentTest,
       MasterKeyRestoredWhenBootstrapFails) {
  // create file with master key
  Path master_key_path(Path(tmp_dir_).join("master_key").str());
  write_to_file(master_key_path, "");

  unsigned server_port = port_pool_.get_next_available();
  ScriptGenerator script_generator(g_origin_path, tmp_dir_);

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --master-key-reader=" + script_generator.get_reader_script() +
      " --master-key-writer=" + script_generator.get_writer_script());

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping failed
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  ASSERT_THAT(master_key_path.str(), FileContentEqual(""));
}

#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
