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
#include <regex>                              // regex
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/gtids/gtids.h"                // Gtid
#include "mysql/sets/sets.h"                  // make_union_view
#include "mysql/strconv/strconv.h"            // encode_text

namespace {

using namespace mysql;

// Primitive data structures: Uuid, Tag, Sequence_number, Tsid, Gtid.
TEST(LibsGtidsExample, Primitives) {
  const std::string uuid_str1("aa29992b-5f58-4d91-938e-3a4e42290a85");

  // Default construction.
  uuids::Uuid uuid1;
  gtids::Tag tag1;
  gtids::Sequence_number sequence_number1;
  gtids::Tsid tsid1;  // will have tag
  gtids::Tsid tsid2;  // will not have tag
  gtids::Gtid gtid1;  // will have tag
  gtids::Gtid gtid2;  // will not have tag

  // String parsing.
  auto parser = strconv::decode_text(uuid_str1, uuid1);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text("tag1", tag1);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text(uuid_str1 + ":tag1", tsid1);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text(uuid_str1, tsid2);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text(uuid_str1 + ":tag1:7", gtid1);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text(uuid_str1 + ":7", gtid2);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);
  parser = strconv::decode_text("7", sequence_number1);
  ASSERT_TRUE(parser.is_ok()) << strconv::throwing::encode_text(parser);

  // Reporting errors from string parsing
  uuids::Uuid uuid2;
  parser = strconv::decode_text("01234567-not-a-uuid", uuid2);
  ASSERT_FALSE(parser.is_ok());
  ASSERT_EQ(strconv::encode_text(parser),
            "Expected hex digit after 9 characters, marked by [HERE] in: "
            "\"01234567-[HERE]not-a-uuid\"");

  // Copy constructors and equality comparison.
  ASSERT_EQ(uuid1, uuids::Uuid(uuid1));
  ASSERT_EQ(tag1, gtids::Tag(tag1));
  ASSERT_EQ(sequence_number1, gtids::Sequence_number(sequence_number1));
  ASSERT_EQ(tsid1, gtids::Tsid(tsid1));
  ASSERT_EQ(tsid2, gtids::Tsid(tsid2));
  ASSERT_EQ(gtid1, gtids::Gtid(gtid1));
  ASSERT_EQ(gtid2, gtids::Gtid(gtid2));
  ASSERT_EQ(sequence_number1, 7LL);  // Sequence_number is std::int64_t

  ASSERT_NE(tsid1, tsid2);
  ASSERT_NE(gtid1, gtid2);

  // Composing compound types from primitive ones.
  ASSERT_EQ(tsid1, gtids::Tsid(uuid1, tag1));
  ASSERT_EQ(tsid2, gtids::Tsid(uuid1));
  ASSERT_EQ(gtid1, gtids::Gtid::throwing_make(uuid1, tag1, sequence_number1));
  ASSERT_EQ(gtid1, gtids::Gtid::throwing_make(tsid1, sequence_number1));
  ASSERT_EQ(gtid2, gtids::Gtid::throwing_make(uuid1, sequence_number1));
  ASSERT_EQ(gtid2, gtids::Gtid::throwing_make(tsid2, sequence_number1));

  // String formatting (throws exception on OOM).
  ASSERT_EQ(strconv::throwing::encode_text(uuid1), uuid_str1);
  ASSERT_EQ(strconv::throwing::encode_text(tag1), "tag1");
  ASSERT_EQ(strconv::throwing::encode_text(sequence_number1), "7");
  ASSERT_EQ(strconv::throwing::encode_text(tsid1), uuid_str1 + ":tag1");
  ASSERT_EQ(strconv::throwing::encode_text(tsid2), uuid_str1);
  ASSERT_EQ(strconv::throwing::encode_text(gtid1), uuid_str1 + ":tag1:7");
  ASSERT_EQ(strconv::throwing::encode_text(gtid2), uuid_str1 + ":7");

  // String formatting without exceptions
  // (returns std::optional<std::string>, no value on OOM).

