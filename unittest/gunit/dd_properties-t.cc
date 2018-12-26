/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/properties.h"
#include "unittest/gunit/test_utils.h"

namespace dd_properties_unittest {

/**
  The fixture for testing the dd::Properties and Properties_impl classes.
  A fresh instance of this class will be created for each of the
  TEST_F functions below.
  The functions SetUp(), TearDown(), SetUpTestCase(), TearDownTestCase() are
  inherited from ::testing::Test (google naming style differs from MySQL).
*/

class PropertiesTest : public ::testing::Test {
 protected:
  PropertiesTest() {}

  virtual void SetUp() { m_props = new dd::Properties_impl(); }

  virtual void TearDown() { delete m_props; }

  static void SetUpTestCase() {}

  static void TearDownTestCase() {}

  dd::Properties *m_props;

 private:
  // Declares (but does not define) copy constructor and assignment operator.
  GTEST_DISALLOW_COPY_AND_ASSIGN_(PropertiesTest);
};

// Tests that valid option parsing is handled
TEST_F(PropertiesTest, ValidStringParsing) {
  dd::Properties *p = dd::Properties_impl::parse_properties("a=b;b=c");
  EXPECT_TRUE(p->value("a") == "b");
  EXPECT_TRUE(p->value("b") == "c");
  EXPECT_TRUE(p->raw_string() == "a=b;b=c;");
  delete p;

  p = dd::Properties_impl::parse_properties("a=b;b=c;");
  EXPECT_TRUE(p->value("a") == "b");
  EXPECT_TRUE(p->value("b") == "c");
  EXPECT_TRUE(p->raw_string() == "a=b;b=c;");
  delete p;

  p = dd::Properties_impl::parse_properties("\\=a=\\;b;b\\==\\=c");
  EXPECT_TRUE(p->value("=a") == ";b");
  EXPECT_TRUE(p->value("b=") == "=c");
  EXPECT_TRUE(p->raw_string() == "\\=a=\\;b;b\\==\\=c;");
  delete p;

  p = dd::Properties_impl::parse_properties("");
  EXPECT_TRUE(p != NULL);
  EXPECT_FALSE(p->exists(""));
  delete p;

  p = dd::Properties_impl::parse_properties("a=;");
  EXPECT_TRUE(p != NULL);
  EXPECT_TRUE(p->value("a") == "");
  delete p;
}

// Tests that option parsing errors are handled
TEST_F(PropertiesTest, FailingStringParsing) {
  dd::Properties *p = dd::Properties_impl::parse_properties("a");
  EXPECT_TRUE(p == NULL);
  delete p;

  p = dd::Properties_impl::parse_properties("a");
  EXPECT_TRUE(p == NULL);
  delete p;

  p = dd::Properties_impl::parse_properties(";");
  EXPECT_TRUE(p == NULL);
  delete p;

  p = dd::Properties_impl::parse_properties("a\\=b");
  EXPECT_TRUE(p == NULL);
  delete p;

  p = dd::Properties_impl::parse_properties("=");
  EXPECT_TRUE(p == NULL);
  delete p;

  p = dd::Properties_impl::parse_properties("=a");
  EXPECT_TRUE(p == NULL);
  delete p;
}

// Tests empty value behavior
TEST_F(PropertiesTest, EmptyValue) {
  dd::Properties *p = dd::Properties_impl::parse_properties("k=;");
  dd::String_type string_value("");

  EXPECT_TRUE(p->value("k") == "");
  EXPECT_TRUE(!p->get("k", string_value) && string_value == "");
  delete p;
}

// Tests utf-8 behavior
TEST_F(PropertiesTest, UTF8) {
  const dd::String_type EUR("\xE2\x82\xAC");
  const dd::String_type CNY("\xe5\x85\x83");
  const dd::String_type JPY("\xe5\x86\x86");
  const dd::String_type GBP("\xc2\xa3");
  const dd::String_type USD("\x24");
  const dd::String_type CNT("\xC2\xA2");

  // Parse a utf-8 string and add more key=value pairs
  dd::Properties *p = dd::Properties_impl::parse_properties("EUR=" + EUR);
  p->set("CNY", CNY);
  p->set("JPY", JPY);
  p->set("GBP", GBP);
  p->set("USD", USD);

  p->set("1/100 " + EUR, "Cent, but not " + CNT);
  p->set("1/100 " + GBP, "Pence, whatever that symbol is");
  p->set("1/100 " + CNY, "Who knows, but not " + JPY + " for sure");
  p->set("1/100 " + USD, "Half of my 2 " + CNT + "s worth");

  EXPECT_TRUE(p->value("EUR") == EUR);
  EXPECT_TRUE(p->value("1/100 " + p->value("EUR")) == "Cent, but not " + CNT);

  EXPECT_TRUE(p->value("CNY") == CNY);
  EXPECT_TRUE(p->value("1/100 " + p->value("CNY")) ==
              "Who knows, but not " + JPY + " for sure");

  EXPECT_TRUE(p->value("JPY") == JPY);

  EXPECT_TRUE(p->value("GBP") == GBP);
  EXPECT_TRUE(p->value("1/100 " + p->value("GBP")) ==
              "Pence, whatever that symbol is");

  EXPECT_TRUE(p->value("USD") == USD);
  EXPECT_TRUE(p->value("1/100 " + p->value("USD")) ==
              "Half of my 2 " + CNT + "s worth");
  delete p;
}

// Tests setting and getting options
TEST_F(PropertiesTest, SetGetStrings) {
  dd::Properties *p = new dd::Properties_impl();
  dd::String_type str("");
  const char *value = NULL;

  p->set("a", "b");
  EXPECT_TRUE(p->value("a") == "b");
  EXPECT_TRUE(!p->get("a", str) && str == "b");
  value = p->value_cstr("a");
  EXPECT_TRUE(strcmp(value, "b") == 0);
  EXPECT_TRUE(strlen(value) == 1);
  EXPECT_TRUE(value[strlen(value)] == '\0');

  p->set("b=", "c;");
  EXPECT_TRUE(p->value("b=") == "c;");
  EXPECT_TRUE(!p->get("b=", str) && str == "c;");
  value = p->value_cstr("b=");
  EXPECT_TRUE(strcmp(value, "c;") == 0);
  EXPECT_TRUE(strlen(value) == 2);
  EXPECT_TRUE(value[strlen(value)] == '\0');

  p->set("d\\=", "e\\;");
  EXPECT_TRUE(p->value("d\\=") == "e\\;");
  EXPECT_TRUE(!p->get("d\\=", str) && str == "e\\;");
  value = p->value_cstr("d\\=");
  EXPECT_TRUE(strcmp(value, "e\\;") == 0);
  EXPECT_TRUE(strlen(value) == 3);
  EXPECT_TRUE(value[strlen(value)] == '\0');

  p->set(";f", "=g");
  EXPECT_TRUE(p->value(";f") == "=g");
  EXPECT_TRUE(!p->get(";f", str) && str == "=g");
  value = p->value_cstr(";f");
  EXPECT_TRUE(strcmp(value, "=g") == 0);
  EXPECT_TRUE(strlen(value) == 2);
  EXPECT_TRUE(value[strlen(value)] == '\0');

  p->set("\\;h", "\\=i");
  EXPECT_TRUE(p->value("\\;h") == "\\=i");
  EXPECT_TRUE(!p->get("\\;h", str) && str == "\\=i");
  value = p->value_cstr("\\;h");
  EXPECT_TRUE(strcmp(value, "\\=i") == 0);
  EXPECT_TRUE(strlen(value) == 3);
  EXPECT_TRUE(value[strlen(value)] == '\0');

  // Key "" is illegal and will not be added
  p->set("", "");
  EXPECT_TRUE(p->get("", str));
  EXPECT_FALSE(p->exists(""));
  EXPECT_TRUE(p->remove(""));

  EXPECT_TRUE(p->raw_string() ==
              "\\;f=\\=g;\\\\\\;h=\\\\\\=i;a=b;b\\==c\\;;d\\\\\\==e\\\\\\;;");

  // Create another object with the raw string as input
  dd::Properties *p_copy =
      dd::Properties_impl::parse_properties(p->raw_string());
  // Verify same values
  EXPECT_TRUE(p->value("a") == p_copy->value("a"));
  EXPECT_TRUE(strcmp(p->value_cstr("a"), p_copy->value_cstr("a")) == 0);
  EXPECT_TRUE(p->value("b=") == p_copy->value("b="));
  EXPECT_TRUE(strcmp(p->value_cstr("b="), p_copy->value_cstr("b=")) == 0);
  EXPECT_TRUE(p->value("d\\=") == p_copy->value("d\\="));
  EXPECT_TRUE(strcmp(p->value_cstr("d\\="), p_copy->value_cstr("d\\=")) == 0);
  EXPECT_TRUE(p->value(";f") == p_copy->value(";f"));
  EXPECT_TRUE(strcmp(p->value_cstr(";f"), p_copy->value_cstr(";f")) == 0);
  EXPECT_TRUE(p->value("\\;h") == p_copy->value("\\;h"));
  EXPECT_TRUE(strcmp(p->value_cstr("\\;h"), p_copy->value_cstr("\\;h")) == 0);

  EXPECT_TRUE(p->raw_string() == p_copy->raw_string());

  delete p;
  delete p_copy;
}

// Tests valid setting and getting of int and bool options
TEST_F(PropertiesTest, ValidSetGetIntBool) {
  int64 val1_int64 = 0;
  int64 val2_int64 = 0;
  uint64 val1_uint64 = 0;
  uint64 val2_uint64 = 0;
  int32 val1_int32 = 0;
  int32 val2_int32 = 0;
  uint32 val1_uint32 = 0;
  uint32 val2_uint32 = 0;

  const char *MAX_INT64_STR = "9223372036854775807";
  const int64 MAX_INT64 = std::numeric_limits<int64>::max();
  const char *MIN_INT64_STR = "-9223372036854775808";
  const int64 MIN_INT64 = std::numeric_limits<int64>::min();

  const char *MAX_UINT64_STR = "18446744073709551615";
  const uint64 MAX_UINT64 = std::numeric_limits<uint64>::max();
  const char *MIN_UINT64_STR = "0";
  const uint64 MIN_UINT64 = 0;

  const char *MAX_INT32_STR = "2147483647";
  const int32 MAX_INT32 = std::numeric_limits<int32>::max();
  const char *MIN_INT32_STR = "-2147483648";
  const int32 MIN_INT32 = std::numeric_limits<int32>::min();

  const char *MAX_UINT32_STR = "4294967295";
  const uint32 MAX_UINT32 = std::numeric_limits<uint32>::max();
  const char *MIN_UINT32_STR = "0";
  const uint32 MIN_UINT32 = 0;

  dd::Properties *p = new dd::Properties_impl();

  // Set and get as strings and ints, int64
  p->set("str_int64", MAX_INT64_STR);
  p->set_int64("num_int64", MAX_INT64);

  EXPECT_TRUE(p->value("str_int64") == p->value("num_int64") &&
              p->value("str_int64") == MAX_INT64_STR);
  EXPECT_TRUE(!p->get_int64("str_int64", &val1_int64) &&
              !p->get_int64("num_int64", &val2_int64));
  EXPECT_TRUE(val1_int64 == val2_int64 && val1_int64 == MAX_INT64);

  p->set("str_int64", MIN_INT64_STR);
  p->set_int64("num_int64", MIN_INT64);

  EXPECT_TRUE(p->value("str_int64") == p->value("num_int64") &&
              p->value("str_int64") == MIN_INT64_STR);
  EXPECT_TRUE(!p->get_int64("str_int64", &val1_int64) &&
              !p->get_int64("num_int64", &val2_int64));
  EXPECT_TRUE(val1_int64 == val2_int64 && val1_int64 == MIN_INT64);

  // Set and get as strings and ints, uint64
  p->set("str_uint64", MAX_UINT64_STR);
  p->set_uint64("num_uint64", MAX_UINT64);

  EXPECT_TRUE(p->value("str_uint64") == p->value("num_uint64") &&
              p->value("str_uint64") == MAX_UINT64_STR);
  EXPECT_TRUE(!p->get_uint64("str_uint64", &val1_uint64) &&
              !p->get_uint64("num_uint64", &val2_uint64));
  EXPECT_TRUE(val1_uint64 == val2_uint64 && val1_uint64 == MAX_UINT64);

  p->set("str_uint64", MIN_UINT64_STR);
  p->set_uint64("num_uint64", MIN_UINT64);

  EXPECT_TRUE(p->value("str_uint64") == p->value("num_uint64") &&
              p->value("str_uint64") == MIN_UINT64_STR);
  EXPECT_TRUE(!p->get_uint64("str_uint64", &val1_uint64) &&
              !p->get_uint64("num_uint64", &val2_uint64));
  EXPECT_TRUE(val1_uint64 == val2_uint64 && val1_uint64 == MIN_UINT64);

  // Set and get as strings and ints, int32
  p->set("str_int32", MAX_INT32_STR);
  p->set_int32("num_int32", MAX_INT32);

  EXPECT_TRUE(p->value("str_int32") == p->value("num_int32") &&
              p->value("str_int32") == MAX_INT32_STR);
  EXPECT_TRUE(!p->get_int32("str_int32", &val1_int32) &&
              !p->get_int32("num_int32", &val2_int32));
  EXPECT_TRUE(val1_int32 == val2_int32 && val1_int32 == MAX_INT32);

  p->set("str_int32", MIN_INT32_STR);
  p->set_int32("num_int32", MIN_INT32);

  EXPECT_TRUE(p->value("str_int32") == p->value("num_int32") &&
              p->value("str_int32") == MIN_INT32_STR);
  EXPECT_TRUE(!p->get_int32("str_int32", &val1_int32) &&
              !p->get_int32("num_int32", &val2_int32));
  EXPECT_TRUE(val1_int32 == val2_int32 && val1_int32 == MIN_INT32);

  // Set and get as strings and ints, uint32
  p->set("str_uint32", MAX_UINT32_STR);
  p->set_uint32("num_uint32", MAX_UINT32);

  EXPECT_TRUE(p->value("str_uint32") == p->value("num_uint32") &&
              p->value("str_uint32") == MAX_UINT32_STR);
  EXPECT_TRUE(!p->get_uint32("str_uint32", &val1_uint32) &&
              !p->get_uint32("num_uint32", &val2_uint32));
  EXPECT_TRUE(val1_uint32 == val2_uint32 && val1_uint32 == MAX_UINT32);

  p->set("str_uint32", MIN_UINT32_STR);
  p->set_uint32("num_uint32", MIN_UINT32);

  EXPECT_TRUE(p->value("str_uint32") == p->value("num_uint32") &&
              p->value("str_uint32") == MIN_UINT32_STR);
  EXPECT_TRUE(!p->get_uint32("str_uint32", &val1_uint32) &&
              !p->get_uint32("num_uint32", &val2_uint32));
  EXPECT_TRUE(val1_uint32 == val2_uint32 && val1_uint32 == MIN_UINT32);

  // Set and get as strings and bool
  bool maybe = false;

  p->set_bool("bool", true);
  EXPECT_TRUE(p->value("bool") == "1");
  EXPECT_TRUE(!p->get_bool("bool", &maybe) && maybe == true);
  EXPECT_TRUE(!p->get_int64("bool", &val1_int64) && val1_int64 == 1);
  EXPECT_TRUE(!p->get_uint64("bool", &val1_uint64) && val1_uint64 == 1);
  EXPECT_TRUE(!p->get_int32("bool", &val1_int32) && val1_int32 == 1);
  EXPECT_TRUE(!p->get_uint32("bool", &val1_uint32) && val1_uint32 == 1);
  p->set_bool("bool", false);
  EXPECT_TRUE(p->value("bool") == "0");
  EXPECT_TRUE(!p->get_bool("bool", &maybe) && maybe == false);
  EXPECT_TRUE(!p->get_int64("bool", &val1_int64) && val1_int64 == 0);
  EXPECT_TRUE(!p->get_uint64("bool", &val1_uint64) && val1_uint64 == 0);
  EXPECT_TRUE(!p->get_int32("bool", &val1_int32) && val1_int32 == 0);
  EXPECT_TRUE(!p->get_uint32("bool", &val1_uint32) && val1_uint32 == 0);
  p->remove("bool");

  p->set_int64("str_int_bool", 0);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);
  p->set_int64("str_int_bool", 1);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_int64("str_int_bool", MAX_INT64);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_int64("str_int_bool", MIN_INT64);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);

  p->set_uint64("str_int_bool", 0);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);
  p->set_uint64("str_int_bool", 1);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_uint64("str_int_bool", MAX_UINT64);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_uint64("str_int_bool", MIN_UINT64);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);

  p->set_int32("str_int_bool", 0);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);
  p->set_int32("str_int_bool", 1);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_int32("str_int_bool", MAX_INT32);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_int32("str_int_bool", MIN_INT32);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);

  p->set_uint32("str_int_bool", 0);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);
  p->set_uint32("str_int_bool", 1);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_uint32("str_int_bool", MAX_UINT32);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == true);
  p->set_uint32("str_int_bool", MIN_UINT32);
  EXPECT_TRUE(!p->get_bool("str_int_bool", &maybe) && maybe == false);

  EXPECT_TRUE(p->raw_string() ==
              "num_int32=-2147483648;num_int64=-9223372036854775808;num_uint32="
              "0;num_uint64=0;str_int32=-2147483648;str_int64=-"
              "9223372036854775808;str_int_bool=0;str_uint32=0;str_uint64=0;");

  delete p;
}

