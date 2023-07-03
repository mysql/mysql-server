/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <exception>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <system_error>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "mysql/harness/access_rights.h"
#include "mysql/harness/filesystem.h"  // make_file_private, make_file_public
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "test/temp_directory.h"

/**
 * unwrap an std::expected<T, E>.
 *
 * @throws std::system_error with E
 * @returns a T
 */
template <class T, class E>
auto unwrap(stdx::expected<T, E> &&v) {
  if (!v) throw std::system_error(v.error());

  return std::move(v.value());
}

static mysql_harness::security_descriptor_type only_user_readable_perms() {
#ifdef _WIN32
  using namespace mysql_harness::win32::access_rights;

  return unwrap(AclBuilder()
                    .set(AclBuilder::CurrentUser{},
                         /* READ_CONTROL | WRITE_DAC | */ FILE_GENERIC_READ)
                    .build());
#else
  return S_IRUSR;
#endif
}

static mysql_harness::security_descriptor_type only_user_read_writable_perms() {
#ifdef _WIN32
  using namespace mysql_harness::win32::access_rights;

  return unwrap(AclBuilder()
                    .set(AclBuilder::CurrentUser{}, READ_CONTROL | WRITE_DAC |
                                                        FILE_GENERIC_READ |
                                                        FILE_GENERIC_WRITE)
                    .build());
#else
  return S_IRUSR | S_IWUSR;
#endif
}

#ifndef _WIN32
static mysql_harness::security_descriptor_type only_user_rwx_perms() {
  return S_IRUSR | S_IWUSR | S_IXUSR;
}
#endif

static mysql_harness::security_descriptor_type other_readable_perms() {
#ifdef _WIN32
  using namespace mysql_harness::win32::access_rights;

  return unwrap(
      AclBuilder()
          .set(AclBuilder::CurrentUser{},
               READ_CONTROL | WRITE_DAC | FILE_ALL_ACCESS)
          .set(AclBuilder::WellKnownSid{WinWorldSid}, FILE_GENERIC_READ)
          .build());
#else
  return S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
#endif
}

static mysql_harness::security_descriptor_type all_read_writable_perms() {
#ifdef _WIN32
  using namespace mysql_harness::win32::access_rights;

  return unwrap(AclBuilder()
                    .set(AclBuilder::CurrentUser{},
                         READ_CONTROL | WRITE_DAC | FILE_ALL_ACCESS)
                    // DenyEveryoneReadWritable checks for world::read|write
                    .set(AclBuilder::WellKnownSid{WinWorldSid},
                         READ_CONTROL | FILE_GENERIC_READ | FILE_GENERIC_WRITE)
                    .build());
#else
  return S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
#endif
}

struct AccessRightsParam {
  std::string test_name;

  std::function<mysql_harness::security_descriptor_type()> set_rights;
  stdx::expected<void, std::error_code> expected_verify_res;
};

class AllowUserReadWritableTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<AccessRightsParam> {
 protected:
  TempDirectory dir_;
};

TEST_P(AllowUserReadWritableTest, set_and_verify) {
  auto fn = mysql_harness::Path(dir_.name()).join("somefile").str();

  SCOPED_TRACE("// create the file " + fn);
  { std::ofstream ofs(fn); }

  SCOPED_TRACE("// setting permissions on file " + fn);

  auto set_res = mysql_harness::access_rights_set(fn, GetParam().set_rights());
  ASSERT_TRUE(set_res) << set_res.error() << " " << set_res.error().message();

  SCOPED_TRACE("// getting permissions on file " + fn);
  auto perms_res = mysql_harness::access_rights_get(fn);
  ASSERT_TRUE(perms_res) << perms_res.error() << " "
                         << perms_res.error().message();

  SCOPED_TRACE("// verifying permissions on file " + fn);
  auto verify_res = mysql_harness::access_rights_verify(
      perms_res.value(), mysql_harness::AllowUserReadWritableVerifier());

  ASSERT_EQ(GetParam().expected_verify_res, verify_res) << verify_res;
}

const AccessRightsParam allow_user_read_writable_params[] = {
    {"r________", only_user_readable_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
    {"rw_______", only_user_read_writable_perms, {}},
#ifndef _WIN32
    {"rwx______", only_user_rwx_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
#endif
    {"rw_rw_r__", other_readable_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
    {"rw_rw_rw_", all_read_writable_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
};

INSTANTIATE_TEST_SUITE_P(Spec, AllowUserReadWritableTest,
                         ::testing::ValuesIn(allow_user_read_writable_params),
                         [](const auto &nfo) { return nfo.param.test_name; });

// Deny

class DenyOtherReadWritableTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<AccessRightsParam> {
 protected:
  TempDirectory dir_;
};

/*
 * check the the DenyOtherReadWritableVerifier works.
 */
TEST_P(DenyOtherReadWritableTest, set_and_verify) {
  auto fn = mysql_harness::Path(dir_.name()).join("somefile").str();

  SCOPED_TRACE("// create the file " + fn);
  { std::ofstream ofs(fn); }

  SCOPED_TRACE("// setting permissions on file " + fn);
  auto set_res = mysql_harness::access_rights_set(fn, GetParam().set_rights());
  ASSERT_TRUE(set_res) << set_res.error() << " " << set_res.error().message();

  SCOPED_TRACE("// getting permissions on file " + fn);
  auto perms_res = mysql_harness::access_rights_get(fn);
  ASSERT_TRUE(perms_res) << perms_res.error() << " "
                         << perms_res.error().message();

  SCOPED_TRACE("// verifying permissions on file " + fn);
  auto verify_res = mysql_harness::access_rights_verify(
      perms_res.value(), mysql_harness::DenyOtherReadWritableVerifier());

  ASSERT_EQ(GetParam().expected_verify_res, verify_res) << verify_res;
}

/*
 * check if check_file_access_rights() works.
 */
TEST_P(DenyOtherReadWritableTest, set_and_check) {
  auto fn = mysql_harness::Path(dir_.name()).join("somefile").str();

  SCOPED_TRACE("// create the file " + fn);
  { std::ofstream ofs(fn); }

  SCOPED_TRACE("// setting permissions on file " + fn);
  auto set_res = mysql_harness::access_rights_set(fn, GetParam().set_rights());
  ASSERT_TRUE(set_res);

  if (GetParam().expected_verify_res) {
    try {
      mysql_harness::check_file_access_rights(fn);
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }
  } else {
    try {
      mysql_harness::check_file_access_rights(fn);
      FAIL() << "succeeded, expected to fail with "
             << GetParam().expected_verify_res.error();
    } catch (const std::system_error &e) {
      EXPECT_EQ(e.code(), GetParam().expected_verify_res.error());
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }
  }
}

const AccessRightsParam deny_other_read_writable_params[] = {
    {"r________", only_user_readable_perms, {}},
    {"rw_______", only_user_read_writable_perms, {}},
    {"rw_rw_r__", other_readable_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
    {"rw_rw_rw_", all_read_writable_perms,
     stdx::make_unexpected(make_error_code(std::errc::permission_denied))},
};

INSTANTIATE_TEST_SUITE_P(Spec, DenyOtherReadWritableTest,
                         ::testing::ValuesIn(deny_other_read_writable_params),
                         [](const auto &nfo) { return nfo.param.test_name; });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
