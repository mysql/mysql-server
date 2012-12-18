/* Copyright (c) 2004, 2012, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  This file defines the NDB Cluster handler: the interface between
  MySQL and NDB Cluster
*/

#include "ha_ndbcluster_glue.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include "ha_ndbcluster.h"
#include <ndbapi/NdbApi.hpp>
#include <ndbapi/NdbIndexStat.hpp>
#include <ndbapi/NdbInterpretedCode.hpp>
#include "../storage/ndb/src/ndbapi/NdbQueryBuilder.hpp"
#include "../storage/ndb/src/ndbapi/NdbQueryOperation.hpp"

#include "ha_ndbcluster_binlog.h"
#include "ha_ndbcluster_push.h"
#include "ha_ndbcluster_cond.h"
#include "ha_ndbcluster_tables.h"
#include "ha_ndbcluster_connection.h"
#include "ndb_thd.h"
#include "ndb_table_guard.h"
#include "ndb_global_schema_lock.h"
#include "ndb_global_schema_lock_guard.h"
#include "abstract_query_plan.h"
#include "ndb_dist_priv_util.h"
#include "ha_ndb_index_stat.h"

#include <mysql/plugin.h>
#include <ndb_version.h>
#include <ndb_global.h>
#include "ndb_mi.h"
#include "ndb_conflict.h"
#include "ndb_anyvalue.h"
#include "ndb_binlog_extra_row_info.h"
#include "ndb_event_data.h"
#include "ndb_schema_dist.h"
#include "ndb_component.h"
#include "ndb_util_thread.h"
#include "ndb_local_connection.h"
#include "ndb_local_schema.h"

using std::min;
using std::max;

// ndb interface initialization/cleanup
extern "C" void ndb_init_internal();
extern "C" void ndb_end_internal();

static const int DEFAULT_PARALLELISM= 0;
static const ha_rows DEFAULT_AUTO_PREFETCH= 32;
static const ulong ONE_YEAR_IN_SECONDS= (ulong) 3600L*24L*365L;

ulong opt_ndb_extra_logging;
static ulong opt_ndb_wait_connected;
ulong opt_ndb_wait_setup;
static ulong opt_ndb_cache_check_time;
static uint opt_ndb_cluster_connection_pool;
static char* opt_ndb_index_stat_option;
static char* opt_ndb_connectstring;
static uint opt_ndb_nodeid;

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
  "only be used if on-line alter table fails).",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0                                  /* default */
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

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define DEFAULT_NDB_INDEX_STAT_ENABLE FALSE
#else
#define DEFAULT_NDB_INDEX_STAT_ENABLE TRUE
#endif

static MYSQL_THDVAR_BOOL(
  index_stat_enable,                 /* name */
  PLUGIN_VAR_OPCMDARG,
  "Use ndb index statistics in query optimization.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  DEFAULT_NDB_INDEX_STAT_ENABLE      /* default */
);


static MYSQL_THDVAR_ULONG(
  index_stat_cache_entries,          /* name */
  PLUGIN_VAR_NOCMDARG,
  "Obsolete (ignored and will be removed later).",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  32,                                /* default */
  0,                                 /* min */
  ULONG_MAX,                         /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_ULONG(
  index_stat_update_freq,            /* name */
  PLUGIN_VAR_NOCMDARG,
  "Obsolete (ignored and will be removed later).",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  20,                                /* default */
  0,                                 /* min */
  ULONG_MAX,                         /* max */
  0                                  /* block */
);


static MYSQL_THDVAR_BOOL(
  table_no_logging,                  /* name */
  PLUGIN_VAR_NOCMDARG,
  "",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  FALSE                              /* default */
);


static MYSQL_THDVAR_BOOL(
  table_temporary,                   /* name */
  PLUGIN_VAR_NOCMDARG,
  "",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  FALSE                              /* default */
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

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define DEFAULT_NDB_JOIN_PUSHDOWN FALSE
#else
#define DEFAULT_NDB_JOIN_PUSHDOWN TRUE
#endif

static MYSQL_THDVAR_BOOL(
  join_pushdown,                     /* name */
  PLUGIN_VAR_OPCMDARG,
  "Enable pushing down of join to datanodes",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  DEFAULT_NDB_JOIN_PUSHDOWN          /* default */
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

static int ndbcluster_end(handlerton *hton, ha_panic_function flag);
static bool ndbcluster_show_status(handlerton *hton, THD*,
                                   stat_print_fn *,
                                   enum ha_stat_type);
static int ndbcluster_alter_tablespace(handlerton *hton,
                                       THD* thd, 
                                       st_alter_tablespace *info);
static int ndbcluster_fill_files_table(handlerton *hton,
                                       THD *thd, 
                                       TABLE_LIST *tables, 
                                       Item *cond);

#if MYSQL_VERSION_ID >= 50501
/**
   Used to fill in INFORMATION_SCHEMA* tables.

   @param hton handle to the handlerton structure
   @param thd the thread/connection descriptor
   @param[in,out] tables the information schema table that is filled up
   @param cond used for conditional pushdown to storage engine
   @param schema_table_idx the table id that distinguishes the type of table

   @return Operation status
 */
static int
ndbcluster_fill_is_table(handlerton *hton, THD *thd, TABLE_LIST *tables,
                         Item *cond, enum enum_schema_tables schema_table_idx)
{
  if (schema_table_idx == SCH_FILES)
    return  ndbcluster_fill_files_table(hton, thd, tables, cond);
  return 0;
}
#endif

static handler *ndbcluster_create_handler(handlerton *hton,
                                          TABLE_SHARE *table,
                                          MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ndbcluster(hton, table);
}

static uint
ndbcluster_partition_flags()
{
  return (HA_CAN_PARTITION | HA_CAN_UPDATE_PARTITION_KEY |
          HA_CAN_PARTITION_UNIQUE | HA_USE_AUTO_PARTITION);
}

static uint
ndbcluster_alter_table_flags(uint flags)
{
  const uint f=
    HA_PARTITION_FUNCTION_SUPPORTED |
    0;

  if (flags & Alter_info::ALTER_DROP_PARTITION)
    return 0;

  return f;
}

#define NDB_AUTO_INCREMENT_RETRIES 100
#define BATCH_FLUSH_SIZE (32768)

static int ndb_to_mysql_error(const NdbError *ndberr);

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
int ndbcluster_terminating= 0;

/* 
   Indicator and CONDVAR used to delay client and slave
   connections until Ndb has Binlog setup
   (bug#46955)
*/
int ndb_setup_complete= 0;
pthread_cond_t COND_ndb_setup_complete; // Signal with ndbcluster_mutex

extern Ndb* g_ndb;

uchar g_node_id_map[max_ndb_nodes];

/// Handler synchronization
pthread_mutex_t ndbcluster_mutex;

/// Table lock handling
HASH ndbcluster_open_tables;
HASH ndbcluster_dropped_tables;

static uchar *ndbcluster_get_key(NDB_SHARE *share, size_t *length,
                                my_bool not_used __attribute__((unused)));

static void modify_shared_stats(NDB_SHARE *share,
                                Ndb_local_table_statistics *local_stat);

static int ndb_get_table_statistics(THD *thd, ha_ndbcluster*, bool, Ndb*,
                                    const NdbRecord *, struct Ndb_statistics *,
                                    bool have_lock= FALSE,
                                    uint part_id= ~(uint)0);

THD *injector_thd= 0;

/* Status variables shown with 'show status like 'Ndb%' */

struct st_ndb_status g_ndb_status;

const char *g_ndb_status_index_stat_status = "";
long g_ndb_status_index_stat_cache_query = 0;
long g_ndb_status_index_stat_cache_clean = 0;

long long g_event_data_count = 0;
long long g_event_nondata_count = 0;
long long g_event_bytes_count = 0;

static long long g_slave_api_client_stats[Ndb::NumClientStatistics];

static long long g_server_api_client_stats[Ndb::NumClientStatistics];

void
update_slave_api_stats(Ndb* ndb)
{
  for (Uint32 i=0; i < Ndb::NumClientStatistics; i++)
    g_slave_api_client_stats[i] = ndb->getClientStat(i);
}

st_ndb_slave_state g_ndb_slave_state;

static int check_slave_state(THD* thd)
{
  DBUG_ENTER("check_slave_state");

#ifdef HAVE_NDB_BINLOG
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
        sql_print_warning("NDB Slave : Could not determine maximum replicated epoch from %s.%s "
                          "at Slave start, error %u %s",
                          NDB_REP_DB,
                          NDB_APPLY_TABLE,
                          ndb_error.code, ndb_error.message);
      }

      /*
        Set Global status variable to the Highest Applied Epoch from
        the Cluster DB.
        If none was found, this will be zero.
      */
      g_ndb_slave_state.max_rep_epoch = highestAppliedEpoch;
      sql_print_information("NDB Slave : MaxReplicatedEpoch set to %llu (%u/%u) at Slave start",
                            g_ndb_slave_state.max_rep_epoch,
                            (Uint32)(g_ndb_slave_state.max_rep_epoch >> 32),
                            (Uint32)(g_ndb_slave_state.max_rep_epoch & 0xffffffff));
    } // Load highest replicated epoch
  } // New Slave SQL thread run id
#endif

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
      sql_print_information("NDB: NodeID is %lu, management server '%s:%lu'",
                            ns->cluster_node_id, ns->connected_host,
                            ns->connected_port);
  }
  ns->number_of_replicas= 0;
  {
    int n= c->get_no_ready();
    ns->number_of_ready_data_nodes= n > 0 ?  n : 0;
  }
  ns->number_of_data_nodes= c->no_db_nodes();
  ns->connect_count= c->get_connect_count();
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
   SHOW_LONGLONG},                                                      \
  {"api_wait_scan_result_count" NAME_SUFFIX,                            \
   (char*) ARRAY_LOCATION[ Ndb::WaitScanResultCount ],                  \
   SHOW_LONGLONG},                                                      \
  {"api_wait_meta_request_count" NAME_SUFFIX,                           \
   (char*) ARRAY_LOCATION[ Ndb::WaitMetaRequestCount ],                 \
   SHOW_LONGLONG},                                                      \
  {"api_wait_nanos_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::WaitNanosCount ],                       \
   SHOW_LONGLONG},                                                      \
  {"api_bytes_sent_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::BytesSentCount ],                       \
   SHOW_LONGLONG},                                                      \
  {"api_bytes_received_count" NAME_SUFFIX,                              \
   (char*) ARRAY_LOCATION[ Ndb::BytesRecvdCount ],                      \
   SHOW_LONGLONG},                                                      \
  {"api_trans_start_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransStartCount ],                      \
   SHOW_LONGLONG},                                                      \
  {"api_trans_commit_count" NAME_SUFFIX,                                \
   (char*) ARRAY_LOCATION[ Ndb::TransCommitCount ],                     \
   SHOW_LONGLONG},                                                      \
  {"api_trans_abort_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransAbortCount ],                      \
   SHOW_LONGLONG},                                                      \
  {"api_trans_close_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::TransCloseCount ],                      \
   SHOW_LONGLONG},                                                      \
  {"api_pk_op_count" NAME_SUFFIX,                                       \
   (char*) ARRAY_LOCATION[ Ndb::PkOpCount ],                            \
   SHOW_LONGLONG},                                                      \
  {"api_uk_op_count" NAME_SUFFIX,                                       \
   (char*) ARRAY_LOCATION[ Ndb::UkOpCount ],                            \
   SHOW_LONGLONG},                                                      \
  {"api_table_scan_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::TableScanCount ],                       \
   SHOW_LONGLONG},                                                      \
  {"api_range_scan_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::RangeScanCount ],                       \
   SHOW_LONGLONG},                                                      \
  {"api_pruned_scan_count" NAME_SUFFIX,                                 \
   (char*) ARRAY_LOCATION[ Ndb::PrunedScanCount ],                      \
   SHOW_LONGLONG},                                                      \
  {"api_scan_batch_count" NAME_SUFFIX,                                  \
   (char*) ARRAY_LOCATION[ Ndb::ScanBatchCount ],                       \
   SHOW_LONGLONG},                                                      \
  {"api_read_row_count" NAME_SUFFIX,                                    \
   (char*) ARRAY_LOCATION[ Ndb::ReadRowCount ],                         \
   SHOW_LONGLONG},                                                      \
  {"api_trans_local_read_row_count" NAME_SUFFIX,                        \
   (char*) ARRAY_LOCATION[ Ndb::TransLocalReadRowCount ],               \
   SHOW_LONGLONG},                                                      \
  {"api_adaptive_send_forced_count" NAME_SUFFIX,                        \
   (char *) ARRAY_LOCATION[ Ndb::ForcedSendsCount ],                    \
   SHOW_LONGLONG},                                                      \
  {"api_adaptive_send_unforced_count" NAME_SUFFIX,                      \
   (char *) ARRAY_LOCATION[ Ndb::UnforcedSendsCount ],                  \
   SHOW_LONGLONG},                                                      \
  {"api_adaptive_send_deferred_count" NAME_SUFFIX,                      \
   (char *) ARRAY_LOCATION[ Ndb::DeferredSendsCount ],                  \
   SHOW_LONGLONG}

SHOW_VAR ndb_status_variables_dynamic[]= {
  {"cluster_node_id",     (char*) &g_ndb_status.cluster_node_id,      SHOW_LONG},
  {"config_from_host",    (char*) &g_ndb_status.connected_host,       SHOW_CHAR_PTR},
  {"config_from_port",    (char*) &g_ndb_status.connected_port,       SHOW_LONG},
//{"number_of_replicas",  (char*) &g_ndb_status.number_of_replicas,   SHOW_LONG},
  {"number_of_data_nodes",(char*) &g_ndb_status.number_of_data_nodes, SHOW_LONG},
  {"number_of_ready_data_nodes",
   (char*) &g_ndb_status.number_of_ready_data_nodes,                  SHOW_LONG},
  {"connect_count",      (char*) &g_ndb_status.connect_count,         SHOW_LONG},
  {"execute_count",      (char*) &g_ndb_status.execute_count,         SHOW_LONG},
  {"scan_count",         (char*) &g_ndb_status.scan_count,            SHOW_LONG},
  {"pruned_scan_count",  (char*) &g_ndb_status.pruned_scan_count,     SHOW_LONG},
  {"schema_locks_count", (char*) &g_ndb_status.schema_locks_count,    SHOW_LONG},
  NDBAPI_COUNTERS("_session", &g_ndb_status.api_client_stats),
  {"sorted_scan_count",  (char*) &g_ndb_status.sorted_scan_count,     SHOW_LONG},
  {"pushed_queries_defined", (char*) &g_ndb_status.pushed_queries_defined, 
   SHOW_LONG},
  {"pushed_queries_dropped", (char*) &g_ndb_status.pushed_queries_dropped,
   SHOW_LONG},
  {"pushed_queries_executed", (char*) &g_ndb_status.pushed_queries_executed,
   SHOW_LONG},
  {"pushed_reads",       (char*) &g_ndb_status.pushed_reads,          SHOW_LONG},
  {NullS, NullS, SHOW_LONG}
};

SHOW_VAR ndb_status_conflict_variables[]= {
  {"fn_max",       (char*) &g_ndb_slave_state.total_violation_count[CFT_NDB_MAX], SHOW_LONGLONG},
  {"fn_old",       (char*) &g_ndb_slave_state.total_violation_count[CFT_NDB_OLD], SHOW_LONGLONG},
  {"fn_max_del_win", (char*) &g_ndb_slave_state.total_violation_count[CFT_NDB_MAX_DEL_WIN], SHOW_LONGLONG},
  {"fn_epoch",     (char*) &g_ndb_slave_state.total_violation_count[CFT_NDB_EPOCH], SHOW_LONGLONG},
  {"fn_epoch_trans", (char*) &g_ndb_slave_state.total_violation_count[CFT_NDB_EPOCH_TRANS], SHOW_LONGLONG},
  {"trans_row_conflict_count", (char*) &g_ndb_slave_state.trans_row_conflict_count, SHOW_LONGLONG},
  {"trans_row_reject_count",   (char*) &g_ndb_slave_state.trans_row_reject_count, SHOW_LONGLONG},
  {"trans_reject_count",       (char*) &g_ndb_slave_state.trans_in_conflict_count, SHOW_LONGLONG},
  {"trans_detect_iter_count",  (char*) &g_ndb_slave_state.trans_detect_iter_count, SHOW_LONGLONG},
  {"trans_conflict_commit_count",
                               (char*) &g_ndb_slave_state.trans_conflict_commit_count, SHOW_LONGLONG},
  {NullS, NullS, SHOW_LONG}
};

SHOW_VAR ndb_status_injector_variables[]= {
  {"api_event_data_count_injector",     (char*) &g_event_data_count, SHOW_LONGLONG},
  {"api_event_nondata_count_injector",  (char*) &g_event_nondata_count, SHOW_LONGLONG},
  {"api_event_bytes_count_injector",    (char*) &g_event_bytes_count, SHOW_LONGLONG},
  {NullS, NullS, SHOW_LONG}
};

SHOW_VAR ndb_status_slave_variables[]= {
  NDBAPI_COUNTERS("_slave", &g_slave_api_client_stats),
  {"slave_max_replicated_epoch", (char*) &g_ndb_slave_state.max_rep_epoch, SHOW_LONGLONG},
  {NullS, NullS, SHOW_LONG}
};

SHOW_VAR ndb_status_server_client_stat_variables[]= {
  NDBAPI_COUNTERS("", &g_server_api_client_stats),
  {"api_event_data_count",     
   (char*) &g_server_api_client_stats[ Ndb::DataEventsRecvdCount ], 
   SHOW_LONGLONG},
  {"api_event_nondata_count",  
   (char*) &g_server_api_client_stats[ Ndb::NonDataEventsRecvdCount ], 
   SHOW_LONGLONG},
  {"api_event_bytes_count",    
   (char*) &g_server_api_client_stats[ Ndb::EventBytesRecvdCount ], 
   SHOW_LONGLONG},
  {NullS, NullS, SHOW_LONG}
};

static int show_ndb_server_api_stats(THD *thd, SHOW_VAR *var, char *buff)
{
  /* This function is called when SHOW STATUS / INFO_SCHEMA wants
   * to see one of our status vars
   * We use this opportunity to :
   *  1) Update the globals with current values
   *  2) Return an array of var definitions, pointing to
   *     the updated globals
   */
  ndb_get_connection_stats((Uint64*) &g_server_api_client_stats[0]);

  var->type= SHOW_ARRAY;
  var->value= (char*) ndb_status_server_client_stat_variables;

  return 0;
}

SHOW_VAR ndb_status_index_stat_variables[]= {
  {"status",          (char*) &g_ndb_status_index_stat_status, SHOW_CHAR_PTR},
  {"cache_query",     (char*) &g_ndb_status_index_stat_cache_query, SHOW_LONG},
  {"cache_clean",     (char*) &g_ndb_status_index_stat_cache_clean, SHOW_LONG},
  {NullS, NullS, SHOW_LONG}
};


/*
  Error handling functions
*/

/* Note for merge: old mapping table, moved to storage/ndb/ndberror.c */

static int ndb_to_mysql_error(const NdbError *ndberr)
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

    /* Mapping missing, go with the ndb error code*/
  case -1:
    error= ndberr->code;
    break;
    /* Mapping exists, go with the mapped code */
  default:
    break;
  }

  /*
    If we don't abort directly on warnings push a warning
    with the internal error information
   */
  if (!current_thd->abort_on_warning)
  {
    /*
      Push the NDB error message as warning
      - Used to be able to use SHOW WARNINGS toget more info on what the error is
      - Used by replication to see if the error was temporary
    */
    if (ndberr->status == NdbError::TemporaryError)
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_GET_TEMPORARY_ERRMSG, ER(ER_GET_TEMPORARY_ERRMSG),
                          ndberr->code, ndberr->message, "NDB");
    else
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          ndberr->code, ndberr->message, "NDB");
  }
  return error;
}

#ifdef HAVE_NDB_BINLOG

int
handle_conflict_op_error(NdbTransaction* trans,
                         const NdbError& err,
                         const NdbOperation* op);

int
handle_row_conflict(NDB_CONFLICT_FN_SHARE* cfn_share,
                    const char* tab_name,
                    bool table_has_blobs,
                    const char* handling_type,
                    const NdbRecord* key_rec,
                    const uchar* pk_row,
                    enum_conflicting_op_type op_type,
                    enum_conflict_cause conflict_cause,
                    const NdbError& conflict_error,
                    NdbTransaction* conflict_trans);
#endif

static const Uint32 error_op_after_refresh_op = 920;

inline int
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
#ifdef HAVE_NDB_BINLOG
  const NdbOperation* lastUserOp = trans->getLastDefinedOperation();
#endif
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
#ifdef HAVE_NDB_BINLOG
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
#endif
    if (err.classification != NdbError::NoError)
      ignores++;

    if (first == last)
      break;

    first= trans->getNextCompletedOperation(first);
  }
  if (ignore_count)
    *ignore_count= ignores;
#ifdef HAVE_NDB_BINLOG
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
        my_snprintf(msg, sizeof(msg), "Executing extra operations for "
                    "conflict handling hit Ndb error %d '%s'",
                    nonMaskedError.code, nonMaskedError.message);
        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_ERROR,
                            ER_EXCEPTIONS_WRITE_ERROR,
                            ER(ER_EXCEPTIONS_WRITE_ERROR), msg);
        /* Slave will stop replication. */
        DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
      }
    }
  }
#endif
  DBUG_RETURN(0);
}

inline int
check_completed_operations(Thd_ndb *thd_ndb, NdbTransaction *trans,
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
#ifdef HAVE_NDB_BINLOG
      /* All conflict detection etc should be done before commit */
      DBUG_ASSERT((err.code != (int) error_conflict_fn_violation) &&
                  (err.code != (int) error_op_after_refresh_op));
#endif
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

int execute_no_commit(THD* thd, Thd_ndb *thd_ndb, NdbTransaction *trans,
                      bool ignore_no_key,
                      uint *ignore_count= 0);
inline
int execute_no_commit(THD* thd, Thd_ndb *thd_ndb, NdbTransaction *trans,
                      bool ignore_no_key,
                      uint *ignore_count)
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

  if (unlikely(thd->slave_thread &&
               rc != 0))
  {
    g_ndb_slave_state.atTransactionAbort();
  }

  DBUG_PRINT("info", ("execute_no_commit rc is %d", rc));
  DBUG_RETURN(rc);
}

int execute_commit(THD* thd, Thd_ndb *thd_ndb, NdbTransaction *trans,
                   int force_send, int ignore_error, uint *ignore_count= 0);
inline
int execute_commit(THD* thd, Thd_ndb *thd_ndb, NdbTransaction *trans,
                   int force_send, int ignore_error, uint *ignore_count)
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

    rc= check_completed_operations(thd_ndb, trans, first, last,
                                   ignore_count);
  } while (0);

  if (thd->slave_thread)
  {
    if (likely(rc == 0))
    {
      /* Success */
      g_ndb_slave_state.atTransactionCommit();
    }
    else
    {
      g_ndb_slave_state.atTransactionAbort();
    }
  }

  DBUG_PRINT("info", ("execute_commit rc is %d", rc));
  DBUG_RETURN(rc);
}

inline
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
typedef struct st_thd_ndb_share {
  const void *key;
  struct Ndb_local_table_statistics stat;
} THD_NDB_SHARE;
static
uchar *thd_ndb_share_get_key(THD_NDB_SHARE *thd_ndb_share, size_t *length,
                            my_bool not_used __attribute__((unused)))
{
  *length= sizeof(thd_ndb_share->key);
  return (uchar*) &thd_ndb_share->key;
}

Thd_ndb::Thd_ndb(THD* thd) :
  m_thd(thd),
  schema_locks_count(0)
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
  m_error= FALSE;
  options= 0;
  (void) my_hash_init(&open_tables, table_alias_charset, 5, 0, 0,
                      (my_hash_get_key)thd_ndb_share_get_key, 0, 0);
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
  global_schema_lock_trans= NULL;
  global_schema_lock_count= 0;
  global_schema_lock_error= 0;
  init_alloc_root(&m_batch_mem_root, BATCH_FLUSH_SIZE/4, 0);
}

Thd_ndb::~Thd_ndb()
{
  if (opt_ndb_extra_logging > 1)
  {
    /*
      print some stats about the connection at disconnect
    */
    for (int i= 0; i < MAX_NDB_NODES; i++)
    {
      if (m_transaction_hint_count[i] > 0 ||
          m_transaction_no_hint_count[i] > 0)
      {
        sql_print_information("tid %u: node[%u] "
                              "transaction_hint=%u, transaction_no_hint=%u",
                              (unsigned)current_thd->thread_id, i,
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
  changed_tables.empty();
  my_hash_free(&open_tables);
  free_root(&m_batch_mem_root, MYF(0));
}


inline
Ndb *ha_ndbcluster::get_ndb(THD *thd) const
{
  return thd_get_thd_ndb(thd)->ndb;
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
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
      key_info->rec_per_key[key_info->user_defined_key_parts-1]= 1;
    }
  }
  DBUG_VOID_RETURN;
}

ha_rows ha_ndbcluster::records()
{
  DBUG_ENTER("ha_ndbcluster::records");
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      m_table_info->no_uncommitted_rows_count));

  if (update_stats(table->in_use, 1) == 0)
  {
    DBUG_RETURN(stats.records);
  }
  else
  {
    DBUG_RETURN(HA_POS_ERROR);
  }
}

void ha_ndbcluster::no_uncommitted_rows_execute_failure()
{
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_execute_failure");
  get_thd_ndb(current_thd)->m_error= TRUE;
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_update(int c)
{
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_update");
  struct Ndb_local_table_statistics *local_info= m_table_info;
  local_info->no_uncommitted_rows_count+= c;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      local_info->no_uncommitted_rows_count));
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_reset(THD *thd)
{
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_reset");
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  thd_ndb->count++;
  thd_ndb->m_error= FALSE;
  thd_ndb->m_unsent_bytes= 0;
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::ndb_err(NdbTransaction *trans,
                           bool have_lock)
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
    TABLE_LIST table_list;
    memset(&table_list, 0, sizeof(table_list));
    table_list.db= m_dbname;
    table_list.alias= table_list.table_name= m_tabname;
    close_cached_tables(thd, &table_list, have_lock, FALSE, FALSE);
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


/**
  Override the default get_error_message in order to add the 
  error message of NDB .
*/

bool ha_ndbcluster::get_error_message(int error, 
                                      String *buf)
{
  DBUG_ENTER("ha_ndbcluster::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  Ndb *ndb= check_ndb_in_thd(current_thd);
  if (!ndb)
    DBUG_RETURN(FALSE);

  const NdbError err= ndb->getNdbError(error);
  bool temporary= err.status==NdbError::TemporaryError;
  buf->set(err.message, (uint32)strlen(err.message), &my_charset_bin);
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
    return TRUE;
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    return FALSE;
  default:
    return FALSE;
  }
}

/*
 * This is used for every additional row operation, to update the guesstimate
 * of pending bytes to send, and to check if it is now time to flush a batch.
 */
bool
ha_ndbcluster::add_row_check_if_batch_full_size(Thd_ndb *thd_ndb, uint size)
{
  if (thd_ndb->m_unsent_bytes == 0)
    free_root(&(thd_ndb->m_batch_mem_root), MY_MARK_BLOCKS_FREE);

  uint unsent= thd_ndb->m_unsent_bytes;
  unsent+= size;
  thd_ndb->m_unsent_bytes= unsent;
  return unsent >= thd_ndb->m_batch_size;
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
  uchar *row= get_buffer(thd_ndb, table->s->reclength);
  if (unlikely(!row))
    return NULL;
  memcpy(row, record, table->s->reclength);
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
      DBUG_ASSERT(FALSE);
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
    my_free(ha->m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
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
      (uchar*) my_malloc((size_t) ha->m_blobs_row_total_size, MYF(MY_WME));
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
    if (! (field->flags & BLOB_FLAG))
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
      if (! (field->flags & BLOB_FLAG))
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
    if (!(field->flags & BLOB_FLAG))
      continue;

    DBUG_PRINT("info", ("fieldnr=%d", i));
    NdbBlob *ndb_blob;
    if (bitmap_is_set(bitmap, i))
    {
      if ((ndb_blob= ndb_op->getBlobHandle(i)) == NULL ||
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

    NdbBlob *ndb_blob= ndb_op->getBlobHandle(field_no);
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
    return FALSE;

  blob_index=     table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    if (bitmap_is_set(bitmap, table->field[*blob_index]->field_index))
      return TRUE;
  } while (++blob_index != blob_index_end);
  return FALSE;
}

void ha_ndbcluster::release_blobs_buffer()
{
  DBUG_ENTER("releaseBlobsBuffer");
  if (m_blobs_buffer_size > 0)
  {
    DBUG_PRINT("info", ("Deleting blobs buffer, size %llu", m_blobs_buffer_size));
    my_free(m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
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
      const NdbDictionary::Column* ndbCol= ndbtab->getColumn(field->field_index); 

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
          DBUG_PRINT("info", ("Default values differ for column %u",
                              field->field_index));
          break;
        }
      }
      else
      {
        /* We don't expect Ndb to have a native default for this column */
        if (unlikely(ndbCol->getDefaultValue() != NULL))
        {
          /* Didn't expect that */
          DBUG_PRINT("info", ("Column %u has native default, but shouldn't."
                              " Flags=%u, type=%u",
                              field->field_index, field->flags, field->real_type()));
          defaults_aligned= false;
          break;
        }
      }
    } 
    tmp_restore_column_map(table->read_set, old_map);
  }

  return (defaults_aligned? 0: -1);
}

int ha_ndbcluster::get_metadata(THD *thd, const char *path)
{
  Ndb *ndb= get_thd_ndb(thd)->ndb;
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *tab;
  int error;
  DBUG_ENTER("get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s, path: %s", m_tabname, path));

  DBUG_ASSERT(m_table == NULL);
  DBUG_ASSERT(m_table_info == NULL);

  uchar *data= NULL, *pack_data= NULL;
  size_t length, pack_length;

  /*
    Compare FrmData in NDB with frm file from disk.
  */
  error= 0;
  if (readfrm(path, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    my_free(data, MYF(MY_ALLOW_ZERO_PTR));
    my_free(pack_data, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }

  ndb->setDatabaseName(m_dbname);
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

  if (get_ndb_share_state(m_share) != NSS_ALTERED 
      && cmp_frm(tab, pack_data, pack_length))
  {
    DBUG_PRINT("error", 
               ("metadata, pack_length: %lu  getFrmLength: %d  memcmp: %d",
                (ulong) pack_length, tab->getFrmLength(),
                memcmp(pack_data, tab->getFrmData(), pack_length)));
    DBUG_DUMP("pack_data", (uchar*) pack_data, pack_length);
    DBUG_DUMP("frm", (uchar*) tab->getFrmData(), tab->getFrmLength());
    error= HA_ERR_TABLE_DEF_CHANGED;
  }
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));

  /* Now check that any Ndb native defaults are aligned 
     with MySQLD defaults
  */
  DBUG_ASSERT(check_default_values(tab) == 0);

  if (error)
    goto err;

  DBUG_PRINT("info", ("fetched table %s", tab->getName()));
  m_table= tab;

  if (bitmap_init(&m_bitmap, m_bitmap_buf, table_share->fields, 0))
  {
    error= HA_ERR_OUT_OF_MEM;
    goto err;
  }
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
  if ((error= open_indexes(thd, ndb, table, FALSE)) != 0)
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
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  data.unique_index_attrid_map= (uchar*)my_malloc(sz,MYF(MY_WME));
  if (data.unique_index_attrid_map == 0)
  {
    sql_print_error("fix_unique_index_attr_order: my_malloc(%u) failure",
                    (unsigned int)sz);
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
int ha_ndbcluster::create_indexes(THD *thd, Ndb *ndb, TABLE *tab) const
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::create_indexes");

  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    error= create_index(thd, index_name, key_info, idx_type, i);
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
  data.status= UNDEFINED;
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
    my_free((char*)data.unique_index_attrid_map, MYF(0));
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
int ha_ndbcluster::add_index_handle(THD *thd, NDBDICT *dict, KEY *key_info,
                                    const char *key_name, uint index_no)
{
  char index_name[FN_LEN + 1];
  int error= 0;

  NDB_INDEX_TYPE idx_type= get_index_type_from_table(index_no);
  m_index[index_no].type= idx_type;
  DBUG_ENTER("ha_ndbcluster::add_index_handle");
  DBUG_PRINT("enter", ("table %s", m_tabname));
  
  ndb_protect_char(key_name, index_name, sizeof(index_name) - 1, '/');
  if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX)
  {
    DBUG_PRINT("info", ("Get handle to index %s", index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(index_name, *m_table);
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
      break;
    } while (1);
    m_index[index_no].index= index;
  }
  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    char unique_index_name[FN_LEN + 1];
    static const char* unique_suffix= "$unique";
    m_has_unique_index= TRUE;
    strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(unique_index_name, *m_table);
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
      break;
    } while (1);
    m_index[index_no].unique_index= index;
    error= fix_unique_index_attr_order(m_index[index_no], index, key_info);
  }

  if (!error)
    error= add_index_ndb_record(dict, key_info, index_no);

  if (!error)
    m_index[index_no].status= ACTIVE;
  
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
                             const NdbDictionary::Table *ndb_table)
{
  spec->column= ndb_table->getColumn(field_no);
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
}

int
ha_ndbcluster::add_table_ndb_record(NDBDICT *dict)
{
  DBUG_ENTER("ha_ndbcluster::add_table_ndb_record()");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;
  uint i;

  for (i= 0; i < table_share->fields; i++)
  {
    ndb_set_record_specification(i, &spec[i], table, m_table);
  }

  rec= dict->createRecord(m_table, spec, i, sizeof(spec[0]),
                          NdbDictionary::RecMysqldBitfield);
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

  spec[0].column= m_table->getColumn(table_share->fields);
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

    spec[i].column= m_table->getColumn(kp->fieldnr - 1);
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
int ha_ndbcluster::open_indexes(THD *thd, Ndb *ndb, TABLE *tab,
                                bool ignore_error)
{
  uint i;
  int error= 0;
  NDBDICT *dict= ndb->getDictionary();
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::open_indexes");
  m_has_unique_index= FALSE;
  btree_keys.clear_all();
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    if ((error= add_index_handle(thd, dict, key_info, *key_name, i)))
    {
      if (ignore_error)
        m_index[i].index= m_index[i].unique_index= NULL;
      else
        break;
    }
    m_index[i].null_in_unique_index= FALSE;
    if (check_index_fields_not_null(key_info))
      m_index[i].null_in_unique_index= TRUE;

    if (error == 0 && test(index_flags(i, 0, 0) & HA_READ_RANGE))
      btree_keys.set_bit(i);
  }

  if (error && !ignore_error)
  {
    while (i > 0)
    {
      i--;
      if (m_index[i].index)
      {
         dict->removeIndexGlobal(*m_index[i].index, 1);
         m_index[i].index= NULL;
      }
      if (m_index[i].unique_index)
      {
         dict->removeIndexGlobal(*m_index[i].unique_index, 1);
         m_index[i].unique_index= NULL;
      }
    }
  }

  DBUG_ASSERT(error == 0 || error == 4243);

  DBUG_RETURN(error);
}