#if !defined(DBUG_OFF)

// Tests invalid setting and getting of int and bool options
typedef PropertiesTest PropertiesTestDeathTest;

TEST_F(PropertiesTestDeathTest, FailingSetGetIntBool) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int32 val_int32 = 0;
  uint32 val_uint32 = 0;
  int64 val_int64 = 0;
  uint64 val_uint64 = 0;

  const char *OFL_INT64_STR = "9223372036854775808";
  const char *UFL_INT64_STR = "-9223372036854775809";

  const char *OFL_UINT64_STR = "18446744073709551616";

  const char *OFL_INT32_STR = "2147483648";
  const char *UFL_INT32_STR = "-2147483649";

  const char *OFL_UINT32_STR = "4294967296";

  dd::Properties *p = new dd::Properties_impl();

  p->set("num_int64", OFL_INT64_STR);
  EXPECT_TRUE(p->value("num_int64") == OFL_INT64_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_int64("num_int64", &val_int64),
                            ".*UTC - mysqld got.*");

  p->set("num_int64", UFL_INT64_STR);
  EXPECT_TRUE(p->value("num_int64") == UFL_INT64_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_int64("num_int64", &val_int64),
                            ".*UTC - mysqld got.*");

  p->set("num_uint64", OFL_UINT64_STR);
  EXPECT_TRUE(p->value("num_uint64") == OFL_UINT64_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_uint64("num_uint64", &val_uint64),
                            ".*UTC - mysqld got.*");

  p->set("num_int32", OFL_INT32_STR);
  EXPECT_TRUE(p->value("num_int32") == OFL_INT32_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_int32("num_int32", &val_int32),
                            ".*UTC - mysqld got.*");

  p->set("num_int32", UFL_INT32_STR);
  EXPECT_TRUE(p->value("num_int32") == UFL_INT32_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_int32("num_int32", &val_int32),
                            ".*UTC - mysqld got.*");

  p->set("num_uint32", OFL_UINT32_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_uint32("num_uint32", &val_uint32),
                            ".*UTC - mysqld got.*");

  // The overflowed unit32 may be retrieved as 64 bit
  EXPECT_TRUE(!p->get_uint64("num_uint32", &val_uint64) &&
              !p->get_int64("num_uint32", &val_int64));

  // An overflowing 64 bit int should not be accepted as a bool
  bool maybe = false;

  p->set("bool", OFL_UINT64_STR);
  EXPECT_TRUE(p->value("bool") == OFL_UINT64_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_uint64("bool", &val_uint64),
                            ".*UTC - mysqld got.*");
  EXPECT_DEATH_IF_SUPPORTED(p->get_bool("bool", &maybe),
                            ".*UTC - mysqld got.*");

  p->set("bool", UFL_INT64_STR);
  EXPECT_TRUE(p->value("bool") == UFL_INT64_STR);
  EXPECT_DEATH_IF_SUPPORTED(p->get_int64("bool", &val_int64),
                            ".*UTC - mysqld got.*");
  EXPECT_DEATH_IF_SUPPORTED(p->get_bool("bool", &maybe),
                            ".*UTC - mysqld got.*");
  p->remove("bool");

  // Integers with empty keys and non existing keys.
  p->set_int64("", 0);
  EXPECT_FALSE(p->exists(""));
  EXPECT_DEATH_IF_SUPPORTED(p->get_int64("non_existing", &val_int64),
                            ".*UTC - mysqld got.*");
  EXPECT_DEATH_IF_SUPPORTED(p->get_int64("", &val_int64),
                            ".*UTC - mysqld got.*");
  EXPECT_DEATH_IF_SUPPORTED(p->value(""), ".*UTC - mysqld got.*");
  EXPECT_TRUE(p->remove(""));

  EXPECT_TRUE(p->raw_string() ==
              "num_int32=-2147483649;num_int64=-9223372036854775809;num_uint32="
              "4294967296;num_uint64=18446744073709551616;");

  delete p;
}

