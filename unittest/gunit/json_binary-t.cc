/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "json_binary.h"
#include "json_dom.h"
#include "sql_time.h"
#include <gtest/gtest.h>
#include "test_utils.h"
#include <cstring>
#include <memory>

namespace json_binary_unittest {

using namespace json_binary;

class JsonBinaryTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
  my_testing::Server_initializer initializer;
};


/**
  Get a copy of the string value represented by val.
*/
std::string get_string(const Value &val)
{
  return std::string(val.get_data(), val.get_data_length());
}


TEST_F(JsonBinaryTest, BasicTest)
{
  const char *doc= "false";

  const char *msg;
  size_t msg_offset;

  std::auto_ptr<Json_dom> dom(Json_dom::parse(doc, strlen(doc),
                                              &msg, &msg_offset));
  String buf;
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val1= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val1.is_valid());
  EXPECT_EQ(Value::LITERAL_FALSE, val1.type());

  doc= "-123";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val2= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val2.is_valid());
  EXPECT_EQ(Value::INT, val2.type());
  EXPECT_EQ(-123LL, val2.get_int64());

  doc= "3.14";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val3= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val3.is_valid());
  EXPECT_EQ(Value::DOUBLE, val3.type());
  EXPECT_EQ(3.14, val3.get_double());

  doc= "18446744073709551615";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val4= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val4.is_valid());
  EXPECT_EQ(Value::UINT, val4.type());
  EXPECT_EQ(18446744073709551615ULL, val4.get_uint64());

  doc= "\"abc\"";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val5= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val5.is_valid());
  EXPECT_EQ(Value::STRING, val5.type());
  EXPECT_EQ("abc", get_string(val5));

  doc= "[ 1, 2, 3 ]";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val6= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val6.is_valid());
  EXPECT_EQ(Value::ARRAY, val6.type());
  EXPECT_EQ(3U, val6.element_count());
  for (int i= 0; i < 3; i++)
  {
    Value v= val6.element(i);
    EXPECT_EQ(Value::INT, v.type());
    EXPECT_EQ(i + 1, v.get_int64());
  }
  EXPECT_EQ(Value::ERROR, val6.element(3).type());

  doc= "[ 1, [ \"a\", [ 3.14 ] ] ]";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  // Top-level doc is an array of size 2.
  Value val7= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val7.is_valid());
  EXPECT_EQ(Value::ARRAY, val7.type());
  EXPECT_EQ(2U, val7.element_count());
  // First element is the integer 1.
  Value v7_1= val7.element(0);
  EXPECT_TRUE(v7_1.is_valid());
  EXPECT_EQ(Value::INT, v7_1.type());
  EXPECT_EQ(1, v7_1.get_int64());
  // The second element is a nested array of size 2.
  Value v7_2= val7.element(1);
  EXPECT_TRUE(v7_2.is_valid());
  EXPECT_EQ(Value::ARRAY, v7_2.type());
  EXPECT_EQ(2U, v7_2.element_count());
  // The first element of the nested array is the string "a".
  Value v7_3= v7_2.element(0);
  EXPECT_TRUE(v7_3.is_valid());
  EXPECT_EQ(Value::STRING, v7_3.type());
  EXPECT_EQ("a", get_string(v7_3));
  // The second element of the nested array is yet another array.
  Value v7_4= v7_2.element(1);
  EXPECT_TRUE(v7_4.is_valid());
  EXPECT_EQ(Value::ARRAY, v7_4.type());
  // The second nested array has one element, the double 3.14.
  EXPECT_EQ(1U, v7_4.element_count());
  Value v7_5= v7_4.element(0);
  EXPECT_TRUE(v7_5.is_valid());
  EXPECT_EQ(Value::DOUBLE, v7_5.type());
  EXPECT_EQ(3.14, v7_5.get_double());

  doc= "{\"key\" : \"val\"}";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val8= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val8.is_valid());
  EXPECT_EQ(Value::OBJECT, val8.type());
  EXPECT_EQ(1U, val8.element_count());
  Value val8_k= val8.key(0);
  EXPECT_TRUE(val8_k.is_valid());
  EXPECT_EQ(Value::STRING, val8_k.type());
  EXPECT_EQ("key", get_string(val8_k));
  Value val8_v= val8.element(0);
  EXPECT_TRUE(val8_v.is_valid());
  EXPECT_EQ(Value::STRING, val8_v.type());
  EXPECT_EQ("val", get_string(val8_v));
  EXPECT_EQ(Value::ERROR, val8.key(1).type());
  EXPECT_EQ(Value::ERROR, val8.element(1).type());

  Value v8_v1= val8.lookup("key", 3);
  EXPECT_EQ(Value::STRING, v8_v1.type());
  EXPECT_TRUE(v8_v1.is_valid());
  EXPECT_EQ("val", get_string(v8_v1));

  doc= "{ \"a\" : \"b\", \"c\" : [ \"d\" ] }";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val9= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val9.is_valid());
  EXPECT_EQ(Value::OBJECT, val9.type());
  EXPECT_EQ(2U, val9.element_count());
  Value v9_k1= val9.key(0);
  EXPECT_EQ(Value::STRING, v9_k1.type());
  EXPECT_EQ("a", get_string(v9_k1));
  Value v9_v1= val9.element(0);
  EXPECT_EQ(Value::STRING, v9_v1.type());
  EXPECT_EQ("b", get_string(v9_v1));
  Value v9_k2= val9.key(1);
  EXPECT_EQ(Value::STRING, v9_k2.type());
  EXPECT_EQ("c", get_string(v9_k2));
  Value v9_v2= val9.element(1);
  EXPECT_EQ(Value::ARRAY, v9_v2.type());
  EXPECT_EQ(1U, v9_v2.element_count());
  Value v9_v2_1= v9_v2.element(0);
  EXPECT_EQ(Value::STRING, v9_v2_1.type());
  EXPECT_EQ("d", get_string(v9_v2_1));

  EXPECT_EQ("b", get_string(val9.lookup("a", 1)));
  Value v9_c= val9.lookup("c", 1);
  EXPECT_EQ(Value::ARRAY, v9_c.type());
  EXPECT_EQ(1U, v9_c.element_count());
  Value v9_c1= v9_c.element(0);
  EXPECT_EQ(Value::STRING, v9_c1.type());
  EXPECT_EQ("d", get_string(v9_c1));

  char blob[4];
  int4store(blob, 0xCAFEBABEU);
  Json_opaque opaque(MYSQL_TYPE_TINY_BLOB, blob, 4);
  EXPECT_FALSE(serialize(&opaque, &buf));
  Value val10= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val10.is_valid());
  EXPECT_EQ(Value::OPAQUE, val10.type());
  EXPECT_EQ(MYSQL_TYPE_TINY_BLOB, val10.field_type());
  EXPECT_EQ(4U, val10.get_data_length());
  EXPECT_EQ(0xCAFEBABEU, uint4korr(val10.get_data()));

  doc= "[true,false,null,0,\"0\",\"\",{},[]]";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val11= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val11.is_valid());
  EXPECT_EQ(Value::ARRAY, val11.type());
  EXPECT_EQ(8U, val11.element_count());
  EXPECT_EQ(Value::LITERAL_TRUE,  val11.element(0).type());
  EXPECT_EQ(Value::LITERAL_FALSE, val11.element(1).type());
  EXPECT_EQ(Value::LITERAL_NULL,  val11.element(2).type());
  EXPECT_EQ(Value::INT,           val11.element(3).type());
  EXPECT_EQ(Value::STRING,        val11.element(4).type());
  EXPECT_EQ(Value::STRING,        val11.element(5).type());
  EXPECT_EQ(Value::OBJECT,        val11.element(6).type());
  EXPECT_EQ(Value::ARRAY,         val11.element(7).type());
  EXPECT_EQ(0, val11.element(3).get_int64());
  EXPECT_EQ("0", get_string(val11.element(4)));
  EXPECT_EQ("", get_string(val11.element(5)));
  EXPECT_EQ(0U, val11.element(6).element_count());
  EXPECT_EQ(0U, val11.element(7).element_count());

  doc= "{}";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val12= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val12.is_valid());
  EXPECT_EQ(Value::OBJECT, val12.type());
  EXPECT_EQ(0U, val12.element_count());
  EXPECT_EQ(Value::ERROR, val12.lookup("", 0).type());
  EXPECT_EQ(Value::ERROR, val12.lookup("key", 3).type());
  EXPECT_FALSE(val12.lookup("no such key", 11).is_valid());

  doc= "[]";
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val13= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val13.is_valid());
  EXPECT_EQ(Value::ARRAY, val13.type());
  EXPECT_EQ(0U, val13.element_count());

  doc= "{\"key1\":1, \"key2\":2, \"key1\":3, \"key1\\u0000x\":4, "
    "\"key1\\u0000y\":5, \"a\":6, \"ab\":7, \"b\":8, \"\":9, \"\":10}";
  const std::string expected_keys[]=
  {
    "", "a", "b", "ab", "key1", "key2",
    std::string("key1\0x", 6), std::string("key1\0y", 6)
  };
  const int64 expected_values[]= { 9, 6, 8, 7, 1, 2, 4, 5 };
  dom.reset(Json_dom::parse(doc, strlen(doc), &msg, &msg_offset));
  EXPECT_FALSE(serialize(dom.get(), &buf));
  Value val14= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val14.is_valid());
  EXPECT_EQ(Value::OBJECT, val14.type());
  EXPECT_EQ(8U, val14.element_count());
  for (size_t i= 0; i < val14.element_count(); i++)
  {
    EXPECT_EQ(expected_keys[i], get_string(val14.key(i)));

    Value val= val14.element(i);
    EXPECT_EQ(Value::INT, val.type());
    EXPECT_EQ(expected_values[i], val.get_int64());

    Value val_lookup= val14.lookup(expected_keys[i].data(),
                                   expected_keys[i].length());
    EXPECT_EQ(Value::INT, val_lookup.type());
    EXPECT_EQ(expected_values[i], val_lookup.get_int64());
  }

  // Store a decimal.
  my_decimal md;
  EXPECT_EQ(E_DEC_OK, double2my_decimal(E_DEC_FATAL_ERROR, 123.45, &md));
  EXPECT_EQ(5U, md.precision());
  EXPECT_EQ(2, md.frac);

  Json_decimal jd(md);
  EXPECT_FALSE(serialize(&jd, &buf));
  Value val15= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val15.is_valid());
  EXPECT_EQ(Value::OPAQUE, val15.type());
  EXPECT_EQ(MYSQL_TYPE_NEWDECIMAL, val15.field_type());

  my_decimal md_out;
  EXPECT_FALSE(Json_decimal::convert_from_binary(val15.get_data(),
                                                 val15.get_data_length(),
                                                 &md_out));
  EXPECT_EQ(5U, md_out.precision());
  EXPECT_EQ(2, md_out.frac);
  double d_out;
  EXPECT_EQ(E_DEC_OK,
            my_decimal2double(E_DEC_FATAL_ERROR, &md_out, &d_out));
  EXPECT_EQ(123.45, d_out);
}