/*
  Renumber indexes in index list by shifting out
  indexes that are to be dropped
 */
void ha_ndbcluster::renumber_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::renumber_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED) 
    {
      DBUG_PRINT("info", ("Shifting index %s(%i) out of the list", 
                          index_name, i));
      NDB_INDEX_DATA tmp;
      uint j= i + 1;
      // Shift index out of list
      while(j != MAX_KEY && m_index[j].status != UNDEFINED)
      {
        tmp=  m_index[j - 1];
        m_index[j - 1]= m_index[j];
        m_index[j]= tmp;
        j++;
      }
    }
  }

  DBUG_VOID_RETURN;
}

/*
  Drop all indexes that are marked for deletion
*/
int ha_ndbcluster::drop_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::drop_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      const NdbDictionary::Index *unique_index= m_index[i].unique_index;
      
      if (index)
      {
        index_name= index->getName();
        DBUG_PRINT("info", ("Dropping index %u: %s", i, index_name));  
        // Drop ordered index from ndb
        error= dict->dropIndexGlobal(*index);
        if (!error)
        {
          dict->removeIndexGlobal(*index, 1);
          m_index[i].index= NULL;
        }
      }
      if (!error && unique_index)
      {
        index_name= unique_index->getName();
        DBUG_PRINT("info", ("Dropping unique index %u: %s", i, index_name));
        // Drop unique index from ndb
        error= dict->dropIndexGlobal(*unique_index);
        if (!error)
        {
          dict->removeIndexGlobal(*unique_index, 1);
          m_index[i].unique_index= NULL;
        }
      }
      if (error)
        DBUG_RETURN(error);
      ndb_clear_index(dict, m_index[i]);
      continue;
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
	DBUG_RETURN(TRUE);
    }
  
  DBUG_RETURN(FALSE);
}

void ha_ndbcluster::release_metadata(THD *thd, Ndb *ndb)
{
  uint i;

  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  NDBDICT *dict= ndb->getDictionary();
  int invalidate_indexes= 0;
  if (thd && thd->lex && thd->lex->sql_command == SQLCOM_FLUSH)
  {
    invalidate_indexes = 1;
  }
  if (m_table != NULL)
  {
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
  }
  // TODO investigate
  DBUG_ASSERT(m_table_info == NULL);
  m_table_info= NULL;

  // Release index list 
  for (i= 0; i < MAX_KEY; i++)
  {
    if (m_index[i].unique_index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].unique_index, invalidate_indexes);
    }
    if (m_index[i].index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].index, invalidate_indexes);
    }
    ndb_clear_index(dict, m_index[i]);
  }

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

inline ulong ha_ndbcluster::index_flags(uint idx_no, uint part,
                                        bool all_parts) const 
{ 
  DBUG_ENTER("ha_ndbcluster::index_flags");
  DBUG_PRINT("enter", ("idx_no: %u", idx_no));
  DBUG_ASSERT(get_index_type_from_table(idx_no) < index_flags_size);
  DBUG_RETURN(index_type_flags[get_index_type_from_table(idx_no)] | 
              HA_KEY_SCAN_NOT_ROR);
}

bool
ha_ndbcluster::primary_key_is_clustered()
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
  const ndb_index_type idx_type =
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

int ha_ndbcluster::pk_read(const uchar *key, uint key_len, uchar *buf,
                           uint32 *part_id)
{
  NdbConnection *trans= m_thd_ndb->trans;
  int res;
  DBUG_ENTER("pk_read");
  DBUG_PRINT("enter", ("key_len: %u read_set=%x",
                       key_len, table->read_set->bitmap[0]));
  DBUG_DUMP("key", key, key_len);
  DBUG_ASSERT(trans);

  NdbOperation::LockMode lm= get_ndb_lock_mode(m_lock.type);

  if (check_if_pushable(NdbQueryOperationDef::PrimaryKeyAccess,
                        table->s->primary_key))
  {
    // Is parent of pushed join
    DBUG_ASSERT(lm == NdbOperation::LM_CommittedRead);
    const int error= pk_unique_index_read_key_pushed(table->s->primary_key, key,
                                                     (m_user_defined_partitioning ?
                                                     part_id : NULL));
    if (unlikely(error))
      DBUG_RETURN(error);

    DBUG_ASSERT(m_active_query!=NULL);
    if ((res = execute_no_commit_ie(m_thd_ndb, trans)) != 0 ||
        m_active_query->getNdbError().code) 
    {
      table->status= STATUS_NOT_FOUND;
      DBUG_RETURN(ndb_err(trans));
    }

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

    if ((res = execute_no_commit_ie(m_thd_ndb, trans)) != 0 ||
        op->getNdbError().code) 
    {
      table->status= STATUS_NOT_FOUND;
      DBUG_RETURN(ndb_err(trans));
    }
    table->status= 0;     
    DBUG_RETURN(0);
  }
}

/**
  Update primary key or part id by doing delete insert.
*/

int ha_ndbcluster::ndb_pk_update_row(THD *thd,
                                     const uchar *old_data, uchar *new_data,
                                     uint32 old_part_id)
{
  NdbTransaction *trans= m_thd_ndb->trans;
  int error;
  const NdbOperation *op;
  DBUG_ENTER("ndb_pk_update_row");
  DBUG_ASSERT(trans);

  NdbOperation::OperationOptions *poptions = NULL;
  NdbOperation::OperationOptions options;
  options.optionsPresent=0;

  DBUG_PRINT("info", ("primary key update or partition change, "
                      "doing read+delete+insert"));
  // Get all old fields, since we optimize away fields not in query

  const NdbRecord *key_rec;
  const uchar *key_row;

  if (m_user_defined_partitioning)
  {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId=old_part_id;
    poptions=&options;
  }

  setup_key_ref_for_ndb_record(&key_rec, &key_row, old_data, FALSE);

  if (!bitmap_is_set_all(table->read_set))
  {
    /*
      Need to read rest of columns for later re-insert.

      Use mask only with columns that are not in write_set, not in
      read_set, and not part of the primary key.
    */

    bitmap_copy(&m_bitmap, table->read_set);
    bitmap_union(&m_bitmap, table->write_set);
    bitmap_invert(&m_bitmap);
    if (!(op= trans->readTuple(key_rec, (const char *)key_row,
                               m_ndb_record, (char *)new_data,
                               get_ndb_lock_mode(m_lock.type),
                               (const unsigned char *)(m_bitmap.bitmap),
                               poptions,
                               sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());

    if (table_share->blob_fields > 0)
    {
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
      error= get_blob_values(op, new_data, &m_bitmap);
      dbug_tmp_restore_column_map(table->read_set, old_map);
      if (error != 0)
        ERR_RETURN(op->getNdbError());
    }
    if (execute_no_commit(thd, m_thd_ndb, trans, m_ignore_no_key) != 0)
    {
      table->status= STATUS_NOT_FOUND;
      DBUG_RETURN(ndb_err(trans));
    }
  }

  // Delete old row
  error= ndb_delete_row(old_data, TRUE);
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
  error= ndb_write_row(new_data, TRUE, batched_update);
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
#ifdef FIXED_OLD_DATA_TO_ACTUALLY_CONTAIN_GOOD_DATA
      int undo_res;
      // Undo delete_row(old_data)
      undo_res= ndb_write_row((uchar *)old_data, TRUE, batched_update);
      if (undo_res)
        push_warning(table->in_use,
                     Sql_condition::WARN_LEVEL_WARN,
                     undo_res,
                     "NDB failed undoing delete at primary key update");
#endif
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
        DBUG_RETURN(FALSE);
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
      DBUG_RETURN(FALSE);      
    }
  }
  DBUG_RETURN(TRUE);
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
  int res, error;
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
    res= execute_no_commit_ie(m_thd_ndb, trans);
  else
  {
    // Table has no keys
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
  }
  if (check_all_operations_for_error(trans, first, last, 
                                     HA_ERR_KEY_NOT_FOUND))
  {
    table->status= STATUS_NOT_FOUND;
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

int ha_ndbcluster::unique_index_read(const uchar *key,
                                     uint key_len, uchar *buf)
{
  NdbTransaction *trans= m_thd_ndb->trans;
  DBUG_ENTER("ha_ndbcluster::unique_index_read");
  DBUG_PRINT("enter", ("key_len: %u, index: %u", key_len, active_index));
  DBUG_DUMP("key", key, key_len);
  DBUG_ASSERT(trans);

  NdbOperation::LockMode lm= get_ndb_lock_mode(m_lock.type);

  if (check_if_pushable(NdbQueryOperationDef::UniqueIndexAccess,
                        active_index))
  {
    DBUG_ASSERT(lm == NdbOperation::LM_CommittedRead);
    const int error= pk_unique_index_read_key_pushed(active_index, key, NULL);
    if (unlikely(error))
      DBUG_RETURN(error);

    DBUG_ASSERT(m_active_query!=NULL);
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        m_active_query->getNdbError().code) 
    {
      table->status= STATUS_GARBAGE;
      DBUG_RETURN(ndb_err(trans));
    }

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
      if(err==HA_ERR_KEY_NOT_FOUND)
        table->status= STATUS_NOT_FOUND;
      else
        table->status= STATUS_GARBAGE;

      DBUG_RETURN(err);
    }

    table->status= 0;
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
    const NdbOperation *op;
    // Lock row
    DBUG_PRINT("info", ("Keeping lock on scanned row"));
      
    if (!(op= scanOp->lockCurrentTuple(trans, m_ndb_record,
                                       dummy_row, empty_mask)))
    {
      /* purecov: begin inspected */
      m_lock_tuple= FALSE;
      ERR_RETURN(trans->getNdbError());
      /* purecov: end */    
    }
    m_thd_ndb->m_unsent_bytes+=12;
  }
  m_lock_tuple= FALSE;
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
      if (execute_no_commit(table->in_use, m_thd_ndb, trans, m_ignore_no_key) != 0)
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
    table->status= 0;
    unpack_record(table->record[0], m_next_row);
//  m_thd_ndb->m_pushed_reads++;
//  DBUG_RETURN(0)
  }
  else if (result == NdbQuery::NextResult_scanComplete)
  {
    DBUG_ASSERT(m_next_row==NULL);
    DBUG_PRINT("info", ("No more records"));
    table->status= STATUS_NOT_FOUND;
//  m_thd_ndb->m_pushed_reads++;
//  DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  else
  {
    DBUG_PRINT("info", ("Error from 'nextResult()'"));
    table->status= STATUS_GARBAGE;
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
    DBUG_RETURN(index_read_map(buf, key, keypart_map, HA_READ_KEY_EXACT));
  }

  // Might need to re-establish first result row (wrt. its parents which may have been navigated)
  NdbQuery::NextResultOutcome result= m_pushed_operation->firstResult();

  // Result from pushed operation will be referred by 'm_next_row' if non-NULL
  if (result == NdbQuery::NextResult_gotRow)
  {
    DBUG_ASSERT(m_next_row!=NULL);
    unpack_record(buf, m_next_row);
    table->status= 0;
    m_thd_ndb->m_pushed_reads++;
  }
  else
  {
    DBUG_ASSERT(result!=NdbQuery::NextResult_gotRow);
    table->status= STATUS_NOT_FOUND;
    DBUG_PRINT("info", ("No record found"));
//  m_thd_ndb->m_pushed_reads++;
//  DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  DBUG_RETURN(0);
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
    DBUG_RETURN(index_next(buf));
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
      table->status= 0;
      DBUG_RETURN(0);
    }
    else if (res == 1)
    {
      // No more records
      table->status= STATUS_NOT_FOUND;
      
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

/**
  Do a primary key or unique key index read operation.
  The key value is taken from a buffer in mysqld key format.
*/
const NdbOperation *
ha_ndbcluster::pk_unique_index_read_key(uint idx, const uchar *key, uchar *buf,
                                        NdbOperation::LockMode lm,
                                        Uint32 *ppartition_id)
{
  const NdbOperation *op;
  const NdbRecord *key_rec;
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent= 0;
  NdbOperation::GetValueSpec gets[2];

  DBUG_ASSERT(m_thd_ndb->trans);

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

  if (ppartition_id != NULL)
  {
    assert(m_user_defined_partitioning);
    options.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId= *ppartition_id;
    poptions= &options;
  }

  op= m_thd_ndb->trans->readTuple(key_rec, (const char *)key, m_ndb_record,
                                  (char *)buf, lm,
                                  (uchar *)(table->read_set->bitmap), poptions,
                                  sizeof(NdbOperation::OperationOptions));

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, buf, table->read_set) != 0)
    return NULL;

  return op;
}

extern void sql_print_information(const char *format, ...);

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
ha_ndbcluster::pk_unique_index_read_key_pushed(uint idx, 
                                               const uchar *key, 
                                               Uint32 *ppartition_id)
{
  DBUG_ENTER("pk_unique_index_read_key_pushed");
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent= 0;
  NdbOperation::GetValueSpec gets[2];

  DBUG_ASSERT(m_thd_ndb->trans);
  DBUG_ASSERT(idx < MAX_KEY);

  if (m_active_query)
  {
    m_active_query->close(FALSE);
    m_active_query= NULL;
  }

  if (table_share->primary_key == MAX_KEY)
  {
    get_hidden_fields_keyop(&options, gets);
    poptions= &options;
  }

  if (ppartition_id != NULL)
  {
    assert(m_user_defined_partitioning);
    options.optionsPresent|= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId= *ppartition_id;
    poptions= &options;
  }

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
    bound.high_inclusive= TRUE;
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
      bound.low_inclusive= TRUE;
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

  // Check that sorted seems to be initialised
  DBUG_ASSERT(sorted == 0 || sorted == 1);

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

    DBUG_ASSERT(!uses_blob_value(table->read_set));  // Can't have BLOB in pushed joins (yet)
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
                               (uchar *)(table->read_set->bitmap),
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

  if (execute_no_commit(table->in_use, m_thd_ndb, trans, m_ignore_no_key) != 0)
    DBUG_RETURN(ndb_err(trans));
  
  DBUG_RETURN(next_result(buf));
}