#endif  // DBUG_OFF

// Tests the exists function
TEST_F(PropertiesTest, OptionsExist) {
  dd::Properties *p = new dd::Properties_impl();
  EXPECT_FALSE(p->exists(""));
  EXPECT_FALSE(p->exists("a"));

  p->set("", "");
  EXPECT_FALSE(p->exists(""));

  p->set("empty", "");
  EXPECT_TRUE(p->exists("empty"));
  EXPECT_TRUE(p->value("empty") == "");

  p->set("a", "b");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "b");

  // Create another object with the raw string as input
  dd::Properties *p_copy =
      dd::Properties_impl::parse_properties(p->raw_string());
  EXPECT_TRUE(p->exists("empty"));
  EXPECT_TRUE(p->value("empty") == "");

  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "b");

  delete p;
  delete p_copy;
}

// Tests replacing values
TEST_F(PropertiesTest, ReplaceValues) {
  dd::Properties *p = new dd::Properties_impl();
  EXPECT_FALSE(p->exists(""));
  EXPECT_FALSE(p->exists("a"));

  p->set("empty", "");
  EXPECT_TRUE(p->exists("empty"));
  EXPECT_TRUE(p->value("empty") == "");

  p->set("empty too", "");
  EXPECT_TRUE(p->exists("empty too"));
  EXPECT_TRUE(p->value("empty too") == "");

  p->set("empty", " ");
  EXPECT_TRUE(p->exists("empty"));
  EXPECT_TRUE(p->value("empty") == " ");

  p->set("a", "b");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "b");

  p->set("a", "b too");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "b too");

  p->set("a", "");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "");

  EXPECT_TRUE(p->raw_string() == "a=;empty= ;empty too=;");

  delete p;
}