  // clang-tidy wrongly claims that we call std::optional::value without first
  // checking std::optional::has_value.
  // NOLINTBEGIN(bugprone-unchecked-optional-access)
  auto opt_string = strconv::encode_text(uuid1);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), uuid_str1);
  opt_string = strconv::encode_text(tag1);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), "tag1");
  opt_string = strconv::encode_text(sequence_number1);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), "7");
  opt_string = strconv::encode_text(tsid1);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), uuid_str1 + ":tag1");
  opt_string = strconv::encode_text(tsid2);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), uuid_str1);
  opt_string = strconv::encode_text(gtid1);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), uuid_str1 + ":tag1:7");
  opt_string = strconv::encode_text(gtid2);
  ASSERT_TRUE(opt_string.has_value());
  ASSERT_EQ(opt_string.value(), uuid_str1 + ":7");
  // NOLINTEND(bugprone-unchecked-optional-access)
}

// Default construction and simple checks on empty sets.
TEST(LibsGtidsExample, GtidSetEmptySets) {
  // The default constructor gives an empty set.
  gtids::Gtid_set gtid_set1;
  gtids::Gtid_set gtid_set2;

  // Simple checks on empty sets
  ASSERT_TRUE(gtid_set1 == gtid_set2);
  ASSERT_TRUE(gtid_set1.empty());
  ASSERT_FALSE((bool)gtid_set1);
  ASSERT_TRUE(!gtid_set1);
  ASSERT_EQ(gtid_set1.size(), 0);           // number of tsids
  ASSERT_EQ(sets::volume(gtid_set1), 0.0);  // number of gtids as double
}

// Conversion from text and to text.
TEST(LibsGtidsExample, GtidSetTextConversion) {
  // decode_text(string, gtid_set) returns a Parser object, which you can query
  // for success using `parser.is_ok()`.
  gtids::Gtid_set gtid_set1;
  auto parser = strconv::decode_text(
      "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-100,"
      "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb:1-200:tag:1-300",
      gtid_set1);
  ASSERT_TRUE(parser.is_ok());

  // Use the non-throwing interface to get back the text representatin of the
  // Gtid_set. This returns an std::optional<std::string>, with no value if
  // allocation of the string failed.
  std::optional<std::string> gtid_set_as_opt_text =
      strconv::encode_text(gtid_set1);
  ASSERT_TRUE(gtid_set_as_opt_text.has_value());
  // clang-tidy wrongly claims that we call std::optional::value without first
  // checking std::optional::has_value.
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  ASSERT_EQ(gtid_set_as_opt_text.value(),
            "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-100,\n"
            "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb:1-200:tag:1-300");

  // Use the throwing interface to get back the text representatin of the
  // Gtid_set. This returns a std::string, or throws bad_alloc if allocation
  // of the string failed.
  std::string gtid_set_as_text = strconv::throwing::encode_text(gtid_set1);
  ASSERT_EQ(gtid_set_as_text,
            "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-100,\n"
            "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb:1-200:tag:1-300");
}

// Error messages from the parser.
TEST(LibsGtidsExample, GtidSetTextConversionError) {
  gtids::Gtid_set gtid_set1;

  // On error, get an error message using `encode_text`.
  auto parser = strconv::decode_text(
      "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-100,?garbage?", gtid_set1);
  ASSERT_FALSE(parser.is_ok());

  // Using the non-throwing interface, which returns an
  // std::optional<std::string>.
  std::optional<std::string> error_message1 = strconv::encode_text(parser);
  ASSERT_TRUE(error_message1.has_value());
  // clang-tidy wrongly claims that we call std::optional::value without first
  // checking std::optional::has_value.
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  ASSERT_EQ(error_message1.value(),
            "Expected hex digit after 43 characters, marked by [HERE] in: "
            "\"...aaaa:1-100,[HERE]?garbage?\"");

  // Using the throwing interface, which returns an std::string, or throws
  // bad_alloc if allocation of the string failed.
  std::string error_message2 = strconv::throwing::encode_text(parser);
  ASSERT_EQ(error_message2,
            "Expected hex digit after 43 characters, marked by [HERE] in: "
            "\"...aaaa:1-100,[HERE]?garbage?\"");
}

