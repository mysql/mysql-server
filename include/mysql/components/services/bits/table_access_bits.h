/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMPONENTS_SERVICES_TABLE_ACCESS_BITS_H
#define COMPONENTS_SERVICES_TABLE_ACCESS_BITS_H

#ifndef MYSQL_ABI_CHECK
#include <stddef.h> /* size_t */
#endif

#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_string.h>

/**
  @file mysql/components/services/bits/table_access_bits.h
  Generic table access interface.

  @defgroup group_table_access_services Table Access services
  @ingroup group_components_services_inventory
  @{
*/

/**
  Table lock type.
*/
enum TA_lock_type {
  /** Table is opened for read. */
  TA_READ,
  /** Table is opened for write. */
  TA_WRITE
};

/**
  Types of columns supported by the table access service.
*/
enum TA_field_type {
  TA_TYPE_UNKNOWN = 0,
  TA_TYPE_INTEGER = 1,
  TA_TYPE_VARCHAR = 2,
  TA_TYPE_JSON = 3,
  TA_TYPE_ENUM = 4,
};

/**
  Expected field definition.
*/
struct TA_table_field_def {
  /** Column ordinal position (0-based). */
  size_t m_index;
  /** Column name, in UTF8MB4. */
  const char *m_name;
  /** Column name length, in bytes. */
  size_t m_name_length;
  /** Column type. */
  TA_field_type m_type;
  /** Nullable. */
  bool m_nullable;
  /** Column length. */
  size_t m_length;
};

/**
  Expected index definition.
*/
struct TA_index_field_def {
  /** Column name, in UTF8MB4. */
  const char *m_name;
  /** Column name length, in bytes. */
  size_t m_name_length;
  /** Index order. */
  bool m_ascending;
};

/**
  Table_access.
  A table access session.
*/
DEFINE_SERVICE_HANDLE(Table_access);

/**
  An opened table.
*/
DEFINE_SERVICE_HANDLE(TA_table);

/**
  An index key.
*/
DEFINE_SERVICE_HANDLE(TA_key);

/**
  Create a table access object.
  @sa destroy_table_access_v1_t

  @param thd The current session. Pass a null ptr if running from a non-MySQLd
  thread
  @param count The maximum number of tables to access.

  @note The table access service will create a child session
  to perform table read and writes.
  The following properties from the current session are used
  to initialize the child session:
  - @c log_bin
  - @c transaction_isolation
  - @c skip_readonly_check

  @warning The table access service will set the current THD thread attribute
  to the temporary child THD it creates. The current THD will be reset back by
  @ref destroy_table_access_v1_t. This means that the whole table access
  routine should happen as fast as possible and without thread switching until
  its completion and disposal of the thread access attribute.
  The @ref Table_access handle should not be cached and reused.
*/
typedef Table_access (*create_table_access_v1_t)(MYSQL_THD thd, size_t count);

/**
  Destroy a table access object.
  @sa create_table_access_v1_t
*/
typedef void (*destroy_table_access_v1_t)(Table_access ta);

/**
  Add a table to a table access session.
  This method adds a table to a table access session.
  It returns a ticket, to use with @ref get_table_v1_t.

  @param ta Table access session
  @param schema_name Schema name raw buffer, in UTF8MB4.
  @param schema_name_length Length, in bytes, of schema_name
  @param table_name Table name raw buffer, in UTF8MB4.
  @param table_name_length Length, in bytes, of table_name
  @param lock_type_arg Lock type to use (read or write)
  @return ticket number for the table added
*/
typedef size_t (*add_table_v1_t)(Table_access ta, const char *schema_name,
                                 size_t schema_name_length,
                                 const char *table_name,
                                 size_t table_name_length,
                                 TA_lock_type lock_type_arg);

#define TA_ERROR_GRL 1
#define TA_ERROR_READONLY 2
#define TA_ERROR_OPEN 3

/**
  Start a table access transaction.
  Open all tables from the table access session, and lock them.

  Locking all the tables at once is necessary to prevent
  deadlocks on table metadata between different sessions.
  The caller should not own the global read lock when opening
  tables, as this would also create deadlocks.

  @return An error status
  @retval 0 success
  @retval TA_ERROR_GRL Global read lock held by the caller
  @retval TA_ERROR_OPEN Failed to open and lock tables.
*/
typedef int (*begin_v1_t)(Table_access ta);

/**
  Commit changes.
*/
typedef int (*commit_v1_t)(Table_access ta);

/**
  Rollback changes.
*/
typedef int (*rollback_v1_t)(Table_access ta);

/**
  Get an opened table.
  Once tables are opened and locked,
  this method returns the opened table that correspond to
  the ticket returned by add tables.

  @return An opened table.
*/
typedef TA_table (*get_table_v1_t)(Table_access ta, size_t ticket);

