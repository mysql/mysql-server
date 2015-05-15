/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SETUP_ACTORS_H
#define TABLE_SETUP_ACTORS_H

/**
  @file storage/perfschema/table_setup_actors.h
  Table SETUP_ACTORS (declarations).
*/

#include "pfs_engine_table.h"

struct PFS_setup_actor;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_ACTORS. */
struct row_setup_actors
{
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

/** Table PERFORMANCE_SCHEMA.SETUP_ACTORS. */
class table_setup_actors : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  /** Table builder. */
  static PFS_engine_table* create();
  static int write_row(TABLE *table, unsigned char *buf, Field **fields);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  virtual int update_row_values(TABLE *table,
                                const unsigned char *old_buf,
                                unsigned char *new_buf,
                                Field **fields);

  virtual int delete_row_values(TABLE *table,
                                const unsigned char *buf,
                                Field **fields);

  table_setup_actors();

public:
  ~table_setup_actors()
  {}

private:
  void make_row(PFS_setup_actor *actor);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_setup_actors m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
