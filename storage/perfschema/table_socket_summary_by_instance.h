/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SOCKET_SUMMARY_BY_INSTANCE_H
#define TABLE_SOCKET_SUMMARY_BY_INSTANCE_H

/**
  @file storage/perfschema/table_socket_summary_by_instance.h
  Table SOCKET_SUMMARY_BY_INSTANCE (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_socket;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.SOCKET_SUMMARY_BY_INSTANCE.
*/
struct row_socket_summary_by_instance {
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;

  /** Column OBJECT_INSTANCE_BEGIN */
  const void *m_identity;

  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER and NUMBER_OF_BYTES for each
   * operation. */
  PFS_socket_io_stat_row m_io_stat;
};

class PFS_index_socket_summary_by_instance : public PFS_engine_index {
 public:
  PFS_index_socket_summary_by_instance(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  ~PFS_index_socket_summary_by_instance() {}

  virtual bool match(const PFS_socket *pfs) = 0;
};

class PFS_index_socket_summary_by_instance_by_instance
    : public PFS_index_socket_summary_by_instance {
 public:
  PFS_index_socket_summary_by_instance_by_instance()
      : PFS_index_socket_summary_by_instance(&m_key),
        m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_socket_summary_by_instance_by_instance() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_socket_summary_by_instance_by_event_name
    : public PFS_index_socket_summary_by_instance {
 public:
  PFS_index_socket_summary_by_instance_by_event_name()
      : PFS_index_socket_summary_by_instance(&m_key), m_key("EVENT_NAME") {}

  ~PFS_index_socket_summary_by_instance_by_event_name() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_event_name m_key;
};

/** Table PERFORMANCE_SCHEMA.SOCKET_SUMMARY_BY_INSTANCE. */
class table_socket_summary_by_instance : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 private:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  table_socket_summary_by_instance();

 public:
  ~table_socket_summary_by_instance() {}

 private:
  int make_row(PFS_socket *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_socket_summary_by_instance m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

 protected:
  PFS_index_socket_summary_by_instance *m_opened_index;
};

/** @} */
#endif
