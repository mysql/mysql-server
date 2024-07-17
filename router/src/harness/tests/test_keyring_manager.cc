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

#include "keyring/keyring_manager.h"

#include <fstream>
#include <set>
#include <stdexcept>
#include <system_error>

#include <gtest/gtest.h>

#include "dim.h"
#include "keyring/keyring_memory.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/stdx/expected.h"
#include "random_generator.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

using namespace testing;

class TemporaryFileCleaner {
 public:
  ~TemporaryFileCleaner() {
    if (!getenv("TEST_DONT_DELETE_FILES")) {
      for (auto path : tmp_files_) {
#ifndef _WIN32
        ::unlink(path.c_str());
#else
        DeleteFile(path.c_str()) ? 0 : -1;
#endif
      }
    }
  }

  const std::string &add(const std::string &path) {
    tmp_files_.insert(path);
    return path;
  }

 private:
  std::set<std::string> tmp_files_;
};

static stdx::expected<void, std::error_code> check_file_private(
    const std::string &filename) {
  const auto rights_res = mysql_harness::access_rights_get(filename);
  if (!rights_res) return stdx::unexpected(rights_res.error());

  const auto verify_res = mysql_harness::access_rights_verify(
      rights_res.value(), mysql_harness::AllowUserReadWritableVerifier());

  return verify_res;
}

static std::string file_content(const std::string &file) {
  std::stringstream ss;
  std::ifstream f;
  f.open(file, std::ifstream::binary);
  if (f.fail()) {
    std::error_code ec{errno, std::generic_category()};
    throw std::system_error(ec, file);
  }
  ss << f.rdbuf();

  return ss.str();
}

class FileChangeChecker {
 public:
  FileChangeChecker(std::string path)
      : path_(std::move(path)), contents_{current_file_content()} {}

  std::string initial_file_content() const { return contents_; }
  std::string current_file_content() const { return file_content(path_); }

  [[nodiscard]] bool check_unchanged() const {
    return initial_file_content() == current_file_content();
  }

 private:
  const std::string path_;
  std::string contents_;
};

static bool file_exists(const std::string &file) {
  return mysql_harness::Path(file).exists();
}

TempDirectory tmp_dir;

TEST(KeyringManager, init_tests) {
  static mysql_harness::FakeRandomGenerator static_rg;

  mysql_harness::DIM::instance().set_static_RandomGenerator(&static_rg);
}

TEST(KeyringManager, init_with_key) {
  TemporaryFileCleaner cleaner;

  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  mysql_harness::init_keyring_with_key(cleaner.add(tmp_dir.file("keyring")),
                                       "secret", true);
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();
    EXPECT_NE(kr, nullptr);

    kr->store("foo", "bar", "baz");
    mysql_harness::flush_keyring();
    EXPECT_TRUE(check_file_private(tmp_dir.file("keyring")));

    // this key will not be saved to disk b/c of missing flush
    kr->store("account", "password", "");
    EXPECT_EQ(kr->fetch("foo", "bar"), "baz");

    EXPECT_EQ(kr->fetch("account", "password"), "");
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  EXPECT_FALSE(file_exists(tmp_dir.file("badkeyring")));
  ASSERT_THROW(mysql_harness::init_keyring_with_key(tmp_dir.file("badkeyring"),
                                                    "secret", false),
               std::runtime_error);
  EXPECT_FALSE(file_exists(tmp_dir.file("badkeyring")));

#ifndef _WIN32
  ASSERT_THROW(
      mysql_harness::init_keyring_with_key("/badkeyring", "secret", false),
      std::runtime_error);
  EXPECT_FALSE(file_exists("/badkeyring"));

  ASSERT_THROW(
      mysql_harness::init_keyring_with_key("/badkeyring", "secret", true),
      std::runtime_error);
  EXPECT_FALSE(file_exists("/badkeyring"));
#endif

  ASSERT_THROW(mysql_harness::init_keyring_with_key(tmp_dir.file("keyring"),
                                                    "badkey", false),
               mysql_harness::decryption_error);

  ASSERT_THROW(
      mysql_harness::init_keyring_with_key(tmp_dir.file("keyring"), "", false),
      mysql_harness::decryption_error);

  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  mysql_harness::init_keyring_with_key(tmp_dir.file("keyring"), "secret",
                                       false);
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();

    EXPECT_EQ(kr->fetch("foo", "bar"), "baz");
    ASSERT_THROW(kr->fetch("account", "password"), std::out_of_range);
  }

  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  // no key no service
  ASSERT_THROW(mysql_harness::init_keyring_with_key(
                   cleaner.add(tmp_dir.file("xkeyring")), "", true),
               std::runtime_error);
  EXPECT_FALSE(file_exists(tmp_dir.file("xkeyring")));

  // try to open non-existing keyring
  ASSERT_THROW(
      mysql_harness::init_keyring_with_key(
          cleaner.add(tmp_dir.file("invalidkeyring")), "secret", false),
      std::runtime_error);
  EXPECT_FALSE(file_exists(tmp_dir.file("invalidkeyring")));

  // check if keyring is created even if empty
  mysql_harness::init_keyring_with_key(
      cleaner.add(tmp_dir.file("emptykeyring")), "secret2", true);
  EXPECT_TRUE(file_exists(tmp_dir.file("emptykeyring")));
  mysql_harness::reset_keyring();
}

