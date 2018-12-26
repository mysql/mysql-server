/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include "my_decimal.h"
#include "sql_string.h"
#include "sql_time.h"
#include "json_binary.h"
#include "json_dom.h"
#include "base64.h"
#include "template_utils.h"     // down_cast

#include <gtest/gtest.h>
#include "test_utils.h"

#include <memory>
#include <cstring>

/**
 Test Json_dom class hierarchy API, cf. json_dom.h
 */
namespace json_dom_unittest {

class JsonDomTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
  my_testing::Server_initializer initializer;
};

/**
   Format a Json_dom object to JSON text using  Json_wrapper's
   to_string functionality.

   @param d The DOM object to be formatted
*/
std::string format(const Json_dom &d)
{
  String buffer;
  Json_wrapper w(d.clone());
  EXPECT_FALSE(w.to_string(&buffer, true, "format"));

  return std::string(buffer.ptr(), buffer.length());
}

std::string format(const Json_dom *ptr)
{
  return format(*ptr);
}

TEST_F(JsonDomTest, BasicTest)
{
  String buffer;
  /* string scalar */
  const std::string std_s("abc");
  Json_string s(std_s);
  EXPECT_EQ(std_s, s.value());
  EXPECT_EQ(Json_dom::J_STRING, s.json_type());
  EXPECT_TRUE(s.is_scalar());
  EXPECT_EQ(1U, s.depth());
  EXPECT_FALSE(s.is_number());
  EXPECT_EQ(std::string("\"abc\""), format(s));

  /*
    Escaping in strings, cf. ECMA-404 The JSON Data Interchange Format
  */
  Json_array a;
  /* double quote and backslash */
  Json_string js1(std::string("a\"b\\c"));
  a.append_clone(&js1);
  EXPECT_EQ(std::string("[\"a\\\"b\\\\c\"]"), format(a));

  a.clear();
  /* Printable control characters */
  Json_string js2(std::string("a\b\f\n\r\tb"));
  a.append_clone(&js2);
  EXPECT_EQ(7U, static_cast<Json_string *>(a[0])->size());
  EXPECT_EQ(std::string("[\"a\\b\\f\\n\\r\\tb\"]"), format(a));

  a.clear();
  /* Unprintable control characters and non-ASCII Unicode characters */
  Json_string js3(std::string("丳\x13" "丽\x3"));
  a.append_clone(&js3);
  EXPECT_EQ(std::string("[\"丳\\u0013丽\\u0003\"]"), format(a));

  /* boolean scalar */
  const Json_boolean jb(true);
  EXPECT_EQ(Json_dom::J_BOOLEAN, jb.json_type());
  EXPECT_EQ(true, jb.value());
  EXPECT_EQ(std::string("true"), format(jb));

  /* Integer scalar */
  const Json_int ji(-123);
  EXPECT_EQ(Json_dom::J_INT, ji.json_type());
  EXPECT_EQ(-123, ji.value());
  EXPECT_EQ(std::string("-123"), format(ji));

  const Json_int max_32_int(2147483647);
  EXPECT_EQ(std::string("2147483647"), format(max_32_int));

  const Json_int max_64_int(9223372036854775807LL);
  EXPECT_EQ(std::string("9223372036854775807"), format(max_64_int));

  const Json_uint max_64_uint(18446744073709551615ULL);
  EXPECT_EQ(Json_dom::J_UINT, max_64_uint.json_type());
  EXPECT_EQ(std::string("18446744073709551615"), format(max_64_uint));

  /* Double scalar */
  const Json_double jdb(-123.45);
  EXPECT_EQ(Json_dom::J_DOUBLE, jdb.json_type());
  EXPECT_EQ(-123.45, jdb.value());
  EXPECT_EQ(std::string("-123.45"), format(jdb));

  /* Simple array with strings */
  a.clear();
  EXPECT_EQ(Json_dom::J_ARRAY, a.json_type());
  EXPECT_FALSE(a.is_scalar());
  EXPECT_EQ(0U, a.size());
  Json_string js4(std::string("val1"));
  a.append_clone(&js4);
  Json_string js5(std::string("val2"));
  a.append_clone(&js5);
  EXPECT_EQ(2U, a.size());
  EXPECT_EQ(std::string("[\"val1\", \"val2\"]"), format(a));
  EXPECT_EQ(2U, a.depth());
  Json_dom *elt0= a[0];
  Json_dom *elt1= a[a.size() - 1];
  EXPECT_EQ(std::string("\"val1\""), format(elt0));
  EXPECT_EQ(std::string("\"val2\""), format(elt1));

  /* Simple object with string values, iterator and array cloning */
  Json_object o;
  EXPECT_EQ(Json_dom::J_OBJECT, o.json_type());
  EXPECT_FALSE(a.is_scalar());
  EXPECT_EQ(0U, o.cardinality());
  Json_null null;
  EXPECT_EQ(Json_dom::J_NULL, null.json_type());
  o.add_clone(std::string("key1"), &null);
  o.add_clone(std::string("key2"), &a);

  const std::string key_expected[]=
    {std::string("key1"), std::string("key2")};
  const std::string value_expected[]=
    {std::string("null"), std::string("[\"val1\", \"val2\"]")};

  int idx= 0;

  for (Json_object::const_iterator i= o.begin(); i != o.end(); ++i)
  {
    EXPECT_EQ(key_expected[idx], i->first);
    EXPECT_EQ(value_expected[idx], format(i->second));
    idx++;
  }

  /* Test uniqueness of keys */
  Json_string js6(std::string("should be discarded"));
  o.add_clone(std::string("key1"), &js6);
  EXPECT_EQ(2U, o.cardinality());
  EXPECT_EQ(std::string("{\"key1\": null, \"key2\": [\"val1\", \"val2\"]}"),
            format(o));
  EXPECT_EQ(3U, o.depth());

  /* Nested array inside object and object inside array,
   * and object cloning
   */
  Json_array level3;
  level3.append_clone(&o);
  Json_int ji2(123);
  level3.insert_clone(0U, &ji2);
  EXPECT_EQ(std::string("[123, {\"key1\": null, \"key2\": "
                        "[\"val1\", \"val2\"]}]"),
            format(level3));
  EXPECT_EQ(4U, level3.depth());

  /* Array access: index */
  Json_dom * const elt= level3[1];
  EXPECT_EQ(std::string("{\"key1\": null, \"key2\": "
                        "[\"val1\", \"val2\"]}"),
            format(elt));

  /* Object access: key look-up */
  EXPECT_EQ(Json_dom::J_OBJECT, elt->json_type());
  Json_object * const object_elt= down_cast<Json_object *>(elt);
  EXPECT_TRUE(object_elt != NULL);
  const Json_dom * const elt2= object_elt->get(std::string("key1"));
  EXPECT_EQ(std::string("null"), format(elt2));

  /* Clear object. */
  object_elt->clear();
  EXPECT_EQ(0U, object_elt->cardinality());

  /* Array remove element */
  EXPECT_TRUE(level3.remove(1));
  EXPECT_EQ(std::string("[123]"), format(level3));
  EXPECT_FALSE(level3.remove(level3.size()));
  EXPECT_EQ(std::string("[123]"), format(level3));

  /* Decimal scalar, including cloning */
  my_decimal m;
  EXPECT_FALSE(double2my_decimal(0, 3.14, &m));

  const Json_decimal jd(m);
  EXPECT_EQ(Json_dom::J_DECIMAL, jd.json_type());
  EXPECT_TRUE(jd.is_number());
  EXPECT_TRUE(jd.is_scalar());
  const my_decimal m_out= *jd.value();
  double m_d;
  double m_out_d;

  decimal2double(&m, &m_d);
  decimal2double(&m_out, &m_out_d);
  EXPECT_EQ(m_d, m_out_d);

  a.append_clone(&jd);
  std::auto_ptr<Json_array> b(static_cast<Json_array *>(a.clone()));
  EXPECT_EQ(std::string("[\"val1\", \"val2\", 3.14]"), format(a));
  EXPECT_EQ(std::string("[\"val1\", \"val2\", 3.14]"), format(b.get()));

  /* Array insert beyond end appends at end */
  a.clear();
  a.insert_alias(0, new (std::nothrow) Json_int(0));
  a.insert_alias(2, new (std::nothrow) Json_int(2));
  EXPECT_EQ(std::string("[0, 2]"), format(a));
  a.clear();
  a.insert_alias(0, new (std::nothrow) Json_int(0));
  a.insert_alias(1, new (std::nothrow) Json_int(1));
  EXPECT_EQ(std::string("[0, 1]"), format(a));

  /* Array clear, null type, boolean literals, including cloning */
  a.clear();
  Json_null jn;
  Json_boolean jbf(false);
  Json_boolean jbt(true);
  a.append_clone(&jn);
  a.append_clone(&jbf);
  a.append_clone(&jbt);
  std::auto_ptr<const Json_dom> c(a.clone());
  EXPECT_EQ(std::string("[null, false, true]"), format(a));
  EXPECT_EQ(std::string("[null, false, true]"), format(c.get()));

  /* DATETIME scalar */
  MYSQL_TIME dt;
  std::memset(&dt, 0, sizeof dt);
  MYSQL_TIME_STATUS status;
  EXPECT_FALSE(str_to_datetime(&my_charset_utf8mb4_bin,
                               "19990412",
                               8,
                               &dt,
                               (my_time_flags_t)0,
                               &status));
  const Json_datetime scalar(dt, MYSQL_TYPE_DATETIME);
  EXPECT_EQ(Json_dom::J_DATETIME, scalar.json_type());

  const MYSQL_TIME *dt_out= scalar.value();

  EXPECT_FALSE(std::memcmp(&dt, dt_out, sizeof(MYSQL_TIME)));
  EXPECT_EQ(std::string("\"1999-04-12\""), format(scalar));

  a.clear();
  a.append_clone(&scalar);
  EXPECT_EQ(std::string("[\"1999-04-12\"]"), format(a));

  EXPECT_FALSE(str_to_datetime(&my_charset_utf8mb4_bin,
                               "14-11-15 12.04.55.123456",
                               24,
                               &dt,
                               (my_time_flags_t)0,
                               &status));

  Json_datetime scalar2(dt, MYSQL_TYPE_DATETIME);
  EXPECT_EQ(std::string("\"2014-11-15 12:04:55.123456\""), format(scalar2));

  /* Opaque type storage scalar */
  const uint32 i= 0xCAFEBABE;
  char i_as_char[4];
  int4store(i_as_char, i);
  Json_opaque opaque(MYSQL_TYPE_TINY_BLOB, i_as_char, sizeof(i_as_char));
  EXPECT_EQ(Json_dom::J_OPAQUE, opaque.json_type());
  EXPECT_EQ(i, uint4korr(opaque.value()));
  EXPECT_EQ(MYSQL_TYPE_TINY_BLOB, opaque.type());
  EXPECT_EQ(sizeof(i_as_char), opaque.size());
  EXPECT_EQ(std::string("\"base64:type249:vrr+yg==\""),
            format(opaque));

  const char *encoded= "vrr+yg==";
  char *buff= new char[static_cast<size_t>
                       (base64_needed_decoded_length(static_cast<int>
                                                     (std::strlen(encoded))))];
  EXPECT_EQ(4, base64_decode(encoded, std::strlen(encoded), buff, NULL, 0));
  EXPECT_EQ(0xCAFEBABE, uint4korr(buff));
  delete[] buff;

  /* Build DOM from JSON text using rapdjson */
  const char *msg;
  size_t msg_offset;
  const char *sample_doc=
    "{\"abc\": 3, \"foo\": [1, 2, {\"foo\": 3.24}, null]}";
  std::auto_ptr<Json_dom> dom(Json_dom::parse(sample_doc,
                                              std::strlen(sample_doc),
                                              &msg, &msg_offset));
  EXPECT_TRUE(dom.get() != NULL);
  EXPECT_EQ(4U, dom->depth());
  EXPECT_EQ(std::string(sample_doc), format(dom.get()));

  const char *sample_array=
    "[3, {\"abc\": \"\\u0000inTheText\"}]";
  dom.reset(Json_dom::parse(sample_array, std::strlen(sample_array),
                            &msg, &msg_offset));
  EXPECT_TRUE(dom.get() != NULL);
  EXPECT_EQ(3U, dom->depth());
  EXPECT_EQ(std::string(sample_array), format(dom.get()));

  const char *sample_scalar_doc= "2";
  dom.reset(Json_dom::parse(sample_scalar_doc, std::strlen(sample_scalar_doc),
                            &msg, &msg_offset));
  EXPECT_TRUE(dom.get() != NULL);
  EXPECT_EQ(std::string(sample_scalar_doc), format(dom.get()));

  const char *max_uint_scalar= "18446744073709551615";
  dom.reset(Json_dom::parse(max_uint_scalar, std::strlen(max_uint_scalar),
                            &msg, &msg_offset));
  EXPECT_EQ(std::string(max_uint_scalar), format(dom.get()));

  /*
    Test that duplicate keys are eliminated, and that the returned
    keys are in the expected order (sorted on length before
    contents).
  */
  const char *sample_object= "{\"key1\":1, \"key2\":2, \"key1\":3, "
    "\"key1\\u0000x\":4, \"key1\\u0000y\":5, \"a\":6, \"ab\":7, \"b\":8, "
    "\"\":9, \"\":10}";
  const std::string expected[8][2]=
    {
      { "",        "9" },
      { "a",       "6" },
      { "b",       "8" },
      { "ab",      "7" },
      { "key1",    "1" },
      { "key2",    "2" },
      { std::string("key1\0x", 6), "4" },
      { std::string("key1\0y", 6), "5" },
    };
  dom.reset(Json_dom::parse(sample_object, std::strlen(sample_object),
                            &msg, &msg_offset));
  EXPECT_TRUE(dom.get() != NULL);
  const Json_object *obj= down_cast<const Json_object *>(dom.get());
  EXPECT_EQ(8U, obj->cardinality());
  idx= 0;

  for (Json_object::const_iterator it= obj->begin(); it != obj->end(); ++it)
  {
    EXPECT_EQ(expected[idx][0], it->first);
    EXPECT_EQ(expected[idx][1], format(it->second));
    idx++;
  }

  EXPECT_EQ(8, idx);

  /* Try to build DOM for JSON text using rapidjson on invalid text
     Included so we test error recovery
  */
  const char *half_object_item= "{\"label\": ";
  dom.reset(Json_dom::parse(half_object_item, std::strlen(half_object_item),
                            &msg, &msg_offset));
  const Json_dom *null_dom= NULL;
  EXPECT_EQ(null_dom, dom.get());

  const char *half_array_item= "[1,";
  dom.reset(Json_dom::parse(half_array_item, std::strlen(half_array_item),
                            &msg, &msg_offset));
  EXPECT_EQ(null_dom, dom.get());
}

