/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include <iostream>
#include <fstream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <m_ctype.h>
#include <string>

#include "json_path.h"
#include "json_dom.h"
#include "sql_string.h"

#include "test_utils.h"

/**
 Test json path abstraction.
 */
namespace json_path_unittest
{

class JsonPathTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  my_testing::Server_initializer initializer;
};

/**
  Struct that defines input and expected result for negative testing
  of path parsing.
*/
struct Bad_path
{
  const bool m_begins_with_column_id;  ///< true if column scope
  const char *m_path_expression;       ///< the path to parse
  const size_t m_expected_index;       ///< the offset of the syntax error
};

/**
  Class that contains parameterized test cases for bad paths.
*/
class JsonBadPathTestP : public ::testing::TestWithParam<Bad_path>
{
};

/**
  Struct that defines input and expected result for positive testing
  of path parsing.
*/
struct Good_path
{
  const bool m_begins_with_column_id;  ///< true if column scope
  const char *m_path_expression;       ///< the path to parse
  const char *m_expected_path;         ///< expected canonical path
};

/**
  Struct that defines input and expected result for testing
  Json_dom.get_location().
*/
struct Location_tuple
{
  const bool m_begins_with_column_id;  // true if column scope
  const char *m_json_text;             // the document text
  const char *m_path_expression;       // the path to parse
};

/**
  Struct that defines input and expected result for testing
  the only_needs_one argument of Json_wrapper.seek().
*/
struct Ono_tuple
{
  const bool m_begins_with_column_id;  // true if column scope
  const char *m_json_text;             // the document text
  const char *m_path_expression;       // the path to parse
  const uint m_expected_hits;         // total number of matches
};

/**
  Struct that defines input for cloning test cases.
*/
struct Clone_tuple
{
  const bool m_begins_with_column_id;  // true if column scope
  const char *m_path_expression_1;     // the first path to parse
  const char *m_path_expression_2;     // the second path to parse
};

/**
  Class that contains parameterized test cases for good paths.
*/
class JsonGoodPathTestP : public ::testing::TestWithParam<Good_path>
{
};

/**
  Class that contains parameterized test cases for dom locations.
*/
class JsonGoodLocationTestP : public ::testing::TestWithParam<Location_tuple>
{
};

/**
  Class that contains parameterized test cases for the only_needs_one
  arg of Json_wrapper.seek().
*/
class JsonGoodOnoTestP : public ::testing::TestWithParam<Ono_tuple>
{
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
  my_testing::Server_initializer initializer;
};

/**
  Class that contains parameterized test cases for cloning tests.
*/
class JsonGoodCloneTestP : public ::testing::TestWithParam<Clone_tuple>
{
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
  my_testing::Server_initializer initializer;
};


/*
  Constants
*/

/*
  Helper functions.
*/

/* Concatenate the left and right strings and write the result into dest */
char *concat (char *dest, char *left, const char *right)
{
  dest[0]= 0;
  std::strcat(dest, left);
  std::strcat(dest, right);

  return dest;
}

/** Code common to good_path() and good_leg_types() */
void good_path_common(bool begins_with_column_id, const char *path_expression,
                      Json_path *json_path)
{
  size_t bad_idx= 0;
  EXPECT_FALSE(parse_path(begins_with_column_id,
                          strlen(path_expression),
                          path_expression,
                          json_path,
                          &bad_idx));

  EXPECT_EQ(0U, bad_idx) <<
    "Parse pointer for " << path_expression <<
    " should have been 0\n";
}

/** Verify that a good path parses correctly */
void good_path(bool begins_with_column_id, bool check_path,
               const char *path_expression, std::string expected_path)
{
  Json_path json_path;
  good_path_common(begins_with_column_id, path_expression, &json_path);
  if (check_path)
  {
    String str;
    EXPECT_EQ(0, json_path.to_string(&str));
    EXPECT_EQ(expected_path, std::string(str.ptr(), str.length()));
  }
}

void good_path(bool begins_with_column_id, const char *path_expression,
               std::string expected_path)
{
  good_path(begins_with_column_id, true, path_expression, expected_path);
}

/** Shorter form of good_path() */
void good_path(bool begins_with_column_id, const char *path_expression)
{
  good_path(false, false, path_expression, "");
}

/** Verify whether the path contains a wildcard or ellipsis token */
void contains_wildcard(bool begins_with_column_id, char *path_expression,
                       bool expected_answer)
{
  Json_path json_path;
  good_path_common(begins_with_column_id, path_expression, &json_path);
  EXPECT_EQ(expected_answer, json_path.contains_wildcard_or_ellipsis());
}

/** Verify that the leg at the given offset looks good */
void good_leg_at(bool begins_with_column_id, char *path_expression,
                 int leg_index, std::string expected_leg,
                 enum_json_path_leg_type expected_leg_type)
{
  Json_path json_path;
  good_path_common(begins_with_column_id, path_expression, &json_path);

  const Json_path_leg *actual_leg= json_path.get_leg_at(leg_index);
  EXPECT_EQ((expected_leg.size() == 0), (actual_leg == NULL));
  if (actual_leg != NULL)
  {
    String str;
    EXPECT_EQ(0, actual_leg->to_string(&str));
    EXPECT_EQ(expected_leg, std::string(str.ptr(), str.length()));
    EXPECT_EQ(expected_leg_type, actual_leg->get_type());
  }
}


