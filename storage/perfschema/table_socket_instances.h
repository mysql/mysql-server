/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SOCKET_INSTANCES_H
#define TABLE_SOCKET_INSTANCES_H

/**
  @file storage/perfschema/table_socket_instances.h
  Table SOCKET_INSTANCES (declarations).
*/

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "mysql/components/services/psi_socket_bits.h"
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

/** A row of PERFORMANCE_SCHEMA.SOCKET_INSTANCES. */
struct row_socket_instances {
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
  char m_ip[INET6_ADDRSTRLEN + 1];
  /** Length in bytes of @c m_ip. */
  uint m_ip_length;
  /** Column PORT */
  uint m_port;
  /** Socket state: ACTIVE or IDLE */
  PSI_socket_state m_state;

  row_socket_instances() { m_thread_id_set = false; }
};

class PFS_index_socket_instances : public PFS_engine_index {
 public:
  PFS_index_socket_instances(PFS_engine_key *key_1) : PFS_engine_index(key_1) {}

  PFS_index_socket_instances(PFS_engine_key *key_1, PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  ~PFS_index_socket_instances() {}

  virtual bool match(const PFS_socket *pfs) = 0;
};

class PFS_index_socket_instances_by_instance
    : public PFS_index_socket_instances {
 public:
  PFS_index_socket_instances_by_instance()
      : PFS_index_socket_instances(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_socket_instances_by_instance() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_socket_instances_by_thread : public PFS_index_socket_instances {
 public:
  PFS_index_socket_instances_by_thread()
      : PFS_index_socket_instances(&m_key), m_key("THREAD_ID") {}

  ~PFS_index_socket_instances_by_thread() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_thread_id m_key;
};

class PFS_index_socket_instances_by_socket : public PFS_index_socket_instances {
 public:
  PFS_index_socket_instances_by_socket()
      : PFS_index_socket_instances(&m_key), m_key("SOCKET_ID") {}

  ~PFS_index_socket_instances_by_socket() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_socket_id m_key;
};

class PFS_index_socket_instances_by_ip_port
    : public PFS_index_socket_instances {
 public:
  PFS_index_socket_instances_by_ip_port()
      : PFS_index_socket_instances(&m_key_1, &m_key_2),
        m_key_1("IP"),
        m_key_2("PORT") {}

  ~PFS_index_socket_instances_by_ip_port() {}

  bool match(const PFS_socket *pfs);

 private:
  PFS_key_ip m_key_1;
  PFS_key_port m_key_2;
};

/** Table PERFORMANCE_SCHEMA.SOCKET_INSTANCES. */
class table_socket_instances : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 private:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);
  table_socket_instances();

 public:
  ~table_socket_instances() {}

 protected:
  int make_row(PFS_socket *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_socket_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_socket_instances *m_opened_index;
};

/** @} */
#endif
