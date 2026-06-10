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

#include <gtest/gtest.h>                          // TEST
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // test_cmp
#include "mysql/gtids/gtids.h"                    // Gtid
#include "mysql/strconv/strconv.h"                // encode_text

namespace {

using namespace mysql;

static_assert(gtids::Is_tag<gtids::Tag>);
static_assert(std::regular<gtids::Tag>);
static_assert(gtids::Is_tag<gtids::Tag_trivial>);
static_assert(std::regular<gtids::Tag_trivial>);

static_assert(gtids::Is_tsid<gtids::Tsid>);
static_assert(std::regular<gtids::Tsid>);
static_assert(gtids::Is_tsid<gtids::Tsid_trivial>);
static_assert(std::regular<gtids::Tsid_trivial>);

static_assert(gtids::Is_gtid<gtids::Gtid>);
static_assert(std::regular<gtids::Gtid>);
static_assert(gtids::Is_gtid<gtids::Gtid_trivial>);
static_assert(std::regular<gtids::Gtid_trivial>);

static_assert(std::is_trivially_default_constructible_v<gtids::Tag_trivial>);
static_assert(std::is_trivially_copy_constructible_v<gtids::Tag_trivial>);

static_assert(std::is_trivially_default_constructible_v<gtids::Tsid_trivial>);
static_assert(std::is_trivially_copy_constructible_v<gtids::Tsid_trivial>);

static_assert(std::is_trivially_default_constructible_v<gtids::Gtid_trivial>);
static_assert(std::is_trivially_copy_constructible_v<gtids::Gtid_trivial>);

static_assert(
    std::same_as<
        strconv::detail::Default_format_type<strconv::Text_format, gtids::Gtid>,
        strconv::Gtid_text_format>);
static_assert(std::same_as<decltype(strconv::Gtid_text_format{}.parent()),
                           strconv::Boundary_set_text_format>);

/// Convert `gtid` to text and back, using the given format, and expect to get
/// the original set back.
template <strconv::Is_format Format_t, gtids::Is_gtid Gtid_t>
void test_convert([[maybe_unused]] std::string_view format_name,
                  const Gtid_t &gtid, std::size_t expected_size) {
  MY_SCOPED_TRACE("format=", format_name);
  std::string str = strconv::throwing::encode(Format_t{}, gtid);
  Gtid_t back;
  auto ret = strconv::decode(Format_t{}, str, back);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);
  ASSERT_EQ(gtid, back);
  ASSERT_EQ(str.size(), expected_size);
}

/// Convert `str` to Gtid and back and expect the result to be `expected`.
template <class Gtid_t>
void test_gtid_type(std::string_view str, std::string_view expected) {
  MY_SCOPED_TRACE("string=", str, " expected=", expected);

  Gtid_t gtid;
  auto ret = strconv::decode(strconv::Text_format{}, str, gtid);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);
  ASSERT_EQ(strconv::throwing::encode_text(gtid), expected);
  std::size_t text_tag_size = 0;
  if (gtid.tag()) text_tag_size = 1 + gtid.tag().size();
  test_convert<strconv::Text_format>(
      "text", gtid,
      uuids::Uuid::text_size + text_tag_size + 1 +
          strconv::compute_encoded_length_text(gtid.get_sequence_number()));
  test_convert<strconv::Binary_format>(
      "binary", gtid,
      uuids::Uuid::byte_size + 1 + gtid.tag().size() +
          strconv::compute_encoded_length(strconv::Binary_format{},
                                          gtid.get_sequence_number()));
}

template <class Gtid_t>
void test_gtid_error_type(std::string_view str) {
  MY_SCOPED_TRACE("string=", str);
  Gtid_t gtid;
  auto ret = strconv::decode(strconv::Text_format{}, str, gtid);
  ASSERT_TRUE(!ret.is_ok());
  ASSERT_TRUE(ret.is_parse_error());
}

template <class Gtid_t>
void test_gtid_cmp_type(std::string_view str1, std::string_view str2,
                        auto cmp) {
  Gtid_t gtid1;
  Gtid_t gtid2;
  auto ret = strconv::decode(strconv::Text_format{}, str1, gtid1);
  ASSERT_TRUE(ret.is_ok());
  ret = strconv::decode(strconv::Text_format{}, str2, gtid2);
  ASSERT_TRUE(ret.is_ok());
  debugging::test_cmp(str1, str2, cmp);
}

void test_gtid(std::string_view str, std::string_view expected) {
  {
    MY_SCOPED_TRACE("type=Gtid_trivial");
    test_gtid_type<gtids::Gtid_trivial>(str, expected);
  }
  {
    MY_SCOPED_TRACE("type=Gtid");
    test_gtid_type<gtids::Gtid>(str, expected);
  }
}

void test_gtid(std::string_view str) { test_gtid(str, str); }

void test_gtid_error(std::string_view str) {
  test_gtid_error_type<gtids::Gtid>(str);
  test_gtid_error_type<gtids::Gtid_trivial>(str);
}

void test_gtid_cmp(std::string_view str1, std::string_view str2, auto cmp) {
  test_gtid_cmp_type<gtids::Gtid>(str1, str2, cmp);
  test_gtid_cmp_type<gtids::Gtid_trivial>(str1, str2, cmp);
}

