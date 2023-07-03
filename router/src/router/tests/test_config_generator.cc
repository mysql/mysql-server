/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

// must be the first header, don't move it
#include <gtest/gtest_prod.h>  // FRIEND_TEST

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <streambuf>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <mysql.h>

#include "cluster_metadata.h"
#include "common.h"  // list_elements
#include "config_generator.h"
#include "dim.h"
#include "gtest_consoleoutput.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"
#include "mysql_session_replayer.h"
#include "mysqld_error.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "router_app.h"
#include "router_test_helpers.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

#define ASSERT_NO_ERROR(x) \
  ASSERT_THAT((x), ::testing::Truly([](const auto &t) { return bool(t); }))

std::string g_program_name;

class TestConfigGenerator : public mysqlrouter::ConfigGenerator {
 public:
  using __base = ConfigGenerator;

  using __base::__base;

  using __base::ExistingConfigOptions;

  void create_accounts(const std::string &username,
                       const std::set<std::string> &hostnames,
                       const std::string &password, bool hash_password = false,
                       bool if_not_exists = false) {
    __base::create_accounts(username, hostnames, password, hash_password,
                            if_not_exists);
  }
  void create_config(
      std::ostream &config_file, std::ostream &state_file, uint32_t router_id,
      const std::string &router_name, const std::string &system_username,
      const mysqlrouter::ClusterInfo &cluster_info, const std::string &username,
      const Options &options,
      const std::map<std::string, std::string> &default_paths,
      const std::map<std::string, std::string> &config_overwrites,
      const std::string &state_file_name = "") {
    return __base::create_config(config_file, state_file, router_id,
                                 router_name, system_username, cluster_info,
                                 username, options, default_paths,
                                 config_overwrites, state_file_name);
  }

  Options fill_options(const std::map<std::string, std::string> &user_options,
                       const std::map<std::string, std::string> &default_paths,
                       const ExistingConfigOptions &existing_config_options) {
    return __base::fill_options(user_options, default_paths,
                                existing_config_options);
  }

  std::unique_ptr<mysqlrouter::ClusterMetadata> &metadata() {
    return this->metadata_;
  }

  // we disable this method by overriding - calling it requires sudo access
  void set_script_permissions(
      const std::string &,
      const std::map<std::string, std::string> &) override {}

  void ensure_router_id_is_ours(uint32_t &router_id,
                                const std::string &hostname_override) {
    __base::ensure_router_id_is_ours(router_id, hostname_override);
  }

  uint32_t register_router(const std::string &router_name,
                           const std::string &hostname_override, bool force) {
    return __base::register_router(router_name, hostname_override, force);
  }

  ExistingConfigOptions get_options_from_config_if_it_exists(
      const std::string &config_file_path, const std::string &cluster_name,
      bool forcing_overwrite) {
    return __base::get_options_from_config_if_it_exists(
        config_file_path, cluster_name, forcing_overwrite);
  }

  void create_start_script(const std::string &program_name,
                           const std::string &directory,
                           bool interactive_master_key,
                           const std::map<std::string, std::string> &options) {
    __base::create_start_script(program_name, directory, interactive_master_key,
                                options);
  }

  void create_stop_script(const std::string &directory,
                          const std::map<std::string, std::string> &options) {
    __base::create_stop_script(directory, options);
  }

  std::string create_router_accounts(
      const std::map<std::string, std::string> &user_options,
      const std::set<std::string> &hostnames, const std::string &username,
      const std::string &password, bool password_change_ok) {
    return __base::create_router_accounts(user_options, hostnames, username,
                                          password, password_change_ok);
  }

  static std::set<std::string> get_account_host_args(
      const std::map<std::string, std::vector<std::string>>
          &multivalue_options) noexcept {
    return __base::get_account_host_args(multivalue_options);
  }
};

class ReplayerWithMockSSL : public MySQLSessionReplayer {
 public:
  void set_ssl_options(mysql_ssl_mode ssl_mode, const std::string &tls_version,
                       const std::string &ssl_cipher, const std::string &ca,
                       const std::string &capath, const std::string &crl,
                       const std::string &crlpath) {
    last_ssl_mode = ssl_mode;
    last_tls_version = tls_version;
    last_ssl_cipher = ssl_cipher;
    last_ssl_ca = ca;
    last_ssl_capath = capath;
    last_ssl_crl = crl;
    last_ssl_crlpath = crlpath;
    if (should_throw_) throw Error("", 0);
  }

  void set_ssl_cert(const std::string &cert, const std::string &key) {
    last_ssl_cert = cert;
    last_ssl_key = key;
    if (should_throw_) throw Error("", 0);
  }

  void set_ssl_mode_should_fail(bool flag) { should_throw_ = flag; }

 public:
  mysql_ssl_mode last_ssl_mode;
  std::string last_tls_version;
  std::string last_ssl_cipher;
  std::string last_ssl_ca;
  std::string last_ssl_capath;
  std::string last_ssl_crl;
  std::string last_ssl_crlpath;
  std::string last_ssl_cert;
  std::string last_ssl_key;

 private:
  bool should_throw_ = false;
};

class ConfigGeneratorTest : public ConsoleOutputTest {
 protected:
  void SetUp() override {
    init_test_logger();

    mysql_harness::DIM::instance().set_RandomGenerator(
        []() {
          static mysql_harness::FakeRandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {}
        // don't delete our static!
    );

    mock_mysql.reset(new ReplayerWithMockSSL());
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() { return mock_mysql.get(); },
        [](mysqlrouter::MySQLSession *) {}  // don't try to delete it
    );

    set_origin(Path(g_program_name).dirname());
    ConsoleOutputTest::SetUp();
    config_path = std::make_unique<Path>(Path(g_program_name).dirname());
    config_path->append("Bug24570426.conf");

    default_paths["logging_folder"] = "";
    default_paths["data_folder"] = test_dir.name();
  }

  std::unique_ptr<Path> config_path;
  std::map<std::string, std::string> default_paths;
  std::unique_ptr<ReplayerWithMockSSL> mock_mysql;

  std::string program_name_{g_program_name};

  TempDirectory test_dir;
};

const std::string kServerUrl = "mysql://test:test@127.0.0.1:3060";

using ::testing::Return;
using namespace testing;
using mysql_harness::delete_dir_recursive;
using mysql_harness::delete_file;
using mysqlrouter::ConfigGenerator;

static void common_pass_setup_session(MySQLSessionReplayer *m) {
  m->expect_execute(
      "SET @@SESSION.autocommit=1, @@SESSION.character_set_client=utf8, "
      "@@SESSION.character_set_results=utf8, "
      "@@SESSION.character_set_connection=utf8, "
      "@@SESSION.sql_mode='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_"
      "DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION', "
      "@@SESSION.optimizer_switch='derived_merge=on'");

  m->expect_execute("SET @@SESSION.group_replication_consistency='EVENTUAL'");
}

static void common_pass_schema_version(MySQLSessionReplayer *m) {
  m->expect_query_one(
      "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  m->then_return(3, {// major, minor
                     {m->string_or_null("2"), m->string_or_null("0"),
                      m->string_or_null("3")}});
}

static void common_pass_cluster_type(MySQLSessionReplayer *m) {
  m->expect_query_one(
      "select cluster_type from "
      "mysql_innodb_cluster_metadata.v2_this_instance");
  m->then_return(1, {{m->string_or_null("gr")}});
}

static void common_pass_metadata_supported(MySQLSessionReplayer *m) {
  m->expect_query_one(
      "select count(*) from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters");
  m->then_return(1, {// has_one_gr_cluster
                     {m->string_or_null("1")}});
}

static void common_pass_group_replication_online(MySQLSessionReplayer *m) {
  m->expect_query_one(
      "SELECT member_state FROM performance_schema.replication_group_members "
      "WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char "
      "ascii)");
  m->then_return(1, {// member_state
                     {m->string_or_null("ONLINE")}});
}

static void common_pass_group_has_quorum(MySQLSessionReplayer *m) {
  m->expect_query_one(
      "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) "
      "as num_total FROM performance_schema.replication_group_members");
  m->then_return(2, {// num_onlines, num_total
                     {m->string_or_null("3"), m->string_or_null("3")}});
}

static void common_pass_group_name(MySQLSessionReplayer *m) {
  m->expect_query_one("select @@group_replication_group_name");
  m->then_return(1, {{m->string_or_null("replication-1")}});
}

#if 0
static void common_pass_member_is_primary(MySQLSessionReplayer *m) {
  m->expect_query_one("SELECT @@group_replication_single_primary_mode=1 as single_primary_mode,        (SELECT variable_value FROM performance_schema.global_status WHERE variable_name='group_replication_primary_member') as primary_member,         @@server_uuid as my_uuid");
  m->then_return(3, {
    // single_primary_mode, primary_member, my_uuid
    {m->string_or_null("0"), m->string_or_null("2d52f178-98f4-11e6-b0ff-8cc844fc24bf"), m->string_or_null("2d52f178-98f4-11e6-b0ff-8cc844fc24bf")}
  });
}
#endif

static void common_pass_metadata_checks(MySQLSessionReplayer *m) {
  m->clear_expects();
  common_pass_setup_session(m);
  common_pass_schema_version(m);
  common_pass_cluster_type(m);
  common_pass_metadata_supported(m);
  common_pass_group_replication_online(m);
  common_pass_group_has_quorum(m);
  common_pass_group_name(m);
  // common_pass_member_is_primary(m);
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_one) {
  {
    TestConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});

    mock_mysql->expect_query("").then_return(
        3, {{"91d01072-63cd-11ec-9a29-080027ac264d", "my-cluster",
             "somehost:3306"}});

    const auto cluster_info = config_gen.metadata()->fetch_metadata_servers();

    ASSERT_THAT(mysql_harness::list_elements(cluster_info.metadata_servers),
                Eq("mysql://somehost:3306"));
    ASSERT_THAT(cluster_info.name, Eq("my-cluster"));
  }

  {
    TestConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});

    mock_mysql->expect_query("").then_return(
        3, {{"91d01072-63cd-11ec-9a29-080027ac264d", "my-cluster",
             "somehost:3306"}});

    const auto cluster_info = config_gen.metadata()->fetch_metadata_servers();

    ASSERT_THAT(mysql_harness::list_elements(cluster_info.metadata_servers),
                Eq("mysql://somehost:3306"));
    ASSERT_THAT(cluster_info.name, Eq("my-cluster"));
  }
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_three) {
  {
    TestConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});

    // select c.cluster_id, c.cluster_name, i.address from
    // mysql_innodb_cluster_metadata.v2_instances...
    mock_mysql->expect_query("").then_return(
        3,
        {{"91d0107263cd11ec9a29080027ac264d", "my-cluster", "somehost:3306"},
         {"91d0107263cd11ec9a29080027ac264d", "my-cluster", "otherhost:3306"},
         {"91d0107263cd11ec9a29080027ac264d", "my-cluster", "sumhost:3306"}});

    const auto cluster_info = config_gen.metadata()->fetch_metadata_servers();

    ASSERT_THAT(mysql_harness::list_elements(cluster_info.metadata_servers),
                Eq("mysql://somehost:3306,mysql://otherhost:3306,mysql://"
                   "sumhost:3306"));
    ASSERT_THAT(cluster_info.name, Eq("my-cluster"));
  }
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_invalid) {
  {
    TestConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});

    mock_mysql->expect_query("").then_return(3, {});
    // no replicasets/clusters defined
    ASSERT_THROW(config_gen.metadata()->fetch_metadata_servers(),
                 std::runtime_error);
  }
}

