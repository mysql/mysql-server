#ifdef __cplusplus
#ifndef SERVICE_RULES_TABLE_INCLUDED
#define SERVICE_RULES_TABLE_INCLUDED

/*  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <my_global.h>
#include <my_dbug.h>
#include <string>

#ifndef MYSQL_ABI_CHECK
#include <stdlib.h>
#endif


/**
  @file service_rules_table.h

  Plugin service that provides access to the rewrite rules table that is used
  by the Rewriter plugin. No other use intended.
*/

class THD;
struct TABLE_LIST;
class Field;

namespace rules_table_service
{


/**
  There must be one function of this kind in order for the symbols in the
  server's dynamic library to be visible to plugins.
*/
int dummy_function_to_ensure_we_are_linked_into_the_server();


/**
  Frees a const char pointer allocated in the server's dynamic library using
  new[].
*/
void free_string(const char *str);


/**
  Writable cursor that allows reading and updating of rows in a persistent
  table.
*/
class Cursor
{
public:
  typedef int column_id;

  static const column_id ILLEGAL_COLUMN_ID= -1;

  /**
    Creates a cursor to an already-opened table. The constructor is kept
    explicit because of implicit conversions from void*.
  */
  explicit Cursor(THD *thd);

  /// Creates a past-the-end cursor.
  Cursor() :
    m_thd(NULL), m_table_list(NULL), m_is_finished(true)
  {}

  column_id pattern_column() const { return m_pattern_column; }
  column_id pattern_database_column() const
  {
    return m_pattern_database_column;
  }
  column_id replacement_column() const { return m_replacement_column; }
  column_id enabled_column() const { return m_enabled_column; }
  column_id message_column() const { return m_message_column; }
  column_id pattern_digest_column() const { return m_pattern_digest_column; }
  column_id normalized_pattern_column() const {
    return m_normalized_pattern_column;
  }

  /**
    True if the table does not contain columns named 'pattern', 'replacement',
    'enabled' and 'message'. In this case the cursor is equal to any
    past-the-end Cursor.
  */
  bool table_is_malformed() { return m_table_is_malformed; }

  /**
    Fetches the value of the column with the given number as a C string.

    This interface is meant for crossing dynamic library boundaries, hence the
    use of C-style const char*. The function casts a column value to a C
    string and returns a copy, allocated in the callee's DL. The pointer
    must be freed using free_string().

    @param fieldno One of PATTERN_COLUMN, REPLACEMENT_COLUMN, ENABLED_COLUMN
    or MESSAGE_COLUMN.
  */
  const char *fetch_string(int fieldno);

  /**
    Equality operator. The only cursors that are equal are past-the-end
    cursors.
  */
  bool operator== (const Cursor &other)
  {
    return (m_is_finished == other.m_is_finished);
  }

  /**
    Inequality operator. All cursors are considered different except
    past-the-end cursors.
  */
  bool operator!= (const Cursor &other) { return !(*this == other); }

  /**
    Advances this Cursor. Read errors are kept, and had_serious_read_error()
    will tell if there was an unexpected error (e.g. not EOF) while reading.
  */
  Cursor &operator++ ()
  {
    if (!m_is_finished)
      read();
    return *this;
  }

  /// Prepares the write buffer for updating the current row.
  void make_writeable();

  /**
    Sets the value of column colno to a string value.

    @param colno The column number.
    @param str The string.
    @param length The string's length.
  */
  void set(int colno, const char* str, size_t length);

  /// Writes the row in the write buffer to the table at the current row.
  int write();

  /// True if there was an unexpected error while reading, e.g. other than EOF.
  bool had_serious_read_error() const;

  /// Closes the table scan if initiated and commits the transaction.
  ~Cursor();

private:
  int field_index(const char *field_name);

  int m_pattern_column;
  int m_pattern_database_column;
  int m_replacement_column;
  int m_enabled_column;
  int m_message_column;
  int m_pattern_digest_column;
  int m_normalized_pattern_column;

  THD *m_thd;
  TABLE_LIST *m_table_list;

  bool m_is_finished;
  bool m_table_is_malformed;
  int m_last_read_status;

  int read();
};


/**
  A past-the-end Cursor. All past-the-end cursors are considered equal
  when compared with operator ==.
*/
Cursor end();

}

#endif // SERVICE_RULES_TABLE_INCLUDED
#endif // __cplusplus
