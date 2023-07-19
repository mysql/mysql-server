/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/classic_protocol_codec_binary.h"

#include <list>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "mysql.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "test_classic_protocol_codec.h"

namespace classic_protocol::borrowable::binary {
template <class T>
std::ostream &operator<<(std::ostream &os, const TypeBase<T> &val) {
  os << val.value();
  return os;
}
}  // namespace classic_protocol::borrowable::binary

namespace bt = classic_protocol::borrowed::binary;
namespace ft = classic_protocol::field_type;

// POD classes

static_assert(bt::Tiny(1).value() == 1);
static_assert(bt::Short(1).value() == 1);
static_assert(bt::Int24(1).value() == 1);
static_assert(bt::Long(1).value() == 1);
static_assert(bt::LongLong(1).value() == 1);
static_assert(bt::Float(1).value() == 1);
static_assert(bt::Double(1).value() == 1);
static_assert(bt::Year(1).value() == 1);
static_assert(bt::Date(1990, 2, 1).year() == 1990);

static_assert(bt::Time(false, 1, 2, 3, 4).hour() == 2);
static_assert(bt::Time(true, 1, 2, 3, 4).hour() == 2);

static_assert(bt::DateTime(1990, 2, 1, 1, 3, 4, 5).year() == 1990);
static_assert(bt::DateTime(1991, 2, 1, 1, 3, 4).year() == 1991);
static_assert(bt::DateTime(1992, 2, 1).year() == 1992);

static_assert(bt::String("abc").value() == "abc");

static_assert(bt::VarString("abc").value() == "abc");
static_assert(bt::Varchar("abc").value() == "abc");
static_assert(bt::Blob("abc").value() == "abc");
static_assert(bt::TinyBlob("abc").value() == "abc");
static_assert(bt::MediumBlob("abc").value() == "abc");
static_assert(bt::LongBlob("abc").value() == "abc");
static_assert(bt::Json("abc").value() == "abc");

// codec type (in ft:: order)

static_assert(classic_protocol::Codec<bt::Decimal>::type() == ft::Decimal);
static_assert(classic_protocol::Codec<bt::Tiny>::type() == ft::Tiny);
static_assert(classic_protocol::Codec<bt::Short>::type() == ft::Short);
static_assert(classic_protocol::Codec<bt::Long>::type() == ft::Long);
static_assert(classic_protocol::Codec<bt::Float>::type() == ft::Float);
static_assert(classic_protocol::Codec<bt::Double>::type() == ft::Double);
static_assert(classic_protocol::Codec<bt::Null>::type() == ft::Null);
static_assert(classic_protocol::Codec<bt::Timestamp>::type() == ft::Timestamp);
static_assert(classic_protocol::Codec<bt::LongLong>::type() == ft::LongLong);
static_assert(classic_protocol::Codec<bt::Int24>::type() == ft::Int24);
static_assert(classic_protocol::Codec<bt::Date>::type() == ft::Date);
static_assert(classic_protocol::Codec<bt::Time>::type() == ft::Time);
static_assert(classic_protocol::Codec<bt::DateTime>::type() == ft::DateTime);
static_assert(classic_protocol::Codec<bt::Year>::type() == ft::Year);
static_assert(classic_protocol::Codec<bt::Varchar>::type() == ft::Varchar);
static_assert(classic_protocol::Codec<bt::Bit>::type() == ft::Bit);
// Timestamp2 ?
static_assert(classic_protocol::Codec<bt::Json>::type() == ft::Json);
static_assert(classic_protocol::Codec<bt::NewDecimal>::type() ==
              ft::NewDecimal);
static_assert(classic_protocol::Codec<bt::Enum>::type() == ft::Enum);
static_assert(classic_protocol::Codec<bt::Set>::type() == ft::Set);
static_assert(classic_protocol::Codec<bt::TinyBlob>::type() == ft::TinyBlob);
static_assert(classic_protocol::Codec<bt::MediumBlob>::type() ==
              ft::MediumBlob);
static_assert(classic_protocol::Codec<bt::LongBlob>::type() == ft::LongBlob);
static_assert(classic_protocol::Codec<bt::Blob>::type() == ft::Blob);

static_assert(classic_protocol::Codec<bt::VarString>::type() == ft::VarString);
static_assert(classic_protocol::Codec<bt::String>::type() == ft::String);
static_assert(classic_protocol::Codec<bt::Geometry>::type() == ft::Geometry);

// check .size() is constexpr.

#ifdef __clang__
// clang compiles this nicely, but gcc says:
//
// error: accessing value of
//
// ‘<anonymous>.classic_protocol::Codec<classic_protocol::borrowable::binary::Tiny>::
//  <anonymous>.classic_protocol::impl::FixedIntCodec<classic_protocol::borrowable::binary::Tiny>::<anonymous>’
//
// through a ‘const
// classic_protocol::impl::FixedIntCodec<classic_protocol::borrowable::binary::Tiny>’
// glvalue in a constant expression
static_assert(classic_protocol::Codec<bt::Tiny>({1}, {/* caps */}).size() == 1);

