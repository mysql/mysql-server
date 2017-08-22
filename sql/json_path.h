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

#include "my_dbug.h"                            // DBUG_ASSERT
#include "prealloced_array.h"                   // Prealloced_array

class String;

/** The type of a Json_path_leg. */
enum enum_json_path_leg_type
{
  /**
    A path leg that represents a JSON object member (such as `.name`).
    This path leg matches a single member in a JSON object.
  */
  jpl_member,

  /**
    A path leg that represents a JSON array cell (such as `[10]`).
    This path leg matches a single element in a JSON object.
  */
  jpl_array_cell,

  /**
    A path leg that represents a range in a JSON array
    (such as `[2 to 7]`).
  */
  jpl_array_range,

  /**
    A path leg that represents the member wildcard (`.*`), which
    matches all the members of a JSON object.
  */
  jpl_member_wildcard,

  /**
    A path leg that represents the array wildcard (`[*]`), which
    matches all the elements of a JSON array.
  */
  jpl_array_cell_wildcard,

  /**
    A path leg that represents the ellipsis (`**`), which matches any
    JSON value and recursively all the JSON values nested within it if
    it is an object or an array.
  */
  jpl_ellipsis
};


/**
  A class that represents the index of an element in a JSON array. The
  index is 0-based and relative to the beginning of the array.
*/
class Json_array_index final
{
  /**
    The array index. It is 0 if the specified index was before the
    first element of the array, or equal to the array length if the
    specified index was after the last element of the array.
  */
  size_t m_index;

  /** True if the array index is within the bounds of the array. */
  bool m_within_bounds;

public:
  /**
    Construct a new Json_array_index object representing the specified
    position in an array of the given length.

    @param index         the array index
    @param from_end      true if @a index is relative to the end of the array
    @param array_length  the length of the array
  */
  Json_array_index(size_t index, bool from_end, size_t array_length)
    : m_index(from_end ?
              (index < array_length ? array_length - index - 1: 0) :
              std::min(index, array_length)),
      m_within_bounds(index < array_length)
  {}

  /**
    Is the array index within the bounds of the array?

    @retval true if the array index is within bounds
    @retval false otherwise
  */
  bool within_bounds() const { return m_within_bounds; }

  /**
    Get the position in the array pointed to by this array index.

    If the index is out of bounds, 0 will be returned if the array
    index is before the first element in the array, or a value equal
    to the length of the array if the index is after the last element.

    @return the position in the array (0-based index relative to the
    start of the array)
  */
  size_t position() const { return m_index; }
};


/**
  One path leg in a JSON path expression.

  A path leg describes either a key/value pair in an object
  or a 0-based index into an array.
*/
class Json_path_leg final
{
  /// The type of this path leg.
  enum_json_path_leg_type m_leg_type;

  /// The index of an array cell, or the start of an array range.
  size_t m_first_array_index;

  /// Is #m_first_array_index relative to the end of the array?
  bool m_first_array_index_from_end= false;

  /// The end (inclusive) of an array range.
  size_t m_last_array_index;

  /// Is #m_last_array_index relative to the end of the array?
  bool m_last_array_index_from_end= false;

  /// The member name of a member path leg.
  std::string m_member_name;

public:
  /**
    Construct a wildcard or ellipsis path leg.

    @param leg_type the type of wildcard (#jpl_ellipsis,
    #jpl_member_wildcard or #jpl_array_cell_wildcard)
  */
  explicit Json_path_leg(enum_json_path_leg_type leg_type)
    : m_leg_type(leg_type)
  {
    DBUG_ASSERT(leg_type == jpl_ellipsis ||
                leg_type == jpl_member_wildcard ||
                leg_type == jpl_array_cell_wildcard);
  }

  /**
    Construct an array cell path leg.

    @param index the 0-based index in the array,
      relative to the beginning of the array
  */
  explicit Json_path_leg(size_t index)
    : Json_path_leg(index, false)
  {}

