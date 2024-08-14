/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/filesystem.h"

////////////////////////////////////////
// Standard include files

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

////////////////////////////////////////
// Test system include files
#include "router/tests/helpers/stdx_expected_no_error.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

using mysql_harness::Directory;
using mysql_harness::Path;

Path g_here;

TEST(TestFilesystem, TestPath) {
  // Testing basic path construction
  EXPECT_EQ(Path("/data/logger.cfg"), "/data/logger.cfg");
  EXPECT_EQ(Path("data/logger.cfg"), "data/logger.cfg");
  EXPECT_EQ(Path("/"), "/");
  EXPECT_EQ(Path("//"), "/");
  EXPECT_EQ(Path("////////"), "/");
  EXPECT_EQ(Path("/data/"), "/data");
  EXPECT_EQ(Path("data/"), "data");
  EXPECT_EQ(Path("data////"), "data");

  // Testing dirname function
  EXPECT_EQ(Path("foo.cfg").dirname(), ".");
  EXPECT_EQ(Path("foo/bar.cfg").dirname(), "foo");
  EXPECT_EQ(Path("/foo/bar.cfg").dirname(), "/foo");
  EXPECT_EQ(Path("/").dirname(), "/");

  // Testing basename function
  EXPECT_EQ(Path("foo.cfg").basename(), "foo.cfg");
  EXPECT_EQ(Path("foo/bar.cfg").basename(), "bar.cfg");
  EXPECT_EQ(Path("/foo/bar.cfg").basename(), "bar.cfg");
  EXPECT_EQ(Path("/").basename(), "/");

  // Testing join function (and indirectly the append function).
  Path new_path = Path("data").join("test");
  EXPECT_EQ(new_path, "data/test");

  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());

  // Testing file status checking functions
  EXPECT_EQ(Path(test_data_dir).type(), Path::FileType::DIRECTORY_FILE);

#ifdef _WIN32
  EXPECT_EQ(Path("c:").type(), Path::FileType::DIRECTORY_FILE);
#endif

  EXPECT_EQ(Path(test_data_dir).join("logger.cfg").type(),
            Path::FileType::REGULAR_FILE);
  EXPECT_EQ(Path(test_data_dir).join("does-not-exist.cfg").type(),
            Path::FileType::FILE_NOT_FOUND);

  EXPECT_TRUE(Path(test_data_dir).is_directory());
  EXPECT_FALSE(Path(test_data_dir).join("logger.cfg").is_directory());
  EXPECT_FALSE(Path(test_data_dir).is_regular());
  EXPECT_TRUE(Path(test_data_dir).join("logger.cfg").is_regular());
}

TEST(TestFilesystem, EmptyPath) {
  // Testing error usage
  EXPECT_THROW({ Path path(""); }, std::invalid_argument);

  // Default-constructed paths should be possible to create, but not
  // to use.
  Path path;
  EXPECT_THROW(path.is_regular(), std::invalid_argument);
  EXPECT_THROW(path.is_directory(), std::invalid_argument);
  EXPECT_THROW(path.type(), std::invalid_argument);
  EXPECT_THROW(path.append(g_here), std::invalid_argument);
  EXPECT_THROW(path.join(g_here), std::invalid_argument);
  EXPECT_THROW(path.basename(), std::invalid_argument);
  EXPECT_THROW(path.dirname(), std::invalid_argument);
  EXPECT_THROW(g_here.append(path), std::invalid_argument);
  EXPECT_THROW(g_here.join(path), std::invalid_argument);

  // Once a real path is moved into it, all should be fine.
  path = g_here;
  EXPECT_EQ(path, g_here);
  EXPECT_TRUE(path.is_directory());
  EXPECT_FALSE(path.is_regular());
}