////////////////////////////////////////////////////////////////////////////////
//
// UUIDs are long and tedious to type and read. Henceforth, we abbreviate
// "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa" as "A" and
// "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb" as "B", and use the function
// make<gtids::Gtid_set> to convert strings like "A:1-100,B:1-200:tag:1-300"
// into Gtid_sets. This omits the error checks which you should always have in
// production, but which would just complicate this unittest.

// Test utility to simplify notation in this test. This replaces:
// - A by aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa
// - B by bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb
// - C by cccccccc-cccc-cccc-cccc-cccccccccccc
// ... and returns the result.
// Do not use in production since it may throw exceptions.
std::string fix_uuids(const std::string &str) {
  auto a_replaced = std::regex_replace(str, std::regex("A"),
                                       "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  auto a_and_b_replaced = std::regex_replace(
      a_replaced, std::regex("B"), "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");
  auto a_and_b_and_c_replaced =
      std::regex_replace(a_and_b_replaced, std::regex("C"),
                         "cccccccc-cccc-cccc-cccc-cccccccccccc");
  return a_and_b_and_c_replaced;
}

// Test utility to parse a string and return an object.
//
// This does not handle errors. It just asserts that the parse operation
// succeeds. That is OK in a unit test where the set is hard-coded. But do not
// use this pattern in production!
template <class Type>
Type make(const std::string &str) {
  Type object;
  auto converted_str = fix_uuids(str);
  auto parser = strconv::decode_text(converted_str, object);
  if (!parser.is_ok()) {
    std::cout << strconv::throwing::encode_text(parser);
    std::terminate();
  }
  return object;
}

// Conversion to binary format and back.
TEST(LibsGtidsExample, GtidSetBinaryConversion) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-200:tag:1-300");

  // Using the non-throwing interface.
  std::optional<std::string> binary_str =
      strconv::throwing::encode(strconv::Binary_format{}, gtid_set1);
  ASSERT_TRUE(binary_str.has_value());

  // Parse back to object format.
  gtids::Gtid_set gtid_set2;
  auto parser =
      strconv::decode(strconv::Binary_format{}, binary_str.value(), gtid_set2);
  ASSERT_TRUE(parser.is_ok());
  ASSERT_EQ(gtid_set1, gtid_set2);

  // Error messages are produced using encode_text(parser), exactly as after
  // decode_text.
}

// Constructors, assignment, and clear.
TEST(LibsGtidsExample, GtidSetConstructAssignClear) {
  // Since Gtid_set cannot throw, it cannot have a copy constructor or copy
  // assignment operators (as they have to handle out-of-memory errors).
  // Instead we have the member assign, which returns a success status.
  // We do suppport move construction and move construction, which steal
  // elements and don't throw. Also assign supports move semantics.
  // (The following two lines depend on copy-elision, which is guaranteed
  // in C++20, and does not invoke the copy constructor.)
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100");
  auto gtid_set2 = make<gtids::Gtid_set>("B:1-200");

  // `assign` copies the parameter to this, and returns a success status.
  auto ret = gtid_set2.assign(gtid_set1);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set2, make<gtids::Gtid_set>("A:1-100"));

  // Move-assignment operator. After this point, we must not use gtid_set1.
  gtid_set2 = std::move(gtid_set1);
  ASSERT_EQ(gtid_set2, make<gtids::Gtid_set>("A:1-100"));

  // Move constructor. After this point, we must not use gtid_set2.
  gtids::Gtid_set gtid_set3(std::move(gtid_set2));
  ASSERT_EQ(gtid_set3, make<gtids::Gtid_set>("A:1-100"));

  // Move using member assign. After this point, we must not use gtid_set2.
  gtids::Gtid_set gtid_set4;
  gtid_set4.assign(std::move(gtid_set3));
  ASSERT_EQ(gtid_set4, make<gtids::Gtid_set>("A:1-100"));

  // `clear` makes the set empty. It cannot fail.
  gtid_set4.clear();
  ASSERT_TRUE(gtid_set4.empty());
}