/* compare two path legs */
void compare_legs(const Json_path_leg *left, const Json_path_leg *right)
{
  String  left_str;
  String  right_str;
  EXPECT_EQ(0, left->to_string(&left_str));
  EXPECT_EQ(0, right->to_string(&right_str));
  EXPECT_EQ(std::string(left_str.ptr(), left_str.length()),
            std::string(right_str.ptr(), right_str.length()));
}


/** Compare two paths */
void compare_paths(Json_path &left, Json_path_clone &right)
{
  EXPECT_EQ(left.leg_count(), right.leg_count());

  for (size_t idx= 0; idx < left.leg_count(); idx++)
  {
    compare_legs(left.get_leg_at(idx), right.get_leg_at(idx));
  }
}


/** Verify that clones look alike */
void verify_clone(bool begins_with_column_id,
                  const char *path_expression_1,
                  const char *path_expression_2)
{
  Json_path_clone cloned_path;

  Json_path real_path1;
  good_path_common(begins_with_column_id, path_expression_1, &real_path1);
  EXPECT_FALSE(cloned_path.set(&real_path1));
  compare_paths(real_path1, cloned_path);

  Json_path real_path2;
  good_path_common(begins_with_column_id, path_expression_2, &real_path2);
  EXPECT_FALSE(cloned_path.set(&real_path2));
  compare_paths(real_path2, cloned_path);
}

/**
   Verify that a good path has the expected sequence of leg types.
*/
void good_leg_types(bool begins_with_column_id, char *path_expression,
                    enum_json_path_leg_type *expected_leg_types, size_t length)
{
  Json_path json_path;
  good_path_common(begins_with_column_id, path_expression, &json_path);

  EXPECT_EQ(length, json_path.leg_count());
  for (size_t idx= 0; idx < length; idx++)
  {
    const Json_path_leg *leg= json_path.get_leg_at(idx);
    EXPECT_EQ(expected_leg_types[idx], leg->get_type());
  }
}

/** Verify that a bad path fails as expected */
void bad_path(bool begins_with_column_id, const char *path_expression,
              size_t expected_index)
{
  size_t actual_index= 0;
  Json_path json_path;
  EXPECT_TRUE(parse_path(begins_with_column_id,
                         strlen(path_expression),
                         path_expression,
                         &json_path,
                         &actual_index))
    << "Unexpectedly parsed " << path_expression;
  EXPECT_EQ(expected_index, actual_index);
}

/** Bad identifiers are ok as membern names if they are double-quoted */
void bad_identifier(const char *identifier, size_t expected_index)
{
  char dummy1[ 30 ];
  char dummy2[ 30 ];
  char *path_expression;

  path_expression= concat(dummy1, (char *) "$.", identifier);
  bad_path(false, path_expression, expected_index);

  path_expression= concat(dummy1, (char *) "$.\"", identifier);
  path_expression= concat(dummy2, path_expression, (const char *) "\"");
  good_path(false, path_expression);
}

/*
  Helper functions for Json_wrapper tests.
*/

void vet_wrapper_seek(Json_wrapper &wrapper, const Json_path &path,
                      std::string expected, bool expected_null)
{
  Json_wrapper_vector hits(PSI_NOT_INSTRUMENTED);
  wrapper.seek(path, &hits, true, false);
  String  result_buffer;

  if (hits.size() == 1)
  {
    EXPECT_FALSE(hits[0].to_string(&result_buffer, true, "test"));
  }
  else
  {
    Json_array *a= new (std::nothrow) Json_array();
    for (uint i= 0; i < hits.size(); ++i)
    {
      a->append_clone(hits[i].to_dom());
    }
    Json_wrapper w(a);
    EXPECT_FALSE(w.to_string(&result_buffer, true, "test"));
  }

  std::string actual= std::string(result_buffer.ptr(),
                                  result_buffer.length());

  if (expected_null)
  {
    const char *source_output= (char *) "";
    const char *result_output= (char *) "";

    if (hits.size() > 0)
    {
      String  source_buffer;
      EXPECT_FALSE(wrapper.to_string(&source_buffer, true, "test"));
      source_output= source_buffer.ptr();
      result_output= actual.c_str();
    }
    EXPECT_TRUE(hits.size() == 0)
      << "Unexpected result wrapper for "
      << source_output
      << ". The output is "
      << result_output
      << "\n";
  }
  else
  {
    EXPECT_EQ(expected, actual);
  }
}

void vet_wrapper_seek(char *json_text, char *path_text,
                      std::string expected, bool expected_null)
{
  const char *msg;
  size_t msg_offset;

  Json_dom *dom= Json_dom::parse(json_text, std::strlen(json_text),
                                 &msg, &msg_offset);
  Json_wrapper dom_wrapper(dom);

  String  serialized_form;
  EXPECT_FALSE(json_binary::serialize(dom, &serialized_form));
  json_binary::Value binary=
    json_binary::parse_binary(serialized_form.ptr(),
                              serialized_form.length());
  Json_wrapper binary_wrapper(binary);

  Json_path path;
  good_path_common(false, path_text, &path);
  vet_wrapper_seek(dom_wrapper, path, expected, expected_null);
  vet_wrapper_seek(binary_wrapper, path, expected, expected_null);
}