static_assert(classic_protocol::Codec<bt::Short>({1}, {/* caps */}).size() ==
              2);

static_assert(classic_protocol::Codec<bt::Int24>({1}, {/* caps */}).size() ==
              4);

static_assert(classic_protocol::Codec<bt::Long>({1}, {/* caps */}).size() == 4);

static_assert(classic_protocol::Codec<bt::Null>({}, {/* caps */}).size() == 0);

static_assert(classic_protocol::Codec<bt::Year>({1}, {/* caps */}).size() == 2);

static_assert(classic_protocol::Codec<bt::LongLong>({1}, {/* caps */}).size() ==
              8);

static_assert(classic_protocol::Codec<bt::Float>({1}, {/* caps */}).size() ==
              4);

static_assert(classic_protocol::Codec<bt::Double>({1}, {/* caps */}).size() ==
              8);

static_assert(
    classic_protocol::Codec<bt::String>({"abc"}, {/* caps */}).size() == 3);
static_assert(
    classic_protocol::Codec<bt::Varchar>({"abc"}, {/* caps */}).size() == 3);

static_assert(
    classic_protocol::Codec<bt::VarString>({"abc"}, {/* caps */}).size() == 3);

static_assert(classic_protocol::Codec<bt::Json>({"abc"}, {/* caps */}).size() ==
              3);

static_assert(
    classic_protocol::Codec<bt::Geometry>({"abc"}, {/* caps */}).size() == 3);

// a empty-time has no data.
static_assert(classic_protocol::Codec<bt::Time>({}, {/* caps */}).size() == 0);

// time with seconds, but no micro-seconds.
static_assert(classic_protocol::Codec<bt::Time>(
                  {
                      false,  // negative
                      1,      // days
                      2,      // hours
                      3,      // minutes
                      4       // seconds
                  },
                  {/* caps */})
                  .size() == 1 + 4 + 1 + 1 + 1);

// time with micro-seconds.
static_assert(classic_protocol::Codec<bt::Time>(
                  {
                      false,  // negative
                      1,      // days
                      2,      // hours
                      3,      // minutes
                      4,      // seconds
                      999999  // microseconds
                  },
                  {/* caps */})
                  .size() == 1 + 4 + 1 + 1 + 1 + 4);

// a empty-time has no data.
static_assert(classic_protocol::Codec<bt::Date>({}, {/* caps */}).size() == 0);

// time with seconds, but no micro-seconds.
static_assert(classic_protocol::Codec<bt::Date>(
                  {
                      1,  // years
                      2,  // month
                      3   // mday
                  },
                  {/* caps */})
                  .size() == 2 + 1 + 1);

// date with time.
static_assert(classic_protocol::Codec<bt::DateTime>(
                  {
                      1,  // years
                      2,  // month
                      3,  // mday
                      2,  // hour
                      3,  // minutes
                      4,  // seconds
                  },
                  {/* caps */})
                  .size() == 2 + 1 + 1 + 1 + 1 + 1);

// date with time and microseconds
static_assert(classic_protocol::Codec<bt::DateTime>(
                  {
                      1,      // years
                      2,      // month
                      3,      // mday
                      2,      // hour
                      3,      // minutes
                      4,      // seconds
                      999999  // microseconds
                  },
                  {/* caps */})
                  .size() == 2 + 1 + 1 + 1 + 1 + 1 + 4);
#endif

// Tiny

using CodecBinaryTinyTest = CodecTest<classic_protocol::binary::Tiny>;