TEST_F(ConfigGeneratorTest, metadata_checks_invalid_data) {
  // invalid number of values returned from schema_version table
  {
    TestConfigGenerator config_gen;

    common_pass_setup_session(mock_mysql.get());
    mock_mysql->expect_query_one(
        "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
    mock_mysql->then_return(1, {// major, [minor missing]
                                {mock_mysql->string_or_null("0")}});

    ASSERT_THROW_LIKE(config_gen.init(kServerUrl, {}), std::out_of_range,
                      "Invalid number of values returned from "
                      "mysql_innodb_cluster_metadata.schema_version: "
                      "expected 2 or 3, got 1");
  }

  {
    TestConfigGenerator config_gen;
    common_pass_setup_session(mock_mysql.get());
    common_pass_schema_version(mock_mysql.get());
    common_pass_cluster_type(mock_mysql.get());
    // invalid number of values returned from query for metadata support
    mock_mysql->expect_query_one(
        "select count(*) from "
        "mysql_innodb_cluster_metadata.v2_gr_clusters");
    mock_mysql->then_return(0,
                            {// [count(*) missing]
                             {}});

    ASSERT_THROW_LIKE(
        config_gen.init(kServerUrl, {}), std::out_of_range,
        "Invalid number of values returned from query for metadata support: "
        "expected 1 got 0");
  }

  // invalid number of values returned from query for member_state
  {
    TestConfigGenerator config_gen;

    common_pass_setup_session(mock_mysql.get());
    common_pass_schema_version(mock_mysql.get());
    common_pass_cluster_type(mock_mysql.get());
    common_pass_metadata_supported(mock_mysql.get());

    mock_mysql->expect_query_one(
        "SELECT member_state FROM performance_schema.replication_group_members "
        "WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char "
        "ascii)");
    mock_mysql->then_return(0, {
                                   // [state field missing]
                               });

    ASSERT_THROW_LIKE(config_gen.init(kServerUrl, {}), std::logic_error,
                      "No result returned for metadata query");
  }

  // invalid number of values returned from query checking for group quorum
  {
    TestConfigGenerator config_gen;

    common_pass_setup_session(mock_mysql.get());
    common_pass_schema_version(mock_mysql.get());
    common_pass_cluster_type(mock_mysql.get());
    common_pass_metadata_supported(mock_mysql.get());
    common_pass_group_replication_online(mock_mysql.get());

    mock_mysql->expect_query_one(
        "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, "
        "COUNT(*) as num_total FROM "
        "performance_schema.replication_group_members");
    mock_mysql->then_return(1, {// num_onlines, [num_total field missing]
                                {mock_mysql->string_or_null("3")}});

    ASSERT_THROW_LIKE(config_gen.init(kServerUrl, {}), std::out_of_range,
                      "Invalid number of values returned from "
                      "performance_schema.replication_group_members: "
                      "expected 2 got 1");
  }
}

/**
 * @test
 * verify that ConfigGenerator::create_accounts() will generate an expected
 * sequence of SQL requests (CREATE USER [IF NOT EXISTS] and GRANTs)
 */
TEST_F(ConfigGeneratorTest, create_accounts_using_password_directly) {
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute("CREATE USER 'cluster_user'@'%' IDENTIFIED BY 'secret'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO 'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'%'")
      .then_ok();

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"%"}, "secret",
                             /*hash password*/ false,
                             /*if not exists*/ false);
}

TEST_F(ConfigGeneratorTest, create_accounts_using_hashed_password) {
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER 'cluster_user'@'%' IDENTIFIED WITH "
          "mysql_native_password "
          "AS '*14E65567ABDB5135D0CFD9A70B3032C179A49EE7'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO 'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'%'")
      .then_ok();

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"%"}, "secret",
                             /*hash password*/ true, /*if not exists*/ false);
}

// expectation: "IF NOT EXISTS " added to "CREATE USER " statement
TEST_F(ConfigGeneratorTest,
       create_accounts_using_hashed_password_if_not_exists) {
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER IF NOT EXISTS 'cluster_user'@'%' IDENTIFIED WITH "
          "mysql_native_password "
          "AS '*14E65567ABDB5135D0CFD9A70B3032C179A49EE7'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO 'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'%'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'%'")
      .then_ok();

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"%"}, "secret",
                             /*hash password*/ true, /*if not exists*/ true);
}

// expectation: CREATE USER and GRANT handle all accounts at once
TEST_F(ConfigGeneratorTest, create_accounts_multiple_accounts) {
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY "
          "'secret','cluster_user'@'host2' IDENTIFIED BY "
          "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                             "secret", /*hash password*/ false,
                             /*if not exists*/ false);
}

// multiple accounts if not exists
// expectation: CREATE USER and GRANT handle all accounts at once
TEST_F(ConfigGeneratorTest, create_accounts_multiple_accounts_if_not_exists) {
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER IF NOT EXISTS "
          "'cluster_user'@'host1' IDENTIFIED BY "
          "'secret','cluster_user'@'host2' IDENTIFIED BY "
          "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                             "secret", /*hash password*/ false,
                             /*if not exists*/ true);
  mock_mysql->execute("END_MARKER");
}

/**
 * @test
 * verify that:
 * - ConfigGenerator::create_accounts() will generate an expected sequence of
 *   SQL requests (CREATE USER IF NOT EXISTS and GRANTs) when some accounts
 *   already exist
 * - SHOW WARNINGS parsing code handles well
 */
TEST_F(ConfigGeneratorTest, create_accounts___show_warnings_parser_1) {
  // SHOW WARNINGS example output
  // +-------+------+---------------------------------------------+
  // | Level | Code | Message                                     |
  // +-------+------+---------------------------------------------+
  // | Note  | 3163 | Authorization ID 'bla'@'h1' already exists. |
  // | Note  | 3163 | Authorization ID 'bla'@'h3' already exists. |
  // +-------+------+---------------------------------------------+

  auto sn = [this](const char *text) -> MySQLSessionReplayer::optional_string {
    return mock_mysql->string_or_null(text);
  };

  // multiple accounts if not exists, warnings with code other than 3163
  // expectation: warnings should be ignored
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER IF NOT EXISTS "
          "'cluster_user'@'host1' IDENTIFIED BY "
          "'secret','cluster_user'@'host2' IDENTIFIED BY "
          "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_ok(0, 2);
  mock_mysql->expect_query("SHOW WARNINGS")
      .then_return(3, {
                          {sn("Note"), sn("123"),
                           sn("Bla bla bla 'cluster_user'@'host1' blablabla.")},
                          {sn("Note"), sn("123"),
                           sn("Bla bla bla 'cluster_user'@'host3' blablabla.")},
                      });
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'")
      .then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                             "secret", /*hash password*/ false,
                             /*if not exists*/ true);
  mock_mysql->execute("END_MARKER");
}

TEST_F(ConfigGeneratorTest, create_accounts___show_warnings_parser_2) {
  auto sn = [this](const char *text) -> MySQLSessionReplayer::optional_string {
    return mock_mysql->string_or_null(text);
  };

  const char *kUserExistsCode = "3163";  // ER_USER_ALREADY_EXISTS

  // multiple accounts if not exists, warnings with message missing proper
  // 'username'@'hostname' expectation: should throw
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER IF NOT EXISTS "
          "'cluster_user'@'host1' IDENTIFIED BY "
          "'secret','cluster_user'@'host2' IDENTIFIED BY "
          "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_ok(0, 2);
  mock_mysql->expect_query("SHOW WARNINGS")
      .then_return(
          3, {
                 // NOTE: there's no single-quotes around username and hostname,
                 // which
                 //       is already enough to make it illegal
                 {sn("Note"), sn(kUserExistsCode),
                  sn("Authorization ID 'cluster_user'@host1 already exists.")},
             });

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", /*hash password*/ false,
                                 /*if not exists*/ true),
      std::runtime_error,
      "SHOW WARNINGS: Failed to extract account name "
      "('cluster_user'@'<anything>') from message \"Authorization ID "
      "'cluster_user'@host1 already exists.\"");
}

TEST_F(ConfigGeneratorTest, create_accounts___show_warnings_parser_3) {
  auto sn = [this](const char *text) -> MySQLSessionReplayer::optional_string {
    return mock_mysql->string_or_null(text);
  };

  const char *kUserExistsCode = "3163";  // ER_USER_ALREADY_EXISTS

  // multiple accounts if not exists, some exist already
  // expectation: GRANTS assigned only to new accounts
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER IF NOT EXISTS "
          "'cluster_user'@'host1' IDENTIFIED BY "
          "'secret','cluster_user'@'host2' IDENTIFIED BY "
          "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_ok(0, 2);
  mock_mysql->expect_query("SHOW WARNINGS")
      .then_return(
          3,
          {
              {sn("Note"), sn(kUserExistsCode),
               sn("Authorization ID 'cluster_user'@'host1' already exists.")},
              {sn("Note"), sn(kUserExistsCode),
               sn("Authorization ID 'cluster_user'@'host3' already exists.")},
          });
  mock_mysql
      ->expect_execute(
          "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_members TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.replication_group_member_stats "
          "TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT SELECT ON performance_schema.global_variables TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.routers TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql
      ->expect_execute(
          "GRANT INSERT, UPDATE, DELETE ON "
          "mysql_innodb_cluster_metadata.v2_routers TO "
          "'cluster_user'@'host2'")
      .then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                             "secret", /*hash password*/ false,
                             /*if not exists*/ true);
  mock_mysql->execute("END_MARKER");
}

TEST_F(ConfigGeneratorTest, create_accounts___show_warnings_parser_4) {
  auto sn = [this](const char *text) -> MySQLSessionReplayer::optional_string {
    return mock_mysql->string_or_null(text);
  };

  const char *kUserExistsCode = "3163";  // ER_USER_ALREADY_EXISTS

  // multiple accounts if not exists, all exist
  // expectation: no GRANTs are assigned
  {
    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql
        ->expect_execute(
            "CREATE USER IF NOT EXISTS "
            "'cluster_user'@'host1' IDENTIFIED BY "
            "'secret','cluster_user'@'host2' IDENTIFIED BY "
            "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
        .then_ok(0, 3);
    mock_mysql->expect_query("SHOW WARNINGS")
        .then_return(
            3,
            {
                {sn("Note"), sn(kUserExistsCode),
                 sn("Authorization ID 'cluster_user'@'host3' already exists.")},
                {sn("Note"), sn(kUserExistsCode),
                 sn("Authorization ID 'cluster_user'@'host1' already exists.")},
                {sn("Note"), sn(kUserExistsCode),
                 sn("Authorization ID 'cluster_user'@'host2' already exists.")},
            });
    mock_mysql->expect_execute("END_MARKER");

    TestConfigGenerator config_gen;
    config_gen.init(kServerUrl, {});
    config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                               "secret", /*hash password*/ false,
                               /*if not exists*/ true);
    mock_mysql->execute("END_MARKER");
  }
}

TEST_F(ConfigGeneratorTest, create_accounts___show_warnings_parser_5) {
  auto sn = [this](const char *text) -> MySQLSessionReplayer::optional_string {
    return mock_mysql->string_or_null(text);
  };

  const char *kUserExistsCode = "3163";  // ER_USER_ALREADY_EXISTS

  // multiple accounts if not exists, SHOW WARNINGS returns some account we
  // didn't want to create expectation: throw
  {
    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql
        ->expect_execute(
            "CREATE USER IF NOT EXISTS "
            "'cluster_user'@'host1' IDENTIFIED BY "
            "'secret','cluster_user'@'host2' IDENTIFIED BY "
            "'secret','cluster_user'@'host3' IDENTIFIED BY 'secret'")
        .then_ok(0, 1);
    mock_mysql->expect_query("SHOW WARNINGS")
        .then_return(
            1,
            {
                {sn("Note"), sn(kUserExistsCode),
                 sn("Authorization ID 'cluster_user'@'foo' already exists.")},
            });

    TestConfigGenerator config_gen;
    config_gen.init(kServerUrl, {});
    EXPECT_THROW_LIKE(
        config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                   "secret", /*hash password*/ false,
                                   /*if not exists*/ true),
        std::runtime_error,
        "SHOW WARNINGS: Unexpected account name 'cluster_user'@'foo' in "
        "message \"Authorization ID 'cluster_user'@'foo' already exists.\"");
  }
}