/*
  Test that special characters are escaped when a Json_string is
  converted to text, so that it is possible to parse the resulting
  string. The JSON parser requires all characters in the range [0x00,
  0x1F] and the characters " (double-quote) and \ (backslash) to be
  escaped.
*/
TEST_F(JsonDomTest, EscapeSpecialChars)
{
  // Create a JSON string with all characters in the range [0, 127].
  char input[128];
  for (size_t i= 0; i < sizeof(input); ++i)
    input[i]= static_cast<char>(i);
  const Json_string jstr(std::string(input, sizeof(input)));

  // Now convert that value from JSON to text and back to JSON.
  std::string str= format(jstr);
  std::auto_ptr<Json_dom> dom(Json_dom::parse(str.c_str(), str.length(),
                                              NULL, NULL));
  EXPECT_NE(static_cast<Json_dom*>(NULL), dom.get());
  EXPECT_EQ(Json_dom::J_STRING, dom->json_type());

  // Expect to get the same string back, including all the special characters.
  const Json_string *jstr2= down_cast<const Json_string *>(dom.get());
  EXPECT_EQ(jstr.value(), jstr2->value());
}

void vet_wrapper_length(char * text, size_t expected_length )
{
  const char *msg;
  size_t msg_offset;
  Json_dom *dom= Json_dom::parse(text, std::strlen(text), &msg, &msg_offset);
  Json_wrapper dom_wrapper(dom);

  EXPECT_EQ(expected_length, dom_wrapper.length())
    << "Wrapped DOM: " << text << "\n";

  String  serialized_form;
  EXPECT_FALSE(json_binary::serialize(dom, &serialized_form));
  json_binary::Value binary=
    json_binary::parse_binary(serialized_form.ptr(),
                              serialized_form.length());
  Json_wrapper  binary_wrapper(binary);

  json_binary::Value::enum_type  binary_type= binary.type();

  if ((binary_type == json_binary::Value::ARRAY) ||
      (binary_type == json_binary::Value::OBJECT))
  {
    EXPECT_EQ(expected_length, binary.element_count())
      << "BINARY: " << text << " and data = " << binary.get_data() << "\n";
  }
  EXPECT_EQ(expected_length, binary_wrapper.length())
    << "Wrapped BINARY: " << text << "\n";
}