// Query emptiness, size, and volume.
TEST(LibsGtidsExample, GtidSetSizeQueries) {
  gtids::Gtid_set gtid_set0;  // empty
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-200:tag:1-300");

  // Emptiness: 3 syntax variants to do the same thing.
  ASSERT_TRUE(gtid_set0.empty());
  ASSERT_FALSE(gtid_set1.empty());

  ASSERT_TRUE(!gtid_set0);
  ASSERT_FALSE(!gtid_set1);

  ASSERT_FALSE((bool)gtid_set0);
  ASSERT_TRUE((bool)gtid_set1);

  // size gives the number of TSIDs.
  ASSERT_EQ(gtid_set0.size(), 0);
  ASSERT_EQ(gtid_set1.size(), 3);

  // volume gives the number of GTIDs.
  // Sets may be larger than 2^64, hence this is a double (which typically
  // loses precision for values greater than 2^53).
  ASSERT_EQ(sets::volume(gtid_set0), 0.0);
  ASSERT_EQ(sets::volume(gtid_set1), 600.0);

  // Difference in volume.
  // volume_difference(s1, s2) is equal to volume(s1)-volume(s2), except that
  // for large sets, volume(s1)-volume(s2) loses precision and
  // volume_difference(s1, s2) has good precision. This is important when
  // comparing set sizes since the lesser precision could make two sets with
  // nearly the same size may appear as equal-sized.
  // NOLINTBEGIN(readability-suspicious-call-argument): silence spurious warning
  auto gtid_set2 = make<gtids::Gtid_set>("A:1-100");
  ASSERT_EQ(sets::volume_difference(gtid_set1, gtid_set2), 500.0);
  ASSERT_EQ(sets::volume_difference(gtid_set2, gtid_set1), -500.0);
  // NOLINTEND(readability-suspicious-call-argument)
}

// Query membership.
TEST(LibsGtidsExample, GtidSetMembershipQueries) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-200:tag:1-300");
  // contains_element accepts either a Gtid...
  ASSERT_TRUE(sets::contains_element(gtid_set1, make<gtids::Gtid>("A:27")));
  ASSERT_FALSE(sets::contains_element(gtid_set1, make<gtids::Gtid>("A:1000")));
  // ... or a Tsid and a Sequence_number.
  ASSERT_TRUE(sets::contains_element(gtid_set1, make<gtids::Tsid>("A"), 27));
  ASSERT_FALSE(sets::contains_element(gtid_set1, make<gtids::Tsid>("A"), 1000));
}

// Query binary set relations.
TEST(LibsGtidsExample, GtidSetSetRelationQueries) {
  gtids::Gtid_set gtid_set0;  // empty
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100");
  auto gtid_set2 = make<gtids::Gtid_set>("A:1-100,B:1-200");
  auto gtid_set3 = make<gtids::Gtid_set>("B:1-200:tag:1-300");

  // NOLINTBEGIN(readability-suspicious-call-argument): silence spurious warning
  ASSERT_TRUE(sets::is_subset(gtid_set1, gtid_set2));
  ASSERT_FALSE(sets::is_subset(gtid_set2, gtid_set3));
  // the empty set is a subset of every set
  ASSERT_TRUE(sets::is_subset(gtid_set0, gtid_set1));
  // every set is a subset of itself.
  ASSERT_TRUE(sets::is_subset(gtid_set1, gtid_set1));

  // superset(x, y) == subset(y, x)
  ASSERT_TRUE(sets::is_superset(gtid_set1, gtid_set0));
  ASSERT_TRUE(sets::is_superset(gtid_set2, gtid_set1));
  ASSERT_FALSE(sets::is_superset(gtid_set3, gtid_set2));
  ASSERT_TRUE(sets::is_superset(gtid_set1, gtid_set1));

  ASSERT_TRUE(sets::is_intersecting(gtid_set1, gtid_set2));
  ASSERT_FALSE(sets::is_intersecting(gtid_set1, gtid_set3));
  // no set intersects with the empty set
  ASSERT_FALSE(sets::is_intersecting(gtid_set0, gtid_set1));
  // every set except the empty set intersects itself
  ASSERT_TRUE(sets::is_intersecting(gtid_set1, gtid_set1));

  // is_disjoint(x, y) == !is_intersecting(x, y)
  ASSERT_FALSE(sets::is_disjoint(gtid_set1, gtid_set2));
  ASSERT_TRUE(sets::is_disjoint(gtid_set1, gtid_set3));
  ASSERT_TRUE(sets::is_disjoint(gtid_set0, gtid_set1));
  ASSERT_FALSE(sets::is_disjoint(gtid_set1, gtid_set1));
  // NOLINTEND(readability-suspicious-call-argument)
}

