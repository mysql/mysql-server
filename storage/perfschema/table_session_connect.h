/* Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SESSION_CONNECT_H
#define TABLE_SESSION_CONNECT_H

/**
  @file storage/perfschema/table_session_connect.h
  TABLE SESSION_CONNECT (abstract)
*/

#include <sys/types.h>

#include "storage/perfschema/cursor_by_thread_connect_attr.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/table_helper.h"

#define MAX_ATTR_NAME_CHARS 32
#define MAX_ATTR_VALUE_CHARS 1024
#define MAX_UTF8MB4_BYTES 4

/** symbolic names for field offsets, keep in sync with field_types */
enum field_offsets {
  FO_PROCESS_ID,
  FO_ATTR_NAME,
  FO_ATTR_VALUE,
  FO_ORDINAL_POSITION
};

/**
  A row of PERFORMANCE_SCHEMA.SESSION_CONNECT_ATTRS and
  PERFORMANCE_SCHEMA.SESSION_ACCOUNT_CONNECT_ATTRS.
*/
struct row_session_connect_attrs {
  /** Column PROCESS_ID. */
  ulong m_process_id;
  /** Column ATTR_NAME. In UTF8MB4 */
  char m_attr_name[MAX_ATTR_NAME_CHARS * MAX_UTF8MB4_BYTES];
  /** Length in bytes of @c m_attr_name. */
  uint m_attr_name_length;
  /** Column ATTR_VALUE. In UTF8MB4 */
  char m_attr_value[MAX_ATTR_VALUE_CHARS * MAX_UTF8MB4_BYTES];
  /** Length in bytes of @c m_attr_name. */
  uint m_attr_value_length;
  /** Column ORDINAL_POSITION. */
  ulong m_ordinal_position;
};

class PFS_index_session_connect : public PFS_engine_index {
 public:
  PFS_index_session_connect()
      : PFS_engine_index(&m_key_1, &m_key_2),
        m_key_1("PROCESSLIST_ID"),
        m_key_2("ATTR_NAME") {}

  ~PFS_index_session_connect() {}

  virtual bool match(PFS_thread *pfs);
  virtual bool match(row_session_connect_attrs *row);

 private:
  PFS_key_processlist_id m_key_1;
  PFS_key_name m_key_2;
};

/** Abstract table PERFORMANCE_SCHEMA.SESSION_CONNECT_ATTRS. */
class table_session_connect : public cursor_by_thread_connect_attr {
 protected:
  table_session_connect(const PFS_engine_table_share *share);

 public:
  ~table_session_connect();

 protected:
  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

  virtual int make_row(PFS_thread *pfs, uint ordinal);
  virtual bool thread_fits(PFS_thread *thread);
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

 protected:
  /** Current row. */
  row_session_connect_attrs m_row;
  /** Safe copy of @c PFS_thread::m_session_connect_attrs. */
  char *m_copy_session_connect_attrs;
  /** Safe copy of @c PFS_thread::m_session_connect_attrs_length. */
  uint m_copy_session_connect_attrs_length;

  PFS_index_session_connect *m_opened_index;
};

bool read_nth_attr(const char *connect_attrs, uint connect_attrs_length,
                   const CHARSET_INFO *connect_attrs_cs, uint ordinal,
                   char *attr_name, uint max_attr_name, uint *attr_name_length,
                   char *attr_value, uint max_attr_value,
                   uint *attr_value_length);

/** @} */
#endif
