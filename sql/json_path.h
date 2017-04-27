#ifndef SQL_JSON_PATH_INCLUDED
#define SQL_JSON_PATH_INCLUDED

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  @file json_path.h

  This file contains interface support for the JSON path abstraction.
  The path abstraction is described by the functional spec
  attached to WL#7909.
 */

#include <stddef.h>
#include <string>

#include "prealloced_array.h"                   // Prealloced_array

class String;

enum enum_json_path_leg_type
{
  jpl_member,
  jpl_array_cell,
  jpl_member_wildcard,
  jpl_array_cell_wildcard,
  jpl_ellipsis
};

/**
  One path leg in a JSON path expression.

  A path leg describes either a key/value pair in an object
  or a 0-based index into an array.
*/
class Json_path_leg final
{
private:
  enum_json_path_leg_type m_leg_type;

  size_t m_array_cell_index;

  std::string m_member_name;

public:
  explicit Json_path_leg(enum_json_path_leg_type leg_type)
    : m_leg_type(leg_type), m_array_cell_index(0), m_member_name()
  {}
  explicit Json_path_leg(size_t array_cell_index)
    : m_leg_type(jpl_array_cell), m_array_cell_index(array_cell_index),
      m_member_name()
  {}
  Json_path_leg(const std::string &member_name)
    : m_leg_type(jpl_member), m_array_cell_index(0),
      m_member_name(member_name)
  {}

  /** Get the type of the path leg. */
  enum_json_path_leg_type get_type() const { return m_leg_type; }

  /** Get the member name of a ::jpl_member path leg. */
  const std::string &get_member_name() const { return m_member_name; }

  /** Get the array cell index of a ::jpl_array_cell path leg. */
  size_t get_array_cell_index() const { return m_array_cell_index; }

  /** Turn into a human-readable string. */
  bool to_string(String *buf) const;
};


/**
  A path expression which can be used to seek to
  a position inside a JSON value.
*/
class Json_seekable_path
{
public:
  virtual ~Json_seekable_path() {}

  /** Return the number of legs in this searchable path */
  virtual size_t leg_count() const =0;

  /**
     Get the ith (numbered from 0) leg

     @param[in] index 0-based index into the searchable path

     @return NULL if the index is out of range. Otherwise, a pointer to the
     corresponding Json_path_leg.
  */
  virtual const Json_path_leg *get_leg_at(size_t index) const =0;

};

/*
  A JSON path expression.

  From the user's point of view,
  a path expression is a string literal with the following structure.
  We parse this structure into a Json_path class:

  <code><pre>

  pathExpression ::= scope  pathLeg (pathLeg)*

  scope ::= [ columnReference ] dollarSign

  columnReference ::=
        [
          [ databaseIdentifier period  ]
          tableIdentifier period
        ]
        columnIdentifier

  databaseIdentifier ::= sqlIdentifier
  tableIdentifier ::= sqlIdentifier
  columnIdentifier ::= sqlIdentifier

  pathLeg ::= member | arrayLocation | doubleAsterisk

  member ::= period (keyName | asterisk)

  arrayLocation ::=
    leftBracket
      (non-negative-integer | asterisk)
    rightBracket

  keyName ::= ECMAScript-identifier | ECMAScript-string-literal

  doubleAsterisk ::= **

  </pre></code>
*/
class Json_path final : public Json_seekable_path
{
private:
  typedef Prealloced_array<Json_path_leg, 8> Path_leg_vector;
  Path_leg_vector m_path_legs;

  /**
     Fills in this Json_path from a path expression.

     The caller must specify
     whether the path expression begins with a column identifier or whether
     it just begins with $. Stops parsing on the first error. Returns true if
     the path is parsed successfully. If parsing fails, then the state of this
     Json_path is undefined.

     @param[in] begins_with_column_id True if the path begins with a column id.

     @param[in] path_length The length of the path expression.

     @param[in] path_expression The string form of the path expression.

     @param[out] status True if the path parsed successfully. False otherwise.

     @return The pointer advanced to around where the error, if any, occurred.
  */
  const char *parse_path(const bool begins_with_column_id,
                         const size_t path_length,
                         const char *path_expression,
                         bool *status);

  /**
     Parse a single path leg and add it to the evolving Json_path.

     @param[in] charptr The current pointer into the path expression.

     @param[in] endptr  The end of the path expression.

     @param[out] status The status variable to be filled in.

     @return The pointer advanced past the consumed leg.
  */
  const char *parse_path_leg(const char *charptr, const char *endptr,
                             bool *status);