TEST(TestFilesystem, TestDirectory) {
  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());

  {
    // These are the files in the "data" directory in the test
    // directory. Please update it if you add more files.
    //
    // TODO(Mats): Do not use the data directory for this but create a
    // dedicated directory for testing this feature.
    Directory directory{Path(test_data_dir).join("logger.d")};
    std::vector<Path> expect{
        Path(test_data_dir).join("logger.d/one.cfg"),
        Path(test_data_dir).join("logger.d/magic.cfg"),
        Path(test_data_dir).join("logger.d/default.cfg"),
    };

    decltype(expect) result(directory.begin(), directory.end());
    EXPECT_SETEQ(expect, result);
  }

  {
    // These are files in the "data" directory in the test
    // directory. Please update it if you add more files.
    Directory directory{Path(test_data_dir)};
    std::vector<Path> expect{
        Path(test_data_dir).join("tests-bad-1.cfg"),
        Path(test_data_dir).join("tests-bad-2.cfg"),
        Path(test_data_dir).join("tests-bad-3.cfg"),
    };

    decltype(expect) result(directory.glob("tests-bad*.cfg"), directory.end());
    EXPECT_SETEQ(expect, result);
  }
}

TEST(TestFilesystem, list_recursive_empty) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};
  const auto &result = test.list_recursive();
  EXPECT_EQ(result.size(), 0);
}

TEST(TestFilesystem, list_recursive_empty_directories) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  mysql_harness::mkdir(Path{dir_name}.join("x").c_str(), 0700);
  mysql_harness::mkdir(Path{dir_name}.join("y").c_str(), 0700);
  Directory test{dir_name};

  const auto &result = test.list_recursive();
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result, ::testing::UnorderedElementsAre(Path{"x"}, Path{"y"}));
}

TEST(TestFilesystem, list_recursive_only_files) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};

  std::ofstream file1(Path{dir_name}.join("f1").str());
  std::ofstream file2(Path{dir_name}.join("f2").str());
  std::ofstream file3(Path{dir_name}.join("f3").str());

  const auto &result = test.list_recursive();
  EXPECT_EQ(result.size(), 3);
  EXPECT_THAT(result, ::testing::UnorderedElementsAre(Path{"f1"}, Path{"f2"},
                                                      Path{"f3"}));
}

TEST(TestFilesystem, list_recursive_multiple_levels) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};

  mysql_harness::mkdir(Path{dir_name}.join("x").c_str(), 0700);
  mysql_harness::mkdir(Path{dir_name}.join("x").join("x2").c_str(), 0700);
  mysql_harness::mkdir(Path{dir_name}.join("y").c_str(), 0700);
  mysql_harness::mkdir(Path{dir_name}.join("z").c_str(), 0700);
  std::ofstream file1(Path{dir_name}.join("x").join("x2").join("xf").str());
  std::ofstream file2(Path{dir_name}.join("f").str());
  std::ofstream file3(Path{dir_name}.join("z").join("zf1").str());
  std::ofstream file4(Path{dir_name}.join("z").join("zf2").str());
  std::ofstream file5(Path{dir_name}.join("z").join("zf3").str());

  const auto &result = test.list_recursive();
  EXPECT_EQ(result.size(), 6);
  EXPECT_THAT(result, ::testing::UnorderedElementsAre(
                          Path{"y"}, Path{"z"}.join("zf1"),
                          Path{"z"}.join("zf2"), Path{"z"}.join("zf3"),
                          Path{"x"}.join("x2").join("xf"), Path{"f"}));
}

TEST(TestFilesystem, is_empty_true) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};
  EXPECT_TRUE(test.is_empty());
}

TEST(TestFilesystem, is_empty_dir_with_empty_subdir) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};
  mysql_harness::mkdir(Path{dir_name}.join("foo").c_str(), 0700);

  EXPECT_FALSE(test.is_empty());
}

TEST(TestFilesystem, is_empty_dir_with_file) {
  TempDirectory tmpdir("tmp");
  const std::string dir_name = tmpdir.name();

  Directory test{dir_name};
  std::ofstream file(Path{dir_name}.join("bar").str());

  EXPECT_FALSE(test.is_empty());
}

