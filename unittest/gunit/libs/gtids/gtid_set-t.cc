// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                      // TEST
#include "../sets/test_decode_prefix.h"       // test_decode_prefix
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/debugging/oom_test.h"         // oom_test_assignable_object
#include "mysql/gtids/gtids.h"                // Gtid
#include "mysql/strconv/strconv.h"            // encode
#include "sql/rpl_gtid.h"                     // Gtid_set
#include "unittest/gunit/test_utils.h"        // Server_initializer

namespace {

// Set to true to enable console output, which compares the sizes of the various
// encoding formats.
constexpr bool verbose = false;

using namespace mysql;

static_assert(gtids::Is_gtid_set<gtids::Gtid_set>);
static_assert(
    std::same_as<
        strconv::detail::Default_format_type<strconv::Text_format, gtids::Gtid>,
        strconv::Gtid_text_format>);
static_assert(std::same_as<decltype(strconv::Gtid_text_format{}.parent()),
                           strconv::Boundary_set_text_format>);

/// Convert `gtid_set` to text and back, using the given format, and expect to
/// get the original set back.
void test_convert(const strconv::Is_format auto &format,
                  [[maybe_unused]] std::string_view format_name,
                  gtids::Is_gtid_set auto &gtid_set) {
  MY_SCOPED_TRACE("format=", format_name);
  std::string str = strconv::throwing::encode(format, gtid_set);
  gtids::Gtid_set back;
  auto ret = strconv::decode(format, str, back);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret) << " '" << str
                           << "'";
  ASSERT_EQ(gtid_set, back);
  if constexpr (verbose) {
    std::cout << "- " << format_name << ": size=" << str.size() << "\n";
  }
}

void test_legacy_gtid_set(const std::string &str, std::string_view expected) {
  // Convert to (legacy) Gtid_set
  Tsid_map tsid_map{nullptr};
  enum_return_status status{};
  Gtid_set legacy_gtid_set(&tsid_map, str.data(), &status, nullptr);
  ASSERT_EQ(status, RETURN_STATUS_OK);

  // Convert back to string
  std::size_t length = legacy_gtid_set.get_string_length();
  char *buf = reinterpret_cast<char *>(malloc(length + 1));
  assert(buf != nullptr);
  legacy_gtid_set.to_string(buf);

  // Assert that the string is as expected
  ASSERT_EQ(std::string_view(buf, length), expected);

  free(buf);
}

void test_legacy_gtid_set_error(const std::string &str) {
  Tsid_map tsid_map{nullptr};
  enum_return_status status{};

  // Make the call to my_error from the parser expect this error
  my_testing::Server_initializer::set_expected_error(
      ER_MALFORMED_GTID_SET_SPECIFICATION);
  Gtid_set legacy_gtid_set(&tsid_map, str.data(), &status, nullptr);
  my_testing::Server_initializer::set_expected_error(0);

  ASSERT_EQ(status, RETURN_STATUS_REPORTED_ERROR);
}

/// Convert `str` to gtids::Gtid_set and back and expect the result to be
/// `expected`.
void test_gtid_set(const std::string &str, const std::string &expected) {
  MY_SCOPED_TRACE("string=", str, " expected=", expected);

  test_legacy_gtid_set(str, expected);

  gtids::Gtid_set gtid_set;
  auto ret = strconv::decode(strconv::Text_format{}, str, gtid_set);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);
  ASSERT_EQ(strconv::throwing::encode_text(gtid_set), expected);

  static constexpr auto v0 = strconv::Gtid_binary_format{
      strconv::Gtid_binary_format::Version_policy::v0_tagless};
  static constexpr auto v1 = strconv::Gtid_binary_format{
      strconv::Gtid_binary_format::Version_policy::v1_tags};
  static constexpr auto v2 = strconv::Gtid_binary_format{
      strconv::Gtid_binary_format::Version_policy::v2_tags_compact};

  if constexpr (verbose) {
    std::cout << "SET: " << strconv::throwing::encode_text(gtid_set) << "\n";
  }
  test_convert(strconv::Text_format{}, "text", gtid_set);
  test_convert(strconv::Binary_format{}, "binary", gtid_set);
  test_convert(strconv::Fixint_binary_format{}, "fixint_binary", gtid_set);
  if (!has_tags(gtid_set)) test_convert(v0, "binary v0", gtid_set);
  test_convert(v1, "binary v1", gtid_set);
  test_convert(v2, "binary v2", gtid_set);
}

