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

#ifndef TABLE_SETUP_ACTORS_H
#define TABLE_SETUP_ACTORS_H

/**
  @file storage/perfschema/table_setup_actors.h
  Table SETUP_ACTORS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "mysql_com.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_setup_actor;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_ACTORS. */
struct row_setup_actors {
  /** Column HOST. */
  char m_hostname[HOSTNAME_LENGTH];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length;
  /** Column USER. */
  char m_username[USERNAME_LENGTH];
  /** Length in bytes of @c m_username. */
  uint m_username_length;
  /** Column ROLE. */
  char m_rolename[16];
  /** Length in bytes of @c m_rolename. */
  uint m_rolename_length;
  /** Column ENABLED. */
  bool *m_enabled_ptr;
  /** Column HISTORY. */
  bool *m_history_ptr;
};

class PFS_index_setup_actors : public PFS_engine_index {
 public:
  PFS_index_setup_actors()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("HOST"),
        m_key_2("USER"),
        m_key_3("ROLE") {}

  ~PFS_index_setup_actors() {}

  virtual bool match(PFS_setup_actor *pfs);

 private:
  PFS_key_host m_key_1;
  PFS_key_user m_key_2;
  PFS_key_role m_key_3;
};

/** Table PERFORMANCE_SCHEMA.SETUP_ACTORS. */
class table_setup_actors : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  /** Table builder. */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int write_row(PFS_engine_table *pfs_table, TABLE *table,
                       unsigned char *buf, Field **fields);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 protected:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  virtual int update_row_values(TABLE *table, const unsigned char *old_buf,
                                unsigned char *new_buf, Field **fields);

  virtual int delete_row_values(TABLE *table, const unsigned char *buf,
                                Field **fields);

  table_setup_actors();

 public:
  ~table_setup_actors() {}

 private:
  int make_row(PFS_setup_actor *actor);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_actors m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

 protected:
  PFS_index_setup_actors *m_opened_index;
};

/** @} */
#endif