/*
  Test storing of TIME, DATE and DATETIME.
*/
TEST_F(JsonBinaryTest, DateAndTimeTest)
{
  const char *tstr= "13:14:15.654321";
  const char *dstr= "20140517";
  const char *dtstr= "2015-01-15 15:16:17.123456";
  MYSQL_TIME t;
  MYSQL_TIME d;
  MYSQL_TIME dt;
  MYSQL_TIME_STATUS status;
  EXPECT_FALSE(str_to_time(&my_charset_utf8mb4_bin, tstr, strlen(tstr),
                           &t, 0, &status));
  EXPECT_FALSE(str_to_datetime(&my_charset_utf8mb4_bin, dstr, strlen(dstr),
                               &d, 0, &status));
  EXPECT_FALSE(str_to_datetime(&my_charset_utf8mb4_bin, dtstr, strlen(dtstr),
                               &dt, 0, &status));

  // Create an array that contains a TIME, a DATE and a DATETIME.
  Json_array array;
  Json_datetime tt(t, MYSQL_TYPE_TIME);
  Json_datetime td(d, MYSQL_TYPE_DATE);
  Json_datetime tdt(dt, MYSQL_TYPE_DATETIME);
  array.append_clone(&tt);
  array.append_clone(&td);
  array.append_clone(&tdt);

  // Store the array ...
  String buf;
  EXPECT_FALSE(serialize(&array, &buf));

  // ... and read it back.
  Value val= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val.is_valid());
  EXPECT_EQ(Value::ARRAY, val.type());
  EXPECT_EQ(3U, val.element_count());

  // The first element should be the TIME "13:14:15.654321".
  Value t_val= val.element(0);
  EXPECT_EQ(Value::OPAQUE, t_val.type());
  EXPECT_EQ(MYSQL_TYPE_TIME, t_val.field_type());
  const size_t json_datetime_packed_size= Json_datetime::PACKED_SIZE;
  EXPECT_EQ(json_datetime_packed_size, t_val.get_data_length());
  MYSQL_TIME t_out;
  Json_datetime::from_packed(t_val.get_data(), t_val.field_type(), &t_out);
  EXPECT_EQ(13U, t_out.hour);
  EXPECT_EQ(14U, t_out.minute);
  EXPECT_EQ(15U, t_out.second);
  EXPECT_EQ(654321U, t_out.second_part);
  EXPECT_FALSE(t_out.neg);
  EXPECT_EQ(MYSQL_TIMESTAMP_TIME, t_out.time_type);

  // The second element should be the DATE "2014-05-17".
  Value d_val= val.element(1);
  EXPECT_EQ(Value::OPAQUE, d_val.type());
  EXPECT_EQ(MYSQL_TYPE_DATE, d_val.field_type());
  EXPECT_EQ(json_datetime_packed_size, d_val.get_data_length());
  MYSQL_TIME d_out;
  Json_datetime::from_packed(d_val.get_data(), d_val.field_type(), &d_out);
  EXPECT_EQ(2014U, d_out.year);
  EXPECT_EQ(5U, d_out.month);
  EXPECT_EQ(17U, d_out.day);
  EXPECT_FALSE(d_out.neg);
  EXPECT_EQ(MYSQL_TIMESTAMP_DATE, d_out.time_type);

  // The third element should be the DATETIME "2015-01-15 15:16:17.123456".
  Value dt_val= val.element(2);
  EXPECT_EQ(Value::OPAQUE, dt_val.type());
  EXPECT_EQ(MYSQL_TYPE_DATETIME, dt_val.field_type());
  EXPECT_EQ(json_datetime_packed_size, dt_val.get_data_length());
  MYSQL_TIME dt_out;
  Json_datetime::from_packed(dt_val.get_data(), dt_val.field_type(), &dt_out);
  EXPECT_EQ(2015U, dt_out.year);
  EXPECT_EQ(1U, dt_out.month);
  EXPECT_EQ(15U, dt_out.day);
  EXPECT_EQ(15U, dt_out.hour);
  EXPECT_EQ(16U, dt_out.minute);
  EXPECT_EQ(17U, dt_out.second);
  EXPECT_EQ(123456U, dt_out.second_part);
  EXPECT_FALSE(dt_out.neg);
  EXPECT_EQ(MYSQL_TIMESTAMP_DATETIME, dt_out.time_type);
}