  /**
     Parse a single ellipsis leg and add it to the evolving Json_path.

     @param[in] charptr The current pointer into the path expression.

     @param[in] endptr  The end of the path expression.

     @param[out] status The status variable to be filled in.

     @return The pointer advanced past the consumed leg.
  */
  const char *parse_ellipsis_leg(const char *charptr, const char *endptr,
                                 bool *status);

  /**
     Parse a single array leg and add it to the evolving Json_path.

     @param[in] charptr The current pointer into the path expression.

     @param[in] endptr  The end of the path expression.

     @param[out] status The status variable to be filled in.

     @return The pointer advanced past the consumed leg.
  */
  const char *parse_array_leg(const char *charptr, const char *endptr,
                              bool *status);

  /**
     Parse a single member leg and add it to the evolving Json_path.

     @param[in] charptr The current pointer into the path expression.

     @param[in] endptr  The end of the path expression.

     @param[out] status The status variable to be filled in.

     @return The pointer advanced past the consumed leg.
  */
  const char *parse_member_leg(const char *charptr, const char *endptr,
                               bool *status);

public:
  Json_path();

  /** Return the number of legs in this path */
  size_t leg_count() const override { return m_path_legs.size(); }

  /**
     Get the ith (numbered from 0) leg

     @param[in] index 0-based index into the path

     @return NULL if the index is out of range. Otherwise, a pointer to the
     corresponding Json_path.
  */
  const Json_path_leg *get_leg_at(size_t index) const override;

  /**
    Add a path leg to the end of this path.
    @param[in] leg the leg to add
    @return false on success, true on error
  */
  bool append(const Json_path_leg &leg) { return m_path_legs.push_back(leg); }

  /**
    Pop the last leg element.

    This effectively lets the path point at the container of the original,
    i.e. an array or an object.

    @result the last leg popped off
  */
  Json_path_leg pop();

  /**
    Resets this to an empty path with no legs.
  */
  void clear() { m_path_legs.clear(); }

  /**
    Return true if the path contains a wildcard
    or ellipsis token
  */
  bool contains_wildcard_or_ellipsis() const;

  /** Turn into a human-readable string. */
  bool to_string(String *buf) const;

  friend bool parse_path(const bool begins_with_column_id,
                         const size_t path_length,
                         const char *path_expression,
                         Json_path *path,
                         size_t *bad_index);
};


/**
  A lightweight path expression. This exists so that paths can be cloned
  from the path legs of other paths without allocating heap memory
  to copy those legs into.
*/
class Json_path_clone final : public Json_seekable_path
{
private:
  using Path_leg_pointers= Prealloced_array<const Json_path_leg *, 8>;
  Path_leg_pointers m_path_legs;

public:
  Json_path_clone();

  /** Return the number of legs in this cloned path */
  size_t leg_count() const override { return m_path_legs.size(); }

  /**
     Get the ith (numbered from 0) leg

     @param[in] index 0-based index into the cloned path

     @return NULL if the index is out of range. Otherwise, a pointer to the
     corresponding Json_path_leg.
  */
  const Json_path_leg *get_leg_at(size_t index) const override;

  /**
    Add a path leg to the end of this cloned path.
    @param[in] leg the leg to add
    @return false on success, true on error
  */
  bool append(const Json_path_leg *leg) { return m_path_legs.push_back(leg); }

  /**
    Clear this clone and then add all of the
    legs from another path.

    @param[in,out] source The source path
    @return false on success, true on error
  */
  bool set(Json_seekable_path *source);

  /**
    Pop the last leg element.

    @result the last leg popped off
  */
  const Json_path_leg * pop();

  /**
    Resets this to an empty path with no legs.
  */
  void clear() { m_path_legs.clear(); }

};


/**
   Initialize a Json_path from a path expression.

   The caller must specify whether the path expression begins with a
   column identifier or whether it begins with $. Stops parsing on the
   first error. It initializes the Json_path and returns false if the
   path is parsed successfully. Otherwise, it returns false. In that
   case, the output bad_index argument will contain an index into the
   path expression. The parsing failed near that index.

   @param[in] begins_with_column_id True if the path begins with a column id.
   @param[in] path_length The length of the path expression.
   @param[in] path_expression The string form of the path expression.
   @param[out] path The Json_path object to be initialized.
   @param[out] bad_index If null is returned, the parsing failed around here.
   @return false on success, true on error
*/
bool parse_path(const bool begins_with_column_id, const size_t path_length,
                const char *path_expression, Json_path *path,
                size_t *bad_index);

#endif /* SQL_JSON_PATH_INCLUDED */
