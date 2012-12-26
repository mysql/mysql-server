/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_SOCKET_INSTANCES_H
#define TABLE_SOCKET_INSTANCES_H

/**
  @file storage/perfschema/table_socket_instances.h
  Table SOCKET_INSTANCES (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SOCKET_INSTANCES. */
struct row_socket_instances
{
  /** Column EVENT_NAME. */
  const char *m_event_name;
  /** Length in bytes of @c m_event_name. */
  uint m_event_name_length;
  /** Column OBJECT_INSTANCE_BEGIN */
  const void *m_identity;
  /** Column THREAD_ID */
  ulonglong m_thread_id;
  /** True if thread_is is set */
  bool m_thread_id_set;
  /** Column SOCKET_ID */
  uint m_fd;
  /** Socket ip address, IPV4 or IPV6 */
  char m_ip[INET6_ADDRSTRLEN+1];
  /** Length in bytes of @c m_ip. */
  uint m_ip_length;
  /** Column PORT */
  uint m_port;
  /** Socket state: ACTIVE or IDLE */
  PSI_socket_state m_state;

  row_socket_instances() {m_thread_id_set= false;}
};

/** Table PERFORMANCE_SCHEMA.SOCKET_INSTANCES. */
class table_socket_instances : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_socket_instances();

public:
  ~table_socket_instances()
  {}

private:
  void make_row(PFS_socket *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_socket_instances m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