/*
  Validate that the contents of an array are as expected. The array
  should contain values that alternate between literal true, literal
  false, literal null and the string "a".
*/
void validate_array_contents(const Value &array, size_t expected_size)
{
  EXPECT_EQ(Value::ARRAY, array.type());
  EXPECT_TRUE(array.is_valid());
  EXPECT_EQ(expected_size, array.element_count());
  for (size_t i= 0; i < array.element_count(); i++)
  {
    Value val= array.element(i);
    EXPECT_TRUE(val.is_valid());
    Value::enum_type t= val.type();
    if (i % 4 == 0)
      EXPECT_EQ(Value::LITERAL_TRUE, t);
    else if (i % 4 == 1)
      EXPECT_EQ(Value::LITERAL_FALSE, t);
    else if (i % 4 == 2)
      EXPECT_EQ(Value::LITERAL_NULL, t);
    else
    {
      EXPECT_EQ(Value::STRING, t);
      EXPECT_EQ("a", get_string(val));
    }
  }
}


/*
  Test some arrays and objects that exceed 64KB. Arrays and objects
  are stored in a different format if more than two bytes are required
  for the internal offsets.
*/
TEST_F(JsonBinaryTest, LargeDocumentTest)
{
  Json_array array;
  Json_boolean literal_true(true);
  Json_boolean literal_false(false);
  Json_null literal_null;
  Json_string string("a");

  for (int i= 0; i < 20000; i++)
  {
    array.append_clone(&literal_true);
    array.append_clone(&literal_false);
    array.append_clone(&literal_null);
    array.append_clone(&string);
  }
  EXPECT_EQ(80000U, array.size());

  String buf;
  EXPECT_FALSE(serialize(&array, &buf));
  Value val= parse_binary(buf.ptr(), buf.length());
  {
    SCOPED_TRACE("");
    validate_array_contents(val, array.size());
  }

  /*
    Extract the raw binary representation of the large array, and verify
    that it is valid.
  */
  String raw;
  EXPECT_FALSE(val.raw_binary(&raw));
  {
    SCOPED_TRACE("");
    validate_array_contents(parse_binary(raw.ptr(), raw.length()),
                            array.size());
  }

  Json_array array2;
  array2.append_clone(&array);
  array2.append_clone(&array);
  EXPECT_FALSE(serialize(&array2, &buf));
  Value val2= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val2.is_valid());
  EXPECT_EQ(Value::ARRAY, val2.type());
  EXPECT_EQ(2U, val2.element_count());
  {
    SCOPED_TRACE("");
    validate_array_contents(val2.element(0), array.size());
  }
  {
    SCOPED_TRACE("");
    validate_array_contents(val2.element(1), array.size());
  }

  Json_object object;
  object.add_clone("a", &array);
  Json_string s_c("c");
  object.add_clone("b", &s_c);
  EXPECT_FALSE(serialize(&object, &buf));
  Value val3= parse_binary(buf.ptr(), buf.length());
  EXPECT_TRUE(val3.is_valid());
  EXPECT_EQ(Value::OBJECT, val3.type());
  EXPECT_EQ(2U, val3.element_count());
  EXPECT_EQ("a", get_string(val3.key(0)));
  {
    SCOPED_TRACE("");
    validate_array_contents(val3.element(0), array.size());
  }
  EXPECT_EQ("b", get_string(val3.key(1)));
  EXPECT_EQ(Value::STRING, val3.element(1).type());
  EXPECT_EQ("c", get_string(val3.element(1)));

  {
    SCOPED_TRACE("");
    validate_array_contents(val3.lookup("a", 1), array.size());
  }
  EXPECT_EQ("c", get_string(val3.lookup("b", 1)));

  /*
    Extract the raw binary representation of the large object, and verify
    that it is valid.
  */
  EXPECT_FALSE(val3.raw_binary(&raw));
  {
    SCOPED_TRACE("");
    Value val_a= parse_binary(raw.ptr(), raw.length()).lookup("a", 1);
    validate_array_contents(val_a, array.size());
  }

  /*
    Bug#23031146: INSERTING 64K SIZE RECORDS TAKE TOO MUCH TIME

    If a big (>64KB) sub-document was located at a deep nesting level,
    serialization used to be very slow.
  */
  {
    SCOPED_TRACE("");
    // Wrap "array" in 50 more levels of arrays.
    const size_t depth= 50;
    Json_array deeply_nested_array;
    Json_array *current_array= &deeply_nested_array;
    for (size_t i= 1; i < depth; i++)
    {
      Json_array *a= new (std::nothrow) Json_array();
      ASSERT_FALSE(current_array->append_alias(a));
      current_array= a;
    }
    current_array->append_clone(&array);
    // Serialize it. This used to take "forever".
    ASSERT_FALSE(serialize(&deeply_nested_array, &buf));
    // Parse the serialized DOM and verify its contents.
    Value val= parse_binary(buf.ptr(), buf.length());
    for (size_t i= 0; i < depth; i++)
    {
      ASSERT_EQ(Value::ARRAY, val.type());
      ASSERT_EQ(1U, val.element_count());
      val= val.element(0);
    }
    validate_array_contents(val, array.size());

    // Now test the same with object.
    Json_object deeply_nested_object;
    Json_object *current_object= &deeply_nested_object;
    for (size_t i= 1; i < depth; i++)
    {
      Json_object *o= new (std::nothrow) Json_object();
      ASSERT_FALSE(current_object->add_alias("key", o));
      current_object= o;
    }
    current_object->add_clone("key", &array);
    ASSERT_FALSE(serialize(&deeply_nested_object, &buf));
    val= parse_binary(buf.ptr(), buf.length());
    for (size_t i= 0; i < depth; i++)
    {
      ASSERT_EQ(Value::OBJECT, val.type());
      ASSERT_EQ(1U, val.element_count());
      ASSERT_EQ("key", get_string(val.key(0)));
      val= val.element(0);
    }
    validate_array_contents(val, array.size());
  }
}


