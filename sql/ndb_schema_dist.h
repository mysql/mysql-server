/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SCHEMA_DIST_H
#define NDB_SCHEMA_DIST_H

#include <mysql/plugin.h>
#include <my_global.h>


/**
  Check if schema distribution has been initialized and is
  ready. Will return true when the component is properly setup
  to receive schema op events from the cluster.
*/
bool ndb_schema_dist_is_ready(void);


/*
  The numbers below must not change as they
  are passed between mysql servers, and if changed
  would break compatablility.  Add new numbers to
  the end.
*/
enum SCHEMA_OP_TYPE
{
  SOT_DROP_TABLE= 0,
  SOT_CREATE_TABLE= 1,
  SOT_RENAME_TABLE_NEW= 2, // Unused, but still reserved
  SOT_ALTER_TABLE_COMMIT= 3,
  SOT_DROP_DB= 4,
  SOT_CREATE_DB= 5,
  SOT_ALTER_DB= 6,
  SOT_CLEAR_SLOCK= 7,
  SOT_TABLESPACE= 8,
  SOT_LOGFILE_GROUP= 9,
  SOT_RENAME_TABLE= 10,
  SOT_TRUNCATE_TABLE= 11,
  SOT_RENAME_TABLE_PREPARE= 12,
  SOT_ONLINE_ALTER_TABLE_PREPARE= 13,
  SOT_ONLINE_ALTER_TABLE_COMMIT= 14,
  SOT_CREATE_USER= 15,
  SOT_DROP_USER= 16,
  SOT_RENAME_USER= 17,
  SOT_GRANT= 18,
  SOT_REVOKE= 19
};


int ndbcluster_log_schema_op(THD* thd,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             SCHEMA_OP_TYPE type,
                             const char *new_db,
                             const char *new_table_name);

const char* get_schema_type_name(uint type);


#endif