void test_gtid_set_error(const std::string &cstr,
                         std::string_view expected_error_message) {
  MY_SCOPED_TRACE("string='", cstr, "' expected_error_message='",
                  expected_error_message, "'");

  test_legacy_gtid_set_error(cstr);

  std::string_view str(cstr);
  gtids::Gtid_set gtid_set;
  auto ret = strconv::decode(strconv::Text_format{}, str, gtid_set);
  ASSERT_FALSE(ret.is_ok());
  ASSERT_EQ(strconv::throwing::encode_text(ret), expected_error_message);
}

void test_gtid_set(const std::string &cstr) { test_gtid_set(cstr, cstr); }

// It's more convenient to concatenate string literals.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define UUID0 "00000000-0000-0000-0000-000000000000"
#define UUID1 "00000000-0000-0000-0000-000000000001"
#define UUID2 "63d02e6c-9555-4d16-a8c3-e9d7590103b0"
#define UUID3 "ffffffff-ffff-ffff-ffff-ffffffffffff"

#define TAG32 "aaaabbbbccccddddeeeeffffgggghhhh"
#define TAG32a "this_is_a_32_character_long_tag_"
#define TAG32b "yet_another_tag_with_that_length"
#define TAG10 "a123456789"
// NOLINTEND(cppcoreguidelines-macro-usage)

const std::string &sequence_number_max_inclusive_str() {
  static std::string ret =
      strconv::throwing::encode_text(std::numeric_limits<int64_t>::max() - 1);
  return ret;
}

const std::string &sequence_number_max_exclusive_str() {
  static std::string ret =
      strconv::throwing::encode_text(std::numeric_limits<int64_t>::max());
  return ret;
}