// Tests removing options
TEST_F(PropertiesTest, RemoveOptions) {
  dd::Properties *p = new dd::Properties_impl();

  EXPECT_FALSE(p->exists(""));
  EXPECT_FALSE(p->exists("a"));

  EXPECT_TRUE(p->remove(""));
  EXPECT_TRUE(p->remove("a"));

  p->set("a", "");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "");
  EXPECT_FALSE(p->remove("a"));
  EXPECT_FALSE(p->exists("a"));
  EXPECT_TRUE(p->remove("a"));

  p->set("a", "b");
  EXPECT_TRUE(p->exists("a"));
  EXPECT_TRUE(p->value("a") == "b");
  EXPECT_FALSE(p->remove("a"));
  EXPECT_FALSE(p->exists("a"));
  EXPECT_TRUE(p->remove("a"));

  EXPECT_TRUE(p->raw_string() == "");

  delete p;
}

// Tests iterating over options
TEST_F(PropertiesTest, IterationSize) {
  dd::Properties *p = new dd::Properties_impl();

  EXPECT_TRUE(p->size() == 0);

  p->set("a", "b");
  EXPECT_TRUE(p->size() == 1);

  p->set("a", "b too");
  EXPECT_TRUE(p->size() == 1);

  p->set("b", "c");
  EXPECT_TRUE(p->size() == 2);

  p->set("c", "d");
  EXPECT_TRUE(p->size() == 3);

  EXPECT_FALSE(p->remove("a"));
  EXPECT_TRUE(p->size() == 2);
  EXPECT_TRUE(p->remove("a"));
  EXPECT_TRUE(p->size() == 2);

  p->set("", "");
  EXPECT_TRUE(p->size() == 2);

  int i = 0;
  for (dd::Properties::Iterator it = p->begin(); it != p->end(); ++it, ++i)
    if (it->first == "b")
      EXPECT_TRUE(it->second == "c");
    else if (it->first == "c")
      EXPECT_TRUE(it->second == "d");
    else
      EXPECT_TRUE(false);

  EXPECT_TRUE(i == 2);

  EXPECT_FALSE(p->remove("b"));
  EXPECT_FALSE(p->remove("c"));
  EXPECT_TRUE(p->remove(""));

  EXPECT_TRUE(p->size() == 0);

  for (dd::Properties::Iterator it = p->begin(); it != p->end(); ++it)
    EXPECT_TRUE(false);

  delete p;
}