// Views over binary set operations.
TEST(LibsGtidsExample, GtidSetBinarySetOperationViews) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100");
  auto gtid_set2 = make<gtids::Gtid_set>("B:1-200");
  auto gtid_set3 = make<gtids::Gtid_set>("A:1-100,B:1-200");
  auto gtid_set4 = make<gtids::Gtid_set>("B:1-200:tag:1-300");

  // A view never allocates, and the construction of a view never fails. There
  // are three kinds of views, corresponding to the three common binary set
  // operations. Each view is a different type (not a container).

  auto uv = make_union_view(gtid_set1, gtid_set2);
  ASSERT_EQ(uv, gtid_set3);

  auto iv = make_intersection_view(gtid_set3, gtid_set4);
  ASSERT_EQ(iv, gtid_set2);

  auto sv = make_subtraction_view(gtid_set3, gtid_set2);
  ASSERT_EQ(sv, gtid_set1);

  auto empty = make_intersection_view(gtid_set1, gtid_set4);
  ASSERT_TRUE(empty.empty());

  // Despite being different types, the views have the same APIs and support
  // the same queries.
  ASSERT_EQ(uv.size(), 2);
  ASSERT_EQ(sets::volume(uv), 300);
  ASSERT_FALSE(sets::contains_element(iv, make<gtids::Gtid>("A:1")));

  // If the sources are modified, the views see the updated sets.
  auto ret = gtid_set1.insert(make<gtids::Gtid>("A:101"));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_TRUE(sets::contains_element(uv, make<gtids::Gtid>("A:101")));

  // (The views can't be modified.)
}

// Insert an element.
TEST(LibsGtidsExample, GtidSetInsert) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100");

  // `insert` may fail with OOM, hence has a return status we must check.
  auto ret = gtid_set1.insert(make<gtids::Gtid>("A:101"));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:1-101"));

  // Insertion is like union; if the element is already there, the operation
  // succeeds without altering the set.
  ret = gtid_set1.insert(make<gtids::Gtid>("A:101"));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:1-101"));

  // `insert` also accepts a Tsid+Sequence_number pair.
  ret = gtid_set1.insert(make<gtids::Tsid>("A"), 102);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:1-102"));
}

// Remove an element.
TEST(LibsGtidsExample, GtidSetRemove) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100");

  // `remove` may fail with OOM, hence has a return status we must check.
  auto ret = gtid_set1.remove(make<gtids::Gtid>("A:50"));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:1-49:51-100"));

  // Removal is like subtraction; if the element is not there, the operation
  // succeeds without altering the set.
  ret = gtid_set1.remove(make<gtids::Gtid>("A:50"));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:1-49:51-100"));

  // `remove` also accepts a Tsid+Sequence_number pair.
  ret = gtid_set1.remove(make<gtids::Tsid>("A"), 1);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, make<gtids::Gtid_set>("A:2-49:51-100"));
}

// In-place union modifies the set.
TEST(LibsGtidsExample, GtidSetInplaceUnion) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-10");
  auto gtid_set2 = make<gtids::Gtid_set>("A:1-10,B:1-200");
  auto gtid_set3 = make<gtids::Gtid_set>("A:1-100,B:1-200");
  auto ret = gtid_set1.inplace_union(gtid_set2);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, gtid_set3);

  // inplace_union also accepts rvalue references. When the two operands have
  // compatible types and their allocators compare as equal, this invokes move
  // semantics, so the operation steals elements and does not allocate. After
  // this point, we cannot use gtid_set2.
  auto gtid_set4 = make<gtids::Gtid_set>("A:1-100,B:1-10");
  ret = gtid_set4.inplace_union(std::move(gtid_set2));
  ASSERT_EQ(ret, utils::Return_status::ok);
}

