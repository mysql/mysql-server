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
#include "key.h"            // key_restore
#include "rpl_constants.h"  // Transid in Binlog
#include "rpl_slave.h"      // Silent retry definition
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

/* thd has no version field anymore */
#define NDB_THD_HAS_NO_VERSION

/* thd->binlog_query has new parameter "direct" */
#define NDB_THD_BINLOG_QUERY_HAS_DIRECT

/* No mysql_rm_table_part2 anymore in 5.5.8 */
#define NDB_NO_MYSQL_RM_TABLE_PART2

#endif

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
#if MYSQL_VERSION_ID < 50500
  return thd->options;
#else
  /* "options" has moved to "variables.option_bits" */
  return thd->variables.option_bits;
#endif
}

/* set the "command" member of thd */
static inline
void thd_set_command(THD* thd, enum enum_server_command command)
{
#if MYSQL_VERSION_ID < 50600
  thd->command = command;
#else
  /* "command" renamed to "m_command", use accessor function */
  thd->set_command(command);
#endif
}

/* get pointer to diagnostic area for statement from THD */
static inline
Diagnostics_area* thd_stmt_da(THD* thd)
{
#if MYSQL_VERSION_ID < 50500
  return &(thd->main_da);
#else
#if MYSQL_VERSION_ID < 50603
  /* "main_da" has been made private and one should use "stmt_da*" */
  return thd->stmt_da;
#else
  /* "stmt_da*" has been made private and one should use 'get_stmt_da()' */
  return thd->get_stmt_da();
#endif
#endif
}

#if MYSQL_VERSION_ID < 50500

/*
  MySQL Server has got its own mutex type in 5.5, add backwards
  compatibility support allowing to write code in 7.0 that works
  in future MySQL Server
*/

typedef pthread_mutex_t mysql_mutex_t;

static inline
int mysql_mutex_lock(mysql_mutex_t* mutex)
{
  return pthread_mutex_lock(mutex);
}

static inline
int mysql_mutex_unlock(mysql_mutex_t* mutex)
{
  return pthread_mutex_unlock(mutex);
}

static inline
void mysql_mutex_assert_owner(mysql_mutex_t* mutex)
{
  return safe_mutex_assert_owner(mutex);
}

typedef pthread_cond_t mysql_cond_t;

static inline
int mysql_cond_wait(mysql_cond_t* cond, mysql_mutex_t* mutex)
{
  return pthread_cond_wait(cond, mutex);
}

static inline
int mysql_cond_timedwait(mysql_cond_t* cond, mysql_mutex_t* mutex,
                         struct timespec* abstime)
{
  return pthread_cond_timedwait(cond, mutex, abstime);
}

#endif

static inline
uint partition_info_num_full_part_fields(const partition_info* part_info)
{
#if MYSQL_VERSION_ID < 50500
  return part_info->no_full_part_fields;
#else
  /* renamed to 'num_full_part_fields' and no accessor function*/
  return part_info->num_full_part_fields;
#endif
}

static inline
uint partition_info_num_parts(const partition_info* part_info)
{
#if MYSQL_VERSION_ID < 50500
  return part_info->no_parts;
#else
  /* renamed to 'num_parts' and no accessor function */
  return part_info->num_parts;
#endif
}

static inline
uint partition_info_num_list_values(const partition_info* part_info)
{
#if MYSQL_VERSION_ID < 50500
  return part_info->no_list_values;
#else
  /* renamed to 'num_list_values' and no accessor function */
  return part_info->num_list_values;
#endif
}

static inline
bool partition_info_use_default_num_partitions(const partition_info* part_info)
{
#if MYSQL_VERSION_ID < 50500
  return part_info->use_default_no_partitions;
#else
  /* renamed to 'use_default_num_partitions' and no accessor function */
  return part_info->use_default_num_partitions;
#endif
}

static inline
uint partition_info_num_subparts(const partition_info* part_info)
{
#if MYSQL_VERSION_ID < 50500
  return part_info->no_subparts;
#else
  /* renamed to 'num_subparts' and no accessor function */
  return part_info->num_subparts;
#endif
}

#endif
