/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <stdexcept>
#include <system_error>

#include <gtest/gtest.h>

#include "keyring/keyring_file.h"
#include "mysql/harness/filesystem.h"  // make_file_private, make_file_public

constexpr const char kAesKey[] = "AesKey";
constexpr const char kKeyringFileName[] = "keyring_config";

/**
 * Generic keyring test.
 *
 * Covers tests common for `KeyringMemory` and `KeyringFile`.
 */
template <typename T>
class KeyringTest : public ::testing::Test {};

using KeyringTestTypes =
    ::testing::Types<mysql_harness::KeyringMemory, mysql_harness::KeyringFile>;
TYPED_TEST_SUITE(KeyringTest, KeyringTestTypes);

/**
 * KeyringFile test.
 *
 * Prepares environment for file-based keyring implementation.
 */
class KeyringFileTest : public ::testing::Test {
 public:
  KeyringFileTest() = default;

  void SetUp() override {
    // succeed or fail with: file-not-found
    const auto res = mysql_harness::delete_file(kKeyringFileName);
    if (!res) {
      auto ec = res.error();

      // should match ERROR_FILE_NOT_FOUND on windows too.
      ASSERT_EQ(ec, make_error_condition(std::errc::no_such_file_or_directory));
    }
  }
};

/**
 * Fills keyring with test data.
 *
 * @param[out] keyring Keyring to be filled with data.
 */
static void fill_keyring(mysql_harness::Keyring &keyring) {
  keyring.store("E1", "E1A1", "E1V1");
  keyring.store("E1", "E1A2", "E1V2");
  keyring.store("E2", "E2A1", "E2V1");
  keyring.store("E2", "E2A2", "E2V2");
}

/**
 * Verifies keyring contents.
 *
 * @param[in] keyring Keyring to be verified.
 */
static void verify_keyring(const mysql_harness::Keyring &keyring) {
  ASSERT_EQ(keyring.fetch("E1", "E1A1"), "E1V1");
  ASSERT_EQ(keyring.fetch("E1", "E1A2"), "E1V2");
  ASSERT_EQ(keyring.fetch("E2", "E2A1"), "E2V1");
  ASSERT_EQ(keyring.fetch("E2", "E2A2"), "E2V2");
}

TYPED_TEST(KeyringTest, StoreFetch) {
  TypeParam keyring;

  fill_keyring(keyring);
  verify_keyring(keyring);
}

TYPED_TEST(KeyringTest, AttributeOverwrite) {
  TypeParam keyring;

  keyring.store("Entry", "Attribute", "Value");
  keyring.store("Entry", "Attribute", "OtherValue");

  ASSERT_EQ(keyring.fetch("Entry", "Attribute"), "OtherValue");
}

TYPED_TEST(KeyringTest, FetchUndefinedEntry) {
  TypeParam keyring;

  fill_keyring(keyring);

  EXPECT_THROW(keyring.fetch("InvalidEntry", "Attr"), std::out_of_range);
}

TYPED_TEST(KeyringTest, FetchUndefinedAttribute) {
  TypeParam keyring;

  fill_keyring(keyring);

  EXPECT_THROW(keyring.fetch("Entry", "AttrInvalid"), std::out_of_range);
}

TYPED_TEST(KeyringTest, RemoveEntry) {
  TypeParam keyring;

  keyring.store("Entry", "Attr", "Value");
  keyring.remove("Entry");

  EXPECT_THROW(keyring.fetch("Entry", "Attr"), std::out_of_range);
}

TYPED_TEST(KeyringTest, RemoveAttribute) {
  TypeParam keyring;

  keyring.store("Entry", "Attr", "Value");
  keyring.remove_attribute("Entry", "Attr");

  EXPECT_THROW(keyring.fetch("Entry", "Attr"), std::out_of_range);
}

TYPED_TEST(KeyringTest, SaveAndLoadEmpty) {
  std::vector<char> keyring_data;

  // Serialize empty keyring.
  {
    TypeParam keyring;

    keyring_data = keyring.serialize(kAesKey);
  }

  // Parse keyring data.
  TypeParam keyring;

  keyring.parse(kAesKey, keyring_data.data(), keyring_data.size());
}

TYPED_TEST(KeyringTest, SaveAndLoadFilled) {
  std::vector<char> keyring_data;

  // Serialize filled keyring.
  {
    TypeParam keyring;

    fill_keyring(keyring);
    keyring_data = keyring.serialize(kAesKey);
  }

  // Parse keyring data and verify contents.
  TypeParam keyring;

  keyring.parse(kAesKey, keyring_data.data(), keyring_data.size());
  verify_keyring(keyring);
}

TYPED_TEST(KeyringTest, SaveAndLoadBroken) {
  std::vector<char> keyring_data;

  // Serialize filled keyring.
  {
    TypeParam keyring;

    fill_keyring(keyring);
    keyring_data = keyring.serialize(kAesKey);
  }

  // Try loading a few randomly broken keyring buffers.
  for (std::size_t test_count = 0; test_count < 20; ++test_count) {
    TypeParam keyring;
    auto buffer_offset = static_cast<size_t>(std::rand()) % keyring_data.size();
    auto buffer_size = static_cast<size_t>(std::rand()) %
                       (keyring_data.size() - buffer_offset + 1);

    if (buffer_offset + buffer_size == keyring_data.size()) {
      // Buffer valid, ignore.
      continue;
    }

    EXPECT_THROW(keyring.parse(kAesKey, keyring_data.data() + buffer_offset,
                               buffer_size),
                 std::exception);
  }
}

TYPED_TEST(KeyringTest, SaveAndLoadWithInvalidKey) {
  std::vector<char> keyring_data;

  // Serialize filled keyring.
  {
    TypeParam keyring;

    fill_keyring(keyring);
    keyring_data = keyring.serialize(kAesKey);
  }

  // Parse keyring data with invalid encryption key.
  TypeParam keyring;

  EXPECT_THROW(
      keyring.parse("invalid_key", keyring_data.data(), keyring_data.size()),
      std::exception);
}

TEST_F(KeyringFileTest, LoadFromFileWithCorrectPermissions) {
  {
    mysql_harness::KeyringFile keyring;

    fill_keyring(keyring);
    keyring.save(kKeyringFileName, kAesKey);
    try {
      mysql_harness::make_file_private(kKeyringFileName);
    } catch (const std::system_error &e) {
#ifdef _WIN32
      if (e.code() !=
          std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
      // if the filesystem can't set permissions, ignore it
#endif
        throw;
    }
  }

  mysql_harness::KeyringFile keyring;

  keyring.load(kKeyringFileName, kAesKey);
  verify_keyring(keyring);
}

TEST_F(KeyringFileTest, LoadFromFileWithWrongPermissions) {
  {
    mysql_harness::KeyringFile keyring;

    fill_keyring(keyring);
    keyring.save(kKeyringFileName, kAesKey);
    try {
      mysql_harness::make_file_public(kKeyringFileName);
    } catch (const std::system_error &e) {
#ifdef _WIN32
      if (e.code() ==
          std::error_code(ERROR_INVALID_FUNCTION, std::system_category())) {
        // if the filesystem can't set permissions, the test later would fail
        return;
      }
#endif
      throw;
    }
  }

  mysql_harness::KeyringFile keyring;

  EXPECT_THROW(keyring.load(kKeyringFileName, kAesKey), std::exception);
}

TEST_F(KeyringFileTest, LoadFromNonexistentFile) {
  mysql_harness::KeyringFile keyring;

  // Setup() deletes keyring file.
  EXPECT_THROW(keyring.load(kKeyringFileName, kAesKey), std::exception);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
