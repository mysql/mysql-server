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

#include <cstdint>  // uint16_t
#include <string>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "mysql/harness/config_option.h"

// bool

template <class T>
struct OptionParam {
  std::string test_name;

  std::string input;
  T expected_value;
};

struct OptionFailParam {
  std::string test_name;

  std::string input;
  std::string expected_error_msg;
};

using BoolOptionParam = OptionParam<bool>;

class BoolOptionTest : public ::testing::Test,
                       public ::testing::WithParamInterface<BoolOptionParam> {};

TEST_P(BoolOptionTest, check) {
  mysql_harness::BoolOption option;

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

static const BoolOptionParam bool_option_params[] = {
    {"zero", "0", false},
    {"one", "1", true},
    {"true", "true", true},
    {"false", "false", false},
};

INSTANTIATE_TEST_SUITE_P(Spec, BoolOptionTest,
                         ::testing::ValuesIn(bool_option_params));

class BoolOptionFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OptionFailParam> {};

TEST_P(BoolOptionFailTest, fails) {
  mysql_harness::BoolOption option;

  try {
    option(GetParam().input, "some_option");
    FAIL();
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  }
}

static const OptionFailParam bool_option_fail_params[] = {
    {"empty", "",
     "some_option needs a value of either 0, 1, false or true, was ''"},
    {"negative", "-1",
     "some_option needs a value of either 0, 1, false or true, was '-1'"},
    {"too_large", "2",
     "some_option needs a value of either 0, 1, false or true, was '2'"},
};

INSTANTIATE_TEST_SUITE_P(Spec, BoolOptionFailTest,
                         ::testing::ValuesIn(bool_option_fail_params));

// double

using DoubleOptionParam = OptionParam<double>;

class DoubleOptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<DoubleOptionParam> {};

TEST_P(DoubleOptionTest, check) {
  mysql_harness::DoubleOption option{-10000, 10000};

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

static const DoubleOptionParam double_option_params[] = {
    {"positive", "1", 1.0},
    {"negative", "-1", -1.0},
    {"milli", "0.001", 0.001},
    {"kilo_e", "1e3", 1000},
    {"positive_in_range", "10000", 10000},
    {"negative_in_range", "-10000", -10000},
};

INSTANTIATE_TEST_SUITE_P(Spec, DoubleOptionTest,
                         ::testing::ValuesIn(double_option_params));

// double::fail

class DoubleOptionFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OptionFailParam> {};

TEST_P(DoubleOptionFailTest, fails) {
  mysql_harness::DoubleOption option{-1, 1};

  try {
    option(GetParam().input, "some_option");
    FAIL();
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  }
}

static const OptionFailParam double_option_fail_params[] = {
    {"empty", "", "some_option needs value between -1 and 1 inclusive, was ''"},
    {"positive_out_of_range", "1.001",
     "some_option needs value between -1 and 1 inclusive, was '1.001'"},
    {"negative_out_of_range", "-1.001",
     "some_option needs value between -1 and 1 inclusive, was '-1.001'"},
};

INSTANTIATE_TEST_SUITE_P(Spec, DoubleOptionFailTest,
                         ::testing::ValuesIn(double_option_fail_params));

// string

using StringOptionParam = OptionParam<std::string>;

class StringOptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<StringOptionParam> {};

TEST_P(StringOptionTest, check) {
  mysql_harness::StringOption option;

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

static const StringOptionParam string_option_params[] = {
    {"positive", "1", "1"},
    {"negative", "-1", "-1"},
    {"empty", "", ""},
};

INSTANTIATE_TEST_SUITE_P(Spec, StringOptionTest,
                         ::testing::ValuesIn(string_option_params));

// uint16_t

using Int16OptionParam = OptionParam<uint16_t>;

class Int16OptionTest : public ::testing::Test,
                        public ::testing::WithParamInterface<Int16OptionParam> {
};

TEST_P(Int16OptionTest, check) {
  mysql_harness::IntOption<uint16_t> option;

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

static const Int16OptionParam uint16_option_params[] = {
    {"zero", "0", 0},
    {"positive", "1", 1},
    {"kilo_e", "1000", 1000},
    {"positive_in_range", "65535", 65535},
};

INSTANTIATE_TEST_SUITE_P(Spec, Int16OptionTest,
                         ::testing::ValuesIn(uint16_option_params));

// uint16::fail

class Int16OptionFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OptionFailParam> {};

TEST_P(Int16OptionFailTest, fails) {
  mysql_harness::IntOption<uint16_t> option;

  try {
    option(GetParam().input, "some_option");
    FAIL();
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  }
}

static const OptionFailParam uint16_option_fail_params[] = {
    {"empty", "",
     "some_option needs value between 0 and 65535 inclusive, was ''"},
    {"float", "1e6",
     "some_option needs value between 0 and 65535 inclusive, was '1e6'"},
    {"positive_out_of_range", "65536",
     "some_option needs value between 0 and 65535 inclusive, was '65536'"},
    {"negative_out_of_range", "-1",
     "some_option needs value between 0 and 65535 inclusive, was '-1'"},
};

INSTANTIATE_TEST_SUITE_P(Spec, Int16OptionFailTest,
                         ::testing::ValuesIn(uint16_option_fail_params));

// Int<bool>

using IntBoolOptionParam = OptionParam<bool>;

class IntBoolOptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<IntBoolOptionParam> {};

TEST_P(IntBoolOptionTest, check) {
  mysql_harness::IntOption<bool> option;

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

static const IntBoolOptionParam int_bool_option_params[] = {
    {"zero", "0", false},
    {"one", "1", true},
};

INSTANTIATE_TEST_SUITE_P(Spec, IntBoolOptionTest,
                         ::testing::ValuesIn(int_bool_option_params));

// uintBool::fail

class IntBoolOptionFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OptionFailParam> {};

TEST_P(IntBoolOptionFailTest, fails) {
  mysql_harness::IntOption<bool> option;

  try {
    option(GetParam().input, "some_option");
    FAIL();
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  }
}

static const OptionFailParam int_bool_option_fail_params[] = {
    {"empty", "", "some_option needs value between 0 and 1 inclusive, was ''"},
    {"true", "true",
     "some_option needs value between 0 and 1 inclusive, was 'true'"},
    {"false", "false",
     "some_option needs value between 0 and 1 inclusive, was 'false'"},
    {"positive_out_of_range", "2",
     "some_option needs value between 0 and 1 inclusive, was '2'"},
    {"negative_out_of_range", "-1",
     "some_option needs value between 0 and 1 inclusive, was '-1'"},
};

INSTANTIATE_TEST_SUITE_P(Spec, IntBoolOptionFailTest,
                         ::testing::ValuesIn(int_bool_option_fail_params));

// milliseconds

using MilliSecondsOptionParam = OptionParam<std::chrono::milliseconds>;

class MilliSecondsOptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<MilliSecondsOptionParam> {};

TEST_P(MilliSecondsOptionTest, check) {
  mysql_harness::MilliSecondsOption option{-10000, 10000};

  EXPECT_EQ(option(GetParam().input, "some_option"), GetParam().expected_value);
}

using namespace std::chrono_literals;

static const MilliSecondsOptionParam millisec_option_params[] = {
    {"positive", "1", 1000ms},
    {"negative", "-1", -1000ms},
    {"milli", "0.001", 1ms},
    {"kilo_e", "1e3", 1000s},
    {"positive_in_range", "10000", 10000s},
    {"negative_in_range", "-10000", -10000s},
};

INSTANTIATE_TEST_SUITE_P(Spec, MilliSecondsOptionTest,
                         ::testing::ValuesIn(millisec_option_params));

// millisec::fail

class MilliSecondsOptionFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OptionFailParam> {};

TEST_P(MilliSecondsOptionFailTest, fails) {
  mysql_harness::MilliSecondsOption option{-1, 1};

  try {
    option(GetParam().input, "some_option");
    FAIL();
  } catch (const std::exception &e) {
    EXPECT_EQ(e.what(), GetParam().expected_error_msg);
  }
}

static const OptionFailParam millisec_option_fail_params[] = {
    {"empty", "", "some_option needs value between -1 and 1 inclusive, was ''"},
    {"positive_out_of_range", "1.001",
     "some_option needs value between -1 and 1 inclusive, was '1.001'"},
    {"negative_out_of_range", "-1.001",
     "some_option needs value between -1 and 1 inclusive, was '-1.001'"},
};

INSTANTIATE_TEST_SUITE_P(Spec, MilliSecondsOptionFailTest,
                         ::testing::ValuesIn(millisec_option_fail_params));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