static
int
guess_scan_flags(NdbOperation::LockMode lm,
		 const NDBTAB* tab, const MY_BITMAP* readset)
{
  int flags= 0;
  flags|= (lm == NdbOperation::LM_Read) ? NdbScanOperation::SF_KeyInfo : 0;
  if (tab->checkColumns(0, 0) & 2)
  {
    int ret = tab->checkColumns(readset->bitmap, no_bytes_in_map(readset));
    
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
  bool use_set_part_id= FALSE;
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
      use_set_part_id= TRUE;
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
  options.scan_flags = guess_scan_flags(lm, m_table, table->read_set);
  options.parallel= DEFAULT_PARALLELISM;

  if (use_set_part_id) {
    assert(m_user_defined_partitioning);
    options.optionsPresent|= NdbScanOperation::ScanOptions::SO_PARTITION_ID;
    options.partitionId = part_spec.start_part;
  };

  if (table_share->primary_key == MAX_KEY)
    get_hidden_fields_scan(&options, gets);

  if (check_if_pushable(NdbQueryOperationDef::TableScan))
  {
    const int error= create_pushed_join();
    if (unlikely(error))
      DBUG_RETURN(error);

    m_thd_ndb->m_scan_count++;
    DBUG_ASSERT(!uses_blob_value(table->read_set));  // Can't have BLOB in pushed joins (yet)
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
      if (!m_cond)
        m_cond= new ha_ndbcluster_cond;

      if (!m_cond)
      {
        my_errno= HA_ERR_OUT_OF_MEM;
        DBUG_RETURN(my_errno);
      }       
      if (m_cond->generate_scan_filter_from_key(&code, &options, key_info, start_key, end_key, buf))
        ERR_RETURN(code.getNdbError());
    }

    if (!(op= trans->scanTable(m_ndb_record, lm,
                               (uchar *)(table->read_set->bitmap),
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
  
  if (execute_no_commit(table->in_use, m_thd_ndb, trans, m_ignore_no_key) != 0)
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


class Ndb_tuple_id_range_guard {
  NDB_SHARE* m_share;
public:
  Ndb_tuple_id_range_guard(NDB_SHARE* share) :
    m_share(share),
    range(share->tuple_id_range)
  {
    pthread_mutex_lock(&m_share->mutex);
  }
  ~Ndb_tuple_id_range_guard()
  {
    pthread_mutex_unlock(&m_share->mutex);
  }
  Ndb::TupleIdRange& range;
};


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
  if (ndb->checkUpdateAutoIncrementValue(m_share->tuple_id_range, value))
  {
    Ndb_tuple_id_range_guard g(m_share);
    if (ndb->setAutoIncrementValue(m_table, g.range, value, TRUE)
        == -1)
      ERR_RETURN(ndb->getNdbError());
  }
  DBUG_RETURN(0);
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
    else if (thd_ndb->trans_options & TNTO_NO_LOGGING)
    {
      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_nologging(options->anyValue);
    }
  }
#ifndef DBUG_OFF
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

static inline bool
thd_allow_batch(const THD* thd)
{
  return (thd_options(thd) & OPTION_ALLOW_BATCH);
}


#ifdef HAVE_NDB_BINLOG

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
                                          const uchar* old_data,
                                          const uchar* new_data,
                                          NdbTransaction* trans,
                                          NdbInterpretedCode* code,
                                          NdbOperation::OperationOptions* options,
                                          bool& conflict_handled)
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
  if (thd->binlog_row_event_extra_data)
  {
    Ndb_binlog_extra_row_info extra_row_info;
    extra_row_info.loadFromBuffer(thd->binlog_row_event_extra_data);
    if (extra_row_info.getFlags() &
        Ndb_binlog_extra_row_info::NDB_ERIF_TRANSID)
      transaction_id = extra_row_info.getTransactionId();
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
      const uchar* row_to_save = (op_type == DELETE_ROW)? old_data : new_data;

      /*
         Directly handle the conflict here - e.g refresh/ write to
         exceptions table etc.
      */
      res = handle_row_conflict(m_share->m_cfn_share,
                                m_share->table_name,
                                m_share->flags & NSF_BLOB_FLAG,
                                "Transaction",
                                key_rec,
                                row_to_save,
                                op_type,
                                TRANS_IN_CONFLICT,
                                noRealConflictError,
                                trans);
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

  if (! (m_share->m_cfn_share &&
         m_share->m_cfn_share->m_conflict_fn))
  {
    /* No conflict function definition required */
    DBUG_RETURN(0);
  }

  const st_conflict_fn_def* conflict_fn = m_share->m_cfn_share->m_conflict_fn;
  assert( conflict_fn != NULL );

  if (unlikely((conflict_fn->flags & CF_TRANSACTIONAL) &&
               (transaction_id == Ndb_binlog_extra_row_info::InvalidTransactionId)))
  {
    sql_print_warning("NDB Slave : Transactional conflict detection defined on table %s, but "
                      "events received without transaction ids.  Check --ndb-log-transaction-id setting "
                      "on upstream Cluster.",
                      m_share->key);
    /* This is a user error, but we want them to notice, so treat seriously */
    DBUG_RETURN( ER_SLAVE_CORRUPT_EVENT );
  }

  /*
     Prepare interpreted code for operation (update + delete only) according
     to algorithm used
  */
  if (op_type != WRITE_ROW)
  {
    res = conflict_fn->prep_func(m_share->m_cfn_share,
                                 op_type,
                                 m_ndb_record,
                                 old_data,
                                 new_data,
                                 table->write_set,
                                 code);

    if (!res)
    {
      /* Attach conflict detecting filter program to operation */
      options->optionsPresent|=NdbOperation::OperationOptions::OO_INTERPRETED;
      options->interpretedCode= code;
    }
  } // if (op_type != WRITE_ROW)

  g_ndb_slave_state.conflict_flags |= SCS_OPS_DEFINED;

  /* Now save data for potential insert to exceptions table... */
  const uchar* row_to_save = (op_type == DELETE_ROW)? old_data : new_data;
  Ndb_exceptions_data ex_data;
  ex_data.share= m_share;
  ex_data.key_rec= key_rec;
  ex_data.op_type= op_type;
  ex_data.trans_id= transaction_id;
  /*
    We need to save the row data for possible conflict resolution after
    execute().
  */
  ex_data.row= copy_row_to_buffer(m_thd_ndb, row_to_save);
  uchar* ex_data_buffer= get_buffer(m_thd_ndb, sizeof(ex_data));
  if (ex_data.row == NULL || ex_data_buffer == NULL)
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

int
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
    const NdbRecord* key_rec= ex_data.key_rec;
    const uchar* row= ex_data.row;
    enum_conflicting_op_type causing_op_type = ex_data.op_type;

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

    DBUG_ASSERT(share != NULL && row != NULL);
    NDB_CONFLICT_FN_SHARE* cfn_share= share->m_cfn_share;
    bool table_has_trans_conflict_detection =
      cfn_share &&
      cfn_share->m_conflict_fn &&
      (cfn_share->m_conflict_fn->flags & CF_TRANSACTIONAL);

    if (table_has_trans_conflict_detection)
    {
      /* Perform special transactional conflict-detected handling */
      int res = g_ndb_slave_state.atTransConflictDetected(ex_data.trans_id);
      if (res)
        DBUG_RETURN(res);
    }

    if (cfn_share)
    {
      /* Now handle the conflict on this row */
      enum_conflict_fn_type cft = cfn_share->m_conflict_fn->type;

      g_ndb_slave_state.current_violation_count[cft]++;

      int res = handle_row_conflict(cfn_share,
                                    share->table_name,
                                    false, /* table_has_blobs */
                                    "Row",
                                    key_rec,
                                    row,
                                    causing_op_type,
                                    conflict_cause,
                                    err,
                                    trans);

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
#endif

int ha_ndbcluster::write_row(uchar *record)
{
  DBUG_ENTER("ha_ndbcluster::write_row");
#ifdef HAVE_NDB_BINLOG
  if (m_share == ndb_apply_status_share && table->in_use->slave_thread)
  {
    uint32 row_server_id, master_server_id= ndb_mi_get_master_server_id();
    uint64 row_epoch;
    memcpy(&row_server_id, table->field[0]->ptr + (record - table->record[0]),
           sizeof(row_server_id));
    memcpy(&row_epoch, table->field[1]->ptr + (record - table->record[0]),
           sizeof(row_epoch));
    g_ndb_slave_state.atApplyStatusWrite(master_server_id,
                                         row_server_id,
                                         row_epoch,
                                         is_serverid_local(row_server_id));
  }
#endif /* HAVE_NDB_BINLOG */
  DBUG_RETURN(ndb_write_row(record, FALSE, FALSE));
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
    m_skip_auto_increment= FALSE;
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
      m_skip_auto_increment= TRUE;
      DBUG_RETURN(error);
    }
  }

  bool uses_blobs= uses_blob_value(table->write_set);

  Uint64 auto_value;
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
      Ndb_tuple_id_range_guard g(m_share);
      if (ndb->getAutoIncrementValue(m_table, g.range, auto_value, 1000) == -1)
      {
	if (--retries && !thd->killed &&
	    ndb->getNdbError().status == NdbError::TemporaryError)
	{
	  do_retry_sleep(retry_sleep);
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
    longlong func_value= 0;
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

  ha_statistic_increment(&SSV::ha_write_count);

  /*
     Setup OperationOptions
   */
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = NULL;
  options.optionsPresent=0;
  
  eventSetAnyValue(thd, &options); 
  bool need_flush= add_row_check_if_batch_full(thd_ndb);

  const Uint32 authorValue = 1;
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

  if (options.optionsPresent != 0)
    poptions=&options;

  const Uint32 bitmapSz= (NDB_MAX_ATTRIBUTES_IN_TABLE + 31)/32;
  uint32 tmpBitmapSpace[bitmapSz];
  MY_BITMAP tmpBitmap;
  MY_BITMAP *user_cols_written_bitmap;
#ifdef HAVE_NDB_BINLOG
  /* Conflict resolution in slave thread */
  bool haveConflictFunction = false;
  if (thd->slave_thread)
  {
    haveConflictFunction =
      m_share->m_cfn_share &&
      m_share->m_cfn_share->m_conflict_fn;
    bool conflict_handled = false;

    if (unlikely((error = prepare_conflict_detection(WRITE_ROW,
                                                     key_rec,
                                                     NULL,    /* old_data */
                                                     record,  /* new_data */
                                                     trans,
                                                     NULL,    /* code */
                                                     &options,
                                                     conflict_handled))))
      DBUG_RETURN(error);

    if (unlikely(conflict_handled))
    {
      /* No need to continue with operation definition */
      /* TODO : Ensure batch execution */
      DBUG_RETURN(0);
    }
  };
#endif
  if (m_use_write
#ifdef HAVE_NDB_BINLOG
      /* Conflict detection must use normal Insert */
      && !haveConflictFunction
#endif
      )
  {
    /* Should we use the supplied table writeset or not?
     * For a REPLACE command, we should ignore it, and write
     * all columns to get correct REPLACE behaviour.
     * For applying Binlog events, we need to use the writeset
     * to avoid trampling unchanged columns when an update is
     * logged as a WRITE
     */
    bool useWriteSet= applying_binlog(thd);
    uchar* mask;

    if (useWriteSet)
    {
      user_cols_written_bitmap= table->write_set;
      mask= (uchar *)(user_cols_written_bitmap->bitmap);
    }
    else
    {
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

      mask= (uchar *)(user_cols_written_bitmap->bitmap);
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

  m_rows_changed++;

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
    int res= flush_bulk_insert();
    if (res != 0)
    {
      m_skip_auto_increment= TRUE;
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
  m_skip_auto_increment= TRUE;

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

#ifdef HAVE_NDB_BINLOG

static Ndb_exceptions_data StaticRefreshExceptionsData=
{ NULL, NULL, NULL, REFRESH_ROW, 0 };

int
handle_row_conflict(NDB_CONFLICT_FN_SHARE* cfn_share,
                    const char* table_name,
                    bool table_has_blobs,
                    const char* handling_type,
                    const NdbRecord* key_rec,
                    const uchar* pk_row,
                    enum_conflicting_op_type op_type,
                    enum_conflict_cause conflict_cause,
                    const NdbError& conflict_error,
                    NdbTransaction* conflict_trans)
{
  DBUG_ENTER("handle_row_conflict");

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
    assert(pk_row != NULL);

    do
    {
      /* We cannot refresh a row which has Blobs, as we do not support
       * Blob refresh yet.
       * Rows implicated by a transactional conflict function may have
       * Blobs.
       * We will generate an error in this case
       */
      if (table_has_blobs)
      {
        char msg[FN_REFLEN];
        my_snprintf(msg, sizeof(msg), "%s conflict handling "
                    "on table %s failed as table has Blobs which cannot be refreshed.",
                    handling_type,
                    table_name);

        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_EXCEPTIONS_WRITE_ERROR,
                            ER(ER_EXCEPTIONS_WRITE_ERROR), msg);

        DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
      }

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

      /* When a delete operation finds that the row does not exist, it indicates
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
       */
      if ((op_type == DELETE_ROW) &&
          (conflict_cause == ROW_DOES_NOT_EXIST))
      {
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

      /* Create a refresh to operation to realign other clusters */
      // TODO AnyValue
      // TODO Do we ever get non-PK key?
      //      Keyless table?
      //      Unique index
      const NdbOperation* refresh_op= conflict_trans->refreshTuple(key_rec,
                                                                   (const char*) pk_row,
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
          my_snprintf(msg, sizeof(msg), "Row conflict handling "
                      "on table %s hit Ndb error %d '%s'",
                      table_name,
                      err.code,
                      err.message);
          push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_EXCEPTIONS_WRITE_ERROR,
                              ER(ER_EXCEPTIONS_WRITE_ERROR), msg);
          /* Slave will stop replication. */
          DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
        }
      }
    } while(0); // End of 'refresh' block
  }

  if (cfn_share &&
      cfn_share->m_ex_tab_writer.hasTable())
  {
    NdbError err;
    if (cfn_share->m_ex_tab_writer.writeRow(conflict_trans,
                                            key_rec,
                                            ::server_id,
                                            ndb_mi_get_master_server_id(),
                                            g_ndb_slave_state.current_master_server_epoch,
                                            pk_row,
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
          my_snprintf(msg, sizeof(msg), "%s conflict handling "
                      "on table %s hit Ndb error %d '%s'",
                      handling_type,
                      table_name,
                      err.code,
                      err.message);
          push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_EXCEPTIONS_WRITE_ERROR,
                              ER(ER_EXCEPTIONS_WRITE_ERROR), msg);
          /* Slave will stop replication. */
          DBUG_RETURN(ER_EXCEPTIONS_WRITE_ERROR);
        }
      }
    }
  } /* if (cfn_share->m_ex_tab != NULL) */

  DBUG_RETURN(0);
};
#endif /* HAVE_NDB_BINLOG */

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
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
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
         to the update loop(which will ask handler in rbwr mode)
    */
    DBUG_PRINT("info", ("committing auto-commit+rbwr early"));
    uint ignore_count= 0;
    const int ignore_error= 1;
    if (execute_commit(table->in_use, m_thd_ndb, trans,
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
      assert(m_rows_changed >= ignore_count);
      assert(m_rows_updated >= ignore_count);
      m_rows_changed-= ignore_count;
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
  THD *thd= table->in_use;
  if (execute_no_commit(thd, m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  if (!applying_binlog(thd))
  {
    assert(m_rows_changed >= ignore_count);
    assert(m_rows_updated >= ignore_count);
    m_rows_changed-= ignore_count;
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
    *key_rec= m_index[active_index].ndb_unique_record_row;
    *key_row= record;
  }
  else if (table_share->primary_key != MAX_KEY)
  {
    /* Use primary key to access table */
    DBUG_PRINT("info", ("Using primary key"));
    *key_rec= m_index[table_share->primary_key].ndb_unique_record_row;
    *key_row= record;
  }
  else
  {
    /* Use hidden primary key previously read into m_ref. */
    DBUG_PRINT("info", ("Using hidden primary key (%llu)", m_ref));
    /* Can't use hidden pk if we didn't read it first */
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
  NdbTransaction *trans= thd_ndb->trans;
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
    NDB_WRITE_OP write_op= (pk_update) ? NDB_PK_UPDATE : NDB_UPDATE;
    int peek_res= peek_indexed_rows(new_data, write_op);
    
    if (!peek_res) 
    {
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  ha_statistic_increment(&SSV::ha_update_count);

  bool skip_partition_for_unique_index= FALSE;
  if (m_use_partition_pruning)
  {
    if (!cursor && m_read_before_write_removal_used)
    {
      ndb_index_type type= get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for update
        without finding the partitions
      */
      if (type == UNIQUE_INDEX ||
          type == UNIQUE_ORDERED_INDEX)
      {
        skip_partition_for_unique_index= TRUE;
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
    DBUG_RETURN(ndb_pk_update_row(thd, old_data, new_data, old_part_id));
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
  uchar *mask= (uchar *)(m_bitmap.bitmap);
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
  
  bool need_flush= add_row_check_if_batch_full(thd_ndb);

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

    m_lock_tuple= FALSE;
    thd_ndb->m_unsent_bytes+= 12;
  }
  else
  {  
    const NdbRecord *key_rec;
    const uchar *key_row;
    setup_key_ref_for_ndb_record(&key_rec, &key_row, new_data,
				 m_read_before_write_removal_used);

#ifdef HAVE_NDB_BINLOG
    Uint32 buffer[ MAX_CONFLICT_INTERPRETED_PROG_SIZE ];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer)/sizeof(buffer[0]));

    if (thd->slave_thread)
    {
      bool conflict_handled = false;
      /* Conflict resolution in slave thread. */

      if (unlikely((error = prepare_conflict_detection(UPDATE_ROW,
                                                       key_rec,
                                                       old_data,
                                                       new_data,
                                                       trans,
                                                       &code,
                                                       &options,
                                                       conflict_handled))))
        DBUG_RETURN(error);

      if (unlikely(conflict_handled))
      {
        /* No need to continue with operation defintion */
        /* TODO : Ensure batch execution */
        DBUG_RETURN(0);
      }
    }
#endif /* HAVE_NDB_BINLOG */
    if (options.optionsPresent !=0)
      poptions= &options;

    if (!(op= trans->updateTuple(key_rec, (const char *)key_row,
                                 m_ndb_record, (const char*)new_data, mask,
                                 poptions,
                                 sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());  
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
    if (execute_no_commit(thd, m_thd_ndb, trans,
                          m_ignore_no_key || m_read_before_write_removal_used,
                          &ignore_count) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else if (blob_count > 0)
    m_blobs_pending= TRUE;

  m_rows_changed++;
  m_rows_updated++;

  if (!applying_binlog(thd))
  {
    assert(m_rows_changed >= ignore_count);
    assert(m_rows_updated >= ignore_count);
    m_rows_changed-= ignore_count;
    m_rows_updated-= ignore_count;
  }

  DBUG_RETURN(0);
}


/*
  handler delete interface
*/

int ha_ndbcluster::delete_row(const uchar *record)
{
  return ndb_delete_row(record, FALSE);
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
    if (execute_commit(table->in_use, m_thd_ndb, trans,
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
  THD *thd= table->in_use;
  if (execute_no_commit(thd, m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0)
  {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }

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
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= m_thd_ndb->trans;
  NdbScanOperation* cursor= m_active_cursor;
  const NdbOperation *op;
  uint32 part_id= ~uint32(0);
  int error;
  bool allow_batch= !m_delete_cannot_batch &&
    (m_is_bulk_delete || thd_allow_batch(thd));

  DBUG_ENTER("ndb_delete_row");
  DBUG_ASSERT(trans);

  error = check_slave_state(thd);
  if (unlikely(error))
    DBUG_RETURN(error);

  ha_statistic_increment(&SSV::ha_delete_count);
  m_rows_changed++;

  bool skip_partition_for_unique_index= FALSE;
  if (m_use_partition_pruning)
  {
    if (!cursor && m_read_before_write_removal_used)
    {
      ndb_index_type type= get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for deleting
        without finding the partitions
      */
      if (type == UNIQUE_INDEX ||
          type == UNIQUE_ORDERED_INDEX)
      {
        skip_partition_for_unique_index= TRUE;
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
  bool need_flush= add_row_check_if_batch_full_size(thd_ndb, delete_size);

  if (thd->slave_thread || THDVAR(thd, deferred_constraints))
  {
    options.optionsPresent |=
      NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
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
    m_lock_tuple= FALSE;
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

#ifdef HAVE_NDB_BINLOG
    Uint32 buffer[ MAX_CONFLICT_INTERPRETED_PROG_SIZE ];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer)/sizeof(buffer[0]));
    if (thd->slave_thread)
    {
       bool conflict_handled = false;

      /* Conflict resolution in slave thread. */
      if (unlikely((error = prepare_conflict_detection(DELETE_ROW,
                                                       key_rec,
                                                       key_row, /* old_data */
                                                       NULL,    /* new_data */
                                                       trans,
                                                       &code,
                                                       &options,
                                                       conflict_handled))))
        DBUG_RETURN(error);

      if (unlikely(conflict_handled))
      {
        /* No need to continue with operation definition */
        /* TODO : Ensure batch execution */
        DBUG_RETURN(0);
      }
    }
#endif /* HAVE_NDB_BINLOG */
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
  if (execute_no_commit(thd, m_thd_ndb, trans,
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
  DBUG_ASSERT(src_row != NULL);

  my_ptrdiff_t dst_offset= dst_row - table->record[0];
  my_ptrdiff_t src_offset= src_row - table->record[0];

  /* Initialize the NULL bitmap. */
  memset(dst_row, 0xff, table->s->null_bytes);

  uchar *blob_ptr= m_blobs_buffer;

  for (uint i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (bitmap_is_set(table->read_set, i))
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
#ifdef HAVE_purify
          /*
            We get Valgrind warnings on uninitialised padding bytes in
            varstrings, for example when writing rows to temporary tables.
            So for valgrind builds we pad with zeros, not needed for
            production code.
          */
          if (actual_length < field->pack_length())
            memset(field->ptr + actual_length, 0,
                  field->pack_length() - actual_length);
#endif
          field->move_field_offset(-dst_offset);
        }
        else
          field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }
  }
}


/**
  Get the default value of the field from default_values of the table.
*/
static void get_default_value(void *def_val, Field *field)
{
  DBUG_ASSERT(field != NULL);

  my_ptrdiff_t src_offset= field->table->s->default_values - field->table->record[0];

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
#ifdef HAVE_purify
          if (actual_length < field->pack_length())
            memset(((char*)def_val) + actual_length, 0,
                  field->pack_length() - actual_length);
#endif
        }
        field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }
  }
}

/*
    DBUG_EXECUTE("value", print_results(););
*/

void ha_ndbcluster::print_results()
{
  DBUG_ENTER("print_results");

#ifndef DBUG_OFF

  char buf_type[MAX_FIELD_WIDTH], buf_val[MAX_FIELD_WIDTH];
  String type(buf_type, sizeof(buf_type), &my_charset_bin);
  String val(buf_val, sizeof(buf_val), &my_charset_bin);
  for (uint f= 0; f < table_share->fields; f++)
  {
    /* Use DBUG_PRINT since DBUG_FILE cannot be filtered out */
    char buf[2000];
    Field *field;
    void* ptr;
    NdbValue value;

    buf[0]= 0;
    field= table->field[f];
    if (!(value= m_value[f]).ptr)
    {
      strmov(buf, "not read");
      goto print_value;
    }

    ptr= field->ptr;

    if (! (field->flags & BLOB_FLAG))
    {
      if (value.rec->isNULL())
      {
        strmov(buf, "NULL");
        goto print_value;
      }
      type.length(0);
      val.length(0);
      field->sql_type(type);
      field->val_str(&val);
      my_snprintf(buf, sizeof(buf), "%s %s", type.c_ptr(), val.c_ptr());
    }
    else
    {
      NdbBlob *ndb_blob= value.blob;
      bool isNull= TRUE;
      assert(ndb_blob->getState() == NdbBlob::Active);
      ndb_blob->getNull(isNull);
      if (isNull)
        strmov(buf, "NULL");
    }

print_value:
    DBUG_PRINT("value", ("%u,%s: %s", f, field->field_name, buf));
  }
#endif
  DBUG_VOID_RETURN;
}


/*
  Set fields in partition functions in read set for underlying handlers

  SYNOPSIS
    include_partition_fields_in_used_fields()

  RETURN VALUE
    NONE

  DESCRIPTION
    Some handlers only read fields as specified by the bitmap for the
    read set. For partitioned handlers we always require that the
    fields of the partition functions are read such that we can
    calculate the partition id to place updated and deleted records.
*/

static void
include_partition_fields_in_used_fields(Field **ptr, MY_BITMAP *read_set)
{
  DBUG_ENTER("include_partition_fields_in_used_fields");
  do
  {
    bitmap_set_bit(read_set, (*ptr)->field_index);
  } while (*(++ptr));
  DBUG_VOID_RETURN;
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
  m_lock_tuple= FALSE;
  if (table_share->primary_key == MAX_KEY &&
      m_use_partition_pruning)
    include_partition_fields_in_used_fields(
      m_part_info->full_part_field_array,
      table->read_set);
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
  key_range start_key;
  bool descending= FALSE;
  DBUG_ENTER("ha_ndbcluster::index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  start_key.key= key;
  start_key.length= key_len;
  start_key.flag= find_flag;
  descending= FALSE;
  switch (find_flag) {
  case HA_READ_KEY_OR_PREV:
  case HA_READ_BEFORE_KEY:
  case HA_READ_PREFIX_LAST:
  case HA_READ_PREFIX_LAST_OR_PREV:
    descending= TRUE;
    break;
  default:
    break;
  }
  const int error= read_range_first_to_buf(&start_key, 0, descending,
                                           m_sorted, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_next(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_next");
  ha_statistic_increment(&SSV::ha_read_next_count);
  const int error= next_result(buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_prev(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_prev");
  ha_statistic_increment(&SSV::ha_read_prev_count);
  const int error= next_result(buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_first(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_first");
  ha_statistic_increment(&SSV::ha_read_first_count);
  // Start the ordered index scan and fetch the first row

  // Only HA_READ_ORDER indexes get called by index_first
  const int error= ordered_index_scan(0, 0, m_sorted, FALSE, buf, NULL);
  table->status=error ? STATUS_NOT_FOUND: 0;
  DBUG_RETURN(error);
}


int ha_ndbcluster::index_last(uchar *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_last");
  ha_statistic_increment(&SSV::ha_read_last_count);
  const int error= ordered_index_scan(0, 0, m_sorted, TRUE, buf, NULL);
  table->status=error ? STATUS_NOT_FOUND: 0;
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
  ndb_index_type type= get_index_type(active_index);
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
      sorted= FALSE;
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
      error= pk_read(start_key->key, start_key->length, buf,
		  (m_use_partition_pruning)? &(part_spec.start_part) : NULL);
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
      error= unique_index_read(start_key->key, start_key->length, buf);
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
                                    bool eq_r, bool sorted)
{
  uchar* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  DBUG_RETURN(read_range_first_to_buf(start_key, end_key, FALSE,
                                      sorted, buf));
}

int ha_ndbcluster::read_range_next()
{
  DBUG_ENTER("ha_ndbcluster::read_range_next");
  DBUG_RETURN(next_result(table->record[0]));
}


int ha_ndbcluster::rnd_init(bool scan)
{
  int error;
  DBUG_ENTER("rnd_init");
  DBUG_PRINT("enter", ("scan: %d", scan));

  if ((error= close_scan()))
    DBUG_RETURN(error);
  index_init(table_share->primary_key, 0);
  DBUG_RETURN(0);
}

int ha_ndbcluster::close_scan()
{
  /*
    workaround for bug #39872 - explain causes segv
    - rnd_end/close_scan is called on unlocked table
    - should be fixed in server code, but this will
    not be done until 6.0 as it is too intrusive
  */
  if (m_thd_ndb == NULL)
    return 0;
  NdbTransaction *trans= m_thd_ndb->trans;
  int error;
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
    if (execute_no_commit(table->in_use, m_thd_ndb, trans, m_ignore_no_key) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  
  cursor->close(m_thd_ndb->m_force_send, TRUE);
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
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  int error;
  if (m_active_cursor)
    error= next_result(buf);
  else if (m_active_query)
    error= next_result(buf);
  else
    error= full_table_scan(NULL, NULL, NULL, buf);
  
  table->status= error ? STATUS_NOT_FOUND: 0;
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
  ha_statistic_increment(&SSV::ha_read_rnd_count);
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
    int res= pk_read(pos, key_length, buf, 
                     (m_user_defined_partitioning) ? 
                     &(part_spec.start_part) 
                     : NULL);
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
    table->status= res ? STATUS_NOT_FOUND: 0;
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
        if (((Field_varstring*)field)->length_bytes == 1)
        {
          /**
           * Keys always use 2 bytes length
           */
          buff[0] = ptr[0];
          buff[1] = 0;
          memcpy(buff+2, ptr + 1, len);
        }
        else
        {
          memcpy(buff, ptr, len + 2);
        }
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
    int hidden_no= table->s->fields;
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
ha_ndbcluster::cmp_ref(const uchar * ref1, const uchar * ref2)
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
  while (flag & HA_STATUS_VARIABLE)
  {
    if (!thd)
      thd= current_thd;
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));

    if (!m_table_info)
    {
      if ((my_errno= check_ndb_connection(thd)))
        DBUG_RETURN(my_errno);
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
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY"));
    errkey= m_dupkey;
  }
  if (flag & HA_STATUS_AUTO)
  {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (m_table && table->found_next_number_field)
    {
      if (!thd)
        thd= current_thd;
      if ((my_errno= check_ndb_connection(thd)))
        DBUG_RETURN(my_errno);
      Ndb *ndb= get_ndb(thd);
      Ndb_tuple_id_range_guard g(m_share);
      
      Uint64 auto_increment_value64;
      if (ndb->readAutoIncrementValue(m_table, g.range,
                                      auto_increment_value64) == -1)
      {
        const NdbError err= ndb->getNdbError();
        sql_print_error("Error %lu in readAutoIncrementValue(): %s",
                        (ulong) err.code, err.message);
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


void ha_ndbcluster::get_dynamic_partition_info(PARTITION_STATS *stat_info,
                                               uint part_id)
{
  DBUG_PRINT("info", ("ha_ndbcluster::get_dynamic_partition_info"));

  memset(stat_info, 0, sizeof(PARTITION_STATS));
  int error = 0;
  THD *thd = table->in_use;

  if (!thd)
    thd = current_thd;
  if (!m_table_info)
  {
    if ((error = check_ndb_connection(thd)))
      goto err;
  }
  error = update_stats(thd, 1, false, part_id);

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
    m_ignore_dup_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    m_ignore_dup_key= FALSE;
    break;
  case HA_EXTRA_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= FALSE;
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
      m_use_write= TRUE;
    }
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CANNOT_REPLACE"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= FALSE;
    break;
  case HA_EXTRA_DELETE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_DELETE_CANNOT_BATCH"));
    m_delete_cannot_batch= TRUE;
    break;
  case HA_EXTRA_UPDATE_CANNOT_BATCH:
    DBUG_PRINT("info", ("HA_EXTRA_UPDATE_CANNOT_BATCH"));
    m_update_cannot_batch= TRUE;
    break;
  // We don't implement 'KEYREAD'. However, KEYREAD also implies DISABLE_JOINPUSH.
  case HA_EXTRA_KEYREAD:
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD"));
    m_disable_pushed_join= TRUE;
    break;
  case HA_EXTRA_NO_KEYREAD:
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYREAD"));
    m_disable_pushed_join= FALSE;
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
  m_read_before_write_removal_possible= TRUE;
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
  m_disable_pushed_join= FALSE;

#if 0
  // Magnus, disble this "hack" until it's possible to test if
  // it's still needed
  /*
    Regular partition pruning will set the bitmap appropriately.
    Some queries like ALTER TABLE doesn't use partition pruning and
    thus the 'used_partitions' bitmap needs to be initialized
  */
  if (m_part_info)
    bitmap_set_all(&m_part_info->used_partitions);
#endif

  /* reset flags set by extra calls */
  m_read_before_write_removal_possible= FALSE;
  m_read_before_write_removal_used= FALSE;
  m_rows_updated= m_rows_deleted= 0;
  m_ignore_dup_key= FALSE;
  m_use_write= FALSE;
  m_ignore_no_key= FALSE;
  m_rows_inserted= (ha_rows) 0;
  m_rows_to_insert= (ha_rows) 1;
  m_delete_cannot_batch= FALSE;
  m_update_cannot_batch= FALSE;

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

  
  if (! (m_thd_ndb->trans_options & TNTO_TRANSACTIONS_OFF))
  {
    if (!allow_batch &&
        execute_no_commit(table->in_use, m_thd_ndb, trans, m_ignore_no_key) != 0)
    {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
  }
  else
  {
    /*
      signal that transaction has been broken up and hence cannot
      be rolled back
    */
    THD *thd= table->in_use;
    thd->transaction.all.mark_modified_non_trans_table();
    thd->transaction.stmt.mark_modified_non_trans_table();
    if (execute_commit(thd, m_thd_ndb, trans, m_thd_ndb->m_force_send,
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
    bool allow_batch= (thd_ndb->m_handler != 0);
    error= flush_bulk_insert(allow_batch);
    if (error != 0)
      my_errno= error;
  }

  m_rows_inserted= (ha_rows) 0;
  m_rows_to_insert= (ha_rows) 1;
  DBUG_RETURN(error);
}


int ha_ndbcluster::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  DBUG_ENTER("extra_opt");
  DBUG_PRINT("enter", ("cache_size: %lu", cache_size));
  DBUG_RETURN(extra(operation));
}

static const char *ha_ndbcluster_exts[] = {
 ha_ndb_ext,
 NullS
};

const char** ha_ndbcluster::bas_ext() const
{
  return ha_ndbcluster_exts;
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

#ifdef HAVE_NDB_BINLOG
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
  ndb_pack_varchar(ndbtab->getColumn(2u), tmp_buf,
                   group_master_log_name, (int)strlen(group_master_log_name));
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
#endif /* HAVE_NDB_BINLOG */

static void transaction_checks(THD *thd, Thd_ndb *thd_ndb)
{
  if (thd->lex->sql_command == SQLCOM_LOAD)
    thd_ndb->trans_options|= TNTO_TRANSACTIONS_OFF;
  else if (!thd->transaction.flags.enabled)
    thd_ndb->trans_options|= TNTO_TRANSACTIONS_OFF;
  else if (!THDVAR(thd, use_transactions))
    thd_ndb->trans_options|= TNTO_TRANSACTIONS_OFF;
  thd_ndb->m_force_send= THDVAR(thd, force_send);
  if (!thd->slave_thread)
    thd_ndb->m_batch_size= THDVAR(thd, batch_size);
  else
  {
    thd_ndb->m_batch_size= THDVAR(NULL, batch_size); /* using global value */
    /* Do not use hinted TC selection in slave thread */
    THDVAR(thd, optimized_node_selection)=
      THDVAR(NULL, optimized_node_selection) & 1; /* using global value */
  }
#ifndef EMBEDDED_LIBRARY
  bool applying_binlog=
    thd->rli_fake? 
    ndb_mi_get_in_relay_log_statement(thd->rli_fake) : false;
  if (applying_binlog)
    thd_ndb->trans_options|= TNTO_APPLYING_BINLOG;
#endif
}

int ha_ndbcluster::start_statement(THD *thd,
                                   Thd_ndb *thd_ndb,
                                   uint table_count)
{
  NdbTransaction *trans= thd_ndb->trans;
  int error;
  DBUG_ENTER("ha_ndbcluster::start_statement");

  m_thd_ndb= thd_ndb;
  transaction_checks(thd, m_thd_ndb);

  if (table_count == 0)
  {
    trans_register_ha(thd, FALSE, ht);
    if (thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    {
      if (!trans)
        trans_register_ha(thd, TRUE, ht);
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
    DBUG_ASSERT(thd_ndb->changed_tables.is_empty() == TRUE);
    thd_ndb->trans_options= 0;

    DBUG_PRINT("trans",("Possibly starting transaction"));
    const uint opti_node_select = THDVAR(thd, optimized_node_selection);
    DBUG_PRINT("enter", ("optimized_node_selection: %u", opti_node_select));
    if (!(opti_node_select & 2) ||
        thd->lex->sql_command == SQLCOM_LOAD)
      if (unlikely(!start_transaction(error)))
        DBUG_RETURN(error);

    thd_ndb->init_open_tables();
    thd_ndb->m_slow_path= FALSE;
    if (!(thd_options(thd) & OPTION_BIN_LOG) ||
        thd->variables.binlog_format == BINLOG_FORMAT_STMT)
    {
      thd_ndb->trans_options|= TNTO_NO_LOGGING;
      thd_ndb->m_slow_path= TRUE;
    }
    else if (thd->slave_thread)
      thd_ndb->m_slow_path= TRUE;
  }
  DBUG_RETURN(0);
}

int
ha_ndbcluster::add_handler_to_open_tables(THD *thd,
                                          Thd_ndb *thd_ndb,
                                          ha_ndbcluster* handler)
{
  DBUG_ENTER("ha_ndbcluster::add_handler_to_open_tables");
  DBUG_PRINT("info", ("Adding %s", handler->m_share->key));

  /**
   * thd_ndb->open_tables is only used iff thd_ndb->m_handler is not
   */
  DBUG_ASSERT(thd_ndb->m_handler == NULL);
  const void *key= handler->m_share;
  HASH_SEARCH_STATE state;
  THD_NDB_SHARE *thd_ndb_share=
    (THD_NDB_SHARE*)my_hash_first(&thd_ndb->open_tables,
                                  (const uchar *)&key, sizeof(key),
                                  &state);
  while (thd_ndb_share && thd_ndb_share->key != key)
  {
    thd_ndb_share=
      (THD_NDB_SHARE*)my_hash_next(&thd_ndb->open_tables,
                                   (const uchar *)&key, sizeof(key),
                                   &state);
  }
  if (thd_ndb_share == 0)
  {
    thd_ndb_share= (THD_NDB_SHARE *) alloc_root(&thd->transaction.mem_root,
                                                sizeof(THD_NDB_SHARE));
    if (!thd_ndb_share)
    {
      mem_alloc_error(sizeof(THD_NDB_SHARE));
      DBUG_RETURN(1);
    }
    thd_ndb_share->key= key;
    thd_ndb_share->stat.last_count= thd_ndb->count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
    my_hash_insert(&thd_ndb->open_tables, (uchar *)thd_ndb_share);
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
  // Start of transaction
  m_rows_changed= 0;
  m_blobs_pending= FALSE;
  release_blobs_buffer();
  m_slow_path= m_thd_ndb->m_slow_path;
#ifdef HAVE_NDB_BINLOG
  if (unlikely(m_slow_path))
  {
    if (m_share == ndb_apply_status_share && thd->slave_thread)
        m_thd_ndb->trans_options|= TNTO_INJECTED_APPLY_STATUS;
  }
#endif

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

    if (m_rows_changed && global_system_variables.query_cache_type)
    {
      DBUG_PRINT("info", ("Rows has changed"));

      if (thd_ndb->trans &&
          thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
      {
        DBUG_PRINT("info", ("Add share to list of changed tables, %p",
                            m_share));
        /* NOTE push_back allocates memory using transactions mem_root! */
        thd_ndb->changed_tables.push_back(get_share(m_share),
                                          &thd->transaction.mem_root);
      }

      if (opt_ndb_cache_check_time)
      {
        pthread_mutex_lock(&m_share->mutex);
        DBUG_PRINT("info", ("Invalidating commit_count"));
        m_share->commit_count= 0;
        m_share->commit_count_lock++;
        pthread_mutex_unlock(&m_share->mutex);
      }
    }

    if (!--thd_ndb->lock_count)
    {
      DBUG_PRINT("trans", ("Last external_lock"));

      if ((!(thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) &&
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
  m_lock_tuple= FALSE;
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
    actually doesn't refer to a commit but only to and end of statement.

    In the case of stored functions, one stored function is treated as one
    statement and the call to commit comes at the end of the stored function.
*/

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type lock_type)
{
  int error=0;
  Thd_ndb *thd_ndb;
  DBUG_ENTER("start_stmt");
  DBUG_ASSERT(thd == table->in_use);

  thd_ndb= get_thd_ndb(thd);
  if ((error= start_statement(thd, thd_ndb, thd_ndb->start_stmt_count++)))
    goto error;
  if ((error= init_handler_for_statement(thd)))
    goto error;
  DBUG_RETURN(0);
error:
  thd_ndb->start_stmt_count--;
  DBUG_RETURN(error);
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

  transaction_checks(table->in_use, m_thd_ndb);

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

  transaction_checks(table->in_use, m_thd_ndb);

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

  transaction_checks(table->in_use, m_thd_ndb);
  const uint opti_node_select= THDVAR(table->in_use, optimized_node_selection);
  m_thd_ndb->connection->set_optimized_node_selection(opti_node_select & 1);
  if ((trans= m_thd_ndb->ndb->startTransaction()))
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

  transaction_checks(table->in_use, m_thd_ndb);
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

int ndbcluster_commit(handlerton *hton, THD *thd, bool all)
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
  if (!all && (thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
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

#ifdef HAVE_NDB_BINLOG
  if (unlikely(thd_ndb->m_slow_path))
  {
    if (thd->slave_thread)
      ndbcluster_update_apply_status
        (thd, thd_ndb->trans_options & TNTO_INJECTED_APPLY_STATUS);
  }
#endif /* HAVE_NDB_BINLOG */

  if (thd->slave_thread)
  {
#ifdef HAVE_NDB_BINLOG
    /* If this slave transaction has included conflict detecting ops
     * and some defined operations are not yet sent, then perform
     * an execute(NoCommit) before committing, as conflict op handling
     * is done by execute(NoCommit)
     */
    /* TODO : Add as function */
    if (g_ndb_slave_state.conflict_flags & SCS_OPS_DEFINED)
    {
      if (thd_ndb->m_unsent_bytes)
        res = execute_no_commit(thd, thd_ndb, trans, TRUE);
    }

    if (likely(res == 0))
      res = g_ndb_slave_state.atConflictPreCommit(retry_slave_trans);
#endif /* HAVE_NDB_BINLOG */

    if (likely(res == 0))
      res= execute_commit(thd, thd_ndb, trans, 1, TRUE);

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
      DBUG_PRINT("info", ("autocommit+rbwr, transaction already comitted"));
      if (trans->commitStatus() != NdbTransaction::Committed)
      {
        sql_print_error("found uncomitted autocommit+rbwr transaction, "
                        "commit status: %d", trans->commitStatus());
        abort();
      }
    }
    else
    {
      bool applying_binlog= (thd_ndb->trans_options & TNTO_APPLYING_BINLOG);
      res= execute_commit(thd, thd_ndb, trans, THDVAR(thd, force_send), applying_binlog);
    }
  }

  if (res != 0)
  {
#ifdef HAVE_NDB_BINLOG
    if (retry_slave_trans)
    {
      if (st_ndb_slave_state::MAX_RETRY_TRANS_COUNT >
          g_ndb_slave_state.retry_trans_count++)
      {
        /*
           Warning is necessary to cause retry from slave.cc
           exec_relay_log_event()
        */
        push_warning(thd, Sql_condition::WARN_LEVEL_WARN,
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
        sql_print_error("Ndb slave retried transaction %u time(s) in vain.  Giving up.",
                        st_ndb_slave_state::MAX_RETRY_TRANS_COUNT);
      }
      res= ER_GET_TEMPORARY_ERRMSG;
    }
    else
#endif
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
    for (uint i= 0; i<thd_ndb->open_tables.records; i++)
    {
      THD_NDB_SHARE *thd_share=
        (THD_NDB_SHARE*)my_hash_element(&thd_ndb->open_tables, i);
      modify_shared_stats((NDB_SHARE*)thd_share->key, &thd_share->stat);
    }
  }

  ndb->closeTransaction(trans);
  thd_ndb->trans= NULL;
  thd_ndb->m_handler= NULL;

  /* Clear commit_count for tables changed by transaction */
  NDB_SHARE* share;
  List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
  while ((share= it++))
  {
    DBUG_PRINT("info", ("Remove share to list of changed tables, %p",
                        share));
    pthread_mutex_lock(&share->mutex);
    DBUG_PRINT("info", ("Invalidate commit_count for %s, share->commit_count: %lu",
                        share->table_name, (ulong) share->commit_count));
    share->commit_count= 0;
    share->commit_count_lock++;
    pthread_mutex_unlock(&share->mutex);
    free_share(&share);
  }
  thd_ndb->changed_tables.empty();

  DBUG_RETURN(res);
}


/**
  Rollback a transaction started in NDB.
*/

static int ndbcluster_rollback(handlerton *hton, THD *thd, bool all)
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
  if (!all && (thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
      (thd_ndb->save_point_count > 0))
  {
    /*
      Ignore end-of-statement until real rollback or commit is called
      as ndb does not support rollback statement
      - mark that rollback was unsuccessful, this will cause full rollback
      of the transaction
    */
    DBUG_PRINT("info", ("Rollback before start or end-of-statement only"));
    mark_transaction_to_rollback(thd, 1);
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

  /* Clear list of tables changed by transaction */
  NDB_SHARE* share;
  List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
  while ((share= it++))
  {
    DBUG_PRINT("info", ("Remove share to list of changed tables, %p",
                        share));
    free_share(&share);
  }
  thd_ndb->changed_tables.empty();

  if (thd->slave_thread)
    update_slave_api_stats(thd_ndb->ndb);

  DBUG_RETURN(res);
}

/**
 * Support for create table/column modifiers
 *   by exploiting the comment field
 */
struct NDB_Modifier
{
  enum { M_BOOL } m_type;
  const char * m_name;
  size_t m_name_len;
  bool m_found;
  union {
    bool m_val_bool;
#ifdef TODO__
    int m_val_int;
    struct {
      const char * str;
      size_t len;
    } m_val_str;
#endif
  };
};

static const
struct NDB_Modifier ndb_table_modifiers[] =
{
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("NOLOGGING"), 0, {0} },
  { NDB_Modifier::M_BOOL, 0, 0, 0, {0} }
};

static const
struct NDB_Modifier ndb_column_modifiers[] =
{
  { NDB_Modifier::M_BOOL, STRING_WITH_LEN("MAX_BLOB_PART_SIZE"), 0, {0} },
  { NDB_Modifier::M_BOOL, 0, 0, 0, {0} }
};

/**
 * NDB_Modifiers
 *
 * This class implements a simple parser for getting modifiers out
 *   of a string (e.g a comment field)
 */
class NDB_Modifiers
{
public:
  NDB_Modifiers(const NDB_Modifier modifiers[]);
  ~NDB_Modifiers();

  /**
   * parse string-with length (not necessarily NULL terminated)
   */
  int parse(THD* thd, const char * prefix, const char * str, size_t strlen);

  /**
   * Get modifier...returns NULL if unknown
   */
  const NDB_Modifier * get(const char * name) const;
private:
  uint m_len;
  struct NDB_Modifier * m_modifiers;

  int parse_modifier(THD *thd, const char * prefix,
                     struct NDB_Modifier* m, const char * str);
};

static
bool
end_of_token(const char * str)
{
  return str[0] == 0 || str[0] == ' ' || str[0] == ',';
}

NDB_Modifiers::NDB_Modifiers(const NDB_Modifier modifiers[])
{
  for (m_len = 0; modifiers[m_len].m_name != 0; m_len++)
  {}
  m_modifiers = new NDB_Modifier[m_len];
  memcpy(m_modifiers, modifiers, m_len * sizeof(NDB_Modifier));
}

NDB_Modifiers::~NDB_Modifiers()
{
  delete [] m_modifiers;
}

int
NDB_Modifiers::parse_modifier(THD *thd,
                              const char * prefix,
                              struct NDB_Modifier* m,
                              const char * str)
{
  if (m->m_found)
  {
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "%s : modifier %s specified twice",
                        prefix, m->m_name);
  }

  switch(m->m_type){
  case NDB_Modifier::M_BOOL:
    if (end_of_token(str))
    {
      m->m_val_bool = true;
      goto found;
    }
    if (str[0] != '=')
      break;

    str++;
    if (str[0] == '1' && end_of_token(str+1))
    {
      m->m_val_bool = true;
      goto found;
    }

    if (str[0] == '0' && end_of_token(str+1))
    {
      m->m_val_bool = false;
      goto found;
    }
  }

  {
    const char * end = strpbrk(str, " ,");
    if (end)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "%s : invalid value '%.*s' for %s",
                          prefix, (int)(end - str), str, m->m_name);
    }
    else
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "%s : invalid value '%s' for %s",
                          prefix, str, m->m_name);
    }
  }
  return -1;
found:
  m->m_found = true;
  return 0;
}

int
NDB_Modifiers::parse(THD *thd,
                     const char * prefix,
                     const char * _source,
                     size_t _source_len)
{
  if (_source == 0 || _source_len == 0)
    return 0;

  const char * source = 0;

  /**
   * Check if _source is NULL-terminated
   */
  for (size_t i = 0; i<_source_len; i++)
  {
    if (_source[i] == 0)
    {
      source = _source;
      break;
    }
  }

  if (source == 0)
  {
    /**
     * Make NULL terminated string so that strXXX-functions are safe
     */
    char * tmp = new char[_source_len+1];
    if (tmp == 0)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "%s : unable to parse due to out of memory",
                          prefix);
      return -1;
    }
    memcpy(tmp, _source, _source_len);
    tmp[_source_len] = 0;
    source = tmp;
  }

  const char * pos = source;
  if ((pos = strstr(pos, prefix)) == 0)
  {
    if (source != _source)
      delete [] source;
    return 0;
  }

  pos += strlen(prefix);

  while (pos && pos[0] != 0 && pos[0] != ' ')
  {
    const char * end = strpbrk(pos, " ,"); // end of current modifier

    for (uint i = 0; i < m_len; i++)
    {
      size_t l = m_modifiers[i].m_name_len;
      if (strncmp(pos, m_modifiers[i].m_name, l) == 0)
      {
        /**
         * Found modifier...
         */

        if (! (end_of_token(pos + l) || pos[l] == '='))
          goto unknown;

        pos += l;
        int res = parse_modifier(thd, prefix, m_modifiers+i, pos);

        if (res == -1)
        {
          /**
           * We continue parsing even if modifier had error
           */
        }

        goto next;
      }
    }

    {
  unknown:
      if (end)
      {
        push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "%s : unknown modifier: %.*s",
                            prefix, (int)(end - pos), pos);
      }
      else
      {
        push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "%s : unknown modifier: %s",
                            prefix, pos);
      }
    }

next:
    pos = end;
    if (pos && pos[0] == ',')
      pos++;
  }

  if (source != _source)
    delete [] source;

  return 0;
}

const NDB_Modifier *
NDB_Modifiers::get(const char * name) const
{
  for (uint i = 0; i < m_len; i++)
  {
    if (strcmp(name, m_modifiers[i].m_name) == 0)
    {
      return m_modifiers + i;
    }
  }
  return 0;
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

static bool
ndb_blob_striping()
{
#ifndef DBUG_OFF
  const char* p= getenv("NDB_BLOB_STRIPING");
  if (p != 0 && *p != 0 && *p != '0' && *p != 'n' && *p != 'N')
    return true;
#endif
  return false;
}

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
const Uint32 OLD_NDB_MAX_TUPLE_SIZE_IN_WORDS = 2013;
#else
const Uint32 OLD_NDB_MAX_TUPLE_SIZE_IN_WORDS = NDB_MAX_TUPLE_SIZE_IN_WORDS;
#endif

static int create_ndb_column(THD *thd,
                             NDBCOL &col,
                             Field *field,
                             HA_CREATE_INFO *create_info
#ifndef NDB_WITHOUT_COLUMN_FORMAT
                             , column_format_type
                               default_format= COLUMN_FORMAT_TYPE_DEFAULT
#endif
                            )
{
  NDBCOL::StorageType type= NDBCOL::StorageTypeMemory;
  bool dynamic= FALSE;

  char buf[MAX_ATTR_DEFAULT_VALUE_SIZE];
  DBUG_ENTER("create_ndb_column");
  // Set name
  if (col.setName(field->field_name))
  {
    DBUG_RETURN(my_errno= errno);
  }
  // Get char set
  CHARSET_INFO *cs= const_cast<CHARSET_INFO*>(field->charset());
  // Set type and sizes
  const enum enum_field_types mysql_type= field->real_type();

  NDB_Modifiers column_modifiers(ndb_column_modifiers);
  column_modifiers.parse(thd, "NDB_COLUMN=",
                         field->comment.str,
                         field->comment.length);

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
          my_ptrdiff_t src_offset= field->table->s->default_values 
            - field->table->record[0];
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
    col.setStripeSize(ndb_blob_striping() ? 0 : 0);
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
        col.setStripeSize(ndb_blob_striping() ? 16 : 0);
        if (mod_maxblob->m_found)
        {
          col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safty */ 13));
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
    col.setStripeSize(ndb_blob_striping() ? 8 : 0);
    if (mod_maxblob->m_found)
    {
      col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safty */ 13));
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
    col.setPartSize(4 * (OLD_NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safty */ 13));
    col.setStripeSize(ndb_blob_striping() ? 4 : 0);
    if (mod_maxblob->m_found)
    {
      col.setPartSize(4 * (NDB_MAX_TUPLE_SIZE_IN_WORDS - /* safty */ 13));
    }
    break;
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
    col.setPartitionKey(TRUE);
  }

  // Set autoincrement
  if (field->flags & AUTO_INCREMENT_FLAG) 
  {
#ifndef DBUG_OFF
    char buff[22];
#endif
    col.setAutoIncrement(TRUE);
    ulonglong value= create_info->auto_increment_value ?
      create_info->auto_increment_value : (ulonglong) 1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %s", llstr(value, buff)));
    col.setAutoIncrementInitialValue(value);
  }
  else
    col.setAutoIncrement(FALSE);

  DBUG_PRINT("info", ("storage: %u  format: %u  ",
                      field->field_storage_type(),
                      field->column_format()));
  switch (field->field_storage_type()) {
  case(HA_SM_DEFAULT):
  default:
    if (create_info->storage_media == HA_SM_DISK)
      type= NDBCOL::StorageTypeDisk;
    else
      type= NDBCOL::StorageTypeMemory;
    break;
  case(HA_SM_DISK):
    type= NDBCOL::StorageTypeDisk;
    break;
  case(HA_SM_MEMORY):
    type= NDBCOL::StorageTypeMemory;
    break;
  }

  switch (field->column_format()) {
  case(COLUMN_FORMAT_TYPE_FIXED):
    dynamic= FALSE;
    break;
  case(COLUMN_FORMAT_TYPE_DYNAMIC):
    dynamic= TRUE;
    break;
  case(COLUMN_FORMAT_TYPE_DEFAULT):
  default:
    if (create_info->row_type == ROW_TYPE_DEFAULT)
      dynamic= default_format;
    else
      dynamic= (create_info->row_type == ROW_TYPE_DYNAMIC);
    break;
  }
  DBUG_PRINT("info", ("Column %s is declared %s", field->field_name,
                      (dynamic) ? "dynamic" : "static"));
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
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "Row format FIXED incompatible with "
                          "dynamic attribute %s",
                          field->field_name);
    }
    break;
  case ROW_TYPE_DYNAMIC:
    /*
      Future: make columns dynamic in this case
    */
    break;
  default:
    break;
  }

  DBUG_PRINT("info", ("Format %s, Storage %s", (dynamic)?"dynamic":"fixed",(type == NDBCOL::StorageTypeDisk)?"disk":"memory"));
  col.setStorageType(type);
  col.setDynamic(dynamic);

  DBUG_RETURN(0);
}

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
          Ndb_tuple_id_range_guard g(m_share);
          if (ndb->readAutoIncrementValue(ndbtab, g.range, auto_value))
          {
            if (--retries && !thd->killed &&
                ndb->getNdbError().status == NdbError::TemporaryError)
            {
              do_retry_sleep(retry_sleep);
              continue;
            }
            const NdbError err= ndb->getNdbError();
            sql_print_error("Error %lu in ::update_create_info(): %s",
                            (ulong) err.code, err.message);
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
    strnmov(dbname, ndb->getDatabaseName(), sizeof(dbname) - 1);
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


/**
  Create a table in NDB Cluster
*/

int ha_ndbcluster::create(const char *name, 
                          TABLE *form, 
                          HA_CREATE_INFO *create_info)
{
  THD *thd= current_thd;
  NDBTAB tab;
  NDBCOL col;
  size_t pack_length, length;
  uint i, pk_length= 0;
  uchar *data= NULL, *pack_data= NULL;
  bool create_temporary= (create_info->options & HA_LEX_CREATE_TMP_TABLE);
  bool create_from_engine= (create_info->table_options & HA_OPTION_CREATE_FROM_ENGINE);
  bool is_truncate= (thd->lex->sql_command == SQLCOM_TRUNCATE);
  bool use_disk= FALSE;
  NdbDictionary::Table::SingleUserMode single_user_mode= NdbDictionary::Table::SingleUserModeLocked;
  bool ndb_sys_table= FALSE;
  int result= 0;
  NdbDictionary::ObjectId objId;

  DBUG_ENTER("ha_ndbcluster::create");
  DBUG_PRINT("enter", ("name: %s", name));

  if (create_temporary)
  {
    /*
      Ndb does not support temporary tables
     */
    my_errno= ER_ILLEGAL_HA_CREATE_OPTION;
    DBUG_PRINT("info", ("Ndb doesn't support temporary tables"));
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        "Ndb doesn't support temporary tables");
    DBUG_RETURN(my_errno);
  }

  DBUG_ASSERT(*fn_rext((char*)name) == 0);
  set_dbname(name);
  set_tabname(name);
  
  /*
    Check that database name and table name will fit within limits
  */
  if (strlen(m_dbname) > NDB_MAX_DDL_NAME_BYTESIZE ||
      strlen(m_tabname) > NDB_MAX_DDL_NAME_BYTESIZE)
  {
    my_errno= ER_TOO_LONG_IDENT;
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        ER_TOO_LONG_IDENT,
                        "Ndb has an internal limit of %u bytes on the size of schema identifiers", NDB_MAX_DDL_NAME_BYTESIZE);
    DBUG_RETURN(my_errno);
  }

  if ((my_errno= check_ndb_connection(thd)))
    DBUG_RETURN(my_errno);
  
  Ndb *ndb= get_ndb(thd);
  NDBDICT *dict= ndb->getDictionary();

  table= form;
  if (create_from_engine)
  {
    /*
      Table already exists in NDB and frm file has been created by 
      caller.
      Do Ndb specific stuff, such as create a .ndb file
    */
    if ((my_errno= write_ndb_file(name)))
      DBUG_RETURN(my_errno);

    ndbcluster_create_binlog_setup(thd, ndb, name, (uint)strlen(name),
                                   m_dbname, m_tabname, form);
    DBUG_RETURN(my_errno);
  }

  Thd_ndb *thd_ndb= get_thd_ndb(thd);

  if (!((thd_ndb->options & TNO_NO_LOCK_SCHEMA_OP) ||
        thd_ndb->has_required_global_schema_lock("ha_ndbcluster::create")))
  
    DBUG_RETURN(HA_ERR_NO_CONNECTION);


  if (!ndb_schema_dist_is_ready())
  {
    /*
      Don't allow table creation unless schema distribution is ready
      ( unless it is a creation of the schema dist table itself )
    */
    if (!(strcmp(m_dbname, NDB_REP_DB) == 0 &&
          strcmp(m_tabname, NDB_SCHEMA_TABLE) == 0))
    {
      DBUG_PRINT("info", ("Schema distribution table not setup"));
      DBUG_RETURN(HA_ERR_NO_CONNECTION);
    }
    single_user_mode = NdbDictionary::Table::SingleUserModeReadWrite;
    ndb_sys_table= TRUE;
  }

  if (!ndb_apply_status_share)
  {
    if ((strcmp(m_dbname, NDB_REP_DB) == 0 &&
         strcmp(m_tabname, NDB_APPLY_TABLE) == 0))
    {
      ndb_sys_table= TRUE;
    }
  }

  if (is_truncate)
  {
    Ndb_table_guard ndbtab_g(dict);
    ndbtab_g.init(m_tabname);
    if (!(m_table= ndbtab_g.get_table()))
      ERR_RETURN(dict->getNdbError());
    m_table= NULL;
    DBUG_PRINT("info", ("Dropping and re-creating table for TRUNCATE"));
    if ((my_errno= delete_table(name)))
      DBUG_RETURN(my_errno);
    ndbtab_g.reinit();
  }

  NDB_Modifiers table_modifiers(ndb_table_modifiers);
  table_modifiers.parse(thd, "NDB_TABLE=", create_info->comment.str,
                        create_info->comment.length);
  const NDB_Modifier * mod_nologging = table_modifiers.get("NOLOGGING");

#ifdef HAVE_NDB_BINLOG
  /* Read ndb_replication entry for this table, if any */
  Uint32 binlog_flags;
  const st_conflict_fn_def* conflict_fn= NULL;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  Uint32 num_args = MAX_CONFLICT_ARGS;

  int rep_read_rc= ndbcluster_get_binlog_replication_info(thd,
                                                          ndb,
                                                          m_dbname,
                                                          m_tabname,
                                                          ::server_id,
                                                          &binlog_flags,
                                                          &conflict_fn,
                                                          args,
                                                          &num_args);
  if (rep_read_rc != 0)
  {
    DBUG_RETURN(rep_read_rc);
  }

  /* Reset database name */
  ndb->setDatabaseName(m_dbname);

  /* TODO : Add as per conflict function 'virtual' */
  /* Use ndb_replication information as required */
  if (conflict_fn != NULL)
  {
    switch(conflict_fn->type)
    {
    case CFT_NDB_EPOCH:
    case CFT_NDB_EPOCH_TRANS:
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
#endif

  if ((dict->beginSchemaTrans() == -1))
  {
    DBUG_PRINT("info", ("Failed to start schema transaction"));
    goto err_return;
  }
  DBUG_PRINT("info", ("Started schema transaction"));

  DBUG_PRINT("table", ("name: %s", m_tabname));  
  if (tab.setName(m_tabname))
  {
    my_errno= errno;
    goto abort;
  }
  if (!ndb_sys_table)
  {
    if (THDVAR(thd, table_temporary))
    {
#ifdef DOES_NOT_WORK_CURRENTLY
      tab.setTemporary(TRUE);
#endif
      tab.setLogging(FALSE);
    }
    else if (THDVAR(thd, table_no_logging))
    {
      tab.setLogging(FALSE);
    }

    if (mod_nologging->m_found)
    {
      tab.setLogging(!mod_nologging->m_val_bool);
    }
  }
  tab.setSingleUserMode(single_user_mode);

  // Save frm data for this table
  if (readfrm(name, &data, &length))
  {
    result= 1;
    goto abort_return;
  }
  if (packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(0));
    result= 2;
    goto abort_return;
  }
  DBUG_PRINT("info",
             ("setFrm data: 0x%lx  len: %lu", (long) pack_data,
              (ulong) pack_length));
  tab.setFrm(pack_data, Uint32(pack_length));      
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));
  
  /*
    Handle table row type

    Default is to let table rows have var part reference so that online 
    add column can be performed in the future.  Explicitly setting row 
    type to fixed will omit var part reference, which will save data 
    memory in ndb, but at the cost of not being able to online add 
    column to this table
  */
  switch (create_info->row_type) {
  case ROW_TYPE_FIXED:
    tab.setForceVarPart(FALSE);
    break;
  case ROW_TYPE_DYNAMIC:
    /* fall through, treat as default */
  default:
    /* fall through, treat as default */
  case ROW_TYPE_DEFAULT:
    tab.setForceVarPart(TRUE);
    break;
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
    DBUG_PRINT("info", ("name: %s, type: %u, pack_length: %d",
                        field->field_name, field->real_type(),
                        field->pack_length()));
    if ((my_errno= create_ndb_column(thd, col, field, create_info)))
      goto abort;

    if (!use_disk &&
        col.getStorageType() == NDBCOL::StorageTypeDisk)
      use_disk= TRUE;

    if (tab.addColumn(col))
    {
      my_errno= errno;
      goto abort;
    }
    if (col.getPrimaryKey())
      pk_length += (field->pack_length() + 3) / 4;
  }

  tmp_restore_column_map(form->read_set, old_map);
  if (use_disk)
  { 
    tab.setLogging(TRUE);
    tab.setTemporary(FALSE);
    if (create_info->tablespace)
      tab.setTablespaceName(create_info->tablespace);
    else
      tab.setTablespaceName("DEFAULT-TS");
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
        push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            ER(ER_ILLEGAL_HA_CREATE_OPTION),
                            ndbcluster_hton_name,
                            "Index on field "
                            "declared with "
                            "STORAGE DISK is not supported");
        result= HA_ERR_UNSUPPORTED;
        goto abort_return;
      }
      tab.getColumn(key_part->fieldnr-1)->setStorageType(
                             NdbDictionary::Column::StorageTypeMemory);
    }
  }

  // No primary key, create shadow key as 64 bit, auto increment  
  if (form->s->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Generating shadow key"));
    if (col.setName("$PK"))
    {
      my_errno= errno;
      goto abort;
    }
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(FALSE);
    col.setPrimaryKey(TRUE);
    col.setAutoIncrement(TRUE);
    col.setDefaultValue(NULL, 0);
    if (tab.addColumn(col))
    {
      my_errno= errno;
      goto abort;
    }
    pk_length += 2;
  }
 
  // Make sure that blob tables don't have too big part size
  for (i= 0; i < form->s->fields; i++) 
  {
    /**
     * The extra +7 concists
     * 2 - words from pk in blob table
     * 5 - from extra words added by tup/dict??
     */

    // To be upgrade/downgrade safe...we currently use
    // old NDB_MAX_TUPLE_SIZE_IN_WORDS, unless MAX_BLOB_PART_SIZE is set
    switch (form->field[i]->real_type()) {
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:    
    case MYSQL_TYPE_MEDIUM_BLOB:   
    case MYSQL_TYPE_LONG_BLOB: 
    {
      NdbDictionary::Column * column= tab.getColumn(i);
      unsigned size= pk_length + (column->getPartSize()+3)/4 + 7;
      unsigned ndb_max= OLD_NDB_MAX_TUPLE_SIZE_IN_WORDS;
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

  // Check partition info
  if ((my_errno= set_up_partition_info(form->part_info, tab)))
    goto abort;

  if (tab.getFragmentType() == NDBTAB::HashMapPartition && 
      tab.getDefaultNoPartitionsFlag() &&
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
                   Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                   "Ndb might have problems storing the max amount "
                   "of rows specified");
    }
    tab.setFragmentCount(reported_frags);
    tab.setDefaultNoPartitionsFlag(false);
    tab.setFragmentData(0, 0);
  }

  // Check for HashMap
  if (tab.getFragmentType() == NDBTAB::HashMapPartition && 
      tab.getDefaultNoPartitionsFlag())
  {
    tab.setFragmentCount(0);
    tab.setFragmentData(0, 0);
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
        my_errno= ndb_to_mysql_error(&err);
        goto abort;
      }

      res= dict->createHashMap(hm);
      if (res == -1)
      {
        const NdbError err= dict->getNdbError();
        my_errno= ndb_to_mysql_error(&err);
        goto abort;
      }
    }
  }

  // Create the table in NDB     
  if (dict->createTable(tab, &objId) != 0)
  {
    const NdbError err= dict->getNdbError();
    my_errno= ndb_to_mysql_error(&err);
    goto abort;
  }

  DBUG_PRINT("info", ("Table %s/%s created successfully", 
                      m_dbname, m_tabname));

  // Create secondary indexes
  tab.assignObjId(objId);
  m_table= &tab;
  my_errno= create_indexes(thd, ndb, form);
  m_table= 0;

  if (!my_errno)
  {
    /*
     * All steps have succeeded, try and commit schema transaction
     */
    if (dict->endSchemaTrans() == -1)
      goto err_return;
    my_errno= write_ndb_file(name);
  }
  else
  {
abort:
/*
 *  Some step during table creation failed, abort schema transaction
 */
    DBUG_PRINT("info", ("Aborting schema transaction due to error %i",
                        my_errno));
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
      DBUG_PRINT("info", ("Failed to abort schema transaction, %i",
                          dict->getNdbError().code));
    m_table= 0;
    DBUG_RETURN(my_errno);
abort_return:
    DBUG_PRINT("info", ("Aborting schema transaction"));
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
      DBUG_PRINT("info", ("Failed to abort schema transaction, %i",
                          dict->getNdbError().code));
    DBUG_RETURN(result);
err_return:
    m_table= 0;
    ERR_RETURN(dict->getNdbError());
  }

  /**
   * createTable/index schema transaction OK
   */
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  m_table= ndbtab_g.get_table();

  if (my_errno)
  {
    /*
      Failed to create an index,
      drop the table (and all it's indexes)
    */
    while (!thd->killed)
    {
      if (dict->beginSchemaTrans() == -1)
        goto cleanup_failed;
      if (dict->dropTableGlobal(*m_table))
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
    DBUG_RETURN(my_errno);
  }
  else // if (!my_errno)
  {
    NDB_SHARE *share= 0;
    pthread_mutex_lock(&ndbcluster_mutex);
    /*
      First make sure we get a "fresh" share here, not an old trailing one...
    */
    {
      uint length= (uint) strlen(name);
      if ((share= (NDB_SHARE*) my_hash_search(&ndbcluster_open_tables,
                                              (const uchar*) name, length)))
        handle_trailing_share(thd, share);
    }
    /*
      get a new share
    */

    /* ndb_share reference create */
    if (!(share= get_share(name, form, TRUE, TRUE)))
    {
      sql_print_error("NDB: allocating table share for %s failed", name);
      /* my_errno is set */
    }
    else
    {
      DBUG_PRINT("NDB_SHARE", ("%s binlog create  use_count: %u",
                               share->key, share->use_count));
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    while (!IS_TMP_PREFIX(m_tabname))
    {
#ifdef HAVE_NDB_BINLOG
      if (share)
      {
        /* Set the Binlogging information we retrieved above */
        ndbcluster_apply_binlog_replication_info(thd,
                                                 share,
                                                 m_table,
                                                 conflict_fn,
                                                 args,
                                                 num_args,
                                                 TRUE, /* Do set binlog flags */
                                                 binlog_flags);
      }
#endif
      String event_name(INJECTOR_EVENT_LEN);
      ndb_rep_event_name(&event_name, m_dbname, m_tabname,
                         get_binlog_full(share));
      int do_event_op= ndb_binlog_running;

      if (!ndb_schema_dist_is_ready() &&
          strcmp(share->db, NDB_REP_DB) == 0 &&
          strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
        do_event_op= 1;

      /*
        Always create an event for the table, as other mysql servers
        expect it to be there.
      */
      if (!Ndb_dist_priv_util::is_distributed_priv_table(m_dbname, m_tabname) &&
          !ndbcluster_create_event(thd, ndb, m_table, event_name.c_ptr(), share,
                                   share && do_event_op ? 2 : 1/* push warning */))
      {
        if (opt_ndb_extra_logging)
          sql_print_information("NDB Binlog: CREATE TABLE Event: %s",
                                event_name.c_ptr());
        if (share && 
            ndbcluster_create_event_ops(thd, share,
                                        m_table, event_name.c_ptr()))
        {
          sql_print_error("NDB Binlog: FAILED CREATE TABLE event operations."
                          " Event: %s", name);
          /* a warning has been issued to the client */
        }
      }
      /*
        warning has been issued if ndbcluster_create_event failed
        and (share && do_event_op)
      */
      if (share && !do_event_op)
        set_binlog_nologging(share);
      ndbcluster_log_schema_op(thd,
                               thd->query(), thd->query_length(),
                               share->db, share->table_name,
                               m_table->getObjectId(),
                               m_table->getObjectVersion(),
                               (is_truncate) ?
			       SOT_TRUNCATE_TABLE : SOT_CREATE_TABLE, 
			       NULL, NULL);
      break;
    }
  }

  m_table= 0;
  DBUG_RETURN(my_errno);
}


int ha_ndbcluster::create_index(THD *thd, const char *name, KEY *key_info, 
                                NDB_INDEX_TYPE idx_type, uint idx_no) const
{
  int error= 0;
  char unique_name[FN_LEN + 1];
  static const char* unique_suffix= "$unique";
  DBUG_ENTER("ha_ndbcluster::create_index");
  DBUG_PRINT("info", ("Creating index %u: %s", idx_no, name));  

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    strxnmov(unique_name, FN_LEN, name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Created unique index name \'%s\' for index %d",
                        unique_name, idx_no));
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
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
			  ER_NULL_COLUMN_IN_INDEX,
			  "Ndb does not support unique index on NULL valued attributes, index access with NULL value will become full table scan");
    }
    error= create_unique_index(thd, unique_name, key_info);
    break;
  case ORDERED_INDEX:
    if (key_info->algorithm == HA_KEY_ALG_HASH)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
			  ER_ILLEGAL_HA_CREATE_OPTION,
			  ER(ER_ILLEGAL_HA_CREATE_OPTION),
			  ndbcluster_hton_name,
			  "Ndb does not support non-unique "
			  "hash based indexes");
      error= HA_ERR_UNSUPPORTED;
      break;
    }
    error= create_ordered_index(thd, name, key_info);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }
  
  DBUG_RETURN(error);
}

int ha_ndbcluster::create_ordered_index(THD *thd, const char *name, 
                                        KEY *key_info) const
{
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_RETURN(create_ndb_index(thd, name, key_info, FALSE));
}

int ha_ndbcluster::create_unique_index(THD *thd, const char *name, 
                                       KEY *key_info) const
{

  DBUG_ENTER("ha_ndbcluster::create_unique_index");
  DBUG_RETURN(create_ndb_index(thd, name, key_info, TRUE));
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
    ndb_index.setLogging(FALSE); 
  }
  if (!m_table->getLogging())
    ndb_index.setLogging(FALSE); 
  if (((NDBTAB*)m_table)->getTemporary())
    ndb_index.setTemporary(TRUE); 
  if (ndb_index.setTable(m_tabname))
  {
    DBUG_RETURN(my_errno= errno);
  }

  for (; key_part != end; key_part++) 
  {
    Field *field= key_part->field;
    if (field->field_storage_type() == HA_SM_DISK)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER(ER_ILLEGAL_HA_CREATE_OPTION),
                          ndbcluster_hton_name,
                          "Index on field "
                          "declared with "
                          "STORAGE DISK is not supported");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    if (ndb_index.addColumnName(field->field_name))
    {
      DBUG_RETURN(my_errno= errno);
    }
  }
  
  if (dict->createIndex(ndb_index, *m_table))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}

/*
 Prepare for an on-line alter table
*/ 
void ha_ndbcluster::prepare_for_alter()
{
  /* ndb_share reference schema */
  ndbcluster_get_share(m_share); // Increase ref_count
  DBUG_PRINT("NDB_SHARE", ("%s binlog schema  use_count: %u",
                           m_share->key, m_share->use_count));
  set_ndb_share_state(m_share, NSS_ALTERED);
}

/*
  Add an index on-line to a table
*/
/*
int ha_ndbcluster::add_index(TABLE *table_arg, 
                             KEY *key_info, uint num_of_keys,
                             handler_add_index **add)
{
  // TODO: As we don't yet implement ::final_add_index(),
  // we don't need a handler_add_index object either..?
  *add= NULL; // new handler_add_index(table_arg, key_info, num_of_keys);
  return add_index_impl(current_thd, table_arg, key_info, num_of_keys);
}
*/

int ha_ndbcluster::add_index_impl(THD *thd, TABLE *table_arg, 
                                  KEY *key_info, uint num_of_keys)
{
  int error= 0;
  uint idx;
  DBUG_ENTER("ha_ndbcluster::add_index");
  DBUG_PRINT("enter", ("table %s", table_arg->s->table_name.str));
  DBUG_ASSERT(m_share->state == NSS_ALTERED);

  for (idx= 0; idx < num_of_keys; idx++)
  {
    KEY *key= key_info + idx;
    KEY_PART_INFO *key_part= key->key_part;
    KEY_PART_INFO *end= key_part + key->user_defined_key_parts;
    NDB_INDEX_TYPE idx_type= get_index_type_from_key(idx, key_info, false);
    DBUG_PRINT("info", ("Adding index: '%s'", key_info[idx].name));
    // Add fields to key_part struct
    for (; key_part != end; key_part++)
      key_part->field= table->field[key_part->fieldnr];
    // Check index type
    // Create index in ndb
    if((error= create_index(thd, key_info[idx].name, key, idx_type, idx)))
      break;
  }
  DBUG_RETURN(error);  
}

/*
  Mark one or several indexes for deletion. and
  renumber the remaining indexes
*/
int ha_ndbcluster::prepare_drop_index(TABLE *table_arg, 
                                      uint *key_num, uint num_of_keys)
{
  DBUG_ENTER("ha_ndbcluster::prepare_drop_index");
  DBUG_ASSERT(m_share->state == NSS_ALTERED);
  // Mark indexes for deletion
  uint idx;
  for (idx= 0; idx < num_of_keys; idx++)
  {
    DBUG_PRINT("info", ("ha_ndbcluster::prepare_drop_index %u", *key_num));
    uint i = *key_num++;
    m_index[i].status= TO_BE_DROPPED;
    // Prepare delete of index stat entry
    if (m_index[i].type == PRIMARY_KEY_ORDERED_INDEX ||
        m_index[i].type == UNIQUE_ORDERED_INDEX ||
        m_index[i].type == ORDERED_INDEX)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      if (index) // safety
      {
        int index_id= index->getObjectId();
        int index_version= index->getObjectVersion();
        ndb_index_stat_free(m_share, index_id, index_version);
      }
    }
  }
  // Renumber indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  renumber_indexes(ndb, table_arg);
  DBUG_RETURN(0);
}
 
/*
  Really drop all indexes marked for deletion
*/
int ha_ndbcluster::final_drop_index(TABLE *table_arg)
{
  int error;
  DBUG_ENTER("ha_ndbcluster::final_drop_index");
  // Really drop indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  error= drop_indexes(ndb, table_arg);
  DBUG_RETURN(error);
}

/**
  Rename a table in NDB Cluster.
*/

int ha_ndbcluster::rename_table(const char *from, const char *to)
{
  THD *thd= current_thd;
  NDBDICT *dict;
  char old_dbname[FN_HEADLEN];
  char new_dbname[FN_HEADLEN];
  char new_tabname[FN_HEADLEN];
  const NDBTAB *orig_tab;
  int result;
  bool recreate_indexes= FALSE;
  NDBDICT::List index_list;

  DBUG_ENTER("ha_ndbcluster::rename_table");
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));

  if (thd == injector_thd)
  {
    /*
      Table was renamed remotely is already
      renamed inside ndb.
      Just rename .ndb file.
     */
    DBUG_RETURN(handler::rename_table(from, to));
  }

  set_dbname(from, old_dbname);
  set_dbname(to, new_dbname);
  set_tabname(from);
  set_tabname(to, new_tabname);

  if (check_ndb_connection(thd))
    DBUG_RETURN(my_errno= HA_ERR_NO_CONNECTION);

  Thd_ndb *thd_ndb= thd_get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::rename_table"))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  Ndb *ndb= get_ndb(thd);
  ndb->setDatabaseName(old_dbname);
  dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(orig_tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

  if (my_strcasecmp(system_charset_info, new_dbname, old_dbname))
  {
    dict->listIndexes(index_list, *orig_tab);    
    recreate_indexes= TRUE;
  }
  // Change current database to that of target table
  set_dbname(to);
  if (ndb->setDatabaseName(m_dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }

  int ndb_table_id= orig_tab->getObjectId();
  int ndb_table_version= orig_tab->getObjectVersion();
  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(from, 0, FALSE);
  int is_old_table_tmpfile= IS_TMP_PREFIX(m_tabname);
  int is_new_table_tmpfile= IS_TMP_PREFIX(new_tabname);
  if (!is_new_table_tmpfile && !is_old_table_tmpfile)
  {
    /*
      this is a "real" rename table, i.e. not tied to an offline alter table
      - send new name == "to" in query field
    */
    ndbcluster_log_schema_op(thd, to, (int)strlen(to),
                             old_dbname, m_tabname,
                             ndb_table_id, ndb_table_version,
                             SOT_RENAME_TABLE_PREPARE,
                             m_dbname, new_tabname);
  }
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
    ndbcluster_prepare_rename_share(share, to);
    int ret = ndbcluster_rename_share(thd, share);
    assert(ret == 0); NDB_IGNORE_VALUE(ret);
  }

  NdbDictionary::Table new_tab= *orig_tab;
  new_tab.setName(new_tabname);
  if (dict->alterTableGlobal(*orig_tab, new_tab) != 0)
  {
    NdbError ndb_error= dict->getNdbError();
    if (share)
    {
      int ret = ndbcluster_undo_rename_share(thd, share);
      assert(ret == 0); NDB_IGNORE_VALUE(ret);
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
    }
    ERR_RETURN(ndb_error);
  }

  // Rename .ndb file
  if ((result= handler::rename_table(from, to)))
  {
    // ToDo in 4.1 should rollback alter table...
    if (share)
    {
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
    }
    DBUG_RETURN(result);
  }

  /* handle old table */
  if (!is_old_table_tmpfile)
  {
    ndbcluster_drop_event(thd, ndb, share, "rename table", 
                          old_dbname, m_tabname);
  }

  if (!result && !is_new_table_tmpfile)
  {
    Ndb_table_guard ndbtab_g2(dict, new_tabname);
    const NDBTAB *ndbtab= ndbtab_g2.get_table();
#ifdef HAVE_NDB_BINLOG
    if (share)
      ndbcluster_read_binlog_replication(thd, ndb, share, ndbtab,
                                         ::server_id, TRUE);
#endif
    /* always create an event for the table */
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, new_dbname, new_tabname, 
                       get_binlog_full(share));

    if (!Ndb_dist_priv_util::is_distributed_priv_table(new_dbname,
                                                       new_tabname) &&
        !ndbcluster_create_event(thd, ndb, ndbtab, event_name.c_ptr(), share,
                                 share && ndb_binlog_running ? 2 : 1/* push warning */))
    {
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: RENAME Event: %s",
                              event_name.c_ptr());
      if (share && (share->op == 0) &&
          ndbcluster_create_event_ops(thd, share, ndbtab, event_name.c_ptr()))
      {
        sql_print_error("NDB Binlog: FAILED create event operations "
                        "during RENAME. Event %s", event_name.c_ptr());
        /* a warning has been issued to the client */
      }
    }
    /*
      warning has been issued if ndbcluster_create_event failed
      and (share && ndb_binlog_running)
    */
    if (!is_old_table_tmpfile)
    {
      /* "real" rename table */
      ndbcluster_log_schema_op(thd, thd->query(), thd->query_length(),
                               old_dbname, m_tabname,
                               ndb_table_id, ndb_table_version,
                               SOT_RENAME_TABLE,
                               m_dbname, new_tabname);
    }
    else
    {
      /* final phase of offline alter table */
      ndbcluster_log_schema_op(thd, thd->query(), thd->query_length(),
                               m_dbname, new_tabname,
                               ndb_table_id, ndb_table_version,
                               SOT_ALTER_TABLE_COMMIT,
                               NULL, NULL);

    }
  }

  // If we are moving tables between databases, we need to recreate
  // indexes
  if (recreate_indexes)
  {
    for (unsigned i = 0; i < index_list.count; i++) 
    {
        NDBDICT::List::Element& index_el = index_list.elements[i];
	// Recreate any indexes not stored in the system database
	if (my_strcasecmp(system_charset_info, 
			  index_el.database, NDB_SYSTEM_DATABASE))
	{
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  const NDBINDEX * index= dict->getIndexGlobal(index_el.name,  new_tab);
	  DBUG_PRINT("info", ("Creating index %s/%s",
			      index_el.database, index->getName()));
	  dict->createIndex(*index, new_tab);
	  DBUG_PRINT("info", ("Dropping index %s/%s",
			      index_el.database, index->getName()));
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  dict->dropIndexGlobal(*index);
	}
    }
  }
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }

  DBUG_RETURN(result);
}


/**
  Delete table from NDB Cluster.
*/

static
void
delete_table_drop_share(NDB_SHARE* share, const char * path)
{
  DBUG_ENTER("delete_table_drop_share");
  if (share)
  {
    pthread_mutex_lock(&ndbcluster_mutex);
do_drop:
    if (share->state != NSS_DROPPED)
    {
      /*
        The share kept by the server has not been freed, free it
      */
      ndbcluster_mark_share_dropped(share);
      /* ndb_share reference create free */
      DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share, TRUE);
    }
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share, TRUE);
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
  else if (path)
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    share= get_share(path, 0, FALSE, TRUE);
    if (share)
    {
      goto do_drop;
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
  DBUG_VOID_RETURN;
}

/* static version which does not need a handler */

int
ha_ndbcluster::drop_table_impl(THD *thd, ha_ndbcluster *h, Ndb *ndb,
                               const char *path,
                               const char *db,
                               const char *table_name)
{
  DBUG_ENTER("ha_ndbcluster::drop_table_impl");
  NDBDICT *dict= ndb->getDictionary();
  int ndb_table_id= 0;
  int ndb_table_version= 0;

  if (!ndb_schema_dist_is_ready())
  {
    /* Don't allow drop table unless schema distribution is ready */
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(path, 0, FALSE);
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
  }

  /* Drop the table from NDB */
  
  int res= 0;
  if (h && h->m_table)
  {
retry_temporary_error1:
    if (dict->dropTableGlobal(*h->m_table) == 0)
    {
      ndb_table_id= h->m_table->getObjectId();
      ndb_table_version= h->m_table->getObjectVersion();
      DBUG_PRINT("info", ("success 1"));
    }
    else
    {
      switch (dict->getNdbError().status)
      {
        case NdbError::TemporaryError:
          if (!thd->killed) 
            goto retry_temporary_error1; // retry indefinitly
          break;
        default:
          break;
      }
      res= ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info", ("error(1) %u", res));
    }
    h->release_metadata(thd, ndb);
  }
  else
  {
    ndb->setDatabaseName(db);
    while (1)
    {
      Ndb_table_guard ndbtab_g(dict, table_name);
      if (ndbtab_g.get_table())
      {
    retry_temporary_error2:
        if (dict->dropTableGlobal(*ndbtab_g.get_table()) == 0)
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
    /* the drop table failed for some reason, drop the share anyways */
    delete_table_drop_share(share, 0);
    DBUG_RETURN(res);
  }

  /* stop the logging of the dropped table, and cleanup */

  /*
    drop table is successful even if table does not exist in ndb
    and in case table was actually not dropped, there is no need
    to force a gcp, and setting the event_name to null will indicate
    that there is no event to be dropped
  */
  int table_dropped= dict->getNdbError().code != 709;

  {
    if (table_dropped)
    {
      ndbcluster_handle_drop_table(thd, ndb, share, "delete table", 
                                   db, table_name);
    }
    else
    {
      /**
       * Setting 0,0 will cause ndbcluster_drop_event *not* to be called
       */
      ndbcluster_handle_drop_table(thd, ndb, share, "delete table", 
                                   0, 0);
    }
  }

  if (!IS_TMP_PREFIX(table_name) && share &&
      thd->lex->sql_command != SQLCOM_TRUNCATE)
  {
    ndbcluster_log_schema_op(thd,
                             thd->query(), thd->query_length(),
                             share->db, share->table_name,
                             ndb_table_id, ndb_table_version,
                             SOT_DROP_TABLE, NULL, NULL);
  }

  delete_table_drop_share(share, 0);
  DBUG_RETURN(0);
}

int ha_ndbcluster::delete_table(const char *name)
{
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);

  DBUG_ENTER("ha_ndbcluster::delete_table");
  DBUG_PRINT("enter", ("name: %s", name));

  if (thd == injector_thd)
  {
    /*
      Table was dropped remotely is already
      dropped inside ndb.
      Just drop local files.
    */
    DBUG_PRINT("info", ("Table is already dropped in NDB"));
    delete_table_drop_share(0, name);
    DBUG_RETURN(handler::delete_table(name));
  }

  set_dbname(name);
  set_tabname(name);

  if (!ndb_schema_dist_is_ready())
  {
    /* Don't allow drop table unless schema distribution is ready */
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  if (check_ndb_connection(thd))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::delete_table"))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  /*
    Drop table in ndb.
    If it was already gone it might have been dropped
    remotely, give a warning and then drop .ndb file.
   */
  int error;
  Ndb* ndb= thd_ndb->ndb;
  if (!(error= drop_table_impl(thd, this, ndb, name,
                               m_dbname, m_tabname)) ||
      error == HA_ERR_NO_SUCH_TABLE)
  {
    /* Call ancestor function to delete .ndb file */
    int error1= handler::delete_table(name);
    if (!error)
      error= error1;
  }

  DBUG_RETURN(error);
}


void ha_ndbcluster::get_auto_increment(ulonglong offset, ulonglong increment,
                                       ulonglong nb_desired_values,
                                       ulonglong *first_value,
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
    Ndb_tuple_id_range_guard g(m_share);
    if ((m_skip_auto_increment &&
         ndb->readAutoIncrementValue(m_table, g.range, auto_value)) ||
        ndb->getAutoIncrementValue(m_table, g.range, auto_value, 
                                   Uint32(m_autoincrement_prefetch), 
                                   increment, offset))
    {
      if (--retries && !thd->killed &&
          ndb->getNdbError().status == NdbError::TemporaryError)
      {
        do_retry_sleep(retry_sleep);
        continue;
      }
      const NdbError err= ndb->getNdbError();
      sql_print_error("Error %lu in ::get_auto_increment(): %s",
                      (ulong) err.code, err.message);
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
  m_thd_ndb(NULL),
  m_active_cursor(NULL),
  m_table(NULL),
  m_ndb_record(0),
  m_ndb_hidden_key_record(0),
  m_table_info(NULL),
  m_share(0),
  m_key_fields(NULL),
  m_part_info(NULL),
  m_user_defined_partitioning(FALSE),
  m_use_partition_pruning(FALSE),
  m_sorted(FALSE),
  m_use_write(FALSE),
  m_ignore_dup_key(FALSE),
  m_has_unique_index(FALSE),
  m_ignore_no_key(FALSE),
  m_read_before_write_removal_possible(FALSE),
  m_read_before_write_removal_used(FALSE),
  m_rows_updated(0),
  m_rows_deleted(0),
  m_rows_to_insert((ha_rows) 1),
  m_rows_inserted((ha_rows) 0),
  m_rows_changed((ha_rows) 0),
  m_delete_cannot_batch(FALSE),
  m_update_cannot_batch(FALSE),
  m_skip_auto_increment(TRUE),
  m_blobs_pending(0),
  m_is_bulk_delete(false),
  m_blobs_row_total_size(0),
  m_blobs_buffer(0),
  m_blobs_buffer_size(0),
  m_dupkey((uint) -1),
  m_autoincrement_prefetch(DEFAULT_AUTO_PREFETCH),
  m_pushed_join_member(NULL),
  m_pushed_join_operation(-1),
  m_disable_pushed_join(FALSE),
  m_active_query(NULL),
  m_pushed_operation(NULL),
  m_cond(NULL),
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
    /* ndb_share reference handler free */
    DBUG_PRINT("NDB_SHARE", ("%s handler free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share);
  }
  release_metadata(thd, ndb);
  release_blobs_buffer();

  // Check for open cursor/transaction
  DBUG_ASSERT(m_thd_ndb == NULL);

  // Discard any generated condition
  DBUG_PRINT("info", ("Deleting generated condition"));
  if (m_cond)
  {
    delete m_cond;
    m_cond= NULL;
  }
  DBUG_PRINT("info", ("Deleting pushed joins"));
  DBUG_ASSERT(m_active_query == NULL);
  DBUG_ASSERT(m_active_cursor == NULL);
  if (m_pushed_join_operation==PUSHED_ROOT)
  {
    delete m_pushed_join_member;             // Also delete QueryDef
  }
  m_pushed_join_member= NULL;
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

int ha_ndbcluster::open(const char *name, int mode, uint test_if_locked)
{
  THD *thd= current_thd;
  int res;
  KEY *key;
  KEY_PART_INFO *key_part_info;
  uint key_parts, i, j;
  DBUG_ENTER("ha_ndbcluster::open");
  DBUG_PRINT("enter", ("name: %s  mode: %d  test_if_locked: %d",
                       name, mode, test_if_locked));

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
    m_key_fields= (MY_BITMAP**)my_malloc(ptr_size + map_size,
                                         MYF(MY_WME + MY_ZEROFILL));
    if (!m_key_fields)
    {
      local_close(thd, FALSE);
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
                      table_share->fields, FALSE))
      {
        m_key_fields[i]= NULL;
        local_close(thd, FALSE);
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
    local_close(thd, FALSE);
    DBUG_RETURN(res);
  }

  // Init table lock structure
  /* ndb_share reference handler */
  if ((m_share=get_share(name, table, FALSE)) == 0)
  {
    /**
     * No share present...we must create one
     */
    if (opt_ndb_extra_logging > 19)
    {
      sql_print_information("Calling ndbcluster_create_binlog_setup(%s) in ::open",
                            name);
    }
    Ndb* ndb= check_ndb_in_thd(thd);
    ndbcluster_create_binlog_setup(thd, ndb, name, (uint)strlen(name),
                                   m_dbname, m_tabname, table);
    if ((m_share=get_share(name, table, FALSE)) == 0)
    {
      local_close(thd, FALSE);
      DBUG_RETURN(1);
    }
  }

  DBUG_PRINT("NDB_SHARE", ("%s handler  use_count: %u",
                           m_share->key, m_share->use_count));
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);

  if ((res= get_metadata(thd, name)))
  {
    local_close(thd, FALSE);
    DBUG_RETURN(res);
  }

  if ((res= update_stats(thd, 1, true)) ||
      (res= info(HA_STATUS_CONST)))
  {
    local_close(thd, TRUE);
    DBUG_RETURN(res);
  }
  if (ndb_binlog_is_read_only())
  {
    table->db_stat|= HA_READ_ONLY;
    sql_print_information("table '%s' opened read only", name);
  }
  DBUG_RETURN(0);
}

/*
 * Support for OPTIMIZE TABLE
 * reclaims unused space of deleted rows
 * and updates index statistics
 */
int ha_ndbcluster::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  ulong error, stats_error= 0;
  const uint delay= (uint)THDVAR(thd, optimization_delay);

  error= ndb_optimize_table(thd, delay);
  stats_error= update_stats(thd, 1);
  return (error) ? error : stats_error;
}

int ha_ndbcluster::ndb_optimize_table(THD* thd, uint delay)
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
    my_sleep(1000*delay);
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
    if (m_index[i].status == ACTIVE)
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
          my_sleep(1000*delay);        
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
          my_sleep(1000*delay);
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

int ha_ndbcluster::analyze(THD* thd, HA_CHECK_OPT* check_opt)
{
  int err;
  if ((err= update_stats(thd, 1)) != 0)
    return err;
  const bool index_stat_enable= THDVAR(NULL, index_stat_enable) &&
                                THDVAR(thd, index_stat_enable);
  if (index_stat_enable)
  {
    if ((err= analyze_index(thd)) != 0)
      return err;
  }
  return 0;
}

int
ha_ndbcluster::analyze_index(THD *thd)
{
  DBUG_ENTER("ha_ndbcluster::analyze_index");

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;

  uint inx_list[MAX_INDEXES];
  uint inx_count= 0;

  uint inx;
  for (inx= 0; inx < table_share->keys; inx++)
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
    int err= ndb_index_stat_analyze(ndb, inx_list, inx_count);
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
    m_use_partition_pruning= FALSE;
    if (!(m_part_info->part_type == HASH_PARTITION &&
          m_part_info->list_of_part_fields &&
          !m_part_info->is_sub_partitioned()))
    {
      /*
        PARTITION BY HASH, RANGE and LIST plus all subpartitioning variants
        all use MySQL defined partitioning. PARTITION BY KEY uses NDB native
        partitioning scheme.
      */
      m_use_partition_pruning= TRUE;
      m_user_defined_partitioning= TRUE;
    }
    if (m_part_info->part_type == HASH_PARTITION &&
        m_part_info->list_of_part_fields &&
        partition_info_num_full_part_fields(m_part_info) == 0)
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
      m_use_partition_pruning= FALSE;
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
    my_free((char*)m_key_fields, MYF(0));
    m_key_fields= NULL;
  }
  if (m_share)
  {
    /* ndb_share reference handler free */
    DBUG_PRINT("NDB_SHARE", ("%s handler free  use_count: %u",
                             m_share->key, m_share->use_count));
    free_share(&m_share);
  }
  m_share= 0;
  if (release_metadata_flag)
  {
    ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
    release_metadata(thd, ndb);
  }
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");
  THD *thd= table->in_use;
  local_close(thd, TRUE);
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


static int ndbcluster_close_connection(handlerton *hton, THD *thd)
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
  Try to discover one table from NDB.
*/
static
int ndbcluster_discover(handlerton *hton, THD* thd, const char *db, 
                        const char *name,
                        uchar **frmblob, 
                        size_t *frmlen)
{
  int error= 0;
  NdbError ndb_error;
  size_t len;
  uchar* data= NULL;
  Ndb* ndb;
  char key[FN_REFLEN + 1];
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name)); 

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);  
  if (ndb->setDatabaseName(db))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  NDBDICT* dict= ndb->getDictionary();
  build_table_filename(key, sizeof(key) - 1, db, name, "", 0);
  /* ndb_share reference temporary */
  NDB_SHARE *share= get_share(key, 0, FALSE);
  if (share)
  {
    DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                             share->key, share->use_count));
  }
  if (share && get_ndb_share_state(share) == NSS_ALTERED)
  {
    // Frm has been altered on disk, but not yet written to ndb
    if (readfrm(key, &data, &len))
    {
      DBUG_PRINT("error", ("Could not read frm"));
      error= 1;
      goto err;
    }
  }
  else
  {
    Ndb_table_guard ndbtab_g(dict, name);
    const NDBTAB *tab= ndbtab_g.get_table();
    if (!tab)
    {
      const NdbError err= dict->getNdbError();
      if (err.code == 709 || err.code == 723)
      {
        error= -1;
        DBUG_PRINT("info", ("ndb_error.code: %u", ndb_error.code));
      }
      else
      {
        error= -1;
        ndb_error= err;
        DBUG_PRINT("info", ("ndb_error.code: %u", ndb_error.code));
      }
      goto err;
    }
    DBUG_PRINT("info", ("Found table %s", tab->getName()));
    
    len= tab->getFrmLength();  
    if (len == 0 || tab->getFrmData() == NULL)
    {
      DBUG_PRINT("error", ("No frm data found."));
      error= 1;
      goto err;
    }
    
    if (unpackfrm(&data, &len, (uchar*) tab->getFrmData()))
    {
      DBUG_PRINT("error", ("Could not unpack table"));
      error= 1;
      goto err;
    }
  }
#ifdef HAVE_NDB_BINLOG
  if (ndbcluster_check_if_local_table(db, name) &&
      !Ndb_dist_priv_util::is_distributed_priv_table(db, name))
  {
    DBUG_PRINT("info", ("ndbcluster_discover: Skipping locally defined table '%s.%s'",
                        db, name));
    sql_print_error("ndbcluster_discover: Skipping locally defined table '%s.%s'",
                    db, name);
    error= 1;
    goto err;
  }
#endif
  *frmlen= len;
  *frmblob= data;
  
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }

  DBUG_RETURN(0);
err:
  my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
  if (share)
  {
    /* ndb_share reference temporary free */
    DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }

  if (ndb_error.code)
  {
    ERR_RETURN(ndb_error);
  }
  DBUG_RETURN(error);
}

/**
  Check if a table exists in NDB.
*/
static
int ndbcluster_table_exists_in_engine(handlerton *hton, THD* thd, 
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
    ERR_RETURN(dict->getNdbError());
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


extern "C" uchar* tables_get_key(const char *entry, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= strlen(entry);
  return (uchar*) entry;
}


/**
  Drop a database in NDB Cluster

  @note
    add a dummy void function, since stupid handlerton is returning void instead of int...
*/
int ndbcluster_drop_database_impl(THD *thd, const char *path)
{
  DBUG_ENTER("ndbcluster_drop_database");
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
    DBUG_RETURN(-1);
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));     
    
    // Add only tables that belongs to db
    // Ignore Blob part tables - they are deleted when their table
    // is deleted.
    if (my_strcasecmp(system_charset_info, elmt.database, dbname) ||
        IS_NDB_BLOB_PREFIX(elmt.name))
      continue;
    DBUG_PRINT("info", ("%s must be dropped", elmt.name));     
    drop_list.push_back(thd->strdup(elmt.name));
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
    if (ha_ndbcluster::drop_table_impl(thd, 0, ndb, full_path, dbname, tabname))
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

static void ndbcluster_drop_database(handlerton *hton, char *path)
{
  THD *thd= current_thd;
  DBUG_ENTER("ndbcluster_drop_database");

  if (!ndb_schema_dist_is_ready())
  {
    /* Don't allow drop database unless schema distribution is ready */
    DBUG_VOID_RETURN;
  }

  ndbcluster_drop_database_impl(thd, path);
  char db[FN_REFLEN];
  ha_ndbcluster::set_dbname(path, db);
  uint32 table_id= 0, table_version= 0;
  /*
    Since databases aren't real ndb schema object
    they don't have any id/version

    But since that id/version is used to make sure that event's on SCHEMA_TABLE
    is correct, we set random numbers
  */
  table_id = (uint32)rand();
  table_version = (uint32)rand();
  ndbcluster_log_schema_op(thd,
                           thd->query(), thd->query_length(),
                           db, "", table_id, table_version,
                           SOT_DROP_DB, NULL, NULL);
  DBUG_VOID_RETURN;
}

int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name)
{
  // Copy db and table_name to stack buffers since functions used by
  // ha_create_table_from_engine may convert to lowercase on some platforms
  char db_buf[FN_REFLEN + 1];
  char table_name_buf[FN_REFLEN + 1];
  strnmov(db_buf, db, sizeof(db_buf));
  strnmov(table_name_buf, table_name, sizeof(table_name_buf));

  LEX *old_lex= thd->lex, newlex;
  thd->lex= &newlex;
  newlex.current_select= NULL;
  lex_start(thd);
  int res= ha_create_table_from_engine(thd, db_buf, table_name_buf);
  thd->lex= old_lex;
  return res;
}


static int
ndbcluster_find_files(handlerton *hton, THD *thd,
                      const char *db, const char *path,
                      const char *wild, bool dir, List<LEX_STRING> *files)
{
  DBUG_ENTER("ndbcluster_find_files");
  DBUG_PRINT("enter", ("db: %s", db));
  { // extra bracket to avoid gcc 2.95.3 warning
  uint i;
  Thd_ndb *thd_ndb;
  Ndb* ndb;
  char name[FN_REFLEN + 1];
  HASH ndb_tables, ok_tables;
  NDBDICT::List list;

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  thd_ndb= get_thd_ndb(thd);

  if (dir)
    DBUG_RETURN(0); // Discover of databases not yet supported

  Ndb_global_schema_lock_guard ndb_global_schema_lock_guard(thd);
  if (ndb_global_schema_lock_guard.lock())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());

  if (my_hash_init(&ndb_tables, table_alias_charset,list.count,0,0,
                   (my_hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ndb_tables"));
    DBUG_RETURN(-1);
  }

  if (my_hash_init(&ok_tables, system_charset_info,32,0,0,
                   (my_hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ok_tables"));
    my_hash_free(&ndb_tables);
    DBUG_RETURN(-1);
  }  

  for (i= 0 ; i < list.count ; i++)
  {
    NDBDICT::List::Element& elmt= list.elements[i];
    if (IS_TMP_PREFIX(elmt.name) || IS_NDB_BLOB_PREFIX(elmt.name))
    {
      DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
      continue;
    }
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));

    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, elmt.database, db))
      continue;

    // Apply wildcard to list of tables in NDB
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info, elmt.name, wild))
          continue;
      }
      else if (wild_compare(elmt.name,wild,0))
        continue;
    }
    DBUG_PRINT("info", ("Inserting %s into ndb_tables hash", elmt.name));     
    my_hash_insert(&ndb_tables, (uchar*)thd->strdup(elmt.name));
  }

  LEX_STRING *file_name;
  List_iterator<LEX_STRING> it(*files);
  List<char> delete_list;
  char *file_name_str;
  while ((file_name=it++))
  {
    bool file_on_disk= FALSE;
    DBUG_PRINT("info", ("%s", file_name->str));
    if (my_hash_search(&ndb_tables,
                       (const uchar*)file_name->str, file_name->length))
    {
      build_table_filename(name, sizeof(name) - 1, db,
                           file_name->str, reg_ext, 0);
      if (my_access(name, F_OK))
      {
        DBUG_PRINT("info", ("Table %s listed and need discovery",
                            file_name->str));
        if (ndb_create_table_from_engine(thd, db, file_name->str))
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_TABLE_EXISTS_ERROR,
                              "Discover of table %s.%s failed",
                              db, file_name->str);
          continue;
        }
      }
      DBUG_PRINT("info", ("%s existed in NDB _and_ on disk ", file_name->str));
      file_on_disk= TRUE;
    }
    
    // Check for .ndb file with this name
    build_table_filename(name, sizeof(name) - 1, db,
                         file_name->str, ha_ndb_ext, 0);
    DBUG_PRINT("info", ("Check access for %s", name));
    if (my_access(name, F_OK))
    {
      DBUG_PRINT("info", ("%s did not exist on disk", name));     
      // .ndb file did not exist on disk, another table type
      if (file_on_disk)
      {
	// Ignore this ndb table 
 	uchar *record= my_hash_search(&ndb_tables,
                                      (const uchar*) file_name->str,
                                      file_name->length);
	DBUG_ASSERT(record);
	my_hash_delete(&ndb_tables, record);
	push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
			    ER_TABLE_EXISTS_ERROR,
			    "Local table %s.%s shadows ndb table",
			    db, file_name->str);
      }
      continue;
    }
    if (file_on_disk) 
    {
      // File existed in NDB and as frm file, put in ok_tables list
      my_hash_insert(&ok_tables, (uchar*) file_name->str);
      continue;
    }
    DBUG_PRINT("info", ("%s existed on disk", name));     
    // The .ndb file exists on disk, but it's not in list of tables in ndb
    // Verify that handler agrees table is gone.
    if (ndbcluster_table_exists_in_engine(hton, thd, db, file_name->str) ==
        HA_ERR_NO_SUCH_TABLE)
    {
      DBUG_PRINT("info", ("NDB says %s does not exists", file_name->str));
      it.remove();
      if (thd == injector_thd &&
	  thd_ndb->options & TNTO_NO_REMOVE_STRAY_FILES)
      {
	/*
	  Don't delete anything when called from
	  the binlog thread. This is a kludge to avoid
	  that something is deleted when "Ndb schema dist"
	  uses find_files() to check for "local tables in db"
	*/
      }
      else
	// Put in list of tables to remove from disk
	delete_list.push_back(thd->strdup(file_name->str));
    }
  }

  /* setup logging to binlog for all discovered tables */
  {
    char *end, *end1= name +
      build_table_filename(name, sizeof(name) - 1, db, "", "", 0);
    for (i= 0; i < ok_tables.records; i++)
    {
      file_name_str= (char*)my_hash_element(&ok_tables, i);
      end= end1 +
        tablename_to_filename(file_name_str, end1, (uint)(sizeof(name) - (end1 - name)));
      ndbcluster_create_binlog_setup(thd, ndb, name, (uint)(end-name),
                                     db, file_name_str, 0);
    }
  }

  // Check for new files to discover
  DBUG_PRINT("info", ("Checking for new files to discover"));       
  List<char> create_list;
  for (i= 0 ; i < ndb_tables.records ; i++)
  {
    file_name_str= (char*) my_hash_element(&ndb_tables, i);
    if (!my_hash_search(&ok_tables,
                        (const uchar*) file_name_str, strlen(file_name_str)))
    {
      build_table_filename(name, sizeof(name) - 1,
                           db, file_name_str, reg_ext, 0);
      if (my_access(name, F_OK))
      {
        DBUG_PRINT("info", ("%s must be discovered", file_name_str));
        // File is in list of ndb tables and not in ok_tables
        // This table need to be created
        create_list.push_back(thd->strdup(file_name_str));
      }
    }
  }

  if (thd == injector_thd)
  {
    /*
      Don't delete anything when called from
      the binlog thread. This is a kludge to avoid
      that something is deleted when "Ndb schema dist"
      uses find_files() to check for "local tables in db"
    */
  }
  else
  {
    /*
      Delete old files
      (.frm files with corresponding .ndb + does not exists in NDB)
    */
    List_iterator_fast<char> it3(delete_list);
    while ((file_name_str= it3++))
    {
      DBUG_PRINT("info", ("Deleting local files for table '%s.%s'",
                          db, file_name_str));

      // Delete the table and its related files from disk
      Ndb_local_schema::Table local_table(thd, db, file_name_str);
      local_table.remove_table();

      // Flush the table out of ndbapi's dictionary cache
      Ndb_table_guard ndbtab_g(ndb->getDictionary(), file_name_str);
      ndbtab_g.invalidate();

      // Flush the table from table def. cache.
      TABLE_LIST table_list;
      memset(&table_list, 0, sizeof(table_list));
      table_list.db= (char*)db;
      table_list.alias= table_list.table_name= file_name_str;
      close_cached_tables(thd, &table_list, false, 0);

      DBUG_ASSERT(!thd->is_error());
    }
  }

  // Create new files
  List_iterator_fast<char> it2(create_list);
  while ((file_name_str=it2++))
  {  
    DBUG_PRINT("info", ("Table %s need discovery", file_name_str));
    if (ndb_create_table_from_engine(thd, db, file_name_str) == 0)
    {
      LEX_STRING *tmp_file_name= 0;
      tmp_file_name= thd->make_lex_string(tmp_file_name, file_name_str,
                                          (uint)strlen(file_name_str), TRUE);
      files->push_back(tmp_file_name); 
    }
  }

  my_hash_free(&ok_tables);
  my_hash_free(&ndb_tables);

  /* Hide mysql.ndb_schema table */
  if (!strcmp(db, NDB_REP_DB))
  {
    LEX_STRING* file_name;
    List_iterator<LEX_STRING> it(*files);
    while ((file_name= it++))
    {
      if (!strcmp(file_name->str, NDB_SCHEMA_TABLE))
      {
        DBUG_PRINT("info", ("Hiding table '%s.%s'", db, file_name->str));
        it.remove();
      }
    }
  }
  } // extra bracket to avoid gcc 2.95.3 warning
  DBUG_RETURN(0);    
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