void vet_dom_location(bool begins_with_column_id,
                      const char *json_text, const char *path_text)
{
  const char *msg;
  size_t msg_offset;
  Json_dom *dom= Json_dom::parse(json_text, std::strlen(json_text),
                                 &msg, &msg_offset);
  Json_wrapper dom_wrapper(dom);
  Json_path path;
  good_path_common(begins_with_column_id, path_text, &path);
  Json_dom_vector hits(PSI_NOT_INSTRUMENTED);

  dom->seek(path, &hits, true, false);
  EXPECT_EQ(1U, hits.size());
  if (hits.size() > 0)
  {
    Json_dom *child= hits[0];
    Json_path location= child->get_location();
    String str;
    EXPECT_EQ(0, location.to_string(&str));
    EXPECT_EQ(path_text, std::string(str.ptr(), str.length()));
  }
}


/**
  Vet the short-circuiting effects of the only_needs_one argument
  of Json_wrapper.seek().

  @param[in] wrapper        A wrapped JSON document.
  @param[in] path           A path to search for.
  @param[in] expected_hits  Total number of expected matches.
*/
void vet_only_needs_one(Json_wrapper &wrapper, const Json_path &path,
                        uint expected_hits)
{
  Json_wrapper_vector all_hits(PSI_NOT_INSTRUMENTED);
  wrapper.seek(path, &all_hits, true, false);

  EXPECT_EQ(expected_hits, all_hits.size());

  Json_wrapper_vector only_needs_one_hits(PSI_NOT_INSTRUMENTED);
  wrapper.seek(path, &only_needs_one_hits, true, true);
  uint expected_onoh_hits= (expected_hits == 0) ? 0 : 1;
  EXPECT_EQ(expected_onoh_hits, only_needs_one_hits.size());
}


/**
  Vet the short-circuiting effects of the only_needs_one argument
  of Json_wrapper.seek().

  @param[in] begins_with_column_id  True if the path begins with a column.
  @param[in] json_text              Text of the json document to search.
  @param[in] path_text              Text of the path expression to use.
  @param[in] expected_hits          Total number of expected matches.
*/
void vet_only_needs_one(bool begins_with_column_id,
                        const char *json_text, const char *path_text,
                        uint expected_hits)
{
  const char *msg;
  size_t msg_offset;

  Json_dom *dom= Json_dom::parse(json_text, std::strlen(json_text),
                                 &msg, &msg_offset);
  Json_wrapper dom_wrapper(dom);

  String  serialized_form;
  EXPECT_FALSE(json_binary::serialize(dom, &serialized_form));
  json_binary::Value binary=
    json_binary::parse_binary(serialized_form.ptr(),
                              serialized_form.length());
  Json_wrapper binary_wrapper(binary);

  Json_path path;
  good_path_common(begins_with_column_id, path_text, &path);
  vet_only_needs_one(dom_wrapper, path, expected_hits);
  vet_only_needs_one(binary_wrapper, path, expected_hits);
}


/*

  Helper functions for testing Json_object.remove()
  and Json_array.remove().
*/

/**
   Format a Json_dom object to JSON text using  Json_wrapper's
   to_string functionality.

   @param d The DOM object to be formatted
*/
std::string format(Json_dom *dom)
{
  String buffer;
  Json_wrapper wrapper(dom->clone());
  EXPECT_FALSE(wrapper.to_string(&buffer, true, "format"));

  return std::string(buffer.ptr(), buffer.length());
}

void vet_remove(Json_dom *parent,
                const Json_path &path,
                std::string expected,
                bool expect_match)
{
  Json_dom_vector hits(PSI_NOT_INSTRUMENTED);

  parent->seek(path, &hits, true, false);

  if (expect_match)
  {
    EXPECT_EQ(1U, hits.size());

    if (hits.size() > 0)
    {
      const Json_dom *child= hits[0];

      bool was_removed= false;
      if (parent->json_type() == Json_dom::J_OBJECT)
      {
        Json_object *object= (Json_object *) parent;
        was_removed= object->remove(child);
      }
      else
      {
        Json_array *array= (Json_array *) parent;
        was_removed= array->remove(child);
      }

      EXPECT_TRUE(was_removed);
    }
  }
  else
  {
    EXPECT_EQ(0U, hits.size());
  }

  EXPECT_EQ(expected, format(parent));
}


void vet_remove(char *json_text, char *path_text, std::string expected,
                bool expect_match)
{
  const char *msg;
  size_t msg_offset;

  Json_dom *parent= Json_dom::parse(json_text, std::strlen(json_text),
                                    &msg, &msg_offset);
  Json_path path;
  good_path_common(false, path_text, &path);
  String  serialized_form;
  EXPECT_FALSE(json_binary::serialize(parent, &serialized_form));
  json_binary::Value parent_binary=
    json_binary::parse_binary(
                              serialized_form.ptr(), serialized_form.length());
  Json_dom *reparsed_parent= Json_dom::parse(parent_binary);

  vet_remove(parent, path, expected, expect_match);
  vet_remove(reparsed_parent, path, expected, expect_match);

  delete parent;
  delete reparsed_parent;
}

/*
  Tests
*/