TEST_F(JsonDomTest, WrapperTest)
{
  // Constructors, assignment, copy constructors, aliasing
  Json_dom *d= new (std::nothrow) Json_null();
  Json_wrapper w(d);
  EXPECT_EQ(w.to_dom(), d);
  Json_wrapper w_2(w);
  EXPECT_NE(w.to_dom(), w_2.to_dom()); // deep copy

  Json_wrapper w_2b;
  EXPECT_TRUE(w_2b.empty());
  w_2b= w;
  EXPECT_NE(w.to_dom(), w_2b.to_dom()); // deep copy

  w.set_alias(); // d is now "free" again
  Json_wrapper w_3(w);
  EXPECT_EQ(w.to_dom(), w_3.to_dom()); // alias copy
  w_3= w;
  EXPECT_EQ(w.to_dom(), w_3.to_dom()); // alias copy

  Json_wrapper w_4(d); // give d a new owner
  Json_wrapper w_5;
  w_5.steal(&w_4); // takes over d
  EXPECT_EQ(w_4.to_dom(), w_5.to_dom());

  Json_wrapper w_6;
  EXPECT_EQ(Json_dom::J_ERROR, w_6.type());
  EXPECT_EQ(0U, w_6.length());
  EXPECT_EQ(0U, w_6.depth());

  Json_dom *i= new (std::nothrow) Json_int(1);
  Json_wrapper w_7(i);
  w_5.steal(&w_7); // should deallocate w_5's original

  // scalars
  vet_wrapper_length((char *) "false", 1);
  vet_wrapper_length((char *) "true", 1);
  vet_wrapper_length((char *) "null", 1);
  vet_wrapper_length((char *) "1.1", 1);
  vet_wrapper_length((char *) "\"hello world\"", 1);

  // objects
  vet_wrapper_length((char *) "{}", 0);
  vet_wrapper_length((char *) "{ \"a\" : 100 }", 1);
  vet_wrapper_length((char *) "{ \"a\" : 100, \"b\" : 200 }", 2);

  // arrays
  vet_wrapper_length((char *) "[]", 0);
  vet_wrapper_length((char *) "[ 100 ]", 1);
  vet_wrapper_length((char *) "[ 100, 200 ]", 2);

  // nested objects
  vet_wrapper_length((char *) "{ \"a\" : 100, \"b\" : { \"c\" : 300 } }", 2);

  // nested arrays
  vet_wrapper_length((char *) "[ 100, [ 200, 300 ] ]", 2);
}