/*
  Initialise all gloal variables before creating 
  a NDB Cluster table handler
 */

/* Call back after cluster connect */
static int connect_callback()
{
  pthread_mutex_lock(&ndb_util_thread.LOCK);
  update_status_variables(NULL, &g_ndb_status,
                          g_ndb_cluster_connection);

  uint node_id, i= 0;
  Ndb_cluster_connection_node_iter node_iter;
  memset((void *)g_node_id_map, 0xFFFF, sizeof(g_node_id_map));
  while ((node_id= g_ndb_cluster_connection->get_next_node(node_iter)))
    g_node_id_map[node_id]= i++;

  pthread_cond_broadcast(&ndb_util_thread.COND);
  pthread_mutex_unlock(&ndb_util_thread.LOCK);
  return 0;
}

/**
 * Components
 */
Ndb_util_thread ndb_util_thread;
Ndb_index_stat_thread ndb_index_stat_thread;

extern THD * ndb_create_thd(char * stackptr);

#ifndef NDB_NO_WAIT_SETUP
static int ndb_wait_setup_func_impl(ulong max_wait)
{
  DBUG_ENTER("ndb_wait_setup_func_impl");

  pthread_mutex_lock(&ndbcluster_mutex);

  struct timespec abstime;
  set_timespec(abstime, 1);

  while (max_wait &&
         (!ndb_setup_complete || !ndb_index_stat_thread.is_setup_complete()))
  {
    int rc= pthread_cond_timedwait(&COND_ndb_setup_complete,
                                   &ndbcluster_mutex,
                                   &abstime);
    if (rc)
    {
      if (rc == ETIMEDOUT)
      {
        DBUG_PRINT("info", ("1s elapsed waiting"));
        max_wait--;
        set_timespec(abstime, 1); /* 1 second from now*/
      }
      else
      {
        DBUG_PRINT("info", ("Bad pthread_cond_timedwait rc : %u",
                            rc));
        assert(false);
        break;
      }
    }
  }

  pthread_mutex_unlock(&ndbcluster_mutex);

#ifndef NDB_WITHOUT_DIST_PRIV
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
      delete thd;
    }
  } while (0);