// In-place intersection modifies the set.
TEST(LibsGtidsExample, GtidSetInplaceIntersection) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-150");
  auto gtid_set2 = make<gtids::Gtid_set>("B:50-200:tag:1-300");
  auto gtid_set3 = make<gtids::Gtid_set>("B:50-150");
  auto ret = gtid_set1.inplace_intersect(gtid_set2);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, gtid_set3);

  // It is also possible to intersect with a Tsid. This never allocates and
  // hence cannot fail.
  gtid_set2.inplace_intersect(make<gtids::Tsid>("B:tag"));
  ASSERT_EQ(gtid_set2, make<gtids::Gtid_set>("B:tag:1-300"));

  // inplace_intersect also accepts rvalue references. When the two operands
  // have compatible types and their allocators compare as equal, this invokes
  // move semantics, so the operation steals elements and does not allocate.
  // After this point, we cannot use gtid_set5.
  auto gtid_set4 = make<gtids::Gtid_set>("A:1-100,B:1-150");
  auto gtid_set5 = make<gtids::Gtid_set>("B:50-200:tag:1-300");
  ret = gtid_set4.inplace_intersect(std::move(gtid_set5));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set4, gtid_set3);
}

// In-place subtraction modifies the set.
TEST(LibsGtidsExample, GtidSetInplaceSubtraction) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-100,B:1-150");
  auto gtid_set2 = make<gtids::Gtid_set>("B:50-200:tag:1-300");
  auto gtid_set3 = make<gtids::Gtid_set>("A:1-100,B:1-49");
  auto ret = gtid_set1.inplace_subtract(gtid_set2);
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set1, gtid_set3);

  // It is also possible to subtract a Tsid. This never allocates and hence
  // cannot fail.
  gtid_set2.inplace_subtract(make<gtids::Tsid>("B:tag"));
  ASSERT_EQ(gtid_set2, make<gtids::Gtid_set>("B:50-200"));

  // inplace_subtract also accepts rvalue references. When the two operands have
  // compatible types and their allocators compare as equal, this invokes move
  // semantics, so the operation steals elements and does not allocate. After
  // this point, we cannot use gtid_set5.
  auto gtid_set4 = make<gtids::Gtid_set>("A:1-100,B:1-150");
  auto gtid_set5 = make<gtids::Gtid_set>("B:50-200:tag:1-300");
  ret = gtid_set4.inplace_subtract(std::move(gtid_set5));
  ASSERT_EQ(ret, utils::Return_status::ok);
  ASSERT_EQ(gtid_set4, gtid_set3);
}

// Empty set view is a helper object; a non-allocating type that represents an
// empty set.
TEST(LibsGtidsExample, GtidSetEmptySetView) {
  // An empty set view has the API of a set, but is a simpler type and behaves
  // as empty.
  auto empty_set_view = sets::make_empty_set_view_like<gtids::Gtid_set>();
  ASSERT_TRUE(empty_set_view.empty());
  ASSERT_EQ(empty_set_view.size(), 0);
}

// You can iterate over Gtid sets, but don't do it unless you know exactly what
// you are doing.
TEST(LibsGtidsExample, GtidSetIteration) {
  // You can iterate over all individual Gtids in a set.
  //
  // WARNING: Remember that Gtid sets are range-compressed for a reason.
  // Usually, there is one element for every transaction committed since the
  // beginning of history, which can be thousands per second for many years.
  // Iterate over individual GTIDs only if you know the set is small (for
  // example, holds the difference between sets captured at two adjacent time
  // points).
  //
  // The only purpose of this code is to illustrate how to iterate. It is a
  // horrible way to copy sets: the right way to do that is
  // `gtid_set2.assign(gtid_set1)` or `gtid_set2.inplace_union(gtid_set1)`
  // (depending on whether you want to preserve existing elements in gtid_set2).
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-10,B:1-20");
  gtids::Gtid_set gtid_set2;
  // The following loop is ok, just one element per TSID in the set.
  //
  // Note: the `&&` makes `tsid` and `interval_set` be *forwarding references*.
  // In effect, the compiler deduces the reference type correctly both when we
  // iterate over a container (like here) and when we iterate over a view (such
  // as `Union_view`). For containers it is essential to use lvalue references,
  // because the interval set is a container that we should not copy in each
  // iteration. For views it is essential to use rvalue references, because the
  // interval set is a (lightweight) temporary view object that we need to copy
  // to avoid dangling references. It is ok to use `&` if you know you iterate
  // over a container, and it is ok to drop the `&&` if you know you iterate
  // over a view; but using `&&` is the only notation that is safe in generic
  // code that may accept either a view or a container.
  for (auto &&[tsid, interval_set] : gtid_set1) {
    // The following loop is ok, just one element per interval for this TSID.
    //
    // These intervals are temporary objects created when dereferencing the
    // iterator, so we make `interval` a non-reference.
    for (auto interval : interval_set) {
      // Loops like the following can be dubious, as intervals can be huge. Be
      // careful and use this only if your use case provides gives a bound on
      // the interval length.
      for (auto sequence_number = interval.start();
           sequence_number != interval.exclusive_end(); ++sequence_number) {
        auto ret = gtid_set2.insert(tsid, sequence_number);
        ASSERT_EQ(ret, utils::Return_status::ok);
      }
    }
  }
  ASSERT_EQ(gtid_set1, gtid_set2);
}

