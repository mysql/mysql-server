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

////////////////////////////////////////
// Standard include files
#include <algorithm>  // std::sort
#include <array>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::bad_option;
using mysql_harness::bad_section;
using mysql_harness::Config;
using mysql_harness::ConfigSection;
using mysql_harness::Path;
using mysql_harness::syntax_error;

using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using testing::TestWithParam;
using testing::UnorderedElementsAreArray;
using testing::ValuesIn;

namespace mysql_harness {

bool operator==(const Config &lhs, const Config &rhs) {
  // We just check the section names to start with
  auto &&lhs_names = lhs.section_names();
  auto &&rhs_names = rhs.section_names();

  // Check if the sizes differ. This is not an optimization since
  // std::equal does not work properly on ranges of unequal size.
  if (lhs_names.size() != rhs_names.size()) return false;

  // Put the lists in vectors and sort them
  std::vector<std::pair<std::string, std::string>> lhs_vec(lhs_names.begin(),
                                                           lhs_names.end());
  std::sort(lhs_vec.begin(), lhs_vec.end());

  std::vector<std::pair<std::string, std::string>> rhs_vec(rhs_names.begin(),
                                                           rhs_names.end());
  std::sort(rhs_vec.begin(), rhs_vec.end());

  // Compare the elements of the sorted vectors
  return std::equal(lhs_vec.begin(), lhs_vec.end(), rhs_vec.begin());
}

}  // namespace mysql_harness

std::list<std::string> section_names(
    const mysql_harness::Config::ConstSectionList &sections) {
  std::list<std::string> result;
  for (const auto *section : sections) result.push_back(section->name);
  return result;
}

void PrintTo(const Config &config, std::ostream &out) {
  for (auto &&val : config.section_names())
    out << val.first << ":" << val.second << " ";
}

class ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<std::string> words;
    words.emplace_back("reserved");
    config.set_reserved(words);
  }

  Config config;
};

Path g_here;

TEST_F(ConfigTest, TestEmpty) {
  EXPECT_TRUE(config.is_reserved("reserved"));
  EXPECT_FALSE(config.is_reserved("legal"));

  // A newly created configuration is always empty.
  EXPECT_TRUE(config.empty());

  // Test that fetching a non-existing section throws an exception.
  EXPECT_THROW(config.get("magic"), std::runtime_error);

  EXPECT_FALSE(config.has("magic"));
}

TEST_F(ConfigTest, SetGetTest) {
  // Add the section
  config.add("magic");

  // Test that fetching a section get the right section back.
  EXPECT_TRUE(config.has("magic"));

  Config::SectionList sections = config.get("magic");
  EXPECT_EQ(1U, sections.size());

  ConfigSection *section = sections.front();
  EXPECT_EQ("magic", section->name);

  // Test that fetching a non-existing option in a section throws a
  // run-time exception.
  EXPECT_THROW(section->get("my_option"), std::runtime_error);

  // Set the value of the option in the section
  section->set("my_option", "my_value");

  // Check that the value can be retrieved.
  EXPECT_EQ("my_value", section->get("my_option"));

  config.clear();
  EXPECT_TRUE(config.empty());
}