#endif

  DBUG_RETURN((ndb_setup_complete == 1)? 0 : 1);
}

int(*ndb_wait_setup_func)(ulong) = 0;
#endif

static int
ndbcluster_make_pushed_join(handlerton *, THD*, const AQP::Join_plan*);

/* Version in composite numerical format */
static Uint32 ndb_version = NDB_VERSION_D;
static MYSQL_SYSVAR_UINT(
  version,                          /* name */
  ndb_version,                      /* var */
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY,
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
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY,
  "Compile version string for ndbcluster",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);

extern int ndb_dictionary_is_mysqld;

handlerton* ndbcluster_hton;

static int ndbcluster_init(void *p)
{
  DBUG_ENTER("ndbcluster_init");

  if (ndbcluster_inited)
    DBUG_RETURN(FALSE);

#ifdef HAVE_NDB_BINLOG
  /* Check const alignment */
  assert(DependencyTracker::InvalidTransactionId ==
         Ndb_binlog_extra_row_info::InvalidTransactionId);
#endif
  ndb_util_thread.init();
  ndb_index_stat_thread.init();

  pthread_mutex_init(&ndbcluster_mutex,MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_ndb_setup_complete, NULL);
  ndbcluster_terminating= 0;
  ndb_dictionary_is_mysqld= 1;
  ndb_setup_complete= 0;
  ndbcluster_hton= (handlerton *)p;
  ndbcluster_global_schema_lock_init(ndbcluster_hton);

  {
    handlerton *h= ndbcluster_hton;
    h->state=            SHOW_OPTION_YES;
    h->db_type=          DB_TYPE_NDBCLUSTER;
    h->close_connection= ndbcluster_close_connection;
    h->commit=           ndbcluster_commit;
    h->rollback=         ndbcluster_rollback;
    h->create=           ndbcluster_create_handler; /* Create a new handler */
    h->drop_database=    ndbcluster_drop_database;  /* Drop a database */
    h->panic=            ndbcluster_end;            /* Panic call */
    h->show_status=      ndbcluster_show_status;    /* Show status */
    h->alter_tablespace= ndbcluster_alter_tablespace;    /* Show status */
    h->partition_flags=  ndbcluster_partition_flags; /* Partition flags */
    h->alter_table_flags=ndbcluster_alter_table_flags; /* Alter table flags */
#if MYSQL_VERSION_ID >= 50501
    h->fill_is_table=    ndbcluster_fill_is_table;
#else
    h->fill_files_table= ndbcluster_fill_files_table;
#endif
    ndbcluster_binlog_init_handlerton();
    h->flags=            HTON_CAN_RECREATE | HTON_TEMPORARY_NOT_SUPPORTED |
      HTON_NO_BINLOG_ROW_OPT;
    h->discover=         ndbcluster_discover;
    h->find_files=       ndbcluster_find_files;
    h->table_exists_in_engine= ndbcluster_table_exists_in_engine;
    h->make_pushed_join= ndbcluster_make_pushed_join;
    h->is_supported_system_table = is_supported_system_table;
  }

  // Initialize ndb interface
  ndb_init_internal();

  /* allocate connection resources and connect to cluster */
  const uint global_opti_node_select= THDVAR(NULL, optimized_node_selection);
  if (ndbcluster_connect(connect_callback, opt_ndb_wait_connected,
                         opt_ndb_cluster_connection_pool,
                         (global_opti_node_select & 1),
                         opt_ndb_connectstring,
                         opt_ndb_nodeid))
  {
    DBUG_PRINT("error", ("Could not initiate connection to cluster"));
    goto ndbcluster_init_error;
  }

  (void) my_hash_init(&ndbcluster_open_tables,table_alias_charset,32,0,0,
                      (my_hash_get_key) ndbcluster_get_key,0,0);
  (void) my_hash_init(&ndbcluster_dropped_tables,table_alias_charset,32,0,0,
                      (my_hash_get_key) ndbcluster_get_key,0,0);
  /* start the ndb injector thread */
  if (ndbcluster_binlog_start())
  {
    DBUG_PRINT("error", ("Could start the injector thread"));
    goto ndbcluster_init_error;
  }

  // Create utility thread
  if (ndb_util_thread.start())
  {
    DBUG_PRINT("error", ("Could not create ndb utility thread"));
    my_hash_free(&ndbcluster_open_tables);
    my_hash_free(&ndbcluster_dropped_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    pthread_cond_destroy(&COND_ndb_setup_complete);
    goto ndbcluster_init_error;
  }

  /* Wait for the util thread to start */
  pthread_mutex_lock(&ndb_util_thread.LOCK);
  while (ndb_util_thread.running < 0)
    pthread_cond_wait(&ndb_util_thread.COND_ready, &ndb_util_thread.LOCK);
  pthread_mutex_unlock(&ndb_util_thread.LOCK);
  
  if (!ndb_util_thread.running)
  {
    DBUG_PRINT("error", ("ndb utility thread exited prematurely"));
    my_hash_free(&ndbcluster_open_tables);
    my_hash_free(&ndbcluster_dropped_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    pthread_cond_destroy(&COND_ndb_setup_complete);
    goto ndbcluster_init_error;
  }

  // Create index statistics thread
  if (ndb_index_stat_thread.start())
  {
    DBUG_PRINT("error", ("Could not create ndb index statistics thread"));
    my_hash_free(&ndbcluster_open_tables);
    my_hash_free(&ndbcluster_dropped_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    goto ndbcluster_init_error;
  }

  /* Wait for the index statistics thread to start */
  pthread_mutex_lock(&ndb_index_stat_thread.LOCK);
  while (ndb_index_stat_thread.running < 0)
    pthread_cond_wait(&ndb_index_stat_thread.COND_ready,
                      &ndb_index_stat_thread.LOCK);
  pthread_mutex_unlock(&ndb_index_stat_thread.LOCK);

  if (!ndb_index_stat_thread.running)
  {
    DBUG_PRINT("error", ("ndb index statistics thread exited prematurely"));
    my_hash_free(&ndbcluster_open_tables);
    my_hash_free(&ndbcluster_dropped_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    goto ndbcluster_init_error;
  }

#ifndef NDB_NO_WAIT_SETUP
  ndb_wait_setup_func= ndb_wait_setup_func_impl;
#endif

  memset(&g_slave_api_client_stats, 0, sizeof(g_slave_api_client_stats));

  ndbcluster_inited= 1;
  DBUG_RETURN(FALSE);

ndbcluster_init_error:
  ndb_util_thread.deinit();
  ndb_index_stat_thread.deinit();
  /* disconnect from cluster and free connection resources */
  ndbcluster_disconnect();
  ndbcluster_hton->state= SHOW_OPTION_DISABLED;               // If we couldn't use handler

  ndbcluster_global_schema_lock_deinit();

  DBUG_RETURN(TRUE);
}

#ifndef DBUG_OFF
static
const char*
get_share_state_string(NDB_SHARE_STATE s)
{
  switch(s) {
  case NSS_INITIAL:
    return "NSS_INITIAL";
  case NSS_ALTERED:
    return "NSS_ALTERED";
  case NSS_DROPPED:
    return "NSS_DROPPED";
  }
  assert(false);
  return "<unknown>";
}
#endif

int ndbcluster_binlog_end(THD *thd);

static int ndbcluster_end(handlerton *hton, ha_panic_function type)
{
  DBUG_ENTER("ndbcluster_end");

  if (!ndbcluster_inited)
    DBUG_RETURN(0);
  ndbcluster_inited= 0;

  /* wait for index stat thread to finish */
  sql_print_information("Stopping Cluster Index Statistics thread");
  pthread_mutex_lock(&ndb_index_stat_thread.LOCK);
  ndbcluster_terminating= 1;
  pthread_cond_signal(&ndb_index_stat_thread.COND);
  while (ndb_index_stat_thread.running > 0)
    pthread_cond_wait(&ndb_index_stat_thread.COND_ready,
                      &ndb_index_stat_thread.LOCK);
  pthread_mutex_unlock(&ndb_index_stat_thread.LOCK);

  /* wait for util and binlog thread to finish */
  ndbcluster_binlog_end(NULL);

  {
    pthread_mutex_lock(&ndbcluster_mutex);
    uint save = ndbcluster_open_tables.records; (void)save;
    while (ndbcluster_open_tables.records)
    {
      NDB_SHARE *share=
        (NDB_SHARE*) my_hash_element(&ndbcluster_open_tables, 0);
#ifndef DBUG_OFF
      fprintf(stderr,
              "NDB: table share %s with use_count %d state: %s(%u) not freed\n",
              share->key, share->use_count,
              get_share_state_string(share->state),
              (uint)share->state);
#endif
      ndbcluster_real_free_share(&share);
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_ASSERT(save == 0);
  }
  my_hash_free(&ndbcluster_open_tables);

  {
    pthread_mutex_lock(&ndbcluster_mutex);
    uint save = ndbcluster_dropped_tables.records; (void)save;
    while (ndbcluster_dropped_tables.records)
    {
      NDB_SHARE *share=
        (NDB_SHARE*) my_hash_element(&ndbcluster_dropped_tables, 0);
#ifndef DBUG_OFF
      fprintf(stderr,
              "NDB: table share %s with use_count %d state: %s(%u) not freed\n",
              share->key, share->use_count,
              get_share_state_string(share->state),
              (uint)share->state);
      /**
       * For unknown reasons...the dist-priv tables linger here
       * TODO investigate why
       */
      if (Ndb_dist_priv_util::is_distributed_priv_table(share->db,
                                                        share->table_name))
      {
        save--;
      }
#endif
      ndbcluster_real_free_share(&share);
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_ASSERT(save == 0);
  }
  my_hash_free(&ndbcluster_dropped_tables);

  ndb_index_stat_end();
  ndbcluster_disconnect();

  ndbcluster_global_schema_lock_deinit();
  ndb_util_thread.deinit();
  ndb_index_stat_thread.deinit();

  pthread_mutex_destroy(&ndbcluster_mutex);
  pthread_cond_destroy(&COND_ndb_setup_complete);

  // cleanup ndb interface
  ndb_end_internal();

  DBUG_RETURN(0);
}

void ha_ndbcluster::print_error(int error, myf errflag)
{
  DBUG_ENTER("ha_ndbcluster::print_error");
  DBUG_PRINT("enter", ("error: %d", error));

  if (error == HA_ERR_NO_PARTITION_FOUND)
    m_part_info->print_no_partition_found(table);
  else
  {
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

    handler::print_error(error, errflag);
  }
  DBUG_VOID_RETURN;
}


/**
  Set a given location from full pathname to database name.
*/

void ha_ndbcluster::set_dbname(const char *path_name, char *dbname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN + 1];
 
  tmp_name= tmp_buff;
  /* Scan name from the end */
  ptr= strend(path_name)-1;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end= ptr;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= (uint)(end - ptr);
  memcpy(tmp_name, ptr + 1, name_len);
  tmp_name[name_len]= '\0';
  filename_to_tablename(tmp_name, dbname, sizeof(tmp_buff) - 1);
}

/**
  Set m_dbname from full pathname to table file.
*/

void ha_ndbcluster::set_dbname(const char *path_name)
{
  set_dbname(path_name, m_dbname);
}

/**
  Set a given location from full pathname to table file.
*/

void
ha_ndbcluster::set_tabname(const char *path_name, char * tabname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN + 1];

  tmp_name= tmp_buff;
  /* Scan name from the end */
  end= strend(path_name)-1;
  ptr= end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= (uint)(end - ptr);
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len]= '\0';
  filename_to_tablename(tmp_name, tabname, sizeof(tmp_buff) - 1);
}

/**
  Set m_tabname from full pathname to table file.
*/

void ha_ndbcluster::set_tabname(const char *path_name)
{
  set_tabname(path_name, m_tabname);
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
        push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
    HA_REC_NOT_IN_SEQ |
    HA_NULL_IN_KEY |
    HA_AUTO_PART_KEY |
    HA_NO_PREFIX_CHAR_KEYS |
    HA_CAN_GEOMETRY |
    HA_CAN_BIT_FIELD |
    HA_PRIMARY_KEY_REQUIRED_FOR_POSITION |
    HA_PRIMARY_KEY_REQUIRED_FOR_DELETE |
    HA_PARTIAL_COLUMN_READ |
    HA_HAS_OWN_BINLOGGING |
    HA_BINLOG_ROW_CAPABLE |
    HA_HAS_RECORDS |
    HA_READ_BEFORE_WRITE_REMOVAL |
    0;

  /*
    To allow for logging of ndb tables during stmt based logging;
    flag cabablity, but also turn off flag for OWN_BINLOGGING
  */
  if (thd->variables.binlog_format == BINLOG_FORMAT_STMT)
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
  return FALSE;
#else
  return TRUE;
#endif
}
const char* ha_ndbcluster::index_type(uint key_number)
{
  switch (get_index_type(key_number)) {
  case ORDERED_INDEX:
  case UNIQUE_ORDERED_INDEX:
  case PRIMARY_KEY_ORDERED_INDEX:
    return "BTREE";
  case UNIQUE_INDEX:
  case PRIMARY_KEY_INDEX:
  default:
    return "HASH";
  }
}

uint8 ha_ndbcluster::table_cache_type()
{
  DBUG_ENTER("ha_ndbcluster::table_cache_type=HA_CACHE_TBL_ASKTRANSACT");
  DBUG_RETURN(HA_CACHE_TBL_ASKTRANSACT);
}

/**
   Retrieve the commit count for the table object.

   @param thd              Thread context.
   @param norm_name        Normalized path to the table.
   @param[out] commit_count Commit count for the table.

   @return 0 on success.
   @return 1 if an error occured.
*/

uint ndb_get_commitcount(THD *thd, char *norm_name,
                         Uint64 *commit_count)
{
  char dbname[NAME_LEN + 1];
  NDB_SHARE *share;
  DBUG_ENTER("ndb_get_commitcount");

  DBUG_PRINT("enter", ("name: %s", norm_name));
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share=(NDB_SHARE*) my_hash_search(&ndbcluster_open_tables,
                                          (const uchar*) norm_name,
                                          strlen(norm_name))))
  {
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_PRINT("info", ("Table %s not found in ndbcluster_open_tables",
                         norm_name));
    DBUG_RETURN(1);
  }
  /* ndb_share reference temporary, free below */
  share->use_count++;
  DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                           share->key, share->use_count));
  pthread_mutex_unlock(&ndbcluster_mutex);

  pthread_mutex_lock(&share->mutex);
  if (opt_ndb_cache_check_time > 0)
  {
    if (share->commit_count != 0)
    {
      *commit_count= share->commit_count;
#ifndef DBUG_OFF
      char buff[22];
#endif
      DBUG_PRINT("info", ("Getting commit_count: %s from share",
                          llstr(share->commit_count, buff)));
      pthread_mutex_unlock(&share->mutex);
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("info", ("Get commit_count from NDB"));
  Ndb *ndb;
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(1);

  ha_ndbcluster::set_dbname(norm_name, dbname);
  if (ndb->setDatabaseName(dbname))
  {
    ERR_RETURN(ndb->getNdbError());
  }
  uint lock= share->commit_count_lock;
  pthread_mutex_unlock(&share->mutex);

  struct Ndb_statistics stat;
  {
    char tblname[NAME_LEN + 1];
    ha_ndbcluster::set_tabname(norm_name, tblname);
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), tblname);
    if (ndbtab_g.get_table() == 0
        || ndb_get_table_statistics(thd, NULL, 
                                    FALSE, 
                                    ndb, 
                                    ndbtab_g.get_table()->getDefaultRecord(),
                                    &stat))
    {
      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      DBUG_RETURN(1);
    }
  }

  pthread_mutex_lock(&share->mutex);
  if (share->commit_count_lock == lock)
  {
#ifndef DBUG_OFF
    char buff[22];
#endif
    DBUG_PRINT("info", ("Setting commit_count to %s",
                        llstr(stat.commit_count, buff)));
    share->commit_count= stat.commit_count;
    *commit_count= stat.commit_count;
  }
  else
  {
    DBUG_PRINT("info", ("Discarding commit_count, comit_count_lock changed"));
    *commit_count= 0;
  }
  pthread_mutex_unlock(&share->mutex);
  /* ndb_share reference temporary free */
  DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                           share->key, share->use_count));
  free_share(&share);
  DBUG_RETURN(0);
}


