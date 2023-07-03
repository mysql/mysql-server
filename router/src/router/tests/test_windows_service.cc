/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
 * @brief Unit tests for Windows Service wrapper
 */

#include <gmock/gmock.h>

#include "gtest_consoleoutput.h"

#include <cctype>
#include <stdexcept>
#include <string>

#include "filesystem_utils.h"
#include "router_test_helpers.h"  // EXPECT_THROW_LIKE

// these tests are Windows-specific
#ifdef _WIN32

// these are not declared in a header, because they're private to
// main-windows.cc
std::string get_logging_folder(const std::string &conf_file);
void allow_windows_service_to_write_logs(const std::string &conf_file);

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::StrEq;

/**
 * @brief get_logging_folder() tests
 */
class GetLoggingFolderTest : public ::testing::Test {
 public:
  void init_conf(const std::string &conf_file_payload) {
    // create conf dir; it will be deleted in TearDown()
    conf_dir_ = mysql_harness::get_tmp_dir();

    // set path to config file
    path_to_conf_file_ = mysql_harness::Path(conf_dir_);
    path_to_conf_file_.append("some.conf");

    // create the config file
    std::ofstream of(path_to_conf_file_.c_str());
    of << conf_file_payload << std::flush;
    ASSERT_TRUE(path_to_conf_file_.is_regular());
  }

 protected:
  void SetUp() override {}
  void TearDown() override {
    if (!conf_dir_.empty()) mysql_harness::delete_dir_recursive(conf_dir_);
  }

  std::string conf_dir_;
  mysql_harness::Path path_to_conf_file_;
};

/**
 * @test
 * Verify get_logging_folder() throws when config file doesn't exist
 */
TEST_F(GetLoggingFolderTest, no_such_config_file) {
  EXPECT_THROW_LIKE(get_logging_folder("no/such/config/file"),
                    std::runtime_error,
                    "Reading configuration file 'no/such/config/file' failed: "
                    "Path 'no/such/config/file' does not exist");
}

/**
 * @test
 * Verify get_logging_folder() returns `logging_folder` value when it is
 * defined in config file
 */
TEST_F(GetLoggingFolderTest, config_file_has_logging_folder) {
  const std::string log_dir = "this/dir/does/not/have/to/exist";
  init_conf(std::string("[DEFAULT]\n") + "logging_folder = " + log_dir + "\n");

  // should return logging folder value (whether it exists doesn't matter)
  EXPECT_THAT(get_logging_folder(path_to_conf_file_.str()), StrEq(log_dir));
}

/**
 * @test
 * Verify get_logging_folder() throws on config parse error config file
 */
TEST_F(GetLoggingFolderTest, config_file_is_invalid) {
  init_conf("some_entry_outside_of_any_section = illegal_config_file\n");

  // should report config parse failure
  const std::string err_msg = std::string("Reading configuration file '") +
                              path_to_conf_file_.str() +
                              "' failed: Option line before start of section";
  EXPECT_THROW_LIKE(get_logging_folder(path_to_conf_file_.str()),
                    std::runtime_error, err_msg.c_str());
}

/**
 * @test
 * Verify that when `logging_folder` is not defined in config file,
 * get_logging_folder() will return (computed) default logging folder.
 */
TEST_F(GetLoggingFolderTest, config_file_does_not_have_logging_folder) {
  init_conf("[DEFAULT]\n");

  // Should return (computed) default value. We don't bother comparing against
  // a particular value, because that would require this test to compute it,
  // essentially reimplementing the get_logging_folder()'s part that does this.
  // We cannot use a precomputed value either, because the value depends on the
  // path of this test executable. The best we can do, is test if returned
  // value is an absolute Windows path, that means, 1 letter followed by ':'
  // and either '\' or '/'
  const std::string logging_folder =
      get_logging_folder(path_to_conf_file_.str());
  EXPECT_THAT(std::toupper(logging_folder.data()[0]), AllOf(Ge('A'), Le('Z')));
  EXPECT_THAT(logging_folder.data()[1], Eq(':'));
  EXPECT_THAT(logging_folder.data()[2], AnyOf(Eq('\\'), Eq('/')));
}

/**
 * @brief allow_windows_service_to_write_logs() tests
 */
class AllowWindowsServiceToWriteLogsTest : public ::testing::Test {
 public:
  void init_dirs_and_config() {
    // create dirs; they will be deleted in TearDown()
    conf_dir_ = mysql_harness::get_tmp_dir();
    log_dir_ = mysql_harness::get_tmp_dir();

    // set path to config file
    path_to_conf_file_ = mysql_harness::Path(conf_dir_);
    path_to_conf_file_.append("some.conf");

    // create the config file
    std::ofstream of(path_to_conf_file_.c_str());
    of << "[DEFAULT]\n"
       << "logging_folder = " << log_dir_ << "\n"
       << std::flush;
    ASSERT_TRUE(path_to_conf_file_.is_regular());
  }