// Tests that valid integer conversions are handled
TEST_F(PropertiesTest, ValidIntConversions) {
  int32 val_int32 = 0;
  uint32 val_uint32 = 0;
  int64 val_int64 = 0;
  uint64 val_uint64 = 0;

  const char *MAX_INT64_STR = "9223372036854775807";
  const int64 MAX_INT64 = std::numeric_limits<int64>::max();
  const char *MIN_INT64_STR = "-9223372036854775808";
  const int64 MIN_INT64 = std::numeric_limits<int64>::min();

  const char *MAX_UINT64_STR = "18446744073709551615";
  const uint64 MAX_UINT64 = std::numeric_limits<uint64>::max();
  const char *MIN_UINT64_STR = "0";
  const uint64 MIN_UINT64 = 0;

  const char *MAX_INT32_STR = "2147483647";
  const int32 MAX_INT32 = std::numeric_limits<int32>::max();
  const char *MIN_INT32_STR = "-2147483648";
  const int32 MIN_INT32 = std::numeric_limits<int32>::min();

  const char *MAX_UINT32_STR = "4294967295";
  const uint32 MAX_UINT32 = std::numeric_limits<uint32>::max();
  const char *MIN_UINT32_STR = "0";
  const uint32 MIN_UINT32 = 0;

  dd::Properties *p = new dd::Properties_impl();

  // ======================================
  // Positive test cases
  // ======================================
  // TC#   Operation    Source      Target
  // -----+------------+-----------+-------
  // PTC1  Convert      +num        int64
  // PTC2  Convert      -num        int64
  // PTC3  Convert      0           int64
  // PTC4  Convert      MAX_INT64   int64
  // PTC5  Convert      MIN_INT64   int64
  // -----+------------+-----------+-------
  // PTC6  Convert      +num        uint64
  // PTC7  Convert      0           uint64
  // PTC8  Convert      MAX_UINT32  uint64
  // PTC9  Convert      MIN_UINT32  uint64
  // -----+------------+-----------+-------
  // PTC10 Convert      +num        int32
  // PTC11 Convert      -num        int32
  // PTC12 Convert      0           int32
  // PTC13 Convert      MAX_INT32   int32
  // PTC14 Convert      MIN_INT32   int32
  // -----+------------+-----------+-------
  // PTC15 Convert      +num        uint32
  // PTC16 Convert      0           uint32
  // PTC17 Convert      MAX_UINT32  uint32
  // PTC18 Convert      MIN_UINT32  uint32
  // ======================================

  // PTC1
  EXPECT_TRUE(!p->to_int64("123", &val_int64) && val_int64 == 123);
  // PTC2
  EXPECT_TRUE(!p->to_int64("-123", &val_int64) && val_int64 == -123);
  // PTC3
  EXPECT_TRUE(!p->to_int64("0", &val_int64) && val_int64 == 0);
  // PTC4
  EXPECT_TRUE(!p->to_int64(MAX_INT64_STR, &val_int64) &&
              val_int64 == MAX_INT64);
  // PTC5
  EXPECT_TRUE(!p->to_int64(MIN_INT64_STR, &val_int64) &&
              val_int64 == MIN_INT64);

  // PTC6
  EXPECT_TRUE(!p->to_uint64("123", &val_uint64) && val_uint64 == 123);
  // PTC7
  EXPECT_TRUE(!p->to_uint64("0", &val_uint64) && val_uint64 == 0);
  // PTC8
  EXPECT_TRUE(!p->to_uint64(MAX_UINT64_STR, &val_uint64) &&
              val_uint64 == MAX_UINT64);
  // PTC9
  EXPECT_TRUE(!p->to_uint64(MIN_UINT64_STR, &val_uint64) &&
              val_uint64 == MIN_UINT64);

  // PTC10
  EXPECT_TRUE(!p->to_int32("123", &val_int32) && val_int32 == 123);
  // PTC11
  EXPECT_TRUE(!p->to_int32("-123", &val_int32) && val_int32 == -123);
  // PTC12
  EXPECT_TRUE(!p->to_int32("0", &val_int32) && val_int32 == 0);
  // PTC13
  EXPECT_TRUE(!p->to_int32(MAX_INT32_STR, &val_int32) &&
              val_int32 == MAX_INT32);
  // PTC14
  EXPECT_TRUE(!p->to_int32(MIN_INT32_STR, &val_int32) &&
              val_int32 == MIN_INT32);

  // PTC15
  EXPECT_TRUE(!p->to_uint32("123", &val_uint32) && val_uint32 == 123);
  // PTC16
  EXPECT_TRUE(!p->to_uint32("0", &val_uint32) && val_uint32 == 0);
  // PTC17
  EXPECT_TRUE(!p->to_uint32(MAX_UINT32_STR, &val_uint32) &&
              val_uint32 == MAX_UINT32);
  // PTC18
  EXPECT_TRUE(!p->to_uint32(MIN_UINT32_STR, &val_uint32) &&
              val_uint32 == MIN_UINT32);

  delete p;
}