/// Test Gtid parsing, formatting, and comparison.
TEST(LibsGtidsGtidSet, Parsing) {
  // ==== Empty set ===

  // just whitespace and separators
  test_gtid_set("");
  test_gtid_set(" ", "");
  test_gtid_set("\r,, ,\t", "");

  // uuids but no intervals
  test_gtid_set(UUID0, "");
  test_gtid_set(UUID0 " ", "");
  test_gtid_set(" " UUID0 " , " UUID1 ",," UUID0, "");

  // uuids and tags but no intervals
  test_gtid_set(" " UUID0 " : " TAG10, "");
  test_gtid_set(" " UUID0 " : " TAG10 ":" TAG10 " : " TAG32, "");
  test_gtid_set(" " UUID0 "," UUID0 " : " TAG10 ":" TAG10 " : " TAG32, "");
  test_gtid_set(" " UUID0 " : " TAG10 ":" TAG10 " : " TAG32 "," UUID0, "");

  // uuids and negative intervals
  test_gtid_set(UUID0 ":2-1", "");
  test_gtid_set(" " UUID0 " : 99 - 1 ", "");
  test_gtid_set(UUID0 ":2-1," UUID1 ":" TAG10 ":99-7", "");

  // uuids and tags and negative intervals
  test_gtid_set(UUID0 ":" TAG10 ":2-1", "");
  test_gtid_set(" " UUID0 " : 99 - 1 : " TAG10 ":" TAG10, "");
  test_gtid_set(UUID0 ":2-1," UUID1 ":" TAG10 ":99-7", "");

  // ==== Sets containing one GTID ====

  // simple gtids
  test_gtid_set(UUID0 ":1");
  test_gtid_set(UUID1 ":999");
  test_gtid_set(std::string(UUID2 ":") + sequence_number_max_inclusive_str());

  // simple gtids, expressed as intervals with the same beginning and end
  test_gtid_set(UUID0 ":1-1", UUID0 ":1");
  test_gtid_set(UUID1 ":999-999", UUID1 ":999");
  test_gtid_set(std::string(UUID2 ":") + sequence_number_max_inclusive_str() +
                    "-" + sequence_number_max_inclusive_str(),
                std::string(UUID2 ":") + sequence_number_max_inclusive_str());

  // simple gtids, expressed with multiple redundant intervals
  test_gtid_set(UUID0 ":1-1:1", UUID0 ":1");
  test_gtid_set(UUID1 ":999-999:999", UUID1 ":999");
  test_gtid_set(std::string(UUID2 ":") + sequence_number_max_inclusive_str() +
                    "-" + sequence_number_max_inclusive_str() + ":" +
                    sequence_number_max_inclusive_str(),
                std::string(UUID2 ":") + sequence_number_max_inclusive_str());

  // gtids with tags
  test_gtid_set(UUID3 ":foo:1");
  test_gtid_set(UUID0 ":" TAG10 ":42");
  test_gtid_set(UUID0 ":" TAG32 ":42");

  // gtids with tags, expressed with multiple redundant intervals
  test_gtid_set(UUID3 ":foo:1-1:1", UUID3 ":foo:1");
  test_gtid_set(UUID0 ":" TAG10 ":42-42:42", UUID0 ":" TAG10 ":42");
  test_gtid_set(UUID0 ":" TAG32 ":42-42:42", UUID0 ":" TAG32 ":42");

  // variations in case and spacing
  test_gtid_set(" " UUID1 ":1", UUID1 ":1");
  test_gtid_set(UUID3 " : 1 ", UUID3 ":1");
  test_gtid_set(" " UUID0 " : 1 ", UUID0 ":1");
  test_gtid_set("\f\f\n" UUID3 "\t \t:\n1\r", UUID3 ":1");

  // ==== sets containing one UUID and multiple GTIDs ====

  // simple gtids, one range
  test_gtid_set(UUID0 ":1-2");
  test_gtid_set(UUID1 ":999-1000");
  test_gtid_set(std::string(UUID2 ":1-") + sequence_number_max_inclusive_str());

  // simple gtids, multiple disjoint ranges
  test_gtid_set(UUID0 ":1-2:9-100");
  test_gtid_set(UUID1 ":999-1000:2000-3000:4000");
  test_gtid_set(std::string(UUID2 ":1-2:4-") +
                sequence_number_max_inclusive_str());

  // simple gtids, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":1-100:50-200:150-300", UUID0 ":1-300");
  test_gtid_set(UUID1 ":9:7:5:3:1", UUID1 ":1:3:5:7:9");
  test_gtid_set(UUID1 ":80-100:50-59:60-79", UUID1 ":50-100");
  test_gtid_set(
      std::string(UUID2 ":100-200:1-") + sequence_number_max_inclusive_str(),
      std::string(UUID2 ":1-") + sequence_number_max_inclusive_str());

  // gtids with one tag, one range
  test_gtid_set(UUID0 ":foo:1-2");
  test_gtid_set(UUID1 ":bar:999-1000");
  test_gtid_set(std::string(UUID2 ":baz:1-") +
                sequence_number_max_inclusive_str());

  // gtids with one tag, multiple disjoint ranges
  test_gtid_set(UUID0 ":foo:1-2:9-100");
  test_gtid_set(UUID1 ":bar:999-1000:2000-3000:4000");
  test_gtid_set(std::string(UUID2 ":baz:1-2:4-") +
                sequence_number_max_inclusive_str());

  // gtids with one tag, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":foo:1-100:50-200:150-300", UUID0 ":foo:1-300");
  test_gtid_set(UUID1 ":foo:9:7:5:3:1", UUID1 ":foo:1:3:5:7:9");
  test_gtid_set(UUID1 ":foo:80-100:50-59:60-79", UUID1 ":foo:50-100");
  test_gtid_set(
      std::string(UUID2 ":foo:100-200:1-") +
          sequence_number_max_inclusive_str(),
      std::string(UUID2 ":foo:1-") + sequence_number_max_inclusive_str());

  // gtids with multiple tags, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":a:1-2:999-1000:foo:1-2:100-2000");
  test_gtid_set(UUID1 ":a:1-2:999-1000:foo:1-100:bar:1-2000",
                UUID1 ":a:1-2:999-1000:bar:1-2000:foo:1-100");
  test_gtid_set(UUID1
                ":a:1-2:999-1000:foo:20-100:bar:1-2000:foo:101-200:foo:1-25",
                UUID1 ":a:1-2:999-1000:bar:1-2000:foo:1-200");

  // gtids with and without tags, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":1-2:999-1000:foo:1-2:100-2000");
  test_gtid_set(UUID1 ":1-2:999-1000:foo:1-100:bar:1-2000",
                UUID1 ":1-2:999-1000:bar:1-2000:foo:1-100");
  test_gtid_set(UUID1
                ":1-2:999-1000:foo:20-100:bar:1-2000:foo:101-200:foo:1-25",
                UUID1 ":1-2:999-1000:bar:1-2000:foo:1-200");

  // ==== sets containing multiple UUIDs ====

  // simple gtids, multiple disjoint ranges
  test_gtid_set(UUID0 ":1-2:9-100,\n" UUID1 ":1");
  test_gtid_set(UUID1 ":999-1000:2000-3000:4000,\n" UUID2 ":1-100:200-5000");

  // simple gtids, repeated UUIDs, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":1-100:50-200,\n" UUID0 ":150-300,\n", UUID0 ":1-300");
  test_gtid_set(UUID1 ":9:7:5:3:1,\n" UUID1 ":2:4:6:8,\n", UUID1 ":1-9");
  test_gtid_set(UUID1 ":80-100,\n" UUID3 ":20-70,\n" UUID1 ":50-59:60-79",
                UUID1 ":50-100,\n" UUID3 ":20-70");

  // gtids with multiple and repeated UUIDs, multiple tags, overlapping and
  // out-of-order ranges
  test_gtid_set(UUID0 ":a:1-2:999-1000:foo:1-2:100-2000,\n" UUID1
                      ":a:2-3:1000-1001:bar:4-9");
  test_gtid_set(UUID1 ":a:1-2:999-1000:baz:foo:1-100:bar:1-2000,\n" UUID2
                      ":a:1-2:999-1000:foo:1-100:baz:bar:1-2000",
                UUID1 ":a:1-2:999-1000:bar:1-2000:foo:1-100,\n" UUID2
                      ":a:1-2:999-1000:bar:1-2000:foo:1-100");
  test_gtid_set(UUID1 ":a:1-2:999-1000:foo:20-100:bar:1-2000,\n" UUID2
                      ":a:1-2:999-1000:foo:1-100:baz:bar:1-2000,\n" UUID1
                      ":baz:foo:101-200:foo:1-25",
                UUID1 ":a:1-2:999-1000:bar:1-2000:foo:1-200,\n" UUID2
                      ":a:1-2:999-1000:bar:1-2000:foo:1-100");

  // gtids with and without tags, overlapping and out-of-order ranges
  test_gtid_set(UUID0 ":1-2:999-1000:foo:1-2:100-2000,\n" UUID1
                      ":2-3:1000-1001:bar:4-9");
  test_gtid_set(UUID1 ":1-2:999-1000:baz:foo:1-100:bar:1-2000,\n" UUID2
                      ":1-2:999-1000:foo:1-100:baz:bar:1-2000",
                UUID1 ":1-2:999-1000:bar:1-2000:foo:1-100,\n" UUID2
                      ":1-2:999-1000:bar:1-2000:foo:1-100");
  test_gtid_set(UUID1 ":1-2:999-1000:foo:20-100:bar:1-2000,\n" UUID2
                      ":1-2:999-1000:foo:1-100:baz:bar:1-2000,\n" UUID1
                      ":baz:foo:101-200:foo:1-25",
                UUID1 ":1-2:999-1000:bar:1-2000:foo:1-200,\n" UUID2
                      ":1-2:999-1000:bar:1-2000:foo:1-100");

  // Same tags repeated for different UUIDs (exercises the mechanism in v2 that
  // stores each tag only once).
  test_gtid_set(UUID0 ":" TAG32 ":1:3:5:" TAG32a ":2:4:6:" TAG32b ":58,\n"    //
                UUID1 ":" TAG32 ":1-10:" TAG32a ":2-19:" TAG32b ":99-999,\n"  //
                UUID2 ":" TAG32 ":4711-9876:" TAG32a ":42:" TAG32b ":16,\n"   //
                UUID3 ":" TAG32 ":78-87:" TAG32a ":1000-1001:" TAG32b ":13:15");
}