/*
  Various tests for the Value::raw_binary() function.
*/
TEST_F(JsonBinaryTest, RawBinaryTest)
{
  Json_array array;
  Json_string as("a string");
  array.append_clone(&as);
  Json_int ji(-123);
  array.append_clone(&ji);
  Json_uint jui(42);
  array.append_clone(&jui);
  Json_double jd(1.5);
  array.append_clone(&jd);
  Json_null jn;
  array.append_clone(&jn);
  Json_boolean jbt(true);
  array.append_clone(&jbt);
  Json_boolean jbf(false);
  array.append_clone(&jbf);
  Json_opaque jo(MYSQL_TYPE_BLOB, "abcd", 4);
  array.append_clone(&jo);

  Json_object object;
  object.add_clone("key", &jbt);
  array.append_clone(&object);

  Json_array array2;
  array2.append_clone(&jbf);
  array.append_clone(&array2);

  String buf;
  EXPECT_FALSE(json_binary::serialize(&array, &buf));
  Value v1= parse_binary(buf.ptr(), buf.length());

  String raw;
  EXPECT_FALSE(v1.raw_binary(&raw));
  Value v1_copy= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::ARRAY, v1_copy.type());
  EXPECT_EQ(array.size(), v1_copy.element_count());

  EXPECT_FALSE(v1.element(0).raw_binary(&raw));
  Value v1_0= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::STRING, v1_0.type());
  EXPECT_EQ("a string", std::string(v1_0.get_data(), v1_0.get_data_length()));

  EXPECT_FALSE(v1.element(1).raw_binary(&raw));
  Value v1_1= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::INT, v1_1.type());
  EXPECT_EQ(-123, v1_1.get_int64());

  EXPECT_FALSE(v1.element(2).raw_binary(&raw));
  Value v1_2= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::UINT, v1_2.type());
  EXPECT_EQ(42U, v1_2.get_uint64());

  EXPECT_FALSE(v1.element(3).raw_binary(&raw));
  Value v1_3= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::DOUBLE, v1_3.type());
  EXPECT_EQ(1.5, v1_3.get_double());

  EXPECT_FALSE(v1.element(4).raw_binary(&raw));
  Value v1_4= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::LITERAL_NULL, v1_4.type());

  EXPECT_FALSE(v1.element(5).raw_binary(&raw));
  Value v1_5= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::LITERAL_TRUE, v1_5.type());

  EXPECT_FALSE(v1.element(6).raw_binary(&raw));
  Value v1_6= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::LITERAL_FALSE, v1_6.type());

  EXPECT_FALSE(v1.element(7).raw_binary(&raw));
  Value v1_7= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::OPAQUE, v1_7.type());
  EXPECT_EQ(MYSQL_TYPE_BLOB, v1_7.field_type());
  EXPECT_EQ("abcd", std::string(v1_7.get_data(), v1_7.get_data_length()));

  EXPECT_FALSE(v1.element(8).raw_binary(&raw));
  Value v1_8= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::OBJECT, v1_8.type());
  EXPECT_EQ(object.cardinality(), v1_8.element_count());
  EXPECT_EQ(Value::LITERAL_TRUE, v1_8.lookup("key", 3).type());

  EXPECT_FALSE(v1.element(8).key(0).raw_binary(&raw));
  Value v1_8_key= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::STRING, v1_8_key.type());
  EXPECT_EQ("key", std::string(v1_8_key.get_data(),
                               v1_8_key.get_data_length()));

  EXPECT_FALSE(v1.element(8).element(0).raw_binary(&raw));
  Value v1_8_val= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::LITERAL_TRUE, v1_8_val.type());

  EXPECT_FALSE(v1.element(9).raw_binary(&raw));
  Value v1_9= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::ARRAY, v1_9.type());
  EXPECT_EQ(array2.size(), v1_9.element_count());
  EXPECT_EQ(Value::LITERAL_FALSE, v1_9.element(0).type());

  EXPECT_FALSE(v1.element(9).element(0).raw_binary(&raw));
  Value v1_9_0= parse_binary(raw.ptr(), raw.length());
  EXPECT_EQ(Value::LITERAL_FALSE, v1_9_0.type());
}