// Tests that integer conversion errors are handled
TEST_F(PropertiesTest, FailingIntConversions) {
  int32 val_int32 = 0;
  uint32 val_uint32 = 0;
  int64 val_int64 = 0;
  uint64 val_uint64 = 0;

  const char *OFL_INT64_STR = "9223372036854775808";
  const char *UFL_INT64_STR = "-9223372036854775809";

  const char *OFL_UINT64_STR = "18446744073709551616";

  const char *OFL_INT32_STR = "2147483648";
  const char *UFL_INT32_STR = "-2147483649";

  const char *OFL_UINT32_STR = "4294967296";

  dd::Properties *p = new dd::Properties_impl();

  // ======================================
  // Negative test cases
  // ======================================
  // TC#   Operation    Source      Target
  // -----+------------+-----------+-------
  // NTC1  Overflow     OFL_INT64   int64
  // NTC2  Underflow    UFL_INT64   int64
  // NTC3  Malformed    str         int64

  // NTC4  Overflow     OFL_UINT64  uint64
  // NTC5  Malformed    str         uint64
  // NTC6  Sign         -num        uint64

  // NTC7  Overflow     OFL_INT32   int32
  // NTC8  Underflow    UFL_INT32   int32
  // NTC9  Malformed    str         int32

  // NTC10 Overflow     OFL_UINT32  uint32
  // NTC11 Malformed    str         uint32
  // NTC12 Sign         -num        uint32
  // ======================================

  // NTC1
  EXPECT_TRUE(p->to_int64(OFL_INT64_STR, &val_int64));
  // NTC2
  EXPECT_TRUE(p->to_int64(UFL_INT64_STR, &val_int64));
  // NTC3
  EXPECT_TRUE(p->to_int64("abc", &val_int64));

  // NTC4
  EXPECT_TRUE(p->to_uint64(OFL_UINT64_STR, &val_uint64));
  // NTC5
  EXPECT_TRUE(p->to_uint64("abc", &val_uint64));
  // NTC6
  EXPECT_TRUE(p->to_uint64("-1", &val_uint64));

  // NTC7
  EXPECT_TRUE(p->to_int32(OFL_INT32_STR, &val_int32));
  // NTC8
  EXPECT_TRUE(p->to_int32(UFL_INT32_STR, &val_int32));
  // NTC9
  EXPECT_TRUE(p->to_int32("abc", &val_int32));

  // NTC10
  EXPECT_TRUE(p->to_uint32(OFL_UINT32_STR, &val_uint32));
  // NTC11
  EXPECT_TRUE(p->to_uint32("abc", &val_uint32));
  // NTC12
  EXPECT_TRUE(p->to_uint32("-1", &val_uint32));

  delete p;
}