// unfortunately it's not (reasonably) possible to make folders read-only on
// Windows, therefore we can run the following 2 tests only on Unix
// https://support.microsoft.com/en-us/help/326549/you-cannot-view-or-change-the-read-only-or-the-system-attributes-of-fo
TEST(TestFilesystem, IsReadableIfFileCanBeRead) {
#ifndef _WIN32

  // create temporary file
  TempDirectory tmpdir("tmp");
  const std::string directory = tmpdir.name();

  mysql_harness::Path path = mysql_harness::Path(directory).join("/tmp_file");
  std::ofstream file(path.str());

  if (!file.good())
    throw(std::runtime_error("Could not create file " + path.str()));

  // make file readable
  chmod(path.c_str(), S_IRUSR);
  ASSERT_TRUE(path.is_readable());
#endif
}

TEST(TestFilesystem, IsNotReadableIfFileCanNotBeRead) {
#ifndef _WIN32

  // create temporary file
  TempDirectory tmpdir("tmp");
  const std::string directory = tmpdir.name();

  mysql_harness::Path path = mysql_harness::Path(directory).join("/tmp_file");
  std::ofstream file(path.str());

  if (!file.good())
    throw(std::runtime_error("Could not create file " + path.str()));

  // make file readable
  chmod(path.c_str(), S_IWUSR | S_IXUSR);
  ASSERT_FALSE(path.is_readable());
#endif
}

TEST(TestFilesystem, delete_dir_recursive) {
  using mysql_harness::mkdir;
  std::ofstream ofs;
  mkdir("testdir", 0700);
  mkdir("testdir/a", 0700);
  mkdir("testdir/a/b", 0700);
  mkdir("testdir/a/a", 0700);
  std::ofstream().open("testdir/f");
  std::ofstream().open("testdir/f2");
  std::ofstream().open("testdir/a/f");
  std::ofstream().open("testdir/a/b/f");
  EXPECT_NO_ERROR(mysql_harness::delete_dir_recursive("testdir"));
}

/*
 * Tests mysql_harness::mkdir()
 */

TEST(TestFilesystem, Mkdir) {
  constexpr auto kMode = 0700;

  TempDirectory tmpdir("tmp");
  const std::string tmp_dir = tmpdir.name();

  // non-recursive should fail
  EXPECT_NE(0, mysql_harness::mkdir(tmp_dir + "/a/b/c/d", kMode));

  // recursive should be fine
  EXPECT_EQ(0, mysql_harness::mkdir(tmp_dir + "/a/b/c/d", kMode, true));

  // make sure it really exists
  mysql_harness::Path my_path(tmp_dir + "/a/b/c/d");
  EXPECT_TRUE(my_path.exists());

  // we just created one, trying to recursively create it once more
  // should succeed as 'mkdir -p' does
  EXPECT_EQ(0, mysql_harness::mkdir(tmp_dir + "/a/b/c/d", kMode, true));

  // create a regular file and try to create a directory with the same name,
  // that should fail
  {
    mysql_harness::Path file_path(tmp_dir + "/a/b/c/regular_file");
    std::fstream f;
    f.open(file_path.str(), std::ios::out);
  }
  EXPECT_NE(0,
            mysql_harness::mkdir(tmp_dir + "/a/b/c/regular_file", kMode, true));

  // empty path should throw
  EXPECT_THROW(mysql_harness::mkdir("", kMode, true), std::invalid_argument);
}

/*
 * Tests mysql_harness::mkdir()
 */
TEST(TestFilesystem, get_tmp_dir_fail) {
  try {
    auto tmp_dir = mysql_harness::get_tmp_dir("/no/such/directory/test");

    FAIL() << "expected get_tmp_dir() to fail";
  } catch (const std::system_error &) {
    SUCCEED();
  } catch (const std::exception &e) {
    FAIL() << "expected std::system_error, got " << e.what();
  }
}

/*
 * Tests mysql_harness::mkdir()
 */
TEST(TestFilesystem, TempDirectory_Constructor_fail) {
  try {
    auto tmp_dir = TempDirectory("/no/such/directory/test");

    FAIL() << "expected TempDirectyory() to fail";
  } catch (const std::system_error &) {
    SUCCEED();
  } catch (const std::exception &e) {
    FAIL() << "expected std::system_error, got " << e.what();
  }
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
