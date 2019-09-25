/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <cstddef>

#include "plugin/x/src/json_generator.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

class JSON_generator_test : public ::testing::Test {
 public:
  using Octet_type = Scalar::Octets::Content_type;
  Query_string_builder m_qb;
};

TEST_F(JSON_generator_test, int_scalar) {
  generate_json(&m_qb, Scalar(-1));
  EXPECT_STREQ("-1", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, unsigned_int_scalar) {
  generate_json(&m_qb, Scalar(2u));
  EXPECT_STREQ("2", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, bool_scalar) {
  generate_json(&m_qb, Scalar(true));
  EXPECT_STREQ("true", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, float_scalar) {
  generate_json(&m_qb, Scalar(3.3f));
  EXPECT_STREQ("3.3", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, double_scalar) {
  generate_json(&m_qb, Scalar(4.4));
  EXPECT_STREQ("4.4", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, c_string_scalar) {
  generate_json(&m_qb, Scalar("five"));
  EXPECT_STREQ("\"five\"", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, string_scalar) {
  generate_json(&m_qb, Scalar(Scalar("six")));
  EXPECT_STREQ("\"six\"", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, string_scalar_with_special_chars) {
  generate_json(&m_qb, Scalar(Scalar("s'e\\ve\"n")));
  EXPECT_STREQ("\"s\\'e\\\\ve\\\"n\"", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, null_scalar) {
  generate_json(&m_qb, Scalar(Scalar::Null{}));
  EXPECT_STREQ("NULL", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, plain_octets_scalar) {
  generate_json(&m_qb,
                Scalar(Scalar::Octets("abc")));
  EXPECT_STREQ("'abc'", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, json_octets_scalar) {
  generate_json(&m_qb, Scalar(Scalar::Octets("{\"test\":\"value\"}",
                                             Octet_type::k_json)));
  EXPECT_STREQ("{\"test\":\"value\"}", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, xml_octets_scalar) {
  generate_json(&m_qb,
                Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml)));
  EXPECT_STREQ("'<tag>foo</tag>'", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, geometry_octets_scalar) {
  EXPECT_THROW(generate_json(&m_qb, Scalar(Scalar::Octets(
                                        "010", Octet_type::k_geometry))),
               Expression_generator::Error);
}

TEST_F(JSON_generator_test, empty_array) {
  generate_json(&m_qb, Any::Array{});
  EXPECT_STREQ("[]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_ints) {
  generate_json(&m_qb, Any::Array{1, 2, 3, 4, 5});
  EXPECT_STREQ("[1,2,3,4,5]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_doubles) {
  generate_json(&m_qb, Any::Array{1.1, 2.2, 3.3, 4.4, 5.5});
  EXPECT_STREQ("[1.1,2.2,3.3,4.4,5.5]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_bools) {
  generate_json(&m_qb, Any::Array{true, false, true, true, true, false});
  EXPECT_STREQ("[true,false,true,true,true,false]",
               m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_strings) {
  generate_json(&m_qb, Any::Array{Scalar::String{"1"}, Scalar::String{"2"},
                                  Scalar::String{"3"}, Scalar::String{"4"},
                                  Scalar::String{"5"}});
  EXPECT_STREQ("[\"1\",\"2\",\"3\",\"4\",\"5\"]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_nulls) {
  generate_json(&m_qb, Any::Array{Scalar::Null{}});
  EXPECT_STREQ("[NULL]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_octets_valid) {
  generate_json(
      &m_qb,
      Any::Array{
          Scalar(Scalar::Octets("abc")),
          Scalar(Scalar::Octets("{\"test\":\"value\"}", Octet_type::k_json)),
          Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml))});
  EXPECT_STREQ("['abc',{\"test\":\"value\"},'<tag>foo</tag>']",
               m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_octets_invalid) {
  EXPECT_THROW(
      generate_json(
          &m_qb,
          Any::Array{
              Scalar(Scalar::Octets("abc")),
              Scalar(
                  Scalar::Octets("{\"test\":\"value\"}", Octet_type::k_json)),
              Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml)),
              Scalar(Scalar::Octets("010", Octet_type::k_geometry))}),
      Expression_generator::Error);
}

TEST_F(JSON_generator_test, array_of_arrays) {
  generate_json(&m_qb, Any::Array{Any::Array{1, 2}, Any::Array{3.3f, 4.4},
                                  Any::Array{true}, Any::Array{}});
  EXPECT_STREQ("[[1,2],[3.3,4.4],[true],[]]", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, object_with_invalid_key) {
  EXPECT_THROW(generate_json(&m_qb, Any::Object{{"", "val"}}),
               Expression_generator::Error);
}

TEST_F(JSON_generator_test, object_with_invalid_value) {
  EXPECT_THROW(generate_json(&m_qb, Any::Object("key", nullptr)),
               Expression_generator::Error);
}

TEST_F(JSON_generator_test, empty_object) {
  generate_json(&m_qb, Any::Object{});
  EXPECT_STREQ("{}", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, object_of_octets_valid) {
  generate_json(
      &m_qb,
      Any::Object{
          {"1", Scalar(Scalar::Octets("abc"))},
          {"2",
           Scalar(Scalar::Octets("{\"test\":\"value\"}", Octet_type::k_json))},
          {"3", Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml))}});
  EXPECT_STREQ(
      "{\"1\":'abc',\"2\":{\"test\":\"value\"},\"3\":'<tag>foo</tag>'}",
      m_qb.get().c_str());
}

TEST_F(JSON_generator_test, object_of_octets_invalid) {
  EXPECT_THROW(
      generate_json(
          &m_qb,
          Any::Object{
              {"1", Scalar(Scalar::Octets("abc"))},
              {"2", Scalar(Scalar::Octets("{\"test\":\"value\"}",
                                          Octet_type::k_json))},
              {"3",
               Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml))},
              {"4", Scalar(Scalar::Octets("010", Octet_type::k_geometry))}}),
      Expression_generator::Error);
}

TEST_F(JSON_generator_test, homogenous_object) {
  generate_json(&m_qb, Any::Object{{"1", Scalar::String{"val1"}},
                                   {"2", Scalar::String{"val2"}},
                                   {"3", Scalar::String{"val3"}}});
  EXPECT_STREQ("{\"1\":\"val1\",\"2\":\"val2\",\"3\":\"val3\"}",
               m_qb.get().c_str());
}

TEST_F(JSON_generator_test, heterogenous_object) {
  generate_json(
      &m_qb,
      Any::Object{
          {"1", 1},
          {"2", Scalar::Null{}},
          {"3", Scalar::String{"val3"}},
          {"4", Any::Array{1, 2, 3}},
          {"5", 5.5},
          {"6", true},
          {"7", Scalar(Scalar::Octets("<tag>foo</tag>", Octet_type::k_xml))}});
  EXPECT_STREQ(
      "{\"1\":1,\"2\":NULL,\"3\":\"val3\",\"4\":[1,2,3],\"5\":5.5,\"6\":"
      "true,\"7\":'<tag>foo</tag>'}",
      m_qb.get().c_str());
}

TEST_F(JSON_generator_test, object_of_objects) {
  generate_json(
      &m_qb,
      Any::Object{
          {"obj1", Any::Object{{"1:", 11}}},
          {"obj2", Any::Object{{"2:", "two"}}},
          {"obj3", Any::Object{{"3:", Scalar::Null{}}, {"3.5:", 3.3}}},
          {"obj4", Any::Object{}},
          {"obj5",
           Any::Object{{"5:", Scalar(Scalar::Octets("{\"test\":\"value\"}",
                                                    Octet_type::k_json))}}}});
  EXPECT_STREQ(
      "{\"obj1\":{\"1:\":11},\"obj2\":{\"2:\":\"two\"},\"obj3\":{\"3:\":NULL,"
      "\"3.5:\":3.3},\"obj4\":{},\"obj5\":{\"5:\":{\"test\":\"value\"}}}",
      m_qb.get().c_str());
}

TEST_F(JSON_generator_test, object_of_arrays) {
  generate_json(&m_qb, Any::Object{{"1", Any::Array{1, 2}},
                                   {"2", Any::Array{3, 4}},
                                   {"3", Any::Array{}}});
  EXPECT_STREQ("{\"1\":[1,2],\"2\":[3,4],\"3\":[]}", m_qb.get().c_str());
}

TEST_F(JSON_generator_test, array_of_objects) {
  generate_json(&m_qb,
                Any::Array{Any::Object{{"1:", 11}}, Any::Object{{"2:", "two"}},
                           Any::Object{{"3:", Scalar::Null{}}, {"3.5:", 3.3}},
                           Any::Object{}});
  EXPECT_STREQ("[{\"1:\":11},{\"2:\":\"two\"},{\"3:\":NULL,\"3.5:\":3.3},{}]",
               m_qb.get().c_str());
}

}  // namespace test
}  // namespace xpl
