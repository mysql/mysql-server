/*
   Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_REPLICATION_GROUP_MEMBERS_H
#define TABLE_REPLICATION_GROUP_MEMBERS_H

/**
  @file storage/perfschema/table_replication_group_members.h
  Table replication_group_members (declarations).
*/

#include <mysql/plugin_group_replication.h>
#include <sys/types.h>

#include "mysql_com.h"
#include "sql/rpl_info.h"
#include "sql/sql_const.h"  // UUID_LENGTH
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row in connection nodes table. The fields with string values have an
  additional
  length field denoted by @<field_name@>_length.
*/
struct st_row_group_members
{
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  char member_id[UUID_LENGTH];
  uint member_id_length;
  char member_host[HOSTNAME_LENGTH];
  uint member_host_length;
  uint member_port;
  char member_state[NAME_LEN];
  uint member_state_length;
  char member_role[NAME_LEN];
  uint member_role_length;
  char member_version[NAME_LEN];
  uint member_version_length;
};

/** Table PERFORMANCE_SCHEMA.replication_group_members. */
class table_replication_group_members : public PFS_engine_table
{
private:
  int make_row(uint index);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_group_members m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_replication_group_members();

public:
  ~table_replication_group_members();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);
};

/** @} */
#endif