/// Test Gtid parsing, formatting, and comparison.
TEST(LibsGtidsGtidSet, ParseErrors) {
  // ==== Invalid characters ====
  test_gtid_set_error(
      ".",
      "Expected at least two hex digits at the beginning of the string: \".\"");
  test_gtid_set_error(
      "/",
      "Expected at least two hex digits at the beginning of the string: \"/\"");
  test_gtid_set_error(UUID0 "!",
                      "Expected \",\" after 36 characters, marked by [HERE] "
                      "in: \"...00000000000[HERE]!\"");
  test_gtid_set_error("%" UUID1,
                      "Expected hex digit at the beginning of the string: "
                      "\"%00000000-0000-0000-0000-...\"");

  // ==== Missing UUID ====
  test_gtid_set_error("1-2",
                      "Expected hex digit after 1 characters, marked by [HERE] "
                      "in: \"1[HERE]-2\"");
  test_gtid_set_error(
      "tag:1", "Expected hex digit at the beginning of the string: \"tag:1\"");
  test_gtid_set_error(
      ":1", "Expected hex digit at the beginning of the string: \":1\"");
  test_gtid_set_error(
      "-1", "Expected hex digit at the beginning of the string: \"-1\"");

  // ==== Out-of-range numbers ====
  test_gtid_set_error(UUID0 ":0",
                      "Interval start out of range after 37 characters, marked "
                      "by [HERE] in: \"...0000000000:[HERE]0\"");
  test_gtid_set_error(UUID0 ":0-1",
                      "Interval start out of range after 37 characters, marked "
                      "by [HERE] in: \"...0000000000:[HERE]0-1\"");
  test_gtid_set_error(UUID0 ":-1",
                      "Expected number after 37 characters, marked by [HERE] "
                      "in: \"...0000000000:[HERE]-1\"");
  test_gtid_set_error(UUID0 ":tag:0",
                      "Interval start out of range after 41 characters, marked "
                      "by [HERE] in: \"...000000:tag:[HERE]0\"");
  test_gtid_set_error(UUID0 ":tag:-1",
                      "Expected number after 41 characters, marked by [HERE] "
                      "in: \"...000000:tag:[HERE]-1\"");

  test_gtid_set_error(
      std::string{UUID0 ":"} + sequence_number_max_exclusive_str(),
      "Interval start out of range after 37 characters, marked by [HERE] in: "
      "\"...0000000000:[HERE]9223372036854775807\"");
  test_gtid_set_error(
      std::string{UUID0 ":"} + sequence_number_max_exclusive_str() + "-1",
      "Interval start out of range after 37 characters, marked by [HERE] in: "
      "\"...0000000000:[HERE]9223372036854775807-1\"");
  test_gtid_set_error(
      std::string{UUID0 ":1-"} + sequence_number_max_exclusive_str(),
      "Interval end out of range after 58 characters, marked by [HERE] in: "
      "\"...36854775807[HERE]\"");

  // ==== Extra colons ====
  test_gtid_set_error(UUID0 ":",
                      "Expected number after 37 characters, marked by [HERE] "
                      "in: \"...0000000000:[HERE]\"");
  test_gtid_set_error(UUID0 "::",
                      "Expected number after 37 characters, marked by [HERE] "
                      "in: \"...0000000000:[HERE]:\"");
  test_gtid_set_error(
      UUID0 ":," UUID1,
      "Expected number after 37 characters, marked by [HERE] in: "
      "\"...0000000000:[HERE],00000000-0000-0000-0000-...\"");
  test_gtid_set_error(
      UUID0 ":" UUID3,
      "Expected \",\" after 45 characters, marked by [HERE] in: "
      "\"...00:ffffffff[HERE]-ffff-ffff-ffff-ffffffffffff\"");
  test_gtid_set_error(
      UUID0 ",:" UUID1,
      "Expected hex digit after 37 characters, marked by [HERE] in: "
      "\"...0000000000,[HERE]:00000000-0000-0000-0000-...\"");
  test_gtid_set_error(UUID0 "::1-2",
                      "Expected number after 37 characters, marked by [HERE] "
                      "in: \"...0000000000:[HERE]:1-2\"");
  test_gtid_set_error(UUID0 ":1-2:",
                      "Expected number after 41 characters, marked by [HERE] "
                      "in: \"...000000:1-2:[HERE]\"");
  test_gtid_set_error(UUID0 ":1-2::3-4",
                      "Expected number after 41 characters, marked by [HERE] "
                      "in: \"...000000:1-2:[HERE]:3-4\"");
  test_gtid_set_error(UUID0 ":" TAG10 "::1-2",
                      "Expected number after 48 characters, marked by [HERE] "
                      "in: \"...a123456789:[HERE]:1-2\"");
  test_gtid_set_error(UUID0 ":" TAG10 ":1-2:",
                      "Expected number after 52 characters, marked by [HERE] "
                      "in: \"...456789:1-2:[HERE]\"");
  test_gtid_set_error(UUID0 ":" TAG10 ":1-2::3-4",
                      "Expected number after 52 characters, marked by [HERE] "
                      "in: \"...456789:1-2:[HERE]:3-4\"");
}