/**
  Check if a cached query can be used.

  This is done by comparing the supplied engine_data to commit_count of
  the table.

  The commit_count is either retrieved from the share for the table, where
  it has been cached by the util thread. If the util thread is not started,
  NDB has to be contacetd to retrieve the commit_count, this will introduce
  a small delay while waiting for NDB to answer.


  @param thd            thread handle
  @param full_name      normalized path to the table in the canonical
                        format.
  @param full_name_len  length of the normalized path to the table.
  @param engine_data    parameter retrieved when query was first inserted into
                        the cache. If the value of engine_data is changed,
                        all queries for this table should be invalidated.

  @retval
    TRUE  Yes, use the query from cache
  @retval
    FALSE No, don't use the cached query, and if engine_data
          has changed, all queries for this table should be invalidated

*/

static my_bool
ndbcluster_cache_retrieval_allowed(THD *thd,
                                   char *full_name, uint full_name_len,
                                   ulonglong *engine_data)
{
  Uint64 commit_count;
  char dbname[NAME_LEN + 1];
  char tabname[NAME_LEN + 1];
#ifndef DBUG_OFF
  char buff[22], buff2[22];
#endif

  ha_ndbcluster::set_dbname(full_name, dbname);
  ha_ndbcluster::set_tabname(full_name, tabname);

  DBUG_ENTER("ndbcluster_cache_retrieval_allowed");
  DBUG_PRINT("enter", ("dbname: %s, tabname: %s",
                       dbname, tabname));

  if (thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    /* Don't allow qc to be used if table has been previously
       modified in transaction */
    if (!check_ndb_in_thd(thd))
      DBUG_RETURN(FALSE);
   Thd_ndb *thd_ndb= get_thd_ndb(thd);
    if (!thd_ndb->changed_tables.is_empty())
    {
      NDB_SHARE* share;
      List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
      while ((share= it++))
      {
        if (strcmp(share->table_name, tabname) == 0 &&
            strcmp(share->db, dbname) == 0)
        {
          DBUG_PRINT("exit", ("No, transaction has changed table"));
          DBUG_RETURN(FALSE);
        }
      }
    }
  }

  if (ndb_get_commitcount(thd, full_name, &commit_count))
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, could not retrieve commit_count"));
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("info", ("*engine_data: %s, commit_count: %s",
                      llstr(*engine_data, buff), llstr(commit_count, buff2)));
  if (commit_count == 0)
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, local commit has been performed"));
    DBUG_RETURN(FALSE);
  }
  else if (*engine_data != commit_count)
  {
    *engine_data= commit_count; /* invalidate */
     DBUG_PRINT("exit", ("No, commit_count has changed"));
     DBUG_RETURN(FALSE);
   }

  DBUG_PRINT("exit", ("OK to use cache, engine_data: %s",
                      llstr(*engine_data, buff)));
  DBUG_RETURN(TRUE);
}


/**
  Register a table for use in the query cache.

  Fetch the commit_count for the table and return it in engine_data,
  this will later be used to check if the table has changed, before
  the cached query is reused.

  @param thd            thread handle
  @param full_name      normalized path to the table in the 
                        canonical format.
  @param full_name_len  length of the normalized path to the table.
  @param engine_callback  function to be called before using cache on
                          this table
  @param[out] engine_data    commit_count for this table

  @retval
    TRUE  Yes, it's ok to cahce this query
  @retval
    FALSE No, don't cach the query
*/

my_bool
ha_ndbcluster::register_query_cache_table(THD *thd,
                                          char *full_name, uint full_name_len,
                                          qc_engine_callback *engine_callback,
                                          ulonglong *engine_data)
{
  Uint64 commit_count;
#ifndef DBUG_OFF
  char buff[22];
#endif
  DBUG_ENTER("ha_ndbcluster::register_query_cache_table");
  DBUG_PRINT("enter",("dbname: %s, tabname: %s",
		      m_dbname, m_tabname));

  if (thd_options(thd) & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    /* Don't allow qc to be used if table has been previously
       modified in transaction */
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    if (!thd_ndb->changed_tables.is_empty())
    {
      DBUG_ASSERT(m_share);
      NDB_SHARE* share;
      List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
      while ((share= it++))
      {
        if (m_share == share)
        {
          DBUG_PRINT("exit", ("No, transaction has changed table"));
          DBUG_RETURN(FALSE);
        }
      }
    }
  }

  if (ndb_get_commitcount(thd, full_name, &commit_count))
  {
    *engine_data= 0;
    DBUG_PRINT("exit", ("Error, could not get commitcount"));
    DBUG_RETURN(FALSE);
  }
  *engine_data= commit_count;
  *engine_callback= ndbcluster_cache_retrieval_allowed;
  DBUG_PRINT("exit", ("commit_count: %s", llstr(commit_count, buff)));
  DBUG_RETURN(commit_count > 0);
}


/**
  Handling the shared NDB_SHARE structure that is needed to
  provide table locking.

  It's also used for sharing data with other NDB handlers
  in the same MySQL Server. There is currently not much
  data we want to or can share.
*/

static uchar *ndbcluster_get_key(NDB_SHARE *share, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= share->key_length;
  return (uchar*) share->key;
}


#ifndef DBUG_OFF

static void print_ndbcluster_open_tables()
{
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, ">ndbcluster_open_tables\n");
  for (uint i= 0; i < ndbcluster_open_tables.records; i++)
  {
    NDB_SHARE* share= (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables, i);
    share->print("", DBUG_FILE);
  }
  fprintf(DBUG_FILE, "<ndbcluster_open_tables\n");
  DBUG_UNLOCK_FILE;
}

#endif


#define dbug_print_open_tables()                \
  DBUG_EXECUTE("info",                          \
               print_ndbcluster_open_tables(););

#define dbug_print_share(t, s)                  \
  DBUG_LOCK_FILE;                               \
  DBUG_EXECUTE("info",                          \
               (s)->print((t), DBUG_FILE););    \
  DBUG_UNLOCK_FILE;


/*
  For some reason a share is still around, try to salvage the situation
  by closing all cached tables. If the share still exists, there is an
  error somewhere but only report this to the error log.  Keep this
  "trailing share" but rename it since there are still references to it
  to avoid segmentation faults.  There is a risk that the memory for
  this trailing share leaks.
  
  Must be called with previous pthread_mutex_lock(&ndbcluster_mutex)
*/
int handle_trailing_share(THD *thd, NDB_SHARE *share)
{
  static ulong trailing_share_id= 0;
  DBUG_ENTER("handle_trailing_share");

  /* ndb_share reference temporary, free below */
  ++share->use_count;
  if (opt_ndb_extra_logging > 9)
    sql_print_information ("handle_trailing_share: %s use_count: %u", share->key, share->use_count);
  DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                           share->key, share->use_count));
  pthread_mutex_unlock(&ndbcluster_mutex);

  TABLE_LIST table_list;
  memset(&table_list, 0, sizeof(table_list));
  table_list.db= share->db;
  table_list.alias= table_list.table_name= share->table_name;
  close_cached_tables(thd, &table_list, TRUE, FALSE, FALSE);

  pthread_mutex_lock(&ndbcluster_mutex);
  /* ndb_share reference temporary free */
  DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                           share->key, share->use_count));
  if (!--share->use_count)
  {
    if (opt_ndb_extra_logging > 9)
      sql_print_information ("handle_trailing_share: %s use_count: %u", share->key, share->use_count);
    if (opt_ndb_extra_logging)
      sql_print_information("NDB_SHARE: trailing share %s, "
                            "released by close_cached_tables", share->key);
    ndbcluster_real_free_share(&share);
    DBUG_RETURN(0);
  }
  if (opt_ndb_extra_logging > 9)
    sql_print_information ("handle_trailing_share: %s use_count: %u", share->key, share->use_count);

  /*
    share still exists, if share has not been dropped by server
    release that share
  */
  if (share->state != NSS_DROPPED)
  {
    ndbcluster_mark_share_dropped(share);
    /* ndb_share reference create free */
    DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                             share->key, share->use_count));
    --share->use_count;
    if (opt_ndb_extra_logging > 9)
      sql_print_information ("handle_trailing_share: %s use_count: %u", share->key, share->use_count);

    if (share->use_count == 0)
    {
      if (opt_ndb_extra_logging)
        sql_print_information("NDB_SHARE: trailing share %s, "
                              "released after NSS_DROPPED check",
                              share->key);
      ndbcluster_real_free_share(&share);
      DBUG_RETURN(0);
    }
  }

  DBUG_PRINT("info", ("NDB_SHARE: %s already exists use_count=%d, op=0x%lx.",
                      share->key, share->use_count, (long) share->op));
  /* 
     Ignore table shares only opened by util thread
   */
  if (!((share->use_count == 1) && share->util_thread))
  {
    sql_print_warning("NDB_SHARE: %s already exists use_count=%d."
                      " Moving away for safety, but possible memleak.",
                      share->key, share->use_count);
  }
  dbug_print_open_tables();

  /*
    Ndb share has not been released as it should
  */
#ifdef NOT_YET
  DBUG_ASSERT(FALSE);
#endif

  /*
    This is probably an error.  We can however save the situation
    at the cost of a possible mem leak, by "renaming" the share
    - First remove from hash
  */
  my_hash_delete(&ndbcluster_open_tables, (uchar*) share);

  /*
    now give it a new name, just a running number
    if space is not enough allocate some more
  */
  {
    const uint min_key_length= 10;
    if (share->key_length < min_key_length)
    {
      share->key= (char*) alloc_root(&share->mem_root, min_key_length + 1);
      share->key_length= min_key_length;
    }
    share->key_length=
      (uint)my_snprintf(share->key, min_key_length + 1, "#leak%lu",
                  trailing_share_id++);
  }
  /* Keep it for possible the future trailing free */
  my_hash_insert(&ndbcluster_open_tables, (uchar*) share);

  DBUG_RETURN(0);
}

/*
  Rename share is used during rename table.
*/
int ndbcluster_prepare_rename_share(NDB_SHARE *share, const char *new_key)
{
  /*
    allocate and set the new key, db etc
    enough space for key, db, and table_name
  */
  uint new_length= (uint) strlen(new_key);
  share->new_key= (char*) alloc_root(&share->mem_root, 2 * (new_length + 1));
  strmov(share->new_key, new_key);
  return 0;
}

int ndbcluster_undo_rename_share(THD *thd, NDB_SHARE *share)
{
  share->new_key= share->old_names;
  ndbcluster_rename_share(thd, share);
  return 0;
}


int ndbcluster_rename_share(THD *thd, NDB_SHARE *share)
{
  NDB_SHARE *tmp;
  pthread_mutex_lock(&ndbcluster_mutex);
  uint new_length= (uint) strlen(share->new_key);
  DBUG_PRINT("ndbcluster_rename_share", ("old_key: %s  old__length: %d",
                              share->key, share->key_length));
  if ((tmp= (NDB_SHARE*) my_hash_search(&ndbcluster_open_tables,
                                        (const uchar*) share->new_key,
                                        new_length)))
    handle_trailing_share(thd, tmp);

  /* remove the share from hash */
  my_hash_delete(&ndbcluster_open_tables, (uchar*) share);
  dbug_print_open_tables();

  /* save old stuff if insert should fail */
  uint old_length= share->key_length;
  char *old_key= share->key;

  share->key= share->new_key;
  share->key_length= new_length;

  if (my_hash_insert(&ndbcluster_open_tables, (uchar*) share))
  {
    // ToDo free the allocated stuff above?
    DBUG_PRINT("error", ("ndbcluster_rename_share: my_hash_insert %s failed",
                         share->key));
    share->key= old_key;
    share->key_length= old_length;
    if (my_hash_insert(&ndbcluster_open_tables, (uchar*) share))
    {
      sql_print_error("ndbcluster_rename_share: failed to recover %s", share->key);
      DBUG_PRINT("error", ("ndbcluster_rename_share: my_hash_insert %s failed",
                           share->key));
    }
    dbug_print_open_tables();
    pthread_mutex_unlock(&ndbcluster_mutex);
    return -1;
  }
  dbug_print_open_tables();

  share->db= share->key + new_length + 1;
  ha_ndbcluster::set_dbname(share->new_key, share->db);
  share->table_name= share->db + strlen(share->db) + 1;
  ha_ndbcluster::set_tabname(share->new_key, share->table_name);

  dbug_print_share("ndbcluster_rename_share:", share);
  Ndb_event_data *event_data= share->get_event_data_ptr();
  if (event_data && event_data->shadow_table)
  {
    if (!IS_TMP_PREFIX(share->table_name))
    {
      event_data->shadow_table->s->db.str= share->db;
      event_data->shadow_table->s->db.length= strlen(share->db);
      event_data->shadow_table->s->table_name.str= share->table_name;
      event_data->shadow_table->s->table_name.length= strlen(share->table_name);
    }
    else
    {
      /**
       * we don't rename the table->s here 
       *   that is used by injector
       *   as we don't know if all events has been processed
       * This will be dropped anyway
       */
    }
  }
  /* else rename will be handled when the ALTER event comes */
  share->old_names= old_key;
  // ToDo free old_names after ALTER EVENT

  if (opt_ndb_extra_logging > 9)
    sql_print_information ("ndbcluster_rename_share: %s-%s use_count: %u", old_key, share->key, share->use_count);

  pthread_mutex_unlock(&ndbcluster_mutex);
  return 0;
}

/*
  Increase refcount on existing share.
  Always returns share and cannot fail.
*/
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndbcluster_mutex);
  share->use_count++;

  dbug_print_open_tables();
  dbug_print_share("ndbcluster_get_share:", share);
  if (opt_ndb_extra_logging > 9)
    sql_print_information ("ndbcluster_get_share: %s use_count: %u", share->key, share->use_count);
  pthread_mutex_unlock(&ndbcluster_mutex);
  return share;
}



NDB_SHARE*
NDB_SHARE::create(const char* key, size_t key_length,
                  TABLE* table, const char* db_name, const char* table_name)
{
  NDB_SHARE* share;
  if (!(share= (NDB_SHARE*) my_malloc(sizeof(*share),
                                      MYF(MY_WME | MY_ZEROFILL))))
    return NULL;

  MEM_ROOT **root_ptr=
    my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
  MEM_ROOT *old_root= *root_ptr;

  init_sql_alloc(&share->mem_root, 1024, 0);
  *root_ptr= &share->mem_root; // remember to reset before return
  share->flags= 0;
  share->state= NSS_INITIAL;
  /* Allocate enough space for key, db, and table_name */
  share->key= (char*) alloc_root(*root_ptr, 2 * (key_length + 1));
  share->key_length= (uint)key_length;
  strmov(share->key, key);
  share->db= share->key + key_length + 1;
  strmov(share->db, db_name);
  share->table_name= share->db + strlen(share->db) + 1;
  strmov(share->table_name, table_name);
  thr_lock_init(&share->lock);
  pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  share->commit_count= 0;
  share->commit_count_lock= 0;

#ifdef HAVE_NDB_BINLOG
  share->m_cfn_share= NULL;
#endif

  share->op= 0;
  share->new_op= 0;
  share->event_data= 0;

  {
    // Create array of bitmap for keeping track of subscribed nodes
    // NOTE! Only the NDB_SHARE for ndb_schema really needs this
    int no_nodes= g_ndb_cluster_connection->no_db_nodes();
    share->subscriber_bitmap= (MY_BITMAP*)
      alloc_root(&share->mem_root, no_nodes * sizeof(MY_BITMAP));
    for (int i= 0; i < no_nodes; i++)
    {
      bitmap_init(&share->subscriber_bitmap[i],
                  (Uint32*)alloc_root(&share->mem_root, max_ndb_nodes/8),
                  max_ndb_nodes, FALSE);
      bitmap_clear_all(&share->subscriber_bitmap[i]);
    }
  }

  if (ndbcluster_binlog_init_share(current_thd, share, table))
  {
    DBUG_PRINT("error", ("get_share: %s could not init share", key));
    DBUG_ASSERT(share->event_data == NULL);
    NDB_SHARE::destroy(share);
    *root_ptr= old_root;
    return NULL;
  }

  *root_ptr= old_root;

  return share;
}


static inline
NDB_SHARE *ndbcluster_get_share(const char *key, TABLE *table,
                                bool create_if_not_exists)
{
  NDB_SHARE *share;
  uint length= (uint) strlen(key);
  DBUG_ENTER("ndbcluster_get_share");
  DBUG_PRINT("enter", ("key: '%s'", key));

//safe_mutex_assert_owner(&ndbcluster_mutex);  ... Need to change ndbcluster_mutex to 'mysql_mutex_t'

  if (!(share= (NDB_SHARE*) my_hash_search(&ndbcluster_open_tables,
                                           (const uchar*) key,
                                           length)))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("error", ("get_share: %s does not exist", key));
      DBUG_RETURN(0);
    }

    /*
      Extract db and table name from key (to avoid that NDB_SHARE
      dependens on ha_ndbcluster)
    */
    char db_name_buf[FN_HEADLEN];
    char table_name_buf[FN_HEADLEN];
    ha_ndbcluster::set_dbname(key, db_name_buf);
    ha_ndbcluster::set_tabname(key, table_name_buf);

    if (!(share= NDB_SHARE::create(key, length, table,
                                   db_name_buf, table_name_buf)))
    {
      DBUG_PRINT("error", ("get_share: failed to alloc share"));
      my_error(ER_OUTOFMEMORY, MYF(0), static_cast<int>(sizeof(*share)));
      DBUG_RETURN(0);
    }

    // Insert the new share in list of open shares
    if (my_hash_insert(&ndbcluster_open_tables, (uchar*) share))
    {
      NDB_SHARE::destroy(share);
      DBUG_RETURN(0);
    }
  }
  share->use_count++;
  if (opt_ndb_extra_logging > 9)
    sql_print_information ("ndbcluster_get_share: %s use_count: %u", share->key, share->use_count);

  dbug_print_open_tables();
  dbug_print_share("ndbcluster_get_share:", share);
  DBUG_RETURN(share);
}


/**
  Get NDB_SHARE for key

  Returns share for key, and increases the refcount on the share.

  @param create_if_not_exists, creates share if it does not already exist
  @param have_lock, ndbcluster_mutex already locked
*/

NDB_SHARE *ndbcluster_get_share(const char *key, TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock)
{
  NDB_SHARE *share;
  DBUG_ENTER("ndbcluster_get_share");
  DBUG_PRINT("enter", ("key: '%s', create_if_not_exists: %d, have_lock: %d",
                       key, create_if_not_exists, have_lock));

  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);

  share= ndbcluster_get_share(key, table, create_if_not_exists);

  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);

  DBUG_RETURN(share);
}

void ndbcluster_real_free_share(NDB_SHARE **share)
{
  DBUG_ENTER("ndbcluster_real_free_share");
  dbug_print_share("ndbcluster_real_free_share:", *share);

  if (opt_ndb_extra_logging > 9)
    sql_print_information ("ndbcluster_real_free_share: %s use_count: %u", (*share)->key, (*share)->use_count);

  ndb_index_stat_free(*share);

  bool found= false;
  if ((* share)->state == NSS_DROPPED)
  {
    found= my_hash_delete(&ndbcluster_dropped_tables, (uchar*) *share) == 0;

    // If this is a 'trailing share', it might still be 'open'
    my_hash_delete(&ndbcluster_open_tables, (uchar*) *share);
  }
  else
  {
    found= my_hash_delete(&ndbcluster_open_tables, (uchar*) *share) == 0;
  }
  assert(found);

  NDB_SHARE::destroy(*share);
  *share= 0;

  dbug_print_open_tables();
  DBUG_VOID_RETURN;
}


void ndbcluster_free_share(NDB_SHARE **share, bool have_lock)
{
  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);
  if (!--(*share)->use_count)
  {
    if (opt_ndb_extra_logging > 9)
      sql_print_information ("ndbcluster_free_share: %s use_count: %u", (*share)->key, (*share)->use_count);
    ndbcluster_real_free_share(share);
  }
  else
  {
    if (opt_ndb_extra_logging > 9)
      sql_print_information ("ndbcluster_free_share: %s use_count: %u", (*share)->key, (*share)->use_count);
    dbug_print_open_tables();
    dbug_print_share("ndbcluster_free_share:", *share);
  }
  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);
}