void vet_merge(char * left_text, char * right_text, std::string expected )
{
  const char *msg;
  size_t msg_offset;
  Json_dom *left_dom= Json_dom::parse(left_text, std::strlen(left_text),
                                      &msg, &msg_offset);
  Json_dom *right_dom= Json_dom::parse(right_text, std::strlen(right_text),
                                       &msg, &msg_offset);
  Json_dom *result_dom= merge_doms(left_dom, right_dom);

  EXPECT_EQ(expected, format(*result_dom));

  delete result_dom;
}

TEST_F(JsonDomTest, MergeTest)
{
  // merge 2 scalars
  {
    SCOPED_TRACE("");
    vet_merge((char *) "1", (char *) "true", "[1, true]");
  }

  // merge a scalar with an array
  {
    SCOPED_TRACE("");
    vet_merge((char *) "1", (char *) "[true, false]", "[1, true, false]");
  }

  // merge an array with a scalar
  {
    SCOPED_TRACE("");
    vet_merge((char *) "[true, false]", (char *) "1", "[true, false, 1]");
  }

  // merge a scalar with an object
  {
    SCOPED_TRACE("");
    vet_merge((char *) "1", (char *) "{\"a\": 2}", "[1, {\"a\": 2}]");
  }

  // merge an object with a scalar
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 2}", (char *) "1", "[{\"a\": 2}, 1]");
  }

  // merge 2 arrays
  {
    SCOPED_TRACE("");
    vet_merge((char *) "[1, 2]", (char *) "[3, 4]", "[1, 2, 3, 4]");
  }

  // merge 2 objects
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": 2 }",
              (char *) "{\"c\": 3, \"d\": 4 }",
              "{\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4}");
  }

  // merge an array with an object
  {
    SCOPED_TRACE("");
    vet_merge((char *) "[1, 2]",
              (char *) "{\"c\": 3, \"d\": 4 }",
              "[1, 2, {\"c\": 3, \"d\": 4}]");
  }

  // merge an object with an array
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"c\": 3, \"d\": 4 }",
              (char *) "[1, 2]",
              "[{\"c\": 3, \"d\": 4}, 1, 2]");
  }

  // merge two objects which share a key. scalar + scalar
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": 2 }",
              (char *) "{\"b\": 3, \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, 3], \"d\": 4}");
  }

  // merge two objects which share a key. scalar + array
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": 2 }",
              (char *) "{\"b\": [3, 4], \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, 3, 4], \"d\": 4}");
  }

  // merge two objects which share a key. array + scalar
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": [2, 3] }",
              (char *) "{\"b\": 4, \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, 3, 4], \"d\": 4}");
  }

  // merge two objects which share a key. scalar + object
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": 2 }",
              (char *) "{\"b\": {\"e\": 7, \"f\": 8}, \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, {\"e\": 7, \"f\": 8}], \"d\": 4}");
  }

  // merge two objects which share a key. object + scalar
  {
    SCOPED_TRACE("");
    vet_merge((char *) (char *) "{\"b\": {\"e\": 7, \"f\": 8}, \"d\": 4 }",
              (char *) "{\"a\": 1, \"b\": 2 }",
              "{\"a\": 1, \"b\": [{\"e\": 7, \"f\": 8}, 2], \"d\": 4}");
  }

  // merge two objects which share a key. array + array
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": [2, 9] }",
              (char *) "{\"b\": [10, 11], \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, 9, 10, 11], \"d\": 4}");
  }

  // merge two objects which share a key. array + object
  {
    SCOPED_TRACE("");
    vet_merge((char *) "{\"a\": 1, \"b\": [2, 9] }",
              (char *) "{\"b\": {\"e\": 7, \"f\": 8}, \"d\": 4 }",
              "{\"a\": 1, \"b\": [2, 9, {\"e\": 7, \"f\": 8}], \"d\": 4}");
  }

  // merge two objects which share a key. object + array
  {
    SCOPED_TRACE("");
    vet_merge((char *) (char *) "{\"b\": {\"e\": 7, \"f\": 8}, \"d\": 4 }",
              (char *) "{\"a\": 1, \"b\": [2, 9] }",
              "{\"a\": 1, \"b\": [{\"e\": 7, \"f\": 8}, 2, 9], \"d\": 4}");
  }

  // merge two objects which share a key. object + object
  {
    SCOPED_TRACE("");
    vet_merge((char *) (char *) "{\"b\": {\"e\": 7, \"f\": 8}, \"d\": 4 }",
              (char *) "{\"a\": 1, \"b\": {\"e\": 20, \"g\": 21 } }",
              "{\"a\": 1, \"b\": {\"e\": [7, 20], \"f\": 8, \"g\": 21}, "
              "\"d\": 4}");
  }
}

}  // namespace
