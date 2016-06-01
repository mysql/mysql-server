/*
   Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_table.h"      // build_table_filename,
                            // tablename_to_filename,
                            // filename_to_tablename
#include "sql_partition.h"  // HA_CAN_*, part_id_range
#include "partition_info.h" // partition_info
#include "sql_base.h"       // close_cached_tables
#include "discover.h"       // readfrm
#include "auth_common.h"    // wild_case_compare
#include "transaction.h"
#include "item_cmpfunc.h"   // Item_func_like
#include "sql_test.h"       // print_where
#include "key.h"            // key_restore
#include "rpl_constants.h"  // Transid in Binlog
#include "rpl_slave.h"      // Silent retry definition
#include "log_event.h"      // my_strmov_quoted_identifier
#include "log.h"            // sql_print_error

#include "sql_show.h"       // init_fill_schema_files_row,
                            // schema_table_store_record

static inline
uint32 thd_unmasked_server_id(const THD* thd)
{
  const uint32 unmasked_server_id = thd->unmasked_server_id;
  assert(thd->server_id == (thd->unmasked_server_id & opt_server_id_mask));
  return unmasked_server_id;
}


/* extract the bitmask of options from THD */
static inline
ulonglong thd_options(const THD * thd)
{
  return thd->variables.option_bits;
}

/* set the "command" member of thd */
static inline
void thd_set_command(THD* thd, enum enum_server_command command)
{
  thd->set_command(command);
}

/* get pointer to Diagnostics Area for statement from THD */
static inline
Diagnostics_area* thd_stmt_da(THD* thd)
{
  return thd->get_stmt_da();
}

#endif
