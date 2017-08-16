#ifndef JSON_DIFF_INCLUDED
#define JSON_DIFF_INCLUDED

/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  Header file for the Json_diff class.

  The Json_diff class is used to represent a logical change in a JSON column,
  so that a replication master can send only what has changed, instead of
  sending the whole new value to the replication slave when a JSON column is
  updated.
*/

#include <algorithm>
#include <memory>                               // std::unique_ptr
#include <vector>

#include "sql/json_dom.h"
#include "sql/json_path.h"
#include "sql/memroot_allocator.h"

class Field_json;
class Json_dom;
class Json_wrapper;

/// Enum that describes what kind of operation a Json_diff object represents.
enum class enum_json_diff_operation
{
  /**
    The JSON value in the given path is replaced with a new value.
    It has the same effect as `JSON_REPLACE(col, path, value)`.
  */
  REPLACE,

  /**
    Add a new element at the given path.

    If the path specifies an array element, it has the same effect as
    `JSON_ARRAY_INSERT(col, path, value)`.

    If the path specifies an object member, it has the same effect as
    `JSON_INSERT(col, path, value)`.
  */
  INSERT,

  /**
    The JSON value at the given path is removed from an array or object.
    It has the same effect as `JSON_REMOVE(col, path)`.
  */
  REMOVE,
};

/**
  A class that represents a logical change to a JSON document. It is used by
  row-based replication to send information about changes in JSON documents
  without sending the whole updated document.
*/
class Json_diff final
{
  /// The path that is changed.
  Json_path m_path;
  /// The operation to perform on the changed path.
  enum_json_diff_operation m_operation;
  /// The new value to add to the changed path.
  std::unique_ptr<Json_dom> m_value;
public:
  /**
    Construct a Json_diff object.

    @param path       the path that is changed
    @param operation  the operation to perform on the path
    @param value      the new value in the path (the Json_diff object
                      takes over the ownership of the value)
  */
  Json_diff(const Json_seekable_path &path,
            enum_json_diff_operation operation,
            std::unique_ptr<Json_dom> value)
    : m_path(), m_operation(operation), m_value(std::move(value))
  {
    for (const Json_path_leg *leg : path)
      m_path.append(*leg);
  }

  /// Get the path that is changed by this diff.
  const Json_path &path() const { return m_path; }

  /// Get the operation that is performed on the path.
  enum_json_diff_operation operation() const { return m_operation; }

  /**
    Get a Json_wrapper representing the new value to add to the path. The
    wrapper is an alias, so the ownership of the contained Json_dom is retained
    by the Json_diff object.
    @see Json_wrapper::set_alias()
  */
  Json_wrapper value() const;
};

/// Vector of logical diffs describing changes to a JSON column.
using Json_diff_vector= std::vector<Json_diff, Memroot_allocator<Json_diff>>;

/**
  The result of applying JSON diffs on a JSON value using apply_json_diffs().
*/
enum class enum_json_diff_status
{
  /**
     The JSON diffs were applied and the JSON value in the column was updated
     successfully.
  */
  SUCCESS,

  /**
    An error was raised while applying one of the diffs. The value in the
    column was not updated.
  */
  ERROR,

  /**
    One of the diffs was rejected. This could happen if the path specified in
    the diff does not exist in the JSON value, or if the diff is supposed to
    add a new value at a given path, but there already is a value at the path.

    This return code would usually indicate that the replication slave where
    the diff is applied, is out of sync with the replication master where the
    diff was created.

    The value in the column was not updated, but no error was raised.
  */
  REJECTED,
};

/**
  Apply a sequence of JSON diffs to the value stored in a JSON column.

  @param field  the column to update
  @param diffs  the diffs to apply
  @return an enum_json_diff_status value that tells if the diffs were
          applied successfully
*/
enum_json_diff_status apply_json_diffs(Field_json *field,
                                       const Json_diff_vector *diffs);


#endif /* JSON_DIFF_INCLUDED */