// Operations to search Gtid sets: begin, end, front, back, find, operator[],
// upper_bound, and lower_bound.
TEST(LibsGtidsExample, GtidSetSearching) {
  auto gtid_set1 = make<gtids::Gtid_set>("A:1-10:20-30:40-50,B:1-20");
  auto tsid_a = make<gtids::Tsid>("A");
  auto tsid_b = make<gtids::Tsid>("B");
  auto tsid_c = make<gtids::Tsid>("C");

  // Find iterators using begin() and end().
  auto it_begin = gtid_set1.begin();
  auto it_end = gtid_set1.end();
  ASSERT_EQ(std::ranges::distance(it_begin, it_end), 2);
  ASSERT_EQ(it_begin->first, tsid_a);

  // Find (Tsid, Gtid_interval_set) pairs using front() and back().
  ASSERT_EQ(*it_begin, gtid_set1.front());
  ASSERT_EQ(gtid_set1.front().first, tsid_a);
  ASSERT_EQ(*std::prev(it_end), gtid_set1.back());
  ASSERT_EQ(gtid_set1.back().first, tsid_b);

  // Lookup Gtid_interval_sets using operator[]. This is only allowed if the
  // given tsid exists in the set; otherwise it is undefined behavior.
  gtids::Gtid_interval_set &ivset_a = gtid_set1[tsid_a];
  ASSERT_EQ(&ivset_a, &gtid_set1.front().second);

  // Lookup using find (and find the element).
  auto it_b = gtid_set1.find(tsid_b);
  ASSERT_NE(it_b, gtid_set1.end());

  // Lookup using find (and don't find the element).
  auto it_c = gtid_set1.find(tsid_c);
  ASSERT_EQ(it_c, gtid_set1.end());

  // Get the boundary set from the interval set.
  auto &boundary_set = ivset_a.boundaries();

  // Compute upper and lower bounds in a boundary set. The upper bound is an
  // iterator to the next boundary whose value is strictly greater than the
  // given value. The lower bound is an iterator to the next boundary whose
  // value is greater than or equal to the given value. Iterators in boundary
  // sets have the member function `is_endpoint` which indicates if the
  // pointed-to boundary is the start or exclusive end of an interval. End
  // boundaries are always exclusive (hence, you see 31 rather than 30 in the
  // following code).
  auto ub1 = boundary_set.upper_bound(25);
  ASSERT_EQ(*ub1, 31);
  ASSERT_TRUE(ub1.is_endpoint());
  auto lb1 = boundary_set.lower_bound(25);
  ASSERT_EQ(*lb1, 31);
  ASSERT_TRUE(lb1.is_endpoint());

  auto ub2 = boundary_set.upper_bound(31);
  ASSERT_EQ(*ub2, 40);
  ASSERT_FALSE(ub2.is_endpoint());
  auto lb2 = boundary_set.lower_bound(31);
  ASSERT_EQ(*lb2, 31);
  ASSERT_TRUE(lb2.is_endpoint());
}

}  // namespace
