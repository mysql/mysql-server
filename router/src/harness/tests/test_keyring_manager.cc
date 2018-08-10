/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"
#include "dim.h"
#include "gtest/gtest.h"
#include "keyring/keyring_manager.h"
#include "keyring/keyring_memory.h"
#include "random_generator.h"
#include "test/helpers.h"

#include <fstream>
#include <set>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

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

#ifdef _WIN32
// Copied from keyring_file.cc

// Smart pointers for WinAPI structures that use C-style memory management.
using SecurityDescriptorPtr =
    std::unique_ptr<SECURITY_DESCRIPTOR,
                    mysql_harness::StdFreeDeleter<SECURITY_DESCRIPTOR>>;
using SidPtr = std::unique_ptr<SID, mysql_harness::StdFreeDeleter<SID>>;

/**
 * Retrieves file's DACL security descriptor.
 *
 * @param[in] file_name File name.
 *
 * @return File's DACL security descriptor.
 *
 * @throw std::exception Failed to retrieve security descriptor.
 */
static SecurityDescriptorPtr get_security_descriptor(
    const std::string &file_name) {
  static constexpr SECURITY_INFORMATION kReqInfo = DACL_SECURITY_INFORMATION;

  // Get the size of the descriptor.
  DWORD sec_desc_size;

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, nullptr, 0,
                       &sec_desc_size) == FALSE) {
    auto error = GetLastError();

    // We expect to receive `ERROR_INSUFFICIENT_BUFFER`.
    if (error != ERROR_INSUFFICIENT_BUFFER) {
      throw std::runtime_error("GetFileSecurity() failed (" + file_name +
                               "): " + std::to_string(error));
    }
  }

  SecurityDescriptorPtr sec_desc(
      static_cast<SECURITY_DESCRIPTOR *>(std::malloc(sec_desc_size)));

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, sec_desc.get(),
                       sec_desc_size, &sec_desc_size) == FALSE) {
    throw std::runtime_error("GetFileSecurity() failed (" + file_name +
                             "): " + std::to_string(GetLastError()));
  }

  return sec_desc;
}

/**
 * Verifies permissions of an access ACE entry.
 *
 * @param[in] access_ace Access ACE entry.
 *
 * @throw std::exception Everyone has access to the ACE access entry or
 *                        an error occured.
 */
static void check_ace_access_rights(ACCESS_ALLOWED_ACE *access_ace) {
  SID *sid = reinterpret_cast<SID *>(&access_ace->SidStart);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SidPtr everyone_sid(static_cast<SID *>(std::malloc(sid_size)));

  if (CreateWellKnownSid(WinWorldSid, nullptr, everyone_sid.get(), &sid_size) ==
      FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  if (EqualSid(sid, everyone_sid.get())) {
    if (access_ace->Mask & (FILE_EXECUTE)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Execute privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Write privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Read privilege granted to Everyone).");
    }
  }
}

/**
 * Verifies access permissions in a DACL.
 *
 * @param[in] dacl DACL to be verified.
 *
 * @throw std::exception DACL contains an ACL entry that grants full access to
 *                        Everyone or an error occured.
 */
static void check_acl_access_rights(ACL *dacl) {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(dacl, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    throw std::runtime_error("GetAclInformation() failed: " +
                             std::to_string(GetLastError()));
  }

  for (DWORD ace_idx = 0; ace_idx < dacl_size_info.AceCount; ++ace_idx) {
    LPVOID ace = nullptr;

    if (GetAce(dacl, ace_idx, &ace) == FALSE) {
      throw std::runtime_error("GetAce() failed: " +
                               std::to_string(GetLastError()));
      continue;
    }

    if (static_cast<ACE_HEADER *>(ace)->AceType == ACCESS_ALLOWED_ACE_TYPE)
      check_ace_access_rights(static_cast<ACCESS_ALLOWED_ACE *>(ace));
  }
}

/**
 * Verifies access permissions in a security descriptor.
 *
 * @param[in] sec_desc Security descriptor to be verified.
 *
 * @throw std::exception Security descriptor grants full access to
 *                        Everyone or an error occured.
 */
static void check_security_descriptor_access_rights(
    SecurityDescriptorPtr sec_desc) {
  BOOL dacl_present;
  ACL *dacl;
  BOOL dacl_defaulted;

  if (GetSecurityDescriptorDacl(sec_desc.get(), &dacl_present, &dacl,
                                &dacl_defaulted) == FALSE) {
    throw std::runtime_error("GetSecurityDescriptorDacl() failed: " +
                             std::to_string(GetLastError()));
  }

  if (!dacl_present) {
    // No DACL means: no access allowed. Which is fine.
    return;
  }

  if (!dacl) {
    // Empty DACL means: all access allowed.
    throw std::runtime_error(
        "Invalid keyring file access rights "
        "(Everyone has full access rights).");
  }

  check_acl_access_rights(dacl);
}
#endif  // _WIN32

static bool check_file_private(const std::string &file) {
#ifdef _WIN32
  try {
    check_security_descriptor_access_rights(get_security_descriptor(file));
    return true;
  } catch (...) {
    return false;
  }
#else
  struct stat st;
  if (stat(file.c_str(), &st) < 0) {
    throw std::runtime_error(file + ": " + strerror(errno));
  }
  if ((st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == (S_IRUSR | S_IWUSR))
    return true;
  return false;
#endif
}

class FileChangeChecker {
 public:
  FileChangeChecker(const std::string &file) : path_(file) {
    std::ifstream f;
    std::stringstream ss;
    f.open(file, std::ifstream::binary);
    if (f.fail())
      throw std::runtime_error(file + " " + mysql_harness::get_strerror(errno));
    ss << f.rdbuf();
    contents_ = ss.str();
    f.close();
  }

  bool check_unchanged() {
    std::ifstream f;
    std::stringstream ss;
    f.open(path_, std::ifstream::binary);
    if (f.fail())
      throw std::runtime_error(path_ + " " +
                               mysql_harness::get_strerror(errno));
    ss << f.rdbuf();
    f.close();
    return ss.str() == contents_;
  }

 private:
  std::string path_;
  std::string contents_;
};

static bool file_exists(const std::string &file) {
  return mysql_harness::Path(file).exists();
}

TmpDir tmp_dir;

TEST(KeyringManager, init_tests) {
  mysql_harness::DIM::instance().set_RandomGenerator(
      []() {
        static mysql_harness::FakeRandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {}
      // don't delete our static!
  );
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

  // the orignal keyring should still be unchanged, but not the keyfile
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