TEST_F(ConfigTest, RemoveTest) {
  constexpr const char *section_name = "my_section_name";
  constexpr const char *section_key = "my_section_key";

  // config without section key
  {
    Config conf;

    // add a section with some key/value pair
    conf.add(section_name);
    conf.get(section_name).front()->set("my_option", "my_value");
    EXPECT_STREQ("my_value",
                 conf.get(section_name).front()->get("my_option").c_str());

    // removing non-existent section should be a no-op, but return false
    EXPECT_FALSE(conf.remove("no_such_section", "no_such_key"));
    EXPECT_FALSE(conf.remove("no_such_section", ""));
    EXPECT_FALSE(conf.remove("no_such_section"));
    EXPECT_FALSE(conf.remove(section_name, "no_such_key"));

    // removing existing section should return true
    EXPECT_TRUE(conf.remove(section_name));
    EXPECT_FALSE(conf.remove(section_name));  // no-op again

    // other tests proving the section got removed
    EXPECT_TRUE(conf.empty());
  }

  // config with section key
  {
    Config conf(mysql_harness::Config::allow_keys);

    // add a section with some key/value pair
    conf.add(section_name, section_key);
    conf.get(section_name, section_key).set("my_option", "my_value");
    EXPECT_STREQ("my_value",
                 conf.get(section_name, section_key).get("my_option").c_str());

    // removing non-existent section should be a no-op, but return false
    EXPECT_FALSE(conf.remove("no_such_section", section_key));
    EXPECT_FALSE(conf.remove("no_such_section", "no_such_key"));
    EXPECT_FALSE(conf.remove("no_such_section", ""));
    EXPECT_FALSE(conf.remove("no_such_section"));
    EXPECT_FALSE(conf.remove(section_name, "no_such_key"));
    EXPECT_FALSE(conf.remove(section_name, ""));
    EXPECT_FALSE(conf.remove(section_name));

    // removing existing section should return true
    EXPECT_TRUE(conf.remove(section_name, section_key));
    EXPECT_FALSE(conf.remove(section_name, section_key));  // no-op again

    // other tests proving the section got removed
    EXPECT_TRUE(conf.empty());
  }
}

TEST_F(ConfigTest, IsEmptyStringWhenOptionNotInSection) {
  config.add("section_name");
  Config::SectionList sections = config.get("section_name");
  ConfigSection *section = sections.front();
  ASSERT_THAT(section->get_section_name("option_name"), testing::Eq(""));
  config.clear();
  EXPECT_TRUE(config.empty());
}

TEST_F(ConfigTest, IsCurrentSectionWhenOptionInCurrentSection) {
  config.add("section_name");
  Config::SectionList sections = config.get("section_name");
  ConfigSection *section = sections.front();
  section->set("option_name", "value");
  ASSERT_THAT(section->get_section_name("option_name"),
              testing::Eq("section_name"));
  config.clear();
  EXPECT_TRUE(config.empty());
}

TEST_F(ConfigTest, IsDefaultWhenOptionInDefault) {
  std::stringstream c;
  c << "[DEFAULT]\ndefault_option=0\n[section_name_1]\noption_1=value_"
       "1\noption_2=value_2\noption_3=value_3\n";
  config.read(c);
  Config::SectionList sections = config.get("section_name_1");
  ConfigSection *section = sections.front();
  ASSERT_THAT(section->get_section_name("default_option"),
              testing::Eq("default"));
  config.clear();
  EXPECT_TRUE(config.empty());
}

class GoodParseTestAllowKey : public ::testing::TestWithParam<const char *> {
 protected:
  void SetUp() override {
    config = new Config(Config::allow_keys);

    std::vector<std::string> words;
    words.emplace_back("reserved");
    config->set_reserved(words);

    std::istringstream input(GetParam());
    config->read(input);
  }