TEST(KeyringManager, init_with_key_file) {
  TemporaryFileCleaner cleaner;

  EXPECT_FALSE(file_exists(tmp_dir.file("keyring")));
  EXPECT_FALSE(file_exists(tmp_dir.file("keyfile")));

  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring")),
                              cleaner.add(tmp_dir.file("keyfile")), true);
  EXPECT_TRUE(file_exists(tmp_dir.file("keyring")));
  EXPECT_TRUE(file_exists(tmp_dir.file("keyfile")));
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();
    EXPECT_NE(kr, nullptr);

    kr->store("foo", "bar", "baz");
    mysql_harness::flush_keyring();
    EXPECT_TRUE(check_file_private(tmp_dir.file("keyring")));
    EXPECT_TRUE(check_file_private(tmp_dir.file("keyfile")));

    // this key will not be saved to disk b/c of missing flush
    kr->store("account", "password", "");
    EXPECT_EQ(kr->fetch("foo", "bar"), "baz");

    EXPECT_EQ(kr->fetch("account", "password"), "");
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  FileChangeChecker check_kf(tmp_dir.file("keyfile"));
  FileChangeChecker check_kr(tmp_dir.file("keyring"));

  EXPECT_FALSE(file_exists(tmp_dir.file("badkeyring")));
  EXPECT_TRUE(file_exists(tmp_dir.file("keyfile")));
  ASSERT_THROW(mysql_harness::init_keyring(tmp_dir.file("badkeyring"),
                                           tmp_dir.file("keyfile"), false),
               std::runtime_error);
  EXPECT_FALSE(file_exists(tmp_dir.file("badkeyring")));

#ifndef _WIN32
  ASSERT_THROW(mysql_harness::init_keyring("/badkeyring",
                                           tmp_dir.file("keyfile"), false),
               std::runtime_error);
  EXPECT_FALSE(file_exists("/badkeyring"));

  ASSERT_THROW(
      mysql_harness::init_keyring("/badkeyring", tmp_dir.file("keyfile"), true),
      std::runtime_error);
  EXPECT_FALSE(file_exists("/badkeyring"));
  EXPECT_TRUE(check_kf.check_unchanged());

  ASSERT_THROW(
      mysql_harness::init_keyring(tmp_dir.file("keyring"), "/keyfile", false),
      std::runtime_error);
  EXPECT_FALSE(file_exists("/keyfile"));

  ASSERT_THROW(mysql_harness::init_keyring("/keyring", "/keyfile", false),
               std::runtime_error);
  EXPECT_FALSE(file_exists("/keyring"));
  EXPECT_FALSE(file_exists("/keyfile"));