// It's more convenient to concatenate string literals.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define UUID0 "00000000-0000-0000-0000-000000000000"
#define UUID1 "00000000-0000-0000-0000-000000000001"
#define UUID2 "63d02e6c-9555-4d16-a8c3-e9d7590103b0"
#define UUID3 "ffffffff-ffff-ffff-ffff-ffffffffffff"
#define UUID0a "00000000000000000000000000000000"
#define UUID1a "00000000000000000000000000000001"
#define UUID2a "63d02e6c95554d16a8c3e9d7590103b0"
#define UUID3a "ffffffffffffffffffffffffffffffff"

#define TAG32 "aaaabbbbccccddddeeeeffffgggghhhh"
#define TAG10 "a123456789"
#define TAGUC "ABCdefGHI"
#define TAGLC "abcdefghi"
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
TEST(LibsGtids, Basic) {
  // simple gtids
  test_gtid(UUID0 ":1");
  test_gtid(UUID1 ":999");
  test_gtid(std::string(UUID2 ":") + sequence_number_max_inclusive_str());

  // gtids with tags
  test_gtid(UUID3 ":foo:1");
  test_gtid(UUID0 ":" TAG10 ":42");
  test_gtid(UUID1 ":" TAG32 ":4711");

  // variations in case, spacing, and leading zeros in the number.
  test_gtid(" " UUID2 ":1", UUID2 ":1");
  test_gtid(UUID0 " : 1 ", UUID0 ":1");
  test_gtid(" " UUID1 " : 1 ", UUID1 ":1");
  test_gtid("\f\f\n" UUID3 "\t \t:\n1\r", UUID3 ":1");
  test_gtid(UUID0 ":0001", UUID0 ":1");
  test_gtid(UUID1 ":tag:0001", UUID1 ":tag:1");

  // test tag case normalization
  test_gtid(UUID3 ":TAG:1", UUID3 ":tag:1");
  test_gtid(UUID3 ":Foo7Bar:99", UUID3 ":foo7bar:99");

  // alternative uuid formats
  test_gtid("{" UUID0 "}:1", UUID0 ":1");
  test_gtid("{" UUID0 "}:tag:1", UUID0 ":tag:1");
  test_gtid(UUID0a ":1", UUID0 ":1");
  test_gtid(UUID0a ":tag:1", UUID0 ":tag:1");
}

TEST(LibsGtids, Comparison) {
  // comparison
  test_gtid_cmp(UUID0 ":1", UUID1 ":1", -1);            // different uuids
  test_gtid_cmp(UUID0 ":z:1", UUID1 ":a:1", -1);        // different uuids
  test_gtid_cmp(UUID0 ":99", UUID1 ":1", -1);           // different uuids
  test_gtid_cmp(UUID0 ":z:99", UUID1 ":z:99", -1);      // different uuids
  test_gtid_cmp(UUID3 ":aa:1", UUID3 ":b:1", -1);       // different tags
  test_gtid_cmp(UUID0 ":aa:99", UUID0 ":b:1", -1);      // different tags
  test_gtid_cmp(UUID1 ":1", UUID1 ":2", -1);            // different numbers
  test_gtid_cmp(UUID2 ":tag1:1", UUID2 ":tag1:2", -1);  // different numbers
}

TEST(LibsGtids, Errors) {
  // wrong structure
  test_gtid_error(UUID3 "0:1");       // extra 0 in the uuid
  test_gtid_error(UUID0 ":1:");       // extra : after the number
  test_gtid_error(":" UUID1 ":1");    // colon before the uuid
  test_gtid_error(UUID2 ":tag:");     // missing number
  test_gtid_error(UUID3 ":tag: ");    // missing number
  test_gtid_error(UUID0 ":tag:tag");  // extra tag and no number
  test_gtid_error(UUID1 "::");        // double colon
  test_gtid_error(UUID3 "::1");       // double colon, number
  test_gtid_error(UUID2 "::tag");     // double colon, tag
  test_gtid_error(UUID3 ":1:");       // colon after the number
  test_gtid_error(UUID0 ":");         // missing number or tag
  test_gtid_error(UUID1 ":1:tag");    // tag after the number

  // malformed uuids
  test_gtid_error(
      "00000000 -0000-0000-0000-000000000000:1");  // space in the uuid
  test_gtid_error("0" UUID2 ":1");                 // extra digit in first group
  test_gtid_error(UUID3 "0:1");                    // extra digit in last group
  test_gtid_error(
      "0000000-0000-0000-0000-000000000000:1");  // missing digit in first group
  test_gtid_error(
      "00000000-0000-0000-0000-00000000000:1");  // missing digit in last group
  test_gtid_error("{" UUID0a "}:1");             // braces but no dashes
  test_gtid_error(UUID0a "0:1");                 // extra digit, no dashes
  test_gtid_error(
      "0000111122223333444455556666777:1");  // missing digit, no dashes

  // malformed tags
  test_gtid_error(UUID0 ":" TAG32 "x:1");  // too long tag
  test_gtid_error(UUID1 ":tag<x:1");       // invalid character
  test_gtid_error(UUID2 ":0tag:1");        // leading number

  // sequence_number out of bounds
  test_gtid_error(UUID1 ":-1");
  test_gtid_error(UUID2 ":-0");
  test_gtid_error(UUID3 ":0");
  test_gtid_error(UUID0 ":00");
  test_gtid_error(UUID1 ":" + sequence_number_max_exclusive_str());
  test_gtid_error(UUID2 ":tag:-1");
  test_gtid_error(UUID3 ":tag:-0");
  test_gtid_error(UUID0 ":tag:0");
  test_gtid_error(UUID1 ":tag:00");
  test_gtid_error(UUID2 ":tag:" + sequence_number_max_exclusive_str());
}

}  // namespace