void
ndbcluster_mark_share_dropped(NDB_SHARE* share)
{
  share->state= NSS_DROPPED;
  if (my_hash_delete(&ndbcluster_open_tables, (uchar*) share) == 0)
  {
    my_hash_insert(&ndbcluster_dropped_tables, (uchar*) share);
  }
  else
  {
    assert(false);
  }
  if (opt_ndb_extra_logging > 9)
  {
    sql_print_information ("ndbcluster_mark_share_dropped: %s use_count: %u",
                           share->key, share->use_count);
  }
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
                                bool have_lock,
                                uint part_id)
{
  struct Ndb_statistics stat;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ENTER("ha_ndbcluster::update_stats");
  do
  {
    if (m_share && !do_read_stat)
    {
      pthread_mutex_lock(&m_share->mutex);
      stat= m_share->stat;
      pthread_mutex_unlock(&m_share->mutex);

      DBUG_ASSERT(stat.row_count != ~(ha_rows)0); // should never be invalid

      /* Accept shared cached statistics if row_count is valid. */
      if (stat.row_count != ~(ha_rows)0)
        break;
    }

    /* Request statistics from datanodes */
    Ndb *ndb= thd_ndb->ndb;
    if (ndb->setDatabaseName(m_dbname))
    {
      DBUG_RETURN(my_errno= HA_ERR_OUT_OF_MEM);
    }
    if (int err= ndb_get_table_statistics(thd, this, TRUE, ndb,
                                          m_ndb_record, &stat,
                                          have_lock, part_id))
    {
      DBUG_RETURN(err);
    }

    /* Update shared statistics with fresh data */
    if (m_share)
    {
      pthread_mutex_lock(&m_share->mutex);
      m_share->stat= stat;
      pthread_mutex_unlock(&m_share->mutex);
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
    pthread_mutex_lock(&share->mutex);
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
    pthread_mutex_unlock(&share->mutex);
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
ndb_get_table_statistics(THD *thd, ha_ndbcluster* file, bool report_error, Ndb* ndb,
                         const NdbRecord *record,
                         struct Ndb_statistics * ndbstat,
                         bool have_lock,
                         uint part_id)
{
  Thd_ndb *thd_ndb= get_thd_ndb(current_thd);
  NdbTransaction* pTrans;
  NdbError error;
  int retries= 100;
  int reterr= 0;
  int retry_sleep= 30; /* 30 milliseconds */
  const char *dummyRowPtr;
  NdbOperation::GetValueSpec extraGets[8];
  Uint64 rows, commits, fixed_mem, var_mem, ext_space, free_ext_space;
  Uint32 size, fragid;
#ifndef DBUG_OFF
  char buff[22], buff2[22], buff3[22], buff4[22], buff5[22], buff6[22];
#endif
  DBUG_ENTER("ndb_get_table_statistics");

  DBUG_ASSERT(record != 0);
  
  /* We use the passed in NdbRecord just to get access to the
     table, we mask out any/all columns it may have and add
     our reads as extraGets.  This is necessary as they are
     all pseudo-columns
  */
  extraGets[0].column= NdbDictionary::Column::ROW_COUNT;
  extraGets[0].appStorage= &rows;
  extraGets[1].column= NdbDictionary::Column::COMMIT_COUNT;
  extraGets[1].appStorage= &commits;
  extraGets[2].column= NdbDictionary::Column::ROW_SIZE;
  extraGets[2].appStorage= &size;
  extraGets[3].column= NdbDictionary::Column::FRAGMENT_FIXED_MEMORY;
  extraGets[3].appStorage= &fixed_mem;
  extraGets[4].column= NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY;
  extraGets[4].appStorage= &var_mem;
  extraGets[5].column= NdbDictionary::Column::FRAGMENT_EXTENT_SPACE;
  extraGets[5].appStorage= &ext_space;
  extraGets[6].column= NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE;
  extraGets[6].appStorage= &free_ext_space;
  extraGets[7].column= NdbDictionary::Column::FRAGMENT;
  extraGets[7].appStorage= &fragid;

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
    Uint64 sum_commits= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    Uint64 sum_ext_space= 0;
    Uint64 sum_free_ext_space= 0;
    NdbScanOperation*pOp;
    int check;

    if ((pTrans= ndb->startTransaction()) == NULL)
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
                        TRUE) == -1)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    while ((check= pOp->nextResult(&dummyRowPtr, TRUE, TRUE)) == 0)
    {
      DBUG_PRINT("info", ("nextResult rows: %d  commits: %d"
                          "fixed_mem_size %d var_mem_size %d "
                          "fragmentid %d extent_space %d free_extent_space %d",
                          (int)rows, (int)commits, (int)fixed_mem,
                          (int)var_mem, (int)fragid, (int)ext_space,
                          (int)free_ext_space));

      if ((part_id != ~(uint)0) && fragid != part_id)
      {
        continue;
      }

      sum_rows+= rows;
      sum_commits+= commits;
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

    pOp->close(TRUE);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->commit_count= sum_commits;
    ndbstat->row_size= (ulong)sum_row_size;
    ndbstat->fragment_memory= sum_mem;
    ndbstat->fragment_extent_space= sum_ext_space;
    ndbstat->fragment_extent_free_space= sum_free_ext_space;

    DBUG_PRINT("exit", ("records: %s  commits: %s "
                        "row_size: %s  mem: %s "
                        "allocated: %s  free: %s "
                        "count: %u",
			llstr(sum_rows, buff),
                        llstr(sum_commits, buff2),
                        llstr(sum_row_size, buff3),
                        llstr(sum_mem, buff4),
                        llstr(sum_ext_space, buff5),
                        llstr(sum_free_ext_space, buff6),
                        count));

    DBUG_RETURN(0);
retry:
    if(report_error)
    {
      if (file && pTrans)
      {
        reterr= file->ndb_err(pTrans, have_lock);
      }
      else
      {
        const NdbError& tmp= error;
        ERR_PRINT(tmp);
        reterr= ndb_to_mysql_error(&tmp);
      }
    }
    else
      reterr= error.code;

    if (pTrans)
    {
      ndb->closeTransaction(pTrans);
      pTrans= NULL;
    }
    if (error.status == NdbError::TemporaryError &&
        retries-- && !thd->killed)
    {
      do_retry_sleep(retry_sleep);
      continue;
    }
    break;
  } while(1);
  DBUG_PRINT("exit", ("failed, reterr: %u, NdbError %u(%s)", reterr,
                      error.code, error.message));
  DBUG_RETURN(reterr);
}

/**
  Create a .ndb file to serve as a placeholder indicating 
  that the table with this name is a ndb table.
*/

int ha_ndbcluster::write_ndb_file(const char *name) const
{
  File file;
  bool error=1;
  char path[FN_REFLEN];
  
  DBUG_ENTER("write_ndb_file");
  DBUG_PRINT("enter", ("name: %s", name));

#ifndef EMBEDDED_LIBRARY
  (void)strxnmov(path, FN_REFLEN-1, 
                 mysql_data_home,"/",name,ha_ndb_ext,NullS);
#else
  (void)strxnmov(path, FN_REFLEN-1, name,ha_ndb_ext, NullS);
#endif

  if ((file=my_create(path, CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    // It's an empty file
    error=0;
    my_close(file,MYF(0));
  }
  DBUG_RETURN(error);
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
multi_range_entry_size(my_bool use_keyop, ulong reclength)
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
  my_bool use_keyop= multi_range_entry_type(p) < enum_ordered_range;
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
  return ((char **)(buffer->buffer))[range_no];
}

static void
multi_range_put_custom(HANDLER_BUFFER *buffer, int range_no, char *custom)
{
  DBUG_ASSERT(range_no < MRR_MAX_RANGES);
  ((char **)(buffer->buffer))[range_no]= custom;
}

/*
  This is used to check if an ordered index scan is needed for a range in
  a multi range read.
  If a scan is not needed, we use a faster primary/unique key operation
  instead.
*/
static my_bool
read_multi_needs_scan(NDB_INDEX_TYPE cur_index_type, const KEY *key_info,
                      const KEY_MULTI_RANGE *r, bool is_pushed)
{
  if (cur_index_type == ORDERED_INDEX || is_pushed)
    return TRUE;
  if (cur_index_type == PRIMARY_KEY_INDEX ||
      cur_index_type == UNIQUE_INDEX)
    return FALSE;
  DBUG_ASSERT(cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
              cur_index_type == UNIQUE_ORDERED_INDEX);
  if (r->start_key.length != key_info->key_length ||
      r->start_key.flag != HA_READ_KEY_EXACT)
    return TRUE;                                // Not exact match, need scan
  if (cur_index_type == UNIQUE_ORDERED_INDEX &&
      check_null_in_key(key_info, r->start_key.key,r->start_key.length))
    return TRUE;                                // Can't use for NULL values
  return FALSE;
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

  @retval TRUE   Default MRR implementation should be used
  @retval FALSE  NDB-MRR implementation should be used
*/

bool ha_ndbcluster::choose_mrr_impl(uint keyno, uint n_ranges, ha_rows n_rows,
                                    uint *bufsz, uint *flags, Cost_estimate *cost)
{
  THD *thd= current_thd;
  NDB_INDEX_TYPE key_type= get_index_type(keyno);

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
      *bufsz= min(save_bufsize,
                  (uint)(n_rows * entry_size + multi_range_fixed_size(max_ranges)));
    }
    DBUG_PRINT("info", ("MRR bufsize set to %u", *bufsz));
  }

  /**
   * Cost based MRR optimization is known to be incorrect.
   * Disabled -> always use NDB-MRR whenever possible
   */
if (false)
{
  /**
   * FIXME: Cost calculation is a copy of current default-MRR 
   *        cost calculation. (Which also is incorrect!)
   * TODO:  We have to invent our own metrics for NDB-MRR.
   */
  Cost_estimate mrr_cost;
  if ((*flags & HA_MRR_INDEX_ONLY) && n_rows > 2)
    cost->add_io(index_only_read_time(keyno, n_rows) *
                 Cost_estimate::IO_BLOCK_READ_COST());
  else
    cost->add_io(read_time(keyno, n_ranges, n_rows) *
                 Cost_estimate::IO_BLOCK_READ_COST());
  cost->add_cpu(n_rows * ROW_EVALUATE_COST + 0.01);

  bool force_mrr;
  /* 
    If @@optimizer_switch has "mrr" on and "mrr_cost_based" off, then set cost
    of DS-MRR to be minimum of DS-MRR and Default implementations cost. This
    allows one to force use of DS-MRR whenever it is applicable without
    affecting other cost-based choices.
  */
  if ((force_mrr=
       (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR) &&
        !thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR_COST_BASED))) &&
      mrr_cost.total_cost() > cost->total_cost())
  {
    mrr_cost= *cost;
  }
  if (!force_mrr && mrr_cost.total_cost() > cost->total_cost())
  {
    /* Use the default MRR implementation */
    return true;
  }
  *cost= mrr_cost;
} // if (false)

  /* Use the NDB-MRR implementation */
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
  ulong bufsize= buffer->buffer_end - buffer->buffer;

  if (mode & HA_MRR_USE_DEFAULT_IMPL
      || bufsize < multi_range_fixed_size(1) +
                   multi_range_max_entry(get_index_type(active_index),
                                         table_share->reclength)
      || (m_pushed_join_operation==PUSHED_ROOT &&
         !m_disable_pushed_join &&
         !m_pushed_join_member->get_query_def().isScanQuery())
      || m_delete_cannot_batch || m_update_cannot_batch)
  {
    m_disable_multi_read= TRUE;
    DBUG_RETURN(handler::multi_range_read_init(seq_funcs, seq_init_param,
                                               n_ranges, mode, buffer));
  }

  /**
   * There may still be an open m_multi_cursor from the previous mrr access on this handler.
   * Close it now to free up resources for this NdbScanOperation.
   */ 
  if (unlikely((error= close_scan())))
    DBUG_RETURN(error);

  m_disable_multi_read= FALSE;

  mrr_is_output_sorted= test(mode & HA_MRR_SORTED);
  /*
    Copy arguments into member variables
  */
  multi_range_buffer= buffer;
  mrr_funcs= *seq_funcs;
  mrr_iter= mrr_funcs.init(seq_init_param, n_ranges, mode);
  ranges_in_seq= n_ranges;
  m_range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range);
  mrr_need_range_assoc = !test(mode & HA_MRR_NO_ASSOCIATION);
  if (mrr_need_range_assoc)
  {
    ha_statistic_increment(&SSV::ha_multi_range_read_init_count);
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
  ulong bufsize= end_of_buffer - multi_range_buffer->buffer;
  int max_range= multi_range_max_ranges(ranges_in_seq,
                                        bufsize - min_entry_size);
  DBUG_ASSERT(max_range > 0);
  uchar *row_buf= multi_range_buffer->buffer + multi_range_fixed_size(max_range);
  m_multi_range_result_ptr= row_buf;

  int range_no= 0;
  int mrr_range_no= starting_range;
  bool any_real_read= FALSE;

  if (m_read_before_write_removal_possible)
    check_read_before_write_removal();

  for (;
       !m_range_res;
       range_no++, m_range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range))
  {
    if (range_no >= max_range)
      break;
    my_bool need_scan=
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

      any_real_read= TRUE;
      DBUG_PRINT("info", ("any_real_read= TRUE"));

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
           (uchar *)(table->read_set->bitmap),
           NULL, /* All bounds specified below */
           &options,
           sizeof(NdbScanOperation::ScanOptions));

        if (!scanOp)
          ERR_RETURN(trans->getNdbError());

        m_multi_cursor= scanOp;

        /*
          We do not get_blob_values() here, as when using blobs we always
          fallback to non-batched multi range read (see multi_range_read_info
          function).
        */

        /* We set m_next_row=0 to say that no row was fetched from the scan yet. */
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
        DBUG_PRINT("info", ("m_read_before_write_removal_used == TRUE"));

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
        any_real_read= TRUE;
        DBUG_PRINT("info", ("any_real_read= TRUE"));

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
          const int error= pk_unique_index_read_key_pushed(active_index,
                                                           mrr_cur_range.start_key.key,
                                                           ppartitionId);
          if (unlikely(error))
            DBUG_RETURN(error);
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
        DBUG_ASSERT(FALSE);
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
                 table_share->reclength);
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
              unpack_record(table->record[0], m_next_row);
              table->status= 0;
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
            else if (current_range_no > expected_range_no)
            {
              /* Nothing more in scan for this range. Move to next. */
              break;
            }
            else
            {
              /*
                Should not happen. Ranges should be returned from NDB API in
                the order we requested them.
              */
              DBUG_ASSERT(0);
              break;                              // Attempt to carry on
            }
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
        m_active_query->close(FALSE);
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
      NdbIndexScanOperation *cursor= (NdbIndexScanOperation *)m_multi_cursor;
      int res= fetch_next(cursor);
      if (res == 0)
      {
        m_current_range_no= cursor->get_range_no();
      }
      else if (res == 1)
      {
        /* We have fetched the last row from the scan. */
        cursor->close(FALSE, TRUE);
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

static
int ndbcluster_make_pushed_join(handlerton *hton,
                                THD* thd,
                                const AQP::Join_plan* plan)
{
  DBUG_ENTER("ndbcluster_make_pushed_join");
  (void)ha_ndb_ext; // prevents compiler warning.

  if (THDVAR(thd, join_pushdown) &&
      // Check for online upgrade/downgrade.
      ndb_join_pushdown(g_ndb_cluster_connection->get_min_db_version()))
  {
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
    const NdbRecord* const resultRec= handler->m_ndb_record;
    int res= op->setResultRowRef(
                        resultRec,
                        handler->_m_next_row,
                        (uchar *)(tab->read_set->bitmap));
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

/**
  @param[in] comment  table comment defined by user

  @return
    table comment + additional
*/
char*
ha_ndbcluster::update_table_comment(
                                /* out: table comment + additional */
        const char*     comment)/* in:  table comment defined by user */
{
  THD *thd= current_thd;
  uint length= (uint)strlen(comment);
  if (length > 64000 - 3)
  {
    return((char*)comment); /* string too long */
  }

  Ndb* ndb;
  if (!(ndb= get_ndb(thd)))
  {
    return((char*)comment);
  }

  if (ndb->setDatabaseName(m_dbname))
  {
    return((char*)comment);
  }
  const NDBTAB* tab= m_table;
  DBUG_ASSERT(tab != NULL);

  char *str;
  const char *fmt="%s%snumber_of_replicas: %d";
  const unsigned fmt_len_plus_extra= length + (uint)strlen(fmt);
  if ((str= (char*) my_malloc(fmt_len_plus_extra, MYF(0))) == NULL)
  {
    sql_print_error("ha_ndbcluster::update_table_comment: "
                    "my_malloc(%u) failed", (unsigned int)fmt_len_plus_extra);
    return (char*)comment;
  }

  my_snprintf(str,fmt_len_plus_extra,fmt,comment,
              length > 0 ? " ":"",
              tab->getReplicaCount());
  return str;
}


/**
  Utility thread main loop.
*/
Ndb_util_thread::Ndb_util_thread()
  : running(-1)
{
  pthread_mutex_init(&LOCK, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND, NULL);
  pthread_cond_init(&COND_ready, NULL);
}

Ndb_util_thread::~Ndb_util_thread()
{
  assert(running <= 0);
  pthread_mutex_destroy(&LOCK);
  pthread_cond_destroy(&COND);
  pthread_cond_destroy(&COND_ready);
}

void
Ndb_util_thread::do_run()
{
  THD *thd; /* needs to be first for thread_stack */
  struct timespec abstime;
  Thd_ndb *thd_ndb= NULL;
  uint share_list_size= 0;
  NDB_SHARE **share_list= NULL;

  DBUG_ENTER("ndb_util_thread");
  DBUG_PRINT("enter", ("cache_check_time: %lu", opt_ndb_cache_check_time));

  pthread_mutex_lock(&ndb_util_thread.LOCK);

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  if (thd == NULL)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_VOID_RETURN;
  }
  THD_CHECK_SENTRY(thd);

  thd->thread_stack= (char*)&thd; /* remember where our stack is */
  if (thd->store_globals())
    goto ndb_util_thread_fail;
  lex_start(thd);
  thd_set_command(thd, COM_DAEMON);
#ifndef NDB_THD_HAS_NO_VERSION
  thd->version=refresh_version;
#endif
  thd->client_capabilities = 0;
  thd->security_ctx->skip_grants();
  my_net_init(&thd->net, 0);

  CHARSET_INFO *charset_connection;
  charset_connection= get_charset_by_csname("utf8",
                                            MY_CS_PRIMARY, MYF(MY_WME));
  thd->variables.character_set_client= charset_connection;
  thd->variables.character_set_results= charset_connection;
  thd->variables.collation_connection= charset_connection;
  thd->update_charset();

  /* Signal successful initialization */
  ndb_util_thread.running= 1;
  pthread_cond_signal(&ndb_util_thread.COND_ready);
  pthread_mutex_unlock(&ndb_util_thread.LOCK);

  /*
    wait for mysql server to start
  */
  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
  {
    set_timespec(abstime, 1);
    mysql_cond_timedwait(&COND_server_started, &LOCK_server_started,
                         &abstime);
    if (ndbcluster_terminating)
    {
      mysql_mutex_unlock(&LOCK_server_started);
      pthread_mutex_lock(&ndb_util_thread.LOCK);
      goto ndb_util_thread_end;
    }
  }
  mysql_mutex_unlock(&LOCK_server_started);

  // Defer call of THD::init_for_query until after mysqld_server_started
  // to ensure that the parts of MySQL Server it uses has been created
  thd->init_for_queries();

  /*
    Wait for cluster to start
  */
  pthread_mutex_lock(&ndb_util_thread.LOCK);
  while (!g_ndb_status.cluster_node_id && (ndbcluster_hton->slot != ~(uint)0))
  {
    /* ndb not connected yet */
    pthread_cond_wait(&ndb_util_thread.COND, &ndb_util_thread.LOCK);
    if (ndbcluster_terminating)
      goto ndb_util_thread_end;
  }
  pthread_mutex_unlock(&ndb_util_thread.LOCK);

  /* Get thd_ndb for this thread */
  if (!(thd_ndb= Thd_ndb::seize(thd)))
  {
    sql_print_error("Could not allocate Thd_ndb object");
    pthread_mutex_lock(&ndb_util_thread.LOCK);
    goto ndb_util_thread_end;
  }
  thd_set_thd_ndb(thd, thd_ndb);
  thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;

  if (opt_ndb_extra_logging && ndb_binlog_running)
    sql_print_information("NDB Binlog: Ndb tables initially read only.");

  set_timespec(abstime, 0);
  for (;;)
  {
    pthread_mutex_lock(&ndb_util_thread.LOCK);
    if (!ndbcluster_terminating)
      pthread_cond_timedwait(&ndb_util_thread.COND,
                             &ndb_util_thread.LOCK,
                             &abstime);
    if (ndbcluster_terminating) /* Shutting down server */
      goto ndb_util_thread_end;
    pthread_mutex_unlock(&ndb_util_thread.LOCK);
#ifdef NDB_EXTRA_DEBUG_UTIL_THREAD
    DBUG_PRINT("ndb_util_thread", ("Started, cache_check_time: %lu",
                                   opt_ndb_cache_check_time));
#endif

    /*
      Check if the Ndb object in thd_ndb is still valid(it will be
      invalid if connection to cluster has been lost) and recycle
      it if necessary.
    */
    if (!check_ndb_in_thd(thd, false))
    {
      set_timespec(abstime, 1);
      continue;
    }

    /*
      Regularly give the ndb_binlog component chance to set it self up
      i.e at first start it needs to create the ndb_* system tables
      and setup event operations on those. In case of lost connection
      to cluster, the ndb_* system tables are hopefully still there
      but the event operations need to be recreated.
    */
    if (!ndb_binlog_setup(thd))
    {
      /* Failed to setup binlog, try again in 1 second */
      set_timespec(abstime, 1);
      continue;
    }

    if (opt_ndb_cache_check_time == 0)
    {
      /* Wake up in 1 second to check if value has changed */
      set_timespec(abstime, 1);
      continue;
    }

    /* Lock mutex and fill list with pointers to all open tables */
    NDB_SHARE *share;
    pthread_mutex_lock(&ndbcluster_mutex);
    uint i, open_count, record_count= ndbcluster_open_tables.records;
    if (share_list_size < record_count)
    {
      NDB_SHARE ** new_share_list= new NDB_SHARE * [record_count];
      if (!new_share_list)
      {
        sql_print_warning("ndb util thread: malloc failure, "
                          "query cache not maintained properly");
        pthread_mutex_unlock(&ndbcluster_mutex);
        goto next;                               // At least do not crash
      }
      delete [] share_list;
      share_list_size= record_count;
      share_list= new_share_list;
    }
    for (i= 0, open_count= 0; i < record_count; i++)
    {
      share= (NDB_SHARE *)my_hash_element(&ndbcluster_open_tables, i);
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 0)
        continue; // injector thread is the only user, skip statistics
      /* ndb_share reference temporary, free below */
      share->use_count++; /* Make sure the table can't be closed */
      share->util_thread= true;
      DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                               share->key, share->use_count));
      DBUG_PRINT("ndb_util_thread",
                 ("Found open table[%d]: %s, use_count: %d",
                  i, share->table_name, share->use_count));

      /* Store pointer to table */
      share_list[open_count++]= share;
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    /* Iterate through the open files list */
    for (i= 0; i < open_count; i++)
    {
      share= share_list[i];
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 1)
      {
        /*
          Util thread and injector thread is the only user, skip statistics
	*/
        /* ndb_share reference temporary free */
        DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                 share->key, share->use_count));
        
        pthread_mutex_lock(&ndbcluster_mutex);
        share->util_thread= false;
        free_share(&share, true);
        pthread_mutex_unlock(&ndbcluster_mutex);
        continue;
      }
      DBUG_PRINT("ndb_util_thread",
                 ("Fetching commit count for: %s", share->key));

      struct Ndb_statistics stat;
      uint lock;
      pthread_mutex_lock(&share->mutex);
      lock= share->commit_count_lock;
      pthread_mutex_unlock(&share->mutex);
      {
        /* Contact NDB to get commit count for table */
        Ndb* ndb= thd_ndb->ndb;
        if (ndb->setDatabaseName(share->db))
        {
          goto loop_next;
        }
        Ndb_table_guard ndbtab_g(ndb->getDictionary(), share->table_name);
        if (ndbtab_g.get_table() &&
            ndb_get_table_statistics(thd, NULL, FALSE, ndb,
                                     ndbtab_g.get_table()->getDefaultRecord(), 
                                     &stat) == 0)
        {
#ifndef DBUG_OFF
          char buff[22], buff2[22];
#endif
          DBUG_PRINT("info",
                     ("Table: %s  commit_count: %s  rows: %s",
                      share->key,
                      llstr(stat.commit_count, buff),
                      llstr(stat.row_count, buff2)));
        }
        else
        {
          DBUG_PRINT("ndb_util_thread",
                     ("Error: Could not get commit count for table %s",
                      share->key));
          stat.commit_count= 0;
        }
      }
  loop_next:
      pthread_mutex_lock(&share->mutex);
      if (share->commit_count_lock == lock)
        share->commit_count= stat.commit_count;
      pthread_mutex_unlock(&share->mutex);

      /* ndb_share reference temporary free */
      DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                               share->key, share->use_count));
      pthread_mutex_lock(&ndbcluster_mutex);
      share->util_thread= false;
      free_share(&share, true);
      pthread_mutex_unlock(&ndbcluster_mutex);
    }
next:
    /* Calculate new time to wake up */
    set_timespec_nsec(abstime, opt_ndb_cache_check_time * 1000000ULL);
  }

  pthread_mutex_lock(&ndb_util_thread.LOCK);

ndb_util_thread_end:
  net_end(&thd->net);
ndb_util_thread_fail:
  if (share_list)
    delete [] share_list;
  if (thd_ndb)
  {
    Thd_ndb::release(thd_ndb);
    thd_set_thd_ndb(thd, NULL);
  }
  delete thd;
  
  /* signal termination */
  ndb_util_thread.running= 0;
  pthread_cond_signal(&ndb_util_thread.COND_ready);
  pthread_mutex_unlock(&ndb_util_thread.LOCK);
  DBUG_PRINT("exit", ("ndb_util_thread"));

  DBUG_LEAVE;                               // Must match DBUG_ENTER()
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

#if 1
  if (cond->used_tables() & ~table->map)
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
#else
  /*
    Make sure that 'cond' does not refer field(s) from other tables
    or other instances of this table.
    (This was a legacy bug in optimizer)
  */
  DBUG_ASSERT(!(cond->used_tables() & ~table->map));
#endif
  if (!m_cond) 
    m_cond= new ha_ndbcluster_cond;
  if (!m_cond)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(cond);
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
  Implements the SHOW NDB STATUS command.
*/
bool
ndbcluster_show_status(handlerton *hton, THD* thd, stat_print_fn *stat_print,
                       enum ha_stat_type stat_type)
{
  char name[16];
  char buf[IO_SIZE];
  uint buflen;
  DBUG_ENTER("ndbcluster_show_status");
  
  if (stat_type != HA_ENGINE_STATUS)
  {
    DBUG_RETURN(FALSE);
  }

  Ndb* ndb= check_ndb_in_thd(thd);
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  struct st_ndb_status ns;
  if (ndb)
    update_status_variables(thd_ndb, &ns, thd_ndb->connection);
  else
    update_status_variables(NULL, &ns, g_ndb_cluster_connection);

  buflen= (uint)
    my_snprintf(buf, sizeof(buf),
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
    DBUG_RETURN(TRUE);

  for (int i= 0; i < MAX_NDB_NODES; i++)
  {
    if (ns.transaction_hint_count[i] > 0 ||
        ns.transaction_no_hint_count[i] > 0)
    {
      uint namelen= (uint)my_snprintf(name, sizeof(name), "node[%d]", i);
      buflen= (uint)my_snprintf(buf, sizeof(buf),
                          "transaction_hint=%ld, transaction_no_hint=%ld",
                          ns.transaction_hint_count[i],
                          ns.transaction_no_hint_count[i]);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     name, namelen, buf, buflen))
        DBUG_RETURN(TRUE);
    }
  }

  if (ndb)
  {
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      buflen= (uint)
        my_snprintf(buf, sizeof(buf),
                  "created=%u, free=%u, sizeof=%u",
                  tmp.m_created, tmp.m_free, tmp.m_sizeof);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     tmp.m_name, (uint)strlen(tmp.m_name), buf, buflen))
        DBUG_RETURN(TRUE);
    }
  }
  ndbcluster_show_status_binlog(thd, stat_print, stat_type);

  DBUG_RETURN(FALSE);
}


int ha_ndbcluster::get_default_no_partitions(HA_CREATE_INFO *create_info)
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
    DBUG_ASSERT(FALSE);
    abort();
  }
  DBUG_RETURN(hash_value);
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
  part_info->list_of_part_fields= TRUE;
  part_info->part_type= HASH_PARTITION;
  switch (opt_ndb_distribution)
  {
  case NDB_DISTRIBUTION_KEYHASH:
    part_info->linear_hash_ind= FALSE;
    break;
  case NDB_DISTRIBUTION_LINHASH:
    part_info->linear_hash_ind= TRUE;
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  DBUG_VOID_RETURN;
}


int
ha_ndbcluster::set_range_data(const partition_info *part_info,
                              NdbDictionary::Table& ndbtab) const
{
  const uint num_parts = partition_info_num_parts(part_info);
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_range_data");

  int32 *range_data= (int32*)my_malloc(num_parts*sizeof(int32), MYF(0));
  if (!range_data)
  {
    mem_alloc_error(num_parts*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (uint i= 0; i < num_parts; i++)
  {
    longlong range_val= part_info->range_int_array[i];
    if (unsigned_flag)
      range_val-= 0x8000000000000000ULL;
    if (range_val < INT_MIN32 || range_val >= INT_MAX32)
    {
      if ((i != num_parts - 1) ||
          (range_val != LONGLONG_MAX))
      {
        my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
        error= 1;
        goto error;
      }
      range_val= INT_MAX32;
    }
    range_data[i]= (int32)range_val;
  }
  ndbtab.setRangeListData(range_data, num_parts);
error:
  my_free((char*)range_data, MYF(0));
  DBUG_RETURN(error);
}


int
ha_ndbcluster::set_list_data(const partition_info *part_info,
                             NdbDictionary::Table& ndbtab) const
{
  const uint num_list_values = partition_info_num_list_values(part_info);
  int32 *list_data= (int32*)my_malloc(num_list_values*2*sizeof(int32), MYF(0));
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_list_data");

  if (!list_data)
  {
    mem_alloc_error(num_list_values*2*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (uint i= 0; i < num_list_values; i++)
  {
    LIST_PART_ENTRY *list_entry= &part_info->list_array[i];
    longlong list_val= list_entry->list_value;
    if (unsigned_flag)
      list_val-= 0x8000000000000000ULL;
    if (list_val < INT_MIN32 || list_val > INT_MAX32)
    {
      my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
      error= 1;
      goto error;
    }
    list_data[2*i]= (int32)list_val;
    list_data[2*i+1]= list_entry->partition_id;
  }
  ndbtab.setRangeListData(list_data, 2*num_list_values);
error:
  my_free((char*)list_data, MYF(0));
  DBUG_RETURN(error);
}

/*
  User defined partitioning set-up. We need to check how many fragments the
  user wants defined and which node groups to put those into. Later we also
  want to attach those partitions to a tablespace.

  All the functionality of the partition function, partition limits and so
  forth are entirely handled by the MySQL Server. There is one exception to
  this rule for PARTITION BY KEY where NDB handles the hash function and
  this type can thus be handled transparently also by NDB API program.
  For RANGE, HASH and LIST and subpartitioning the NDB API programs must
  implement the function to map to a partition.
*/

int
ha_ndbcluster::set_up_partition_info(partition_info *part_info,
                                     NdbDictionary::Table& ndbtab) const
{
  uint32 frag_data[MAX_PARTITIONS];
  char *ts_names[MAX_PARTITIONS];
  ulong fd_index= 0, i, j;
  NDBTAB::FragmentType ftype= NDBTAB::UserDefined;
  partition_element *part_elem;
  List_iterator<partition_element> part_it(part_info->partitions);
  int error;
  DBUG_ENTER("ha_ndbcluster::set_up_partition_info");

  if (part_info->part_type == HASH_PARTITION &&
      part_info->list_of_part_fields == TRUE)
  {
    Field **fields= part_info->part_field_array;

    ftype= NDBTAB::HashMapPartition;

    for (i= 0; i < part_info->part_field_list.elements; i++)
    {
      NDBCOL *col= ndbtab.getColumn(fields[i]->field_index);
      DBUG_PRINT("info",("setting dist key on %s", col->getName()));
      col->setPartitionKey(TRUE);
    }
  }
  else 
  {
    if (!current_thd->variables.new_mode)
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER(ER_ILLEGAL_HA_CREATE_OPTION),
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
    col.setNullable(FALSE);
    col.setPrimaryKey(FALSE);
    col.setAutoIncrement(FALSE);
    ndbtab.addColumn(col);
    if (part_info->part_type == RANGE_PARTITION)
    {
      if ((error= set_range_data(part_info, ndbtab)))
      {
        DBUG_RETURN(error);
      }
    }
    else if (part_info->part_type == LIST_PARTITION)
    {
      if ((error= set_list_data(part_info, ndbtab)))
      {
        DBUG_RETURN(error);
      }
    }
  }
  ndbtab.setFragmentType(ftype);
  i= 0;
  do
  {
    uint ng;
    part_elem= part_it++;
    if (!part_info->is_sub_partitioned())
    {
      ng= part_elem->nodegroup_id;
      ts_names[fd_index]= part_elem->tablespace_name;
      frag_data[fd_index++]= ng;
    }
    else
    {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      j= 0;
      do
      {
        part_elem= sub_it++;
        ng= part_elem->nodegroup_id;
        ts_names[fd_index]= part_elem->tablespace_name;
        frag_data[fd_index++]= ng;
      } while (++j < partition_info_num_subparts(part_info));
    }
  } while (++i < partition_info_num_parts(part_info));

  const bool use_default_num_parts =
    partition_info_use_default_num_partitions(part_info);
  ndbtab.setDefaultNoPartitionsFlag(use_default_num_parts);
  ndbtab.setLinearFlag(part_info->linear_hash_ind);
  {
    ha_rows max_rows= table_share->max_rows;
    ha_rows min_rows= table_share->min_rows;
    if (max_rows < min_rows)
      max_rows= min_rows;
    if (max_rows != (ha_rows)0) /* default setting, don't set fragmentation */
    {
      ndbtab.setMaxRows(max_rows);
      ndbtab.setMinRows(min_rows);
    }
  }
  ndbtab.setFragmentCount(fd_index);
  ndbtab.setFragmentData(frag_data, fd_index);
  DBUG_RETURN(0);
}

class NDB_ALTER_DATA : public inplace_alter_handler_ctx 
{
public:
  NDB_ALTER_DATA(NdbDictionary::Dictionary *dict,
		 const NdbDictionary::Table *table) :
    dictionary(dict),
    old_table(table),
    new_table(new NdbDictionary::Table(*table)),
      table_id(table->getObjectId()),
      old_table_version(table->getObjectVersion())
  {}
  ~NDB_ALTER_DATA()
  { delete new_table; }
  NdbDictionary::Dictionary *dictionary;
  const  NdbDictionary::Table *old_table;
  NdbDictionary::Table *new_table;
  Uint32 table_id;
  Uint32 old_table_version;
};

static
Alter_inplace_info::HA_ALTER_FLAGS supported_alter_operations()
{
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags= 0;
  return alter_flags |
    Alter_inplace_info::ADD_INDEX |
    Alter_inplace_info::DROP_INDEX |
    Alter_inplace_info::ADD_UNIQUE_INDEX |
    Alter_inplace_info::DROP_UNIQUE_INDEX |
    Alter_inplace_info::ADD_COLUMN |
    Alter_inplace_info::ALTER_COLUMN_DEFAULT |
    Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE |
    Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT |
    Alter_inplace_info::ADD_PARTITION |
    Alter_inplace_info::ALTER_TABLE_REORG |
    Alter_inplace_info::CHANGE_CREATE_OPTION;
}

enum_alter_inplace_result
  ha_ndbcluster::check_if_supported_inplace_alter(TABLE *altered_table,
                                                  Alter_inplace_info *ha_alter_info)
{
  THD *thd= current_thd;
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags= ha_alter_info->handler_flags;
  Alter_inplace_info::HA_ALTER_FLAGS supported= supported_alter_operations();
  Alter_inplace_info::HA_ALTER_FLAGS not_supported= ~supported;
  uint i;
  const NDBTAB *tab= (const NDBTAB *) m_table;
  Alter_inplace_info::HA_ALTER_FLAGS add_column= 0;
  Alter_inplace_info::HA_ALTER_FLAGS adding= 0;
  Alter_inplace_info::HA_ALTER_FLAGS dropping= 0;
  enum_alter_inplace_result result= HA_ALTER_INPLACE_SHARED_LOCK;
  bool auto_increment_value_changed= false;
  bool max_rows_changed= false;

  DBUG_ENTER("ha_ndbcluster::check_if_supported_inplace_alter");
  add_column= add_column | Alter_inplace_info::ADD_COLUMN;
  adding= adding |
    Alter_inplace_info::ADD_INDEX
    | Alter_inplace_info::ADD_UNIQUE_INDEX;
  dropping= dropping |
    Alter_inplace_info::DROP_INDEX
    | Alter_inplace_info::DROP_UNIQUE_INDEX;
  partition_info *part_info= altered_table->part_info;
  const NDBTAB *old_tab= m_table;

  if (THDVAR(thd, use_copying_alter_table))
  {
    DBUG_PRINT("info", ("On-line alter table disabled"));
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }
#ifndef DBUG_OFF
  {
    DBUG_PRINT("info", ("Passed alter flags 0x%lx", alter_flags));
    DBUG_PRINT("info", ("Supported 0x%lx", supported));
    DBUG_PRINT("info", ("Not supported 0x%lx", not_supported));
    DBUG_PRINT("info", ("alter_flags & not_supported 0x%lx",
                        alter_flags & not_supported));
  }
#endif

  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    if (create_info->auto_increment_value !=
      table->file->stats.auto_increment_value)
      auto_increment_value_changed= true;
    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS)
      max_rows_changed= true;
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
  {
    /*
      sql_partition.cc tries to compute what is going on
      and sets flags...that we clear
    */
    if (part_info->use_default_num_partitions)
    {
      alter_flags= alter_flags & ~Alter_inplace_info::COALESCE_PARTITION;
      alter_flags= alter_flags & ~Alter_inplace_info::ADD_PARTITION;
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_COLUMN_DEFAULT &&
      !(alter_flags & Alter_inplace_info::ADD_COLUMN))
  {
    DBUG_PRINT("info", ("Altering default value is not supported"));
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  if (alter_flags & not_supported)
  {
#ifndef DBUG_OFF
    Alter_inplace_info::HA_ALTER_FLAGS tmp= alter_flags;
    tmp&= not_supported;
    DBUG_PRINT("info", ("Detected unsupported change: 0x%lx", tmp));
#endif
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  if (alter_flags & Alter_inplace_info::ADD_COLUMN ||
      alter_flags & Alter_inplace_info::ADD_PARTITION ||
      alter_flags & Alter_inplace_info::ALTER_TABLE_REORG ||
      max_rows_changed)
  {
     Ndb *ndb= get_ndb(thd);
     NDBDICT *dict= ndb->getDictionary();
     ndb->setDatabaseName(m_dbname);
     NdbDictionary::Table new_tab= *old_tab;

     result= HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
     if (alter_flags & Alter_inplace_info::ADD_COLUMN)
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
         DBUG_PRINT("info", ("Only add column exclusively can be performed on-line"));
         DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
       }
       /*
         Check for extra fields for hidden primary key
         or user defined partitioning
       */
       if (table_share->primary_key == MAX_KEY ||
           part_info->part_type != HASH_PARTITION ||
           !part_info->list_of_part_fields)
         DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);

       /* Find the new fields */
       for (uint i= table->s->fields; i < altered_table->s->fields; i++)
       {
         Field *field= altered_table->field[i];
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
             DBUG_PRINT("info",("Adding column with non-null default value is not supported on-line"));
             DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
           }
         }
         /* Create new field to check if it can be added */
         if ((my_errno= create_ndb_column(thd, col, field, create_info,
                                          COLUMN_FORMAT_TYPE_DYNAMIC)))
         {
           DBUG_PRINT("info", ("create_ndb_column returned %u", my_errno));
           DBUG_RETURN(HA_ALTER_ERROR);
         }
         if (new_tab.addColumn(col))
         {
           my_errno= errno;
           DBUG_PRINT("info", ("NdbDictionary::Table::addColumn returned %u", my_errno));
           DBUG_RETURN(HA_ALTER_ERROR);
         }
       }
     }

     if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
     {
       /* 
          Refuse if Max_rows has been used before...
          Workaround is to use ALTER ONLINE TABLE <t> MAX_ROWS=<bigger>;
       */
       if (old_tab->getMaxRows() != 0)
       {
         push_warning(current_thd,
                      Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                      "Cannot online REORGANIZE a table with Max_Rows set.  "
                      "Use ALTER TABLE ... MAX_ROWS=<new_val> or offline REORGANIZE "
                      "to redistribute this table.");
         DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
       }
       new_tab.setFragmentCount(0);
       new_tab.setFragmentData(0, 0);
     }
     else if (alter_flags & Alter_inplace_info::ADD_PARTITION)
     {
       DBUG_PRINT("info", ("Adding partition (%u)", part_info->num_parts));
       new_tab.setFragmentCount(part_info->num_parts);
     }
     if (max_rows_changed)
     {
       ulonglong rows= create_info->max_rows;
       uint no_fragments= get_no_fragments(rows);
       uint reported_frags= no_fragments;
       if (adjusted_frag_count(ndb, no_fragments, reported_frags))
       {
         push_warning(current_thd,
                      Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                      "Ndb might have problems storing the max amount "
                      "of rows specified");
       }
       if (reported_frags < old_tab->getFragmentCount())
       {
         DBUG_PRINT("info", ("Online reduction in number of fragments not supported"));
         DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
       }
       new_tab.setFragmentCount(reported_frags);
       new_tab.setDefaultNoPartitionsFlag(false);
       new_tab.setFragmentData(0, 0);
     }

     NDB_Modifiers table_modifiers(ndb_table_modifiers);
     table_modifiers.parse(thd, "NDB_TABLE=", create_info->comment.str,
                           create_info->comment.length);
     const NDB_Modifier* mod_nologging = table_modifiers.get("NOLOGGING");

     if (mod_nologging->m_found)
     {
       new_tab.setLogging(!mod_nologging->m_val_bool);
     }
     
     if (dict->supportedAlterTable(*old_tab, new_tab))
     {
       DBUG_PRINT("info", ("Adding column(s) supported on-line"));
     }
     else
     {
       DBUG_PRINT("info",("Adding column not supported on-line"));
       DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
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
       DBUG_PRINT("info",("Only one index can be added on-line"));
       DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
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
       DBUG_PRINT("info",("Only one index can be dropped on-line"));
       DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }

  for (i= 0; i < table->s->fields; i++)
  {
    Field *field= table->field[i];
    const NDBCOL *col= tab->getColumn(i);

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
        DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
      }
      new_col.setStorageType(NdbDictionary::Column::StorageTypeMemory);
    }
    else if (field->field_storage_type() == HA_SM_DEFAULT)
    {
      /**
       * If user didn't specify any column format, keep old
       *   to make as many alter's as possible online
       */
      new_col.setStorageType(col->getStorageType());
    }

    if (col->getStorageType() != new_col.getStorageType())
    {
      DBUG_PRINT("info", ("Column storage media is changed"));
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    if (field->flags & FIELD_IS_RENAMED)
    {
      DBUG_PRINT("info", ("Field has been renamed, copy table"));
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    if ((field->flags & FIELD_IN_ADD_INDEX) &&
        (col->getStorageType() == NdbDictionary::Column::StorageTypeDisk))
    {
      DBUG_PRINT("info", ("add/drop index not supported for disk stored column"));
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }

  /* Check that only auto_increment value was changed */
  if (auto_increment_value_changed)
  {
    if (create_info->used_fields ^ ~HA_CREATE_USED_AUTO)
    {
      DBUG_PRINT("info", ("Not only auto_increment value changed"));
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }
  else
  {
    /* Check that row format didn't change */
    if (create_info->used_fields & HA_CREATE_USED_AUTO &&
        get_row_type() != create_info->row_type)
    {
      DBUG_PRINT("info", ("Row format changed"));
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }
  DBUG_PRINT("info", ("Ndb supports ALTER on-line"));
  DBUG_RETURN(result);
}

bool
ha_ndbcluster::prepare_inplace_alter_table(TABLE *altered_table,
                                              Alter_inplace_info *ha_alter_info)
{
  int error= 0;
  uint i;
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= get_ndb(thd);
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(m_dbname);
  NDB_ALTER_DATA *alter_data;
  const NDBTAB *old_tab;
  NdbDictionary::Table *new_tab;
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags= ha_alter_info->handler_flags;
  Alter_inplace_info::HA_ALTER_FLAGS adding= 0;
  Alter_inplace_info::HA_ALTER_FLAGS dropping= 0;
  bool auto_increment_value_changed= false;
  bool max_rows_changed= false;

  DBUG_ENTER("ha_ndbcluster::prepare_inplace_alter_table");
  adding=  adding |
    Alter_inplace_info::ADD_INDEX
    |  Alter_inplace_info::ADD_UNIQUE_INDEX;
  dropping= dropping |
    Alter_inplace_info::DROP_INDEX
    |  Alter_inplace_info::DROP_UNIQUE_INDEX;

  ha_alter_info->handler_ctx= 0;
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::prepare_inplace_alter_table"))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  if (!(alter_data= new NDB_ALTER_DATA(dict, m_table)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  old_tab= alter_data->old_table;
  new_tab= alter_data->new_table;
  ha_alter_info->handler_ctx= alter_data;

#ifndef DBUG_OFF
  {
    DBUG_PRINT("info", ("altered_table %s, alter_flags 0x%lx",
                        altered_table->s->table_name.str,
                        alter_flags));

  }
#endif

  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    if (create_info->auto_increment_value !=
      table->file->stats.auto_increment_value)
      auto_increment_value_changed= true;
    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS)
      max_rows_changed= true;
  }

  prepare_for_alter();

  if (dict->beginSchemaTrans() == -1)
  {
    DBUG_PRINT("info", ("Failed to start schema transaction"));
    ERR_PRINT(dict->getNdbError());
    error= ndb_to_mysql_error(&dict->getNdbError());
    table->file->print_error(error, MYF(0));
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
    if ((error= add_index_impl(thd, altered_table, key_info,
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
    uint          *key_numbers;
    uint          *keyno_p;
    KEY           **idx_p;
    KEY           **idx_end_p;
    DBUG_PRINT("info", ("Renumbering indexes"));
    /* The prepare_drop_index() method takes an array of key numbers. */
    key_numbers= (uint*) thd->alloc(sizeof(uint) * ha_alter_info->index_drop_count);
    keyno_p= key_numbers;
    /* Get the number of each key. */
    for (idx_p= ha_alter_info->index_drop_buffer,
	 idx_end_p= idx_p + ha_alter_info->index_drop_count;
	 idx_p < idx_end_p;
	 idx_p++, keyno_p++)
    {
      // Find the key number matching the key to be dropped
      KEY *keyp= *idx_p;
      uint i;
      for(i=0; i < table->s->keys; i++)
      {
	if (keyp == table->key_info + i)
	  break;
      }
      DBUG_PRINT("info", ("Dropping index %u", i)); 
      *keyno_p= i;
    }
    /*
      Tell the handler to prepare for drop indexes.
      This re-numbers the indexes to get rid of gaps.
    */
    if ((error= prepare_drop_index(table, key_numbers,
				   ha_alter_info->index_drop_count)))
    {
      table->file->print_error(error, MYF(0));
      goto abort;
    }
  }

  if (alter_flags &  Alter_inplace_info::ADD_COLUMN)
  {
     NDBCOL col;

     /* Find the new fields */
     for (i= table->s->fields; i < altered_table->s->fields; i++)
     {
       Field *field= altered_table->field[i];
       DBUG_PRINT("info", ("Found new field %s", field->field_name));
       if ((my_errno= create_ndb_column(thd, col, field, create_info,
                                        COLUMN_FORMAT_TYPE_DYNAMIC)))
       {
         error= my_errno;
         goto abort;
       }
       /*
         If the user has not specified the field format
         make it dynamic to enable on-line add attribute
       */
       if (field->column_format() == COLUMN_FORMAT_TYPE_DEFAULT &&
           create_info->row_type == ROW_TYPE_DEFAULT &&
           col.getDynamic())
       {
         push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                             ER_ILLEGAL_HA_CREATE_OPTION,
                             "Converted FIXED field to DYNAMIC "
                             "to enable on-line ADD COLUMN",
                             field->field_name);
       }
       new_tab->addColumn(col);
     }
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG ||
      alter_flags & Alter_inplace_info::ADD_PARTITION ||
      max_rows_changed)
  {
    if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG)
    {
      new_tab->setFragmentCount(0);
      new_tab->setFragmentData(0, 0);
    }
    else if (alter_flags & Alter_inplace_info::ADD_PARTITION)
    {
      partition_info *part_info= altered_table->part_info;
      new_tab->setFragmentCount(part_info->num_parts);
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
    }
    
    int res= dict->prepareHashMap(*old_tab, *new_tab);
    if (res == -1)
    {
      const NdbError err= dict->getNdbError();
      my_errno= ndb_to_mysql_error(&err);
      goto abort;
    }
  }

  DBUG_RETURN(0);
abort:
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
        == -1)
  {
    DBUG_PRINT("info", ("Failed to abort schema transaction"));
    ERR_PRINT(dict->getNdbError());
    error= ndb_to_mysql_error(&dict->getNdbError());
  }

err:
  DBUG_RETURN(error);
}

int ha_ndbcluster::alter_frm(THD *thd, const char *file, 
                             NDB_ALTER_DATA *alter_data)
{
  uchar *data= NULL, *pack_data= NULL;
  size_t length, pack_length;
  int error= 0;

  DBUG_ENTER("alter_frm");

  DBUG_PRINT("enter", ("file: %s", file));

  NDBDICT *dict= alter_data->dictionary;

  // TODO handle this
  DBUG_ASSERT(m_table != 0);

  DBUG_ASSERT(get_ndb_share_state(m_share) == NSS_ALTERED);
  if (readfrm(file, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    DBUG_PRINT("info", ("Missing frm for %s", m_tabname));
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    error= 1;
    my_error(ER_FILE_NOT_FOUND, MYF(0), file); 
  }
  else
  {
    DBUG_PRINT("info", ("Table %s has changed, altering frm in ndb",
                        m_tabname));
    const NDBTAB *old_tab= alter_data->old_table;
    NdbDictionary::Table *new_tab= alter_data->new_table;

    new_tab->setFrm(pack_data, (Uint32)pack_length);
    if (dict->alterTableGlobal(*old_tab, *new_tab))
    {
      DBUG_PRINT("info", ("On-line alter of table %s failed", m_tabname));
      error= ndb_to_mysql_error(&dict->getNdbError());
      my_error(error, MYF(0));
    }
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
  }

  /* ndb_share reference schema(?) free */
  DBUG_PRINT("NDB_SHARE", ("%s binlog schema(?) free  use_count: %u",
                           m_share->key, m_share->use_count));

  DBUG_RETURN(error);
}

bool
ha_ndbcluster::inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info)
{
  DBUG_ENTER("ha_ndbcluster::inplace_alter_table");
  int error= 0;
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  NDBDICT *dict= alter_data->dictionary;
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags= ha_alter_info->handler_flags;
  Alter_inplace_info::HA_ALTER_FLAGS dropping= 0;
  bool auto_increment_value_changed= false;

  dropping= dropping  |
    Alter_inplace_info::DROP_INDEX
    | Alter_inplace_info::DROP_UNIQUE_INDEX;

  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::inplace_alter_table"))
  {
    error= HA_ERR_NO_CONNECTION;
    goto err;
  }

  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION)
  {
    if (create_info->auto_increment_value !=
      table->file->stats.auto_increment_value)
      auto_increment_value_changed= true;
  }

  if (alter_flags & dropping)
  {
    /* Tell the handler to finally drop the indexes. */
    if ((error= final_drop_index(table)))
    {
      print_error(error, MYF(0));
      goto abort;
    }
  }

  DBUG_PRINT("info", ("getting frm file %s", altered_table->s->path.str));

  DBUG_ASSERT(alter_data);
  error= alter_frm(thd, altered_table->s->path.str, alter_data);
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
  DBUG_RETURN(error);
}

bool
ha_ndbcluster::commit_inplace_alter_table(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info,
                                          bool commit)
{
  DBUG_ENTER("ha_ndbcluster::commit_inplace_alter_table");

  if (!commit)
    DBUG_RETURN(abort_inplace_alter_table(altered_table,
                                          ha_alter_info));
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::commit_inplace_alter_table"))
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  const char *db= table->s->db.str;
  const char *name= table->s->table_name.str;
  uint32 table_id= 0, table_version= 0;
  DBUG_ASSERT(alter_data != 0);
  if (alter_data)
  {
    table_id= alter_data->table_id;
    table_version= alter_data->old_table_version;
  }
  ndbcluster_log_schema_op(thd, thd->query(), thd->query_length(),
                           db, name,
                           table_id, table_version,
                           SOT_ONLINE_ALTER_TABLE_PREPARE,
                           NULL, NULL);
  delete alter_data;
  ha_alter_info->handler_ctx= 0;
  set_ndb_share_state(m_share, NSS_INITIAL);
  free_share(&m_share); // Decrease ref_count
  DBUG_RETURN(0);
}

bool
ha_ndbcluster::abort_inplace_alter_table(TABLE *altered_table,
                                         Alter_inplace_info *ha_alter_info)
{
  DBUG_ENTER("ha_ndbcluster::abort_inplace_alter_table");
  int error= 0;
  NDB_ALTER_DATA *alter_data= (NDB_ALTER_DATA *) ha_alter_info->handler_ctx;
  if (!alter_data)
    DBUG_RETURN(0);

  NDBDICT *dict= alter_data->dictionary;
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort)
      == -1)
  {
    DBUG_PRINT("info", ("Failed to abort schema transaction"));
    ERR_PRINT(dict->getNdbError());
  }
  /* ndb_share reference schema free */
  DBUG_PRINT("NDB_SHARE", ("%s binlog schema free  use_count: %u",
                           m_share->key, m_share->use_count));
  delete alter_data;
  ha_alter_info->handler_ctx= 0;
  set_ndb_share_state(m_share, NSS_INITIAL);
  free_share(&m_share); // Decrease ref_count
  DBUG_RETURN(error);
}