// Tests that valid boolean conversions are handled
TEST_F(PropertiesTest, ValidBoolConversions) {
  bool val = false;
  dd::Properties *p = new dd::Properties_impl();

  const char *MIN_INT64_STR = "-9223372036854775808";
  const char *MAX_INT64_STR = "9223372036854775807";
  const char *MAX_UINT64_STR = "18446744073709551615";
  const char *OFL_INT64_STR = "9223372036854775808";

  // =======================================
  // Positive test cases
  // =======================================
  // TC#   Operation    Source        Target
  // -----+------------+-------------+------
  // PTC1  Convert      true           bool
  // PTC2  Convert      1              bool
  // PTC3  Convert      true           bool
  // PTC4  Convert      0              bool
  // PTC5  Convert      MIN_INT64_STR  bool
  // PTC6  Convert      MAX_INT64_STR  bool
  // PTC7  Convert      MAX_UINT64_STR bool
  // PTC8  Convert      OFL_INT64_STR  bool
  // =======================================

  // PTC1
  EXPECT_TRUE(!p->to_bool("true", &val) && val == true);
  // PTC2
  EXPECT_TRUE(!p->to_bool("1", &val) && val == true);
  // PTC3
  EXPECT_TRUE(!p->to_bool("false", &val) && val == false);
  // PTC4
  EXPECT_TRUE(!p->to_bool("0", &val) && val == false);
  // PTC5
  EXPECT_TRUE(!p->to_bool(MIN_INT64_STR, &val) && val == true);
  // PTC6
  EXPECT_TRUE(!p->to_bool(MAX_INT64_STR, &val) && val == true);
  // PTC7
  EXPECT_TRUE(!p->to_bool(MAX_UINT64_STR, &val) && val == true);
  // PTC8
  EXPECT_TRUE(!p->to_bool(OFL_INT64_STR, &val) && val == true);

  delete p;
}