// Good paths with no column scope.
static const Good_path good_paths_no_column_scope[]=
{
  { false, "$", "$" },
  { false, " $", "$" },
  { false, "$ ", "$" },
  { false, "  $   ", "$" },

  { false, "$[5]", "$[5]" },
  { false, "$[ 5 ]", "$[5]" },
  { false, " $[ 5 ] ", "$[5]" },
  { false, " $ [ 5  ] ", "$[5]" },

  { false, "$[456]", "$[456]" },
  { false, "$[ 456 ]", "$[456]" },
  { false, " $[ 456 ] ", "$[456]" },
  { false, " $ [  456   ] ", "$[456]" },

  { false, "$.a", "$.a" },
  { false, "$ .a", "$.a" },
  { false, "$. a", "$.a" },
  { false, " $ .  a ", "$.a" },

  { false, " $. abc", "$.abc" },
  { false, " $ . abc", "$.abc" },
  { false, " $ . abc ", "$.abc" },
  { false, " $  . abc ", "$.abc" },

  { false, "$.a[7]", "$.a[7]" },
  { false, " $ . a [ 7 ] ", "$.a[7]" },

  { false, "$[7].a", "$[7].a" },
  { false, " $ [ 7 ] . a ", "$[7].a" },

  { false, "$.*", "$.*" },
  { false, " $ . * ", "$.*" },

  { false, "$.*.b", "$.*.b" },
  { false, " $ . * . b ", "$.*.b" },

  { false, "$.*[4]", "$.*[4]" },
  { false, "  $ . * [ 4 ]  ", "$.*[4]" },

  { false, "$[*]", "$[*]" },
  { false, " $ [ * ] ", "$[*]" },

  { false, "$[*].a", "$[*].a" },
  { false, "  $ [ * ] . a ", "$[*].a" },

  { false, "$[*][31]", "$[*][31]" },
  { false, " $ [ * ] [ 31 ] ", "$[*][31]" },

  { false, "$**.abc", "$**.abc" },
  { false, " $  ** . abc ", "$**.abc" },

  { false, "$**[0]", "$**[0]" },
  { false, " $ ** [ 0 ] ", "$**[0]" },

  { false, "$**.a", "$**.a" },
  { false, " $ ** . a ", "$**.a" },

  // backslash in front of a quote
  { false, "$.\"\\\\\"", "$.\"\\\\\"" },
};

/** Test good paths without column scope */
TEST_P(JsonGoodPathTestP, GoodPaths)
{
  Good_path param= GetParam();
  good_path(param.m_begins_with_column_id,
            param.m_path_expression,
            param.m_expected_path);
}

INSTANTIATE_TEST_CASE_P(PositiveNoColumnScope, JsonGoodPathTestP,
                        ::testing::ValuesIn(good_paths_no_column_scope));

/** Test that path leg types look correct. */
TEST_F(JsonPathTest, LegTypes)
{
  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types1[]= { jpl_member };
    good_leg_types(false, (char *) "$.a", leg_types1, 1);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types2[]= { jpl_array_cell };
    good_leg_types(false, (char *) "$[3456]", leg_types2, 1);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types3[]= { jpl_member_wildcard };
    good_leg_types(false, (char *) "$.*", leg_types3, 1);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types4[]= { jpl_array_cell_wildcard };
    good_leg_types(false, (char *) "$[*]", leg_types4, 1);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types5[]= { jpl_member, jpl_member };
    good_leg_types(false, (char *) "$.foo.bar", leg_types5, 2);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types6[]= { jpl_member, jpl_array_cell };
    good_leg_types(false, (char *) "$.foo[9876543210]", leg_types6, 2);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types7[]= { jpl_member, jpl_member_wildcard };
    good_leg_types(false, (char *) "$.foo.*", leg_types7, 2);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types8[]=
      { jpl_member, jpl_array_cell_wildcard };
    good_leg_types(false, (char *) "$.foo[*]", leg_types8, 2);
  }

  {
    SCOPED_TRACE("");
    enum_json_path_leg_type leg_types9[]= { jpl_ellipsis, jpl_member };
    good_leg_types(false, (char *) "$**.foo", leg_types9, 2);
  }

  {
    SCOPED_TRACE("");
    good_leg_types(false, (char *) " $ ", NULL, 0);
  }
}

/** Test accessors. */
TEST_F(JsonPathTest, Accessors)
{
  {
    SCOPED_TRACE("");
    good_leg_at(false, (char *) "$[*][31]", 0, "[*]",
                jpl_array_cell_wildcard);
  }
  {
    SCOPED_TRACE("");
    good_leg_at(false, (char *) "$.abc[ 3 ].def", 2, ".def", jpl_member);
  }
  {
    SCOPED_TRACE("");
    good_leg_at(false, (char *) "$.abc**.def", 1, "**",
                jpl_ellipsis);
  }
  {
    SCOPED_TRACE("");
    good_leg_at(false, (char *) "$.abc**.def", 3, "", jpl_member);
  }
  {
    SCOPED_TRACE("");
    good_leg_at(false, (char *) "$", 0, "", jpl_member);
  }
}

/** Test detection of wildcard/ellipsis tokens. */
TEST_F(JsonPathTest, WildcardDetection)
{
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$.foo", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[3]", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$.foo.bar", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[3].foo", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[3][5]", false);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$.*", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[*]", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$.*.bar", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$**.bar", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[*].foo", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$**.foo", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[3].*", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$[*][5]", true);
  }
  {
    SCOPED_TRACE("");
    contains_wildcard(false, (char *) "$**[5]", true);
  }
}

TEST_P(JsonBadPathTestP, BadPaths)
{
  Bad_path param= GetParam();
  bad_path(param.m_begins_with_column_id,
           param.m_path_expression,
           param.m_expected_index);
}

