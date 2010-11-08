/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HA_NDBCLUSTER_GLUE_H
#define HA_NDBCLUSTER_GLUE_H

#include <mysql_version.h>

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#if MYSQL_VERSION_ID >= 50501
/* Include files for sql/ was split in 5.5, and ha_ndb*  uses a few.. */
#include "sql_priv.h"
#include "unireg.h"         // REQUIRED: for other includes
#include "sql_table.h"      // build_table_filename,
                            // tablename_to_filename,
                            // filename_to_tablename
#include "sql_partition.h"  // HA_CAN_*, partition_info, part_id_range
#include "sql_base.h"       // close_cached_tables
#include "discover.h"       // readfrm
#include "sql_acl.h"        // wild_case_compare
#include "transaction.h"
#include "sql_test.h"       // print_where
#else
#include "mysql_priv.h"
#endif

#include "sql_show.h"       // init_fill_schema_files_row,
                            // schema_table_store_record


#if MYSQL_VERSION_ID >= 50501

/* my_free has lost last argument */
static inline
void my_free(void* ptr, myf MyFlags)
{
  my_free(ptr);
}


/* close_cached_tables has new signature, emulate old */
static inline
bool close_cached_tables(THD *thd, TABLE_LIST *tables, bool have_lock,
                         bool wait_for_refresh, bool wait_for_placeholders)
{
  return close_cached_tables(thd, tables, wait_for_refresh, LONG_TIMEOUT);
}

/* Online alter table not supported */
#define NDB_WITHOUT_ONLINE_ALTER

/* Column format not supported */
#define NDB_WITHOUT_COLUMN_FORMAT

enum column_format_type {
  COLUMN_FORMAT_TYPE_NOT_USED= -1,
  COLUMN_FORMAT_TYPE_DEFAULT=   0,
  COLUMN_FORMAT_TYPE_FIXED=     1,
  COLUMN_FORMAT_TYPE_DYNAMIC=   2
};

/* Tablespace in .frm and TABLE_SHARE->tablespace not supported */
#define NDB_WITHOUT_TABLESPACE_IN_FRM

/* Read before write removal not supported */
#define NDB_WITHOUT_READ_BEFORE_WRITE_REMOVAL

#endif


/* extract the bitmask of options from THD */
static inline
ulonglong thd_options(const THD * thd)
{
#if MYSQL_VERSION_ID < 50500
  return thd->options;
#else
  /* "options" has moved to "variables.option_bits" */
  return thd->variables.option_bits;
#endif
}

#endif