// Tests that boolean conversion errors are handled
TEST_F(PropertiesTest, FailingBoolConversions) {
  bool val = false;
  dd::Properties *p = new dd::Properties_impl();

  const char *UFL_INT64_STR = "-9223372036854775809";
  const char *OFL_UINT64_STR = "18446744073709551616";

  // =======================================
  // Negative test cases
  // =======================================
  // TC#   Operation    Source        Target
  // -----+------------+-------------+------
  // NTC1  Convert      TRUE           bool
  // NTC2  Convert      FALSE          bool
  // NTC3  Convert      <empty>        bool
  // NTC4  Convert      ttrue          bool
  // NTC5  Convert      truee          bool
  // NTC6  Convert      ffalse         bool
  // NTC7  Convert      falsee         bool
  // NTC8  Convert      UFL_INT64_STR  bool
  // NTC9  Convert      OFL_UINT64_STR bool
  // =======================================

  // NTC1
  EXPECT_TRUE(p->to_bool("TRUE", &val));
  // NTC2
  EXPECT_TRUE(p->to_bool("FALSE", &val));
  // NTC3
  EXPECT_TRUE(p->to_bool("", &val));
  // NTC4
  EXPECT_TRUE(p->to_bool("ttrue", &val));
  // NTC5
  EXPECT_TRUE(p->to_bool("truee", &val));
  // NTC6
  EXPECT_TRUE(p->to_bool("ffalse", &val));
  // NTC7
  EXPECT_TRUE(p->to_bool("falsee", &val));
  // NTC8
  EXPECT_TRUE(p->to_bool(UFL_INT64_STR, &val));
  // NTC9
  EXPECT_TRUE(p->to_bool(OFL_UINT64_STR, &val));

  delete p;
}

// Invoke conversion functions through class name
TEST_F(PropertiesTest, StaticConversionMethods) {
  int32 val_int32 = 0;
  uint32 val_uint32 = 0;
  int64 val_int64 = 0;
  uint64 val_uint64 = 0;
  bool maybe = false;

  EXPECT_TRUE(!dd::Properties::to_int64("-123", &val_int64) &&
              val_int64 == -123);

  EXPECT_TRUE(dd::Properties::to_uint64("-123", &val_uint64));
  EXPECT_TRUE(!dd::Properties::to_uint64("0", &val_uint64) && val_uint64 == 0);

  EXPECT_TRUE(!dd::Properties::to_int32("-123", &val_int32) &&
              val_int32 == -123);

  EXPECT_TRUE(dd::Properties::to_uint32("-123", &val_uint32));
  EXPECT_TRUE(!dd::Properties::to_uint32("0", &val_uint32) && val_uint32 == 0);

  EXPECT_TRUE(!dd::Properties::to_bool("true", &maybe) && maybe == true);
  EXPECT_TRUE(!dd::Properties::to_bool("1", &maybe) && maybe == true);
  EXPECT_TRUE(!dd::Properties::to_bool("false", &maybe) && maybe == false);
  EXPECT_TRUE(!dd::Properties::to_bool("0", &maybe) && maybe == false);

  EXPECT_TRUE(dd::Properties::to_bool("", &maybe));
}

// Test assignment operator for deep copy of Property objects
TEST_F(PropertiesTest, Assign) {
  dd::Properties *p = new dd::Properties_impl();
  p->set_int32("a", 1);

  // Assign to a different object
  dd::Properties *p_copy = new dd::Properties_impl();
  p_copy->assign(*p);

  // The "a" key should be present with the same value in both objects
  int32 val_p = 0;
  int32 val_p_copy = 0;
  EXPECT_TRUE(!p->get_int32("a", &val_p) && val_p == 1 &&
              !p_copy->get_int32("a", &val_p_copy) && val_p_copy == 1);

  // Changing the value in one object should not be reflected in the other
  p->set_int32("a", 2);
  EXPECT_TRUE(!p->get_int32("a", &val_p) && val_p == 2 &&
              !p_copy->get_int32("a", &val_p_copy) && val_p_copy == 1);
  delete p_copy;
  delete p;
}

}  // namespace dd_properties_unittest