// Bad paths with no column scope.
static const Bad_path bad_paths_no_column_scope[]=
{
  // no leading $
  { false, "foo", 1 },
  { false, "[5]", 1 },

  // no period before key name
  { false, "$foo", 1 },
  { false, "$[5]foo", 4 },

  // array index not a number
  { false, "$[a]", 2 },
  { false, "$[5].foo[b]", 9 },

  // absurdly large array index
  { false, "$[9999999999999999999999999999999999999999"
    "999999999999999999999999999]", 69 },

  // period not followed by member name
  { false, "$.", 2 },
  { false, "$.foo.", 6 },
  { false, "$[3].", 5 },
  { false, "$.[3]", 2 },
  { false, "$.foo[4].", 9 },

  // array index not terminated by ]
  { false, "$[4", 3 },
  { false, "$[4a]", 4 },
  { false, "$[4abc]", 4 },

  // ends in ellipsis
  { false, "$**", 3 },
  { false, "$.foo**", 7 },

  // paths shouldn't have column scopes if the caller says
  // they don't
  { false, "a.b.c$", 1 },
  { false, "b.c$", 1 },
  { false, "c$", 1 },
  { false, "a.b.c$.e", 1 },
  { false, "b.c$.e", 1 },
  { false, "c$.e", 1 },

  // unterminated double-quoted name
  { false, "$.\"bar", 6 },

  // 0-length member name
  { false, "$..ab", 2 },
  { false, "$.", 2 },
  { false, "$.\"\"", 4 },

  // backslash in front of a quote, and no end quote
  { false, "$.\"\\\"", 5 },
};

INSTANTIATE_TEST_CASE_P(NegativeNoColumnScope, JsonBadPathTestP,
                        ::testing::ValuesIn(bad_paths_no_column_scope));

/** Good paths with column scope not supported yet */
TEST_F(JsonPathTest, PositiveColumnScope)
{
  //
  // Test good path syntax
  //
  bad_path(true, (char *) "a.b.c$", 0);
}

/** Test good quoted key names */
static const Good_path good_quoted_key_names[]=
{
  { false, "$.\"a\"", "$.a" },
  { false, "$ .\"a\"", "$.a" },
  { false, "$. \"a\"", "$.a" },
  { false, " $ .  \"a\" ", "$.a" },

  { false, " $. \"abc\"", "$.abc" },
  { false, " $ . \"abc\"", "$.abc" },
  { false, " $ . \"abc\" ", "$.abc" },
  { false, " $  . \"abc\" ", "$.abc" },

  { false, "$.\"a\"[7]", "$.a[7]" },
  { false, " $ . \"a\" [ 7 ] ", "$.a[7]" },

  { false, "$[7].\"a\"", "$[7].a" },
  { false, " $ [ 7 ] . \"a\" ", "$[7].a" },

  { false, "$.*.\"b\"", "$.*.b" },
  { false, " $ . * . \"b\" ", "$.*.b" },

  { false, "$[*].\"a\"", "$[*].a" },
  { false, "  $ [ * ] . \"a\" ", "$[*].a" },

  { false, "$**.\"abc\"", "$**.abc" },
  { false, " $ ** . \"abc\" ", "$**.abc" },

  { false, "$**.\"a\"", "$**.a" },
  { false, " $ ** . \"a\" ", "$**.a" },

  // embedded spaces
  { false, "$.\" c d \"", "$.\" c d \"" },
  { false, "$.\" c d \".\"a b\"", "$.\" c d \".\"a b\"" },
  { false, "$.\"a b\".\" c d \"", "$.\"a b\".\" c d \"" },
};

INSTANTIATE_TEST_CASE_P(QuotedKeyNamesPositive, JsonGoodPathTestP,
                        ::testing::ValuesIn(good_quoted_key_names));

/** Test bad quoted key names */
static const Bad_path bad_quoted_key_names[]=
{
  // no closing quote
  { false, "$.a.\"bcd", 8 },
  { false, "$.a.\"", 5 },
  { false, "$.\"a\".\"bcd", 10 },

  // empty key name
  { false, "$.abc.\"\"", 8 },
  { false, "$.abc.\"\".def", 8 },
  { false, "$.\"abc\".\"\".def", 10 },

  // not followed by a member or array cell
  { false, "$.abc.\"def\"ghi", 11 },
  { false, "$.abc.\"def\"5", 11 },

  // unrecognized escape character
  { false, "$.abc.\"def\\aghi\"", 16 },

  // unrecognized unicode escape
  { false, "$.abcd.\"ef\\u01kfmno\"", 20 },

  // not preceded by a period
  { false, "$\"abcd\"", 1 },
  //{ false, "$.ghi\"abcd\"", 5 },
};

INSTANTIATE_TEST_CASE_P(QuotedKeyNamesNegative, JsonBadPathTestP,
                        ::testing::ValuesIn(bad_quoted_key_names));

/* Test that unquoted key names may not be ECMAScript identifiers */

static const Good_path good_ecmascript_identifiers[]=
{
  // keywords, however, are allowed
  { false, "$.if.break.return", "$.if.break.return" },

  // member name can start with $ and _
  { false, "$.$abc", "$.$abc" },
  { false, "$.$abc", "$.$abc" },

  // internal digits are ok
  { false, "$.a1_$bc", "$.a1_$bc" },

  // and so are internal <ZWNJ> and <ZWJ> characters
  { false, "$.a\\u200Cbc", "$.a\xE2\x80\x8C" "bc" },
  { false, "$.a\\u200Dbc", "$.a\xE2\x80\x8D" "bc" },

  // and so are internal unicode combining marks
  { false, "$.a\\u0300bc", "$.a\xCC\x80" "bc" },
  { false, "$.a\\u030Fbc", "$.a\xCC\x8F" "bc" },
  { false, "$.a\\u036Fbc", "$.a\xCD\xAF" "bc" },

  // and so are internal unicode connector punctuation codepoints
  { false, "$.a\\uFE33bc", "$.a\xEF\xB8\xB3" "bc" },
};

