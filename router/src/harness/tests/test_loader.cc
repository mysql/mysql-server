/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

// must have this first, before #includes that rely on it
#include <gtest/gtest_prod.h>

////////////////////////////////////////
// Standard include files
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"

#include "exception.h"                   // bad_plugin
#include "mysql/harness/string_utils.h"  // split_string

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"  // EXPECT_SECTION_AVAILABLE

using mysql_harness::bad_section;
using mysql_harness::Loader;
using mysql_harness::Path;
using mysql_harness::Plugin;

using testing::UnorderedElementsAre;

Path g_here;

class TestLoader : public Loader {
 public:
  using Loader::Loader;

  const Plugin *load(const std::string &plugin_name) {
    return Loader::load(plugin_name);
  }

  const Plugin *load(const std::string &plugin_name, const std::string &key) {
    return Loader::load(plugin_name, key);
  }
};

class LoaderTest : public ::testing::TestWithParam<const char *> {
 protected:
  void SetUp() override {
    test_data_dir_ = mysql_harness::get_tests_data_dir(g_here.str());

    config_ = std::make_unique<mysql_harness::LoaderConfig>(
        std::map<std::string, std::string>{{
            {"program", "harness"},
            {"prefix", test_data_dir_},
        }},
        std::vector<std::string>(), mysql_harness::Config::allow_keys);
    loader = std::make_unique<TestLoader>("harness", *config_);
  }

  std::unique_ptr<TestLoader> loader;
  std::unique_ptr<mysql_harness::LoaderConfig> config_;
  std::string test_data_dir_;
};

class LoaderReadTest : public LoaderTest {
 protected:
  void SetUp() override {
    LoaderTest::SetUp();
    loader->get_config().read(Path(test_data_dir_).join(GetParam()));
  }
};

TEST_P(LoaderReadTest, Available) {
  auto lst = loader->available();
  EXPECT_EQ(5U, lst.size());

  EXPECT_SECTION_AVAILABLE("routertestplugin_example", loader.get());
  EXPECT_SECTION_AVAILABLE("routertestplugin_magic", loader.get());
}

TEST_P(LoaderReadTest, load_non_existant_fails) {
  // Test that loading something non-existent works
  EXPECT_THROW(loader->load("nonexistant-plugin"), bad_section);
}

TEST_P(LoaderReadTest, load_missing_dep_fails) {
  // Dependent plugin does not exist
  EXPECT_THROW(loader->load("routertestplugin_bad_one"), bad_section);
}

TEST_P(LoaderReadTest, load_wrong_version) {
  // Wrong version of dependent sections
  EXPECT_THROW(loader->load("routertestplugin_bad_two"), bad_plugin);
}

TEST_P(LoaderReadTest, load_example_succeeds) {
  // These should all be OK.
  const Plugin *ext1 = loader->load("routertestplugin_example", "one");
  EXPECT_NE(ext1, nullptr);
  EXPECT_STREQ("An example plugin", ext1->brief);
}

TEST_P(LoaderReadTest, load_example_section_two_succeeds) {
  const Plugin *ext2 = loader->load("routertestplugin_example", "two");
  EXPECT_NE(ext2, nullptr);
  EXPECT_STREQ("An example plugin", ext2->brief);
}

TEST_P(LoaderReadTest, load_magic_succeeds) {
  const Plugin *ext3 = loader->load("routertestplugin_magic");
  EXPECT_NE(ext3, nullptr);
  EXPECT_STREQ("A magic plugin", ext3->brief);
}

static const char *good_cfgs[] = {
    "tests-good-1.cfg",
    "tests-good-2.cfg",
};

INSTANTIATE_TEST_SUITE_P(TestLoaderGood, LoaderReadTest,
                         ::testing::ValuesIn(good_cfgs));

TEST_P(LoaderTest, BadSection) {
  EXPECT_THROW(loader->get_config().read(Path(test_data_dir_).join(GetParam())),
               bad_section);
}

// TODO: this test is fixed in WL#10822
#if 0
TEST(TestStart, StartLogger) {
  std::map<std::string, std::string> params;
  params["program"] = "harness";
  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());
  params["prefix"] = Path(test_data_dir).c_str();

  Loader loader("harness", params);
  loader.read(Path(test_data_dir).join("tests-start-2.cfg"));
  loader.load_all();

  loader.setup_logging();

  std::exception_ptr eptr = loader.init_all();
  ASSERT_FALSE(eptr);

  // Check that all plugins have a module registered with the logger.
  auto loggers = get_logger_names();
  EXPECT_THAT(loggers, UnorderedElementsAre("main", "magic"));
}
#endif

TEST(TestStart, StartFailure) {
  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());

  mysql_harness::LoaderConfig config(std::map<std::string, std::string>{{
                                         {"program", "harness"},
                                         {"prefix", test_data_dir},
                                     }},
                                     std::vector<std::string>(),
                                     mysql_harness::Config::allow_keys);
  config.read(Path(test_data_dir).join("tests-start-1.cfg"));

  mysql_harness::Loader loader("harness", config);
  try {
    loader.start();
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &exc) {
    EXPECT_STREQ("The suki was bad, please throw away", exc.what());
    return;
  }
  FAIL() << "Did not catch expected exception";
}

const char *bad_cfgs[] = {
    "tests-bad-1.cfg",
    "tests-bad-2.cfg",
    "tests-bad-3.cfg",
};

INSTANTIATE_TEST_SUITE_P(TestLoaderBad, LoaderTest,
                         ::testing::ValuesIn(bad_cfgs));

/*
   @test arch-descriptor has 3 slashes
 * @test arch-descriptor has no empty parts
 */
TEST(TestPlugin, ArchDescriptor) {
  auto parts =
      mysql_harness::split_string(mysql_harness::ARCHITECTURE_DESCRIPTOR, '/');

  EXPECT_THAT(parts, ::testing::SizeIs(4));
  EXPECT_THAT(parts, ::testing::Not(::testing::Contains("")));
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();
  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