  /**
    Construct an array cell path leg.

    @param index the 0-based index in the array
    @param from_end true if @a index is relative to the end of the array
  */
  Json_path_leg(size_t index, bool from_end)
    : m_leg_type(jpl_array_cell),
      m_first_array_index(index),
      m_first_array_index_from_end(from_end)
  {}

  /**
    Construct an array range path leg.

    @param idx1  the start index of the range, inclusive
    @param idx1_from_end  true if the start index is relative
                          to the end of the array
    @param idx2  the last index of the range, inclusive
    @param idx2_from_end  true if the last index is relative
                          to the end of the array
  */
  Json_path_leg(size_t idx1, bool idx1_from_end,
                size_t idx2, bool idx2_from_end)
    : m_leg_type(jpl_array_range),
      m_first_array_index(idx1), m_first_array_index_from_end(idx1_from_end),
      m_last_array_index(idx2), m_last_array_index_from_end(idx2_from_end)
  {}

  /**
    Construct an object member path leg.

    @param member_name  the name of the object member
  */
  explicit Json_path_leg(const std::string &member_name)
    : m_leg_type(jpl_member), m_member_name(member_name)
  {}

  /** Get the type of the path leg. */
  enum_json_path_leg_type get_type() const { return m_leg_type; }

  /** Get the member name of a ::jpl_member path leg. */
  const std::string &get_member_name() const { return m_member_name; }

  /** Turn into a human-readable string. */
  bool to_string(String *buf) const;

  /**
    Is this path leg an auto-wrapping array accessor?

    An auto-wrapping array accessor is an array accessor that matches
    non-arrays by auto-wrapping them in a single-element array before doing
    the matching.

    This function returns true for any ::jpl_array_cell or ::jpl_array_range
    path leg that would match the element contained in a single-element
    array, and which therefore would also match non-arrays that have been
    auto-wrapped in single-element arrays.
  */
  bool is_autowrap() const;

  /**
    Get the first array cell pointed to by an array range, or the
    array cell pointed to by an array cell index.

    @param array_length the length of the array
  */
  Json_array_index first_array_index(size_t array_length) const
  {
    DBUG_ASSERT(m_leg_type == jpl_array_cell || m_leg_type == jpl_array_range);
    return Json_array_index(m_first_array_index,
                            m_first_array_index_from_end,
                            array_length);
  }

  /**
    Get the last array cell pointed to by an array range. The range
    includes this cell.

    @param array_length the length of the array
  */
  Json_array_index last_array_index(size_t array_length) const
  {
    DBUG_ASSERT(m_leg_type == jpl_array_range);
    return Json_array_index(m_last_array_index,
                            m_last_array_index_from_end,
                            array_length);
  }

  /**
    A structure that represents an array range.
  */
  struct Array_range
  {
    size_t m_begin;           ///< Beginning of the range, inclusive.
    size_t m_end;             ///< End of the range, exclusive.
  };

  /**
    Get the array range pointed to by a path leg of type
    ::jpl_array_range or ::jpl_array_cell_wildcard.
    @param array_length  the length of the array
  */
  Array_range get_array_range(size_t array_length) const;
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

/**
  A JSON path expression.

  From the user's point of view,
  a path expression is a string literal with the following structure.
  We parse this structure into a Json_path object:

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
          (arrayIndex | arrayRange | asterisk)
        rightBracket

      arrayIndex ::=
        non-negative-integer |
        last [ minus non-negative-integer ]

      arrayRange ::= arrayIndex to arrayIndex

      keyName ::= ECMAScript-identifier | ECMAScript-string-literal

      doubleAsterisk ::= **

      to ::= "to"

      last ::= "last"
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
    Return true if the path can match more than one value in a JSON document.

    @retval true   if the path contains a path leg which is a wildcard,
                   ellipsis or array range
    @retval false  otherwise
  */
  bool can_match_many() const;

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
