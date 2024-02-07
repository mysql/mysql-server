/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

/**
 * BUG24909259 ROUTER IS NOT ABLE TO CONNECT TO M/C AFTER BOOTSTRAPPED WITH DIR
 * AND NAME OPTIONS
 *
 */

#include <gtest/gtest_prod.h>

#include <fstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "dim.h"
#include "keyring/keyring_manager.h"
#include "keyring/keyring_memory.h"
#include "mysql/harness/config_parser.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "router_app.h"

static mysql_harness::Path g_origin;

static std::string kTestKRFile = "tkeyfile";
static std::string kTestKRFile2 = "tkeyfile2";
static struct Initter {
  Initter() {
    tmpdir_ = mysql_harness::get_tmp_dir();
    kTestKRFile = tmpdir_ + "/" + kTestKRFile;
    kTestKRFile2 = tmpdir_ + "/" + kTestKRFile2;
  }
  ~Initter() { mysql_harness::delete_dir_recursive(tmpdir_); }

 private:
  std::string tmpdir_;
} init;
static std::string kTestKey = "mykey";

static std::string my_prompt_password(const std::string &,
                                      int *num_password_prompts) {
  *num_password_prompts = *num_password_prompts + 1;
  return kTestKey;
}

static void create_keyfile(const std::string &path) {
  mysql_harness::delete_file(path);
  mysql_harness::delete_file(path + ".master");
  mysql_harness::init_keyring(path, path + ".master", true);
  mysql_harness::reset_keyring();
}

static void create_keyfile_withkey(const std::string &path,
                                   const std::string &key) {
  mysql_harness::delete_file(path);
  mysql_harness::init_keyring_with_key(path, key, true);
  mysql_harness::reset_keyring();
}

TEST(Bug24909259, init_tests) {
  mysql_harness::DIM::instance().set_RandomGenerator(
      []() {
        static mysql_harness::FakeRandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {}
      // don't delete our static!
  );
}

// bug#24909259
TEST(Bug24909259, PasswordPrompt_plain) {
  create_keyfile(kTestKRFile);
  create_keyfile_withkey(kTestKRFile2, kTestKey);

  int num_password_prompts = 0;
  mysqlrouter::set_prompt_password(
      [&num_password_prompts](const std::string &prompt) {
        return my_prompt_password(prompt, &num_password_prompts);
      });

  // metadata_cache
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    MySQLRouter router;

    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache]\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
    EXPECT_EQ(0, num_password_prompts);
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache]\nuser=foo\n");
    config.read(ss);

    try {
      router.init_keyring(config);

      FAIL() << "expected std::runtime_error, got no exception";
    } catch (const std::runtime_error &) {
      // ok
    } catch (const std::exception &e) {
      FAIL() << "expected std::runtime_error, got " << typeid(e).name() << ": "
             << e.what();
    }
    EXPECT_EQ(1, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  }
  mysql_harness::reset_keyring();
  {
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path=" + kTestKRFile2 +
                         "\n[metadata_cache]\nuser=foo\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_EQ(2, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
  }
  mysql_harness::reset_keyring();
  {
    // this one should succeed completely
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path=" + kTestKRFile +
                         "\nmaster_key_path=" + kTestKRFile +
                         ".master\n[metadata_cache]\nuser=foo\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
    EXPECT_EQ(2, num_password_prompts);
  }
  mysql_harness::reset_keyring();
}

TEST(Bug24909259, PasswordPrompt_keyed) {
  create_keyfile(kTestKRFile);
  create_keyfile_withkey(kTestKRFile2, kTestKey);

  int num_password_prompts = 0;
  mysqlrouter::set_prompt_password(
      [&num_password_prompts](const std::string &prompt) {
        return my_prompt_password(prompt, &num_password_prompts);
      });

  // metadata_cache
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache:foo]\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
    EXPECT_EQ(0, num_password_prompts);
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    try {
      router.init_keyring(config);

      FAIL() << "expected std::runtime_error, got no exception";
    } catch (const std::runtime_error &) {
      // ok
    } catch (const std::exception &e) {
      FAIL() << "expected std::runtime_error, got " << typeid(e).name() << ": "
             << e.what();
    }
    EXPECT_EQ(1, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  }
  mysql_harness::reset_keyring();
  {
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path=" + kTestKRFile2 +
                         "\n[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_EQ(2, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
  }
  mysql_harness::reset_keyring();
  {
    // this one should succeed completely
    MySQLRouter router;
    mysql_harness::Config config(mysqlrouter::get_default_paths(g_origin),
                                 mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path=" + kTestKRFile +
                         "\nmaster_key_path=" + kTestKRFile +
                         ".master\n[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
    EXPECT_EQ(2, num_password_prompts);
  }
  mysql_harness::reset_keyring();
}

int main(int argc, char *argv[]) {
  g_origin = mysql_harness::Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