/**
 * @test
 * verify that:
 * - ConfigGenerator::create_accounts() will generate an expected sequence of
 *   SQL requests (CREATE USER which fails, no GRANTs) when some accounts
 *   already exist
 * - CREATE USER parsing code handles well
 */
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_1) {
  // clang-format off
  // Example error message from failed CREATE USER on existing accounts:
  //   ERROR 1396 (HY000): Operation CREATE USER failed for 'foo'@'host1','foo'@'host2'
  // clang-format on
  const std::string kErrCode = std::to_string(ER_CANNOT_USER);  // 1396
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;

  // sunny day scenario, create one account, it doesn't exist
  // expectation: everything ok
  {
    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql
        ->expect_execute(
            "CREATE USER "
            "'cluster_user'@'host1' IDENTIFIED BY 'secret'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT SELECT ON performance_schema.replication_group_members TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT SELECT ON performance_schema.replication_group_member_stats "
            "TO 'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT SELECT ON performance_schema.global_variables TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT INSERT, UPDATE, DELETE ON "
            "mysql_innodb_cluster_metadata.routers TO 'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT INSERT, UPDATE, DELETE ON "
            "mysql_innodb_cluster_metadata.v2_routers TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT SELECT ON performance_schema.global_variables TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT INSERT, UPDATE, DELETE ON "
            "mysql_innodb_cluster_metadata.routers TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql
        ->expect_execute(
            "GRANT INSERT, UPDATE, DELETE ON "
            "mysql_innodb_cluster_metadata.v2_routers TO "
            "'cluster_user'@'host1'")
        .then_ok();
    mock_mysql->expect_execute("END_MARKER");

    TestConfigGenerator config_gen;
    config_gen.init(kServerUrl, {});
    EXPECT_NO_THROW(config_gen.create_accounts(
        "cluster_user", {"host1"}, "secret", kNoHashPassword, kNoIfNotExists));
  }
}

// single account, it exists
// expectation: throws, message tells which ones (this one) doesn't exist
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_2) {
  const std::string kErrCode = std::to_string(ER_CANNOT_USER);  // 1396
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;
  {
    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql
        ->expect_execute(
            "CREATE USER "
            "'cluster_user'@'host1' IDENTIFIED BY 'secret'")
        .then_error(
            "Operation CREATE USER failed for "
            "'cluster_user'@'host1'",
            ER_CANNOT_USER);
    mock_mysql->expect_execute("ROLLBACK").then_ok();
    mock_mysql->expect_execute("END_MARKER");

    TestConfigGenerator config_gen;
    config_gen.init(kServerUrl, {});
    EXPECT_THROW_LIKE(
        config_gen.create_accounts("cluster_user", {"host1"}, "secret",
                                   kNoHashPassword, kNoIfNotExists),
        std::runtime_error,
        "Account(s) 'cluster_user'@'host1' already exist(s). If this is "
        "expected, please rerun without `--account-create always`.");
  }
}

// multiple accounts, some exist
// expectation: throws, message tells which ones exist already
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_3) {
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;

  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_error(
          "Operation CREATE USER failed for "
          "'cluster_user'@'host1','cluster_user'@'host2'",
          ER_CANNOT_USER);
  mock_mysql->expect_execute("ROLLBACK").then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", kNoHashPassword, kNoIfNotExists),
      std::runtime_error,
      "Account(s) 'cluster_user'@'host1','cluster_user'@'host2' already "
      "exist(s). If this is expected, please rerun without `--account-create "
      "always`.");
}

// multiple accounts, all exist
// expectation: throws, message tells which ones (all) exist already
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_4) {
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;

  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_error(
          "Operation CREATE USER failed for "
          "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'"
          "host3'",
          ER_CANNOT_USER);
  mock_mysql->expect_execute("ROLLBACK").then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", kNoHashPassword, kNoIfNotExists),
      std::runtime_error,
      "Account(s) "
      "'cluster_user'@'host1','cluster_user'@'host2','cluster_user'@'host3' "
      "already exist(s). If this is expected, please rerun without "
      "`--account-create always`.");
}

// multiple accounts, error message contains only unrecognised accounts
// (different username) (this is defensive programming scenario, as something
// like that should
//  never happen - it would be a bug on the Server side)
// expectation: throws, message informs of parsing failure
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_5) {
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;

  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_error(
          "Operation CREATE USER failed for "
          "'differet_user'@'host1','differet_user'@'host2','differet_user'@'"
          "host3'",
          ER_CANNOT_USER);
  mock_mysql->expect_execute("ROLLBACK").then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", kNoHashPassword, kNoIfNotExists),
      std::runtime_error,
      "Failed to parse error message returned by CREATE USER command: "
      "Operation CREATE USER failed for "
      "'differet_user'@'host1','differet_user'@'host2','differet_user'@'"
      "host3'");
}

// multiple accounts, error message contains 1 unrecognised account (different
// username) (this is defensive programming scenario, as something like that
// should
//  never happen - it would be a bug on the Server side)
// expectation: throws, unrecognised account will be ignored
//              (unfortunately, it would require complicating implementation
//              logic to correctly detect this situation.  Given that this is
//              just defensive programming handling a hypothetical Server
//              bug, it's not worth it)
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_6) {
  {
    constexpr bool kNoHashPassword = false;
    constexpr bool kNoIfNotExists = false;
    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql
        ->expect_execute(
            "CREATE USER "
            "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
            "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
            "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
        .then_error(
            "Operation CREATE USER failed for "
            "'cluster_user'@'host1','different_user'@'host2'",
            ER_CANNOT_USER);
    mock_mysql->expect_execute("ROLLBACK").then_ok();
    mock_mysql->expect_execute("END_MARKER");
    TestConfigGenerator config_gen;
    config_gen.init(kServerUrl, {});
    EXPECT_THROW_LIKE(
        config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                   "secret", kNoHashPassword, kNoIfNotExists),
        std::runtime_error,
        "Account(s) 'cluster_user'@'host1' already exist(s). If this is "
        "expected, please rerun without `--account-create always`.");
  }
}

// multiple accounts, error message doesn't contain any accounts
// (this is defensive programming scenario, as something like that should
//  never happen - it would be a bug on the Server side)
// expectation: throws, message informs of parsing failure
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_7) {
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_error("Operation CREATE USER failed for ",  // no accounts given
                  ER_CANNOT_USER);
  mock_mysql->expect_execute("ROLLBACK").then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", kNoHashPassword, kNoIfNotExists),
      std::runtime_error,
      "Failed to parse error message returned by CREATE USER command: "
      "Operation CREATE USER failed for");
}

// multiple accounts, error message is not code ER_CANNOT_USER
// expectation: no special handling, act like for any general failure (throw
//              with error message received from the Server)
TEST_F(ConfigGeneratorTest, create_accounts___users_exist_parser_8) {
  constexpr bool kNoHashPassword = false;
  constexpr bool kNoIfNotExists = false;
  ::testing::InSequence s;
  common_pass_metadata_checks(mock_mysql.get());
  mock_mysql
      ->expect_execute(
          "CREATE USER "
          "'cluster_user'@'host1' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host2' IDENTIFIED BY 'secret',"
          "'cluster_user'@'host3' IDENTIFIED BY 'secret'")
      .then_error(
          "Operation CREATE USER failed for "
          "'cluster_user'@'host1','cluster_user'@'host2'",
          42);  // not ER_CANNOT_USER nor something failover-friendly
  mock_mysql->expect_execute("ROLLBACK").then_ok();
  mock_mysql->expect_execute("END_MARKER");

  TestConfigGenerator config_gen;
  config_gen.init(kServerUrl, {});
  EXPECT_THROW_LIKE(
      config_gen.create_accounts("cluster_user", {"host1", "host2", "host3"},
                                 "secret", kNoHashPassword, kNoIfNotExists),
      std::runtime_error,
      "Error creating MySQL account for router (CREATE USER stage): "
      "Operation CREATE USER failed for "
      "'cluster_user'@'host1','cluster_user'@'host2'");
}

TEST_F(ConfigGeneratorTest, create_router_accounts) {
  enum TestType {
    NATIVE,   // CREATE USER using mysql_native_password and hashed password
    FALLBACK  // CREATE USER using fallback method with plaintext password
  };

  for (TestType tt : {NATIVE, FALLBACK}) {
    constexpr unsigned kDontFail = 99;

    auto account_native = [](const std::string &host) {
      return "'cluster_user'@'" + host + "'" +
             " IDENTIFIED WITH mysql_native_password"
             " AS '*BDF9890F9606F18B2E92EF0CA972006F1DBC44DF'";
    };
    auto account_fallback = [](const std::string &host) {
      return "'cluster_user'@'" + host + "'" +
             " IDENTIFIED BY '0123456789012345'";
    };
    auto account = [](const std::string &host) {
      return "'cluster_user'@'" + host + "'";
    };
    auto make_list =
        [](const std::set<std::string> &items,
           std::function<std::string(const std::string &)> generator) {
          if (items.empty())
            throw std::logic_error(
                "make_list() called with an empty set of items");

          std::string res;
          for (const std::string &i : items) res += generator(i) + ",";
          res.resize(res.size() - 1);  // trim last ,
          return res;
        };

    auto generate_expected_SQL = [&](const std::set<std::string> &hosts,
                                     unsigned fail_on) {
      // kDontFail => don't fail, 1..4 => fail on 1..4
      assert((1 <= fail_on && fail_on <= 4) || fail_on == kDontFail);

      if (tt == NATIVE) {
        // CREATE USER using mysql_native_password and hashed password
        if (fail_on > 0)
          mock_mysql
              ->expect_execute("CREATE USER IF NOT EXISTS " +
                               make_list(hosts, account_native))
              .then_ok();
      } else {
        // fail mysql_native_password method to induce fallback to plaintext
        // method.
        mock_mysql
            ->expect_execute("CREATE USER IF NOT EXISTS " +
                             make_list(hosts, account_native))
            .then_error("no such plugin", 1524);

        // CREATE USER using fallback method with plaintext password
        if (fail_on > 0)
          mock_mysql
              ->expect_execute("CREATE USER IF NOT EXISTS " +
                               make_list(hosts, account_fallback))
              .then_ok();
      }
      if (fail_on > 1)
        mock_mysql
            ->expect_execute(
                "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO " +
                make_list(hosts, account))
            .then_ok();
      if (fail_on > 2)
        mock_mysql
            ->expect_execute(
                "GRANT SELECT ON performance_schema.replication_group_members "
                "TO " +
                make_list(hosts, account))
            .then_ok();
      if (fail_on > 3) {
        mock_mysql
            ->expect_execute(
                "GRANT SELECT ON "
                "performance_schema.replication_group_member_stats TO " +
                make_list(hosts, account))
            .then_ok();

        mock_mysql
            ->expect_execute(
                "GRANT SELECT ON performance_schema.global_variables TO " +
                make_list(hosts, account))
            .then_ok();

        mock_mysql
            ->expect_execute(
                "GRANT INSERT, UPDATE, DELETE ON "
                "mysql_innodb_cluster_metadata.routers TO " +
                make_list(hosts, account))
            .then_ok();

        mock_mysql
            ->expect_execute(
                "GRANT INSERT, UPDATE, DELETE ON "
                "mysql_innodb_cluster_metadata.v2_routers TO " +
                make_list(hosts, account))
            .then_ok();
      }

      if (fail_on != kDontFail)
        mock_mysql->then_error("some error",
                               1234);  // i-th statement will return this error
    };

    auto h = [](const std::vector<std::string> &hostnames) {
      return TestConfigGenerator::get_account_host_args(
          {{"account-host", hostnames}});
    };

    // default hostname
    {
      ::testing::InSequence s;
      common_pass_metadata_checks(mock_mysql.get());
      generate_expected_SQL({"%"}, kDontFail);

      TestConfigGenerator config_gen;
      std::string password;
      config_gen.init(kServerUrl, {});
      config_gen.create_router_accounts({}, h({}), "cluster_user", password,
                                        true);
    }

    // 1 hostname
    {
      ::testing::InSequence s;
      common_pass_metadata_checks(mock_mysql.get());
      generate_expected_SQL({"host1"}, kDontFail);

      TestConfigGenerator config_gen;
      std::string password;
      config_gen.init(kServerUrl, {});
      config_gen.create_router_accounts({}, h({"host1"}), "cluster_user",
                                        password, true);
    }

    // many hostnames
    {
      // NOTE:
      // When we run bootstrap in real life, all --account-host entries should
      // get sorted and any non-unique entries eliminated (to ensure CREATE
      // USER does not get called twice for the same user@host).  However, this
      // happens at the commandline parsing level, so by the time
      // ConfigGenerator runs, the list of hostnames is already unique and
      // sorted. Here we just give an arbitrary list to ensure it will work
      // irrespective of input.

      ::testing::InSequence s;
      common_pass_metadata_checks(mock_mysql.get());

      // note: hostnames will be processed in sorted order (this is not a
      //       functional requirement, just how our code works)
      generate_expected_SQL({"%", "host1", "host3%"}, kDontFail);

      TestConfigGenerator config_gen;
      std::string password;
      config_gen.init(kServerUrl, {});
      config_gen.create_router_accounts({}, h({"host1", "%", "host3%"}),
                                        "cluster_user", password, true);
    }

    // one of user-creating statements fails
    for (unsigned fail_sql = 1; fail_sql <= 4; fail_sql++) {
      ::testing::InSequence s;
      common_pass_metadata_checks(mock_mysql.get());
      generate_expected_SQL({"host1", "host2", "host3"}, fail_sql);

      // fail_sql-th SQL statement of fail_host will return this error
      mock_mysql->then_error("some error", 1234);

      mock_mysql->expect_execute("ROLLBACK");

      TestConfigGenerator config_gen;
      std::string password;
      config_gen.init(kServerUrl, {});
      EXPECT_THROW_LIKE(
          config_gen.create_router_accounts({}, h({"host1", "host2", "host3"}),
                                            "cluster_user", password, true),
          std::runtime_error,
          (fail_sql == 1 ? "Error creating MySQL account for router (CREATE "
                           "USER stage): some error"
                         : "Error creating MySQL account for router (GRANTs "
                           "stage): some error"));

      EXPECT_TRUE(mock_mysql->empty());

    }  // for (unsigned fail_sql = 1; fail_sql <= 4; fail_sql++)
  }    // for (TestType tt : {NATIVE, FALLBACK})
}