INSTANTIATE_TEST_CASE_P(GoodECMAScriptIdentifiers, JsonGoodPathTestP,
                        ::testing::ValuesIn(good_ecmascript_identifiers));

TEST_F(JsonPathTest, BadECMAScriptIdentifiers)
{
  // key names may not contain embedded quotes
  {
    SCOPED_TRACE("");
    bad_path(false, (char *) "$.a\"bc", 6);
  }

  // key names may not start with a digit or punctuation
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "1abc", 6);
  }
  {
    SCOPED_TRACE("");
    bad_identifier((char *) ";abc", 6);
  }

  // and not with the <ZWNJ> and <ZWJ> characters
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "\\u200Cabc", 11);
  }

  // and not with a unicode combining mark
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "\\u0300abc", 11);
  }
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "\\u030Fabc", 11);
  }
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "\\u036Fabc", 11);
  }

  // and not with unicode connector punctuation
  {
    SCOPED_TRACE("");
    bad_identifier((char *) "\\uFE33abc", 11);
  }
}

TEST_F(JsonPathTest, WrapperSeekTest)
{
  // vacuous path
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "false", (char *) "$", "false", false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, true, 1 ]",
                     (char *) "$",
                     "[false, true, 1]",
                     false);
  }

  // no match
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "false", (char *) "$.a", "", true);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, true, 1 ]",
                     (char *) "$[3]",
                     "",
                     true);
  }

  // first level retrieval
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, true, 1 ]",
                     (char *) "$[2]",
                     "1",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\" : 1, \"b\" : { \"c\" : [ 1, 2, 3 ] }, "
                     "\"d\" : 4 }",
                     (char *) "$.b",
                     "{\"c\": [1, 2, 3]}",
                     false);
  }

  // second level retrieval
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, true, [ 1, null, 200, 300 ], 400 ]",
                     (char *) "$[2][3]",
                     "300",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\" : 1, \"b\" : { \"c\" : [ 1, 2, 3 ] }, "
                     "\"d\" : 4 }",
                     (char *) "$.b.c",
                     "[1, 2, 3]",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, {\"abc\": 500}, "
                     "[ 1, null, 200, 300 ], 400 ]",
                     (char *) "$[1].abc",
                     "500",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\" : 1, \"b\" : [ 100, 200, 300 ], "
                     "\"d\" : 4 }",
                     (char *) "$.b[2]",
                     "300",
                     false);
  }

  // wildcards
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\" : 1, \"b\" : [ 100, 200, 300 ], "
                     "\"d\" : 4 }",
                     (char *) "$.*",
                     "[1, [100, 200, 300], 4]",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "[ false, {\"a\": true}, {\"b\": 200}, "
                     "{\"a\": 300} ]",
                     (char *) "$[*].a",
                     "[true, 300]",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"b\": {\"c\": 100}, \"d\": {\"a\": 200}, "
                     "\"e\": {\"a\": 300}}",
                     (char *) "$.*.a",
                     "[200, 300]",
                     false);
  }

  //
  // ellipsis
  //
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"b\": {\"c\": 100}, \"d\": {\"a\": 200}, "
                     "\"e\": {\"a\": 300}, \"f\": {\"g\": {\"a\": 500} } }",
                     (char *) "$**.a",
                     "[200, 300, 500]",
                     false);
  }

  // ellipsis with array recursing into object
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\": 100, "
                     "\"d\": [ {\"a\": 200}, "
                     "{ \"e\": {\"a\": 300, \"f\": 500} }, "
                     " { \"g\" : true, \"a\": 600 } ] }",
                     (char *) "$.d**.a",
                     "[200, 300, 600]",
                     false);
  }

  // ellipsis with object recursing into arrays
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\": true, "
                     " \"b\": { "
                     " \"a\": 100,"
                     " \"c\": [ "
                     "200, { \"a\": 300 }, "
                     "{ \"d\": { \"e\": { \"a\": 400 } }, \"f\": true }, "
                     "500, [ { \"a\": 600 } ]"
                     "]"
                     "}, "
                     " \"g\": { \"a\": 700 } }",
                     (char *) "$.b**.a",
                     "[100, 300, 400, 600]",
                     false);
  }

  // daisy-chained ellipses
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ \"a\": { \"x\" : { \"b\": { \"y\": { \"b\": "
                     "{ \"z\": { \"c\": 100 } } } } } } }",
                     (char *) "$.a**.b**.c",
                     "100",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *) "{ "
                     " \"c\": true"
                     ", \"a\": { "
                     " \"d\": [ "
                     " { "
                     " \"b\" : { "
                     " \"e\": ["
                     "{ \"c\": 100 "
                     ", \"f\": { \"a\": 200, \"b\": { \"g\" : {  \"h\": "
                     "{ \"c\": 300 } } } }"
                     " }"
                     " ]"
                     " }"
                     " }"
                     " ]"
                     " }"
                     ", \"b\": true"
                     " }",
                     (char *) "$.a**.b**.c",
                     "[100, 300]",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "["
                     "  100,"
                     "  ["
                     "    true,"
                     "    false,"
                     "    true,"
                     "    false,"
                     "    { \"a\": ["
                     "                  300,"
                     "                  400,"
                     "                  ["
                     "                     1, 2, 3, 4, 5,"
                     "                     {"
                     "                      \"b\": [ 500, 600, 700, 800, 900 ]"
                     "                     }"
                     "                  ]"
                     "               ]"
                     "    }"
                     "  ],"
                     "  200"
                     "]",
                     (char *) "$[1]**[2]**[3]",
                     "[4, 800]",
                     false);
  }

  // $[1][2][3].b[3] is a match for $[1]**[2]**[3]
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "["
                     "  100,"
                     "  ["
                     "                  300,"
                     "                  400,"
                     "                  ["
                     "                     1, 2, 3, 4, 5,"
                     "                     {"
                     "                      \"b\": [ 500, 600, 700, 800, 900 ]"
                     "                     }"
                     "                  ]"
                     "  ],"
                     "  200"
                     "]",
                     (char *) "$[1]**[2]**[3]",
                     "[4, 800]",
                     false);
  }

  /*
    $**[2]**.c matches

    $.a[ 2 ][ 1 ].c
    $.c.d[2][5].c
    $.d[2][4].d.c

    but not

    $.b[ 1 ][ 1 ].c
    $.e[2].c
  */
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{"
                     " \"a\": [ 0, 1, [ 0, { \"c\": 100 } ] ],"
                     " \"b\": [ 0, [ 0, { \"c\": 200 } ] ],"
                     " \"c\": { \"d\": [ 0, 1, [ 0, 1, 2, 3, 4, "
                     "{ \"c\": 300 } ] ] },"
                     " \"d\": [ 0, 1, [ 0, 1, 2, 3, { \"d\": "
                     "{ \"c\": 400 } } ] ],"
                     " \"e\": [ 0, 1, { \"c\": 500 } ]"
                     "}",
                     (char *) "$**[2]**.c",
                     "[100, 300, 400, 500]",
                     false);
  }

  // auto-wrapping
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"a\": 100 }",
                     (char *) "$.a[ 0 ]",
                     "100",
                     false);
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "[ [ 100, 200, 300 ], 400, { \"c\": 500 } ]",
                     (char *) "$[*][ 0 ]",
                     "[100, 400, {\"c\": 500}]",
                     false);
  }

  // auto-wrapping only works for the 0th index
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "[ [ 100, 200, 300 ], 400, { \"c\": 500 } ]",
                     (char *) "$[*][ 1 ]",
                     "200",
                     false);
  }

  // verify more ellipsis and autowrapping cases.

  // these two should have the same result.
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "[1]",
                     (char *) "$[0][0]",
                     "1",
                     false);
    SCOPED_TRACE("");
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "[1]",
                     (char *) "$**[0]",
                     "1",
                     false);
  }

  // these two should have the same result.
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"a\": 1 }",
                     (char *) "$.a[0]",
                     "1",
                     false);
    SCOPED_TRACE("");
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"a\": 1 }",
                     (char *) "$**[0]",
                     "[{\"a\": 1}, 1]",
                     false);
  }

  // these two should have the same result.
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"a\": 1 }",
                     (char *) "$[0].a",
                     "1",
                     false);
    SCOPED_TRACE("");
  }
  {
    SCOPED_TRACE("");
    vet_wrapper_seek((char *)
                     "{ \"a\": 1 }",
                     (char *) "$**.a",
                     "1",
                     false);
  }
}