  void TearDown() override {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(GoodParseTestAllowKey, SectionOne) {
  // Checking that getting a non-existent section throws exception
  EXPECT_THROW(config->get("nonexistant-section"), bad_section);

  Config::SectionList sections = config->get("one");
  EXPECT_EQ(1U, sections.size());

  ConfigSection *section = sections.front();
  EXPECT_EQ("one", section->name);
  EXPECT_EQ("bar", section->get("foo"));

  // Checking that getting a non-existient option in an existing
  // section throws exception.
  EXPECT_THROW(section->get("nonexistant-option"), bad_option);
}

const char *good_examples[] = {("[one]\n"
                                "foo = bar\n"),
                               ("[one]\n"
                                "foo: bar\n"),
                               (" [one]   \n"
                                "  foo: bar   \n"),
                               (" [one]\n"
                                "  foo   :bar   \n"),
                               ("# Hello\n"
                                " [one]\n"
                                "  foo   :bar   \n"),
                               ("# Hello\n"
                                "# World!\n"
                                " [one]\n"
                                "  foo   :bar   \n"),
                               ("; Hello\n"
                                " [one]\n"
                                "  foo   :bar   \n"),
                               ("[DEFAULT]\n"
                                "foo = bar\n"
                                "[one]\n"),
                               ("[DEFAULT]\n"
                                "other = ar\n"
                                "[one]\n"
                                "foo = b{other}\n"),
                               ("[DEFAULT]\n"
                                "one = b\n"
                                "two = r\n"
                                "[one]\n"
                                "foo = {one}a{two}\n"),
                               ("[DEFAULT]\n"
                                "one = b\n"
                                "two = r\n"
                                "[one:my_key]\n"
                                "foo = {one}a{two}\n")};

INSTANTIATE_TEST_SUITE_P(TestParsing, GoodParseTestAllowKey,
                         ::testing::ValuesIn(good_examples));

// Test fixture to compare option value with the result of
// interpolating the value.
using Sample = std::pair<std::string, std::string>;
class TestInterpolate : public TestWithParam<Sample> {
 protected:
  void SetUp() override {
    config_ = new Config(Config::allow_keys);
    config_->add("testing", "a_key");
    config_->set_default("datadir", "--path--");
  }

  void TearDown() override {
    delete config_;
    config_ = nullptr;
  }

  Config *config_;
};

TEST_P(TestInterpolate, CheckExpected) {
  auto value = std::get<0>(GetParam());
  auto expect = std::get<1>(GetParam());

  auto &&section = config_->get("testing", "a_key");
  section.set("option_name", value);
  EXPECT_THAT(section.get("option_name"), Eq(expect));
}

const Sample interpolate_examples[] = {
    {"foo", "foo"},
    {R"(c:\foo\bar\{datadir})", R"(c:\foo\bar\--path--)"},
    {R"(c:\foo\bar\{undefined})", R"(c:\foo\bar\{undefined})"},
    {"{datadir}\\foo", "--path--\\foo"},
    {"{datadir}", "--path--"},
    {"foo{datadir}bar", "foo--path--bar"},
    {"{{datadir}}", "{--path--}"},
    {"{datadir}}", "--path--}"},
    {"{{datadir}", "{--path--"},
    {"{{{datadir}}}", "{{--path--}}"},
    {"{datadir", "{datadir"},
    {R"(c:\foo\bar\{425432-5425432-5423534253-542342})",
     R"(c:\foo\bar\{425432-5425432-5423534253-542342})"},
};

INSTANTIATE_TEST_SUITE_P(TestParsing, TestInterpolate,
                         ValuesIn(interpolate_examples));

TEST(TestConfig, RecursiveInterpolate) {
  const char *const config_text{
      "[DEFAULT]\n"
      "basedir = /root/dir\n"
      "datadir = {basedir}/data\n"

      "[one]\n"
      "log = {datadir}/router.log\n"
      "rec = {other}\n"  // Recursive reference
      "other = {rec}\n"};

  Config config(Config::allow_keys);
  std::istringstream input(config_text);
  config.read(input);

  auto &&section = config.get("one", "");
  EXPECT_THAT(section.get("log"), Eq("/root/dir/data/router.log"));
  EXPECT_THROW(section.get("rec"), syntax_error);
}

class BadParseTestForbidKey : public ::testing::TestWithParam<const char *> {
 protected:
  void SetUp() override {
    config = new Config;

    std::vector<std::string> words;
    words.emplace_back("reserved");
    config->set_reserved(words);
  }

  void TearDown() override {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(BadParseTestForbidKey, SyntaxError) {
  std::istringstream input{GetParam()};
  EXPECT_ANY_THROW(config->read(input));
}

static const char *syntax_problems[] = {
    // Unterminated section header line
    ("[one\n"
     "foo = bar\n"),

    // Malformed start of a section
    ("one]\n"
     "foo: bar\n"),

    // Bad section name
    ("[one]\n"
     "foo = bar\n"
     "[reserved]\n"
     "foo = baz\n"),

    // Options before first section
    ("  foo: bar   \n"
     "[one]\n"),

    // Repeated option
    ("[one]\n"
     "foo = bar\n"
     "foo = baz\n"),
    ("[one]\n"
     "foo = bar\n"
     "Foo = baz\n"),

    // Space in option
    ("[one]\n"
     "foo bar = bar\n"
     "bar = baz\n"),

    // Repeated section
    ("[one]\n"
     "foo = bar\n"
     "[one]\n"
     "foo = baz\n"),
    ("[one]\n"
     "foo = bar\n"
     "[ONE]\n"
     "foo = baz\n"),

    // Key but keys not allowed
    ("[one:my_key]\n"
     "foo = bar\n"
     "[two]\n"
     "foo = baz\n"),
};

INSTANTIATE_TEST_SUITE_P(TestParsingSyntaxError, BadParseTestForbidKey,
                         ::testing::ValuesIn(syntax_problems));

class BadParseTestAllowKeys : public ::testing::TestWithParam<const char *> {
 protected:
  void SetUp() override {
    config = new Config(Config::allow_keys);

    std::vector<std::string> words;
    words.emplace_back("reserved");
    config->set_reserved(words);
  }

  void TearDown() override {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(BadParseTestAllowKeys, SemanticError) {
  std::istringstream input{GetParam()};
  EXPECT_THROW(config->read(input), syntax_error);
}

static const char *semantic_problems[] = {
    // Empty key
    ("[one:]\n"
     "foo = bar\n"
     "[two]\n"
     "foo = baz\n"),

    // Key on default section
    ("[DEFAULT:key]\n"
     "one = b\n"
     "two = r\n"
     "[one:key1]\n"
     "foo = {one}a{two}\n"
     "[one:key2]\n"
     "foo = {one}a{two}\n"),
};

INSTANTIATE_TEST_SUITE_P(TestParseErrorAllowKeys, BadParseTestAllowKeys,
                         ::testing::ValuesIn(semantic_problems));

TEST(TestConfig, ConfigUpdate) {
  const char *const configs[]{
      ("[one]\n"
       "one = first\n"
       "two = second\n"),
      ("[one]\n"
       "one = new first\n"
       "[two]\n"
       "one = first\n"),
  };

  Config config(Config::allow_keys);
  std::istringstream input(configs[0]);
  config.read(input);

  Config other(Config::allow_keys);
  std::istringstream other_input(configs[1]);
  other.read(other_input);

  Config expected(Config::allow_keys);
  config.update(other);

  ConfigSection &one = config.get("one", "");
  ConfigSection &two = config.get("two", "");
  EXPECT_EQ("new first", one.get("one"));
  EXPECT_EQ("second", one.get("two"));
  EXPECT_EQ("first", two.get("one"));

  // Non-existent options should still throw an exception
  auto &&section = config.get("one", "");
  EXPECT_THROW(section.get("nonexistant-option"), bad_option);

  // Check that merging sections with mismatching names generates an
  // exception
  EXPECT_THROW(one.update(two), bad_section);
}

TEST(TestConfig, ConfigReadBasic) {
  // Here are three different sources of configurations that should
  // all be identical. One is a single file, one is a directory, and
  // one is a stream.
  Config dir_config = Config(Config::allow_keys);
  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());
  dir_config.read(Path(test_data_dir).join("logger.d"), "*.cfg");

  Config file_config = Config(Config::allow_keys);
  file_config.read(Path(test_data_dir).join("logger.cfg"));

  const char *const config_string =
      ("[DEFAULT]\n"
       "logging_folder = var/log\n"
       "config_folder = etc\n"
       "plugin_folder = var/lib\n"
       "runtime_folder = var/run\n"
       "[example]\n"
       "library = example\n"
       "[magic]\n"
       "library = magic\n"
       "message = Some kind of\n");

  Config stream_config(Config::allow_keys);
  std::istringstream stream_input(config_string);
  stream_config.read(stream_input);

  EXPECT_EQ(dir_config, file_config);
  EXPECT_EQ(dir_config, stream_config);
  EXPECT_EQ(file_config, stream_config);
}

// Here we test that reads of configuration entries overwrite previous
// read entries.
TEST(TestConfig, ConfigReadOverwrite) {
  Config config = Config(Config::allow_keys);
  std::string test_data_dir = mysql_harness::get_tests_data_dir(g_here.str());
  config.read(Path(test_data_dir).join("logger.d"), "*.cfg");
  EXPECT_EQ("Some kind of", config.get("magic", "").get("message"));

  // Non-existent options should still throw an exception
  {
    auto &&section = config.get("magic", "");
    EXPECT_THROW(section.get("not-in-section"), bad_option);
  }

  config.read(Path(test_data_dir).join("magic-alt.cfg"));
  EXPECT_EQ("Another message", config.get("magic", "").get("message"));

  // Non-existent options should still throw an exception
  {
    auto &&section = config.get("magic", "");
    EXPECT_THROW(section.get("not-in-section"), bad_option);
  }
}

TEST(TestConfig, SectionRead) {
  static const char *const config_string =
      ("[DEFAULT]\n"
       "logging_folder = var/log\n"
       "config_folder = etc\n"
       "plugin_folder = var/lib\n"
       "runtime_folder = var/run\n"
       "[empty]\n"
       "[example]\n"
       "library = magic\n"
       "message = Some kind of\n");

  Config config(Config::allow_keys);
  std::istringstream stream_input(config_string);
  config.read(stream_input);

  // Test that the sections command return the right sections
  EXPECT_THAT(section_names(config.sections()),
              UnorderedElementsAreArray({"example", "empty"}));

  // Test that options for a section is correct
  std::set<std::pair<std::string, std::string>> expected_options{
      {"library", "magic"}, {"message", "Some kind of"}};

  // ElementsAreArray() segfaults with Sun Studio compiler
  //  EXPECT_THAT(config.get("example", "").get_options(),
  //              ElementsAreArray(expected_options));
  auto config_options = config.get("example", "").get_options();
  for (const auto &op : config_options) {
    EXPECT_EQ(1u, expected_options.count(op));
  }
  EXPECT_THAT(config_options, SizeIs(2));

  EXPECT_THAT(config.get("empty", "").get_options(), IsEmpty());
  EXPECT_THAT(config.get("empty", "").get_options(), SizeIs(0));
}

TEST(TestConfig, CrLf) {
  static const char *const config_string =
      ("[example]\r\n"
       "library = magic\r\n"
       "message = Some kind of\r\n");

  Config config(Config::allow_keys);
  std::istringstream stream_input(config_string);
  config.read(stream_input);

  // Test that the sections command return the right sections
  EXPECT_THAT(section_names(config.sections()),
              UnorderedElementsAreArray({"example"}));

  // Test that options for a section is correct
  std::set<std::pair<std::string, std::string>> expected_options{
      {"library", "magic"}, {"message", "Some kind of"}};

  // ElementsAreArray() segfaults with Sun Studio compiler
  //  EXPECT_THAT(config.get("example", "").get_options(),
  //              ElementsAreArray(expected_options));
  auto config_options = config.get("example", "").get_options();
  for (const auto &op : config_options) {
    EXPECT_EQ(1u, expected_options.count(op));
  }
  EXPECT_THAT(config_options, SizeIs(2));
}

TEST(TestConfig, CrLfUnterminatedLastLine) {
  static const char *const config_string =
      R"([example]
library = magic
message = Some kind of)";

  Config config(Config::allow_keys);
  std::istringstream stream_input(config_string);
  config.read(stream_input);

  // Test that the sections command return the right sections
  EXPECT_THAT(section_names(config.sections()),
              UnorderedElementsAreArray({"example"}));

  // Test that options for a section is correct
  std::set<std::pair<std::string, std::string>> expected_options{
      {"library", "magic"}, {"message", "Some kind of"}};

  // ElementsAreArray() segfaults with Sun Studio compiler
  //  EXPECT_THAT(config.get("example", "").get_options(),
  //              ElementsAreArray(expected_options));
  auto config_options = config.get("example", "").get_options();
  for (const auto &op : config_options) {
    EXPECT_EQ(1u, expected_options.count(op));
  }
  EXPECT_THAT(config_options, SizeIs(2));
}

TEST(TestConfig, ConfigInitialDefaultsOverwritten) {
  const std::map<std::string, std::string> defaults{
      {"a", "B"}, {"c", "D"}, {"e", "F"}};

  const Config::ConfigOverwrites conf_overwrites{
      {{"DEFAULT", ""}, {{"a", "X"}, {"c", "Y"}}}};

  // create a configuration with some initial default, some of them overwritten
  Config config(defaults, 0, conf_overwrites);

  // 'a' and 'c' were overwritten
  EXPECT_STREQ("X", config.get_default("a").c_str());
  EXPECT_STREQ("Y", config.get_default("c").c_str());

  // 'e' should have initial value
  EXPECT_STREQ("F", config.get_default("e").c_str());
}

struct InvalidConfigParam {
  std::string test_name;
  std::string input;
  std::string expected_error_msg;
};

class InvalidConfigTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<InvalidConfigParam> {};

TEST_P(InvalidConfigTest, ensure_fails) {
  Config config(Config::allow_keys);
  std::istringstream stream_input(GetParam().input);
  try {
    config.read(stream_input);
    FAIL() << "expected to throw an exception, did not throw.";
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  } catch (...) {
    FAIL() << "expected a std::exception, caught some other exception";
  }
}

static const std::array<InvalidConfigParam, 4> invalid_config_params = {{
    {"invalid_char_in_section_key", "[example:abc:def]",
     "config-section '[example:abc:def]' contains invalid "
     "character ':' in section key 'abc:def'. Only alpha-numeric "
     "characters and _ are valid."},
    {"empty_section_key", "[example:]",
     "section key in config-section '[example:]' may not be empty."},
    {"empty_section_name", "[:example]",
     "section name in config-section '[:example]' may not be empty."},
    {"invalid_char_in_section_name", "[foo-bar:foo]",
     "config-section '[foo-bar:foo]' contains invalid "
     "character '-' in section name 'foo-bar'. Only alpha-numeric "
     "characters and _ are valid."},
}};

INSTANTIATE_TEST_SUITE_P(Spec, InvalidConfigTest,
                         ::testing::ValuesIn(invalid_config_params),
                         [](auto const &test_params) {
                           return test_params.param.test_name;
                         });

template <class T>
struct InvalidUintOptionValueParam {
  std::string value;
  T min_value;
  T max_value;
  std::string expected_error;
};

class InvalidUint64OptionValueTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          InvalidUintOptionValueParam<uint64_t>> {};

TEST_P(InvalidUint64OptionValueTest, ensure_fails) {
  try {
    auto v = mysql_harness::option_as_uint<uint64_t>(
        GetParam().value, "invalid", GetParam().min_value,
        GetParam().max_value);

    FAIL() << "expected to throw an exception, did not throw. Got " << v;
  } catch (const std::exception &e) {
    EXPECT_EQ(std::string(e.what()), GetParam().expected_error);
  } catch (...) {
    FAIL() << "expected a std::exception, caught some other exception";
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, InvalidUint64OptionValueTest,
                         ::testing::Values(
                             InvalidUintOptionValueParam<uint64_t>{
                                 "AB", 0, std::numeric_limits<int64_t>::max(),
                                 "invalid needs value between 0 and "
                                 "9223372036854775807 inclusive, was "
                                 "'AB'"},
                             InvalidUintOptionValueParam<uint64_t>{
                                 "-1", 0, std::numeric_limits<int64_t>::max(),
                                 "invalid needs value between 0 and "
                                 "9223372036854775807 inclusive, was "
                                 "'-1'"},
                             InvalidUintOptionValueParam<uint64_t>{
                                 "9223372036854775808", 0,
                                 std::numeric_limits<int64_t>::max(),
                                 "invalid needs value between 0 and "
                                 "9223372036854775807 inclusive, was "
                                 "'9223372036854775808'"},
                             InvalidUintOptionValueParam<uint64_t>{
                                 "5", 0, 4,
                                 "invalid needs value between 0 and "
                                 "4 inclusive, was "
                                 "'5'"},
                             InvalidUintOptionValueParam<uint64_t>{
                                 "5", 6, 2000,
                                 "invalid needs value between 6 and "
                                 "2000 inclusive, was "
                                 "'5'"},
                             InvalidUintOptionValueParam<uint64_t>{
                                 "-1", 0, std::numeric_limits<uint64_t>::max(),
                                 "invalid needs value between 0 and "
                                 "18446744073709551615 inclusive, was "
                                 "'-1'"}));

template <class T>
struct ValidUintOptionValueParam {
  std::string value;
  T min_value;
  T max_value;
  T expected_value;
};

class ValidUint64OptionValueTest : public ::testing::Test,
                                   public ::testing::WithParamInterface<
                                       ValidUintOptionValueParam<uint64_t>> {};

TEST_P(ValidUint64OptionValueTest, ensure_ok) {
  try {
    EXPECT_EQ(GetParam().expected_value,
              mysql_harness::option_as_uint<uint64_t>(GetParam().value, "valid",
                                                      GetParam().min_value,
                                                      GetParam().max_value));
  } catch (const std::exception &e) {
    FAIL() << "Unexpected exception: " << e.what();
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ValidUint64OptionValueTest,
    ::testing::Values(
        ValidUintOptionValueParam<uint64_t>{
            "1", 0, std::numeric_limits<int64_t>::max(), 1},
        ValidUintOptionValueParam<uint64_t>{
            "0", 0, std::numeric_limits<int64_t>::max(), 0},
        ValidUintOptionValueParam<uint64_t>{"9223372036854775807", 0,
                                            std::numeric_limits<int64_t>::max(),
                                            9223372036854775807},
        ValidUintOptionValueParam<uint64_t>{
            "18446744073709551615", 0, std::numeric_limits<uint64_t>::max(),
            UINT64_C(18446744073709551615)}));

class InvalidUint8OptionValueTest : public ::testing::Test,
                                    public ::testing::WithParamInterface<
                                        InvalidUintOptionValueParam<uint8_t>> {
};

TEST_P(InvalidUint8OptionValueTest, ensure_fails) {
  try {
    mysql_harness::option_as_uint<uint8_t>(GetParam().value, "invalid",
                                           GetParam().min_value,
                                           GetParam().max_value);

    FAIL() << "expected to throw an exception, did not throw.";
  } catch (const std::exception &e) {
    EXPECT_EQ(std::string(e.what()), GetParam().expected_error);
  } catch (...) {
    FAIL() << "expected a std::exception, caught some other exception";
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, InvalidUint8OptionValueTest,
                         ::testing::Values(
                             InvalidUintOptionValueParam<uint8_t>{
                                 "2", 0, 1,
                                 "invalid needs value between 0 and "
                                 "1 inclusive, was '2'"},
                             InvalidUintOptionValueParam<uint8_t>{
                                 "", 0, 1,
                                 "invalid needs value between 0 and "
                                 "1 inclusive, was ''"},
                             InvalidUintOptionValueParam<uint8_t>{
                                 "-1", 0, 255,
                                 "invalid needs value between 0 and "
                                 "255 inclusive, was '-1'"},
                             InvalidUintOptionValueParam<uint8_t>{
                                 "256", 0, 255,
                                 "invalid needs value between 0 and "
                                 "255 inclusive, was '256'"}));

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