class CreateConfigGeneratorTest : public ConfigGeneratorTest {
 public:
  void SetUp() override {
    ConfigGeneratorTest::SetUp();

    cluster_info = {{"server1", "server2", "server3"},
                    "91d0107263cd11ec9a29080027ac264d",
                    "gr_id",
                    "my-cluster"};

    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
  }

 protected:
  std::map<std::string, std::string> user_options;
  TestConfigGenerator config_gen;
  mysqlrouter::ClusterInfo cluster_info;
  std::stringstream conf_output, state_output;

  const mysql_harness::Path tmp_path =
      mysql_harness::Path{test_dir.name()}.real_path();

  const std::vector<std::string> rest_config{
      "[http_server]",
      "port=8443",
      "ssl=1",
      "ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "ssl_key=" + tmp_path.join("router-key.pem").str(),
      "",
      "[http_auth_realm:default_auth_realm]",
      "backend=default_auth_backend",
      "method=basic",
      "name=default_realm",
      "",
      "[rest_router]",
      "require_realm=default_auth_realm",
      "",
      "[rest_api]",
      "",
      "[http_auth_backend:default_auth_backend]",
      "backend=metadata_cache",
      "",
      "[rest_routing]",
      "require_realm=default_auth_realm",
      "",
      "[rest_metadata_cache]",
      "require_realm=default_auth_realm",
      "",
  };
};

TEST_F(CreateConfigGeneratorTest, create_config_basic) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "myrouter",
                           "mysqlrouter", cluster_info, "cluster_user", options,
                           default_paths, {}, "state_file_name.json");

  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "name=myrouter",
      "user=mysqlrouter",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6446",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6447",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6448",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6449",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));

  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_system_instance) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  config_gen.create_config(conf_output, state_output, 123, "", "", cluster_info,
                           "cluster_user", options, default_paths, {},
                           "state_file_name.json");

  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6446",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6447",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6448",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6449",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));

  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_base_port) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  auto opts = user_options;
  opts["base-port"] = "1234";
  options = config_gen.fill_options(opts, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "", "", cluster_info,
                           "cluster_user", options, default_paths, {},
                           "state_file_name.json");
  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=0.0.0.0",
      "bind_port=1234",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=0.0.0.0",
      "bind_port=1235",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=0.0.0.0",
      "bind_port=1236",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=0.0.0.0",
      "bind_port=1237",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));

  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_skip_tcp) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  auto opts = user_options;
  opts["base-port"] = "123";
  opts["use-sockets"] = "1";
  opts["skip-tcp"] = "1";
  opts["socketsdir"] = test_dir.name();
  options = config_gen.fill_options(opts, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "", "", cluster_info,
                           "cluster_user", options, default_paths, {},
                           "state_file_name.json");

  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",

      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "socket=" + test_dir.name() + "/mysql.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "socket=" + test_dir.name() + "/mysqlro.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "socket=" + test_dir.name() + "/mysqlx.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "socket=" + test_dir.name() + "/mysqlxro.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));
  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_use_sockets) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  auto opts = user_options;
  opts["use-sockets"] = "1";
  opts["socketsdir"] = test_dir.name();
  options = config_gen.fill_options(opts, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "", "", cluster_info,
                           "cluster_user", options, default_paths, {},
                           "state_file_name.json");
  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6446",
      "socket=" + test_dir.name() + "/mysql.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6447",
      "socket=" + test_dir.name() + "/mysqlro.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6448",
      "socket=" + test_dir.name() + "/mysqlx.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6449",
      "socket=" + test_dir.name() + "/mysqlxro.sock",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));

  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_bind_address) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  auto opts = user_options;
  opts["bind-address"] = "127.0.0.1";
  options = config_gen.fill_options(opts, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "myrouter",
                           "mysqlrouter", cluster_info, "cluster_user", options,
                           default_paths, {}, "state_file_name.json");

  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "name=myrouter",
      "user=mysqlrouter",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=127.0.0.1",
      "bind_port=6446",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=127.0.0.1",
      "bind_port=6447",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=127.0.0.1",
      "bind_port=6448",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=127.0.0.1",
      "bind_port=6449",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // append the rest-config
  for (const auto &line : rest_config) {
    expected_config_lines.push_back(line);
  }

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));
  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(CreateConfigGeneratorTest, create_config_disable_rest) {
  ConfigGenerator::Options options =
      config_gen.fill_options(user_options, default_paths, {});
  auto opts = user_options;
  opts["disable-rest"] = "";
  options = config_gen.fill_options(opts, default_paths, {});

  config_gen.create_config(conf_output, state_output, 123, "myrouter",
                           "mysqlrouter", cluster_info, "cluster_user", options,
                           default_paths, {}, "state_file_name.json");

  std::vector<std::string> lines;
  for (std::string line; std::getline(conf_output, line);) {
    lines.push_back(line);
  }

  std::vector<std::string> expected_config_lines = {
      "# File automatically generated during MySQL Router bootstrap",
      "[DEFAULT]",
      "name=myrouter",
      "user=mysqlrouter",
      "connect_timeout=5",
      "read_timeout=30",
      "dynamic_state=state_file_name.json",
      "client_ssl_cert=" + tmp_path.join("router-cert.pem").str(),
      "client_ssl_key=" + tmp_path.join("router-key.pem").str(),
      "client_ssl_mode=PREFERRED",
      "server_ssl_mode=AS_CLIENT",
      "server_ssl_verify=DISABLED",
      "unknown_config_option=error",
      "",
      "[logger]",
      "level=INFO",
      "",
      "[metadata_cache:bootstrap]",
      "cluster_type=gr",
      "router_id=123",
      "user=cluster_user",
      "metadata_cluster=my-cluster",
      "ttl=0.5",
      "auth_cache_ttl=-1",
      "auth_cache_refresh_interval=2",
      "use_gr_notifications=0",
      "",
      "[routing:bootstrap_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6446",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=classic",
      "",
      "[routing:bootstrap_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6447",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=classic",
      "",
      "[routing:bootstrap_x_rw]",
      "bind_address=0.0.0.0",
      "bind_port=6448",
      "destinations=metadata-cache://my-cluster/"
      "?role=PRIMARY",
      "routing_strategy=first-available",
      "protocol=x",
      "",
      "[routing:bootstrap_x_ro]",
      "bind_address=0.0.0.0",
      "bind_port=6449",
      "destinations=metadata-cache://my-cluster/"
      "?role=SECONDARY",
      "routing_strategy=round-robin-with-fallback",
      "protocol=x",
      ""};

  // don't append the rest-config

  ASSERT_THAT(lines, ::testing::ElementsAreArray(expected_config_lines));

  ASSERT_THAT(state_output.str(),
              Eq("{\n"
                 "    \"metadata-cache\": {\n"
                 "        \"group-replication-id\": \"replication-1\",\n"
                 "        \"cluster-metadata-servers\": [\n"
                 "            \"server1\",\n"
                 "            \"server2\",\n"
                 "            \"server3\"\n"
                 "        ]\n"
                 "    },\n"
                 "    \"version\": \"1.0.0\"\n"
                 "}"));
}

TEST_F(ConfigGeneratorTest, fill_options) {
  TestConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql.get());
  config_gen.init(kServerUrl, {});

  ConfigGenerator::Options options;
  {
    std::map<std::string, std::string> user_options;
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["bind-address"] = "127.0.0.1";
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq("127.0.0.1"));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["base-port"] = "1234";
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(1234));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(1235));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["base-port"] = "1";
    options = config_gen.fill_options(user_options, default_paths, {});
    EXPECT_THAT(options.rw_endpoint.port, Eq(1));
    user_options["base-port"] = "3306";
    options = config_gen.fill_options(user_options, default_paths, {});
    EXPECT_THAT(options.rw_endpoint.port, Eq(3306));
    user_options["base-port"] = "";
    EXPECT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument);
    user_options["base-port"] = "-1";
    EXPECT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument);
    user_options["base-port"] = "999999";
    EXPECT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument);
    user_options["base-port"] = "65536";
    EXPECT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument);
    user_options["base-port"] = "2000bozo";
    EXPECT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument);

    // Bug #24808309
    user_options["base-port"] = "65533";
    EXPECT_THROW_LIKE(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::invalid_argument,
        "base-port needs value between 0 and 65532 inclusive, was '65533'");

    user_options["base-port"] = "65532";
    EXPECT_NO_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}));

    EXPECT_THAT(options.rw_endpoint, Eq(true));
    EXPECT_THAT(options.rw_endpoint.port, Eq(65532));
    EXPECT_THAT(options.rw_endpoint.socket, Eq(""));
    EXPECT_THAT(options.ro_endpoint, Eq(true));
    EXPECT_THAT(options.ro_endpoint.port, Eq(65533));
    EXPECT_THAT(options.ro_endpoint.socket, Eq(""));
    EXPECT_THAT(options.rw_x_endpoint, Eq(true));
    EXPECT_THAT(options.ro_x_endpoint, Eq(true));
    EXPECT_THAT(options.rw_x_endpoint.port, Eq(65534));
    EXPECT_THAT(options.rw_x_endpoint.socket, Eq(""));
    EXPECT_THAT(options.ro_x_endpoint, Eq(true));
    EXPECT_THAT(options.ro_x_endpoint.port, Eq(65535));
    EXPECT_THAT(options.ro_x_endpoint.socket, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["bind-address"] = "invalid..";
    ASSERT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::runtime_error);
    user_options["bind-address"] = "";
    ASSERT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::runtime_error);
    user_options["bind-address"] = "1.2.3.4..5";
    ASSERT_THROW(
        options = config_gen.fill_options(user_options, default_paths, {}),
        std::runtime_error);
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["use-sockets"] = "1";
    user_options["skip-tcp"] = "1";
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(0));
    ASSERT_THAT(options.rw_endpoint.socket, Eq("mysql.sock"));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(0));
    ASSERT_THAT(options.ro_endpoint.socket, Eq("mysqlro.sock"));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["skip-tcp"] = "1";
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(false));
    ASSERT_THAT(options.rw_endpoint.port, Eq(0));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(false));
    ASSERT_THAT(options.ro_endpoint.port, Eq(0));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(false));
    ASSERT_THAT(options.ro_x_endpoint, Eq(false));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["use-sockets"] = "1";
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq("mysql.sock"));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(6447));
    ASSERT_THAT(options.ro_endpoint.socket, Eq("mysqlro.sock"));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    options = config_gen.fill_options(user_options, default_paths, {});
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(6447));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
}

// XXX TODO: add recursive directory delete function

namespace {
enum action_t { ACTION_EXECUTE, ACTION_QUERY, ACTION_QUERY_ONE, ACTION_ERROR };

struct query_entry_t {
  const char *query;
  action_t action;
  unsigned result_cols;
  std::vector<std::vector<MySQLSessionReplayer::optional_string>> results;
  uint64_t last_insert_id;
  unsigned error_code;