/// Convert the given Gtid_set to string, then run an oom_test on decoding that
/// string.
///
/// @param format Format to use.
///
/// @param gtid_set Gtid_set to encode+decode.
int oomt(const auto &format [[maybe_unused]],
         const gtids::Gtid_set &gtid_set [[maybe_unused]]) {
// MSVC standard containers allocate extra objects in debug mode, and when
// allocation of those objects fails, apparently it may crash. It is not
// critical that this part of the test has wide platform coverage: it tests
// platform-agnostic code just to cover error cases. We we simply disable the
// test on Windows.
#ifdef _WIN32
  return 1;
#else
  const allocators::Memory_resource *memory_resource;
  std::string str = strconv::throwing::encode(format, gtid_set);
  return debugging::oom_test(
      [&memory_resource](const allocators::Memory_resource &mr) {
        memory_resource = &mr;
      },
      [&format, &str, &gtid_set, &memory_resource] {
        gtids::Gtid_set out{*memory_resource};
        auto ret = strconv::decode(format, str, out);
        if (ret.is_store_error()) {
          ASSERT_EQ(strconv::throwing::encode_text(ret), "Out of memory");
          throw std::bad_alloc{};
        }
        ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);
        ASSERT_EQ(out, gtid_set) << strconv::throwing::concat_text(
            "out: ", out, " gtid_set: ", gtid_set);
      });