#endif
  ASSERT_THROW(mysql_harness::init_keyring(tmp_dir.file("keyring"), "", false),
               std::invalid_argument);

  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  // ensure that none of the tests above touched the keyring files
  EXPECT_TRUE(check_kf.check_unchanged());
  EXPECT_TRUE(check_kr.check_unchanged());

  EXPECT_TRUE(file_exists(tmp_dir.file("keyring")));
  EXPECT_TRUE(file_exists(tmp_dir.file("keyfile")));
  // reopen it
  mysql_harness::init_keyring(tmp_dir.file("keyring"), tmp_dir.file("keyfile"),
                              false);
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();

    EXPECT_EQ(kr->fetch("foo", "bar"), "baz");

    ASSERT_THROW(kr->fetch("account", "password"), std::out_of_range);
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  // try to reopen keyring with bad key file
  ASSERT_THROW(mysql_harness::init_keyring(tmp_dir.file("keyring"),
                                           tmp_dir.file("badkeyfile"), false),
               std::runtime_error);

  // try to reopen bad keyring with right keyfile
  ASSERT_THROW(mysql_harness::init_keyring(tmp_dir.file("badkeyring"),
                                           tmp_dir.file("keyfile"), false),
               std::runtime_error);

  ASSERT_THROW(mysql_harness::init_keyring(tmp_dir.file("badkeyring"),
                                           tmp_dir.file("badkeyfile"), false),
               std::runtime_error);
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);

  // ensure that none of the tests above touched the keyring files
  EXPECT_TRUE(check_kf.check_unchanged());
  EXPECT_TRUE(check_kr.check_unchanged());

  // create a new keyring reusing the same keyfile, which should result in
  // 2 master keys stored in the same keyfile
  EXPECT_FALSE(file_exists(tmp_dir.file("keyring2")));
  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring2")),
                              cleaner.add(tmp_dir.file("keyfile")), true);
  EXPECT_TRUE(file_exists(tmp_dir.file("keyring2")));
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();
    EXPECT_NE(kr, nullptr);

    kr->store("user", "pass", "hooray");
    mysql_harness::flush_keyring();
    EXPECT_TRUE(check_file_private(tmp_dir.file("keyring2")));

    mysql_harness::flush_keyring();
    EXPECT_TRUE(file_exists(tmp_dir.file("keyring2")));
  }
  mysql_harness::reset_keyring();

  // the original keyring should still be unchanged, but not the keyfile
  bool b1 = check_kf.check_unchanged();
  bool b2 = check_kr.check_unchanged();
  EXPECT_FALSE(b1);
  EXPECT_TRUE(b2);

  // now try to reopen both keyrings
  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring2")),
                              cleaner.add(tmp_dir.file("keyfile")), false);
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();
    EXPECT_EQ(kr->fetch("user", "pass"), "hooray");
  }
  mysql_harness::reset_keyring();

  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring")),
                              cleaner.add(tmp_dir.file("keyfile")), false);
  {
    mysql_harness::Keyring *kr = mysql_harness::get_keyring();
    EXPECT_EQ(kr->fetch("foo", "bar"), "baz");
  }
  mysql_harness::reset_keyring();

  // now try to open with bogus key file
  ASSERT_THROW(
      mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring")),
                                  cleaner.add(tmp_dir.file("keyring2")), false),
      std::runtime_error);
}

TEST(KeyringManager, regression) {
  TemporaryFileCleaner cleaner;

  // init keyring with no create flag was writing to existing file on open
  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring")),
                              cleaner.add(tmp_dir.file("keyfile")), true);
  mysql_harness::Keyring *kr = mysql_harness::get_keyring();
  kr->store("1", "2", "3");
  mysql_harness::flush_keyring();
  mysql_harness::reset_keyring();

  FileChangeChecker check_kf(tmp_dir.file("keyfile"));
  FileChangeChecker check_kr(tmp_dir.file("keyring"));

  mysql_harness::init_keyring(cleaner.add(tmp_dir.file("keyring")),
                              cleaner.add(tmp_dir.file("keyfile")), false);
  EXPECT_TRUE(check_kf.check_unchanged());
  EXPECT_TRUE(check_kr.check_unchanged());

  ASSERT_THROW(
      mysql_harness::init_keyring(cleaner.add(tmp_dir.file("bogus1")),
                                  cleaner.add(tmp_dir.file("bogus2")), false),
      std::runtime_error);
  ASSERT_THROW(
      mysql_harness::init_keyring(cleaner.add(tmp_dir.file("bogus1")),
                                  cleaner.add(tmp_dir.file("keyfile")), false),
      std::runtime_error);
  EXPECT_FALSE(file_exists(tmp_dir.file("bogus1")));
  EXPECT_FALSE(file_exists(tmp_dir.file("bogus2")));

  EXPECT_TRUE(check_kf.check_unchanged());
  EXPECT_TRUE(check_kr.check_unchanged());

  mysql_harness::reset_keyring();
}

#ifndef _WIN32
TEST(KeyringManager, symlink_dir) {
  SCOPED_TRACE("// prepare symlinked directory");
  TempDirectory tmpdir;

  auto subdir = mysql_harness::Path(tmpdir.name()).join("subdir").str();
  auto symlinkdir = mysql_harness::Path(tmpdir.name()).join("symlink").str();
  ASSERT_EQ(mysql_harness::mkdir(subdir, 0700), 0);
  ASSERT_EQ(symlink(subdir.c_str(), symlinkdir.c_str()), 0);

  auto keyring = symlinkdir + "/keyring";
  auto masterring = symlinkdir + "/keyfile";

  SCOPED_TRACE("// create the encrypted keyring.");
  EXPECT_FALSE(mysql_harness::init_keyring(keyring, masterring, true));
  mysql_harness::reset_keyring();

  SCOPED_TRACE("// try to open it again, via the symlink dir.");
  EXPECT_TRUE(mysql_harness::init_keyring(keyring, masterring, false));
  mysql_harness::reset_keyring();

  SCOPED_TRACE("// try to open it again, via the real dir.");
  EXPECT_TRUE(
      mysql_harness::init_keyring(subdir + "/keyring", masterring, false));
  mysql_harness::reset_keyring();

  SCOPED_TRACE("// try to open it again, via the real dir.");
  EXPECT_TRUE(mysql_harness::init_keyring(subdir + "/keyring",
                                          subdir + "/keyfile", false));
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