TEST_P(CodecBinaryTinyTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryTinyTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::Tiny> codec_binary_tiny_param[] = {
    {"0", {0}, {}, {0x00}},
    {"1", {1}, {}, {0x01}},
    {"255", {255}, {}, {0xff}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryTinyTest,
                         ::testing::ValuesIn(codec_binary_tiny_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Short

using CodecBinaryShortTest = CodecTest<classic_protocol::binary::Short>;

TEST_P(CodecBinaryShortTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryShortTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::Short> codec_binary_short_param[] = {
    {"0", {0}, {}, {0x00, 0x00}},
    {"1", {1}, {}, {0x01, 0x00}},
    {"255", {255}, {}, {0xff, 0x00}},
    {"256", {256}, {}, {0x00, 0x01}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryShortTest,
                         ::testing::ValuesIn(codec_binary_short_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Int24

using CodecBinaryInt24Test = CodecTest<classic_protocol::binary::Int24>;

TEST_P(CodecBinaryInt24Test, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryInt24Test, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::Int24> codec_binary_int24_param[] = {
    {"0", {0}, {}, {0x00, 0x00, 0x00, 0x00}},
    {"1", {1}, {}, {0x01, 0x00, 0x00, 0x00}},
    {"1_byte_end", {0xff}, {}, {0xff, 0x00, 0x00, 0x00}},
    {"2_byte_start", {0x0100}, {}, {0x00, 0x01, 0x00, 0x00}},
    {"2_byte_end", {0xffff}, {}, {0xff, 0xff, 0x00, 0x00}},
    {"3_byte_start", {0x10000}, {}, {0x00, 0x00, 0x01, 0x00}},
    {"3_byte_end", {0xffffff}, {}, {0xff, 0xff, 0xff, 0x00}},

    // the 4th byte is undefined.
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryInt24Test,
                         ::testing::ValuesIn(codec_binary_int24_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Long

using CodecBinaryLongTest = CodecTest<classic_protocol::binary::Long>;

TEST_P(CodecBinaryLongTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryLongTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::Long> codec_binary_long_param[] = {
    {"0", {0}, {}, {0x00, 0x00, 0x00, 0x00}},
    {"1", {1}, {}, {0x01, 0x00, 0x00, 0x00}},
    {"1_byte_end", {0xff}, {}, {0xff, 0x00, 0x00, 0x00}},
    {"2_byte_start", {0x0100}, {}, {0x00, 0x01, 0x00, 0x00}},
    {"2_byte_end", {0xffff}, {}, {0xff, 0xff, 0x00, 0x00}},
    {"3_byte_start", {0x10000}, {}, {0x00, 0x00, 0x01, 0x00}},
    {"3_byte_end", {0xffffff}, {}, {0xff, 0xff, 0xff, 0x00}},
    {"4_byte_start", {0x1000000}, {}, {0x00, 0x00, 0x00, 0x01}},
    {"4_byte_end", {0xffffffff}, {}, {0xff, 0xff, 0xff, 0xff}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryLongTest,
                         ::testing::ValuesIn(codec_binary_long_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// LongLong

using CodecBinaryLongLongTest = CodecTest<classic_protocol::binary::LongLong>;

TEST_P(CodecBinaryLongLongTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryLongLongTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::LongLong>
    codec_binary_longlong_param[] = {
        {"0", {0x00}, {}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"1", {0x01}, {}, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"1_byte_end",
         {0xff},
         {},
         {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"2_byte_start",
         {0x0100},
         {},
         {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"2_byte_end",
         {0xffff},
         {},
         {0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"3_byte_start",
         {0x010000},
         {},
         {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"3_byte_end",
         {0xffffff},
         {},
         {0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"4_byte_start",
         {0x01000000},
         {},
         {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}},
        {"4_byte_end",
         {0xffffffff},
         {},
         {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00}},
        {"8_byte_start",
         {0x0100000000000000},
         {},
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
        {"8_byte_end",
         {0xffffffffffffffff},
         {},
         {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryLongLongTest,
                         ::testing::ValuesIn(codec_binary_longlong_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Datetime

using CodecBinaryDatetimeTest = CodecTest<classic_protocol::binary::DateTime>;

TEST_P(CodecBinaryDatetimeTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryDatetimeTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::DateTime>
    codec_binary_datetime_param[] = {
        {"empty", {/* default */}, {}, {}},
        {"full",
         {2010, 10, 17, 19, 27, 30, 1},
         {},
         {0xda, 0x07, 0x0a, 0x11, 0x13, 0x1b, 0x1e, 0x01, 0x00, 0x00, 0x00}},
        {"no_microsec",
         {2010, 10, 17, 19, 27, 30},
         {},
         {0xda, 0x07, 0x0a, 0x11, 0x13, 0x1b, 0x1e}},
        {"no_time",  // date
         {2010, 10, 17},
         {},
         {0xda, 0x07, 0x0a, 0x11}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryDatetimeTest,
                         ::testing::ValuesIn(codec_binary_datetime_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

// Time

using CodecBinaryTimeTest = CodecTest<classic_protocol::binary::Time>;

TEST_P(CodecBinaryTimeTest, encode) { test_encode(GetParam()); }
TEST_P(CodecBinaryTimeTest, decode) { test_decode(GetParam()); }

const CodecParam<classic_protocol::binary::Time> codec_binary_time_param[] = {
    {"empty", {/* default */}, {}, {}},
    {"full",
     {false, 120, 19, 27, 30, 1},
     {},
     {0x00, 0x78, 0x00, 0x00, 0x00, 0x13, 0x1b, 0x1e, 0x01, 0x00, 0x00, 0x00}},
    {"full_negative",
     {true, 120, 19, 27, 30, 1},
     {},
     {0x01, 0x78, 0x00, 0x00, 0x00, 0x13, 0x1b, 0x1e, 0x01, 0x00, 0x00, 0x00}},
    {"no_microsec",
     {false, 120, 19, 27, 30},
     {},
     {0x00, 0x78, 0x00, 0x00, 0x00, 0x13, 0x1b, 0x1e}},
    {"no_microsec_negative",
     {true, 120, 19, 27, 30},
     {},
     {0x01, 0x78, 0x00, 0x00, 0x00, 0x13, 0x1b, 0x1e}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CodecBinaryTimeTest,
                         ::testing::ValuesIn(codec_binary_time_param),
                         [](auto const &test_param_info) {
                           return test_param_info.param.test_name;
                         });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