TEST_F(JsonPathTest, RemoveDomTest)
{
  // successful removes
  {
    SCOPED_TRACE("");
    vet_remove((char *) "[100, 200, 300]",
               (char *) "$[1]",
               "[100, 300]",
               true);
  }
  {
    SCOPED_TRACE("");
    vet_remove((char *) "{\"a\": 100, \"b\": 200, \"c\": 300}",
               (char *) "$.b",
               "{\"a\": 100, \"c\": 300}",
               true);
  }

  /*
    test the adding of parent pointers
  */

  // Json_dom.add_alias()

  Json_object object1;
  Json_boolean true_literal1(true);
  Json_boolean false_literal1(false);
  Json_null *null_literal1= new (std::nothrow) Json_null();
  const Json_null *json_null= NULL;
  EXPECT_EQ(json_null, null_literal1->parent());
  object1.add_clone(std::string("a"), &true_literal1);
  object1.add_clone(std::string("b"), &false_literal1);
  object1.add_alias(std::string("c"), null_literal1);
  EXPECT_EQ(&object1, null_literal1->parent());
  EXPECT_EQ((char *) "{\"a\": true, \"b\": false, \"c\": null}",
            format(&object1));
  SCOPED_TRACE("");
  EXPECT_TRUE(object1.remove(null_literal1));
  EXPECT_EQ((char *) "{\"a\": true, \"b\": false}",
            format(&object1));
  EXPECT_FALSE(object1.remove(null_literal1));
  EXPECT_EQ((char *) "{\"a\": true, \"b\": false}",
            format(&object1));

  // Json_dom.add_clone()

  Json_null null_literal2;
  EXPECT_EQ(json_null, null_literal2.parent());
  std::string key("d");
  object1.add_clone(key, &null_literal2);
  Json_dom *clone= object1.get(key);
  EXPECT_EQ(&object1, clone->parent());

  // Json_array.append_clone()

  Json_array array;
  Json_boolean true_literal2(true);
  Json_boolean false_literal2(false);
  Json_null null_literal3;
  array.append_clone(&true_literal2);
  array.append_clone(&false_literal2);
  array.append_clone(&null_literal3);
  EXPECT_EQ((char *) "[true, false, null]", format(&array));
  Json_dom *cell= array[2];
  EXPECT_EQ(&array, cell->parent());

  // Json_array.append_alias()

  Json_boolean *true_literal3= new (std::nothrow) Json_boolean(true);
  array.append_alias(true_literal3);
  EXPECT_EQ((char *) "[true, false, null, true]", format(&array));
  EXPECT_EQ(&array, true_literal3->parent());
  EXPECT_TRUE(array.remove(true_literal3));
  EXPECT_EQ((char *) "[true, false, null]", format(&array));
  EXPECT_FALSE(array.remove(true_literal3));
  EXPECT_EQ((char *) "[true, false, null]", format(&array));

  // Json_array.insert_clone()

  Json_boolean true_literal4(true);
  array.insert_clone(2, &true_literal4);
  EXPECT_EQ((char *) "[true, false, true, null]", format(&array));
  cell= array[2];
  EXPECT_EQ(&array, cell->parent());

  // Json_array.insert_alias()

  Json_boolean *false_literal3= new (std::nothrow) Json_boolean(false);
  array.insert_alias(3, false_literal3);
  EXPECT_EQ((char *) "[true, false, true, false, null]", format(&array));
  EXPECT_EQ(&array, false_literal3->parent());
  EXPECT_TRUE(array.remove(false_literal3));
  EXPECT_EQ((char *) "[true, false, true, null]", format(&array));
  EXPECT_FALSE(array.remove(false_literal3));
  EXPECT_EQ((char *) "[true, false, true, null]", format(&array));

  // Json_array.insert_clone()
  Json_boolean true_literal5(true);
  array.insert_clone(5, &true_literal5);
  EXPECT_EQ((char *) "[true, false, true, null, true]", format(&array));
  EXPECT_EQ(&array, array[4]->parent());

  // Json_array.insert_alias()
  Json_boolean *false_literal4= new (std::nothrow) Json_boolean(false);
  array.insert_alias(7, false_literal4);
  EXPECT_EQ((char *) "[true, false, true, null, true, false]",
            format(&array));
  EXPECT_EQ(&array, false_literal4->parent());
  EXPECT_EQ(&array, array[5]->parent());
  EXPECT_TRUE(array.remove(false_literal4));
  EXPECT_EQ((char *) "[true, false, true, null, true]",
            format(&array));
  EXPECT_FALSE(array.remove(false_literal4));
  EXPECT_EQ((char *) "[true, false, true, null, true]",
            format(&array));
}