 protected:
  void SetUp() override {}
  void TearDown() override {
    if (!log_dir_.empty()) mysql_harness::delete_dir_recursive(log_dir_);
    if (!conf_dir_.empty()) mysql_harness::delete_dir_recursive(conf_dir_);
  }

  std::string conf_dir_;
  std::string log_dir_;
  mysql_harness::Path path_to_conf_file_;
};

/**
 * @test
 * Sunny day scenario: verify that when log dir and file both exist, both are
 * assigned RW permissions for Windows Service user (LocalService)
 */
TEST_F(AllowWindowsServiceToWriteLogsTest, log_dir_and_file_exist) {
  init_dirs_and_config();
  const Path path_to_log_file{Path{log_dir_}.join("mysqlrouter.log")};

  // create log file
  std::ofstream of(path_to_log_file.c_str());
  ASSERT_TRUE(path_to_log_file.is_regular());

  // set permissions
  EXPECT_NO_THROW(
      allow_windows_service_to_write_logs(path_to_conf_file_.str()));

  // verify log dir has RW permissions set for LocalUser
  ASSERT_NO_FATAL_FAILURE(
      check_config_file_access_rights(log_dir_, /*read_only=*/false));
}

/**
 * @test
 * Sunny day scenario: verify that when (only) log dir exists, it is assigned
 * RW permissions for Windows Service user (LocalService)
 */
TEST_F(AllowWindowsServiceToWriteLogsTest, log_dir_exists) {
  init_dirs_and_config();

  // set permissions
  EXPECT_NO_THROW(
      allow_windows_service_to_write_logs(path_to_conf_file_.str()));

  // verify log dir have RW permissions set for LocalUser
  ASSERT_NO_FATAL_FAILURE(
      check_config_file_access_rights(log_dir_, /*read_only=*/false));
}

/**
 * @test
 * Verify that passing invalid config file raises an exception (actual throwing
 * should be done by get_logging_folder() inside, and other cases that trigger
 * this are tested in GetLoggingFolderTest; here we just verify that the
 * exception will be passed on to the outside code)
 */
TEST_F(AllowWindowsServiceToWriteLogsTest, bad_config_file) {
  EXPECT_THROW_LIKE(allow_windows_service_to_write_logs("no/such/config/file"),
                    std::runtime_error,
                    "Reading configuration file 'no/such/config/file' failed: "
                    "Path 'no/such/config/file' does not exist");
}

/**
 * @test
 * Verify that when log dir does not exist, an appropriate exception is thrown
 */
TEST_F(AllowWindowsServiceToWriteLogsTest, log_dir_does_not_exist) {
  init_dirs_and_config();

  // erase log dir
  std::string erased_log_dir;
  mysql_harness::delete_dir_recursive(log_dir_);
  log_dir_.swap(erased_log_dir);  // prevent TearDown() from trying to erase it

  // test without log dir
  std::string expected_error =
      std::string("logging_folder '") + erased_log_dir +
      "' specified (or implied) by configuration file '" +
      path_to_conf_file_.str() + "' does not point to a valid directory";
  EXPECT_THROW_LIKE(
      allow_windows_service_to_write_logs(path_to_conf_file_.str()),
      std::runtime_error, expected_error);
}

/**
 * @test
 * Verify that when `log dir` actually refers to something else other than a
 * dir (e.g. a file), an appropriate exception is thrown
 */
TEST_F(AllowWindowsServiceToWriteLogsTest, log_dir_is_not_a_dir) {
  init_dirs_and_config();

  // erase log dir
  std::string erased_log_dir;
  mysql_harness::delete_dir_recursive(log_dir_);
  log_dir_.swap(erased_log_dir);  // prevent TearDown() from trying to erase it

  // create a file with the same name as log dir
  std::string expected_error =
      std::string("logging_folder '") + erased_log_dir +
      "' specified (or implied) by configuration file '" +
      path_to_conf_file_.str() + "' does not point to a valid directory";
  std::ofstream of(erased_log_dir);
  std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { mysql_harness::delete_file(erased_log_dir); });
  ASSERT_TRUE(Path{erased_log_dir}.is_regular());

  // test with file in place of log dir
  EXPECT_THROW_LIKE(
      allow_windows_service_to_write_logs(path_to_conf_file_.str()),
      std::runtime_error, expected_error);
}

#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