void ha_ndbcluster::notify_table_changed()
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
      }
    }
  }

  /*
    all mysqld's will switch to using the new_op, and delete the old
    event operation
  */
  ndbcluster_log_schema_op(thd, thd->query(), thd->query_length(),
                           db, name,
                           table_id, table_version,
                           SOT_ONLINE_ALTER_TABLE_COMMIT,
                           NULL, NULL);

  DBUG_VOID_RETURN;
}

static
bool set_up_tablespace(st_alter_tablespace *alter_info,
                       NdbDictionary::Tablespace *ndb_ts)
{
  if (alter_info->extent_size >= (Uint64(1) << 32))
  {
    // TODO set correct error
    return TRUE;
  }
  ndb_ts->setName(alter_info->tablespace_name);
  ndb_ts->setExtentSize(Uint32(alter_info->extent_size));
  ndb_ts->setDefaultLogfileGroup(alter_info->logfile_group_name);
  return FALSE;
}

static
bool set_up_datafile(st_alter_tablespace *alter_info,
                     NdbDictionary::Datafile *ndb_df)
{
  if (alter_info->max_size > 0)
  {
    my_error(ER_TABLESPACE_AUTO_EXTEND_ERROR, MYF(0));
    return TRUE;
  }
  ndb_df->setPath(alter_info->data_file_name);
  ndb_df->setSize(alter_info->initial_size);
  ndb_df->setTablespace(alter_info->tablespace_name);
  return FALSE;
}

static
bool set_up_logfile_group(st_alter_tablespace *alter_info,
                          NdbDictionary::LogfileGroup *ndb_lg)
{
  if (alter_info->undo_buffer_size >= (Uint64(1) << 32))
  {
    // TODO set correct error
    return TRUE;
  }

  ndb_lg->setName(alter_info->logfile_group_name);
  ndb_lg->setUndoBufferSize(Uint32(alter_info->undo_buffer_size));
  return FALSE;
}

static
bool set_up_undofile(st_alter_tablespace *alter_info,
                     NdbDictionary::Undofile *ndb_uf)
{
  ndb_uf->setPath(alter_info->undo_file_name);
  ndb_uf->setSize(alter_info->initial_size);
  ndb_uf->setLogfileGroup(alter_info->logfile_group_name);
  return FALSE;
}

static
int ndbcluster_alter_tablespace(handlerton *hton,
                                THD* thd, st_alter_tablespace *alter_info)
{
  int is_tablespace= 0;
  NdbError err;
  NDBDICT *dict;
  int error;
  const char *errmsg;
  Ndb *ndb;
  DBUG_ENTER("ndbcluster_alter_tablespace");
  LINT_INIT(errmsg);

  ndb= check_ndb_in_thd(thd);
  if (ndb == NULL)
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  dict= ndb->getDictionary();

  uint32 table_id= 0, table_version= 0;
  switch (alter_info->ts_cmd_type){
  case (CREATE_TABLESPACE):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
    
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
    table_id = objid.getObjectId();
    table_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnExtentRoundUp)
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
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
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          dict->getWarningFlags(),
                          "Datafile size rounded up to extent size");
    }
    else /* produce only 1 message */
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnDatafileRoundDown)
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          dict->getWarningFlags(),
                          "Datafile size rounded down to extent size");
    }
    is_tablespace= 1;
    break;
  }
  case (ALTER_TABLESPACE):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
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
      table_id= objid.getObjectId();
      table_version= objid.getObjectVersion();
      if (dict->getWarningFlags() &
          NdbDictionary::Dictionary::WarnDatafileRoundUp)
      {
        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                            dict->getWarningFlags(),
                            "Datafile size rounded up to extent size");
      }
      else /* produce only 1 message */
      if (dict->getWarningFlags() &
          NdbDictionary::Dictionary::WarnDatafileRoundDown)
      {
        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                            dict->getWarningFlags(),
                            "Datafile size rounded down to extent size");
      }
    }
    else if(alter_info->ts_alter_tablespace_type == ALTER_TABLESPACE_DROP_FILE)
    {
      NdbDictionary::Tablespace ts= dict->getTablespace(alter_info->tablespace_name);
      NdbDictionary::Datafile df= dict->getDatafile(0, alter_info->data_file_name);
      NdbDictionary::ObjectId objid;
      df.getTablespaceId(&objid);
      table_id = df.getObjectId();
      table_version = df.getObjectVersion();
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
    is_tablespace= 1;
    break;
  }
  case (CREATE_LOGFILE_GROUP):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
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
    table_id = objid.getObjectId();
    table_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnUndobufferRoundUp)
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
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
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          dict->getWarningFlags(),
                          "Undofile size rounded down to kernel page size");
    }
    break;
  }
  case (ALTER_LOGFILE_GROUP):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
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
    table_id = objid.getObjectId();
    table_version = objid.getObjectVersion();
    if (dict->getWarningFlags() &
        NdbDictionary::Dictionary::WarnUndofileRoundDown)
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          dict->getWarningFlags(),
                          "Undofile size rounded down to kernel page size");
    }
    break;
  }
  case (DROP_TABLESPACE):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "TABLESPACE";
    NdbDictionary::Tablespace ts=
      dict->getTablespace(alter_info->tablespace_name);
    table_id= ts.getObjectId();
    table_version= ts.getObjectVersion();
    if (dict->dropTablespace(ts))
    {
      goto ndberror;
    }
    is_tablespace= 1;
    break;
  }
  case (DROP_LOGFILE_GROUP):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "LOGFILE GROUP";
    NdbDictionary::LogfileGroup lg=
      dict->getLogfileGroup(alter_info->logfile_group_name);
    table_id= lg.getObjectId();
    table_version= lg.getObjectVersion();
    if (dict->dropLogfileGroup(lg))
    {
      goto ndberror;
    }
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
  if (is_tablespace)
    ndbcluster_log_schema_op(thd,
                             thd->query(), thd->query_length(),
                             "", alter_info->tablespace_name,
                             table_id, table_version,
                             SOT_TABLESPACE, NULL, NULL);
  else
    ndbcluster_log_schema_op(thd,
                             thd->query(), thd->query_length(),
                             "", alter_info->logfile_group_name,
                             table_id, table_version,
                             SOT_LOGFILE_GROUP, NULL, NULL);
  DBUG_RETURN(FALSE);

ndberror:
  err= dict->getNdbError();
ndberror2:
  ndb_to_mysql_error(&err);
  
  my_error(error, MYF(0), errmsg);
  DBUG_RETURN(1);
}


bool ha_ndbcluster::get_no_parts(const char *name, uint *no_parts)
{
  THD *thd= current_thd;
  Ndb *ndb;
  NDBDICT *dict;
  int err;
  DBUG_ENTER("ha_ndbcluster::get_no_parts");
  LINT_INIT(err);

  set_dbname(name);
  set_tabname(name);
  for (;;)
  {
    if (check_ndb_connection(thd))
    {
      err= HA_ERR_NO_CONNECTION;
      break;
    }
    ndb= get_ndb(thd);
    ndb->setDatabaseName(m_dbname);
    Ndb_table_guard ndbtab_g(dict= ndb->getDictionary(), m_tabname);
    if (!ndbtab_g.get_table())
      ERR_BREAK(dict->getNdbError(), err);
    *no_parts= ndbtab_g.get_table()->getFragmentCount();
    DBUG_RETURN(FALSE);
  }

  print_error(err, MYF(0));
  DBUG_RETURN(TRUE);
}

static int ndbcluster_fill_files_table(handlerton *hton, 
                                       THD *thd, 
                                       TABLE_LIST *tables,
                                       Item *cond)
{
  TABLE* table= tables->table;
  Ndb *ndb= check_ndb_in_thd(thd);
  NdbDictionary::Dictionary* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List dflist;
  NdbError ndberr;
  uint i;
  DBUG_ENTER("ndbcluster_fill_files_table");

  dict->listObjects(dflist, NdbDictionary::Object::Datafile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < dflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt = dflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    uint id;
    
    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_alive_node(iter)))
    {
      init_fill_schema_files_row(table);
      NdbDictionary::Datafile df= dict->getDatafile(id, elt.name);
      ndberr= dict->getNdbError();
      if(ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;

        if (ndberr.classification == NdbError::UnknownResultError)
          continue;

        ERR_RETURN(ndberr);
      }
      NdbDictionary::Tablespace ts= dict->getTablespace(df.getTablespace());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      table->field[IS_FILES_FILE_NAME]->set_notnull();
      table->field[IS_FILES_FILE_NAME]->store(elt.name, (uint)strlen(elt.name),
                                              system_charset_info);
      table->field[IS_FILES_FILE_TYPE]->set_notnull();
      table->field[IS_FILES_FILE_TYPE]->store("DATAFILE",8,
                                              system_charset_info);
      table->field[IS_FILES_TABLESPACE_NAME]->set_notnull();
      table->field[IS_FILES_TABLESPACE_NAME]->store(df.getTablespace(),
                                                    (uint)strlen(df.getTablespace()),
                                                    system_charset_info);
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->
        store(ts.getDefaultLogfileGroup(),
              (uint)strlen(ts.getDefaultLogfileGroup()),
              system_charset_info);
      table->field[IS_FILES_ENGINE]->set_notnull();
      table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                           ndbcluster_hton_name_length,
                                           system_charset_info);

      table->field[IS_FILES_FREE_EXTENTS]->set_notnull();
      table->field[IS_FILES_FREE_EXTENTS]->store(df.getFree()
                                                 / ts.getExtentSize(), true);
      table->field[IS_FILES_TOTAL_EXTENTS]->set_notnull();
      table->field[IS_FILES_TOTAL_EXTENTS]->store(df.getSize()
                                                  / ts.getExtentSize(), true);
      table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
      table->field[IS_FILES_EXTENT_SIZE]->store(ts.getExtentSize(), true);
      table->field[IS_FILES_INITIAL_SIZE]->set_notnull();
      table->field[IS_FILES_INITIAL_SIZE]->store(df.getSize(), true);
      table->field[IS_FILES_MAXIMUM_SIZE]->set_notnull();
      table->field[IS_FILES_MAXIMUM_SIZE]->store(df.getSize(), true);
      table->field[IS_FILES_VERSION]->set_notnull();
      table->field[IS_FILES_VERSION]->store(df.getObjectVersion(), true);

      table->field[IS_FILES_ROW_FORMAT]->set_notnull();
      table->field[IS_FILES_ROW_FORMAT]->store("FIXED", 5, system_charset_info);

      char extra[30];
      int len= (int)my_snprintf(extra, sizeof(extra), "CLUSTER_NODE=%u", id);
      table->field[IS_FILES_EXTRA]->set_notnull();
      table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }

  NdbDictionary::Dictionary::List tslist;
  dict->listObjects(tslist, NdbDictionary::Object::Tablespace);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < tslist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element&elt= tslist.elements[i];

    NdbDictionary::Tablespace ts= dict->getTablespace(elt.name);
    ndberr= dict->getNdbError();
    if (ndberr.classification != NdbError::NoError)
    {
      if (ndberr.classification == NdbError::SchemaError)
        continue;
      ERR_RETURN(ndberr);
    }

    init_fill_schema_files_row(table);
    table->field[IS_FILES_FILE_TYPE]->set_notnull();
    table->field[IS_FILES_FILE_TYPE]->store("TABLESPACE", 10,
                                            system_charset_info);

    table->field[IS_FILES_TABLESPACE_NAME]->set_notnull();
    table->field[IS_FILES_TABLESPACE_NAME]->store(elt.name,
                                                     (uint)strlen(elt.name),
                                                     system_charset_info);
    table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
    table->field[IS_FILES_LOGFILE_GROUP_NAME]->
      store(ts.getDefaultLogfileGroup(),
           (uint)strlen(ts.getDefaultLogfileGroup()),
           system_charset_info);

    table->field[IS_FILES_ENGINE]->set_notnull();
    table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                         ndbcluster_hton_name_length,
                                         system_charset_info);

    table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
    table->field[IS_FILES_EXTENT_SIZE]->store(ts.getExtentSize(), true);

    table->field[IS_FILES_VERSION]->set_notnull();
    table->field[IS_FILES_VERSION]->store(ts.getObjectVersion(), true);

    schema_table_store_record(thd, table);
  }

  NdbDictionary::Dictionary::List uflist;
  dict->listObjects(uflist, NdbDictionary::Object::Undofile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < uflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt= uflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    unsigned id;

    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_alive_node(iter)))
    {
      NdbDictionary::Undofile uf= dict->getUndofile(id, elt.name);
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        if (ndberr.classification == NdbError::UnknownResultError)
          continue;
        ERR_RETURN(ndberr);
      }
      NdbDictionary::LogfileGroup lfg=
        dict->getLogfileGroup(uf.getLogfileGroup());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      init_fill_schema_files_row(table);
      table->field[IS_FILES_FILE_NAME]->set_notnull();
      table->field[IS_FILES_FILE_NAME]->store(elt.name, (uint)strlen(elt.name),
                                              system_charset_info);
      table->field[IS_FILES_FILE_TYPE]->set_notnull();
      table->field[IS_FILES_FILE_TYPE]->store("UNDO LOG", 8,
                                              system_charset_info);
      NdbDictionary::ObjectId objid;
      uf.getLogfileGroupId(&objid);
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NAME]->store(uf.getLogfileGroup(),
                                                  (uint)strlen(uf.getLogfileGroup()),
                                                       system_charset_info);
      table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->set_notnull();
      table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->store(objid.getObjectId(), true);
      table->field[IS_FILES_ENGINE]->set_notnull();
      table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                           ndbcluster_hton_name_length,
                                           system_charset_info);

      table->field[IS_FILES_TOTAL_EXTENTS]->set_notnull();
      table->field[IS_FILES_TOTAL_EXTENTS]->store(uf.getSize()/4, true);
      table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
      table->field[IS_FILES_EXTENT_SIZE]->store(4, true);

      table->field[IS_FILES_INITIAL_SIZE]->set_notnull();
      table->field[IS_FILES_INITIAL_SIZE]->store(uf.getSize(), true);
      table->field[IS_FILES_MAXIMUM_SIZE]->set_notnull();
      table->field[IS_FILES_MAXIMUM_SIZE]->store(uf.getSize(), true);

      table->field[IS_FILES_VERSION]->set_notnull();
      table->field[IS_FILES_VERSION]->store(uf.getObjectVersion(), true);

      char extra[100];
      int len= (int)my_snprintf(extra,sizeof(extra),"CLUSTER_NODE=%u;UNDO_BUFFER_SIZE=%lu",
                           id, (ulong) lfg.getUndoBufferSize());
      table->field[IS_FILES_EXTRA]->set_notnull();
      table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }

  // now for LFGs
  NdbDictionary::Dictionary::List lfglist;
  dict->listObjects(lfglist, NdbDictionary::Object::LogfileGroup);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < lfglist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt= lfglist.elements[i];

    NdbDictionary::LogfileGroup lfg= dict->getLogfileGroup(elt.name);
    ndberr= dict->getNdbError();
    if (ndberr.classification != NdbError::NoError)
    {
      if (ndberr.classification == NdbError::SchemaError)
        continue;
      ERR_RETURN(ndberr);
    }

    init_fill_schema_files_row(table);
    table->field[IS_FILES_FILE_TYPE]->set_notnull();
    table->field[IS_FILES_FILE_TYPE]->store("UNDO LOG", 8,
                                            system_charset_info);

    table->field[IS_FILES_LOGFILE_GROUP_NAME]->set_notnull();
    table->field[IS_FILES_LOGFILE_GROUP_NAME]->store(elt.name,
                                                     (uint)strlen(elt.name),
                                                     system_charset_info);
    table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->set_notnull();
    table->field[IS_FILES_LOGFILE_GROUP_NUMBER]->store(lfg.getObjectId(), true);
    table->field[IS_FILES_ENGINE]->set_notnull();
    table->field[IS_FILES_ENGINE]->store(ndbcluster_hton_name,
                                         ndbcluster_hton_name_length,
                                         system_charset_info);

    table->field[IS_FILES_FREE_EXTENTS]->set_notnull();
    table->field[IS_FILES_FREE_EXTENTS]->store(lfg.getUndoFreeWords(), true);
    table->field[IS_FILES_EXTENT_SIZE]->set_notnull();
    table->field[IS_FILES_EXTENT_SIZE]->store(4, true);

    table->field[IS_FILES_VERSION]->set_notnull();
    table->field[IS_FILES_VERSION]->store(lfg.getObjectVersion(), true);

    char extra[100];
    int len= (int)my_snprintf(extra,sizeof(extra),
                         "UNDO_BUFFER_SIZE=%lu",
                         (ulong) lfg.getUndoBufferSize());
    table->field[IS_FILES_EXTRA]->set_notnull();
    table->field[IS_FILES_EXTRA]->store(extra, len, system_charset_info);
    schema_table_store_record(thd, table);
  }
  DBUG_RETURN(0);
}

static int show_ndb_vars(THD *thd, SHOW_VAR *var, char *buff)
{
  if (!check_ndb_in_thd(thd))
    return -1;
  struct st_ndb_status *st;
  SHOW_VAR *st_var;
  {
    char *mem= (char*)sql_alloc(sizeof(struct st_ndb_status) +
                                sizeof(ndb_status_variables_dynamic));
    st= new (mem) st_ndb_status;
    st_var= (SHOW_VAR*)(mem + sizeof(struct st_ndb_status));
    memcpy(st_var, &ndb_status_variables_dynamic, sizeof(ndb_status_variables_dynamic));
    int i= 0;
    SHOW_VAR *tmp= &(ndb_status_variables_dynamic[0]);
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

SHOW_VAR ndb_status_variables_export[]= {
  {"Ndb",          (char*) &show_ndb_vars,                 SHOW_FUNC},
  {"Ndb_conflict", (char*) &ndb_status_conflict_variables, SHOW_ARRAY},
  {"Ndb",          (char*) &ndb_status_injector_variables, SHOW_ARRAY},
  {"Ndb",          (char*) &ndb_status_slave_variables,    SHOW_ARRAY},
  {"Ndb",          (char*) &show_ndb_server_api_stats,     SHOW_FUNC},
  {"Ndb_index_stat", (char*) &ndb_status_index_stat_variables, SHOW_ARRAY},
  {NullS, NullS, SHOW_LONG}
};

static MYSQL_SYSVAR_ULONG(
  cache_check_time,                  /* name */
  opt_ndb_cache_check_time,              /* var */
  PLUGIN_VAR_RQCMDARG,
  "A dedicated thread is created to, at the given "
  "millisecond interval, invalidate the query cache "
  "if another MySQL server in the cluster has changed "
  "the data in the database.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0,                                 /* default */
  0,                                 /* min */
  ONE_YEAR_IN_SECONDS,               /* max */
  0                                  /* block */
);


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


static MYSQL_SYSVAR_UINT(
  cluster_connection_pool,           /* name */
  opt_ndb_cluster_connection_pool,   /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Pool of cluster connections to be used by mysql server.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1,                                 /* default */
  1,                                 /* min */
  63,                                /* max */
  0                                  /* block */
);

/* should be in index_stat.h */

extern int
ndb_index_stat_option_check(MYSQL_THD,
                            struct st_mysql_sys_var *var,
                            void *save,
                            struct st_mysql_value *value);
extern void
ndb_index_stat_option_update(MYSQL_THD,
                             struct st_mysql_sys_var *var,
                             void *var_ptr,
                             const void *save);

extern char ndb_index_stat_option_buf[];

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
  "Threshold on number of epochs to be behind before reporting binlog "
  "status. E.g. 3 means that if the difference between what epoch has "
  "been received from the storage nodes and what has been applied to "
  "the binlog is 3 or more, a status message will be sent to the cluster "
  "log.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  3,                                 /* default */
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


my_bool opt_ndb_log_update_as_write;
static MYSQL_SYSVAR_BOOL(
  log_update_as_write,               /* name */
  opt_ndb_log_update_as_write,       /* var */
  PLUGIN_VAR_OPCMDARG,
  "For efficiency log only after image as a write event. "
  "Ignore before image. This may cause compatability problems if "
  "replicating to other storage engines than ndbcluster.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


my_bool opt_ndb_log_updated_only;
static MYSQL_SYSVAR_BOOL(
  log_updated_only,                  /* name */
  opt_ndb_log_updated_only,          /* var */
  PLUGIN_VAR_OPCMDARG,
  "For efficiency log only updated columns. Columns are considered "
  "as \"updated\" even if they are updated with the same value. "
  "This may cause compatability problems if "
  "replicating to other storage engines than ndbcluster.",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  1                                  /* default */
);


my_bool opt_ndb_log_orig;
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


my_bool opt_ndb_log_bin;
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


my_bool opt_ndb_log_binlog_index;
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


static my_bool opt_ndb_log_empty_epochs;
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

my_bool opt_ndb_log_apply_status;
static MYSQL_SYSVAR_BOOL(
  log_apply_status,                 /* name */
  opt_ndb_log_apply_status,         /* var */
  PLUGIN_VAR_OPCMDARG,
  "Log ndb_apply_status updates from Master in the Binlog",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0                                 /* default */
);


my_bool opt_ndb_log_transaction_id;
static MYSQL_SYSVAR_BOOL(
  log_transaction_id,               /* name */
  opt_ndb_log_transaction_id,       /* var  */
  PLUGIN_VAR_OPCMDARG,
  "Log Ndb transaction identities per row in the Binlog",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0                                 /* default */
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

#ifndef DBUG_OFF

static
void
dbg_check_shares_update(THD*, st_mysql_sys_var*, void*, const void*)
{
  sql_print_information("dbug_check_shares open:");
  for (uint i= 0; i < ndbcluster_open_tables.records; i++)
  {
    NDB_SHARE *share= (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables, i);
    sql_print_information("  %s.%s: state: %s(%u) use_count: %u",
                          share->db, share->table_name,
                          get_share_state_string(share->state),
                          (unsigned)share->state,
                          share->use_count);
    DBUG_ASSERT(share->state != NSS_DROPPED);
  }

  sql_print_information("dbug_check_shares dropped:");
  for (uint i= 0; i < ndbcluster_dropped_tables.records; i++)
  {
    NDB_SHARE *share= (NDB_SHARE*)my_hash_element(&ndbcluster_dropped_tables,i);
    sql_print_information("  %s.%s: state: %s(%u) use_count: %u",
                          share->db, share->table_name,
                          get_share_state_string(share->state),
                          (unsigned)share->state,
                          share->use_count);
    DBUG_ASSERT(share->state == NSS_DROPPED);
  }

  /**
   * Only shares in mysql database may be open...
   */
  for (uint i= 0; i < ndbcluster_open_tables.records; i++)
  {
    NDB_SHARE *share= (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables, i);
    DBUG_ASSERT(strcmp(share->db, "mysql") == 0);
  }

  /**
   * Only shares in mysql database may be open...
   */
  for (uint i= 0; i < ndbcluster_dropped_tables.records; i++)
  {
    NDB_SHARE *share= (NDB_SHARE*)my_hash_element(&ndbcluster_dropped_tables,i);
    DBUG_ASSERT(strcmp(share->db, "mysql") == 0);
  }
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

static struct st_mysql_sys_var* system_variables[]= {
  MYSQL_SYSVAR(cache_check_time),
  MYSQL_SYSVAR(extra_logging),
  MYSQL_SYSVAR(wait_connected),
  MYSQL_SYSVAR(wait_setup),
  MYSQL_SYSVAR(cluster_connection_pool),
  MYSQL_SYSVAR(report_thresh_binlog_mem_usage),
  MYSQL_SYSVAR(report_thresh_binlog_epoch_slip),
  MYSQL_SYSVAR(log_update_as_write),
  MYSQL_SYSVAR(log_updated_only),
  MYSQL_SYSVAR(log_orig),
  MYSQL_SYSVAR(distribution),
  MYSQL_SYSVAR(autoincrement_prefetch_sz),
  MYSQL_SYSVAR(force_send),
  MYSQL_SYSVAR(use_exact_count),
  MYSQL_SYSVAR(use_transactions),
  MYSQL_SYSVAR(use_copying_alter_table),
  MYSQL_SYSVAR(optimized_node_selection),
  MYSQL_SYSVAR(batch_size),
  MYSQL_SYSVAR(optimization_delay),
  MYSQL_SYSVAR(index_stat_enable),
  MYSQL_SYSVAR(index_stat_option),
  MYSQL_SYSVAR(index_stat_cache_entries),
  MYSQL_SYSVAR(index_stat_update_freq),
  MYSQL_SYSVAR(table_no_logging),
  MYSQL_SYSVAR(table_temporary),
  MYSQL_SYSVAR(log_bin),
  MYSQL_SYSVAR(log_binlog_index),
  MYSQL_SYSVAR(log_empty_epochs),
  MYSQL_SYSVAR(log_apply_status),
  MYSQL_SYSVAR(log_transaction_id),
  MYSQL_SYSVAR(connectstring),
  MYSQL_SYSVAR(mgmd_host),
  MYSQL_SYSVAR(nodeid),
  MYSQL_SYSVAR(blob_read_batch_bytes),
  MYSQL_SYSVAR(blob_write_batch_bytes),
  MYSQL_SYSVAR(deferred_constraints),
  MYSQL_SYSVAR(join_pushdown),
#ifndef DBUG_OFF
  MYSQL_SYSVAR(dbg_check_shares),
#endif
  MYSQL_SYSVAR(version),
  MYSQL_SYSVAR(version_string),
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
  NULL,                       /* plugin deinit */
  0x0100,                     /* plugin version */
  ndb_status_variables_export,/* status variables                */
  system_variables,           /* system variables */
  NULL,                       /* config options */
  0                           /* flags */
},
ndbinfo_plugin, /* ndbinfo plugin */
/* IS plugin table which maps between mysql connection id and ndb trans-id */
i_s_ndb_transid_mysql_connection_map_plugin
mysql_declare_plugin_end;

#endif