  query_entry_t(const char *query_, action_t action_,
                uint64_t last_insert_id_ = 0, unsigned error_code_ = 0)
      : query(query_),
        action(action_),
        result_cols(0),
        last_insert_id(last_insert_id_),
        error_code(error_code_) {}
  query_entry_t(
      const char *query_, action_t action_, unsigned results_cols_,
      const std::vector<std::vector<MySQLSessionReplayer::optional_string>>
          &results_,
      uint64_t last_insert_id_ = 0, unsigned error_code_ = 0)
      : query(query_),
        action(action_),
        result_cols(results_cols_),
        results(results_),
        last_insert_id(last_insert_id_),
        error_code(error_code_) {}
};

std::vector<query_entry_t> expected_bootstrap_queries = {
    {"select @@group_replication_group_name", ACTION_QUERY_ONE, 1, {{"gr-id"}}},
    {"START TRANSACTION", ACTION_EXECUTE},
    {"INSERT INTO mysql_innodb_cluster_metadata.v2_routers", ACTION_EXECUTE, 4},
    {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO "
     "'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT SELECT ON performance_schema.replication_group_members TO "
     "'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT SELECT ON performance_schema.replication_group_member_stats TO "
     "'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT SELECT ON performance_schema.global_variables TO "
     "'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT INSERT, UPDATE, DELETE ON mysql_innodb_cluster_metadata.routers "
     "TO 'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},
    {"GRANT INSERT, UPDATE, DELETE ON mysql_innodb_cluster_metadata.v2_routers "
     "TO 'mysql_router4_012345678901'@'%'",
     ACTION_EXECUTE},

    {"UPDATE mysql_innodb_cluster_metadata.v2_routers SET attributes = ",
     ACTION_EXECUTE},
    {"COMMIT", ACTION_EXECUTE},
};

static void expect_bootstrap_queries(
    MySQLSessionReplayer *m, const char *cluster_name,
    const std::vector<query_entry_t> &expected_queries =
        expected_bootstrap_queries) {
  m->expect_query("").then_return(
      3, {{"91d0107263cd11ec9a29080027ac264d", cluster_name, "somehost:3306"}});
  for (const auto &query : expected_queries) {
    switch (query.action) {
      case ACTION_EXECUTE:
        m->expect_execute(query.query).then_ok(query.last_insert_id);
        break;
      case ACTION_QUERY:
        m->expect_query(query.query)
            .then_return(query.result_cols, query.results);
        break;
      case ACTION_QUERY_ONE:
        m->expect_query_one(query.query)
            .then_return(query.result_cols, query.results);
        break;
      default: /*ACTION_ERROR*/
        m->expect_execute(query.query).then_error("ERROR:", query.error_code);
    }
  }
}

static void bootstrap_name_test(
    MySQLSessionReplayer *mock_mysql, const std::string &program_name,
    const std::string &dir, const std::string &name, bool expect_fail,
    const std::map<std::string, std::string> &default_paths) {
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(kServerUrl, {});
  if (!expect_fail) expect_bootstrap_queries(mock_mysql, "my-cluster");

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  options["id"] = "4";
  options["report-host"] = "dont.query.dns";

  KeyringInfo keyring_info("delme", "delme.key");
  config_gen.set_keyring_info(keyring_info);

  config_gen.bootstrap_directory_deployment(program_name, dir, options, {},
                                            default_paths);
}

}  // anonymous namespace

TEST_F(ConfigGeneratorTest, bootstrap_invalid_name) {
  TempDirectory test_dir;
  const std::string dir = test_dir.name() + "/bug24807941";

  // Bug#24807941
  ASSERT_NO_THROW(bootstrap_name_test(mock_mysql.get(), program_name_, dir,
                                      "myname", false, default_paths));
  delete_dir_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(mock_mysql.get(), program_name_, dir,
                                      "myname", false, default_paths));
  delete_dir_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(mock_mysql.get(), program_name_, dir, "",
                                      false, default_paths));
  delete_dir_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(bootstrap_name_test(mock_mysql.get(), program_name_, dir,
                                        "system", true, default_paths),
                    std::runtime_error, "Router name 'system' is reserved");
  delete_dir_recursive(dir);
  mysql_harness::reset_keyring();

  std::vector<std::string> bad_names{
      "new\nline",
      "car\rreturn",
  };
  for (std::string &name : bad_names) {
    ASSERT_THROW_LIKE(
        bootstrap_name_test(mock_mysql.get(), program_name_, dir, name, true,
                            default_paths),
        std::runtime_error,
        "Router name '" + name + "' contains invalid characters.");
    delete_dir_recursive(dir);
    mysql_harness::reset_keyring();
  }

  ASSERT_THROW_LIKE(
      bootstrap_name_test(
          mock_mysql.get(), program_name_, dir,
          "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
          "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
          "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
          "veryveryveryveryveryveryveryveryveryveryverylongname",
          true, default_paths),
      std::runtime_error, "too long (max 255).");
  delete_dir_recursive(dir);
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, bootstrap_cleanup_on_failure) {
  TempDirectory test_dir;
  const std::string dir = test_dir.name() + "/bug24808634";

  std::map<std::string, std::string> options;
  options["name"] = "foobar";
  options["quiet"] = "1";
  options["report-host"] = "dont.query.dns";

  // cleanup on failure when dir didn't exist before
  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    mock_mysql->expect_query("select c.cluster_id")
        .then_return(3, {{"91d0107263cd11ec9a29080027ac264d", "my-cluster",
                          "somehost:3306"}});
    common_pass_group_name(mock_mysql.get());
    mock_mysql->expect_execute("START TRANSACTION").then_error("boo!", 1234);

    KeyringInfo keyring_info("delme", "delme.key");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_THROW_LIKE(config_gen.bootstrap_directory_deployment(
                          program_name_, dir, options, {}, default_paths),
                      mysqlrouter::MySQLSession::Error, "boo!");

    ASSERT_FALSE(mysql_harness::Path(dir).exists());
    ASSERT_FALSE(mysql_harness::Path(test_dir.name() + "/delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // this should succeed, so that we can test that cleanup doesn't delete
  // existing stuff
  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    expect_bootstrap_queries(mock_mysql.get(), "my-cluster");

    KeyringInfo keyring_info("delme", "delme.key");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_NO_THROW(config_gen.bootstrap_directory_deployment(
        program_name_, dir, options, {}, default_paths));

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure if dir already existed before
  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    mock_mysql->expect_query("").then_return(
        3,
        {{"91d0107263cd11ec9a29080027ac264d", "my-cluster", "somehost:3306"}});
    common_pass_group_name(mock_mysql.get());
    // force a failure during account creation
    mock_mysql->expect_execute("").then_error("boo!", 1234);

    KeyringInfo keyring_info("delme", "delme.key");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_THROW_LIKE(config_gen.bootstrap_directory_deployment(
                          program_name_, dir, options, {}, default_paths),
                      std::runtime_error, "boo!");

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure in early validation if dir already existed before
  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    mock_mysql->expect_query("").then_return(
        3, {{"91d0107263cd11ec9a29080027ac264d", "mycluter", "somehost:3306"}});

    std::map<std::string, std::string> options2 = options;
    options2["name"] = "force\nfailure";

    KeyringInfo keyring_info("delme", "delme.key");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_THROW(config_gen.bootstrap_directory_deployment(
                     program_name_, dir, options2, {}, default_paths),
                 std::runtime_error);
    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, bug25391460) {
  TempDirectory test_dir;
  const std::string dir = test_dir.name() + "/bug25391460";

  // Bug#24807941
  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    expect_bootstrap_queries(mock_mysql.get(), "my-cluster");
    config_gen.init(kServerUrl, {});
    mock_mysql->expect_query("").then_return(
        3,
        {{"91d0107263cd11ec9a29080027ac264d", "my-cluster", "somehost:3306"}});

    std::map<std::string, std::string> options;
    options["quiet"] = "1";
    options["use-sockets"] = "1";
    options["report-host"] = "dont.query.dns";

    KeyringInfo keyring_info("delme", "delme.key");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_NO_THROW(config_gen.bootstrap_directory_deployment(
        program_name_, dir, options, {}, default_paths));
    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }

  // now read the config file and check that all socket paths are
  // .../bug25391460/mysql*.sock instead of
  // .../bug25391460/socketsdir/mysql*.sock
  std::ifstream cf;
  std::string basedir = mysql_harness::Path(dir).real_path().str();
  cf.open(mysql_harness::Path(dir).join("mysqlrouter.conf").str());
  while (!cf.eof()) {
    std::string line;
    cf >> line;
    if (line.compare(0, 7, "socket=") == 0) {
      line = line.substr(7);
      // check prefix/basedir
      EXPECT_EQ(basedir, line.substr(0, basedir.length()));
      std::string suffix = line.substr(basedir.length() + 1);
      // check filename extension
      EXPECT_EQ(".sock", suffix.substr(suffix.length() - strlen(".sock")));
      std::string::size_type end = suffix.rfind('/');
      if (end == std::string::npos) end = suffix.rfind('\\');
      // check that the file is directly under the deployment directory
      EXPECT_EQ(suffix.substr(end + 1), suffix);
    }
  }

  mysql_harness::reset_keyring();
  delete_dir_recursive(dir);
}

static void bootstrap_overwrite_test(
    MySQLSessionReplayer *mock_mysql, const std::string &program_name,
    const std::string &dir, const std::string &name, bool force,
    const char *cluster_name, bool expect_fail,
    const std::map<std::string, std::string> &default_paths) {
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(kServerUrl, {});
  if (!expect_fail)
    expect_bootstrap_queries(mock_mysql, cluster_name);
  else
    mock_mysql->expect_query("").then_return(
        3,
        {{"91d0107263cd11ec9a29080027ac264d", cluster_name, "somehost:3306"}});

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  options["report-host"] = "dont.query.dns";
  if (force) options["force"] = "1";

  KeyringInfo keyring_info("delme", "delme.key");
  config_gen.set_keyring_info(keyring_info);

  config_gen.bootstrap_directory_deployment(program_name, dir, options, {},
                                            default_paths);
}

TEST_F(ConfigGeneratorTest, bootstrap_overwrite) {
  TempDirectory test_dir;
  std::string dir = test_dir.name() + "/configtest";

  mysql_harness::reset_keyring();

  // Overwrite tests. Run bootstrap twice on the same output directory
  //
  // Name    --force     cluster_name   Expected
  // -------------------------------------------
  // same    no          same           OK (refreshing config)
  // same    no          diff           FAIL
  // same    yes         same           OK
  // same    yes         diff           OK (replacing config)
  // diff    no          same           OK
  // diff    no          diff           FAIL
  // diff    yes         same           OK
  // diff    yes         diff           OK
  //
  // diff name is just a rename, so no issue

  SCOPED_TRACE("bootstrap_overwrite1");
  // same    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  SCOPED_TRACE("bootstrap_overwrite2");
  dir = test_dir.name() + "/configtest2";
  // same    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(
      bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir, "myname",
                               false, "kluster", true, default_paths),
      std::runtime_error,
      "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest3";
  SCOPED_TRACE("bootstrap_overwrite3");
  // same    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", true, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", true, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest4";
  SCOPED_TRACE("bootstrap_overwrite4");
  // same    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", true, "kluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest5";
  SCOPED_TRACE("bootstrap_overwrite5");
  // diff    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "xmyname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest6";
  SCOPED_TRACE("bootstrap_overwrite6");
  // diff    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(
      bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir, "xmyname",
                               false, "kluster", true, default_paths),
      std::runtime_error,
      "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest7";
  SCOPED_TRACE("bootstrap_overwrite7");
  // diff    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", true, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "xmyname", true, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_NO_ERROR(delete_dir_recursive(dir));

  dir = test_dir.name() + "/configtest8";
  SCOPED_TRACE("bootstrap_overwrite8");
  // diff    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "myname", false, "cluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(mock_mysql.get(), program_name_, dir,
                                           "xmyname", true, "kluster", false,
                                           default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
}

static void test_key_length(
    MySQLSessionReplayer *mock_mysql, const std::string &program_name,
    const std::string &key,
    const std::map<std::string, std::string> &default_paths,
    const std::string &directory) {
  ::testing::InSequence s;

  mysqlrouter::set_prompt_password(
      [key](const std::string &) -> std::string { return key; });
  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(kServerUrl, {});
  expect_bootstrap_queries(mock_mysql, "my-cluster");

  std::map<std::string, std::string> options;
  options["name"] = "test";
  options["quiet"] = "1";
  options["report-host"] = "dont.query.dns";

  KeyringInfo keyring_info("delme", "");
  config_gen.set_keyring_info(keyring_info);

  config_gen.bootstrap_directory_deployment(program_name, directory, options,
                                            {}, default_paths);
}