/*
  Create a JSON string of the given size, serialize it as a JSON binary, and
  then deserialize it and verify that we get the same string back.
*/
void serialize_deserialize_string(size_t size)
{
  SCOPED_TRACE(testing::Message() << "size = " << size);
  char *str= new char[size];
  memset(str, 'a', size);
  Json_string jstr(std::string(str, size));

  String buf;
  EXPECT_FALSE(json_binary::serialize(&jstr, &buf));
  Value v= parse_binary(buf.ptr(), buf.length());
  EXPECT_EQ(Value::STRING, v.type());
  EXPECT_EQ(size, v.get_data_length());
  EXPECT_EQ(0, memcmp(str, v.get_data(), size));

  delete[] str;
}


/*
  Test strings of variable length. Test especially around the boundaries
  where the representation of the string length changes:

  - Strings of length 0-127 use 1 byte length fields.
  - Strings of length 128-16383 use 2 byte length fields.
  - Strings of length 16384-2097151 use 3 byte length fields.
  - Strings of length 2097152-268435455 use 4 byte length fields.
  - Strings of length 268435456-... use 5 byte length fields.

  We probably don't have enough memory to test the last category here...
*/
TEST_F(JsonBinaryTest, StringLengthTest)
{
  serialize_deserialize_string(0);
  serialize_deserialize_string(1);
  serialize_deserialize_string(127);
  serialize_deserialize_string(128);
  serialize_deserialize_string(16383);
  serialize_deserialize_string(16384);
  serialize_deserialize_string(2097151);
  serialize_deserialize_string(2097152);
  serialize_deserialize_string(3000000);
}

}
