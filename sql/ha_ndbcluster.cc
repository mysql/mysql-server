/* Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  This file defines the NDB Cluster handler: the interface between
  MySQL and NDB Cluster
*/

#include "sql/ha_ndbcluster.h"

#include <memory>
#include <string>


#include "m_ctype.h"
#include "my_dbug.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_thread.h"
#include "sql/abstract_query_plan.h"
#include "sql/current_thd.h"
#include "sql/derror.h"     // ER_THD
#include "sql/ha_ndb_index_stat.h"
#include "sql/ha_ndbcluster_binlog.h"
#include "sql/ha_ndbcluster_cond.h"
#include "sql/ha_ndbcluster_connection.h"
#include "sql/ha_ndbcluster_push.h"
#include "sql/ha_ndbcluster_tables.h"
#include "sql/mysqld.h"     // global_system_variables table_alias_charset ...
#include "sql/mysqld_thd_manager.h" // Global_THD_manager
#include "sql/ndb_anyvalue.h"
#include "sql/ndb_binlog_client.h"
#include "sql/ndb_binlog_extra_row_info.h"
#include "sql/ndb_bitmap.h"
#include "sql/ndb_component.h"
#include "sql/ndb_conflict.h"
#include "sql/ndb_dist_priv_util.h"
#include "sql/ndb_event_data.h"
#include "sql/ndb_global_schema_lock.h"
#include "sql/ndb_global_schema_lock_guard.h"
#include "sql/ndb_local_connection.h"
#include "sql/ndb_local_schema.h"
#include "sql/ndb_log.h"
#include "sql/ndb_mi.h"
#include "sql/ndb_name_util.h"
#include "sql/ndb_modifiers.h"
#include "sql/ndb_schema_dist.h"
#include "sql/ndb_sleep.h"
#include "sql/ndb_table_guard.h"
#include "sql/ndb_metadata.h"
#include "sql/ndb_tdc.h"
#include "sql/ndb_thd.h"
#include "sql/partition_info.h"
#include "sql/sql_alter.h"
#include "sql/sql_lex.h"
#include "storage/ndb/include/ndb_global.h"
#include "storage/ndb/include/ndb_version.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/ndbapi/NdbIndexStat.hpp"
#include "storage/ndb/include/ndbapi/NdbInterpretedCode.hpp"
#include "storage/ndb/include/util/SparseBitmask.hpp"
#include "storage/ndb/src/common/util/parse_mask.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryBuilder.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryOperation.hpp"
#include "template_utils.h"

#ifndef DBUG_OFF
#include "sql/sql_test.h"   // print_where
#endif
#include "sql/ndb_dd.h"
                            // tablename_to_filename
#include "sql/sql_class.h"
#include "sql/sql_table.h"  // build_table_filename,
#include "sql/ndb_dd.h"
#include "sql/ndb_dd_client.h"
#include "sql/ndb_dd_disk_data.h"
#include "sql/ndb_dd_table.h"
#include "sql/ndb_dummy_ts.h"
#include "sql/ndb_server_hooks.h"

typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index NDBINDEX;
typedef NdbDictionary::Dictionary NDBDICT;

// ndb interface initialization/cleanup
extern "C" void ndb_init_internal(Uint32);
extern "C" void ndb_end_internal(Uint32);

static const int DEFAULT_PARALLELISM= 0;
static const ha_rows DEFAULT_AUTO_PREFETCH= 32;
static const ulong ONE_YEAR_IN_SECONDS= (ulong) 3600L*24L*365L;

ulong opt_ndb_extra_logging;
static ulong opt_ndb_wait_connected;
static ulong opt_ndb_wait_setup;
static uint opt_ndb_cluster_connection_pool;
static char* opt_connection_pool_nodeids_str;
static uint opt_ndb_recv_thread_activation_threshold;
static char* opt_ndb_recv_thread_cpu_mask;
static char* opt_ndb_index_stat_option;
static char* opt_ndb_connectstring;
static uint opt_ndb_nodeid;
static bool opt_ndb_read_backup;
static ulong opt_ndb_data_node_neighbour;
static bool opt_ndb_fully_replicated;

// The version where ndbcluster uses DYNAMIC by default when creating columns
static ulong NDB_VERSION_DYNAMIC_IS_DEFAULT = 50711;
enum ndb_default_colum_format_enum {
  NDB_DEFAULT_COLUMN_FORMAT_FIXED= 0,
  NDB_DEFAULT_COLUMN_FORMAT_DYNAMIC= 1
};
static const char* default_column_format_names[]= { "FIXED", "DYNAMIC", NullS };
static ulong opt_ndb_default_column_format;
static TYPELIB default_column_format_typelib= {
  array_elements(default_column_format_names) - 1,
  "",
  default_column_format_names,
  NULL
};
static MYSQL_SYSVAR_ENUM(
  default_column_format,               /* name */
  opt_ndb_default_column_format,       /* var */
  PLUGIN_VAR_RQCMDARG,
  "Change COLUMN_FORMAT default value (fixed or dynamic) "
  "for backward compatibility. Also affects the default value "
  "of ROW_FORMAT.",
  NULL,                                /* check func. */
  NULL,                                /* update func. */
  NDB_DEFAULT_COLUMN_FORMAT_FIXED,     /* default */
  &default_column_format_typelib       /* typelib */
);

static MYSQL_THDVAR_UINT(
  autoincrement_prefetch_sz,         /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specify number of autoincrement values that are prefetched.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1,                                 /* default */
  1,                                 /* min */
  65535,                             /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_BOOL(
  force_send,                        /* name */
  PLUGIN_VAR_OPCMDARG,
  "Force send of buffers to ndb immediately without waiting for "
  "other threads.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


static MYSQL_THDVAR_BOOL(
  use_exact_count,                   /* name */
  PLUGIN_VAR_OPCMDARG,
  "Use exact records count during query planning and for fast "
  "select count(*), disable for faster queries.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);


static MYSQL_THDVAR_BOOL(
  use_transactions,                  /* name */
  PLUGIN_VAR_OPCMDARG,
  "Use transactions for large inserts, if enabled then large "
  "inserts will be split into several smaller transactions",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


static MYSQL_THDVAR_BOOL(
  use_copying_alter_table,           /* name */
  PLUGIN_VAR_OPCMDARG,
  "Force ndbcluster to always copy tables at alter table (should "
  "only be used if online alter table fails).",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);


static MYSQL_THDVAR_BOOL(
  allow_copying_alter_table,         /* name */
  PLUGIN_VAR_OPCMDARG,
  "Specifies if implicit copying alter table is allowed. Can be overridden "
  "by using ALGORITHM=COPY in the alter table command.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


static MYSQL_THDVAR_UINT(
  optimized_node_selection,          /* name */
  PLUGIN_VAR_OPCMDARG,
  "Select nodes for transactions in a more optimal way.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  3,                                 /* default */
  0,                                 /* min */
  3,                                 /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_ULONG(
  batch_size,                        /* name */
  PLUGIN_VAR_RQCMDARG,
  "Batch size in bytes.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  32768,                             /* default */
  0,                                 /* min */
  ONE_YEAR_IN_SECONDS,               /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_ULONG(
  optimization_delay,                /* name */
  PLUGIN_VAR_RQCMDARG,
  "For optimize table, specifies the delay in milliseconds "
  "for each batch of rows sent.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  10,                                /* default */
  0,                                 /* min */
  100000,                            /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_BOOL(
  index_stat_enable,                 /* name */
  PLUGIN_VAR_OPCMDARG,
  "Use ndb index statistics in query optimization.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  true                               /* default */
);


static MYSQL_THDVAR_BOOL(
  table_no_logging,                  /* name */
  PLUGIN_VAR_NOCMDARG,
  "",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  false                              /* default */
);


static MYSQL_THDVAR_BOOL(
  table_temporary,                   /* name */
  PLUGIN_VAR_NOCMDARG,
  "",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  false                              /* default */
);

static MYSQL_THDVAR_UINT(
  blob_read_batch_bytes,             /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specifies the bytesize large Blob reads "
  "should be batched into.  0 == No limit.",
  NULL,                              /* check func */
  NULL,                              /* update func */
  65536,                             /* default */
  0,                                 /* min */
  UINT_MAX,                          /* max */
  0                                  /* block */
);

static MYSQL_THDVAR_UINT(
  blob_write_batch_bytes,            /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specifies the bytesize large Blob writes "
  "should be batched into.  0 == No limit.",
  NULL,                              /* check func */
  NULL,                              /* update func */
  65536,                             /* default */
  0,                                 /* min */
  UINT_MAX,                          /* max */
  0                                  /* block */
);

static MYSQL_THDVAR_UINT(
  deferred_constraints,              /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specified that constraints should be checked deferred (when supported)",
  NULL,                              /* check func */
  NULL,                              /* update func */
  0,                                 /* default */
  0,                                 /* min */
  1,                                 /* max */
  0                                  /* block */
);

static MYSQL_THDVAR_BOOL(
  show_foreign_key_mock_tables,          /* name */
  PLUGIN_VAR_OPCMDARG,
  "Show the mock tables which is used to support foreign_key_checks= 0. "
  "Extra info warnings are shown when creating and dropping the tables. "
  "The real table name is show in SHOW CREATE TABLE",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);

static MYSQL_THDVAR_BOOL(
  join_pushdown,                     /* name */
  PLUGIN_VAR_OPCMDARG,
  "Enable pushing down of join to datanodes",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  true                               /* default */
);

static MYSQL_THDVAR_BOOL(
  log_exclusive_reads,               /* name */
  PLUGIN_VAR_OPCMDARG,
  "Log primary key reads with exclusive locks "
  "to allow conflict resolution based on read conflicts",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);


/*
  Required in index_stat.cc but available only from here
  thanks to use of top level anonymous structs.
*/
bool ndb_index_stat_get_enable(THD *thd)
{
  const bool value = THDVAR(thd, index_stat_enable);
  return value;
}

bool ndb_show_foreign_key_mock_tables(THD* thd)
{
  const bool value = THDVAR(thd, show_foreign_key_mock_tables);
  return value;
}

static int ndbcluster_end(handlerton *hton, ha_panic_function);
static bool ndbcluster_show_status(handlerton *, THD*,
                                   stat_print_fn *,
                                   enum ha_stat_type);
static int
ndbcluster_make_pushed_join(handlerton *, THD*, const AQP::Join_plan*);

static int ndbcluster_get_tablespace(THD* thd,
                                     LEX_CSTRING db_name,
                                     LEX_CSTRING table_name,
                                     LEX_CSTRING *tablespace_name);
static int ndbcluster_alter_tablespace(handlerton*, THD* thd,
                                       st_alter_tablespace* info,
                                       const dd::Tablespace*,
                                       dd::Tablespace*);
static bool
ndbcluster_get_tablespace_statistics(const char *tablespace_name,
                                     const char *file_name,
                                     const dd::Properties &ts_se_private_data,
                                     ha_tablespace_statistics *stats);


static handler *ndbcluster_create_handler(handlerton *hton, TABLE_SHARE *table,
                                          bool /* partitioned */,
                                          MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ndbcluster(hton, table);
}

static uint
ndbcluster_partition_flags()
{
  return (HA_CAN_UPDATE_PARTITION_KEY |
          HA_CAN_PARTITION_UNIQUE | HA_USE_AUTO_PARTITION);
}

uint ha_ndbcluster::alter_flags(uint flags) const
{
  const uint f=
    HA_PARTITION_FUNCTION_SUPPORTED |
    0;

  if (flags & Alter_info::ALTER_DROP_PARTITION)
    return 0;

  return f;
}

static constexpr uint NDB_AUTO_INCREMENT_RETRIES = 100;
#define BATCH_FLUSH_SIZE (32768)

#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("%d  message: %s", err.code, err.message))

#define ERR_RETURN(err)                  \
{                                        \
  const NdbError& tmp= err;              \
  DBUG_RETURN(ndb_to_mysql_error(&tmp)); \
}

#define ERR_BREAK(err, code)             \
{                                        \
  const NdbError& tmp= err;              \
  code= ndb_to_mysql_error(&tmp);        \
  break;                                 \
}

#define ERR_SET(err, code)               \
{                                        \
  const NdbError& tmp= err;              \
  code= ndb_to_mysql_error(&tmp);        \
}

static int ndbcluster_inited= 0;

/* 
   Indicator used to delay client and slave
   connections until Ndb has Binlog setup
   (bug#46955)
*/
int ndb_setup_complete= 0; // Use ndbcluster_mutex & ndbcluster_cond
extern Ndb* g_ndb;
extern Ndb_cluster_connection *g_ndb_cluster_connection;

/// Handler synchronization
mysql_mutex_t ndbcluster_mutex;
mysql_cond_t  ndbcluster_cond;

static const char* ndbcluster_hton_name = "ndbcluster";
static const int ndbcluster_hton_name_length = sizeof(ndbcluster_hton_name)-1;

static void modify_shared_stats(NDB_SHARE *share,
                                Ndb_local_table_statistics *local_stat);

static int ndb_get_table_statistics(THD *thd, ha_ndbcluster*, Ndb*,
                                    const NdbDictionary::Table*,
                                    const NdbRecord *,
                                    struct Ndb_statistics *,
                                    uint part_id= ~(uint)0);

static ulong multi_range_fixed_size(int num_ranges);

static ulong multi_range_max_entry(NDB_INDEX_TYPE keytype, ulong reclength);

/* Status variables shown with 'show status like 'Ndb%' */

struct st_ndb_status g_ndb_status;

long long g_event_data_count = 0;
long long g_event_nondata_count = 0;
long long g_event_bytes_count = 0;

static long long g_slave_api_client_stats[Ndb::NumClientStatistics];

static long long g_server_api_client_stats[Ndb::NumClientStatistics];

/**
  @brief Copy the slave threads Ndb statistics to global
         variables, thus allowing the statistics to be read
         from other threads when those display status variables. This
         copy out need to happen with regular intervals and as
         such the slave thread will call it at convenient times.
  @note This differs from other threads who will copy statistics
        from their own Ndb object before showing the values.
  @param ndb The ndb object
*/
void
update_slave_api_stats(const Ndb* ndb)
{
  // Should only be called by the slave (applier) thread
  DBUG_ASSERT(current_thd->slave_thread);

  for (Uint32 i=0; i < Ndb::NumClientStatistics; i++)
  {
    g_slave_api_client_stats[i] = ndb->getClientStat(i);
  }
}

st_ndb_slave_state g_ndb_slave_state;

static int check_slave_config()
{
  DBUG_ENTER("check_slave_config");

  if (ndb_get_number_of_channels() > 1)
  {
    ndb_log_error("NDB Slave: Configuration with number of replication "
                  "masters = %u is not supported when applying to NDB",
                  ndb_get_number_of_channels());
    DBUG_RETURN(HA_ERR_UNSUPPORTED);
  }
  if (ndb_mi_get_slave_parallel_workers() > 0)
  {
    ndb_log_error("NDB Slave: Configuration 'slave_parallel_workers = %lu' is "
                  "not supported when applying to NDB",
                  ndb_mi_get_slave_parallel_workers());
    DBUG_RETURN(HA_ERR_UNSUPPORTED);
  }

  DBUG_RETURN(0);
}

static int check_slave_state(THD* thd)
{
  DBUG_ENTER("check_slave_state");

  if (!thd->slave_thread)
    DBUG_RETURN(0);

  const Uint32 runId = ndb_mi_get_slave_run_id();
  DBUG_PRINT("info", ("Slave SQL thread run id is %u",
                      runId));
  if (unlikely(runId != g_ndb_slave_state.sql_run_id))
  {
    DBUG_PRINT("info", ("Slave run id changed from %u, "
                        "treating as Slave restart",
                        g_ndb_slave_state.sql_run_id));

    /*
     * Check that the slave configuration is supported
     */
    int error = check_slave_config();
    if (unlikely(error))
      DBUG_RETURN(error);

    g_ndb_slave_state.sql_run_id = runId;

    g_ndb_slave_state.atStartSlave();

    /* Always try to load the Max Replicated Epoch info
     * first.
     * Could be made optional if it's a problem
     */
    {
      /*
         Load highest replicated epoch from a local
         MySQLD from the cluster.
      */
      DBUG_PRINT("info", ("Loading applied epoch information from %s",
                          NDB_APPLY_TABLE));
      NdbError ndb_error;
      Uint64 highestAppliedEpoch = 0;
      do
      {
        Ndb* ndb= check_ndb_in_thd(thd);
        NDBDICT* dict= ndb->getDictionary();
        NdbTransaction* trans= NULL;
        ndb->setDatabaseName(NDB_REP_DB);
        Ndb_table_guard ndbtab_g(dict, NDB_APPLY_TABLE);

        const NDBTAB* ndbtab= ndbtab_g.get_table();
        if (unlikely(ndbtab == NULL))
        {
          ndb_error = dict->getNdbError();
          break;
        }

        trans= ndb->startTransaction();
        if (unlikely(trans == NULL))
        {
          ndb_error = ndb->getNdbError();
          break;
        }

        do
        {
          NdbScanOperation* sop = trans->getNdbScanOperation(ndbtab);
          if (unlikely(sop == NULL))
          {
            ndb_error = trans->getNdbError();
            break;
          }

          const Uint32 server_id_col_num = 0;
          const Uint32 epoch_col_num = 1;
          NdbRecAttr* server_id_ra = 0;
          NdbRecAttr* epoch_ra = 0;

          if (unlikely((sop->readTuples(NdbOperation::LM_CommittedRead) != 0)   ||
                       ((server_id_ra = sop->getValue(server_id_col_num)) == NULL)  ||
                       ((epoch_ra = sop->getValue(epoch_col_num)) == NULL)))
          {
            ndb_error = sop->getNdbError();
            break;
          }

          if (trans->execute(NdbTransaction::Commit))
          {
            ndb_error = trans->getNdbError();
            break;
          }

          int rc = 0;
          while (0 == (rc= sop->nextResult(true)))
          {
            Uint32 serverid = server_id_ra->u_32_value();
            Uint64 epoch = epoch_ra->u_64_value();

            if ((serverid == ::server_id) ||
                (ndb_mi_get_ignore_server_id(serverid)))
            {
              highestAppliedEpoch = MAX(epoch, highestAppliedEpoch);
            }
          }

          if (rc != 1)
          {
            ndb_error = sop->getNdbError();
            break;
          }
        } while (0);

        trans->close();
      } while(0);

      if (ndb_error.code != 0)
      {
        ndb_log_warning("NDB Slave: Could not determine maximum replicated "
                        "epoch from %s.%s at Slave start, error %u %s",
                        NDB_REP_DB, NDB_APPLY_TABLE,
                        ndb_error.code, ndb_error.message);
      }

      /*
        Set Global status variable to the Highest Applied Epoch from
        the Cluster DB.
        If none was found, this will be zero.
      */
      g_ndb_slave_state.max_rep_epoch = highestAppliedEpoch;
      ndb_log_info("NDB Slave: MaxReplicatedEpoch set to %llu (%u/%u) at "
                   "Slave start",
                   g_ndb_slave_state.max_rep_epoch,
                   (Uint32)(g_ndb_slave_state.max_rep_epoch >> 32),
                   (Uint32)(g_ndb_slave_state.max_rep_epoch & 0xffffffff));
    } // Load highest replicated epoch
  } // New Slave SQL thread run id

  DBUG_RETURN(0);
}


static int update_status_variables(Thd_ndb *thd_ndb,
                                   st_ndb_status *ns,
                                   Ndb_cluster_connection *c)
{
  ns->connected_port= c->get_connected_port();
  ns->connected_host= c->get_connected_host();
  if (ns->cluster_node_id != (int) c->node_id())
  {
    ns->cluster_node_id= c->node_id();
    if (&g_ndb_status == ns && g_ndb_cluster_connection == c)
      ndb_log_info("NodeID is %lu, management server '%s:%lu'",
                   ns->cluster_node_id, ns->connected_host,
                   ns->connected_port);
  }
  {
    int n= c->get_no_ready();
    ns->number_of_ready_data_nodes= n > 0 ?  n : 0;
  }
  ns->number_of_data_nodes= c->no_db_nodes();
  ns->connect_count= c->get_connect_count();
  ns->system_name = c->get_system_name();
  ns->last_commit_epoch_server= ndb_get_latest_trans_gci();
  if (thd_ndb)
  {
    ns->execute_count= thd_ndb->m_execute_count;
    ns->scan_count= thd_ndb->m_scan_count;
    ns->pruned_scan_count= thd_ndb->m_pruned_scan_count;
    ns->sorted_scan_count= thd_ndb->m_sorted_scan_count;
    ns->pushed_queries_defined= thd_ndb->m_pushed_queries_defined;
    ns->pushed_queries_dropped= thd_ndb->m_pushed_queries_dropped;
    ns->pushed_queries_executed= thd_ndb->m_pushed_queries_executed;
    ns->pushed_reads= thd_ndb->m_pushed_reads;
    ns->last_commit_epoch_session = thd_ndb->m_last_commit_epoch_session;
    for (int i= 0; i < MAX_NDB_NODES; i++)
    {
      ns->transaction_no_hint_count[i]= thd_ndb->m_transaction_no_hint_count[i];
      ns->transaction_hint_count[i]= thd_ndb->m_transaction_hint_count[i];
    }
    for (int i=0; i < Ndb::NumClientStatistics; i++)
    {
      ns->api_client_stats[i] = thd_ndb->ndb->getClientStat(i);
    }
    ns->schema_locks_count= thd_ndb->schema_locks_count;
  }
  return 0;
}

/* Helper macro for definitions of NdbApi status variables */

#define NDBAPI_COUNTERS(NAME_SUFFIX, ARRAY_LOCATION)                    \
  {"api_wait_exec_complete_count" NAME_SUFFIX,                          \
   (char*) ARRAY_LOCATION[ Ndb::WaitExecCompleteCount ],                \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_wait_scan_result_count" NAME_SUFFIX,                            \
   (char*) ARRAY_LOCATION[ Ndb::WaitScanResultCount ],                  \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_wait_meta_request_count" NAME_SUFFIX,                           \
   (char*) ARRAY_LOCATION[ Ndb::WaitMetaRequestCount ],                 \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_wait_nanos_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::WaitNanosCount ],                       \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_bytes_sent_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::BytesSentCount ],                       \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_bytes_received_count" NAME_SUFFIX,                              \
   (char*) ARRAY_LOCATION[ Ndb::BytesRecvdCount ],                      \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_trans_start_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransStartCount ],                      \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_trans_commit_count" NAME_SUFFIX,                                \
   (char*) ARRAY_LOCATION[ Ndb::TransCommitCount ],                     \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_trans_abort_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransAbortCount ],                      \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_trans_close_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransCloseCount ],                      \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_pk_op_count" NAME_SUFFIX,                                       \
   (char*) ARRAY_LOCATION[ Ndb::PkOpCount ],                            \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_uk_op_count" NAME_SUFFIX,                                       \
   (char*) ARRAY_LOCATION[ Ndb::UkOpCount ],                            \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_table_scan_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::TableScanCount ],                       \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_range_scan_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::RangeScanCount ],                       \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_pruned_scan_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::PrunedScanCount ],                      \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_scan_batch_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::ScanBatchCount ],                       \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_read_row_count" NAME_SUFFIX,                                    \
   (char*) ARRAY_LOCATION[ Ndb::ReadRowCount ],                         \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_trans_local_read_row_count" NAME_SUFFIX,                        \
   (char*) ARRAY_LOCATION[ Ndb::TransLocalReadRowCount ],               \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_adaptive_send_forced_count" NAME_SUFFIX,                        \
   (char *) ARRAY_LOCATION[ Ndb::ForcedSendsCount ],                    \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_adaptive_send_unforced_count" NAME_SUFFIX,                      \
   (char *) ARRAY_LOCATION[ Ndb::UnforcedSendsCount ],                  \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                   \
  {"api_adaptive_send_deferred_count" NAME_SUFFIX,                      \
   (char *) ARRAY_LOCATION[ Ndb::DeferredSendsCount ],                  \
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL}


static SHOW_VAR ndb_status_vars_dynamic[]=
{
  {"cluster_node_id",     (char*) &g_ndb_status.cluster_node_id,      SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"config_from_host",    (char*) &g_ndb_status.connected_host,       SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL},
  {"config_from_port",    (char*) &g_ndb_status.connected_port,       SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"number_of_data_nodes",(char*) &g_ndb_status.number_of_data_nodes, SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"number_of_ready_data_nodes",
   (char*) &g_ndb_status.number_of_ready_data_nodes,                  SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"connect_count",      (char*) &g_ndb_status.connect_count,         SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"execute_count",      (char*) &g_ndb_status.execute_count,         SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"scan_count",         (char*) &g_ndb_status.scan_count,            SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"pruned_scan_count",  (char*) &g_ndb_status.pruned_scan_count,     SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"schema_locks_count", (char*) &g_ndb_status.schema_locks_count,    SHOW_LONG, SHOW_SCOPE_GLOBAL},
  NDBAPI_COUNTERS("_session", &g_ndb_status.api_client_stats),
  {"sorted_scan_count",  (char*) &g_ndb_status.sorted_scan_count,     SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"pushed_queries_defined", (char*) &g_ndb_status.pushed_queries_defined, 
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"pushed_queries_dropped", (char*) &g_ndb_status.pushed_queries_dropped,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"pushed_queries_executed", (char*) &g_ndb_status.pushed_queries_executed,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"pushed_reads",       (char*) &g_ndb_status.pushed_reads,          SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"last_commit_epoch_server", 
                         (char*) &g_ndb_status.last_commit_epoch_server,
                                                                      SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"last_commit_epoch_session", 
                         (char*) &g_ndb_status.last_commit_epoch_session, 
                                                                      SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"system_name", (char*) &g_ndb_status.system_name, SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL},
  {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}
};


static SHOW_VAR ndb_status_vars_injector[]=
{
  {"api_event_data_count_injector",     (char*) &g_event_data_count, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"api_event_nondata_count_injector",  (char*) &g_event_nondata_count, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"api_event_bytes_count_injector",    (char*) &g_event_bytes_count, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}
};

static SHOW_VAR ndb_status_vars_slave[]=
{
  NDBAPI_COUNTERS("_slave", &g_slave_api_client_stats),
  {"slave_max_replicated_epoch", (char*) &g_ndb_slave_state.max_rep_epoch, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}
};

static SHOW_VAR ndb_status_vars_server_api[]=
{
  NDBAPI_COUNTERS("", &g_server_api_client_stats),
  {"api_event_data_count",     
   (char*) &g_server_api_client_stats[ Ndb::DataEventsRecvdCount ], 
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"api_event_nondata_count",  
   (char*) &g_server_api_client_stats[ Ndb::NonDataEventsRecvdCount ], 
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {"api_event_bytes_count",    
   (char*) &g_server_api_client_stats[ Ndb::EventBytesRecvdCount ], 
   SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}
};


/*
   Called when SHOW STATUS or performance_schema.[global|session]_status
   wants to see the status variables. We use this opportunity to:
   1) Update the globals with current values
   2) Return an array of var definitions, pointing to
      the updated globals
*/

static
int show_ndb_status_server_api(THD*, SHOW_VAR *var, char*)
{
  ndb_get_connection_stats((Uint64*) &g_server_api_client_stats[0]);

  var->type= SHOW_ARRAY;
  var->value= (char*) ndb_status_vars_server_api;
  var->scope= SHOW_SCOPE_GLOBAL;

  return 0;
}


/*
  Error handling functions
*/

/* Note for merge: old mapping table, moved to storage/ndb/ndberror.c */

int ndb_to_mysql_error(const NdbError *ndberr)
{
  /* read the mysql mapped error code */
  int error= ndberr->mysql_code;

  switch (error)
  {
    /* errors for which we do not add warnings, just return mapped error code
    */
  case HA_ERR_NO_SUCH_TABLE:
  case HA_ERR_KEY_NOT_FOUND:
    return error;

    /* Mapping missing, go with the ndb error code */
  case -1:
  case 0:
    /* Never map to errors below HA_ERR_FIRST */
    if (ndberr->code < HA_ERR_FIRST)
      error= HA_ERR_INTERNAL_ERROR;
    else
      error= ndberr->code;
    break;
    /* Mapping exists, go with the mapped code */
  default:
    break;
  }

  {
    /*
      Push the NDB error message as warning
      - Used to be able to use SHOW WARNINGS to get more info
        on what the error is
      - Used by replication to see if the error was temporary
    */
    if (ndberr->status == NdbError::TemporaryError)
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_GET_TEMPORARY_ERRMSG,
                          ER_THD(current_thd, ER_GET_TEMPORARY_ERRMSG),
                          ndberr->code, ndberr->message, "NDB");
    else
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG, ER_THD(current_thd, ER_GET_ERRMSG),
                          ndberr->code, ndberr->message, "NDB");
  }
  return error;
}

/**
  Report error using my_error() and the values extracted from the NdbError.
  If a proper mysql_code mapping is not available, the error message
  from the ndbError is pushed to my_error.
  If a proper mapping is available, the ndb error message is pushed as a
  warning and the mapped mysql error code is pushed as the error.

  @param    ndberr            The NdbError object with error information
*/
void ndb_my_error(const NdbError *ndberr)
{
  if (ndberr->mysql_code == -1)
  {
    /* No mysql_code mapping present - print ndb error message */
    const int error_number = (ndberr->status == NdbError::TemporaryError)
                         ? ER_GET_TEMPORARY_ERRMSG
                         : ER_GET_ERRMSG;
    my_error(error_number, MYF(0), ndberr->code, ndberr->message, "NDB");
  }
  else
  {
    /* MySQL error code mapping is present.
     * Now call ndb_to_mysql_error() with the ndberr object.
     * This will check the validity of the mysql error code
     * and convert it into a more proper error if required.
     * It will also push the ndb error message as a warning.
     */
    const int error_number = ndb_to_mysql_error(ndberr);
    /* Now push the relevant mysql error to my_error */
    my_error(error_number, MYF(0));
  }
}

ulong opt_ndb_slave_conflict_role;

static int
handle_conflict_op_error(NdbTransaction* trans,
                         const NdbError& err,
                         const NdbOperation* op);

static int
handle_row_conflict(NDB_CONFLICT_FN_SHARE* cfn_share,
                    const char* tab_name,
                    const char* handling_type,
                    const NdbRecord* key_rec,
                    const NdbRecord* data_rec,
                    const uchar* old_row,
                    const uchar* new_row,
                    enum_conflicting_op_type op_type,
                    enum_conflict_cause conflict_cause,
                    const NdbError& conflict_error,
                    NdbTransaction* conflict_trans,
                    const MY_BITMAP *write_set,
                    Uint64 transaction_id);

static const Uint32 error_op_after_refresh_op = 920;

static inline
int
check_completed_operations_pre_commit(Thd_ndb *thd_ndb, NdbTransaction *trans,
                                      const NdbOperation *first,
                                      const NdbOperation *last,
                                      uint *ignore_count)
{
  uint ignores= 0;
  DBUG_ENTER("check_completed_operations_pre_commit");

  if (unlikely(first == 0))
  {
    assert(last == 0);
    DBUG_RETURN(0);
  }

  /*
    Check that all errors are "accepted" errors
    or exceptions to report
  */
  const NdbOperation* lastUserOp = trans->getLastDefinedOperation();
  while (true)
  {
    const NdbError &err= first->getNdbError();
    const bool op_has_conflict_detection = (first->getCustomData() != NULL);
    if (!op_has_conflict_detection)
    {
      DBUG_ASSERT(err.code != (int) error_op_after_refresh_op);

      /* 'Normal path' - ignore key (not) present, others are errors */
      if (err.classification != NdbError::NoError &&
          err.classification != NdbError::ConstraintViolation &&
          err.classification != NdbError::NoDataFound)
      {
        /* Non ignored error, report it */
        DBUG_PRINT("info", ("err.code == %u", err.code));
        DBUG_RETURN(err.code);
      }
    }
    else
    {
      /*
         Op with conflict detection, use special error handling method
       */

      if (err.classification != NdbError::NoError)
      {
        int res = handle_conflict_op_error(trans,
                                           err,
                                           first);
        if (res != 0)
          DBUG_RETURN(res);
      }
    } // if (!op_has_conflict_detection)
    if (err.classification != NdbError::NoError)
      ignores++;

    if (first == last)
      break;

    first= trans->getNextCompletedOperation(first);
  }
  if (ignore_count)
    *ignore_count= ignores;

  /*
     Conflict detection related error handling above may have defined
     new operations on the transaction.  If so, execute them now
  */
  if (trans->getLastDefinedOperation() != lastUserOp)
  {
    const NdbOperation* last_conflict_op = trans->getLastDefinedOperation();

    NdbError nonMaskedError;
    assert(nonMaskedError.code == 0);

    if (trans->execute(NdbTransaction::NoCommit,
                       NdbOperation::AO_IgnoreError,
                       thd_ndb->m_force_send))
    {
      /* Transaction execute failed, even with IgnoreError... */
      nonMaskedError = trans->getNdbError();
      assert(nonMaskedError.code != 0);
    }
    else if (trans->getNdbError().code)
    {
      /* Check the result codes of the operations we added */
      const NdbOperation* conflict_op = NULL;
      do
      {
        conflict_op = trans->getNextCompletedOperation(conflict_op);
        assert(conflict_op != NULL);
        /* We will ignore 920 which represents a refreshOp or other op
         * arriving after a refreshOp
         */
        const NdbError& err = conflict_op->getNdbError();
        if ((err.code != 0) &&
            (err.code != (int) error_op_after_refresh_op))
        {
          /* Found a real error, break out and handle it */
          nonMaskedError = err;
          break;
        }
      } while (conflict_op != last_conflict_op);
    }

    /* Handle errors with extra conflict handling operations */
    if (nonMaskedError.code != 0)
    {
      if (nonMaskedError.status == NdbError::TemporaryError)
      {
        /* Slave will roll back and retry entire transaction. */
        ERR_RETURN(nonMaskedError);
      }
      else
      {
        char msg[FN_REFLEN];
        snprintf(msg, sizeof(msg), "Executing extra operations for "
                    "conflict handling hit Ndb error %d '%s'",
                    nonMaskedError.code, nonMaskedError.message);
        push_warning_printf(current_thd, Sql_condition::SL_ERROR,
                            ER_EXCEPTIONS_WRITE_ERROR,
                            ER_THD(current_thd,
                                   ER_EXCEPTIONS_WRITE_ERROR), msg);
        /* Slave will stop replication. */
        DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
      }
    }
  }
  DBUG_RETURN(0);
}

static inline int check_completed_operations(NdbTransaction *trans,
                                             const NdbOperation *first,
                                             const NdbOperation *last,
                                             uint *ignore_count)
{
  uint ignores= 0;
  DBUG_ENTER("check_completed_operations");

  if (unlikely(first == 0))
  {
    assert(last == 0);
    DBUG_RETURN(0);
  }

  /*
    Check that all errors are "accepted" errors
  */
  while (true)
  {
    const NdbError &err= first->getNdbError();
    if (err.classification != NdbError::NoError &&
        err.classification != NdbError::ConstraintViolation &&
        err.classification != NdbError::NoDataFound)
    {
      /* All conflict detection etc should be done before commit */
      DBUG_ASSERT((err.code != (int) error_conflict_fn_violation) &&
                  (err.code != (int) error_op_after_refresh_op));
      DBUG_RETURN(err.code);
    }
    if (err.classification != NdbError::NoError)
      ignores++;

    if (first == last)
      break;

    first= trans->getNextCompletedOperation(first);
  }
  if (ignore_count)
    *ignore_count= ignores;
  DBUG_RETURN(0);
}

void
ha_ndbcluster::release_completed_operations(NdbTransaction *trans)
{
  /**
   * mysqld reads/write blobs fully,
   *   which means that it does not keep blobs
   *   open/active over execute, which means
   *   that it should be safe to release anything completed here
   *
   *   i.e don't check for blobs, but just go ahead and release
   */
  trans->releaseCompletedOperations();
  trans->releaseCompletedQueries();
}


static inline
int
execute_no_commit(Thd_ndb *thd_ndb, NdbTransaction *trans,
                  bool ignore_no_key,
                  uint *ignore_count = 0)
{
  DBUG_ENTER("execute_no_commit");
  ha_ndbcluster::release_completed_operations(trans);
  const NdbOperation *first= trans->getFirstDefinedOperation();
  const NdbOperation *last= trans->getLastDefinedOperation();
  thd_ndb->m_execute_count++;
  thd_ndb->m_unsent_bytes= 0;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  int rc= 0;
  do
  {
    if (trans->execute(NdbTransaction::NoCommit,
                       NdbOperation::AO_IgnoreError,
                       thd_ndb->m_force_send))
    {
      rc= -1;
      break;
    }
    if (!ignore_no_key || trans->getNdbError().code == 0)
    {
      rc= trans->getNdbError().code;
      break;
    }

    rc = check_completed_operations_pre_commit(thd_ndb, trans,
                                               first, last,
                                               ignore_count);
  } while (0);

  if (unlikely(thd_ndb->is_slave_thread() &&
               rc != 0))
  {
    g_ndb_slave_state.atTransactionAbort();
  }

  DBUG_PRINT("info", ("execute_no_commit rc is %d", rc));
  DBUG_RETURN(rc);
}


static inline
int
execute_commit(Thd_ndb *thd_ndb, NdbTransaction *trans,
               int force_send, int ignore_error, uint *ignore_count = 0)
{
  DBUG_ENTER("execute_commit");
  NdbOperation::AbortOption ao= NdbOperation::AO_IgnoreError;
  if (thd_ndb->m_unsent_bytes && !ignore_error)
  {
    /*
      We have unsent bytes and cannot ignore error.  Calling execute
      with NdbOperation::AO_IgnoreError will result in possible commit
      of a transaction although there is an error.
    */
    ao= NdbOperation::AbortOnError;
  }
  const NdbOperation *first= trans->getFirstDefinedOperation();
  const NdbOperation *last= trans->getLastDefinedOperation();
  thd_ndb->m_execute_count++;
  thd_ndb->m_unsent_bytes= 0;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  int rc= 0;
  do
  {
    if (trans->execute(NdbTransaction::Commit, ao, force_send))
    {
      rc= -1;
      break;
    }

    if (!ignore_error || trans->getNdbError().code == 0)
    {
      rc= trans->getNdbError().code;
      break;
    }

    rc= check_completed_operations(trans, first, last,
                                   ignore_count);
  } while (0);

  if (likely(rc == 0))
  {
    /* Committed ok, update session GCI, if it's available
     * (Not available for reads, empty transactions etc...)
     */
    Uint64 reportedGCI;
    if (trans->getGCI(&reportedGCI) == 0 &&
        reportedGCI != 0)
    {
      assert(reportedGCI >= thd_ndb->m_last_commit_epoch_session);
      thd_ndb->m_last_commit_epoch_session = reportedGCI;
    }
  }

  if (thd_ndb->is_slave_thread())
  {
    if (likely(rc == 0))
    {
      /* Success */
      g_ndb_slave_state.atTransactionCommit(thd_ndb->m_last_commit_epoch_session);
    }
    else
    {
      g_ndb_slave_state.atTransactionAbort();
    }
  }

  DBUG_PRINT("info", ("execute_commit rc is %d", rc));
  DBUG_RETURN(rc);
}

static inline
int execute_no_commit_ie(Thd_ndb *thd_ndb, NdbTransaction *trans)
{
  DBUG_ENTER("execute_no_commit_ie");
  ha_ndbcluster::release_completed_operations(trans);
  int res= trans->execute(NdbTransaction::NoCommit,
                          NdbOperation::AO_IgnoreError,
                          thd_ndb->m_force_send);
  thd_ndb->m_unsent_bytes= 0;
  thd_ndb->m_execute_count++;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  DBUG_RETURN(res);
}

/*
  Place holder for ha_ndbcluster thread specific data
*/
struct THD_NDB_SHARE {
  const void *key;
  struct Ndb_local_table_statistics stat;
};

Thd_ndb::Thd_ndb(THD* thd) :
  m_thd(thd),
  m_slave_thread(thd->slave_thread),
  options(0),
  trans_options(0),
  global_schema_lock_trans(NULL),
  global_schema_lock_count(0),
  global_schema_lock_error(0),
  schema_locks_count(0),
  m_last_commit_epoch_session(0)
{
  connection= ndb_get_cluster_connection();
  m_connect_count= connection->get_connect_count();
  ndb= new Ndb(connection, "");
  lock_count= 0;
  start_stmt_count= 0;
  save_point_count= 0;
  count= 0;
  trans= NULL;
  m_handler= NULL;
  m_error= false;
  m_unsent_bytes= 0;
  m_execute_count= 0;
  m_scan_count= 0;
  m_pruned_scan_count= 0;
  m_sorted_scan_count= 0;
  m_pushed_queries_defined= 0;
  m_pushed_queries_dropped= 0;
  m_pushed_queries_executed= 0;
  m_pushed_reads= 0;
  memset(m_transaction_no_hint_count, 0, sizeof(m_transaction_no_hint_count));
  memset(m_transaction_hint_count, 0, sizeof(m_transaction_hint_count));

  init_alloc_root(PSI_INSTRUMENT_ME,
                  &m_batch_mem_root, BATCH_FLUSH_SIZE/4, 0);
}

Thd_ndb::~Thd_ndb()
{
  DBUG_ASSERT(global_schema_lock_count == 0);

  if (unlikely(opt_ndb_extra_logging > 1))
  {
    /*
      print some stats about the connection at disconnect
    */
    for (int i= 0; i < MAX_NDB_NODES; i++)
    {
      if (m_transaction_hint_count[i] > 0 ||
          m_transaction_no_hint_count[i] > 0)
      {
        ndb_log_info("tid %u: node[%u] "
                     "transaction_hint=%u, transaction_no_hint=%u",
                     m_thd->thread_id(), i,
                     m_transaction_hint_count[i],
                     m_transaction_no_hint_count[i]);
      }
    }
  }
  if (ndb)
  {
    delete ndb;
    ndb= NULL;
  }
  free_root(&m_batch_mem_root, MYF(0));
}


Ndb *ha_ndbcluster::get_ndb(THD *thd) const
{
  return get_thd_ndb(thd)->ndb;
}

/*
 * manage uncommitted insert/deletes during transactio to get records correct
 */

void ha_ndbcluster::set_rec_per_key()
{
  DBUG_ENTER("ha_ndbcluster::set_rec_per_key");
  /*
    Set up the 'rec_per_key[]' for keys which we have good knowledge
    about the distribution. 'rec_per_key[]' is init'ed to '0' by 
    open_binary_frm(), which is interpreted as 'unknown' by optimizer.
    -> Not setting 'rec_per_key[]' will force the optimizer to use
    its own heuristic to estimate 'records pr. key'.
  */
  for (uint i=0 ; i < table_share->keys ; i++)
  {
    bool is_unique_index= false;
    KEY* key_info= table->key_info + i;
    switch (get_index_type(i))
    {
    case UNIQUE_INDEX:
    case PRIMARY_KEY_INDEX:
    {
      // Index is unique when all 'key_parts' are specified,
      // else distribution is unknown and not specified here.
      is_unique_index= true;
      break;
    }
    case UNIQUE_ORDERED_INDEX:
    case PRIMARY_KEY_ORDERED_INDEX:
      is_unique_index= true;
      // intentional fall thru to logic for ordered index
    case ORDERED_INDEX:
      // 'Records pr. key' are unknown for non-unique indexes.
      // (May change when we get better index statistics.)
    {
      THD *thd= current_thd;
      const bool index_stat_enable= THDVAR(NULL, index_stat_enable) &&
                                    THDVAR(thd, index_stat_enable);
      if (index_stat_enable)
      {
        int err= ndb_index_stat_set_rpk(i);
        if (err != 0 &&
            /* no stats is not unexpected error */
            err != NdbIndexStat::NoIndexStats &&
            /* warning was printed at first error */
            err != NdbIndexStat::MyHasError &&
            /* stats thread aborted request */
            err != NdbIndexStat::MyAbortReq)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANT_GET_STAT, /* pun? */
                              "index stats (RPK) for key %s:"
                              " unexpected error %d",
                              key_info->name, err);
        }
      }
      // no fallback method...
      break;
    }
    default:
      DBUG_ASSERT(false);
    }
    // set rows per key to 1 for complete key given for unique/primary index
    if (is_unique_index)
    {
      key_info->set_records_per_key(key_info->user_defined_key_parts-1, 1.0f);
    }
  }
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::records(ha_rows* num_rows)
{
  DBUG_ENTER("ha_ndbcluster::records");
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      m_table->getTableId(),
                      m_table_info->no_uncommitted_rows_count));

  int error = update_stats(table->in_use, 1);
  if (error != 0)
  {
    *num_rows = HA_POS_ERROR;
    DBUG_RETURN(error);
  }

  *num_rows = stats.records;
  DBUG_RETURN(0);
}

void ha_ndbcluster::no_uncommitted_rows_execute_failure()
{
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_execute_failure");
  get_thd_ndb(current_thd)->m_error= true;
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_update(int c)
{
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_update");
  struct Ndb_local_table_statistics *local_info= m_table_info;
  local_info->no_uncommitted_rows_count+= c;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      m_table->getTableId(),
                      local_info->no_uncommitted_rows_count));
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::ndb_err(NdbTransaction *trans)
{
  THD *thd= current_thd;
  int res;
  NdbError err= trans->getNdbError();
  DBUG_ENTER("ndb_err");

  switch (err.classification) {
  case NdbError::SchemaError:
  {
    // TODO perhaps we need to do more here, invalidate also in the cache
    m_table->setStatusInvalid();
    /* Close other open handlers not used by any thread */
    ndb_tdc_close_cached_table(thd, m_dbname, m_tabname);
    break;
  }
  default:
    break;
  }
  res= ndb_to_mysql_error(&err);
  DBUG_PRINT("info", ("transformed ndbcluster error %d to mysql error %d", 
                      err.code, res));
  if (res == HA_ERR_FOUND_DUPP_KEY)
  {
    char *error_data= err.details;
    uint dupkey= MAX_KEY;

    for (uint i= 0; i < MAX_KEY; i++)
    {
      if (m_index[i].type == UNIQUE_INDEX || 
          m_index[i].type == UNIQUE_ORDERED_INDEX)
      {
        const NDBINDEX *unique_index=
          (const NDBINDEX *) m_index[i].unique_index;
        if (unique_index && UintPtr(unique_index->getObjectId()) == UintPtr(error_data))
        {
          dupkey= i;
          break;
        }
      }
    }
    if (m_rows_to_insert == 1)
    {
      /*
	We can only distinguish between primary and non-primary
	violations here, so we need to return MAX_KEY for non-primary
	to signal that key is unknown
      */
      m_dupkey= err.code == 630 ? table_share->primary_key : dupkey; 
    }
    else
    {
      /* We are batching inserts, offending key is not available */
      m_dupkey= (uint) -1;
    }
  }
  DBUG_RETURN(res);
}


extern bool
ndb_fk_util_generate_constraint_string(THD* thd, Ndb *ndb,
                                       const NdbDictionary::ForeignKey &fk,
                                       const int child_tab_id,
                                       const bool print_mock_table_names,
                                       String &fk_string);


/**
  Generate error messages when requested by the caller.
  Fetches the error description from NdbError and print it in the caller's
  buffer. This function also additionally handles HA_ROW_REF fk errors.

  @param    error             The error code sent by the caller.
  @param    buf               String buffer to print the error message.

  @retval   true              if the error is permanent
            false             if its temporary
*/

bool ha_ndbcluster::get_error_message(int error,
                                      String *buf)
{
  DBUG_ENTER("ha_ndbcluster::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  Ndb *ndb= check_ndb_in_thd(current_thd);
  if (!ndb)
    DBUG_RETURN(false);

  bool temporary = false;

  if(unlikely(error == HA_ERR_NO_REFERENCED_ROW ||
              error == HA_ERR_ROW_IS_REFERENCED))
  {
    /* Error message to be generated from NdbError in latest trans or dict */
    Thd_ndb *thd_ndb = get_thd_ndb(current_thd);
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    NdbError err;
    if (thd_ndb->trans != NULL)
    {
      err = thd_ndb->trans->getNdbError();
    }
    else
    {
      //Drop table failure. get error from dictionary.
      err = dict->getNdbError();
      DBUG_ASSERT(err.code == 21080);
    }
    temporary= (err.status==NdbError::TemporaryError);

    String fk_string;
    {
      /* copy default error message to be used on failure */
      const char* unknown_fk = "Unknown FK Constraint";
      buf->copy(unknown_fk, (uint32)strlen(unknown_fk), &my_charset_bin);
    }

    /* fk name of format parent_id/child_id/fk_name */
    char fully_qualified_fk_name[MAX_ATTR_NAME_SIZE +
                                 (2*MAX_INT_WIDTH) + 3];
    /* get the fully qualified FK name from ndb using getNdbErrorDetail */
    if (ndb->getNdbErrorDetail(err, &fully_qualified_fk_name[0],
                               sizeof(fully_qualified_fk_name)) == NULL)
    {
      DBUG_ASSERT(false);
      ndb_to_mysql_error(&dict->getNdbError());
      DBUG_RETURN(temporary);
    }

    /* fetch the foreign key */
    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, fully_qualified_fk_name) != 0)
    {
      DBUG_ASSERT(false);
      ndb_to_mysql_error(&dict->getNdbError());
      DBUG_RETURN(temporary);
    }

    /* generate constraint string from fk object */
    if(!ndb_fk_util_generate_constraint_string(current_thd, ndb,
                                               fk, 0, false,
                                               fk_string))
    {
      DBUG_ASSERT(false);
      DBUG_RETURN(temporary);
    }

    /* fk found and string has been generated. set the buf */
    buf->copy(fk_string);
    DBUG_RETURN(temporary);
  }
  else
  {
    /* NdbError code. Fetch error description from ndb */
    const NdbError err= ndb->getNdbError(error);
    temporary= err.status==NdbError::TemporaryError;
    buf->set(err.message, (uint32)strlen(err.message), &my_charset_bin);
  }

  DBUG_PRINT("exit", ("message: %s, temporary: %d", buf->ptr(), temporary));
  DBUG_RETURN(temporary);
}


/*
  field_used_length() returns the number of bytes actually used to
  store the data of the field. So for a varstring it includes both
  length byte(s) and string data, and anything after data_length()
  bytes are unused.
*/
static
uint32 field_used_length(const Field* field)
{
 if (field->type() == MYSQL_TYPE_VARCHAR)
 {
   const Field_varstring* f = static_cast<const Field_varstring*>(field);
   return f->length_bytes + const_cast<Field_varstring*>(f)->data_length();
                            // ^ no 'data_length() const'
 }
 return field->pack_length();
}


/**
  Check if MySQL field type forces var part in ndb storage
*/
static bool field_type_forces_var_part(enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    return true;
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_JSON:
  case MYSQL_TYPE_GEOMETRY:
    return false;
  default:
    return false;
  }
}

/*
  Return a generic buffer that will remain valid until after next execute.

  The memory is freed by the first call to add_row_check_if_batch_full_size()
  following any execute() call. The intention is that the memory is associated
  with one batch of operations during batched slave updates.

  Note in particular that using get_buffer() / copy_row_to_buffer() separately
  from add_row_check_if_batch_full_size() could make meory usage grow without
  limit, and that this sequence:

    execute()
    get_buffer() / copy_row_to_buffer()
    add_row_check_if_batch_full_size()
    ...
    execute()

  will free the memory already at add_row_check_if_batch_full_size() time, it
  will not remain valid until the second execute().
*/
uchar *
ha_ndbcluster::get_buffer(Thd_ndb *thd_ndb, uint size)
{
  return (uchar*)alloc_root(&(thd_ndb->m_batch_mem_root), size);
}

uchar *
ha_ndbcluster::copy_row_to_buffer(Thd_ndb *thd_ndb, const uchar *record)
{
  uchar *row= get_buffer(thd_ndb, table->s->stored_rec_length);
  if (unlikely(!row))
    return NULL;
  memcpy(row, record, table->s->stored_rec_length);
  return row;
}

/**
 * findBlobError
 * This method attempts to find an error in the hierarchy of runtime
 * NDBAPI objects from Blob up to transaction.
 * It will return -1 if no error is found, 0 if an error is found.
 */
int findBlobError(NdbError& error, NdbBlob* pBlob)
{
  error= pBlob->getNdbError();
  if (error.code != 0)
    return 0;
  
  const NdbOperation* pOp= pBlob->getNdbOperation();
  error= pOp->getNdbError();
  if (error.code != 0)
    return 0;
  
  NdbTransaction* pTrans= pOp->getNdbTransaction();
  error= pTrans->getNdbError();
  if (error.code != 0)
    return 0;
  
  /* No error on any of the objects */
  return -1;
}


/* 
 Calculate the length of the blob/text after applying mysql limits
 on blob/text sizes. If the blob contains multi-byte characters, the length is 
 reduced till the end of the last well-formed char, so that data is not truncated 
 in the middle of a multi-byte char.
*/
static uint64 calc_ndb_blob_len(const CHARSET_INFO *cs, uchar *blob_ptr,
                                uint64 maxlen)
{
  int errors = 0;
  
  const char *begin = (const char*) blob_ptr;
  const char *end = (const char*) (blob_ptr+maxlen);
  
  // avoid truncation in the middle of a multi-byte character by 
  // stopping at end of last well-formed character before max length
  uint32 numchars = cs->cset->numchars(cs, begin, end);
  uint64 len64 = cs->cset->well_formed_len(cs, begin, end, numchars, &errors);
  assert(len64 <= maxlen);

  return len64; 
}


int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg)
{
  ha_ndbcluster *ha= (ha_ndbcluster *)arg;
  DBUG_ENTER("g_get_ndb_blobs_value");
  DBUG_PRINT("info", ("destination row: %p", ha->m_blob_destination_record));

  if (ha->m_blob_counter == 0)   /* Reset total size at start of row */
    ha->m_blobs_row_total_size= 0;

  /* Count the total length needed for blob data. */
  int isNull;
  if (ndb_blob->getNull(isNull) != 0)
    ERR_RETURN(ndb_blob->getNdbError());
  if (isNull == 0) {
    Uint64 len64= 0;
    if (ndb_blob->getLength(len64) != 0)
      ERR_RETURN(ndb_blob->getNdbError());
    /* Align to Uint64. */
    ha->m_blobs_row_total_size+= (len64 + 7) & ~((Uint64)7);
    if (ha->m_blobs_row_total_size > 0xffffffff)
    {
      DBUG_ASSERT(false);
      DBUG_RETURN(-1);
    }
    DBUG_PRINT("info", ("Blob number %d needs size %llu, total buffer reqt. now %llu",
                        ha->m_blob_counter,
                        len64,
                        ha->m_blobs_row_total_size));
  }
  ha->m_blob_counter++;

  /*
    Wait until all blobs in this row are active, so we can allocate
    and use a common buffer containing all.
  */
  if (ha->m_blob_counter < ha->m_blob_expected_count_per_row)
    DBUG_RETURN(0);

  /* Reset blob counter for next row (scan scenario) */
  ha->m_blob_counter= 0;

  /* Re-allocate bigger blob buffer for this row if necessary. */
  if (ha->m_blobs_row_total_size > ha->m_blobs_buffer_size)
  {
    my_free(ha->m_blobs_buffer);
    DBUG_PRINT("info", ("allocate blobs buffer size %u",
                        (uint32)(ha->m_blobs_row_total_size)));
    /* Windows compiler complains about my_malloc on non-size_t
     * validate mapping from Uint64 to size_t
     */
    if(((size_t)ha->m_blobs_row_total_size) != ha->m_blobs_row_total_size)
    {
      ha->m_blobs_buffer= NULL;
      ha->m_blobs_buffer_size= 0;
      DBUG_RETURN(-1);
    }

    ha->m_blobs_buffer=
      (uchar*) my_malloc(PSI_INSTRUMENT_ME,
                         (size_t) ha->m_blobs_row_total_size, MYF(MY_WME));
    if (ha->m_blobs_buffer == NULL)
    {
      ha->m_blobs_buffer_size= 0;
      DBUG_RETURN(-1);
    }
    ha->m_blobs_buffer_size= ha->m_blobs_row_total_size;
  }

  /*
    Now read all blob data.
    If we know the destination mysqld row, we also set the blob null bit and
    pointer/length (if not, it will be done instead in unpack_record()).
  */
  uint32 offset= 0;
  for (uint i= 0; i < ha->table->s->fields; i++)
  {
    Field *field= ha->table->field[i];
    if (! ((field->flags & BLOB_FLAG) && field->stored_in_db))
      continue;
    NdbValue value= ha->m_value[i];
    if (value.blob == NULL)
    {
      DBUG_PRINT("info",("[%u] skipped", i));
      continue;
    }
    Field_blob *field_blob= (Field_blob *)field;
    NdbBlob *ndb_blob= value.blob;
    int isNull;
    if (ndb_blob->getNull(isNull) != 0)
      ERR_RETURN(ndb_blob->getNdbError());
    if (isNull == 0) {
      Uint64 len64= 0;
      if (ndb_blob->getLength(len64) != 0)
        ERR_RETURN(ndb_blob->getNdbError());
      DBUG_ASSERT(len64 < 0xffffffff);
      uchar *buf= ha->m_blobs_buffer + offset;
	  uint32 len= (uint32)(ha->m_blobs_buffer_size - offset);
      if (ndb_blob->readData(buf, len) != 0)
      {
        NdbError err;
        if (findBlobError(err, ndb_blob) == 0)
        {
          ERR_RETURN(err);
        }
        else
        {
          /* Should always have some error code set */
          assert(err.code != 0);
          ERR_RETURN(err);
        }
      }   
      DBUG_PRINT("info", ("[%u] offset: %u  buf: 0x%lx  len=%u",
                          i, offset, (long) buf, len));
      DBUG_ASSERT(len == len64);
      if (ha->m_blob_destination_record)
      {
        my_ptrdiff_t ptrdiff=
          ha->m_blob_destination_record - ha->table->record[0];
        field_blob->move_field_offset(ptrdiff);

        if(len > field_blob->max_data_length())
        {
          len = calc_ndb_blob_len(field_blob->charset(),
                                  buf, field_blob->max_data_length());

          // push a warning
          push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                      WARN_DATA_TRUNCATED,
                      "Truncated value from TEXT field \'%s\'", field_blob->field_name);
        }

        field_blob->set_ptr(len, buf);
        field_blob->set_notnull();
        field_blob->move_field_offset(-ptrdiff);
      }
      offset+= Uint32((len64 + 7) & ~((Uint64)7));
    }
    else if (ha->m_blob_destination_record)
    {
      /* Have to set length even in this case. */
      my_ptrdiff_t ptrdiff=
        ha->m_blob_destination_record - ha->table->record[0];
      uchar *buf= ha->m_blobs_buffer + offset;
      field_blob->move_field_offset(ptrdiff);
      field_blob->set_ptr((uint32)0, buf);
      field_blob->set_null();
      field_blob->move_field_offset(-ptrdiff);
      DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
    }
  }

  if (!ha->m_active_cursor)
  {
    /* Non-scan, Blob reads have been issued
     * execute them and then close the Blob 
     * handles
     */
    for (uint i= 0; i < ha->table->s->fields; i++)
    {
      Field *field= ha->table->field[i];
      if (! ((field->flags & BLOB_FLAG) && field->stored_in_db))
        continue;
      NdbValue value= ha->m_value[i];
      if (value.blob == NULL)
      {
        DBUG_PRINT("info",("[%u] skipped", i));
        continue;
      }
      NdbBlob *ndb_blob= value.blob;
    
      assert(ndb_blob->getState() == NdbBlob::Active);

      /* Call close() with execPendingBlobOps == true
       * For LM_CommittedRead access, this will enqueue
       * an unlock operation, which the Blob framework 
       * code invoking this callback will execute before
       * returning control to the caller of execute()
       */
      if (ndb_blob->close(true) != 0)
      {
        ERR_RETURN(ndb_blob->getNdbError());
      }
    }
  }

  DBUG_RETURN(0);
}

/*
  Request reading of blob values.

  If dst_record is specified, the blob null bit, pointer, and length will be
  set in that record. Otherwise they must be set later by calling
  unpack_record().
*/
int
ha_ndbcluster::get_blob_values(const NdbOperation *ndb_op, uchar *dst_record,
                               const MY_BITMAP *bitmap)
{
  uint i;
  DBUG_ENTER("ha_ndbcluster::get_blob_values");

  m_blob_counter= 0;
  m_blob_expected_count_per_row= 0;
  m_blob_destination_record= dst_record;
  m_blobs_row_total_size= 0;
  ndb_op->getNdbTransaction()->
    setMaxPendingBlobReadBytes(THDVAR(current_thd, blob_read_batch_bytes));

  for (i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (! ((field->flags & BLOB_FLAG) && field->stored_in_db))
      continue;

    DBUG_PRINT("info", ("fieldnr=%d", i));
    NdbBlob *ndb_blob;
    if (bitmap_is_set(bitmap, i))
    {
      if ((ndb_blob= m_table_map->getBlobHandle(ndb_op, i)) == NULL ||
          ndb_blob->setActiveHook(g_get_ndb_blobs_value, this) != 0)
        DBUG_RETURN(1);
      m_blob_expected_count_per_row++;
    }
    else
      ndb_blob= NULL;

    m_value[i].blob= ndb_blob;
  }

  DBUG_RETURN(0);
}

int
ha_ndbcluster::set_blob_values(const NdbOperation *ndb_op,
                               my_ptrdiff_t row_offset, const MY_BITMAP *bitmap,
                               uint *set_count, bool batch)
{
  uint field_no;
  uint *blob_index, *blob_index_end;
  int res= 0;
  DBUG_ENTER("ha_ndbcluster::set_blob_values");

  *set_count= 0;

  if (table_share->blob_fields == 0)
    DBUG_RETURN(0);

  ndb_op->getNdbTransaction()->
    setMaxPendingBlobWriteBytes(THDVAR(current_thd, blob_write_batch_bytes));
  blob_index= table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    field_no= *blob_index;
    /* A NULL bitmap sets all blobs. */
    if (bitmap && !bitmap_is_set(bitmap, field_no))
      continue;
    Field *field= table->field[field_no];
    if(field->is_virtual_gcol())
      continue;

    NdbBlob *ndb_blob= m_table_map->getBlobHandle(ndb_op, field_no);
    if (ndb_blob == NULL)
      ERR_RETURN(ndb_op->getNdbError());
    if (field->is_real_null(row_offset))
    {
      DBUG_PRINT("info", ("Setting Blob %d to NULL", field_no));
      if (ndb_blob->setNull() != 0)
        ERR_RETURN(ndb_op->getNdbError());
    }
    else
    {
      Field_blob *field_blob= (Field_blob *)field;

      // Get length and pointer to data
      const uchar *field_ptr= field->ptr + row_offset;
      uint32 blob_len= field_blob->get_length(field_ptr);
      uchar* blob_ptr= NULL;
      field_blob->get_ptr(&blob_ptr);

      // Looks like NULL ptr signals length 0 blob
      if (blob_ptr == NULL) {
        DBUG_ASSERT(blob_len == 0);
        blob_ptr= (uchar*)"";
      }

      DBUG_PRINT("value", ("set blob ptr: 0x%lx  len: %u",
                           (long) blob_ptr, blob_len));
      DBUG_DUMP("value", blob_ptr, MIN(blob_len, 26));

      /*
        NdbBlob requires the data pointer to remain valid until execute() time.
        So when batching, we need to copy the value to a temporary buffer.
      */
      if (batch && blob_len > 0)
      {
        uchar *tmp_buf= get_buffer(m_thd_ndb, blob_len);
        if (!tmp_buf)
          DBUG_RETURN(HA_ERR_OUT_OF_MEM);
        memcpy(tmp_buf, blob_ptr, blob_len);
        blob_ptr= tmp_buf;
      }
      res= ndb_blob->setValue((char*)blob_ptr, blob_len);
      if (res != 0)
        ERR_RETURN(ndb_op->getNdbError());
    }

    ++(*set_count);
  } while (++blob_index != blob_index_end);

  DBUG_RETURN(res);
}


/**
  Check if any set or get of blob value in current query.
*/

bool ha_ndbcluster::uses_blob_value(const MY_BITMAP *bitmap) const
{
  uint *blob_index, *blob_index_end;
  if (table_share->blob_fields == 0)
    return false;

  blob_index=     table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    Field *field= table->field[*blob_index];
    if (bitmap_is_set(bitmap, field->field_index) && ! field->is_virtual_gcol())
      return true;
  } while (++blob_index != blob_index_end);
  return false;
}

void ha_ndbcluster::release_blobs_buffer()
{
  DBUG_ENTER("releaseBlobsBuffer");
  if (m_blobs_buffer_size > 0)
  {
    DBUG_PRINT("info", ("Deleting blobs buffer, size %llu", m_blobs_buffer_size));
    my_free(m_blobs_buffer);
    m_blobs_buffer= 0;
    m_blobs_row_total_size= 0;
    m_blobs_buffer_size= 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Does type support a default value?
*/
static bool
type_supports_default_value(enum_field_types mysql_type)
{
  bool ret = (mysql_type != MYSQL_TYPE_BLOB &&
              mysql_type != MYSQL_TYPE_TINY_BLOB &&
              mysql_type != MYSQL_TYPE_MEDIUM_BLOB &&
              mysql_type != MYSQL_TYPE_LONG_BLOB &&
              mysql_type != MYSQL_TYPE_JSON &&
              mysql_type != MYSQL_TYPE_GEOMETRY);

  return ret;
}

/**
   Check that Ndb data dictionary has the same default values
   as MySQLD for the current table.
   Called as part of a DBUG check as part of table open
   
   Returns
     0  - Defaults are ok
     -1 - Some default(s) are bad
*/
int ha_ndbcluster::check_default_values(const NDBTAB* ndbtab)
{
  /* Debug only method for checking table defaults aligned
     between MySQLD and Ndb
  */
  bool defaults_aligned= true;

  if (ndbtab->hasDefaultValues())
  {
    /* Ndb supports native defaults for non-pk columns */
    my_bitmap_map *old_map= tmp_use_all_columns(table, table->read_set);

    for (uint f=0; f < table_share->fields; f++)
    {
      Field* field= table->field[f]; // Use Field struct from MySQLD table rep
      if(! field->stored_in_db)
        continue;

      const NdbDictionary::Column* ndbCol= m_table_map->getColumn(field->field_index);

      if ((! (field->flags & (PRI_KEY_FLAG |
                              NO_DEFAULT_VALUE_FLAG))) &&
          type_supports_default_value(field->real_type()))
      {
        /* We expect Ndb to have a native default for this
         * column
         */
        my_ptrdiff_t src_offset= table_share->default_values - 
          field->table->record[0];

        /* Move field by offset to refer to default value */
        field->move_field_offset(src_offset);
        
        const uchar* ndb_default= (const uchar*) ndbCol->getDefaultValue();

        if (ndb_default == NULL)
          /* MySQLD default must also be NULL */
          defaults_aligned= field->is_null();
        else
        {
          if (field->type() != MYSQL_TYPE_BIT)
          {
            defaults_aligned= (0 == field->cmp(ndb_default));
          }
          else
          {
            longlong value= (static_cast<Field_bit*>(field))->val_int();
            /* Map to NdbApi format - two Uint32s */
            Uint32 out[2];
            out[0] = 0;
            out[1] = 0;
            for (int b=0; b < 64; b++)
            {
              out[b >> 5] |= (value & 1) << (b & 31);
              
              value= value >> 1;
            }
            Uint32 defaultLen = field_used_length(field);
            defaultLen = ((defaultLen + 3) & ~(Uint32)0x7);
            defaults_aligned= (0 == memcmp(ndb_default, 
                                           out, 
                                           defaultLen));
          }
        }
        
        field->move_field_offset(-src_offset);

        if (unlikely(!defaults_aligned))
        {
          ndb_log_error("Internal error, Default values differ "
                        "for column %u, ndb_default: %d",
                        field->field_index, ndb_default != NULL);
        }
      }
      else
      {
        /* We don't expect Ndb to have a native default for this column */
        if (unlikely(ndbCol->getDefaultValue() != NULL))
        {
          /* Didn't expect that */
          ndb_log_error("Internal error, Column %u has native "
                        "default, but shouldn't. Flags=%u, type=%u",
                        field->field_index, field->flags,
                        field->real_type());
          defaults_aligned= false;
        }
      }
      if (unlikely(!defaults_aligned))
      {
        // Dump field
        ndb_log_error("field[ name: '%s', type: %u, real_type: %u, "
                      "flags: 0x%x, is_null: %d]",
                      field->field_name, field->type(), field->real_type(),
                      field->flags, field->is_null());
        // Dump ndbCol
        ndb_log_error("ndbCol[name: '%s', type: %u, column_no: %d, "
                      "nullable: %d]",
                      ndbCol->getName(), ndbCol->getType(),
                      ndbCol->getColumnNo(), ndbCol->getNullable());
        break;
      }
    }
    tmp_restore_column_map(table->read_set, old_map);
  }

  return (defaults_aligned? 0: -1);
}


int ha_ndbcluster::get_metadata(THD *thd, const dd::Table* table_def)
{
  Ndb *ndb= get_thd_ndb(thd)->ndb;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  DBUG_ASSERT(m_table == NULL);
  DBUG_ASSERT(m_table_info == NULL);

  int object_id, object_version;
  if (!ndb_dd_table_get_object_id_and_version(table_def,
                                              object_id, object_version))
  {
    DBUG_PRINT("error", ("Could not extract object_id and object_version "
                         "from table definition"));
    DBUG_RETURN(1);
  }

  ndb->setDatabaseName(m_dbname);
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  const NDBTAB *tab = ndbtab_g.get_table();
  if (tab == nullptr)
  {
    ERR_RETURN(dict->getNdbError());
  }

  // Check that the id and version from DD
  // matches the id and version of the NDB table
  const int ndb_object_id = tab->getObjectId();
  const int ndb_object_version = tab->getObjectVersion();
  if (ndb_object_id != object_id ||
      ndb_object_version != object_version)
  {
    DBUG_PRINT("error", ("Table id or version mismatch"));
    DBUG_PRINT("error", ("NDB table id: %u, version: %u",
                         ndb_object_id, ndb_object_version));
    DBUG_PRINT("error", ("DD table id: %u, version: %u",
                         object_id, object_version));

    ndb_log_verbose(10,
                    "Table id or version mismatch for table '%s.%s', "
                    "[%d, %d] != [%d, %d]",
                    m_dbname, m_tabname,
                    object_id, object_version,
                    ndb_object_id, ndb_object_version);

    ndbtab_g.invalidate();

    // When returning HA_ERR_TABLE_DEF_CHANGED from handler::open()
    // the caller is intended to call ha_discover() in order to let
    // the engine install the correct table definition in the
    // data dictionary, then the open() will be retried and presumably
    // the table definition will be correct
    DBUG_RETURN(HA_ERR_TABLE_DEF_CHANGED);
  }

  // Check that NDB and DD metadata matches
  DBUG_ASSERT(Ndb_metadata::compare(thd, tab, table_def));

  if (DBUG_EVALUATE_IF("ndb_get_metadata_fail", true, false))
  {
    fprintf(stderr, "ndb_get_metadata_fail\n");
    DBUG_RETURN(HA_ERR_TABLE_DEF_CHANGED);
  }

  // Create field to column map when table is opened
  m_table_map = new Ndb_table_map(table, tab);

  /* Now check that any Ndb native defaults are aligned 
     with MySQLD defaults
  */
  DBUG_ASSERT(check_default_values(tab) == 0);

  DBUG_PRINT("info", ("fetched table %s", tab->getName()));
  m_table= tab;

  ndb_bitmap_init(m_bitmap, m_bitmap_buf, table_share->fields);

  int error = 0;
  if (table_share->primary_key == MAX_KEY)
  {
    /* Hidden primary key. */
    if ((error= add_hidden_pk_ndb_record(dict)) != 0)
      goto err;
  }

  if ((error= add_table_ndb_record(dict)) != 0)
    goto err;

  /*
    Approx. write size in bytes over transporter
  */
  m_bytes_per_write= 12 + tab->getRowSizeInBytes() + 4 * tab->getNoOfColumns();

  /* Open indexes */
  if ((error= open_indexes(ndb, table)) != 0)
    goto err;

  /* Read foreign keys where this table is child or parent */
  if ((error= get_fk_data(thd, ndb)) != 0)
    goto err;

  /*
    Backward compatibility for tables created without tablespace
    in .frm => read tablespace setting from engine
  */
  if (table_share->mysql_version < 50120 &&
      !table_share->tablespace /* safety */)
  {
    Uint32 id;
    if (tab->getTablespace(&id))
    {
      NdbDictionary::Tablespace ts= dict->getTablespace(id);
      NdbError ndberr= dict->getNdbError();
      if (ndberr.classification == NdbError::NoError)
      {
        const char *tablespace= ts.getName();
        const size_t tablespace_len= strlen(tablespace);
        if (tablespace_len != 0)
        {
          DBUG_PRINT("info", ("Found tablespace '%s'", tablespace));
          table_share->tablespace= strmake_root(&table_share->mem_root,
                                                tablespace,
                                                tablespace_len);
        }
      }
    }
  }

  ndbtab_g.release();

  DBUG_RETURN(0);

err:
  // Function failed, release all resources allocated by this function
  // before returning
  release_indexes(dict, 1 /* invalidate */);

  // Release NdbRecord's allocated for the table
  if (m_ndb_record != NULL)
  {
    dict->releaseRecord(m_ndb_record);
    m_ndb_record= NULL;
  }
  if (m_ndb_hidden_key_record != NULL)
  {
    dict->releaseRecord(m_ndb_hidden_key_record);
    m_ndb_hidden_key_record= NULL;
  }

  ndbtab_g.invalidate();
  m_table= NULL;
  DBUG_RETURN(error);
}

static int fix_unique_index_attr_order(NDB_INDEX_DATA &data,
                                       const NDBINDEX *index,
                                       KEY *key_info)
{
  DBUG_ENTER("fix_unique_index_attr_order");
  unsigned sz= index->getNoOfIndexColumns();

  if (data.unique_index_attrid_map)
    my_free(data.unique_index_attrid_map);
  data.unique_index_attrid_map= (uchar*)my_malloc(PSI_INSTRUMENT_ME, sz,MYF(MY_WME));
  if (data.unique_index_attrid_map == 0)
  {
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
  DBUG_ASSERT(key_info->user_defined_key_parts == sz);
  for (unsigned i= 0; key_part != end; key_part++, i++) 
  {
    const char *field_name= key_part->field->field_name;
#ifndef DBUG_OFF
   data.unique_index_attrid_map[i]= 255;
#endif
    for (unsigned j= 0; j < sz; j++)
    {
      const NDBCOL *c= index->getColumn(j);
      if (strcmp(field_name, c->getName()) == 0)
      {
        data.unique_index_attrid_map[i]= j;
        break;
      }
    }
    DBUG_ASSERT(data.unique_index_attrid_map[i] != 255);
  }
  DBUG_RETURN(0);
}

/*
  Create all the indexes for a table.
  If any index should fail to be created,
  the error is returned immediately
*/
int ha_ndbcluster::create_indexes(THD *thd, TABLE *tab) const
{
  int error= 0;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::create_indexes");

  for (uint i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    const char* index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    error= create_index(thd, index_name, key_info, idx_type);
    if (error)
    {
      DBUG_PRINT("error", ("Failed to create index %u", i));
      break;
    }
  }

  DBUG_RETURN(error);
}

static void ndb_init_index(NDB_INDEX_DATA &data)
{
  data.type= UNDEFINED_INDEX;
  data.status= NDB_INDEX_DATA::UNDEFINED;
  data.unique_index= NULL;
  data.index= NULL;
  data.unique_index_attrid_map= NULL;
  data.ndb_record_key= NULL;
  data.ndb_unique_record_key= NULL;
  data.ndb_unique_record_row= NULL;
}

static void ndb_clear_index(NDBDICT *dict, NDB_INDEX_DATA &data)
{
  if (data.unique_index_attrid_map)
  {
    my_free(data.unique_index_attrid_map);
  }
  if (data.ndb_unique_record_key)
    dict->releaseRecord(data.ndb_unique_record_key);
  if (data.ndb_unique_record_row)
    dict->releaseRecord(data.ndb_unique_record_row);
  if (data.ndb_record_key)
    dict->releaseRecord(data.ndb_record_key);
  ndb_init_index(data);
}

static
void ndb_protect_char(const char* from, char* to, uint to_length, char protect)
{
  uint fpos= 0, tpos= 0;

  while(from[fpos] != '\0' && tpos < to_length - 1)
  {
    if (from[fpos] == protect)
    {
      int len= 0;
      to[tpos++]= '@';
      if(tpos < to_length - 5)
      {
        len= sprintf(to+tpos, "00%u", (uint) protect);
        tpos+= len;
      }
    }
    else
    {
      to[tpos++]= from[fpos];
    }
    fpos++;
  }
  to[tpos]= '\0';
}

/*
  Associate a direct reference to an index handle
  with an index (for faster access)
 */
int ha_ndbcluster::add_index_handle(NDBDICT *dict, KEY *key_info,
                                    const char *key_name, uint index_no)
{
  char index_name[FN_LEN + 1];
  int error= 0;

  const NDB_INDEX_TYPE idx_type= get_index_type_from_table(index_no);
  m_index[index_no].type= idx_type;
  DBUG_ENTER("ha_ndbcluster::add_index_handle");
  DBUG_PRINT("enter", ("table %s", m_tabname));
  
  ndb_protect_char(key_name, index_name, sizeof(index_name) - 1, '/');
  if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX)
  {
    DBUG_PRINT("info", ("Get handle to index %s", index_name));
    const NDBINDEX *index= dict->getIndexGlobal(index_name, *m_table);
    if (!index)
      ERR_RETURN(dict->getNdbError());
    DBUG_PRINT("info", ("index: 0x%lx  id: %d  version: %d.%d  status: %d",
                        (long) index,
                        index->getObjectId(),
                        index->getObjectVersion() & 0xFFFFFF,
                        index->getObjectVersion() >> 24,
                        index->getObjectStatus()));
    DBUG_ASSERT(index->getObjectStatus() ==
                NdbDictionary::Object::Retrieved);
    m_index[index_no].index= index;
  }

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    char unique_index_name[FN_LEN + 1];
    static const char* unique_suffix= "$unique";
    m_has_unique_index= true;
    strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
    const NDBINDEX *index =
        dict->getIndexGlobal(unique_index_name, *m_table);
    if (!index)
      ERR_RETURN(dict->getNdbError());
    DBUG_PRINT("info", ("index: 0x%lx  id: %d  version: %d.%d  status: %d",
                        (long) index,
                        index->getObjectId(),
                        index->getObjectVersion() & 0xFFFFFF,
                        index->getObjectVersion() >> 24,
                        index->getObjectStatus()));
    DBUG_ASSERT(index->getObjectStatus() ==
                NdbDictionary::Object::Retrieved);
    m_index[index_no].unique_index= index;
    error= fix_unique_index_attr_order(m_index[index_no], index, key_info);
  }

  if (!error)
    error= add_index_ndb_record(dict, key_info, index_no);

  if (!error)
    m_index[index_no].status= NDB_INDEX_DATA::ACTIVE;
  
  DBUG_RETURN(error);
}

/*
  We use this function to convert null bit masks, as found in class Field,
  to bit numbers, as used in NdbRecord.
*/
static uint
null_bit_mask_to_bit_number(uchar bit_mask)
{
  switch (bit_mask)
  {
    case  0x1: return 0;
    case  0x2: return 1;
    case  0x4: return 2;
    case  0x8: return 3;
    case 0x10: return 4;
    case 0x20: return 5;
    case 0x40: return 6;
    case 0x80: return 7;
    default:
      DBUG_ASSERT(false);
      return 0;
  }
}

static void
ndb_set_record_specification(uint field_no,
                             NdbDictionary::RecordSpecification *spec,
                             const TABLE *table,
                             const NdbDictionary::Column *ndb_column)
{
  DBUG_ENTER("ndb_set_record_specification");
  DBUG_ASSERT(ndb_column);
  spec->column= ndb_column;
  spec->offset= Uint32(table->field[field_no]->ptr - table->record[0]);
  if (table->field[field_no]->real_maybe_null())
  {
    spec->nullbit_byte_offset=
      Uint32(table->field[field_no]->null_offset());
    spec->nullbit_bit_in_byte=
      null_bit_mask_to_bit_number(table->field[field_no]->null_bit);
  }
  else if (table->field[field_no]->type() == MYSQL_TYPE_BIT)
  {
    /* We need to store the position of the overflow bits. */
    const Field_bit* field_bit= static_cast<Field_bit*>(table->field[field_no]);
    spec->nullbit_byte_offset=
      Uint32(field_bit->bit_ptr - table->record[0]);
    spec->nullbit_bit_in_byte= field_bit->bit_ofs;
  }
  else
  {
    spec->nullbit_byte_offset= 0;
    spec->nullbit_bit_in_byte= 0;
  }
  spec->column_flags= 0;
  if (table->field[field_no]->type() == MYSQL_TYPE_STRING &&
      table->field[field_no]->pack_length() == 0)
  {
    /*
      This is CHAR(0), which we represent as
      a nullable BIT(1) column where we ignore the data bit
    */
    spec->column_flags |=
        NdbDictionary::RecordSpecification::BitColMapsNullBitOnly;
  }
  DBUG_PRINT("info",
             ("%s.%s field: %d, col: %d, offset: %d, null bit: %d",
             table->s->table_name.str, ndb_column->getName(),
             field_no, ndb_column->getColumnNo(),
             spec->offset,
             (8 * spec->nullbit_byte_offset) + spec->nullbit_bit_in_byte));
  DBUG_VOID_RETURN;
}

int
ha_ndbcluster::add_table_ndb_record(NDBDICT *dict)
{
  DBUG_ENTER("ha_ndbcluster::add_table_ndb_record()");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;
  uint fieldId, colId;

  for (fieldId= 0, colId= 0; fieldId < table_share->fields; fieldId++)
  {
    if(table->field[fieldId]->stored_in_db)
    {
      ndb_set_record_specification(fieldId, &spec[colId], table,
                                   m_table->getColumn(colId));
      colId++;
    }
  }

  rec= dict->createRecord(m_table, spec, colId, sizeof(spec[0]),
                          NdbDictionary::RecMysqldBitfield |
                          NdbDictionary::RecPerColumnFlags);
  if (! rec)
    ERR_RETURN(dict->getNdbError());
  m_ndb_record= rec;

  DBUG_RETURN(0);
}

/* Create NdbRecord for setting hidden primary key from Uint64. */
int
ha_ndbcluster::add_hidden_pk_ndb_record(NDBDICT *dict)
{
  DBUG_ENTER("ha_ndbcluster::add_hidden_pk_ndb_record");
  NdbDictionary::RecordSpecification spec[1];
  NdbRecord *rec;

  spec[0].column= m_table->getColumn(m_table_map->get_hidden_key_column());
  spec[0].offset= 0;
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;

  rec= dict->createRecord(m_table, spec, 1, sizeof(spec[0]));
  if (! rec)
    ERR_RETURN(dict->getNdbError());
  m_ndb_hidden_key_record= rec;

  DBUG_RETURN(0);
}

int
ha_ndbcluster::add_index_ndb_record(NDBDICT *dict, KEY *key_info, uint index_no)
{
  DBUG_ENTER("ha_ndbcluster::add_index_ndb_record");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;

  Uint32 offset= 0;
  for (uint i= 0; i < key_info->user_defined_key_parts; i++)
  {
    KEY_PART_INFO *kp= &key_info->key_part[i];
    spec[i].column= m_table_map->getColumn(kp->fieldnr - 1);
    if (! spec[i].column)
      ERR_RETURN(dict->getNdbError());
    if (kp->null_bit)
    {
      /* Nullable column. */
      spec[i].offset= offset + 1;           // First byte is NULL flag
      spec[i].nullbit_byte_offset= offset;
      spec[i].nullbit_bit_in_byte= 0;
    }
    else
    {
      /* Not nullable column. */
      spec[i].offset= offset;
      spec[i].nullbit_byte_offset= 0;
      spec[i].nullbit_bit_in_byte= 0;
    }
    offset+= kp->store_length;
  }

  if (m_index[index_no].index)
  {
    /*
      Enable MysqldShrinkVarchar flag so that the two-byte length used by
      mysqld for short varchar keys is correctly converted into a one-byte
      length used by Ndb kernel.
    */
    rec= dict->createRecord(m_index[index_no].index, m_table,
                            spec, key_info->user_defined_key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_record_key= rec;
  }
  else
    m_index[index_no].ndb_record_key= NULL;

  if (m_index[index_no].unique_index)
  {
    rec= dict->createRecord(m_index[index_no].unique_index, m_table,
                            spec, key_info->user_defined_key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key= rec;
  }
  else if (index_no == table_share->primary_key)
  {
    /* The primary key is special, there is no explicit NDB index associated. */
    rec= dict->createRecord(m_table,
                            spec, key_info->user_defined_key_parts, sizeof(spec[0]),
                            ( NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield ));
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key= rec;
  }
  else
    m_index[index_no].ndb_unique_record_key= NULL;

  /* Now do the same, but this time with offsets from Field, for row access. */
  for (uint i= 0; i < key_info->user_defined_key_parts; i++)
  {
    const KEY_PART_INFO *kp= &key_info->key_part[i];

    spec[i].offset= kp->offset;
    if (kp->null_bit)
    {
      /* Nullable column. */
      spec[i].nullbit_byte_offset= kp->null_offset;
      spec[i].nullbit_bit_in_byte= null_bit_mask_to_bit_number(kp->null_bit);
    }
    else
    {
      /* Not nullable column. */
      spec[i].nullbit_byte_offset= 0;
      spec[i].nullbit_bit_in_byte= 0;
    }
  }

  if (m_index[index_no].unique_index)
  {
    rec= dict->createRecord(m_index[index_no].unique_index, m_table,
                            spec, key_info->user_defined_key_parts, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row= rec;
  }
  else if (index_no == table_share->primary_key)
  {
    rec= dict->createRecord(m_table,
                            spec, key_info->user_defined_key_parts, sizeof(spec[0]),
                            NdbDictionary::RecMysqldBitfield);
    if (! rec)
      ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row= rec;
  }
  else
    m_index[index_no].ndb_unique_record_row= NULL;

  DBUG_RETURN(0);
}

/*
  Associate index handles for each index of a table
*/
int ha_ndbcluster::open_indexes(Ndb *ndb, TABLE *tab)
{

  NDBDICT *dict= ndb->getDictionary();
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::open_indexes");
  m_has_unique_index= false;

  for (uint i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    const int error= add_index_handle(dict, key_info, *key_name, i);
    if (error)
    {
      DBUG_RETURN(error);
    }

    m_index[i].null_in_unique_index= false;
    if (check_index_fields_not_null(key_info))
      m_index[i].null_in_unique_index= true;
  }

  DBUG_RETURN(0);
}



void
ha_ndbcluster::release_indexes(NdbDictionary::Dictionary *dict,
                               int invalidate)
{
  DBUG_ENTER("ha_ndbcluster::release_indexes");

  for (uint i= 0; i < MAX_KEY; i++)
  {
    NDB_INDEX_DATA& index = m_index[i];
    if (index.unique_index)
    {
      // Release reference to index in NdbAPI
      dict->removeIndexGlobal(*index.unique_index, invalidate);
    }
    if (index.index)
    {
      // Release reference to index in NdbAPI
      dict->removeIndexGlobal(*index.index, invalidate);
    }
    ndb_clear_index(dict, index);
  }
  DBUG_VOID_RETURN;
}


/*
  Renumber indexes in index list by shifting out
  the index that was dropped
 */
void ha_ndbcluster::inplace__renumber_indexes(uint dropped_index_num)
{
  DBUG_ENTER("ha_ndbcluster::inplace__renumber_indexes");

  // Shift the dropped index out of list
  for(uint i= dropped_index_num + 1;
      i != MAX_KEY && m_index[i].status != NDB_INDEX_DATA::UNDEFINED; i++)
  {
    NDB_INDEX_DATA tmp=  m_index[i - 1];
    m_index[i - 1]= m_index[i];
    m_index[i]= tmp;
  }

  DBUG_VOID_RETURN;
}

/*
  Drop all indexes that are marked for deletion
*/
int ha_ndbcluster::inplace__drop_indexes(Ndb *ndb, TABLE *tab)
{
  int error= 0;
  KEY* key_info= tab->key_info;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::inplace__drop_indexes");
  
  for (uint i= 0; i < tab->s->keys; i++, key_info++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == NDB_INDEX_DATA::TO_BE_DROPPED)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      const NdbDictionary::Index *unique_index= m_index[i].unique_index;

      if (unique_index)
      {
        DBUG_PRINT("info", ("Dropping unique index %u: %s", i,
                            unique_index->getName()));
        // Drop unique index from ndb
        if (dict->dropIndexGlobal(*unique_index) == 0)
        {
          dict->removeIndexGlobal(*unique_index, 1);
          m_index[i].unique_index= NULL;
        }
        else
        {
          error= ndb_to_mysql_error(&dict->getNdbError());
          m_dupkey= i; // for HA_ERR_DROP_INDEX_FK
        }
      }
      if (!error && index)
      {
        DBUG_PRINT("info", ("Dropping index %u: %s", i, index->getName()));
        // Drop ordered index from ndb
        if (dict->dropIndexGlobal(*index) == 0)
        {
          dict->removeIndexGlobal(*index, 1);
          m_index[i].index= NULL;
        }
        else
        {
          error=ndb_to_mysql_error(&dict->getNdbError());
          m_dupkey= i; // for HA_ERR_DROP_INDEX_FK
        }
      }
      if (error)
      {
        // Change the status back to active. since it was not dropped
        m_index[i].status = NDB_INDEX_DATA::ACTIVE;
        DBUG_RETURN(error);
      }
      // Renumber the indexes by shifting out the dropped index
      inplace__renumber_indexes(i);
      // clear the dropped index at last now
      ndb_clear_index(dict, m_index[tab->s->keys]);
    }
  }
  
  DBUG_RETURN(error);
}

/**
  Decode the type of an index from information 
  provided in table object.
*/
NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_table(uint inx) const
{
  return get_index_type_from_key(inx, table_share->key_info,
                                 inx == table_share->primary_key);
}

NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_key(uint inx,
                                                      KEY *key_info,
                                                      bool primary) const
{
  bool is_hash_index=  (key_info[inx].algorithm == 
                        HA_KEY_ALG_HASH);
  if (primary)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;
  
  return ((key_info[inx].flags & HA_NOSAME) ? 
          (is_hash_index ? UNIQUE_INDEX : UNIQUE_ORDERED_INDEX) :
          ORDERED_INDEX);
} 

bool ha_ndbcluster::check_index_fields_not_null(KEY* key_info) const
{
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
  DBUG_ENTER("ha_ndbcluster::check_index_fields_not_null");
  
  for (; key_part != end; key_part++) 
    {
      Field* field= key_part->field;
      if (field->maybe_null())
        DBUG_RETURN(true);
    }
  
  DBUG_RETURN(false);
}

void ha_ndbcluster::release_metadata(THD *thd, Ndb *ndb)
{
  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  if(m_table == NULL) 
  {
    DBUG_VOID_RETURN;  // table already released
  }

  NDBDICT *dict= ndb->getDictionary();
  int invalidate_indexes= 0;
  if (thd && thd->lex && thd->lex->sql_command == SQLCOM_FLUSH)
  {
    invalidate_indexes = 1;
  }
  if (m_ndb_record != NULL)
  {
    dict->releaseRecord(m_ndb_record);
    m_ndb_record= NULL;
  }
  if (m_ndb_hidden_key_record != NULL)
  {
    dict->releaseRecord(m_ndb_hidden_key_record);
    m_ndb_hidden_key_record= NULL;
  }
  if (m_table->getObjectStatus() == NdbDictionary::Object::Invalid)
    invalidate_indexes= 1;
  dict->removeTableGlobal(*m_table, invalidate_indexes);

  m_table_info= NULL;

  release_indexes(dict, invalidate_indexes);

  // Release FK data
  release_fk_data();

  m_table= NULL;
  DBUG_VOID_RETURN;
}


/*
  Map from thr_lock_type to NdbOperation::LockMode
*/
static inline
NdbOperation::LockMode get_ndb_lock_mode(enum thr_lock_type type)
{
  if (type >= TL_WRITE_ALLOW_WRITE)
    return NdbOperation::LM_Exclusive;
  if (type ==  TL_READ_WITH_SHARED_LOCKS)
    return NdbOperation::LM_Read;
  return NdbOperation::LM_CommittedRead;
}


static const ulong index_type_flags[]=
{
  /* UNDEFINED_INDEX */
  0,                         

  /* PRIMARY_KEY_INDEX */
  HA_ONLY_WHOLE_INDEX, 

  /* PRIMARY_KEY_ORDERED_INDEX */
  /* 
     Enable HA_KEYREAD_ONLY when "sorted" indexes are supported, 
     thus ORDER BY clauses can be optimized by reading directly 
     through the index.
  */
  // HA_KEYREAD_ONLY | 
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* UNIQUE_INDEX */
  HA_ONLY_WHOLE_INDEX,

  /* UNIQUE_ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER
};

static const int index_flags_size= sizeof(index_type_flags)/sizeof(ulong);

inline NDB_INDEX_TYPE ha_ndbcluster::get_index_type(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_index[idx_no].type;
}

inline bool ha_ndbcluster::has_null_in_unique_index(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_index[idx_no].null_in_unique_index;
}


/**
  Get the flags for an index.

  @return
    flags depending on the type of the index.
*/

inline ulong ha_ndbcluster::index_flags(uint idx_no, uint, bool) const
{
  DBUG_ENTER("ha_ndbcluster::index_flags");
  DBUG_PRINT("enter", ("idx_no: %u", idx_no));
  DBUG_ASSERT(get_index_type_from_table(idx_no) < index_flags_size);
  DBUG_RETURN(index_type_flags[get_index_type_from_table(idx_no)] | 
              HA_KEY_SCAN_NOT_ROR);
}

bool
ha_ndbcluster::primary_key_is_clustered() const
{

  if (table->s->primary_key == MAX_KEY)
    return false;

  /*
    NOTE 1: our ordered indexes are not really clustered
    but since accesing data when scanning index is free
    it's a good approximation

    NOTE 2: We really should consider DD attributes here too
    (for which there is IO to read data when scanning index)
    but that will need to be handled later...
  */
  const NDB_INDEX_TYPE idx_type =
    get_index_type_from_table(table->s->primary_key);
  return (idx_type == PRIMARY_KEY_ORDERED_INDEX ||
          idx_type == UNIQUE_ORDERED_INDEX ||
          idx_type == ORDERED_INDEX);
}

bool ha_ndbcluster::check_index_fields_in_write_set(uint keyno)
{
  KEY* key_info= table->key_info + keyno;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
  uint i;
  DBUG_ENTER("check_index_fields_in_write_set");

  for (i= 0; key_part != end; key_part++, i++)
  {
    Field* field= key_part->field;
    if (!bitmap_is_set(table->write_set, field->field_index))
    {
      DBUG_RETURN(false);
    }
  }

  DBUG_RETURN(true);
}


/**
  Read one record from NDB using primary key.
*/

int ha_ndbcluster::pk_read(const uchar *key, uchar *buf, uint32 *part_id)
{
  NdbConnection *trans= m_thd_ndb->trans;
  DBUG_ENTER("pk_read");
  DBUG_ASSERT(trans);

  NdbOperation::LockMode lm= get_ndb_lock_mode(m_lock.type);

  if (check_if_pushable(NdbQueryOperationDef::PrimaryKeyAccess,
                        table->s->primary_key))
  {
    // Is parent of pushed join
    DBUG_ASSERT(lm == NdbOperation::LM_CommittedRead);
    const int error =
        pk_unique_index_read_key_pushed(table->s->primary_key, key);
    if (unlikely(error))
    {
      DBUG_RETURN(error);
    }

    DBUG_ASSERT(m_active_query!=NULL);
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        m_active_query->getNdbError().code) 
      DBUG_RETURN(ndb_err(trans));

    int result= fetch_next_pushed();
    if (result == NdbQuery::NextResult_gotRow)
    {
      DBUG_RETURN(0);
    }
    else if (result == NdbQuery::NextResult_scanComplete)
    {
      DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
    }
    else
    {
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else
  {
    if (m_pushed_join_operation == PUSHED_ROOT)
    {
      m_thd_ndb->m_pushed_queries_dropped++;
    }

    const NdbOperation *op;
    if (!(op= pk_unique_index_read_key(table->s->primary_key, key, buf, lm,
                                       (m_user_defined_partitioning ?
                                        part_id :
                                        NULL))))
      ERR_RETURN(trans->getNdbError());

    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        op->getNdbError().code) 
      DBUG_RETURN(ndb_err(trans));

    DBUG_RETURN(0);
  }
}

/**
  Update primary key or part id by doing delete insert.
*/

int ha_ndbcluster::ndb_pk_update_row(THD *thd,
                                     const uchar *old_data, uchar *new_data)
{
  NdbTransaction *trans= m_thd_ndb->trans;
  int error;
  DBUG_ENTER("ndb_pk_update_row");
  DBUG_ASSERT(trans);

  DBUG_PRINT("info", ("primary key update or partition change, "
                      "doing delete+insert"));

#ifndef DBUG_OFF
  /*
   * 'old_data' contain colums as specified in 'read_set'.
   * All PK columns must be included for ::ndb_delete_row()
   */
  DBUG_ASSERT(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
  /*
   * As a complete 'new_data' row is reinserted after the delete,
   * all columns must be contained in the read+write union.
   */
  bitmap_copy(&m_bitmap, table->read_set);
  bitmap_union(&m_bitmap, table->write_set);
  DBUG_ASSERT(bitmap_is_set_all(&m_bitmap));
#endif

  // Delete old row
  error= ndb_delete_row(old_data, true);
  if (error)
  {
    DBUG_PRINT("info", ("delete failed"));
    DBUG_RETURN(error);
  }

  // Insert new row
  DBUG_PRINT("info", ("delete succeded"));
  bool batched_update= (m_active_cursor != 0);
  /*
    If we are updating a primary key with auto_increment
    then we need to update the auto_increment counter
  */
  if (table->found_next_number_field &&
      bitmap_is_set(table->write_set, 
                    table->found_next_number_field->field_index) &&
      (error= set_auto_inc(thd, table->found_next_number_field)))
  {
    DBUG_RETURN(error);
  }

  /*
    We are mapping a MySQLD PK changing update to an NdbApi delete 
    and insert.
    The original PK changing update may not have written new values
    to all columns, so the write set may be partial.
    We set the write set to be all columns so that all values are
    copied from the old row to the new row.
  */
  my_bitmap_map *old_map=
    tmp_use_all_columns(table, table->write_set);
  error= ndb_write_row(new_data, true, batched_update);
  tmp_restore_column_map(table->write_set, old_map);

  if (error)
  {
    DBUG_PRINT("info", ("insert failed"));
    if (trans->commitStatus() == NdbConnection::Started)
    {
      if (thd->slave_thread)
        g_ndb_slave_state.atTransactionAbort();
      m_thd_ndb->m_unsent_bytes= 0;
      m_thd_ndb->m_execute_count++;
      DBUG_PRINT("info", ("execute_count: %u", m_thd_ndb->m_execute_count));
      trans->execute(NdbTransaction::Rollback);
    }
    DBUG_RETURN(error);
  }
  DBUG_PRINT("info", ("delete+insert succeeded"));

  DBUG_RETURN(0);
}

/**
  Check that all operations between first and last all
  have gotten the errcode
  If checking for HA_ERR_KEY_NOT_FOUND then update m_dupkey
  for all succeeding operations
*/
bool ha_ndbcluster::check_all_operations_for_error(NdbTransaction *trans,
                                                   const NdbOperation *first,
                                                   const NdbOperation *last,
                                                   uint errcode)
{
  const NdbOperation *op= first;
  DBUG_ENTER("ha_ndbcluster::check_all_operations_for_error");

  while(op)
  {
    NdbError err= op->getNdbError();
    if (err.status != NdbError::Success)
    {
      if (ndb_to_mysql_error(&err) != (int) errcode)
        DBUG_RETURN(false);
      if (op == last) break;
      op= trans->getNextCompletedOperation(op);
    }
    else
    {
      // We found a duplicate
      if (op->getType() == NdbOperation::UniqueIndexAccess)
      {
        if (errcode == HA_ERR_KEY_NOT_FOUND)
        {
          NdbIndexOperation *iop= (NdbIndexOperation *) op;
          const NDBINDEX *index= iop->getIndex();
          // Find the key_no of the index
          for(uint i= 0; i<table->s->keys; i++)
          {
            if (m_index[i].unique_index == index)
            {
              m_dupkey= i;
              break;
            }
          }
        }
      }
      else
      {
        // Must have been primary key access
        DBUG_ASSERT(op->getType() == NdbOperation::PrimaryKeyAccess);
        if (errcode == HA_ERR_KEY_NOT_FOUND)
          m_dupkey= table->s->primary_key;
      }
      DBUG_RETURN(false);
    }
  }
  DBUG_RETURN(true);
}


/**
 * Check if record contains any null valued columns that are part of a key
 */
static
int
check_null_in_record(const KEY* key_info, const uchar *record)
{
  KEY_PART_INFO *curr_part, *end_part;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->user_defined_key_parts;

  while (curr_part != end_part)
  {
    if (curr_part->null_bit &&
        (record[curr_part->null_offset] & curr_part->null_bit))
      return 1;
    curr_part++;
  }
  return 0;
  /*
    We could instead pre-compute a bitmask in table_share with one bit for
    every null-bit in the key, and so check this just by OR'ing the bitmask
    with the null bitmap in the record.
    But not sure it's worth it.
  */
}

/* Empty mask and dummy row, for reading no attributes using NdbRecord. */
/* Mask will be initialized to all zeros by linker. */
static unsigned char empty_mask[(NDB_MAX_ATTRIBUTES_IN_TABLE+7)/8];
static char dummy_row[1];

/**
  Peek to check if any rows already exist with conflicting
  primary key or unique index values
*/

int ha_ndbcluster::peek_indexed_rows(const uchar *record, 
                                     NDB_WRITE_OP write_op)
{
  NdbTransaction *trans;
  const NdbOperation *op;
  const NdbOperation *first, *last;
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions=NULL;
  options.optionsPresent = 0;
  uint i;
  int error;
  DBUG_ENTER("peek_indexed_rows");
  if (unlikely(!(trans= get_transaction(error))))
  {
    DBUG_RETURN(error);
  }
  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);
  first= NULL;
  if (write_op != NDB_UPDATE && table->s->primary_key != MAX_KEY)
  {
    /*
     * Fetch any row with colliding primary key
     */
    const NdbRecord *key_rec=
      m_index[table->s->primary_key].ndb_unique_record_row;

    if (m_user_defined_partitioning)
    {
      uint32 part_id;
      int error;
      longlong func_value;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
      error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
      dbug_tmp_restore_column_map(table->read_set, old_map);
      if (error)
      {
        m_part_info->err_value= func_value;
        DBUG_RETURN(error);
      }
      options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId=part_id;
      poptions=&options;
    }    

    if (!(op= trans->readTuple(key_rec, (const char *)record,
                               m_ndb_record, dummy_row, lm, empty_mask,
                               poptions, 
                               sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());
    
    first= op;
  }
  /*
   * Fetch any rows with colliding unique indexes
   */
  KEY* key_info;
  for (i= 0, key_info= table->key_info; i < table->s->keys; i++, key_info++)
  {
    if (i != table_share->primary_key &&
        key_info->flags & HA_NOSAME &&
        bitmap_is_overlapping(table->write_set, m_key_fields[i]))
    {
      /*
        A unique index is defined on table and it's being updated
        We cannot look up a NULL field value in a unique index. But since
        keys with NULLs are not indexed, such rows cannot conflict anyway, so
        we just skip the index in this case.
      */
      if (check_null_in_record(key_info, record))
      {
        DBUG_PRINT("info", ("skipping check for key with NULL"));
        continue;
      }
      if (write_op != NDB_INSERT && !check_index_fields_in_write_set(i))
      {
        DBUG_PRINT("info", ("skipping check for key %u not in write_set", i));
        continue;
      }

      const NdbOperation *iop;
      const NdbRecord *key_rec= m_index[i].ndb_unique_record_row;
      if (!(iop= trans->readTuple(key_rec, (const char *)record,
                                  m_ndb_record, dummy_row,
                                  lm, empty_mask)))
        ERR_RETURN(trans->getNdbError());

      if (!first)
        first= iop;
    }
  }
  last= trans->getLastDefinedOperation();
  if (first)
  {
    (void)execute_no_commit_ie(m_thd_ndb, trans);
  }
  else
  {
    // Table has no keys
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
  }
  const NdbError ndberr= trans->getNdbError();
  error= ndberr.mysql_code;
  if ((error != 0 && error != HA_ERR_KEY_NOT_FOUND) ||
      check_all_operations_for_error(trans, first, last, 
                                     HA_ERR_KEY_NOT_FOUND))
  {
    DBUG_RETURN(ndb_err(trans));
  }
  else
  {
    DBUG_PRINT("info", ("m_dupkey %d", m_dupkey));
  }
  DBUG_RETURN(0);
}


/**
  Read one record from NDB using unique secondary index.
*/

int ha_ndbcluster::unique_index_read(const uchar *key, uchar *buf)
{
  NdbTransaction *trans= m_thd_ndb->trans;
  NdbOperation::LockMode lm= get_ndb_lock_mode(m_lock.type);
  DBUG_ENTER("ha_ndbcluster::unique_index_read");
  DBUG_PRINT("enter", ("index: %u, lm: %u", active_index, (unsigned int)lm));
  DBUG_ASSERT(trans);


  if (check_if_pushable(NdbQueryOperationDef::UniqueIndexAccess,
                        active_index))
  {
    DBUG_ASSERT(lm == NdbOperation::LM_CommittedRead);
    const int error= pk_unique_index_read_key_pushed(active_index, key);
    if (unlikely(error))
    {
      DBUG_RETURN(error);
    }

    DBUG_ASSERT(m_active_query!=NULL);
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        m_active_query->getNdbError().code) 
      DBUG_RETURN(ndb_err(trans));

    int result= fetch_next_pushed();
    if (result == NdbQuery::NextResult_gotRow)
    {
      DBUG_RETURN(0);
    }
    else if (result == NdbQuery::NextResult_scanComplete)
    {
      DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
    }
    else
    {
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else
  {
    if (m_pushed_join_operation == PUSHED_ROOT)
    {
      m_thd_ndb->m_pushed_queries_dropped++;
    }

    const NdbOperation *op;

    if (!(op= pk_unique_index_read_key(active_index, key, buf, lm, NULL)))
      ERR_RETURN(trans->getNdbError());
  
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        op->getNdbError().code) 
    {
      int err= ndb_err(trans);

      DBUG_RETURN(err);
    }

    DBUG_RETURN(0);
  }
}

int
ha_ndbcluster::scan_handle_lock_tuple(NdbScanOperation *scanOp,
                                      NdbTransaction *trans)
{
  DBUG_ENTER("ha_ndbcluster::scan_handle_lock_tuple");
  if (m_lock_tuple)
  {
    /*
      Lock level m_lock.type either TL_WRITE_ALLOW_WRITE
      (SELECT FOR UPDATE) or TL_READ_WITH_SHARED_LOCKS (SELECT
      LOCK WITH SHARE MODE) and row was not explictly unlocked 
      with unlock_row() call
    */
    DBUG_PRINT("info", ("Keeping lock on scanned row"));
      
    if (!(scanOp->lockCurrentTuple(trans, m_ndb_record,
                                   dummy_row, empty_mask)))
    {
      m_lock_tuple= false;
      ERR_RETURN(trans->getNdbError());
    }

    /* Perform 'empty update' to mark the read in the binlog, iff required */
    /*
     * Lock_mode = exclusive
     * Session_state = marking_exclusive_reads
     * THEN
     * issue updateCurrentTuple with AnyValue explicitly set
     */
    if ((m_lock.type >= TL_WRITE_ALLOW_WRITE) &&
        THDVAR(current_thd, log_exclusive_reads))
    {
      if (scan_log_exclusive_read(scanOp, trans))
      { 
        m_lock_tuple= false;
        ERR_RETURN(trans->getNdbError());
      }
    }

    m_thd_ndb->m_unsent_bytes+=12;
    m_lock_tuple= false;
  }
  DBUG_RETURN(0);
}

inline int ha_ndbcluster::fetch_next(NdbScanOperation* cursor)
{
  DBUG_ENTER("fetch_next");
  int local_check;
  int error;
  NdbTransaction *trans= m_thd_ndb->trans;
  
  DBUG_ASSERT(trans);
  if ((error= scan_handle_lock_tuple(cursor, trans)) != 0)
    DBUG_RETURN(error);
  
  bool contact_ndb= m_lock.type < TL_WRITE_ALLOW_WRITE &&
                    m_lock.type != TL_READ_WITH_SHARED_LOCKS;
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    /*
      We can only handle one tuple with blobs at a time.
    */
    if (m_thd_ndb->m_unsent_bytes && m_blobs_pending)
    {
      if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
        DBUG_RETURN(ndb_err(trans));
    }
    
    /* Should be no unexamined completed operations
       nextResult() on Blobs generates Blob part read ops,
       so we will free them here
    */
    release_completed_operations(trans);
    
    if ((local_check= cursor->nextResult(&_m_next_row,
                                         contact_ndb,
                                         m_thd_ndb->m_force_send)) == 0)
    {
      /*
	Explicitly lock tuple if "select for update" or
	"select lock in share mode"
      */
      m_lock_tuple= (m_lock.type == TL_WRITE_ALLOW_WRITE
		     || 
		     m_lock.type == TL_READ_WITH_SHARED_LOCKS);
      DBUG_RETURN(0);
    } 
    else if (local_check == 1 || local_check == 2)
    {
      // 1: No more records
      // 2: No more cached records
      
      /*
        Before fetching more rows and releasing lock(s),
        all pending update or delete operations should 
        be sent to NDB
      */
      DBUG_PRINT("info", ("thd_ndb->m_unsent_bytes: %ld",
                          (long) m_thd_ndb->m_unsent_bytes));
      if (m_thd_ndb->m_unsent_bytes)
      {
        if ((error = flush_bulk_insert()) != 0)
          DBUG_RETURN(error);
      }
      contact_ndb= (local_check == 2);
    }
    else
    {
      DBUG_RETURN(ndb_err(trans));
    }
  } while (local_check == 2);

  DBUG_RETURN(1);
}

int ha_ndbcluster::fetch_next_pushed()
{
  DBUG_ENTER("fetch_next_pushed (from pushed operation)");

  DBUG_ASSERT(m_pushed_operation);
  NdbQuery::NextResultOutcome result= m_pushed_operation->nextResult(true, m_thd_ndb->m_force_send);

  /**
   * Only prepare result & status from this operation in pushed join.
   * Consecutive rows are prepared through ::index_read_pushed() and
   * ::index_next_pushed() which unpack and set correct status for each row.
   */
  if (result == NdbQuery::NextResult_gotRow)
  {
    DBUG_ASSERT(m_next_row!=NULL);
    DBUG_PRINT("info", ("One more record found"));    
    unpack_record_and_set_generated_fields(table, table->record[0],
                                           m_next_row);
//  m_thd_ndb->m_pushed_reads++;
//  DBUG_RETURN(0)
  }
  else if (result == NdbQuery::NextResult_scanComplete)
  {
    DBUG_ASSERT(m_next_row==NULL);
    DBUG_PRINT("info", ("No more records"));
//  m_thd_ndb->m_pushed_reads++;
//  DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  else
  {
    DBUG_PRINT("info", ("Error from 'nextResult()'"));
//  DBUG_ASSERT(false);
    DBUG_RETURN(ndb_err(m_thd_ndb->trans));
  }
  DBUG_RETURN(result);
}


/**
  Get the first record from an indexed table access being a child 
  operation in a pushed join. Fetch will be from prefetched
  cached records which are materialized into the bound buffer 
  areas as result of this call. 
*/

int 
ha_ndbcluster::index_read_pushed(uchar *buf, const uchar *key,
                                 key_part_map keypart_map)
{
  DBUG_ENTER("index_read_pushed");

  // Handler might have decided to not execute the pushed joins which has been prepared
  // In this case we do an unpushed index_read based on 'Plain old' NdbOperations
  if (unlikely(!check_is_pushed()))
  {
    int res= index_read_map(buf, key, keypart_map, HA_READ_KEY_EXACT);
    DBUG_RETURN(res);
  }

  // Might need to re-establish first result row (wrt. its parents which may have been navigated)
  NdbQuery::NextResultOutcome result= m_pushed_operation->firstResult();

  // Result from pushed operation will be referred by 'm_next_row' if non-NULL
  if (result == NdbQuery::NextResult_gotRow)
  {
    DBUG_ASSERT(m_next_row!=NULL);
    unpack_record_and_set_generated_fields(table, buf, m_next_row);
    m_thd_ndb->m_pushed_reads++;
    DBUG_RETURN(0);
  }
  else
  {
    DBUG_ASSERT(result!=NdbQuery::NextResult_gotRow);
    DBUG_PRINT("info", ("No record found"));
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
}


/**
  Get the next record from an indexes table access being a child 
  operation in a pushed join. Fetch will be from prefetched
  cached records which are materialized into the bound buffer 
  areas as result of this call. 
*/
int ha_ndbcluster::index_next_pushed(uchar *buf)
{
  DBUG_ENTER("index_next_pushed");

  // Handler might have decided to not execute the pushed joins which has been prepared
  // In this case we do an unpushed index_read based on 'Plain old' NdbOperations
  if (unlikely(!check_is_pushed()))
  {
    int res= index_next(buf);
    DBUG_RETURN(res);
  }

  DBUG_ASSERT(m_pushed_join_operation>PUSHED_ROOT);  // Child of a pushed join
  DBUG_ASSERT(m_active_query==NULL);

  int res = fetch_next_pushed();
  if (res == NdbQuery::NextResult_gotRow)
  {
    DBUG_RETURN(0);
  }
  else if (res == NdbQuery::NextResult_scanComplete)
  {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  else
  {
    DBUG_RETURN(ndb_err(m_thd_ndb->trans));
  }
}


/**
  Get the next record of a started scan. Try to fetch
  it locally from NdbApi cached records if possible, 
  otherwise ask NDB for more.

  @note
    If this is a update/delete make sure to not contact
    NDB before any pending ops have been sent to NDB.
*/

inline int ha_ndbcluster::next_result(uchar *buf)
{  
  int res;
  DBUG_ENTER("next_result");
    
  if (m_active_cursor)
  {
    if ((res= fetch_next(m_active_cursor)) == 0)
    {
      DBUG_PRINT("info", ("One more record found"));    

      unpack_record(buf, m_next_row);
      DBUG_RETURN(0);
    }
    else if (res == 1)
    {
      // No more records
      DBUG_PRINT("info", ("No more records"));
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    else
    {
      DBUG_RETURN(ndb_err(m_thd_ndb->trans));
    }
  }
  else if (m_active_query)
  {
    res= fetch_next_pushed();
    if (res == NdbQuery::NextResult_gotRow)
    {
      DBUG_RETURN(0);
    }
    else if (res == NdbQuery::NextResult_scanComplete)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    else
    {
      DBUG_RETURN(ndb_err(m_thd_ndb->trans));
    }
  }
  else
    DBUG_RETURN(HA_ERR_END_OF_FILE);
}

int
ha_ndbcluster::log_exclusive_read(const NdbRecord *key_rec,
                                  const uchar *key,
                                  uchar *buf,
                                  Uint32 *ppartition_id)
{
  DBUG_ENTER("log_exclusive_read");
  NdbOperation::OperationOptions opts;
  opts.optionsPresent=
    NdbOperation::OperationOptions::OO_ABORTOPTION |
    NdbOperation::OperationOptions::OO_ANYVALUE;
  
  /* If the key does not exist, that is ok */
  opts.abortOption= NdbOperation::AO_IgnoreError;
  
  /* 
     Mark the AnyValue as a read operation, so that the update
     is processed
  */
  opts.anyValue= 0;
  ndbcluster_anyvalue_set_read_op(opts.anyValue);

  if (ppartition_id != NULL)
  {
    assert(m_user_defined_partitioning);
    opts.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
    opts.partitionId= *ppartition_id;
  }
  
  const NdbOperation* markingOp=
    m_thd_ndb->trans->updateTuple(key_rec,
                                  (const char*) key,
                                  m_ndb_record,
                                  (char*)buf,
                                  empty_mask,
                                  &opts,
                                  opts.size());
  if (!markingOp)
  {
    char msg[FN_REFLEN];
    snprintf(msg, sizeof(msg), "Error logging exclusive reads, failed creating markingOp, %u, %s\n",
                m_thd_ndb->trans->getNdbError().code,
                m_thd_ndb->trans->getNdbError().message);
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_EXCEPTIONS_WRITE_ERROR,
                        ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
    /*
      By returning -1 the caller (pk_unique_index_read_key) will return
      NULL and error on transaction object will be returned.
    */
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

int
ha_ndbcluster::scan_log_exclusive_read(NdbScanOperation *cursor,
                                       NdbTransaction *trans)
{
  DBUG_ENTER("ha_ndbcluster::scan_log_exclusive_read");
  NdbOperation::OperationOptions opts;
  opts.optionsPresent= NdbOperation::OperationOptions::OO_ANYVALUE;

  /* 
     Mark the AnyValue as a read operation, so that the update
     is processed
  */
  opts.anyValue= 0;
  ndbcluster_anyvalue_set_read_op(opts.anyValue);
  
  const NdbOperation* markingOp=
    cursor->updateCurrentTuple(trans, m_ndb_record,
                               dummy_row, empty_mask,
                               &opts,
                               sizeof(NdbOperation::OperationOptions));
  if (markingOp == NULL)
  {
    char msg[FN_REFLEN];
    snprintf(msg, sizeof(msg), "Error logging exclusive reads during scan, failed creating markingOp, %u, %s\n",
                m_thd_ndb->trans->getNdbError().code,
                m_thd_ndb->trans->getNdbError().message);
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_EXCEPTIONS_WRITE_ERROR,
                        ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/**
  Do a primary key or unique key index read operation.
  The key value is taken from a buffer in mysqld key format.
*/
const NdbOperation *
ha_ndbcluster::pk_unique_index_read_key(uint idx, const uchar *key, uchar *buf,
                                        NdbOperation::LockMode lm,
                                        Uint32 *ppartition_id)
{
  DBUG_ENTER("pk_unique_index_read_key");
  const NdbOperation *op;
  const NdbRecord *key_rec;
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent= 0;
  NdbOperation::GetValueSpec gets[2];
  const NDB_INDEX_TYPE idx_type=
    (idx != MAX_KEY)?
    get_index_type(idx)
    : UNDEFINED_INDEX;

  DBUG_ASSERT(m_thd_ndb->trans);

  DBUG_PRINT("info", ("pk_unique_index_read_key of table %s", table->s->table_name.str));

  if (idx != MAX_KEY)
    key_rec= m_index[idx].ndb_unique_record_key;
  else
    key_rec= m_ndb_hidden_key_record;

  /* Initialize the null bitmap, setting unused null bits to 1. */
  memset(buf, 0xff, table->s->null_bytes);

  if (table_share->primary_key == MAX_KEY)
  {
    get_hidden_fields_keyop(&options, gets);
    poptions= &options;
  }
  get_read_set(false, idx);

  if (ppartition_id != NULL)
  {
    assert(m_user_defined_partitioning);
    options.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId= *ppartition_id;
    poptions= &options;
  }

  op= m_thd_ndb->trans->readTuple(key_rec, (const char *)key, m_ndb_record,
                                  (char *)buf, lm,
                                  m_table_map->get_column_mask(table->read_set),
                                  poptions,
                                  sizeof(NdbOperation::OperationOptions));

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, buf, table->read_set) != 0)
    DBUG_RETURN(NULL);

  /* Perform 'empty update' to mark the read in the binlog, iff required */
  /*
   * Lock_mode = exclusive
   * Index = primary or unique
   * Session_state = marking_exclusive_reads
   * THEN
   * issue updateTuple with AnyValue explicitly set
   */
  if ((lm == NdbOperation::LM_Exclusive) &&
      /*
        We don't need to check index type
        (idx_type == PRIMARY_KEY_INDEX ||
        idx_type == PRIMARY_KEY_ORDERED_INDEX ||
        idx_type == UNIQUE_ORDERED_INDEX ||
        idx_type == UNIQUE_INDEX) 
        since this method is only invoked for
        primary or unique indexes, but we do need to check
        if it was a hidden primary key.
      */
      idx_type != UNDEFINED_INDEX &&
      THDVAR(current_thd, log_exclusive_reads))
  {
    if (log_exclusive_read(key_rec, key, buf, ppartition_id) != 0)
      DBUG_RETURN(NULL);
  }

  DBUG_RETURN(op);
}


static
bool
is_shrinked_varchar(const Field *field)
{
  if (field->real_type() ==  MYSQL_TYPE_VARCHAR)
  {
    if (((Field_varstring*)field)->length_bytes == 1)
      return true;
  }

  return false;
}

int
ha_ndbcluster::pk_unique_index_read_key_pushed(uint idx, const uchar *key)
{
  DBUG_ENTER("pk_unique_index_read_key_pushed");
  DBUG_ASSERT(m_thd_ndb->trans);
  DBUG_ASSERT(idx < MAX_KEY);

  if (m_active_query)
  {
    m_active_query->close(false);
    m_active_query= NULL;
  }

  get_read_set(false, idx);

  KEY *key_def= &table->key_info[idx];
  KEY_PART_INFO *key_part;

  uint i;
  Uint32 offset= 0;
  NdbQueryParamValue paramValues[ndb_pushed_join::MAX_KEY_PART];
  DBUG_ASSERT(key_def->user_defined_key_parts <= ndb_pushed_join::MAX_KEY_PART);

  uint map[ndb_pushed_join::MAX_KEY_PART];
  ndbcluster_build_key_map(m_table, m_index[idx], &table->key_info[idx], map);

  // Bind key values defining root of pushed join
  for (i = 0, key_part= key_def->key_part; i < key_def->user_defined_key_parts; i++, key_part++)
  {
    bool shrinkVarChar= is_shrinked_varchar(key_part->field);

    if (key_part->null_bit)                         // Column is nullable
    {
      DBUG_ASSERT(idx != table_share->primary_key); // PK can't be nullable
      DBUG_ASSERT(*(key+offset)==0);                // Null values not allowed in key
                                                    // Value is imm. after NULL indicator
      paramValues[map[i]]= NdbQueryParamValue(key+offset+1,shrinkVarChar);
    }
    else                                            // Non-nullable column
    {
      paramValues[map[i]]= NdbQueryParamValue(key+offset,shrinkVarChar);
    }
    offset+= key_part->store_length;
  }

  const int ret= create_pushed_join(paramValues, key_def->user_defined_key_parts);
  DBUG_RETURN(ret);
}


/** Count number of columns in key part. */
static uint
count_key_columns(const KEY *key_info, const key_range *key)
{
  KEY_PART_INFO *first_key_part= key_info->key_part;
  KEY_PART_INFO *key_part_end= first_key_part + key_info->user_defined_key_parts;
  KEY_PART_INFO *key_part;
  uint length= 0;
  for(key_part= first_key_part; key_part < key_part_end; key_part++)
  {
    if (length >= key->length)
      break;
    length+= key_part->store_length;
  }
  return (uint)(key_part - first_key_part);
}

/* Helper method to compute NDB index bounds. Note: does not set range_no. */
/* Stats queries may differ so add "from" 0:normal 1:RIR 2:RPK. */
void
compute_index_bounds(NdbIndexScanOperation::IndexBound & bound,
                     const KEY *key_info,
                     const key_range *start_key, const key_range *end_key,
                     int from)
{
  DBUG_ENTER("ha_ndbcluster::compute_index_bounds");
  DBUG_PRINT("info", ("from: %d", from));

#ifndef DBUG_OFF
  DBUG_PRINT("info", ("key parts: %u length: %u",
                      key_info->user_defined_key_parts, key_info->key_length));
  {
    for (uint j= 0; j <= 1; j++)
    {
      const key_range* kr= (j == 0 ? start_key : end_key);
      if (kr)
      {
        DBUG_PRINT("info", ("key range %u: length: %u map: %lx flag: %d",
                          j, kr->length, kr->keypart_map, kr->flag));
        DBUG_DUMP("key", kr->key, kr->length);
      }
      else
      {
        DBUG_PRINT("info", ("key range %u: none", j));
      }
    }
  }
#endif

  if (start_key)
  {
    bound.low_key= (const char*)start_key->key;
    bound.low_key_count= count_key_columns(key_info, start_key);
    bound.low_inclusive=
      start_key->flag != HA_READ_AFTER_KEY &&
      start_key->flag != HA_READ_BEFORE_KEY;
  }
  else
  {
    bound.low_key= NULL;
    bound.low_key_count= 0;
  }

  /* RIR query for x >= 1 inexplicably passes HA_READ_KEY_EXACT. */
  if (start_key &&
      (start_key->flag == HA_READ_KEY_EXACT ||
       start_key->flag == HA_READ_PREFIX_LAST) &&
      from != 1)
  {
    bound.high_key= bound.low_key;
    bound.high_key_count= bound.low_key_count;
    bound.high_inclusive= true;
  }
  else if (end_key)
  {
    bound.high_key= (const char*)end_key->key;
    bound.high_key_count= count_key_columns(key_info, end_key);
    /*
      For some reason, 'where b >= 1 and b <= 3' uses HA_READ_AFTER_KEY for
      the end_key.
      So HA_READ_AFTER_KEY in end_key sets high_inclusive, even though in
      start_key it does not set low_inclusive.
    */
    bound.high_inclusive= end_key->flag != HA_READ_BEFORE_KEY;
    if (end_key->flag == HA_READ_KEY_EXACT ||
        end_key->flag == HA_READ_PREFIX_LAST)
    {
      bound.low_key= bound.high_key;
      bound.low_key_count= bound.high_key_count;
      bound.low_inclusive= true;
    }
  }
  else
  {
    bound.high_key= NULL;
    bound.high_key_count= 0;
  }
  DBUG_PRINT("info", ("start_flag=%d end_flag=%d"
                      " lo_keys=%d lo_incl=%d hi_keys=%d hi_incl=%d",
                      start_key?start_key->flag:0, end_key?end_key->flag:0,
                      bound.low_key_count,
                      bound.low_key_count?bound.low_inclusive:0,
                      bound.high_key_count,
                      bound.high_key_count?bound.high_inclusive:0));
  DBUG_VOID_RETURN;
}

/**
  Start ordered index scan in NDB
*/

int ha_ndbcluster::ordered_index_scan(const key_range *start_key,
                                      const key_range *end_key,
                                      bool sorted, bool descending,
                                      uchar* buf, part_id_range *part_spec)
{  
  NdbTransaction *trans;
  NdbIndexScanOperation *op;
  int error;

  DBUG_ENTER("ha_ndbcluster::ordered_index_scan");
  DBUG_PRINT("enter", ("index: %u, sorted: %d, descending: %d read_set=0x%x",
             active_index, sorted, descending, table->read_set->bitmap[0]));
  DBUG_PRINT("enter", ("Starting new ordered scan on %s", m_tabname));

  if (unlikely(!(trans= get_transaction(error))))
  {
    DBUG_RETURN(error);
  }

  if ((error= close_scan()))
    DBUG_RETURN(error);

  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);

  const NdbRecord *key_rec= m_index[active_index].ndb_record_key;
  const NdbRecord *row_rec= m_ndb_record;

  NdbIndexScanOperation::IndexBound bound;
  NdbIndexScanOperation::IndexBound *pbound = NULL;
  if (start_key != NULL || end_key != NULL)
  {
    /* 
       Compute bounds info, reversing range boundaries
       if descending
     */
    compute_index_bounds(bound, 
                         table->key_info + active_index,
                         (descending?
                          end_key : start_key),
                         (descending?
                          start_key : end_key),
                         0);
    bound.range_no = 0;
    pbound = &bound;
  }

  if (check_if_pushable(NdbQueryOperationDef::OrderedIndexScan, active_index))
  {
    const int error= create_pushed_join();
    if (unlikely(error))
      DBUG_RETURN(error);

    NdbQuery* const query= m_active_query;
    if (sorted && query->getQueryOperation((uint)PUSHED_ROOT)
                       ->setOrdering(descending ? NdbQueryOptions::ScanOrdering_descending
                                                : NdbQueryOptions::ScanOrdering_ascending))
    {
      ERR_RETURN(query->getNdbError());
    }

    if (pbound  && query->setBound(key_rec, pbound)!=0)
      ERR_RETURN(query->getNdbError());

    m_thd_ndb->m_scan_count++;

    bool prunable = false;
    if (unlikely(query->isPrunable(prunable) != 0))
      ERR_RETURN(query->getNdbError());
    if (prunable)
      m_thd_ndb->m_pruned_scan_count++;

    // Can't have BLOB in pushed joins (yet)
    DBUG_ASSERT(!uses_blob_value(table->read_set));
  }
  else
  {
    if (m_pushed_join_operation == PUSHED_ROOT)
    {
      m_thd_ndb->m_pushed_queries_dropped++;
    }

    NdbScanOperation::ScanOptions options;
    options.optionsPresent=NdbScanOperation::ScanOptions::SO_SCANFLAGS;
    options.scan_flags=0;

    NdbOperation::GetValueSpec gets[2];
    if (table_share->primary_key == MAX_KEY)
      get_hidden_fields_scan(&options, gets);

    get_read_set(true, active_index);

    if (lm == NdbOperation::LM_Read)
      options.scan_flags|= NdbScanOperation::SF_KeyInfo;
    if (sorted)
      options.scan_flags|= NdbScanOperation::SF_OrderByFull;
    if (descending)
      options.scan_flags|= NdbScanOperation::SF_Descending;

    /* Partition pruning */
    if (m_use_partition_pruning && 
        m_user_defined_partitioning && part_spec != NULL &&
        part_spec->start_part == part_spec->end_part)
    {
      /* Explicitly set partition id when pruning User-defined partitioned scan */
      options.partitionId = part_spec->start_part;
      options.optionsPresent |= NdbScanOperation::ScanOptions::SO_PARTITION_ID;
    }

    NdbInterpretedCode code(m_table);
    if (m_cond && m_cond->generate_scan_filter(&code, &options))
      ERR_RETURN(code.getNdbError());

    if (!(op= trans->scanIndex(key_rec, row_rec, lm,
                               m_table_map->get_column_mask(table->read_set),
                               pbound,
                               &options,
                               sizeof(NdbScanOperation::ScanOptions))))
      ERR_RETURN(trans->getNdbError());

    DBUG_PRINT("info", ("Is scan pruned to 1 partition? : %u", op->getPruned()));
    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (op->getPruned()? 1 : 0);

    if (uses_blob_value(table->read_set) &&
        get_blob_values(op, NULL, table->read_set) != 0)
      ERR_RETURN(op->getNdbError());

    m_active_cursor= op;
  }

  if (sorted)
  {        
    m_thd_ndb->m_sorted_scan_count++;
  }

  if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
    DBUG_RETURN(ndb_err(trans));
  
  DBUG_RETURN(next_result(buf));
}

static
int
guess_scan_flags(NdbOperation::LockMode lm, Ndb_table_map * table_map,
		 const NDBTAB* tab, const MY_BITMAP* readset)
{
  int flags= 0;
  flags|= (lm == NdbOperation::LM_Read) ? NdbScanOperation::SF_KeyInfo : 0;
  if (tab->checkColumns(0, 0) & 2)
  {
    const Uint32 * colmap = (const Uint32 *) table_map->get_column_mask(readset);
    int ret = tab->checkColumns(colmap, no_bytes_in_map(readset));
    
    if (ret & 2)
    { // If disk columns...use disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    }
    else if ((ret & 4) == 0 && (lm == NdbOperation::LM_Exclusive))
    {
      // If no mem column is set and exclusive...guess disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    }
  }
  return flags;
}

/*
  Start full table scan in NDB or unique index scan
 */

int ha_ndbcluster::full_table_scan(const KEY* key_info, 
                                   const key_range *start_key,
                                   const key_range *end_key,
                                   uchar *buf)
{
  int error;
  NdbTransaction *trans= m_thd_ndb->trans;
  part_id_range part_spec;
  bool use_set_part_id= false;
  NdbOperation::GetValueSpec gets[2];

  DBUG_ENTER("full_table_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));

  if (m_use_partition_pruning && m_user_defined_partitioning)
  {
    DBUG_ASSERT(m_pushed_join_operation != PUSHED_ROOT);
    part_spec.start_part= 0;
    part_spec.end_part= m_part_info->get_tot_partitions() - 1;
    prune_partition_set(table, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
    */
    if (part_spec.start_part > part_spec.end_part)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }

    if (part_spec.start_part == part_spec.end_part)
    {
      /*
       * Only one partition is required to scan, if sorted is required
       * don't need it anymore since output from one ordered partitioned
       * index is always sorted.
       *
       * Note : This table scan pruning currently only occurs for 
       * UserDefined partitioned tables.
       * It could be extended to occur for natively partitioned tables if
       * the Partitioning layer can make a key (e.g. start or end key)
       * available so that we can determine the correct pruning in the 
       * NDBAPI layer.
       */
      use_set_part_id= true;
      if (!trans)
        if (unlikely(!(trans= get_transaction_part_id(part_spec.start_part,
                                                      error))))
          DBUG_RETURN(error);
    }
  }
  if (!trans)
    if (unlikely(!(trans= start_transaction(error))))
      DBUG_RETURN(error);

  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);
  NdbScanOperation::ScanOptions options;
  options.optionsPresent = (NdbScanOperation::ScanOptions::SO_SCANFLAGS |
                            NdbScanOperation::ScanOptions::SO_PARALLEL);
  options.scan_flags = guess_scan_flags(lm, m_table_map, m_table, table->read_set);
  options.parallel= DEFAULT_PARALLELISM;

  if (use_set_part_id) {
    assert(m_user_defined_partitioning);
    options.optionsPresent|= NdbScanOperation::ScanOptions::SO_PARTITION_ID;
    options.partitionId = part_spec.start_part;
  };

  if (table_share->primary_key == MAX_KEY)
    get_hidden_fields_scan(&options, gets);

  get_read_set(true, MAX_KEY);

  if (check_if_pushable(NdbQueryOperationDef::TableScan))
  {
    const int error= create_pushed_join();
    if (unlikely(error))
      DBUG_RETURN(error);

    m_thd_ndb->m_scan_count++;
    // Can't have BLOB in pushed joins (yet)
    DBUG_ASSERT(!uses_blob_value(table->read_set));
  }
  else
  {
    if (m_pushed_join_operation == PUSHED_ROOT)
    {
      m_thd_ndb->m_pushed_queries_dropped++;
    }

    NdbScanOperation *op;
    NdbInterpretedCode code(m_table);

    if (!key_info)
    {
      if (m_cond && m_cond->generate_scan_filter(&code, &options))
        ERR_RETURN(code.getNdbError());
    }
    else
    {
      /* Unique index scan in NDB (full table scan with scan filter) */
      DBUG_PRINT("info", ("Starting unique index scan"));
      if (m_cond == nullptr)
      {
        m_cond = new (std::nothrow) ha_ndbcluster_cond;
        if (m_cond == nullptr)
          DBUG_RETURN(HA_ERR_OUT_OF_MEM);
      }

      if (m_cond->generate_scan_filter_from_key(&code, &options, key_info,
                                                start_key, end_key))
        ERR_RETURN(code.getNdbError());
    }
    if (!(op= trans->scanTable(m_ndb_record, lm,
                               m_table_map->get_column_mask(table->read_set),
                               &options, sizeof(NdbScanOperation::ScanOptions))))
      ERR_RETURN(trans->getNdbError());

    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (op->getPruned()? 1 : 0);

    DBUG_ASSERT(m_active_cursor==NULL);
    m_active_cursor= op;

    if (uses_blob_value(table->read_set) &&
        get_blob_values(op, NULL, table->read_set) != 0)
      ERR_RETURN(op->getNdbError());
  } // if (check_if_pushable(NdbQueryOperationDef::TableScan))
  
  if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_PRINT("exit", ("Scan started successfully"));
  DBUG_RETURN(next_result(buf));
} // ha_ndbcluster::full_table_scan()

int
ha_ndbcluster::set_auto_inc(THD *thd, Field *field)
{
  DBUG_ENTER("ha_ndbcluster::set_auto_inc");
  bool read_bit= bitmap_is_set(table->read_set, field->field_index);
  bitmap_set_bit(table->read_set, field->field_index);
  Uint64 next_val= (Uint64) field->val_int() + 1;
  if (!read_bit)
    bitmap_clear_bit(table->read_set, field->field_index);
  DBUG_RETURN(set_auto_inc_val(thd, next_val));
}

inline
int
ha_ndbcluster::set_auto_inc_val(THD *thd, Uint64 value)
{
  Ndb *ndb= get_ndb(thd);
  DBUG_ENTER("ha_ndbcluster::set_auto_inc_val");
#ifndef DBUG_OFF
  char buff[22];
  DBUG_PRINT("info", 
             ("Trying to set next auto increment value to %s",
              llstr(value, buff)));
#endif
  {
    NDB_SHARE::Tuple_id_range_guard g(m_share);

    if (ndb->checkUpdateAutoIncrementValue(g.range, value))
    {
      if (ndb->setAutoIncrementValue(m_table, g.range, value, true) == -1)
      {
        ERR_RETURN(ndb->getNdbError());
      }
    }
  }
  DBUG_RETURN(0);
}


void
ha_ndbcluster::get_read_set(bool use_cursor, uint idx MY_ATTRIBUTE((unused)))
{
  const bool is_delete=
    table->in_use->lex->sql_command == SQLCOM_DELETE ||
    table->in_use->lex->sql_command == SQLCOM_DELETE_MULTI;

  const bool is_update=
    table->in_use->lex->sql_command == SQLCOM_UPDATE ||
    table->in_use->lex->sql_command == SQLCOM_UPDATE_MULTI;

  DBUG_ASSERT(use_cursor ||
              idx == table_share->primary_key ||
              table->key_info[idx].flags & HA_NOSAME);

  if (!is_delete && !is_update)
  {
    return;
  }

  /**
   * It is questionable that we in some cases seems to
   * do a read even if 'm_read_before_write_removal_used'.
   * The usage pattern for this seems to be update/delete 
   * cursors which establish a 'current of' position before
   * a delete- / updateCurrentTuple().
   * Anyway, as 'm_read_before_write_removal_used' we don't
   * have to add more columns to 'read_set'.
   *
   * FUTURE: Investigate if we could have completely 
   * cleared the 'read_set'.
   *
   */
  if (m_read_before_write_removal_used)
  {
    return;
  }

  /**
   * If (part of) a primary key is updated, it is executed
   * as a delete+reinsert. In order to avoid extra read-round trips
   * to fetch missing columns required by reinsert: 
   * Ensure all columns not being modified (in write_set)
   * are read prior to ::ndb_pk_update_row().
   * All PK columns are also required by ::ndb_delete_row()
   */
  if (bitmap_is_overlapping(table->write_set, m_pk_bitmap_p))
  {
    DBUG_ASSERT(table_share->primary_key != MAX_KEY);
    bitmap_set_all(&m_bitmap);
    bitmap_subtract(&m_bitmap, table->write_set);
    bitmap_union(table->read_set, &m_bitmap);
    bitmap_union(table->read_set, m_pk_bitmap_p);
  }

  /**
   * Determine whether we have to read PK columns in 
   * addition to those columns already present in read_set.
   * NOTE: As checked above, It is a precondition that
   *       a read is required as part of delete/update
   *       (!m_read_before_write_removal_used)
   *
   * PK columns are required when:
   *  1) This is a primary/unique keyop.
   *     (i.e. not a positioned update/delete which
   *      maintain a 'current of' position.)
   *
   * In addition, when a 'current of' position is available:
   *  2) When deleting a row containing BLOBs PK is required
   *     to delete BLOB stored in seperate fragments.
   *  3) When updating BLOB columns PK is required to delete 
   *     old BLOB + insert new BLOB contents
   */
  else
  if (!use_cursor ||                             // 1)
      (is_delete && table_share->blob_fields) || // 2)
      uses_blob_value(table->write_set))         // 3)
  {
    bitmap_union(table->read_set, m_pk_bitmap_p);
  }

  /**
   * If update/delete use partition pruning, we need
   * to read the column values which being part of the 
   * partition spec as they are used by
   * ::get_parts_for_update() / ::get_parts_for_delete()
   * Part. columns are always part of PK, so we only
   * have to do this if pk_bitmap wasnt added yet,
   */
  else if (m_use_partition_pruning)  // && m_user_defined_partitioning)
  {
    DBUG_ASSERT(bitmap_is_subset(&m_part_info->full_part_field_set,
                                 m_pk_bitmap_p));
    bitmap_union(table->read_set, &m_part_info->full_part_field_set);
  }
      

  /**
   * Update might cause PK or Unique key violation.
   * Error reporting need values from the offending 
   * unique columns to have been read:
   *
   * NOTE: This is NOT required for the correctness
   *       of the update operation itself. Maybe we
   *       should consider other strategies, like
   *       defering reading of the column values
   *       until formating the error message.
   */
  if (is_update && m_has_unique_index)
  {
    for (uint i= 0; i < table_share->keys; i++)
    {
      if ((table->key_info[i].flags & HA_NOSAME) &&
          bitmap_is_overlapping(table->write_set, m_key_fields[i]))
      {
        bitmap_union(table->read_set, m_key_fields[i]);
      }
    }
  }
}


Uint32
ha_ndbcluster::setup_get_hidden_fields(NdbOperation::GetValueSpec gets[2])
{
  Uint32 num_gets= 0;
  /*
    We need to read the hidden primary key, and possibly the FRAGMENT
    pseudo-column.
  */
  gets[num_gets].column= get_hidden_key_column();
  gets[num_gets].appStorage= &m_ref;
  num_gets++;
  if (m_user_defined_partitioning)
  {
    /* Need to read partition id to support ORDER BY columns. */
    gets[num_gets].column= NdbDictionary::Column::FRAGMENT;
    gets[num_gets].appStorage= &m_part_id;
    num_gets++;
  }
  return num_gets;
}

void
ha_ndbcluster::get_hidden_fields_keyop(NdbOperation::OperationOptions *options,
                                       NdbOperation::GetValueSpec gets[2])
{
  Uint32 num_gets= setup_get_hidden_fields(gets);
  options->optionsPresent|= NdbOperation::OperationOptions::OO_GETVALUE;
  options->extraGetValues= gets;
  options->numExtraGetValues= num_gets;
}

void
ha_ndbcluster::get_hidden_fields_scan(NdbScanOperation::ScanOptions *options,
                                      NdbOperation::GetValueSpec gets[2])
{
  Uint32 num_gets= setup_get_hidden_fields(gets);
  options->optionsPresent|= NdbScanOperation::ScanOptions::SO_GETVALUE;
  options->extraGetValues= gets;
  options->numExtraGetValues= num_gets;
}

inline void
ha_ndbcluster::eventSetAnyValue(THD *thd, 
                                NdbOperation::OperationOptions *options) const
{
  options->anyValue= 0;
  if (unlikely(m_slow_path))
  {
    /*
      Ignore TNTO_NO_LOGGING for slave thd.  It is used to indicate
      log-slave-updates option.  This is instead handled in the
      injector thread, by looking explicitly at the
      opt_log_slave_updates flag.
    */
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    if (thd->slave_thread)
    {
      /*
        Slave-thread, we are applying a replicated event.
        We set the server_id to the value received from the log which
        may be a composite of server_id and other data according
        to the server_id_bits option.
        In future it may be useful to support *not* mapping composite
        AnyValues to/from Binlogged server-ids
      */
      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      options->anyValue = thd_unmasked_server_id(thd);
    }
    else if (thd_ndb->check_trans_option(Thd_ndb::TRANS_NO_LOGGING))
    {
      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_nologging(options->anyValue);
    }
  }
#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("ndb_set_reflect_anyvalue", true, false))
  {
      fprintf(stderr, "Ndb forcing reflect AnyValue\n");
      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_reflect_op(options->anyValue);
  }
  if (DBUG_EVALUATE_IF("ndb_set_refresh_anyvalue", true, false))
  {
    fprintf(stderr, "Ndb forcing refresh AnyValue\n");
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    ndbcluster_anyvalue_set_refresh_op(options->anyValue);
  }
  
  /*
    MySQLD will set the user-portion of AnyValue (if any) to all 1s
    This tests code filtering ServerIds on the value of server-id-bits.
  */
  const char* p = getenv("NDB_TEST_ANYVALUE_USERDATA");
  if (p != 0  && *p != 0 && *p != '0' && *p != 'n' && *p != 'N')
  {
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    dbug_ndbcluster_anyvalue_set_userbits(options->anyValue);
  }
#endif
}

extern NDB_SHARE *ndb_apply_status_share;


/**
   prepare_conflict_detection

   This method is called during operation definition by the slave,
   when writing to a table with conflict detection defined.

   It is responsible for defining and adding any operation filtering
   required, and for saving any operation definition state required
   for post-execute analysis.

   For transactional detection, this method may determine that the
   operation being defined should not be executed, and conflict
   handling should occur immediately.  In this case, conflict_handled
   is set to true.
*/
int
ha_ndbcluster::prepare_conflict_detection(enum_conflicting_op_type op_type,
                                          const NdbRecord* key_rec,
                                          const NdbRecord* data_rec,
                                          const uchar* old_data,
                                          const uchar* new_data,
                                          const MY_BITMAP *write_set,
                                          NdbTransaction* trans,
                                          NdbInterpretedCode* code,
                                          NdbOperation::OperationOptions* options,
                                          bool& conflict_handled,
                                          bool& avoid_ndbapi_write)
{
  DBUG_ENTER("prepare_conflict_detection");
  THD* thd = table->in_use;
  int res = 0;
  assert(thd->slave_thread);

  conflict_handled = false;

  /*
    Special check for apply_status table, as we really don't want
    to do any special handling with it
  */
  if (unlikely(m_share == ndb_apply_status_share))
  {
    DBUG_RETURN(0);
  }

  /*
     Check transaction id first, as in transactional conflict detection,
     the transaction id is what eventually dictates whether an operation
     is applied or not.

     Not that this applies even if the current operation's table does not
     have a conflict function defined - if a transaction spans a 'transactional
     conflict detection' table and a non transactional table, the non-transactional
     table's data will also be reverted.
  */
  Uint64 transaction_id = Ndb_binlog_extra_row_info::InvalidTransactionId;
  bool op_is_marked_as_read= false;
  bool op_is_marked_as_reflected= false;
  // Only used for sanity check and debug printout
  bool op_is_marked_as_refresh MY_ATTRIBUTE((unused))= false;

  if (thd->binlog_row_event_extra_data)
  {
    Ndb_binlog_extra_row_info extra_row_info;
    if (extra_row_info.loadFromBuffer(thd->binlog_row_event_extra_data) != 0)
    {
      ndb_log_warning("NDB Slave: Malformed event received on table %s "
                      "cannot parse.  Stopping Slave.",
                      m_share->key_string());
      DBUG_RETURN( ER_SLAVE_CORRUPT_EVENT );
    }

    if (extra_row_info.getFlags() &
        Ndb_binlog_extra_row_info::NDB_ERIF_TRANSID)
    {
      transaction_id = extra_row_info.getTransactionId();
    }

    if (extra_row_info.getFlags() &
        Ndb_binlog_extra_row_info::NDB_ERIF_CFT_FLAGS)
    {
      const Uint16 conflict_flags = extra_row_info.getConflictFlags();
      DBUG_PRINT("info", ("conflict flags : %x\n", conflict_flags));

      if (conflict_flags & NDB_ERIF_CFT_REFLECT_OP)
      {
        op_is_marked_as_reflected= true;
        g_ndb_slave_state.current_reflect_op_prepare_count++;
      }
      
      if (conflict_flags & NDB_ERIF_CFT_REFRESH_OP)
      {
        op_is_marked_as_refresh= true;
        g_ndb_slave_state.current_refresh_op_count++;
      }

      if (conflict_flags & NDB_ERIF_CFT_READ_OP)
      {
        op_is_marked_as_read= true;
      }

      /* Sanity - 1 flag at a time at most */
      assert(! (op_is_marked_as_reflected &&
                op_is_marked_as_refresh));
      assert(! (op_is_marked_as_read &&
                (op_is_marked_as_reflected ||
                 op_is_marked_as_refresh)));
    }
  }

  const st_conflict_fn_def* conflict_fn = (m_share->m_cfn_share?
                                           m_share->m_cfn_share->m_conflict_fn:
                                           NULL);

  bool pass_mode = false;
  if (conflict_fn)
  {
    /* Check Slave Conflict Role Variable setting */
    if (conflict_fn->flags & CF_USE_ROLE_VAR)
    {
      switch (opt_ndb_slave_conflict_role)
      {
      case SCR_NONE:
      {
        ndb_log_warning("NDB Slave: Conflict function %s defined on "
                        "table %s requires ndb_slave_conflict_role variable "
                        "to be set.  Stopping slave.",
                        conflict_fn->name,
                        m_share->key_string());
        DBUG_RETURN(ER_SLAVE_CONFIGURATION);
      }
      case SCR_PASS:
      {
        pass_mode = true;
      }
      default:
        /* PRIMARY, SECONDARY */
        break;
      }
    }
  }

  {
    bool handle_conflict_now = false;
    const uchar* row_data = (op_type == WRITE_ROW? new_data : old_data);
    int res = g_ndb_slave_state.atPrepareConflictDetection(m_table,
                                                           key_rec,
                                                           row_data,
                                                           transaction_id,
                                                           handle_conflict_now);
    if (res)
      DBUG_RETURN(res);

    if (handle_conflict_now)
    {
      DBUG_PRINT("info", ("Conflict handling for row occurring now"));
      NdbError noRealConflictError;
      /*
       * If the user operation was a read and we receive an update
       * log event due to an AnyValue update, then the conflicting operation
       * should be reported as a read. 
       */
      enum_conflicting_op_type conflicting_op=
        (op_type == UPDATE_ROW && op_is_marked_as_read)?
        READ_ROW
        : op_type;
      /*
         Directly handle the conflict here - e.g refresh/ write to
         exceptions table etc.
      */
      res = handle_row_conflict(m_share->m_cfn_share,
                                m_share->table_name,
                                "Transaction",
                                key_rec,
                                data_rec,
                                old_data,
                                new_data,
                                conflicting_op,
                                TRANS_IN_CONFLICT,
                                noRealConflictError,
                                trans,
                                write_set,
                                transaction_id);
      if (unlikely(res))
        DBUG_RETURN(res);

      g_ndb_slave_state.conflict_flags |= SCS_OPS_DEFINED;

      /*
        Indicate that there (may be) some more operations to
        execute before committing
      */
      m_thd_ndb->m_unsent_bytes+= 12;
      conflict_handled = true;
      DBUG_RETURN(0);
    }
  }

  if (conflict_fn == NULL ||
      pass_mode)
  {
    /* No conflict function definition required */
    DBUG_RETURN(0);
  }

  /**
   * By default conflict algorithms use the 'natural' NdbApi ops 
   * (insert/update/delete) which can detect presence anomalies, 
   * as opposed to NdbApi write which ignores them.
   * However in some cases, we want to use NdbApi write to apply 
   * events received on tables with conflict detection defined 
   * (e.g. when we want to forcibly align a row with a refresh op).
   */
  avoid_ndbapi_write = true;

  if (unlikely((conflict_fn->flags & CF_TRANSACTIONAL) &&
               (transaction_id == Ndb_binlog_extra_row_info::InvalidTransactionId)))
  {
    ndb_log_warning("NDB Slave: Transactional conflict detection defined on "
                    "table %s, but events received without transaction ids.  "
                    "Check --ndb-log-transaction-id setting on "
                    "upstream Cluster.",
                    m_share->key_string());
    /* This is a user error, but we want them to notice, so treat seriously */
    DBUG_RETURN( ER_SLAVE_CORRUPT_EVENT );
  }

  /**
   * Normally, update and delete have an attached program executed against
   * the existing row content.  Insert (and NdbApi write) do not.  
   * Insert cannot as there is no pre-existing row to examine (and therefore
   * no non prepare-time deterministic decisions to make).
   * NdbApi Write technically could if the row already existed, but this is 
   * not currently supported by NdbApi.
   */
  bool prepare_interpreted_program = (op_type != WRITE_ROW);

  if (conflict_fn->flags & CF_REFLECT_SEC_OPS)
  {
    /* This conflict function reflects secondary ops at the Primary */
    
    if (opt_ndb_slave_conflict_role == SCR_PRIMARY)
    {
      /**
       * Here we mark the applied operations to indicate that they
       * should be reflected back to the SECONDARY cluster.
       * This is required so that :
       *   1.  They are given local Binlog Event source serverids
       *       and so will pass through to the storage engine layer
       *       on the SECONDARY.
       *       (Normally they would be filtered in the Slave IO thread
       *        as having returned-to-source)
       *
       *   2.  They can be tagged as reflected so that the SECONDARY
       *       can handle them differently
       *       (They are force-applied)
       */
      DBUG_PRINT("info", ("Setting AnyValue to reflect secondary op"));

      options->optionsPresent |=
        NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_reflect_op(options->anyValue);
    }
    else if (opt_ndb_slave_conflict_role == SCR_SECONDARY)
    {
      /**
       * On the Secondary, we receive reflected operations which
       * we want to attempt to apply under certain conditions.
       * This is done to recover from situations where
       * both PRIMARY and SECONDARY have performed concurrent
       * DELETEs.
       * 
       * For non reflected operations we want to apply Inserts and
       * Updates using write_tuple() to get an idempotent effect
       */
      if (op_is_marked_as_reflected)
      {
        /**
         * Apply operations using their 'natural' operation types
         * with interpreted programs attached where appropriate.
         * Natural operation types used so that we become aware
         * of any 'presence' issues (row does/not exist).
         */
        DBUG_PRINT("info", ("Reflected operation"));
      }
      else
      {
        /**
         * Either a normal primary sourced change, or a refresh
         * operation.
         * In both cases we want to apply the operation idempotently,
         * and there's no need for an interpreted program.
         * e.g.
         *   WRITE_ROW  -> NdbApi write_row
         *   UPDATE_ROW -> NdbApi write_row
         *   DELETE_ROW -> NdbApi delete_row
         *
         * NdbApi write_row does not fail.
         * NdbApi delete_row will complain if the row does not exist
         * but this will be ignored
         */
        DBUG_PRINT("info", ("Allowing use of NdbApi write_row "
                            "for non reflected op (%u)",
                            op_is_marked_as_refresh));
        prepare_interpreted_program = false;
        avoid_ndbapi_write = false;
      }
    }
  }

  /*
     Prepare interpreted code for operation (update + delete only) according
     to algorithm used
  */
  if (prepare_interpreted_program)
  {
    res = conflict_fn->prep_func(m_share->m_cfn_share,
                                 op_type,
                                 m_ndb_record,
                                 old_data,
                                 new_data,
                                 table->read_set,  // Before image
                                 table->write_set, // After image
                                 code);

    if (res == 0)
    {
      if (code->getWordsUsed() > 0)
      {
        /* Attach conflict detecting filter program to operation */
        options->optionsPresent|=
          NdbOperation::OperationOptions::OO_INTERPRETED;
        options->interpretedCode= code;
      }
    }
    else
    {
      ndb_log_warning("NDB Slave: Binlog event on table %s missing "
                      "info necessary for conflict detection.  "
                      "Check binlog format options on upstream cluster.",
                      m_share->key_string());
      DBUG_RETURN( ER_SLAVE_CORRUPT_EVENT);
    }
  } // if (op_type != WRITE_ROW)

  g_ndb_slave_state.conflict_flags |= SCS_OPS_DEFINED;

  /* Now save data for potential insert to exceptions table... */
  Ndb_exceptions_data ex_data;
  ex_data.share= m_share;
  ex_data.key_rec= key_rec;
  ex_data.data_rec= data_rec;
  ex_data.op_type= op_type;
  ex_data.reflected_operation = op_is_marked_as_reflected;
  ex_data.trans_id= transaction_id;
  /*
    We need to save the row data for possible conflict resolution after
    execute().
  */
  if (old_data)
    ex_data.old_row= copy_row_to_buffer(m_thd_ndb, old_data);
  if (old_data != NULL && ex_data.old_row == NULL)
  {
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }
  if (new_data)
    ex_data.new_row= copy_row_to_buffer(m_thd_ndb, new_data);
  if (new_data !=  NULL && ex_data.new_row == NULL)
  {
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  ex_data.bitmap_buf= NULL;
  ex_data.write_set= NULL;
  if (table->write_set)
  {
    /* Copy table write set */
    ex_data.bitmap_buf=
      (my_bitmap_map *) get_buffer(m_thd_ndb, table->s->column_bitmap_size);
    if (ex_data.bitmap_buf == NULL)
    {
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
    ex_data.write_set= (MY_BITMAP*) get_buffer(m_thd_ndb, sizeof(MY_BITMAP));
    if (ex_data.write_set == NULL)
    {
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
    bitmap_init(ex_data.write_set, ex_data.bitmap_buf,
                table->write_set->n_bits, false);
    bitmap_copy(ex_data.write_set, table->write_set);
  }

  uchar* ex_data_buffer= get_buffer(m_thd_ndb, sizeof(ex_data));
  if (ex_data_buffer == NULL)
  {
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }
  memcpy(ex_data_buffer, &ex_data, sizeof(ex_data));

  /* Store ptr to exceptions data in operation 'customdata' ptr */
  options->optionsPresent|= NdbOperation::OperationOptions::OO_CUSTOMDATA;
  options->customData= (void*)ex_data_buffer;

  DBUG_RETURN(0);
}

/**
   handle_conflict_op_error

   This method is called when an error is detected after executing an
   operation with conflict detection active.

   If the operation error is related to conflict detection, handling
   starts.

   Handling involves incrementing the relevant counter, and optionally
   refreshing the row and inserting an entry into the exceptions table
*/

static int
handle_conflict_op_error(NdbTransaction* trans,
                         const NdbError& err,
                         const NdbOperation* op)
{
  DBUG_ENTER("handle_conflict_op_error");
  DBUG_PRINT("info", ("ndb error: %d", err.code));

  if ((err.code == (int) error_conflict_fn_violation) ||
      (err.code == (int) error_op_after_refresh_op) ||
      (err.classification == NdbError::ConstraintViolation) ||
      (err.classification == NdbError::NoDataFound))
  {
    DBUG_PRINT("info",
               ("err.code = %s, err.classification = %s",
               ((err.code == (int) error_conflict_fn_violation)?
                "error_conflict_fn_violation":
                ((err.code == (int) error_op_after_refresh_op)?
                 "error_op_after_refresh_op" : "?")),
               ((err.classification == NdbError::ConstraintViolation)?
                "ConstraintViolation":
                ((err.classification == NdbError::NoDataFound)?
                 "NoDataFound" : "?"))));

    enum_conflict_cause conflict_cause;

    /* Map cause onto our conflict description type */
    if ((err.code == (int) error_conflict_fn_violation) ||
        (err.code == (int) error_op_after_refresh_op))
    {
      DBUG_PRINT("info", ("ROW_IN_CONFLICT"));
      conflict_cause= ROW_IN_CONFLICT;
    }
    else if (err.classification == NdbError::ConstraintViolation)
    {
      DBUG_PRINT("info", ("ROW_ALREADY_EXISTS"));
      conflict_cause= ROW_ALREADY_EXISTS;
    }
    else
    {
      assert(err.classification == NdbError::NoDataFound);
      DBUG_PRINT("info", ("ROW_DOES_NOT_EXIST"));
      conflict_cause= ROW_DOES_NOT_EXIST;
    }

    /* Get exceptions data from operation */
    const void* buffer=op->getCustomData();
    assert(buffer);
    Ndb_exceptions_data ex_data;
    memcpy(&ex_data, buffer, sizeof(ex_data));
    NDB_SHARE *share= ex_data.share;
    NDB_CONFLICT_FN_SHARE* cfn_share= share ? share->m_cfn_share : NULL;

    const NdbRecord* key_rec= ex_data.key_rec;
    const NdbRecord* data_rec= ex_data.data_rec;
    const uchar* old_row= ex_data.old_row;
    const uchar* new_row= ex_data.new_row;
#ifndef DBUG_OFF
    const uchar* row=
      (ex_data.op_type == DELETE_ROW)?
      ex_data.old_row : ex_data.new_row;
#endif
    enum_conflicting_op_type causing_op_type= ex_data.op_type;
    const MY_BITMAP *write_set= ex_data.write_set;

    DBUG_PRINT("info", ("Conflict causing op type : %u",
                        causing_op_type));

    if (causing_op_type == REFRESH_ROW)
    {
      /*
         The failing op was a refresh row, we require that it
         failed due to being a duplicate (e.g. a refresh
         occurring on a refreshed row)
       */
      if (err.code == (int) error_op_after_refresh_op)
      {
        DBUG_PRINT("info", ("Operation after refresh - ignoring"));
        DBUG_RETURN(0);
      }
      else
      {
        DBUG_PRINT("info", ("Refresh op hit real error %u", err.code));
        /* Unexpected error, normal handling*/
        DBUG_RETURN(err.code);
      }
    }

    if (ex_data.reflected_operation)
    {
      DBUG_PRINT("info", ("Reflected operation error : %u.",
                          err.code));
      
      /**
       * Expected cases are :
       *   Insert : Row already exists :      Don't care - discard
       *              Secondary has this row, or a future version
       *  
       *   Update : Row does not exist :      Don't care - discard
       *              Secondary has deleted this row later.
       *
       *            Conflict
       *            (Row written here last) : Don't care - discard
       *              Secondary has this row, or a future version
       * 
       *   Delete : Row does not exist :      Don't care - discard
       *              Secondary has deleted this row later.
       * 
       *            Conflict
       *            (Row written here last) : Don't care - discard
       *              Secondary has a future version of this row
       *
       *   Presence and authorship conflicts are used to determine
       *   whether to apply a reflecte operation.
       *   The presence checks avoid divergence and the authorship
       *   checks avoid all actions being applied in delayed 
       *   duplicate.
       */
      assert((err.code == (int) error_conflict_fn_violation) ||
             (err.classification == NdbError::ConstraintViolation) ||
             (err.classification == NdbError::NoDataFound));
      
      g_ndb_slave_state.current_reflect_op_discard_count++;

      DBUG_RETURN(0);
    }

    {
      /**
       * For asymmetric algorithms that use the ROLE variable to 
       * determine their role, we check whether we are on the
       * SECONDARY cluster.
       * This is far as we want to process conflicts on the
       * SECONDARY.
       */
      bool secondary = cfn_share &&
        cfn_share->m_conflict_fn &&
        (cfn_share->m_conflict_fn->flags & CF_USE_ROLE_VAR) &&
        (opt_ndb_slave_conflict_role == SCR_SECONDARY);
      
      if (secondary)
      {
        DBUG_PRINT("info", ("Conflict detected, on secondary - ignore"));
        DBUG_RETURN(0);
      }
    }

    DBUG_ASSERT(share != NULL && row != NULL);
    bool table_has_trans_conflict_detection =
      cfn_share &&
      cfn_share->m_conflict_fn &&
      (cfn_share->m_conflict_fn->flags & CF_TRANSACTIONAL);

    if (table_has_trans_conflict_detection)
    {
      /* Mark this transaction as in-conflict.
       * For Delete-NoSuchRow (aka Delete-Delete) conflicts, we
       * do not always mark the transaction as in-conflict, as
       *  i) Row based algorithms cannot do so safely w.r.t batching
       * ii) NDB$EPOCH_TRANS cannot avoid divergence in any case,
       *     and so chooses to ignore such conflicts
       * So only NDB$EPOCH_TRANS2 (controlled by the CF_DEL_DEL_CFT
       * flag will mark the transaction as in-conflict due to a
       * delete of a non-existent row.
       */
      bool is_del_del_cft = ((causing_op_type == DELETE_ROW) &&
                             (conflict_cause == ROW_DOES_NOT_EXIST));
      bool fn_treats_del_del_as_cft = 
        (cfn_share->m_conflict_fn->flags & CF_DEL_DEL_CFT);
      
      if (!is_del_del_cft ||
          fn_treats_del_del_as_cft)
      {
        /* Perform special transactional conflict-detected handling */
        int res = g_ndb_slave_state.atTransConflictDetected(ex_data.trans_id);
        if (res)
          DBUG_RETURN(res);
      }
    }

    if (cfn_share)
    {
      /* Now handle the conflict on this row */
      enum_conflict_fn_type cft = cfn_share->m_conflict_fn->type;

      g_ndb_slave_state.current_violation_count[cft]++;

      int res = handle_row_conflict(cfn_share,
                                    share->table_name,
                                    "Row",
                                    key_rec,
                                    data_rec,
                                    old_row,
                                    new_row,
                                    causing_op_type,
                                    conflict_cause,
                                    err,
                                    trans,
                                    write_set,
                                    /*
                                      ORIG_TRANSID not available for
                                      non-transactional conflict detection.
                                    */
                                    Ndb_binlog_extra_row_info::InvalidTransactionId);

      DBUG_RETURN(res);
    }
    else
    {
      DBUG_PRINT("info", ("missing cfn_share"));
      DBUG_RETURN(0); // TODO : Correct?
    }
  }
  else
  {
    /* Non conflict related error */
    DBUG_PRINT("info", ("err.code == %u", err.code));
    DBUG_RETURN(err.code);
  }

  DBUG_RETURN(0); // Reachable?
}

/*
  is_serverid_local
*/
static bool is_serverid_local(Uint32 serverid)
{
  /*
     If it's not our serverid, check the
     IGNORE_SERVER_IDS setting to check if
     it's local.
  */
  return ((serverid == ::server_id) ||
          ndb_mi_get_ignore_server_id(serverid));
}


int ha_ndbcluster::write_row(uchar *record)
{
  DBUG_ENTER("ha_ndbcluster::write_row");

  if (m_share == ndb_apply_status_share && table->in_use->slave_thread)
  {
    uint32 row_server_id, master_server_id= ndb_mi_get_master_server_id();
    uint64 row_epoch;
    memcpy(&row_server_id, table->field[0]->ptr + (record - table->record[0]),
           sizeof(row_server_id));
    memcpy(&row_epoch, table->field[1]->ptr + (record - table->record[0]),
           sizeof(row_epoch));
    int rc = g_ndb_slave_state.atApplyStatusWrite(master_server_id,
                                                  row_server_id,
                                                  row_epoch,
                                                  is_serverid_local(row_server_id));
    if (rc != 0)
    {
      /* Stop Slave */
      DBUG_RETURN(rc);
    }
  }

  DBUG_RETURN(ndb_write_row(record, false, false));
}

/**
  Insert one record into NDB
*/
int ha_ndbcluster::ndb_write_row(uchar *record,
                                 bool primary_key_update,
                                 bool batched_update)
{
  bool has_auto_increment;
  const NdbOperation *op;
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= m_thd_ndb;
  NdbTransaction *trans;
  uint32 part_id;
  int error= 0;
  Uint64 auto_value;
  longlong func_value= 0;
  const Uint32 authorValue = 1;
  NdbOperation::SetValueSpec sets[3];
  Uint32 num_sets= 0;
  DBUG_ENTER("ha_ndbcluster::ndb_write_row");

  error = check_slave_state(thd);
  if (unlikely(error))
    DBUG_RETURN(error);

  has_auto_increment= (table->next_number_field && record == table->record[0]);

  if (has_auto_increment && table_share->primary_key != MAX_KEY) 
  {
    /*
     * Increase any auto_incremented primary key
     */
    m_skip_auto_increment= false;
    if ((error= update_auto_increment()))
      DBUG_RETURN(error);
    m_skip_auto_increment= (insert_id_for_cur_row == 0 ||
                            thd->auto_inc_intervals_forced.nb_elements());
  }

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys
   */
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in start_bulk_insert()
      start_bulk_insert will set parameters to ensure that each
      write_row is committed individually
    */
    int peek_res= peek_indexed_rows(record, NDB_INSERT);
    
    if (!peek_res) 
    {
      error= HA_ERR_FOUND_DUPP_KEY;
    }
    else if (peek_res != HA_ERR_KEY_NOT_FOUND)
    {
      error= peek_res;
    }
    if (error)
    {
      if ((has_auto_increment) && (m_skip_auto_increment))
      {
        int ret_val;
        if ((ret_val= set_auto_inc(thd, table->next_number_field)))
        {
          DBUG_RETURN(ret_val);
        }
      }
      m_skip_auto_increment= true;
      DBUG_RETURN(error);
    }
  }

  bool uses_blobs= uses_blob_value(table->write_set);

  const NdbRecord *key_rec;
  const uchar *key_row;
  if (table_share->primary_key == MAX_KEY)
  {
    /* Table has hidden primary key. */
    Ndb *ndb= get_ndb(thd);
    uint retries= NDB_AUTO_INCREMENT_RETRIES;
    int retry_sleep= 30; /* 30 milliseconds, transaction */
    for (;;)
    {
      NDB_SHARE::Tuple_id_range_guard g(m_share);
      if (ndb->getAutoIncrementValue(m_table, g.range, auto_value, 1000) == -1)
      {
	if (--retries && !thd->killed &&
	    ndb->getNdbError().status == NdbError::TemporaryError)
	{
          ndb_retry_sleep(retry_sleep);
	  continue;
	}
	ERR_RETURN(ndb->getNdbError());
      }
      break;
    }
    sets[num_sets].column= get_hidden_key_column();
    sets[num_sets].value= &auto_value;
    num_sets++;
    key_rec= m_ndb_hidden_key_record;
    key_row= (const uchar *)&auto_value;
  }
  else
  {
    key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
    key_row= record;
  }

  trans= thd_ndb->trans;
  if (m_user_defined_partitioning)
  {
    DBUG_ASSERT(m_use_partition_pruning);
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (unlikely(error))
    {
      m_part_info->err_value= func_value;
      DBUG_RETURN(error);
    }
    {
      /*
        We need to set the value of the partition function value in
        NDB since the NDB kernel doesn't have easy access to the function
        to calculate the value.
      */
      if (func_value >= INT_MAX32)
        func_value= INT_MAX32;
      sets[num_sets].column= get_partition_id_column();
      sets[num_sets].value= &func_value;
      num_sets++;
    }
    if (!trans)
      if (unlikely(!(trans= start_transaction_part_id(part_id, error))))
        DBUG_RETURN(error);
  }
  else if (!trans)
  {
    if (unlikely(!(trans= start_transaction_row(key_rec, key_row, error))))
      DBUG_RETURN(error);
  }
  DBUG_ASSERT(trans);

  ha_statistic_increment(&System_status_var::ha_write_count);

  /*
     Setup OperationOptions
   */
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent=0;
  
  eventSetAnyValue(thd, &options); 
  const bool need_flush=
      thd_ndb->add_row_check_if_batch_full(m_bytes_per_write);

  if ((thd->slave_thread) &&
      (m_table->getExtraRowAuthorBits()))
  {
    /* Set author to indicate slave updated last */
    sets[num_sets].column= NdbDictionary::Column::ROW_AUTHOR;
    sets[num_sets].value= &authorValue;
    num_sets++;
  }

  if (m_user_defined_partitioning)
  {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId= part_id;
  }
  if (num_sets)
  {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_SETVALUE;
    options.extraSetValues= sets;
    options.numExtraSetValues= num_sets;
  }
  if (thd->slave_thread || THDVAR(thd, deferred_constraints))
  {
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
  {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (options.optionsPresent != 0)
    poptions=&options;

  const Uint32 bitmapSz= (NDB_MAX_ATTRIBUTES_IN_TABLE + 31)/32;
  uint32 tmpBitmapSpace[bitmapSz];
  MY_BITMAP tmpBitmap;
  MY_BITMAP *user_cols_written_bitmap;
  bool avoidNdbApiWriteOp = false; /* ndb_write_row defaults to write */

  /* Conflict resolution in slave thread */
  if (thd->slave_thread)
  {
    bool conflict_handled = false;

    if (unlikely((error = prepare_conflict_detection(WRITE_ROW,
                                                     key_rec,
                                                     m_ndb_record,
                                                     NULL,    /* old_data */
                                                     record,  /* new_data */
                                                     table->write_set,
                                                     trans,
                                                     NULL,    /* code */
                                                     &options,
                                                     conflict_handled,
                                                     avoidNdbApiWriteOp))))
      DBUG_RETURN(error);

    if (unlikely(conflict_handled))
    {
      /* No need to continue with operation definition */
      /* TODO : Ensure batch execution */
      DBUG_RETURN(0);
    }
  };

  if (m_use_write &&
      !avoidNdbApiWriteOp)
  {
    uchar* mask;

    if (applying_binlog(thd))
    {
      /*
        Use write_set when applying binlog to avoid trampling
        unchanged columns
      */
      user_cols_written_bitmap= table->write_set;
      mask= m_table_map->get_column_mask(user_cols_written_bitmap);
    }
    else
    {
      /* Ignore write_set for REPLACE command */
      user_cols_written_bitmap= NULL;
      mask= NULL;
    }
    /* TODO : Add conflict detection etc when interpreted write supported */
    op= trans->writeTuple(key_rec, (const char *)key_row, m_ndb_record,
                          (char *)record, mask,
                          poptions, sizeof(NdbOperation::OperationOptions));
  }
  else
  {
    uchar *mask;

    /* Check whether Ndb table definition includes any default values. */
    if (m_table->hasDefaultValues())
    {
      DBUG_PRINT("info", ("Not sending values for native defaulted columns"));

      /*
        If Ndb is unaware of the table's defaults, we must provide all column values to the insert.  
        This is done using a NULL column mask.
        If Ndb is aware of the table's defaults, we only need to provide 
        the columns explicitly mentioned in the write set, 
        plus any extra columns required due to bug#41616. 
        plus the primary key columns required due to bug#42238.
      */
      /*
        The following code for setting user_cols_written_bitmap
        should be removed after BUG#41616 and Bug#42238 are fixed
      */
      /* Copy table write set so that we can add to it */
      user_cols_written_bitmap= &tmpBitmap;
      bitmap_init(user_cols_written_bitmap, tmpBitmapSpace,
                  table->write_set->n_bits, false);
      bitmap_copy(user_cols_written_bitmap, table->write_set);

      for (uint i= 0; i < table->s->fields; i++)
      {
        Field *field= table->field[i];
        DBUG_PRINT("info", ("Field#%u, (%u), Type : %u "
                            "NO_DEFAULT_VALUE_FLAG : %u PRI_KEY_FLAG : %u",
                            i, 
                            field->field_index,
                            field->real_type(),
                            field->flags & NO_DEFAULT_VALUE_FLAG,
                            field->flags & PRI_KEY_FLAG));
        if ((field->flags & (NO_DEFAULT_VALUE_FLAG | // bug 41616
                             PRI_KEY_FLAG)) ||       // bug 42238
            ! type_supports_default_value(field->real_type()))
        {
          bitmap_set_bit(user_cols_written_bitmap, field->field_index);
        }
      }
      /* Finally, translate the whole bitmap from MySQL field numbers 
         to NDB column numbers */
      mask= m_table_map->get_column_mask(user_cols_written_bitmap);
    }
    else
    {
      /* No defaults in kernel, provide all columns ourselves */
      DBUG_PRINT("info", ("No native defaults, sending all values"));
      user_cols_written_bitmap= NULL;
      mask = NULL;
    }
      
    /* Using insert, we write all non default columns */
    op= trans->insertTuple(key_rec, (const char *)key_row, m_ndb_record,
                           (char *)record, mask, // Default value should be masked
                           poptions, sizeof(NdbOperation::OperationOptions));
  }
  if (!(op))
    ERR_RETURN(trans->getNdbError());

  bool do_batch= !need_flush &&
    (batched_update || thd_allow_batch(thd));
  uint blob_count= 0;
  if (table_share->blob_fields > 0)
  {
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    /* Set Blob values for all columns updated by the operation */
    int res= set_blob_values(op, record - table->record[0],
                             user_cols_written_bitmap, &blob_count, do_batch);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (res != 0)
      DBUG_RETURN(res);
  }

  /*
    Execute write operation
    NOTE When doing inserts with many values in 
    each INSERT statement it should not be necessary
    to NoCommit the transaction between each row.
    Find out how this is detected!
  */
  m_rows_inserted++;
  no_uncommitted_rows_update(1);
  if (( (m_rows_to_insert == 1 || uses_blobs) && !do_batch ) ||
      primary_key_update ||
      need_flush)
  {
    const int res = flush_bulk_insert();
    if (res != 0)
    {
      m_skip_auto_increment= true;
      DBUG_RETURN(res);
    }
  }
  if ((has_auto_increment) && (m_skip_auto_increment))
  {
    int ret_val;
    if ((ret_val= set_auto_inc(thd, table->next_number_field)))
    {
      DBUG_RETURN(ret_val);
    }
  }
  m_skip_auto_increment= true;

  DBUG_PRINT("exit",("ok"));
  DBUG_RETURN(0);
}


/* Compare if an update changes the primary key in a row. */
int ha_ndbcluster::primary_key_cmp(const uchar * old_row, const uchar * new_row)
{
  uint keynr= table_share->primary_key;
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].user_defined_key_parts;

  for (; key_part != end ; key_part++)
  {
    if (!bitmap_is_set(table->write_set, key_part->fieldnr - 1))
      continue;

    /* The primary key does not allow NULLs. */
    DBUG_ASSERT(!key_part->null_bit);

    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {

      if (key_part->field->cmp_binary((old_row + key_part->offset),
                                      (new_row + key_part->offset),
                                      (ulong) key_part->length))
        return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
                 key_part->length))
        return 1;
    }
  }
  return 0;
}


static Ndb_exceptions_data StaticRefreshExceptionsData=
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, REFRESH_ROW, false, 0 };

static int
handle_row_conflict(NDB_CONFLICT_FN_SHARE* cfn_share,
                    const char* table_name,
                    const char* handling_type,
                    const NdbRecord* key_rec,
                    const NdbRecord* data_rec,
                    const uchar* old_row,
                    const uchar* new_row,
                    enum_conflicting_op_type op_type,
                    enum_conflict_cause conflict_cause,
                    const NdbError& conflict_error,
                    NdbTransaction* conflict_trans,
                    const MY_BITMAP *write_set,
                    Uint64 transaction_id)
{
  DBUG_ENTER("handle_row_conflict");

  const uchar* row = (op_type == DELETE_ROW)? old_row : new_row;
  /*
     We will refresh the row if the conflict function requires
     it, or if we are handling a transactional conflict.
  */
  bool refresh_row =
    (conflict_cause == TRANS_IN_CONFLICT) ||
    (cfn_share &&
     (cfn_share->m_flags & CFF_REFRESH_ROWS));

  if (refresh_row)
  {
    /* A conflict has been detected between an applied replicated operation
     * and the data in the DB.
     * The attempt to change the local DB will have been rejected.
     * We now take steps to generate a refresh Binlog event so that
     * other clusters will be re-aligned.
     */
    DBUG_PRINT("info", ("Conflict on table %s.  Operation type : %s, "
                        "conflict cause :%s, conflict error : %u : %s",
                        table_name,
                        ((op_type == WRITE_ROW)? "WRITE_ROW":
                         (op_type == UPDATE_ROW)? "UPDATE_ROW":
                         "DELETE_ROW"),
                        ((conflict_cause == ROW_ALREADY_EXISTS)?"ROW_ALREADY_EXISTS":
                         (conflict_cause == ROW_DOES_NOT_EXIST)?"ROW_DOES_NOT_EXIST":
                         "ROW_IN_CONFLICT"),
                        conflict_error.code,
                        conflict_error.message));

    assert(key_rec != NULL);
    assert(row != NULL);

    do
    {
      /* When the slave splits an epoch into batches, a conflict row detected
       * and refreshed in an early batch can be written to by operations in
       * a later batch.  As the operations will not have applied, and the
       * row has already been refreshed, we need not attempt to refresh
       * it again
       */
      if ((conflict_cause == ROW_IN_CONFLICT) &&
          (conflict_error.code == (int) error_op_after_refresh_op))
      {
        /* Attempt to apply an operation after the row was refreshed
         * Ignore the error
         */
        DBUG_PRINT("info", ("Operation after refresh error - ignoring"));
        break;
      }

      /**
       * Delete - NoSuchRow conflicts (aka Delete-Delete conflicts)
       *
       * Row based algorithms + batching :
       * When a delete operation finds that the row does not exist, it indicates
       * a DELETE vs DELETE conflict.  If we refresh the row then we can get
       * non deterministic behaviour depending on slave batching as follows :
       *   Row is deleted
       *
       *     Case 1
       *       Slave applied DELETE, INSERT in 1 batch
       *
       *         After first batch, the row is present (due to INSERT), it is
       *         refreshed.
       *
       *     Case 2
       *       Slave applied DELETE in 1 batch, INSERT in 2nd batch
       *
       *         After first batch, the row is not present, it is refreshed
       *         INSERT is then rejected.
       *
       * The problem of not being able to 'record' a DELETE vs DELETE conflict
       * is known.  We attempt at least to give consistent behaviour for
       * DELETE vs DELETE conflicts by :
       *   NOT refreshing a row when a DELETE vs DELETE conflict is detected
       * This should map all batching scenarios onto Case1.
       * 
       * Transactional algorithms
       * 
       * For transactional algorithms, there are multiple passes over the
       * epoch transaction.  Earlier passes 'mark' in-conflict transactions
       * so that any row changes to in-conflict rows are automatically
       * in-conflict.  Therefore the batching problem above is avoided.
       *
       * NDB$EPOCH_TRANS chooses to ignore DELETE-DELETE conflicts entirely
       * and so skips refreshing rows with only DELETE-DELETE conflicts.
       * NDB$EPOCH2_TRANS does not ignore them, and so refreshes them.
       * This behaviour is controlled by the algorthm's CF_DEL_DEL_CFT 
       * flag at conflict detection time.
       * 
       * For the final pass of the transactional algorithms, every conflict
       * is a TRANS_IN_CONFLICT error here, so no need to adjust behaviour.
       * 
       */
      if ((op_type == DELETE_ROW) &&
          (conflict_cause == ROW_DOES_NOT_EXIST))
      {
        g_ndb_slave_state.current_delete_delete_count++;
        DBUG_PRINT("info", ("Delete vs Delete detected, NOT refreshing"));
        break;
      }

      /*
        We give the refresh operation some 'exceptions data', so that
        it can be identified as part of conflict resolution when
        handling operation errors.
        Specifically we need to be able to handle duplicate row
        refreshes.
        As there is no unique exceptions data, we use a singleton.

        We also need to 'force' the ANYVALUE of the row to 0 to
        indicate that the refresh is locally-sourced.
        Otherwise we can 'pickup' the ANYVALUE of a previous
        update to the row.
        If some previous update in this transaction came from a
        Slave, then using its ANYVALUE can result in that Slave
        ignoring this correction.
      */
      NdbOperation::OperationOptions options;
      options.optionsPresent =
        NdbOperation::OperationOptions::OO_CUSTOMDATA |
        NdbOperation::OperationOptions::OO_ANYVALUE;
      options.customData = &StaticRefreshExceptionsData;
      options.anyValue = 0;

      /* Use AnyValue to indicate that this is a refreshTuple op */
      ndbcluster_anyvalue_set_refresh_op(options.anyValue);

      /* Create a refresh to operation to realign other clusters */
      // TODO Do we ever get non-PK key?
      //      Keyless table?
      //      Unique index
      const NdbOperation* refresh_op= conflict_trans->refreshTuple(key_rec,
                                                                   (const char*) row,
                                                                   &options,
                                                                   sizeof(options));
      if (!refresh_op)
      {
        NdbError err = conflict_trans->getNdbError();

        if (err.status == NdbError::TemporaryError)
        {
          /* Slave will roll back and retry entire transaction. */
          ERR_RETURN(err);
        }
        else
        {
          char msg[FN_REFLEN];

          /* We cannot refresh a row which has Blobs, as we do not support
           * Blob refresh yet.
           * Rows implicated by a transactional conflict function may have
           * Blobs.
           * We will generate an error in this case
           */
          const int NDBAPI_ERR_REFRESH_ON_BLOB_TABLE = 4343;
          if (err.code == NDBAPI_ERR_REFRESH_ON_BLOB_TABLE)
          {
            // Generate legacy error message instead of using
            // the error code and message returned from NdbApi
            snprintf(msg, sizeof(msg),
                     "%s conflict handling on table %s failed as table "
                     "has Blobs which cannot be refreshed.",
                     handling_type,
                     table_name);

            push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                                ER_EXCEPTIONS_WRITE_ERROR,
                                ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR),
                                msg);

            DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
          }

          snprintf(msg, sizeof(msg), "Row conflict handling "
                      "on table %s hit Ndb error %d '%s'",
                      table_name,
                      err.code,
                      err.message);
          push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                              ER_EXCEPTIONS_WRITE_ERROR,
                              ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR),
                              msg);
          /* Slave will stop replication. */
          DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
        }
      }
    } while(0); // End of 'refresh' block
  }

  DBUG_PRINT("info", ("Table %s does%s have an exceptions table",
                      table_name,
                      (cfn_share && cfn_share->m_ex_tab_writer.hasTable())
                      ? "" : " not"));
  if (cfn_share &&
      cfn_share->m_ex_tab_writer.hasTable())
  {
    NdbError err;
    if (cfn_share->m_ex_tab_writer.writeRow(conflict_trans,
                                            key_rec,
                                            data_rec,
                                            ::server_id,
                                            ndb_mi_get_master_server_id(),
                                            g_ndb_slave_state.current_master_server_epoch,
                                            old_row,
                                            new_row,
                                            op_type,
                                            conflict_cause,
                                            transaction_id,
                                            write_set,
                                            err) != 0)
    {
      if (err.code != 0)
      {
        if (err.status == NdbError::TemporaryError)
        {
          /* Slave will roll back and retry entire transaction. */
          ERR_RETURN(err);
        }
        else
        {
          char msg[FN_REFLEN];
          snprintf(msg, sizeof(msg), "%s conflict handling "
                      "on table %s hit Ndb error %d '%s'",
                      handling_type,
                      table_name,
                      err.code,
                      err.message);
          push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                              ER_EXCEPTIONS_WRITE_ERROR,
                              ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR),
                              msg);
          /* Slave will stop replication. */
          DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
        }
      }
    }
  } /* if (cfn_share->m_ex_tab != NULL) */

  DBUG_RETURN(0);
}


/**
  Update one record in NDB using primary key.
*/

bool ha_ndbcluster::start_bulk_update()
{
  DBUG_ENTER("ha_ndbcluster::start_bulk_update");
  if (!m_use_write && m_ignore_dup_key)
  {
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

int ha_ndbcluster::bulk_update_row(const uchar *old_data, uchar *new_data,
                                   uint *dup_key_found)
{
  DBUG_ENTER("ha_ndbcluster::bulk_update_row");
  *dup_key_found= 0;
  DBUG_RETURN(ndb_update_row(old_data, new_data, 1));
}

int ha_ndbcluster::exec_bulk_update(uint *dup_key_found)
{
  NdbTransaction* trans= m_thd_ndb->trans;
  DBUG_ENTER("ha_ndbcluster::exec_bulk_update");
  *dup_key_found= 0;

  /* If a fatal error is encountered during an update op, the error
   * is saved and exec continues. So exec_bulk_update may be called
   * even when init functions fail. Check for error conditions like
   * an uninit'ed transaction.
   */
  if(unlikely(!m_thd_ndb->trans))
  {
    DBUG_PRINT("exit", ("Transaction was not started"));
    int error = 0;
    ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
    DBUG_RETURN(error);
  }

  // m_handler must be NULL or point to _this_ handler instance
  assert(m_thd_ndb->m_handler == NULL || m_thd_ndb->m_handler == this);

  /*
   * Normal bulk update execution, driven by mysql_update() in sql_update.cc
   * - read_record calls start_transaction and inits m_thd_ndb->trans.
   * - ha_bulk_update calls ha_ndbcluster::bulk_update_row().
   * - ha_ndbcluster::bulk_update_row calls ha_ndbcluster::ndb_update_row().
   *   with flag is_bulk_update = 1.
   * - ndb_update_row sets up update, sets various flags and options,
   *   but does not execute_nocommit() because of batched exec.
   * - after read_record processes all rows, exec_bulk_update checks for
   *   rbwr and does an execute_commit() if rbwr enabled. If rbwr is
   *   enabled, exec_bulk_update does an execute_nocommit().
   * - if rbwr not enabled, execute_commit() done in ndbcluster_commit().
   */
  if (m_thd_ndb->m_handler &&
      m_read_before_write_removal_possible)
  {
    /*
      This is an autocommit involving only one table and rbwr is on

      Commit the autocommit transaction early(before the usual place
      in ndbcluster_commit) in order to:
      1) save one round trip, "no-commit+commit" converted to "commit"
      2) return the correct number of updated and affected rows
         to the update loop(which will ask handler in rbwr mode)
    */
    DBUG_PRINT("info", ("committing auto-commit+rbwr early"));
    uint ignore_count= 0;
    const int ignore_error= 1;
    if (execute_commit(m_thd_ndb, trans,
                       m_thd_ndb->m_force_send, ignore_error,
                       &ignore_count) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
    THD *thd= table->in_use;
    if (!applying_binlog(thd))
    {
      DBUG_PRINT("info", ("ignore_count: %u", ignore_count));
      assert(m_rows_updated >= ignore_count);
      m_rows_updated-= ignore_count;
    }
    DBUG_RETURN(0);
  }

  if (m_thd_ndb->m_unsent_bytes == 0)
  {
    DBUG_PRINT("exit", ("skip execute - no unsent bytes"));
    DBUG_RETURN(0);
  }

  if (thd_allow_batch(table->in_use))
  {
    /*
      Turned on by @@transaction_allow_batching=ON
      or implicitly by slave exec thread
    */
    DBUG_PRINT("exit", ("skip execute - transaction_allow_batching is ON"));
    DBUG_RETURN(0);
  }

  if (m_thd_ndb->m_handler &&
      !m_blobs_pending)
  {
    // Execute at commit time(in 'ndbcluster_commit') to save a round trip
    DBUG_PRINT("exit", ("skip execute - simple autocommit"));
    DBUG_RETURN(0);
  }

  uint ignore_count= 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  THD *thd= table->in_use;
  if (!applying_binlog(thd))
  {
    assert(m_rows_updated >= ignore_count);
    m_rows_updated-= ignore_count;
  }
  DBUG_RETURN(0);
}

void ha_ndbcluster::end_bulk_update()
{
  DBUG_ENTER("ha_ndbcluster::end_bulk_update");
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::update_row(const uchar *old_data, uchar *new_data)
{
  return ndb_update_row(old_data, new_data, 0);
}

void
ha_ndbcluster::setup_key_ref_for_ndb_record(const NdbRecord **key_rec,
                                            const uchar **key_row,
                                            const uchar *record,
                                            bool use_active_index)
{
  DBUG_ENTER("setup_key_ref_for_ndb_record");
  if (use_active_index)
  {
    /* Use unique key to access table */
    DBUG_PRINT("info", ("Using unique index (%u)", active_index));
    DBUG_ASSERT((table->key_info[active_index].flags & HA_NOSAME));
    /* Can't use key if we didn't read it first */
    DBUG_ASSERT(bitmap_is_subset(m_key_fields[active_index], table->read_set));
    *key_rec= m_index[active_index].ndb_unique_record_row;
    *key_row= record;
  }
  else if (table_share->primary_key != MAX_KEY)
  {
    /* Use primary key to access table */
    DBUG_PRINT("info", ("Using primary key"));
    /* Can't use pk if we didn't read it first */
    DBUG_ASSERT(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
    *key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
    *key_row= record;
  }
  else
  {
    /* Use hidden primary key previously read into m_ref. */
    DBUG_PRINT("info", ("Using hidden primary key (%llu)", m_ref));
    /* Can't use hidden pk if we didn't read it first */
    DBUG_ASSERT(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
    DBUG_ASSERT(m_read_before_write_removal_used == false);
    *key_rec= m_ndb_hidden_key_record;
    *key_row= (const uchar *)(&m_ref);
  }
  DBUG_VOID_RETURN;
}


/*
  Update one record in NDB using primary key
*/

int ha_ndbcluster::ndb_update_row(const uchar *old_data, uchar *new_data,
                                  int is_bulk_update)
{
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= m_thd_ndb;
  NdbScanOperation* cursor= m_active_cursor;
  const NdbOperation *op;
  uint32 old_part_id= ~uint32(0), new_part_id= ~uint32(0);
  int error;
  longlong func_value;
  Uint32 func_value_uint32;
  bool have_pk= (table_share->primary_key != MAX_KEY);
  bool pk_update= (!m_read_before_write_removal_possible &&
                   have_pk &&
                   bitmap_is_overlapping(table->write_set, m_pk_bitmap_p) &&
                   primary_key_cmp(old_data, new_data));
  bool batch_allowed= !m_update_cannot_batch && 
    (is_bulk_update || thd_allow_batch(thd));
  NdbOperation::SetValueSpec sets[2];
  Uint32 num_sets= 0;

  DBUG_ENTER("ndb_update_row");

  /* Start a transaction now if none available
   * (Manual Binlog application...)
   */
  /* TODO : Consider hinting */
  if (unlikely((!m_thd_ndb->trans) && 
               !get_transaction(error)))
  {
    DBUG_RETURN(error);
  }

  NdbTransaction *trans= m_thd_ndb->trans;
  DBUG_ASSERT(trans);

  error = check_slave_state(thd);
  if (unlikely(error))
    DBUG_RETURN(error);

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys,
   * but check that it is not part of INSERT ... ON DUPLICATE KEY UPDATE
   */
  if (m_ignore_dup_key && (thd->lex->sql_command == SQLCOM_UPDATE ||
                           thd->lex->sql_command == SQLCOM_UPDATE_MULTI))
  {
    const NDB_WRITE_OP write_op= (pk_update) ? NDB_PK_UPDATE : NDB_UPDATE;
    int peek_res= peek_indexed_rows(new_data, write_op);
    
    if (!peek_res) 
    {
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  ha_statistic_increment(&System_status_var::ha_update_count);

  bool skip_partition_for_unique_index= false;
  if (m_use_partition_pruning)
  {
    if (!cursor && m_read_before_write_removal_used)
    {
      const NDB_INDEX_TYPE type= get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for update
        without finding the partitions
      */
      if (type == UNIQUE_INDEX ||
          type == UNIQUE_ORDERED_INDEX)
      {
        skip_partition_for_unique_index= true;
        goto skip_partition_pruning;
      }
    }
    if ((error= get_parts_for_update(old_data, new_data, table->record[0],
                                     m_part_info, &old_part_id, &new_part_id,
                                     &func_value)))
    {
      m_part_info->err_value= func_value;
      DBUG_RETURN(error);
    }
    DBUG_PRINT("info", ("old_part_id: %u  new_part_id: %u", old_part_id, new_part_id));
  skip_partition_pruning:
    (void)0;
  }

  /*
   * Check for update of primary key or partition change
   * for special handling
   */  
  if (pk_update || old_part_id != new_part_id)
  {
    DBUG_RETURN(ndb_pk_update_row(thd, old_data, new_data));
  }
  /*
    If we are updating a unique key with auto_increment
    then we need to update the auto_increment counter
   */
  if (table->found_next_number_field &&
      bitmap_is_set(table->write_set, 
		    table->found_next_number_field->field_index) &&
      (error= set_auto_inc(thd, table->found_next_number_field)))
  {
    DBUG_RETURN(error);
  }
  /*
    Set only non-primary-key attributes.
    We already checked that any primary key attribute in write_set has no
    real changes.
  */
  bitmap_copy(&m_bitmap, table->write_set);
  bitmap_subtract(&m_bitmap, m_pk_bitmap_p);
  uchar *mask= m_table_map->get_column_mask(& m_bitmap);
  DBUG_ASSERT(!pk_update);

  NdbOperation::OperationOptions *poptions = NULL;
  NdbOperation::OperationOptions options;
  options.optionsPresent=0;

  /* Need to set the value of any user-defined partitioning function. 
     (excecpt for when using unique index)
  */
  if (m_user_defined_partitioning && !skip_partition_for_unique_index)
  {
    if (func_value >= INT_MAX32)
      func_value_uint32= INT_MAX32;
    else
      func_value_uint32= (uint32)func_value;
    sets[num_sets].column= get_partition_id_column();
    sets[num_sets].value= &func_value_uint32;
    num_sets++;

    if (!cursor)
    {
      options.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId= new_part_id;
    }
  }
  
  eventSetAnyValue(thd, &options);
  
  const bool need_flush=
      thd_ndb->add_row_check_if_batch_full(m_bytes_per_write);

 const Uint32 authorValue = 1;
 if ((thd->slave_thread) &&
     (m_table->getExtraRowAuthorBits()))
 {
   /* Set author to indicate slave updated last */
   sets[num_sets].column= NdbDictionary::Column::ROW_AUTHOR;
   sets[num_sets].value= &authorValue;
   num_sets++;
 }

 if (num_sets)
 {
   options.optionsPresent|= NdbOperation::OperationOptions::OO_SETVALUE;
   options.extraSetValues= sets;
   options.numExtraSetValues= num_sets;
 }

  if (thd->slave_thread || THDVAR(thd, deferred_constraints))
  {
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
  {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (cursor)
  {
    /*
      We are scanning records and want to update the record
      that was just found, call updateCurrentTuple on the cursor 
      to take over the lock to a new update operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling updateTuple on cursor, write_set=0x%x",
                        table->write_set->bitmap[0]));

    if (options.optionsPresent != 0)
      poptions = &options;

    if (!(op= cursor->updateCurrentTuple(trans, m_ndb_record,
                                         (const char*)new_data, mask,
                                         poptions,
                                         sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());

    m_lock_tuple= false;
    thd_ndb->m_unsent_bytes+= 12;
  }
  else
  {  
    const NdbRecord *key_rec;
    const uchar *key_row;
    setup_key_ref_for_ndb_record(&key_rec, &key_row, new_data,
				 m_read_before_write_removal_used);

    bool avoidNdbApiWriteOp = true; /* Default update op for ndb_update_row */
    Uint32 buffer[ MAX_CONFLICT_INTERPRETED_PROG_SIZE ];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer)/sizeof(buffer[0]));

    if (thd->slave_thread)
    {
      bool conflict_handled = false;
      /* Conflict resolution in slave thread. */
      DBUG_PRINT("info", ("Slave thread, preparing conflict resolution for update with mask : %x", *((Uint32*)mask)));

      if (unlikely((error = prepare_conflict_detection(UPDATE_ROW,
                                                       key_rec,
                                                       m_ndb_record,
                                                       old_data,
                                                       new_data,
                                                       table->write_set,
                                                       trans,
                                                       &code,
                                                       &options,
                                                       conflict_handled,
                                                       avoidNdbApiWriteOp))))
        DBUG_RETURN(error);

      if (unlikely(conflict_handled))
      {
        /* No need to continue with operation defintion */
        /* TODO : Ensure batch execution */
        DBUG_RETURN(0);
      }
    }

    if (options.optionsPresent !=0)
      poptions= &options;

    if (likely(avoidNdbApiWriteOp))
    {
      if (!(op= trans->updateTuple(key_rec, (const char *)key_row,
                                   m_ndb_record, (const char*)new_data, mask,
                                   poptions,
                                   sizeof(NdbOperation::OperationOptions))))
        ERR_RETURN(trans->getNdbError());
    }
    else
    {
      DBUG_PRINT("info", ("Update op using writeTuple"));
      if (!(op= trans->writeTuple(key_rec, (const char *)key_row,
                                  m_ndb_record, (const char*)new_data, mask,
                                  poptions,
                                  sizeof(NdbOperation::OperationOptions))))
        ERR_RETURN(trans->getNdbError());
    }
  }

  uint blob_count= 0;
  if (uses_blob_value(table->write_set))
  {
    int row_offset= (int)(new_data - table->record[0]);
    int res= set_blob_values(op, row_offset, table->write_set, &blob_count,
                             (batch_allowed && !need_flush));
    if (res != 0)
      DBUG_RETURN(res);
  }
  uint ignore_count= 0;
  /*
    Batch update operation if we are doing a scan for update, unless
    there exist UPDATE AFTER triggers
  */
  if (m_update_cannot_batch ||
      !(cursor || (batch_allowed && have_pk)) ||
      need_flush)
  {
    if (execute_no_commit(m_thd_ndb, trans,
                          m_ignore_no_key || m_read_before_write_removal_used,
                          &ignore_count) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else if (blob_count > 0)
    m_blobs_pending= true;

  m_rows_updated++;

  if (!applying_binlog(thd))
  {
    assert(m_rows_updated >= ignore_count);
    m_rows_updated-= ignore_count;
  }

  DBUG_RETURN(0);
}


/*
  handler delete interface
*/

int ha_ndbcluster::delete_row(const uchar *record)
{
  return ndb_delete_row(record, false);
}

bool ha_ndbcluster::start_bulk_delete()
{
  DBUG_ENTER("start_bulk_delete");
  m_is_bulk_delete = true;
  DBUG_RETURN(0); // Bulk delete used by handler
}

int ha_ndbcluster::end_bulk_delete()
{
  NdbTransaction* trans= m_thd_ndb->trans;
  DBUG_ENTER("end_bulk_delete");
  assert(m_is_bulk_delete); // Don't allow end() without start()
  m_is_bulk_delete = false;

  // m_handler must be NULL or point to _this_ handler instance
  assert(m_thd_ndb->m_handler == NULL || m_thd_ndb->m_handler == this);

  if (m_thd_ndb->m_handler &&
      m_read_before_write_removal_possible)
  {
    /*
      This is an autocommit involving only one table and rbwr is on

      Commit the autocommit transaction early(before the usual place
      in ndbcluster_commit) in order to:
      1) save one round trip, "no-commit+commit" converted to "commit"
      2) return the correct number of updated and affected rows
         to the delete loop(which will ask handler in rbwr mode)
    */
    DBUG_PRINT("info", ("committing auto-commit+rbwr early"));
    uint ignore_count= 0;
    const int ignore_error= 1;
    if (execute_commit(m_thd_ndb, trans,
                       m_thd_ndb->m_force_send, ignore_error,
                       &ignore_count) != 0)
    {
      no_uncommitted_rows_execute_failure();
      m_rows_deleted = 0;
      DBUG_RETURN(ndb_err(trans));
    }
    THD *thd= table->in_use;
    if (!applying_binlog(thd))
    {
      DBUG_PRINT("info", ("ignore_count: %u", ignore_count));
      assert(m_rows_deleted >= ignore_count);
      m_rows_deleted-= ignore_count;
    }
    DBUG_RETURN(0);
  }

  if (m_thd_ndb->m_unsent_bytes == 0)
  {
    DBUG_PRINT("exit", ("skip execute - no unsent bytes"));
    DBUG_RETURN(0);
  }

  if (thd_allow_batch(table->in_use))
  {
    /*
      Turned on by @@transaction_allow_batching=ON
      or implicitly by slave exec thread
    */
    DBUG_PRINT("exit", ("skip execute - transaction_allow_batching is ON"));
    DBUG_RETURN(0);
  }

  if (m_thd_ndb->m_handler)
  {
    // Execute at commit time(in 'ndbcluster_commit') to save a round trip
    DBUG_PRINT("exit", ("skip execute - simple autocommit"));
    DBUG_RETURN(0);
  }

  uint ignore_count= 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }

  THD *thd= table->in_use;
  if (!applying_binlog(thd))
  {
    assert(m_rows_deleted >= ignore_count);
    m_rows_deleted-= ignore_count;
    no_uncommitted_rows_update(ignore_count);
  }
  DBUG_RETURN(0);
}


/**
  Delete one record from NDB, using primary key .
*/

int ha_ndbcluster::ndb_delete_row(const uchar *record,
                                  bool primary_key_update)
{
  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= m_thd_ndb;
  NdbScanOperation* cursor= m_active_cursor;
  const NdbOperation *op;
  uint32 part_id= ~uint32(0);
  int error;
  bool allow_batch= !m_delete_cannot_batch &&
    (m_is_bulk_delete || thd_allow_batch(thd));

  DBUG_ENTER("ndb_delete_row");

  /* Start a transaction now if none available
   * (Manual Binlog application...)
   */
  /* TODO : Consider hinting */
  if (unlikely((!m_thd_ndb->trans) && 
               !get_transaction(error)))
  {
    DBUG_RETURN(error);
  }
    
  NdbTransaction *trans= m_thd_ndb->trans;
  DBUG_ASSERT(trans);

  error = check_slave_state(thd);
  if (unlikely(error))
    DBUG_RETURN(error);

  ha_statistic_increment(&System_status_var::ha_delete_count);

  bool skip_partition_for_unique_index= false;
  if (m_use_partition_pruning)
  {
    if (!cursor && m_read_before_write_removal_used)
    {
      const NDB_INDEX_TYPE type= get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for deleting
        without finding the partitions
      */
      if (type == UNIQUE_INDEX ||
          type == UNIQUE_ORDERED_INDEX)
      {
        skip_partition_for_unique_index= true;
        goto skip_partition_pruning;
      }
    }
    if ((error= get_part_for_delete(record, table->record[0], m_part_info,
                                    &part_id)))
    {
      DBUG_RETURN(error);
    }
  skip_partition_pruning:
    (void)0;
  }

  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent=0;

  eventSetAnyValue(thd, &options);

  /*
    Poor approx. let delete ~ tabsize / 4
  */
  uint delete_size= 12 + (m_bytes_per_write >> 2);
  const bool need_flush =
      thd_ndb->add_row_check_if_batch_full(delete_size);

  if (thd->slave_thread || THDVAR(thd, deferred_constraints))
  {
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
  {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (cursor)
  {
    if (options.optionsPresent != 0)
      poptions = &options;

    /*
      We are scanning records and want to delete the record
      that was just found, call deleteTuple on the cursor 
      to take over the lock to a new delete operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling deleteTuple on cursor"));
    if ((op = cursor->deleteCurrentTuple(trans, m_ndb_record,
                                         NULL, // result_row
                                         NULL, // result_mask
                                         poptions, 
                                         sizeof(NdbOperation::OperationOptions))) == 0)
      ERR_RETURN(trans->getNdbError());     
    m_lock_tuple= false;
    thd_ndb->m_unsent_bytes+= 12;

    no_uncommitted_rows_update(-1);
    m_rows_deleted++;

    if (!(primary_key_update || m_delete_cannot_batch))
    {
      // If deleting from cursor, NoCommit will be handled in next_result
      DBUG_RETURN(0);
    }
  }
  else
  {
    const NdbRecord *key_rec;
    const uchar *key_row;

    if (m_user_defined_partitioning && !skip_partition_for_unique_index)
    {
      options.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId= part_id;
    }

    setup_key_ref_for_ndb_record(&key_rec, &key_row, record,
				 m_read_before_write_removal_used);

    Uint32 buffer[ MAX_CONFLICT_INTERPRETED_PROG_SIZE ];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer)/sizeof(buffer[0]));
    if (thd->slave_thread)
    {
       bool conflict_handled = false;
       bool dummy_delete_does_not_care = false;

      /* Conflict resolution in slave thread. */
      if (unlikely((error = prepare_conflict_detection(DELETE_ROW,
                                                       key_rec,
                                                       m_ndb_record,
                                                       key_row, /* old_data */
                                                       NULL,    /* new_data */
                                                       table->write_set,
                                                       trans,
                                                       &code,
                                                       &options,
                                                       conflict_handled,
                                                       dummy_delete_does_not_care))))
        DBUG_RETURN(error);

      if (unlikely(conflict_handled))
      {
        /* No need to continue with operation definition */
        /* TODO : Ensure batch execution */
        DBUG_RETURN(0);
      }
    }

    if (options.optionsPresent != 0)
      poptions= &options;

    if (!(op=trans->deleteTuple(key_rec, (const char *)key_row,
                                m_ndb_record,
                                NULL, // row
                                NULL, // mask
                                poptions,
                                sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());

    no_uncommitted_rows_update(-1);
    m_rows_deleted++;

    /*
      Check if we can batch the delete.

      We don't batch deletes as part of primary key updates.
      We do not batch deletes on tables with no primary key. For such tables,
      replication uses full table scan to locate the row to delete. The
      problem is the following scenario when deleting 2 (or more) rows:

       1. Table scan to locate the first row.
       2. Delete the row, batched so no execute.
       3. Table scan to locate the second row is executed, along with the
          batched delete operation from step 2.
       4. The first row is returned from nextResult() (not deleted yet).
       5. The kernel deletes the row (operation from step 2).
       6. lockCurrentTuple() is called on the row returned in step 4. However,
          as that row is now deleted, the operation fails and the transaction
          is aborted.
       7. The delete of the second tuple now fails, as the transaction has
          been aborted.
    */

    if ( allow_batch &&
	 table_share->primary_key != MAX_KEY &&
	 !primary_key_update &&
	 !need_flush)
    {
      DBUG_RETURN(0);
    }
  }

  // Execute delete operation
  uint ignore_count= 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  if (!primary_key_update)
  {
    if (!applying_binlog(thd))
    {
      assert(m_rows_deleted >= ignore_count);
      m_rows_deleted-= ignore_count;
      no_uncommitted_rows_update(ignore_count);
    }
  }
  DBUG_RETURN(0);
}
  
/**
  Unpack a record returned from a scan.
  We copy field-for-field to
   1. Avoid unnecessary copying for sparse rows.
   2. Properly initialize not used null bits.
  Note that we do not unpack all returned rows; some primary/unique key
  operations can read directly into the destination row.
*/
void ha_ndbcluster::unpack_record(uchar *dst_row, const uchar *src_row)
{
  int res;
  (void)res; // Bug#27150980 NDB_UNPACK_RECORD NEED ERROR HANDLING
  DBUG_ASSERT(src_row != NULL);

  my_ptrdiff_t dst_offset= dst_row - table->record[0];
  my_ptrdiff_t src_offset= src_row - table->record[0];

  /* Initialize the NULL bitmap. */
  memset(dst_row, 0xff, table->s->null_bytes);

  uchar *blob_ptr= m_blobs_buffer;

  for (uint i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (bitmap_is_set(table->read_set, i) && field->stored_in_db)
    {
      if (field->type() == MYSQL_TYPE_BIT)
      {
        Field_bit *field_bit= static_cast<Field_bit*>(field);
        if (!field->is_real_null(src_offset))
        {
          field->move_field_offset(src_offset);
          longlong value= field_bit->val_int();
          field->move_field_offset(dst_offset-src_offset);
          field_bit->set_notnull();
          /* Field_bit in DBUG requires the bit set in write_set for store(). */
          my_bitmap_map *old_map=
            dbug_tmp_use_all_columns(table, table->write_set);
          int res = field_bit->store(value, true);
          assert(res == 0); NDB_IGNORE_VALUE(res);
          dbug_tmp_restore_column_map(table->write_set, old_map);
          field->move_field_offset(-dst_offset);
        }
      }
      else if (field->flags & BLOB_FLAG)
      {
        Field_blob *field_blob= (Field_blob *)field;
        NdbBlob *ndb_blob= m_value[i].blob;
        /* unpack_record *only* called for scan result processing
         * *while* the scan is open and the Blob is active.
         * Verify Blob state to be certain.
         * Accessing PK/UK op Blobs after execute() is unsafe
         */
        DBUG_ASSERT(ndb_blob != 0);
        DBUG_ASSERT(ndb_blob->getState() == NdbBlob::Active);
        int isNull;
        res= ndb_blob->getNull(isNull);
        DBUG_ASSERT(res == 0);                  // Already succeeded once
        Uint64 len64= 0;
        field_blob->move_field_offset(dst_offset);
        if (!isNull)
        {
          res= ndb_blob->getLength(len64);
          DBUG_ASSERT(res == 0 && len64 <= (Uint64)0xffffffff);

          if(len64 > field_blob->max_data_length())
          {
            len64 = calc_ndb_blob_len(ndb_blob->getColumn()->getCharset(), 
                                    blob_ptr, field_blob->max_data_length());

            // push a warning
            push_warning_printf(table->in_use, Sql_condition::SL_WARNING,
                        WARN_DATA_TRUNCATED,
                        "Truncated value from TEXT field \'%s\'", field_blob->field_name);

          }
          field->set_notnull();
        }
        /* Need not set_null(), as we initialized null bits to 1 above. */
        field_blob->set_ptr((uint32)len64, blob_ptr);
        field_blob->move_field_offset(-dst_offset);
        blob_ptr+= (len64 + 7) & ~((Uint64)7);
      }
      else
      {
        field->move_field_offset(src_offset);
        /* Normal field (not blob or bit type). */
        if (!field->is_null())
        {
          /* Only copy actually used bytes of varstrings. */
          uint32 actual_length= field_used_length(field);
          uchar *src_ptr= field->ptr;
          field->move_field_offset(dst_offset - src_offset);
          field->set_notnull();
          memcpy(field->ptr, src_ptr, actual_length);
          field->move_field_offset(-dst_offset);
        }
        else
          field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }  // if(bitmap_is_set...
  }  // for(...
}

void ha_ndbcluster::unpack_record_and_set_generated_fields(
  TABLE *table,
  uchar *dst_row,
  const uchar *src_row)
{
  unpack_record(dst_row, src_row);
  if(Ndb_table_map::has_virtual_gcol(table))
  {
    update_generated_read_fields(dst_row, table);
  }
}

/**
  Get the default value of the field from default_values of the table.
*/
static void get_default_value(void *def_val, Field *field)
{
  DBUG_ASSERT(field != NULL);
  DBUG_ASSERT(field->stored_in_db);

  my_ptrdiff_t src_offset= field->table->default_values_offset();

  {
    if (bitmap_is_set(field->table->read_set, field->field_index))
    {
      if (field->type() == MYSQL_TYPE_BIT)
      {
        Field_bit *field_bit= static_cast<Field_bit*>(field);
        if (!field->is_real_null(src_offset))
        {
          field->move_field_offset(src_offset);
          longlong value= field_bit->val_int();
          /* Map to NdbApi format - two Uint32s */
          Uint32 out[2];
          out[0] = 0;
          out[1] = 0;
          for (int b=0; b < 64; b++)
          {
            out[b >> 5] |= (value & 1) << (b & 31);
            
            value= value >> 1;
          }
          memcpy(def_val, out, sizeof(longlong));
          field->move_field_offset(-src_offset);
        }
      }
      else if (field->flags & BLOB_FLAG)
      {
        assert(false);
      }
      else
      {
        field->move_field_offset(src_offset);
        /* Normal field (not blob or bit type). */
        if (!field->is_null())
        {
          /* Only copy actually used bytes of varstrings. */
          uint32 actual_length= field_used_length(field);
          uchar *src_ptr= field->ptr;
          field->set_notnull();
          memcpy(def_val, src_ptr, actual_length);
        }
        field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }
  }
}


int ha_ndbcluster::index_init(uint index, bool sorted)
{
  DBUG_ENTER("ha_ndbcluster::index_init");
  DBUG_PRINT("enter", ("index: %u  sorted: %d", index, sorted));
  active_index= index;
  m_sorted= sorted;
  /*
    Locks are are explicitly released in scan
    unless m_lock.type == TL_READ_HIGH_PRIORITY
    and no sub-sequent call to unlock_row()
  */
  m_lock_tuple= false;

  if (table_share->primary_key == MAX_KEY &&
      m_use_partition_pruning)
  {
    bitmap_union(table->read_set, &m_part_info->full_part_field_set);
  }

  DBUG_RETURN(0);
}


int ha_ndbcluster::index_end()
{
  DBUG_ENTER("ha_ndbcluster::index_end");
  DBUG_RETURN(close_scan());
}

/**
  Check if key contains null.
*/
static
int
check_null_in_key(const KEY* key_info, const uchar *key, uint key_len)
{
  KEY_PART_INFO *curr_part, *end_part;
  const uchar* end_ptr= key + key_len;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->user_defined_key_parts;

  for (; curr_part != end_part && key < end_ptr; curr_part++)
  {
    if (curr_part->null_bit && *key)
      return 1;

    key += curr_part->store_length;
  }
  return 0;
}

int ha_ndbcluster::index_read(uchar *buf,
                              const uchar *key, uint key_len, 
                              enum ha_rkey_function find_flag)
{
  key_range start_key, end_key, *end_key_p=NULL;
  bool descending= false;
  DBUG_ENTER("ha_ndbcluster::index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  start_key.key= key;
  start_key.length= key_len;
  start_key.flag= find_flag;
  switch (find_flag) {
  case HA_READ_KEY_EXACT:
    /**
     * Specify as a closed EQ_RANGE.
     * Setting HA_READ_AFTER_KEY seems odd, but this is according
     * to MySQL convention, see opt_range.cc.
     */
    end_key.key= key;
    end_key.length= key_len;
    end_key.flag= HA_READ_AFTER_KEY;
    end_key_p= &end_key;
    break;
  case HA_READ_KEY_OR_PREV:
  case HA_READ_BEFORE_KEY:
  case HA_READ_PREFIX_LAST:
  case HA_READ_PREFIX_LAST_OR_PREV:
    descending= true;
    break;
  default:
    break;
  }
  const int error= read_range_first_to_buf(&start_key, end_key_p,
                                           descending,
                                           m_sorted, buf);
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_next(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_next");
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  const int error= next_result(buf);
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_prev(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_prev");
  ha_statistic_increment(&System_status_var::ha_read_prev_count);
  const int error= next_result(buf);
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_first(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_first");
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  // Start the ordered index scan and fetch the first row

  // Only HA_READ_ORDER indexes get called by index_first
  const int error= ordered_index_scan(0, 0, m_sorted, false, buf, NULL);
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_last(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_last");
  ha_statistic_increment(&System_status_var::ha_read_last_count);
  const int error= ordered_index_scan(0, 0, m_sorted, true, buf, NULL);
  DBUG_RETURN(error);
}

int ha_ndbcluster::index_read_last(uchar * buf, const uchar * key, uint key_len)
{
  DBUG_ENTER("ha_ndbcluster::index_read_last");
  DBUG_RETURN(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));
}


int ha_ndbcluster::read_range_first_to_buf(const key_range *start_key,
                                           const key_range *end_key,
                                           bool desc, bool sorted,
                                           uchar* buf)
{
  part_id_range part_spec;
  const NDB_INDEX_TYPE type= get_index_type(active_index);
  const KEY* key_info= table->key_info+active_index;
  int error; 
  DBUG_ENTER("ha_ndbcluster::read_range_first_to_buf");
  DBUG_PRINT("info", ("desc: %d, sorted: %d", desc, sorted));

  if (unlikely((error= close_scan())))
    DBUG_RETURN(error);

  if (m_use_partition_pruning)
  {
    DBUG_ASSERT(m_pushed_join_operation != PUSHED_ROOT);
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part > part_spec.end_part)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }

    if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      sorted= false;
      if (unlikely(!get_transaction_part_id(part_spec.start_part, error)))
      {
        DBUG_RETURN(error);
      }
    }
  }

  switch (type){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    if (start_key && 
        start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT)
    {
      if (!m_thd_ndb->trans)
        if (unlikely(!start_transaction_key(active_index,
                                            start_key->key, error)))
          DBUG_RETURN(error);
      DBUG_DUMP("key", start_key->key, start_key->length);
      error =
          pk_read(start_key->key, buf,
                  (m_use_partition_pruning) ? &(part_spec.start_part) : NULL);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    if (start_key && start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT && 
        !check_null_in_key(key_info, start_key->key, start_key->length))
    {
      if (!m_thd_ndb->trans)
        if (unlikely(!start_transaction_key(active_index,
                                            start_key->key, error)))
          DBUG_RETURN(error);
      DBUG_DUMP("key", start_key->key, start_key->length);
      error= unique_index_read(start_key->key, buf);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    else if (type == UNIQUE_INDEX)
      DBUG_RETURN(full_table_scan(key_info, 
                                  start_key,
                                  end_key,
                                  buf));
    break;
  default:
    break;
  }
  if (!m_use_partition_pruning && !m_thd_ndb->trans)
  {
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    if (part_spec.start_part == part_spec.end_part)
      if (unlikely(!start_transaction_part_id(part_spec.start_part, error)))
        DBUG_RETURN(error);
  }
  // Start the ordered index scan and fetch the first row
  DBUG_RETURN(ordered_index_scan(start_key, end_key, sorted, desc, buf,
	  (m_use_partition_pruning)? &part_spec : NULL));
}

int ha_ndbcluster::read_range_first(const key_range *start_key,
                                    const key_range *end_key,
                                    bool /* eq_range */, bool sorted)
{
  uchar* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  DBUG_RETURN(read_range_first_to_buf(start_key, end_key, false,
                                      sorted, buf));
}

int ha_ndbcluster::read_range_next()
{
  DBUG_ENTER("ha_ndbcluster::read_range_next");
  DBUG_RETURN(next_result(table->record[0]));
}


int ha_ndbcluster::rnd_init(bool)
{
  int error;
  DBUG_ENTER("rnd_init");

  if ((error= close_scan()))
    DBUG_RETURN(error);
  index_init(table_share->primary_key, 0);
  DBUG_RETURN(0);
}

int ha_ndbcluster::close_scan()
{
  DBUG_ENTER("close_scan");

  if (m_active_query)
  {
    m_active_query->close(m_thd_ndb->m_force_send);
    m_active_query= NULL;
  }

  NdbScanOperation *cursor= m_active_cursor;
  if (!cursor)
  {
    cursor = m_multi_cursor;
    if (!cursor)
      DBUG_RETURN(0);
  }

  int error;
  NdbTransaction *trans= m_thd_ndb->trans;
  if ((error= scan_handle_lock_tuple(cursor, trans)) != 0)
    DBUG_RETURN(error);

  if (m_thd_ndb->m_unsent_bytes)
  {
    /*
      Take over any pending transactions to the 
      deleteing/updating transaction before closing the scan    
    */
    DBUG_PRINT("info", ("thd_ndb->m_unsent_bytes: %ld",
                        (long) m_thd_ndb->m_unsent_bytes));    
    if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  
  cursor->close(m_thd_ndb->m_force_send, true);
  m_active_cursor= NULL;
  m_multi_cursor= NULL;
  DBUG_RETURN(0);
}

int ha_ndbcluster::rnd_end()
{
  DBUG_ENTER("rnd_end");
  DBUG_RETURN(close_scan());
}


int ha_ndbcluster::rnd_next(uchar *buf)
{
  DBUG_ENTER("rnd_next");
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);

  int error;
  if (m_active_cursor || m_active_query)
    error= next_result(buf);
  else
    error= full_table_scan(NULL, NULL, NULL, buf);
  
  DBUG_RETURN(error);
}


/**
  An "interesting" record has been found and it's pk 
  retrieved by calling position. Now it's time to read
  the record from db once again.
*/

int ha_ndbcluster::rnd_pos(uchar *buf, uchar *pos)
{
  DBUG_ENTER("rnd_pos");
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  // The primary key for the record is stored in pos
  // Perform a pk_read using primary key "index"
  {
    part_id_range part_spec;
    uint key_length= ref_length;
    if (m_user_defined_partitioning)
    {
      if (table_share->primary_key == MAX_KEY)
      {
        /*
          The partition id has been fetched from ndb
          and has been stored directly after the hidden key
        */
        DBUG_DUMP("key+part", pos, key_length);
        key_length= ref_length - sizeof(m_part_id);
        part_spec.start_part= part_spec.end_part= *(uint32 *)(pos + key_length);
      }
      else
      {
        key_range key_spec;
        KEY *key_info= table->key_info + table_share->primary_key;
        key_spec.key= pos;
        key_spec.length= key_length;
        key_spec.flag= HA_READ_KEY_EXACT;
        get_full_part_id_from_key(table, buf, key_info, 
                                  &key_spec, &part_spec);
        DBUG_ASSERT(part_spec.start_part == part_spec.end_part);
      }
      DBUG_PRINT("info", ("partition id %u", part_spec.start_part));
    }
    DBUG_DUMP("key", pos, key_length);
    int res =
        pk_read(pos, buf,
                (m_user_defined_partitioning) ? &(part_spec.start_part) : NULL);
    if (res == HA_ERR_KEY_NOT_FOUND)
    {
      /**
       * When using rnd_pos
       *   server first retrives a set of records (typically scans them)
       *   and store a unique identifier (for ndb this is the primary key)
       *   and later retreives the record again using rnd_pos and the
       *   saved primary key. For ndb, since we only support committed read
       *   the record could have been deleted in between the "save" and
       *   the rnd_pos.
       *   Therefor we return HA_ERR_RECORD_DELETED in this case rather than
       *   HA_ERR_KEY_NOT_FOUND (which will cause statment to be aborted)
       *   
       */
      res= HA_ERR_RECORD_DELETED;
    }
    DBUG_RETURN(res);
  }
}


/**
  Store the primary key of this record in ref 
  variable, so that the row can be retrieved again later
  using "reference" in rnd_pos.
*/

void ha_ndbcluster::position(const uchar *record)
{
  KEY *key_info;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  uchar *buff;
  uint key_length;

  DBUG_ENTER("position");

  if (table_share->primary_key != MAX_KEY) 
  {
    key_length= ref_length;
    key_info= table->key_info + table_share->primary_key;
    key_part= key_info->key_part;
    end= key_part + key_info->user_defined_key_parts;
    buff= ref;
    
    for (; key_part != end; key_part++) 
    {
      if (key_part->null_bit) {
        /* Store 0 if the key part is a NULL part */      
        if (record[key_part->null_offset]
            & key_part->null_bit) {
          *buff++= 1;
          continue;
        }      
        *buff++= 0;
      }

      size_t len = key_part->length;
      const uchar * ptr = record + key_part->offset;
      Field *field = key_part->field;
      if (field->type() ==  MYSQL_TYPE_VARCHAR)
      {
        size_t var_length;
        if (((Field_varstring*)field)->length_bytes == 1)
        {
          /**
           * Keys always use 2 bytes length
           */
          buff[0] = ptr[0];
          buff[1] = 0;
          var_length = ptr[0];
          DBUG_ASSERT(var_length <= len);
          memcpy(buff+2, ptr + 1, var_length);
        }
        else
        {
          var_length = ptr[0] + (ptr[1]*256);
          DBUG_ASSERT(var_length <= len);
          memcpy(buff, ptr, var_length + 2);
        }
        /**
          We have to zero-pad any unused VARCHAR buffer so that MySQL is 
          able to use simple memcmp to compare two instances of the same
          unique key value to determine if they are equal. 
          MySQL does this to compare contents of two 'ref' values.
          (Duplicate weedout algorithm is one such case.)
        */
        memset(buff+2+var_length, 0, len - var_length);
        len += 2;
      }
      else
      {
        memcpy(buff, ptr, len);
      }
      buff += len;
    }
  } 
  else 
  {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    // If table has user defined partition save the partition id as well
    if (m_user_defined_partitioning)
    {
      DBUG_PRINT("info", ("Saving partition id %u", m_part_id));
      key_length= ref_length - sizeof(m_part_id);
      memcpy(ref+key_length, (void *)&m_part_id, sizeof(m_part_id));
    }
    else
      key_length= ref_length;
#ifndef DBUG_OFF
    const int hidden_no= Ndb_table_map::num_stored_fields(table);
    const NDBTAB *tab= m_table;  
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    DBUG_ASSERT(hidden_col->getPrimaryKey() && 
                hidden_col->getAutoIncrement() &&
                key_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
#endif
    memcpy(ref, &m_ref, key_length);
  }
#ifndef DBUG_OFF
  if (table_share->primary_key == MAX_KEY && m_user_defined_partitioning) 
    DBUG_DUMP("key+part", ref, key_length+sizeof(m_part_id));
#endif
  DBUG_DUMP("ref", ref, key_length);
  DBUG_VOID_RETURN;
}

int
ha_ndbcluster::cmp_ref(const uchar * ref1, const uchar * ref2) const
{
  DBUG_ENTER("cmp_ref");

  if (table_share->primary_key != MAX_KEY) 
  {
    KEY *key_info= table->key_info + table_share->primary_key;
    KEY_PART_INFO *key_part= key_info->key_part;
    KEY_PART_INFO *end= key_part + key_info->user_defined_key_parts;
    
    for (; key_part != end; key_part++) 
    {
      // NOTE: No need to check for null since PK is not-null

      Field *field= key_part->field;
      int result= field->key_cmp(ref1, ref2);
      if (result)
      {
        DBUG_RETURN(result);
      }

      if (field->type() ==  MYSQL_TYPE_VARCHAR)
      {
        ref1+= 2;
        ref2+= 2;
      }
      
      ref1+= key_part->length;
      ref2+= key_part->length;
    }
    DBUG_RETURN(0);
  } 
  else
  {
    DBUG_RETURN(memcmp(ref1, ref2, ref_length));
  }
}

int ha_ndbcluster::info(uint flag)
{
  THD *thd= table->in_use;
  int result= 0;
  DBUG_ENTER("info");
  DBUG_PRINT("enter", ("flag: %d", flag));
  
  if (flag & HA_STATUS_POS)
    DBUG_PRINT("info", ("HA_STATUS_POS"));
  if (flag & HA_STATUS_TIME)
    DBUG_PRINT("info", ("HA_STATUS_TIME"));
  if (flag & HA_STATUS_CONST)
  {
    /*
      Set size required by a single record in the MRR 'HANDLER_BUFFER'.
      MRR buffer has both a fixed and a variable sized part.
      Size is calculated assuming max size of the variable part.

      See comments for multi_range_fixed_size() and
      multi_range_max_entry() regarding how the MRR buffer is organized.
    */
    stats.mrr_length_per_rec= multi_range_fixed_size(1) +
      multi_range_max_entry(PRIMARY_KEY_INDEX, table_share->reclength);
  }
  while (flag & HA_STATUS_VARIABLE)
  {
    if (!thd)
      thd= current_thd;
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));

    if (!m_table_info)
    {
      if (check_ndb_connection(thd))
        DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    /*
      May need to update local copy of statistics in
      'm_table_info', either directly from datanodes,
      or from shared (mutex protected) cached copy, if:
       1) 'use_exact_count' has been set (by config or user).
       2) HA_STATUS_NO_LOCK -> read from shared cached copy.
       3) Local copy is invalid.
    */
    bool exact_count= THDVAR(thd, use_exact_count);
    if (exact_count                 ||         // 1)
        !(flag & HA_STATUS_NO_LOCK) ||         // 2)
        m_table_info == NULL        ||         // 3)
        m_table_info->records == ~(ha_rows)0)  // 3)
    {
      result= update_stats(thd, (exact_count || !(flag & HA_STATUS_NO_LOCK)));
      if (result)
        DBUG_RETURN(result);
    }
    /* Read from local statistics, fast and fuzzy, wo/ locks */
    else
    {
      DBUG_ASSERT(m_table_info->records != ~(ha_rows)0);
      stats.records= m_table_info->records +
                     m_table_info->no_uncommitted_rows_count;
    }

    if (thd->lex->sql_command != SQLCOM_SHOW_TABLE_STATUS &&
        thd->lex->sql_command != SQLCOM_SHOW_KEYS)
    {
      /*
        just use whatever stats we have. However,
        optimizer interprets the values 0 and 1 as EXACT:
          -> < 2 should not be returned.
      */
      if (stats.records < 2)
        stats.records= 2;
    }
    break;
  }
  /* RPK moved to variable part */
  if (flag & HA_STATUS_VARIABLE)
  {
    /* No meaningful way to return error */
    DBUG_PRINT("info", ("rec_per_key"));
    set_rec_per_key();
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY dupkey=%u", m_dupkey));
    errkey= m_dupkey;
  }
  if (flag & HA_STATUS_AUTO)
  {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (m_table && table->found_next_number_field)
    {
      if (!thd)
        thd= current_thd;
      if (check_ndb_connection(thd))
        DBUG_RETURN(HA_ERR_NO_CONNECTION);
      Ndb *ndb= get_ndb(thd);
      NDB_SHARE::Tuple_id_range_guard g(m_share);
      
      Uint64 auto_increment_value64;
      if (ndb->readAutoIncrementValue(m_table, g.range,
                                      auto_increment_value64) == -1)
      {
        const NdbError err= ndb->getNdbError();
        ndb_log_error("Error %d in readAutoIncrementValue(): %s",
                      err.code, err.message);
        stats.auto_increment_value= ~(ulonglong)0;
      }
      else
        stats.auto_increment_value= (ulonglong)auto_increment_value64;
    }
  }

  if(result == -1)
    result= HA_ERR_NO_CONNECTION;

  DBUG_RETURN(result);
}


void ha_ndbcluster::get_dynamic_partition_info(ha_statistics *stat_info,
                                               ha_checksum *checksum,
                                               uint part_id)
{
  DBUG_PRINT("info", ("ha_ndbcluster::get_dynamic_partition_info"));

  int error = 0;
  THD *thd = table->in_use;

  /* Checksum not supported, set it to NULL.*/
  *checksum = 0;

  if (!thd)
    thd = current_thd;
  if (!m_table_info)
  {
    if ((error = check_ndb_connection(thd)))
      goto err;
  }
  error = update_stats(thd, 1, part_id);

  if (error == 0)
  {
    stat_info->records = stats.records;
    stat_info->mean_rec_length = stats.mean_rec_length;
    stat_info->data_file_length = stats.data_file_length;
    stat_info->delete_length = stats.delete_length;
    stat_info->max_data_file_length = stats.max_data_file_length;
    return;
  }

err: 

  DBUG_PRINT("warning", 
    ("ha_ndbcluster::get_dynamic_partition_info failed with error code %u", 
     error));
}


int ha_ndbcluster::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("extra");
  switch (operation) {
  case HA_EXTRA_IGNORE_DUP_KEY:       /* Dup keys don't rollback everything*/
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));
    DBUG_PRINT("info", ("Ignoring duplicate key"));
    m_ignore_dup_key= true;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    m_ignore_dup_key= false;
    break;
  case HA_EXTRA_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= true;
    break;
  case HA_EXTRA_NO_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= false;
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CAN_REPLACE"));
    if (!m_has_unique_index ||
        /* 
           Always set if slave, quick fix for bug 27378
           or if manual binlog application, for bug 46662
        */
        applying_binlog(current_thd))
    {
      DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
      m_use_write= true;
    }
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CANNOT_REPLACE"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= false;
    break;
  case HA_EXTRA_DELETE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_DELETE_CANNOT_BATCH"));
    m_delete_cannot_batch= true;
    break;
  case HA_EXTRA_UPDATE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_UPDATE_CANNOT_BATCH"));
    m_update_cannot_batch= true;
    break;
  // We don't implement 'KEYREAD'. However, KEYREAD also implies DISABLE_JOINPUSH.
  case HA_EXTRA_KEYREAD:
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD"));
    m_disable_pushed_join= true;
    break;
  case HA_EXTRA_NO_KEYREAD:
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYREAD"));
    m_disable_pushed_join= false;
    break;
  case HA_EXTRA_BEGIN_ALTER_COPY:
    // Start of copy into intermediate table during copying alter, turn
    // off transactions when writing into the intermediate table in order to
    // avoid exhausting NDB transaction resources, this is safe as it would
    // be dropped anyway if there is a failure during the alter
    DBUG_PRINT("info", ("HA_EXTRA_BEGIN_ALTER_COPY"));
    m_thd_ndb->set_trans_option(Thd_ndb::TRANS_TRANSACTIONS_OFF);
    break;
  case HA_EXTRA_END_ALTER_COPY:
    // End of copy into intermediate table during copying alter.
    // Nothing to do, the transactions will automatically be enabled
    // again for subsequent statement
    DBUG_PRINT("info", ("HA_EXTRA_END_ALTER_COPY"));
    break;
  default:
    break;
  }

  DBUG_RETURN(0);
}


bool ha_ndbcluster::start_read_removal()
{
  THD *thd= table->in_use;
  DBUG_ENTER("start_read_removal");

  if (uses_blob_value(table->write_set))
  {
    DBUG_PRINT("exit", ("No! Blob field in write_set"));
    DBUG_RETURN(false);
  }

  if (thd->lex->sql_command == SQLCOM_DELETE &&
      table_share->blob_fields)
  {
    DBUG_PRINT("exit", ("No! DELETE from table with blob(s)"));
    DBUG_RETURN(false);
  }

  if (table_share->primary_key == MAX_KEY)
  {
    DBUG_PRINT("exit", ("No! Table with hidden key"));
    DBUG_RETURN(false);
  }

  if (bitmap_is_overlapping(table->write_set, m_pk_bitmap_p))
  {
    DBUG_PRINT("exit", ("No! Updating primary key"));
    DBUG_RETURN(false);
  }

  if (m_has_unique_index)
  {
    for (uint i= 0; i < table_share->keys; i++)
    {
      const KEY* key= table->key_info + i;
      if ((key->flags & HA_NOSAME) &&
          bitmap_is_overlapping(table->write_set,
                                m_key_fields[i]))
      {
        DBUG_PRINT("exit", ("No! Unique key %d is updated", i));
        DBUG_RETURN(false);
      }
    }
  }
  m_read_before_write_removal_possible= true;
  DBUG_PRINT("exit", ("Yes, rbwr is possible!"));
  DBUG_RETURN(true);
}


ha_rows ha_ndbcluster::end_read_removal(void)
{
  DBUG_ENTER("end_read_removal");
  DBUG_ASSERT(m_read_before_write_removal_possible);
  DBUG_PRINT("info", ("updated: %llu, deleted: %llu",
                      m_rows_updated, m_rows_deleted));
  DBUG_RETURN(m_rows_updated + m_rows_deleted);
}


int ha_ndbcluster::reset()
{
  DBUG_ENTER("ha_ndbcluster::reset");
  if (m_cond)
  {
    m_cond->cond_clear();
  }
  DBUG_ASSERT(m_active_query == NULL);
  if (m_pushed_join_operation==PUSHED_ROOT)  // Root of pushed query
  {
    delete m_pushed_join_member;             // Also delete QueryDef
  }
  m_pushed_join_member= NULL;
  m_pushed_join_operation= -1;
  m_disable_pushed_join= false;

  /* reset flags set by extra calls */
  m_read_before_write_removal_possible= false;
  m_read_before_write_removal_used= false;
  m_rows_updated= m_rows_deleted= 0;
  m_ignore_dup_key= false;
  m_use_write= false;
  m_ignore_no_key= false;
  m_rows_inserted= (ha_rows) 0;
  m_rows_to_insert= (ha_rows) 1;
  m_delete_cannot_batch= false;
  m_update_cannot_batch= false;

  assert(m_is_bulk_delete == false);
  m_is_bulk_delete = false;
  DBUG_RETURN(0);
}


/**
  Start of an insert, remember number of rows to be inserted, it will
  be used in write_row and get_autoincrement to send an optimal number
  of rows in each roundtrip to the server.

  @param
   rows     number of rows to insert, 0 if unknown
*/

int
ha_ndbcluster::flush_bulk_insert(bool allow_batch)
{
  NdbTransaction *trans= m_thd_ndb->trans;
  DBUG_ENTER("ha_ndbcluster::flush_bulk_insert");
  DBUG_PRINT("info", ("Sending inserts to NDB, rows_inserted: %d", 
                      (int)m_rows_inserted));
  DBUG_ASSERT(trans);

  if (m_thd_ndb->check_trans_option(Thd_ndb::TRANS_TRANSACTIONS_OFF))
  {
    /*
      signal that transaction will be broken up and hence cannot
      be rolled back
    */
    THD *thd= table->in_use;
    thd->get_transaction()->mark_modified_non_trans_table(Transaction_ctx::SESSION);
    thd->get_transaction()->mark_modified_non_trans_table(Transaction_ctx::STMT);
    if (execute_commit(m_thd_ndb, trans, m_thd_ndb->m_force_send,
                       m_ignore_no_key) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
    if (trans->restart() != 0)
    {
      DBUG_ASSERT(0);
      DBUG_RETURN(-1);
    }
    DBUG_RETURN(0);
  }

  if (!allow_batch &&
      execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }

  DBUG_RETURN(0);
}

void ha_ndbcluster::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("start_bulk_insert");
  DBUG_PRINT("enter", ("rows: %d", (int)rows));
  
  m_rows_inserted= (ha_rows) 0;
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in write_row
      we have a situation where peek_indexed_rows() will be called
      so we cannot batch
    */
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    m_rows_to_insert= 1;
    DBUG_VOID_RETURN;
  }
  if (rows == (ha_rows) 0)
  {
    /* We don't know how many will be inserted, guess */
    m_rows_to_insert=
      (m_autoincrement_prefetch > DEFAULT_AUTO_PREFETCH)
      ? m_autoincrement_prefetch
      : DEFAULT_AUTO_PREFETCH;
    m_autoincrement_prefetch= m_rows_to_insert;
  }
  else
  {
    m_rows_to_insert= rows;
    if (m_autoincrement_prefetch < m_rows_to_insert)
      m_autoincrement_prefetch= m_rows_to_insert;
  }

  DBUG_VOID_RETURN;
}

/**
  End of an insert.
*/
int ha_ndbcluster::end_bulk_insert()
{
  int error= 0;

  DBUG_ENTER("end_bulk_insert");
  // Check if last inserts need to be flushed

  THD *thd= table->in_use;
  Thd_ndb *thd_ndb= m_thd_ndb;
  
  if (!thd_allow_batch(thd) && thd_ndb->m_unsent_bytes)
  {
    const bool allow_batch= (thd_ndb->m_handler != 0);
    error= flush_bulk_insert(allow_batch);
    if (error != 0)
    {
      // The requirement to calling set_my_errno() here is
      // not according to the handler interface specification
      // However there it is still code in Sql_cmd_load_table::execute_inner()
      // which checks 'my_errno' after end_bulk_insert has reported failure
      // The call to set_my_errno() can be removed from here when
      // Bug #26126535 	MYSQL_LOAD DOES NOT CHECK RETURN VALUES
      // FROM HANDLER BULK INSERT FUNCTIONS has been fixed upstream
      set_my_errno(error);
    }
  }

  m_rows_inserted = 0;
  m_rows_to_insert = 1;
  DBUG_RETURN(error);
}


/**
  How many seeks it will take to read through the table.

  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_ndbcluster::scan_time()
{
  DBUG_ENTER("ha_ndbcluster::scan_time()");
  double res= rows2double(stats.records*1000);
  DBUG_PRINT("exit", ("table: %s value: %f", 
                      m_tabname, res));
  DBUG_RETURN(res);
}

/*
  Convert MySQL table locks into locks supported by Ndb Cluster.
  Note that MySQL Cluster does currently not support distributed
  table locks, so to be safe one should set cluster in Single
  User Mode, before relying on table locks when updating tables
  from several MySQL servers
*/

THR_LOCK_DATA **ha_ndbcluster::store_lock(THD *thd,
                                          THR_LOCK_DATA **to,
                                          enum thr_lock_type lock_type)
{
  DBUG_ENTER("store_lock");
  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK) 
  {

    /* If we are not doing a LOCK TABLE, then allow multiple
       writers */
    
    /* Since NDB does not currently have table locks
       this is treated as a ordinary lock */

    const bool in_lock_tables = thd_in_lock_tables(thd);
    const uint sql_command = thd_sql_command(thd);
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) &&
        !(in_lock_tables && sql_command == SQLCOM_LOCK_TABLES))
      lock_type= TL_WRITE_ALLOW_WRITE;
    
    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
       MySQL would use the lock TL_READ_NO_INSERT on t2, and that
       would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
       to t2. Convert the lock to a normal read lock to allow
       concurrent inserts to t2. */
    
    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;

    /**
     * We need locks on source table when
     *   doing offline alter...
     * In 5.1 this worked due to TL_WRITE_ALLOW_READ...
     * but that has been removed in 5.5
     * I simply add this to get it...
     */
    if (sql_command == SQLCOM_ALTER_TABLE)
      lock_type = TL_WRITE;

    m_lock.type=lock_type;
  }
  *to++= &m_lock;

  DBUG_PRINT("exit", ("lock_type: %d", lock_type));
  
  DBUG_RETURN(to);
}

/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement, this will be stored in thd_ndb.stmt.
  If not, we have to start a master transaction if there doesn't exist
  one from before, this will be stored in thd_ndb.all
 
  When a table lock is held one transaction will be started which holds
  the table lock and for each statement a hupp transaction will be started  
  If we are locking the table then:
  - save the NdbDictionary::Table for easy access
  - save reference to table statistics
  - refresh list of the indexes for the table if needed (if altered)
 */

static int ndbcluster_update_apply_status(THD *thd, int do_update)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *ndbtab;
  NdbTransaction *trans= thd_ndb->trans;
  ndb->setDatabaseName(NDB_REP_DB);
  Ndb_table_guard ndbtab_g(dict, NDB_APPLY_TABLE);
  if (!(ndbtab= ndbtab_g.get_table()))
  {
    return -1;
  }
  NdbOperation *op= 0;
  int r= 0;
  r|= (op= trans->getNdbOperation(ndbtab)) == 0;
  DBUG_ASSERT(r == 0);
  if (do_update)
    r|= op->updateTuple();
  else
    r|= op->writeTuple();
  DBUG_ASSERT(r == 0);
  // server_id
  r|= op->equal(0u, (Uint32)thd->server_id);
  DBUG_ASSERT(r == 0);
  if (!do_update)
  {
    // epoch
    r|= op->setValue(1u, (Uint64)0);
    DBUG_ASSERT(r == 0);
  }
  const char* group_master_log_name =
    ndb_mi_get_group_master_log_name();
  const Uint64 group_master_log_pos =
    ndb_mi_get_group_master_log_pos();
  const Uint64 future_event_relay_log_pos =
    ndb_mi_get_future_event_relay_log_pos();
  const Uint64 group_relay_log_pos =
    ndb_mi_get_group_relay_log_pos();

  // log_name
  char tmp_buf[FN_REFLEN];
  ndb_pack_varchar(ndbtab, 2u, tmp_buf,
                   group_master_log_name, strlen(group_master_log_name));
  r|= op->setValue(2u, tmp_buf);
  DBUG_ASSERT(r == 0);
  // start_pos
  r|= op->setValue(3u, group_master_log_pos);
  DBUG_ASSERT(r == 0);
  // end_pos
  r|= op->setValue(4u, group_master_log_pos +
                   (future_event_relay_log_pos - group_relay_log_pos));
  DBUG_ASSERT(r == 0);
  return 0;
}


void
Thd_ndb::transaction_checks()
{
  THD* thd = m_thd;

  if (thd_sql_command(thd) == SQLCOM_LOAD ||
      !THDVAR(thd, use_transactions))
  {
    // Turn off transactional behaviour for the duration of this
    // statement/transaction
    set_trans_option(TRANS_TRANSACTIONS_OFF);
  }

  m_force_send= THDVAR(thd, force_send);
  if (!thd->slave_thread)
    m_batch_size= THDVAR(thd, batch_size);
  else
  {
    m_batch_size= THDVAR(NULL, batch_size); /* using global value */
    /* Do not use hinted TC selection in slave thread */
    THDVAR(thd, optimized_node_selection)=
      THDVAR(NULL, optimized_node_selection) & 1; /* using global value */
  }
}


int ha_ndbcluster::start_statement(THD *thd,
                                   Thd_ndb *thd_ndb,
                                   uint table_count)
{
  NdbTransaction *trans= thd_ndb->trans;
  int error;
  DBUG_ENTER("ha_ndbcluster::start_statement");

  m_thd_ndb= thd_ndb;
  m_thd_ndb->transaction_checks();

  if (table_count == 0)
  {
    trans_register_ha(thd, false, ht, NULL);
    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    {
      if (!trans)
        trans_register_ha(thd, true, ht, NULL);
      thd_ndb->m_handler= NULL;
    }
    else
    {
      /*
        this is an autocommit, we may keep a reference to the
        handler to be used in the commit phase for optimization
        reasons, defering execute
      */
      thd_ndb->m_handler= this;
    }
  }
  else
  {
    /*
      there is more than one handler involved, execute deferal
      not possible
    */
    ha_ndbcluster* handler = thd_ndb->m_handler;
    thd_ndb->m_handler= NULL;
    if (handler != NULL)
    {
      /**
       * If we initially belived that this could be run
       *  using execute deferal...but changed out mind
       *  add handler to thd_ndb->open_tables like it would
       *  have done "normally"
       */
      add_handler_to_open_tables(thd, thd_ndb, handler);
    }
  }
  if (!trans && table_count == 0)
  {
    thd_ndb->reset_trans_options();

    DBUG_PRINT("trans",("Possibly starting transaction"));
    const uint opti_node_select = THDVAR(thd, optimized_node_selection);
    DBUG_PRINT("enter", ("optimized_node_selection: %u", opti_node_select));
    if (!(opti_node_select & 2) ||
        thd->lex->sql_command == SQLCOM_LOAD)
      if (unlikely(!start_transaction(error)))
        DBUG_RETURN(error);

    thd_ndb->init_open_tables();
    thd_ndb->m_slow_path= false;
    if (!(thd_test_options(thd, OPTION_BIN_LOG)) ||
        thd->variables.binlog_format == BINLOG_FORMAT_STMT)
    {
      thd_ndb->set_trans_option(Thd_ndb::TRANS_NO_LOGGING);
      thd_ndb->m_slow_path= true;
    }
    else if (thd->slave_thread)
      thd_ndb->m_slow_path= true;
  }
  DBUG_RETURN(0);
}

int
ha_ndbcluster::add_handler_to_open_tables(THD *thd,
                                          Thd_ndb *thd_ndb,
                                          ha_ndbcluster* handler)
{
  DBUG_ENTER("ha_ndbcluster::add_handler_to_open_tables");
  DBUG_PRINT("info", ("Adding %s", handler->m_share->key_string()));

  /**
   * thd_ndb->open_tables is only used iff thd_ndb->m_handler is not
   */
  DBUG_ASSERT(thd_ndb->m_handler == NULL);
  const void *key= handler->m_share;
  THD_NDB_SHARE *thd_ndb_share= find_or_nullptr(thd_ndb->open_tables, key);
  if (thd_ndb_share == 0)
  {
    thd_ndb_share=
      (THD_NDB_SHARE *) thd->get_transaction()->allocate_memory(sizeof(THD_NDB_SHARE));
    if (!thd_ndb_share)
    {
      mem_alloc_error(sizeof(THD_NDB_SHARE));
      DBUG_RETURN(1);
    }
    thd_ndb_share->key= key;
    thd_ndb_share->stat.last_count= thd_ndb->count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
    thd_ndb->open_tables.emplace(thd_ndb_share->key, thd_ndb_share);
  }
  else if (thd_ndb_share->stat.last_count != thd_ndb->count)
  {
    thd_ndb_share->stat.last_count= thd_ndb->count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
  }

  handler->m_table_info= &thd_ndb_share->stat;
  DBUG_RETURN(0);
}

int ha_ndbcluster::init_handler_for_statement(THD *thd)
{
  /*
    This is the place to make sure this handler instance
    has a started transaction.
     
    The transaction is started by the first handler on which 
    MySQL Server calls external lock
   
    Other handlers in the same stmt or transaction should use 
    the same NDB transaction. This is done by setting up the m_thd_ndb
    pointer to point to the NDB transaction object. 
   */

  DBUG_ENTER("ha_ndbcluster::init_handler_for_statement");
  Thd_ndb *thd_ndb= m_thd_ndb;
  DBUG_ASSERT(thd_ndb);

  // store thread specific data first to set the right context
  m_autoincrement_prefetch= THDVAR(thd, autoincrement_prefetch_sz);
  m_blobs_pending= false;
  release_blobs_buffer();
  m_slow_path= m_thd_ndb->m_slow_path;

  if (unlikely(m_slow_path))
  {
    if (m_share == ndb_apply_status_share && thd->slave_thread)
        m_thd_ndb->set_trans_option(Thd_ndb::TRANS_INJECTED_APPLY_STATUS);
  }

  int ret = 0;
  if (thd_ndb->m_handler == 0)
  {
    DBUG_ASSERT(m_share);
    ret = add_handler_to_open_tables(thd, thd_ndb, this);
  }
  else
  {
    struct Ndb_local_table_statistics &stat= m_table_info_instance;
    stat.last_count= thd_ndb->count;
    stat.no_uncommitted_rows_count= 0;
    stat.records= ~(ha_rows)0;
    m_table_info= &stat;
  }
  DBUG_RETURN(ret);
}

int ha_ndbcluster::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("external_lock");
  if (lock_type != F_UNLCK)
  {
    int error;
    /*
      Check that this handler instance has a connection
      set up to the Ndb object of thd
    */
    if (check_ndb_connection(thd))
      DBUG_RETURN(1);
    Thd_ndb *thd_ndb= get_thd_ndb(thd);

    DBUG_PRINT("enter", ("lock_type != F_UNLCK "
                         "this: 0x%lx  thd: 0x%lx  thd_ndb: %lx  "
                         "thd_ndb->lock_count: %d",
                         (long) this, (long) thd, (long) thd_ndb,
                         thd_ndb->lock_count));

    if ((error= start_statement(thd, thd_ndb,
                                thd_ndb->lock_count++)))
    {
      thd_ndb->lock_count--;
      DBUG_RETURN(error);
    }
    if ((error= init_handler_for_statement(thd)))
    {
      thd_ndb->lock_count--;
      DBUG_RETURN(error);
    }
    DBUG_RETURN(0);
  }
  else
  {
    Thd_ndb *thd_ndb= m_thd_ndb;
    DBUG_ASSERT(thd_ndb);

    DBUG_PRINT("enter", ("lock_type == F_UNLCK "
                         "this: 0x%lx  thd: 0x%lx  thd_ndb: %lx  "
                         "thd_ndb->lock_count: %d",
                         (long) this, (long) thd, (long) thd_ndb,
                         thd_ndb->lock_count));

    if (!--thd_ndb->lock_count)
    {
      DBUG_PRINT("trans", ("Last external_lock"));

      if ((!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
          thd_ndb->trans)
      {
        if (thd_ndb->trans)
        {
          /*
            Unlock is done without a transaction commit / rollback.
            This happens if the thread didn't update any rows
            We must in this case close the transaction to release resources
          */
          DBUG_PRINT("trans",("ending non-updating transaction"));
          thd_ndb->ndb->closeTransaction(thd_ndb->trans);
          thd_ndb->trans= NULL;
          thd_ndb->m_handler= NULL;
        }
      }
    }
    m_table_info= NULL;

    /*
      This is the place to make sure this handler instance
      no longer are connected to the active transaction.

      And since the handler is no longer part of the transaction 
      it can't have open cursors, ops, queries or blobs pending.
    */
    m_thd_ndb= NULL;    

    DBUG_ASSERT(m_active_query == NULL);
    if (m_active_query)
      DBUG_PRINT("warning", ("m_active_query != NULL"));
    m_active_query= NULL;

    if (m_active_cursor)
      DBUG_PRINT("warning", ("m_active_cursor != NULL"));
    m_active_cursor= NULL;

    if (m_multi_cursor)
      DBUG_PRINT("warning", ("m_multi_cursor != NULL"));
    m_multi_cursor= NULL;

    if (m_blobs_pending)
      DBUG_PRINT("warning", ("blobs_pending != 0"));
    m_blobs_pending= 0;
    
    DBUG_RETURN(0);
  }
}

/**
  Unlock the last row read in an open scan.
  Rows are unlocked by default in ndb, but
  for SELECT FOR UPDATE and SELECT LOCK WIT SHARE MODE
  locks are kept if unlock_row() is not called.
*/

void ha_ndbcluster::unlock_row() 
{
  DBUG_ENTER("unlock_row");

  DBUG_PRINT("info", ("Unlocking row"));
  m_lock_tuple= false;
  DBUG_VOID_RETURN;
}

/**
  Start statement, used when one of the tables are locked and also when
  a stored function is executed.

  start_stmt()
    thd                    Thd object
    lock_type              Lock type on table

  RETURN VALUE
    0                      Success
    >0                     Error code

  DESCRIPTION
    This call indicates the start of a statement when one of the tables in
    the statement are locked. In this case we cannot call external_lock.
    It also implies that external_lock is not called at end of statement.
    Rather the handlerton call commit (ndbcluster_commit) is called to
    indicate end of transaction. There are cases thus when the commit call
    actually doesn't refer to a commit but only to an end of statement.

    In the case of stored functions, one stored function is treated as one
    statement and the call to commit comes at the end of the stored function.
*/

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type)
{
  DBUG_ENTER("start_stmt");
  DBUG_ASSERT(thd == table->in_use);

  int error;
  Thd_ndb* thd_ndb= get_thd_ndb(thd);
  if ((error= start_statement(thd, thd_ndb, thd_ndb->start_stmt_count++)))
  {
    thd_ndb->start_stmt_count--;
    DBUG_RETURN(error);
  }
  if ((error= init_handler_for_statement(thd)))
  {
    thd_ndb->start_stmt_count--;
    DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}

NdbTransaction *
ha_ndbcluster::start_transaction_row(const NdbRecord *ndb_record,
                                     const uchar *record,
                                     int &error)
{
  NdbTransaction *trans;
  DBUG_ENTER("ha_ndbcluster::start_transaction_row");
  DBUG_ASSERT(m_thd_ndb);
  DBUG_ASSERT(m_thd_ndb->trans == NULL);

  m_thd_ndb->transaction_checks();

  Ndb *ndb= m_thd_ndb->ndb;

  Uint64 tmp[(MAX_KEY_SIZE_IN_WORDS*MAX_XFRM_MULTIPLY) >> 1];
  char *buf= (char*)&tmp[0];
  trans= ndb->startTransaction(ndb_record,
                               (const char*)record,
                               buf, sizeof(tmp));

  if (trans)
  {
    m_thd_ndb->m_transaction_hint_count[trans->getConnectedNodeId()]++;
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    DBUG_RETURN(m_thd_ndb->trans= trans);
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  DBUG_RETURN(NULL);
}

NdbTransaction *
ha_ndbcluster::start_transaction_key(uint inx_no,
                                     const uchar *key_data,
                                     int &error)
{
  NdbTransaction *trans;
  DBUG_ENTER("ha_ndbcluster::start_transaction_key");
  DBUG_ASSERT(m_thd_ndb);
  DBUG_ASSERT(m_thd_ndb->trans == NULL);

  m_thd_ndb->transaction_checks();

  Ndb *ndb= m_thd_ndb->ndb;
  const NdbRecord *key_rec= m_index[inx_no].ndb_unique_record_key;

  Uint64 tmp[(MAX_KEY_SIZE_IN_WORDS*MAX_XFRM_MULTIPLY) >> 1];
  char *buf= (char*)&tmp[0];
  trans= ndb->startTransaction(key_rec,
                               (const char*)key_data,
                               buf, sizeof(tmp));

  if (trans)
  {
    m_thd_ndb->m_transaction_hint_count[trans->getConnectedNodeId()]++;
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    DBUG_RETURN(m_thd_ndb->trans= trans);
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  DBUG_RETURN(NULL);
}

NdbTransaction *
ha_ndbcluster::start_transaction(int &error)
{
  NdbTransaction *trans;
  DBUG_ENTER("ha_ndbcluster::start_transaction");

  DBUG_ASSERT(m_thd_ndb);
  DBUG_ASSERT(m_thd_ndb->trans == NULL);

  m_thd_ndb->transaction_checks();

  const uint opti_node_select= THDVAR(table->in_use, optimized_node_selection);
  m_thd_ndb->connection->set_optimized_node_selection(opti_node_select & 1);
  if ((trans= m_thd_ndb->ndb->startTransaction(m_table)))
  {
    m_thd_ndb->m_transaction_no_hint_count[trans->getConnectedNodeId()]++;
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    DBUG_RETURN(m_thd_ndb->trans= trans);
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  DBUG_RETURN(NULL);
}
   
NdbTransaction *
ha_ndbcluster::start_transaction_part_id(Uint32 part_id, int &error)
{
  NdbTransaction *trans;
  DBUG_ENTER("ha_ndbcluster::start_transaction_part_id");

  DBUG_ASSERT(m_thd_ndb);
  DBUG_ASSERT(m_thd_ndb->trans == NULL);

  m_thd_ndb->transaction_checks();

  if ((trans= m_thd_ndb->ndb->startTransaction(m_table, part_id)))
  {
    m_thd_ndb->m_transaction_hint_count[trans->getConnectedNodeId()]++;
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    DBUG_RETURN(m_thd_ndb->trans= trans);
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  DBUG_RETURN(NULL);
}
   
/**
  Static error print function called from static handler method
  ndbcluster_commit and ndbcluster_rollback.
*/
static void
ndbcluster_print_error(int error, const NdbOperation *error_op)
{
  DBUG_ENTER("ndbcluster_print_error");
  TABLE_SHARE share;
  const char *tab_name= (error_op) ? error_op->getTableName() : "";
  if (tab_name == NULL)
  {
    DBUG_ASSERT(tab_name != NULL);
    tab_name= "";
  }
  share.db.str= (char*) "";
  share.db.length= 0;
  share.table_name.str= (char *) tab_name;
  share.table_name.length= strlen(tab_name);
  ha_ndbcluster error_handler(ndbcluster_hton, &share);
  error_handler.print_error(error, MYF(0));
  DBUG_VOID_RETURN;
}


/**
  Commit a transaction started in NDB.
*/

int ndbcluster_commit(handlerton*, THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= thd_ndb->trans;
  bool retry_slave_trans = false;
  (void) retry_slave_trans;

  DBUG_ENTER("ndbcluster_commit");
  DBUG_ASSERT(ndb);
  DBUG_PRINT("enter", ("Commit %s", (all ? "all" : "stmt")));
  thd_ndb->start_stmt_count= 0;
  if (trans == NULL)
  {
    DBUG_PRINT("info", ("trans == NULL"));
    DBUG_RETURN(0);
  }
  if (!all && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    /*
      An odditity in the handler interface is that commit on handlerton
      is called to indicate end of statement only in cases where 
      autocommit isn't used and the all flag isn't set.
   
      We also leave quickly when a transaction haven't even been started,
      in this case we are safe that no clean up is needed. In this case
      the MySQL Server could handle the query without contacting the
      NDB kernel.
    */
    thd_ndb->save_point_count++;
    DBUG_PRINT("info", ("Commit before start or end-of-statement only"));
    DBUG_RETURN(0);
  }
  thd_ndb->save_point_count= 0;

  if (unlikely(thd_ndb->m_slow_path))
  {
    if (thd->slave_thread)
    {
      ndbcluster_update_apply_status(thd,
          thd_ndb->check_trans_option(Thd_ndb::TRANS_INJECTED_APPLY_STATUS));
    }
  }

  if (thd->slave_thread)
  {
    /* If this slave transaction has included conflict detecting ops
     * and some defined operations are not yet sent, then perform
     * an execute(NoCommit) before committing, as conflict op handling
     * is done by execute(NoCommit)
     */
    /* TODO : Add as function */
    if (g_ndb_slave_state.conflict_flags & SCS_OPS_DEFINED)
    {
      if (thd_ndb->m_unsent_bytes)
        res = execute_no_commit(thd_ndb, trans, true);
    }

    if (likely(res == 0))
      res = g_ndb_slave_state.atConflictPreCommit(retry_slave_trans);

    if (likely(res == 0))
      res= execute_commit(thd_ndb, trans, 1, true);

    // Copy-out slave thread statistics
    update_slave_api_stats(thd_ndb->ndb);
  }
  else
  {
    if (thd_ndb->m_handler &&
        thd_ndb->m_handler->m_read_before_write_removal_possible)
    {
      /*
        This is an autocommit involving only one table and
        rbwr is on, thus the transaction has already been
        committed in exec_bulk_update() or end_bulk_delete()
      */
      DBUG_PRINT("info", ("autocommit+rbwr, transaction already committed"));
      const NdbTransaction::CommitStatusType commitStatus = trans->commitStatus();
      
      if(commitStatus == NdbTransaction::Committed)
      {
        /* Already committed transaction to save roundtrip */
        DBUG_ASSERT(get_thd_ndb(current_thd)->m_error == false);
      }
      else if(commitStatus == NdbTransaction::Aborted)
      {
        /* Commit failed before transaction was started */ 
        DBUG_ASSERT(get_thd_ndb(current_thd)->m_error == true);
      }
      else if(commitStatus == NdbTransaction::NeedAbort)
      {
        /* Commit attempt failed and rollback is needed */
        res = -1; 
        
      }
      else
      {
        /* Commit was never attempted - this should not be possible */
        DBUG_ASSERT(commitStatus == NdbTransaction::Started || commitStatus == NdbTransaction::NotStarted);
        ndb_log_error("found uncommitted autocommit+rbwr transaction, "
                      "commit status: %d", commitStatus);
        abort();
      }
    }
    else
    {
      const bool ignore_error= applying_binlog(thd);
      res= execute_commit(thd_ndb, trans,
                          THDVAR(thd, force_send),
                          ignore_error);
    }
  }

  if (res != 0)
  {
    if (retry_slave_trans)
    {
      if (st_ndb_slave_state::MAX_RETRY_TRANS_COUNT >
          g_ndb_slave_state.retry_trans_count++)
      {
        /*
           Warning is necessary to cause retry from slave.cc
           exec_relay_log_event()
        */
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_SLAVE_SILENT_RETRY_TRANSACTION,
                     "Slave transaction rollback requested");
        /*
          Set retry count to zero to:
          1) Avoid consuming slave-temp-error retry attempts
          2) Ensure no inter-attempt sleep

          Better fix : Save + restore retry count around transactional
          conflict handling
        */
        ndb_mi_set_relay_log_trans_retries(0);
      }
      else
      {
        /*
           Too many retries, print error and exit - normal
           too many retries mechanism will cause exit
         */
        ndb_log_error("Ndb slave retried transaction %u time(s) in vain.  "
                      "Giving up.",
                      st_ndb_slave_state::MAX_RETRY_TRANS_COUNT);
      }
      res= ER_GET_TEMPORARY_ERRMSG;
    }
    else
    {
      const NdbError err= trans->getNdbError();
      const NdbOperation *error_op= trans->getNdbErrorOperation();
      res= ndb_to_mysql_error(&err);
      if (res != -1)
        ndbcluster_print_error(res, error_op);
    }
  }
  else
  {
    /* Update shared statistics for tables inserted into / deleted from*/
    if (thd_ndb->m_handler &&      // Autocommit Txn
        thd_ndb->m_handler->m_share &&
        thd_ndb->m_handler->m_table_info)
    {
      modify_shared_stats(thd_ndb->m_handler->m_share, thd_ndb->m_handler->m_table_info);
    }

    /* Manual commit: Update all affected NDB_SHAREs found in 'open_tables' */
    for (const auto &key_and_value : thd_ndb->open_tables)
    {
      THD_NDB_SHARE *thd_share= key_and_value.second;
      modify_shared_stats((NDB_SHARE*)thd_share->key, &thd_share->stat);
    }
  }

  ndb->closeTransaction(trans);
  thd_ndb->trans= NULL;
  thd_ndb->m_handler= NULL;

  DBUG_RETURN(res);
}


/**
  Rollback a transaction started in NDB.
*/

static int ndbcluster_rollback(handlerton*, THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= thd_ndb->trans;

  DBUG_ENTER("ndbcluster_rollback");
  DBUG_PRINT("enter", ("all: %d  thd_ndb->save_point_count: %d",
                       all, thd_ndb->save_point_count));
  DBUG_ASSERT(ndb);
  thd_ndb->start_stmt_count= 0;
  if (trans == NULL)
  {
    /* Ignore end-of-statement until real rollback or commit is called */
    DBUG_PRINT("info", ("trans == NULL"));
    DBUG_RETURN(0);
  }
  if (!all &&
      thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) &&
      (thd_ndb->save_point_count > 0))
  {
    /*
      Ignore end-of-statement until real rollback or commit is called
      as ndb does not support rollback statement
      - mark that rollback was unsuccessful, this will cause full rollback
      of the transaction
    */
    DBUG_PRINT("info", ("Rollback before start or end-of-statement only"));
    thd_mark_transaction_to_rollback(thd, 1);
    my_error(ER_WARN_ENGINE_TRANSACTION_ROLLBACK, MYF(0), "NDB");
    DBUG_RETURN(0);
  }
  thd_ndb->save_point_count= 0;
  if (thd->slave_thread)
    g_ndb_slave_state.atTransactionAbort();
  thd_ndb->m_unsent_bytes= 0;
  thd_ndb->m_execute_count++;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  if (trans->execute(NdbTransaction::Rollback) != 0)
  {
    const NdbError err= trans->getNdbError();
    const NdbOperation *error_op= trans->getNdbErrorOperation();
    res= ndb_to_mysql_error(&err);
    if (res != -1) 
      ndbcluster_print_error(res, error_op);
  }
  ndb->closeTransaction(trans);
  thd_ndb->trans= NULL;
  thd_ndb->m_handler= NULL;

  if (thd->slave_thread)
  {
    // Copy-out slave thread statistics
    update_slave_api_stats(thd_ndb->ndb);
  }

  DBUG_RETURN(res);
}


static const char* ndb_table_modifier_prefix = "NDB_TABLE=";

/* Modifiers that we support currently */
static const
struct NDB_Modifier ndb_table_modifiers[] =
{
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("NOLOGGING"), 0, {0} },
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("READ_BACKUP"), 0, {0} },
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("FULLY_REPLICATED"), 0, {0} },
  { NDB_Modifier::M_STRING, STRING_WITH_LEN("PARTITION_BALANCE"), 0, {0} },
  { NDB_Modifier::M_BOOL, 0, 0, 0, {0} }
};

static const char* ndb_column_modifier_prefix = "NDB_COLUMN=";

static const
struct NDB_Modifier ndb_column_modifiers[] =
{
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("MAX_BLOB_PART_SIZE"), 0, {0} },
  { NDB_Modifier::M_BOOL, 0, 0, 0, {0} }
};



static bool
ndb_column_is_dynamic(THD *thd,
                      Field *field,
                      HA_CREATE_INFO *create_info,
                      bool use_dynamic_as_default,
                      NDBCOL::StorageType type)
{
  DBUG_ENTER("ndb_column_is_dynamic");
  /*
    Check if COLUMN_FORMAT is declared FIXED or DYNAMIC.

    The COLUMN_FORMAT for all non primary key columns defaults to DYNAMIC,
    unless ROW_FORMAT is explictly defined.

    If an explicit declaration of ROW_FORMAT as FIXED contradicts
    with a dynamic COLUMN_FORMAT a warning will be issued.

    The COLUMN_FORMAT can also be overridden with --ndb-default-column-format.

    NOTE! For COLUMN_STORAGE defined as DISK, the DYNAMIC COLUMN_FORMAT is not
    supported and a warning will be issued if explicitly declared.
   */
  const bool default_was_fixed=
      (opt_ndb_default_column_format == NDB_DEFAULT_COLUMN_FORMAT_FIXED) ||
      (field->table->s->mysql_version < NDB_VERSION_DYNAMIC_IS_DEFAULT);

  bool dynamic;
  switch (field->column_format()) {
  case(COLUMN_FORMAT_TYPE_FIXED):
    dynamic= false;
    break;
  case(COLUMN_FORMAT_TYPE_DYNAMIC):
    dynamic= true;
    break;
  case(COLUMN_FORMAT_TYPE_DEFAULT):
  default:
    if (create_info->row_type == ROW_TYPE_DEFAULT)
    {
      if (default_was_fixed || // Created in old version where fixed was
                               // the default choice
          (field->flags & PRI_KEY_FLAG)) // Primary key
      {
        dynamic = use_dynamic_as_default;
      }
      else
      {
        dynamic = true;
      }
    }
    else
      dynamic= (create_info->row_type == ROW_TYPE_DYNAMIC);
    break;
  }
  if (type == NDBCOL::StorageTypeDisk)
  {
    if (dynamic)
    {
      DBUG_PRINT("info", ("Dynamic disk stored column %s changed to static",
                          field->field_name));
      dynamic= false;
    }
    if (thd && field->column_format() == COLUMN_FORMAT_TYPE_DYNAMIC)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "DYNAMIC column %s with "
                          "STORAGE DISK is not supported, "
                          "column will become FIXED",
                          field->field_name);
    }
  }

  switch (create_info->row_type) {
  case ROW_TYPE_FIXED:
    if (thd && (dynamic || field_type_forces_var_part(field->type())))
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "Row format FIXED incompatible with "
                          "dynamic attribute %s",
                          field->field_name);
    }
    break;
  default:
    /*
      Columns will be dynamic unless explictly specified FIXED
    */
    break;
  }

  DBUG_RETURN(dynamic);
}


/**
  Define NDB column based on Field.

  Not member of ha_ndbcluster because NDBCOL cannot be declared.

  MySQL text types with character set "binary" are mapped to true
  NDB binary types without a character set.

  Blobs are V2 and striping from mysql level is not supported
  due to lack of syntax and lack of support for partitioning.

  @return
    Returns 0 or mysql error code.
*/

static int
create_ndb_column(THD *thd,
                  NDBCOL &col,
                  Field *field,
                  HA_CREATE_INFO *create_info,
                  bool use_dynamic_as_default = false)
{
  DBUG_ENTER("create_ndb_column");

  char buf[MAX_ATTR_DEFAULT_VALUE_SIZE];
  assert(field->stored_in_db);

  // Set name
  if (col.setName(field->field_name))
  {
    // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  // Get char set
  CHARSET_INFO *cs= const_cast<CHARSET_INFO*>(field->charset());
  // Set type and sizes
  const enum enum_field_types mysql_type= field->real_type();

  NDB_Modifiers column_modifiers(ndb_column_modifier_prefix,
                                 ndb_column_modifiers);
  if (column_modifiers.loadComment(field->comment.str,
                                   field->comment.length) == -1)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "%s",
                        column_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");

    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  const NDB_Modifier * mod_maxblob = column_modifiers.get("MAX_BLOB_PART_SIZE");

  {
    /* Clear default value (col obj is reused for whole table def) */
    col.setDefaultValue(NULL, 0); 

    /* If the data nodes are capable then set native 
     * default.
     */
    bool nativeDefaults =
      ! (thd &&
         (! ndb_native_default_support(get_thd_ndb(thd)->
                                       ndb->getMinDbNodeVersion())));

    if (likely( nativeDefaults ))
    {
      if ((!(field->flags & PRI_KEY_FLAG) ) &&
          type_supports_default_value(mysql_type))
      {
        if (!(field->flags & NO_DEFAULT_VALUE_FLAG))
        {
          my_ptrdiff_t src_offset= field->table->default_values_offset();
          if ((! field->is_real_null(src_offset)) ||
              ((field->flags & NOT_NULL_FLAG)))
          {
            /* Set a non-null native default */
            memset(buf, 0, MAX_ATTR_DEFAULT_VALUE_SIZE);
            get_default_value(buf, field);

            /* For bit columns, default length is rounded up to 
               nearest word, ensuring all data sent
            */
            Uint32 defaultLen = field_used_length(field);
            if(field->type() == MYSQL_TYPE_BIT)
              defaultLen = ((defaultLen + 3) /4) * 4;
            col.setDefaultValue(buf, defaultLen);
          }
        }
      }
    }
  }
  switch (mysql_type) {
  // Numeric types
  case MYSQL_TYPE_TINY:        
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Tinyunsigned);
    else
      col.setType(NDBCOL::Tinyint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_SHORT:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Smallunsigned);
    else
      col.setType(NDBCOL::Smallint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Unsigned);
    else
      col.setType(NDBCOL::Int);
    col.setLength(1);
    break;
  case MYSQL_TYPE_INT24:       
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Mediumunsigned);
    else
      col.setType(NDBCOL::Mediumint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONGLONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Bigunsigned);
    else
      col.setType(NDBCOL::Bigint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_FLOAT:
    col.setType(NDBCOL::Float);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DOUBLE:
    col.setType(NDBCOL::Double);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DECIMAL:    
    {
      Field_decimal *f= (Field_decimal*)field;
      uint precision= f->pack_length();
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Olddecimalunsigned);
        precision-= (scale > 0);
      }
      else
      {
        col.setType(NDBCOL::Olddecimal);
        precision-= 1 + (scale > 0);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  case MYSQL_TYPE_NEWDECIMAL:    
    {
      Field_new_decimal *f= (Field_new_decimal*)field;
      uint precision= f->precision;
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Decimalunsigned);
      }
      else
      {
        col.setType(NDBCOL::Decimal);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  // Date types
  case MYSQL_TYPE_DATETIME:    
    col.setType(NDBCOL::Datetime);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DATETIME2:    
    {
      Field_datetimef *f= (Field_datetimef*)field;
      uint prec= f->decimals();
      col.setType(NDBCOL::Datetime2);
      col.setLength(1);
      col.setPrecision(prec);
    }
    break;
  case MYSQL_TYPE_DATE: // ?
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_NEWDATE:
    col.setType(NDBCOL::Date);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIME:        
    col.setType(NDBCOL::Time);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIME2:        
    {
      Field_timef *f= (Field_timef*)field;
      uint prec= f->decimals();
      col.setType(NDBCOL::Time2);
      col.setLength(1);
      col.setPrecision(prec);
    }
    break;
  case MYSQL_TYPE_YEAR:
    col.setType(NDBCOL::Year);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    col.setType(NDBCOL::Timestamp);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIMESTAMP2:
    {
      Field_timestampf *f= (Field_timestampf*)field;
      uint prec= f->decimals();
      col.setType(NDBCOL::Timestamp2);
      col.setLength(1);
      col.setPrecision(prec);
    }
    break;
  // Char types
  case MYSQL_TYPE_STRING:      
    if (field->pack_length() == 0)
    {
      col.setType(NDBCOL::Bit);
      col.setLength(1);
    }
    else if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
    {
      col.setType(NDBCOL::Binary);
      col.setLength(field->pack_length());
    }
    else
    {
      col.setType(NDBCOL::Char);
      col.setCharset(cs);
      col.setLength(field->pack_length());
    }
    break;
  case MYSQL_TYPE_VAR_STRING: // ?
  case MYSQL_TYPE_VARCHAR:
    {
      Field_varstring* f= (Field_varstring*)field;
      if (f->length_bytes == 1)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Varbinary);
        else {
          col.setType(NDBCOL::Varchar);
          col.setCharset(cs);
        }
      }
      else if (f->length_bytes == 2)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Longvarbinary);
        else {
          col.setType(NDBCOL::Longvarchar);
          col.setCharset(cs);
        }
      }
      else
      {
        DBUG_RETURN(HA_ERR_UNSUPPORTED);
      }
      col.setLength(field->field_length);
    }
    break;
  // Blob types (all come in as MYSQL_TYPE_BLOB)
  mysql_type_tiny_blob:
  case MYSQL_TYPE_TINY_BLOB:
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    // No parts
    col.setPartSize(0);
    col.setStripeSize(0);
    break;
  //mysql_type_blob:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_BLOB:    
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    {
      Field_blob *field_blob= (Field_blob *)field;
      /*
       * max_data_length is 2^8-1, 2^16-1, 2^24-1 for tiny, blob, medium.
       * Tinyblob gets no blob parts.  The other cases are just a crude
       * way to control part size and striping.
       *
       * In mysql blob(256) is promoted to blob(65535) so it does not
       * in fact fit "inline" in NDB.
       */
      if (field_blob->max_data_length() < (1 << 8))
        goto mysql_type_tiny_blob;
      else if (field_blob->max_data_length() < (1 << 16))
      {
        col.setInlineSize(256);
        col.setPartSize(2000);
        col.setStripeSize(0);
        if (mod_maxblob->m_found)
        {
          col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safety */ 13));
        }
      }
      else if (field_blob->max_data_length() < (1 << 24))
        goto mysql_type_medium_blob;
      else
        goto mysql_type_long_blob;
    }
    break;
  mysql_type_medium_blob:
  case MYSQL_TYPE_MEDIUM_BLOB:   
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(4000);
    col.setStripeSize(0);
    if (mod_maxblob->m_found)
    {
      col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safety */ 13));
    }
    break;
  mysql_type_long_blob:
  case MYSQL_TYPE_LONG_BLOB:  
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safety */ 13));
    col.setStripeSize(0);
    // The mod_maxblob modified has no effect here, already at max
    break;

  // MySQL 5.7 binary-encoded JSON type
  case MYSQL_TYPE_JSON:
  {
    /*
      JSON columns are just like LONG BLOB columns except for inline size
      and part size. Inline size is chosen to accommodate a large number
      of embedded json documents without spilling over to the part table.
      The tradeoff is that only three JSON columns can be defined in a table
      due to the large inline size. Part size is chosen to optimize use of
      pages in the part table. Note that much of the JSON functionality is
      available by storing JSON documents in VARCHAR columns, including
      extracting keys from documents to be used as indexes.
     */
    const int NDB_JSON_INLINE_SIZE = 4000;
    const int NDB_JSON_PART_SIZE = 8100;

    col.setType(NDBCOL::Blob);
    col.setInlineSize(NDB_JSON_INLINE_SIZE);
    col.setPartSize(NDB_JSON_PART_SIZE);
    col.setStripeSize(0);
    break;
  }

  // Other types
  case MYSQL_TYPE_ENUM:
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_SET:         
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_BIT:
  {
    int no_of_bits= field->field_length;
    col.setType(NDBCOL::Bit);
    if (!no_of_bits)
      col.setLength(1);
      else
        col.setLength(no_of_bits);
    break;
  }
  case MYSQL_TYPE_NULL:        
    goto mysql_type_unsupported;
  mysql_type_unsupported:
  default:
    DBUG_RETURN(HA_ERR_UNSUPPORTED);
  }
  // Set nullable and pk
  col.setNullable(field->maybe_null());
  col.setPrimaryKey(field->flags & PRI_KEY_FLAG);
  if ((field->flags & FIELD_IN_PART_FUNC_FLAG) != 0)
  {
    col.setPartitionKey(true);
  }

  // Set autoincrement
  if (field->flags & AUTO_INCREMENT_FLAG) 
  {
    col.setAutoIncrement(true);
    ulonglong value= create_info->auto_increment_value ?
      create_info->auto_increment_value : (ulonglong) 1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %llu", value));
    col.setAutoIncrementInitialValue(value);
  }
  else
    col.setAutoIncrement(false);

  // Storage type
  {
    NDBCOL::StorageType type = NDBCOL::StorageTypeMemory;
    switch (field->field_storage_type())
    {
    case HA_SM_DEFAULT:
      DBUG_PRINT("info", ("No storage_type for field, check create_info"));
      if (create_info->storage_media == HA_SM_DISK)
      {
        DBUG_PRINT("info", ("Table storage type is 'disk', using 'disk' "
                            "for field"));
        type = NDBCOL::StorageTypeDisk;
      }
      break;

    case HA_SM_DISK:
      DBUG_PRINT("info", ("Field storage_type is 'disk'"));
      type = NDBCOL::StorageTypeDisk;
      break;

    case HA_SM_MEMORY:
      break;
    }

    DBUG_PRINT("info", ("Using storage type: '%s'",
                        (type == NDBCOL::StorageTypeDisk) ? "disk" : "memory"));
    col.setStorageType(type);
  }

  // Dynamic
  {
    const bool dynamic=
        ndb_column_is_dynamic(thd, field, create_info, use_dynamic_as_default,
                              col.getStorageType());

    DBUG_PRINT("info", ("Using dynamic: %d", dynamic));
    col.setDynamic(dynamic);
  }

  DBUG_RETURN(0);
}

static const NdbDictionary::Object::PartitionBalance g_default_partition_balance =
  NdbDictionary::Object::PartitionBalance_ForRPByLDM;

void ha_ndbcluster::update_create_info(HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_ndbcluster::update_create_info");
  THD *thd= current_thd;
  const NDBTAB *ndbtab= m_table;
  Ndb *ndb= check_ndb_in_thd(thd);

  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    /*
      Find any initial auto_increment value
    */
    for (uint i= 0; i < table->s->fields; i++) 
    {
      Field *field= table->field[i];
      if (field->flags & AUTO_INCREMENT_FLAG)
      {
        ulonglong auto_value;
        uint retries= NDB_AUTO_INCREMENT_RETRIES;
        int retry_sleep= 30; /* 30 milliseconds, transaction */
        for (;;)
        {
          NDB_SHARE::Tuple_id_range_guard g(m_share);
          if (ndb->readAutoIncrementValue(ndbtab, g.range, auto_value))
          {
            if (--retries && !thd->killed &&
                ndb->getNdbError().status == NdbError::TemporaryError)
            {
              ndb_retry_sleep(retry_sleep);
              continue;
            }
            const NdbError err= ndb->getNdbError();
            ndb_log_error("Error %d in ::update_create_info(): %s",
                          err.code, err.message);
            DBUG_VOID_RETURN;
          }
          break;
        }
        if (auto_value > 1)
        {
          create_info->auto_increment_value= auto_value;
        }
        break;
      }
    }
  }

  /**
   * We have things that are required in the comment section of the
   * frm-file. These are essentially table properties that we need to
   * maintain also when we are performing an ALTER TABLE.
   *
   * Our design approach is that if a table is fully replicated and
   * we alter the table, then the table should remain fully replicated
   * unless we explicitly specify in the comment section that we should
   * change the table property.
   *
   * We start by parsing the new comment string. If there are missing
   * parts of the string we will add those parts by creating a new
   * comment string.
   */
  if (thd->lex->sql_command == SQLCOM_ALTER_TABLE)
  {
    update_comment_info(thd, create_info, m_table);
  }
  else if (thd->lex->sql_command == SQLCOM_SHOW_CREATE)
  {
    update_comment_info(thd, NULL, m_table);
  }
  DBUG_VOID_RETURN;
}

void
ha_ndbcluster::update_comment_info(THD* thd,
                                   HA_CREATE_INFO *create_info,
                                   const NdbDictionary::Table *ndbtab)
{
  DBUG_ENTER("ha_ndbcluster::update_comment_info");
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix,
                                ndb_table_modifiers);
  char *comment_str = create_info == NULL ?
                      table->s->comment.str :
                      create_info->comment.str;
  unsigned comment_len = create_info == NULL ?
                      table->s->comment.length :
                      create_info->comment.length;

  if (table_modifiers.loadComment(comment_str,
                                  comment_len) == -1)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "%s",
                        table_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");
    DBUG_VOID_RETURN;
  }
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier *mod_fully_replicated =
    table_modifiers.get("FULLY_REPLICATED");
  const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");
  DBUG_PRINT("info", ("Before: comment_len: %u, comment: %s",
                      (unsigned int)comment_len,
                      comment_str));
  
  bool old_nologging = !ndbtab->getLogging();
  bool old_read_backup = ndbtab->getReadBackupFlag();
  bool old_fully_replicated = ndbtab->getFullyReplicated();
  NdbDictionary::Object::PartitionBalance old_part_bal =
    ndbtab->getPartitionBalance();

  /**
   * We start by calculating how much more space we need in the comment
   * string.
   */
  bool add_nologging = false;
  bool add_read_backup = false;
  bool add_fully_replicated = false;
  bool add_part_bal = false;

  bool is_fully_replicated = false;
  if ((mod_fully_replicated->m_found &&
       mod_fully_replicated->m_val_bool) ||
      (old_fully_replicated &&
       !mod_fully_replicated->m_found))
  {
    is_fully_replicated = true;
  }
  if (old_nologging && !mod_nologging->m_found)
  {
    add_nologging = true;
    table_modifiers.set("NOLOGGING", true);
    DBUG_PRINT("info", ("added nologging"));
  }
  if (!is_fully_replicated &&
      old_read_backup &&
      !mod_read_backup->m_found)
  {
    add_read_backup = true;
    table_modifiers.set("READ_BACKUP", true);
    DBUG_PRINT("info", ("added read_backup"));
  }
  if (old_fully_replicated && !mod_fully_replicated->m_found)
  {
    add_fully_replicated = true;
    table_modifiers.set("FULLY_REPLICATED", true);
    DBUG_PRINT("info", ("added fully_replicated"));
  }
  if (!mod_frags->m_found &&
      (old_part_bal != g_default_partition_balance) &&
      (old_part_bal != NdbDictionary::Object::PartitionBalance_Specific))
  {
    add_part_bal = true;
    const char *old_part_bal_str =
      NdbDictionary::Table::getPartitionBalanceString(old_part_bal);
    table_modifiers.set("PARTITION_BALANCE", old_part_bal_str);
    DBUG_PRINT("info", ("added part_bal_str"));
  }
  if (!(add_nologging ||
        add_read_backup ||
        add_fully_replicated ||
        add_part_bal))
  {
    /* No change of comment is needed. */
    DBUG_VOID_RETURN;
  }

  /**
   * All necessary modifiers are set, now regenerate the comment
   */
  const char *updated_str = table_modifiers.generateCommentString();
  if (updated_str == NULL)
  {
    mem_alloc_error(0);
    DBUG_VOID_RETURN;
  }
  Uint32 new_len = strlen(updated_str);
  char* new_str;
  new_str = (char*)alloc_root(&table->s->mem_root, (size_t)new_len);
  if (new_str == NULL)
  {
    mem_alloc_error(0);
    DBUG_VOID_RETURN;
  }
  memcpy(new_str, updated_str, new_len);
  DBUG_PRINT("info", ("new_str: %s", new_str));

  /* Update structures */
  if (create_info != NULL)
  {
    create_info->comment.str = new_str;
    create_info->comment.length = new_len;
  }
  else
  {
    table->s->comment.str = new_str;
    table->s->comment.length = new_len;
  }
  DBUG_PRINT("info", ("After: comment_len: %u, comment: %s",
                      new_len,
                      new_str));
  DBUG_VOID_RETURN;
}

/*
  Create a table in NDB Cluster
 */
static uint get_no_fragments(ulonglong max_rows)
{
  ulonglong acc_row_size= 25 + /*safety margin*/ 2;
  ulonglong acc_fragment_size= 512*1024*1024;
  return uint((max_rows*acc_row_size)/acc_fragment_size)+1;
}


/*
  Routine to adjust default number of partitions to always be a multiple
  of number of nodes and never more than 4 times the number of nodes.

*/
static
bool
adjusted_frag_count(Ndb* ndb,
                    uint requested_frags,
                    uint &reported_frags)
{
  unsigned no_nodes= g_ndb_cluster_connection->no_db_nodes();
  unsigned no_replicas= no_nodes == 1 ? 1 : 2;

  unsigned no_threads= 1;
  const unsigned no_nodegroups= g_ndb_cluster_connection->max_nodegroup() + 1;

  {
    /**
     * Use SYSTAB_0 to get #replicas, and to guess #threads
     */
    char dbname[FN_HEADLEN+1];
    dbname[FN_HEADLEN]= 0;
    my_stpnmov(dbname, ndb->getDatabaseName(), sizeof(dbname) - 1);
    ndb->setDatabaseName("sys");
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), "SYSTAB_0");
    const NdbDictionary::Table * tab = ndbtab_g.get_table();
    if (tab)
    {
      no_replicas= ndbtab_g.get_table()->getReplicaCount();

      /**
       * Guess #threads
       */
      {
        const Uint32 frags = tab->getFragmentCount();
        Uint32 node = 0;
        Uint32 cnt = 0;
        for (Uint32 i = 0; i<frags; i++)
        {
          Uint32 replicas[4];
          if (tab->getFragmentNodes(i, replicas, NDB_ARRAY_SIZE(replicas)))
          {
            if (node == replicas[0] || node == 0)
            {
              node = replicas[0];
              cnt ++;
            }
          }
        }
        no_threads = cnt; // No of primary replica on 1-node
      }
    }
    ndb->setDatabaseName(dbname);
  }

  const unsigned usable_nodes = no_replicas * no_nodegroups;
  const uint max_replicas = 8 * usable_nodes * no_threads;

  reported_frags = usable_nodes * no_threads; // Start with 1 frag per threads
  Uint32 replicas = reported_frags * no_replicas;

  /**
   * Loop until requested replicas, and not exceed max-replicas
   */
  while (reported_frags < requested_frags &&
         (replicas + usable_nodes * no_threads * no_replicas) <= max_replicas)
  {
    reported_frags += usable_nodes * no_threads;
    replicas += usable_nodes * no_threads * no_replicas;
  }

  return (reported_frags < requested_frags);
}

static
bool
parsePartitionBalance(THD *thd,
                       const NDB_Modifier * mod,
                       NdbDictionary::Object::PartitionBalance * part_bal)
{
  if (mod->m_found == false)
    return false; // OK

  NdbDictionary::Object::PartitionBalance ret =
    NdbDictionary::Table::getPartitionBalance(mod->m_val_str.str);

  if (ret == 0)
  {
    DBUG_PRINT("info", ("PartitionBalance: %s not supported",
                        mod->m_val_str.str));
    /**
     * Comment section contains a partition balance we cannot
     * recognize, we will print warning about this and will
     * not change the comment string.
     */
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG),
                        4500,
                        "Comment contains non-supported fragment"
                        " count type",
                        "NDB");
    return false;
  }

  if (part_bal)
  {
    * part_bal = ret;
  }
  return true;
}


extern bool ndb_fk_util_truncate_allowed(THD* thd,
                                         NdbDictionary::Dictionary* dict,
                                         const char* db,
                                         const NdbDictionary::Table* tab,
                                         bool& allow);

/*
  Forward declaration of the utility functions used
  when creating partitioned tables
*/
static int
create_table_set_up_partition_info(partition_info *part_info,
                                   NdbDictionary::Table&,
                                   Ndb_table_map &);
static int
create_table_set_range_data(const partition_info* part_info,
                            NdbDictionary::Table&);
static int
create_table_set_list_data(const partition_info* part_info,
                           NdbDictionary::Table&);


void ha_ndbcluster::append_create_info(String*)
{
  THD *thd = current_thd;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NDBDICT *dict = ndb->getDictionary();
  ndb->setDatabaseName(table_share->db.str);
  Ndb_table_guard ndbtab_g(dict, table_share->table_name.str);
  const NdbDictionary::Table * tab = ndbtab_g.get_table();
  NdbDictionary::Object::PartitionBalance part_bal = tab->getPartitionBalance();
  bool logged_table = tab->getLogging();
  bool read_backup = tab->getReadBackupFlag();
  bool fully_replicated = tab->getFullyReplicated();

  DBUG_PRINT("info", ("append_create_info: comment: %s, logged_table = %u,"
                      " part_bal = %d, read_backup = %u, fully_replicated = %u",
                      table_share->comment.length == 0 ?
                      "NULL" : table_share->comment.str,
                      logged_table,
                      part_bal,
                      read_backup,
                      fully_replicated));
  if (table_share->comment.length == 0 &&
      part_bal == NdbDictionary::Object::PartitionBalance_Specific &&
      !read_backup &&
      logged_table &&
      !fully_replicated)
  {
    /**
     * No comment set by user
     * The partition balance is default and thus no need to set
     * The table is logged which is default and thus no need to set
     * The table is not using read backup which is default
     */
    return;
  }

  /**
   * Now parse the comment string if there is one to deduce the
   * settings already in the comment string, no need to set a
   * property already set in the comment string.
   */
  NdbDictionary::Object::PartitionBalance comment_part_bal =
    g_default_partition_balance;

  bool comment_part_bal_set = false;
  bool comment_logged_table_set = false;
  bool comment_read_backup_set = false;
  bool comment_fully_replicated_set = false;

  bool comment_logged_table = true;
  bool comment_read_backup = false;
  bool comment_fully_replicated = false;

  if (table_share->comment.length)
  {
    /* Parse the current comment string */
    NDB_Modifiers table_modifiers(ndb_table_modifier_prefix,
                                  ndb_table_modifiers);
    if (table_modifiers.loadComment(table_share->comment.str,
                                    table_share->comment.length) == -1)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "%s",
                          table_modifiers.getErrMsg());
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
               "Syntax error in COMMENT modifier");
      return;
    }
    const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
    const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
    const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");
    const NDB_Modifier *mod_fully_replicated =
      table_modifiers.get("FULLY_REPLICATED");

    if (mod_nologging->m_found)
    {
      /**
       * NOLOGGING is set, ensure that it is set to the same value as
       * the table object value. If it is then no need to print anything.
       */
      comment_logged_table = !mod_nologging->m_val_bool;
      comment_logged_table_set = true;
    }
    if (mod_read_backup->m_found)
    {
      comment_read_backup_set = true;
      comment_read_backup = mod_read_backup->m_val_bool;
    }
    if (mod_frags->m_found)
    {
      if (parsePartitionBalance(thd /* for pushing warning */,
                                 mod_frags,
                                 &comment_part_bal))
      {
        if (comment_part_bal != part_bal)
        {
          /**
           * The table property and the comment on the table differs.
           * Let the comment string stay as is, but push warning
           * about this fact.
           */
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_GET_ERRMSG,
                              ER_THD(thd, ER_GET_ERRMSG),
                              4501,
                              "Table property is not the same as in"
                              " comment for PARTITION_BALANCE"
                              " property",
                              "NDB");
        }
      }
      comment_part_bal_set = true;
    }
    if (mod_fully_replicated->m_found)
    {
      comment_fully_replicated_set = true;
      comment_fully_replicated = mod_fully_replicated->m_val_bool;
    }
  }
  DBUG_PRINT("info", ("comment_read_backup_set: %u, comment_read_backup: %u",
                      comment_read_backup_set,
                      comment_read_backup));
  DBUG_PRINT("info", ("comment_logged_table_set: %u, comment_logged_table: %u",
                      comment_logged_table_set,
                      comment_logged_table));
  DBUG_PRINT("info", ("comment_part_bal_set: %u, comment_part_bal: %d",
                      comment_part_bal_set,
                      comment_part_bal));
  if (!comment_read_backup_set)
  {
    if (read_backup && !fully_replicated)
    {
      /**
       * No property was given in table comment, but table is using read backup
       * Also table isn't fully replicated.
       */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG,
                          ER_THD(thd, ER_GET_ERRMSG),
                          4502,
                          "Table property is READ_BACKUP=1,"
                          " but not in comment",
                          "NDB");
    }
  }
  else if (read_backup != comment_read_backup)
  {
    /**
     * The table property and the comment property differs, we will
     * print comment string as is and issue a warning to this effect.
     */
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG),
                        4502,
                        "Table property is not the same as in"
                        " comment for READ_BACKUP property",
                        "NDB");
  }
  if (!comment_fully_replicated_set)
  {
    if (fully_replicated)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG,
                          ER_THD(thd, ER_GET_ERRMSG),
                          4502,
                          "Table property is FULLY_REPLICATED=1,"
                          " but not in comment",
                          "NDB");
    }
  }
  else if (fully_replicated != comment_fully_replicated)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG),
                        4502,
                        "Table property is not the same as in"
                        " comment for FULLY_REPLICATED property",
                        "NDB");
  }
  if (!comment_logged_table_set)
  {
    if (!logged_table)
    {
      /**
       * No property was given in table comment, but table is not logged.
       */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG,
                          ER_THD(thd, ER_GET_ERRMSG),
                          4502,
                          "Table property is NOLOGGING=1,"
                          " but not in comment",
                          "NDB");
    }
  }
  else if (logged_table != comment_logged_table)
  {
    /**
     * The table property and the comment property differs, we will
     * print comment string as is and issue a warning to this effect.
     */
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG),
                        4502,
                        "Table property is not the same as in"
                        " comment for NOLOGGING property",
                        "NDB");
  }
  if (!comment_part_bal_set)
  {
    if (part_bal != NdbDictionary::Object::PartitionBalance_Specific)
    {
      /**
       * There is a table property not reflected in the COMMENT string,
       * most likely someone has done an ALTER TABLE with a new comment
       * string and hasn't changed this property in this comment string.
       * In this case the table property will stay, so we print this in
       * the SHOW CREATE TABLE comment string.
       */

      /**
       * The default partition balance need not be visible in comment.
       */
      const NdbDictionary::Object::PartitionBalance default_partition_balance =
        g_default_partition_balance;

      if (part_bal != default_partition_balance)
      {
        const char * pbname = NdbDictionary::Table::getPartitionBalanceString(part_bal);
        if (pbname != NULL)
        {
          char msg[200];
          snprintf(msg,
                      sizeof(msg),
                      "Table property is PARTITION_BALANCE=%s but not in comment",
                      pbname);
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_GET_ERRMSG,
                              ER_THD(thd, ER_GET_ERRMSG),
                              4503,
                              msg,
                              "NDB");
        }
        else
        {
          assert(false);
          /**
           * This should never happen, the table property should not be set
           * to an incorrect value. Potential problem if a lower MySQL version
           * is used to print the comment string where the table property comes
           * from a cluster on a newer version where additional types have been
           * added.
           */
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_GET_ERRMSG,
                              ER_THD(thd, ER_GET_ERRMSG),
                              4503,
                              "Table property PARTITION_BALANCE is set to"
                              " an unknown value, could be an upgrade issue"
                              "NDB");
        }
      }
    }
  }
}

static bool drop_table_and_related(THD *thd, Ndb *ndb,
                                   NdbDictionary::Dictionary *dict,
                                   const NdbDictionary::Table *table,
                                   int drop_flags, bool skip_related);

static int drop_table_impl(THD *thd, Ndb *ndb,
                           Ndb_schema_dist_client &schema_dist_client,
                           const char *path, const char *db,
                           const char *table_name);

/**
  Create a table in NDB Cluster

  ERROR HANDLING:
  1) when a new error is added call my_error()/my_printf_error() with proper
  mysql error code from(mysqld_error.h ,mysqld_ername.h). only the first call to
  my_error() will be displayed on the prompt, other error message can be viewed only
  by calling 'SHOW WARNING' command. hence, make sure the new error  is first
  in the error flow.

  2)Caller of ha_ndbcluster::create() will call ha_ndbcluster::print_error() and
  handler::print_error() with the return value.

  so, incase we return MySQL error code, make sure that the error code we return is
  present in handler::print_error(). not all error codes that are listed in mysqld_error.h
  can be returned.

  */

int ha_ndbcluster::create(const char *name, 
                          TABLE *form, 
                          HA_CREATE_INFO *create_info,
                          dd::Table* table_def)
{
  THD *thd= current_thd;
  NDBTAB tab;
  NDBCOL col;
  uint i, pk_length= 0;
  bool use_disk= false;
  bool ndb_sys_table= false;
  int result= 0;
  Ndb_fk_list fk_list_for_truncate;

  // Verify default value for "single user mode" of the table
  DBUG_ASSERT(tab.getSingleUserMode() ==
              NdbDictionary::Table::SingleUserModeLocked);

  DBUG_ENTER("ha_ndbcluster::create");
  DBUG_PRINT("enter", ("name: %s", name));

  /* Use SQL form to create a map from stored field number to column number */
  Ndb_table_map table_map(form);

  /*
    Don't allow CREATE TEMPORARY TABLE, it's not allowed since there is
    no guarantee that the table "is visible only to the current
    session, and is dropped automatically when the session is closed".
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {

    /*
      NOTE! This path is just a safeguard, the mysqld should never try to
      create a temporary table as long as the HTON_TEMPORARY_NOT_SUPPORTED
      flag is set on the handlerton.
    */
    DBUG_ASSERT(false);

    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name, "TEMPORARY");
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  set_dbname(name);
  set_tabname(name);

  ndb_log_verbose(1,
                  "Creating table, name: '%s', m_dbname: '%s', "
                  "m_tabname: '%s', name in DD: '%s'",
                  name, m_dbname, m_tabname,
                  ndb_dd_table_get_name(table_def).c_str());

  Ndb_schema_dist_client schema_dist_client(thd);

  /*
    Check that database name and table name will fit within limits
  */
  if (strlen(m_dbname) > NDB_MAX_DDL_NAME_BYTESIZE ||
      strlen(m_tabname) > NDB_MAX_DDL_NAME_BYTESIZE)
  {
    char *invalid_identifier=
        (strlen(m_dbname) > NDB_MAX_DDL_NAME_BYTESIZE)?m_dbname:m_tabname;
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TOO_LONG_IDENT,
                        "Ndb has an internal limit of %u bytes on the size of schema identifiers",
                        NDB_MAX_DDL_NAME_BYTESIZE);
    my_error(ER_TOO_LONG_IDENT, MYF(0), invalid_identifier);
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  if (check_ndb_connection(thd))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  
  Ndb *ndb= get_ndb(thd);
  NDBDICT *dict= ndb->getDictionary();

  table= form;

  if (create_info->table_options & HA_OPTION_CREATE_FROM_ENGINE)
  {
    // This is the final step of table discovery, the table already exists
    // in NDB and it has already been added to local DD by
    // calling ha_discover() and thus ndbcluster_discover()
    // Just finish this process by setting up the binlog for this table
    const int setup_result =
        ndbcluster_binlog_setup_table(thd, ndb,
                                      m_dbname, m_tabname,
                                      table_def);
    DBUG_ASSERT(setup_result == 0); // Catch in debug
    if (setup_result == HA_ERR_TABLE_EXIST)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_TABLE_EXISTS_ERROR,
                          "Failed to setup replication of table %s.%s",
                          m_dbname, m_tabname);

    }

    DBUG_RETURN(setup_result);
  }

  /*
    Check if the create table is part of a copying alter table.
    Note, this has to be done after the check for auto-discovering
    tables since a table being altered might not be known to the
    mysqld issuing the alter statement.
   */
  if (thd_sql_command(thd) == SQLCOM_ALTER_TABLE)
  {
    DBUG_PRINT("info", ("Detected copying ALTER TABLE"));

    // Check that the table name is a temporary name
    DBUG_ASSERT(ndb_name_is_temp(form->s->table_name.str));

    if (!THDVAR(thd, allow_copying_alter_table) &&
        (thd->lex->alter_info->requested_algorithm ==
         Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT))
    {
      // Copying alter table is not allowed and user
      // have not specified ALGORITHM=COPY

      DBUG_PRINT("info", ("Refusing implicit copying alter table"));
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
              "Implicit copying alter", "ndb_allow_copying_alter_table=0",
              "ALGORITHM=COPY to force the alter");
      DBUG_RETURN(HA_WRONG_CREATE_OPTION);
    }

    /*
      Renaming a table and at the same time doing some other change
      is currently not supported, see Bug #16021021 ALTER ... RENAME
      FAILS TO RENAME ON PARTICIPANT MYSQLD.

      Refuse such ALTER TABLE .. RENAME already when trying to
      create the destination table.
    */
    const uint flags= thd->lex->alter_info->flags;
    if (flags & Alter_info::ALTER_RENAME &&
        flags & ~Alter_info::ALTER_RENAME)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), thd->query().str);
      DBUG_RETURN(ER_NOT_SUPPORTED_YET);
    }
  }

  Thd_ndb *thd_ndb= get_thd_ndb(thd);

  if (!(thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT) ||
        thd_ndb->has_required_global_schema_lock("ha_ndbcluster::create")))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  if (ndb_name_is_temp(m_tabname))
  {
    // Creating table with temporary name, table will only be access by this
    // MySQL Server -> skip schema distribution
    DBUG_PRINT("info", ("Creating table with temporary name"));
  }
  else if (Ndb_schema_dist_client::is_schema_dist_table(m_dbname, m_tabname))
  {
    // Creating the schema distribution table itself -> skip schema distribution
    // but apply special settings for the table
    DBUG_PRINT("info", ("Creating the schema distribution table"));

    // Set mysql.ndb_schema table to read+write also in single user mode
    tab.setSingleUserMode(NdbDictionary::Table::SingleUserModeReadWrite);

    ndb_sys_table= true;

    // Mark the mysql.ndb_schema table as hidden in the DD
    ndb_dd_table_mark_as_hidden(table_def);
  }
  else
  {
    // Prepare schema distribution
    if (!schema_dist_client.prepare(m_dbname, m_tabname))
    {
      // Failed to prepare schema distributions
      DBUG_PRINT("info", ("Schema distribution failed to initialize"));
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
  }

  if (!ndb_apply_status_share)
  {
    if ((strcmp(m_dbname, NDB_REP_DB) == 0 &&
         strcmp(m_tabname, NDB_APPLY_TABLE) == 0))
    {
      ndb_sys_table= true;
    }
  }

  if (thd_sql_command(thd) == SQLCOM_TRUNCATE)
  {
    Ndb_table_guard ndbtab_g(dict, m_tabname);
    if (!ndbtab_g.get_table())
      ERR_RETURN(dict->getNdbError());

    /*
      Don't allow truncate on table which is foreign key parent.
      This is kind of a kludge to get legacy compatibility behaviour
      but it also reduces the complexity involved in rewriting
      fks during this "recreate".
     */
    bool allow;
    if (!ndb_fk_util_truncate_allowed(thd, dict, m_dbname,
                                      ndbtab_g.get_table(), allow))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
    if (!allow)
    {
      my_error(ER_TRUNCATE_ILLEGAL_FK, MYF(0), "");
      DBUG_RETURN(1);
    }

    /* save the foreign key information in fk_list */
    int err;
    if ((err= get_fk_data_for_truncate(dict, ndbtab_g.get_table(),
                                       fk_list_for_truncate)))
      DBUG_RETURN(err);

    DBUG_PRINT("info", ("Dropping and re-creating table for TRUNCATE"));
    const int drop_result = drop_table_impl(
        thd, thd_ndb->ndb, schema_dist_client, name, m_dbname, m_tabname);
    if (drop_result) {
      DBUG_RETURN(drop_result);
    }
  }

  DBUG_PRINT("info", ("Start parse of table modifiers, comment = %s",
                      create_info->comment.str));
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix,
                                ndb_table_modifiers);
  if (table_modifiers.loadComment(create_info->comment.str,
                                  create_info->comment.length) == -1)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "%s",
                        table_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }
  const NDB_Modifier * mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier * mod_frags = table_modifiers.get("PARTITION_BALANCE");
  const NDB_Modifier * mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier * mod_fully_replicated =
    table_modifiers.get("FULLY_REPLICATED");
  NdbDictionary::Object::PartitionBalance part_bal =
    g_default_partition_balance;
  if (parsePartitionBalance(thd /* for pushing warning */,
                             mod_frags,
                             &part_bal) == false)
  {
    /**
     * unable to parse => modifier which is not found
     */
    mod_frags = table_modifiers.notfound();
  }
  else if (ndbd_support_partition_balance(
            ndb->getMinDbNodeVersion()) == 0)
  {
    /**
     * NDB_TABLE=PARTITION_BALANCE not supported by data nodes.
     */
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name,
             "PARTITION_BALANCE not supported by current data node versions");
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  /* Verify we can support read backup table property if set */
  if ((mod_read_backup->m_found ||
       opt_ndb_read_backup) &&
      ndbd_support_read_backup(
            ndb->getMinDbNodeVersion()) == 0)
  {
    /**
     * NDB_TABLE=READ_BACKUP not supported by data nodes.
     */
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name,
             "READ_BACKUP not supported by current data node versions");
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }


  /*
    ROW_FORMAT=[DEFAULT|FIXED|DYNAMIC|COMPRESSED|REDUNDANT|COMPACT]

    Only DEFAULT,FIXED or DYNAMIC supported
  */
  if (!(create_info->row_type == ROW_TYPE_DEFAULT ||
        create_info->row_type == ROW_TYPE_FIXED ||
        create_info->row_type == ROW_TYPE_DYNAMIC))
  {
    /*Unsupported row format requested */
    String err_message;
    err_message.append("ROW_FORMAT=");
    switch (create_info->row_type)
    {
    case ROW_TYPE_COMPRESSED:
      err_message.append("COMPRESSED");
      break;
    case ROW_TYPE_REDUNDANT:
      err_message.append("REDUNDANT");
      break;
    case ROW_TYPE_COMPACT:
      err_message.append("COMPACT");
      break;
    case ROW_TYPE_PAGED:
      err_message.append("PAGED");
      break;
    default:
      err_message.append("<unknown>");
      DBUG_ASSERT(false);
      break;
    }
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name,
             err_message.c_ptr());
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  /* Verify we can support fully replicated table property if set */
  if ((mod_fully_replicated->m_found ||
       opt_ndb_fully_replicated) &&
      ndbd_support_fully_replicated(
            ndb->getMinDbNodeVersion()) == 0)
  {
    /**
     * NDB_TABLE=FULLY_REPLICATED not supported by data nodes.
     */
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name,
             "FULLY_REPLICATED not supported by current data node versions");
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  // Read mysql.ndb_replication settings for this table, if any
  uint32 binlog_flags;
  const st_conflict_fn_def* conflict_fn= NULL;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  uint num_args = MAX_CONFLICT_ARGS;

  Ndb_binlog_client binlog_client(thd, m_dbname, m_tabname);
  if (binlog_client.read_replication_info(ndb, m_dbname,
                                          m_tabname, ::server_id,
                                          &binlog_flags, &conflict_fn,
                                          args, &num_args))
  {
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  /* Reset database name */
  ndb->setDatabaseName(m_dbname);

  // Use mysql.ndb_replication settings when creating table
  if (conflict_fn != NULL)
  {
    switch(conflict_fn->type)
    {
    case CFT_NDB_EPOCH:
    case CFT_NDB_EPOCH_TRANS:
    case CFT_NDB_EPOCH2:
    case CFT_NDB_EPOCH2_TRANS:
    {
      /* Default 6 extra Gci bits allows 2^6 == 64
       * epochs / saveGCP, a comfortable default
       */
      Uint32 numExtraGciBits = 6;
      Uint32 numExtraAuthorBits = 1;

      if ((num_args == 1) &&
          (args[0].type == CFAT_EXTRA_GCI_BITS))
      {
        numExtraGciBits = args[0].extraGciBits;
      }
      DBUG_PRINT("info", ("Setting ExtraRowGciBits to %u, "
                          "ExtraAuthorBits to %u",
                          numExtraGciBits,
                          numExtraAuthorBits));

      tab.setExtraRowGciBits(numExtraGciBits);
      tab.setExtraRowAuthorBits(numExtraAuthorBits);
    }
    default:
      break;
    }
  }

  if ((dict->beginSchemaTrans() == -1))
  {
    DBUG_PRINT("info", ("Failed to start schema transaction"));
    m_table= 0;
    ERR_RETURN(dict->getNdbError());
  }
  DBUG_PRINT("info", ("Started schema transaction"));

  int abort_error = 0;
  int create_result;
  DBUG_PRINT("table", ("name: %s", m_tabname));  
  if (tab.setName(m_tabname))
  {
    abort_error = errno;
    goto abort;
  }
  if (!ndb_sys_table)
  {
    if (THDVAR(thd, table_temporary))
    {
#ifdef DOES_NOT_WORK_CURRENTLY
      tab.setTemporary(true);
#endif
      DBUG_PRINT("info", ("table_temporary set"));
      tab.setLogging(false);
    }
    else if (THDVAR(thd, table_no_logging))
    {
      DBUG_PRINT("info", ("table_no_logging set"));
      tab.setLogging(false);
    }

    if (mod_nologging->m_found)
    {
      DBUG_PRINT("info", ("tab.setLogging(%u)",
                         (!mod_nologging->m_val_bool)));
      tab.setLogging(!mod_nologging->m_val_bool);
    }
    else
    {
      DBUG_PRINT("info",
                 ("mod_nologging not found, getLogging()=%u",
                  tab.getLogging()));
    }
    bool use_fully_replicated;
    bool use_read_backup;

    if (mod_fully_replicated->m_found)
    {
      use_fully_replicated = mod_fully_replicated->m_val_bool;
    }
    else
    {
      use_fully_replicated = opt_ndb_fully_replicated;
    }

    if (mod_read_backup->m_found)
    {
      use_read_backup = mod_read_backup->m_val_bool;
    }
    else if (use_fully_replicated)
    {
      use_read_backup = true;
    }
    else
    {
      use_read_backup = opt_ndb_read_backup;
    }

    if (use_fully_replicated)
    {
      /* Fully replicated table */
      if (mod_read_backup->m_found && !mod_read_backup->m_val_bool)
      {
        /**
         * Cannot mix FULLY_REPLICATED=1 and READ_BACKUP=0 since
         * FULLY_REPLICATED=1 implies READ_BACKUP=1.
         */
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
                 ndbcluster_hton_name,
          "READ_BACKUP=0 cannot be used for fully replicated tables");
        result = HA_WRONG_CREATE_OPTION;
        goto abort_return;
      }
      tab.setReadBackupFlag(true);
      tab.setFullyReplicated(true);
    }
    else if (use_read_backup)
    {
      tab.setReadBackupFlag(true);
    }
  }
  else
  {
    DBUG_PRINT("info", ("ndb_sys_table true"));
  }

  if (thd_sql_command(thd) != SQLCOM_ALTER_TABLE)
  {
    update_comment_info(thd, create_info, &tab);
  }

  {
    /*
      Save the serialized table definition for this table as
      extra metadata of the table in the dictionary of NDB
    */

    dd::sdi_t sdi;
    if (!ndb_sdi_serialize(thd, table_def, m_dbname, sdi))
    {
      result= 1;
      goto abort_return;
    }

    result = tab.setExtraMetadata(2, // version 2 for sdi
                                  sdi.c_str(), (Uint32)sdi.length());
    if (result != 0)
    {
      goto abort_return;
    }
  }

  /*
    ROW_FORMAT=[DEFAULT|FIXED|DYNAMIC|etc.]

    Controls wheter the NDB table will be created with a "varpart reference",
    thus allowing columns to be added inplace at a later time.
    It's possible to turn off "varpart reference" with ROW_FORMAT=FIXED, this
    will save datamemory in NDB at the cost of not being able to add
    columns inplace. Any other value enables "varpart reference".
  */
  if (create_info->row_type == ROW_TYPE_FIXED)
  {
    // CREATE TABLE .. ROW_FORMAT=FIXED
    DBUG_PRINT("info", ("Turning off 'varpart reference'"));
    tab.setForceVarPart(false);
    DBUG_ASSERT(ndb_dd_table_is_using_fixed_row_format(table_def));
  }
  else
  {
    tab.setForceVarPart(true);
    DBUG_ASSERT(!ndb_dd_table_is_using_fixed_row_format(table_def));
  }

  /*
     TABLESPACE=

     Controls wheter the NDB table have corresponding tablespace. It's
     possible for a table to have tablespace although no columns are on disk.
  */
  if (create_info->tablespace)
  {
    // Turn on use_disk if create_info says that table has got a tablespace
    DBUG_PRINT("info", ("Using 'disk' since create_info says table "
                        "have tablespace"));
    use_disk = true;
  }

  /*
    Setup columns
  */
  my_bitmap_map *old_map;
  {
    restore_record(form, s->default_values);
    old_map= tmp_use_all_columns(form, form->read_set);
  }

  for (i= 0; i < form->s->fields; i++) 
  {
    Field *field= form->field[i];
    DBUG_PRINT("info", ("name: %s, type: %u, pack_length: %d, stored: %d",
                        field->field_name, field->real_type(),
                        field->pack_length(), field->stored_in_db));
    if(field->stored_in_db)
    {
      const int create_column_result =
          create_ndb_column(thd, col, field, create_info);
      if (create_column_result)
      {
        abort_error = create_column_result;
        goto abort;
      }

      // Turn on use_disk if the column is configured to be on disk
      if (col.getStorageType() == NDBCOL::StorageTypeDisk)
      {
        use_disk = true;
      }

      if (tab.addColumn(col))
      {
        abort_error = errno;
        goto abort;
      }
      if (col.getPrimaryKey())
        pk_length += (field->pack_length() + 3) / 4;
    }
  }

  tmp_restore_column_map(form->read_set, old_map);
  if (use_disk)
  {
    if (mod_nologging->m_found &&
        mod_nologging->m_val_bool)
    {
      // Setting NOLOGGING=1 on a disk table isn't permitted.
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER_THD(thd, ER_ILLEGAL_HA_CREATE_OPTION),
                          ndbcluster_hton_name,
                          "NOLOGGING=1 on table with fields "
                          "using STORAGE DISK");
      result= HA_ERR_UNSUPPORTED;
      goto abort_return;
    }
    tab.setLogging(true);
    tab.setTemporary(false);

    if (create_info->tablespace)
    {
      tab.setTablespaceName(create_info->tablespace);
    }
    else
    {
      // It's not possible to create a table which uses disk without
      // also specifying a tablespace name
      my_error(ER_MISSING_HA_CREATE_OPTION, MYF(0),
               ndbcluster_hton_name);
      result = HA_MISSING_CREATE_OPTION;
      goto abort_return;
    }
  }

  // Save the table level storage media setting
  switch(create_info->storage_media)
  {
    case HA_SM_DISK:
      tab.setStorageType(NdbDictionary::Column::StorageTypeDisk);
      break;
    case HA_SM_DEFAULT:
      tab.setStorageType(NdbDictionary::Column::StorageTypeDefault);
      break;
    case HA_SM_MEMORY:
      tab.setStorageType(NdbDictionary::Column::StorageTypeMemory);
      break;
  }

  DBUG_PRINT("info", ("Table %s is %s stored with tablespace %s",
                      m_tabname,
                      (use_disk) ? "disk" : "memory",
                      (use_disk) ? tab.getTablespaceName() : "N/A"));

  KEY* key_info;
  for (i= 0, key_info= form->key_info; i < form->s->keys; i++, key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    KEY_PART_INFO *end= key_part + key_info->user_defined_key_parts;
    for (; key_part != end; key_part++)
    {
      if (key_part->field->field_storage_type() == HA_SM_DISK)
      {
        my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION,
                        "Cannot create index on DISK column '%s'. Alter it "
                        "in a way to use STORAGE MEMORY.",
                        MYF(0),
                        key_part->field->field_name);
        result= HA_ERR_UNSUPPORTED;
        goto abort_return;
      }
      table_map.getColumn(tab, key_part->fieldnr-1)->setStorageType(
        NdbDictionary::Column::StorageTypeMemory);
    }
  }

  // No primary key, create shadow key as 64 bit, auto increment  
  if (form->s->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Generating shadow key"));
    if (col.setName("$PK"))
    {
      abort_error = errno;
      goto abort;
    }
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(false);
    col.setPrimaryKey(true);
    col.setAutoIncrement(true);
    col.setDefaultValue(NULL, 0);
    if (tab.addColumn(col))
    {
      abort_error = errno;
      goto abort;
    }
    pk_length += 2;
  }
 
  // Make sure that blob tables don't have too big part size
  for (i= 0; i < form->s->fields; i++) 
  {
    if(! form->field[i]->stored_in_db)
      continue;

    /**
     * The extra +7 concists
     * 2 - words from pk in blob table
     * 5 - from extra words added by tup/dict??
     */

    // Use NDB_MAX_TUPLE_SIZE_IN_WORDS, unless MAX_BLOB_PART_SIZE is set
    switch (form->field[i]->real_type()) {
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:    
    case MYSQL_TYPE_MEDIUM_BLOB:   
    case MYSQL_TYPE_LONG_BLOB: 
    case MYSQL_TYPE_JSON:
    {
      NdbDictionary::Column * column= table_map.getColumn(tab, i);
      unsigned size= pk_length + (column->getPartSize()+3)/4 + 7;
      unsigned ndb_max= NDB_MAX_TUPLE_SIZE_IN_WORDS;
      if (column->getPartSize() > (int)(4 * ndb_max))
        ndb_max= NDB_MAX_TUPLE_SIZE_IN_WORDS; // MAX_BLOB_PART_SIZE

      if (size > ndb_max &&
          (pk_length+7) < ndb_max)
      {
        size= ndb_max - pk_length - 7;
        column->setPartSize(4*size);
      }
      /**
       * If size > NDB_MAX and pk_length+7 >= NDB_MAX
       *   then the table can't be created anyway, so skip
       *   changing part size, and have error later
       */ 
    }
    default:
      break;
    }
  }

  // Assume that table_share->max/min_rows equals create_info->min/max
  // although this is create so create_info should be used
  DBUG_ASSERT(create_info->max_rows == table_share->max_rows);
  DBUG_ASSERT(create_info->min_rows == table_share->min_rows);

  {
    ha_rows max_rows= create_info->max_rows;
    ha_rows min_rows= create_info->min_rows;
    if (max_rows < min_rows)
      max_rows= min_rows;
    if (max_rows != (ha_rows)0) /* default setting, don't set fragmentation */
    {
      tab.setMaxRows(max_rows);
      tab.setMinRows(min_rows);
    }
  }

  // Check partition info
  {
    const int setup_partinfo_result =
        create_table_set_up_partition_info(form->part_info,
                                           tab, table_map);
    if (setup_partinfo_result)
    {
      abort_error = setup_partinfo_result;
      goto abort;
    }
  }

  if (tab.getFullyReplicated() &&
      (tab.getFragmentType() != NDBTAB::HashMapPartition ||
       !tab.getDefaultNoPartitionsFlag()))
  {
    /**
     * Fully replicated are only supported on hash map partitions
     * with standard partition balances, no user defined partitioning
     * fragment count.
     *
     * We expect that ndbapi fail on create table with error 797
     * (Wrong fragment count for fully replicated table)
     */
  }
  if (tab.getFragmentType() == NDBTAB::HashMapPartition && 
      tab.getDefaultNoPartitionsFlag() &&
      !mod_frags->m_found && // Let PARTITION_BALANCE override max_rows
      !tab.getFullyReplicated() && //Ignore max_rows for fully replicated
      (create_info->max_rows != 0 || create_info->min_rows != 0))
  {
    ulonglong rows= create_info->max_rows >= create_info->min_rows ? 
      create_info->max_rows : 
      create_info->min_rows;
    uint no_fragments= get_no_fragments(rows);
    uint reported_frags= no_fragments;
    if (adjusted_frag_count(ndb, no_fragments, reported_frags))
    {
      push_warning(current_thd,
                   Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                   "Ndb might have problems storing the max amount "
                   "of rows specified");
    }
    tab.setFragmentCount(reported_frags);
    tab.setDefaultNoPartitionsFlag(false);
    tab.setFragmentData(0, 0);
    tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
  }

  // Check for HashMap
  if (tab.getFragmentType() == NDBTAB::HashMapPartition && 
      tab.getDefaultNoPartitionsFlag())
  {
    /**
     * Default partitioning
     */
    tab.setFragmentCount(0);
    tab.setFragmentData(0, 0);
    tab.setPartitionBalance(part_bal);
  }
  else if (tab.getFragmentType() == NDBTAB::HashMapPartition)
  {
    NdbDictionary::HashMap hm;
    int res= dict->getDefaultHashMap(hm, tab.getFragmentCount());
    if (res == -1)
    {
      res= dict->initDefaultHashMap(hm, tab.getFragmentCount());
      if (res == -1)
      {
        const NdbError err= dict->getNdbError();
        abort_error = ndb_to_mysql_error(&err);
        goto abort;
      }

      res= dict->createHashMap(hm);
      if (res == -1)
      {
        const NdbError err= dict->getNdbError();
        abort_error = ndb_to_mysql_error(&err);
        goto abort;
      }
    }
  }

  // Create the table in NDB
  if (dict->createTable(tab) != 0)
  {
    const NdbError err= dict->getNdbError();
    abort_error = ndb_to_mysql_error(&err);
    goto abort;
  }



  DBUG_PRINT("info", ("Table '%s/%s' created in NDB, id: %d, version: %d",
                      m_dbname, m_tabname,
                      tab.getObjectId(),
                      tab.getObjectVersion()));

  // Update table definition with the table id and version of the newly
  // created table, the caller will then save this information in the DD
  ndb_dd_table_set_object_id_and_version(table_def,
                                         tab.getObjectId(),
                                         tab.getObjectVersion());

  m_table= &tab;

  // Create secondary indexes
  create_result = create_indexes(thd, form);

  if (create_result == 0 &&
      thd_sql_command(thd) != SQLCOM_TRUNCATE)
  {
    create_result = create_fks(thd, ndb);
  }

  if (create_result == 0 &&
      (thd->lex->sql_command == SQLCOM_ALTER_TABLE ||
       thd->lex->sql_command == SQLCOM_DROP_INDEX ||
       thd->lex->sql_command == SQLCOM_CREATE_INDEX))
  {
    /**
     * mysql doesnt know/care about FK (buhhh)
     *   so we need to copy the old ones ourselves
     */
    create_result = copy_fk_for_offline_alter(thd, ndb, &tab);
  }

  if (create_result == 0 &&
      !fk_list_for_truncate.is_empty())
  {
    /*
     create FKs for the new table from the list got from old table.
     for truncate table.
     */
    create_result = recreate_fk_for_truncate(thd, ndb, tab.getName(),
                                             fk_list_for_truncate);
  }

  m_table= 0;

  if (create_result == 0)
  {
    // Check that NDB and DD metadata matches
    DBUG_ASSERT(Ndb_metadata::compare(thd, &tab, table_def));

    /*
     * All steps have succeeded, try and commit schema transaction
     */
    if (dict->endSchemaTrans() == -1)
    {
      m_table= 0;
      ERR_RETURN(dict->getNdbError());
    }

    Ndb_table_guard ndbtab_g(dict);
    ndbtab_g.init(m_tabname);
    ndbtab_g.invalidate();
  }
  else
  {
    DBUG_PRINT("error", ("Failed to create schema objects in NDB, "
                         "create_result: %d", create_result));
    abort_error = create_result;

abort:
    /*
     *  Some step during table creation failed, abort schema transaction
     */

    // Require that 'abort_error' was set before "goto abort"
    DBUG_ASSERT(abort_error);

    {
      // Flush out the indexes(if any) from ndbapi dictionary's cache first
      NDBDICT::List index_list;
      dict->listIndexes(index_list, tab);
      for (unsigned i = 0; i < index_list.count; i++)
      {
        const char * index_name= index_list.elements[i].name;
        const NDBINDEX* index= dict->getIndexGlobal(index_name, tab);
        if(index != NULL)
        {
          dict->removeIndexGlobal(*index, true);
        }
      }
    }

    // Now abort schema transaction
    DBUG_PRINT("info", ("Aborting schema transaction due to error %i",
                        abort_error));
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
      DBUG_PRINT("info", ("Failed to abort schema transaction, %i",
                          dict->getNdbError().code));
    m_table= 0;

    {
      DBUG_PRINT("info", ("Flush out table %s out of dict cache",
                          m_tabname));
      // Flush the table out of ndbapi's dictionary cache
      Ndb_table_guard ndbtab_g(dict);
      ndbtab_g.init(m_tabname);
      ndbtab_g.invalidate();
    }

    DBUG_RETURN(abort_error);

abort_return:

    // Require that "result" was set before "goto abort_return"
    DBUG_ASSERT(result);

    DBUG_PRINT("info", ("Aborting schema transaction"));
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
      DBUG_PRINT("info", ("Failed to abort schema transaction, %i",
                          dict->getNdbError().code));
    DBUG_RETURN(result);
  }

  // All objects have been created sucessfully and
  // thus "create_result" have to be zero here
  DBUG_ASSERT(create_result == 0);

  /**
   * createTable/index schema transaction OK
   */
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  m_table= ndbtab_g.get_table();
  if (m_table == NULL)
  {
    /*
      Failed to create an index,
      drop the table (and all it's indexes)
    */
    while (!thd->killed)
    {
      if (dict->beginSchemaTrans() == -1)
        goto cleanup_failed;
      if (m_table && dict->dropTableGlobal(*m_table))
      {
        switch (dict->getNdbError().status)
        {
        case NdbError::TemporaryError:
          if (!thd->killed) 
          {
            if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
                == -1)
              DBUG_PRINT("info", ("Failed to abort schema transaction, %i",
                                  dict->getNdbError().code));
            goto cleanup_failed;
          }
          break;
        default:
          break;
        }
      }
      if (dict->endSchemaTrans() == -1)
      {
cleanup_failed:
        DBUG_PRINT("info", ("Could not cleanup failed create %i",
                          dict->getNdbError().code));
        continue; // retry indefinitly
      }
      break;
    }
    m_table = 0;

    // The above code is activated when the table can't be opened in NDB,
    // it then tries to drop the table which it can't open, having m_table
    // being NULL indicates that most of the code is dead(and obfuscated).
    // However an NDB error must have occured since the table can't
    // be opened and as such the NDB error can be returned here
    ERR_RETURN(dict->getNdbError());
  }

  mysql_mutex_lock(&ndbcluster_mutex);

  // Create NDB_SHARE for the new table
  NDB_SHARE *share = NDB_SHARE::create_and_acquire_reference(name, "create");

  mysql_mutex_unlock(&ndbcluster_mutex);

  if (!share)
  {
    // Failed to create the NDB_SHARE instance for this table, most likely OOM.
    // Try to drop the table from NDB before returning
    (void)drop_table_and_related(thd, ndb, dict, m_table,
                                 0,          // drop_flags
                                 false);     // skip_related
    m_table = nullptr;
    my_printf_error(ER_OUTOFMEMORY,
                    "Failed to acquire NDB_SHARE while creating table '%s'",
                    MYF(0), name);
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  if (ndb_name_is_temp(m_tabname))
  {
    // Temporary named table created OK
    NDB_SHARE::release_reference(share, "create"); // temporary ref.
    m_table= 0;
    DBUG_RETURN(0); // All OK
  }

  // Apply the mysql.ndb_replication settings
  // NOTE! Should check error and fail the create
  (void)binlog_client.apply_replication_info(ndb, share,
                                             m_table,
                                             conflict_fn,
                                             args,
                                             num_args,
                                             binlog_flags);

  if (binlog_client.table_should_have_event(share, m_table))
  {
    if (binlog_client.create_event(ndb, m_table, share))
    {
      // Failed to create event for this table, fail the CREATE
      // and drop the table from NDB before returning
      (void)drop_table_and_related(thd, ndb, dict, m_table,
                                   0,          // drop_flags
                                   false);     // skip_related
      NDB_SHARE::release_reference(share, "create"); // temporary ref.
      m_table = nullptr;
      my_printf_error(ER_INTERNAL_ERROR,
                      "Failed to create event for table '%s'",
                      MYF(0), name);
      DBUG_RETURN(ER_INTERNAL_ERROR);
    }

    if (binlog_client.table_should_have_event_op(share))
    {
      Ndb_event_data* event_data;
      if (!binlog_client.create_event_data(share, table_def, &event_data) ||
          binlog_client.create_event_op(share, m_table, event_data))
      {
        // Failed to create event operation for this table, fail the CREATE
        // and drop the table from NDB before returning
        (void)drop_table_and_related(thd, ndb, dict, m_table,
                                     0,          // drop_flags
                                     false);     // skip_related
        NDB_SHARE::release_reference(share, "create"); // temporary ref.
        m_table = nullptr;
        my_printf_error(ER_INTERNAL_ERROR,
                        "Failed to create event operation for table '%s'",
                        MYF(0), name);
        DBUG_RETURN(ER_INTERNAL_ERROR);
      }
    }
  }

  bool schema_dist_result;
  if (thd_sql_command(thd) == SQLCOM_TRUNCATE)
  {
    schema_dist_result = schema_dist_client.truncate_table(
        share->db, share->table_name, m_table->getObjectId(),
        m_table->getObjectVersion());
  }
  else
  {
    DBUG_ASSERT(thd_sql_command(thd) == SQLCOM_CREATE_TABLE);
    schema_dist_result = schema_dist_client.create_table(
        share->db, share->table_name, m_table->getObjectId(),
        m_table->getObjectVersion());
  }
  if (!schema_dist_result)
  {
    // Failed to distribute the create/truncate of this table to the
    // other MySQL Servers, fail the CREATE/TRUNCATE and drop the table
    // from NDB before returning
    // NOTE! Should probably not rollback a failed TRUNCATE by dropping
    // the new table(same in other places above).
    (void)drop_table_and_related(thd, ndb, dict, m_table,
                                 0,                 // drop_flags
                                 false);            // skip_related
    NDB_SHARE::release_reference(share, "create");  // temporary ref.
    m_table = nullptr;
    my_printf_error(ER_INTERNAL_ERROR, "Failed to distribute table '%s'",
                    MYF(0), name);
    DBUG_RETURN(ER_INTERNAL_ERROR);
  }

  NDB_SHARE::release_reference(share, "create"); // temporary ref.

  m_table= 0;
  DBUG_RETURN(0); // All OK
}


int ha_ndbcluster::create_index(THD *thd, const char *name, KEY *key_info,
                                NDB_INDEX_TYPE idx_type) const
{
  int error= 0;
  char unique_name[FN_LEN + 1];
  static const char* unique_suffix= "$unique";
  DBUG_ENTER("ha_ndbcluster::create_index");
  DBUG_PRINT("enter", ("name: %s", name));

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    strxnmov(unique_name, FN_LEN, name, unique_suffix, NullS);
    DBUG_PRINT("info", ("unique_name: '%s'", unique_name));
  }
    
  switch (idx_type){
  case PRIMARY_KEY_INDEX:
    // Do nothing, already created
    break;
  case PRIMARY_KEY_ORDERED_INDEX:
    error= create_ordered_index(thd, name, key_info);
    break;
  case UNIQUE_ORDERED_INDEX:
    if (!(error= create_ordered_index(thd, name, key_info)))
      error= create_unique_index(thd, unique_name, key_info);
    break;
  case UNIQUE_INDEX:
    if (check_index_fields_not_null(key_info))
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
			  ER_NULL_COLUMN_IN_INDEX,
			  "Ndb does not support unique index on NULL valued attributes, index access with NULL value will become full table scan");
    }
    error= create_unique_index(thd, unique_name, key_info);
    break;
  case ORDERED_INDEX:
    if (key_info->algorithm == HA_KEY_ALG_HASH)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
			  ER_ILLEGAL_HA_CREATE_OPTION,
			  ER_THD(thd, ER_ILLEGAL_HA_CREATE_OPTION),
			  ndbcluster_hton_name,
			  "Ndb does not support non-unique "
			  "hash based indexes");
      error= HA_ERR_UNSUPPORTED;
      break;
    }
    error= create_ordered_index(thd, name, key_info);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  
  DBUG_RETURN(error);
}

int ha_ndbcluster::create_ordered_index(THD *thd, const char *name, 
                                        KEY *key_info) const
{
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_RETURN(create_ndb_index(thd, name, key_info, false));
}

int ha_ndbcluster::create_unique_index(THD *thd, const char *name, 
                                       KEY *key_info) const
{

  DBUG_ENTER("ha_ndbcluster::create_unique_index");
  DBUG_RETURN(create_ndb_index(thd, name, key_info, true));
}

/**
  Create an index in NDB Cluster.

  @todo
    Only temporary ordered indexes supported
*/

int ha_ndbcluster::create_ndb_index(THD *thd, const char *name, 
                                    KEY *key_info,
                                    bool unique) const
{
  char index_name[FN_LEN + 1];
  Ndb *ndb= get_ndb(thd);
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part + key_info->user_defined_key_parts;
  
  DBUG_ENTER("ha_ndbcluster::create_index");
  DBUG_PRINT("enter", ("name: %s ", name));

  ndb_protect_char(name, index_name, sizeof(index_name) - 1, '/');
  DBUG_PRINT("info", ("index name: %s ", index_name));

  NdbDictionary::Index ndb_index(index_name);
  if (unique)
    ndb_index.setType(NdbDictionary::Index::UniqueHashIndex);
  else 
  {
    ndb_index.setType(NdbDictionary::Index::OrderedIndex);
    // TODO Only temporary ordered indexes supported
    ndb_index.setLogging(false);
  }
  if (!m_table->getLogging())
    ndb_index.setLogging(false);
  if (((NDBTAB*)m_table)->getTemporary())
    ndb_index.setTemporary(true);
  if (ndb_index.setTable(m_tabname))
  {
    // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  for (; key_part != end; key_part++) 
  {
    Field *field= key_part->field;
    if (field->field_storage_type() == HA_SM_DISK)
    {
        my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION,
                        "Cannot create index on DISK column '%s'. Alter it "
                        "in a way to use STORAGE MEMORY.",
                        MYF(0),
                        field->field_name);
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    if (ndb_index.addColumnName(field->field_name))
    {
      // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
  }
  
  if (dict->createIndex(ndb_index, *m_table))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}


/**
  Truncate a table in NDB, after this command there should
  be no rows left in the table and the autoincrement
  value should be reset to its start value.

  This is currently implemented by dropping the table and
  creating it again, thus rendering it empty

  @param[in,out]  table_def dd::Table describing table to be
                  truncated. Can be adjusted by SE, the changes
                  will be saved in the DD at statement commit time.

  @return         error number
  @retval         0 on success
*/

int ha_ndbcluster::truncate(dd::Table *table_def)
{
  DBUG_ENTER("ha_ndbcluster::truncate");

  /* Table should have been opened */
  DBUG_ASSERT(m_table);

  /* Fill in create_info from the open table */
  HA_CREATE_INFO create_info;
  update_create_info_from_table(&create_info, table);

  // Close the table, will always return 0
  (void)close();

  // Call ha_ndbcluster::create which will detect that this is a
  // truncate and thus drop the table before creating it again.
  const int truncate_error =
      create(table->s->normalized_path.str, table,
             &create_info,
             table_def);

  // Open the table again even if the truncate failed, the caller
  // expect the table to be open. Report any error during open.
  const int open_error =
      open(table->s->normalized_path.str, 0, 0, table_def);

  if (truncate_error)
    DBUG_RETURN(truncate_error);
  DBUG_RETURN(open_error);
}



int ha_ndbcluster::prepare_inplace__add_index(THD *thd,
                                              KEY *key_info,
                                              uint num_of_keys) const
{
  int error= 0;
  DBUG_ENTER("ha_ndbcluster::prepare_inplace__add_index");

  for (uint idx= 0; idx < num_of_keys; idx++)
  {
    KEY *key= key_info + idx;
    KEY_PART_INFO *key_part= key->key_part;
    KEY_PART_INFO *end= key_part + key->user_defined_key_parts;
    // Add fields to key_part struct
    for (; key_part != end; key_part++)
      key_part->field= table->field[key_part->fieldnr];
    // Check index type
    // Create index in ndb
    const NDB_INDEX_TYPE idx_type =
        get_index_type_from_key(idx, key_info, false);
    if ((error = create_index(thd, key_info[idx].name, key, idx_type)))
    {
      break;
    }
  }
  DBUG_RETURN(error);  
}


/*
  Mark the index at m_index[key_num] as to be dropped

  * key_num - position of index in m_index
*/

void ha_ndbcluster::prepare_inplace__drop_index(uint key_num)
{
  DBUG_ENTER("ha_ndbcluster::prepare_inplace__drop_index");

  // Mark indexes for deletion
  DBUG_PRINT("info", ("marking index as dropped: %u", key_num));
  m_index[key_num].status= NDB_INDEX_DATA::TO_BE_DROPPED;

  // Prepare delete of index stat entry
  if (m_index[key_num].type == PRIMARY_KEY_ORDERED_INDEX ||
      m_index[key_num].type == UNIQUE_ORDERED_INDEX ||
      m_index[key_num].type == ORDERED_INDEX)
  {
    const NdbDictionary::Index *index= m_index[key_num].index;
    if (index) // safety
    {
      int index_id= index->getObjectId();
      int index_version= index->getObjectVersion();
      ndb_index_stat_free(m_share, index_id, index_version);
    }
  }
  DBUG_VOID_RETURN;
}
 
/*
  Really drop all indexes marked for deletion
*/
int ha_ndbcluster::inplace__final_drop_index(TABLE *table_arg)
{
  int error;
  DBUG_ENTER("ha_ndbcluster::inplace__final_drop_index");
  // Really drop indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  error= inplace__drop_indexes(ndb, table_arg);
  DBUG_RETURN(error);
}


extern void ndb_fk_util_resolve_mock_tables(THD* thd,
                                            NdbDictionary::Dictionary* dict,
                                            const char* new_parent_db,
                                            const char* new_parent_name);


int
ha_ndbcluster::rename_table_impl(THD* thd, Ndb* ndb,
                                 Ndb_schema_dist_client& schema_dist_client,
                                 const NdbDictionary::Table* orig_tab,
                                 dd::Table* to_table_def,
                                 const char* from, const char* to,
                                 const char* old_dbname,
                                 const char* old_tabname,
                                 const char* new_dbname,
                                 const char* new_tabname,
                                 bool real_rename,
                                 const char* real_rename_db,
                                 const char* real_rename_name,
                                 bool real_rename_log_on_participant,
                                 bool drop_events,
                                 bool create_events,
                                 bool commit_alter)
{
  DBUG_ENTER("ha_ndbcluster::rename_table_impl");
  DBUG_PRINT("info", ("real_rename: %d", real_rename));
  DBUG_PRINT("info", ("real_rename_db: '%s'", real_rename_db));
  DBUG_PRINT("info", ("real_rename_name: '%s'", real_rename_name));
  DBUG_PRINT("info", ("real_rename_log_on_participant: %d",
                      real_rename_log_on_participant));
  // Verify default values of real_rename related parameters
  DBUG_ASSERT(real_rename ||
              (real_rename_db == NULL &&
               real_rename_name == NULL &&
               real_rename_log_on_participant == false));

  DBUG_PRINT("info", ("drop_events: %d", drop_events));
  DBUG_PRINT("info", ("create_events: %d", create_events));
  DBUG_PRINT("info", ("commit_alter: %d", commit_alter));

  NDBDICT* dict = ndb->getDictionary();
  NDBDICT::List index_list;
  if (my_strcasecmp(system_charset_info, new_dbname, old_dbname))
  {
    // When moving tables between databases the indexes need to be
    // recreated, save list of indexes before rename to allow
    // them to be recreated afterwards
    dict->listIndexes(index_list, *orig_tab);
  }

  // Change current database to that of target table
  if (ndb->setDatabaseName(new_dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }

  const int ndb_table_id= orig_tab->getObjectId();
  const int ndb_table_version= orig_tab->getObjectVersion();

  Ndb_share_temp_ref share(from, "rename_table_impl");
  if (real_rename)
  {
    /*
      Prepare the rename on the participant, i.e make the participant
      save the final table name in the NDB_SHARE of the table to be renamed.

      NOTE! The tricky thing here is that the NDB_SHARE haven't yet been
      renamed on the participant and thus you have to use the original
      table name when communicating with the participant, otherwise it
      will not find the share where to stash the final table name.

      Also note that the main reason for doing this prepare phase
      (which the participant can't refuse) is due to lack of placeholders
      available in the schema dist protocol. There are simply not
      enough placeholders available to transfer all required parameters
      at once.
   */
    if (!schema_dist_client.rename_table_prepare(real_rename_db,
                                                 real_rename_name, ndb_table_id,
                                                 ndb_table_version, to))
    {
      // Failed to distribute the prepare rename of this table to the
      // other MySQL Servers, just log error and continue
      // NOTE! Actually it's no point in continuing trying to rename since
      // the participants will most likley not know what the new name of
      // the table is.
      ndb_log_error("Failed to distribute prepare rename for '%s'",
                    real_rename_name);
    }
  }
  NDB_SHARE_KEY* old_key = share->key; // Save current key
  NDB_SHARE_KEY* new_key = NDB_SHARE::create_key(to);
  (void)NDB_SHARE::rename_share(share, new_key);

  NdbDictionary::Table new_tab= *orig_tab;
  new_tab.setName(new_tabname);

  // Create a new serialized table definition for the table to be
  // renamed since it contains the table name
  {
    dd::sdi_t sdi;
    if (!ndb_sdi_serialize(thd, to_table_def, new_dbname, sdi))
    {
      my_error(ER_INTERNAL_ERROR, MYF(0), "Table def. serialization failed");
      DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
    }

    const int set_result =
        new_tab.setExtraMetadata(2, // version 2 for sdi
                                 sdi.c_str(), (Uint32)sdi.length());
    if (set_result != 0)
    {
      my_printf_error(ER_INTERNAL_ERROR,
                      "Failed to set extra metadata during"
                      "rename table, error: %d",
                      MYF(0), set_result);
      DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
    }
  }

  if (dict->alterTableGlobal(*orig_tab, new_tab) != 0)
  {
    const NdbError ndb_error= dict->getNdbError();
    // Rename the share back to old_key
    (void)NDB_SHARE::rename_share(share, old_key);
    // Release the unused new_key
    NDB_SHARE::free_key(new_key);
    ERR_RETURN(ndb_error);
  }
  // Release the unused old_key
  NDB_SHARE::free_key(old_key);

  // Fetch the new table version and write it to the table definition,
  // the caller will then save it into DD
  {
    Ndb_table_guard ndbtab_g(dict, new_tabname);
    const NDBTAB *ndbtab= ndbtab_g.get_table();

    // The id should still be the same as before the rename
    DBUG_ASSERT(ndbtab->getObjectId() == ndb_table_id);
    // The version should have been changed by the rename
    DBUG_ASSERT(ndbtab->getObjectVersion() != ndb_table_version);

    ndb_dd_table_set_object_id_and_version(to_table_def,
                                           ndb_table_id,
                                           ndbtab->getObjectVersion());
  }

  ndb_fk_util_resolve_mock_tables(thd, ndb->getDictionary(),
                                  new_dbname, new_tabname);

  /* handle old table */
  if (drop_events)
  {
    Ndb_binlog_client::drop_events_for_table(thd, ndb,
                                             old_dbname, old_tabname);
  }

  Ndb_binlog_client binlog_client(thd, new_dbname, new_tabname);

  if (create_events)
  {
    Ndb_table_guard ndbtab_g2(dict, new_tabname);
    const NDBTAB *ndbtab= ndbtab_g2.get_table();

    // NOTE! Should check error and fail the rename
    (void)binlog_client.read_and_apply_replication_info(ndb, share, ndbtab,
                                                        ::server_id);

    if (binlog_client.table_should_have_event(share, ndbtab))
    {
      if (binlog_client.create_event(ndb, ndbtab, share))
      {
        // Failed to create event for this table, fail the rename
        // NOTE! Should cover whole function with schema transaction to cleanup
        my_printf_error(ER_INTERNAL_ERROR,
                        "Failed to to create event for table '%s'",
                        MYF(0), share->key_string());
        DBUG_RETURN(ER_INTERNAL_ERROR);
      }

      if (binlog_client.table_should_have_event_op(share))
      {
        // NOTE! Simple renames performs the rename without recreating the event
        // operation, thus the check of share->op below.
        Ndb_event_data* event_data;
        if (share->op == nullptr &&
            (!binlog_client.create_event_data(share, to_table_def,
                                              &event_data) ||
             binlog_client.create_event_op(share, ndbtab, event_data)))
        {
          // Failed to create event for this table, fail the rename
          // NOTE! Should cover whole function with schema transaction to cleanup
          my_printf_error(ER_INTERNAL_ERROR,
                          "Failed to to create event operation for table '%s'",
                          MYF(0), share->key_string());
          DBUG_RETURN(ER_INTERNAL_ERROR);
        }
      }
    }
  }

  if (real_rename)
  {
    /*
      Commit of "real" rename table on participant i.e make the participant
      extract the original table name which it got in prepare.

      NOTE! The tricky thing also here is that the NDB_SHARE haven't yet been
      renamed on the participant and thus you have to use the original
      table name when communicating with the participant, otherwise it
      will not find the share where the final table name has been stashed.

      Also note the special flag which control wheter or not this
      query is written to binlog or not on the participants.
    */
    if (!schema_dist_client.rename_table(real_rename_db, real_rename_name,
                                         ndb_table_id, ndb_table_version,
                                         new_dbname, new_tabname,
                                         real_rename_log_on_participant))
    {
      // Failed to distribute the rename of this table to the
      // other MySQL Servers, just log error and continue
      ndb_log_error("Failed to distribute rename for '%s'", real_rename_name);
    }
  }

  if (commit_alter)
  {
    /* final phase of offline alter table */
    if (!schema_dist_client.alter_table(new_dbname, new_tabname,
                                        ndb_table_id, ndb_table_version))
    {
      // Failed to distribute the alter of this table to the
      // other MySQL Servers, just log error and continue
      ndb_log_error("Failed to distribute 'ALTER TABLE %s'", new_tabname);
    }
  }

  for (unsigned i = 0; i < index_list.count; i++)
  {
    NDBDICT::List::Element& index_el = index_list.elements[i];
    // Recreate any indexes not stored in the system database
    if (my_strcasecmp(system_charset_info,
                      index_el.database, NDB_SYSTEM_DATABASE))
    {
      // Get old index
      ndb->setDatabaseName(old_dbname);
      const NDBINDEX * index= dict->getIndexGlobal(index_el.name,  new_tab);
      DBUG_PRINT("info", ("Creating index %s/%s",
                          index_el.database, index->getName()));
      // Create the same "old" index on new tab
      dict->createIndex(*index, new_tab);
      DBUG_PRINT("info", ("Dropping index %s/%s",
                          index_el.database, index->getName()));
      // Drop old index
      ndb->setDatabaseName(old_dbname);
      dict->dropIndexGlobal(*index);
    }
  }
  DBUG_RETURN(0);
}


static
bool
check_table_id_and_version(const dd::Table* table_def,
                           const NdbDictionary::Table* ndbtab)
{
  DBUG_ENTER("check_table_id_and_version");

  int object_id, object_version;
  if (!ndb_dd_table_get_object_id_and_version(table_def,
                                              object_id, object_version))
  {
    DBUG_RETURN(false);
  }

  // Check that the id and version from DD
  // matches the id and version of the NDB table
  if (ndbtab->getObjectId() != object_id ||
      ndbtab->getObjectVersion() != object_version)
  {
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);

}


/**
  Rename a table in NDB and on the participating mysqld(s)
*/

int ha_ndbcluster::rename_table(const char *from, const char *to,
                                const dd::Table* from_table_def,
                                dd::Table* to_table_def)
{
  THD *thd= current_thd;
  char old_dbname[FN_HEADLEN];
  char new_dbname[FN_HEADLEN];
  char new_tabname[FN_HEADLEN];

  DBUG_ENTER("ha_ndbcluster::rename_table");
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));

  set_dbname(from, old_dbname);
  set_dbname(to, new_dbname);
  set_tabname(from);
  set_tabname(to, new_tabname);

  DBUG_PRINT("info", ("old_tabname: '%s'", m_tabname));
  DBUG_PRINT("info", ("new_tabname: '%s'", new_tabname));

  Ndb_schema_dist_client schema_dist_client(thd);

  /* Check that the new table or database name does not exceed max limit */
  if (strlen(new_dbname) > NDB_MAX_DDL_NAME_BYTESIZE ||
       strlen(new_tabname) > NDB_MAX_DDL_NAME_BYTESIZE)
  {
    char *invalid_identifier=
        (strlen(new_dbname) > NDB_MAX_DDL_NAME_BYTESIZE) ?
          new_dbname : new_tabname;
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TOO_LONG_IDENT,
                        "Ndb has an internal limit of %u bytes on the "\
                        "size of schema identifiers",
                        NDB_MAX_DDL_NAME_BYTESIZE);
    my_error(ER_TOO_LONG_IDENT, MYF(0), invalid_identifier);
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }

  if (check_ndb_connection(thd))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  {
    // Prepare schema distribution, find the names which will be used in this
    // rename by looking at the parameters and the lex structures.
    const char *prepare_dbname;
    const char *prepare_tabname;
    switch (thd_sql_command(thd)) {
      case SQLCOM_CREATE_INDEX:
      case SQLCOM_DROP_INDEX:
      case SQLCOM_ALTER_TABLE:
        prepare_dbname = thd->lex->select_lex->table_list.first->db;
        prepare_tabname = thd->lex->select_lex->table_list.first->table_name;
        break;

      case SQLCOM_RENAME_TABLE:
        prepare_dbname = old_dbname;
        prepare_tabname = m_tabname;
        break;

    default:
      ndb_log_error("INTERNAL ERROR: Unexpected sql command: %u "
                    "using rename_table", thd_sql_command(thd));
      abort();
      break;
    }

    if (!schema_dist_client.prepare_rename(prepare_dbname, prepare_tabname,
                                           new_dbname, new_tabname)) {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
  }

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::rename_table"))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  // Open the table which is to be renamed(aka. the old)
  Ndb *ndb= get_ndb(thd);
  ndb->setDatabaseName(old_dbname);
  NDBDICT *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  const NDBTAB *orig_tab;
  if (!(orig_tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());
  DBUG_PRINT("info", ("NDB table name: '%s'", orig_tab->getName()));

  // Check that id and version of the table to be renamed
  // matches the id and version of the NDB table
  if (!check_table_id_and_version(from_table_def,
                                 orig_tab))
  {
    DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
  }

  // Magically detect if this is a rename or some form of alter
  // and decide which actions need to be performed
  const bool old_is_temp = ndb_name_is_temp(m_tabname);
  const bool new_is_temp = ndb_name_is_temp(new_tabname);

  switch (thd_sql_command(thd))
  {
  case SQLCOM_DROP_INDEX:
  case SQLCOM_CREATE_INDEX:
    DBUG_PRINT("info", ("CREATE or DROP INDEX as copying ALTER"));
    // fallthrough
  case SQLCOM_ALTER_TABLE:
    DBUG_PRINT("info", ("SQLCOM_ALTER_TABLE"));

    if (!new_is_temp && !old_is_temp)
    {
      /*
        This is a rename directly from real to real which occurs:
        1) when the ALTER is "simple" RENAME i.e only consists of RENAME
           and/or enable/disable indexes
        2) as part of inplace ALTER .. RENAME
       */
      DBUG_PRINT("info", ("simple rename detected"));
      DBUG_RETURN(rename_table_impl(thd, ndb,
                                    schema_dist_client,
                                    orig_tab, to_table_def,
                                    from, to,
                                    old_dbname, m_tabname,
                                    new_dbname, new_tabname,
                                    true, // real_rename
                                    old_dbname, // real_rename_db
                                    m_tabname, // real_rename_name
                                    true, // real_rename_log_on_participants
                                    true, // drop_events
                                    true, // create events
                                    false)); // commit_alter
    }

    // Make sure that inplace was not requested
    DBUG_ASSERT(thd->lex->alter_info->requested_algorithm !=
                  Alter_info::ALTER_TABLE_ALGORITHM_INPLACE);

    /*
      This is a copying alter table which is implemented as
      1) Create destination table with temporary name
          -> ha_ndbcluster::create_table('#sql_75636-87')
          There are now the source table and one with temporary name:
             [t1] + [#sql_75636-87]
      2) Copy data from source table to destination table.
      3) Backup the source table by renaming it to another temporary name.
          -> ha_ndbcluster::rename_table('t1', '#sql_86545-98')
          There are now two temporary named tables:
            [#sql_86545-98] + [#sql_75636-87]
      4) Rename the destination table to it's real name.
          ->  ha_ndbcluster::rename_table('#sql_75636-87', 't1')
      5) Drop the source table


    */

    if (new_is_temp)
    {
      /*
        This is an alter table which renames real name to temp name.
        ie. step 3) per above and is the first of
        two rename_table() calls. Drop events from the table.
      */
      DBUG_PRINT("info", ("real -> temp"));
      DBUG_RETURN(rename_table_impl(thd, ndb,
                                    schema_dist_client,
                                    orig_tab, to_table_def,
                                    from, to,
                                    old_dbname, m_tabname,
                                    new_dbname, new_tabname,
                                    false, // real_rename
                                    NULL, // real_rename_db
                                    NULL, // real_rename_name
                                    false, // real_rename_log_on_participants
                                    true, // drop_events
                                    false, // create events
                                    false)); // commit_alter
    }

    if (old_is_temp)
    {
      /*
        This is an alter table which renames temp name to real name.
        ie. step 5) per above and is the second call to rename_table().
        Create new events and commit the alter so that participant are
        made aware that the table changed and can reopen the table.
      */
      DBUG_PRINT("info", ("temp -> real"));

      /*
        Detect if this is the special case which occurs when
        the table is both altered and renamed.

        Important here is to remeber to rename the table also
        on all partiticipants so they will find the table when
        the alter is completed. This is slightly problematic since
        their table is renamed directly from real to real name, while
        the mysqld who performs the alter renames from temp to real
        name. Fortunately it's possible to lookup the original table
        name via THD.
      */
      const char* orig_name = thd->lex->select_lex->table_list.first->table_name;
      const char* orig_db = thd->lex->select_lex->table_list.first->db;
      if (thd->lex->alter_info->flags & Alter_info::ALTER_RENAME &&
          (my_strcasecmp(system_charset_info, orig_db, new_dbname) ||
           my_strcasecmp(system_charset_info, orig_name, new_tabname)))
      {
        DBUG_PRINT("info", ("ALTER with RENAME detected"));
        /*
          Use the original table name when communicating with participant
        */
        const char* real_rename_db = orig_db;
        const char* real_rename_name = orig_name;

        /*
          Don't log the rename query on participant since that would
          cause both an ALTER TABLE RENAME and RENAME TABLE to appear in
          the binlog
        */
        const bool real_rename_log_on_participant = false;
        DBUG_RETURN(rename_table_impl(thd, ndb,
                                      schema_dist_client,
                                      orig_tab, to_table_def,
                                      from, to,
                                      old_dbname, m_tabname,
                                      new_dbname, new_tabname,
                                      true, // real_rename
                                      real_rename_db,
                                      real_rename_name,
                                      real_rename_log_on_participant,
                                      false, // drop_events
                                      true, // create events
                                      true)); // commit_alter
      }

      DBUG_RETURN(rename_table_impl(thd, ndb,
                                    schema_dist_client,
                                    orig_tab, to_table_def,
                                    from, to,
                                    old_dbname, m_tabname,
                                    new_dbname, new_tabname,
                                    false, // real_rename
                                    NULL, // real_rename_db
                                    NULL, // real_rename_name
                                    false, // real_rename_log_on_participants
                                    false, // drop_events
                                    true, // create events
                                    true)); // commit_alter
    }
    break;

  case SQLCOM_RENAME_TABLE:
    DBUG_PRINT("info", ("SQLCOM_RENAME_TABLE"));

    DBUG_RETURN(rename_table_impl(thd, ndb,
                                  schema_dist_client,
                                  orig_tab, to_table_def,
                                  from, to,
                                  old_dbname, m_tabname,
                                  new_dbname, new_tabname,
                                  true, // real_rename
                                  old_dbname, // real_rename_db
                                  m_tabname, // real_rename_name
                                  true, // real_rename_log_on_participants
                                  true, // drop_events
                                  true, // create events
                                  false)); // commit_alter
    break;

  default:
    ndb_log_error("Unexpected rename case detected, sql_command: %d",
                  thd_sql_command(thd));
    abort();
    break;
  }

  // Never reached
  DBUG_RETURN(HA_ERR_UNSUPPORTED);
}





// Declare adapter functions for Dummy_table_util function
extern bool ndb_fk_util_build_list(THD*, NdbDictionary::Dictionary*,
                                   const NdbDictionary::Table*, List<char>&);
extern void ndb_fk_util_drop_list(THD*, Ndb* ndb, NdbDictionary::Dictionary*, List<char>&);
extern bool ndb_fk_util_drop_table(THD*, Ndb* ndb, NdbDictionary::Dictionary*,
                                   const NdbDictionary::Table*);
extern bool ndb_fk_util_is_mock_name(const char* table_name);

/**
  Delete table and its related objects from NDB.
*/

static
bool
drop_table_and_related(THD* thd, Ndb* ndb, NdbDictionary::Dictionary* dict,
                       const NdbDictionary::Table* table,
                       int drop_flags, bool skip_related)
{
  DBUG_ENTER("drop_table_and_related");
  DBUG_PRINT("enter", ("cascade_constraints: %d dropdb: %d skip_related: %d",
    static_cast<bool>(drop_flags & NDBDICT::DropTableCascadeConstraints),
    static_cast<bool>(drop_flags & NDBDICT::DropTableCascadeConstraintsDropDB),
    skip_related));

  /*
    Build list of objects which should be dropped after the table
    unless the caller ask to skip dropping related
  */
  List<char> drop_list;
  if (!skip_related &&
      !ndb_fk_util_build_list(thd, dict, table, drop_list))
  {
    DBUG_RETURN(false);
  }

  // Drop the table
  if (dict->dropTableGlobal(*table, drop_flags) != 0)
  {
    const NdbError& ndb_err = dict->getNdbError();
    if (ndb_err.code == 21080 &&
        thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
    {
      /*
        Drop was not allowed because table is still referenced by
        foreign key(s). Since foreign_key_checks=0 the problem is
        worked around by creating a mock table, recreating the foreign
        key(s) to point at the mock table and finally dropping
        the requested table.
      */
      if (!ndb_fk_util_drop_table(thd, ndb, dict, table))
      {
        DBUG_RETURN(false);
      }
    }
    else
    {
      DBUG_RETURN(false);
    }
  }

  // Drop objects which should be dropped after table
  ndb_fk_util_drop_list(thd, ndb, dict, drop_list);

  DBUG_RETURN(true);
}


static
int
drop_table_impl(THD *thd, Ndb *ndb,
                Ndb_schema_dist_client& schema_dist_client,
                const char *path,
                const char *db,
                const char *table_name)
{
  DBUG_ENTER("drop_table_impl");
  NDBDICT *dict= ndb->getDictionary();
  int ndb_table_id= 0;
  int ndb_table_version= 0;

  NDB_SHARE *share=
      NDB_SHARE::acquire_reference_by_key(path,
                                          "delete_table");

  bool skip_related= false;
  int drop_flags = 0;
  // Copying alter can leave temporary named table which is parent of old FKs
  if ((thd_sql_command(thd) == SQLCOM_ALTER_TABLE ||
       thd_sql_command(thd) == SQLCOM_DROP_INDEX ||
       thd_sql_command(thd) == SQLCOM_CREATE_INDEX) &&
      ndb_name_is_temp(table_name))
  {
    DBUG_PRINT("info", ("Using cascade constraints for ALTER of temp table"));
    drop_flags |= NDBDICT::DropTableCascadeConstraints;
    // Cascade constraint is used and related will be dropped anyway
    skip_related = true;
  }

  if (thd_sql_command(thd) == SQLCOM_DROP_DB)
  {
    DBUG_PRINT("info", ("Using cascade constraints DB for drop database"));
    drop_flags |= NDBDICT::DropTableCascadeConstraintsDropDB;
  }

  if (thd_sql_command(thd) == SQLCOM_TRUNCATE)
  {
    DBUG_PRINT("info", ("Deleting table for TRUNCATE, skip dropping related"));
    skip_related= true;
  }

  /* Drop the table from NDB */
  int res= 0;
  {
    ndb->setDatabaseName(db);
    while (1)
    {
      Ndb_table_guard ndbtab_g(dict, table_name);
      if (ndbtab_g.get_table())
      {
    retry_temporary_error2:
        if (drop_table_and_related(thd, ndb, dict, ndbtab_g.get_table(),
                                   drop_flags, skip_related))
        {
          ndb_table_id= ndbtab_g.get_table()->getObjectId();
          ndb_table_version= ndbtab_g.get_table()->getObjectVersion();
          DBUG_PRINT("info", ("success 2"));
          break;
        }
        else
        {
          switch (dict->getNdbError().status)
          {
            case NdbError::TemporaryError:
              if (!thd->killed) 
                goto retry_temporary_error2; // retry indefinitly
              break;
            default:
              if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT)
              {
                ndbtab_g.invalidate();
                continue;
              }
              break;
          }
        }
      }
      res= ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info", ("error(2) %u", res));
      break;
    }
  }

  if (res)
  {
    // The drop table failed for some reason, just release the share
    // reference and return the error
    if (share)
    {
      NDB_SHARE::release_reference(share, "delete_table");
    }
    DBUG_RETURN(res);
  }

  // Drop table is successful even if table didn't exist in NDB
  const bool table_dropped= dict->getNdbError().code != 709;
  if (table_dropped)
  {
    // Drop the event(s) for the table
    Ndb_binlog_client::drop_events_for_table(thd, ndb,
                                             db, table_name);
  }

  if (share)
  {
    // Wait for binlog thread to detect the dropped table
    // and release it's event operations
    ndbcluster_binlog_wait_synch_drop_table(thd, share);
  }

  if (!ndb_name_is_temp(table_name) &&
      thd_sql_command(thd) != SQLCOM_TRUNCATE &&
      thd_sql_command(thd) != SQLCOM_DROP_DB)
  {
    if (!schema_dist_client.drop_table(db, table_name,
                                       ndb_table_id, ndb_table_version))
    {
      // Failed to distribute the drop of this table to the
      // other MySQL Servers, just log error and continue
      ndb_log_error("Failed to distribute 'DROP TABLE %s'", table_name);
    }
  }


  /*
    Detect the special case which occurs when a table is altered
    to another engine. In such case the altered table has been
    renamed to a temporary name in the same engine before copying
    the data to the new table in the other engine. When copying is
    successful, the original table(which now has a temporary name)
    is then dropped. However the participants has not yet been informed
    about the alter. Since this is the last call that ndbcluster get
    for this alter and it's time to inform the participants that the
    original table is no longer in NDB. Unfortunately the original
    table name is not available in this function, but it's possible
    to look that up via THD.
  */
  if (thd_sql_command(thd) == SQLCOM_ALTER_TABLE)
  {
    const HA_CREATE_INFO* create_info = thd->lex->create_info;
    if (create_info->used_fields & HA_CREATE_USED_ENGINE &&
        create_info->db_type != ndbcluster_hton)
    {
      DBUG_PRINT("info",
                 ("ALTER to different engine = '%s' detected",
                  ha_resolve_storage_engine_name(create_info->db_type)));

      // Assumption is that this is the drop of original table
      // which now has a temporary name
      DBUG_ASSERT(ndb_name_is_temp(table_name));

      const char* orig_db = thd->lex->select_lex->table_list.first->db;
      const char* orig_name =
          thd->lex->select_lex->table_list.first->table_name;
      DBUG_PRINT("info", ("original table name: '%s.%s'", orig_db, orig_name));

      if (!schema_dist_client.drop_table(orig_db, orig_name,
                                         ndb_table_id, ndb_table_version))
      {
        // Failed to distribute the drop of this table to the
        // other MySQL Servers, just log error and continue
        ndb_log_error("Failed to distribute 'DROP TABLE %s'", orig_name);
      }
    }
  }

  if (share)
  {
    mysql_mutex_lock(&ndbcluster_mutex);
    NDB_SHARE::mark_share_dropped(&share);
    NDB_SHARE::release_reference_have_lock(share, "delete_table");
    mysql_mutex_unlock(&ndbcluster_mutex);
  }

  DBUG_RETURN(0);
}


int ha_ndbcluster::delete_table(const char *path, const dd::Table *)
{
  THD *thd= current_thd;

  DBUG_ENTER("ha_ndbcluster::delete_table");
  DBUG_PRINT("enter", ("path: %s", path));

  // Never called on an open handler
  DBUG_ASSERT(m_table == NULL);

  set_dbname(path);
  set_tabname(path);

  Ndb_schema_dist_client schema_dist_client(thd);

  const char* prepare_name = m_tabname;
  if (ndb_name_is_temp(prepare_name))
  {
    prepare_name = thd->lex->select_lex->table_list.first->table_name;
  }

  if (!schema_dist_client.prepare(m_dbname, prepare_name)) {
    /* Don't allow delete table unless schema distribution is ready */
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  if (check_ndb_connection(thd))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::delete_table"))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  /*
    Drop table in NDB and on the other mysqld(s)
  */
  const int drop_result = drop_table_impl(thd, thd_ndb->ndb, schema_dist_client,
                                          path, m_dbname, m_tabname);
  DBUG_RETURN(drop_result);
}

void ha_ndbcluster::get_auto_increment(ulonglong offset, ulonglong increment,
                                       ulonglong, ulonglong *first_value,
                                       ulonglong *nb_reserved_values)
{
  Uint64 auto_value;
  THD *thd= current_thd;
  DBUG_ENTER("get_auto_increment");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));
  Ndb *ndb= get_ndb(table->in_use);
  uint retries= NDB_AUTO_INCREMENT_RETRIES;
  int retry_sleep= 30; /* 30 milliseconds, transaction */
  for (;;)
  {
    NDB_SHARE::Tuple_id_range_guard g(m_share);
    if ((m_skip_auto_increment &&
         ndb->readAutoIncrementValue(m_table, g.range, auto_value)) ||
        ndb->getAutoIncrementValue(m_table, g.range, auto_value, 
                                   Uint32(m_autoincrement_prefetch), 
                                   increment, offset))
    {
      if (--retries && !thd->killed &&
          ndb->getNdbError().status == NdbError::TemporaryError)
      {
        ndb_retry_sleep(retry_sleep);
        continue;
      }
      const NdbError err= ndb->getNdbError();
      ndb_log_error("Error %d in ::get_auto_increment(): %s",
                    err.code, err.message);
      *first_value= ~(ulonglong) 0;
      DBUG_VOID_RETURN;
    }
    break;
  }
  *first_value= (longlong)auto_value;
  /* From the point of view of MySQL, NDB reserves one row at a time */
  *nb_reserved_values= 1;
  DBUG_VOID_RETURN;
}


/**
  Constructor for the NDB Cluster table handler .
*/

ha_ndbcluster::ha_ndbcluster(handlerton *hton, TABLE_SHARE *table_arg):
  handler(hton, table_arg),
  m_table_map(NULL),
  m_thd_ndb(NULL),
  m_active_cursor(NULL),
  m_table(NULL),
  m_ndb_record(0),
  m_ndb_hidden_key_record(0),
  m_table_info(NULL),
  m_share(0),
  m_key_fields(NULL),
  m_part_info(NULL),
  m_user_defined_partitioning(false),
  m_use_partition_pruning(false),
  m_sorted(false),
  m_use_write(false),
  m_ignore_dup_key(false),
  m_has_unique_index(false),
  m_ignore_no_key(false),
  m_read_before_write_removal_possible(false),
  m_read_before_write_removal_used(false),
  m_rows_updated(0),
  m_rows_deleted(0),
  m_rows_to_insert((ha_rows) 1),
  m_rows_inserted((ha_rows) 0),
  m_delete_cannot_batch(false),
  m_update_cannot_batch(false),
  m_skip_auto_increment(true),
  m_blobs_pending(0),
  m_is_bulk_delete(false),
  m_blobs_row_total_size(0),
  m_blobs_buffer(0),
  m_blobs_buffer_size(0),
  m_dupkey((uint) -1),
  m_autoincrement_prefetch(DEFAULT_AUTO_PREFETCH),
  m_pushed_join_member(NULL),
  m_pushed_join_operation(-1),
  m_disable_pushed_join(false),
  m_active_query(NULL),
  m_pushed_operation(NULL),
  m_cond(nullptr),
  m_multi_cursor(NULL)
{
  uint i;
 
  DBUG_ENTER("ha_ndbcluster");

  m_tabname[0]= '\0';
  m_dbname[0]= '\0';

  stats.records= ~(ha_rows)0; // uninitialized
  stats.block_size= 1024;

  for (i= 0; i < MAX_KEY; i++)
    ndb_init_index(m_index[i]);

  // make sure is initialized
  init_alloc_root(PSI_INSTRUMENT_ME, &m_fk_mem_root, fk_root_block_size, 0);
  m_fk_data= NULL;

  DBUG_VOID_RETURN;
}


/**
  Destructor for NDB Cluster table handler.
*/

ha_ndbcluster::~ha_ndbcluster() 
{
  THD *thd= current_thd;
  Ndb *ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
  DBUG_ENTER("~ha_ndbcluster");

  if (m_share)
  {
    // NOTE! Release the m_share acquired in create(), this
    // violates the normal flow which acquires in open() and
    // releases in close(). Code path seems unused.
    DBUG_ASSERT(false);

    NDB_SHARE::release_for_handler(m_share, this);
  }
  release_metadata(thd, ndb);
  release_blobs_buffer();

  // Check for open cursor/transaction
  DBUG_ASSERT(m_thd_ndb == NULL);

  // Discard any generated condition
  delete m_cond;
  m_cond = nullptr;

  DBUG_PRINT("info", ("Deleting pushed joins"));
  DBUG_ASSERT(m_active_query == NULL);
  DBUG_ASSERT(m_active_cursor == NULL);
  if (m_pushed_join_operation==PUSHED_ROOT)
  {
    delete m_pushed_join_member;             // Also delete QueryDef
  }
  m_pushed_join_member= NULL;

  // make sure is released
  free_root(&m_fk_mem_root, 0);
  m_fk_data= NULL;
  DBUG_VOID_RETURN;
}


/**
  Open a table for further use
  - fetch metadata for this table from NDB
  - check that table exists

  @retval
    0    ok
  @retval
    < 0  Table has changed
*/

int ha_ndbcluster::open(const char *name, int, uint,
                        const dd::Table* table_def)
{
  THD *thd= current_thd;
  int res;
  KEY *key;
  KEY_PART_INFO *key_part_info;
  uint key_parts, i, j;
  DBUG_ENTER("ha_ndbcluster::open");
  DBUG_PRINT("enter", ("name: %s", name));

  if (table_share->primary_key != MAX_KEY)
  {
    /*
      Setup ref_length to make room for the whole
      primary key to be written in the ref variable
    */
    key= table->key_info+table_share->primary_key;
    ref_length= key->key_length;
  }
  else
  {
    if (m_user_defined_partitioning)
    {
      /* Add space for partid in ref */
      ref_length+= sizeof(m_part_id);
    }
  }
  DBUG_PRINT("info", ("ref_length: %d", ref_length));

  {
    char* bitmap_array;
    uint extra_hidden_keys= table_share->primary_key != MAX_KEY ? 0 : 1;
    uint n_keys= table_share->keys + extra_hidden_keys;
    uint ptr_size= sizeof(MY_BITMAP*) * (n_keys + 1 /* null termination */);
    uint map_size= sizeof(MY_BITMAP) * n_keys;
    m_key_fields= (MY_BITMAP**)my_malloc(PSI_INSTRUMENT_ME,
                                         ptr_size + map_size,
                                         MYF(MY_WME + MY_ZEROFILL));
    if (!m_key_fields)
    {
      local_close(thd, false);
      DBUG_RETURN(1);
    } 
    bitmap_array= ((char*)m_key_fields) + ptr_size;
    for (i= 0; i < n_keys; i++)
    {
      my_bitmap_map *bitbuf= NULL;
      bool is_hidden_key= (i == table_share->keys);
      m_key_fields[i]= (MY_BITMAP*)bitmap_array;
      if (is_hidden_key || (i == table_share->primary_key))
      {
        m_pk_bitmap_p= m_key_fields[i];
        bitbuf= m_pk_bitmap_buf;
      }
      if (bitmap_init(m_key_fields[i], bitbuf,
                      table_share->fields, false))
      {
        m_key_fields[i]= NULL;
        local_close(thd, false);
        DBUG_RETURN(1);
      }
      if (!is_hidden_key)
      {
        key= table->key_info + i;
        key_part_info= key->key_part;
        key_parts= key->user_defined_key_parts;
        for (j= 0; j < key_parts; j++, key_part_info++)
          bitmap_set_bit(m_key_fields[i], key_part_info->fieldnr-1);
      }
      else
      {
        uint field_no= table_share->fields;
        ((uchar *)m_pk_bitmap_buf)[field_no>>3]|= (1 << (field_no & 7));
      }
      bitmap_array+= sizeof(MY_BITMAP);
    }
    m_key_fields[i]= NULL;
  }

  set_dbname(name);
  set_tabname(name);

  if ((res= check_ndb_connection(thd)) != 0)
  {
    local_close(thd, false);
    DBUG_RETURN(res);
  }

  // Acquire NDB_SHARE reference for handler
  m_share = NDB_SHARE::acquire_for_handler(name, this);
  if (m_share == nullptr)
  {
    // NOTE! This never happens, the NDB_SHARE should already have been
    // created by schema distribution or auto discovery
    local_close(thd, false);
    DBUG_RETURN(1);
  }

  // Init table lock structure
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);

  if ((res= get_metadata(thd, table_def)))
  {
    local_close(thd, false);
    DBUG_RETURN(res);
  }

  if ((res= update_stats(thd, 1)) ||
      (res= info(HA_STATUS_CONST)))
  {
    local_close(thd, true);
    DBUG_RETURN(res);
  }
  if (ndb_binlog_is_read_only())
  {
    table->db_stat|= HA_READ_ONLY;
    ndb_log_info("table '%s' opened read only", name);
  }
  DBUG_RETURN(0);
}

/*
 * Support for OPTIMIZE TABLE
 * reclaims unused space of deleted rows
 * and updates index statistics
 */
int ha_ndbcluster::optimize(THD* thd, HA_CHECK_OPT*)
{
  ulong error, stats_error= 0;
  const uint delay= (uint)THDVAR(thd, optimization_delay);

  error= ndb_optimize_table(thd, delay);
  stats_error= update_stats(thd, 1);
  return (error) ? error : stats_error;
}

int ha_ndbcluster::ndb_optimize_table(THD* thd, uint delay) const
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NDBDICT *dict= ndb->getDictionary();
  int result=0, error= 0;
  uint i;
  NdbDictionary::OptimizeTableHandle th;
  NdbDictionary::OptimizeIndexHandle ih;

  DBUG_ENTER("ndb_optimize_table");
  if ((error= dict->optimizeTable(*m_table, th)))
  {
    DBUG_PRINT("info",
               ("Optimze table %s returned %d", m_tabname, error));
    ERR_RETURN(ndb->getNdbError());
  }
  while((result= th.next()) == 1)
  {
    if (thd->killed)
      DBUG_RETURN(-1);
    ndb_milli_sleep(delay);
  }
  if (result == -1 || th.close() == -1)
  {
    DBUG_PRINT("info",
               ("Optimize table %s did not complete", m_tabname));
    ERR_RETURN(ndb->getNdbError());
  };
  for (i= 0; i < MAX_KEY; i++)
  {
    if (thd->killed)
      DBUG_RETURN(-1);
    if (m_index[i].status == NDB_INDEX_DATA::ACTIVE)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      const NdbDictionary::Index *unique_index= m_index[i].unique_index;
      
      if (index)
      {
        if ((error= dict->optimizeIndex(*index, ih)))
        {
          DBUG_PRINT("info",
                     ("Optimze index %s returned %d", 
                      index->getName(), error));
          ERR_RETURN(ndb->getNdbError());
          
        }
        while((result= ih.next()) == 1)
        {
          if (thd->killed)
            DBUG_RETURN(-1);
          ndb_milli_sleep(delay);
        }
        if (result == -1 || ih.close() == -1)
        {
          DBUG_PRINT("info",
                     ("Optimize index %s did not complete", index->getName()));
          ERR_RETURN(ndb->getNdbError());
        }          
      }
      if (unique_index)
      {
        if ((error= dict->optimizeIndex(*unique_index, ih)))
        {
          DBUG_PRINT("info",
                     ("Optimze unique index %s returned %d", 
                      unique_index->getName(), error));
          ERR_RETURN(ndb->getNdbError());
        } 
        while((result= ih.next()) == 1)
        {
          if (thd->killed)
            DBUG_RETURN(-1);
          ndb_milli_sleep(delay);
        }
        if (result == -1 || ih.close() == -1)
        {
          DBUG_PRINT("info",
                     ("Optimize index %s did not complete", index->getName()));
          ERR_RETURN(ndb->getNdbError());
        }
      }
    }
  }
  DBUG_RETURN(0);
}

int ha_ndbcluster::analyze(THD* thd, HA_CHECK_OPT*)
{
  int err;
  if ((err= update_stats(thd, 1)) != 0)
    return err;
  const bool index_stat_enable= THDVAR(NULL, index_stat_enable) &&
                                THDVAR(thd, index_stat_enable);
  if (index_stat_enable)
  {
    if ((err= analyze_index()) != 0)
    {
      return err;
    }
  }
  return 0;
}

int
ha_ndbcluster::analyze_index()
{
  DBUG_ENTER("ha_ndbcluster::analyze_index");

  uint inx_list[MAX_INDEXES];
  uint inx_count= 0;

  for (uint inx= 0; inx < table_share->keys; inx++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type(inx);  

    if ((idx_type == PRIMARY_KEY_ORDERED_INDEX ||
         idx_type == UNIQUE_ORDERED_INDEX ||
         idx_type == ORDERED_INDEX))
    {
      if (inx_count < MAX_INDEXES)
        inx_list[inx_count++]= inx;
    }
  }

  if (inx_count != 0)
  {
    int err= ndb_index_stat_analyze(inx_list, inx_count);
    if (err != 0)
      DBUG_RETURN(err);
  }
  DBUG_RETURN(0);
}

/*
  Set partition info

  SYNOPSIS
    set_part_info()
    part_info

  RETURN VALUE
    NONE

  DESCRIPTION
    Set up partition info when handler object created
*/

void ha_ndbcluster::set_part_info(partition_info *part_info, bool early)
{
  DBUG_ENTER("ha_ndbcluster::set_part_info");
  m_part_info= part_info;
  if (!early)
  {
    m_use_partition_pruning= false;
    if (!(m_part_info->part_type == partition_type::HASH &&
          m_part_info->list_of_part_fields &&
          !m_part_info->is_sub_partitioned()))
    {
      /*
        PARTITION BY HASH, RANGE and LIST plus all subpartitioning variants
        all use MySQL defined partitioning. PARTITION BY KEY uses NDB native
        partitioning scheme.
      */
      m_use_partition_pruning= true;
      m_user_defined_partitioning= true;
    }
    if (m_part_info->part_type == partition_type::HASH &&
        m_part_info->list_of_part_fields &&
        m_part_info->num_full_part_fields == 0)
    {
      /*
        CREATE TABLE t (....) ENGINE NDB PARTITON BY KEY();
        where no primary key is defined uses a hidden key as partition field
        and this makes it impossible to use any partition pruning. Partition
        pruning requires partitioning based on real fields, also the lack of
        a primary key means that all accesses to tables are based on either
        full table scans or index scans and they can never be pruned those
        scans given that the hidden key is unknown. In write_row, update_row,
        and delete_row the normal hidden key handling will fix things.
      */
      m_use_partition_pruning= false;
    }
    DBUG_PRINT("info", ("m_use_partition_pruning = %d",
                         m_use_partition_pruning));
  }
  DBUG_VOID_RETURN;
}

/**
  Close the table
  - release resources setup by open()
 */

void ha_ndbcluster::local_close(THD *thd, bool release_metadata_flag)
{
  Ndb *ndb;
  DBUG_ENTER("ha_ndbcluster::local_close");
  if (m_key_fields)
  {
    MY_BITMAP **inx_bitmap;
    for (inx_bitmap= m_key_fields;
         (inx_bitmap != NULL) && ((*inx_bitmap) != NULL);
         inx_bitmap++)
      if ((*inx_bitmap)->bitmap != m_pk_bitmap_buf)
        bitmap_free(*inx_bitmap);
    my_free(m_key_fields);
    m_key_fields= NULL;
  }
  if (m_share)
  {
    NDB_SHARE::release_for_handler(m_share, this);
    m_share = nullptr;
  }
  if (release_metadata_flag)
  {
    ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
    release_metadata(thd, ndb);
  }

  //  Release field to column map when table is closed
  delete m_table_map;
  m_table_map = NULL;

  DBUG_VOID_RETURN;
}

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");
  THD *thd= table->in_use;
  local_close(thd, true);
  DBUG_RETURN(0);
}


int ha_ndbcluster::check_ndb_connection(THD* thd) const
{
  Ndb *ndb;
  DBUG_ENTER("check_ndb_connection");
  
  if (!(ndb= check_ndb_in_thd(thd, true)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  if (ndb->setDatabaseName(m_dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  DBUG_RETURN(0);
}


static int ndbcluster_close_connection(handlerton*, THD *thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ENTER("ndbcluster_close_connection");
  if (thd_ndb)
  {
    Thd_ndb::release(thd_ndb);
    thd_set_thd_ndb(thd, NULL);
  }
  DBUG_RETURN(0);
}


/**
  Try to discover one table from NDB. Return the "serialized
  table definition".

  @note The ha_discover/ndbcluster_discover function is called in
        two different contexts:
        1) ha_check_if_table_exists() which want to determine
           if the table exists in the engine. Any returned
           frmblob is simply discarded, it's the existence which is
           interesting. This check is intended to prevent table with
           same name to be created in other engine.
        2) ha_discover() which is intended to take the returned frmblob
           and install it into the DD. The install path is however not
           implemented and it's actually much better if the install code
           is kept inside ndbcluster to allow full control over how a
           table and any related objects are installed.

  @note The caller does not check the return code other
        than zero or not.
*/

static
int ndbcluster_discover(handlerton*, THD* thd,
                        const char *db, const char *name,
                        uchar **frmblob, 
                        size_t *frmlen)
{
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name)); 

  Ndb* ndb;
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  if (ndb->setDatabaseName(db))
  {
    ERR_RETURN(ndb->getNdbError());
  }

  NDBDICT* dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, name);
  const NDBTAB *tab= ndbtab_g.get_table();
  if (!tab)
  {
    // Could not open the table from NDB
    const NdbError err= dict->getNdbError();
    if (err.code == 709 || err.code == 723)
    {
      // Got the normal 'No such table existed'
      DBUG_PRINT("info", ("No such table, error: %u", err.code));
      DBUG_RETURN(1);
    }

    // Got an unexpected error, it's unknown if the table exists or
    // not but unfortunately there is no way to return such information
    // to the caller.
    DBUG_PRINT("error", ("Got unexpected error when trying to open table "
                         "from NDB, error %u", err.code));
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("Found table '%s'", tab->getName()));

  // Magically detect which context this function is called in by
  // checking which kind of metadata locks are held on the table name.
  if (!thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                    db,
                                                    name,
                                                    MDL_EXCLUSIVE))
  {
    // No exclusive MDL lock, this is ha_check_if_table_exists, just
    // return a dummy frmblob to indicate that table exists
    DBUG_PRINT("info", ("return dummy exists for ha_check_if_table_exists()"));
    *frmlen= 37;
    *frmblob= (uchar*)my_malloc(PSI_NOT_INSTRUMENTED,
                                *frmlen,
                                MYF(0));
    DBUG_RETURN(0); // Table exists
  }

  DBUG_PRINT("info", ("table exists, check if it can also be discovered"));

  // 2) Assume that exclusive MDL lock is held on the table at this point
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           db,
                                                           name,
                                                           MDL_EXCLUSIVE));

  // Don't allow discover unless ndb_schema distribution is ready and
  // "schema distribution synchronization" have completed(which currently
  // can be checked using ndb_binlog_is_read_only()).
  // The user who want to use this table simply have to wait
  if (!ndb_schema_dist_is_ready() ||
      ndb_binlog_is_read_only())
  {
    // Can't discover, table is not available yet
    DBUG_RETURN(1);
  }

  {
    Uint32 version;
    void* unpacked_data;
    Uint32 unpacked_len;
    const int get_result =
        tab->getExtraMetadata(version,
                              &unpacked_data, &unpacked_len);
    if (get_result != 0)
    {
      DBUG_PRINT("error", ("Could not get extra metadata, error: %d",
                           get_result));
      DBUG_RETURN(1);
    }

    if (version == 1)
    {
      // Upgrade of tables with version 1 extra metadata should have
      // occurred much earlier during startup. Hitting this path
      // means that something has gone wrong with upgrade of the
      // table. It's likely the same error will occur if we attempt
      // to upgrade again so return an error instead
      my_printf_error(ER_NO,
                      "Table '%s' with extra metadata version: %d "
                      "was not upgraded properly during startup",
                      MYF(0), name, version);

      // The error returned from here is effectively ignored in
      // Open_table_context::recover_from_failed_open(), abort to
      // avoid infinite hang
      ndb_log_error("INTERNAL ERROR: return code is ignored by caller, "
                    "aborting to avoid infinite hang");
      abort();

      DBUG_RETURN(1); // Could not discover table
    }

    // Assign the unpacked data to sdi_t(which is string data type)
    // then release the unpacked data
    dd::sdi_t sdi;
    sdi.assign(static_cast<const char*>(unpacked_data), unpacked_len);
    free(unpacked_data);

    // Install the table into DD, don't use force_overwrite since
    // this function would never have been called unless the table
    // didn't exist
    Ndb_dd_client dd_client(thd);

    if (!dd_client.install_table(db, name, sdi,
                                 tab->getObjectId(), tab->getObjectVersion(),
                                 false))
    {
      // Table existed in NDB but it could not be inserted into DD
      DBUG_ASSERT(false);
      DBUG_RETURN(1);
    }

    // NOTE! It might be possible to not commit the transaction
    // here, assuming the caller would then commit or rollback.
    dd_client.commit();

  }

  // Don't return any sdi in order to indicate that table definitions exists
  // and has been installed into DD
  DBUG_PRINT("info", ("no sdi returned for ha_create_table_from_engine() "
                      "since the table definition is already installed"));
  *frmlen= 0;
  *frmblob= nullptr;

  DBUG_RETURN(0);
}


/**
  Check if a table exists in NDB.
*/
static
int ndbcluster_table_exists_in_engine(handlerton*, THD* thd,
                                      const char *db,
                                      const char *name)
{
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s  name: %s", db, name));

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  NDBDICT* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
  {
    ndb_to_mysql_error(&dict->getNdbError());
    DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
  }
  for (uint i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    if (my_strcasecmp(table_alias_charset, elmt.database, db))
      continue;
    if (my_strcasecmp(table_alias_charset, elmt.name, name))
      continue;
    DBUG_PRINT("info", ("Found table"));
    DBUG_RETURN(HA_ERR_TABLE_EXIST);
  }
  DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
}


/**
  Drop a database and all its tables from NDB
*/

static int ndbcluster_drop_database_impl(
    THD *thd, Ndb_schema_dist_client& schema_dist_client, const char *path)
{
  DBUG_ENTER("ndbcluster_drop_database_impl");
  char dbname[FN_HEADLEN];
  Ndb* ndb;
  NdbDictionary::Dictionary::List list;
  uint i;
  char *tabname;
  List<char> drop_list;
  int ret= 0;
  ha_ndbcluster::set_dbname(path, (char *)&dbname);
  DBUG_PRINT("enter", ("db: %s", dbname));
  
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(-1);
  
  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
  {
    const NdbError err= dict->getNdbError();
    if (err.code == 4008 || err.code == 4012)
    {
      ret= ndb_to_mysql_error(&err);
    }
    DBUG_RETURN(ret);
  }
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));     
    
    // Add only tables that belongs to db
    // Ignore Blob part tables - they are deleted when their table
    // is deleted.
    if (my_strcasecmp(system_charset_info, elmt.database, dbname) ||
        ndb_name_is_blob_prefix(elmt.name) ||
        ndb_fk_util_is_mock_name(elmt.name))
      continue;
    DBUG_PRINT("info", ("%s must be dropped", elmt.name));     
    drop_list.push_back(thd->mem_strdup(elmt.name));
  }
  // Drop any tables belonging to database
  char full_path[FN_REFLEN + 1];
  char *tmp= full_path +
    build_table_filename(full_path, sizeof(full_path) - 1, dbname, "", "", 0);
  if (ndb->setDatabaseName(dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  List_iterator_fast<char> it(drop_list);
  while ((tabname=it++))
  {
    tablename_to_filename(tabname, tmp, (uint)(FN_REFLEN - (tmp - full_path)-1));
    if (drop_table_impl(thd, ndb, schema_dist_client,
                        full_path, dbname, tabname))
    {
      const NdbError err= dict->getNdbError();
      if (err.code != 709 && err.code != 723)
      {
        ret= ndb_to_mysql_error(&err);
      }
    }
  }

  dict->invalidateDbGlobal(dbname);
  DBUG_RETURN(ret);
}


static void ndbcluster_drop_database(handlerton*, char *path)
{
  THD *thd= current_thd;
  DBUG_ENTER("ndbcluster_drop_database");
  DBUG_PRINT("enter", ("path: '%s'", path));

  char db[FN_REFLEN];
  ndb_set_dbname(path, db);
  Ndb_schema_dist_client schema_dist_client(thd);

  if (!schema_dist_client.prepare(db, ""))
  {
    /* Don't allow drop database unless schema distribution is ready */
    DBUG_VOID_RETURN;
  }

  const int res = ndbcluster_drop_database_impl(thd, schema_dist_client, path);
  if (res != 0)
  {
    DBUG_VOID_RETURN;
  }

  // NOTE! While upgrading MySQL Server from version
  // without DD there might be remaining .ndb and .frm files.
  // Such files would prevent DROP DATABASE to drop the actual
  // data directory and should be removed probably remove at this
  // point or in the "schema distribution synch" code.

  if (!schema_dist_client.drop_db(db))
  {
    // NOTE! There is currently no way to report an error from this
    // function, just log an error and proceed
    ndb_log_error("Failed to distribute 'DROP DATABASE %s'", db);
  }
  DBUG_VOID_RETURN;
}


/**
  Check if the given table is a system table which is
  supported to store in NDB

*/
static bool is_supported_system_table(const char *db,
                                      const char *table_name,
                                      bool is_sql_layer_system_table)
{
  if (!is_sql_layer_system_table)
  {
    // No need to check tables which MySQL Server does not
    // consider as system tables
    return false;
  }

  if (Ndb_dist_priv_util::is_distributed_priv_table(db, table_name))
  {
    // Table is supported as distributed system table and should be allowed
    // to be stored in NDB
    return true;
  }

  return false;
}


/* Call back after cluster connect */
static int connect_callback()
{
  mysql_mutex_lock(&ndbcluster_mutex);
  update_status_variables(NULL, &g_ndb_status,
                          g_ndb_cluster_connection);

  mysql_cond_broadcast(&ndbcluster_cond);
  mysql_mutex_unlock(&ndbcluster_mutex);
  return 0;
}

bool ndbcluster_is_connected(uint max_wait_sec)
{
  mysql_mutex_lock(&ndbcluster_mutex);
  bool connected=
    !(!g_ndb_status.cluster_node_id && ndbcluster_hton->slot != ~(uint)0);

  if (!connected)
  {
    /* ndb not connected yet */
    struct timespec abstime;
    set_timespec(&abstime, max_wait_sec);
    mysql_cond_timedwait(&ndbcluster_cond, &ndbcluster_mutex, &abstime);
    connected=
      !(!g_ndb_status.cluster_node_id && ndbcluster_hton->slot != ~(uint)0);
  }
  mysql_mutex_unlock(&ndbcluster_mutex);
  return connected;
}


Ndb_index_stat_thread ndb_index_stat_thread;

extern THD * ndb_create_thd(char * stackptr);

static int ndb_wait_setup_func(ulong max_wait)
{
  DBUG_ENTER("ndb_wait_setup_func");

  mysql_mutex_lock(&ndbcluster_mutex);

  struct timespec abstime;
  set_timespec(&abstime, 1);

  while (max_wait &&
         (!ndb_setup_complete || !ndb_index_stat_thread.is_setup_complete()))
  {
    const int rc= mysql_cond_timedwait(&ndbcluster_cond,
                                       &ndbcluster_mutex,
                                       &abstime);
    if (rc)
    {
      if (rc == ETIMEDOUT)
      {
        DBUG_PRINT("info", ("1s elapsed waiting"));
        max_wait--;
        set_timespec(&abstime, 1); /* 1 second from now*/
      }
      else
      {
        DBUG_PRINT("info", ("Bad mysql_cond_timedwait rc : %u",
                            rc));
        assert(false);
        break;
      }
    }
  }

  mysql_mutex_unlock(&ndbcluster_mutex);

  do
  {
    /**
     * Check if we (might) need a flush privileges
     */
    THD* thd= current_thd;
    bool own_thd= thd == NULL;
    if (own_thd)
    {
      thd= ndb_create_thd((char*)&thd);
      if (thd == 0)
        break;
    }

    if (Ndb_dist_priv_util::priv_tables_are_in_ndb(thd))
    {
      Ndb_local_connection mysqld(thd);
      mysqld.raw_run_query("FLUSH PRIVILEGES", sizeof("FLUSH PRIVILEGES"), 0);
    }

    if (own_thd)
    {
      // TLS variables should not point to thd anymore.
      thd->restore_globals();
      delete thd;
    }
  } while (0);

  DBUG_RETURN((ndb_setup_complete == 1)? 0 : 1);
}


/*
  Function installed as server hook to be called just before
  connections are allowed. Wait for --ndb-wait-setup= seconds
  for ndbcluster connect to NDB and complete setup.
*/

static int
ndb_wait_setup_server_startup(void*)
{
  // Signal components that server is started
  ndb_index_stat_thread.set_server_started();
  ndbcluster_binlog_set_server_started();

  if (ndb_wait_setup_func(opt_ndb_wait_setup) != 0)
  {
    ndb_log_error("Tables not available after %lu seconds. Consider "
                  "increasing --ndb-wait-setup value", opt_ndb_wait_setup);
  }
  return 0; // NOTE! return value ignored by caller
}


/*
  Function installed as server hook to be called before the applier thread
  starts. Wait --ndb-wait-setup= seconds for ndbcluster connect to NDB
  and complete setup.
*/

static int
ndb_wait_setup_replication_applier(void*)
{
  if (ndb_wait_setup_func(opt_ndb_wait_setup) != 0)
  {
    ndb_log_error("NDB Slave: Tables not available after %lu seconds. Consider "
                  "increasing --ndb-wait-setup value", opt_ndb_wait_setup);
  }
  return 0; // NOTE! could return error to fail applier
}

static Ndb_server_hooks ndb_server_hooks;


/* Version in composite numerical format */
static Uint32 ndb_version = NDB_VERSION_D;
static MYSQL_SYSVAR_UINT(
  version,                          /* name */
  ndb_version,                      /* var */
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY | PLUGIN_VAR_NOPERSIST,
  "Compile version for ndbcluster",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0,                                /* default */
  0,                                /* min */
  0,                                /* max */
  0                                 /* block */
);

/* Version in ndb-Y.Y.Y[-status] format */
static char* ndb_version_string = (char*)NDB_NDB_VERSION_STRING;
static MYSQL_SYSVAR_STR(
  version_string,                  /* name */
  ndb_version_string,              /* var */
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY | PLUGIN_VAR_NOPERSIST,
  "Compile version string for ndbcluster",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);

extern int ndb_dictionary_is_mysqld;

Uint32 recv_thread_num_cpus;
static int ndb_recv_thread_cpu_mask_check_str(const char *str);
static int ndb_recv_thread_cpu_mask_update();
handlerton* ndbcluster_hton;


/*
  Handle failure from ndbcluster_init() by printing error
  message(s) and exit the MySQL Server.

  NOTE! This is done to avoid the current undefined behaviour which occurs
  when an error return code from plugin's init() function just disables
  the plugin.
*/

static
void ndbcluster_init_abort(const char* error)
{
  ndb_log_error("%s", error);
  ndb_log_error("Failed to initialize ndbcluster, aborting!");
  ndb_log_error("Use --skip-ndbcluster to start without ndbcluster.");
  exit(1);
}


/*
  Initialize the ndbcluster storage engine part of the "ndbcluster plugin"

  NOTE! As this is the init() function for a storage engine plugin,
  the function is passed a pointer to the handlerton and not
  the "ndbcluster plugin"
*/

static
int ndbcluster_init(void* handlerton_ptr)
{
  DBUG_ENTER("ndbcluster_init");
  DBUG_ASSERT(!ndbcluster_inited);

  handlerton* hton = static_cast<handlerton*>(handlerton_ptr);

  if (unlikely(opt_initialize))
  {
    /* Don't schema-distribute 'mysqld --initialize' of data dictionary */
    ndb_log_info("'--initialize' -> ndbcluster plugin disabled");
    hton->state = SHOW_OPTION_DISABLED;
    DBUG_ASSERT(!ha_storage_engine_is_enabled(hton));
    DBUG_RETURN(0); // Return before init will disable ndbcluster-SE.
  }

  /* Check const alignment */
  assert(DependencyTracker::InvalidTransactionId ==
         Ndb_binlog_extra_row_info::InvalidTransactionId);

  if (global_system_variables.binlog_format == BINLOG_FORMAT_STMT)
  {
    /* Set global to mixed - note that this is not the default,
     * but the current global value
     */
    global_system_variables.binlog_format = BINLOG_FORMAT_MIXED;
    ndb_log_info("Changed global value of binlog_format from STATEMENT to MIXED");

  }

  if (opt_mts_slave_parallel_workers)
  {
    ndb_log_info("Changed global value of --slave-parallel-workers "
                 "from %lu to 0", opt_mts_slave_parallel_workers);
    opt_mts_slave_parallel_workers = 0;
  }

  if (ndb_index_stat_thread.init() ||
      DBUG_EVALUATE_IF("ndbcluster_init_fail1", true, false))
  {
    ndbcluster_init_abort("Failed to initialize NDB Index Stat");
  }

  mysql_mutex_init(PSI_INSTRUMENT_ME, &ndbcluster_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &ndbcluster_cond);
  ndb_dictionary_is_mysqld= 1;
  ndb_setup_complete= 0;
  
  ndbcluster_hton= hton;
  ndbcluster_global_schema_lock_init(hton);

  hton->state=            SHOW_OPTION_YES;
  hton->db_type=          DB_TYPE_NDBCLUSTER;
  hton->close_connection= ndbcluster_close_connection;
  hton->commit=           ndbcluster_commit;
  hton->rollback=         ndbcluster_rollback;
  hton->create=           ndbcluster_create_handler; /* Create a new handler */
  hton->drop_database=    ndbcluster_drop_database;  /* Drop a database */
  hton->panic=            ndbcluster_end;            /* Panic call */
  hton->show_status=      ndbcluster_show_status;    /* Show status */
  hton->get_tablespace=   ndbcluster_get_tablespace; /* Get ts for old ver */
  hton->alter_tablespace=
      ndbcluster_alter_tablespace; /* Tablespace and logfile group */
  hton->get_tablespace_statistics=
      ndbcluster_get_tablespace_statistics; /* Provide data to I_S */
  hton->partition_flags=  ndbcluster_partition_flags; /* Partition flags */
  ndbcluster_binlog_init(hton);
  hton->flags=            HTON_TEMPORARY_NOT_SUPPORTED |
                          HTON_NO_BINLOG_ROW_OPT |
                          HTON_SUPPORTS_FOREIGN_KEYS |
                          HTON_SUPPORTS_ATOMIC_DDL;
  hton->discover=         ndbcluster_discover;
  hton->table_exists_in_engine= ndbcluster_table_exists_in_engine;
  hton->make_pushed_join= ndbcluster_make_pushed_join;
  hton->is_supported_system_table = is_supported_system_table;

  // Install dummy callbacks to avoid writing <tablename>_<id>.SDI files
  // in the data directory, those are just cumbersome having to delete
  // and or rename on the other MySQL servers
  hton->sdi_create = ndb_dummy_ts::sdi_create;
  hton->sdi_drop = ndb_dummy_ts::sdi_drop;
  hton->sdi_get_keys = ndb_dummy_ts::sdi_get_keys;
  hton->sdi_get = ndb_dummy_ts::sdi_get;
  hton->sdi_set = ndb_dummy_ts::sdi_set;
  hton->sdi_delete = ndb_dummy_ts::sdi_delete;

  // Initialize NdbApi
  ndb_init_internal(1);

  if (!ndb_server_hooks.register_server_started(ndb_wait_setup_server_startup))
  {
    ndbcluster_init_abort("Failed to register ndb_wait_setup at server startup");
  }

  if (!ndb_server_hooks.register_applier_start(ndb_wait_setup_replication_applier))
  {
    ndbcluster_init_abort("Failed to register ndb_wait_setup at applier start");
  }

  // Initialize NDB_SHARE factory
  NDB_SHARE::initialize(table_alias_charset);

  /* allocate connection resources and connect to cluster */
  const uint global_opti_node_select= THDVAR(NULL, optimized_node_selection);
  if (ndbcluster_connect(connect_callback, opt_ndb_wait_connected,
                         opt_ndb_cluster_connection_pool,
                         opt_connection_pool_nodeids_str,
                         (global_opti_node_select & 1),
                         opt_ndb_connectstring,
                         opt_ndb_nodeid,
                         opt_ndb_recv_thread_activation_threshold,
                         opt_ndb_data_node_neighbour))
  {
    ndbcluster_init_abort("Failed to initialize connection(s)");
  }

  /* Translate recv thread cpu mask if set */
  if (ndb_recv_thread_cpu_mask_check_str(opt_ndb_recv_thread_cpu_mask) == 0)
  {
    if (recv_thread_num_cpus)
    {
      if (ndb_recv_thread_cpu_mask_update())
      {
        ndbcluster_init_abort("Failed to lock receive thread(s) to CPU(s)");
      }
    }
  }

  /* start the ndb injector thread */
  if (ndbcluster_binlog_start())
  {
    ndbcluster_init_abort("Failed to start NDB Binlog");
  }

  // Create index statistics thread
  if (ndb_index_stat_thread.start() ||
      DBUG_EVALUATE_IF("ndbcluster_init_fail2", true, false))
  {
    ndbcluster_init_abort("Failed to start NDB Index Stat");
  }

  memset(&g_slave_api_client_stats, 0, sizeof(g_slave_api_client_stats));

  ndbcluster_inited= 1;

  DBUG_RETURN(0); // OK
}


static int ndbcluster_end(handlerton *hton, ha_panic_function)
{
  DBUG_ENTER("ndbcluster_end");

  if (!ndbcluster_inited)
    DBUG_RETURN(0);
  ndbcluster_inited= 0;

  /* Stop threads started by ndbcluster_init() */
  ndb_index_stat_thread.stop();
  ndbcluster_binlog_end();

  // Unregister all server hooks
  ndb_server_hooks.unregister_all();

  NDB_SHARE::deinitialize();

  ndb_index_stat_end();
  ndbcluster_disconnect();

  ndbcluster_global_schema_lock_deinit(hton);
  ndb_index_stat_thread.deinit();

  mysql_mutex_destroy(&ndbcluster_mutex);
  mysql_cond_destroy(&ndbcluster_cond);

  // Cleanup NdbApi
  ndb_end_internal(1);

  DBUG_RETURN(0);
}


/*
  Deintialize the ndbcluster storage engine part of the "ndbcluster plugin"

  NOTE! As this is the deinit() function for a storage engine plugin,
  the function is passed a pointer to the handlerton and not
  the "ndbcluster plugin"
*/

static int
ndbcluster_deinit(void*)
{
  return 0;
}


void ha_ndbcluster::print_error(int error, myf errflag)
{
  DBUG_ENTER("ha_ndbcluster::print_error");
  DBUG_PRINT("enter", ("error: %d", error));

  if (error == HA_ERR_NO_PARTITION_FOUND)
  {
    m_part_info->print_no_partition_found(current_thd, table);
    DBUG_VOID_RETURN;
  }

  if (error == HA_ERR_NO_CONNECTION)
  {
    handler::print_error(4009, errflag);
    DBUG_VOID_RETURN;
  }

  if (error == HA_ERR_FOUND_DUPP_KEY &&
      (table == NULL || table->file == NULL))
  {
    /*
      This is a sideffect of 'ndbcluster_print_error' (called from
      'ndbcluster_commit' and 'ndbcluster_rollback') which realises
      that it "knows nothing" and creates a brand new ha_ndbcluster
      in order to be able to call the print_error() function.
      Unfortunately the new ha_ndbcluster hasn't been open()ed
      and thus table pointer etc. is not set. Since handler::print_error()
      will use that pointer without checking for NULL(it naturally
      assumes an error can only be returned when the handler is open)
      this would crash the mysqld unless it's handled here.
    */
    my_error(ER_DUP_KEY, errflag, table_share->table_name.str, error);
    DBUG_VOID_RETURN;
  }

  if (error == ER_CANT_DROP_FIELD_OR_KEY)
  {
    /*
      Called on drop unknown FK by server when algorithm=copy or
      by handler when algorithm=inplace.  In both cases the error
      was already printed in ha_ndb_ddl_fk.cc.
    */
    THD* thd= NULL;
    if (table != NULL &&
        (thd= table->in_use) != NULL &&
        thd->lex != NULL &&
        thd_sql_command(thd) == SQLCOM_ALTER_TABLE)
    {
      DBUG_VOID_RETURN;
    }
    DBUG_ASSERT(false);
  }

  handler::print_error(error, errflag);
  DBUG_VOID_RETURN;
}


/**
  Set a given location from full pathname to database name.
*/

void ha_ndbcluster::set_dbname(const char *path_name, char *dbname)
{
  ndb_set_dbname(path_name, dbname);
}

/**
  Set m_dbname from full pathname to table file.
*/

void ha_ndbcluster::set_dbname(const char *path_name)
{
  ndb_set_dbname(path_name, m_dbname);
}

/**
  Set a given location from full pathname to table file.
*/

void
ha_ndbcluster::set_tabname(const char *path_name, char * tabname)
{
  ndb_set_tabname(path_name, tabname);
}

/**
  Set m_tabname from full pathname to table file.
*/

void ha_ndbcluster::set_tabname(const char *path_name)
{
  ndb_set_tabname(path_name, m_tabname);
}


/*
  If there are no stored stats, should we do a tree-dive on all db
  nodes.  The result is fairly good but does mean a round-trip.
 */
static const bool g_ndb_records_in_range_tree_dive= false;

/* Determine roughly how many records are in the range specified */
ha_rows 
ha_ndbcluster::records_in_range(uint inx, key_range *min_key,
                                key_range *max_key)
{
  KEY *key_info= table->key_info + inx;
  uint key_length= key_info->key_length;
  NDB_INDEX_TYPE idx_type= get_index_type(inx);  

  DBUG_ENTER("records_in_range");
  // Prevent partial read of hash indexes by returning HA_POS_ERROR
  if ((idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX) &&
      ((min_key && min_key->length < key_length) ||
       (max_key && max_key->length < key_length)))
    DBUG_RETURN(HA_POS_ERROR);
  
  // Read from hash index with full key
  // This is a "const" table which returns only one record!      
  if ((idx_type != ORDERED_INDEX) &&
      ((min_key && min_key->length == key_length) &&
       (max_key && max_key->length == key_length) &&
       (min_key->key==max_key->key ||
        memcmp(min_key->key, max_key->key, key_length)==0)))
    DBUG_RETURN(1);
  
  // XXX why this if
  if ((idx_type == PRIMARY_KEY_ORDERED_INDEX ||
       idx_type == UNIQUE_ORDERED_INDEX ||
       idx_type == ORDERED_INDEX))
  {
    THD *thd= current_thd;
    const bool index_stat_enable= THDVAR(NULL, index_stat_enable) &&
                                  THDVAR(thd, index_stat_enable);

    if (index_stat_enable)
    {
      ha_rows rows= HA_POS_ERROR;
      int err= ndb_index_stat_get_rir(inx, min_key, max_key, &rows);
      if (err == 0)
      {
        /**
         * optmizer thinks that all values < 2 are exact...but
         * but we don't provide exact statistics
         */
        if (rows < 2)
          rows = 2;
        DBUG_RETURN(rows);
      }
      if (err != 0 &&
          /* no stats is not unexpected error */
          err != NdbIndexStat::NoIndexStats &&
          /* warning was printed at first error */
          err != NdbIndexStat::MyHasError &&
          /* stats thread aborted request */
          err != NdbIndexStat::MyAbortReq)
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANT_GET_STAT, /* pun? */
                            "index stats (RIR) for key %s:"
                            " unexpected error %d",
                            key_info->name, err);
      }
      /*fall through*/
    }

    if (g_ndb_records_in_range_tree_dive)
    {
      NDB_INDEX_DATA& d=m_index[inx];
      const NDBINDEX* index= d.index;
      Ndb *ndb= get_ndb(thd);
      NdbTransaction* active_trans= m_thd_ndb ? m_thd_ndb->trans : 0;
      NdbTransaction* trans=NULL;
      int res=0;
      Uint64 rows;

      do
      {
        if ((trans=active_trans) == NULL || 
            trans->commitStatus() != NdbTransaction::Started)
        {
          DBUG_PRINT("info", ("no active trans"));
          if (! (trans=ndb->startTransaction()))
            ERR_BREAK(ndb->getNdbError(), res);
        }
        
        /* Create an IndexBound struct for the keys */
        NdbIndexScanOperation::IndexBound ib;
        compute_index_bounds(ib,
                             key_info,
                             min_key, 
                             max_key,
                             0);

        ib.range_no= 0;

        NdbIndexStat is;
        if (is.records_in_range(index, 
                                trans, 
                                d.ndb_record_key,
                                m_ndb_record,
                                &ib, 
                                0, 
                                &rows, 
                                0) == -1)
          ERR_BREAK(is.getNdbError(), res);
      } while (0);

      if (trans != active_trans && rows == 0)
        rows = 1;
      if (trans != active_trans && trans != NULL)
        ndb->closeTransaction(trans);
      if (res == 0)
        DBUG_RETURN(rows);
      /*fall through*/
    }
  }

  /* Use simple heuristics to estimate fraction
     of 'stats.record' returned from range.
  */
  do
  {
    if (stats.records == ~(ha_rows)0 || stats.records == 0)
    {
      /* Refresh statistics, only read from datanodes if 'use_exact_count' */
      THD *thd= current_thd;
      if (update_stats(thd, THDVAR(thd, use_exact_count)))
        break;
    }

    Uint64 rows;
    Uint64 table_rows= stats.records;
    size_t eq_bound_len= 0;
    size_t min_key_length= (min_key) ? min_key->length : 0;
    size_t max_key_length= (max_key) ? max_key->length : 0; 

    // Might have an closed/open range bound:
    // Low range open
    if (!min_key_length)
    {
      rows= (!max_key_length) 
           ? table_rows             // No range was specified
           : table_rows/10;         // -oo .. <high range> -> 10% selectivity
    }
    // High range open
    else if (!max_key_length)
    {
      rows= table_rows/10;          // <low range>..oo -> 10% selectivity
    }
    else
    {
      size_t bounds_len= MIN(min_key_length,max_key_length);
      uint eq_bound_len= 0;
      uint eq_bound_offs= 0;

      KEY_PART_INFO* key_part= key_info->key_part;
      KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
      for (; key_part != end; key_part++) 
      {
        uint part_length= key_part->store_length;
        if (eq_bound_offs+part_length > bounds_len ||
            memcmp(&min_key->key[eq_bound_offs],
                   &max_key->key[eq_bound_offs],
                   part_length))
        {
          break;
        }
        eq_bound_len+= key_part->length;
        eq_bound_offs+= part_length;
      }

      if (!eq_bound_len)
      {
        rows= table_rows/20;        // <low range>..<high range> -> 5% 
      }
      else
      {
        // Has an equality range on a leading part of 'key_length':
        // - Assume reduced selectivity for non-unique indexes
        //   by decreasing 'eq_fraction' by 20%
        // - Assume equal selectivity for all eq_parts in key.

        double eq_fraction = (double)(eq_bound_len) / key_length;
        if (idx_type == ORDERED_INDEX) // Non-unique index -> less selectivity
          eq_fraction/= 1.20;
        if (eq_fraction >= 1.0)        // Exact match -> 1 row
          DBUG_RETURN(1);

        rows = (Uint64)((double)table_rows / pow((double)table_rows, eq_fraction));
        if (rows > (table_rows/50))    // EQ-range: Max 2% of rows
          rows= (table_rows/50);

        if (min_key_length > eq_bound_offs)
          rows/= 2;
        if (max_key_length > eq_bound_offs)
          rows/= 2;
      }
    }

    // Make sure that EQ is preferred even if row-count is low
    if (eq_bound_len && rows < 2)      // At least 2 rows as not exact
      rows= 2;
    else if (rows < 3)
      rows= 3;
    DBUG_RETURN(MIN(rows,table_rows));
  } while (0);

  DBUG_RETURN(10); /* Poor guess when you don't know anything */
}

ulonglong ha_ndbcluster::table_flags(void) const
{
  THD *thd= current_thd;
  ulonglong f=
    HA_NULL_IN_KEY |
    HA_AUTO_PART_KEY |
    HA_NO_PREFIX_CHAR_KEYS |
    HA_CAN_GEOMETRY |
    HA_CAN_BIT_FIELD |
    HA_PRIMARY_KEY_REQUIRED_FOR_POSITION |
    HA_PARTIAL_COLUMN_READ |
    HA_HAS_OWN_BINLOGGING |
    HA_BINLOG_ROW_CAPABLE |
    HA_HAS_RECORDS |
    HA_READ_BEFORE_WRITE_REMOVAL |
    HA_GENERATED_COLUMNS |
    0;

  /*
    To allow for logging of ndb tables during stmt based logging;
    flag cabablity, but also turn off flag for OWN_BINLOGGING
  */
  if (thd->variables.binlog_format == BINLOG_FORMAT_STMT)
    f= (f | HA_BINLOG_STMT_CAPABLE) & ~HA_HAS_OWN_BINLOGGING;

   /*
     Allow MySQL Server to decide that STATEMENT logging should be used
     for the distributed privilege tables. NOTE! This is a workaround
     for generic problem with forcing STATEMENT logging see BUG16482501.
   */
  if (Ndb_dist_priv_util::is_distributed_priv_table(m_dbname,m_tabname))
    f= (f | HA_BINLOG_STMT_CAPABLE) & ~HA_HAS_OWN_BINLOGGING;

  /*
     Allow MySQL Server to decide that STATEMENT logging should be used
     during TRUNCATE TABLE, thus writing the truncate query to the binlog
     in STATEMENT format. Basically this is shortcutting the logic
     in THD::decide_logging_format() to not handle the truncated
     table as a "no_replicate" table.
  */
  if (thd_sql_command(thd) == SQLCOM_TRUNCATE)
    f= (f | HA_BINLOG_STMT_CAPABLE) & ~HA_HAS_OWN_BINLOGGING;

  /**
   * To maximize join pushability we want const-table 
   * optimization blocked if 'ndb_join_pushdown= on'
   */
  if (THDVAR(thd, join_pushdown))
    f= f | HA_BLOCK_CONST_TABLE;

  return f;
}

const char * ha_ndbcluster::table_type() const 
{
  return("NDBCLUSTER");
}
uint ha_ndbcluster::max_supported_record_length() const
{ 
  return NDB_MAX_TUPLE_SIZE;
}
uint ha_ndbcluster::max_supported_keys() const
{
  return MAX_KEY;
}
uint ha_ndbcluster::max_supported_key_parts() const 
{
  return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY;
}
uint ha_ndbcluster::max_supported_key_length() const
{
  return NDB_MAX_KEY_SIZE;
}
uint ha_ndbcluster::max_supported_key_part_length() const
{
  return NDB_MAX_KEY_SIZE;
}
bool ha_ndbcluster::low_byte_first() const
{ 
#ifdef WORDS_BIGENDIAN
  return false;
#else
  return true;
#endif
}


struct ndb_table_statistics_row {
  Uint64 rows;
  Uint64 commits;
  Uint32 size;
  Uint64 fixed_mem;
  Uint64 var_mem;
};

int ha_ndbcluster::update_stats(THD *thd,
                                bool do_read_stat,
                                uint part_id)
{
  struct Ndb_statistics stat;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ENTER("ha_ndbcluster::update_stats");
  do
  {
    if (m_share && !do_read_stat)
    {
      mysql_mutex_lock(&m_share->mutex);
      stat= m_share->stat;
      mysql_mutex_unlock(&m_share->mutex);

      DBUG_ASSERT(stat.row_count != ~(ha_rows)0); // should never be invalid

      /* Accept shared cached statistics if row_count is valid. */
      if (stat.row_count != ~(ha_rows)0)
        break;
    }

    /* Request statistics from datanodes */
    Ndb *ndb= thd_ndb->ndb;
    if (ndb->setDatabaseName(m_dbname))
    {
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
    if (int err= ndb_get_table_statistics(thd,
                                          this,
                                          ndb,
                                          m_table,
                                          m_ndb_record,
                                          &stat,
                                          part_id))
    {
      DBUG_RETURN(err);
    }

    /* Update shared statistics with fresh data */
    if (m_share)
    {
      mysql_mutex_lock(&m_share->mutex);
      m_share->stat= stat;
      mysql_mutex_unlock(&m_share->mutex);
    }
    break;
  }
  while(0);

  int no_uncommitted_rows_count= 0;
  if (m_table_info && !thd_ndb->m_error)
  {
    m_table_info->records= stat.row_count;
    m_table_info->last_count= thd_ndb->count;
    no_uncommitted_rows_count= m_table_info->no_uncommitted_rows_count;
  }
  stats.mean_rec_length= stat.row_size;
  stats.data_file_length= stat.fragment_memory;
  stats.records= stat.row_count + no_uncommitted_rows_count;
  stats.max_data_file_length= stat.fragment_extent_space;
  stats.delete_length= stat.fragment_extent_free_space;

  DBUG_PRINT("exit", ("stats.records: %d  "
                      "stat->row_count: %d  "
                      "no_uncommitted_rows_count: %d"
                      "stat->fragment_extent_space: %u  "
                      "stat->fragment_extent_free_space: %u",
                      (int)stats.records,
                      (int)stat.row_count,
                      (int)no_uncommitted_rows_count,
                      (uint)stat.fragment_extent_space,
                      (uint)stat.fragment_extent_free_space));
  DBUG_RETURN(0);
}

/**
  Update 'row_count' in shared table statistcs if any rows where
  inserted/deleted by the local transaction related to specified
 'local_stat'.
  Should be called when transaction has succesfully commited its changes.
*/
static
void modify_shared_stats(NDB_SHARE *share,
                         Ndb_local_table_statistics *local_stat)
{
  if (local_stat->no_uncommitted_rows_count)
  {
    mysql_mutex_lock(&share->mutex);
    DBUG_ASSERT(share->stat.row_count != ~(ha_rows)0);// should never be invalid
    if (share->stat.row_count != ~(ha_rows)0)
    {
      DBUG_PRINT("info", ("Update row_count for %s, row_count: %lu, with:%d",
                          share->table_name, (ulong) share->stat.row_count,
                          local_stat->no_uncommitted_rows_count));
      share->stat.row_count=
        ((Int64)share->stat.row_count+local_stat->no_uncommitted_rows_count > 0)
         ? share->stat.row_count+local_stat->no_uncommitted_rows_count
         : 0;
    }
    mysql_mutex_unlock(&share->mutex);
    local_stat->no_uncommitted_rows_count= 0;
  }
}

/* If part_id contains a legal partition id, ndbstat returns the
   partition-statistics pertaining to that partition only.
   Otherwise, it returns the table-statistics,
   which is an aggregate over all partitions of that table.
 */
static 
int
ndb_get_table_statistics(THD *thd,
                         ha_ndbcluster* file,
                         Ndb* ndb,
                         const NdbDictionary::Table* tab,
                         const NdbRecord *record,
                         struct Ndb_statistics * ndbstat,
                         uint part_id)
{
  Thd_ndb *thd_ndb= get_thd_ndb(current_thd);
  NdbTransaction* pTrans;
  NdbError error;
  int retries= 100;
  int reterr= 0;
  int retry_sleep= 30; /* 30 milliseconds */
  const char *dummyRowPtr;
  NdbOperation::GetValueSpec extraGets[7];
  Uint64 rows, fixed_mem, var_mem, ext_space, free_ext_space;
  Uint32 size, fragid;

  DBUG_ENTER("ndb_get_table_statistics");

  DBUG_ASSERT(record != 0);
  
  /* We use the passed in NdbRecord just to get access to the
     table, we mask out any/all columns it may have and add
     our reads as extraGets.  This is necessary as they are
     all pseudo-columns
  */
  extraGets[0].column= NdbDictionary::Column::ROW_COUNT;
  extraGets[0].appStorage= &rows;
  extraGets[1].column= NdbDictionary::Column::ROW_SIZE;
  extraGets[1].appStorage= &size;
  extraGets[2].column= NdbDictionary::Column::FRAGMENT_FIXED_MEMORY;
  extraGets[2].appStorage= &fixed_mem;
  extraGets[3].column= NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY;
  extraGets[3].appStorage= &var_mem;
  extraGets[4].column= NdbDictionary::Column::FRAGMENT_EXTENT_SPACE;
  extraGets[4].appStorage= &ext_space;
  extraGets[5].column= NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE;
  extraGets[5].appStorage= &free_ext_space;
  extraGets[6].column= NdbDictionary::Column::FRAGMENT;
  extraGets[6].appStorage= &fragid;

  const Uint32 codeWords= 1;
  Uint32 codeSpace[ codeWords ];
  NdbInterpretedCode code(NULL, // Table is irrelevant
                          &codeSpace[0],
                          codeWords);
  if ((code.interpret_exit_last_row() != 0) ||
      (code.finalise() != 0))
  {
    reterr= code.getNdbError().code;
    DBUG_PRINT("exit", ("failed, reterr: %u, NdbError %u(%s)", reterr,
                        error.code, error.message));
    DBUG_RETURN(reterr);
  }

  do
  {
    Uint32 count= 0;
    Uint64 sum_rows= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    Uint64 sum_ext_space= 0;
    Uint64 sum_free_ext_space= 0;
    NdbScanOperation*pOp;
    int check;

    /**
     * TODO WL#9019, pass table to startTransaction to allow fully
     * replicated table to select data_node_neighbour
     */
    if ((pTrans= ndb->startTransaction(tab)) == NULL)
    {
      error= ndb->getNdbError();
      goto retry;
    }

    NdbScanOperation::ScanOptions options;
    options.optionsPresent= NdbScanOperation::ScanOptions::SO_BATCH |
                            NdbScanOperation::ScanOptions::SO_GETVALUE |
                            NdbScanOperation::ScanOptions::SO_INTERPRETED;
    /* Set batch_size=1, as we need only one row per fragment. */
    options.batch= 1;
    options.extraGetValues= &extraGets[0];
    options.numExtraGetValues= sizeof(extraGets)/sizeof(extraGets[0]); 
    options.interpretedCode= &code;

    if ((pOp= pTrans->scanTable(record, NdbOperation::LM_CommittedRead,
                                empty_mask,
                                &options,
                                sizeof(NdbScanOperation::ScanOptions))) == NULL)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    thd_ndb->m_scan_count++;
    thd_ndb->m_pruned_scan_count += (pOp->getPruned()? 1 : 0);
    
    thd_ndb->m_execute_count++;
    DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
    if (pTrans->execute(NdbTransaction::NoCommit,
                        NdbOperation::AbortOnError,
                        true) == -1)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    while ((check= pOp->nextResult(&dummyRowPtr, true, true)) == 0)
    {
      DBUG_PRINT("info", ("nextResult rows: %llu, "
                          "fixed_mem_size %llu var_mem_size %llu "
                          "fragmentid %u extent_space %llu free_extent_space %llu",
                          rows, fixed_mem, var_mem, fragid,
                          ext_space, free_ext_space));

      if ((part_id != ~(uint)0) && fragid != part_id)
      {
        continue;
      }

      sum_rows+= rows;
      if (sum_row_size < size)
        sum_row_size= size;
      sum_mem+= fixed_mem + var_mem;
      count++;
      sum_ext_space += ext_space;
      sum_free_ext_space += free_ext_space;

      if ((part_id != ~(uint)0) && fragid == part_id)
      {
        break;
      }
    }
    
    if (check == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }

    pOp->close(true);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->row_size= (ulong)sum_row_size;
    ndbstat->fragment_memory= sum_mem;
    ndbstat->fragment_extent_space= sum_ext_space;
    ndbstat->fragment_extent_free_space= sum_free_ext_space;

    DBUG_PRINT("exit", ("records: %llu row_size: %llu "
                        "mem: %llu allocated: %llu free: %llu count: %u",
                        sum_rows, sum_row_size, sum_mem, sum_ext_space,
                        sum_free_ext_space, count));

    DBUG_RETURN(0);
retry:
    if (file && pTrans)
    {
      reterr= file->ndb_err(pTrans);
    }
    else
    {
      const NdbError& tmp= error;
      ERR_PRINT(tmp);
      reterr= ndb_to_mysql_error(&tmp);
    }

    if (pTrans)
    {
      ndb->closeTransaction(pTrans);
      pTrans= NULL;
    }
    if (error.status == NdbError::TemporaryError &&
        retries-- && !thd->killed)
    {
      ndb_retry_sleep(retry_sleep);
      continue;
    }
    break;
  } while(1);
  DBUG_PRINT("exit", ("failed, reterr: %u, NdbError %u(%s)", reterr,
                      error.code, error.message));
  DBUG_RETURN(reterr);
}


void ha_ndbcluster::check_read_before_write_removal()
{
  DBUG_ENTER("check_read_before_write_removal");

  /* Must have determined that rbwr is possible */
  assert(m_read_before_write_removal_possible);
  m_read_before_write_removal_used= true;

  /* Can't use on table with hidden primary key */
  assert(table_share->primary_key != MAX_KEY);

  /* Index must be unique */
  DBUG_PRINT("info", ("using index %d", active_index));
  const KEY *key= table->key_info + active_index;
  assert((key->flags & HA_NOSAME)); NDB_IGNORE_VALUE(key);

  DBUG_VOID_RETURN;
}


/****************************************************************************
 * MRR interface implementation
 ***************************************************************************/

/**
   We will not attempt to deal with more than this many ranges in a single
   MRR execute().
*/
#define MRR_MAX_RANGES 128

/*
  Types of ranges during multi_range_read.

  Code assumes that X < enum_ordered_range is a valid check for range converted
  to key operation.
*/
enum multi_range_types
{
  enum_unique_range,            /// Range converted to key operation
  enum_empty_unique_range,      /// No data found (in key operation)
  enum_ordered_range,           /// Normal ordered index scan range
  enum_skip_range               /// Empty range (eg. partition pruning)
};

/**
  Usage of the MRR buffer is as follows:

  First, N char * values, each being the custom value obtained from
  RANGE_SEQ_IF::next() that needs to be returned from multi_range_read_next().
  N is usually == total number of ranges, but never more than MRR_MAX_RANGES
  (the MRR is split across several execute()s if necessary). N may be lower
  than actual number of ranges in a single execute() in case of split for
  other reasons.

  This is followed by N variable-sized entries, each

   - 1 byte of multi_range_types for this range.

   - (Only) for ranges converted to key operations (enum_unique_range and
     enum_empty_unique_range), this is followed by table_share->reclength
     bytes of row data.
*/

static inline
ulong multi_range_buffer_size(const HANDLER_BUFFER* buffer)
{
  const size_t buf_size = buffer->buffer_end - buffer->buffer;
  DBUG_ASSERT(buf_size < ULONG_MAX);
  return (ulong)buf_size;
}

/* Return the needed size of the fixed array at start of HANDLER_BUFFER. */
static ulong
multi_range_fixed_size(int num_ranges)
{
  if (num_ranges > MRR_MAX_RANGES)
    num_ranges= MRR_MAX_RANGES;
  return num_ranges * sizeof(char *);
}

/* Return max number of ranges so that fixed part will still fit in buffer. */
static int
multi_range_max_ranges(int num_ranges, ulong bufsize)
{
  if (num_ranges > MRR_MAX_RANGES)
    num_ranges= MRR_MAX_RANGES;
  if (num_ranges * sizeof(char *) > bufsize)
    num_ranges= bufsize / sizeof(char *);
  return num_ranges;
}

/* Return the size in HANDLER_BUFFER of a variable-sized entry. */
static ulong
multi_range_entry_size(bool use_keyop, ulong reclength)
{
  /* Space for type byte. */
  ulong len= 1;
  if (use_keyop)
    len+= reclength;
  return len;
}

/*
  Return the maximum size of a variable-sized entry in HANDLER_BUFFER.

  Actual size may depend on key values (whether the actual value can be
  converted to a hash key operation or needs to be done as an ordered index
  scan).
*/
static ulong
multi_range_max_entry(NDB_INDEX_TYPE keytype, ulong reclength)
{
  return multi_range_entry_size(keytype != ORDERED_INDEX, reclength);
}

static uchar &
multi_range_entry_type(uchar *p)
{
  return *p;
}

/* Find the start of the next entry in HANDLER_BUFFER. */
static uchar *
multi_range_next_entry(uchar *p, ulong reclength)
{
  bool use_keyop= multi_range_entry_type(p) < enum_ordered_range;
  return p + multi_range_entry_size(use_keyop, reclength);
}

/* Get pointer to row data (for range converted to key operation). */
static uchar *
multi_range_row(uchar *p)
{
  DBUG_ASSERT(multi_range_entry_type(p) == enum_unique_range);
  return p + 1;
}

/* Get and put upper layer custom char *, use memcpy() for unaligned access. */
static char *
multi_range_get_custom(HANDLER_BUFFER *buffer, int range_no)
{
  DBUG_ASSERT(range_no < MRR_MAX_RANGES);
  char* res;
  memcpy(&res, buffer->buffer + range_no*sizeof(char*), sizeof(char*));
  return res;
}

static void
multi_range_put_custom(HANDLER_BUFFER *buffer, int range_no, char *custom)
{
  DBUG_ASSERT(range_no < MRR_MAX_RANGES);
  // memcpy() required for unaligned access.
  memcpy(buffer->buffer + range_no*sizeof(char*), &custom, sizeof(char*));
}

/*
  This is used to check if an ordered index scan is needed for a range in
  a multi range read.
  If a scan is not needed, we use a faster primary/unique key operation
  instead.
*/
static bool
read_multi_needs_scan(NDB_INDEX_TYPE cur_index_type, const KEY *key_info,
                      const KEY_MULTI_RANGE *r, bool is_pushed)
{
  if (cur_index_type == ORDERED_INDEX || is_pushed)
    return true;
  if (cur_index_type == PRIMARY_KEY_INDEX ||
      cur_index_type == UNIQUE_INDEX)
    return false;
  DBUG_ASSERT(cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
              cur_index_type == UNIQUE_ORDERED_INDEX);
  if (r->start_key.length != key_info->key_length ||
      r->start_key.flag != HA_READ_KEY_EXACT)
    return true;                                // Not exact match, need scan
  if (cur_index_type == UNIQUE_ORDERED_INDEX &&
      check_null_in_key(key_info, r->start_key.key,r->start_key.length))
    return true;                                // Can't use for NULL values
  return false;
}

/*
  Get cost and other information about MRR scan over a known list of ranges

  SYNOPSIS
    See handler::multi_range_read_info_const.

  DESCRIPTION
    The implementation is copied from handler::multi_range_read_info_const.
    The only difference is that NDB-MRR cannot handle blob columns or keys
    with NULLs for unique indexes. We disable MRR for those cases.

  NOTES
    See NOTES for handler::multi_range_read_info_const().
*/

ha_rows 
ha_ndbcluster::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                           void *seq_init_param, 
                                           uint n_ranges, uint *bufsz,
                                           uint *flags, Cost_estimate *cost)
{
  ha_rows rows;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;

  DBUG_ENTER("ha_ndbcluster::multi_range_read_info_const");

  /* Get cost/flags/mem_usage of default MRR implementation */
  rows= handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                             n_ranges, &def_bufsz, 
                                             &def_flags, cost);
  if (unlikely(rows == HA_POS_ERROR))
  {
    DBUG_RETURN(rows);
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is
    an order to use the default MRR implementation.
    Otherwise, make a choice based on requested *flags, handler
    capabilities, cost and mrr* flags of @@optimizer_switch.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, n_ranges, rows, bufsz, flags, cost))
  {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
    DBUG_ASSERT(*flags & HA_MRR_USE_DEFAULT_IMPL);
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("NDB-MRR implementation choosen"));
    DBUG_ASSERT(!(*flags & HA_MRR_USE_DEFAULT_IMPL));
  }
  DBUG_RETURN(rows);
}


/*
  Get cost and other information about MRR scan over some sequence of ranges

  SYNOPSIS
    See handler::multi_range_read_info.
*/

ha_rows 
ha_ndbcluster::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                     uint *bufsz, uint *flags,
                                     Cost_estimate *cost)
{
  ha_rows res;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;

  DBUG_ENTER("ha_ndbcluster::multi_range_read_info");

  /* Get cost/flags/mem_usage of default MRR implementation */
  res= handler::multi_range_read_info(keyno, n_ranges, n_rows,
                                      &def_bufsz, &def_flags,
                                      cost);
  if (unlikely(res == HA_POS_ERROR))
  {
    /* Default implementation can't perform MRR scan => we can't either */
    DBUG_RETURN(res);
  }
  DBUG_ASSERT(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) || 
      choose_mrr_impl(keyno, n_ranges, n_rows, bufsz, flags, cost))
  {
    /* Default implementation is choosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
    DBUG_ASSERT(*flags & HA_MRR_USE_DEFAULT_IMPL);
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("NDB-MRR implementation choosen"));
    DBUG_ASSERT(!(*flags & HA_MRR_USE_DEFAULT_IMPL));
  }
  DBUG_RETURN(res);
}

/**
  Internals: Choose between Default MRR implementation and 
                    native ha_ndbcluster MRR

  Make the choice between using Default MRR implementation and ha_ndbcluster-MRR.
  This function contains common functionality factored out of multi_range_read_info()
  and multi_range_read_info_const(). The function assumes that the default MRR
  implementation's applicability requirements are satisfied.

  @param keyno       Index number
  @param n_ranges    Number of ranges/keys (i.e. intervals) in the range sequence.
  @param n_rows      E(full rows to be retrieved)
  @param bufsz  OUT  If DS-MRR is choosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is choosen, flags of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is choosen, cost of DS-MRR scan
                     else the value is not modified

  @retval true   Default MRR implementation should be used
  @retval false  NDB-MRR implementation should be used
*/

bool ha_ndbcluster::choose_mrr_impl(uint keyno, uint n_ranges, ha_rows n_rows,
                                    uint *bufsz, uint *flags, Cost_estimate*)
{
  THD *thd= current_thd;
  NDB_INDEX_TYPE key_type= get_index_type(keyno);

  get_read_set(true, keyno);

  /* Disable MRR on blob read and on NULL lookup in unique index. */
  if (!thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR) ||
      uses_blob_value(table->read_set) ||
      ( key_type == UNIQUE_INDEX &&
        has_null_in_unique_index(keyno) &&
        !(*flags & HA_MRR_NO_NULL_ENDPOINTS)))
  {
    /* Use the default implementation, don't modify args: See comments  */
    return true;
  }

  /**
   * Calculate *bufsz, fallback to default MRR if we can't allocate
   * suffient buffer space for NDB-MRR
   */
  {
    uint save_bufsize= *bufsz;
    ulong reclength= table_share->reclength;
    uint entry_size= multi_range_max_entry(key_type, reclength);
    uint min_total_size= entry_size + multi_range_fixed_size(1);
    DBUG_PRINT("info", ("MRR bufsize suggested=%u want=%u limit=%d",
                        save_bufsize, (uint)(n_rows + 1) * entry_size,
                        (*flags & HA_MRR_LIMITS) != 0));
    if (save_bufsize < min_total_size)
    {
      if (*flags & HA_MRR_LIMITS)
      {
        /* Too small buffer limit for native NDB-MRR. */
        return true;
      }
      *bufsz= min_total_size;
    }
    else
    {
      uint max_ranges= (n_ranges > 0) ? n_ranges : MRR_MAX_RANGES;
      *bufsz= std::min(save_bufsize,
                       (uint)(n_rows * entry_size +
                              multi_range_fixed_size(max_ranges)));
    }
    DBUG_PRINT("info", ("MRR bufsize set to %u", *bufsz));
  }

  /**
   * Cost based MRR optimization is known to be incorrect.
   * Disabled -> always use NDB-MRR whenever possible
   */
  *flags&= ~HA_MRR_USE_DEFAULT_IMPL;
  *flags|= HA_MRR_SUPPORT_SORTED;

  return false;
}


int ha_ndbcluster::multi_range_read_init(RANGE_SEQ_IF *seq_funcs, 
                                         void *seq_init_param,
                                         uint n_ranges, uint mode,
                                         HANDLER_BUFFER *buffer)
{
  int error;
  DBUG_ENTER("ha_ndbcluster::multi_range_read_init");

  /*
    If supplied buffer is smaller than needed for just one range, we cannot do
    multi_range_read.
  */
  const ulong bufsize= multi_range_buffer_size(buffer);

  if (mode & HA_MRR_USE_DEFAULT_IMPL
      || bufsize < multi_range_fixed_size(1) +
                   multi_range_max_entry(get_index_type(active_index),
                                         table_share->reclength)
      || (m_pushed_join_operation==PUSHED_ROOT &&
         !m_disable_pushed_join &&
         !m_pushed_join_member->get_query_def().isScanQuery())
      || m_delete_cannot_batch || m_update_cannot_batch)
  {
    m_disable_multi_read= true;
    DBUG_RETURN(handler::multi_range_read_init(seq_funcs, seq_init_param,
                                               n_ranges, mode, buffer));
  }

  /**
   * There may still be an open m_multi_cursor from the previous mrr access on this handler.
   * Close it now to free up resources for this NdbScanOperation.
   */ 
  if (unlikely((error= close_scan())))
    DBUG_RETURN(error);

  m_disable_multi_read= false;

  mrr_is_output_sorted= (mode & HA_MRR_SORTED);
  /*
    Copy arguments into member variables
  */
  multi_range_buffer= buffer;
  mrr_funcs= *seq_funcs;
  mrr_iter= mrr_funcs.init(seq_init_param, n_ranges, mode);
  ranges_in_seq= n_ranges;
  m_range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range);
  const bool mrr_need_range_assoc = !(mode & HA_MRR_NO_ASSOCIATION);
  if (mrr_need_range_assoc)
  {
    ha_statistic_increment(&System_status_var::ha_multi_range_read_init_count);
  }

  /*
    We do not start fetching here with execute(), rather we defer this to the
    first call to multi_range_read_next() by setting first_running_range and
    first_unstarted_range like this.

    The reason is that the MRR interface is designed so that in some cases
    multi_range_read_next() may never get called (eg. in case of WHERE
    condition on previous table that is never satisfied). So we may not need
    to fetch anything.

    Also, at the time of writing, returning an error from
    multi_range_read_init() does not correctly set the error status, so we get
    an assert on missing result status in net_end_statement().
  */
  first_running_range= 0;
  first_unstarted_range= 0;

  DBUG_RETURN(0);
}


int ha_ndbcluster::multi_range_start_retrievals(uint starting_range)
{
  KEY* key_info= table->key_info + active_index;
  ulong reclength= table_share->reclength;
  const NdbOperation* op;
  NDB_INDEX_TYPE cur_index_type= get_index_type(active_index);
  const NdbOperation *oplist[MRR_MAX_RANGES];
  uint num_keyops= 0;
  NdbTransaction *trans= m_thd_ndb->trans;
  int error;
  const bool is_pushed=
    check_if_pushable(NdbQueryOperationDef::OrderedIndexScan,
                      active_index);

  DBUG_ENTER("multi_range_start_retrievals");

  /*
   * read multi range will read ranges as follows (if not ordered)
   *
   * input    read order
   * ======   ==========
   * pk-op 1  pk-op 1
   * pk-op 2  pk-op 2
   * range 3  range (3,5) NOTE result rows will be intermixed
   * pk-op 4  pk-op 4
   * range 5
   * pk-op 6  pk-op 6
   */   

  /*
    We loop over all ranges, converting into primary/unique key operations if
    possible, and adding ranges to an ordered index scan for the rest.

    If the supplied HANDLER_BUFFER is too small, we may also need to do only
    part of the multi read at once.
  */

  DBUG_ASSERT(cur_index_type != UNDEFINED_INDEX);
  DBUG_ASSERT(m_multi_cursor==NULL);
  DBUG_ASSERT(m_active_query==NULL);

  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);
  const uchar *end_of_buffer= multi_range_buffer->buffer_end;

  /*
    Normally we should have sufficient buffer for the whole fixed_sized part.
    But we need to make sure we do not crash if upper layer gave us a _really_
    small buffer.

    We already checked (in multi_range_read_init()) that we got enough buffer
    for at least one range.
  */
  uint min_entry_size=
    multi_range_entry_size(!read_multi_needs_scan(cur_index_type, key_info,
                                                  &mrr_cur_range, is_pushed),
                                                  reclength);
  const ulong bufsize= multi_range_buffer_size(multi_range_buffer);
  int max_range= multi_range_max_ranges(ranges_in_seq,
                                        bufsize - min_entry_size);
  DBUG_ASSERT(max_range > 0);
  uchar *row_buf= multi_range_buffer->buffer + multi_range_fixed_size(max_range);
  m_multi_range_result_ptr= row_buf;

  int range_no= 0;
  int mrr_range_no= starting_range;
  bool any_real_read= false;

  if (m_read_before_write_removal_possible)
    check_read_before_write_removal();

  for (;
       !m_range_res;
       range_no++, m_range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range))
  {
    if (range_no >= max_range)
      break;
    bool need_scan=
      read_multi_needs_scan(cur_index_type, key_info, &mrr_cur_range, is_pushed);
    if (row_buf + multi_range_entry_size(!need_scan, reclength) > end_of_buffer)
      break;
    if (need_scan)
    {
      if (range_no > NdbIndexScanOperation::MaxRangeNo)
        break;
      /*
        Check how much KEYINFO data we already used for index bounds, and
        split the MRR here if it exceeds a certain limit. This way we avoid
        overloading the TC block in the ndb kernel.

        The limit used is based on the value MAX_KEY_SIZE_IN_WORDS.
      */
      if (m_multi_cursor && m_multi_cursor->getCurrentKeySize() >= 1000)
        break;
    }

    mrr_range_no++;
    multi_range_put_custom(multi_range_buffer, range_no, mrr_cur_range.ptr);

    part_id_range part_spec;
    if (m_use_partition_pruning)
    {
      get_partition_set(table, table->record[0], active_index,
                        &mrr_cur_range.start_key,
                        &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                          part_spec.start_part, part_spec.end_part));
      /*
        If partition pruning has found no partition in set
        we can skip this scan
      */
      if (part_spec.start_part > part_spec.end_part)
      {
        /*
          We can skip this range since the key won't fit into any
          partition
        */
        multi_range_entry_type(row_buf)= enum_skip_range;
        row_buf= multi_range_next_entry(row_buf, reclength);
        continue;
      }
      if (!trans &&
          (part_spec.start_part == part_spec.end_part))
        if (unlikely(!(trans= start_transaction_part_id(part_spec.start_part,
                                                        error))))
          DBUG_RETURN(error);
    }

    if (need_scan)
    {
      if (!trans)
      {
        // ToDo see if we can use start_transaction_key here instead
        if (!m_use_partition_pruning)
        {
          get_partition_set(table, table->record[0], active_index,
                            &mrr_cur_range.start_key,
                            &part_spec);
          if (part_spec.start_part == part_spec.end_part)
          {
            if (unlikely(!(trans= start_transaction_part_id(part_spec.start_part,
                                                            error))))
              DBUG_RETURN(error);
          }
          else if (unlikely(!(trans= start_transaction(error))))
            DBUG_RETURN(error);
        }
        else if (unlikely(!(trans= start_transaction(error))))
          DBUG_RETURN(error);
      }

      any_real_read= true;
      DBUG_PRINT("info", ("any_real_read= true"));

      /* Create the scan operation for the first scan range. */
      if (check_if_pushable(NdbQueryOperationDef::OrderedIndexScan, 
                            active_index))
      {
        DBUG_ASSERT(!m_read_before_write_removal_used);
        if (!m_active_query)
        {
          const int error= create_pushed_join();
          if (unlikely(error))
            DBUG_RETURN(error);

          NdbQuery* const query= m_active_query;
          if (mrr_is_output_sorted &&
              query->getQueryOperation((uint)PUSHED_ROOT)->setOrdering(NdbQueryOptions::ScanOrdering_ascending))
            ERR_RETURN(query->getNdbError());
        }
      } // check_if_pushable()
      else
      if (!m_multi_cursor)
      {
        /* Do a multi-range index scan for ranges not done by primary/unique key. */
        NdbScanOperation::ScanOptions options;
        NdbInterpretedCode code(m_table);

        options.optionsPresent=
          NdbScanOperation::ScanOptions::SO_SCANFLAGS |
          NdbScanOperation::ScanOptions::SO_PARALLEL;

        options.scan_flags= 
          NdbScanOperation::SF_ReadRangeNo |
          NdbScanOperation::SF_MultiRange;

        if (lm == NdbOperation::LM_Read)
          options.scan_flags|= NdbScanOperation::SF_KeyInfo;
        if (mrr_is_output_sorted)
          options.scan_flags|= NdbScanOperation::SF_OrderByFull;

        options.parallel= DEFAULT_PARALLELISM;

        NdbOperation::GetValueSpec gets[2];
        if (table_share->primary_key == MAX_KEY)
          get_hidden_fields_scan(&options, gets);

        if (m_cond && m_cond->generate_scan_filter(&code, &options))
          ERR_RETURN(code.getNdbError());

        /* Define scan */
        NdbIndexScanOperation *scanOp= trans->scanIndex
          (m_index[active_index].ndb_record_key,
           m_ndb_record, 
           lm,
           m_table_map->get_column_mask(table->read_set),
           NULL, /* All bounds specified below */
           &options,
           sizeof(NdbScanOperation::ScanOptions));

        if (!scanOp)
          ERR_RETURN(trans->getNdbError());

        m_multi_cursor= scanOp;

        /* Can't have blobs in multi range read */
        DBUG_ASSERT(!uses_blob_value(table->read_set));

        /* We set m_next_row=0 to m that no row was fetched from the scan yet. */
        m_next_row= 0;
      }

      Ndb::PartitionSpec ndbPartitionSpec;
      const Ndb::PartitionSpec* ndbPartSpecPtr= NULL;

      /* If this table uses user-defined partitioning, use MySQLD provided
       * partition info as pruning info
       * Otherwise, scan range pruning is performed automatically by
       * NDBAPI based on distribution key values.
       */
      if (m_use_partition_pruning && 
          m_user_defined_partitioning &&
          (part_spec.start_part == part_spec.end_part))
      {
        DBUG_PRINT("info", ("Range on user-def-partitioned table can be pruned to part %u",
                            part_spec.start_part));
        ndbPartitionSpec.type= Ndb::PartitionSpec::PS_USER_DEFINED;
        ndbPartitionSpec.UserDefined.partitionId= part_spec.start_part;
        ndbPartSpecPtr= &ndbPartitionSpec;
      }

      /* Include this range in the ordered index scan. */
      NdbIndexScanOperation::IndexBound bound;
      compute_index_bounds(bound, key_info,
			   &mrr_cur_range.start_key, &mrr_cur_range.end_key, 0);
      bound.range_no= range_no;

      const NdbRecord *key_rec= m_index[active_index].ndb_record_key;
      if (m_active_query)
      {
        DBUG_PRINT("info", ("setBound:%d, for pushed join", bound.range_no));
        if (m_active_query->setBound(key_rec, &bound))
        {
          ERR_RETURN(trans->getNdbError());
        }
      }
      else
      {
        if (m_multi_cursor->setBound(m_index[active_index].ndb_record_key,
                                     bound,
                                     ndbPartSpecPtr, // Only for user-def tables
                                     sizeof(Ndb::PartitionSpec)))
        {
          ERR_RETURN(trans->getNdbError());
        }
      }

      multi_range_entry_type(row_buf)= enum_ordered_range;
      row_buf= multi_range_next_entry(row_buf, reclength);
    }
    else
    {
      multi_range_entry_type(row_buf)= enum_unique_range;

      if (!trans)
      {
        DBUG_ASSERT(active_index != MAX_KEY);
        if (unlikely(!(trans= start_transaction_key(active_index,
                                                    mrr_cur_range.start_key.key,
                                                    error))))
          DBUG_RETURN(error);
      }

      if (m_read_before_write_removal_used)
      {
        DBUG_PRINT("info", ("m_read_before_write_removal_used == true"));

        /* Key will later be returned as result record.
         * Save it in 'row_buf' from where it will later retrieved.
         */
        key_restore(multi_range_row(row_buf),
                    (uchar*)mrr_cur_range.start_key.key,
                    key_info, key_info->key_length);

        op= NULL;  // read_before_write_removal
      }
      else
      {
        any_real_read= true;
        DBUG_PRINT("info", ("any_real_read= true"));

        /* Convert to primary/unique key operation. */
        Uint32 partitionId;
        Uint32* ppartitionId = NULL;

        if (m_user_defined_partitioning &&
            (cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
             cur_index_type == PRIMARY_KEY_INDEX))
        {
          partitionId=part_spec.start_part;
          ppartitionId=&partitionId;
        }

        /**
         * 'Pushable codepath' is incomplete and expected not
         * to be produced as make_join_pushed() handle 
         * AT_MULTI_UNIQUE_KEY as non-pushable.
         */
        if (m_pushed_join_operation==PUSHED_ROOT &&
            !m_disable_pushed_join &&
            !m_pushed_join_member->get_query_def().isScanQuery())
        {
          op= NULL;            // Avoid compiler warning
          DBUG_ASSERT(false);  // FIXME: Incomplete code, should not be executed
          DBUG_ASSERT(lm == NdbOperation::LM_CommittedRead);
          const int error =
              pk_unique_index_read_key_pushed(active_index,
                                              mrr_cur_range.start_key.key);
          if (unlikely(error))
          {
            DBUG_RETURN(error);
          }
        }
        else
        {
          if (m_pushed_join_operation == PUSHED_ROOT)
          {
            DBUG_PRINT("info", ("Cannot push join due to incomplete implementation."));
            m_thd_ndb->m_pushed_queries_dropped++;
          }
          if (!(op= pk_unique_index_read_key(active_index,
                                             mrr_cur_range.start_key.key,
                                             multi_range_row(row_buf), lm,
                                             ppartitionId)))
            ERR_RETURN(trans->getNdbError());
        }
      }
      oplist[num_keyops++]= op;
      row_buf= multi_range_next_entry(row_buf, reclength);
    }
  }

  if (m_active_query != NULL &&         
      m_pushed_join_member->get_query_def().isScanQuery())
  {
    m_thd_ndb->m_scan_count++;
    if (mrr_is_output_sorted)
    {        
      m_thd_ndb->m_sorted_scan_count++;
    }

    bool prunable= false;
    if (unlikely(m_active_query->isPrunable(prunable) != 0))
      ERR_RETURN(m_active_query->getNdbError());
    if (prunable)
      m_thd_ndb->m_pruned_scan_count++;

    DBUG_PRINT("info", ("Is MRR scan-query pruned to 1 partition? :%u", prunable));
    DBUG_ASSERT(!m_multi_cursor);
  }
  if (m_multi_cursor)
  {
    DBUG_PRINT("info", ("Is MRR scan pruned to 1 partition? :%u",
                        m_multi_cursor->getPruned()));
    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (m_multi_cursor->getPruned()? 1 : 0);
    if (mrr_is_output_sorted)
    {        
      m_thd_ndb->m_sorted_scan_count++;
    }
  }

  if (any_real_read && execute_no_commit_ie(m_thd_ndb, trans))
    ERR_RETURN(trans->getNdbError());

  if (!m_range_res)
  {
    DBUG_PRINT("info",
               ("Split MRR read, %d-%d of %d bufsize=%lu used=%lu range_no=%d",
                starting_range, mrr_range_no - 1, ranges_in_seq,
                (ulong)(end_of_buffer - multi_range_buffer->buffer),
                (ulong)(row_buf - multi_range_buffer->buffer), range_no));
    /*
      Mark that we're using entire buffer (even if might not) as we are not
      reading read all ranges yet.

      This as we don't want mysqld to reuse the buffer when we read the
      remaining ranges.
    */
    multi_range_buffer->end_of_used_area= multi_range_buffer->buffer_end;
  }
  else
    multi_range_buffer->end_of_used_area= row_buf;

  first_running_range= first_range_in_batch= starting_range;
  first_unstarted_range= mrr_range_no;
  m_current_range_no= 0;

  /*
    Now we need to inspect all ranges that were converted to key operations.

    We need to check for any error (in particular NoDataFound), and remember
    the status, since the operation pointer may no longer be valid when we
    actually get to it in multi_range_next_entry() (we may have done further
    execute()'s in a different handler object during joins eg.)
  */
  row_buf= m_multi_range_result_ptr;
  uint op_idx= 0;
  for (uint r= first_range_in_batch; r < first_unstarted_range; r++)
  {
    uchar &type_loc= multi_range_entry_type(row_buf);
    row_buf= multi_range_next_entry(row_buf, reclength);
    if (type_loc >= enum_ordered_range)
      continue;

    DBUG_ASSERT(op_idx < MRR_MAX_RANGES);
    if ((op= oplist[op_idx++]) == NULL)
      continue;  // read_before_write_removal

    const NdbError &error= op->getNdbError();
    if (error.code != 0)
    {
      if (error.classification == NdbError::NoDataFound)
        type_loc= enum_empty_unique_range;
      else
      {
        /*
          This shouldn't really happen.

          There aren't really any other errors that could happen on the read
          without also aborting the transaction and causing execute() to
          return failure.

          (But we can still safely return an error code in non-debug builds).
        */
        DBUG_ASSERT(false);
        ERR_RETURN(error);      /* purecov: deadcode */
      }
    }
  }

  DBUG_RETURN(0);
}

int ha_ndbcluster::multi_range_read_next(char **range_info)
{
  int res;
  DBUG_ENTER("ha_ndbcluster::multi_range_read_next");

  if (m_disable_multi_read)
  {
    DBUG_RETURN(handler::multi_range_read_next(range_info));
  }

  for(;;)
  {
    /* for each range (we should have remembered the number) */
    while (first_running_range < first_unstarted_range)
    {
      uchar *row_buf= m_multi_range_result_ptr;
      int expected_range_no= first_running_range - first_range_in_batch;

      switch (multi_range_entry_type(row_buf))
      {
        case enum_skip_range:
        case enum_empty_unique_range:
          /* Nothing in this range; continue with next. */
          break;

        case enum_unique_range:
          /*
            Move to next range; we can have at most one record from a unique
            range.
          */
          first_running_range++;
          m_multi_range_result_ptr=
            multi_range_next_entry(m_multi_range_result_ptr,
                                   table_share->reclength);

          /*
            Clear m_active_cursor; it is used as a flag in update_row() /
            delete_row() to know whether the current tuple is from a scan
            or pk operation.
          */
          m_active_cursor= NULL;

          /* Return the record. */
          *range_info= multi_range_get_custom(multi_range_buffer,
                                              expected_range_no);
          memcpy(table->record[0], multi_range_row(row_buf),
                 table_share->stored_rec_length);
          if(table->has_gcol())
          {
            update_generated_read_fields(table->record[0], table);
          }
          DBUG_RETURN(0);

        case enum_ordered_range:
          /* An index scan range. */
          {
            int res;
            if ((res= read_multi_range_fetch_next()) != 0)
            {
              *range_info= multi_range_get_custom(multi_range_buffer,
                                                  expected_range_no);
              first_running_range++;
              m_multi_range_result_ptr=
                multi_range_next_entry(m_multi_range_result_ptr,
                                       table_share->reclength);
              DBUG_RETURN(res);
            }
          }
          if (!m_next_row)
          {
            /*
              The whole scan is done, and the cursor has been closed.
              So nothing more for this range. Move to next.
            */
            break;
          }
          else
          {
            int current_range_no= m_current_range_no;
            /*
              For a sorted index scan, we will receive rows in increasing
              range_no order, so we can return ranges in order, pausing when
              range_no indicate that the currently processed range
              (first_running_range) is done.

              But for unsorted scan, we may receive a high range_no from one
              fragment followed by a low range_no from another fragment. So we
              need to process all index scan ranges together.
            */
            if (!mrr_is_output_sorted || expected_range_no == current_range_no)
            {
              *range_info= multi_range_get_custom(multi_range_buffer,
                                                  current_range_no);
              /* Copy out data from the new row. */
              unpack_record_and_set_generated_fields(table, table->record[0],
                                                     m_next_row);
              /*
                Mark that we have used this row, so we need to fetch a new
                one on the next call.
              */
              m_next_row= 0;
              /*
                Set m_active_cursor; it is used as a flag in update_row() /
                delete_row() to know whether the current tuple is from a scan or
                pk operation.
              */
              m_active_cursor= m_multi_cursor;

              DBUG_RETURN(0);
            }

            if (current_range_no > expected_range_no)
            {
              /* Nothing more in scan for this range. Move to next. */
              break;
            }

            /*
              Should not happen. Ranges should be returned from NDB API in
              the order we requested them.
            */
            DBUG_ASSERT(0);
            break;                              // Attempt to carry on
          }

        default:
          DBUG_ASSERT(0);
      }
      /* At this point the current range is done, proceed to next. */
      first_running_range++;
      m_multi_range_result_ptr=
        multi_range_next_entry(m_multi_range_result_ptr, table_share->reclength);
    }

    if (m_range_res)   // mrr_funcs.next() has consumed all ranges.
      DBUG_RETURN(HA_ERR_END_OF_FILE);

    /*
      Read remaining ranges
    */
    if ((res= multi_range_start_retrievals(first_running_range)))
      DBUG_RETURN(res);

  } // for(;;)
}


/*
  Fetch next row from the ordered index cursor in multi range scan.

  We keep the next row in m_next_row, and the range_no of the
  next row in m_current_range_no. This is used in sorted index scan
  to correctly interleave rows from primary/unique key operations with
  rows from the scan.
*/
int
ha_ndbcluster::read_multi_range_fetch_next()
{
  DBUG_ENTER("read_multi_range_fetch_next");

  if (m_active_query)
  {
    DBUG_PRINT("info", ("read_multi_range_fetch_next from pushed join, m_next_row:%p", m_next_row));
    if (!m_next_row)
    {
      int res= fetch_next_pushed();
      if (res == NdbQuery::NextResult_gotRow)
      {
        m_current_range_no= 0;
//      m_current_range_no= cursor->get_range_no();  // FIXME SPJ, need rangeNo from index scan
      }
      else if (res == NdbQuery::NextResult_scanComplete)
      {
        /* We have fetched the last row from the scan. */
        m_active_query->close(false);
        m_active_query= NULL;
        m_next_row= 0;
        DBUG_RETURN(0);
      }
      else
      {
        /* An error. */
        DBUG_RETURN(res);
      }
    }
  }
  else if (m_multi_cursor)
  {
    if (!m_next_row)
    {
      NdbIndexScanOperation *cursor= m_multi_cursor;
      int res= fetch_next(cursor);
      if (res == 0)
      {
        m_current_range_no= cursor->get_range_no();
      }
      else if (res == 1)
      {
        /* We have fetched the last row from the scan. */
        cursor->close(false, true);
        m_active_cursor= 0;
        m_multi_cursor= 0;
        m_next_row= 0;
        DBUG_RETURN(0);
      }
      else
      {
        /* An error. */
        DBUG_RETURN(res);
      }
    }
  }
  DBUG_RETURN(0);
}


/**
 * Try to find pushable subsets of a join plan.
 * @param hton unused (maybe useful for other engines).
 * @param thd Thread.
 * @param plan The join plan to examine.
 * @return Possible error code.
 */

static int ndbcluster_make_pushed_join(handlerton *, THD *thd,
                                       const AQP::Join_plan *plan)
{
  DBUG_ENTER("ndbcluster_make_pushed_join");

  if (THDVAR(thd, join_pushdown) &&
      // Check for online upgrade/downgrade.
      ndbd_join_pushdown(g_ndb_cluster_connection->get_min_db_version()))
  {
    bool pushed_something = false;
    ndb_pushed_builder_ctx pushed_builder(*plan);

    for (uint i= 0; i < plan->get_access_count()-1; i++)
    {
      const AQP::Table_access* const join_root= plan->get_table_access(i);
      const ndb_pushed_join* pushed_join= NULL;

      // Try to build a ndb_pushed_join starting from 'join_root'
      int error= pushed_builder.make_pushed_join(join_root, pushed_join);
      if (unlikely(error))
      {
        if (error < 0)  // getNdbError() gives us the error code
        {
          ERR_SET(pushed_builder.getNdbError(),error);
        }
        join_root->get_table()->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }

      // Assign any produced pushed_join definitions to 
      // the ha_ndbcluster instance representing its root.
      if (pushed_join != NULL)
      {
        ha_ndbcluster* const handler=
          static_cast<ha_ndbcluster*>(join_root->get_table()->file);

        error= handler->assign_pushed_join(pushed_join);
        if (unlikely(error))
        {
          delete pushed_join;
          handler->print_error(error, MYF(0));
          DBUG_RETURN(error);
        }
        // Something was pushed and the QEP need to be modified
        pushed_something = true;
      }
    }

    if (pushed_something)
    {
      // Modify the QEP_TAB's to use the 'linked' read functions
      // for those parts of the join which have been pushed down.
      for (uint i= 0; i < plan->get_access_count(); i++)
      {
        plan->get_table_access(i)->set_pushed_table_access_method();
      }
    }
  }
  DBUG_RETURN(0);
}


/**
 * In case a pushed join having the table for this handler as its root
 * has been produced. ::assign_pushed_join() is responsible for setting
 * up this ha_ndbcluster instance such that the prepared NdbQuery 
 * might be instantiated at execution time.
 */
int
ha_ndbcluster::assign_pushed_join(const ndb_pushed_join* pushed_join)
{
  DBUG_ENTER("assign_pushed_join");
  m_thd_ndb->m_pushed_queries_defined++;

  for (uint i = 0; i < pushed_join->get_operation_count(); i++)
  {
    const TABLE* const tab= pushed_join->get_table(i);
    DBUG_ASSERT(tab->file->ht == ht);
    ha_ndbcluster* child= static_cast<ha_ndbcluster*>(tab->file);
    child->m_pushed_join_member= pushed_join;
    child->m_pushed_join_operation= i;
  }

  DBUG_PRINT("info", ("Assigned pushed join with %d child operations", 
                      pushed_join->get_operation_count()-1));

  DBUG_RETURN(0);
}


/**
 * First level of filtering tables which *maybe* may be part of
 * a pushed query: Returning 'false' will eliminate this table
 * from being a part of a pushed join.
 * A 'reason' for rejecting this table is required if 'false'
 * is returned.
 */
bool
ha_ndbcluster::maybe_pushable_join(const char*& reason) const
{
  reason= NULL;
  if (uses_blob_value(table->read_set))
  {
    reason= "select list can't contain BLOB columns";
    return false;
  }
  if (m_user_defined_partitioning)
  {
    reason= "has user defined partioning";
    return false;
  }

  // Pushed operations may not set locks.
  const NdbOperation::LockMode lockMode= get_ndb_lock_mode(m_lock.type);
  switch (lockMode)
  {
  case NdbOperation::LM_CommittedRead:
    return true;

  case NdbOperation::LM_Read:
  case NdbOperation::LM_Exclusive:
    reason= "lock modes other than 'read committed' not implemented";
    return false;
        
  default: // Other lock modes not used by handler.
    assert(false);
    return false;
  }

  return true;
}

/**
 * Check if this table access operation (and a number of succeding operation)
 * can be pushed to the cluster and executed there. This requires that there
 * is an NdbQueryDefiniton and that it still matches the corresponds to the 
 * type of operation that we intend to execute. (The MySQL server will 
 * sometimes change its mind and replace a scan with a lookup or vice versa
 * as it works its way into the nested loop join.)
 *
 * @param type This is the operation type that the server want to execute.
 * @param idx  Index used whenever relevant for operation type
 * @param needSorted True if the root operation is an ordered index scan 
 * with sorted results.
 * @return True if the operation may be pushed.
 */
bool 
ha_ndbcluster::check_if_pushable(int type,  //NdbQueryOperationDef::Type, 
                                 uint idx) const
{
  if (m_disable_pushed_join)
  {
    DBUG_PRINT("info", ("Push disabled (HA_EXTRA_KEYREAD)"));
    return false;
  }
  return   m_pushed_join_operation == PUSHED_ROOT
        && m_pushed_join_member    != NULL
        && m_pushed_join_member->match_definition(
                        type,
                        (idx<MAX_KEY) ? &m_index[idx] : NULL);
}


int
ha_ndbcluster::create_pushed_join(const NdbQueryParamValue* keyFieldParams, uint paramCnt)
{
  DBUG_ENTER("create_pushed_join");
  DBUG_ASSERT(m_pushed_join_member && m_pushed_join_operation == PUSHED_ROOT);

  NdbQuery* const query= 
    m_pushed_join_member->make_query_instance(m_thd_ndb->trans, keyFieldParams, paramCnt);

  if (unlikely(query==NULL))
    ERR_RETURN(m_thd_ndb->trans->getNdbError());

  // Bind to instantiated NdbQueryOperations.
  for (uint i= 0; i < m_pushed_join_member->get_operation_count(); i++)
  {
    const TABLE* const tab= m_pushed_join_member->get_table(i);
    ha_ndbcluster* handler= static_cast<ha_ndbcluster*>(tab->file);

    DBUG_ASSERT(handler->m_pushed_join_operation==(int)i);
    NdbQueryOperation* const op= query->getQueryOperation(i);
    handler->m_pushed_operation= op;

    // Bind to result buffers
    int res= op->setResultRowRef(
                        handler->m_ndb_record,
                        handler->_m_next_row,
                        handler->m_table_map->get_column_mask(tab->read_set));
    if (unlikely(res))
      ERR_RETURN(query->getNdbError());
    
    // We clear 'm_next_row' to say that no row was fetched from the query yet.
    handler->_m_next_row= 0;
  }

  DBUG_ASSERT(m_active_query==NULL);
  m_active_query= query;
  m_thd_ndb->m_pushed_queries_executed++;

  DBUG_RETURN(0);
}


/**
 * Check if this table access operation is part of a pushed join operation
 * which is actively executing.
 */
bool 
ha_ndbcluster::check_is_pushed() const
{
  if (m_pushed_join_member == NULL)
    return false;

  handler *root= m_pushed_join_member->get_table(PUSHED_ROOT)->file;
  return (static_cast<ha_ndbcluster*>(root)->m_active_query);
}

uint 
ha_ndbcluster::number_of_pushed_joins() const
{
  if (m_pushed_join_member == NULL)
    return 0;
  else
    return m_pushed_join_member->get_operation_count();
}

const TABLE*
ha_ndbcluster::root_of_pushed_join() const
{
  if (m_pushed_join_member == NULL)
    return NULL;
  else
    return m_pushed_join_member->get_table(PUSHED_ROOT);
}

const TABLE*
ha_ndbcluster::parent_of_pushed_join() const
{
  if (m_pushed_join_operation > PUSHED_ROOT)
  {
    DBUG_ASSERT(m_pushed_join_member!=NULL);
    uint parent_ix= m_pushed_join_member
                    ->get_query_def().getQueryOperation(m_pushed_join_operation)
                    ->getParentOperation(0)
                    ->getOpNo();
    return m_pushed_join_member->get_table(parent_ix);
  }
  return NULL;
}


/*
  Condition pushdown
*/
/**
  Push a condition to ndbcluster storage engine for evaluation 
  during table   and index scans. The conditions will be stored on a stack
  for possibly storing several conditions. The stack can be popped
  by calling cond_pop, handler::extra(HA_EXTRA_RESET) (handler::reset())
  will clear the stack.
  The current implementation supports arbitrary AND/OR nested conditions
  with comparisons between columns and constants (including constant
  expressions and function calls) and the following comparison operators:
  =, !=, >, >=, <, <=, "is null", and "is not null".
  
  @retval
    NULL The condition was supported and will be evaluated for each 
         row found during the scan
  @retval
    cond The condition was not supported and all rows will be returned from
         the scan for evaluation (and thus not saved on stack)
*/
const 
Item* 
ha_ndbcluster::cond_push(const Item *cond) 
{ 
  DBUG_ENTER("ha_ndbcluster::cond_push");

  if (cond->used_tables() & ~table->pos_in_table_list->map())
  {
    /**
     * 'cond' refers fields from other tables, or other instances 
     * of this table, -> reject it.
     * (Optimizer need to have a better understanding of what is 
     *  pushable by each handler.)
     */
    DBUG_EXECUTE("where",print_where((Item *)cond, "Rejected cond_push", QT_ORDINARY););
    DBUG_RETURN(cond);
  }

  if (m_cond == nullptr)
  {
    m_cond= new (std::nothrow) ha_ndbcluster_cond;
    if (m_cond == nullptr)
    {
      // Failed to allocate condition pushdown, return
      // the full cond in order to indicate that it was not supported
      // and caller has to evalute each row returned
      DBUG_RETURN(cond);
    }
  }

  DBUG_EXECUTE("where",print_where((Item *)cond, m_tabname, QT_ORDINARY););
  DBUG_RETURN(m_cond->cond_push(cond, table, (NDBTAB *)m_table));
}

/**
  Pop the top condition from the condition stack of the handler instance.
*/
void 
ha_ndbcluster::cond_pop() 
{ 
  if (m_cond)
    m_cond->cond_pop();
}


/*
  Implements the SHOW ENGINE NDB STATUS command.
*/
bool
ndbcluster_show_status(handlerton*, THD* thd, stat_print_fn *stat_print,
                       enum ha_stat_type stat_type)
{
  char name[16];
  char buf[IO_SIZE];
  uint buflen;
  DBUG_ENTER("ndbcluster_show_status");
  
  if (stat_type != HA_ENGINE_STATUS)
  {
    DBUG_RETURN(false);
  }

  Ndb* ndb= check_ndb_in_thd(thd);
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  struct st_ndb_status ns;
  if (ndb)
    update_status_variables(thd_ndb, &ns, thd_ndb->connection);
  else
    update_status_variables(NULL, &ns, g_ndb_cluster_connection);

  buflen= (uint)
    snprintf(buf, sizeof(buf),
                "cluster_node_id=%ld, "
                "connected_host=%s, "
                "connected_port=%ld, "
                "number_of_data_nodes=%ld, "
                "number_of_ready_data_nodes=%ld, "
                "connect_count=%ld",
                ns.cluster_node_id,
                ns.connected_host,
                ns.connected_port,
                ns.number_of_data_nodes,
                ns.number_of_ready_data_nodes,
                ns.connect_count);
  if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                 STRING_WITH_LEN("connection"), buf, buflen))
    DBUG_RETURN(true);

  for (int i= 0; i < MAX_NDB_NODES; i++)
  {
    if (ns.transaction_hint_count[i] > 0 ||
        ns.transaction_no_hint_count[i] > 0)
    {
      uint namelen= (uint)snprintf(name, sizeof(name), "node[%d]", i);
      buflen= (uint)snprintf(buf, sizeof(buf),
                          "transaction_hint=%ld, transaction_no_hint=%ld",
                          ns.transaction_hint_count[i],
                          ns.transaction_no_hint_count[i]);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     name, namelen, buf, buflen))
        DBUG_RETURN(true);
    }
  }

  if (ndb)
  {
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      buflen= (uint)
        snprintf(buf, sizeof(buf),
                  "created=%u, free=%u, sizeof=%u",
                  tmp.m_created, tmp.m_free, tmp.m_sizeof);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     tmp.m_name, (uint)strlen(tmp.m_name), buf, buflen))
        DBUG_RETURN(true);
    }
  }

  buflen = (uint)ndbcluster_show_status_binlog(buf, sizeof(buf));
  if (buflen)
  {
    if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                   STRING_WITH_LEN("binlog"), buf, buflen))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


int ha_ndbcluster::get_default_num_partitions(HA_CREATE_INFO *create_info)
{
  if (unlikely(g_ndb_cluster_connection->get_no_ready() <= 0))
  {
err:
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return -1;
  }

  THD* thd = current_thd;
  if (thd == 0)
    goto err;
  Thd_ndb * thd_ndb = get_thd_ndb(thd);
  if (thd_ndb == 0)
    goto err;

  ha_rows max_rows, min_rows;
  if (create_info)
  {
    max_rows= create_info->max_rows;
    min_rows= create_info->min_rows;
  }
  else
  {
    max_rows= table_share->max_rows;
    min_rows= table_share->min_rows;
  }
  uint no_fragments= get_no_fragments(max_rows >= min_rows ?
                                      max_rows : min_rows);
  uint reported_frags;
  adjusted_frag_count(thd_ndb->ndb,
                      no_fragments,
                      reported_frags);
  return reported_frags;
}

uint32 ha_ndbcluster::calculate_key_hash_value(Field **field_array)
{
  Uint32 hash_value;
  struct Ndb::Key_part_ptr key_data[MAX_REF_PARTS];
  struct Ndb::Key_part_ptr *key_data_ptr= &key_data[0];
  Uint32 i= 0;
  int ret_val;
  Uint64 tmp[(MAX_KEY_SIZE_IN_WORDS*MAX_XFRM_MULTIPLY) >> 1];
  void *buf= (void*)&tmp[0];
  DBUG_ENTER("ha_ndbcluster::calculate_key_hash_value");

  do
  {
    Field *field= *field_array;
    uint len= field->data_length();
    DBUG_ASSERT(!field->is_real_null());
    if (field->real_type() == MYSQL_TYPE_VARCHAR)
      len+= ((Field_varstring*)field)->length_bytes;
    key_data[i].ptr= field->ptr;
    key_data[i++].len= len;
  } while (*(++field_array));
  key_data[i].ptr= 0;
  if ((ret_val= Ndb::computeHash(&hash_value, m_table,
                                 key_data_ptr, buf, sizeof(tmp))))
  {
    DBUG_PRINT("info", ("ret_val = %d", ret_val));
    DBUG_ASSERT(false);
    abort();
  }
  DBUG_RETURN(m_table->getPartitionId(hash_value));
}


/*
  Set-up auto-partitioning for NDB Cluster

  SYNOPSIS
    set_auto_partitions()
    part_info                  Partition info struct to set-up
 
  RETURN VALUE
    NONE

  DESCRIPTION
    Set-up auto partitioning scheme for tables that didn't define any
    partitioning. We'll use PARTITION BY KEY() in this case which
    translates into partition by primary key if a primary key exists
    and partition by hidden key otherwise.
*/

enum ndb_distribution_enum {
  NDB_DISTRIBUTION_KEYHASH= 0,
  NDB_DISTRIBUTION_LINHASH= 1
};
static const char* distribution_names[]= { "KEYHASH", "LINHASH", NullS };
static ulong opt_ndb_distribution;
static TYPELIB distribution_typelib= {
  array_elements(distribution_names) - 1,
  "",
  distribution_names,
  NULL
};
static MYSQL_SYSVAR_ENUM(
  distribution,                      /* name */
  opt_ndb_distribution,              /* var */
  PLUGIN_VAR_RQCMDARG,
  "Default distribution for new tables in ndb",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  NDB_DISTRIBUTION_KEYHASH,          /* default */
  &distribution_typelib              /* typelib */
);


void ha_ndbcluster::set_auto_partitions(partition_info *part_info)
{
  DBUG_ENTER("ha_ndbcluster::set_auto_partitions");
  part_info->list_of_part_fields= true;
  part_info->part_type= partition_type::HASH;
  switch (opt_ndb_distribution)
  {
  case NDB_DISTRIBUTION_KEYHASH:
    part_info->linear_hash_ind= false;
    break;
  case NDB_DISTRIBUTION_LINHASH:
    part_info->linear_hash_ind= true;
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  DBUG_VOID_RETURN;
}


static int
create_table_set_range_data(const partition_info *part_info,
                            NdbDictionary::Table& ndbtab)
{
  const uint num_parts = part_info->num_parts;
  DBUG_ENTER("create_table_set_range_data");

  int32 *range_data= (int32*)my_malloc(PSI_INSTRUMENT_ME, num_parts*sizeof(int32), MYF(0));
  if (!range_data)
  {
    mem_alloc_error(num_parts*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (uint i= 0; i < num_parts; i++)
  {
    longlong range_val= part_info->range_int_array[i];
    const bool unsigned_flag= part_info->part_expr->unsigned_flag;
    if (unsigned_flag)
      range_val-= 0x8000000000000000ULL;
    if (range_val < INT_MIN32 || range_val >= INT_MAX32)
    {
      if ((i != num_parts - 1) ||
          (range_val != LLONG_MAX))
      {
        my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
        my_free(range_data);
        DBUG_RETURN(1);
      }
      range_val= INT_MAX32;
    }
    range_data[i]= (int32)range_val;
  }
  ndbtab.setRangeListData(range_data, num_parts);
  my_free(range_data);
  DBUG_RETURN(0);
}


static int
create_table_set_list_data(const partition_info *part_info,
                           NdbDictionary::Table& ndbtab)
{
  const uint num_list_values = part_info->num_list_values;
  int32 *list_data= (int32*)my_malloc(PSI_INSTRUMENT_ME,
                                      num_list_values*2*sizeof(int32), MYF(0));
  DBUG_ENTER("create_table_set_list_data");

  if (!list_data)
  {
    mem_alloc_error(num_list_values*2*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (uint i= 0; i < num_list_values; i++)
  {
    LIST_PART_ENTRY *list_entry= &part_info->list_array[i];
    longlong list_val= list_entry->list_value;
    const bool unsigned_flag= part_info->part_expr->unsigned_flag;
    if (unsigned_flag)
      list_val-= 0x8000000000000000ULL;
    if (list_val < INT_MIN32 || list_val > INT_MAX32)
    {
      my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
      my_free(list_data);
      DBUG_RETURN(1);
    }
    list_data[2*i]= (int32)list_val;
    list_data[2*i+1]= list_entry->partition_id;
  }
  ndbtab.setRangeListData(list_data, 2*num_list_values);
  my_free(list_data);
  DBUG_RETURN(0);
}

/*
  User defined partitioning set-up. We need to check how many fragments the
  user wants defined and which node groups to put those into.

  All the functionality of the partition function, partition limits and so
  forth are entirely handled by the MySQL Server. There is one exception to
  this rule for PARTITION BY KEY where NDB handles the hash function and
  this type can thus be handled transparently also by NDB API program.
  For RANGE, HASH and LIST and subpartitioning the NDB API programs must
  implement the function to map to a partition.
*/

static int
create_table_set_up_partition_info(partition_info *part_info,
                                   NdbDictionary::Table& ndbtab,
                                   Ndb_table_map & colIdMap)
{
  DBUG_ENTER("create_table_set_up_partition_info");

  if (part_info->part_type == partition_type::HASH &&
      part_info->list_of_part_fields == true)
  {
    Field **fields= part_info->part_field_array;

    DBUG_PRINT("info", ("Using HashMapPartition fragmentation type"));
    ndbtab.setFragmentType(NDBTAB::HashMapPartition);

    for (uint i= 0; i < part_info->part_field_list.elements; i++)
    {
      DBUG_ASSERT(fields[i]->stored_in_db);
      NDBCOL *col= colIdMap.getColumn(ndbtab, fields[i]->field_index);
      DBUG_PRINT("info",("setting dist key on %s", col->getName()));
      col->setPartitionKey(true);
    }
  }
  else 
  {
    if (!current_thd->variables.new_mode)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER_THD(current_thd, ER_ILLEGAL_HA_CREATE_OPTION),
                          ndbcluster_hton_name,
                          "LIST, RANGE and HASH partition disabled by default,"
                          " use --new option to enable");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
   /*
      Create a shadow field for those tables that have user defined
      partitioning. This field stores the value of the partition
      function such that NDB can handle reorganisations of the data
      even when the MySQL Server isn't available to assist with
      calculation of the partition function value.
    */
    NDBCOL col;
    DBUG_PRINT("info", ("Generating partition func value field"));
    col.setName("$PART_FUNC_VALUE");
    col.setType(NdbDictionary::Column::Int);
    col.setLength(1);
    col.setNullable(false);
    col.setPrimaryKey(false);
    col.setAutoIncrement(false);
    ndbtab.addColumn(col);
    if (part_info->part_type == partition_type::RANGE)
    {
      const int error = create_table_set_range_data(part_info, ndbtab);
      if (error)
      {
        DBUG_RETURN(error);
      }
    }
    else if (part_info->part_type == partition_type::LIST)
    {
      const int error = create_table_set_list_data(part_info, ndbtab);
      if (error)
      {
        DBUG_RETURN(error);
      }
    }

    DBUG_PRINT("info", ("Using UserDefined fragmentation type"));
    ndbtab.setFragmentType(NDBTAB::UserDefined);
  }

  const bool use_default_num_parts = part_info->use_default_num_partitions;
  ndbtab.setDefaultNoPartitionsFlag(use_default_num_parts);
  ndbtab.setLinearFlag(part_info->linear_hash_ind);

  if (ndbtab.getFragmentType()  == NDBTAB::HashMapPartition &&
      use_default_num_parts)
  {
    /**
     * Skip below for default partitioning, this removes the need to undo
     * these settings later in ha_ndbcluster::create.
     */
    DBUG_RETURN(0);
  }

  {
    // Count number of fragments to use for the table and
    // build array describing which nodegroup should store each
    // partition(each partition is mapped to one fragment in the table).
    uint32 frag_data[MAX_PARTITIONS];
    ulong fd_index= 0;

    partition_element *part_elem;
    List_iterator<partition_element> part_it(part_info->partitions);
    while((part_elem = part_it++))
    {
      if (!part_info->is_sub_partitioned())
      {
        const Uint32 ng= part_elem->nodegroup_id;
        assert(fd_index < NDB_ARRAY_SIZE(frag_data));
        frag_data[fd_index++]= ng;
      }
      else
      {
        partition_element *subpart_elem;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        while((subpart_elem = sub_it++))
        {
          const Uint32 ng= subpart_elem->nodegroup_id;
          assert(fd_index < NDB_ARRAY_SIZE(frag_data));
          frag_data[fd_index++]= ng;
        }
      }
    }

    // Double check number of partitions vs. fragments
    DBUG_ASSERT(part_info->get_tot_partitions() == fd_index);

    ndbtab.setFragmentCount(fd_index);
    ndbtab.setFragmentData(frag_data, fd_index);
    ndbtab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
  }
  DBUG_RETURN(0);
}

class NDB_ALTER_DATA : public inplace_alter_handler_ctx 
{
public:
  NDB_ALTER_DATA(THD* thd, NdbDictionary::Dictionary *dict,
		 const NdbDictionary::Table *table) :
    dictionary(dict),
    old_table(table),
    new_table(new NdbDictionary::Table(*table)),
    table_id(table->getObjectId()),
    old_table_version(table->getObjectVersion()),
    schema_dist_client(thd)
  {}
  ~NDB_ALTER_DATA()
  { delete new_table; }
  NdbDictionary::Dictionary *dictionary;
  const  NdbDictionary::Table *old_table;
  NdbDictionary::Table *new_table;
  const Uint32 table_id;
  const Uint32 old_table_version;
  Ndb_schema_dist_client schema_dist_client;
};

/*
  Utility function to use when reporting that inplace alter
  is not supported.
*/

static inline
enum_alter_inplace_result
inplace_unsupported(Alter_inplace_info *alter_info,
                    const char* reason)
{
  DBUG_ENTER("inplace_unsupported");
  DBUG_PRINT("info", ("%s", reason));
  alter_info->unsupported_reason = reason;
  DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
}

void
ha_ndbcluster::check_implicit_column_format_change(TABLE *altered_table,
                                                   Alter_inplace_info *ha_alter_info) const
{
  /*
    We need to check if the table was defined when the default COLUMN_FORMAT
    was FIXED and will now be become DYNAMIC.
    We need to warn the user if the ALTER TABLE isn't defined to be INPLACE
    and the column which will change isn't about to be dropped.
  */
  DBUG_ENTER("ha_ndbcluster::check_implicit_column_format_change");
  DBUG_PRINT("info", ("Checking table with version %lu",
                      table->s->mysql_version));
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags=
    ha_alter_info->handler_flags;

  /* Find the old fields */
  for (uint i= 0; i < table->s->fields; i++)
  {
    Field *field= table->field[i];

    /*
      Find fields that are not part of the primary key
      and that have a default COLUMN_FORMAT.
    */
    if ((! (field->flags & PRI_KEY_FLAG)) &&
        field->column_format() == COLUMN_FORMAT_TYPE_DEFAULT)
    {
      DBUG_PRINT("info", ("Found old non-pk field %s", field->field_name));
      bool modified_explicitly= false;
      bool dropped= false;
      /*
        If the field is dropped or
        modified with and explicit COLUMN_FORMAT (FIXED or DYNAMIC)
        we don't need to warn the user about that field.
      */
      if (alter_flags & Alter_inplace_info::DROP_COLUMN ||
          alter_flags & Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT)
      {
        if (alter_flags & Alter_inplace_info::DROP_COLUMN)
          dropped= true;
        /* Find the fields in modified table*/
        for (uint j= 0; j < altered_table->s->fields; j++)
        {
          Field *field2= altered_table->field[j];
          if (!my_strcasecmp(system_charset_info,
                             field->field_name, field2->field_name))
          {
            dropped= false;
            if (field2->column_format() != COLUMN_FORMAT_TYPE_DEFAULT)
            {
              modified_explicitly= true;
            }
          }
        }
        if (dropped)
          DBUG_PRINT("info", ("Field %s is to be dropped", field->field_name));
        if (modified_explicitly)
          DBUG_PRINT("info", ("Field  %s is modified with explicit COLUMN_FORMAT",
                              field->field_name));
      }
      if ((! dropped) && (! modified_explicitly))
      {
        // push a warning of COLUMN_FORMAT change
        push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                            ER_ALTER_INFO,
                            "check_if_supported_inplace_alter: "
                            "field %s has default COLUMN_FORMAT fixed "
                            "which will be changed to dynamic "
                            "unless explicitly defined as COLUMN_FORMAT FIXED",
                            field->field_name);
      }
    }
  }

  DBUG_VOID_RETURN;
}

enum_alter_inplace_result
ha_ndbcluster::check_inplace_alter_supported(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info)
{
  THD *thd= current_thd;
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  Alter_info *alter_info= ha_alter_info->alter_info;
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags=
      ha_alter_info->handler_flags;
  const Alter_inplace_info::HA_ALTER_FLAGS supported=
    Alter_inplace_info::ADD_INDEX |
    Alter_inplace_info::DROP_INDEX |
    Alter_inplace_info::ADD_UNIQUE_INDEX |
    Alter_inplace_info::DROP_UNIQUE_INDEX |
    Alter_inplace_info::ADD_STORED_BASE_COLUMN |
    Alter_inplace_info::ADD_VIRTUAL_COLUMN |
    Alter_inplace_info::ALTER_COLUMN_DEFAULT |
    Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE |
    Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT |
    Alter_inplace_info::ADD_PARTITION |
    Alter_inplace_info::ALTER_TABLE_REORG |
    Alter_inplace_info::CHANGE_CREATE_OPTION |
    Alter_inplace_info::ADD_FOREIGN_KEY |
    Alter_inplace_info::DROP_FOREIGN_KEY |
    Alter_inplace_info::ALTER_INDEX_COMMENT;

  const Alter_inplace_info::HA_ALTER_FLAGS not_supported= ~supported;

  Alter_inplace_info::HA_ALTER_FLAGS add_column=
    Alter_inplace_info::ADD_VIRTUAL_COLUMN |
    Alter_inplace_info::ADD_STORED_BASE_COLUMN;

  const Alter_inplace_info::HA_ALTER_FLAGS adding=
    Alter_inplace_info::ADD_INDEX |
    Alter_inplace_info::ADD_UNIQUE_INDEX;

  const Alter_inplace_info::HA_ALTER_FLAGS dropping=
    Alter_inplace_info::DROP_INDEX |
    Alter_inplace_info::DROP_UNIQUE_INDEX;

  enum_alter_inplace_result result= HA_ALTER_INPLACE_SHARED_LOCK;

  DBUG_ENTER("ha_ndbcluster::check_inplace_alter_supported");
  partition_info *part_info= altered_table->part_info;
  const NDBTAB *old_tab= m_table;

  if (THDVAR(thd, use_copying_alter_table) &&
      (alter_info->requested_algorithm ==
       Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT))
  {
    // Usage of copying alter has been forced and user has not specified
    // any ALGORITHM=, don't allow inplace
    DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                    "ndb_use_copying_alter_table is set"));
  }

  DBUG_PRINT("info", ("Passed alter flags 0x%llx", alter_flags));
  DBUG_PRINT("info", ("Supported 0x%llx", supported));
  DBUG_PRINT("info", ("Not supported 0x%llx", not_supported));
  DBUG_PRINT("info", ("alter_flags & not_supported 0x%llx",
                        alter_flags & not_supported));

  bool max_rows_changed= false;
  bool comment_changed = false;
  bool table_storage_changed= false;
  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    DBUG_PRINT("info", ("Some create options changed"));
    if (create_info->used_fields & HA_CREATE_USED_AUTO &&
        create_info->auto_increment_value != stats.auto_increment_value)
    {
      DBUG_PRINT("info", ("The AUTO_INCREMENT value changed"));

      /* Check that no other create option changed */
      if (create_info->used_fields ^ ~HA_CREATE_USED_AUTO)
      {
        DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                        "Not only AUTO_INCREMENT value "
                                        "changed"));
      }
    }

    /* Check that ROW_FORMAT didn't change */
    if (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT &&
        create_info->row_type != table_share->real_row_type)
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info, "ROW_FORMAT changed"));
    }

    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS)
    {
      DBUG_PRINT("info", ("The MAX_ROWS value changed"));

      max_rows_changed= true;

      const ulonglong curr_max_rows = table_share->max_rows;
      if (curr_max_rows == 0)
      {
        // Don't support setting MAX_ROWS on a table without MAX_ROWS
        DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                        "setting MAX_ROWS on table "
                                        "without MAX_ROWS"));
      }
    }
    if (create_info->used_fields & HA_CREATE_USED_COMMENT)
    {
      DBUG_PRINT("info", ("The COMMENT string changed"));
      comment_changed = true;
    }

    enum ha_storage_media new_table_storage= create_info->storage_media;
    if (new_table_storage == HA_SM_DEFAULT)
      new_table_storage= HA_SM_MEMORY;
    enum ha_storage_media old_table_storage= table->s->default_storage_media;
    if (old_table_storage == HA_SM_DEFAULT)
      old_table_storage= HA_SM_MEMORY;
    if (new_table_storage != old_table_storage)
    {
      table_storage_changed= true;
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
  {
    DBUG_PRINT("info", ("Reorganize partitions"));
    /*
      sql_partition.cc tries to compute what is going on
      and sets flags...that we clear
    */
    if (part_info->use_default_num_partitions)
    {
      DBUG_PRINT("info", ("Using default number of partitions, "
                          "clear some flags"));
      alter_flags= alter_flags & ~Alter_inplace_info::COALESCE_PARTITION;
      alter_flags= alter_flags & ~Alter_inplace_info::ADD_PARTITION;
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_COLUMN_DEFAULT &&
      !(alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN))
  {
    DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                    "Altering default value is not supported"));
  }

  if (alter_flags & not_supported)
  {
    if (alter_info->requested_algorithm ==
        Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ALTER_INFO,
                          "Detected unsupported change: "
                          "HA_ALTER_FLAGS = 0x%llx",
                          alter_flags & not_supported);
    DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                    "Detected unsupported change"));
  }

  if (alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN ||
      alter_flags & Alter_inplace_info::ADD_PARTITION ||
      alter_flags & Alter_inplace_info::ALTER_TABLE_REORG ||
      max_rows_changed ||
      comment_changed)
  {
     Ndb *ndb= get_ndb(thd);
     NDBDICT *dict= ndb->getDictionary();
     ndb->setDatabaseName(m_dbname);
     NdbDictionary::Table new_tab= *old_tab;

     result= HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
     if (alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN)
     {
       NDBCOL col;

       /*
         Check that we are only adding columns
       */
       /*
         HA_COLUMN_DEFAULT_VALUE & HA_COLUMN_STORAGE & HA_COLUMN_FORMAT
         are set if they are specified in an later cmd
         even if they're no change. This is probably a bug
         conclusion: add them to add_column-mask, so that we silently "accept" them
         In case of someone trying to change a column, the HA_CHANGE_COLUMN would be set
         which we don't support, so we will still return HA_ALTER_NOT_SUPPORTED in those cases
       */
       add_column|= Alter_inplace_info::ALTER_COLUMN_DEFAULT;
       add_column|= Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE;
       add_column|= Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT;
       if (alter_flags & ~add_column)
       {
         DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                         "Only add column exclusively can be "
                                         "performed online"));
       }
       /*
         Check for extra fields for hidden primary key
         or user defined partitioning
       */
       if (table_share->primary_key == MAX_KEY ||
           part_info->part_type != partition_type::HASH ||
           !part_info->list_of_part_fields)
       {
         DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                         "Found hidden primary key or "
                                         "user defined partitioning"));
       }

       /* Find the new fields */
       for (uint i= table->s->fields; i < altered_table->s->fields; i++)
       {
         Field *field= altered_table->field[i];
         if(field->is_virtual_gcol())
         {
           DBUG_PRINT("info", ("Field %s is VIRTUAL; not adding.", field->field_name));
           continue;
         }
         DBUG_PRINT("info", ("Found new field %s", field->field_name));
         DBUG_PRINT("info", ("storage_type %i, column_format %i",
                             (uint) field->field_storage_type(),
                             (uint) field->column_format()));
         if (!(field->flags & NO_DEFAULT_VALUE_FLAG))
         {
           my_ptrdiff_t src_offset= field->table->s->default_values 
             - field->table->record[0];
           if ((! field->is_real_null(src_offset)) ||
               ((field->flags & NOT_NULL_FLAG)))
           {
             DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                             "Adding column with non-null default value "
                                             "is not supported online"));
           }
         }
         /* Create new field to check if it can be added */
         const int create_column_result =
             create_ndb_column(thd, col, field, create_info,
                               true /* use_dynamic_as_default */);
         if (create_column_result)
         {
           DBUG_PRINT("info", ("Failed to create NDB column, error %d",
                               create_column_result));
           DBUG_RETURN(HA_ALTER_ERROR);
         }
         if (new_tab.addColumn(col))
         {
           DBUG_PRINT("info", ("Failed to add NDB column to table"));
           DBUG_RETURN(HA_ALTER_ERROR);
         }
       }
     }

     if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
     {
       const ulonglong curr_max_rows = table_share->max_rows;
       if (curr_max_rows != 0)
       {
         // No inplace REORGANIZE PARTITION for table with MAX_ROWS
         DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                        "REORGANIZE of table "
                                        "with MAX_ROWS"));
       }
       new_tab.setFragmentCount(0);
       new_tab.setFragmentData(0, 0);
     }
     else if (alter_flags & Alter_inplace_info::ADD_PARTITION)
     {
       DBUG_PRINT("info", ("Adding partition (%u)", part_info->num_parts));
       new_tab.setFragmentCount(part_info->num_parts);
       new_tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
       if (new_tab.getFullyReplicated())
       {
         DBUG_PRINT("info", ("Add partition isn't supported on fully"
                             " replicated tables"));
         DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
       }
     }
     if (comment_changed &&
         parse_comment_changes(&new_tab,
                               old_tab,
                               create_info,
                               thd,
                               max_rows_changed))
     {
       DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                       "Unsupported table modifiers"));
     }
     else if (max_rows_changed)
     {
       ulonglong rows= create_info->max_rows;
       uint no_fragments= get_no_fragments(rows);
       uint reported_frags= no_fragments;
       if (adjusted_frag_count(ndb, no_fragments, reported_frags))
       {
         push_warning(current_thd,
                      Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                      "Ndb might have problems storing the max amount "
                      "of rows specified");
       }
       if (reported_frags < old_tab->getFragmentCount())
       {
         DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                         "Online reduction in number of "
                                         "fragments not supported"));
       }
       else if (rows == 0)
       {
         /* Dont support setting MAX_ROWS to 0 inplace */
         DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                         "Setting MAX_ROWS to 0 is "
                                         "not supported online"));
       }
       new_tab.setFragmentCount(reported_frags);
       new_tab.setDefaultNoPartitionsFlag(false);
       new_tab.setFragmentData(0, 0);
       new_tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
     }

     if (dict->supportedAlterTable(*old_tab, new_tab))
     {
       DBUG_PRINT("info", ("Adding column(s) or add/reorganize partition supported online"));
     }
     else
     {
       DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                       "Adding column(s) or add/reorganize partition not supported online"));
     }
  }

  /*
    Check that we are not adding multiple indexes
  */
  if (alter_flags & adding)
  {
    if (((altered_table->s->keys - table->s->keys) != 1) ||
        (alter_flags & dropping))
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                      "Only one index can be added online"));
    }
  }

  /*
    Check that we are not dropping multiple indexes
  */
  if (alter_flags & dropping)
  {
    if (((table->s->keys - altered_table->s->keys) != 1) ||
        (alter_flags & adding))
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                      "Only one index can be dropped online"));
    }
  }

  for (uint i= 0; i < table->s->fields; i++)
  {
    Field *field= table->field[i];
    if(field->is_virtual_gcol())
      continue;
    const NDBCOL *col= m_table_map->getColumn(i);

    NDBCOL new_col;
    create_ndb_column(0, new_col, field, create_info);

    bool index_on_column = false;
    /**
     * Check all indexes to determine if column has index instead of checking
     *   field->flags (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG
     *   since field->flags appears to only be set on first column in
     *   multi-part index
     */
    for (uint j= 0; j<table->s->keys; j++)
    {
      KEY* key_info= table->key_info + j;
      KEY_PART_INFO* key_part= key_info->key_part;
      KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
      for (; key_part != end; key_part++)
      {
        if (key_part->field->field_index == i)
        {
          index_on_column= true;
          j= table->s->keys; // break outer loop
          break;
        }
      }
    }

    if (index_on_column == false && (alter_flags & adding))
    {
      for (uint j= table->s->keys; j<altered_table->s->keys; j++)
      {
        KEY* key_info= altered_table->key_info + j;
        KEY_PART_INFO* key_part= key_info->key_part;
        KEY_PART_INFO* end= key_part+key_info->user_defined_key_parts;
        for (; key_part != end; key_part++)
        {
          if (key_part->field->field_index == i)
          {
            index_on_column= true;
            j= altered_table->s->keys; // break outer loop
            break;
          }
        }
      }
    }

    /**
     * This is a "copy" of code in ::create()
     *   that "auto-converts" columns with keys into memory
     *   (unless storage disk is explicitly added)
     * This is needed to check if getStorageType() == getStorageType() 
     * further down
     */
    if (index_on_column)
    {
      if (field->field_storage_type() == HA_SM_DISK)
      {
        DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                        "Found change of COLUMN_STORAGE to disk (Explicit STORAGE DISK on index column)."));
      }
      new_col.setStorageType(NdbDictionary::Column::StorageTypeMemory);
    }
    else if (field->field_storage_type() == HA_SM_DEFAULT)
    {
      if (table_storage_changed &&
             new_col.getStorageType() != col->getStorageType())
      {
        DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                        "Column storage media is changed due to change in table storage media"));
      }

      /**
       * If user didn't specify any column format, keep old
       *   to make as many alter's as possible online. E.g.:
       *
       *   When an index is created on an implicit disk column,
       *   the column storage is silently converted to memory.
       *   It is not changed back to disk again when the index is
       *   dropped, unless the user explicitly specifies copy algorithm.
       *
       *   Also, keep the storage format Ndb has for BLOB/TEXT columns
       *   since NDB stores BLOB/TEXT < 256 bytes in memory,
       *   irrespective of storage type.
       */
      new_col.setStorageType(col->getStorageType());
    }

    if (col->getStorageType() != new_col.getStorageType())
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                      "Column storage media is changed"));
    }

    if (field->flags & FIELD_IS_RENAMED)
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                      "Field has been renamed, copy table"));
    }

    if ((field->flags & FIELD_IN_ADD_INDEX) &&
        (col->getStorageType() == NdbDictionary::Column::StorageTypeDisk))
    {
      DBUG_RETURN(inplace_unsupported(ha_alter_info,
                                      "Add/drop index not supported for disk "
                                      "stored column"));
    }
  }

  // All unsupported cases should have returned directly
  DBUG_ASSERT(result != HA_ALTER_INPLACE_NOT_SUPPORTED);
  DBUG_PRINT("info", ("Inplace alter is supported"));
  DBUG_RETURN(result);
}

enum_alter_inplace_result
ha_ndbcluster::check_if_supported_inplace_alter(TABLE *altered_table,
                                                Alter_inplace_info *ha_alter_info)
{
  DBUG_ENTER("ha_ndbcluster::check_if_supported_inplace_alter");
  Alter_info *alter_info= ha_alter_info->alter_info;

  enum_alter_inplace_result result=
    check_inplace_alter_supported(altered_table,
                                  ha_alter_info);

  if (result == HA_ALTER_INPLACE_NOT_SUPPORTED)
  {
    /*
      The ALTER TABLE is not supported inplace and will fall back
      to use copying ALTER TABLE. If --ndb-default-column-format is dynamic
      by default, the table was created by an older MySQL version and the
      algorithm for the alter table is not  inplace then then check for
      implicit changes and print warnings.
    */
    if ((opt_ndb_default_column_format == NDB_DEFAULT_COLUMN_FORMAT_DYNAMIC) &&
        (table->s->mysql_version < NDB_VERSION_DYNAMIC_IS_DEFAULT) &&
        (alter_info->requested_algorithm !=
         Alter_info::ALTER_TABLE_ALGORITHM_INPLACE))
    {
      check_implicit_column_format_change(altered_table, ha_alter_info);
    }
  }
  DBUG_RETURN(result);
}

bool
ha_ndbcluster::parse_comment_changes(NdbDictionary::Table *new_tab,
                                     const NdbDictionary::Table *old_tab,
                                     HA_CREATE_INFO *create_info,
                                     THD *thd,
                                     bool & max_rows_changed) const
{
  DBUG_ENTER("ha_ndbcluster::parse_comment_changes");
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix,
                                ndb_table_modifiers);
  if (table_modifiers.loadComment(create_info->comment.str,
                                  create_info->comment.length) == -1)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "%s",
                        table_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");
    DBUG_RETURN(true);
  }
  const NDB_Modifier* mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier* mod_frags = table_modifiers.get("PARTITION_BALANCE");
  const NDB_Modifier* mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier* mod_fully_replicated =
    table_modifiers.get("FULLY_REPLICATED");

  NdbDictionary::Object::PartitionBalance part_bal =
    g_default_partition_balance;
  if (parsePartitionBalance(thd /* for pushing warning */,
                             mod_frags, &part_bal) == false)
  {
    /**
     * unable to parse => modifier which is not found
     */
    mod_frags = table_modifiers.notfound();
  }
  else if (ndbd_support_partition_balance(
            get_thd_ndb(thd)->ndb->getMinDbNodeVersion()) == 0)
  {
    /**
     * NDB_TABLE=PARTITION_BALANCE not supported by data nodes.
     */
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ndbcluster_hton_name,
             "PARTITION_BALANCE not supported by current data node versions");
    DBUG_RETURN(true);
  }
  if (mod_nologging->m_found)
  {
    if (new_tab->getLogging() != (!mod_nologging->m_val_bool))
    {
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               ndbcluster_hton_name,
               "Cannot alter nologging inplace");
      DBUG_RETURN(true);
    }
    new_tab->setLogging(!mod_nologging->m_val_bool);
  }
  if (mod_read_backup->m_found)
  {
    if (ndbd_support_read_backup(
         get_thd_ndb(thd)->ndb->getMinDbNodeVersion()) == 0)
    {
      /**
       * NDB_TABLE=READ_BACKUP not supported by data nodes.
       */
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               ndbcluster_hton_name,
               "READ_BACKUP not supported by current data node versions");
      DBUG_RETURN(true);
    }
    if (old_tab->getFullyReplicated() &&
        (!mod_read_backup->m_val_bool))
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=INPLACE",
               "READ_BACKUP off with FULLY_REPLICATED on",
               "ALGORITHM=COPY");
      DBUG_RETURN(true);
    }
    new_tab->setReadBackupFlag(mod_read_backup->m_val_bool);
  }
  if (mod_fully_replicated->m_found)
  {
    if (ndbd_support_fully_replicated(
         get_thd_ndb(thd)->ndb->getMinDbNodeVersion()) == 0)
    {
      /**
       * NDB_TABLE=FULLY_REPLICATED not supported by data nodes.
       */
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               ndbcluster_hton_name,
         "FULLY_REPLICATED not supported by current data node versions");
      DBUG_RETURN(true);
    }
    if (!old_tab->getFullyReplicated())
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=INPLACE",
               "Turning FULLY_REPLICATED on after create",
               "ALGORITHM=COPY");
      DBUG_RETURN(true);
    }
  }
  /**
   * We will not silently change tables during ALTER TABLE to use read
   * backup or fully replicated. We will only use this configuration
   * variable to affect new tables. For ALTER TABLE one has to set these
   * properties explicitly.
   */
  if (mod_frags->m_found)
  {
    if (max_rows_changed)
    {
      max_rows_changed = false;
    }
    new_tab->setFragmentCount(0);
    new_tab->setFragmentData(0,0);
    new_tab->setPartitionBalance(part_bal);
    DBUG_PRINT("info", ("parse_comment_changes: PartitionBalance: %s",
                        new_tab->getPartitionBalanceString()));
  }
  else
  {
    part_bal = old_tab->getPartitionBalance();
  }
  if (old_tab->getFullyReplicated())
  {
    if (part_bal != old_tab->getPartitionBalance())
    {
      /**
       * We cannot change partition balance inplace for fully
       * replicated tables.
       */
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=INPLACE",
               "Changing PARTITION_BALANCE with FULLY_REPLICATED on",
               "ALGORITHM=COPY");
      DBUG_RETURN(true); /* Error */
    }
    max_rows_changed = false;
  }
  DBUG_RETURN(false);
}


/**
   Updates the internal structures and prepares them for the inplace alter.

   @note Function is responsible for reporting any errors by
   calling my_error()/print_error()

   @param    altered_table     TABLE object for new version of table.
   @param    ha_alter_info     Structure describing changes to be done
                               by ALTER TABLE and holding data used
                               during in-place alter.

   @retval   true              Error
   @retval   false             Success
*/
bool
ha_ndbcluster::prepare_inplace_alter_table(TABLE *altered_table,
                                           Alter_inplace_info *ha_alter_info,
                                           const dd::Table *, dd::Table *)
{
  int error= 0;
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= get_ndb(thd);
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(m_dbname);

  HA_CREATE_INFO *create_info= ha_alter_info->create_info;

  const Alter_inplace_info::HA_ALTER_FLAGS alter_flags=
    ha_alter_info->handler_flags;

  const Alter_inplace_info::HA_ALTER_FLAGS adding=
    Alter_inplace_info::ADD_INDEX |
    Alter_inplace_info::ADD_UNIQUE_INDEX;

  const Alter_inplace_info::HA_ALTER_FLAGS dropping=
    Alter_inplace_info::DROP_INDEX |
    Alter_inplace_info::DROP_UNIQUE_INDEX;

  DBUG_ENTER("ha_ndbcluster::prepare_inplace_alter_table");

  ha_alter_info->handler_ctx= 0;
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::prepare_inplace_alter_table"))
    DBUG_RETURN(true);

  NDB_ALTER_DATA *alter_data;
  if (!(alter_data= new (*THR_MALLOC) NDB_ALTER_DATA(thd, dict, m_table)))
    DBUG_RETURN(true);

  if (!alter_data->schema_dist_client.prepare(m_dbname, m_tabname))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  const NDBTAB* const old_tab = alter_data->old_table;
  NdbDictionary::Table * const new_tab = alter_data->new_table;
  ha_alter_info->handler_ctx= alter_data;

  DBUG_PRINT("info", ("altered_table: '%s, alter_flags: 0x%llx",
                      altered_table->s->table_name.str,
                      alter_flags));

  bool max_rows_changed= false;
  bool comment_changed = false;
  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS)
      max_rows_changed= true;
    if (create_info->used_fields & HA_CREATE_USED_COMMENT)
    {
      DBUG_PRINT("info", ("The COMMENT string changed"));
      comment_changed= true;
    }
  }

  // Pin the NDB_SHARE of the altered table
  NDB_SHARE::acquire_reference_on_existing(m_share,
                                           "inplace_alter");

  if (dict->beginSchemaTrans() == -1)
  {
    DBUG_PRINT("info", ("Failed to start schema transaction"));
    ERR_PRINT(dict->getNdbError());
    ndb_my_error(&dict->getNdbError());
    goto err;
  }

  if (alter_flags & adding)
  {
    KEY           *key_info;
    KEY           *key;
    uint          *idx_p;
    uint          *idx_end_p;
    KEY_PART_INFO *key_part;
    KEY_PART_INFO *part_end;
    DBUG_PRINT("info", ("Adding indexes"));
    key_info= (KEY*) thd->alloc(sizeof(KEY) * ha_alter_info->index_add_count);
    key= key_info;
    for (idx_p=  ha_alter_info->index_add_buffer,
	 idx_end_p= idx_p + ha_alter_info->index_add_count;
	 idx_p < idx_end_p;
	 idx_p++, key++)
    {
      /* Copy the KEY struct. */
      *key= ha_alter_info->key_info_buffer[*idx_p];
      /* Fix the key parts. */
      part_end= key->key_part + key->user_defined_key_parts;
      for (key_part= key->key_part; key_part < part_end; key_part++)
	key_part->field= table->field[key_part->fieldnr];
    }
    if ((error = prepare_inplace__add_index(thd, key_info,
                                            ha_alter_info->index_add_count)))
    {
      /*
	Exchange the key_info for the error message. If we exchange
	key number by key name in the message later, we need correct info.
      */
      KEY *save_key_info= table->key_info;
      table->key_info= key_info;
      table->file->print_error(error, MYF(0));
      table->key_info= save_key_info;
      goto abort;
    }
  }

  if (alter_flags & dropping)
  {
    for (uint i =0; i < ha_alter_info->index_drop_count; i++)
    {
      const KEY* key_ptr = ha_alter_info->index_drop_buffer[i];
      for(uint key_num=0; key_num < table->s->keys; key_num++)
      {
        /*
           Find the key_num of the key to be dropped and
           mark it as dropped
        */
        if (key_ptr == table->key_info + key_num)
        {
          prepare_inplace__drop_index(key_num);
          break;
        }
      }
    }
  }

  if (alter_flags &  Alter_inplace_info::ADD_STORED_BASE_COLUMN)
  {
     NDBCOL col;

     /* Find the new fields */
     for (uint i = table->s->fields; i < altered_table->s->fields; i++)
     {
       Field *field= altered_table->field[i];
       if(! field->stored_in_db)
         continue;

       DBUG_PRINT("info", ("Found new field %s", field->field_name));
       if (create_ndb_column(thd, col, field, create_info,
                             true /* use_dynamic_as_default */) != 0)
       {
         // Failed to create column in NDB
         goto abort;
       }

       /*
         If the user has not specified the field format
         make it dynamic to enable online add attribute
       */
       if (field->column_format() == COLUMN_FORMAT_TYPE_DEFAULT &&
           create_info->row_type == ROW_TYPE_DEFAULT &&
           col.getDynamic())
       {
         push_warning_printf(thd, Sql_condition::SL_WARNING,
                             ER_ILLEGAL_HA_CREATE_OPTION,
                             "Converted FIXED field '%s' to DYNAMIC "
                             "to enable online ADD COLUMN",
                             field->field_name);
       }
       new_tab->addColumn(col);
     }
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG ||
      alter_flags & Alter_inplace_info::ADD_PARTITION ||
      max_rows_changed ||
      comment_changed)
  {
    if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
    {
      new_tab->setFragmentCount(0);
      new_tab->setFragmentData(0, 0);
    }
    else if (alter_flags & Alter_inplace_info::ADD_PARTITION)
    {
      partition_info *part_info= altered_table->part_info;
      DBUG_PRINT("info", ("Adding partition (%u)", part_info->num_parts));
      new_tab->setFragmentCount(part_info->num_parts);
      new_tab->setPartitionBalance(
        NdbDictionary::Object::PartitionBalance_Specific);
    }
    else if (comment_changed &&
             parse_comment_changes(new_tab,
                                   old_tab,
                                   create_info,
                                   thd,
                                   max_rows_changed))
    {
      goto abort;
    }
    else if (max_rows_changed)
    {
      ulonglong rows= create_info->max_rows;
      uint no_fragments= get_no_fragments(rows);
      uint reported_frags= no_fragments;
      if (adjusted_frag_count(ndb, no_fragments, reported_frags))
      {
        DBUG_ASSERT(false); /* Checked above */
      }
      if (reported_frags < old_tab->getFragmentCount())
      {
        DBUG_ASSERT(false);
        DBUG_RETURN(false);
      }
      /* Note we don't set the ndb table's max_rows param, as that 
       * is considered a 'real' change
       */
      //new_tab->setMaxRows(create_info->max_rows);
      new_tab->setFragmentCount(reported_frags);
      new_tab->setDefaultNoPartitionsFlag(false);
      new_tab->setFragmentData(0, 0);
      new_tab->setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
    }
    
    int res= dict->prepareHashMap(*old_tab, *new_tab);
    if (res == -1)
    {
      ndb_my_error(&dict->getNdbError());
      goto abort;
    }
  }

  if (alter_flags & Alter_inplace_info::ADD_FOREIGN_KEY)
  {
    const int create_fks_result = create_fks(thd, ndb);
    if (create_fks_result != 0)
    {
      table->file->print_error(create_fks_result, MYF(0));
      goto abort;
    }
  }

  DBUG_RETURN(false);
abort:
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
  {
    DBUG_PRINT("info", ("Failed to abort schema transaction"));
    ERR_PRINT(dict->getNdbError());
    error= ndb_to_mysql_error(&dict->getNdbError());
  }

err:
  DBUG_RETURN(true);
}


static
int
inplace__set_sdi_and_alter_in_ndb(THD *thd,
                                  const NDB_ALTER_DATA* alter_data,
                                  dd::Table* new_table_def,
                                  const char* schema_name)
{
  DBUG_ENTER("inplace__set_sdi_and_alter_in_ndb");

  ndb_dd_fix_inplace_alter_table_def(new_table_def,
                                     alter_data->old_table->getName());

  dd::sdi_t sdi;
  if (!ndb_sdi_serialize(thd, new_table_def, schema_name, sdi))
  {
    DBUG_RETURN(1);
  }


  NdbDictionary::Table* new_tab= alter_data->new_table;
  const int set_result =
      new_tab->setExtraMetadata(2, // version 2 for frm
                                sdi.c_str(), (Uint32)sdi.length());
  if (set_result != 0)
  {
    my_printf_error(ER_GET_ERRMSG,
                    "Failed to set extra metadata during"
                    "inplace alter table, error: %d",
                    MYF(0), set_result);
    DBUG_RETURN(2);
  }

  NdbDictionary::Dictionary* dict= alter_data->dictionary;
  if (dict->alterTableGlobal(*alter_data->old_table, *new_tab))
  {
    DBUG_PRINT("info", ("Inplace alter of table %s failed",
                        new_tab->getName()));
    const NdbError ndberr= dict->getNdbError();
    const int error= ndb_to_mysql_error(&ndberr);
    my_error(ER_GET_ERRMSG, MYF(0), error, ndberr.message, "NDBCLUSTER");
    DBUG_RETURN(error);
  }


  DBUG_RETURN(0);
}

bool ha_ndbcluster::inplace_alter_table(TABLE *,
                                        Alter_inplace_info *ha_alter_info,
                                        const dd::Table *,
                                        dd::Table *new_table_def) {
  DBUG_ENTER("ha_ndbcluster::inplace_alter_table");
  int error= 0;
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  NDBDICT *dict= alter_data->dictionary;
  const Alter_inplace_info::HA_ALTER_FLAGS alter_flags=
    ha_alter_info->handler_flags;
  const Alter_inplace_info::HA_ALTER_FLAGS dropping=
    Alter_inplace_info::DROP_INDEX |
    Alter_inplace_info::DROP_UNIQUE_INDEX;

  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::inplace_alter_table"))
  {
    DBUG_RETURN(true);
  }

  bool auto_increment_value_changed= false;
  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    if (create_info->auto_increment_value !=
      table->file->stats.auto_increment_value)
      auto_increment_value_changed= true;
  }

  if (alter_flags & dropping)
  {
    /* Tell the handler to finally drop the indexes. */
    if ((error= inplace__final_drop_index(table)))
    {
      print_error(error, MYF(0));
      goto abort;
    }
  }

  if (alter_flags & Alter_inplace_info::DROP_FOREIGN_KEY)
  {
    const NDBTAB* tab= alter_data->old_table;
    if ((error= inplace__drop_fks(thd, thd_ndb->ndb, dict, tab)) != 0)
    {
      print_error(error, MYF(0));
      goto abort;
    }
  }

  DBUG_ASSERT(m_table != 0);

  error= inplace__set_sdi_and_alter_in_ndb(thd, alter_data,
                                           new_table_def, m_dbname);
  if (!error)
  {
    /*
     * Alter succesful, commit schema transaction
     */
    if (dict->endSchemaTrans() == -1)
    {
      error= ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info", ("Failed to commit schema transaction, error %u",
                          error));
      table->file->print_error(error, MYF(0));
      goto err;
    }
    if (auto_increment_value_changed)
      error= set_auto_inc_val(thd, create_info->auto_increment_value);
    if (error)
    {
      DBUG_PRINT("info", ("Failed to set auto_increment value"));
      goto err;
    }
  }
  else // if (error)
  {
abort:
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
    {
      DBUG_PRINT("info", ("Failed to abort schema transaction"));
      ERR_PRINT(dict->getNdbError());
    }
  }

err:
  DBUG_RETURN(error ? true : false);
}

bool
ha_ndbcluster::commit_inplace_alter_table(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info,
                                          bool commit,
                                          const dd::Table*,
                                          dd::Table* new_table_def)
{
  DBUG_ENTER("ha_ndbcluster::commit_inplace_alter_table");

  if (!commit)
    DBUG_RETURN(abort_inplace_alter_table(altered_table,
                                          ha_alter_info));
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::commit_inplace_alter_table"))
  {
    DBUG_RETURN(true); // Error
  }

  const char *db= table->s->db.str;
  const char *name= table->s->table_name.str;
  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  const Uint32 table_id= alter_data->table_id;
  const Uint32 table_version= alter_data->old_table_version;

  // Pass pointer to table_def for usage by schema dist participant
  // in the binlog thread of this mysqld.
  m_share->inplace_alter_new_table_def = new_table_def;

  Ndb_schema_dist_client &schema_dist_client = alter_data->schema_dist_client;
  if (!schema_dist_client.alter_table_inplace_prepare(db, name, table_id,
                                                      table_version))
  {
    // Failed to distribute the prepare of this alter table to the
    // other MySQL Servers, just log error and continue
    ndb_log_error("Failed to distribute inplace alter table prepare for '%s'",
                  name);
    DBUG_ASSERT(false);  // Catch in debug
  }

  // The pointer to new table_def is not valid anymore
  m_share->inplace_alter_new_table_def = nullptr;

  // Fetch the new table version and write it to the table definition,
  // the caller will then save it into DD
  {
    Ndb_table_guard ndbtab_g(alter_data->dictionary, name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();

    // The id should still be the same as before the alter
    DBUG_ASSERT((Uint32)ndbtab->getObjectId() == table_id);
    // The version should have been changed by the alter
    DBUG_ASSERT((Uint32)ndbtab->getObjectVersion() != table_version);

    ndb_dd_table_set_object_id_and_version(new_table_def,
                                           table_id,
                                           ndbtab->getObjectVersion());
  }

  // Unpin the NDB_SHARE of the altered table
  NDB_SHARE::release_reference(m_share, "inplace_alter");

  DBUG_RETURN(false); // OK
}

bool ha_ndbcluster::abort_inplace_alter_table(TABLE *,
                                              Alter_inplace_info *ha_alter_info)
{
  DBUG_ENTER("ha_ndbcluster::abort_inplace_alter_table");

  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  if (!alter_data)
  {
    // Could not find any alter_data, nothing to abort or already aborted
    DBUG_RETURN(false); // OK
  }

  NDBDICT *dict= alter_data->dictionary;
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort) == -1)
  {
    DBUG_PRINT("info", ("Failed to abort schema transaction"));
    ERR_PRINT(dict->getNdbError());
  }

  // NOTE! There is nothing informing participants that the prepared
  // schema distribution has been aborted

  destroy(alter_data);
  ha_alter_info->handler_ctx= 0;

  // Unpin the NDB_SHARE of the altered table
  NDB_SHARE::release_reference(m_share, "inplace_alter");

  DBUG_RETURN(false); // OK
}

void ha_ndbcluster::notify_table_changed(Alter_inplace_info *alter_info)
{
  DBUG_ENTER("ha_ndbcluster::notify_table_changed ");

  /*
    all mysqld's will read frms from disk and setup new
    event operation for the table (new_op)
  */
  THD *thd= current_thd;
  const char *db= table->s->db.str;
  const char *name= table->s->table_name.str;
  uint32 table_id= 0, table_version= 0;

  /*
    Get table id/version for new table
  */
  {
    Ndb* ndb= get_ndb(thd);
    DBUG_ASSERT(ndb != 0);
    if (ndb)
    {
      ndb->setDatabaseName(db);
      Ndb_table_guard ndbtab(ndb->getDictionary(), name);
      const NDBTAB *new_tab= ndbtab.get_table();
      DBUG_ASSERT(new_tab != 0);
      if (new_tab)
      {
        table_id= new_tab->getObjectId();
        table_version= new_tab->getObjectVersion();
        // NOTE! There is already table id, version etc. in NDB_ALTER_DATA,
        // why not take it from there instead of doing an additional
        // NDB roundtrip to fetch the table definition
      }
    }
  }

  /*
    all mysqld's will switch to using the new_op, and delete the old
    event operation
  */
  NDB_ALTER_DATA *alter_data =
      static_cast<NDB_ALTER_DATA *>(alter_info->handler_ctx);
  Ndb_schema_dist_client &schema_dist_client = alter_data->schema_dist_client;
  if (!schema_dist_client.alter_table_inplace_commit(db, name, table_id,
                                                     table_version))
  {
    // Failed to distribute the prepare of this alter table to the
    // other MySQL Servers, just log error and continue
    ndb_log_error("Failed to distribute inplace alter table commit of '%s'",
                  name);
  }

  destroy(alter_data);
  alter_info->handler_ctx= 0;

  DBUG_VOID_RETURN;
}

static
bool set_up_tablespace(st_alter_tablespace *alter_info,
                       NdbDictionary::Tablespace *ndb_ts)
{
  if (alter_info->extent_size >= (Uint64(1) << 32))
  {
    // TODO set correct error
    return true;
  }
  ndb_ts->setName(alter_info->tablespace_name);
  ndb_ts->setExtentSize(Uint32(alter_info->extent_size));
  ndb_ts->setDefaultLogfileGroup(alter_info->logfile_group_name);
  return false;
}

static
bool set_up_datafile(st_alter_tablespace *alter_info,
                     NdbDictionary::Datafile *ndb_df)
{
  if (alter_info->max_size > 0)
  {
    my_error(ER_TABLESPACE_AUTO_EXTEND_ERROR, MYF(0));
    return true;
  }
  ndb_df->setPath(alter_info->data_file_name);
  ndb_df->setSize(alter_info->initial_size);
  ndb_df->setTablespace(alter_info->tablespace_name);
  return false;
}

static
bool set_up_logfile_group(st_alter_tablespace *alter_info,
                          NdbDictionary::LogfileGroup *ndb_lg)
{
  if (alter_info->undo_buffer_size >= (Uint64(1) << 32))
  {
    // TODO set correct error
    return true;
  }

  ndb_lg->setName(alter_info->logfile_group_name);
  ndb_lg->setUndoBufferSize(Uint32(alter_info->undo_buffer_size));
  return false;
}

static
bool set_up_undofile(st_alter_tablespace *alter_info,
                     NdbDictionary::Undofile *ndb_uf)
{
  ndb_uf->setPath(alter_info->undo_file_name);
  ndb_uf->setSize(alter_info->initial_size);
  ndb_uf->setLogfileGroup(alter_info->logfile_group_name);
  return false;
}


/**
  Get the tablespace name from the NDB dictionary for the given table in the
  given schema.

  @note For NDB tables with version before 50120, the server must ask the
        SE for the tablespace name, because for these tables, the tablespace
        name is not stored in the .FRM file, but only within the SE itself.

  @note The function is essentially doing the same as the corresponding code
        block in the function 'get_metadata()', except for the handling of
        empty strings, which are in this case returned as "" rather than NULL.

  @param       thd              Thread context.
  @param       db_name          Name of the relevant schema.
  @param       table_name       Name of the relevant table.
  @param [out] tablespace_name  Name of the tablespace containing the table.

  @return Operation status.
    @retval == 0  Success.
    @retval != 0  Error (handler error code returned).
 */

static
int ndbcluster_get_tablespace(THD* thd,
                              LEX_CSTRING db_name,
                              LEX_CSTRING table_name,
                              LEX_CSTRING *tablespace_name)
{
  DBUG_ENTER("ndbcluster_get_tablespace");
  DBUG_PRINT("enter", ("db_name: %s, table_name: %s", db_name.str,
             table_name.str));
  DBUG_ASSERT(tablespace_name != NULL);

  Ndb* ndb= check_ndb_in_thd(thd);
  if (ndb == NULL)
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *tab= NULL;

  ndb->setDatabaseName(db_name.str);
  Ndb_table_guard ndbtab_g(dict, table_name.str);
  if (!(tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

  Uint32 id;
  if (tab->getTablespace(&id))
  {
    NdbDictionary::Tablespace ts= dict->getTablespace(id);
    NdbError ndberr= dict->getNdbError();
    if (ndberr.classification == NdbError::NoError)
    {
      const char *tablespace= ts.getName();
      DBUG_ASSERT(tablespace);
      const size_t tablespace_len= strlen(tablespace);
      DBUG_PRINT("info", ("Found tablespace '%s'", tablespace));
      thd->make_lex_string(tablespace_name, tablespace, tablespace_len, false);
    }
  }

  DBUG_RETURN(0);
}


/**
  Create/drop or alter tablespace or logfile group

  @param          hton        Hadlerton of the SE.
  @param          thd         Thread context.
  @param          ts_info     Description of tablespace and specific
                              operation on it.
  @param          old_ts_def  dd::Tablespace object describing old version
                              of tablespace.
  @param [in,out] new_ts_def  dd::Tablespace object describing new version
                              of tablespace. Engines which support atomic DDL
                              can adjust this object. The updated information
                              will be saved to the data-dictionary.

  @return Operation status.
    @retval == 0  Success.
    @retval != 0  Error, only a subset of handler error codes (i.e those
                  that start with HA_) can be returned. Special case seems
                  to be 1 which is to be used when my_error() already has
                  been called to set the MySQL error code.

  @note There are many places in this function which return 1 without
        calling my_error() first.
*/

static
int ndbcluster_alter_tablespace(handlerton*,
                                THD* thd, st_alter_tablespace *alter_info,
                                const dd::Tablespace*,
                                dd::Tablespace* new_ts_def)
{
  NdbError err;
  int error;
  const char *errmsg= NULL;
  DBUG_ENTER("ndbcluster_alter_tablespace");

  Ndb* ndb= check_ndb_in_thd(thd);
  if (ndb == NULL)
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  NdbDictionary::Dictionary* dict= ndb->getDictionary();

  Ndb_schema_dist_client schema_dist_client(thd);

  bool is_tablespace= false;
  int object_id= 0;
  int object_version= 0;
  switch (alter_info->ts_cmd_type){
  case (CREATE_TABLESPACE):
  {
    error= ER_CREATE_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->tablespace_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
    
    NdbDictionary::Tablespace ndb_ts;
    NdbDictionary::Datafile ndb_df;
    NdbDictionary::ObjectId objid;
    if (set_up_tablespace(alter_info, &ndb_ts))
    {
      DBUG_RETURN(1);
    }
    if (set_up_datafile(alter_info, &ndb_df))
    {
      DBUG_RETURN(1);
    }
    errmsg= "TABLESPACE";
    if (dict->createTablespace(ndb_ts, &objid))
    {
      DBUG_PRINT("error", ("createTablespace returned %d", error));
      goto ndberror;
    }
    object_id = objid.getObjectId();
    object_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnExtentRoundUp)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Extent size rounded up to kernel page size");
    }
    DBUG_PRINT("alter_info", ("Successfully created Tablespace"));
    errmsg= "DATAFILE";
    if (dict->createDatafile(ndb_df))
    {
      err= dict->getNdbError();
      NdbDictionary::Tablespace tmp= dict->getTablespace(ndb_ts.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropTablespace(tmp);
      }
      
      DBUG_PRINT("error", ("createDatafile returned %d", error));
      goto ndberror2;
    }
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnDatafileRoundUp)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Datafile size rounded up to extent size");
    }
    else /* produce only 1 message */
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnDatafileRoundDown)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Datafile size rounded down to extent size");
    }
    is_tablespace = true;

    /*
     * Set se_private_data for the tablespace. This is required later
     * in order to populate the I_S.FILES table
     */

    ndb_dd_disk_data_set_object_type(new_ts_def, object_type::TABLESPACE);

    break;
  }
  case (ALTER_TABLESPACE):
  {
    error= ER_ALTER_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->tablespace_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    if (alter_info->ts_alter_tablespace_type == ALTER_TABLESPACE_ADD_FILE)
    {
      NdbDictionary::Datafile ndb_df;
      if (set_up_datafile(alter_info, &ndb_df))
      {
	DBUG_RETURN(1);
      }
      errmsg= " CREATE DATAFILE";
      NdbDictionary::ObjectId objid;
      if (dict->createDatafile(ndb_df, false, &objid))
      {
	goto ndberror;
      }
      object_id = objid.getObjectId();
      object_version = objid.getObjectVersion();
      if (dict->getWarningFlags() &
          NdbDictionary::Dictionary::WarnDatafileRoundUp)
      {
        push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                            dict->getWarningFlags(),
                            "Datafile size rounded up to extent size");
      }
      else /* produce only 1 message */
      if (dict->getWarningFlags() &
          NdbDictionary::Dictionary::WarnDatafileRoundDown)
      {
        push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                            dict->getWarningFlags(),
                            "Datafile size rounded down to extent size");
      }
    }
    else if(alter_info->ts_alter_tablespace_type == ALTER_TABLESPACE_DROP_FILE)
    {
      NdbDictionary::Tablespace ts= dict->getTablespace(alter_info->tablespace_name);
      NdbDictionary::Datafile df= dict->getDatafile(0, alter_info->data_file_name);
      const NdbError ndberr= dict->getNdbError();
      if(ndberr.classification != NdbError::NoError)
      {
        errmsg = " NO SUCH FILE"; // mapping all errors to "NO SUCH FILE"
        goto ndberror;
      }

      NdbDictionary::ObjectId objid;
      df.getTablespaceId(&objid);
      object_id = df.getObjectId();
      object_version = df.getObjectVersion();
      if (ts.getObjectId() == objid.getObjectId() && 
	  strcmp(df.getPath(), alter_info->data_file_name) == 0)
      {
	errmsg= " DROP DATAFILE";
	if (dict->dropDatafile(df))
	{
	  goto ndberror;
	}
      }
      else
      {
	DBUG_PRINT("error", ("No such datafile"));
	my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), " NO SUCH FILE");
	DBUG_RETURN(1);
      }
    }
    else
    {
      DBUG_PRINT("error", ("Unsupported alter tablespace: %d", 
			   alter_info->ts_alter_tablespace_type));
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    is_tablespace = true;
    break;
  }
  case (CREATE_LOGFILE_GROUP):
  {
    error= ER_CREATE_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->logfile_group_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    NdbDictionary::LogfileGroup ndb_lg;
    NdbDictionary::Undofile ndb_uf;
    NdbDictionary::ObjectId objid;
    if (alter_info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    if (set_up_logfile_group(alter_info, &ndb_lg))
    {
      DBUG_RETURN(1);
    }
    errmsg= "LOGFILE GROUP";
    if (dict->createLogfileGroup(ndb_lg, &objid))
    {
      goto ndberror;
    }
    object_id = objid.getObjectId();
    object_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnUndobufferRoundUp)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Undo buffer size rounded up to kernel page size");
    }
    DBUG_PRINT("alter_info", ("Successfully created Logfile Group"));
    if (set_up_undofile(alter_info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "UNDOFILE";
    if (dict->createUndofile(ndb_uf))
    {
      err= dict->getNdbError();
      NdbDictionary::LogfileGroup tmp= dict->getLogfileGroup(ndb_lg.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropLogfileGroup(tmp);
      }
      goto ndberror2;
    }

    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnUndofileRoundDown)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Undofile size rounded down to kernel page size");
    }

    /*
     * Add Logfile Group entry to the DD as a tablespace. This
     * is to ensure that it is propagated to INFORMATION_SCHEMA
     * since I_S is now a database view over the DD tables.
     *
     * NOTE: The information stored will only be used by I_S
     * subsystem and not by NDB. Thus, any failure in adding
     * the entry to DD is not treated as an error. Instead, a
     * warning is returned and we continue as if nothing
     * went wrong. Problem with this of course is the fact that
     * I_S will have missing entries.
     */

    Ndb_dd_client dd_client(thd);
    const char* logfile_group_name= alter_info->logfile_group_name;
    const char* undo_file_name= alter_info->undo_file_name;

    // Acquire MDL locks on logfile group and add entry to DD
    const bool lock_logfile_group_result=
        dd_client.mdl_lock_logfile_group(logfile_group_name);
    if (!lock_logfile_group_result)
    {
      DBUG_PRINT("warning",("MDL lock could not be acquired for "
                            "logfile_group %s", logfile_group_name));
      DBUG_ASSERT(false);
      break;
    }

    const bool install_logfile_group_result=
        dd_client.install_logfile_group(logfile_group_name,
                                        undo_file_name);
    if (!install_logfile_group_result)
    {
      DBUG_PRINT("warning",("Logfile Group %s could not be stored in DD",
                            logfile_group_name));
      DBUG_ASSERT(false);
      break;
    }

    // All okay, commit to DD
    dd_client.commit();
    break;
  }
  case (ALTER_LOGFILE_GROUP):
  {
    error= ER_ALTER_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->logfile_group_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    if (alter_info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    NdbDictionary::Undofile ndb_uf;
    if (set_up_undofile(alter_info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "CREATE UNDOFILE";
    NdbDictionary::ObjectId objid;
    if (dict->createUndofile(ndb_uf, false, &objid))
    {
      goto ndberror;
    }
    object_id = objid.getObjectId();
    object_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnUndofileRoundDown)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          dict->getWarningFlags(),
                          "Undofile size rounded down to kernel page size");
    }

    /*
     * Update Logfile Group entry in the DD.
     *
     * NOTE: The information stored will only be used by I_S
     * subsystem and not by NDB. Thus, any failure in adding
     * the entry to DD is not treated as an error. Instead, a
     * warning is returned and we continue as if nothing
     * went wrong. Problem with this of course is the fact that
     * I_S will have missing entries.
     */

    Ndb_dd_client dd_client(thd);
    const char* logfile_group_name= alter_info->logfile_group_name;
    const char* undo_file_name= alter_info->undo_file_name;

    // Acquire MDL locks on logfile group and modify DD entry
    const bool lock_logfile_group_result=
        dd_client.mdl_lock_logfile_group(logfile_group_name);
    if (!lock_logfile_group_result)
    {
      DBUG_PRINT("warning",("MDL lock could not be acquired for "
                            "logfile_group %s", logfile_group_name));
      DBUG_ASSERT(false);
      break;
    }

    const bool install_undo_file_result=
        dd_client.install_undo_file(logfile_group_name,
                                    undo_file_name);
    if (!install_undo_file_result)
    {
      DBUG_PRINT("warning",("Undo file %s could not be added to logfile "
                            "group %s", logfile_group_name,
                            undo_file_name));
      DBUG_ASSERT(false);
      break;
    }

    // All okay, commit to DD
    dd_client.commit();
    break;
  }
  case (DROP_TABLESPACE):
  {
    error= ER_DROP_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->tablespace_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    errmsg= "TABLESPACE";
    NdbDictionary::Tablespace ts=
      dict->getTablespace(alter_info->tablespace_name);
    object_id = ts.getObjectId();
    object_version = ts.getObjectVersion();
    if (dict->dropTablespace(ts))
    {
      goto ndberror;
    }
    is_tablespace = true;
    break;
  }
  case (DROP_LOGFILE_GROUP):
  {
    error= ER_DROP_FILEGROUP_FAILED;

    if (!schema_dist_client.prepare("", alter_info->logfile_group_name))
    {
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }

    errmsg= "LOGFILE GROUP";
    NdbDictionary::LogfileGroup lg=
      dict->getLogfileGroup(alter_info->logfile_group_name);
    object_id = lg.getObjectId();
    object_version = lg.getObjectVersion();
    if (dict->dropLogfileGroup(lg))
    {
      goto ndberror;
    }

    /*
     * Drop Logfile Group entry from the DD.
     *
     * NOTE: The information stored will only be used by I_S
     * subsystem and not by NDB. Thus, any failure in adding
     * the entry to DD is not treated as an error. Instead, a
     * warning is returned and we continue as if nothing
     * went wrong. Problem with this of course is the fact that
     * I_S will have missing entries.
     */

    Ndb_dd_client dd_client(thd);
    const char* logfile_group_name= alter_info->logfile_group_name;

    // Acquire MDL locks on logfile group and modify DD entry
    const bool lock_logfile_group_result=
        dd_client.mdl_lock_logfile_group(logfile_group_name);
    if (!lock_logfile_group_result)
    {
      DBUG_PRINT("warning",("MDL lock could not be acquired for "
                            "logfile_group %s", logfile_group_name));
      DBUG_ASSERT(false);
      break;
    }

    const bool drop_logfile_group_result=
        dd_client.drop_logfile_group(logfile_group_name);

    if (!drop_logfile_group_result)
    {
      DBUG_PRINT("warning",("Logfile group %s could not be dropped from DD",
                            logfile_group_name));
      DBUG_ASSERT(false);
      break;
    }

    // All okay, commit to DD
    dd_client.commit();
    break;
  }
  case (CHANGE_FILE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  case (ALTER_ACCESS_MODE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  default:
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  }
  bool schema_dist_result;
  if (is_tablespace)
  {
    schema_dist_result =
        schema_dist_client.tablespace_changed(alter_info->tablespace_name,
                                              object_id, object_version);
  }
  else
  {
    schema_dist_result =
        schema_dist_client.logfilegroup_changed(alter_info->logfile_group_name,
                                                object_id, object_version);
  }
  if (!schema_dist_result)
  {
    // Although it's possible to return an error here that's
    // not the tradition, just log an error and continue
    ndb_log_error("Failed to distribute '%s'", errmsg);
  }
  DBUG_RETURN(0);

ndberror:
  err= dict->getNdbError();
ndberror2:
  ndb_to_mysql_error(&err);
  
  my_error(error, MYF(0), errmsg);
  DBUG_RETURN(1); // Error, my_error called
}

/**
  Retrieve ha_tablespace_statistics for tablespace or logfile group

  @param        tablespace_name       Name of the tablespace/logfile group
  @param        file_name             Name of the datafile/undo log file
  @param        ts_se_private_data    Tablespace/logfile group SE private data
  @param [out]  stats                 Contains tablespace/logfile group
                                      statistics read from SE

  @returns false on success, true on failure
*/

static
bool ndbcluster_get_tablespace_statistics(const char *tablespace_name,
                                          const char *file_name,
                                          const dd::Properties &ts_se_private_data,
                                          ha_tablespace_statistics *stats)
{
  DBUG_ENTER("ndbcluster_get_tablespace_statistics");

  // Find out type of object. The type is stored in se_private_data
  enum object_type type;

  ndb_dd_disk_data_get_object_type(ts_se_private_data, type);

  if (type == object_type::LOGFILE_GROUP)
  {
    THD *thd= current_thd;

    Ndb *ndb= check_ndb_in_thd(thd);
    if (!ndb)
    {
      // No connection to NDB
      my_error(HA_ERR_NO_CONNECTION, MYF(0));
      DBUG_RETURN(true);
    }

    NdbDictionary::Dictionary* dict= ndb->getDictionary();

    /* Find a node which is alive. NDB's view of an undo file
     * is actually a composite of the stats found across all
     * data nodes. However this does not fit well with the
     * server's view which thinks of it as a single file.
     * Since the stats of interest don't vary across the data
     * nodes, using the first available data node is acceptable.
     */
    NdbDictionary::Undofile uf= dict->getUndofile(-1, file_name);
    if (dict->getNdbError().classification != NdbError::NoError)
    {
      ndb_my_error(&dict->getNdbError());
      DBUG_RETURN(true);
    }

    NdbDictionary::LogfileGroup lfg=
      dict->getLogfileGroup(uf.getLogfileGroup());
    if (dict->getNdbError().classification != NdbError::NoError)
    {
      ndb_my_error(&dict->getNdbError());
      DBUG_RETURN(true);
    }

    /* Check if logfile group name matches tablespace name.
     * Failure means that the NDB dictionary has gone out
     * of sync with the DD
     */
    if (strcmp(lfg.getName(), tablespace_name) != 0)
    {
      my_error(ER_TABLESPACE_MISSING, MYF(0), tablespace_name);
      DBUG_ASSERT(false);
      DBUG_RETURN(true);
    }

    // Populate statistics
    stats->m_id= uf.getObjectId();
    stats->m_type= "UNDO LOG";
    stats->m_logfile_group_name= lfg.getName();
    stats->m_logfile_group_number= lfg.getObjectId();
    stats->m_total_extents= uf.getSize() / 4;
    stats->m_extent_size= 4;
    stats->m_initial_size= uf.getSize();
    stats->m_maximum_size= uf.getSize();
    stats->m_version= uf.getObjectVersion();

    DBUG_RETURN(false);
  }

  if (type == object_type::TABLESPACE)
  {
    THD *thd= current_thd;

    Ndb *ndb= check_ndb_in_thd(thd);
    if (!ndb)
    {
      // No connection to NDB
      my_error(HA_ERR_NO_CONNECTION, MYF(0));
      DBUG_RETURN(true);
    }

    NdbDictionary::Dictionary* dict= ndb->getDictionary();

    /* Find a node which is alive. NDB's view of a data file
     * is actually a composite of the stats found across all
     * data nodes. However this does not fit well with the
     * server's view which thinks of it as a single file.
     * Since the stats of interest don't vary across the data
     * nodes, using the first available data node is acceptable.
     */
    NdbDictionary::Datafile df= dict->getDatafile(-1, file_name);
    if (dict->getNdbError().classification != NdbError::NoError)
    {
      ndb_my_error(&dict->getNdbError());
      DBUG_RETURN(true);
    }

    NdbDictionary::Tablespace ts=
      dict->getTablespace(df.getTablespace());
    if (dict->getNdbError().classification != NdbError::NoError)
    {
      ndb_my_error(&dict->getNdbError());
      DBUG_RETURN(true);
    }

    /* Check if tablespace name from NDB matches tablespace name
     * from DD. Failure means that the NDB dictionary has gone out
     * of sync with the DD
     */
    if (strcmp(ts.getName(), tablespace_name) != 0)
    {
      my_error(ER_TABLESPACE_MISSING, MYF(0), tablespace_name);
      DBUG_ASSERT(false);
      DBUG_RETURN(true);
    }

    // Populate statistics
    stats->m_id= df.getObjectId();
    stats->m_type= "DATAFILE";
    stats->m_logfile_group_name= ts.getDefaultLogfileGroup();
    stats->m_logfile_group_number= ts.getDefaultLogfileGroupId();
    stats->m_free_extents= df.getFree() / ts.getExtentSize();
    stats->m_total_extents= df.getSize()/ ts.getExtentSize();
    stats->m_extent_size= ts.getExtentSize();
    stats->m_initial_size= df.getSize();
    stats->m_maximum_size= df.getSize();
    stats->m_version= df.getObjectVersion();
    stats->m_row_format= "FIXED";

    DBUG_RETURN(false);
  }

  // Should never reach here
  DBUG_ASSERT(false);
  DBUG_RETURN(true);
}



/**
  Return number of partitions for table in SE

  @param name normalized path(same as open) to the table

  @param[out] num_parts Number of partitions

  @retval false for success
  @retval true for failure, for example table didn't exist in engine
*/

bool
ha_ndbcluster::get_num_parts(const char *name, uint *num_parts)
{
  /*
    NOTE! This function is called very early in the code path
    for opening a table and ha_ndbcluster might not have been
    involved ealier in this query. Also it's asking questions
    about a table but is using a ha_ndbcluster instance which
    haven't been opened yet. Implement as a local static function
    to avoid having access to member variables and functions.
  */

  struct impl {

    static
    int get_num_parts(const char* name, uint* num_parts)
    {
      DBUG_ENTER("impl::get_num_parts");

      // Since this function is always called early in the code
      // path, it's safe to allow the Ndb object to be recycled
      const bool allow_recycle_ndb = true;
      Ndb * const ndb = check_ndb_in_thd(current_thd, allow_recycle_ndb);
      if (!ndb)
      {
        // No connection to NDB
        DBUG_RETURN(HA_ERR_NO_CONNECTION);
      }

      // Split name into db and table name
      char db_name[FN_HEADLEN];
      char table_name[FN_HEADLEN];
      set_dbname(name, db_name);
      set_tabname(name, table_name);

      // Open the table from NDB
      ndb->setDatabaseName(db_name);
      NdbDictionary::Dictionary* dict= ndb->getDictionary();
      Ndb_table_guard ndbtab_g(dict, table_name);
      if (!ndbtab_g.get_table())
      {
        // Could not open table from NDB
        ERR_RETURN(dict->getNdbError());
      }

      // Return number of partitions used in the table
      *num_parts= ndbtab_g.get_table()->getPartitionCount();

      DBUG_RETURN(0);
    }
  };

  const int error = impl::get_num_parts(name, num_parts);
  if (error)
  {
    print_error(error, MYF(0));
    return true; // Could not return number of partitions
  }
  return false;
}


/**
  Set Engine specific data to dd::Table object for upgrade.

  @param[in,out]  thd         thread handle
  @param[in]      db_name     database name
  @param[in]      table_name  table name
  @param[in,out]  dd_table    data dictionary cache object

  @return false on success, true on failure
*/

bool
ha_ndbcluster::upgrade_table(THD* thd,
                             const char*,
                             const char* table_name,
                             dd::Table* dd_table)
{

  Ndb *ndb= check_ndb_in_thd(thd);

  if (!ndb)
  {
    // No connection to NDB
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return true;
  }

  NdbDictionary::Dictionary* dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, table_name);
  const NdbDictionary::Table *ndbtab= ndbtab_g.get_table();

  if (ndbtab == nullptr)
  {
    return true;
  }

  // Set object id and version
  ndb_dd_table_set_object_id_and_version(dd_table,
                                         ndbtab->getObjectId(),
                                         ndbtab->getObjectVersion());

  /*
    Detect and set row format for the table. This is done here
    since the row format of a table is determined by the
    'varpart_reference' which isn't available earlier upstream
  */
  ndb_dd_table_set_row_format(dd_table, ndbtab->getForceVarPart());

  return false;
}

static
int show_ndb_status(THD* thd, SHOW_VAR* var, char*)
{
  if (!check_ndb_in_thd(thd))
    return -1;

  struct st_ndb_status *st;
  SHOW_VAR *st_var;
  {
    char *mem= (char*)sql_alloc(sizeof(struct st_ndb_status) +
                                sizeof(ndb_status_vars_dynamic));
    st= new (mem) st_ndb_status;
    st_var= (SHOW_VAR*)(mem + sizeof(struct st_ndb_status));
    memcpy(st_var, &ndb_status_vars_dynamic, sizeof(ndb_status_vars_dynamic));
    int i= 0;
    SHOW_VAR *tmp= &(ndb_status_vars_dynamic[0]);
    for (; tmp->value; tmp++, i++)
      st_var[i].value= mem + (tmp->value - (char*)&g_ndb_status);
  }
  {
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    Ndb_cluster_connection *c= thd_ndb->connection;
    update_status_variables(thd_ndb, st, c);
  }
  var->type= SHOW_ARRAY;
  var->value= (char *) st_var;
  return 0;
}

/*
   Array defining the status variables which can be returned by
   the ndbcluster plugin in a SHOW STATUS or performance_schema query.

   The list consist of functions as well as further sub arrays. Functions
   are used when the array first need to be populated before its values
   can be read.
*/

static SHOW_VAR ndb_status_vars[] =
{
  {"Ndb",          (char*) &show_ndb_status, SHOW_FUNC,  SHOW_SCOPE_GLOBAL},
  {"Ndb_conflict", (char*) &show_ndb_status_conflict, SHOW_FUNC,  SHOW_SCOPE_GLOBAL},
  {"Ndb",          (char*) &ndb_status_vars_injector, SHOW_ARRAY, SHOW_SCOPE_GLOBAL},
  {"Ndb",          (char*) &ndb_status_vars_slave,    SHOW_ARRAY, SHOW_SCOPE_GLOBAL},
  {"Ndb",          (char*) &show_ndb_status_server_api,     SHOW_FUNC,  SHOW_SCOPE_GLOBAL},
  {"Ndb_index_stat", (char*) &show_ndb_status_index_stat, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}
};


static MYSQL_SYSVAR_ULONG(
  extra_logging,                     /* name */
  opt_ndb_extra_logging,                 /* var */
  PLUGIN_VAR_OPCMDARG,
  "Turn on more logging in the error log.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1,                                 /* default */
  0,                                 /* min */
  0,                                 /* max */
  0                                  /* block */
);


static MYSQL_SYSVAR_ULONG(
  wait_connected,                    /* name */
  opt_ndb_wait_connected,            /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Time (in seconds) for mysqld to wait for connection "
  "to cluster management and data nodes.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  30,                                /* default */
  0,                                 /* min */
  ONE_YEAR_IN_SECONDS,               /* max */
  0                                  /* block */
);


static MYSQL_SYSVAR_ULONG(
  wait_setup,                        /* name */
  opt_ndb_wait_setup,                /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Time (in seconds) for mysqld to wait for setup to "
  "complete (0 = no wait)",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  30,                                /* default */
  0,                                 /* min */
  ONE_YEAR_IN_SECONDS,               /* max */
  0                                  /* block */
);

static const int MAX_CLUSTER_CONNECTIONS = 63;

static MYSQL_SYSVAR_UINT(
  cluster_connection_pool,           /* name */
  opt_ndb_cluster_connection_pool,   /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Pool of cluster connections to be used by mysql server.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1,                                 /* default */
  1,                                 /* min */
  MAX_CLUSTER_CONNECTIONS,           /* max */
  0                                  /* block */
);

static MYSQL_SYSVAR_STR(
  cluster_connection_pool_nodeids,  /* name */
  opt_connection_pool_nodeids_str,  /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Comma separated list of nodeids to use for the cluster connection pool. "
  "Overrides node id specified in --ndb-connectstring. First nodeid "
  "must be equal to --ndb-nodeid(if specified)." ,
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);

static const int MIN_ACTIVATION_THRESHOLD = 0;
static const int MAX_ACTIVATION_THRESHOLD = 16;

static
int
ndb_recv_thread_activation_threshold_check(THD*,
                                           SYS_VAR*,
                                           void *,
                                           struct st_mysql_value *value)
{
  long long int_buf;
  int val = (int)value->val_int(value, &int_buf);
  int new_val = (int)int_buf;

  if (val != 0 || 
      new_val < MIN_ACTIVATION_THRESHOLD ||
      new_val > MAX_ACTIVATION_THRESHOLD)
  {
    return 1;
  }
  opt_ndb_recv_thread_activation_threshold = new_val;
  return 0;
}

static
void
ndb_recv_thread_activation_threshold_update(THD*,
                                            SYS_VAR*,
                                            void *,
                                            const void *)
{
  ndb_set_recv_thread_activation_threshold(
    opt_ndb_recv_thread_activation_threshold);
}

static MYSQL_SYSVAR_UINT(
  recv_thread_activation_threshold,         /* name */
  opt_ndb_recv_thread_activation_threshold, /* var */
  PLUGIN_VAR_RQCMDARG,
  "Activation threshold when receive thread takes over the polling "
  "of the cluster connection (measured in concurrently active "
  "threads)",
  ndb_recv_thread_activation_threshold_check,  /* check func. */
  ndb_recv_thread_activation_threshold_update, /* update func. */
  8,                                           /* default */
  MIN_ACTIVATION_THRESHOLD,                    /* min */
  MAX_ACTIVATION_THRESHOLD,                    /* max */
  0                                            /* block */
);


/* Definitions needed for receive thread cpu mask config variable */
static const int ndb_recv_thread_cpu_mask_option_buf_size = 512;
char ndb_recv_thread_cpu_mask_option_buf[ndb_recv_thread_cpu_mask_option_buf_size];
Uint16 recv_thread_cpuid_array[1 * MAX_CLUSTER_CONNECTIONS];

static
int
ndb_recv_thread_cpu_mask_check(THD*,
                               SYS_VAR*,
                               void *,
                               struct st_mysql_value *value)
{
  char buf[ndb_recv_thread_cpu_mask_option_buf_size];
  int len = sizeof(buf);
  const char *str = value->val_str(value, buf, &len);

  return ndb_recv_thread_cpu_mask_check_str(str);
}

static int
ndb_recv_thread_cpu_mask_check_str(const char *str)
{
  unsigned i;
  SparseBitmask bitmask;

  recv_thread_num_cpus = 0;
  if (str == 0)
  {
    /* Setting to empty string is interpreted as remove locking to CPU */
    return 0;
  }

  if (parse_mask(str, bitmask) < 0)
  {
    ndb_log_info("Trying to set ndb_recv_thread_cpu_mask to"
                 " illegal value = %s, ignored",
                 str);
    goto error;
  }
  for (i = bitmask.find(0);
       i != SparseBitmask::NotFound;
       i = bitmask.find(i + 1))
  {
    if (recv_thread_num_cpus ==
        1 * MAX_CLUSTER_CONNECTIONS)
    {
      ndb_log_info("Trying to set too many CPU's in "
                   "ndb_recv_thread_cpu_mask, ignored"
                   " this variable, erroneus value = %s",
                   str);
      goto error;
    }
    recv_thread_cpuid_array[recv_thread_num_cpus++] = i;
  }
  return 0;
error:
  return 1;
}

static
int
ndb_recv_thread_cpu_mask_update()
{
  return ndb_set_recv_thread_cpu(recv_thread_cpuid_array,
                                 recv_thread_num_cpus);
}

static
void
ndb_recv_thread_cpu_mask_update_func(THD *,
                                     SYS_VAR*,
                                     void *, const void *)
{
  (void)ndb_recv_thread_cpu_mask_update();
}

static MYSQL_SYSVAR_STR(
  recv_thread_cpu_mask,             /* name */
  opt_ndb_recv_thread_cpu_mask,     /* var */
  PLUGIN_VAR_RQCMDARG,
  "CPU mask for locking receiver threads to specific CPU, specified "
  " as hexadecimal as e.g. 0x33, one CPU is used per receiver thread.",
  ndb_recv_thread_cpu_mask_check,      /* check func. */
  ndb_recv_thread_cpu_mask_update_func,/* update func. */
  ndb_recv_thread_cpu_mask_option_buf
);



static MYSQL_SYSVAR_STR(
  index_stat_option,                /* name */
  opt_ndb_index_stat_option,        /* var */
  PLUGIN_VAR_RQCMDARG,
  "Comma-separated tunable options for ndb index statistics",
  ndb_index_stat_option_check,      /* check func. */
  ndb_index_stat_option_update,     /* update func. */
  ndb_index_stat_option_buf
);


ulong opt_ndb_report_thresh_binlog_epoch_slip;
static MYSQL_SYSVAR_ULONG(
  report_thresh_binlog_epoch_slip,   /* name */
  opt_ndb_report_thresh_binlog_epoch_slip,/* var */
  PLUGIN_VAR_RQCMDARG,
  "Threshold for Binlog injector thread consumption lag, "
  "before reporting the Event buffer status' message with reason "
  "BUFFERED_EPOCHS_OVER_THRESHOLD. "
  "The lag is defined as the number of epochs completely buffered in "
  "the event buffer, but not consumed by the Binlog injector thread yet.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  10,                                /* default */
  0,                                 /* min */
  256,                               /* max */
  0                                  /* block */
);


ulong opt_ndb_report_thresh_binlog_mem_usage;
static MYSQL_SYSVAR_ULONG(
  report_thresh_binlog_mem_usage,    /* name */
  opt_ndb_report_thresh_binlog_mem_usage,/* var */
  PLUGIN_VAR_RQCMDARG,
  "Threshold on percentage of free memory before reporting binlog "
  "status. E.g. 10 means that if amount of available memory for "
  "receiving binlog data from the storage nodes goes below 10%, "
  "a status message will be sent to the cluster log.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  10,                                /* default */
  0,                                 /* min */
  100,                               /* max */
  0                                  /* block */
);


ulong opt_ndb_eventbuffer_max_alloc;
static MYSQL_SYSVAR_ULONG(
  eventbuffer_max_alloc,             /* name */
  opt_ndb_eventbuffer_max_alloc,     /* var */
  PLUGIN_VAR_RQCMDARG,
  "Maximum memory that can be allocated for buffering "
  "events by the ndb api.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0,                                 /* default */
  0,                                 /* min */
  UINT_MAX32,                        /* max */
  0                                  /* block */
);


uint opt_ndb_eventbuffer_free_percent;
static MYSQL_SYSVAR_UINT(
  eventbuffer_free_percent, /* name */
  opt_ndb_eventbuffer_free_percent,/* var */
  PLUGIN_VAR_RQCMDARG,
  "Percentage of free memory that should be available "
  "in event buffer before resuming buffering "
  "after the max_alloc limit is hit.",
  NULL, /* check func. */
  NULL, /* update func. */
  20, /* default */
  1, /* min */
  99, /* max */
  0 /* block */
);

static MYSQL_SYSVAR_BOOL(
  fully_replicated,                        /* name */
  opt_ndb_fully_replicated,                /* var  */
  PLUGIN_VAR_OPCMDARG,
  "Create tables that are fully replicated by default. This enables reading"
  " from any data node when using ReadCommitted. This is great for read"
  " scalability but hampers write scalability",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);

static MYSQL_SYSVAR_BOOL(
  read_backup,                       /* name */
  opt_ndb_read_backup,               /* var  */
  PLUGIN_VAR_OPCMDARG,
  "Create tables with Read Backup flag set. Enables those tables to be"
  " read from backup replicas as well as from primary replicas. Delays"
  " commit acknowledge of write transactions to accomplish this.",
  NULL,                              /* check func.  */
  NULL,                              /* update func. */
  0                                  /* default      */
);

static
void
ndb_data_node_neighbour_update_func(THD*,
                                    SYS_VAR*,
                                    void *var_ptr,
                                    const void *save)
{
  const ulong data_node_neighbour = *static_cast<const ulong*>(save);
  *static_cast<ulong*>(var_ptr) = data_node_neighbour;
  ndb_set_data_node_neighbour(data_node_neighbour);
}

static MYSQL_SYSVAR_ULONG(
  data_node_neighbour,                 /* name */
  opt_ndb_data_node_neighbour,         /* var  */
  PLUGIN_VAR_OPCMDARG,
  "My closest data node, if 0 no closest neighbour, used to select"
  " an appropriate data node to contact to run a transaction at.",
  NULL,                                /* check func.  */
  ndb_data_node_neighbour_update_func, /* update func. */
  0,                                   /* default      */
  0,                                   /* min          */
  MAX_NDB_NODES,                       /* max          */
  0                                    /* block        */
);

bool opt_ndb_log_update_as_write;
static MYSQL_SYSVAR_BOOL(
  log_update_as_write,               /* name */
  opt_ndb_log_update_as_write,       /* var */
  PLUGIN_VAR_OPCMDARG,
  "For efficiency log only after image as a write event. "
  "Ignore before image. This may cause compatibility problems if "
  "replicating to other storage engines than ndbcluster.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);

bool opt_ndb_log_update_minimal;
static MYSQL_SYSVAR_BOOL(
  log_update_minimal,                  /* name */
  opt_ndb_log_update_minimal,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "For efficiency, log updates in a minimal format"
  "Log only the primary key value(s) in the before "
  "image. Log only the changed columns in the after "
  "image. This may cause compatibility problems if "
  "replicating to other storage engines than ndbcluster.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);

bool opt_ndb_log_updated_only;
static MYSQL_SYSVAR_BOOL(
  log_updated_only,                  /* name */
  opt_ndb_log_updated_only,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "For efficiency log only updated columns. Columns are considered "
  "as \"updated\" even if they are updated with the same value. "
  "This may cause compatibility problems if "
  "replicating to other storage engines than ndbcluster.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);

bool opt_ndb_log_empty_update;
static MYSQL_SYSVAR_BOOL(
  log_empty_update,                  /* name */
  opt_ndb_log_empty_update,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "Normally empty updates are filtered away "
  "before they are logged. However, for read tracking "
  "in conflict resolution a hidden pesudo attribute is "
  "set which will result in an empty update along with "
  "special flags set. For this to work empty updates "
  "have to be allowed.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);

bool opt_ndb_log_orig;
static MYSQL_SYSVAR_BOOL(
  log_orig,                          /* name */
  opt_ndb_log_orig,                  /* var */
  PLUGIN_VAR_OPCMDARG,
  "Log originating server id and epoch in ndb_binlog_index. Each epoch "
  "may in this case have multiple rows in ndb_binlog_index, one for "
  "each originating epoch.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);


bool opt_ndb_log_bin;
static MYSQL_SYSVAR_BOOL(
  log_bin,                           /* name */
  opt_ndb_log_bin,                   /* var */
  PLUGIN_VAR_OPCMDARG,
  "Log ndb tables in the binary log. Option only has meaning if "
  "the binary log has been turned on for the server.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


bool opt_ndb_log_binlog_index;
static MYSQL_SYSVAR_BOOL(
  log_binlog_index,                  /* name */
  opt_ndb_log_binlog_index,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "Insert mapping between epochs and binlog positions into the "
  "ndb_binlog_index table.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


static bool opt_ndb_log_empty_epochs;
static MYSQL_SYSVAR_BOOL(
  log_empty_epochs,                  /* name */
  opt_ndb_log_empty_epochs,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
);

bool ndb_log_empty_epochs(void)
{
  return opt_ndb_log_empty_epochs;
}

bool opt_ndb_log_apply_status;
static MYSQL_SYSVAR_BOOL(
  log_apply_status,                 /* name */
  opt_ndb_log_apply_status,         /* var */
  PLUGIN_VAR_OPCMDARG,
  "Log ndb_apply_status updates from Master in the Binlog",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0                                 /* default */
);


bool opt_ndb_log_transaction_id;
static MYSQL_SYSVAR_BOOL(
  log_transaction_id,               /* name */
  opt_ndb_log_transaction_id,       /* var  */
  PLUGIN_VAR_OPCMDARG,
  "Log Ndb transaction identities per row in the Binlog",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0                                 /* default */
);

bool opt_ndb_clear_apply_status;
static MYSQL_SYSVAR_BOOL(
  clear_apply_status,               /* name */
  opt_ndb_clear_apply_status,       /* var  */
  PLUGIN_VAR_OPCMDARG,
  "Whether RESET SLAVE will clear all entries in ndb_apply_status",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  1                                 /* default */
);

static MYSQL_SYSVAR_STR(
  connectstring,                    /* name */
  opt_ndb_connectstring,            /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Connect string for ndbcluster.",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);


static MYSQL_SYSVAR_STR(
  mgmd_host,                        /* name */
  opt_ndb_connectstring,                /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Same as --ndb-connectstring",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);


static MYSQL_SYSVAR_UINT(
  nodeid,                           /* name */
  opt_ndb_nodeid,                   /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Set nodeid for this node. Overrides node id specified "
  "in --ndb-connectstring.",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0,                                /* default */
  0,                                /* min */
  MAX_NODES_ID,                     /* max */
  0                                 /* block */
);

static const char* slave_conflict_role_names[] =
{
  "NONE",
  "SECONDARY",
  "PRIMARY",
  "PASS",
  NullS
};

static TYPELIB slave_conflict_role_typelib = 
{
  array_elements(slave_conflict_role_names) - 1,
  "",
  slave_conflict_role_names,
  NULL
};


/**
 * slave_conflict_role_check_func.
 * 
 * Perform most validation of a role change request.
 * Inspired by sql_plugin.cc::check_func_enum()
 */
static int slave_conflict_role_check_func(THD *thd, SYS_VAR*,
                                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  long long tmp;
  long result;
  int length;

  do
  {
    if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
    {
      length= sizeof(buff);
      if (!(str= value->val_str(value, buff, &length)))
        break;
      if ((result= (long)find_type(str, &slave_conflict_role_typelib, 0) - 1) < 0)
        break;
    }
    else
    {
      if (value->val_int(value, &tmp))
        break;
      if (tmp < 0 ||
          tmp >= static_cast<long long>(slave_conflict_role_typelib.count))
        break;
      result= (long) tmp;
    }
    
    const char* failure_cause_str = NULL;
    if (!st_ndb_slave_state::checkSlaveConflictRoleChange(
               (enum_slave_conflict_role) opt_ndb_slave_conflict_role,
               (enum_slave_conflict_role) result,
               &failure_cause_str))
    {
      char msgbuf[256];
      snprintf(msgbuf, 
                  sizeof(msgbuf), 
                  "Role change from %s to %s failed : %s",
                  get_type(&slave_conflict_role_typelib, opt_ndb_slave_conflict_role),
                  get_type(&slave_conflict_role_typelib, result),
                  failure_cause_str);
      
      thd->raise_error_printf(ER_ERROR_WHEN_EXECUTING_COMMAND,
                              "SET GLOBAL ndb_slave_conflict_role",
                              msgbuf);
      
      break;
    }
    
    /* Ok */
    *(long*)save= result;
    return 0;
  } while (0);
  /* Error */
  return 1;
}

/**
 * slave_conflict_role_update_func
 *
 * Perform actual change of role, using saved 'long' enum value
 * prepared by the update func above.
 *
 * Inspired by sql_plugin.cc::update_func_long()
 */
static void slave_conflict_role_update_func(THD*, SYS_VAR*,
                                            void *tgt, const void *save)
{
  *(long *)tgt= *(long *) save;
}

static MYSQL_SYSVAR_ENUM(
  slave_conflict_role,               /* Name */
  opt_ndb_slave_conflict_role,       /* Var */
  PLUGIN_VAR_RQCMDARG,
  "Role for Slave to play in asymmetric conflict algorithms.",
  slave_conflict_role_check_func,    /* Check func */
  slave_conflict_role_update_func,   /* Update func */
  SCR_NONE,                          /* Default value */
  &slave_conflict_role_typelib       /* typelib */
);

#ifndef DBUG_OFF

static
void
dbg_check_shares_update(THD*, SYS_VAR*, void*, const void*)
{
  NDB_SHARE::dbg_check_shares_update();
}

static MYSQL_THDVAR_UINT(
  dbg_check_shares,                  /* name */
  PLUGIN_VAR_RQCMDARG,
  "Debug, only...check that no shares are lingering...",
  NULL,                              /* check func */
  dbg_check_shares_update,           /* update func */
  0,                                 /* default */
  0,                                 /* min */
  1,                                 /* max */
  0                                  /* block */
);

#endif

static SYS_VAR* system_variables[]= {
  MYSQL_SYSVAR(extra_logging),
  MYSQL_SYSVAR(wait_connected),
  MYSQL_SYSVAR(wait_setup),
  MYSQL_SYSVAR(cluster_connection_pool),
  MYSQL_SYSVAR(cluster_connection_pool_nodeids),
  MYSQL_SYSVAR(recv_thread_activation_threshold),
  MYSQL_SYSVAR(recv_thread_cpu_mask),
  MYSQL_SYSVAR(report_thresh_binlog_mem_usage),
  MYSQL_SYSVAR(report_thresh_binlog_epoch_slip),
  MYSQL_SYSVAR(eventbuffer_max_alloc),
  MYSQL_SYSVAR(eventbuffer_free_percent),
  MYSQL_SYSVAR(log_update_as_write),
  MYSQL_SYSVAR(log_updated_only),
  MYSQL_SYSVAR(log_update_minimal),
  MYSQL_SYSVAR(log_empty_update),
  MYSQL_SYSVAR(log_orig),
  MYSQL_SYSVAR(distribution),
  MYSQL_SYSVAR(autoincrement_prefetch_sz),
  MYSQL_SYSVAR(force_send),
  MYSQL_SYSVAR(use_exact_count),
  MYSQL_SYSVAR(use_transactions),
  MYSQL_SYSVAR(use_copying_alter_table),
  MYSQL_SYSVAR(allow_copying_alter_table),
  MYSQL_SYSVAR(optimized_node_selection),
  MYSQL_SYSVAR(batch_size),
  MYSQL_SYSVAR(optimization_delay),
  MYSQL_SYSVAR(index_stat_enable),
  MYSQL_SYSVAR(index_stat_option),
  MYSQL_SYSVAR(table_no_logging),
  MYSQL_SYSVAR(table_temporary),
  MYSQL_SYSVAR(log_bin),
  MYSQL_SYSVAR(log_binlog_index),
  MYSQL_SYSVAR(log_empty_epochs),
  MYSQL_SYSVAR(log_apply_status),
  MYSQL_SYSVAR(log_transaction_id),
  MYSQL_SYSVAR(clear_apply_status),
  MYSQL_SYSVAR(connectstring),
  MYSQL_SYSVAR(mgmd_host),
  MYSQL_SYSVAR(nodeid),
  MYSQL_SYSVAR(blob_read_batch_bytes),
  MYSQL_SYSVAR(blob_write_batch_bytes),
  MYSQL_SYSVAR(deferred_constraints),
  MYSQL_SYSVAR(join_pushdown),
  MYSQL_SYSVAR(log_exclusive_reads),
  MYSQL_SYSVAR(read_backup),
  MYSQL_SYSVAR(data_node_neighbour),
  MYSQL_SYSVAR(fully_replicated),
#ifndef DBUG_OFF
  MYSQL_SYSVAR(dbg_check_shares),
#endif
  MYSQL_SYSVAR(version),
  MYSQL_SYSVAR(version_string),
  MYSQL_SYSVAR(show_foreign_key_mock_tables),
  MYSQL_SYSVAR(slave_conflict_role),
  MYSQL_SYSVAR(default_column_format),
  NULL
};

struct st_mysql_storage_engine ndbcluster_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };


extern struct st_mysql_plugin i_s_ndb_transid_mysql_connection_map_plugin;
extern struct st_mysql_plugin ndbinfo_plugin;

mysql_declare_plugin(ndbcluster)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &ndbcluster_storage_engine,
  ndbcluster_hton_name,
  "MySQL AB",
  "Clustered, fault-tolerant tables",
  PLUGIN_LICENSE_GPL,
  ndbcluster_init,            /* plugin init */
  NULL,                       /* plugin check uninstall */
  ndbcluster_deinit,          /* plugin deinit */
  0x0100,                     /* plugin version */
  ndb_status_vars,            /* status variables */
  system_variables,           /* system variables */
  NULL,                       /* config options */
  0                           /* flags */
},
ndbinfo_plugin, /* ndbinfo plugin */
/* IS plugin table which maps between mysql connection id and ndb trans-id */
i_s_ndb_transid_mysql_connection_map_plugin
mysql_declare_plugin_end;