// Tuples for the test of Json_dom.get_location()
static const Location_tuple location_tuples[]=
{
  { false, "true", "$" },
  { false, "[true, false, null]", "$" },
  { false, "[true, false, null]", "$[1]" },
  { false, "{ \"a\": true}", "$" },
  { false, "{ \"a\": true}", "$.a" },
  { false, "{ \"a\": true, \"b\": [1, 2, 3] }", "$.b[2]" },
  { false, "[ 0, 1, { \"a\": true, \"b\": [1, 2, 3] } ]", "$[2].b[0]" },
};

/** Test good paths without column scope */
TEST_P(JsonGoodLocationTestP, GoodLocations)
{
  Location_tuple param= GetParam();
  vet_dom_location(param.m_begins_with_column_id,
                   param.m_json_text,
                   param.m_path_expression);
}

INSTANTIATE_TEST_CASE_P(LocationTesting, JsonGoodLocationTestP,
                        ::testing::ValuesIn(location_tuples));


// Tuples for the test of the only_needs_one arg of Json_wrapper.seek()
static const Ono_tuple ono_tuples[]=
{
  { false, "[ { \"a\": 1  }, { \"a\": 2 }  ]", "$[*].a", 2 },
  { false, "[ { \"a\": 1  }, { \"a\": 2 }  ]", "$**.a", 2 },
  {
    false,
    "{ \"a\": { \"x\" : { \"b\": { \"y\": { \"b\": "
    "{ \"z\": { \"c\": 100 }, \"c\": 200 } } } } } }",
    "$.a**.b**.c",
    2
  },
};

/** Test good paths without column scope */
TEST_P(JsonGoodOnoTestP, GoodOno)
{
  Ono_tuple param= GetParam();
  vet_only_needs_one(param.m_begins_with_column_id,
                     param.m_json_text,
                     param.m_path_expression,
                     param.m_expected_hits);
}

INSTANTIATE_TEST_CASE_P(OnoTesting, JsonGoodOnoTestP,
                        ::testing::ValuesIn(ono_tuples));

// Tuples for tests of cloning
static const Clone_tuple clone_tuples[]=
{
  { false, "$", "$[33]" },
  { false, "$[*].a", "$.a.b.c.d.e" },
  { false, "$.a.b.c[73]", "$**.abc.d.e.f.g" },
};

/** Test cloning without column scope */
TEST_P(JsonGoodCloneTestP, GoodClone)
{
  Clone_tuple param= GetParam();
  verify_clone(param.m_begins_with_column_id,
               param.m_path_expression_1,
               param.m_path_expression_2);
}

INSTANTIATE_TEST_CASE_P(CloneTesting, JsonGoodCloneTestP,
                        ::testing::ValuesIn(clone_tuples));


} // end namespace json_path_unittest