TEST_F(ConfigGeneratorTest, key_too_long) {
  TempDirectory test_dir;
  std::string bs_dir = test_dir.name() + "/key_too_long";

  // bug #24942008, keyring key too long
  ASSERT_NO_THROW(test_key_length(mock_mysql.get(), program_name_,
                                  std::string(250, 'x'), default_paths,
                                  bs_dir));
  delete_dir_recursive(bs_dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(test_key_length(mock_mysql.get(), program_name_,
                                  std::string(255, 'x'), default_paths,
                                  bs_dir));
  delete_dir_recursive(bs_dir);
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(
      test_key_length(mock_mysql.get(), program_name_, std::string(256, 'x'),
                      default_paths, bs_dir),
      std::runtime_error, "too long");
  delete_dir_recursive(bs_dir);
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(
      test_key_length(mock_mysql.get(), program_name_, std::string(5000, 'x'),
                      default_paths, bs_dir),
      std::runtime_error, "too long");
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, bad_master_key) {
  // bug #24955928
  TempDirectory test_dir;

  std::map<std::string, std::string> options;
  options["name"] = "foo";
  options["quiet"] = "1";
  options["report-host"] = "dont.query.dns";

  // reconfiguring with an empty master key file throws an error referencing
  // the temporary file name instead of the actual name
  {
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    expect_bootstrap_queries(mock_mysql.get(), "my-cluster");

    KeyringInfo keyring_info("delme", "key");
    config_gen.set_keyring_info(keyring_info);

    config_gen.bootstrap_directory_deployment(program_name_, test_dir.name(),
                                              options, {}, default_paths);

    mysql_harness::reset_keyring();
  }
  const std::string empty_file_name = test_dir.name() + "/emptyfile";
  {
    std::ofstream f(empty_file_name);
    mysql_harness::make_file_private(empty_file_name);
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    expect_bootstrap_queries(mock_mysql.get(), "my-cluster");

    KeyringInfo keyring_info(test_dir.name(), "emptyfile");
    config_gen.set_keyring_info(keyring_info);

    try {
      config_gen.bootstrap_directory_deployment(program_name_, test_dir.name(),
                                                options, {}, default_paths);
      FAIL() << "Was expecting exception but got none\n";
    } catch (const std::runtime_error &e) {
      ASSERT_THAT(e.what(), ::testing::Not(::testing::HasSubstr(".tmp")));
      ASSERT_THAT(e.what(), ::testing::HasSubstr("Invalid master key file "));
    }
  }
  delete_dir_recursive(test_dir.name());
  mysql_harness::reset_keyring();
  // directory name but no filename
  {
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql.get());
    config_gen.init(kServerUrl, {});
    expect_bootstrap_queries(mock_mysql.get(), "my-cluster");

    KeyringInfo keyring_info(test_dir.name(), ".");
    config_gen.set_keyring_info(keyring_info);

    ASSERT_THROW_LIKE(
        config_gen.bootstrap_directory_deployment(
            program_name_, test_dir.name(), options, {}, default_paths),
        std::runtime_error, "Invalid master key file");
  }
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, full_test) {
  TempDirectory test_dir;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql.get());
  config_gen.init(kServerUrl, {});
  expect_bootstrap_queries(mock_mysql.get(), "my-cluster");

  std::map<std::string, std::string> options;
  options["name"] = "foo";
  options["quiet"] = "1";
  options["report-host"] = "dont.query.dns";

  KeyringInfo keyring_info("delme", "masterkey");
  config_gen.set_keyring_info(keyring_info);

  ASSERT_NO_THROW(config_gen.bootstrap_directory_deployment(
      program_name_, test_dir.name(), options, {}, default_paths));

  std::string value;
  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  config.read(test_dir.name() + "/mysqlrouter.conf");

  value = config.get_default("master_key_path");
  EXPECT_TRUE(
      mysql_harness::utility::ends_with(value, test_dir.name() + "/masterkey"));

  value = config.get_default("name");
  EXPECT_EQ(value, "foo");

  value = config.get_default("keyring_path");
  EXPECT_EQ(mysql_harness::Path(value).basename(), "delme");

  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, empty_config_file) {
  TestConfigGenerator config_gen;
  TempDirectory test_dir;
  const std::string conf_path(test_dir.name() + "/mysqlrouter.conf");

  std::ofstream file(conf_path, std::ofstream::out | std::ofstream::trunc);
  file.close();

  TestConfigGenerator::ExistingConfigOptions conf_options;
  EXPECT_NO_THROW(conf_options =
                      config_gen.get_options_from_config_if_it_exists(
                          conf_path, "dummy", false));
  EXPECT_EQ(conf_options.router_id, uint32_t(0));

  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse) {
  // These tests verify that SSL options are handled correctly at argument
  // parsing stage during bootstrap. Note that at this stage, we only care about
  // arguments being passed further down, and rely on mysql_*() calls to deal
  // with eventual inconsistencies. The only exception to this rule is parsing
  // --ssl-mode, which is a string that has to be converted to an SSL_MODE_*
  // enum (though arguably that validation could also be delayed).

  // --ssl-mode not given
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ(0u, router.bootstrap_options_.count("ssl_mode"));
  }

  // --ssl-mode missing or empty argument
  {
    const std::vector<std::string> argument_required_options{
        "--ssl-mode",    "--ssl-cipher", "--tls-version",
        "--ssl-ca",      "--ssl-capath", "--ssl-crl",
        "--ssl-crlpath", "--ssl-cert",   "--ssl-key"};

    for (auto &opt : argument_required_options) {
      // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
      const std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", opt};
      try {
        MySQLRouter router(program_name_, argv);
        FAIL() << "Expected std::invalid_argument to be thrown";
      } catch (const std::runtime_error &e) {
        EXPECT_EQ("option '" + opt + "' expects a value, got nothing",
                  e.what());  // TODO it would be nice to make case consistent
        SUCCEED();
      } catch (...) {
        FAIL() << "Expected std::runtime_error to be thrown";
      }

      // the value is required but also it CAN'T be empty, like when the user
      // uses --tls-version ""
      const std::vector<std::string> argv2{"-V", "--bootstrap", "0:3310", opt,
                                           ""};
      try {
        MySQLRouter router(program_name_, argv2);
        FAIL() << "Expected std::invalid_argument to be thrown";
      } catch (const std::runtime_error &e) {
        if (opt == "--ssl-mode") {
          // The error for -ssl-mode is slightly different than for other
          // options
          // - detected differently
          EXPECT_STREQ("Invalid value for --ssl-mode option", e.what());
        } else {
          EXPECT_EQ("Value for option '" + opt + "' can't be empty.", e.what());
        }
        SUCCEED();
      } catch (...) {
        FAIL() << "Expected std::runtime_error to be thrown";
      }
    }
  }

  // --ssl-mode has an invalid argument
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--ssl-mode", "bad", "--bootstrap",
                                  "0:3310"};
    try {
      MySQLRouter router(program_name_, argv);
      FAIL() << "Expected std::invalid_argument to be thrown";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("Invalid value for --ssl-mode option", e.what());
      SUCCEED();
    } catch (...) {
      FAIL() << "Expected std::runtime_error to be thrown";
    }
  }

  // --ssl-mode has an invalid argument
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", "--ssl-mode",
                                  "bad"};
    try {
      MySQLRouter router(program_name_, argv);
      FAIL() << "Expected std::invalid_argument to be thrown";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("Invalid value for --ssl-mode option", e.what());
      SUCCEED();
    } catch (...) {
      FAIL() << "Expected std::runtime_error to be thrown";
    }
  }

  // --ssl-mode = DISABLED + uppercase
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", "--ssl-mode",
                                  "DISABLED"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("DISABLED", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = PREFERRED + lowercase
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", "--ssl-mode",
                                  "preferred"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("preferred", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = REQUIRED + mixedcase
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", "--ssl-mode",
                                  "rEqUIrEd"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("rEqUIrEd", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = VERIFY_CA
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V", "--bootstrap", "0:3310", "--ssl-mode",
                                  "verify_ca"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("verify_ca", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = VERIFY_CA, --ssl-ca etc
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V",
                                  "--bootstrap",
                                  "0:3310",
                                  "--ssl-mode",
                                  "verify_ca",
                                  "--ssl-ca=/some/ca.pem",
                                  "--ssl-capath=/some/cadir",
                                  "--ssl-crl=/some/crl.pem",
                                  "--ssl-crlpath=/some/crldir"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("verify_ca", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/ca.pem", router.bootstrap_options_.at("ssl_ca"));
    EXPECT_EQ("/some/cadir", router.bootstrap_options_.at("ssl_capath"));
    EXPECT_EQ("/some/crl.pem", router.bootstrap_options_.at("ssl_crl"));
    EXPECT_EQ("/some/crldir", router.bootstrap_options_.at("ssl_crlpath"));
  }

  // --ssl-mode = VERIFY_IDENTITY, --ssl-ca etc
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V",
                                  "--bootstrap",
                                  "0:3310",
                                  "--ssl-mode",
                                  "verify_identity",
                                  "--ssl-ca=/some/ca.pem",
                                  "--ssl-capath=/some/cadir",
                                  "--ssl-crl=/some/crl.pem",
                                  "--ssl-crlpath=/some/crldir"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("verify_identity", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/ca.pem", router.bootstrap_options_.at("ssl_ca"));
    EXPECT_EQ("/some/cadir", router.bootstrap_options_.at("ssl_capath"));
    EXPECT_EQ("/some/crl.pem", router.bootstrap_options_.at("ssl_crl"));
    EXPECT_EQ("/some/crldir", router.bootstrap_options_.at("ssl_crlpath"));
  }

  // --ssl-mode = REQUIRED, --ssl-* cipher options
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{
        "-V",       "--bootstrap",  "0:3310",         "--ssl-mode",
        "required", "--ssl-cipher", "FOO-BAR-SHA678", "--tls-version",
        "TLSv1"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("required", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("FOO-BAR-SHA678", router.bootstrap_options_.at("ssl_cipher"));
    EXPECT_EQ("TLSv1", router.bootstrap_options_.at("tls_version"));
  }

  // --ssl-mode = REQUIRED, --ssl-cert, --ssl-key
  {  // vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv{"-V",
                                  "--bootstrap",
                                  "0:3310",
                                  "--ssl-mode",
                                  "required",
                                  "--ssl-cert=/some/cert.pem",
                                  "--ssl-key=/some/key.pem"};
    MySQLRouter router(program_name_, argv);
    EXPECT_EQ("required", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/cert.pem", router.bootstrap_options_.at("ssl_cert"));
    EXPECT_EQ("/some/key.pem", router.bootstrap_options_.at("ssl_key"));
  }
}

TEST_F(ConfigGeneratorTest, ssl_stage2_bootstrap_connection) {
  // These tests verify that MySQLSession::set_ssl_options() gets called with
  // appropriate SSL options before making connection to metadata server during
  // bootstrap

  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  // mode
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "DISABLED"}});  // DISABLED + uppercase
    EXPECT_EQ(mock_mysql->last_ssl_mode, SSL_MODE_DISABLED);
  }
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "preferred"}});  // PREFERRED + lowercase
    EXPECT_EQ(mock_mysql->last_ssl_mode, SSL_MODE_PREFERRED);
  }
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "rEqUIrEd"}});  // REQUIRED + mixedcase
    EXPECT_EQ(mock_mysql->last_ssl_mode, SSL_MODE_REQUIRED);
  }
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "VERIFY_CA"}});
    EXPECT_EQ(mock_mysql->last_ssl_mode, SSL_MODE_VERIFY_CA);
  }
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "VERIFY_IDENTITY"}});
    EXPECT_EQ(mock_mysql->last_ssl_mode, SSL_MODE_VERIFY_IDENTITY);
  }
  {
      // invalid ssl_mode should get handled at arg-passing stage, and so we
      // have a unit test for that in ssl_stage1_cmdline_arg_parse test above
  }

  // other fields
  {
    common_pass_metadata_checks(mock_mysql.get());
    ConfigGenerator config_gen;
    config_gen.init("", {
                            {"ssl_ca", "/some/ca/file"},
                            {"ssl_capath", "/some/ca/dir"},
                            {"ssl_crl", "/some/crl/file"},
                            {"ssl_crlpath", "/some/crl/dir"},
                            {"ssl_cipher", "FOO-BAR-SHA678"},
                            {"tls_version", "TLSv1"},
                            {"ssl_cert", "/some/cert.pem"},
                            {"ssl_key", "/some/key.pem"},
                        });
    EXPECT_EQ(mock_mysql->last_ssl_ca, "/some/ca/file");
    EXPECT_EQ(mock_mysql->last_ssl_capath, "/some/ca/dir");
    EXPECT_EQ(mock_mysql->last_ssl_crl, "/some/crl/file");
    EXPECT_EQ(mock_mysql->last_ssl_crlpath, "/some/crl/dir");
    EXPECT_EQ(mock_mysql->last_ssl_cipher, "FOO-BAR-SHA678");
    EXPECT_EQ(mock_mysql->last_tls_version, "TLSv1");
    EXPECT_EQ(mock_mysql->last_ssl_cert, "/some/cert.pem");
    EXPECT_EQ(mock_mysql->last_ssl_key, "/some/key.pem");
  }
}