#endif  // ifdef _WIN32 ... else
}

/// For a given text-encoded Gtid_set:
/// 1. For each binary format, encode into that format, then for each prefix
///    try to decode it and expect it to fail.
/// 2. For each binary format and for text format, encode into that format and
///    then run oom_test on decoding the string.
///
/// @param str Text representation of the set.
void test_coverage_oom_and_prefix(const std::string &str) {
  MY_SCOPED_TRACE(str);
  strconv::Gtid_binary_format v0(
      strconv::Gtid_binary_format::Version_policy::v0_tagless);
  strconv::Gtid_binary_format v1(
      strconv::Gtid_binary_format::Version_policy::v1_tags);
  strconv::Gtid_binary_format v2(
      strconv::Gtid_binary_format::Version_policy::v2_tags_compact);
  strconv::Text_format txt;

  gtids::Gtid_set gtid_set;
  auto ret = strconv::decode_text(str, gtid_set);
  ASSERT_TRUE(ret.is_ok());

  if (!gtids::has_tags(gtid_set)) {
    unittest::libs::sets::test_decode_prefix(gtid_set, v0);
  }
  unittest::libs::sets::test_decode_prefix(gtid_set, v1);
  unittest::libs::sets::test_decode_prefix(gtid_set, v2);

  ASSERT_GE(oomt(txt, gtid_set), 1);
  if (!gtids::has_tags(gtid_set)) {
    ASSERT_GE(oomt(v0, gtid_set), 1);
  }
  ASSERT_GE(oomt(v1, gtid_set), 1);
  ASSERT_GE(oomt(v2, gtid_set), 1);
}

// Error case coverage for binary formats, and for out-of-memory conditions for
// all formats:
// 1. Any prefix of a binary format encoding is invalid. Thus we try all
//    prefixes hoping that it covers several different parse_error cases.
// 2. Try out-of-memory on each allocation hoping it covers all out-of-memory
//    conditions in the decoder.
TEST(LibsGtidsGtidSet, ErrorCoverageOomAndPrefix) {
  std::string notag_str{
      UUID0 ":1,"         //
      UUID1 ":1-2:4:6:9"  //
  };
  std::string tag_str{
      notag_str + ","                                //
          UUID2 ":tag:1-9:1000-200000:tag2:99-998,"  //
          UUID3 ":1-99:2-30:tag2:1-1000000",         //
  };

  test_coverage_oom_and_prefix(notag_str);
  test_coverage_oom_and_prefix(tag_str);
}

}  // namespace
