/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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


#ifndef TABLE_REPLICATION_CONNECTION_NODES_H
#define TABLE_REPLICATION_CONNECTION_NODES_H

/**
  @file storage/perfschema/table_replication_connection_nodes.h
  Table replication_connection_nodes (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "rpl_mi.h"
#include "rpl_reporting.h"
#include "mysql_com.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** enum values for Node_State*/
// TODO: Add functionality for recovering node state

enum enum_node_state {
    PS_NODE_STATE_ONLINE= 1,
    PS_NODE_STATE_OFFLINE,
    PS_NODE_STATE_RECOVERING
};

/**
  A row in connection nodes table. The fields with string values have an additional
  length field denoted by <field_name>_length.
*/
struct st_row_connect_nodes {
  char group_name[UUID_LENGTH];
  bool is_gcs_plugin_loaded;
  bool is_group_name_null;
  uint node_id;
  char node_address[HOSTNAME_LENGTH];
  uint node_address_length;
  enum_node_state node_state;
};

/** Table PERFORMANCE_SCHEMA.REPLICATION_CONNECTION_NODES. */
class table_replication_connection_nodes: public PFS_engine_table
{
private:
  void make_row();
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current row */
  st_row_connect_nodes m_row;
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

  table_replication_connection_nodes();

public:
  ~table_replication_connection_nodes();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