TEST_F(ConfigGeneratorTest, ssl_stage3_create_config) {
  // These tests verify that config parameters passed to
  // ConfigGenerator::create_config() will make it to configuration file as
  // expected. Note that even though ssl_mode options are not case-sensive,
  // their case should be preserved (written to config file exactly as given in
  // bootstrap options).

  TestConfigGenerator config_gen;
  common_pass_setup_session(mock_mysql.get());
  common_pass_schema_version(mock_mysql.get());
  common_pass_cluster_type(mock_mysql.get());
  common_pass_metadata_supported(mock_mysql.get());
  common_pass_group_replication_online(mock_mysql.get());
  common_pass_group_has_quorum(mock_mysql.get());
  common_pass_group_name(mock_mysql.get());
  config_gen.init(kServerUrl, {});

  auto test_config_output =
      [&config_gen, this](
          const std::map<std::string, std::string> &user_options,
          const char *result) {
        ConfigGenerator::Options options =
            config_gen.fill_options(user_options, default_paths, {});
        std::stringstream conf_output, state_output;
        mysqlrouter::ClusterInfo cluster_info{
            {"server1", "server2", "server3"}, "", "gr_id", "my-cluster"};
        config_gen.create_config(conf_output, state_output, 123, "myrouter",
                                 "user", cluster_info, "cluster_user", options,
                                 default_paths, {});
        EXPECT_THAT(conf_output.str(), HasSubstr(result));
      };

  test_config_output({{"ssl_mode", "DISABLED"}},
                     "ssl_mode=DISABLED");  // DISABLED + uppercase
  test_config_output({{"ssl_mode", "preferred"}},
                     "ssl_mode=preferred");  // PREFERRED + lowercase
  test_config_output({{"ssl_mode", "rEqUIrEd"}},
                     "ssl_mode=rEqUIrEd");  // REQUIRED + mixedcase
  test_config_output({{"ssl_mode", "Verify_Ca"}}, "ssl_mode=Verify_Ca");
  test_config_output({{"ssl_mode", "Verify_identity"}},
                     "ssl_mode=Verify_identity");

  test_config_output({{"ssl_ca", "/some/path"}}, "ssl_ca=/some/path");
  test_config_output({{"ssl_capath", "/some/path"}}, "ssl_capath=/some/path");
  test_config_output({{"ssl_crl", "/some/path"}}, "ssl_crl=/some/path");
  test_config_output({{"ssl_crlpath", "/some/path"}}, "ssl_crlpath=/some/path");
  test_config_output({{"ssl_cipher", "FOO-BAR-SHA678"}},
                     "ssl_cipher=FOO-BAR-SHA678");
  test_config_output({{"tls_version", "TLSv1"}}, "tls_version=TLSv1");
}

TEST_F(ConfigGeneratorTest, warn_on_no_ssl) {
  // These test warn_on_no_ssl(). For convenience, it returns true if no warning
  // has been issued, false if it issued a warning. And it throws if something
  // went wrong.

  constexpr char kQuery[] = "show status like 'ssl_cipher'";
  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql.get());
  config_gen.init(kServerUrl, {});

  // anything other than PREFERRED (or empty, which defaults to PREFERRED)
  // should never warn. warn_on_no_ssl() shouldn't even bother querying the
  // database.
  {
    EXPECT_TRUE(config_gen.warn_on_no_ssl(
        {{"ssl_mode", mysqlrouter::MySQLSession::kSslModeRequired}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl(
        {{"ssl_mode", mysqlrouter::MySQLSession::kSslModeDisabled}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl(
        {{"ssl_mode", mysqlrouter::MySQLSession::kSslModeVerifyCa}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl(
        {{"ssl_mode", mysqlrouter::MySQLSession::kSslModeVerifyIdentity}}));
  }

  // run for 2 ssl_mode cases: unspecified and PREFERRED (they are equivalent)
  typedef std::map<std::string, std::string> Opts;
  std::vector<Opts> opts{
      Opts{}, Opts{{"ssl_mode", mysqlrouter::MySQLSession::kSslModePreferred}}};
  for (const Opts &opt : opts) {
    {  // have SLL
      mock_mysql->expect_query_one(kQuery).then_return(
          0, {{"ssl_cipher", "some_cipher"}});
      EXPECT_TRUE(config_gen.warn_on_no_ssl(opt));
    }

    {  // don't have SLL - empty string
      mock_mysql->expect_query_one(kQuery).then_return(0, {{"ssl_cipher", ""}});
      EXPECT_FALSE(config_gen.warn_on_no_ssl(opt));
    }

    {  // don't have SLL - null string
      mock_mysql->expect_query_one(kQuery).then_return(
          0, {{"ssl_cipher", nullptr}});
      EXPECT_FALSE(config_gen.warn_on_no_ssl(opt));
    }

    // CORNERCASES FOLLOW

    {  // query failure
      mock_mysql->expect_query_one(kQuery).then_error("boo!", 1234);
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    {  // bogus query result - no columns
      mock_mysql->expect_query_one(kQuery).then_return(0, {});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    {  // bogus query result - null column
      mock_mysql->expect_query_one(kQuery).then_return(0, {{nullptr}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    {  // bogus query result - 1 column
      mock_mysql->expect_query_one(kQuery).then_return(0, {{"foo"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    {  // bogus query result - 1 column (ssl_cipher)
      mock_mysql->expect_query_one(kQuery).then_return(0, {{"ssl_cipher"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    {  // bogus query result - 2 columns, but first is not ssl_cipher
      mock_mysql->expect_query_one(kQuery).then_return(0, {{"foo", "bar"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }
  }
}

TEST_F(ConfigGeneratorTest, warn_no_ssl_false) {
  const std::vector<std::string> prefered_values{"PREFERRED", "preferred",
                                                 "Preferred"};
  for (size_t i = 0u; i < prefered_values.size(); ++i) {
    ConfigGenerator config_gen;

    common_pass_metadata_checks(mock_mysql.get());
    mock_mysql->expect_query_one("show status like 'ssl_cipher'");
    mock_mysql->then_return(2, {{mock_mysql->string_or_null("ssl_cipher"),
                                 mock_mysql->string_or_null("")}});

    std::map<std::string, std::string> options;
    options["ssl_mode"] = prefered_values[i];

    config_gen.init(kServerUrl, {});
    const bool res = config_gen.warn_on_no_ssl(options);

    ASSERT_FALSE(res);
  }
}

TEST_F(ConfigGeneratorTest, warn_no_ssl_true) {
  {
    ConfigGenerator config_gen;

    common_pass_metadata_checks(mock_mysql.get());

    std::map<std::string, std::string> options;
    options["ssl_mode"] = "DISABLED";

    config_gen.init(kServerUrl, {});
    const bool res = config_gen.warn_on_no_ssl(options);

    ASSERT_TRUE(res);
  }
}

TEST_F(ConfigGeneratorTest, set_file_owner_no_user) {
  ConfigGenerator config_gen;

  std::map<std::string, std::string> empty_options;
  ASSERT_NO_THROW(
      config_gen.set_file_owner(empty_options, test_dir.name() + "/somefile"));
}

TEST_F(ConfigGeneratorTest, set_file_owner_user_empty) {
  ConfigGenerator config_gen;

  std::map<std::string, std::string> bootstrap_options{{"user", ""}};
  ASSERT_NO_THROW(config_gen.set_file_owner(bootstrap_options,
                                            test_dir.name() + "/somefile"));
}

// bootstrap from URI/unix-socket/hostname checks
const std::string kDefaultUsername = "root";
const std::string kDefaultPassword = "";
const std::string kEmptyUnixSocket = "";
const uint16_t kDefaultMysqlPort = 0;

// passing a unix-socket path to --bootstrap should raise a runtime_error
TEST_F(ConfigGeneratorTest, bootstrap_from_unixsocket) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return kDefaultPassword; });

  mock_mysql->expect_connect("", kDefaultMysqlPort, kDefaultUsername,
                             kDefaultPassword, test_dir.name() + "/mysql.sock");

  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  EXPECT_THROW({ config_gen.init(test_dir.name() + "/mysql.sock", {}); },
               std::runtime_error);
}

TEST_F(ConfigGeneratorTest, bootstrap_from_ipv6) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("::1", kDefaultMysqlPort, kDefaultUsername,
                             kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  config_gen.init("[::1]", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_ipv6_with_port) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("::1", 3306, kDefaultUsername, kDefaultPassword,
                             kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  config_gen.init("[::1]:3306", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_hostname) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("127.0.0.1", 0, kDefaultUsername, kDefaultPassword,
                             kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  config_gen.init("localhost", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_hostname_with_port) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("127.0.0.1", 3306, kDefaultUsername,
                             kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  config_gen.init("localhost:3306", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_uri) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("127.0.0.1", 3306, kDefaultUsername,
                             kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  config_gen.init("mysql://localhost:3306/", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_uri_unixsocket) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("localhost", 3306, kDefaultUsername,
                             kDefaultPassword, test_dir.name() + "/mysql.sock");
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  EXPECT_NO_THROW({
    config_gen.init("mysql://localhost:3306/",
                    {{"bootstrap_socket", test_dir.name() + "/mysql.sock"}});
  });
}

// a invalid URI (port too large) should trigger an exception
TEST_F(ConfigGeneratorTest, bootstrap_from_invalid_uri) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  EXPECT_THROW(
      {
        config_gen.init(
            "mysql://localhost:330660/",
            {{"bootstrap_socket", test_dir.name() + "/mysql.sock"}});
      },
      std::runtime_error);
}

// if socket-name is specified, the hostname in the bootstrap-uri has to be
// 'localhost'
TEST_F(ConfigGeneratorTest, bootstrap_fail_if_socket_and_hostname) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  EXPECT_THROW(
      {
        config_gen.init("somehost", {{"bootstrap_socket",
                                      test_dir.name() + "/mysql.sock"}});
      },
      std::runtime_error);
}

// if socket-name is specified and hostname is 'localhost' then  bootstrap
// should work
TEST_F(ConfigGeneratorTest, bootstrap_if_socket_and_localhost) {
  mysqlrouter::set_prompt_password(
      [](const std::string &) -> std::string { return ""; });

  mock_mysql->expect_connect("localhost", 0, kDefaultUsername, kDefaultPassword,
                             test_dir.name() + "/mysql.sock");
  common_pass_metadata_checks(mock_mysql.get());

  ConfigGenerator config_gen;
  EXPECT_NO_THROW({
    config_gen.init("localhost",
                    {{"bootstrap_socket", test_dir.name() + "/mysql.sock"}});
  });
}

static void bootstrap_password_test(
    MySQLSessionReplayer *mysql, const std::string &program_name,
    const std::string &dir,
    const std::map<std::string, std::string> &default_paths,
    const std::vector<query_entry_t> &bootstrap_queries,
    std::string password_retries = "5",
    bool force_password_validation = false) {
  ConfigGenerator config_gen;
  ::testing::InSequence s;
  common_pass_metadata_checks(mysql);
  config_gen.init(kServerUrl, {});
  expect_bootstrap_queries(mysql, "my-cluster", bootstrap_queries);

  std::map<std::string, std::string> options;
  options["name"] = "name";
  options["password-retries"] = password_retries;
  options["report-host"] = "dont.query.dns";
  if (force_password_validation) options["force-password-validation"] = "1";

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    delete_dir_recursive(dir);
    mysql_harness::reset_keyring();
  });

  KeyringInfo keyring_info("delme", "delme.key");
  config_gen.set_keyring_info(keyring_info);

  config_gen.bootstrap_directory_deployment(program_name, dir, options, {},
                                            default_paths);
}

static constexpr unsigned kCreateUserQuery = 3;   // measured from front
static constexpr unsigned kCreateUserQuery2 = 9;  // measured backwards from end

TEST_F(ConfigGeneratorTest,
       bootstrap_generate_password_force_password_validation) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries before CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // we expect the user to be created without using HASHed password
  // and mysql_native_password plugin as we are forcing password validation
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED BY",
       ACTION_EXECUTE});

  // copy the remaining bootstrap queries
  for (unsigned i = kCreateUserQuery + 1; i < expected_bootstrap_queries.size();
       ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // verify the user is re-created as required
  bootstrap_queries.at(bootstrap_queries.size() - kCreateUserQuery2) = {
      "CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%' IDENTIFIED "
      "BY",
      ACTION_EXECUTE};

  bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                          default_paths, bootstrap_queries, "5",
                          true /*force_password_validation*/);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_no_native_plugin) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries before CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED WITH mysql_native_password AS",
       ACTION_ERROR, 0, 1524});

  // that should lead to rollback and retry without hashed password
  bootstrap_queries.push_back({"ROLLBACK", ACTION_EXECUTE});

  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED BY",
       ACTION_EXECUTE});

  // copy the remaining bootstrap queries
  for (unsigned i = kCreateUserQuery + 1; i < expected_bootstrap_queries.size();
       ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // verify the user is re-created as required
  bootstrap_queries.at(bootstrap_queries.size() - kCreateUserQuery2) = {
      "CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%' IDENTIFIED "
      "BY",
      ACTION_EXECUTE};

  bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                          default_paths, bootstrap_queries);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_with_native_plugin) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries before CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED WITH mysql_native_password AS",
       ACTION_EXECUTE});

  // copy the remaining bootstrap queries
  for (unsigned i = kCreateUserQuery + 1; i < expected_bootstrap_queries.size();
       ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // verify the user is re-created as required
  bootstrap_queries.at(bootstrap_queries.size() - kCreateUserQuery2) = {
      "CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%' IDENTIFIED "
      "WITH "
      "mysql_native_password AS",
      ACTION_EXECUTE};

  bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                          default_paths, bootstrap_queries);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_retry_ok) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries before CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED WITH mysql_native_password AS",
       ACTION_ERROR, 0, 1524});

  // that should lead to rollback and retry without hashed password
  bootstrap_queries.push_back({"ROLLBACK", ACTION_EXECUTE});

  // emulate error 1819) (password does not satisfy the current policy
  // requirements) after the call to second CREATE USER
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED BY",
       ACTION_ERROR, 0, 1819});

  // that should lead to rollback and another retry without hashed password
  bootstrap_queries.push_back({"ROLLBACK", ACTION_EXECUTE});

  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED BY",
       ACTION_EXECUTE});

  // copy the remaining bootstrap queries
  for (unsigned i = kCreateUserQuery + 1; i < expected_bootstrap_queries.size();
       ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // verify the user is re-created as required
  bootstrap_queries.at(bootstrap_queries.size() - kCreateUserQuery2) = {
      "CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%' IDENTIFIED "
      "BY",
      ACTION_EXECUTE};

  bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                          default_paths, bootstrap_queries);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_retry_failed) {
  const std::string kDirName = "./gen_pass_test";
  const unsigned kPasswordRetries = 3;

  // copy expected bootstrap queries before CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back(
      {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
       " IDENTIFIED WITH mysql_native_password AS",
       ACTION_ERROR, 0, 1524});

  // that should lead to rollback and retry without hashed password for
  // "kPasswordRetries" number of times
  for (unsigned i = 0; i < kPasswordRetries; ++i) {
    bootstrap_queries.push_back({"ROLLBACK", ACTION_EXECUTE});

    // each time emulate error 1819) (password does not satisfy the current
    // policy requirements) after the call to second CREATE USER
    bootstrap_queries.push_back(
        {"CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
         " IDENTIFIED BY",
         ACTION_ERROR, 0, 1819});
  }
  bootstrap_queries.push_back({"ROLLBACK", ACTION_EXECUTE});

  try {
    bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                            default_paths, bootstrap_queries,
                            std::to_string(kPasswordRetries));
    FAIL() << "Expecting exception";
  } catch (const std::runtime_error &exc) {
    ASSERT_NE(std::string::npos,
              std::string(exc.what())
                  .find("Try to decrease the validate_password rules and try "
                        "the operation again."));
  }
}