/**
  Check the actual table fields against expected fields.

  @retval 0 success
*/
typedef int (*check_table_fields_v1_t)(Table_access ta, TA_table table,
                                       const TA_table_field_def *fields,
                                       size_t fields_count);

/**
  Open a table index.

  An index is identified by name.
  The caller lists the columns expected in the index.
  If the index DDL matches the expected columns and count,
  the index is opened.
  Otherwise, an error is returned, and the index is not opened.

  @note The list of columns must contain the ACTUAL columns in the index
  rather than USER defined (used on SQL level). The actual list depends on
  engine and current implementation. E.g. InnoDB may add columns to the user
  defined set.

  @param ta Table access
  @param table Opened table
  @param index_name Index name, in UTF8MB4, must match the table DDL.
  @param index_name_length Length of index_name, in bytes.
  @param fields Expected fields for the index
  @param fields_count Expected number of fields for the index
  @param [out] key Opened index.

  @retval 0 Success

  @sa index_end_v1_t
*/
typedef int (*index_init_v1_t)(Table_access ta, TA_table table,
                               const char *index_name, size_t index_name_length,
                               const TA_index_field_def *fields,
                               size_t fields_count, TA_key *key);

/**
  Position a table index at a search key.

  The search key is not passed explicitly in this call.
  The search key is set by populating columns in the table current record,
  before invoking this method.

  @param ta Table access
  @param table Opened table
  @param num_parts Number of key parts populated in the current record.
  @param key Opened index
*/
typedef int (*index_read_map_v1_t)(Table_access ta, TA_table table,
                                   size_t num_parts, TA_key key);

/**
  Position on index at the beginning.
*/
typedef int (*index_first_v1_t)(Table_access ta, TA_table table, TA_key key);

/**
  Advance to the next record in the index.
*/
typedef int (*index_next_v1_t)(Table_access ta, TA_table table, TA_key key);

/**
  Advance to the next record that matches the current search key.
*/
typedef int (*index_next_same_v1_t)(Table_access ta, TA_table table,
                                    TA_key key);

/**
  Close an index.
*/
typedef int (*index_end_v1_t)(Table_access ta, TA_table table, TA_key key);

/**
  Start a full table scan.
*/
typedef int (*rnd_init_v1_t)(Table_access ta, TA_table table);

/**
  Advance to the next record in a table scan.
*/
typedef int (*rnd_next_v1_t)(Table_access ta, TA_table table);

/**
  End a full table scan.
*/
typedef int (*rnd_end_v1_t)(Table_access ta, TA_table table);

/**
  Insert a new row in the table.
*/
typedef int (*write_row_v1_t)(Table_access ta, TA_table table);

/**
  Update the current row.
*/
typedef int (*update_row_v1_t)(Table_access ta, TA_table table);

/**
  Delete the current row.
*/
typedef int (*delete_row_v1_t)(Table_access ta, TA_table table);

/**
  Set a column to NULL.
*/
typedef void (*set_field_null_v1_t)(Table_access ta, TA_table table,
                                    size_t index);

/**
  Is a column NULL.
*/
typedef bool (*is_field_null_v1_t)(Table_access ta, TA_table table,
                                   size_t index);

/**
  Can a column be NULL.
*/
typedef bool (*maybe_field_null_v1_t)(Table_access ta, TA_table table,
                                      size_t index);

/**
  Write an INTEGER column value.
*/
typedef int (*set_field_integer_value_v1_t)(Table_access ta, TA_table table,
                                            size_t index, long long v);

/**
  Read an INTEGER column value.
*/
typedef int (*get_field_integer_value_v1_t)(Table_access ta, TA_table table,
                                            size_t index, long long *v);

/**
  Write a VARCHAR column value.
*/
typedef int (*set_field_varchar_value_v1_t)(Table_access ta, TA_table table,
                                            size_t index, my_h_string v);

/**
  Read a VARCHAR column value.
*/
typedef int (*get_field_varchar_value_v1_t)(Table_access ta, TA_table table,
                                            size_t index, my_h_string v);

/**
  Write any column value.
  This is a generic interface.
  The column value must be expressed in UTF8MB4_bin,
  and is converted to the column type.
*/
typedef int (*set_field_any_value_v1_t)(Table_access ta, TA_table table,
                                        size_t index, my_h_string v);

/**
  Read any column value.
  This is a generic interface.
  The column value is converted to a string,
  expressed in UTF8MB4_bin.
*/
typedef int (*get_field_any_value_v1_t)(Table_access ta, TA_table table,
                                        size_t index, my_h_string v);

/** @} (end of group_table_access_services) */

#endif /* COMPONENTS_SERVICES_TABLE_ACCESS_BITS_H */