TEST_F(ConfigGeneratorTest, bootstrap_password_retry_param_wrong_values) {
  const std::string kDirName = "./gen_pass_test";
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < kCreateUserQuery; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }
  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.emplace_back(
      "CREATE USER IF NOT EXISTS 'mysql_router4_012345678901'@'%'"
      " IDENTIFIED WITH mysql_native_password AS",
      ACTION_ERROR, 0, 1524);
  bootstrap_queries.emplace_back("ROLLBACK", ACTION_EXECUTE);

  // without --bootstrap
  {
    const std::vector<std::string> argv{"--password-retries", "2"};
    try {
      MySQLRouter router(program_name_, argv);
      FAIL() << "Expected exception";
    } catch (const std::exception &e) {
      EXPECT_STREQ(
          "Option --password-retries can only be used together with "
          "-B/--bootstrap",
          e.what());
    }
  }

  // value too small
  {
    try {
      bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                              default_paths, bootstrap_queries, "0");
      FAIL() << "Expecting exception";
    } catch (const std::exception &exc) {
      EXPECT_STREQ(
          "--password-retries needs value between 1 and 10000 inclusive, was "
          "'0'",
          exc.what());
    }
  }

  // value too big
  {
    try {
      bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                              default_paths, bootstrap_queries, "999999");
      FAIL() << "Expecting exception";
    } catch (const std::exception &exc) {
      EXPECT_STREQ(
          "--password-retries needs value between 1 and 10000 inclusive, was "
          "'999999'",
          exc.what());
    }
  }

  // value wrong type
  {
    try {
      bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                              default_paths, bootstrap_queries, "foo");
      FAIL() << "Expecting exception";
    } catch (const std::exception &exc) {
      EXPECT_STREQ(
          "--password-retries needs value between 1 and 10000 inclusive, was "
          "'foo'",
          exc.what());
    }
  }

  // value empty
  {
    try {
      bootstrap_password_test(mock_mysql.get(), program_name_, kDirName,
                              default_paths, bootstrap_queries, "");
      FAIL() << "Expecting exception";
    } catch (const std::exception &exc) {
      EXPECT_STREQ(
          "--password-retries needs value between 1 and 10000 inclusive, was "
          "''",
          exc.what());
    }
  }
}

// start.sh/stop.sh is unix-specific
#ifndef _WIN32
TEST_F(ConfigGeneratorTest, start_sh) {
  // This test verifies that start.sh is generated correctly

  // dir where we'll test start.sh
  const std::string deployment_dir = mysql_harness::get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    mysql_harness::delete_dir_recursive(deployment_dir);
  });

  // get path to start.sh
  mysql_harness::Path start_sh(deployment_dir);
  start_sh.append("start.sh");

  // no --user
  {
    // generate start.sh
    TestConfigGenerator().create_start_script(program_name_, deployment_dir,
                                              false, {});

    // test file contents
    ASSERT_TRUE(start_sh.exists());
    char buf[1024] = {0};
    std::ifstream ifs(start_sh.c_str());
    ifs.read(buf, sizeof(buf));
    EXPECT_STREQ(
        (std::string("#!/bin/bash\n") + "basedir=" + deployment_dir.c_str() +
         "\n"
         "ROUTER_PID=$basedir/mysqlrouter.pid " +
         program_name_ +
         " -c $basedir/mysqlrouter.conf &\n"
         "disown %-\n")
            .c_str(),
        buf);
  }

  // with --user
  {
    // generate start.sh
    TestConfigGenerator().create_start_script(program_name_, deployment_dir,
                                              false, {{"user", "loser"}});

    // test file contents
    ASSERT_TRUE(start_sh.exists());
    char buf[1024] = {0};
    std::ifstream ifs(start_sh.c_str());
    ifs.read(buf, sizeof(buf));
    EXPECT_STREQ(
        (std::string("#!/bin/bash\n") + "basedir=" + deployment_dir.c_str() +
         "\n"
         "if [ `whoami` == 'loser' ]; then\n"
         "  ROUTER_PID=$basedir/mysqlrouter.pid " +
         program_name_ +
         " -c $basedir/mysqlrouter.conf &\n"
         "else\n"
         "  sudo ROUTER_PID=$basedir/mysqlrouter.pid " +
         program_name_ +
         " -c $basedir/mysqlrouter.conf --user=loser &\n"
         "fi\n"
         "disown %-\n")
            .c_str(),
        buf);
  }
}

TEST_F(ConfigGeneratorTest, stop_sh) {
  // This test verifies that stop.sh is generated correctly

  // dir where we'll test stop.sh
  const std::string deployment_dir = mysql_harness::get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    mysql_harness::delete_dir_recursive(deployment_dir);
  });

  // generate stop.sh
  TestConfigGenerator().create_stop_script(deployment_dir, {});

  // get path to stop.sh
  mysql_harness::Path stop_sh(deployment_dir);
  stop_sh.append("stop.sh");

  // test file contents
  ASSERT_TRUE(stop_sh.exists());
  char buf[1024] = {0};
  std::ifstream ifs(stop_sh.c_str());
  ifs.read(buf, sizeof(buf));
  const std::string pid_file = deployment_dir + "/mysqlrouter.pid";
  EXPECT_STREQ((std::string("#!/bin/bash\n") + "if [ -f " + pid_file +
                " ]; then\n"
                "  kill -TERM `cat " +
                pid_file + "` && rm -f " + pid_file +
                "\n"
                "fi\n")
                   .c_str(),
               buf);
}
#endif  // #ifndef _WIN32

// TODO Create MockSocketOperations with all methods mocked somewhere in a place
//     commonly accessible to all our tests.  At the time of writing this,
//     there's several MockSocketOperations we have in our test code, they're
//     mostly identical, but unfortunately cannot be reused because they're in
//     different codecases.
class MockSocketOperations : public mysql_harness::SocketOperationsBase {
 public:
  // this one is key to our tests here
  std::string get_local_hostname() {
    throw LocalHostnameResolutionError(
        "some error message from get_local_hostname()");
  }
};

/**
 * @test verify that exception thrown by
 * (Mock)SocketOperations::get_local_hostname() when local hostname lookup fails
 * in ConfigGenerator::register_router() will be caught
 * and rethrown with a user-friendly message
 */

static const mysqlrouter::MetadataSchemaVersion kNewSchemaVersion{2, 0, 3};

TEST_F(ConfigGeneratorTest, register_router_error_message) {
  ::testing::StrictMock<MockSocketOperations>
      sock_ops;  // this implementation will trigger our scenario by throwing

  mysql_harness::RandomGenerator rg;

  TestConfigGenerator conf_gen;

  MySQLSessionReplayer mysql;
  common_pass_cluster_type(&mysql);

  conf_gen.metadata() =
      mysqlrouter::create_metadata(kNewSchemaVersion, &mysql, {}, &sock_ops);

  EXPECT_THROW_LIKE(conf_gen.register_router("foo", "", false),
                    std::runtime_error,
                    "Could not register this Router instance with the cluster "
                    "because querying this host's hostname from OS failed:\n"
                    "  some error message from get_local_hostname()\n"
                    "You may want to try --report-host option to manually "
                    "supply this hostname.");
}

/**
 * @test verify that exception thrown by
 * (Mock)SocketOperations::get_local_hostname() when local hostname lookup fails
 * in ConfigGenerator::ensure_router_id_is_ours() will be caught and rethrown
 * with a user-friendly message
 */
TEST_F(ConfigGeneratorTest, ensure_router_id_is_ours_error_message) {
  ::testing::StrictMock<MockSocketOperations>
      sock_ops;  // this implementation will trigger our scenario by throwing

  MySQLSessionReplayer mysql;
  common_pass_cluster_type(&mysql);
  mysql
      .expect_query_one(
          "SELECT address FROM mysql_innodb_cluster_metadata.v2_routers WHERE "
          "router_id = 1")
      .then_return(1, {{mysql.string_or_null("foo")}});
  mysql_harness::RandomGenerator rg;
  uint32_t router_id = 1u;
  TestConfigGenerator conf_gen;
  conf_gen.metadata() =
      mysqlrouter::create_metadata(kNewSchemaVersion, &mysql, {}, &sock_ops);
  EXPECT_THROW_LIKE(
      conf_gen.ensure_router_id_is_ours(router_id, ""), std::runtime_error,
      "Could not verify if this Router instance is already registered with the "
      "cluster because querying this host's hostname from OS failed:\n"
      "  some error message from get_local_hostname()\n"
      "You may want to try --report-host option to manually supply this "
      "hostname.");
}

class GlobalTestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    auto init_res = net::impl::socket::init();
    ASSERT_TRUE(init_res) << init_res.error();
  }
};

int main(int argc, char *argv[]) {
  // must be full path for .start_sh to pass
  g_program_name = mysql_harness::Path(argv[0]).real_path().str();

  ::testing::AddGlobalTestEnvironment(new GlobalTestEnv);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
