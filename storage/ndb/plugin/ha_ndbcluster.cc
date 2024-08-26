/* Copyright (c) 2004, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include "storage/ndb/plugin/ha_ndbcluster.h"

#include <algorithm>  // std::min(),std::max()
#include <memory>
#include <sstream>
#include <string>

#include "my_config.h"  // WORDS_BIGENDIAN
#include "my_dbug.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql/strings/m_ctype.h"
#include "nulls.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/derror.h"      // ER_THD
#include "sql/filesort.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/mysqld.h"  // global_system_variables table_alias_charset ...
#include "sql/partition_info.h"
#include "sql/sql_alter.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"  // QEP_TAB
#include "sql/sql_lex.h"
#include "sql/sql_plugin_var.h"  // SYS_VAR
#include "sql/transaction.h"
#ifndef NDEBUG
#include "sql/sql_test.h"  // print_where
#endif
#include "sql/strfunc.h"
#include "storage/ndb/include/ndb_global.h"
#include "storage/ndb/include/ndb_version.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/util/SparseBitmask.hpp"
#include "storage/ndb/plugin/ha_ndb_index_stat.h"
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"
#include "storage/ndb/plugin/ha_ndbcluster_cond.h"
#include "storage/ndb/plugin/ha_ndbcluster_connection.h"
#include "storage/ndb/plugin/ha_ndbcluster_push.h"
#include "storage/ndb/plugin/ndb_anyvalue.h"
#include "storage/ndb/plugin/ndb_applier.h"
#include "storage/ndb/plugin/ndb_binlog_client.h"
#include "storage/ndb/plugin/ndb_binlog_extra_row_info.h"
#include "storage/ndb/plugin/ndb_binlog_thread.h"
#include "storage/ndb/plugin/ndb_bitmap.h"
#include "storage/ndb/plugin/ndb_conflict.h"
#include "storage/ndb/plugin/ndb_conflict_trans.h"  // DependencyTracker
#include "storage/ndb/plugin/ndb_create_helper.h"
#include "storage/ndb/plugin/ndb_dd.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_dd_disk_data.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_ddl_definitions.h"
#include "storage/ndb/plugin/ndb_ddl_transaction_ctx.h"
#include "storage/ndb/plugin/ndb_dist_priv_util.h"
#include "storage/ndb/plugin/ndb_dummy_ts.h"
#include "storage/ndb/plugin/ndb_event_data.h"
#include "storage/ndb/plugin/ndb_fk_util.h"
#include "storage/ndb/plugin/ndb_global_schema_lock.h"
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_metadata.h"
#include "storage/ndb/plugin/ndb_metadata_change_monitor.h"
#include "storage/ndb/plugin/ndb_metadata_sync.h"
#include "storage/ndb/plugin/ndb_modifiers.h"
#include "storage/ndb/plugin/ndb_mysql_services.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_ndbapi_errors.h"
#include "storage/ndb/plugin/ndb_pfs_init.h"
#include "storage/ndb/plugin/ndb_replica.h"
#include "storage/ndb/plugin/ndb_require.h"
#include "storage/ndb/plugin/ndb_schema_dist.h"
#include "storage/ndb/plugin/ndb_schema_trans_guard.h"
#include "storage/ndb/plugin/ndb_server_hooks.h"
#include "storage/ndb/plugin/ndb_sleep.h"
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "storage/ndb/plugin/ndb_table_stats.h"
#include "storage/ndb/plugin/ndb_tdc.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/src/common/util/parse_mask.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryBuilder.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryOperation.hpp"
#include "string_with_len.h"
#include "strxnmov.h"
#include "template_utils.h"

typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Dictionary NDBDICT;

// ndb interface initialization/cleanup
extern "C" void ndb_init_internal(Uint32);
extern "C" void ndb_end_internal(Uint32);

static const int DEFAULT_PARALLELISM = 0;
static const ha_rows DEFAULT_AUTO_PREFETCH = 32;
static const ulong ONE_YEAR_IN_SECONDS = (ulong)3600L * 24L * 365L;

static constexpr unsigned DEFAULT_REPLICA_BATCH_SIZE = 2UL * 1024 * 1024;
static constexpr unsigned MAX_BLOB_ROW_SIZE = 14000;
static constexpr unsigned DEFAULT_MAX_BLOB_PART_SIZE =
    MAX_BLOB_ROW_SIZE - 4 * 13;

ulong opt_ndb_extra_logging;
static ulong opt_ndb_wait_connected;
static ulong opt_ndb_wait_setup;
static ulong opt_ndb_replica_batch_size;
static uint opt_ndb_replica_blob_write_batch_bytes;
static uint opt_ndb_cluster_connection_pool;
static char *opt_connection_pool_nodeids_str;
static uint opt_ndb_recv_thread_activation_threshold;
static char *opt_ndb_recv_thread_cpu_mask;
static char *opt_ndb_index_stat_option;
static char *opt_ndb_connectstring;
static uint opt_ndb_nodeid;
static bool opt_ndb_read_backup;
static ulong opt_ndb_data_node_neighbour;
static bool opt_ndb_fully_replicated;
static ulong opt_ndb_row_checksum;

char *opt_ndb_tls_search_path;
ulong opt_ndb_mgm_tls_level;

// The version where ndbcluster uses DYNAMIC by default when creating columns
static const ulong NDB_VERSION_DYNAMIC_IS_DEFAULT = 50711;
enum ndb_default_colum_format_enum {
  NDB_DEFAULT_COLUMN_FORMAT_FIXED = 0,
  NDB_DEFAULT_COLUMN_FORMAT_DYNAMIC = 1
};
static const char *default_column_format_names[] = {"FIXED", "DYNAMIC", NullS};
static ulong opt_ndb_default_column_format;
static TYPELIB default_column_format_typelib = {
    array_elements(default_column_format_names) - 1, "",
    default_column_format_names, nullptr};
static MYSQL_SYSVAR_ENUM(
    default_column_format,         /* name */
    opt_ndb_default_column_format, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Change COLUMN_FORMAT default value (fixed or dynamic) "
    "for backward compatibility. Also affects the default value "
    "of ROW_FORMAT.",
    nullptr,                         /* check func. */
    nullptr,                         /* update func. */
    NDB_DEFAULT_COLUMN_FORMAT_FIXED, /* default */
    &default_column_format_typelib   /* typelib */
);

static MYSQL_THDVAR_UINT(
    autoincrement_prefetch_sz, /* name */
    PLUGIN_VAR_RQCMDARG,
    "Specify number of autoincrement values that are prefetched.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    512,     /* default */
    1,       /* min */
    65535,   /* max */
    0        /* block */
);

static MYSQL_THDVAR_BOOL(
    force_send, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Force send of buffers to ndb immediately without waiting for "
    "other threads.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

static MYSQL_THDVAR_BOOL(
    use_exact_count, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Use exact records count during query planning and for fast "
    "select count(*), disable for faster queries.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

static MYSQL_THDVAR_BOOL(
    use_transactions, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Use transactions for large inserts, if enabled then large "
    "inserts will be split into several smaller transactions",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

static MYSQL_THDVAR_BOOL(
    use_copying_alter_table, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Force ndbcluster to always copy tables at alter table (should "
    "only be used if online alter table fails).",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

static MYSQL_THDVAR_BOOL(
    allow_copying_alter_table, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Specifies if implicit copying alter table is allowed. Can be overridden "
    "by using ALGORITHM=COPY in the alter table command.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

/**
   @brief Determine if copying alter table is allowed for current query

   @param thd Pointer to current THD
   @return true if allowed
 */
static bool is_copying_alter_table_allowed(THD *thd) {
  if (THDVAR(thd, allow_copying_alter_table)) {
    //  Copying alter table is allowed
    return true;
  }
  if (thd->lex->alter_info->requested_algorithm ==
      Alter_info::ALTER_TABLE_ALGORITHM_COPY) {
    // User have specified ALGORITHM=COPY, thus overriding the fact that
    // --ndb-allow-copying-alter-table is OFF
    return true;
  }
  return false;
}

static MYSQL_THDVAR_UINT(optimized_node_selection, /* name */
                         PLUGIN_VAR_OPCMDARG,
                         "Select nodes for transactions in a more optimal way.",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         3,       /* default */
                         0,       /* min */
                         3,       /* max */
                         0        /* block */
);

static MYSQL_THDVAR_ULONG(batch_size, /* name */
                          PLUGIN_VAR_RQCMDARG, "Batch size in bytes.",
                          nullptr,                  /* check func. */
                          nullptr,                  /* update func. */
                          32768,                    /* default */
                          0,                        /* min */
                          2UL * 1024 * 1024 * 1024, /* max */
                          0                         /* block */
);

static MYSQL_THDVAR_ULONG(
    optimization_delay, /* name */
    PLUGIN_VAR_RQCMDARG,
    "For optimize table, specifies the delay in milliseconds "
    "for each batch of rows sent.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    10,      /* default */
    0,       /* min */
    100000,  /* max */
    0        /* block */
);

static MYSQL_THDVAR_BOOL(index_stat_enable, /* name */
                         PLUGIN_VAR_OPCMDARG,
                         "Use ndb index statistics in query optimization.",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         true     /* default */
);

static MYSQL_THDVAR_BOOL(table_no_logging,                 /* name */
                         PLUGIN_VAR_NOCMDARG, "", nullptr, /* check func. */
                         nullptr,                          /* update func. */
                         false                             /* default */
);

static MYSQL_THDVAR_BOOL(table_temporary,                  /* name */
                         PLUGIN_VAR_NOCMDARG, "", nullptr, /* check func. */
                         nullptr,                          /* update func. */
                         false                             /* default */
);

static MYSQL_THDVAR_UINT(blob_read_batch_bytes, /* name */
                         PLUGIN_VAR_RQCMDARG,
                         "Specifies the bytesize large Blob reads "
                         "should be batched into.  0 == No limit.",
                         nullptr,  /* check func */
                         nullptr,  /* update func */
                         65536,    /* default */
                         0,        /* min */
                         UINT_MAX, /* max */
                         0         /* block */
);

static MYSQL_THDVAR_UINT(blob_write_batch_bytes, /* name */
                         PLUGIN_VAR_RQCMDARG,
                         "Specifies the bytesize large Blob writes "
                         "should be batched into.  0 == No limit.",
                         nullptr,  /* check func */
                         nullptr,  /* update func */
                         65536,    /* default */
                         0,        /* min */
                         UINT_MAX, /* max */
                         0         /* block */
);

static MYSQL_THDVAR_UINT(
    deferred_constraints, /* name */
    PLUGIN_VAR_RQCMDARG,
    "Specified that constraints should be checked deferred (when supported)",
    nullptr, /* check func */
    nullptr, /* update func */
    0,       /* default */
    0,       /* min */
    1,       /* max */
    0        /* block */
);

static MYSQL_THDVAR_BOOL(
    show_foreign_key_mock_tables, /* name */
    PLUGIN_VAR_OPCMDARG,
    "Show the mock tables which is used to support foreign_key_checks= 0. "
    "Extra info warnings are shown when creating and dropping the tables. "
    "The real table name is show in SHOW CREATE TABLE",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

static MYSQL_THDVAR_BOOL(join_pushdown, /* name */
                         PLUGIN_VAR_OPCMDARG,
                         "Enable pushing down of join to datanodes",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         true     /* default */
);

static MYSQL_THDVAR_BOOL(log_exclusive_reads, /* name */
                         PLUGIN_VAR_OPCMDARG,
                         "Log primary key reads with exclusive locks "
                         "to allow conflict resolution based on read conflicts",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

/*
  Required in index_stat.cc but available only from here
  thanks to use of top level anonymous structs.
*/
bool ndb_index_stat_get_enable(THD *thd) {
  const bool value = THDVAR(thd, index_stat_enable);
  return value;
}

bool ndb_show_foreign_key_mock_tables(THD *thd) {
  const bool value = THDVAR(thd, show_foreign_key_mock_tables);
  return value;
}

int ndbcluster_push_to_engine(THD *, AccessPath *, JOIN *);

static bool inplace_ndb_column_comment_changed(std::string_view old_comment,
                                               std::string_view new_comment,
                                               const char **reason);

static int ndbcluster_end(handlerton *, ha_panic_function);
static bool ndbcluster_show_status(handlerton *, THD *, stat_print_fn *,
                                   enum ha_stat_type);

static int ndbcluster_get_tablespace(THD *thd, LEX_CSTRING db_name,
                                     LEX_CSTRING table_name,
                                     LEX_CSTRING *tablespace_name);
static int ndbcluster_alter_tablespace(handlerton *, THD *thd,
                                       st_alter_tablespace *info,
                                       const dd::Tablespace *,
                                       dd::Tablespace *);
static bool ndbcluster_get_tablespace_statistics(
    const char *tablespace_name, const char *file_name,
    const dd::Properties &ts_se_private_data, ha_tablespace_statistics *stats);
static void ndbcluster_pre_dd_shutdown(handlerton *);

static handler *ndbcluster_create_handler(handlerton *hton, TABLE_SHARE *table,
                                          bool /* partitioned */,
                                          MEM_ROOT *mem_root) {
  return new (mem_root) ha_ndbcluster(hton, table);
}

static uint ndbcluster_partition_flags() {
  return (HA_CAN_UPDATE_PARTITION_KEY | HA_CAN_PARTITION_UNIQUE |
          HA_USE_AUTO_PARTITION);
}

uint ha_ndbcluster::alter_flags(uint flags) const {
  const uint f = HA_PARTITION_FUNCTION_SUPPORTED | 0;

  if (flags & Alter_info::ALTER_DROP_PARTITION) return 0;

  return f;
}

static constexpr uint NDB_AUTO_INCREMENT_RETRIES = 100;

#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("%d  message: %s", err.code, err.message))

#define ERR_RETURN(err)              \
  {                                  \
    const NdbError &tmp = err;       \
    return ndb_to_mysql_error(&tmp); \
  }

#define ERR_SET(err, code)           \
  {                                  \
    const NdbError &tmp = err;       \
    code = ndb_to_mysql_error(&tmp); \
  }

static int ndbcluster_inited = 0;

extern Ndb *g_ndb;
extern Ndb_cluster_connection *g_ndb_cluster_connection;

static const char *ndbcluster_hton_name = "ndbcluster";
static const int ndbcluster_hton_name_length = sizeof(ndbcluster_hton_name) - 1;

static ulong multi_range_fixed_size(int num_ranges);

static ulong multi_range_max_entry(NDB_INDEX_TYPE keytype, ulong reclength);

struct st_ndb_status {
  st_ndb_status() { memset(this, 0, sizeof(struct st_ndb_status)); }
  long cluster_node_id;
  const char *connected_host;
  long connected_port;
  long config_generation;
  long number_of_data_nodes;
  long number_of_ready_data_nodes;
  long connect_count;
  long execute_count;
  long trans_hint_count;
  long scan_count;
  long pruned_scan_count;
  long schema_locks_count;
  long sorted_scan_count;
  long pushed_queries_defined;
  long pushed_queries_dropped;
  long pushed_queries_executed;
  long pushed_reads;
  long long last_commit_epoch_server;
  long long last_commit_epoch_session;
  long long api_client_stats[Ndb::NumClientStatistics];
  const char *system_name;
  long fetch_table_stats;
};

/* Status variables shown with 'show status like 'Ndb%' */
static st_ndb_status g_ndb_status;

static long long g_server_api_client_stats[Ndb::NumClientStatistics];

static int update_status_variables(Thd_ndb *thd_ndb, st_ndb_status *ns,
                                   Ndb_cluster_connection *c) {
  ns->connected_port = c->get_connected_port();
  ns->connected_host = c->get_connected_host();
  if (ns->cluster_node_id != (int)c->node_id()) {
    ns->cluster_node_id = c->node_id();
    if (&g_ndb_status == ns && g_ndb_cluster_connection == c)
      ndb_log_info("NodeID is %lu, management server '%s:%lu'",
                   ns->cluster_node_id, ns->connected_host, ns->connected_port);
  }
  {
    int n = c->get_no_ready();
    ns->number_of_ready_data_nodes = n > 0 ? n : 0;
  }
  ns->config_generation = c->get_config_generation();
  ns->number_of_data_nodes = c->no_db_nodes();
  ns->connect_count = c->get_connect_count();
  ns->system_name = c->get_system_name();
  ns->last_commit_epoch_server = ndb_get_latest_trans_gci();
  if (thd_ndb) {
    ns->execute_count = thd_ndb->m_execute_count;
    ns->trans_hint_count = thd_ndb->hinted_trans_count();
    ns->scan_count = thd_ndb->m_scan_count;
    ns->pruned_scan_count = thd_ndb->m_pruned_scan_count;
    ns->sorted_scan_count = thd_ndb->m_sorted_scan_count;
    ns->pushed_queries_defined = thd_ndb->m_pushed_queries_defined;
    ns->pushed_queries_dropped = thd_ndb->m_pushed_queries_dropped;
    ns->pushed_queries_executed = thd_ndb->m_pushed_queries_executed;
    ns->pushed_reads = thd_ndb->m_pushed_reads;
    ns->last_commit_epoch_session = thd_ndb->m_last_commit_epoch_session;
    for (int i = 0; i < Ndb::NumClientStatistics; i++) {
      ns->api_client_stats[i] = thd_ndb->ndb->getClientStat(i);
    }
    ns->schema_locks_count = thd_ndb->schema_locks_count;
    ns->fetch_table_stats = thd_ndb->m_fetch_table_stats;
  }
  return 0;
}

/* Helper macro for definitions of NdbApi status variables */

#define NDBAPI_COUNTERS(NAME_SUFFIX, ARRAY_LOCATION)                          \
  {"api_wait_exec_complete_count" NAME_SUFFIX,                                \
   (char *)ARRAY_LOCATION[Ndb::WaitExecCompleteCount], SHOW_LONGLONG,         \
   SHOW_SCOPE_GLOBAL},                                                        \
      {"api_wait_scan_result_count" NAME_SUFFIX,                              \
       (char *)ARRAY_LOCATION[Ndb::WaitScanResultCount], SHOW_LONGLONG,       \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_wait_meta_request_count" NAME_SUFFIX,                             \
       (char *)ARRAY_LOCATION[Ndb::WaitMetaRequestCount], SHOW_LONGLONG,      \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_wait_nanos_count" NAME_SUFFIX,                                    \
       (char *)ARRAY_LOCATION[Ndb::WaitNanosCount], SHOW_LONGLONG,            \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_bytes_sent_count" NAME_SUFFIX,                                    \
       (char *)ARRAY_LOCATION[Ndb::BytesSentCount], SHOW_LONGLONG,            \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_bytes_received_count" NAME_SUFFIX,                                \
       (char *)ARRAY_LOCATION[Ndb::BytesRecvdCount], SHOW_LONGLONG,           \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_trans_start_count" NAME_SUFFIX,                                   \
       (char *)ARRAY_LOCATION[Ndb::TransStartCount], SHOW_LONGLONG,           \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_trans_commit_count" NAME_SUFFIX,                                  \
       (char *)ARRAY_LOCATION[Ndb::TransCommitCount], SHOW_LONGLONG,          \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_trans_abort_count" NAME_SUFFIX,                                   \
       (char *)ARRAY_LOCATION[Ndb::TransAbortCount], SHOW_LONGLONG,           \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_trans_close_count" NAME_SUFFIX,                                   \
       (char *)ARRAY_LOCATION[Ndb::TransCloseCount], SHOW_LONGLONG,           \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_pk_op_count" NAME_SUFFIX, (char *)ARRAY_LOCATION[Ndb::PkOpCount], \
       SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                     \
      {"api_uk_op_count" NAME_SUFFIX, (char *)ARRAY_LOCATION[Ndb::UkOpCount], \
       SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},                                     \
      {"api_table_scan_count" NAME_SUFFIX,                                    \
       (char *)ARRAY_LOCATION[Ndb::TableScanCount], SHOW_LONGLONG,            \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_range_scan_count" NAME_SUFFIX,                                    \
       (char *)ARRAY_LOCATION[Ndb::RangeScanCount], SHOW_LONGLONG,            \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_pruned_scan_count" NAME_SUFFIX,                                   \
       (char *)ARRAY_LOCATION[Ndb::PrunedScanCount], SHOW_LONGLONG,           \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_scan_batch_count" NAME_SUFFIX,                                    \
       (char *)ARRAY_LOCATION[Ndb::ScanBatchCount], SHOW_LONGLONG,            \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_read_row_count" NAME_SUFFIX,                                      \
       (char *)ARRAY_LOCATION[Ndb::ReadRowCount], SHOW_LONGLONG,              \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_trans_local_read_row_count" NAME_SUFFIX,                          \
       (char *)ARRAY_LOCATION[Ndb::TransLocalReadRowCount], SHOW_LONGLONG,    \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_adaptive_send_forced_count" NAME_SUFFIX,                          \
       (char *)ARRAY_LOCATION[Ndb::ForcedSendsCount], SHOW_LONGLONG,          \
       SHOW_SCOPE_GLOBAL},                                                    \
      {"api_adaptive_send_unforced_count" NAME_SUFFIX,                        \
       (char *)ARRAY_LOCATION[Ndb::UnforcedSendsCount], SHOW_LONGLONG,        \
       SHOW_SCOPE_GLOBAL},                                                    \
  {                                                                           \
    "api_adaptive_send_deferred_count" NAME_SUFFIX,                           \
        (char *)ARRAY_LOCATION[Ndb::DeferredSendsCount], SHOW_LONGLONG,       \
        SHOW_SCOPE_GLOBAL                                                     \
  }

static SHOW_VAR ndb_status_vars_dynamic[] = {
    {"cluster_node_id", (char *)&g_ndb_status.cluster_node_id, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"config_from_host", (char *)&g_ndb_status.connected_host, SHOW_CHAR_PTR,
     SHOW_SCOPE_GLOBAL},
    {"config_from_port", (char *)&g_ndb_status.connected_port, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"config_generation", (char *)&g_ndb_status.config_generation, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"number_of_data_nodes", (char *)&g_ndb_status.number_of_data_nodes,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"number_of_ready_data_nodes",
     (char *)&g_ndb_status.number_of_ready_data_nodes, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"connect_count", (char *)&g_ndb_status.connect_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"execute_count", (char *)&g_ndb_status.execute_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"scan_count", (char *)&g_ndb_status.scan_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"pruned_scan_count", (char *)&g_ndb_status.pruned_scan_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"schema_locks_count", (char *)&g_ndb_status.schema_locks_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    NDBAPI_COUNTERS("_session", &g_ndb_status.api_client_stats),
    {"trans_hint_count_session",
     reinterpret_cast<char *>(&g_ndb_status.trans_hint_count), SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"sorted_scan_count", (char *)&g_ndb_status.sorted_scan_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"pushed_queries_defined", (char *)&g_ndb_status.pushed_queries_defined,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"pushed_queries_dropped", (char *)&g_ndb_status.pushed_queries_dropped,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"pushed_queries_executed", (char *)&g_ndb_status.pushed_queries_executed,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"pushed_reads", (char *)&g_ndb_status.pushed_reads, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"last_commit_epoch_server", (char *)&g_ndb_status.last_commit_epoch_server,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"last_commit_epoch_session",
     (char *)&g_ndb_status.last_commit_epoch_session, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"system_name", (char *)&g_ndb_status.system_name, SHOW_CHAR_PTR,
     SHOW_SCOPE_GLOBAL},
    {"fetch_table_stats", (char *)&g_ndb_status.fetch_table_stats, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

// Global instance of stats for the default replication channel, populated
// from Ndb_replica when the channel state changes
static Ndb_replica::Channel_stats g_default_channel_stats;
// List of status variables for the default replication channel
static SHOW_VAR ndb_status_vars_replica[] = {
    NDBAPI_COUNTERS("_slave", &g_default_channel_stats.api_stats),
    NDBAPI_COUNTERS("_replica", &g_default_channel_stats.api_stats),
    {"slave_max_replicated_epoch",
     (char *)&g_default_channel_stats.max_rep_epoch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"replica_max_replicated_epoch",
     (char *)&g_default_channel_stats.max_rep_epoch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_fn_max",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_MAX],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_old",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_OLD],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_max_del_win",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_MAX_DEL_WIN],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_max_ins",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_MAX_INS],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_max_del_win_ins",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_MAX_DEL_WIN_INS],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_epoch",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_EPOCH],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_epoch_trans",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_EPOCH_TRANS],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_epoch2",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_EPOCH2],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_fn_epoch2_trans",
     (char *)&g_default_channel_stats.violation_count[CFT_NDB_EPOCH2_TRANS],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_trans_row_conflict_count",
     (char *)&g_default_channel_stats.trans_row_conflict_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_trans_row_reject_count",
     (char *)&g_default_channel_stats.trans_row_reject_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_trans_reject_count",
     (char *)&g_default_channel_stats.trans_in_conflict_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_trans_detect_iter_count",
     (char *)&g_default_channel_stats.trans_detect_iter_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_trans_conflict_commit_count",
     (char *)&g_default_channel_stats.trans_conflict_commit_count,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"conflict_epoch_delete_delete_count",
     (char *)&g_default_channel_stats.delete_delete_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_reflected_op_prepare_count",
     (char *)&g_default_channel_stats.reflect_op_prepare_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_reflected_op_discard_count",
     (char *)&g_default_channel_stats.reflect_op_discard_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_refresh_op_count",
     (char *)&g_default_channel_stats.refresh_op_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_last_conflict_epoch",
     (char *)&g_default_channel_stats.last_conflicted_epoch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"conflict_last_stable_epoch",
     (char *)&g_default_channel_stats.last_stable_epoch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},

    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

static SHOW_VAR ndb_status_vars_server_api[] = {
    NDBAPI_COUNTERS("", &g_server_api_client_stats),
    {"api_event_data_count",
     (char *)&g_server_api_client_stats[Ndb::DataEventsRecvdCount],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"api_event_nondata_count",
     (char *)&g_server_api_client_stats[Ndb::NonDataEventsRecvdCount],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"api_event_bytes_count",
     (char *)&g_server_api_client_stats[Ndb::EventBytesRecvdCount],
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

/*
   Called when SHOW STATUS or performance_schema.[global|session]_status
   wants to see the status variables. We use this opportunity to:
   1) Update the globals with current values
   2) Return an array of var definitions, pointing to
      the updated globals
*/

static int show_ndb_status_server_api(THD *, SHOW_VAR *var, char *) {
  ndb_get_connection_stats((Uint64 *)&g_server_api_client_stats[0]);

  var->type = SHOW_ARRAY;
  var->value = (char *)ndb_status_vars_server_api;
  var->scope = SHOW_SCOPE_GLOBAL;

  return 0;
}

/*
  Error handling functions
*/

/* Note for merge: old mapping table, moved to storage/ndb/ndberror.c */

int ndb_to_mysql_error(const NdbError *ndberr) {
  /* read the mysql mapped error code */
  int error = ndberr->mysql_code;

  switch (error) {
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
        error = HA_ERR_INTERNAL_ERROR;
      else
        error = ndberr->code;
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
      push_warning_printf(current_thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                          ER_THD(current_thd, ER_GET_ERRMSG), ndberr->code,
                          ndberr->message, "NDB");
  }
  return error;
}

ulong opt_ndb_slave_conflict_role;
ulong opt_ndb_applier_conflict_role;

static int handle_conflict_op_error(Ndb_applier *const applier,
                                    NdbTransaction *trans, const NdbError &err,
                                    const NdbOperation *op);

static bool ndbcluster_notify_alter_table(THD *, const MDL_key *,
                                          ha_notification_type);

static bool ndbcluster_notify_exclusive_mdl(THD *, const MDL_key *,
                                            ha_notification_type, bool *);

static int handle_row_conflict(
    Ndb_applier *const applier, NDB_CONFLICT_FN_SHARE *cfn_share,
    const char *tab_name, const char *handling_type, const NdbRecord *key_rec,
    const NdbRecord *data_rec, const uchar *old_row, const uchar *new_row,
    enum_conflicting_op_type op_type, enum_conflict_cause conflict_cause,
    const NdbError &conflict_error, NdbTransaction *conflict_trans,
    const MY_BITMAP *write_set, Uint64 transaction_id);

// Error code returned when "refresh occurs on a refreshed row"
static constexpr int ERROR_OP_AFTER_REFRESH_OP = 920;

static inline int check_completed_operations_pre_commit(
    Thd_ndb *thd_ndb, NdbTransaction *trans, const NdbOperation *first,
    const NdbOperation *last, uint *ignore_count) {
  uint ignores = 0;
  DBUG_TRACE;

  if (unlikely(first == nullptr)) {
    assert(last == nullptr);
    return 0;
  }

  /*
    Check that all errors are "accepted" errors
    or exceptions to report
  */
  const NdbOperation *lastUserOp = trans->getLastDefinedOperation();
  while (true) {
    const NdbError &err = first->getNdbError();
    const bool op_has_conflict_detection = (first->getCustomData() != nullptr);
    if (!op_has_conflict_detection) {
      assert(err.code != ERROR_OP_AFTER_REFRESH_OP);

      /* 'Normal path' - ignore key (not) present, others are errors */
      if (err.classification != NdbError::NoError &&
          err.classification != NdbError::ConstraintViolation &&
          err.classification != NdbError::NoDataFound) {
        /* Non ignored error, report it */
        DBUG_PRINT("info", ("err.code == %u", err.code));
        return err.code;
      }
    } else {
      /*
         Op with conflict detection, use special error handling method
       */

      if (err.classification != NdbError::NoError) {
        const int res =
            handle_conflict_op_error(thd_ndb->get_applier(), trans, err, first);
        if (res != 0) {
          return res;
        }
      }
    }  // if (!op_has_conflict_detection)
    if (err.classification != NdbError::NoError) ignores++;

    if (first == last) break;

    first = trans->getNextCompletedOperation(first);
  }
  if (ignore_count) *ignore_count = ignores;

  /*
     Conflict detection related error handling above may have defined
     new operations on the transaction.  If so, execute them now
  */
  if (trans->getLastDefinedOperation() != lastUserOp) {
    const NdbOperation *last_conflict_op = trans->getLastDefinedOperation();

    NdbError nonMaskedError;
    assert(nonMaskedError.code == 0);

    if (trans->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError,
                       thd_ndb->m_force_send)) {
      /* Transaction execute failed, even with IgnoreError... */
      nonMaskedError = trans->getNdbError();
      assert(nonMaskedError.code != 0);
    } else if (trans->getNdbError().code) {
      /* Check the result codes of the operations we added */
      const NdbOperation *conflict_op = nullptr;
      do {
        conflict_op = trans->getNextCompletedOperation(conflict_op);
        assert(conflict_op != nullptr);
        // Ignore 920 (ERROR_OP_AFTER_REFRESH_OP) which represents a refreshOp
        // or other op arriving after a refreshOp
        const NdbError &err = conflict_op->getNdbError();
        if (err.code != 0 && err.code != ERROR_OP_AFTER_REFRESH_OP) {
          /* Found a real error, break out and handle it */
          nonMaskedError = err;
          break;
        }
      } while (conflict_op != last_conflict_op);
    }

    /* Handle errors with extra conflict handling operations */
    if (nonMaskedError.code != 0) {
      if (nonMaskedError.status == NdbError::TemporaryError) {
        /* Slave will roll back and retry entire transaction. */
        ERR_RETURN(nonMaskedError);
      } else {
        thd_ndb->push_ndb_error_warning(nonMaskedError);
        thd_ndb->push_warning(
            ER_EXCEPTIONS_WRITE_ERROR,
            ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR),
            "Failed executing extra operations for conflict handling");
        /* Slave will stop replication. */
        return ER_EXCEPTIONS_WRITE_ERROR;
      }
    }
  }
  return 0;
}

static inline int check_completed_operations(NdbTransaction *trans,
                                             const NdbOperation *first,
                                             const NdbOperation *last,
                                             uint *ignore_count) {
  uint ignores = 0;
  DBUG_TRACE;

  if (unlikely(first == nullptr)) {
    assert(last == nullptr);
    return 0;
  }

  /*
    Check that all errors are "accepted" errors
  */
  while (true) {
    const NdbError &err = first->getNdbError();
    if (err.classification != NdbError::NoError &&
        err.classification != NdbError::ConstraintViolation &&
        err.classification != NdbError::NoDataFound) {
      /* All conflict detection etc should be done before commit */
      assert(err.code != ERROR_CONFLICT_FN_VIOLATION &&
             err.code != ERROR_OP_AFTER_REFRESH_OP);
      return err.code;
    }
    if (err.classification != NdbError::NoError) ignores++;

    if (first == last) break;

    first = trans->getNextCompletedOperation(first);
  }
  if (ignore_count) *ignore_count = ignores;
  return 0;
}

static inline int execute_no_commit(Thd_ndb *thd_ndb, NdbTransaction *trans,
                                    bool ignore_no_key,
                                    uint *ignore_count = nullptr) {
  DBUG_TRACE;

  trans->releaseCompletedOpsAndQueries();

  const NdbOperation *first = trans->getFirstDefinedOperation();
  const NdbOperation *last = trans->getLastDefinedOperation();
  thd_ndb->m_execute_count++;
  thd_ndb->m_unsent_bytes = 0;
  thd_ndb->m_unsent_blob_ops = false;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  int rc = 0;
  do {
    if (trans->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError,
                       thd_ndb->m_force_send)) {
      rc = -1;
      break;
    }
    if (!ignore_no_key || trans->getNdbError().code == 0) {
      rc = trans->getNdbError().code;
      break;
    }

    rc = check_completed_operations_pre_commit(thd_ndb, trans, first, last,
                                               ignore_count);
  } while (0);

  if (unlikely(rc != 0)) {
    Ndb_applier *const applier = thd_ndb->get_applier();
    if (applier) {
      applier->atTransactionAbort();
    }
  }

  DBUG_PRINT("info", ("execute_no_commit rc is %d", rc));
  return rc;
}

static inline int execute_commit(Thd_ndb *thd_ndb, NdbTransaction *trans,
                                 int force_send, int ignore_error,
                                 uint *ignore_count = nullptr) {
  DBUG_TRACE;
  NdbOperation::AbortOption ao = NdbOperation::AO_IgnoreError;
  if (thd_ndb->m_unsent_bytes && !ignore_error) {
    /*
      We have unsent bytes and cannot ignore error.  Calling execute
      with NdbOperation::AO_IgnoreError will result in possible commit
      of a transaction although there is an error.
    */
    ao = NdbOperation::AbortOnError;
  }
  const NdbOperation *first = trans->getFirstDefinedOperation();
  const NdbOperation *last = trans->getLastDefinedOperation();
  thd_ndb->m_execute_count++;
  thd_ndb->m_unsent_bytes = 0;
  thd_ndb->m_unsent_blob_ops = false;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  int rc = 0;
  do {
    if (trans->execute(NdbTransaction::Commit, ao, force_send)) {
      rc = -1;
      break;
    }

    if (!ignore_error || trans->getNdbError().code == 0) {
      rc = trans->getNdbError().code;
      break;
    }

    rc = check_completed_operations(trans, first, last, ignore_count);
  } while (0);

  if (likely(rc == 0)) {
    /* Committed ok, update session GCI, if it's available
     * (Not available for reads, empty transactions etc...)
     */
    Uint64 reportedGCI;
    if (trans->getGCI(&reportedGCI) == 0 && reportedGCI != 0) {
      assert(reportedGCI >= thd_ndb->m_last_commit_epoch_session);
      thd_ndb->m_last_commit_epoch_session = reportedGCI;
    }
  }

  Ndb_applier *const applier = thd_ndb->get_applier();
  if (applier) {
    if (likely(rc == 0)) {
      /* Success */
      applier->atTransactionCommit(thd_ndb->m_last_commit_epoch_session);
    } else {
      applier->atTransactionAbort();
    }
  }

  DBUG_PRINT("info", ("execute_commit rc is %d", rc));
  return rc;
}

static inline int execute_no_commit_ie(Thd_ndb *thd_ndb,
                                       NdbTransaction *trans) {
  DBUG_TRACE;

  trans->releaseCompletedOpsAndQueries();

  const int res =
      trans->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError,
                     thd_ndb->m_force_send);
  thd_ndb->m_unsent_bytes = 0;
  thd_ndb->m_execute_count++;
  thd_ndb->m_unsent_blob_ops = false;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  return res;
}

Thd_ndb::Thd_ndb(THD *thd, const char *name)
    : m_thd(thd),
      options(0),
      trans_options(0),
      m_ddl_ctx(nullptr),
      m_thread_name(name),
      m_batch_mem_root(key_memory_thd_ndb_batch_mem_root,
                       BATCH_MEM_ROOT_BLOCK_SIZE),
      global_schema_lock_trans(nullptr),
      global_schema_lock_count(0),
      global_schema_lock_error(0),
      schema_locks_count(0),
      m_last_commit_epoch_session(0) {
  connection = ndb_get_cluster_connection();
  m_connect_count = connection->get_connect_count();
  ndb = new Ndb(connection, "");
  save_point_count = 0;
  trans = nullptr;
  m_handler = nullptr;
  m_unsent_bytes = 0;
  m_unsent_blob_ops = false;
  m_execute_count = 0;
  m_scan_count = 0;
  m_pruned_scan_count = 0;
  m_sorted_scan_count = 0;
  m_pushed_queries_defined = 0;
  m_pushed_queries_dropped = 0;
  m_pushed_queries_executed = 0;
  m_pushed_reads = 0;
}

Thd_ndb::~Thd_ndb() {
  assert(global_schema_lock_count == 0);
  assert(m_ddl_ctx == nullptr);

  // The applier uses the Ndb object when removing its NdbApi table from dict
  // cache, release applier first
  m_applier.reset();

  delete ndb;

  m_batch_mem_root.Clear();
}

void ha_ndbcluster::set_rec_per_key(THD *thd) {
  DBUG_TRACE;
  /*
    Set up the 'records per key' value for keys which there are good knowledge
    about the distribution. The default value for 'records per key' is otherwise
    0 (interpreted as 'unknown' by optimizer), which would force the optimizer
    to use its own heuristic to estimate 'records per key'.
  */
  for (uint i = 0; i < table_share->keys; i++) {
    KEY *const key_info = table->key_info + i;
    switch (m_index[i].type) {
      case UNIQUE_INDEX:
      case PRIMARY_KEY_INDEX: {
        // Index is unique when all 'key_parts' are specified,
        // else distribution is unknown and not specified here.

        // Set 'records per key' to 1 for complete key given
        key_info->set_records_per_key(key_info->user_defined_key_parts - 1,
                                      1.0F);
        break;
      }
      case UNIQUE_ORDERED_INDEX:
      case PRIMARY_KEY_ORDERED_INDEX:
        // Set 'records per key' to 1 for complete key given
        key_info->set_records_per_key(key_info->user_defined_key_parts - 1,
                                      1.0F);

        // intentional fall thru to logic for ordered index
        [[fallthrough]];
      case ORDERED_INDEX:
        // 'records per key' are unknown for non-unique indexes (may change when
        // we get better index statistics).

        {
          const bool index_stat_enable = ndb_index_stat_get_enable(nullptr) &&
                                         ndb_index_stat_get_enable(thd);
          if (index_stat_enable) {
            const int err = ndb_index_stat_set_rpk(i);
            if (err != 0 &&
                /* no stats is not unexpected error */
                err != NdbIndexStat::NoIndexStats &&
                /* warning was printed at first error */
                err != NdbIndexStat::MyHasError &&
                /* stats thread aborted request */
                err != NdbIndexStat::MyAbortReq) {
              push_warning_printf(thd, Sql_condition::SL_WARNING,
                                  ER_CANT_GET_STAT,
                                  "index stats (RPK) for key %s:"
                                  " unexpected error %d",
                                  key_info->name, err);
            }
          }
          // no fallback method...
          break;
        }
      case UNDEFINED_INDEX:  // index is currently unavailable
        break;
    }
  }
}

int ha_ndbcluster::records(ha_rows *num_rows) {
  DBUG_TRACE;

  // Read fresh stats from NDB (one roundtrip)
  const int error = update_stats(table->in_use, true);
  if (error != 0) {
    *num_rows = HA_POS_ERROR;
    return error;
  }

  // Return the "records" from handler::stats::records
  *num_rows = stats.records;
  return 0;
}

int ha_ndbcluster::ndb_err(NdbTransaction *trans) {
  DBUG_TRACE;

  const NdbError &err = trans->getNdbError();
  switch (err.classification) {
    case NdbError::SchemaError: {
      // Mark the NDB table def as invalid, this will cause also all index defs
      // to be invalidate on close
      m_table->setStatusInvalid();
      // Close other open handlers not used by any thread
      ndb_tdc_close_cached_table(current_thd, table->s->db.str,
                                 table->s->table_name.str);
      break;
    }
    default:
      break;
  }
  const int res = ndb_to_mysql_error(&err);
  DBUG_PRINT("info", ("transformed ndbcluster error %d to mysql error %d",
                      err.code, res));
  if (res == HA_ERR_FOUND_DUPP_KEY) {
    char *error_data = err.details;
    uint dupkey = MAX_KEY;

    for (uint i = 0; i < MAX_KEY; i++) {
      if (m_index[i].type == UNIQUE_INDEX ||
          m_index[i].type == UNIQUE_ORDERED_INDEX) {
        const NdbDictionary::Index *unique_index = m_index[i].unique_index;
        if (unique_index &&
            UintPtr(unique_index->getObjectId()) == UintPtr(error_data)) {
          dupkey = i;
          break;
        }
      }
    }
    if (m_rows_to_insert == 1) {
      /*
        We can only distinguish between primary and non-primary
        violations here, so we need to return MAX_KEY for non-primary
        to signal that key is unknown
      */
      m_dupkey = err.code == 630 ? table_share->primary_key : dupkey;
    } else {
      /* We are batching inserts, offending key is not available */
      m_dupkey = (uint)-1;
    }
  }
  return res;
}

extern bool ndb_fk_util_generate_constraint_string(
    THD *thd, Ndb *ndb, const NdbDictionary::ForeignKey &fk,
    const int child_tab_id, String &fk_string);

/**
  Generate error messages when requested by the caller.
  Fetches the error description from NdbError and print it in the caller's
  buffer. This function also additionally handles HA_ROW_REF fk errors.

  @param    error             The error code sent by the caller.
  @param    buf               String buffer to print the error message.

  @retval   true              if the error is permanent
            false             if its temporary
*/

bool ha_ndbcluster::get_error_message(int error, String *buf) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));

  Ndb *ndb = check_ndb_in_thd(current_thd);
  if (!ndb) return false;

  bool temporary = false;

  if (unlikely(error == HA_ERR_NO_REFERENCED_ROW ||
               error == HA_ERR_ROW_IS_REFERENCED)) {
    /* Error message to be generated from NdbError in latest trans or dict */
    Thd_ndb *thd_ndb = get_thd_ndb(current_thd);
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    NdbError err;
    if (thd_ndb->trans != nullptr) {
      err = thd_ndb->trans->getNdbError();
    } else {
      // Drop table failure. get error from dictionary.
      err = dict->getNdbError();
      assert(err.code == 21080);
    }
    temporary = (err.status == NdbError::TemporaryError);

    String fk_string;
    {
      /* copy default error message to be used on failure */
      const char *unknown_fk = "Unknown FK Constraint";
      buf->copy(unknown_fk, (uint32)strlen(unknown_fk), &my_charset_bin);
    }

    /* fk name of format parent_id/child_id/fk_name */
    char fully_qualified_fk_name[MAX_ATTR_NAME_SIZE + (2 * MAX_INT_WIDTH) + 3];
    /* get the fully qualified FK name from ndb using getNdbErrorDetail */
    if (ndb->getNdbErrorDetail(err, &fully_qualified_fk_name[0],
                               sizeof(fully_qualified_fk_name)) == nullptr) {
      assert(false);
      ndb_to_mysql_error(&dict->getNdbError());
      return temporary;
    }

    /* fetch the foreign key */
    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, fully_qualified_fk_name) != 0) {
      assert(false);
      ndb_to_mysql_error(&dict->getNdbError());
      return temporary;
    }

    /* generate constraint string from fk object */
    if (!ndb_fk_util_generate_constraint_string(current_thd, ndb, fk, 0,
                                                fk_string)) {
      assert(false);
      return temporary;
    }

    /* fk found and string has been generated. set the buf */
    buf->copy(fk_string);
    return temporary;
  } else {
    /* NdbError code. Fetch error description from ndb */
    const NdbError err = ndb->getNdbError(error);
    temporary = err.status == NdbError::TemporaryError;
    buf->set(err.message, (uint32)strlen(err.message), &my_charset_bin);
  }

  DBUG_PRINT("exit", ("message: %s, temporary: %d", buf->ptr(), temporary));
  return temporary;
}

/*
  field_used_length() returns the number of bytes actually used to
  store the data of the field. So for a varstring it includes both
  length byte(s) and string data, and anything after data_length()
  bytes are unused.
*/
static uint32 field_used_length(const Field *field, ptrdiff_t row_offset = 0) {
  if (field->type() == MYSQL_TYPE_VARCHAR) {
    return field->get_length_bytes() + field->data_length(row_offset);
  }
  return field->pack_length();
}

/**
  Check if MySQL field type forces var part in ndb storage
*/
static bool field_type_forces_var_part(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
      return true;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VECTOR:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_GEOMETRY:
      return false;
    default:
      return false;
  }
}

/**
 * findBlobError
 * This method attempts to find an error in the hierarchy of runtime
 * NDBAPI objects from Blob up to transaction.
 * It will return -1 if no error is found, 0 if an error is found.
 */
static int findBlobError(NdbError &error, NdbBlob *pBlob) {
  error = pBlob->getNdbError();
  if (error.code != 0) return 0;

  const NdbOperation *pOp = pBlob->getNdbOperation();
  error = pOp->getNdbError();
  if (error.code != 0) return 0;

  NdbTransaction *pTrans = pOp->getNdbTransaction();
  error = pTrans->getNdbError();
  if (error.code != 0) return 0;

  /* No error on any of the objects */
  return -1;
}

/*
 Calculate the length of the blob/text after applying mysql limits
 on blob/text sizes. If the blob contains multi-byte characters, the length is
 reduced till the end of the last well-formed char, so that data is not
 truncated in the middle of a multi-byte char.
*/
static uint64 calc_ndb_blob_len(const CHARSET_INFO *cs, uchar *blob_ptr,
                                uint64 maxlen) {
  int errors = 0;

  if (!cs) cs = &my_charset_bin;

  const char *begin = (const char *)blob_ptr;
  const char *end = (const char *)(blob_ptr + maxlen);

  // avoid truncation in the middle of a multi-byte character by
  // stopping at end of last well-formed character before max length
  uint32 numchars = cs->cset->numchars(cs, begin, end);
  uint64 len64 = cs->cset->well_formed_len(cs, begin, end, numchars, &errors);
  assert(len64 <= maxlen);

  return len64;
}

int ha_ndbcluster::get_ndb_blobs_value_hook(NdbBlob *ndb_blob, void *arg) {
  ha_ndbcluster *ha = (ha_ndbcluster *)arg;
  DBUG_TRACE;
  DBUG_PRINT("info", ("destination row: %p", ha->m_blob_destination_record));

  if (ha->m_blob_counter == 0) /* Reset total size at start of row */
    ha->m_blobs_row_total_size = 0;

  /* Count the total length needed for blob data. */
  int isNull;
  if (ndb_blob->getNull(isNull) != 0) ERR_RETURN(ndb_blob->getNdbError());
  if (isNull == 0) {
    Uint64 len64 = 0;
    if (ndb_blob->getLength(len64) != 0) ERR_RETURN(ndb_blob->getNdbError());
    /* Align to Uint64. */
    ha->m_blobs_row_total_size += (len64 + 7) & ~((Uint64)7);
    if (ha->m_blobs_row_total_size > 0xffffffff) {
      assert(false);
      return -1;
    }
    DBUG_PRINT("info", ("blob[%d]: size %llu, total size now %llu",
                        ha->m_blob_counter, len64, ha->m_blobs_row_total_size));
  }
  ha->m_blob_counter++;

  if (ha->m_blob_counter < ha->m_blob_expected_count_per_row) {
    // Wait until all blobs in this row are active so that a large buffer
    // with space for all can be allocated
    return 0;
  }

  /* Reset blob counter for next row (scan scenario) */
  ha->m_blob_counter = 0;

  // Check if buffer is large enough or need to be extended
  if (ha->m_blobs_row_total_size > ha->m_blobs_buffer.size()) {
    if (!ha->m_blobs_buffer.allocate(ha->m_blobs_row_total_size)) {
      ha->m_thd_ndb->push_warning(ER_OUTOFMEMORY,
                                  "Failed to allocate blobs buffer, size: %llu",
                                  ha->m_blobs_row_total_size);
      return -1;
    }
  }

  /*
    Now read all blob data.
    If we know the destination mysqld row, we also set the blob null bit and
    pointer/length (if not, it will be done instead in unpack_record()).
  */
  uint32 offset = 0;
  for (uint i = 0; i < ha->table->s->fields; i++) {
    Field *field = ha->table->field[i];
    if (!(field->is_flag_set(BLOB_FLAG) && field->stored_in_db)) continue;
    NdbValue value = ha->m_value[i];
    if (value.blob == nullptr) {
      DBUG_PRINT("info", ("[%u] skipped", i));
      continue;
    }
    Field_blob *field_blob = (Field_blob *)field;
    NdbBlob *ndb_blob = value.blob;
    int isNull;
    if (ndb_blob->getNull(isNull) != 0) ERR_RETURN(ndb_blob->getNdbError());
    if (isNull == 0) {
      Uint64 len64 = 0;
      if (ndb_blob->getLength(len64) != 0) ERR_RETURN(ndb_blob->getNdbError());
      assert(len64 < 0xffffffff);
      uchar *buf = ha->m_blobs_buffer.get_ptr(offset);
      uint32 len = ha->m_blobs_buffer.size() - offset;
      if (ndb_blob->readData(buf, len) != 0) {
        NdbError err;
        if (findBlobError(err, ndb_blob) == 0) {
          ERR_RETURN(err);
        } else {
          /* Should always have some error code set */
          assert(err.code != 0);
          ERR_RETURN(err);
        }
      }
      DBUG_PRINT("info",
                 ("[%u] offset: %u  buf: %p  len=%u", i, offset, buf, len));
      assert(len == len64);
      if (ha->m_blob_destination_record) {
        ptrdiff_t ptrdiff =
            ha->m_blob_destination_record - ha->table->record[0];
        field_blob->move_field_offset(ptrdiff);

        if (len > field_blob->max_data_length()) {
          len = calc_ndb_blob_len(field_blob->charset(), buf,
                                  field_blob->max_data_length());

          // push a warning
          push_warning_printf(
              current_thd, Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED,
              "Truncated value from TEXT field \'%s\'", field_blob->field_name);
        }

        field_blob->set_ptr(len, buf);
        field_blob->set_notnull();
        field_blob->move_field_offset(-ptrdiff);
      }
      offset += Uint32((len64 + 7) & ~((Uint64)7));
    } else if (ha->m_blob_destination_record) {
      /* Have to set length even in this case. */
      ptrdiff_t ptrdiff = ha->m_blob_destination_record - ha->table->record[0];
      const uchar *buf = ha->m_blobs_buffer.get_ptr(offset);
      field_blob->move_field_offset(ptrdiff);
      field_blob->set_ptr((uint32)0, buf);
      field_blob->set_null();
      field_blob->move_field_offset(-ptrdiff);
      DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
    }
  }

  /**
   * For non-scan, non autocommit reads, call NdbBlob::close()
   * to allow Blob read related resources to be freed
   * early
   */
  const bool autocommit = (get_thd_ndb(current_thd)->m_handler != nullptr);
  if (!autocommit && !ha->m_active_cursor) {
    for (uint i = 0; i < ha->table->s->fields; i++) {
      Field *field = ha->table->field[i];
      if (!(field->is_flag_set(BLOB_FLAG) && field->stored_in_db)) continue;
      NdbValue value = ha->m_value[i];
      if (value.blob == nullptr) {
        DBUG_PRINT("info", ("[%u] skipped", i));
        continue;
      }
      NdbBlob *ndb_blob = value.blob;

      assert(ndb_blob->getState() == NdbBlob::Active);

      /* Call close() with execPendingBlobOps == true
       * For LM_CommittedRead access, this will enqueue
       * an unlock operation, which the Blob framework
       * code invoking this callback will execute before
       * returning control to the caller of execute()
       */
      if (ndb_blob->close(true) != 0) {
        ERR_RETURN(ndb_blob->getNdbError());
      }
    }
  }

  return 0;
}

/*
  Request reading of blob values.

  If dst_record is specified, the blob null bit, pointer, and length will be
  set in that record. Otherwise they must be set later by calling
  unpack_record().
*/
int ha_ndbcluster::get_blob_values(const NdbOperation *ndb_op,
                                   uchar *dst_record, const MY_BITMAP *bitmap) {
  uint i;
  DBUG_TRACE;

  m_blob_counter = 0;
  m_blob_expected_count_per_row = 0;
  m_blob_destination_record = dst_record;
  m_blobs_row_total_size = 0;
  ndb_op->getNdbTransaction()->setMaxPendingBlobReadBytes(
      THDVAR(current_thd, blob_read_batch_bytes));

  for (i = 0; i < table_share->fields; i++) {
    Field *field = table->field[i];
    if (!(field->is_flag_set(BLOB_FLAG) && field->stored_in_db)) continue;

    DBUG_PRINT("info", ("fieldnr=%d", i));
    NdbBlob *ndb_blob;
    if (bitmap_is_set(bitmap, i)) {
      if ((ndb_blob = m_table_map->getBlobHandle(ndb_op, i)) == nullptr ||
          ndb_blob->setActiveHook(get_ndb_blobs_value_hook, this) != 0)
        return 1;
      m_blob_expected_count_per_row++;
    } else
      ndb_blob = nullptr;

    m_value[i].blob = ndb_blob;
  }

  return 0;
}

int ha_ndbcluster::set_blob_values(const NdbOperation *ndb_op,
                                   ptrdiff_t row_offset,
                                   const MY_BITMAP *bitmap, uint *set_count,
                                   bool batch) const {
  uint field_no;
  uint *blob_index, *blob_index_end;
  int res = 0;
  DBUG_TRACE;

  *set_count = 0;

  if (table_share->blob_fields == 0) return 0;

  // Note! This settings seems to be lazily assigned for every row rather than
  // once up front when transaction is started. For many rows, it might be
  // better to do it once.
  m_thd_ndb->trans->setMaxPendingBlobWriteBytes(
      m_thd_ndb->get_blob_write_batch_size());

  blob_index = table_share->blob_field;
  blob_index_end = blob_index + table_share->blob_fields;
  do {
    field_no = *blob_index;
    /* A NULL bitmap sets all blobs. */
    if (bitmap && !bitmap_is_set(bitmap, field_no)) continue;
    Field *field = table->field[field_no];
    if (field->is_virtual_gcol()) continue;

    NdbBlob *ndb_blob = m_table_map->getBlobHandle(ndb_op, field_no);
    if (ndb_blob == nullptr) ERR_RETURN(ndb_op->getNdbError());
    if (field->is_real_null(row_offset)) {
      DBUG_PRINT("info", ("Setting Blob %d to NULL", field_no));
      if (ndb_blob->setNull() != 0) ERR_RETURN(ndb_op->getNdbError());
    } else {
      Field_blob *field_blob = (Field_blob *)field;

      // Get length and pointer to data
      const uint32 blob_len = field_blob->get_length(row_offset);
      const uchar *blob_ptr = field_blob->get_blob_data(row_offset);

      // Looks like NULL ptr signals length 0 blob
      if (blob_ptr == nullptr) {
        assert(blob_len == 0);
        blob_ptr = pointer_cast<const uchar *>("");
      }

      DBUG_PRINT("value", ("set blob ptr: %p  len: %u", blob_ptr, blob_len));
      DBUG_DUMP("value", blob_ptr, std::min(blob_len, (Uint32)26));

      if (batch && blob_len > 0) {
        /*
          The blob data pointer is required to remain valid until execute()
          time. So when batching, copy the blob data to batch memory.
        */
        uchar *blob_copy = m_thd_ndb->copy_to_batch_mem(blob_ptr, blob_len);
        if (!blob_copy) {
          return HA_ERR_OUT_OF_MEM;
        }
        blob_ptr = blob_copy;
      }
      res = ndb_blob->setValue(pointer_cast<const char *>(blob_ptr), blob_len);
      if (res != 0) ERR_RETURN(ndb_op->getNdbError());
    }

    ++(*set_count);
  } while (++blob_index != blob_index_end);

  return res;
}

/**
  Check if any set or get of blob value in current query.
*/

bool ha_ndbcluster::uses_blob_value(const MY_BITMAP *bitmap) const {
  uint *blob_index, *blob_index_end;
  if (table_share->blob_fields == 0) return false;

  blob_index = table_share->blob_field;
  blob_index_end = blob_index + table_share->blob_fields;
  do {
    Field *field = table->field[*blob_index];
    if (bitmap_is_set(bitmap, field->field_index()) &&
        !field->is_virtual_gcol())
      return true;
  } while (++blob_index != blob_index_end);
  return false;
}

void ha_ndbcluster::release_blobs_buffer() {
  DBUG_TRACE;
  m_blobs_buffer.release();
  m_blobs_row_total_size = 0;
}

/*
  Does type support a default value?
*/
static bool type_supports_default_value(enum_field_types mysql_type) {
  bool ret =
      (mysql_type != MYSQL_TYPE_BLOB && mysql_type != MYSQL_TYPE_TINY_BLOB &&
       mysql_type != MYSQL_TYPE_MEDIUM_BLOB &&
       mysql_type != MYSQL_TYPE_LONG_BLOB && mysql_type != MYSQL_TYPE_JSON &&
       mysql_type != MYSQL_TYPE_GEOMETRY && mysql_type != MYSQL_TYPE_VECTOR);

  return ret;
}

#ifndef NDEBUG
/**

   Check that NDB table has the same default values
   as the MySQL table def.
   Called as part of a DBUG check when opening table.

   @return true Defaults are ok
*/
bool ha_ndbcluster::check_default_values() const {
  if (!m_table->hasDefaultValues()) {
    // There are no default values in the NDB table
    return true;
  }

  bool defaults_aligned = true;

  /* NDB supports native defaults for non-pk columns */
  my_bitmap_map *old_map = tmp_use_all_columns(table, table->read_set);

  for (uint f = 0; f < table_share->fields; f++) {
    Field *field = table->field[f];
    if (!field->stored_in_db) continue;

    const NdbDictionary::Column *ndbCol =
        m_table_map->getColumn(field->field_index());

    if ((!(field->is_flag_set(PRI_KEY_FLAG) ||
           field->is_flag_set(NO_DEFAULT_VALUE_FLAG))) &&
        type_supports_default_value(field->real_type())) {
      // Expect NDB to have a native default for this column
      ptrdiff_t src_offset =
          table_share->default_values - field->table->record[0];

      /* Move field by offset to refer to default value */
      field->move_field_offset(src_offset);

      const uchar *ndb_default = (const uchar *)ndbCol->getDefaultValue();

      if (ndb_default == nullptr) {
        /* MySQL default must also be NULL */
        defaults_aligned = field->is_null();
      } else {
        if (field->type() != MYSQL_TYPE_BIT) {
          defaults_aligned = (0 == field->cmp(ndb_default));
        } else {
          longlong value = (static_cast<Field_bit *>(field))->val_int();
          /* Map to NdbApi format - two Uint32s */
          Uint32 out[2];
          out[0] = 0;
          out[1] = 0;
          for (int b = 0; b < 64; b++) {
            out[b >> 5] |= (value & 1) << (b & 31);

            value = value >> 1;
          }
          Uint32 defaultLen = field_used_length(field);
          defaultLen = ((defaultLen + 3) & ~(Uint32)0x7);
          defaults_aligned = (0 == memcmp(ndb_default, out, defaultLen));
        }
      }

      field->move_field_offset(-src_offset);

      if (unlikely(!defaults_aligned)) {
        ndb_log_error(
            "Internal error, Default values differ "
            "for column %u, ndb_default: %d",
            field->field_index(), ndb_default != nullptr);
      }
    } else {
      /* Don't expect Ndb to have a native default for this column */
      if (unlikely(ndbCol->getDefaultValue() != nullptr)) {
        /* Didn't expect that */
        ndb_log_error(
            "Internal error, Column %u has native "
            "default, but shouldn't. Flags=%u, type=%u",
            field->field_index(), field->all_flags(), field->real_type());
        defaults_aligned = false;
      }
    }
    if (unlikely(!defaults_aligned)) {
      // Dump field
      ndb_log_error(
          "field[ name: '%s', type: %u, real_type: %u, "
          "flags: 0x%x, is_null: %d]",
          field->field_name, field->type(), field->real_type(),
          field->all_flags(), field->is_null());
      // Dump ndbCol
      ndb_log_error(
          "ndbCol[name: '%s', type: %u, column_no: %d, "
          "nullable: %d]",
          ndbCol->getName(), ndbCol->getType(), ndbCol->getColumnNo(),
          ndbCol->getNullable());
      break;
    }
  }
  tmp_restore_column_map(table->read_set, old_map);

  return defaults_aligned;
}
#endif

int ha_ndbcluster::get_metadata(Ndb *ndb, const char *dbname,
                                const char *tabname,
                                const dd::Table *table_def) {
  DBUG_TRACE;

  // The NDB table should not be open
  assert(m_table == nullptr);
  assert(m_trans_table_stats == nullptr);

  Ndb_dd_handle dd_handle = ndb_dd_table_get_spi_and_version(table_def);
  if (!dd_handle.valid()) {
    DBUG_PRINT("error", ("Could not extract object_id and object_version "
                         "from table definition"));
    return 1;
  }

  Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
  const NDBTAB *tab = ndbtab_g.get_table();
  if (tab == nullptr) {
    ERR_RETURN(ndbtab_g.getNdbError());
  }

  {
    // Check that the id and version from DD
    // matches the id and version of the NDB table
    Ndb_dd_handle curr_handle{tab->getObjectId(), tab->getObjectVersion()};
    if (curr_handle != dd_handle) {
      DBUG_PRINT("error", ("Table id or version mismatch"));
      DBUG_PRINT("error", ("NDB table id: %llu, version: %d", curr_handle.spi,
                           curr_handle.version));
      DBUG_PRINT("error", ("DD table id: %llu, version: %d", dd_handle.spi,
                           dd_handle.version));

      ndb_log_verbose(10,
                      "Table id or version mismatch for table '%s.%s', "
                      "[%llu, %d] != [%llu, %d]",
                      dbname, tabname, dd_handle.spi, dd_handle.version,
                      curr_handle.spi, curr_handle.version);

      ndbtab_g.invalidate();

      // When returning HA_ERR_TABLE_DEF_CHANGED from handler::open()
      // the caller is intended to call ha_discover() in order to let
      // the engine install the correct table definition in the
      // data dictionary, then the open() will be retried and presumably
      // the table definition will be correct
      return HA_ERR_TABLE_DEF_CHANGED;
    }
  }

  if (DBUG_EVALUATE_IF("ndb_get_metadata_fail", true, false)) {
    fprintf(stderr, "ndb_get_metadata_fail\n");
    DBUG_SET("-d,ndb_get_metadata_fail");
    ndbtab_g.invalidate();
    return HA_ERR_TABLE_DEF_CHANGED;
  }

  // Remember the opened NDB table
  m_table = tab;

  // Create field to column map for table
  m_table_map = new Ndb_table_map(table, m_table);

  // Check that NDB default values match those in MySQL table def.
  assert(check_default_values());

  ndb_bitmap_init(&m_bitmap, m_bitmap_buf, table_share->fields);

  NDBDICT *dict = ndb->getDictionary();
  int error = 0;
  if (table_share->primary_key == MAX_KEY) {
    /* Hidden primary key. */
    if ((error = add_hidden_pk_ndb_record(dict)) != 0) goto err;
  }

  if ((error = add_table_ndb_record(dict)) != 0) goto err;

  // Approximate row size
  m_bytes_per_write = 12 + tab->getRowSizeInBytes() + 4 * tab->getNoOfColumns();

  /* Open indexes */
  if ((error = open_indexes(dict)) != 0) goto err;

  /*
    Backward compatibility for tables created without tablespace
    in .frm => read tablespace setting from engine
  */
  if (table_share->mysql_version < 50120 &&
      !table_share->tablespace /* safety */) {
    Uint32 id;
    if (tab->getTablespace(&id)) {
      NdbDictionary::Tablespace ts = dict->getTablespace(id);
      if (ndb_dict_check_NDB_error(dict)) {
        const char *tablespace = ts.getName();
        const size_t tablespace_len = strlen(tablespace);
        if (tablespace_len != 0) {
          DBUG_PRINT("info", ("Found tablespace '%s'", tablespace));
          table_share->tablespace =
              strmake_root(&table_share->mem_root, tablespace, tablespace_len);
        }
      }
    }
  }

  // Tell the Ndb_table_guard to release ownership of the NDB table def since
  // it's now owned by this ha_ndbcluster instance
  ndbtab_g.release();

  return 0;

err:
  // Function failed, release all resources allocated by this function
  // before returning
  release_indexes(dict, true /* invalidate */);

  //  Release field to column map
  if (m_table_map != nullptr) {
    delete m_table_map;
    m_table_map = nullptr;
  }
  // Release NdbRecord's allocated for the table
  if (m_ndb_record != nullptr) {
    dict->releaseRecord(m_ndb_record);
    m_ndb_record = nullptr;
  }
  if (m_ndb_hidden_key_record != nullptr) {
    dict->releaseRecord(m_ndb_hidden_key_record);
    m_ndb_hidden_key_record = nullptr;
  }

  ndbtab_g.invalidate();
  m_table = nullptr;
  return error;
}

/**
   @brief Create Attrid_map for mapping the columns of KEY to a NDB index.
   @param key_info key to create mapping for
   @param index NDB index definition
 */
NDB_INDEX_DATA::Attrid_map::Attrid_map(const KEY *key_info,
                                       const NdbDictionary::Index *index) {
  m_ids.reserve(key_info->user_defined_key_parts);

  for (unsigned i = 0; i < key_info->user_defined_key_parts; i++) {
    const KEY_PART_INFO *key_part = key_info->key_part + i;
    const char *key_part_name = key_part->field->field_name;

    // Find the NDB index column by name
    for (unsigned j = 0; j < index->getNoOfColumns(); j++) {
      const NdbDictionary::Column *column = index->getColumn(j);
      if (strcmp(key_part_name, column->getName()) == 0) {
        // Save id of NDB index column
        m_ids.push_back(j);
        break;
      }
    }
  }
  // Must have found one NDB column for each key
  ndbcluster::ndbrequire(m_ids.size() == key_info->user_defined_key_parts);
  // Check that the map is not ordered
  assert(std::is_sorted(m_ids.begin(), m_ids.end()) == false);
}

/**
   @brief Create Attrid_map for mapping the columns of KEY to a NDB table.
   @param key_info key to create mapping for
   @param table NDB table definition
 */
NDB_INDEX_DATA::Attrid_map::Attrid_map(const KEY *key_info,
                                       const NdbDictionary::Table *table) {
  m_ids.reserve(key_info->user_defined_key_parts);

  uint key_pos = 0;
  int columnnr = 0;
  const KEY_PART_INFO *key_part = key_info->key_part;
  const KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
  for (; key_part != end; key_part++) {
    // As NdbColumnImpl::m_keyInfoPos isn't available through
    // NDB API it has to be calculated, else it could have been retrieved with
    //   table->getColumn(key_part->fieldnr-1)->m_impl.m_keyInfoPos;

    if (key_part->fieldnr < columnnr) {
      // PK columns are not in same order as the columns are defined in the
      // table, Restart PK search from first column:
      key_pos = 0;
      columnnr = 0;
    }

    while (columnnr < key_part->fieldnr - 1) {
      if (table->getColumn(columnnr++)->getPrimaryKey()) {
        key_pos++;
      }
    }

    assert(table->getColumn(columnnr)->getPrimaryKey());
    // Save id of NDB column
    m_ids.push_back(key_pos);

    columnnr++;
    key_pos++;
  }
  // Must have found one NDB column for each key
  ndbcluster::ndbrequire(m_ids.size() == key_info->user_defined_key_parts);
  // Check that the map is not ordered
  assert(std::is_sorted(m_ids.begin(), m_ids.end()) == false);
}

void NDB_INDEX_DATA::Attrid_map::fill_column_map(uint column_map[]) const {
  assert(m_ids.size());
  for (size_t i = 0; i < m_ids.size(); i++) {
    column_map[i] = m_ids[i];
  }
}

/**
   @brief Check if columns in KEY is ordered
   @param key_info key to check
   @return true if columns are ordered
   @note the function actually don't check for consecutive numbers. The
   assumption is that if columns are in same order they will be consecutive. i.e
   [0,1,2...] and not [0,3,6,...]
 */
static bool check_ordered_columns(const KEY *key_info) {
  int columnnr = 0;
  const KEY_PART_INFO *key_part = key_info->key_part;
  const KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
  for (; key_part != end; key_part++) {
    if (key_part->fieldnr < columnnr) {
      // PK columns are not in same order as the columns in the table
      DBUG_PRINT("info", ("Detected different order in table"));
      return false;
    }

    while (columnnr < key_part->fieldnr - 1) {
      columnnr++;
    }
    columnnr++;
  }
  return true;
}

void NDB_INDEX_DATA::create_attrid_map(const KEY *key_info,
                                       const NdbDictionary::Table *table) {
  DBUG_TRACE;
  assert(!attrid_map);  // Should not already have been created

  if (key_info->user_defined_key_parts == 1) {
    DBUG_PRINT("info", ("Skip creating map for index with only one column"));
    return;
  }

  if (check_ordered_columns(key_info)) {
    DBUG_PRINT("info", ("Skip creating map for table with same order"));
    return;
  }

  attrid_map = new Attrid_map(key_info, table);
}

/**
   Check if columns in KEY matches the order of the index
   @param key_info key to check
   @param index NDB index to compare with
   @return true if columns in KEY and index have same order
 */
static bool check_same_order_in_index(const KEY *key_info,
                                      const NdbDictionary::Index *index) {
  // Check if key and NDB column order is same
  for (unsigned i = 0; i < key_info->user_defined_key_parts; i++) {
    const KEY_PART_INFO *key_part = key_info->key_part + i;
    const char *key_part_name = key_part->field->field_name;
    for (unsigned j = 0; j < index->getNoOfColumns(); j++) {
      const NdbDictionary::Column *column = index->getColumn(j);
      if (strcmp(key_part_name, column->getName()) == 0) {
        if (i != j) {
          DBUG_PRINT("info", ("Detected different order in index"));
          return false;
        }
        break;
      }
    }
  }
  return true;
}

void NDB_INDEX_DATA::create_attrid_map(const KEY *key_info,
                                       const NdbDictionary::Index *index) {
  DBUG_TRACE;
  assert(index);
  assert(!attrid_map);  // Should not already have been created

  if (key_info->user_defined_key_parts == 1) {
    DBUG_PRINT("info", ("Skip creating map for index with only one column"));
    return;
  }

  if (check_same_order_in_index(key_info, index)) {
    DBUG_PRINT("info", ("Skip creating map for index with same order"));
    return;
  }

  attrid_map = new Attrid_map(key_info, index);
}

void NDB_INDEX_DATA::delete_attrid_map() {
  delete attrid_map;
  attrid_map = nullptr;
}

void NDB_INDEX_DATA::fill_column_map(const KEY *key_info,
                                     uint column_map[]) const {
  if (attrid_map) {
    // Use the cached Attrid_map
    attrid_map->fill_column_map(column_map);
    return;
  }
  // Use the default sequential column order
  for (uint i = 0; i < key_info->user_defined_key_parts; i++) {
    column_map[i] = i;
  }
}

/**
  @brief Create all the indexes for a table.
  @note If any index should fail to be created, the error is returned
  immediately
*/
int ha_ndbcluster::create_indexes(THD *thd, TABLE *tab,
                                  const NdbDictionary::Table *ndbtab) const {
  int error = 0;
  const KEY *key_info = tab->key_info;
  const char **key_name = tab->s->keynames.type_names;
  DBUG_TRACE;

  for (uint i = 0; i < tab->s->keys; i++, key_info++, key_name++) {
    const char *index_name = *key_name;
    NDB_INDEX_TYPE idx_type = get_declared_index_type(i);
    error = create_index(thd, index_name, key_info, idx_type, ndbtab);
    if (error) {
      DBUG_PRINT("error", ("Failed to create index %u", i));
      break;
    }
  }

  return error;
}

static void ndb_protect_char(const char *from, char *to, uint to_length,
                             char protect) {
  uint fpos = 0, tpos = 0;

  while (from[fpos] != '\0' && tpos < to_length - 1) {
    if (from[fpos] == protect) {
      int len = 0;
      to[tpos++] = '@';
      if (tpos < to_length - 5) {
        len = sprintf(to + tpos, "00%u", (uint)protect);
        tpos += len;
      }
    } else {
      to[tpos++] = from[fpos];
    }
    fpos++;
  }
  to[tpos] = '\0';
}

int ha_ndbcluster::open_index(NdbDictionary::Dictionary *dict,
                              const KEY *key_info, const char *key_name,
                              uint index_no) {
  DBUG_TRACE;

  NDB_INDEX_TYPE idx_type = get_declared_index_type(index_no);
  NDB_INDEX_DATA &index_data = m_index[index_no];

  char index_name[FN_LEN + 1];
  ndb_protect_char(key_name, index_name, sizeof(index_name) - 1, '/');
  if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX) {
    DBUG_PRINT("info", ("Get handle to index %s", index_name));
    const NdbDictionary::Index *index =
        dict->getIndexGlobal(index_name, *m_table);
    if (index) {
      DBUG_PRINT("info",
                 ("index: %p  id: %d  version: %d.%d  status: %d", index,
                  index->getObjectId(), index->getObjectVersion() & 0xFFFFFF,
                  index->getObjectVersion() >> 24, index->getObjectStatus()));
      assert(index->getObjectStatus() == NdbDictionary::Object::Retrieved);
      index_data.index = index;
    } else {
      const NdbError &err = dict->getNdbError();
      if (err.code != 4243) ERR_RETURN(err);
      // Index Not Found. Proceed with this index unavailable.
    }
  }

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX) {
    char unique_index_name[FN_LEN + 1];
    static const char *unique_suffix = "$unique";
    strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
    const NdbDictionary::Index *index =
        dict->getIndexGlobal(unique_index_name, *m_table);
    if (index) {
      DBUG_PRINT("info",
                 ("index: %p  id: %d  version: %d.%d  status: %d", index,
                  index->getObjectId(), index->getObjectVersion() & 0xFFFFFF,
                  index->getObjectVersion() >> 24, index->getObjectStatus()));
      assert(index->getObjectStatus() == NdbDictionary::Object::Retrieved);
      m_has_unique_index = true;
      index_data.unique_index = index;
      // Create attrid map for unique index
      index_data.create_attrid_map(key_info, index);
    } else {
      const NdbError &err = dict->getNdbError();
      if (err.code != 4243) ERR_RETURN(err);
      // Index Not Found. Proceed with this index unavailable.
    }
  }

  /* Set type of index as actually opened */
  switch (idx_type) {
    case UNDEFINED_INDEX:
      assert(false);
      break;
    case PRIMARY_KEY_INDEX:
      break;
    case PRIMARY_KEY_ORDERED_INDEX:
      if (!index_data.index) idx_type = PRIMARY_KEY_INDEX;
      break;
    case UNIQUE_INDEX:
      if (!index_data.unique_index) idx_type = UNDEFINED_INDEX;
      break;
    case UNIQUE_ORDERED_INDEX:
      if (!(index_data.unique_index || index_data.index))
        idx_type = UNDEFINED_INDEX;
      else if (!index_data.unique_index)
        idx_type = ORDERED_INDEX;
      else if (!index_data.index)
        idx_type = UNIQUE_INDEX;
      break;
    case ORDERED_INDEX:
      if (!index_data.index) idx_type = UNDEFINED_INDEX;
      break;
  }
  index_data.type = idx_type;

  if (idx_type == UNDEFINED_INDEX) return 0;

  if (idx_type == PRIMARY_KEY_ORDERED_INDEX || idx_type == PRIMARY_KEY_INDEX) {
    // Create attrid map for primary key
    index_data.create_attrid_map(key_info, m_table);
  }

  return open_index_ndb_record(dict, key_info, index_no);
}

/*
  We use this function to convert null bit masks, as found in class Field,
  to bit numbers, as used in NdbRecord.
*/
static uint null_bit_mask_to_bit_number(uchar bit_mask) {
  switch (bit_mask) {
    case 0x1:
      return 0;
    case 0x2:
      return 1;
    case 0x4:
      return 2;
    case 0x8:
      return 3;
    case 0x10:
      return 4;
    case 0x20:
      return 5;
    case 0x40:
      return 6;
    case 0x80:
      return 7;
    default:
      assert(false);
      return 0;
  }
}

static void ndb_set_record_specification(
    uint field_no, NdbDictionary::RecordSpecification *spec, const TABLE *table,
    const NdbDictionary::Column *ndb_column) {
  DBUG_TRACE;
  assert(ndb_column);
  spec->column = ndb_column;
  spec->offset = Uint32(table->field[field_no]->offset(table->record[0]));
  if (table->field[field_no]->is_nullable()) {
    spec->nullbit_byte_offset = Uint32(table->field[field_no]->null_offset());
    spec->nullbit_bit_in_byte =
        null_bit_mask_to_bit_number(table->field[field_no]->null_bit);
  } else if (table->field[field_no]->type() == MYSQL_TYPE_BIT) {
    /* We need to store the position of the overflow bits. */
    const Field_bit *field_bit =
        static_cast<Field_bit *>(table->field[field_no]);
    spec->nullbit_byte_offset = Uint32(field_bit->bit_ptr - table->record[0]);
    spec->nullbit_bit_in_byte = field_bit->bit_ofs;
  } else {
    spec->nullbit_byte_offset = 0;
    spec->nullbit_bit_in_byte = 0;
  }
  spec->column_flags = 0;
  if (table->field[field_no]->type() == MYSQL_TYPE_STRING &&
      table->field[field_no]->pack_length() == 0) {
    /*
      This is CHAR(0), which we represent as
      a nullable BIT(1) column where we ignore the data bit
    */
    spec->column_flags |=
        NdbDictionary::RecordSpecification::BitColMapsNullBitOnly;
  }
  DBUG_PRINT("info",
             ("%s.%s field: %d, col: %d, offset: %d, null bit: %d",
              table->s->table_name.str, ndb_column->getName(), field_no,
              ndb_column->getColumnNo(), spec->offset,
              (8 * spec->nullbit_byte_offset) + spec->nullbit_bit_in_byte));
}

int ha_ndbcluster::add_table_ndb_record(NdbDictionary::Dictionary *dict) {
  DBUG_TRACE;
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;
  uint fieldId, colId;

  for (fieldId = 0, colId = 0; fieldId < table_share->fields; fieldId++) {
    if (table->field[fieldId]->stored_in_db) {
      ndb_set_record_specification(fieldId, &spec[colId], table,
                                   m_table->getColumn(colId));
      colId++;
    }
  }

  rec = dict->createRecord(
      m_table, spec, colId, sizeof(spec[0]),
      NdbDictionary::RecMysqldBitfield | NdbDictionary::RecPerColumnFlags);
  if (!rec) ERR_RETURN(dict->getNdbError());
  m_ndb_record = rec;

  return 0;
}

/* Create NdbRecord for setting hidden primary key from Uint64. */
int ha_ndbcluster::add_hidden_pk_ndb_record(NdbDictionary::Dictionary *dict) {
  DBUG_TRACE;
  NdbDictionary::RecordSpecification spec[1];
  NdbRecord *rec;

  spec[0].column = m_table->getColumn(m_table_map->get_hidden_key_column());
  spec[0].offset = 0;
  spec[0].nullbit_byte_offset = 0;
  spec[0].nullbit_bit_in_byte = 0;

  rec = dict->createRecord(m_table, spec, 1, sizeof(spec[0]));
  if (!rec) ERR_RETURN(dict->getNdbError());
  m_ndb_hidden_key_record = rec;

  return 0;
}

int ha_ndbcluster::open_index_ndb_record(NdbDictionary::Dictionary *dict,
                                         const KEY *key_info, uint index_no) {
  DBUG_TRACE;
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE + 2];
  NdbRecord *rec;

  Uint32 offset = 0;
  for (uint i = 0; i < key_info->user_defined_key_parts; i++) {
    KEY_PART_INFO *kp = &key_info->key_part[i];
    spec[i].column = m_table_map->getColumn(kp->fieldnr - 1);
    if (!spec[i].column) ERR_RETURN(dict->getNdbError());
    if (kp->null_bit) {
      /* Nullable column. */
      spec[i].offset = offset + 1;  // First byte is NULL flag
      spec[i].nullbit_byte_offset = offset;
      spec[i].nullbit_bit_in_byte = 0;
    } else {
      /* Not nullable column. */
      spec[i].offset = offset;
      spec[i].nullbit_byte_offset = 0;
      spec[i].nullbit_bit_in_byte = 0;
    }
    offset += kp->store_length;
  }

  if (m_index[index_no].index) {
    /*
      Enable MysqldShrinkVarchar flag so that the two-byte length used by
      mysqld for short varchar keys is correctly converted into a one-byte
      length used by Ndb kernel.
    */
    rec = dict->createRecord(m_index[index_no].index, m_table, spec,
                             key_info->user_defined_key_parts, sizeof(spec[0]),
                             (NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield));
    if (!rec) ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_record_key = rec;
  } else
    m_index[index_no].ndb_record_key = nullptr;

  if (m_index[index_no].unique_index) {
    rec = dict->createRecord(m_index[index_no].unique_index, m_table, spec,
                             key_info->user_defined_key_parts, sizeof(spec[0]),
                             (NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield));
    if (!rec) ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key = rec;
  } else if (index_no == table_share->primary_key) {
    /* The primary key is special, there is no explicit NDB index associated. */
    rec = dict->createRecord(m_table, spec, key_info->user_defined_key_parts,
                             sizeof(spec[0]),
                             (NdbDictionary::RecMysqldShrinkVarchar |
                              NdbDictionary::RecMysqldBitfield));
    if (!rec) ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_key = rec;
  } else
    m_index[index_no].ndb_unique_record_key = nullptr;

  /* Now do the same, but this time with offsets from Field, for row access. */
  for (uint i = 0; i < key_info->user_defined_key_parts; i++) {
    const KEY_PART_INFO *kp = &key_info->key_part[i];

    spec[i].offset = kp->offset;
    if (kp->null_bit) {
      /* Nullable column. */
      spec[i].nullbit_byte_offset = kp->null_offset;
      spec[i].nullbit_bit_in_byte = null_bit_mask_to_bit_number(kp->null_bit);
    } else {
      /* Not nullable column. */
      spec[i].nullbit_byte_offset = 0;
      spec[i].nullbit_bit_in_byte = 0;
    }
  }

  if (m_index[index_no].unique_index) {
    rec = dict->createRecord(m_index[index_no].unique_index, m_table, spec,
                             key_info->user_defined_key_parts, sizeof(spec[0]),
                             NdbDictionary::RecMysqldBitfield);
    if (!rec) ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row = rec;
  } else if (index_no == table_share->primary_key) {
    rec = dict->createRecord(m_table, spec, key_info->user_defined_key_parts,
                             sizeof(spec[0]), NdbDictionary::RecMysqldBitfield);
    if (!rec) ERR_RETURN(dict->getNdbError());
    m_index[index_no].ndb_unique_record_row = rec;
  } else
    m_index[index_no].ndb_unique_record_row = nullptr;

  return 0;
}

static bool check_index_fields_not_null(const KEY *key_info) {
  DBUG_TRACE;
  const KEY_PART_INFO *key_part = key_info->key_part;
  const KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
  for (; key_part != end; key_part++) {
    const Field *field = key_part->field;
    if (field->is_nullable()) return true;
  }
  return false;
}

/**
  @brief Open handles to physical indexes in NDB and create NdbRecord's for
  accessing NDB via the index. The intention is to setup this handler instance
  for efficient DML processing in the transaction code path.

  @param dict NdbDictionary pointer
  @return 0 if successful, otherwise random error returned from NdbApi, message
  pushed as warning
*/
int ha_ndbcluster::open_indexes(NdbDictionary::Dictionary *dict) {
  DBUG_TRACE;

  // Flag indicating if table has unique index will be turned on as a sideffect
  // of the below loop if table has unique index
  m_has_unique_index = false;

  const KEY *key_info = table->key_info;
  const char **key_name = table->s->keynames.type_names;
  for (uint i = 0; i < table->s->keys; i++, key_info++, key_name++) {
    const int error = open_index(dict, key_info, *key_name, i);
    if (error) {
      return error;
    }
    m_index[i].null_in_unique_index = check_index_fields_not_null(key_info);
  }

  return 0;
}

/**
   @brief Close handles to physical indexes in NDB and release NdbRecord's
   @param dict NdbDictionary pointer
   @param invalidate Invalidate the index in NdbApi dict cache when
   reference to the NdbApi index is released.
 */
void ha_ndbcluster::release_indexes(NdbDictionary::Dictionary *dict,
                                    bool invalidate) {
  DBUG_TRACE;
  for (NDB_INDEX_DATA &index_data : m_index) {
    if (index_data.unique_index) {
      // Release reference to unique index in NdbAPI
      dict->removeIndexGlobal(*index_data.unique_index, invalidate);
      index_data.unique_index = nullptr;
    }
    if (index_data.index) {
      // Release reference to index in NdbAPI
      dict->removeIndexGlobal(*index_data.index, invalidate);
      index_data.index = nullptr;
    }
    index_data.delete_attrid_map();

    if (index_data.ndb_record_key) {
      dict->releaseRecord(index_data.ndb_record_key);
      index_data.ndb_record_key = nullptr;
    }
    if (index_data.ndb_unique_record_key) {
      dict->releaseRecord(index_data.ndb_unique_record_key);
      index_data.ndb_unique_record_key = nullptr;
    }
    if (index_data.ndb_unique_record_row) {
      dict->releaseRecord(index_data.ndb_unique_record_row);
      index_data.ndb_unique_record_row = nullptr;
    }
    index_data.type = UNDEFINED_INDEX;
  }
}

/**
  @brief Drop all physical NDB indexes for one MySQL index from NDB
  @param dict NdbDictionary pointer
  @param index_num Number of the index in m_index array
  @return 0 if successful, otherwise random error returned from NdbApi, message
  pushed as warning
*/
int ha_ndbcluster::inplace__drop_index(NdbDictionary::Dictionary *dict,
                                       uint index_num) {
  DBUG_TRACE;

  const NdbDictionary::Index *unique_index = m_index[index_num].unique_index;
  if (unique_index) {
    DBUG_PRINT("info", ("Drop unique index: %s", unique_index->getName()));
    // Drop unique index from NDB
    if (dict->dropIndexGlobal(*unique_index) != 0) {
      m_dupkey = index_num;  // for HA_ERR_DROP_INDEX_FK
      return ndb_to_mysql_error(&dict->getNdbError());
    }
  }

  const NdbDictionary::Index *index = m_index[index_num].index;
  if (index) {
    DBUG_PRINT("info", ("Drop index: %s", index->getName()));
    // Drop ordered index from NDB
    if (dict->dropIndexGlobal(*index) != 0) {
      m_dupkey = index_num;  // for HA_ERR_DROP_INDEX_FK
      return ndb_to_mysql_error(&dict->getNdbError());
    }
  }

  return 0;
}

/**
  Decode the declared type of an index from information
  provided in table object.
*/
NDB_INDEX_TYPE get_index_type_from_key(uint index_num, const KEY *key_info,
                                       bool primary) {
  const bool is_hash_index = (key_info[index_num].algorithm == HA_KEY_ALG_HASH);
  if (primary)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;

  if (!(key_info[index_num].flags & HA_NOSAME)) return ORDERED_INDEX;

  return is_hash_index ? UNIQUE_INDEX : UNIQUE_ORDERED_INDEX;
}

inline NDB_INDEX_TYPE ha_ndbcluster::get_declared_index_type(uint idxno) const {
  return get_index_type_from_key(idxno, table_share->key_info,
                                 idxno == table_share->primary_key);
}

/* Return the actual type of the index as currently available
 */
NDB_INDEX_TYPE ha_ndbcluster::get_index_type(uint idx_no) const {
  assert(idx_no < MAX_KEY);
  assert(m_table);
  return m_index[idx_no].type;
}

void ha_ndbcluster::release_metadata(NdbDictionary::Dictionary *dict,
                                     bool invalidate) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("invalidate: %d", invalidate));

  if (m_table == nullptr) {
    return;  // table already released
  }

  if (invalidate == false &&
      m_table->getObjectStatus() == NdbDictionary::Object::Invalid) {
    DBUG_PRINT("info", ("table status invalid -> invalidate both table and "
                        "indexes in 'global dict cache'"));
    invalidate = true;
  }

  if (m_ndb_record != nullptr) {
    dict->releaseRecord(m_ndb_record);
    m_ndb_record = nullptr;
  }
  if (m_ndb_hidden_key_record != nullptr) {
    dict->releaseRecord(m_ndb_hidden_key_record);
    m_ndb_hidden_key_record = nullptr;
  }

  dict->removeTableGlobal(*m_table, invalidate);
  m_table = nullptr;

  release_indexes(dict, invalidate);

  // NOTE! Sometimes set here but should really be reset only by trans logic
  m_trans_table_stats = nullptr;

  //  Release field to column map
  delete m_table_map;
  m_table_map = nullptr;
}

/*
  Map from thr_lock_type to NdbOperation::LockMode
*/
static inline NdbOperation::LockMode get_ndb_lock_mode(
    enum thr_lock_type type) {
  if (type >= TL_WRITE_ALLOW_WRITE) return NdbOperation::LM_Exclusive;
  if (type == TL_READ_WITH_SHARED_LOCKS) return NdbOperation::LM_Read;
  return NdbOperation::LM_CommittedRead;
}

inline bool ha_ndbcluster::has_null_in_unique_index(uint idx_no) const {
  assert(idx_no < MAX_KEY);
  return m_index[idx_no].null_in_unique_index;
}

/**
  Get the flags for an index.

  The index currently available in NDB may differ from the one defined in the
  data dictionary, if ndb_restore or ndb_drop_index has caused some component
  of it to be dropped.

  Generally, index_flags() is called after the table has been open, so that the
  NdbDictionary::Table pointer in m_table is non-null, and index_flags()
  can return the flags for the index as actually available.

  But in a small number of cases index_flags() is called without an open table.
  This happens in CREATE TABLE, where index_flags() is called from
  setup_key_part_field(). It also happens in DD code as discussed in a
  long comment at fill_dd_indexes_from_keyinfo() in dd_table.cc. And it can
  happen as the result of an information_schema or SHOW query. In these cases
  index_flags() returns the flags for the index as declared in the dictionary.
*/

ulong ha_ndbcluster::index_flags(uint idx_no, uint, bool) const {
  const NDB_INDEX_TYPE index_type =
      m_table ? get_index_type(idx_no) : get_declared_index_type(idx_no);

  switch (index_type) {
    case UNDEFINED_INDEX:
      return 0;

    case PRIMARY_KEY_INDEX:
      return HA_ONLY_WHOLE_INDEX;

    case UNIQUE_INDEX:
      return HA_ONLY_WHOLE_INDEX | HA_TABLE_SCAN_ON_NULL;

    case PRIMARY_KEY_ORDERED_INDEX:
    case UNIQUE_ORDERED_INDEX:
    case ORDERED_INDEX:
      return HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_READ_ORDER |
             HA_KEY_SCAN_NOT_ROR;
  }
  assert(false);  // unreachable
  return 0;
}

bool ha_ndbcluster::primary_key_is_clustered() const {
  if (table->s->primary_key == MAX_KEY) return false;

  /*
    NOTE 1: our ordered indexes are not really clustered
    but since accessing data when scanning index is free
    it's a good approximation

    NOTE 2: We really should consider DD attributes here too
    (for which there is IO to read data when scanning index)
    but that will need to be handled later...
  */
  const NDB_INDEX_TYPE idx_type = m_index[table->s->primary_key].type;
  return (idx_type == PRIMARY_KEY_ORDERED_INDEX ||
          idx_type == UNIQUE_ORDERED_INDEX || idx_type == ORDERED_INDEX);
}

/**
  Read one record from NDB using primary key.
*/

int ha_ndbcluster::pk_read(const uchar *key, uchar *buf, uint32 *part_id) {
  NdbConnection *trans = m_thd_ndb->trans;
  DBUG_TRACE;
  assert(trans);

  NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);

  if (check_if_pushable(NdbQueryOperationDef::PrimaryKeyAccess,
                        table->s->primary_key)) {
    // Is parent of pushed join
    assert(lm == NdbOperation::LM_CommittedRead);
    const int error =
        pk_unique_index_read_key_pushed(table->s->primary_key, key);
    if (unlikely(error)) {
      return error;
    }

    assert(m_active_query != nullptr);
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        m_active_query->getNdbError().code)
      return ndb_err(trans);

    int result = fetch_next_pushed();
    if (result == NdbQuery::NextResult_gotRow) {
      assert(pushed_cond == nullptr ||
             const_cast<Item *>(pushed_cond)->val_int());
      return 0;
    } else if (result == NdbQuery::NextResult_scanComplete) {
      return HA_ERR_KEY_NOT_FOUND;
    } else {
      return ndb_err(trans);
    }
  } else {
    const NdbOperation *op;
    if (!(op = pk_unique_index_read_key(
              table->s->primary_key, key, buf, lm,
              (m_user_defined_partitioning ? part_id : nullptr))))
      ERR_RETURN(trans->getNdbError());

    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 || op->getNdbError().code)
      return ndb_err(trans);

    if (unlikely(!m_cond.check_condition())) {
      return HA_ERR_KEY_NOT_FOUND;  // False condition
    }
    assert(pushed_cond == nullptr ||
           const_cast<Item *>(pushed_cond)->val_int());
    return 0;
  }
}

/**
  Update primary key or part id by doing delete insert.
*/

int ha_ndbcluster::ndb_pk_update_row(const uchar *old_data, uchar *new_data) {
  int error;
  DBUG_TRACE;

  DBUG_PRINT("info", ("primary key update or partition change, "
                      "doing delete+insert"));

#ifndef NDEBUG
  /*
   * 'old_data' contains columns as specified in 'read_set'.
   * All PK columns must be included for ::ndb_delete_row()
   */
  assert(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
  /*
   * As a complete 'new_data' row is reinserted after the delete,
   * all columns must be contained in the read+write union.
   */
  bitmap_copy(&m_bitmap, table->read_set);
  bitmap_union(&m_bitmap, table->write_set);
  assert(bitmap_is_set_all(&m_bitmap));
#endif

  // Delete old row
  error = ndb_delete_row(old_data, true);
  if (error) {
    DBUG_PRINT("info", ("delete failed"));
    return error;
  }

  // Insert new row
  DBUG_PRINT("info", ("delete succeded"));
  bool batched_update = (m_active_cursor != nullptr);
  /*
    If we are updating a primary key with auto_increment
    then we need to update the auto_increment counter
  */
  if (table->found_next_number_field &&
      bitmap_is_set(table->write_set,
                    table->found_next_number_field->field_index()) &&
      (error = set_auto_inc(m_thd_ndb->ndb, table->found_next_number_field))) {
    return error;
  }

  /*
    We are mapping a MySQLD PK changing update to an NdbApi delete
    and insert.
    The original PK changing update may not have written new values
    to all columns, so the write set may be partial.
    We set the write set to be all columns so that all values are
    copied from the old row to the new row.
  */
  my_bitmap_map *old_map = tmp_use_all_columns(table, table->write_set);
  error = ndb_write_row(new_data, true, batched_update);
  tmp_restore_column_map(table->write_set, old_map);

  if (error) {
    DBUG_PRINT("info", ("insert failed"));
    if (m_thd_ndb->trans->commitStatus() == NdbConnection::Started) {
      Ndb_applier *const applier = m_thd_ndb->get_applier();
      if (applier) {
        applier->atTransactionAbort();
      }
      m_thd_ndb->m_unsent_bytes = 0;
      m_thd_ndb->m_unsent_blob_ops = false;
      m_thd_ndb->m_execute_count++;
      DBUG_PRINT("info", ("execute_count: %u", m_thd_ndb->m_execute_count));
      m_thd_ndb->trans->execute(NdbTransaction::Rollback);
    }
    return error;
  }
  DBUG_PRINT("info", ("delete+insert succeeded"));

  return 0;
}

bool ha_ndbcluster::peek_index_rows_check_index_fields_in_write_set(
    const KEY *key_info) const {
  DBUG_TRACE;

  KEY_PART_INFO *key_part = key_info->key_part;
  const KEY_PART_INFO *const end = key_part + key_info->user_defined_key_parts;

  for (; key_part != end; key_part++) {
    Field *field = key_part->field;
    if (!bitmap_is_set(table->write_set, field->field_index())) {
      return false;
    }
  }

  return true;
}

/**
  Check if any operation used for the speculative "peek index rows" read has
  succeeded. Finding a successful read indicates that a conflicting key already
  exists and thus the peek has failed.

  @param trans   The transaction owning the operations to check
  @param first   First operation to check
  @param last    Last operation to check (may point to same operation as first)

  @note Function requires that at least one read operation has been defined in
        transactions.

  @return true peek succeeded, no duplicate rows was found
  @return false peek failed, at least one duplicate row was found. The number of
          the index where it was a duplicate key is available in m_dupkey.

 */
bool ha_ndbcluster::peek_index_rows_check_ops(NdbTransaction *trans,
                                              const NdbOperation *first,
                                              const NdbOperation *last) {
  DBUG_TRACE;
  ndbcluster::ndbrequire(first != nullptr);
  ndbcluster::ndbrequire(last != nullptr);

  const NdbOperation *op = first;
  while (op) {
    const NdbError err = op->getNdbError();
    if (err.status == NdbError::Success) {
      // One "peek index rows" read has succeeded, this means there is a
      // duplicate entry in the primary or unique index. Assign the number of
      // that index to m_dupkey and return error.

      switch (op->getType()) {
        case NdbOperation::PrimaryKeyAccess:
          m_dupkey = table_share->primary_key;
          break;

        case NdbOperation::UniqueIndexAccess: {
          const NdbIndexOperation *iop =
              down_cast<const NdbIndexOperation *>(op);
          const NdbDictionary::Index *index = iop->getIndex();
          // Find the number of the index
          for (uint i = 0; i < table_share->keys; i++) {
            if (m_index[i].unique_index == index) {
              m_dupkey = i;
              break;  // for
            }
          }
          break;
        }

        default:
          // Internal error, since only primary and unique indexes are peeked
          // there should never be any other type of operation in the
          // transaction
          ndbcluster::ndbrequire(false);
          break;
      }
      DBUG_PRINT("info", ("m_dupkey: %u", m_dupkey));
      return false;  // Found duplicate key
    }

    // Check that this "peek index rows" read has failed because the row could
    // not be found, otherwise the caller should report this as a NDB error
    if (err.mysql_code != HA_ERR_KEY_NOT_FOUND) {
      return false;  // Some unexpected error occurred while reading from NDB
    }

    if (op == last) {
      break;
    }

    op = trans->getNextCompletedOperation(op);
  }

  return true;  // No duplicates keys found
}

// Check if record contains any null valued columns that are part of a key
static int peek_index_rows_check_null_in_record(const KEY *key_info,
                                                const uchar *record) {
  const KEY_PART_INFO *curr_part = key_info->key_part;
  const KEY_PART_INFO *const end_part =
      curr_part + key_info->user_defined_key_parts;

  while (curr_part != end_part) {
    if (curr_part->null_bit &&
        (record[curr_part->null_offset] & curr_part->null_bit))
      return 1;
    curr_part++;
  }
  return 0;
}

/* Empty mask and dummy row, for reading no attributes using NdbRecord. */
/* Mask will be initialized to all zeros by linker. */
static unsigned char empty_mask[(NDB_MAX_ATTRIBUTES_IN_TABLE + 7) / 8];
static char dummy_row[1];

/**
  Peek to check if any rows already exist with conflicting
  primary key or unique index values
*/

int ha_ndbcluster::peek_indexed_rows(const uchar *record,
                                     NDB_WRITE_OP write_op) {
  DBUG_TRACE;

  int error;
  NdbTransaction *trans;
  if (unlikely(!(trans = get_transaction(error)))) {
    return error;
  }
  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);

  const NdbOperation *first = nullptr;
  const NdbOperation *last = nullptr;
  if (write_op != NDB_UPDATE && table_share->primary_key != MAX_KEY) {
    // Define speculative read of row with colliding primary key
    const NdbRecord *key_rec =
        m_index[table->s->primary_key].ndb_unique_record_row;

    NdbOperation::OperationOptions options;
    NdbOperation::OperationOptions *poptions = nullptr;
    options.optionsPresent = 0;

    if (m_user_defined_partitioning) {
      uint32 part_id;
      longlong func_value;
      my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
      const int part_id_error =
          m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
      dbug_tmp_restore_column_map(table->read_set, old_map);
      if (part_id_error) {
        m_part_info->err_value = func_value;
        return part_id_error;
      }
      options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId = part_id;
      poptions = &options;
    }

    const NdbOperation *const op = trans->readTuple(
        key_rec, (const char *)record, m_ndb_record, dummy_row, lm, empty_mask,
        poptions, sizeof(NdbOperation::OperationOptions));
    if (op == nullptr) {
      ERR_RETURN(trans->getNdbError());
    }

    first = op;
    last = op;
  }

  // Define speculative read of colliding row(s) in unique indexes
  const KEY *key_info = table->key_info;
  for (uint i = 0; i < table_share->keys; i++, key_info++) {
    if (i == table_share->primary_key) {
      DBUG_PRINT("info", ("skip primary key"));
      continue;
    }

    if (key_info->flags & HA_NOSAME &&
        bitmap_is_overlapping(table->write_set, m_key_fields[i])) {
      // Unique index being written

      /*
        It's not possible to lookup a NULL field value in a unique index. But
        since keys with NULLs are not indexed, such rows cannot conflict anyway
        -> just skip checking the index in that case.
      */
      if (peek_index_rows_check_null_in_record(key_info, record)) {
        DBUG_PRINT("info", ("skipping check for key with NULL"));
        continue;
      }

      if (write_op != NDB_INSERT &&
          !peek_index_rows_check_index_fields_in_write_set(key_info)) {
        DBUG_PRINT("info", ("skipping check for key %u not in write_set", i));
        continue;
      }

      const NdbRecord *const key_rec = m_index[i].ndb_unique_record_row;
      const NdbOperation *const iop =
          trans->readTuple(key_rec, (const char *)record, m_ndb_record,
                           dummy_row, lm, empty_mask);
      if (iop == nullptr) {
        ERR_RETURN(trans->getNdbError());
      }

      if (!first) first = iop;
      last = iop;
    }
  }

  if (first == nullptr) {
    // Table has no keys
    return HA_ERR_KEY_NOT_FOUND;
  }

  (void)execute_no_commit_ie(m_thd_ndb, trans);

  const NdbError ndberr = trans->getNdbError();
  error = ndberr.mysql_code;
  if ((error != 0 && error != HA_ERR_KEY_NOT_FOUND) ||
      peek_index_rows_check_ops(trans, first, last)) {
    return ndb_err(trans);
  }
  return 0;
}

/**
  Read one record from NDB using unique secondary index.
*/

int ha_ndbcluster::unique_index_read(const uchar *key, uchar *buf) {
  NdbTransaction *trans = m_thd_ndb->trans;
  NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);
  DBUG_TRACE;
  DBUG_PRINT("enter", ("index: %u, lm: %u", active_index, (unsigned int)lm));
  assert(trans);

  if (check_if_pushable(NdbQueryOperationDef::UniqueIndexAccess,
                        active_index)) {
    assert(lm == NdbOperation::LM_CommittedRead);
    const int error = pk_unique_index_read_key_pushed(active_index, key);
    if (unlikely(error)) {
      return error;
    }

    assert(m_active_query != nullptr);
    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 ||
        m_active_query->getNdbError().code)
      return ndb_err(trans);

    int result = fetch_next_pushed();
    if (result == NdbQuery::NextResult_gotRow) {
      assert(pushed_cond == nullptr ||
             const_cast<Item *>(pushed_cond)->val_int());
      return 0;
    } else if (result == NdbQuery::NextResult_scanComplete) {
      return HA_ERR_KEY_NOT_FOUND;
    } else {
      return ndb_err(trans);
    }
  } else {
    const NdbOperation *op;

    if (!(op = pk_unique_index_read_key(active_index, key, buf, lm, nullptr)))
      ERR_RETURN(trans->getNdbError());

    if (execute_no_commit_ie(m_thd_ndb, trans) != 0 || op->getNdbError().code) {
      return ndb_err(trans);
    }

    if (unlikely(!m_cond.check_condition())) {
      return HA_ERR_KEY_NOT_FOUND;
    }
    assert(pushed_cond == nullptr ||
           const_cast<Item *>(pushed_cond)->val_int());
    return 0;
  }
}

int ha_ndbcluster::scan_handle_lock_tuple(NdbScanOperation *scanOp,
                                          NdbTransaction *trans) {
  DBUG_TRACE;
  if (m_lock_tuple) {
    /*
      Lock level m_lock.type either TL_WRITE_ALLOW_WRITE
      (SELECT FOR UPDATE) or TL_READ_WITH_SHARED_LOCKS (SELECT
      LOCK WITH SHARE MODE) and row was not explicitly unlocked
      with unlock_row() call
    */
    DBUG_PRINT("info", ("Keeping lock on scanned row"));

    if (!(scanOp->lockCurrentTuple(trans, m_ndb_record, dummy_row,
                                   empty_mask))) {
      m_lock_tuple = false;
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
        THDVAR(current_thd, log_exclusive_reads)) {
      if (scan_log_exclusive_read(scanOp, trans)) {
        m_lock_tuple = false;
        ERR_RETURN(trans->getNdbError());
      }
    }

    m_thd_ndb->m_unsent_bytes += 12;
    m_lock_tuple = false;
  }
  return 0;
}

/*
  Some MySQL table locks are mapped to Ndb internal exclusive
  row locks to achieve part of the table locking semantics. If rows are
  not exclusively locked a new batch of rows need to be fetched.
 */
static bool table_lock_not_mapped_to_row_lock(enum thr_lock_type lock_type) {
  return (lock_type < TL_READ_NO_INSERT &&
          lock_type != TL_READ_WITH_SHARED_LOCKS);
}

inline int ha_ndbcluster::fetch_next(NdbScanOperation *cursor) {
  DBUG_TRACE;
  int local_check;
  int error;
  NdbTransaction *trans = m_thd_ndb->trans;

  assert(trans);
  if ((error = scan_handle_lock_tuple(cursor, trans)) != 0) return error;

  bool contact_ndb = table_lock_not_mapped_to_row_lock(m_lock.type);
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    /*
      We can only handle one tuple with blobs at a time.
    */
    if (m_thd_ndb->m_unsent_blob_ops) {
      if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
        return ndb_err(trans);
    }

    /* Should be no unexamined completed operations
       nextResult() on Blobs generates Blob part read ops,
       so we will free them here
    */
    trans->releaseCompletedOpsAndQueries();

    if ((local_check = cursor->nextResult(&_m_next_row, contact_ndb,
                                          m_thd_ndb->m_force_send)) == 0) {
      /*
        Explicitly lock tuple if "select for update" or
        "select lock in share mode"
      */
      m_lock_tuple = (m_lock.type == TL_WRITE_ALLOW_WRITE ||
                      m_lock.type == TL_READ_WITH_SHARED_LOCKS);
      return 0;
    } else if (local_check == 1 || local_check == 2) {
      // 1: No more records
      // 2: No more cached records

      /*
        Before fetching more rows and releasing lock(s),
        all pending update or delete operations should
        be sent to NDB
      */
      DBUG_PRINT("info", ("thd_ndb->m_unsent_bytes: %ld",
                          (long)m_thd_ndb->m_unsent_bytes));
      if (m_thd_ndb->m_unsent_bytes) {
        if ((error = flush_bulk_insert()) != 0) return error;
      }
      contact_ndb = (local_check == 2);
    } else {
      return ndb_err(trans);
    }
  } while (local_check == 2);

  return 1;
}

int ha_ndbcluster::fetch_next_pushed() {
  DBUG_TRACE;
  assert(m_pushed_operation);

  /**
   * Only prepare result & status from this operation in pushed join.
   * Consecutive rows are prepared through ::index_read_pushed() and
   * ::index_next_pushed() which unpack and set correct status for each row.
   */
  NdbQuery::NextResultOutcome result;
  while ((result = m_pushed_operation->nextResult(
              true, m_thd_ndb->m_force_send)) == NdbQuery::NextResult_gotRow) {
    assert(m_next_row != nullptr);
    DBUG_PRINT("info", ("One more record found"));
    const int ignore =
        unpack_record_and_set_generated_fields(table->record[0], m_next_row);
    //  m_thd_ndb->m_pushed_reads++;
    if (likely(!ignore)) {
      return NdbQuery::NextResult_gotRow;
    }
  }
  if (likely(result == NdbQuery::NextResult_scanComplete)) {
    assert(m_next_row == nullptr);
    DBUG_PRINT("info", ("No more records"));
    //  m_thd_ndb->m_pushed_reads++;
    return result;
  }
  DBUG_PRINT("info", ("Error from 'nextResult()'"));
  return ndb_err(m_thd_ndb->trans);
}

/**
  Get the first record from an indexed table access being a child
  operation in a pushed join. Fetch will be from prefetched
  cached records which are materialized into the bound buffer
  areas as result of this call.
*/

int ha_ndbcluster::index_read_pushed(uchar *buf, const uchar *key,
                                     key_part_map keypart_map) {
  DBUG_TRACE;

  // Handler might have decided to not execute the pushed joins which has been
  // prepared In this case we do an unpushed index_read based on 'Plain old'
  // NdbOperations
  if (unlikely(!check_is_pushed())) {
    int res = index_read_map(buf, key, keypart_map, HA_READ_KEY_EXACT);
    return res;
  }

  assert(m_pushed_join_operation > PUSHED_ROOT);  // Child of a pushed join
  assert(m_active_query == nullptr);

  // Might need to re-establish first result row (wrt. its parents which may
  // have been navigated)
  NdbQuery::NextResultOutcome result = m_pushed_operation->firstResult();

  // Result from pushed operation will be referred by 'm_next_row' if non-NULL
  if (result == NdbQuery::NextResult_gotRow) {
    assert(m_next_row != nullptr);
    const int ignore = unpack_record_and_set_generated_fields(buf, m_next_row);
    m_thd_ndb->m_pushed_reads++;

    // Pushed join results are Ref-compared using the correlation key, not
    // the specified key (unless where it is not push-executed after all).
    // Check that we still returned a row matching the specified key.
    assert(key_cmp_if_same(
               table, key, active_index,
               calculate_key_len(table, active_index, keypart_map)) == 0);

    if (unlikely(ignore)) {
      return index_next_pushed(buf);
    }
    return 0;
  }
  assert(result != NdbQuery::NextResult_gotRow);
  DBUG_PRINT("info", ("No record found"));
  return HA_ERR_END_OF_FILE;
}

/**
  Get the next record from an indexes table access being a child
  operation in a pushed join. Fetch will be from prefetched
  cached records which are materialized into the bound buffer
  areas as result of this call.
*/
int ha_ndbcluster::index_next_pushed(uchar *buf) {
  DBUG_TRACE;

  // Handler might have decided to not execute the pushed joins which has been
  // prepared In this case we do an unpushed index_read based on 'Plain old'
  // NdbOperations
  if (unlikely(!check_is_pushed())) {
    int res = index_next(buf);
    return res;
  }

  assert(m_pushed_join_operation > PUSHED_ROOT);  // Child of a pushed join
  assert(m_active_query == nullptr);

  int res = fetch_next_pushed();
  if (res == NdbQuery::NextResult_gotRow) {
    assert(pushed_cond == nullptr ||
           const_cast<Item *>(pushed_cond)->val_int());
    return 0;
  } else if (res == NdbQuery::NextResult_scanComplete) {
    return HA_ERR_END_OF_FILE;
  }
  return ndb_err(m_thd_ndb->trans);
}

/**
  Get the next record of a started scan. Try to fetch
  it locally from NdbApi cached records if possible,
  otherwise ask NDB for more.

  @note
    If this is a update/delete make sure to not contact
    NDB before any pending ops have been sent to NDB.
*/

inline int ha_ndbcluster::next_result(uchar *buf) {
  int res;
  DBUG_TRACE;

  if (m_active_cursor) {
    while ((res = fetch_next(m_active_cursor)) == 0) {
      DBUG_PRINT("info", ("One more record found"));

      const int ignore = unpack_record(buf, m_next_row);
      if (likely(!ignore)) {
        assert(pushed_cond == nullptr ||
               const_cast<Item *>(pushed_cond)->val_int());
        return 0;  // Found a row
      }
    }
    // No rows found, or error
    if (res == 1) {
      // No more records
      DBUG_PRINT("info", ("No more records"));

      if (m_thd_ndb->sql_command() == SQLCOM_ALTER_TABLE) {
        // Detected end of scan for copying ALTER TABLE. Check commit_count of
        // the scanned (source) table in order to detect that no concurrent
        // changes has occurred.
        DEBUG_SYNC(table->in_use, "ndb.before_commit_count_check");

        if (int error =
                copying_alter.check_saved_commit_count(m_thd_ndb, m_table)) {
          return error;
        }
        DEBUG_SYNC(table->in_use, "ndb.after_commit_count_check");
      }

      return HA_ERR_END_OF_FILE;
    }
    return ndb_err(m_thd_ndb->trans);
  } else if (m_active_query) {
    res = fetch_next_pushed();
    if (res == NdbQuery::NextResult_gotRow) {
      assert(pushed_cond == nullptr ||
             const_cast<Item *>(pushed_cond)->val_int());
      return 0;  // Found a row
    } else if (res == NdbQuery::NextResult_scanComplete) {
      return HA_ERR_END_OF_FILE;
    }
    return ndb_err(m_thd_ndb->trans);
  }
  return HA_ERR_END_OF_FILE;
}

int ha_ndbcluster::log_exclusive_read(const NdbRecord *key_rec,
                                      const uchar *key, uchar *buf,
                                      Uint32 *ppartition_id) const {
  DBUG_TRACE;
  NdbOperation::OperationOptions opts;
  opts.optionsPresent = NdbOperation::OperationOptions::OO_ABORTOPTION |
                        NdbOperation::OperationOptions::OO_ANYVALUE;

  /* If the key does not exist, that is ok */
  opts.abortOption = NdbOperation::AO_IgnoreError;

  /*
     Mark the AnyValue as a read operation, so that the update
     is processed
  */
  opts.anyValue = 0;
  ndbcluster_anyvalue_set_read_op(opts.anyValue);

  if (ppartition_id != nullptr) {
    assert(m_user_defined_partitioning);
    opts.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
    opts.partitionId = *ppartition_id;
  }

  const NdbOperation *markingOp = m_thd_ndb->trans->updateTuple(
      key_rec, (const char *)key, m_ndb_record, (char *)buf, empty_mask, &opts,
      opts.size());
  if (!markingOp) {
    char msg[FN_REFLEN];
    snprintf(
        msg, sizeof(msg),
        "Error logging exclusive reads, failed creating markingOp, %u, %s\n",
        m_thd_ndb->trans->getNdbError().code,
        m_thd_ndb->trans->getNdbError().message);
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_EXCEPTIONS_WRITE_ERROR,
                        ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
    /*
      By returning -1 the caller (pk_unique_index_read_key) will return
      NULL and error on transaction object will be returned.
    */
    return -1;
  }

  return 0;
}

int ha_ndbcluster::scan_log_exclusive_read(NdbScanOperation *cursor,
                                           NdbTransaction *trans) const {
  DBUG_TRACE;
  NdbOperation::OperationOptions opts;
  opts.optionsPresent = NdbOperation::OperationOptions::OO_ANYVALUE;

  /*
     Mark the AnyValue as a read operation, so that the update
     is processed
  */
  opts.anyValue = 0;
  ndbcluster_anyvalue_set_read_op(opts.anyValue);

  const NdbOperation *markingOp =
      cursor->updateCurrentTuple(trans, m_ndb_record, dummy_row, empty_mask,
                                 &opts, sizeof(NdbOperation::OperationOptions));
  if (markingOp == nullptr) {
    char msg[FN_REFLEN];
    snprintf(msg, sizeof(msg),
             "Error logging exclusive reads during scan, failed creating "
             "markingOp, %u, %s\n",
             m_thd_ndb->trans->getNdbError().code,
             m_thd_ndb->trans->getNdbError().message);
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_EXCEPTIONS_WRITE_ERROR,
                        ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
    return -1;
  }

  return 0;
}

/**
  Do a primary key or unique key index read operation.
  The key value is taken from a buffer in mysqld key format.
*/
const NdbOperation *ha_ndbcluster::pk_unique_index_read_key(
    uint idx, const uchar *key, uchar *buf, NdbOperation::LockMode lm,
    Uint32 *ppartition_id) {
  DBUG_TRACE;
  const NdbOperation *op;
  const NdbRecord *key_rec;
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = nullptr;
  options.optionsPresent = 0;
  NdbOperation::GetValueSpec gets[2];

  assert(m_thd_ndb->trans);

  DBUG_PRINT("info", ("pk_unique_index_read_key of table %s",
                      table->s->table_name.str));

  if (idx != MAX_KEY)
    key_rec = m_index[idx].ndb_unique_record_key;
  else
    key_rec = m_ndb_hidden_key_record;

  /* Initialize the null bitmap, setting unused null bits to 1. */
  memset(buf, 0xff, table->s->null_bytes);

  if (table_share->primary_key == MAX_KEY) {
    get_hidden_fields_keyop(&options, gets);
    poptions = &options;
  }

  if (ppartition_id != nullptr) {
    assert(m_user_defined_partitioning);
    options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId = *ppartition_id;
    poptions = &options;
  }

  /*
    We prepared a ScanFilter. However it turns out that we will
    do a primary/unique key readTuple which does not use ScanFilter (yet)
    We set up the handler to evaluate the condition itself
  */
  m_cond.set_condition(pushed_cond);

  get_read_set(false, idx);
  op = m_thd_ndb->trans->readTuple(
      key_rec, (const char *)key, m_ndb_record, (char *)buf, lm,
      m_table_map->get_column_mask(table->read_set), poptions,
      sizeof(NdbOperation::OperationOptions));

  if (uses_blob_value(table->read_set) &&
      get_blob_values(op, buf, table->read_set) != 0)
    return nullptr;

  /* Perform 'empty update' to mark the read in the binlog, iff required */
  /*
   * Lock_mode = exclusive
   * Index = primary or unique (always true inside this method)
   * Index is not the hidden primary key
   * Session_state = marking_exclusive_reads
   * THEN
   * issue updateTuple with AnyValue explicitly set
   */
  if ((lm == NdbOperation::LM_Exclusive) && idx != MAX_KEY &&
      THDVAR(current_thd, log_exclusive_reads)) {
    if (log_exclusive_read(key_rec, key, buf, ppartition_id) != 0)
      return nullptr;
  }

  return op;
}

static bool is_shrinked_varchar(const Field *field) {
  if (field->real_type() == MYSQL_TYPE_VARCHAR) {
    if (field->get_length_bytes() == 1) return true;
  }

  return false;
}

int ha_ndbcluster::pk_unique_index_read_key_pushed(uint idx, const uchar *key) {
  DBUG_TRACE;
  assert(m_thd_ndb->trans);
  assert(idx < MAX_KEY);

  if (m_active_query) {
    m_active_query->close(false);
    m_active_query = nullptr;
  }

  KEY *key_def = &table->key_info[idx];
  KEY_PART_INFO *key_part;

  uint i;
  Uint32 offset = 0;
  NdbQueryParamValue paramValues[ndb_pushed_join::MAX_KEY_PART];
  assert(key_def->user_defined_key_parts <= ndb_pushed_join::MAX_KEY_PART);

  uint map[ndb_pushed_join::MAX_KEY_PART];
  m_index[idx].fill_column_map(key_def, map);

  // Bind key values defining root of pushed join
  for (i = 0, key_part = key_def->key_part; i < key_def->user_defined_key_parts;
       i++, key_part++) {
    bool shrinkVarChar = is_shrinked_varchar(key_part->field);

    if (key_part->null_bit)  // Column is nullable
    {
      assert(idx != table_share->primary_key);  // PK can't be nullable
      assert(*(key + offset) == 0);  // Null values not allowed in key
                                     // Value is imm. after NULL indicator
      paramValues[map[i]] = NdbQueryParamValue(key + offset + 1, shrinkVarChar);
    } else  // Non-nullable column
    {
      paramValues[map[i]] = NdbQueryParamValue(key + offset, shrinkVarChar);
    }
    offset += key_part->store_length;
  }

  const int ret =
      create_pushed_join(paramValues, key_def->user_defined_key_parts);
  return ret;
}

/** Count number of columns in key part. */
static uint count_key_columns(const KEY *key_info, const key_range *key) {
  KEY_PART_INFO *first_key_part = key_info->key_part;
  KEY_PART_INFO *key_part_end =
      first_key_part + key_info->user_defined_key_parts;
  KEY_PART_INFO *key_part;
  uint length = 0;
  for (key_part = first_key_part; key_part < key_part_end; key_part++) {
    if (length >= key->length) break;
    length += key_part->store_length;
  }
  return (uint)(key_part - first_key_part);
}

/* Helper method to compute NDB index bounds. Note: does not set range_no. */
/* Stats queries may differ so add "from" 0:normal 1:RIR 2:RPK. */
void compute_index_bounds(NdbIndexScanOperation::IndexBound &bound,
                          const KEY *key_info, const key_range *start_key,
                          const key_range *end_key, int from) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("from: %d", from));

#ifndef NDEBUG
  DBUG_PRINT("info", ("key parts: %u length: %u",
                      key_info->user_defined_key_parts, key_info->key_length));
  {
    for (uint j = 0; j <= 1; j++) {
      const key_range *kr = (j == 0 ? start_key : end_key);
      if (kr) {
        DBUG_PRINT("info", ("key range %u: length: %u map: %lx flag: %d", j,
                            kr->length, kr->keypart_map, kr->flag));
        DBUG_DUMP("key", kr->key, kr->length);
      } else {
        DBUG_PRINT("info", ("key range %u: none", j));
      }
    }
  }
#endif

  if (start_key) {
    bound.low_key = (const char *)start_key->key;
    bound.low_key_count = count_key_columns(key_info, start_key);
    bound.low_inclusive = start_key->flag != HA_READ_AFTER_KEY &&
                          start_key->flag != HA_READ_BEFORE_KEY;
  } else {
    bound.low_key = nullptr;
    bound.low_key_count = 0;
  }

  /* RIR query for x >= 1 inexplicably passes HA_READ_KEY_EXACT. */
  if (start_key &&
      (start_key->flag == HA_READ_KEY_EXACT ||
       start_key->flag == HA_READ_PREFIX_LAST) &&
      from != 1) {
    bound.high_key = bound.low_key;
    bound.high_key_count = bound.low_key_count;
    bound.high_inclusive = true;
  } else if (end_key) {
    bound.high_key = (const char *)end_key->key;
    bound.high_key_count = count_key_columns(key_info, end_key);
    /*
      For some reason, 'where b >= 1 and b <= 3' uses HA_READ_AFTER_KEY for
      the end_key.
      So HA_READ_AFTER_KEY in end_key sets high_inclusive, even though in
      start_key it does not set low_inclusive.
    */
    bound.high_inclusive = end_key->flag != HA_READ_BEFORE_KEY;
    if (end_key->flag == HA_READ_KEY_EXACT ||
        end_key->flag == HA_READ_PREFIX_LAST) {
      bound.low_key = bound.high_key;
      bound.low_key_count = bound.high_key_count;
      bound.low_inclusive = true;
    }
  } else {
    bound.high_key = nullptr;
    bound.high_key_count = 0;
  }
  DBUG_PRINT(
      "info",
      ("start_flag=%d end_flag=%d"
       " lo_keys=%d lo_incl=%d hi_keys=%d hi_incl=%d",
       start_key ? start_key->flag : 0, end_key ? end_key->flag : 0,
       bound.low_key_count, bound.low_key_count ? bound.low_inclusive : 0,
       bound.high_key_count, bound.high_key_count ? bound.high_inclusive : 0));
}

/**
  Start ordered index scan in NDB
*/

int ha_ndbcluster::ordered_index_scan(const key_range *start_key,
                                      const key_range *end_key, bool sorted,
                                      bool descending, uchar *buf,
                                      part_id_range *part_spec) {
  NdbTransaction *trans;
  NdbIndexScanOperation *op;
  int error;

  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("index: %u, sorted: %d, descending: %d read_set=0x%x",
              active_index, sorted, descending, table->read_set->bitmap[0]));
  DBUG_PRINT("enter",
             ("Starting new ordered scan on %s", table_share->table_name.str));

  if (unlikely(!(trans = get_transaction(error)))) {
    return error;
  }

  if ((error = close_scan())) return error;

  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);

  const NdbRecord *key_rec = m_index[active_index].ndb_record_key;
  const NdbRecord *row_rec = m_ndb_record;

  NdbIndexScanOperation::IndexBound bound;
  NdbIndexScanOperation::IndexBound *pbound = nullptr;
  if (start_key != nullptr || end_key != nullptr) {
    /*
       Compute bounds info, reversing range boundaries
       if descending
     */
    compute_index_bounds(bound, table->key_info + active_index,
                         (descending ? end_key : start_key),
                         (descending ? start_key : end_key), 0);
    bound.range_no = 0;
    pbound = &bound;
  }

  if (check_if_pushable(NdbQueryOperationDef::OrderedIndexScan, active_index)) {
    const int error = create_pushed_join();
    if (unlikely(error)) return error;

    NdbQuery *const query = m_active_query;
    if (sorted &&
        query->getQueryOperation((uint)PUSHED_ROOT)
            ->setOrdering(descending
                              ? NdbQueryOptions::ScanOrdering_descending
                              : NdbQueryOptions::ScanOrdering_ascending)) {
      ERR_RETURN(query->getNdbError());
    }

    if (pbound && query->setBound(key_rec, pbound) != 0)
      ERR_RETURN(query->getNdbError());

    m_thd_ndb->m_scan_count++;

    bool prunable = false;
    if (unlikely(query->isPrunable(prunable) != 0))
      ERR_RETURN(query->getNdbError());
    if (prunable) m_thd_ndb->m_pruned_scan_count++;

    // Can't have BLOB in pushed joins (yet)
    assert(!uses_blob_value(table->read_set));
  } else {
    NdbScanOperation::ScanOptions options;
    options.optionsPresent = NdbScanOperation::ScanOptions::SO_SCANFLAGS;
    options.scan_flags = 0;

    NdbOperation::GetValueSpec gets[2];
    if (table_share->primary_key == MAX_KEY)
      get_hidden_fields_scan(&options, gets);

    if (lm == NdbOperation::LM_Read)
      options.scan_flags |= NdbScanOperation::SF_KeyInfo;
    if (sorted) options.scan_flags |= NdbScanOperation::SF_OrderByFull;
    if (descending) options.scan_flags |= NdbScanOperation::SF_Descending;

    /* Partition pruning */
    if (m_use_partition_pruning && m_user_defined_partitioning &&
        part_spec != nullptr && part_spec->start_part == part_spec->end_part) {
      /* Explicitly set partition id when pruning User-defined partitioned scan
       */
      options.partitionId = part_spec->start_part;
      options.optionsPresent |= NdbScanOperation::ScanOptions::SO_PARTITION_ID;
    }

    NdbInterpretedCode code(m_table);
    generate_scan_filter(&code, &options);

    get_read_set(true, active_index);
    if (!(op = trans->scanIndex(key_rec, row_rec, lm,
                                m_table_map->get_column_mask(table->read_set),
                                pbound, &options,
                                sizeof(NdbScanOperation::ScanOptions))))
      ERR_RETURN(trans->getNdbError());

    DBUG_PRINT("info",
               ("Is scan pruned to 1 partition? : %u", op->getPruned()));
    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (op->getPruned() ? 1 : 0);

    if (uses_blob_value(table->read_set) &&
        get_blob_values(op, nullptr, table->read_set) != 0)
      ERR_RETURN(op->getNdbError());

    m_active_cursor = op;
  }

  if (sorted) {
    m_thd_ndb->m_sorted_scan_count++;
  }

  if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
    return ndb_err(trans);

  return next_result(buf);
}

static int guess_scan_flags(NdbOperation::LockMode lm, Ndb_table_map *table_map,
                            const NDBTAB *tab, const MY_BITMAP *readset) {
  int flags = 0;
  flags |= (lm == NdbOperation::LM_Read) ? NdbScanOperation::SF_KeyInfo : 0;
  if (tab->checkColumns(nullptr, 0) & 2) {
    const Uint32 *colmap = (const Uint32 *)table_map->get_column_mask(readset);
    int ret = tab->checkColumns(colmap, no_bytes_in_map(readset));

    if (ret & 2) {  // If disk columns...use disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    } else if ((ret & 4) == 0 && (lm == NdbOperation::LM_Exclusive)) {
      // If no mem column is set and exclusive...guess disk scan
      flags |= NdbScanOperation::SF_DiskScan;
    }
  }
  return flags;
}

/*
  Start full table scan in NDB or unique index scan
 */

int ha_ndbcluster::full_table_scan(const KEY *key_info,
                                   const key_range *start_key,
                                   const key_range *end_key, uchar *buf) {
  THD *thd = table->in_use;
  int error;
  NdbTransaction *trans = m_thd_ndb->trans;
  part_id_range part_spec;
  bool use_set_part_id = false;
  NdbOperation::GetValueSpec gets[2];

  DBUG_TRACE;
  DBUG_PRINT("enter", ("Starting new scan on %s", table_share->table_name.str));

  if (m_use_partition_pruning && m_user_defined_partitioning) {
    assert(m_pushed_join_operation != PUSHED_ROOT);
    part_spec.start_part = 0;
    part_spec.end_part = m_part_info->get_tot_partitions() - 1;
    prune_partition_set(table, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
    */
    if (part_spec.start_part > part_spec.end_part) {
      return HA_ERR_END_OF_FILE;
    }

    if (part_spec.start_part == part_spec.end_part) {
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
      use_set_part_id = true;
      if (!trans)
        if (unlikely(!(
                trans = get_transaction_part_id(part_spec.start_part, error))))
          return error;
    }
  }
  if (!trans)
    if (unlikely(!(trans = start_transaction(error)))) return error;

  /*
    If the scan is part of an ALTER TABLE we need exclusive locks on rows
    to block parallel updates from other connections to Ndb.
   */
  const NdbOperation::LockMode lm = (thd_sql_command(thd) == SQLCOM_ALTER_TABLE)
                                        ? NdbOperation::LM_Exclusive
                                        : get_ndb_lock_mode(m_lock.type);
  NdbScanOperation::ScanOptions options;
  options.optionsPresent = (NdbScanOperation::ScanOptions::SO_SCANFLAGS |
                            NdbScanOperation::ScanOptions::SO_PARALLEL);
  options.scan_flags =
      guess_scan_flags(lm, m_table_map, m_table, table->read_set);
  options.parallel = DEFAULT_PARALLELISM;

  if (use_set_part_id) {
    assert(m_user_defined_partitioning);
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_PARTITION_ID;
    options.partitionId = part_spec.start_part;
  };

  if (table_share->primary_key == MAX_KEY)
    get_hidden_fields_scan(&options, gets);

  if (check_if_pushable(NdbQueryOperationDef::TableScan)) {
    const int error = create_pushed_join();
    if (unlikely(error)) return error;

    m_thd_ndb->m_scan_count++;
    // Can't have BLOB in pushed joins (yet)
    assert(!uses_blob_value(table->read_set));
  } else {
    NdbScanOperation *op;
    NdbInterpretedCode code(m_table);

    if (!key_info) {
      generate_scan_filter(&code, &options);
    } else {
      /* Unique index scan in NDB (full table scan with scan filter) */
      DBUG_PRINT("info", ("Starting unique index scan"));
      if (generate_scan_filter_with_key(&code, &options, key_info, start_key,
                                        end_key))
        ERR_RETURN(code.getNdbError());
    }

    get_read_set(true, MAX_KEY);
    if (!(op = trans->scanTable(
              m_ndb_record, lm, m_table_map->get_column_mask(table->read_set),
              &options, sizeof(NdbScanOperation::ScanOptions))))
      ERR_RETURN(trans->getNdbError());

    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (op->getPruned() ? 1 : 0);

    assert(m_active_cursor == nullptr);
    m_active_cursor = op;

    if (uses_blob_value(table->read_set) &&
        get_blob_values(op, nullptr, table->read_set) != 0)
      ERR_RETURN(op->getNdbError());
  }  // if (check_if_pushable(NdbQueryOperationDef::TableScan))

  if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0)
    return ndb_err(trans);
  DBUG_PRINT("exit", ("Scan started successfully"));
  return next_result(buf);
}  // ha_ndbcluster::full_table_scan()

int ha_ndbcluster::set_auto_inc(Ndb *ndb, Field *field) {
  DBUG_TRACE;
  bool read_bit = bitmap_is_set(table->read_set, field->field_index());
  bitmap_set_bit(table->read_set, field->field_index());
  Uint64 next_val = (Uint64)field->val_int() + 1;
  if (!read_bit) bitmap_clear_bit(table->read_set, field->field_index());
  return set_auto_inc_val(ndb, next_val);
}

inline int ha_ndbcluster::set_auto_inc_val(Ndb *ndb, Uint64 value) const {
  DBUG_TRACE;
  DBUG_PRINT("info", ("Trying to set auto increment value to %llu", value));
  {
    NDB_SHARE::Tuple_id_range_guard g(m_share);

    if (ndb->checkUpdateAutoIncrementValue(g.range, value)) {
      if (ndb->setAutoIncrementValue(m_table, g.range, value, true) == -1) {
        ERR_RETURN(ndb->getNdbError());
      }
    }
  }
  return 0;
}

void ha_ndbcluster::get_read_set(bool use_cursor, uint idx [[maybe_unused]]) {
  const bool is_delete = table->in_use->lex->sql_command == SQLCOM_DELETE ||
                         table->in_use->lex->sql_command == SQLCOM_DELETE_MULTI;

  const bool is_update = table->in_use->lex->sql_command == SQLCOM_UPDATE ||
                         table->in_use->lex->sql_command == SQLCOM_UPDATE_MULTI;

  /**
   * Any fields referred from an unpushed condition is not guaranteed to
   * be included in the read_set requested by server. Thus, ha_ndbcluster
   * has to make sure they are read.
   */
  m_cond.add_read_set(table);

#ifndef NDEBUG
  /**
   * In DEBUG build we also need to include all fields referred
   * from the assert:
   *
   *  assert(pushed_cond == nullptr || ((Item*)pushed_cond)->val_int())
   */
  m_cond.add_read_set(table, pushed_cond);
#endif

  if (!is_delete && !is_update) {
    return;
  }

  assert(use_cursor || idx == MAX_KEY || idx == table_share->primary_key ||
         table->key_info[idx].flags & HA_NOSAME);

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
  if (m_read_before_write_removal_used) {
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
  if (bitmap_is_overlapping(table->write_set, m_pk_bitmap_p)) {
    assert(table_share->primary_key != MAX_KEY);
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
   *     to delete BLOB stored in separate fragments.
   *  3) When updating BLOB columns PK is required to delete
   *     old BLOB + insert new BLOB contents
   */
  else if (!use_cursor ||                              // 1)
           (is_delete && table_share->blob_fields) ||  // 2)
           uses_blob_value(table->write_set))          // 3)
  {
    bitmap_union(table->read_set, m_pk_bitmap_p);
  }

  /**
   * If update/delete use partition pruning, we need
   * to read the column values which being part of the
   * partition spec as they are used by
   * ::get_parts_for_update() / ::get_parts_for_delete()
   * Part. columns are always part of PK, so we only
   * have to do this if pk_bitmap wasn't added yet,
   */
  else if (m_use_partition_pruning)  // && m_user_defined_partitioning)
  {
    assert(bitmap_is_subset(&m_part_info->full_part_field_set, m_pk_bitmap_p));
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
   *       deferring reading of the column values
   *       until formatting the error message.
   */
  if (is_update && m_has_unique_index) {
    for (uint i = 0; i < table_share->keys; i++) {
      if ((table->key_info[i].flags & HA_NOSAME) &&
          bitmap_is_overlapping(table->write_set, m_key_fields[i])) {
        bitmap_union(table->read_set, m_key_fields[i]);
      }
    }
  }
}

Uint32 ha_ndbcluster::setup_get_hidden_fields(
    NdbOperation::GetValueSpec gets[2]) {
  Uint32 num_gets = 0;
  /*
    We need to read the hidden primary key, and possibly the FRAGMENT
    pseudo-column.
  */
  gets[num_gets].column = get_hidden_key_column();
  gets[num_gets].appStorage = &m_ref;
  num_gets++;
  if (m_user_defined_partitioning) {
    /* Need to read partition id to support ORDER BY columns. */
    gets[num_gets].column = NdbDictionary::Column::FRAGMENT;
    gets[num_gets].appStorage = &m_part_id;
    num_gets++;
  }
  return num_gets;
}

void ha_ndbcluster::get_hidden_fields_keyop(
    NdbOperation::OperationOptions *options,
    NdbOperation::GetValueSpec gets[2]) {
  Uint32 num_gets = setup_get_hidden_fields(gets);
  options->optionsPresent |= NdbOperation::OperationOptions::OO_GETVALUE;
  options->extraGetValues = gets;
  options->numExtraGetValues = num_gets;
}

void ha_ndbcluster::get_hidden_fields_scan(
    NdbScanOperation::ScanOptions *options,
    NdbOperation::GetValueSpec gets[2]) {
  Uint32 num_gets = setup_get_hidden_fields(gets);
  options->optionsPresent |= NdbScanOperation::ScanOptions::SO_GETVALUE;
  options->extraGetValues = gets;
  options->numExtraGetValues = num_gets;
}

static inline void eventSetAnyValue(Thd_ndb *thd_ndb,
                                    NdbOperation::OperationOptions *options) {
  options->anyValue = 0;
  if (thd_ndb->get_applier()) {
    /*
      Applier thread is applying a replicated event.
      Set the server_id to the value received from the log which may be a
      composite of server_id and other data according to the server_id_bits
      option. In future it may be useful to support *not* mapping composite
      AnyValues to/from Binlogged server-ids
    */
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    options->anyValue = thd_unmasked_server_id(thd_ndb->get_thd());

    /*
      Ignore TRANS_NO_LOGGING for applier thread. For other threads it's used to
      indicate log-replica-updates option. This is instead handled in the
      injector thread, by looking explicitly at "opt_log_replica_updates".
    */
  } else {
    if (thd_ndb->check_trans_option(Thd_ndb::TRANS_NO_LOGGING)) {
      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_nologging(options->anyValue);
    }
  }
#ifndef NDEBUG
  if (DBUG_EVALUATE_IF("ndb_set_reflect_anyvalue", true, false)) {
    fprintf(stderr, "Ndb forcing reflect AnyValue\n");
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    ndbcluster_anyvalue_set_reflect_op(options->anyValue);
  }
  if (DBUG_EVALUATE_IF("ndb_set_refresh_anyvalue", true, false)) {
    fprintf(stderr, "Ndb forcing refresh AnyValue\n");
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    ndbcluster_anyvalue_set_refresh_op(options->anyValue);
  }

  /*
    MySQLD will set the user-portion of AnyValue (if any) to all 1s
    This tests code filtering ServerIds on the value of server-id-bits.
  */
  const char *p = getenv("NDB_TEST_ANYVALUE_USERDATA");
  if (p != nullptr && *p != 0 && *p != '0' && *p != 'n' && *p != 'N') {
    options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    dbug_ndbcluster_anyvalue_set_userbits(options->anyValue);
  }
#endif
}

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
int ha_ndbcluster::prepare_conflict_detection(
    enum_conflicting_op_type op_type, const NdbRecord *key_rec,
    const NdbRecord *data_rec, const uchar *old_data, const uchar *new_data,
    const MY_BITMAP *write_set, NdbTransaction *trans, NdbInterpretedCode *code,
    NdbOperation::OperationOptions *options, bool &conflict_handled,
    bool &avoid_ndbapi_write) {
  DBUG_TRACE;

  conflict_handled = false;

  if (unlikely(m_share->is_apply_status_table())) {
    // The ndb_apply_status table should not have any conflict detection
    return 0;
  }

  Ndb_applier *const applier = m_thd_ndb->get_applier();
  assert(applier);

  /*
     Check transaction id first, as in transactional conflict detection,
     the transaction id is what eventually dictates whether an operation
     is applied or not.

     Not that this applies even if the current operation's table does not
     have a conflict function defined - if a transaction spans a 'transactional
     conflict detection' table and a non transactional table, the
     non-transactional table's data will also be reverted.
  */
  Uint64 transaction_id = Ndb_binlog_extra_row_info::InvalidTransactionId;
  bool op_is_marked_as_read = false;
  bool op_is_marked_as_reflected = false;
  // Only used for sanity check and debug printout
  bool op_is_marked_as_refresh [[maybe_unused]] = false;

  THD *thd = table->in_use;
  if (thd->binlog_row_event_extra_data) {
    Ndb_binlog_extra_row_info extra_row_info;
    if (extra_row_info.loadFromBuffer(thd->binlog_row_event_extra_data) != 0) {
      ndb_log_warning(
          "Replica: Malformed event received on table %s "
          "cannot parse. Stopping SQL thread.",
          m_share->key_string());
      return ER_REPLICA_CORRUPT_EVENT;
    }

    if (extra_row_info.getFlags() &
        Ndb_binlog_extra_row_info::NDB_ERIF_TRANSID) {
      transaction_id = extra_row_info.getTransactionId();
    }

    if (extra_row_info.getFlags() &
        Ndb_binlog_extra_row_info::NDB_ERIF_CFT_FLAGS) {
      const Uint16 conflict_flags = extra_row_info.getConflictFlags();
      DBUG_PRINT("info", ("conflict flags : %x\n", conflict_flags));

      if (conflict_flags & NDB_ERIF_CFT_REFLECT_OP) {
        op_is_marked_as_reflected = true;
        applier->increment_reflect_op_prepare_count();
      }

      if (conflict_flags & NDB_ERIF_CFT_REFRESH_OP) {
        op_is_marked_as_refresh = true;
        applier->increment_refresh_op_count();
      }

      if (conflict_flags & NDB_ERIF_CFT_READ_OP) {
        op_is_marked_as_read = true;
      }

      /* Sanity - 1 flag at a time at most */
      assert(!(op_is_marked_as_reflected && op_is_marked_as_refresh));
      assert(!(op_is_marked_as_read &&
               (op_is_marked_as_reflected || op_is_marked_as_refresh)));
    }
  }

  const st_conflict_fn_def *conflict_fn =
      (m_share->m_cfn_share ? m_share->m_cfn_share->m_conflict_fn : nullptr);

  bool pass_mode = false;
  if (conflict_fn) {
    /* Check Slave Conflict Role Variable setting */
    if (conflict_fn->flags & CF_USE_ROLE_VAR) {
      switch (opt_ndb_slave_conflict_role) {
        case SCR_NONE: {
          ndb_log_warning(
              "Replica: Conflict function %s defined on "
              "table %s requires ndb_applier_conflict_role variable "
              "to be set. Stopping SQL thread.",
              conflict_fn->name, m_share->key_string());
          return ER_REPLICA_CONFIGURATION;
        }
        case SCR_PASS: {
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
    const uchar *row_data = (op_type == WRITE_ROW ? new_data : old_data);
    int res = applier->atPrepareConflictDetection(
        m_table, key_rec, row_data, transaction_id, handle_conflict_now);
    if (res) {
      return res;
    }

    if (handle_conflict_now) {
      DBUG_PRINT("info", ("Conflict handling for row occurring now"));
      NdbError noRealConflictError;
      /*
       * If the user operation was a read and we receive an update
       * log event due to an AnyValue update, then the conflicting operation
       * should be reported as a read.
       */
      enum_conflicting_op_type conflicting_op =
          (op_type == UPDATE_ROW && op_is_marked_as_read) ? READ_ROW : op_type;
      /*
         Directly handle the conflict here - e.g refresh/ write to
         exceptions table etc.
      */
      res = handle_row_conflict(applier, m_share->m_cfn_share,
                                m_share->table_name, "Transaction", key_rec,
                                data_rec, old_data, new_data, conflicting_op,
                                TRANS_IN_CONFLICT, noRealConflictError, trans,
                                write_set, transaction_id);
      if (unlikely(res)) {
        return res;
      }

      applier->set_flag(Ndb_applier::OPS_DEFINED);

      /*
        Indicate that there (may be) some more operations to
        execute before committing
      */
      m_thd_ndb->m_unsent_bytes += 12;
      conflict_handled = true;
      return 0;
    }
  }

  if (conflict_fn == nullptr || pass_mode) {
    /* No conflict function definition required */
    return 0;
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
               (transaction_id ==
                Ndb_binlog_extra_row_info::InvalidTransactionId))) {
    ndb_log_warning(
        "Replica: Transactional conflict detection defined on "
        "table %s, but events received without transaction ids.  "
        "Check --ndb-log-transaction-id setting on "
        "upstream Cluster.",
        m_share->key_string());
    /* This is a user error, but we want them to notice, so treat seriously */
    return ER_REPLICA_CORRUPT_EVENT;
  }

  bool prepare_interpreted_program = false;
  if (op_type != WRITE_ROW) {
    prepare_interpreted_program = true;
  } else if (conflict_fn->flags & CF_USE_INTERP_WRITE) {
    prepare_interpreted_program = true;
    avoid_ndbapi_write = false;
  }

  if (conflict_fn->flags & CF_REFLECT_SEC_OPS) {
    /* This conflict function reflects secondary ops at the Primary */

    if (opt_ndb_slave_conflict_role == SCR_PRIMARY) {
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

      options->optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
      ndbcluster_anyvalue_set_reflect_op(options->anyValue);
    } else if (opt_ndb_slave_conflict_role == SCR_SECONDARY) {
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
      if (op_is_marked_as_reflected) {
        /**
         * Apply operations using their 'natural' operation types
         * with interpreted programs attached where appropriate.
         * Natural operation types used so that we become aware
         * of any 'presence' issues (row does/not exist).
         */
        DBUG_PRINT("info", ("Reflected operation"));
      } else {
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
     Prepare interpreted code for operation according to algorithm used
  */
  if (prepare_interpreted_program) {
    const int res = conflict_fn->prep_func(m_share->m_cfn_share, op_type,
                                           m_ndb_record, old_data, new_data,
                                           table->read_set,   // Before image
                                           table->write_set,  // After image
                                           code, applier->get_max_rep_epoch());

    if (res == 0) {
      if (code->getWordsUsed() > 0) {
        /* Attach conflict detecting filter program to operation */
        options->optionsPresent |=
            NdbOperation::OperationOptions::OO_INTERPRETED;
        options->interpretedCode = code;
      }
    } else {
      ndb_log_warning(
          "Replica: Binlog event on table %s missing "
          "info necessary for conflict detection.  "
          "Check binlog format options on upstream cluster.",
          m_share->key_string());
      return ER_REPLICA_CORRUPT_EVENT;
    }
  }

  applier->set_flag(Ndb_applier::OPS_DEFINED);

  /* Now save data for potential insert to exceptions table... */
  Ndb_exceptions_data ex_data;
  ex_data.share = m_share;
  ex_data.key_rec = key_rec;
  ex_data.data_rec = data_rec;
  ex_data.op_type = op_type;
  ex_data.reflected_operation = op_is_marked_as_reflected;
  ex_data.trans_id = transaction_id;

  // Save the row data for possible conflict resolution after execute()
  if (old_data) {
    ex_data.old_row =
        m_thd_ndb->copy_to_batch_mem(old_data, table_share->stored_rec_length);
    if (ex_data.old_row == nullptr) {
      return HA_ERR_OUT_OF_MEM;
    }
  }
  if (new_data) {
    ex_data.new_row =
        m_thd_ndb->copy_to_batch_mem(new_data, table_share->stored_rec_length);
    if (ex_data.new_row == nullptr) {
      return HA_ERR_OUT_OF_MEM;
    }
  }

  ex_data.bitmap_buf = nullptr;
  ex_data.write_set = nullptr;
  if (table->write_set) {
    /* Copy table write set */
    // NOTE! Could copy only data here and create bitmap if there is a conflict
    ex_data.bitmap_buf =
        (my_bitmap_map *)m_thd_ndb->get_buffer(table->s->column_bitmap_size);
    if (ex_data.bitmap_buf == nullptr) {
      return HA_ERR_OUT_OF_MEM;
    }
    ex_data.write_set = (MY_BITMAP *)m_thd_ndb->get_buffer(sizeof(MY_BITMAP));
    if (ex_data.write_set == nullptr) {
      return HA_ERR_OUT_OF_MEM;
    }
    bitmap_init(ex_data.write_set, ex_data.bitmap_buf,
                table->write_set->n_bits);
    bitmap_copy(ex_data.write_set, table->write_set);
  }

  // Save the control structure for possible conflict detection after execute()
  void *ex_data_buffer =
      m_thd_ndb->copy_to_batch_mem(&ex_data, sizeof(ex_data));
  if (ex_data_buffer == nullptr) {
    return HA_ERR_OUT_OF_MEM;
  }

  /* Store pointer to the copied exceptions data in operations 'customdata' */
  options->optionsPresent |= NdbOperation::OperationOptions::OO_CUSTOMDATA;
  options->customData = ex_data_buffer;

  return 0;
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

static int handle_conflict_op_error(Ndb_applier *const applier,
                                    NdbTransaction *trans, const NdbError &err,
                                    const NdbOperation *op) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("ndb error: %d", err.code));

  if (err.code == ERROR_CONFLICT_FN_VIOLATION ||
      err.code == ERROR_OP_AFTER_REFRESH_OP ||
      err.classification == NdbError::ConstraintViolation ||
      err.classification == NdbError::NoDataFound) {
    DBUG_PRINT("info", ("err.code = %s, err.classification = %s",
                        ((err.code == ERROR_CONFLICT_FN_VIOLATION)
                             ? "error_conflict_fn_violation"
                             : ((err.code == ERROR_OP_AFTER_REFRESH_OP)
                                    ? "error_op_after_refresh_op"
                                    : "?")),
                        ((err.classification == NdbError::ConstraintViolation)
                             ? "ConstraintViolation"
                             : ((err.classification == NdbError::NoDataFound)
                                    ? "NoDataFound"
                                    : "?"))));

    enum_conflict_cause conflict_cause;

    /* Map cause onto our conflict description type */
    if (err.code == ERROR_CONFLICT_FN_VIOLATION ||
        err.code == ERROR_OP_AFTER_REFRESH_OP) {
      DBUG_PRINT("info", ("ROW_IN_CONFLICT"));
      conflict_cause = ROW_IN_CONFLICT;
    } else if (err.classification == NdbError::ConstraintViolation) {
      DBUG_PRINT("info", ("ROW_ALREADY_EXISTS"));
      conflict_cause = ROW_ALREADY_EXISTS;
    } else {
      assert(err.classification == NdbError::NoDataFound);
      DBUG_PRINT("info", ("ROW_DOES_NOT_EXIST"));
      conflict_cause = ROW_DOES_NOT_EXIST;
    }

    /* Get exceptions data from operation */
    const void *buffer = op->getCustomData();
    assert(buffer);
    Ndb_exceptions_data ex_data;
    memcpy(&ex_data, buffer, sizeof(ex_data));
    NDB_SHARE *share = ex_data.share;
    NDB_CONFLICT_FN_SHARE *cfn_share = share ? share->m_cfn_share : nullptr;

    const NdbRecord *key_rec = ex_data.key_rec;
    const NdbRecord *data_rec = ex_data.data_rec;
    const uchar *old_row = ex_data.old_row;
    const uchar *new_row = ex_data.new_row;
#ifndef NDEBUG
    const uchar *row =
        (ex_data.op_type == DELETE_ROW) ? ex_data.old_row : ex_data.new_row;
#endif
    enum_conflicting_op_type causing_op_type = ex_data.op_type;
    const MY_BITMAP *write_set = ex_data.write_set;

    DBUG_PRINT("info", ("Conflict causing op type : %u", causing_op_type));

    if (causing_op_type == REFRESH_ROW) {
      /*
         The failing op was a refresh row, require that it
         failed due to being a duplicate (e.g. a refresh
         occurring on a refreshed row)
       */
      if (err.code == ERROR_OP_AFTER_REFRESH_OP) {
        DBUG_PRINT("info", ("Operation after refresh - ignoring"));
        return 0;
      } else {
        DBUG_PRINT("info", ("Refresh op hit real error %u", err.code));
        /* Unexpected error, normal handling*/
        return err.code;
      }
    }

    if (ex_data.reflected_operation) {
      DBUG_PRINT("info", ("Reflected operation error : %u.", err.code));

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
      assert(err.code == ERROR_CONFLICT_FN_VIOLATION ||
             err.classification == NdbError::ConstraintViolation ||
             err.classification == NdbError::NoDataFound);

      applier->increment_reflect_op_discard_count();
      return 0;
    }

    {
      /**
       * For asymmetric algorithms that use the ROLE variable to
       * determine their role, we check whether we are on the
       * SECONDARY cluster.
       * This is far as we want to process conflicts on the
       * SECONDARY.
       */
      bool secondary = cfn_share && cfn_share->m_conflict_fn &&
                       (cfn_share->m_conflict_fn->flags & CF_USE_ROLE_VAR) &&
                       (opt_ndb_slave_conflict_role == SCR_SECONDARY);

      if (secondary) {
        DBUG_PRINT("info", ("Conflict detected, on secondary - ignore"));
        return 0;
      }
    }

    assert(share != nullptr && row != nullptr);
    bool table_has_trans_conflict_detection =
        cfn_share && cfn_share->m_conflict_fn &&
        (cfn_share->m_conflict_fn->flags & CF_TRANSACTIONAL);

    if (table_has_trans_conflict_detection) {
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

      if (!is_del_del_cft || fn_treats_del_del_as_cft) {
        /* Perform special transactional conflict-detected handling */
        const int res = applier->atTransConflictDetected(ex_data.trans_id);
        if (res) {
          return res;
        }
      }
    }

    if (cfn_share) {
      /* Now handle the conflict on this row */
      enum_conflict_fn_type cft = cfn_share->m_conflict_fn->type;
      applier->increment_violation_count(cft);

      int res = handle_row_conflict(
          applier, cfn_share, share->table_name, "Row", key_rec, data_rec,
          old_row, new_row, causing_op_type, conflict_cause, err, trans,
          write_set,
          /*
            ORIG_TRANSID not available for
            non-transactional conflict detection.
          */
          Ndb_binlog_extra_row_info::InvalidTransactionId);

      return res;
    } else {
      DBUG_PRINT("info", ("missing cfn_share"));
      return 0;  // TODO : Correct?
    }
  } else {
    /* Non conflict related error */
    DBUG_PRINT("info", ("err.code == %u", err.code));
    return err.code;
  }

  return 0;  // Reachable?
}

int ha_ndbcluster::write_row(uchar *record) {
  DBUG_TRACE;

  Ndb_applier *const applier = m_thd_ndb->get_applier();
  if (applier && m_share->is_apply_status_table()) {
    // Applier is writing to ndb_apply_status table

    // Extract server_id and epoch from the written row
    assert(record == table->record[0]);
    const Uint32 row_server_id = table->field[0]->val_int();
    const Uint64 row_epoch = table->field[1]->val_int();

    bool skip_write = false;
    const int result =
        applier->atApplyStatusWrite(row_server_id, row_epoch, skip_write);
    if (result != 0) {
      // Stop applier
      return result;
    }

    if (skip_write) {
      // The applier has handled this write by deferring it until commit time
      return 0;
    }
  }

  return ndb_write_row(record, false, false);
}

/**
  Insert one record into NDB
*/
int ha_ndbcluster::ndb_write_row(uchar *record, bool primary_key_update,
                                 bool batched_update) {
  bool has_auto_increment;
  const NdbOperation *op;
  THD *thd = table->in_use;
  Thd_ndb *thd_ndb = m_thd_ndb;
  NdbTransaction *trans;
  uint32 part_id = 0;
  int error = 0;
  Uint64 auto_value;
  longlong func_value = 0;
  const Uint32 authorValue = 1;
  NdbOperation::SetValueSpec sets[3];
  Uint32 num_sets = 0;
  DBUG_TRACE;

  has_auto_increment = (table->next_number_field && record == table->record[0]);

  if (has_auto_increment && table_share->primary_key != MAX_KEY) {
    /*
     * Increase any auto_incremented primary key
     */
    m_skip_auto_increment = false;
    if ((error = update_auto_increment())) return error;
    m_skip_auto_increment = (insert_id_for_cur_row == 0 ||
                             thd->auto_inc_intervals_forced.nb_elements());
  }

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys
   */
  if (!m_use_write && m_ignore_dup_key) {
    /*
      compare if expression with that in start_bulk_insert()
      start_bulk_insert will set parameters to ensure that each
      write_row is committed individually
    */
    const int peek_res = peek_indexed_rows(record, NDB_INSERT);

    if (!peek_res) {
      error = HA_ERR_FOUND_DUPP_KEY;
    } else if (peek_res != HA_ERR_KEY_NOT_FOUND) {
      error = peek_res;
    }
    if (error) {
      if ((has_auto_increment) && (m_skip_auto_increment)) {
        int ret_val;
        if ((ret_val =
                 set_auto_inc(m_thd_ndb->ndb, table->next_number_field))) {
          return ret_val;
        }
      }
      m_skip_auto_increment = true;
      return error;
    }
  }

  bool uses_blobs = uses_blob_value(table->write_set);

  const NdbRecord *key_rec;
  const uchar *key_row;
  if (table_share->primary_key == MAX_KEY) {
    /* Table has hidden primary key. */
    Ndb *ndb = m_thd_ndb->ndb;
    uint retries = NDB_AUTO_INCREMENT_RETRIES;
    for (;;) {
      NDB_SHARE::Tuple_id_range_guard g(m_share);
      if (ndb->getAutoIncrementValue(m_table, g.range, auto_value, 1000) ==
          -1) {
        if (--retries && !thd_killed(thd) &&
            ndb->getNdbError().status == NdbError::TemporaryError) {
          ndb_trans_retry_sleep();
          continue;
        }
        ERR_RETURN(ndb->getNdbError());
      }
      break;
    }
    sets[num_sets].column = get_hidden_key_column();
    sets[num_sets].value = &auto_value;
    num_sets++;
    key_rec = m_ndb_hidden_key_record;
    key_row = (const uchar *)&auto_value;
  } else {
    key_rec = m_index[table_share->primary_key].ndb_unique_record_row;
    key_row = record;
  }

  trans = thd_ndb->trans;
  if (m_user_defined_partitioning) {
    assert(m_use_partition_pruning);
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
    error = m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (unlikely(error)) {
      m_part_info->err_value = func_value;
      return error;
    }
    {
      /*
        We need to set the value of the partition function value in
        NDB since the NDB kernel doesn't have easy access to the function
        to calculate the value.
      */
      if (func_value >= INT_MAX32) func_value = INT_MAX32;
      sets[num_sets].column = get_partition_id_column();
      sets[num_sets].value = &func_value;
      num_sets++;
    }
    if (!trans)
      if (unlikely(!(trans = start_transaction_part_id(part_id, error))))
        return error;
  } else if (!trans) {
    if (unlikely(!(trans = start_transaction_row(key_rec, key_row, error))))
      return error;
  }
  assert(trans);

  ha_statistic_increment(&System_status_var::ha_write_count);

  /*
     Setup OperationOptions
   */
  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = nullptr;
  options.optionsPresent = 0;

  eventSetAnyValue(m_thd_ndb, &options);
  const bool need_flush =
      thd_ndb->add_row_check_if_batch_full(m_bytes_per_write);

  if (thd_ndb->get_applier() && m_table->getExtraRowAuthorBits()) {
    /* Set author to indicate slave updated last */
    sets[num_sets].column = NdbDictionary::Column::ROW_AUTHOR;
    sets[num_sets].value = &authorValue;
    num_sets++;
  }

  if (m_user_defined_partitioning) {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
    options.partitionId = part_id;
  }
  if (num_sets) {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_SETVALUE;
    options.extraSetValues = sets;
    options.numExtraSetValues = num_sets;
  }
  if (thd_ndb->get_applier() || THDVAR(thd, deferred_constraints)) {
    options.optionsPresent |=
        NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |= NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (options.optionsPresent != 0) poptions = &options;

  const Uint32 bitmapSz = (NDB_MAX_ATTRIBUTES_IN_TABLE + 31) / 32;
  uint32 tmpBitmapSpace[bitmapSz];
  MY_BITMAP tmpBitmap;
  MY_BITMAP *user_cols_written_bitmap;
  bool avoidNdbApiWriteOp = false; /* ndb_write_row defaults to write */
  Uint32 buffer[MAX_CONFLICT_INTERPRETED_PROG_SIZE];
  NdbInterpretedCode code(m_table, buffer, sizeof(buffer) / sizeof(buffer[0]));

  /* Conflict resolution in applier */
  const Ndb_applier *const applier = m_thd_ndb->get_applier();
  if (applier) {
    bool conflict_handled = false;
    if (unlikely((error = prepare_conflict_detection(
                      WRITE_ROW, key_rec, m_ndb_record, nullptr, /* old_data */
                      record,                                    /* new_data */
                      table->write_set, trans, &code,            /* code */
                      &options, conflict_handled, avoidNdbApiWriteOp))))
      return error;

    if (unlikely(conflict_handled)) {
      /* No need to continue with operation definition */
      /* TODO : Ensure batch execution */
      return 0;
    }
  };

  if (m_use_write && !avoidNdbApiWriteOp) {
    uchar *mask;

    if (applying_binlog(thd)) {
      /*
        Use write_set when applying binlog to avoid trampling
        unchanged columns
      */
      user_cols_written_bitmap = table->write_set;
      mask = m_table_map->get_column_mask(user_cols_written_bitmap);
    } else {
      /* Ignore write_set for REPLACE command */
      user_cols_written_bitmap = nullptr;
      mask = nullptr;
    }

    op = trans->writeTuple(key_rec, (const char *)key_row, m_ndb_record,
                           (char *)record, mask, poptions,
                           sizeof(NdbOperation::OperationOptions));
  } else {
    uchar *mask;

    /* Check whether Ndb table definition includes any default values. */
    if (m_table->hasDefaultValues()) {
      DBUG_PRINT("info", ("Not sending values for native defaulted columns"));

      /*
        If Ndb is unaware of the table's defaults, we must provide all column
        values to the insert. This is done using a NULL column mask. If Ndb is
        aware of the table's defaults, we only need to provide the columns
        explicitly mentioned in the write set, plus any extra columns required
        due to bug#41616. plus the primary key columns required due to
        bug#42238.
      */
      /*
        The following code for setting user_cols_written_bitmap
        should be removed after BUG#41616 and Bug#42238 are fixed
      */
      /* Copy table write set so that we can add to it */
      user_cols_written_bitmap = &tmpBitmap;
      bitmap_init(user_cols_written_bitmap, tmpBitmapSpace,
                  table->write_set->n_bits);
      bitmap_copy(user_cols_written_bitmap, table->write_set);

      for (uint i = 0; i < table->s->fields; i++) {
        Field *field = table->field[i];
        DBUG_PRINT("info", ("Field#%u, (%u), Type : %u "
                            "NO_DEFAULT_VALUE_FLAG : %u PRI_KEY_FLAG : %u",
                            i, field->field_index(), field->real_type(),
                            field->is_flag_set(NO_DEFAULT_VALUE_FLAG),
                            field->is_flag_set(PRI_KEY_FLAG)));
        if (field->is_flag_set(NO_DEFAULT_VALUE_FLAG) ||  // bug 41616
            field->is_flag_set(PRI_KEY_FLAG) ||           // bug 42238
            !type_supports_default_value(field->real_type())) {
          bitmap_set_bit(user_cols_written_bitmap, field->field_index());
        }
      }
      /* Finally, translate the whole bitmap from MySQL field numbers
         to NDB column numbers */
      mask = m_table_map->get_column_mask(user_cols_written_bitmap);
    } else {
      /* No defaults in kernel, provide all columns ourselves */
      DBUG_PRINT("info", ("No native defaults, sending all values"));
      user_cols_written_bitmap = nullptr;
      mask = nullptr;
    }

    /* Using insert, we write all non default columns */
    op = trans->insertTuple(key_rec, (const char *)key_row, m_ndb_record,
                            (char *)record,
                            mask,  // Default value should be masked
                            poptions, sizeof(NdbOperation::OperationOptions));
  }
  if (!(op)) ERR_RETURN(trans->getNdbError());

  /**
   * Batching
   *
   * iff :
   *   Batching allowed (bulk insert, update, thd_allow())
   *   Don't need to flush batch
   *   Not doing pk updates
   */
  const bool bulk_insert = (m_rows_to_insert > 1);
  const bool will_batch =
      !need_flush && (bulk_insert || batched_update || thd_allow_batch(thd)) &&
      !primary_key_update;

  uint blob_count = 0;
  if (table_share->blob_fields > 0) {
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
    /* Set Blob values for all columns updated by the operation */
    int res =
        set_blob_values(op, record - table->record[0], user_cols_written_bitmap,
                        &blob_count, will_batch);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (res != 0) return res;
  }

  /*
    Execute operation
  */
  m_trans_table_stats->update_uncommitted_rows(1);
  if (will_batch) {
    if (uses_blobs) {
      m_thd_ndb->m_unsent_bytes += 12;
      m_thd_ndb->m_unsent_blob_ops = true;
    }
  } else {
    const int res = flush_bulk_insert();
    if (res != 0) {
      m_skip_auto_increment = true;
      return res;
    }
  }
  if ((has_auto_increment) && (m_skip_auto_increment)) {
    int ret_val;
    if ((ret_val = set_auto_inc(m_thd_ndb->ndb, table->next_number_field))) {
      return ret_val;
    }
  }
  m_skip_auto_increment = true;

  DBUG_PRINT("exit", ("ok"));
  return 0;
}

/* Compare if an update changes the primary key in a row. */
int ha_ndbcluster::primary_key_cmp(const uchar *old_row, const uchar *new_row) {
  uint keynr = table_share->primary_key;
  KEY_PART_INFO *key_part = table->key_info[keynr].key_part;
  KEY_PART_INFO *end = key_part + table->key_info[keynr].user_defined_key_parts;

  for (; key_part != end; key_part++) {
    if (!bitmap_is_set(table->write_set, key_part->fieldnr - 1)) continue;

    /* The primary key does not allow NULLs. */
    assert(!key_part->null_bit);

    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART)) {
      if (key_part->field->cmp_binary((old_row + key_part->offset),
                                      (new_row + key_part->offset),
                                      (ulong)key_part->length))
        return 1;
    } else {
      if (memcmp(old_row + key_part->offset, new_row + key_part->offset,
                 key_part->length))
        return 1;
    }
  }
  return 0;
}

static Ndb_exceptions_data StaticRefreshExceptionsData = {
    nullptr, nullptr, nullptr,     nullptr, nullptr,
    nullptr, nullptr, REFRESH_ROW, false,   0};

static int handle_row_conflict(
    Ndb_applier *const applier, NDB_CONFLICT_FN_SHARE *cfn_share,
    const char *table_name, const char *handling_type, const NdbRecord *key_rec,
    const NdbRecord *data_rec, const uchar *old_row, const uchar *new_row,
    enum_conflicting_op_type op_type, enum_conflict_cause conflict_cause,
    const NdbError &conflict_error, NdbTransaction *conflict_trans,
    const MY_BITMAP *write_set, Uint64 transaction_id) {
  DBUG_TRACE;

  const uchar *row = (op_type == DELETE_ROW) ? old_row : new_row;
  /*
     We will refresh the row if the conflict function requires
     it, or if we are handling a transactional conflict.
  */
  bool refresh_row = (conflict_cause == TRANS_IN_CONFLICT) ||
                     (cfn_share && (cfn_share->m_flags & CFF_REFRESH_ROWS));

  if (refresh_row) {
    /* A conflict has been detected between an applied replicated operation
     * and the data in the DB.
     * The attempt to change the local DB will have been rejected.
     * We now take steps to generate a refresh Binlog event so that
     * other clusters will be re-aligned.
     */
    DBUG_PRINT("info",
               ("Conflict on table %s.  Operation type : %s, "
                "conflict cause :%s, conflict error : %u : %s",
                table_name,
                ((op_type == WRITE_ROW)    ? "WRITE_ROW"
                 : (op_type == UPDATE_ROW) ? "UPDATE_ROW"
                                           : "DELETE_ROW"),
                ((conflict_cause == ROW_ALREADY_EXISTS)   ? "ROW_ALREADY_EXISTS"
                 : (conflict_cause == ROW_DOES_NOT_EXIST) ? "ROW_DOES_NOT_EXIST"
                                                          : "ROW_IN_CONFLICT"),
                conflict_error.code, conflict_error.message));

    assert(key_rec != nullptr);
    assert(row != nullptr);

    do {
      /* When the slave splits an epoch into batches, a conflict row detected
       * and refreshed in an early batch can be written to by operations in
       * a later batch.  As the operations will not have applied, and the
       * row has already been refreshed, we need not attempt to refresh
       * it again
       */
      if (conflict_cause == ROW_IN_CONFLICT &&
          conflict_error.code == ERROR_OP_AFTER_REFRESH_OP) {
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
      if ((op_type == DELETE_ROW) && (conflict_cause == ROW_DOES_NOT_EXIST)) {
        applier->increment_delete_delete_count();
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
      options.optionsPresent = NdbOperation::OperationOptions::OO_CUSTOMDATA |
                               NdbOperation::OperationOptions::OO_ANYVALUE;
      options.customData = &StaticRefreshExceptionsData;
      options.anyValue = 0;

      /* Use AnyValue to indicate that this is a refreshTuple op */
      ndbcluster_anyvalue_set_refresh_op(options.anyValue);

      /* Create a refresh to operation to realign other clusters */
      // TODO Do we ever get non-PK key?
      //      Keyless table?
      //      Unique index
      const NdbOperation *refresh_op = conflict_trans->refreshTuple(
          key_rec, (const char *)row, &options, sizeof(options));
      if (!refresh_op) {
        NdbError err = conflict_trans->getNdbError();

        if (err.status == NdbError::TemporaryError) {
          /* Slave will roll back and retry entire transaction. */
          ERR_RETURN(err);
        } else {
          char msg[FN_REFLEN];

          /* We cannot refresh a row which has Blobs, as we do not support
           * Blob refresh yet.
           * Rows implicated by a transactional conflict function may have
           * Blobs.
           * We will generate an error in this case
           */
          const int NDBAPI_ERR_REFRESH_ON_BLOB_TABLE = 4343;
          if (err.code == NDBAPI_ERR_REFRESH_ON_BLOB_TABLE) {
            // Generate legacy error message instead of using
            // the error code and message returned from NdbApi
            snprintf(msg, sizeof(msg),
                     "%s conflict handling on table %s failed as table "
                     "has Blobs which cannot be refreshed.",
                     handling_type, table_name);

            push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                                ER_EXCEPTIONS_WRITE_ERROR,
                                ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR),
                                msg);

            return ER_EXCEPTIONS_WRITE_ERROR;
          }

          snprintf(msg, sizeof(msg),
                   "Row conflict handling "
                   "on table %s hit Ndb error %d '%s'",
                   table_name, err.code, err.message);
          push_warning_printf(
              current_thd, Sql_condition::SL_WARNING, ER_EXCEPTIONS_WRITE_ERROR,
              ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
          /* Slave will stop replication. */
          return ER_EXCEPTIONS_WRITE_ERROR;
        }
      }
    } while (0);  // End of 'refresh' block
  }

  DBUG_PRINT(
      "info",
      ("Table %s does%s have an exceptions table", table_name,
       (cfn_share && cfn_share->m_ex_tab_writer.hasTable()) ? "" : " not"));
  if (cfn_share && cfn_share->m_ex_tab_writer.hasTable()) {
    NdbError err;
    const auto current_state = applier->get_current_epoch_state();
    if (cfn_share->m_ex_tab_writer.writeRow(
            conflict_trans, key_rec, data_rec, current_state.own_server_id,
            current_state.source_server_id, current_state.epoch_value, old_row,
            new_row, op_type, conflict_cause, transaction_id, write_set,
            err) != 0) {
      if (err.code != 0) {
        if (err.status == NdbError::TemporaryError) {
          /* Slave will roll back and retry entire transaction. */
          ERR_RETURN(err);
        } else {
          char msg[FN_REFLEN];
          snprintf(msg, sizeof(msg),
                   "%s conflict handling "
                   "on table %s hit Ndb error %d '%s'",
                   handling_type, table_name, err.code, err.message);
          push_warning_printf(
              current_thd, Sql_condition::SL_WARNING, ER_EXCEPTIONS_WRITE_ERROR,
              ER_THD(current_thd, ER_EXCEPTIONS_WRITE_ERROR), msg);
          /* Slave will stop replication. */
          return ER_EXCEPTIONS_WRITE_ERROR;
        }
      }
    }
  } /* if (cfn_share->m_ex_tab != NULL) */

  return 0;
}

/**
  Update one record in NDB using primary key.
*/

bool ha_ndbcluster::start_bulk_update() {
  DBUG_TRACE;
  if (!m_use_write && m_ignore_dup_key) {
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    return true;
  }
  return false;
}

int ha_ndbcluster::bulk_update_row(const uchar *old_data, uchar *new_data,
                                   uint *dup_key_found) {
  DBUG_TRACE;
  *dup_key_found = 0;
  return ndb_update_row(old_data, new_data, 1);
}

int ha_ndbcluster::exec_bulk_update(uint *dup_key_found) {
  NdbTransaction *trans = m_thd_ndb->trans;
  DBUG_TRACE;
  *dup_key_found = 0;

  /* If a fatal error is encountered during an update op, the error
   * is saved and exec continues. So exec_bulk_update may be called
   * even when init functions fail. Check for error conditions like
   * an uninit'ed transaction.
   */
  if (unlikely(!m_thd_ndb->trans)) {
    DBUG_PRINT("exit", ("Transaction was not started"));
    int error = 0;
    ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
    return error;
  }

  // m_handler must be NULL or point to _this_ handler instance
  assert(m_thd_ndb->m_handler == nullptr || m_thd_ndb->m_handler == this);

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
  if (m_thd_ndb->m_handler && m_read_before_write_removal_possible) {
    /*
      This is an autocommit involving only one table and rbwr is on

      Commit the autocommit transaction early(before the usual place
      in ndbcluster_commit) in order to:
      1) save one round trip, "no-commit+commit" converted to "commit"
      2) return the correct number of updated and affected rows
         to the update loop(which will ask handler in rbwr mode)
    */
    DBUG_PRINT("info", ("committing auto-commit+rbwr early"));
    uint ignore_count = 0;
    const int ignore_error = 1;
    if (execute_commit(m_thd_ndb, trans, m_thd_ndb->m_force_send, ignore_error,
                       &ignore_count) != 0) {
      m_thd_ndb->trans_tables.reset_stats();
      return ndb_err(trans);
    }
    THD *thd = table->in_use;
    if (!applying_binlog(thd)) {
      DBUG_PRINT("info", ("ignore_count: %u", ignore_count));
      assert(m_rows_updated >= ignore_count);
      m_rows_updated -= ignore_count;
    }
    return 0;
  }

  if (m_thd_ndb->m_unsent_bytes == 0) {
    DBUG_PRINT("exit", ("skip execute - no unsent bytes"));
    return 0;
  }

  if (thd_allow_batch(table->in_use)) {
    /*
      Turned on by @@transaction_allow_batching=ON
      or implicitly by slave exec thread
    */
    DBUG_PRINT("exit", ("skip execute - transaction_allow_batching is ON"));
    return 0;
  }

  if (m_thd_ndb->m_handler && !m_thd_ndb->m_unsent_blob_ops) {
    // Execute at commit time(in 'ndbcluster_commit') to save a round trip
    DBUG_PRINT("exit", ("skip execute - simple autocommit"));
    return 0;
  }

  uint ignore_count = 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0) {
    m_thd_ndb->trans_tables.reset_stats();
    return ndb_err(trans);
  }
  THD *thd = table->in_use;
  if (!applying_binlog(thd)) {
    assert(m_rows_updated >= ignore_count);
    m_rows_updated -= ignore_count;
  }
  return 0;
}

void ha_ndbcluster::end_bulk_update() { DBUG_TRACE; }

int ha_ndbcluster::update_row(const uchar *old_data, uchar *new_data) {
  return ndb_update_row(old_data, new_data, 0);
}

void ha_ndbcluster::setup_key_ref_for_ndb_record(const NdbRecord **key_rec,
                                                 const uchar **key_row,
                                                 const uchar *record,
                                                 bool use_active_index) {
  DBUG_TRACE;
  if (use_active_index) {
    /* Use unique key to access table */
    DBUG_PRINT("info", ("Using unique index (%u)", active_index));
    assert((table->key_info[active_index].flags & HA_NOSAME));
    /* Can't use key if we didn't read it first */
    assert(bitmap_is_subset(m_key_fields[active_index], table->read_set));
    *key_rec = m_index[active_index].ndb_unique_record_row;
    *key_row = record;
  } else if (table_share->primary_key != MAX_KEY) {
    /* Use primary key to access table */
    DBUG_PRINT("info", ("Using primary key"));
    /* Can't use pk if we didn't read it first */
    assert(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
    *key_rec = m_index[table_share->primary_key].ndb_unique_record_row;
    *key_row = record;
  } else {
    /* Use hidden primary key previously read into m_ref. */
    DBUG_PRINT("info", ("Using hidden primary key (%llu)", m_ref));
    /* Can't use hidden pk if we didn't read it first */
    assert(bitmap_is_subset(m_pk_bitmap_p, table->read_set));
    assert(m_read_before_write_removal_used == false);
    *key_rec = m_ndb_hidden_key_record;
    *key_row = (const uchar *)(&m_ref);
  }
}

/*
  Update one record in NDB using primary key
*/

int ha_ndbcluster::ndb_update_row(const uchar *old_data, uchar *new_data,
                                  int is_bulk_update) {
  THD *thd = table->in_use;
  Thd_ndb *thd_ndb = m_thd_ndb;
  NdbScanOperation *cursor = m_active_cursor;
  const NdbOperation *op;
  uint32 old_part_id = ~uint32(0), new_part_id = ~uint32(0);
  int error = 0;
  longlong func_value = 0;
  Uint32 func_value_uint32;
  bool have_pk = (table_share->primary_key != MAX_KEY);
  const bool pk_update =
      (!m_read_before_write_removal_possible && have_pk &&
       bitmap_is_overlapping(table->write_set, m_pk_bitmap_p) &&
       primary_key_cmp(old_data, new_data));
  bool batch_allowed =
      !m_update_cannot_batch && (is_bulk_update || thd_allow_batch(thd));
  NdbOperation::SetValueSpec sets[2];
  Uint32 num_sets = 0;

  DBUG_TRACE;

  /* Start a transaction now if none available
   * (Manual Binlog application...)
   */
  /* TODO : Consider hinting */
  if (unlikely((!m_thd_ndb->trans) && !get_transaction(error))) {
    return error;
  }

  NdbTransaction *trans = m_thd_ndb->trans;
  assert(trans);

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys,
   * but check that it is not part of INSERT ... ON DUPLICATE KEY UPDATE
   */
  if (m_ignore_dup_key && (thd->lex->sql_command == SQLCOM_UPDATE ||
                           thd->lex->sql_command == SQLCOM_UPDATE_MULTI)) {
    const NDB_WRITE_OP write_op = pk_update ? NDB_PK_UPDATE : NDB_UPDATE;
    const int peek_res = peek_indexed_rows(new_data, write_op);

    if (!peek_res) {
      return HA_ERR_FOUND_DUPP_KEY;
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND) return peek_res;
  }

  ha_statistic_increment(&System_status_var::ha_update_count);

  bool skip_partition_for_unique_index = false;
  if (m_use_partition_pruning) {
    if (!cursor && m_read_before_write_removal_used) {
      const NDB_INDEX_TYPE type = get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for update
        without finding the partitions
      */
      if (type == UNIQUE_INDEX || type == UNIQUE_ORDERED_INDEX) {
        skip_partition_for_unique_index = true;
        goto skip_partition_pruning;
      }
    }
    if ((error = get_parts_for_update(old_data, new_data, table->record[0],
                                      m_part_info, &old_part_id, &new_part_id,
                                      &func_value))) {
      m_part_info->err_value = func_value;
      return error;
    }
    DBUG_PRINT("info",
               ("old_part_id: %u  new_part_id: %u", old_part_id, new_part_id));
  skip_partition_pruning:
    (void)0;
  }

  /*
   * Check for update of primary key or partition change
   * for special handling
   */
  if (pk_update || old_part_id != new_part_id) {
    return ndb_pk_update_row(old_data, new_data);
  }
  /*
    If we are updating a unique key with auto_increment
    then we need to update the auto_increment counter
   */
  if (table->found_next_number_field &&
      bitmap_is_set(table->write_set,
                    table->found_next_number_field->field_index()) &&
      (error = set_auto_inc(m_thd_ndb->ndb, table->found_next_number_field))) {
    return error;
  }
  /*
    Set only non-primary-key attributes.
    We already checked that any primary key attribute in write_set has no
    real changes.
  */
  bitmap_copy(&m_bitmap, table->write_set);
  bitmap_subtract(&m_bitmap, m_pk_bitmap_p);
  uchar *mask = m_table_map->get_column_mask(&m_bitmap);
  assert(!pk_update);

  NdbOperation::OperationOptions *poptions = nullptr;
  NdbOperation::OperationOptions options;
  options.optionsPresent = 0;

  /* Need to set the value of any user-defined partitioning function.
     (except for when using unique index)
  */
  if (m_user_defined_partitioning && !skip_partition_for_unique_index) {
    if (func_value >= INT_MAX32)
      func_value_uint32 = INT_MAX32;
    else
      func_value_uint32 = (uint32)func_value;
    sets[num_sets].column = get_partition_id_column();
    sets[num_sets].value = &func_value_uint32;
    num_sets++;

    if (!cursor) {
      options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId = new_part_id;
    }
  }

  eventSetAnyValue(m_thd_ndb, &options);

  const bool need_flush =
      thd_ndb->add_row_check_if_batch_full(m_bytes_per_write);

  const Uint32 authorValue = 1;
  if (thd_ndb->get_applier() && m_table->getExtraRowAuthorBits()) {
    /* Set author to indicate slave updated last */
    sets[num_sets].column = NdbDictionary::Column::ROW_AUTHOR;
    sets[num_sets].value = &authorValue;
    num_sets++;
  }

  if (num_sets) {
    options.optionsPresent |= NdbOperation::OperationOptions::OO_SETVALUE;
    options.extraSetValues = sets;
    options.numExtraSetValues = num_sets;
  }

  if (thd_ndb->get_applier() || THDVAR(thd, deferred_constraints)) {
    options.optionsPresent |=
        NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |= NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (cursor) {
    /*
      We are scanning records and want to update the record
      that was just found, call updateCurrentTuple on the cursor
      to take over the lock to a new update operation
      And thus setting the primary key of the record from
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling updateTuple on cursor, write_set=0x%x",
                        table->write_set->bitmap[0]));

    if (options.optionsPresent != 0) poptions = &options;

    if (!(op = cursor->updateCurrentTuple(
              trans, m_ndb_record, (const char *)new_data, mask, poptions,
              sizeof(NdbOperation::OperationOptions))))
      ERR_RETURN(trans->getNdbError());

    m_lock_tuple = false;
    thd_ndb->m_unsent_bytes += 12;
  } else {
    const NdbRecord *key_rec;
    const uchar *key_row;
    setup_key_ref_for_ndb_record(&key_rec, &key_row, new_data,
                                 m_read_before_write_removal_used);

    bool avoidNdbApiWriteOp = true; /* Default update op for ndb_update_row */
    Uint32 buffer[MAX_CONFLICT_INTERPRETED_PROG_SIZE];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer) / sizeof(buffer[0]));

    /* Conflict resolution in Applier */
    const Ndb_applier *const applier = m_thd_ndb->get_applier();
    if (applier) {
      bool conflict_handled = false;
      /* Conflict resolution in slave thread. */
      DBUG_PRINT("info", ("Slave thread, preparing conflict resolution for "
                          "update with mask : %x",
                          *((Uint32 *)mask)));

      if (unlikely((error = prepare_conflict_detection(
                        UPDATE_ROW, key_rec, m_ndb_record, old_data, new_data,
                        table->write_set, trans, &code, &options,
                        conflict_handled, avoidNdbApiWriteOp))))
        return error;

      if (unlikely(conflict_handled)) {
        /* No need to continue with operation definition */
        /* TODO : Ensure batch execution */
        return 0;
      }
    }

    if (options.optionsPresent != 0) poptions = &options;

    if (likely(avoidNdbApiWriteOp)) {
      if (!(op =
                trans->updateTuple(key_rec, (const char *)key_row, m_ndb_record,
                                   (const char *)new_data, mask, poptions,
                                   sizeof(NdbOperation::OperationOptions))))
        ERR_RETURN(trans->getNdbError());
    } else {
      DBUG_PRINT("info", ("Update op using writeTuple"));
      if (!(op = trans->writeTuple(key_rec, (const char *)key_row, m_ndb_record,
                                   (const char *)new_data, mask, poptions,
                                   sizeof(NdbOperation::OperationOptions))))
        ERR_RETURN(trans->getNdbError());
    }
  }

  uint blob_count = 0;
  if (uses_blob_value(table->write_set)) {
    int row_offset = (int)(new_data - table->record[0]);
    int res = set_blob_values(op, row_offset, table->write_set, &blob_count,
                              (batch_allowed && !need_flush));
    if (res != 0) return res;
  }
  uint ignore_count = 0;
  /*
    Batch update operation if we are doing a scan for update, unless
    there exist UPDATE AFTER triggers
  */
  if (m_update_cannot_batch || !(cursor || (batch_allowed && have_pk)) ||
      need_flush) {
    if (execute_no_commit(m_thd_ndb, trans,
                          m_ignore_no_key || m_read_before_write_removal_used,
                          &ignore_count) != 0) {
      m_thd_ndb->trans_tables.reset_stats();
      return ndb_err(trans);
    }
  } else if (blob_count > 0)
    m_thd_ndb->m_unsent_blob_ops = true;

  m_rows_updated++;

  if (!applying_binlog(thd)) {
    assert(m_rows_updated >= ignore_count);
    m_rows_updated -= ignore_count;
  }

  return 0;
}

/*
  handler delete interface
*/

int ha_ndbcluster::delete_row(const uchar *record) {
  return ndb_delete_row(record, false);
}

bool ha_ndbcluster::start_bulk_delete() {
  DBUG_TRACE;
  m_is_bulk_delete = true;
  return 0;  // Bulk delete used by handler
}

int ha_ndbcluster::end_bulk_delete() {
  NdbTransaction *trans = m_thd_ndb->trans;
  DBUG_TRACE;
  assert(m_is_bulk_delete);  // Don't allow end() without start()
  m_is_bulk_delete = false;

  // m_handler must be NULL or point to _this_ handler instance
  assert(m_thd_ndb->m_handler == nullptr || m_thd_ndb->m_handler == this);

  if (unlikely(trans == nullptr)) {
    /* Problem with late starting transaction, do nothing here */
    return 0;
  }

  if (m_thd_ndb->m_handler && m_read_before_write_removal_possible) {
    /*
      This is an autocommit involving only one table and rbwr is on

      Commit the autocommit transaction early(before the usual place
      in ndbcluster_commit) in order to:
      1) save one round trip, "no-commit+commit" converted to "commit"
      2) return the correct number of updated and affected rows
         to the delete loop(which will ask handler in rbwr mode)
    */
    DBUG_PRINT("info", ("committing auto-commit+rbwr early"));
    uint ignore_count = 0;
    const int ignore_error = 1;
    if (execute_commit(m_thd_ndb, trans, m_thd_ndb->m_force_send, ignore_error,
                       &ignore_count) != 0) {
      m_thd_ndb->trans_tables.reset_stats();
      m_rows_deleted = 0;
      return ndb_err(trans);
    }
    THD *thd = table->in_use;
    if (!applying_binlog(thd)) {
      DBUG_PRINT("info", ("ignore_count: %u", ignore_count));
      assert(m_rows_deleted >= ignore_count);
      m_rows_deleted -= ignore_count;
    }
    return 0;
  }

  if (m_thd_ndb->m_unsent_bytes == 0) {
    DBUG_PRINT("exit", ("skip execute - no unsent bytes"));
    return 0;
  }

  if (thd_allow_batch(table->in_use)) {
    /*
      Turned on by @@transaction_allow_batching=ON
      or implicitly by slave exec thread
    */
    DBUG_PRINT("exit", ("skip execute - transaction_allow_batching is ON"));
    return 0;
  }

  if (m_thd_ndb->m_handler) {
    // Execute at commit time(in 'ndbcluster_commit') to save a round trip
    DBUG_PRINT("exit", ("skip execute - simple autocommit"));
    return 0;
  }

  uint ignore_count = 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0) {
    m_thd_ndb->trans_tables.reset_stats();
    return ndb_err(trans);
  }

  THD *thd = table->in_use;
  if (!applying_binlog(thd)) {
    assert(m_rows_deleted >= ignore_count);
    m_rows_deleted -= ignore_count;
    m_trans_table_stats->update_uncommitted_rows(ignore_count);
  }
  return 0;
}

/**
  Delete one record from NDB, using primary key .
*/

int ha_ndbcluster::ndb_delete_row(const uchar *record,
                                  bool primary_key_update) {
  THD *thd = table->in_use;
  Thd_ndb *thd_ndb = m_thd_ndb;
  NdbScanOperation *cursor = m_active_cursor;
  uint32 part_id = ~uint32(0);
  int error = 0;
  bool allow_batch =
      !m_delete_cannot_batch && (m_is_bulk_delete || thd_allow_batch(thd));

  DBUG_TRACE;

  /* Start a transaction now if none available
   * (Manual Binlog application...)
   */
  /* TODO : Consider hinting */
  if (unlikely((!m_thd_ndb->trans) && !get_transaction(error))) {
    return error;
  }

  NdbTransaction *trans = m_thd_ndb->trans;
  assert(trans);

  ha_statistic_increment(&System_status_var::ha_delete_count);

  bool skip_partition_for_unique_index = false;
  if (m_use_partition_pruning) {
    if (!cursor && m_read_before_write_removal_used) {
      const NDB_INDEX_TYPE type = get_index_type(active_index);
      /*
        Ndb unique indexes are global so when
        m_read_before_write_removal_used is active
        the unique index can be used directly for deleting
        without finding the partitions
      */
      if (type == UNIQUE_INDEX || type == UNIQUE_ORDERED_INDEX) {
        skip_partition_for_unique_index = true;
        goto skip_partition_pruning;
      }
    }
    if ((error = get_part_for_delete(record, table->record[0], m_part_info,
                                     &part_id))) {
      return error;
    }
  skip_partition_pruning:
    (void)0;
  }

  NdbOperation::OperationOptions options;
  NdbOperation::OperationOptions *poptions = nullptr;
  options.optionsPresent = 0;

  eventSetAnyValue(m_thd_ndb, &options);

  // Approximate number of bytes that need to be sent to NDB when deleting a row
  // of this table
  const uint delete_size = 12 + (m_bytes_per_write >> 2);
  const bool need_flush = thd_ndb->add_row_check_if_batch_full(delete_size);

  if (thd_ndb->get_applier() || THDVAR(thd, deferred_constraints)) {
    options.optionsPresent |=
        NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
  }

  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
    DBUG_PRINT("info", ("Disabling foreign keys"));
    options.optionsPresent |= NdbOperation::OperationOptions::OO_DISABLE_FK;
  }

  if (cursor) {
    if (options.optionsPresent != 0) poptions = &options;

    /*
      We are scanning records and want to delete the record
      that was just found, call deleteTuple on the cursor
      to take over the lock to a new delete operation
      And thus setting the primary key of the record from
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling deleteTuple on cursor"));
    if (cursor->deleteCurrentTuple(
            trans, m_ndb_record,
            nullptr,  // result_row
            nullptr,  // result_mask
            poptions, sizeof(NdbOperation::OperationOptions)) == nullptr) {
      ERR_RETURN(trans->getNdbError());
    }
    m_lock_tuple = false;
    thd_ndb->m_unsent_bytes += 12;

    m_trans_table_stats->update_uncommitted_rows(-1);
    m_rows_deleted++;

    if (!(primary_key_update || m_delete_cannot_batch)) {
      thd_ndb->m_unsent_blob_ops |= ndb_table_has_blobs(m_table);
      // If deleting from cursor, NoCommit will be handled in next_result
      return 0;
    }
  } else {
    const NdbRecord *key_rec;
    const uchar *key_row;

    if (m_user_defined_partitioning && !skip_partition_for_unique_index) {
      options.optionsPresent |= NdbOperation::OperationOptions::OO_PARTITION_ID;
      options.partitionId = part_id;
    }

    setup_key_ref_for_ndb_record(&key_rec, &key_row, record,
                                 m_read_before_write_removal_used);

    Uint32 buffer[MAX_CONFLICT_INTERPRETED_PROG_SIZE];
    NdbInterpretedCode code(m_table, buffer,
                            sizeof(buffer) / sizeof(buffer[0]));
    /* Conflict resolution in Applier */
    const Ndb_applier *const applier = m_thd_ndb->get_applier();
    if (applier) {
      bool conflict_handled = false;
      bool dummy_delete_does_not_care = false;

      /* Conflict resolution in slave thread. */
      if (unlikely(
              (error = prepare_conflict_detection(
                   DELETE_ROW, key_rec, m_ndb_record, key_row, /* old_data */
                   nullptr,                                    /* new_data */
                   table->write_set, trans, &code, &options, conflict_handled,
                   dummy_delete_does_not_care))))
        return error;

      if (unlikely(conflict_handled)) {
        /* No need to continue with operation definition */
        /* TODO : Ensure batch execution */
        return 0;
      }
    }

    if (options.optionsPresent != 0) poptions = &options;

    if (trans->deleteTuple(key_rec, (const char *)key_row, m_ndb_record,
                           nullptr,  // row
                           nullptr,  // mask
                           poptions,
                           sizeof(NdbOperation::OperationOptions)) == nullptr) {
      ERR_RETURN(trans->getNdbError());
    }

    m_trans_table_stats->update_uncommitted_rows(-1);
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

    if (allow_batch && table_share->primary_key != MAX_KEY &&
        !primary_key_update && !need_flush) {
      return 0;
    }
  }

  // Execute delete operation
  uint ignore_count = 0;
  if (execute_no_commit(m_thd_ndb, trans,
                        m_ignore_no_key || m_read_before_write_removal_used,
                        &ignore_count) != 0) {
    m_thd_ndb->trans_tables.reset_stats();
    return ndb_err(trans);
  }
  if (!primary_key_update) {
    if (!applying_binlog(thd)) {
      assert(m_rows_deleted >= ignore_count);
      m_rows_deleted -= ignore_count;
      m_trans_table_stats->update_uncommitted_rows(ignore_count);
    }
  }
  return 0;
}

/**
  Unpack a record returned from a scan.
  We copy field-for-field to
   1. Avoid unnecessary copying for sparse rows.
   2. Properly initialize not used null bits.
  Note that we do not unpack all returned rows; some primary/unique key
  operations can read directly into the destination row.
*/
int ha_ndbcluster::unpack_record(uchar *dst_row, const uchar *src_row) {
  DBUG_TRACE;
  assert(src_row != nullptr);

  ptrdiff_t dst_offset = dst_row - table->record[0];
  ptrdiff_t src_offset = src_row - table->record[0];

  // Set the NULL flags for all fields
  memset(dst_row, 0xff, table->s->null_bytes);

  uchar *blob_ptr = m_blobs_buffer.get_ptr(0);

  for (uint i = 0; i < table_share->fields; i++) {
    if (!bitmap_is_set(table->read_set, i)) continue;

    Field *field = table->field[i];
    if (!field->stored_in_db) continue;

    // Handle Field_blob (BLOB, JSON, GEOMETRY)
    if (field->is_flag_set(BLOB_FLAG)) {
      Field_blob *field_blob = (Field_blob *)field;
      NdbBlob *ndb_blob = m_value[i].blob;
      /* unpack_record *only* called for scan result processing
       * *while* the scan is open and the Blob is active.
       * Verify Blob state to be certain.
       * Accessing PK/UK op Blobs after execute() is unsafe
       */
      assert(ndb_blob != nullptr);
      assert(ndb_blob->getState() == NdbBlob::Active);
      int isNull;
      ndbcluster::ndbrequire(ndb_blob->getNull(isNull) == 0);
      Uint64 len64 = 0;
      field_blob->move_field_offset(dst_offset);
      if (!isNull) {
        ndbcluster::ndbrequire(ndb_blob->getLength(len64) == 0);
        ndbcluster::ndbrequire(len64 <= (Uint64)0xffffffff);

        if (len64 > field_blob->max_data_length()) {
          len64 = calc_ndb_blob_len(ndb_blob->getColumn()->getCharset(),
                                    blob_ptr, field_blob->max_data_length());

          // push a warning
          push_warning_printf(
              table->in_use, Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED,
              "Truncated value from TEXT field \'%s\'", field_blob->field_name);
        }
        field->set_notnull();
      }
      /* Need not set_null(), as we initialized null bits to 1 above. */
      field_blob->set_ptr((uint32)len64, blob_ptr);
      field_blob->move_field_offset(-dst_offset);
      blob_ptr += (len64 + 7) & ~((Uint64)7);
      continue;
    }

    // Handle Field_bit
    // Store value in destination even if NULL (i.e. 0)
    if (field->type() == MYSQL_TYPE_BIT) {
      Field_bit *field_bit = down_cast<Field_bit *>(field);
      field->move_field_offset(src_offset);
      longlong value = field_bit->val_int();
      field->move_field_offset(dst_offset - src_offset);
      if (field->is_real_null(src_offset)) {
        // This sets the uneven highbits, located after the null bit
        // in the Field_bit ptr, to 0
        value = 0;
        // Make sure destination null flag is correct
        field->set_null(dst_offset);
      } else {
        field->set_notnull(dst_offset);
      }
      // Field_bit in DBUG requires the bit set in write_set for store().
      my_bitmap_map *old_map =
          dbug_tmp_use_all_columns(table, table->write_set);
      ndbcluster::ndbrequire(field_bit->store(value, true) == 0);
      dbug_tmp_restore_column_map(table->write_set, old_map);
      field->move_field_offset(-dst_offset);
      continue;
    }

    // A normal field (not blob or bit type).
    if (field->is_real_null(src_offset)) {
      // Field is NULL and the null flags are already set
      continue;
    }
    const uint32 actual_length = field_used_length(field, src_offset);
    field->set_notnull(dst_offset);
    memcpy(field->field_ptr() + dst_offset, field->field_ptr() + src_offset,
           actual_length);
  }

  if (unlikely(!m_cond.check_condition())) {
    return HA_ERR_KEY_NOT_FOUND;  // False condition
  }
  assert(pushed_cond == nullptr || const_cast<Item *>(pushed_cond)->val_int());
  return 0;
}

int ha_ndbcluster::unpack_record_and_set_generated_fields(
    uchar *dst_row, const uchar *src_row) {
  const int res = unpack_record(dst_row, src_row);
  if (res == 0 && Ndb_table_map::has_virtual_gcol(table)) {
    update_generated_read_fields(dst_row, table);
  }
  return res;
}

/**
  Get the default value of the field from default_values of the table.
*/
static void get_default_value(void *def_val, Field *field) {
  assert(field != nullptr);
  assert(field->stored_in_db);

  ptrdiff_t src_offset = field->table->default_values_offset();

  {
    if (bitmap_is_set(field->table->read_set, field->field_index())) {
      if (field->type() == MYSQL_TYPE_BIT) {
        Field_bit *field_bit = static_cast<Field_bit *>(field);
        if (!field->is_real_null(src_offset)) {
          field->move_field_offset(src_offset);
          longlong value = field_bit->val_int();
          /* Map to NdbApi format - two Uint32s */
          Uint32 out[2];
          out[0] = 0;
          out[1] = 0;
          for (int b = 0; b < 64; b++) {
            out[b >> 5] |= (value & 1) << (b & 31);

            value = value >> 1;
          }
          memcpy(def_val, out, sizeof(longlong));
          field->move_field_offset(-src_offset);
        }
      } else if (field->is_flag_set(BLOB_FLAG)) {
        assert(false);
      } else {
        field->move_field_offset(src_offset);
        /* Normal field (not blob or bit type). */
        if (!field->is_null()) {
          /* Only copy actually used bytes of varstrings. */
          uint32 actual_length = field_used_length(field);
          uchar *src_ptr = field->field_ptr();
          field->set_notnull();
          memcpy(def_val, src_ptr, actual_length);
        }
        field->move_field_offset(-src_offset);
        /* No action needed for a NULL field. */
      }
    }
  }
}

static inline int fail_index_offline(TABLE *t, int index) {
  KEY *key_info = t->key_info + index;
  push_warning_printf(
      t->in_use, Sql_condition::SL_WARNING, ER_NOT_KEYFILE,
      "Index %s is not available in NDB. Use \"ALTER TABLE %s ALTER INDEX %s "
      "INVISIBLE\" to prevent MySQL from attempting to access it, or use "
      "\"ndb_restore --rebuild-indexes\" to rebuild it.",
      key_info->name, t->s->table_name.str, key_info->name);
  return HA_ERR_CRASHED;
}

int ha_ndbcluster::index_init(uint index, bool sorted) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("index: %u  sorted: %d", index, sorted));
  if (index < MAX_KEY && m_index[index].type == UNDEFINED_INDEX)
    return fail_index_offline(table, index);

  if (m_thd_ndb->get_applier()) {
    if (table_share->primary_key == MAX_KEY &&  // hidden pk
        m_thd_ndb->m_unsent_bytes) {
      // Applier starting read from table with hidden pk when there are already
      // defined operations that need to be prepared in order to "read your own
      // writes" as well as handle errors uniformly.
      DBUG_PRINT("info", ("Prepare already defined operations before read"));
      constexpr bool IGNORE_NO_KEY = true;
      if (execute_no_commit(m_thd_ndb, m_thd_ndb->trans, IGNORE_NO_KEY) != 0) {
        m_thd_ndb->trans_tables.reset_stats();
        return ndb_err(m_thd_ndb->trans);
      }
    }
  }

  active_index = index;
  m_sorted = sorted;
  /*
    Locks are are explicitly released in scan
    unless m_lock.type == TL_READ_HIGH_PRIORITY
    and no sub-sequent call to unlock_row()
  */
  m_lock_tuple = false;

  if (table_share->primary_key == MAX_KEY && m_use_partition_pruning) {
    bitmap_union(table->read_set, &m_part_info->full_part_field_set);
  }

  return 0;
}

int ha_ndbcluster::index_end() {
  DBUG_TRACE;
  return close_scan();
}

/**
  Check if key contains null.
*/
static int check_null_in_key(const KEY *key_info, const uchar *key,
                             uint key_len) {
  KEY_PART_INFO *curr_part, *end_part;
  const uchar *end_ptr = key + key_len;
  curr_part = key_info->key_part;
  end_part = curr_part + key_info->user_defined_key_parts;

  for (; curr_part != end_part && key < end_ptr; curr_part++) {
    if (curr_part->null_bit && *key) return 1;

    key += curr_part->store_length;
  }
  return 0;
}

int ha_ndbcluster::index_read(uchar *buf, const uchar *key, uint key_len,
                              enum ha_rkey_function find_flag) {
  key_range start_key, end_key, *end_key_p = nullptr;
  bool descending = false;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d",
                       active_index, key_len, find_flag));

  start_key.key = key;
  start_key.length = key_len;
  start_key.flag = find_flag;
  switch (find_flag) {
    case HA_READ_KEY_EXACT:
      /**
       * Specify as a closed EQ_RANGE.
       * Setting HA_READ_AFTER_KEY seems odd, but this is according
       * to MySQL convention, see opt_range.cc.
       */
      end_key.key = key;
      end_key.length = key_len;
      end_key.flag = HA_READ_AFTER_KEY;
      end_key_p = &end_key;
      break;
    case HA_READ_KEY_OR_PREV:
    case HA_READ_BEFORE_KEY:
    case HA_READ_PREFIX_LAST:
    case HA_READ_PREFIX_LAST_OR_PREV:
      descending = true;
      break;
    default:
      break;
  }
  const int error =
      read_range_first_to_buf(&start_key, end_key_p, descending, m_sorted, buf);
  return error;
}

int ha_ndbcluster::index_next(uchar *buf) {
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  const int error = next_result(buf);
  return error;
}

int ha_ndbcluster::index_prev(uchar *buf) {
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_prev_count);
  const int error = next_result(buf);
  return error;
}

int ha_ndbcluster::index_first(uchar *buf) {
  DBUG_TRACE;
  if (!m_index[active_index].index)
    return fail_index_offline(table, active_index);
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  // Start the ordered index scan and fetch the first row

  // Only HA_READ_ORDER indexes get called by index_first
  const int error =
      ordered_index_scan(nullptr, nullptr, m_sorted, false, buf, nullptr);
  return error;
}

int ha_ndbcluster::index_last(uchar *buf) {
  DBUG_TRACE;
  if (!m_index[active_index].index)
    return fail_index_offline(table, active_index);
  ha_statistic_increment(&System_status_var::ha_read_last_count);
  const int error =
      ordered_index_scan(nullptr, nullptr, m_sorted, true, buf, nullptr);
  return error;
}

int ha_ndbcluster::index_next_same(uchar *buf,
                                   const uchar *key [[maybe_unused]],
                                   uint length [[maybe_unused]]) {
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  const int error = next_result(buf);
  return error;
}

int ha_ndbcluster::index_read_last(uchar *buf, const uchar *key, uint key_len) {
  DBUG_TRACE;
  return index_read(buf, key, key_len, HA_READ_PREFIX_LAST);
}

int ha_ndbcluster::read_range_first_to_buf(const key_range *start_key,
                                           const key_range *end_key, bool desc,
                                           bool sorted, uchar *buf) {
  part_id_range part_spec;
  const NDB_INDEX_TYPE type = get_index_type(active_index);
  const KEY *key_info = table->key_info + active_index;
  int error;
  DBUG_TRACE;
  DBUG_PRINT("info", ("desc: %d, sorted: %d", desc, sorted));

  if (unlikely((error = close_scan()))) return error;

  if (m_use_partition_pruning) {
    assert(m_pushed_join_operation != PUSHED_ROOT);
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part > part_spec.end_part) {
      return HA_ERR_END_OF_FILE;
    }

    if (part_spec.start_part == part_spec.end_part) {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      sorted = false;
      if (unlikely(!get_transaction_part_id(part_spec.start_part, error))) {
        return error;
      }
    }
  }

  switch (type) {
    case PRIMARY_KEY_ORDERED_INDEX:
    case PRIMARY_KEY_INDEX:
      if (start_key && start_key->length == key_info->key_length &&
          start_key->flag == HA_READ_KEY_EXACT) {
        if (!m_thd_ndb->trans)
          if (unlikely(
                  !start_transaction_key(active_index, start_key->key, error)))
            return error;
        DBUG_DUMP("key", start_key->key, start_key->length);
        error = pk_read(
            start_key->key, buf,
            (m_use_partition_pruning) ? &(part_spec.start_part) : nullptr);
        return error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error;
      }
      break;
    case UNIQUE_ORDERED_INDEX:
    case UNIQUE_INDEX:
      if (start_key && start_key->length == key_info->key_length &&
          start_key->flag == HA_READ_KEY_EXACT &&
          !check_null_in_key(key_info, start_key->key, start_key->length)) {
        if (!m_thd_ndb->trans)
          if (unlikely(
                  !start_transaction_key(active_index, start_key->key, error)))
            return error;
        DBUG_DUMP("key", start_key->key, start_key->length);
        error = unique_index_read(start_key->key, buf);
        return error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error;
      } else if (type == UNIQUE_INDEX)
        return full_table_scan(key_info, start_key, end_key, buf);
      break;
    default:
      break;
  }
  if (!m_use_partition_pruning && !m_thd_ndb->trans) {
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    if (part_spec.start_part == part_spec.end_part)
      if (unlikely(!start_transaction_part_id(part_spec.start_part, error)))
        return error;
  }
  // Start the ordered index scan and fetch the first row
  return ordered_index_scan(start_key, end_key, sorted, desc, buf,
                            (m_use_partition_pruning) ? &part_spec : nullptr);
}

int ha_ndbcluster::read_range_first(const key_range *start_key,
                                    const key_range *end_key,
                                    bool /* eq_range */, bool sorted) {
  uchar *buf = table->record[0];
  DBUG_TRACE;
  return read_range_first_to_buf(start_key, end_key, false, sorted, buf);
}

int ha_ndbcluster::read_range_next() {
  DBUG_TRACE;
  return next_result(table->record[0]);
}

int ha_ndbcluster::Copying_alter::save_commit_count(
    Thd_ndb *thd_ndb, const NdbDictionary::Table *ndbtab) {
  NdbError ndb_err;
  Uint64 commit_count;
  if (ndb_get_table_commit_count(thd_ndb->ndb, ndbtab, ndb_err,
                                 &commit_count)) {
    return ndb_to_mysql_error(&ndb_err);
  }

  DBUG_PRINT("info", ("Saving commit count: %llu", commit_count));
  m_saved_commit_count = commit_count;
  return 0;
}

// Check that commit count have not changed since it was saved
int ha_ndbcluster::Copying_alter::check_saved_commit_count(
    Thd_ndb *thd_ndb, const NdbDictionary::Table *ndbtab) const {
  NdbError ndb_err;
  Uint64 commit_count;
  if (ndb_get_table_commit_count(thd_ndb->ndb, ndbtab, ndb_err,
                                 &commit_count)) {
    return ndb_to_mysql_error(&ndb_err);
  }

  DBUG_PRINT("info", ("Comparing commit count: %llu with saved value: %llu",
                      commit_count, m_saved_commit_count));
  if (commit_count != m_saved_commit_count) {
    my_printf_error(
        ER_TABLE_DEF_CHANGED,
        "Detected change to data in source table during copying ALTER "
        "TABLE. Alter aborted to avoid inconsistency.",
        MYF(0));
    return HA_ERR_GENERIC;  // Does not set a new error
  }
  return 0;
}

int ha_ndbcluster::rnd_init(bool) {
  DBUG_TRACE;

  if (int error = close_scan()) {
    return error;
  }

  if (int error = index_init(table_share->primary_key, 0)) {
    return error;
  }

  if (m_thd_ndb->sql_command() == SQLCOM_ALTER_TABLE) {
    // Detected start of scan for copying ALTER TABLE. Save commit count of the
    // scanned (source) table.
    if (int error = copying_alter.save_commit_count(m_thd_ndb, m_table)) {
      return error;
    }
  }

  return 0;
}

int ha_ndbcluster::close_scan() {
  DBUG_TRACE;

  if (m_active_query) {
    m_active_query->close(m_thd_ndb->m_force_send);
    m_active_query = nullptr;
  }

  m_cond.cond_close();

  NdbScanOperation *cursor = m_active_cursor;
  if (!cursor) {
    cursor = m_multi_cursor;
    if (!cursor) return 0;
  }

  int error;
  NdbTransaction *trans = m_thd_ndb->trans;
  if ((error = scan_handle_lock_tuple(cursor, trans)) != 0) return error;

  if (m_thd_ndb->m_unsent_bytes) {
    /*
      Take over any pending transactions to the
      deleting/updating transaction before closing the scan
    */
    DBUG_PRINT("info", ("thd_ndb->m_unsent_bytes: %ld",
                        (long)m_thd_ndb->m_unsent_bytes));
    if (execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0) {
      m_thd_ndb->trans_tables.reset_stats();
      return ndb_err(trans);
    }
  }

  cursor->close(m_thd_ndb->m_force_send, true);
  m_active_cursor = nullptr;
  m_multi_cursor = nullptr;
  return 0;
}

int ha_ndbcluster::rnd_end() {
  DBUG_TRACE;
  return close_scan();
}

int ha_ndbcluster::rnd_next(uchar *buf) {
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);

  int error;
  if (m_active_cursor || m_active_query)
    error = next_result(buf);
  else
    error = full_table_scan(nullptr, nullptr, nullptr, buf);

  return error;
}

/**
  An "interesting" record has been found and it's pk
  retrieved by calling position. Now it's time to read
  the record from db once again.
*/

int ha_ndbcluster::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  // The primary key for the record is stored in pos
  // Perform a pk_read using primary key "index"
  {
    part_id_range part_spec;
    uint key_length = ref_length;
    if (m_user_defined_partitioning) {
      if (table_share->primary_key == MAX_KEY) {
        /*
          The partition id has been fetched from ndb
          and has been stored directly after the hidden key
        */
        DBUG_DUMP("key+part", pos, key_length);
        key_length = ref_length - sizeof(m_part_id);
        part_spec.start_part = part_spec.end_part =
            *(uint32 *)(pos + key_length);
      } else {
        key_range key_spec;
        KEY *key_info = table->key_info + table_share->primary_key;
        key_spec.key = pos;
        key_spec.length = key_length;
        key_spec.flag = HA_READ_KEY_EXACT;
        get_full_part_id_from_key(table, buf, key_info, &key_spec, &part_spec);
        assert(part_spec.start_part == part_spec.end_part);
      }
      DBUG_PRINT("info", ("partition id %u", part_spec.start_part));
    }
    DBUG_DUMP("key", pos, key_length);
    int res = pk_read(
        pos, buf,
        (m_user_defined_partitioning) ? &(part_spec.start_part) : nullptr);
    if (res == HA_ERR_KEY_NOT_FOUND) {
      /**
       * When using rnd_pos
       *   server first retrieves a set of records (typically scans them)
       *   and store a unique identifier (for ndb this is the primary key)
       *   and later retrieves the record again using rnd_pos and the
       *   saved primary key. For ndb, since we only support committed read
       *   the record could have been deleted in between the "save" and
       *   the rnd_pos.
       *   Therefore we return HA_ERR_RECORD_DELETED in this case rather than
       *   HA_ERR_KEY_NOT_FOUND (which will cause statement to be aborted)
       *
       */
      res = HA_ERR_RECORD_DELETED;
    }
    return res;
  }
}

/**
  Store the primary key of this record in ref
  variable, so that the row can be retrieved again later
  using "reference" in rnd_pos.
*/

void ha_ndbcluster::position(const uchar *record) {
  KEY *key_info;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  uchar *buff;
  uint key_length;

  DBUG_TRACE;

  if (table_share->primary_key != MAX_KEY) {
    key_length = ref_length;
    key_info = table->key_info + table_share->primary_key;
    key_part = key_info->key_part;
    end = key_part + key_info->user_defined_key_parts;
    buff = ref;

    for (; key_part != end; key_part++) {
      if (key_part->null_bit) {
        /* Store 0 if the key part is a NULL part */
        if (record[key_part->null_offset] & key_part->null_bit) {
          *buff++ = 1;
          continue;
        }
        *buff++ = 0;
      }

      size_t len = key_part->length;
      const uchar *ptr = record + key_part->offset;
      Field *field = key_part->field;
      if (field->type() == MYSQL_TYPE_VARCHAR) {
        size_t var_length;
        if (field->get_length_bytes() == 1) {
          /**
           * Keys always use 2 bytes length
           */
          buff[0] = ptr[0];
          buff[1] = 0;
          var_length = ptr[0];
          assert(var_length <= len);
          memcpy(buff + 2, ptr + 1, var_length);
        } else {
          var_length = ptr[0] + (ptr[1] * 256);
          assert(var_length <= len);
          memcpy(buff, ptr, var_length + 2);
        }
        /**
          We have to zero-pad any unused VARCHAR buffer so that MySQL is
          able to use simple memcmp to compare two instances of the same
          unique key value to determine if they are equal.
          MySQL does this to compare contents of two 'ref' values.
          (Duplicate weedout algorithm is one such case.)
        */
        memset(buff + 2 + var_length, 0, len - var_length);
        len += 2;
      } else {
        memcpy(buff, ptr, len);
      }
      buff += len;
    }
  } else {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    // If table has user defined partition save the partition id as well
    if (m_user_defined_partitioning) {
      DBUG_PRINT("info", ("Saving partition id %u", m_part_id));
      key_length = ref_length - sizeof(m_part_id);
      memcpy(ref + key_length, (void *)&m_part_id, sizeof(m_part_id));
    } else
      key_length = ref_length;
#ifndef NDEBUG
    constexpr uint NDB_HIDDEN_PRIMARY_KEY_LENGTH = 8;
    const int hidden_no = Ndb_table_map::num_stored_fields(table);
    const NDBCOL *hidden_col = m_table->getColumn(hidden_no);
    assert(hidden_col->getPrimaryKey() && hidden_col->getAutoIncrement() &&
           key_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
#endif
    memcpy(ref, &m_ref, key_length);
  }
#ifndef NDEBUG
  if (table_share->primary_key == MAX_KEY && m_user_defined_partitioning)
    DBUG_DUMP("key+part", ref, key_length + sizeof(m_part_id));
#endif
  DBUG_DUMP("ref", ref, key_length);
}

int ha_ndbcluster::cmp_ref(const uchar *ref1, const uchar *ref2) const {
  DBUG_TRACE;

  if (table_share->primary_key != MAX_KEY) {
    KEY *key_info = table->key_info + table_share->primary_key;
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;

    for (; key_part != end; key_part++) {
      // NOTE: No need to check for null since PK is not-null

      Field *field = key_part->field;
      int result = field->key_cmp(ref1, ref2);
      if (result) {
        return result;
      }

      if (field->type() == MYSQL_TYPE_VARCHAR) {
        ref1 += 2;
        ref2 += 2;
      }

      ref1 += key_part->length;
      ref2 += key_part->length;
    }
    return 0;
  } else {
    return memcmp(ref1, ref2, ref_length);
  }
}

int ha_ndbcluster::info(uint flag) {
  THD *thd = table->in_use;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("flag: %d", flag));

  if (flag & HA_STATUS_POS) DBUG_PRINT("info", ("HA_STATUS_POS"));
  if (flag & HA_STATUS_TIME) DBUG_PRINT("info", ("HA_STATUS_TIME"));
  if (flag & HA_STATUS_CONST) {
    /*
      Set size required by a single record in the MRR 'HANDLER_BUFFER'.
      MRR buffer has both a fixed and a variable sized part.
      Size is calculated assuming max size of the variable part.

      See comments for multi_range_fixed_size() and
      multi_range_max_entry() regarding how the MRR buffer is organized.
    */
    stats.mrr_length_per_rec =
        multi_range_fixed_size(1) +
        multi_range_max_entry(PRIMARY_KEY_INDEX, table_share->reclength);
  }
  if (flag & HA_STATUS_VARIABLE) {
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));

    if (!thd) {
      thd = current_thd;
    }

    if (!m_trans_table_stats) {
      if (check_ndb_connection(thd)) return HA_ERR_NO_CONNECTION;
    }

    /*
      May need to update local copy of statistics in
      'm_trans_table_stats', either directly from datanodes,
      or from NDB_SHARE cached copy (mutex protected), if:
       1) 'ndb_use_exact_count' has been set (by config or user).
       2) HA_STATUS_NO_LOCK -> read from NDB_SHARE cached copy.
       3) Local copy is invalid.
    */
    const bool exact_count = THDVAR(thd, use_exact_count);
    DBUG_PRINT("info", ("exact_count: %d", exact_count));

    const bool no_lock_flag = flag & HA_STATUS_NO_LOCK;
    DBUG_PRINT("info", ("no_lock: %d", no_lock_flag));

    if (exact_count ||                     // 1)
        !no_lock_flag ||                   // 2)
        m_trans_table_stats == nullptr ||  // 3) no trans stats registered
        m_trans_table_stats->invalid())    // 3)
    {
      const int result = update_stats(thd, exact_count || !no_lock_flag);
      if (result) {
        return result;
      }
    } else {
      // Use transaction table stats, these stats are only used by this thread
      // so no locks are required. Just double check that the stats have been
      // updated previously.
      assert(!m_trans_table_stats->invalid());

      // Update handler::stats with rows in table plus rows changed by trans
      // This is doing almost the same thing as in update_stats()
      // i.e the number of records in active transaction plus number of
      // uncommitted are assigned to stats.records
      stats.records = m_trans_table_stats->table_rows +
                      m_trans_table_stats->uncommitted_rows;
      DBUG_PRINT("table_stats",
                 ("records updated from trans stats: %llu ", stats.records));
    }

    const int sql_command = thd_sql_command(thd);
    if (sql_command == SQLCOM_SHOW_TABLE_STATUS ||
        sql_command == SQLCOM_SHOW_KEYS) {
      DBUG_PRINT("table_stats",
                 ("Special case for showing actual number of records: %llu",
                  stats.records));
    } else {
      // Adjust `stats.records` to never be < 2 since optimizer interprets the
      // values 0 and 1 as EXACT.
      // NOTE! It looks like the above statement is correct only when
      // HA_STATS_RECORDS_IS_EXACT is returned from table_flags(), something
      // which ndbcluster does not.
      if (stats.records < 2) {
        DBUG_PRINT("table_stats", ("adjust records %llu -> 2", stats.records));
        stats.records = 2;
      }
    }
    set_rec_per_key(thd);
  }
  if (flag & HA_STATUS_ERRKEY) {
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY dupkey=%u", m_dupkey));
    errkey = m_dupkey;
  }
  if (flag & HA_STATUS_AUTO) {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (m_table && table->found_next_number_field) {
      if (!thd) thd = current_thd;
      if (check_ndb_connection(thd)) return HA_ERR_NO_CONNECTION;
      Ndb *ndb = get_thd_ndb(thd)->ndb;
      NDB_SHARE::Tuple_id_range_guard g(m_share);

      Uint64 auto_increment_value64;
      if (ndb->readAutoIncrementValue(m_table, g.range,
                                      auto_increment_value64) == -1) {
        const NdbError err = ndb->getNdbError();
        ndb_log_error("Error %d in readAutoIncrementValue(): %s", err.code,
                      err.message);
        stats.auto_increment_value = ~(ulonglong)0;
      } else
        stats.auto_increment_value = (ulonglong)auto_increment_value64;
    }
  }

  return 0;
}

/**
   @brief Return statistics for given partition

   @param[out] stat_info    The place where to return updated statistics
   @param[out] checksum     The place where to return checksum (if any)
   @param part_id           Id of the partition to return statistics for
 */
void ha_ndbcluster::get_dynamic_partition_info(ha_statistics *stat_info,
                                               ha_checksum *checksum,
                                               uint part_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("part_id: %d", part_id));

  THD *thd = current_thd;
  if (check_ndb_connection(thd)) {
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return;
  }
  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  // Checksum not supported, set it to 0
  *checksum = 0;

  // Read fresh stats from NDB for given partition (one roundtrip)
  NdbError ndb_error;
  Ndb_table_stats part_stats;
  if (ndb_get_table_statistics(thd, thd_ndb->ndb, m_table, &part_stats,
                               ndb_error, part_id)) {
    if (ndb_error.classification == NdbError::SchemaError) {
      // Updating stats for table failed due to a schema error. Mark the NDB
      // table def as invalid, this will cause also all index defs to be
      // invalidate on close
      m_table->setStatusInvalid();
    }
    ndb_to_mysql_error(&ndb_error);  // Called to push any NDB error as warning

    // Nothing else to do, caller has initialized stat_info to zero
    DBUG_PRINT("error", ("Failed to update stats"));
    return;
  }

  // Copy partition stats into callers stats buffer
  stat_info->records = part_stats.row_count;
  stat_info->mean_rec_length = part_stats.row_size;
  stat_info->data_file_length = part_stats.fragment_memory;
  stat_info->delete_length = part_stats.fragment_extent_free_space;
  stat_info->max_data_file_length = part_stats.fragment_extent_space;
}

int ha_ndbcluster::extra(enum ha_extra_function operation) {
  DBUG_TRACE;
  switch (operation) {
    case HA_EXTRA_IGNORE_DUP_KEY: /* Dup keys don't rollback everything*/
      DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));
      DBUG_PRINT("info", ("Ignoring duplicate key"));
      m_ignore_dup_key = true;
      break;
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
      DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
      m_ignore_dup_key = false;
      break;
    case HA_EXTRA_IGNORE_NO_KEY:
      DBUG_PRINT("info", ("HA_EXTRA_IGNORE_NO_KEY"));
      DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
      m_ignore_no_key = true;
      break;
    case HA_EXTRA_NO_IGNORE_NO_KEY:
      DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_NO_KEY"));
      DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
      m_ignore_no_key = false;
      break;
    case HA_EXTRA_WRITE_CAN_REPLACE:
      DBUG_PRINT("info", ("HA_EXTRA_WRITE_CAN_REPLACE"));
      if (!m_has_unique_index ||
          /*
             Always set if slave, quick fix for bug 27378
             or if manual binlog application, for bug 46662
          */
          applying_binlog(current_thd)) {
        DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
        m_use_write = true;
      }
      break;
    case HA_EXTRA_WRITE_CANNOT_REPLACE:
      DBUG_PRINT("info", ("HA_EXTRA_WRITE_CANNOT_REPLACE"));
      DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
      m_use_write = false;
      break;
    case HA_EXTRA_DELETE_CANNOT_BATCH:
      DBUG_PRINT("info", ("HA_EXTRA_DELETE_CANNOT_BATCH"));
      m_delete_cannot_batch = true;
      break;
    case HA_EXTRA_UPDATE_CANNOT_BATCH:
      DBUG_PRINT("info", ("HA_EXTRA_UPDATE_CANNOT_BATCH"));
      m_update_cannot_batch = true;
      break;
    // We don't implement 'KEYREAD'. However, KEYREAD also implies
    // DISABLE_JOINPUSH.
    case HA_EXTRA_KEYREAD:
      DBUG_PRINT("info", ("HA_EXTRA_KEYREAD"));
      m_disable_pushed_join = true;
      break;
    case HA_EXTRA_NO_KEYREAD:
      DBUG_PRINT("info", ("HA_EXTRA_NO_KEYREAD"));
      m_disable_pushed_join = false;
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

  return 0;
}

bool ha_ndbcluster::start_read_removal() {
  THD *thd = table->in_use;
  DBUG_TRACE;

  if (uses_blob_value(table->write_set)) {
    DBUG_PRINT("exit", ("No! Blob field in write_set"));
    return false;
  }

  if (thd->lex->sql_command == SQLCOM_DELETE && table_share->blob_fields) {
    DBUG_PRINT("exit", ("No! DELETE from table with blob(s)"));
    return false;
  }

  if (table_share->primary_key == MAX_KEY) {
    DBUG_PRINT("exit", ("No! Table with hidden key"));
    return false;
  }

  if (bitmap_is_overlapping(table->write_set, m_pk_bitmap_p)) {
    DBUG_PRINT("exit", ("No! Updating primary key"));
    return false;
  }

  if (m_has_unique_index) {
    for (uint i = 0; i < table_share->keys; i++) {
      const KEY *key = table->key_info + i;
      if ((key->flags & HA_NOSAME) &&
          bitmap_is_overlapping(table->write_set, m_key_fields[i])) {
        DBUG_PRINT("exit", ("No! Unique key %d is updated", i));
        return false;
      }
    }
  }
  m_read_before_write_removal_possible = true;
  DBUG_PRINT("exit", ("Yes, rbwr is possible!"));
  return true;
}

ha_rows ha_ndbcluster::end_read_removal(void) {
  DBUG_TRACE;
  assert(m_read_before_write_removal_possible);
  DBUG_PRINT("info",
             ("updated: %llu, deleted: %llu", m_rows_updated, m_rows_deleted));
  return m_rows_updated + m_rows_deleted;
}

int ha_ndbcluster::reset() {
  DBUG_TRACE;
  m_cond.cond_clear();

  assert(m_active_query == nullptr);
  if (m_pushed_join_operation == PUSHED_ROOT)  // Root of pushed query
  {
    delete m_pushed_join_member;  // Also delete QueryDef
  }
  m_pushed_join_member = nullptr;
  m_pushed_join_operation = -1;
  m_disable_pushed_join = false;

  /* reset flags set by extra calls */
  m_read_before_write_removal_possible = false;
  m_read_before_write_removal_used = false;
  m_rows_updated = m_rows_deleted = 0;
  m_ignore_dup_key = false;
  m_use_write = false;
  m_ignore_no_key = false;
  m_rows_to_insert = (ha_rows)1;
  m_delete_cannot_batch = false;
  m_update_cannot_batch = false;

  assert(m_is_bulk_delete == false);
  m_is_bulk_delete = false;
  return 0;
}

int ha_ndbcluster::flush_bulk_insert(bool allow_batch) {
  NdbTransaction *trans = m_thd_ndb->trans;
  DBUG_TRACE;
  assert(trans);

  if (m_thd_ndb->check_trans_option(Thd_ndb::TRANS_TRANSACTIONS_OFF)) {
    /*
      signal that transaction will be broken up and hence cannot
      be rolled back
    */
    THD *thd = table->in_use;
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::SESSION);
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);
    if (execute_commit(m_thd_ndb, trans, m_thd_ndb->m_force_send,
                       m_ignore_no_key) != 0) {
      m_thd_ndb->trans_tables.reset_stats();
      return ndb_err(trans);
    }
    if (trans->restart() != 0) {
      assert(0);
      return -1;
    }
    return 0;
  }

  if (!allow_batch &&
      execute_no_commit(m_thd_ndb, trans, m_ignore_no_key) != 0) {
    m_thd_ndb->trans_tables.reset_stats();
    return ndb_err(trans);
  }

  return 0;
}

/**
  Start of an insert, remember number of rows to be inserted, it will
  be used in write_row and get_autoincrement to send an optimal number
  of rows in each roundtrip to the server.

  @param
   rows     number of rows to insert, 0 if unknown
*/

void ha_ndbcluster::start_bulk_insert(ha_rows rows) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("rows: %d", (int)rows));

  if (!m_use_write && m_ignore_dup_key) {
    /*
      compare if expression with that in write_row
      we have a situation where peek_indexed_rows() will be called
      so we cannot batch
    */
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    m_rows_to_insert = 1;
    return;
  }
  if (rows == (ha_rows)0) {
    /* We don't know how many will be inserted, guess */
    m_rows_to_insert = (m_autoincrement_prefetch > DEFAULT_AUTO_PREFETCH)
                           ? m_autoincrement_prefetch
                           : DEFAULT_AUTO_PREFETCH;
    m_autoincrement_prefetch = m_rows_to_insert;
  } else {
    m_rows_to_insert = rows;
    if (m_autoincrement_prefetch < m_rows_to_insert)
      m_autoincrement_prefetch = m_rows_to_insert;
  }
}

/**
  End of an insert.
*/
int ha_ndbcluster::end_bulk_insert() {
  int error = 0;

  DBUG_TRACE;
  // Check if last inserts need to be flushed

  THD *thd = table->in_use;
  Thd_ndb *thd_ndb = m_thd_ndb;

  if (!thd_allow_batch(thd) && thd_ndb->m_unsent_bytes) {
    const bool allow_batch = (thd_ndb->m_handler != nullptr);
    error = flush_bulk_insert(allow_batch);
    if (error != 0) {
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

  m_rows_to_insert = 1;
  return error;
}

/**
  How many seeks it will take to read through the table.

  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/
double ha_ndbcluster::scan_time() {
  DBUG_TRACE;
  const double res = rows2double(stats.records * 1000);
  DBUG_PRINT("exit", ("table: %s value: %f", table_share->table_name.str, res));
  return res;
}

/**
  read_time() need to differentiate between single row type lookups,
  and accesses where an ordered index need to be scanned.
  The later will need to scan all fragments, which might be
  significantly more expensive - imagine a deployment with hundreds
  of partitions.
 */
double ha_ndbcluster::read_time(uint index, uint ranges, ha_rows rows) {
  DBUG_TRACE;
  assert(rows > 0);
  assert(ranges > 0);
  assert(rows >= ranges);

  const NDB_INDEX_TYPE index_type =
      (index < MAX_KEY)    ? get_index_type(index)
      : (index == MAX_KEY) ? PRIMARY_KEY_INDEX  // Hidden primary key
                           : UNDEFINED_INDEX;   // -> worst index

  // fanout_factor is intended to compensate for the amount
  // of roundtrips between API <-> data node and between data nodes
  // themself by the different index type. As an initial guess
  // we assume a single full roundtrip for each 'range'.
  double fanout_factor;

  /**
   * Note that for now we use the default handler cost estimate
   * 'rows2double(ranges + rows)' as the baseline - Even if it
   * might have some obvious flaws. For now it is more important
   * to get the relative cost between PK/UQ and order index scan
   * more correct. It is also a matter of not changing too many
   * existing MTR tests. (and customer queries as well!)
   *
   * We also estimate the same cost for a request roundtrip as
   * for returning a row. Thus the baseline cost 'ranges + rows'
   */
  if (index_type == PRIMARY_KEY_INDEX) {
    assert(index == table->s->primary_key);
    // Need a full roundtrip for each row
    fanout_factor = 1.0 * rows2double(rows);
  } else if (index_type == UNIQUE_INDEX) {
    // Need to lookup first on UQ, then on PK, + lock/unlock
    fanout_factor = 2.0 * rows2double(rows);

  } else if (rows > ranges || index_type == ORDERED_INDEX ||
             index_type == UNDEFINED_INDEX) {
    // Assume || need a range scan

    // TODO: - Handler call need a parameter specifying whether
    //         key was fully specified or not (-> scan or lookup)
    //       - The range scan could be pruned -> lower cost, or
    //       - The scan need to be 'ordered' -> higher cost.
    //       - Returning multiple rows pr range has a lower
    //         pr. row cost?
    const uint fragments_to_scan =
        m_table->getFullyReplicated() ? 1 : m_table->getPartitionCount();

    // The range scan does one API -> TC request, which scale out the
    // requests to all fragments. Assume a somewhat (*0.5) lower cost
    // for these requests, as they are not full roundtrips back to the API
    fanout_factor = (double)ranges * (1.0 + ((double)fragments_to_scan * 0.5));

  } else {
    assert(rows == ranges);

    // Assume a set of PK/UQ single row lookups.
    // We assume the hash key is used for a direct lookup
    if (index_type == PRIMARY_KEY_ORDERED_INDEX) {
      assert(index == table->s->primary_key);
      fanout_factor = (double)ranges * 1.0;
    } else {
      assert(index_type == UNIQUE_ORDERED_INDEX);
      // Unique key access has a higher cost than PK. Need to first
      // lookup in index, then use that to lookup the row + lock & unlock
      fanout_factor = (double)ranges * 2.0;  // Assume twice as many roundtrips
    }
  }
  return fanout_factor + rows2double(rows);
}

/**
 * Estimate the cost for reading the specified number of rows,
 * using 'index'. Note that there is no such thing as a 'page'-read
 * in ha_ndbcluster. Unfortunately, the optimizer does some
 * assumptions about an underlying page based storage engine,
 * which explains the name.
 *
 * In the NDB implementation we simply ignore the 'page', and
 * calculate it as any other read_cost()
 */
double ha_ndbcluster::page_read_cost(uint index, double rows) {
  DBUG_TRACE;
  return read_cost(index, 1, rows).total_cost();
}

/**
 * Estimate the upper cost for reading rows in a seek-and-read fashion.
 * Calculation is based on the worst index we can find for this table, such
 * that any other better way of reading the rows will be preferred.
 *
 * Note that worst_seek will be compared against page_read_cost().
 * Thus, it need to calculate the cost using comparable 'metrics'.
 */
double ha_ndbcluster::worst_seek_times(double reads) {
  // Specifying the 'UNDEFINED_INDEX' is a special case in read_time(),
  // where the cost for the most expensive/worst index will be calculated.
  const uint undefined_index = MAX_KEY + 1;
  return page_read_cost(undefined_index, std::max(reads, 1.0));
}

/*
  Convert MySQL table locks into locks supported by Ndb Cluster.
  Note that MySQL Cluster does currently not support distributed
  table locks, so to be safe one should set cluster in Single
  User Mode, before relying on table locks when updating tables
  from several MySQL servers
*/

THR_LOCK_DATA **ha_ndbcluster::store_lock(THD *thd, THR_LOCK_DATA **to,
                                          enum thr_lock_type lock_type) {
  DBUG_TRACE;

  DBUG_PRINT("info", ("table %s, request lock_type: %d",
                      table_share->table_name.str, lock_type));

  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK) {
    /* If we are not doing a LOCK TABLE, then allow multiple
       writers */

    /* Since NDB does not currently have table locks
       this is treated as a ordinary lock */

    const bool in_lock_tables = thd_in_lock_tables(thd);
    const int sql_command = thd_sql_command(thd);
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
        !(in_lock_tables && sql_command == SQLCOM_LOCK_TABLES))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
       MySQL would use the lock TL_READ_NO_INSERT on t2, and that
       would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
       to t2. If table is not explicitly locked or part of an ALTER TABLE
       then convert the lock to a normal read lock to allow
       concurrent inserts to t2. */

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables &&
        sql_command != SQLCOM_ALTER_TABLE)
      lock_type = TL_READ;

    m_lock.type = lock_type;
  }
  *to++ = &m_lock;

  DBUG_PRINT("exit", ("lock_type: %d", lock_type));

  return to;
}

void Thd_ndb::transaction_checks() {
  THD *thd = m_thd;

  if (thd_sql_command(thd) == SQLCOM_LOAD || !THDVAR(thd, use_transactions)) {
    // Turn off transactional behaviour for the duration of this
    // statement/transaction
    set_trans_option(TRANS_TRANSACTIONS_OFF);
  }

  m_force_send = THDVAR(thd, force_send);
  if (get_applier() == nullptr) {
    // Normal user thread
    m_batch_size = THDVAR(thd, batch_size);
    m_blob_write_batch_size = THDVAR(thd, blob_write_batch_bytes);
  } else {
    // Applier benefit from higher batch size, thus use the maximum
    // between the default and the global batch_size if
    // replica_batch_size is unset
    m_batch_size =
        opt_ndb_replica_batch_size == DEFAULT_REPLICA_BATCH_SIZE
            ? std::max(opt_ndb_replica_batch_size, THDVAR(nullptr, batch_size))
            : opt_ndb_replica_batch_size;

    m_blob_write_batch_size =
        opt_ndb_replica_blob_write_batch_bytes == DEFAULT_REPLICA_BATCH_SIZE
            ? std::max(opt_ndb_replica_blob_write_batch_bytes,
                       THDVAR(nullptr, blob_write_batch_bytes))
            : opt_ndb_replica_blob_write_batch_bytes;

    /* Do not use hinted TC selection in slave thread */
    THDVAR(thd, optimized_node_selection) =
        THDVAR(nullptr, optimized_node_selection) & 1; /* using global value */
  }

  /* Set Ndb object's optimized_node_selection (locality) value */
  ndb->set_optimized_node_selection(THDVAR(thd, optimized_node_selection) & 1);
}

int ha_ndbcluster::start_statement(THD *thd, Thd_ndb *thd_ndb,
                                   uint table_count) {
  DBUG_TRACE;

  // Setup m_thd_ndb for quick access, to be used in all functions during trans
  m_thd_ndb = thd_ndb;

  m_thd_ndb->transaction_checks();

  if (table_count == 0) {
    const NdbTransaction *const trans = m_thd_ndb->trans;
    ndb_thd_register_trans(thd, trans == nullptr);

    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
      m_thd_ndb->m_handler = nullptr;
    } else {
      // This is an autocommit, setup reference to this handler for use in
      // the commit phase, deferring execute for optimization reasons
      m_thd_ndb->m_handler = this;
    }

    if (trans == nullptr) {
      // Reset trans options
      m_thd_ndb->reset_trans_options();

      // Reset trans table stats
      m_thd_ndb->trans_tables.clear();

      // Check if NDB transaction should be started early
      const uint opti_node_select = THDVAR(thd, optimized_node_selection);
      DBUG_PRINT("enter", ("optimized_node_selection: %u", opti_node_select));
      if (!(opti_node_select & 2) || thd_sql_command(thd) == SQLCOM_LOAD) {
        int error;
        if (unlikely(!start_transaction(error))) {
          return error;
        }
      }

      if (!(thd_test_options(thd, OPTION_BIN_LOG)) ||
          thd->variables.binlog_format == BINLOG_FORMAT_STMT) {
        m_thd_ndb->set_trans_option(Thd_ndb::TRANS_NO_LOGGING);
      }
    }

  } else {
    // There are more than one handler involved, execute deferral not possible
    m_thd_ndb->m_handler = nullptr;
  }

  // store thread specific data first to set the right context
  m_autoincrement_prefetch = THDVAR(thd, autoincrement_prefetch_sz);

  release_blobs_buffer();

  // Register table stats for transaction
  m_trans_table_stats = m_thd_ndb->trans_tables.register_stats(m_share);
  if (m_trans_table_stats == nullptr) {
    return 1;
  }

  return 0;
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

int ha_ndbcluster::external_lock(THD *thd, int lock_type) {
  DBUG_TRACE;
  if (lock_type != F_UNLCK) {
    /*
      Check that this handler instance has a connection
      set up to the Ndb object of thd
    */
    if (check_ndb_connection(thd)) {
      return 1;
    }
    Thd_ndb *thd_ndb = get_thd_ndb(thd);

    DBUG_PRINT("enter", ("lock_type != F_UNLCK "
                         "this: %p  thd: %p  thd_ndb: %p  "
                         "thd_ndb->external_lock_count: %d",
                         this, thd, thd_ndb, thd_ndb->external_lock_count));

    const int error =
        start_statement(thd, thd_ndb, thd_ndb->external_lock_count);
    if (error != 0) {
      return error;
    }
    thd_ndb->external_lock_count++;
    return 0;
  } else {
    Thd_ndb *thd_ndb = m_thd_ndb;
    assert(thd_ndb);

    DBUG_PRINT("enter", ("lock_type == F_UNLCK "
                         "this: %p  thd: %p  thd_ndb: %p  "
                         "thd_ndb->external_lock_count: %d",
                         this, thd, thd_ndb, thd_ndb->external_lock_count));

    thd_ndb->external_lock_count--;
    if (thd_ndb->external_lock_count == 0) {
      DBUG_PRINT("trans", ("Last external_lock() unlock"));

      const bool autocommit_enabled =
          !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN);
      // Only the 'CREATE TABLE ... SELECT' variant of the
      // SQLCOM_CREATE_TABLE DDL calls external_lock
      const bool is_create_table_select =
          (thd_sql_command(thd) == SQLCOM_CREATE_TABLE);

      if (thd_ndb->trans && (autocommit_enabled || is_create_table_select)) {
        /*
          Unlock is done without transaction commit / rollback.
          This happens if the thread didn't update any rows as a part of normal
          DMLs or `CREATE TABLE ... SELECT` DDL .
          We must in this case close the transaction to release resources
        */
        DBUG_PRINT("trans", ("ending non-updating transaction"));
        thd_ndb->ndb->closeTransaction(thd_ndb->trans);
        thd_ndb->trans = nullptr;
        thd_ndb->m_handler = nullptr;
      }
    }

    // Disconnect from transaction table stats
    // NOTE! The actual stats are not released until next trans starts, could
    // perhaps be done earlier
    m_trans_table_stats = nullptr;

    /*
      This is the place to make sure this handler instance
      no longer are connected to the active transaction.

      And since the handler is no longer part of the transaction
      it can't have open cursors, ops, queries or blobs pending.
    */
    m_thd_ndb = nullptr;

    assert(m_active_query == nullptr);
    if (m_active_query) DBUG_PRINT("warning", ("m_active_query != NULL"));
    m_active_query = nullptr;

    if (m_active_cursor) DBUG_PRINT("warning", ("m_active_cursor != NULL"));
    m_active_cursor = nullptr;

    if (m_multi_cursor) DBUG_PRINT("warning", ("m_multi_cursor != NULL"));
    m_multi_cursor = nullptr;

    return 0;
  }
}

/**
  Unlock the last row read in an open scan.
  Rows are unlocked by default in ndb, but
  for SELECT FOR UPDATE and SELECT LOCK WITH SHARE MODE
  locks are kept if unlock_row() is not called.
*/

void ha_ndbcluster::unlock_row() {
  DBUG_TRACE;

  DBUG_PRINT("info", ("Unlocking row"));
  m_lock_tuple = false;
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

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type) {
  DBUG_TRACE;
  assert(thd == table->in_use);

  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  const int error = start_statement(thd, thd_ndb, thd_ndb->start_stmt_count);
  if (error != 0) {
    return error;
  }
  thd_ndb->start_stmt_count++;
  return 0;
}

NdbTransaction *ha_ndbcluster::start_transaction_row(
    const NdbRecord *ndb_record, const uchar *record, int &error) {
  NdbTransaction *trans;
  DBUG_TRACE;
  assert(m_thd_ndb);
  assert(m_thd_ndb->trans == nullptr);

  m_thd_ndb->transaction_checks();

  Ndb *ndb = m_thd_ndb->ndb;

  Uint32 tmp[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  char *buf = (char *)&tmp[0];
  trans =
      ndb->startTransaction(ndb_record, (const char *)record, buf, sizeof(tmp));

  if (trans) {
    m_thd_ndb->increment_hinted_trans_count();
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    return m_thd_ndb->trans = trans;
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  return nullptr;
}

NdbTransaction *ha_ndbcluster::start_transaction_key(uint index_num,
                                                     const uchar *key_data,
                                                     int &error) {
  NdbTransaction *trans;
  DBUG_TRACE;
  assert(m_thd_ndb);
  assert(m_thd_ndb->trans == nullptr);

  m_thd_ndb->transaction_checks();

  Ndb *ndb = m_thd_ndb->ndb;
  const NdbRecord *key_rec = m_index[index_num].ndb_unique_record_key;

  Uint32 tmp[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  char *buf = (char *)&tmp[0];
  trans =
      ndb->startTransaction(key_rec, (const char *)key_data, buf, sizeof(tmp));

  if (trans) {
    m_thd_ndb->increment_hinted_trans_count();
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    return m_thd_ndb->trans = trans;
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  return nullptr;
}

NdbTransaction *ha_ndbcluster::start_transaction(int &error) {
  NdbTransaction *trans;
  DBUG_TRACE;

  assert(m_thd_ndb);
  assert(m_thd_ndb->trans == nullptr);

  if (DBUG_EVALUATE_IF("ndb_fail_start_trans", true, false)) {
    fprintf(stderr, "ndb_fail_start_trans\n");
    error = HA_ERR_NO_CONNECTION;
    return nullptr;
  }

  m_thd_ndb->transaction_checks();

  if ((trans = m_thd_ndb->ndb->startTransaction(m_table))) {
    // NOTE! No hint provided when starting transaction

    DBUG_PRINT("info", ("Delayed allocation of TC"));
    return m_thd_ndb->trans = trans;
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  return nullptr;
}

NdbTransaction *ha_ndbcluster::start_transaction_part_id(Uint32 part_id,
                                                         int &error) {
  NdbTransaction *trans;
  DBUG_TRACE;

  assert(m_thd_ndb);
  assert(m_thd_ndb->trans == nullptr);

  m_thd_ndb->transaction_checks();

  if ((trans = m_thd_ndb->ndb->startTransaction(m_table, part_id))) {
    m_thd_ndb->increment_hinted_trans_count();
    DBUG_PRINT("info", ("Delayed allocation of TC"));
    return m_thd_ndb->trans = trans;
  }

  ERR_SET(m_thd_ndb->ndb->getNdbError(), error);
  return nullptr;
}

/**
  Static error print function called from static handler method
  ndbcluster_commit and ndbcluster_rollback.
*/
static int ndbcluster_print_error(NdbTransaction *trans,
                                  ha_ndbcluster *ndb_handler) {
  DBUG_TRACE;
  assert(trans);
  int error;

  if (ndb_handler != nullptr) {
    error = ndb_handler->ndb_err(trans);
    ndb_handler->print_error(error, MYF(0));
  } else {
    /*
      ndb_handler will be null if the transaction involves multiple
      tables or if autocommit is off. During such cases, create a
      new handler and, report error through it..
    */
    TABLE_SHARE share;
    error = ndb_to_mysql_error(&trans->getNdbError());
    if (error != -1) {
      const NdbOperation *error_op = trans->getNdbErrorOperation();
      const char *tab_name = (error_op) ? error_op->getTableName() : "";
      if (tab_name == nullptr) {
        assert(tab_name != nullptr);
        tab_name = "";
      }
      share.db.str = "";
      share.db.length = 0;
      share.table_name.str = tab_name;
      share.table_name.length = strlen(tab_name);
      ha_ndbcluster error_handler(ndbcluster_hton, &share);
      error_handler.print_error(error, MYF(0));
    }
  }
  return error;
}

/**
  Commit a transaction started in NDB.
*/

int ndbcluster_commit(handlerton *, THD *thd, bool all) {
  int res = 0;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NdbTransaction *trans = thd_ndb->trans;
  bool retry_slave_trans = false;

  DBUG_TRACE;
  assert(ndb);
  DBUG_PRINT("enter", ("Commit %s", (all ? "all" : "stmt")));

  Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx();
  if (all && ddl_ctx != nullptr && ddl_ctx->has_uncommitted_schema_changes()) {
    /* There is an ongoing DDL transaction that needs to be committed.
       Call commit on the DDL transaction context. */
    ddl_ctx->commit();
  }

  // Reset reference counter for start_stmt()
  thd_ndb->start_stmt_count = 0;

  if (trans == nullptr) {
    DBUG_PRINT("info", ("trans == NULL"));
    return 0;
  }

  Ndb_applier *const applier = thd_ndb->get_applier();

  if (!all && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
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

    // To achieve parallelism when using more than one worker, any defined
    // operations should be prepared in NDB before entering the serial commit
    // phase. The last part of parallel phase is normally when the worker thread
    // has completed the current binlog group and commits the statement.
    if (applier && applier->get_num_workers() > 1) {
      if (thd_ndb->m_unsent_bytes) {
        DBUG_PRINT("info", ("Applier preparing defined operations"));
        res = execute_no_commit(thd_ndb, trans, true);
        if (res != 0) {
          // Fatal transaction error occured
          const NdbError &trans_error = trans->getNdbError();
          if (trans_error.code == 4350) {  // Transaction already aborted
            thd_ndb->push_ndb_error_warning(trans_error);
            res = HA_ERR_ROLLED_BACK;
          } else {
            res = ndbcluster_print_error(trans, thd_ndb->m_handler);
          }
        }
      }
    }

    return res;
  }
  thd_ndb->save_point_count = 0;

  if (applier) {
    // Define operations for transaction to change the ndb_apply_status table
    if (!applier->define_apply_status_operations()) {
      // Failed to define ndb_apply_status operations, catch in debug only
      assert(false);
    }

    /* If this slave transaction has included conflict detecting ops
     * and some defined operations are not yet sent, then perform
     * an execute(NoCommit) before committing, as conflict op handling
     * is done by execute(NoCommit)
     */
    if (applier->check_flag(Ndb_applier::OPS_DEFINED) &&
        thd_ndb->m_unsent_bytes) {
      res = execute_no_commit(thd_ndb, trans, true);
    }

    if (likely(res == 0)) {
      res = applier->atConflictPreCommit(retry_slave_trans);
    }

    if (likely(res == 0)) {
      res = execute_commit(thd_ndb, trans, 1, true);
    }
  } else {
    if (thd_ndb->m_handler &&
        thd_ndb->m_handler->m_read_before_write_removal_possible) {
      // This is an autocommit involving only one table and rbwr is on, thus
      // the transaction should already have been committed early
      DBUG_PRINT("info", ("autocommit+rbwr, transaction committed early"));
      switch (trans->commitStatus()) {
        case NdbTransaction::Committed:
        case NdbTransaction::Aborted:
          // Already committed or aborted
          break;
        case NdbTransaction::NeedAbort:
          // Commit attempt failed and rollback is needed
          res = -1;
          assert(false);
          break;
        default:
          // Commit was never attempted - should not be possible
          ndb_log_error(
              "INTERNAL ERROR: found uncommitted autocommit+rbwr transaction, "
              "commit status: %d",
              trans->commitStatus());
          abort();
          break;
      }
    } else {
      const bool ignore_error = applying_binlog(thd);
      res =
          execute_commit(thd_ndb, trans, THDVAR(thd, force_send), ignore_error);
    }
  }

  if (res != 0) {
    const NdbError &trans_error = trans->getNdbError();
    if (retry_slave_trans) {
      if (!applier->check_retry_trans()) {
        /*
           Applier retried transaction too many times, print error and exit -
           normal too many retries mechanism will cause exit
         */
        ndb_log_error("Replica: retried transaction in vain. Giving up.");
      }
      res = ER_GET_TEMPORARY_ERRMSG;
    } else if (trans_error.code == 4350) {  // Transaction already aborted
      thd_ndb->push_ndb_error_warning(trans_error);
      res = HA_ERR_ROLLED_BACK;
    } else {
      res = ndbcluster_print_error(trans, thd_ndb->m_handler);
    }
  } else {
    // Update cached table stats for tables being part of transaction
    thd_ndb->trans_tables.update_cached_stats_with_committed();
  }

  ndb->closeTransaction(trans);
  thd_ndb->trans = nullptr;
  thd_ndb->m_handler = nullptr;

  return res;
}

/**
 * @brief Rollback any ongoing DDL transaction
 * @param thd_ndb   The Thd_ndb object
 */
static void ndbcluster_rollback_ddl(Thd_ndb *thd_ndb) {
  Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx();
  if (ddl_ctx != nullptr && ddl_ctx->has_uncommitted_schema_changes()) {
    /* There is an ongoing DDL transaction that needs to be rolled back.
       Call rollback on the DDL transaction context. */
    if (!ddl_ctx->rollback()) {
      thd_ndb->push_warning("DDL rollback failed.");
    }
  }
}

/**
  Rollback a transaction started in NDB.
*/

static int ndbcluster_rollback(handlerton *, THD *thd, bool all) {
  int res = 0;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NdbTransaction *trans = thd_ndb->trans;

  DBUG_TRACE;
  DBUG_PRINT("enter", ("all: %d  thd_ndb->save_point_count: %d", all,
                       thd_ndb->save_point_count));
  assert(ndb);

  // Reset reference counter for start_stmt()
  thd_ndb->start_stmt_count = 0;

  if (trans == nullptr) {
    // NdbTransaction was never started
    DBUG_PRINT("info", ("trans == NULL"));
    // Rollback any DDL changes made as a part of this transaction if this
    // rollback has been called at the end of transaction
    if (all) ndbcluster_rollback_ddl(thd_ndb);
    return 0;
  }
  if (!all && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) &&
      (thd_ndb->save_point_count > 0)) {
    /*
      Ignore end-of-statement until real rollback or commit is called
      as ndb does not support rollback statement
      - mark that rollback was unsuccessful, this will cause full rollback
      of the transaction
    */
    DBUG_PRINT("info", ("Rollback before start or end-of-statement only"));
    thd_mark_transaction_to_rollback(thd, 1);
    my_error(ER_WARN_ENGINE_TRANSACTION_ROLLBACK, MYF(0), "NDB");
    return 0;
  }
  thd_ndb->save_point_count = 0;

  thd_ndb->m_unsent_bytes = 0;
  thd_ndb->m_unsent_blob_ops = false;
  thd_ndb->m_execute_count++;
  DBUG_PRINT("info", ("execute_count: %u", thd_ndb->m_execute_count));
  if (trans->execute(NdbTransaction::Rollback) != 0) {
    res = ndbcluster_print_error(trans, thd_ndb->m_handler);
  }
  ndb->closeTransaction(trans);
  thd_ndb->trans = nullptr;
  thd_ndb->m_handler = nullptr;

  Ndb_applier *const applier = thd_ndb->get_applier();
  if (applier) {
    applier->atTransactionAbort();
  }

  // Rollback any DDL changes made as a part of this transaction
  ndbcluster_rollback_ddl(thd_ndb);

  return res;
}

/*
  @brief  Finalize a DDL transaction by wrapping it up if required and
          then clear the DDL transaction context stored in the Thd_ndb.

  @param  thd Thread object
*/
static void ndbcluster_post_ddl(THD *thd) {
  DBUG_TRACE;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx();
  if (ddl_ctx != nullptr) {
    /* Run the post_ddl_hooks to wrap up the ddl commit or rollback */
    if (!ddl_ctx->run_post_ddl_hooks()) {
      thd_ndb->push_warning("Post DDL hooks failed to update schema.");
    }
    /* Destroy and clear the ddl_ctx in thd_ndb */
    thd_ndb->clear_ddl_transaction_ctx();
  }
}

static const char *ndb_table_modifier_prefix = "NDB_TABLE=";

/* Modifiers that we support currently */
static const struct NDB_Modifier ndb_table_modifiers[] = {
    {NDB_Modifier::M_BOOL, STRING_WITH_LEN("NOLOGGING"), 0, {0}},
    {NDB_Modifier::M_BOOL, STRING_WITH_LEN("READ_BACKUP"), 0, {0}},
    {NDB_Modifier::M_BOOL, STRING_WITH_LEN("FULLY_REPLICATED"), 0, {0}},
    {NDB_Modifier::M_STRING, STRING_WITH_LEN("PARTITION_BALANCE"), 0, {0}},
    {NDB_Modifier::M_BOOL, nullptr, 0, 0, {0}}};

static const char *ndb_column_modifier_prefix = "NDB_COLUMN=";

static const struct NDB_Modifier ndb_column_modifiers[] = {
    {NDB_Modifier::M_BOOL, STRING_WITH_LEN("MAX_BLOB_PART_SIZE"), 0, {0}},
    {NDB_Modifier::M_STRING, STRING_WITH_LEN("BLOB_INLINE_SIZE"), 0, {0}},
    {NDB_Modifier::M_BOOL, nullptr, 0, 0, {0}}};

static bool ndb_column_is_dynamic(THD *thd, Field *field,
                                  HA_CREATE_INFO *create_info,
                                  bool use_dynamic_as_default,
                                  NDBCOL::StorageType type) {
  DBUG_TRACE;
  /*
    Check if COLUMN_FORMAT is declared FIXED or DYNAMIC.

    The COLUMN_FORMAT for all non primary key columns defaults to DYNAMIC,
    unless ROW_FORMAT is explicitly defined.

    If an explicit declaration of ROW_FORMAT as FIXED contradicts
    with a dynamic COLUMN_FORMAT a warning will be issued.

    The COLUMN_FORMAT can also be overridden with --ndb-default-column-format.

    NOTE! For COLUMN_STORAGE defined as DISK, the DYNAMIC COLUMN_FORMAT is not
    supported and a warning will be issued if explicitly declared.
   */
  const bool default_was_fixed =
      (opt_ndb_default_column_format == NDB_DEFAULT_COLUMN_FORMAT_FIXED) ||
      (field->table->s->mysql_version < NDB_VERSION_DYNAMIC_IS_DEFAULT);

  bool dynamic;
  switch (field->column_format()) {
    case (COLUMN_FORMAT_TYPE_FIXED):
      dynamic = false;
      break;
    case (COLUMN_FORMAT_TYPE_DYNAMIC):
      dynamic = true;
      break;
    case (COLUMN_FORMAT_TYPE_DEFAULT):
    default:
      if (create_info->row_type == ROW_TYPE_DEFAULT) {
        if (default_was_fixed ||  // Created in old version where fixed was
                                  // the default choice
            field->is_flag_set(PRI_KEY_FLAG))  // Primary key
        {
          dynamic = use_dynamic_as_default;
        } else {
          dynamic = true;
        }
      } else
        dynamic = (create_info->row_type == ROW_TYPE_DYNAMIC);
      break;
  }
  if (type == NDBCOL::StorageTypeDisk) {
    if (dynamic) {
      DBUG_PRINT("info", ("Dynamic disk stored column %s changed to static",
                          field->field_name));
      dynamic = false;
    }
    if (thd && field->column_format() == COLUMN_FORMAT_TYPE_DYNAMIC) {
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
      if (thd && (dynamic || field_type_forces_var_part(field->type()))) {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "Row format FIXED incompatible with "
                            "dynamic attribute %s",
                            field->field_name);
      }
      break;
    default:
      /*
        Columns will be dynamic unless explicitly specified FIXED
      */
      break;
  }

  return dynamic;
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

static int create_ndb_column(THD *thd, NDBCOL &col, Field *field,
                             HA_CREATE_INFO *create_info,
                             bool use_dynamic_as_default = false) {
  DBUG_TRACE;

  char buf[MAX_ATTR_DEFAULT_VALUE_SIZE];
  assert(field->stored_in_db);

  // Set name
  if (col.setName(field->field_name)) {
    // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
    return HA_ERR_OUT_OF_MEM;
  }

  // Get char set
  CHARSET_INFO *cs = const_cast<CHARSET_INFO *>(field->charset());
  // Set type and sizes
  const enum enum_field_types mysql_type = field->real_type();

  NDB_Modifiers column_modifiers(ndb_column_modifier_prefix,
                                 ndb_column_modifiers);
  if (column_modifiers.loadComment(field->comment.str, field->comment.length) ==
      -1) {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION, "%s",
                        column_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");

    return HA_WRONG_CREATE_OPTION;
  }

  const NDB_Modifier *mod_maxblob = column_modifiers.get("MAX_BLOB_PART_SIZE");

  const auto set_blob_inline_size =
      [&column_modifiers](THD *thd, NdbDictionary::Column &col, int size) {
        const NDB_Modifier *mod = column_modifiers.get("BLOB_INLINE_SIZE");

        if (mod->m_found) {
          char *end = nullptr;
          long mod_size = strtol(mod->m_val_str.str, &end, 10);

          if (mod_size > INT_MAX) mod_size = INT_MAX;

          if (*end != 0 || mod_size < 0) {
            if (thd) {
              get_thd_ndb(thd)->push_warning(
                  "Failed to parse BLOB_INLINE_SIZE=%s, "
                  "using default value %d",
                  mod->m_val_str.str, size);
            }
            mod_size = size;
          }
          col.setInlineSize(mod_size);
        } else {
          col.setInlineSize(size);
        }
      };

  {
    /* Clear default value (col obj is reused for whole table def) */
    col.setDefaultValue(nullptr, 0);

    if ((!field->is_flag_set(PRI_KEY_FLAG)) &&
        type_supports_default_value(mysql_type)) {
      if (!field->is_flag_set(NO_DEFAULT_VALUE_FLAG)) {
        ptrdiff_t src_offset = field->table->default_values_offset();
        if ((!field->is_real_null(src_offset)) ||
            field->is_flag_set(NOT_NULL_FLAG)) {
          /* Set a non-null native default */
          memset(buf, 0, MAX_ATTR_DEFAULT_VALUE_SIZE);
          get_default_value(buf, field);

          /* For bit columns, default length is rounded up to
             nearest word, ensuring all data sent
          */
          Uint32 defaultLen = field_used_length(field);
          if (field->type() == MYSQL_TYPE_BIT)
            defaultLen = ((defaultLen + 3) / 4) * 4;
          col.setDefaultValue(buf, defaultLen);
        }
      }
    }
  }
  switch (mysql_type) {
    // Numeric types
    case MYSQL_TYPE_TINY:
      if (field->is_flag_set(UNSIGNED_FLAG))
        col.setType(NDBCOL::Tinyunsigned);
      else
        col.setType(NDBCOL::Tinyint);
      col.setLength(1);
      break;
    case MYSQL_TYPE_SHORT:
      if (field->is_flag_set(UNSIGNED_FLAG))
        col.setType(NDBCOL::Smallunsigned);
      else
        col.setType(NDBCOL::Smallint);
      col.setLength(1);
      break;
    case MYSQL_TYPE_LONG:
      if (field->is_flag_set(UNSIGNED_FLAG))
        col.setType(NDBCOL::Unsigned);
      else
        col.setType(NDBCOL::Int);
      col.setLength(1);
      break;
    case MYSQL_TYPE_INT24:
      if (field->is_flag_set(UNSIGNED_FLAG))
        col.setType(NDBCOL::Mediumunsigned);
      else
        col.setType(NDBCOL::Mediumint);
      col.setLength(1);
      break;
    case MYSQL_TYPE_LONGLONG:
      if (field->is_flag_set(UNSIGNED_FLAG))
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
    case MYSQL_TYPE_DECIMAL: {
      Field_decimal *f = (Field_decimal *)field;
      uint precision = f->pack_length();
      uint scale = f->decimals();
      if (field->is_flag_set(UNSIGNED_FLAG)) {
        col.setType(NDBCOL::Olddecimalunsigned);
        precision -= (scale > 0);
      } else {
        col.setType(NDBCOL::Olddecimal);
        precision -= 1 + (scale > 0);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    } break;
    case MYSQL_TYPE_NEWDECIMAL: {
      Field_new_decimal *f = (Field_new_decimal *)field;
      uint precision = f->precision;
      uint scale = f->decimals();
      if (field->is_flag_set(UNSIGNED_FLAG)) {
        col.setType(NDBCOL::Decimalunsigned);
      } else {
        col.setType(NDBCOL::Decimal);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    } break;
    // Date types
    case MYSQL_TYPE_DATETIME:
      col.setType(NDBCOL::Datetime);
      col.setLength(1);
      break;
    case MYSQL_TYPE_DATETIME2: {
      Field_datetimef *f = (Field_datetimef *)field;
      uint prec = f->decimals();
      col.setType(NDBCOL::Datetime2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    case MYSQL_TYPE_DATE:  // ?
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
    case MYSQL_TYPE_TIME2: {
      Field_timef *f = (Field_timef *)field;
      uint prec = f->decimals();
      col.setType(NDBCOL::Time2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    case MYSQL_TYPE_YEAR:
      col.setType(NDBCOL::Year);
      col.setLength(1);
      break;
    case MYSQL_TYPE_TIMESTAMP:
      col.setType(NDBCOL::Timestamp);
      col.setLength(1);
      break;
    case MYSQL_TYPE_TIMESTAMP2: {
      Field_timestampf *f = (Field_timestampf *)field;
      uint prec = f->decimals();
      col.setType(NDBCOL::Timestamp2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    // Char types
    case MYSQL_TYPE_STRING:
      if (field->pack_length() == 0) {
        col.setType(NDBCOL::Bit);
        col.setLength(1);
      } else if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin) {
        col.setType(NDBCOL::Binary);
        col.setLength(field->pack_length());
      } else {
        col.setType(NDBCOL::Char);
        col.setCharset(cs);
        col.setLength(field->pack_length());
      }
      break;
    case MYSQL_TYPE_VAR_STRING:  // ?
    case MYSQL_TYPE_VARCHAR: {
      if (field->get_length_bytes() == 1) {
        if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Varbinary);
        else {
          col.setType(NDBCOL::Varchar);
          col.setCharset(cs);
        }
      } else if (field->get_length_bytes() == 2) {
        if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Longvarbinary);
        else {
          col.setType(NDBCOL::Longvarchar);
          col.setCharset(cs);
        }
      } else {
        return HA_ERR_UNSUPPORTED;
      }
      col.setLength(field->field_length);
    } break;
    // Blob types (all come in as MYSQL_TYPE_BLOB)
    mysql_type_tiny_blob:
    case MYSQL_TYPE_TINY_BLOB:
      if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
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
    // mysql_type_blob:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:
      if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
        col.setType(NDBCOL::Blob);
      else {
        col.setType(NDBCOL::Text);
        col.setCharset(cs);
      }
      {
        Field_blob *field_blob = (Field_blob *)field;
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
        else if (field_blob->max_data_length() < (1 << 16)) {
          set_blob_inline_size(thd, col, 256);
          col.setPartSize(2000);
          col.setStripeSize(0);
          if (mod_maxblob->m_found) {
            col.setPartSize(DEFAULT_MAX_BLOB_PART_SIZE);
          }
        } else if (field_blob->max_data_length() < (1 << 24))
          goto mysql_type_medium_blob;
        else
          goto mysql_type_long_blob;
      }
      break;
    mysql_type_medium_blob:
    case MYSQL_TYPE_MEDIUM_BLOB:
      if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
        col.setType(NDBCOL::Blob);
      else {
        col.setType(NDBCOL::Text);
        col.setCharset(cs);
      }
      set_blob_inline_size(thd, col, 256);
      col.setPartSize(4000);
      col.setStripeSize(0);
      if (mod_maxblob->m_found) {
        col.setPartSize(DEFAULT_MAX_BLOB_PART_SIZE);
      }
      break;
    mysql_type_long_blob:
    case MYSQL_TYPE_LONG_BLOB:
      if (field->is_flag_set(BINARY_FLAG) && cs == &my_charset_bin)
        col.setType(NDBCOL::Blob);
      else {
        col.setType(NDBCOL::Text);
        col.setCharset(cs);
      }
      set_blob_inline_size(thd, col, 256);
      col.setPartSize(DEFAULT_MAX_BLOB_PART_SIZE);
      col.setStripeSize(0);
      // The mod_maxblob modified has no effect here, already at max
      break;

    // MySQL 5.7 binary-encoded JSON type
    case MYSQL_TYPE_JSON: {
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
      set_blob_inline_size(thd, col, NDB_JSON_INLINE_SIZE);
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
    case MYSQL_TYPE_BIT: {
      int no_of_bits = field->field_length;
      col.setType(NDBCOL::Bit);
      if (!no_of_bits)
        col.setLength(1);
      else
        col.setLength(no_of_bits);
      break;
    }

    case MYSQL_TYPE_VECTOR:
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_UNSUPPORTED_EXTENSION,
          "VECTOR type is not supported by NDB in this MySQL version");
      return HA_ERR_UNSUPPORTED;

    case MYSQL_TYPE_NULL:
    default:
      return HA_ERR_UNSUPPORTED;
  }
  // Set nullable and pk
  col.setNullable(field->is_nullable());
  col.setPrimaryKey(field->is_flag_set(PRI_KEY_FLAG));
  if (field->is_flag_set(FIELD_IN_PART_FUNC_FLAG)) {
    col.setPartitionKey(true);
  }

  // Set autoincrement
  if (field->is_flag_set(AUTO_INCREMENT_FLAG)) {
    col.setAutoIncrement(true);
    ulonglong value = create_info->auto_increment_value
                          ? create_info->auto_increment_value
                          : (ulonglong)1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %llu", value));
    col.setAutoIncrementInitialValue(value);
  } else
    col.setAutoIncrement(false);

  // Storage type
  {
    NDBCOL::StorageType type = NDBCOL::StorageTypeMemory;
    switch (field->field_storage_type()) {
      case HA_SM_DEFAULT:
        DBUG_PRINT("info", ("No storage_type for field, check create_info"));
        if (create_info->storage_media == HA_SM_DISK) {
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
    const bool dynamic = ndb_column_is_dynamic(
        thd, field, create_info, use_dynamic_as_default, col.getStorageType());

    DBUG_PRINT("info", ("Using dynamic: %d", dynamic));
    col.setDynamic(dynamic);
  }

  return 0;
}

/**
  Define NDB column based on Ha_fk_column_type.

  @note This is simplified version of create_ndb_column() (with some
  added knowledge about how Field classes work) which produces NDB
  column objects which are only suitable for foreign key type
  compatibility checks. It only cares about type, length, precision,
  scale and charset and nothing else.
*/
static void create_ndb_fk_fake_column(NDBCOL &col,
                                      const Ha_fk_column_type &fk_col_type) {
  // Get character set.
  CHARSET_INFO *cs = const_cast<CHARSET_INFO *>(fk_col_type.field_charset);

  switch (fk_col_type.type) {
    // Numeric types
    case dd::enum_column_types::TINY:
      if (fk_col_type.is_unsigned)
        col.setType(NDBCOL::Tinyunsigned);
      else
        col.setType(NDBCOL::Tinyint);
      col.setLength(1);
      break;
    case dd::enum_column_types::SHORT:
      if (fk_col_type.is_unsigned)
        col.setType(NDBCOL::Smallunsigned);
      else
        col.setType(NDBCOL::Smallint);
      col.setLength(1);
      break;
    case dd::enum_column_types::LONG:
      if (fk_col_type.is_unsigned)
        col.setType(NDBCOL::Unsigned);
      else
        col.setType(NDBCOL::Int);
      col.setLength(1);
      break;
    case dd::enum_column_types::INT24:
      if (fk_col_type.is_unsigned)
        col.setType(NDBCOL::Mediumunsigned);
      else
        col.setType(NDBCOL::Mediumint);
      col.setLength(1);
      break;
    case dd::enum_column_types::LONGLONG:
      if (fk_col_type.is_unsigned)
        col.setType(NDBCOL::Bigunsigned);
      else
        col.setType(NDBCOL::Bigint);
      col.setLength(1);
      break;
    case dd::enum_column_types::FLOAT:
      col.setType(NDBCOL::Float);
      col.setLength(1);
      break;
    case dd::enum_column_types::DOUBLE:
      col.setType(NDBCOL::Double);
      col.setLength(1);
      break;
    case dd::enum_column_types::DECIMAL: {
      uint precision = fk_col_type.char_length;
      uint scale = fk_col_type.numeric_scale;
      if (fk_col_type.is_unsigned) {
        col.setType(NDBCOL::Olddecimalunsigned);
        precision -= (scale > 0);
      } else {
        col.setType(NDBCOL::Olddecimal);
        precision -= 1 + (scale > 0);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    } break;
    case dd::enum_column_types::NEWDECIMAL: {
      uint precision = my_decimal_length_to_precision(fk_col_type.char_length,
                                                      fk_col_type.numeric_scale,
                                                      fk_col_type.is_unsigned);
      uint scale = fk_col_type.numeric_scale;
      if (fk_col_type.is_unsigned) {
        col.setType(NDBCOL::Decimalunsigned);
      } else {
        col.setType(NDBCOL::Decimal);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    } break;
    // Date types
    case dd::enum_column_types::DATETIME:
      col.setType(NDBCOL::Datetime);
      col.setLength(1);
      break;
    case dd::enum_column_types::DATETIME2: {
      uint prec = (fk_col_type.char_length > MAX_DATETIME_WIDTH)
                      ? fk_col_type.char_length - 1 - MAX_DATETIME_WIDTH
                      : 0;
      col.setType(NDBCOL::Datetime2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    case dd::enum_column_types::NEWDATE:
      col.setType(NDBCOL::Date);
      col.setLength(1);
      break;
    case dd::enum_column_types::TIME:
      col.setType(NDBCOL::Time);
      col.setLength(1);
      break;
    case dd::enum_column_types::TIME2: {
      uint prec = (fk_col_type.char_length > MAX_TIME_WIDTH)
                      ? fk_col_type.char_length - 1 - MAX_TIME_WIDTH
                      : 0;
      col.setType(NDBCOL::Time2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    case dd::enum_column_types::YEAR:
      col.setType(NDBCOL::Year);
      col.setLength(1);
      break;
    case dd::enum_column_types::TIMESTAMP:
      col.setType(NDBCOL::Timestamp);
      col.setLength(1);
      break;
    case dd::enum_column_types::TIMESTAMP2: {
      uint prec = (fk_col_type.char_length > MAX_DATETIME_WIDTH)
                      ? fk_col_type.char_length - 1 - MAX_DATETIME_WIDTH
                      : 0;
      col.setType(NDBCOL::Timestamp2);
      col.setLength(1);
      col.setPrecision(prec);
    } break;
    // Char types
    case dd::enum_column_types::STRING:
      if (fk_col_type.char_length == 0) {
        col.setType(NDBCOL::Bit);
        col.setLength(1);
      } else if (cs == &my_charset_bin) {
        col.setType(NDBCOL::Binary);
        col.setLength(fk_col_type.char_length);
      } else {
        col.setType(NDBCOL::Char);
        col.setCharset(cs);
        col.setLength(fk_col_type.char_length);
      }
      break;
    case dd::enum_column_types::VARCHAR: {
      uint length_bytes = HA_VARCHAR_PACKLENGTH(fk_col_type.char_length);
      if (length_bytes == 1) {
        if (cs == &my_charset_bin)
          col.setType(NDBCOL::Varbinary);
        else {
          col.setType(NDBCOL::Varchar);
          col.setCharset(cs);
        }
      } else if (length_bytes == 2) {
        if (cs == &my_charset_bin)
          col.setType(NDBCOL::Longvarbinary);
        else {
          col.setType(NDBCOL::Longvarchar);
          col.setCharset(cs);
        }
      } else {
        /*
          This branch is dead at the moment and has been left for consistency
          with create_ndb_column(). Instead of returning an error we cheat and
          use Blob type which is not supported in foreign keys.
        */
        col.setType(NDBCOL::Blob);
      }
      col.setLength(fk_col_type.char_length);
    } break;
    // Blob types
    case dd::enum_column_types::TINY_BLOB:
    case dd::enum_column_types::BLOB:
    case dd::enum_column_types::VECTOR:
    case dd::enum_column_types::MEDIUM_BLOB:
    case dd::enum_column_types::LONG_BLOB:
    case dd::enum_column_types::GEOMETRY:
    case dd::enum_column_types::JSON:
      /*
        Since NDB doesn't support foreign keys over Blob and Text columns
        anyway, we cheat and always use Blob type in this case without
        calculating exact type and other attributes.
      */
      col.setType(NDBCOL::Blob);
      break;
    // Other types
    case dd::enum_column_types::ENUM:
      col.setType(NDBCOL::Char);
      col.setLength(get_enum_pack_length(fk_col_type.elements_count));
      break;
    case dd::enum_column_types::SET:
      col.setType(NDBCOL::Char);
      col.setLength(get_set_pack_length(fk_col_type.elements_count));
      break;
    case dd::enum_column_types::BIT: {
      int no_of_bits = fk_col_type.char_length;
      col.setType(NDBCOL::Bit);
      if (!no_of_bits)
        col.setLength(1);
      else
        col.setLength(no_of_bits);
      break;
    }
    // Legacy types. Modern server is not supposed to use them.
    case dd::enum_column_types::DATE:
    case dd::enum_column_types::VAR_STRING:
    // Unsupported types.
    case dd::enum_column_types::TYPE_NULL:
    default:
      /*
        Instead of returning an error we cheat and use Blob type
        which is not supported in foreign keys.
      */
      col.setType(NDBCOL::Blob);
      break;
  }
}

static const NdbDictionary::Object::PartitionBalance
    g_default_partition_balance =
        NdbDictionary::Object::PartitionBalance_ForRPByLDM;

void ha_ndbcluster::update_create_info(HA_CREATE_INFO *create_info) {
  DBUG_TRACE;
  THD *thd = current_thd;
  Ndb *ndb = check_ndb_in_thd(thd);

  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    /*
      Find any initial auto_increment value
    */
    for (uint i = 0; i < table->s->fields; i++) {
      Field *field = table->field[i];
      if (field->is_flag_set(AUTO_INCREMENT_FLAG)) {
        ulonglong auto_value;
        uint retries = NDB_AUTO_INCREMENT_RETRIES;
        for (;;) {
          NDB_SHARE::Tuple_id_range_guard g(m_share);
          if (ndb->readAutoIncrementValue(m_table, g.range, auto_value)) {
            if (--retries && !thd_killed(thd) &&
                ndb->getNdbError().status == NdbError::TemporaryError) {
              ndb_trans_retry_sleep();
              continue;
            }
            const NdbError err = ndb->getNdbError();
            ndb_log_error("Error %d in ::update_create_info(): %s", err.code,
                          err.message);
            return;
          }
          break;
        }
        if (auto_value > 1) {
          create_info->auto_increment_value = auto_value;
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
  if (thd->lex->sql_command == SQLCOM_ALTER_TABLE) {
    update_comment_info(thd, create_info, m_table);
  }
}

enum COMMENT_ITEMS {
  NOLOGGING = 0,
  READ_BACKUP = 1,
  FULLY_REPLICATED = 2,
  PARTITION_BALANCE = 3
};

/**
 * @brief  Set comment_items_shown for the comment items found in the
 * comment_str
 */
int ha_ndbcluster::get_old_table_comment_items(THD *thd,
                                               bool *comment_items_shown,
                                               char *comment_str,
                                               unsigned comment_len) {
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix, ndb_table_modifiers);

  if (table_modifiers.loadComment(comment_str, comment_len) == -1) {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION, "%s",
                        table_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");
    return -1;
  }
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier *mod_fully_replicated =
      table_modifiers.get("FULLY_REPLICATED");
  const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");

  if (mod_nologging->m_found) comment_items_shown[NOLOGGING] = true;
  if (mod_read_backup->m_found) comment_items_shown[READ_BACKUP] = true;
  if (mod_fully_replicated->m_found)
    comment_items_shown[FULLY_REPLICATED] = true;
  if (mod_frags->m_found) comment_items_shown[PARTITION_BALANCE] = true;
  return 0;
}

/**
 * @brief  Supplement create_info's comment item with the other
 * comment items present in the old table, which are specified
 * in create table or earlier alter table commands.
 */
void ha_ndbcluster::update_comment_info(THD *thd, HA_CREATE_INFO *create_info,
                                        const NdbDictionary::Table *ndbtab) {
  DBUG_TRACE;
  assert(thd->lex->sql_command == SQLCOM_ALTER_TABLE);
  DBUG_PRINT("info", ("update_comment_info: Before: table comment str %s",
                      table->s->comment.str));
  assert(create_info != nullptr);
  if (create_info->comment.str == nullptr) {
    DBUG_PRINT("info", ("create_info->comment.str is null, "
                        "command %u, returning",
                        thd->lex->sql_command));
    return;
  }

  DBUG_PRINT("info",
             ("Before: creinf comment str %s", create_info->comment.str));

  if (table->s->comment.str != nullptr) {
    std::string cre_inf_str = create_info->comment.str;
    if (cre_inf_str.compare(table->s->comment.str) == 0) {
      DBUG_PRINT("info", ("Comment from create_info and table->s are equal, "
                          "command %u, returning",
                          thd->lex->sql_command));
      return;
    }
  }

  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix, ndb_table_modifiers);
  char *comment_str = create_info->comment.str;
  unsigned comment_len = create_info->comment.length;

  if (table_modifiers.loadComment(comment_str, comment_len) == -1) {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_ILLEGAL_HA_CREATE_OPTION, "%s",
                        table_modifiers.getErrMsg());
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), ndbcluster_hton_name,
             "Syntax error in COMMENT modifier");
    return;
  }
  // Get the comment items from create_info
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier *mod_fully_replicated =
      table_modifiers.get("FULLY_REPLICATED");
  const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");

  // Get the comment items from the old Ndb table
  bool old_nologging = !ndbtab->getLogging();
  bool old_read_backup = ndbtab->getReadBackupFlag();
  bool old_fully_replicated = ndbtab->getFullyReplicated();
  NdbDictionary::Object::PartitionBalance old_part_bal =
      ndbtab->getPartitionBalance();

  // Merge any previous comment changes from the old table from share
  // into the current changes specified in create_info
  bool old_table_comment[4] = {false, false, false, false};
  if (get_old_table_comment_items(thd, old_table_comment, table->s->comment.str,
                                  table->s->comment.length)) {
    return;
  }

  bool new_fully_replicated =
      (mod_fully_replicated->m_found && mod_fully_replicated->m_val_bool);
  bool new_read_backup =
      (mod_read_backup->m_found && mod_read_backup->m_val_bool);

  // Adding fully_replicated without having and adding read_backup,
  // and read_backup is specified in the old comment (thus it cannot
  // be changed to true implicitly)
  if (new_fully_replicated && !old_read_backup && !new_read_backup &&
      old_table_comment[READ_BACKUP]) {
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
             "READ_BACKUP=0 cannot be used for fully replicated tables",
             "either 'set global ndb_read_backup=1;' or "
             "add READ_BACKUP=1 to comment");
    return;
  }

  // Cannot alter read_backup to 0 when fully_replicated was on
  // and is not altered to 0 in this alter
  if (old_fully_replicated && !mod_fully_replicated->m_found &&
      mod_read_backup->m_found && !mod_read_backup->m_val_bool) {
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
             "cannot change READ_BACKUP to 0 for fully replicated tables",
             "fully_replicated=0 if that is the intention");
  }

  /**
   * We start by calculating how much more space we need in the comment
   * string.
   */
  bool add_nologging = false;
  if (!mod_nologging->m_found) {
    if (old_table_comment[NOLOGGING]) {
      // Not specified in current alter command nor specified in old table
      add_nologging = true;
      table_modifiers.set("NOLOGGING", old_nologging);
      DBUG_PRINT("info", ("added nologging"));
    } else if (old_nologging != THDVAR(thd, table_no_logging)) {
      char msg1[128];
      snprintf(msg1, sizeof(msg1),
               "Alter will use the default value for NOLOGGING (=%u) "
               "which is different from the table's current value",
               THDVAR(thd, table_no_logging));
      char msg2[128];
      snprintf(msg2, sizeof(msg2),
               "either 'set table_no_logging=%u;' or "
               "add NOLOGGING=%u to comment",
               old_nologging, old_nologging);
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
               msg1, msg2);
      return;
    }
  }

  bool add_fully_replicated = false;
  if (!mod_fully_replicated->m_found) {
    if (old_table_comment[FULLY_REPLICATED]) {
      // Not specified in current command nor specified in old table
      add_fully_replicated = true;
      table_modifiers.set("FULLY_REPLICATED", old_fully_replicated);
      DBUG_PRINT("info", ("added fully_replicated"));
    } else if (old_fully_replicated != opt_ndb_fully_replicated) {
      char msg1[128];
      snprintf(msg1, sizeof(msg1),
               "Alter will use the default value for FULLY_REPLICATED (=%u) "
               "which is different from the table's current value",
               opt_ndb_fully_replicated);
      char msg2[128];
      snprintf(msg2, sizeof(msg2),
               "either 'set global ndb_fully_replicated=%u;' or "
               "add FULLY_REPLICATED=%u to comment",
               old_fully_replicated, old_fully_replicated);

      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
               msg1, msg2);
      return;
    }
  }

  bool add_read_backup = false;
  if (!mod_read_backup->m_found) {
    if (old_table_comment[READ_BACKUP]) {
      // Not specified in current command nor specified in old table
      add_read_backup = true;
      table_modifiers.set("READ_BACKUP", old_read_backup);
      DBUG_PRINT("info", ("added read_backup"));
    } else if (old_read_backup != opt_ndb_read_backup) {
      char msg1[128];
      snprintf(msg1, sizeof(msg1),
               "Alter will use the default value for READ_BACKUP (=%u) "
               "which is different from the table's current value",
               opt_ndb_read_backup);
      char msg2[128];
      snprintf(msg2, sizeof(msg2),
               "either 'set global ndb_read_backup=%u;' or "
               "add READ_BACKUP=%u to comment",
               old_read_backup, old_read_backup);
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
               msg1, msg2);
      return;
    }
  }

  bool add_part_bal = false;
  const char *old_part_bal_str =
      NdbDictionary::Table::getPartitionBalanceString(old_part_bal);
  if (!mod_frags->m_found) {
    if (old_table_comment[PARTITION_BALANCE]) {
      // Not specified in current command nor specified in old table
      add_part_bal = true;
      table_modifiers.set("PARTITION_BALANCE", old_part_bal_str);
      DBUG_PRINT("info", ("added part_bal_str"));
    } else if (old_part_bal != g_default_partition_balance) {
      const char *default_part_bal_str =
          NdbDictionary::Table::getPartitionBalanceString(
              g_default_partition_balance);
      char msg1[128];
      snprintf(msg1, sizeof(msg1),
               "Alter will use the default value for PARTITION_BALANCE (=%s) "
               "which is different from the table's current value",
               default_part_bal_str);
      char msg2[128];
      snprintf(msg2, sizeof(msg2), "Add PARTITION_BALANCE=%s to comment",
               old_part_bal_str);

      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "Alter table",
               msg1, msg2);
      return;
    }
  }

  if (!(add_nologging || add_read_backup || add_fully_replicated ||
        add_part_bal)) {
    /* No change of comment is needed. */
    return;
  }

  /**
   * All necessary modifiers are set, now regenerate the comment
   */
  const char *updated_str = table_modifiers.generateCommentString();
  if (updated_str == nullptr) {
    mem_alloc_error(0);
    return;
  }
  const Uint32 new_len = strlen(updated_str);
  // Allocate comment memory from TABLE_SHARE's MEM_ROOT
  char *const new_str = (char *)table->s->mem_root.Alloc((size_t)new_len);
  if (new_str == nullptr) {
    mem_alloc_error(0);
    return;
  }
  memcpy(new_str, updated_str, new_len);
  DBUG_PRINT("info", ("new_str: %s", new_str));

  /* Update structures */
  create_info->comment.str = new_str;
  create_info->comment.length = new_len;
  DBUG_PRINT("info", ("After: comment_len: %u, comment: %s", new_len, new_str));
}

static uint get_no_fragments(ulonglong max_rows) {
  ulonglong acc_row_size = 25 + /*safety margin*/ 2;
  ulonglong acc_fragment_size = 512 * 1024 * 1024;
  return uint((max_rows * acc_row_size) / acc_fragment_size) + 1;
}

/*
  Routine to adjust default number of partitions to always be a multiple
  of number of nodes and never more than 4 times the number of nodes.

*/
static bool adjusted_frag_count(Ndb *ndb, uint requested_frags,
                                uint &reported_frags) {
  unsigned no_nodes = g_ndb_cluster_connection->no_db_nodes();
  unsigned no_replicas = no_nodes == 1 ? 1 : 2;

  unsigned no_threads = 1;
  const unsigned no_nodegroups = g_ndb_cluster_connection->max_nodegroup() + 1;

  {
    /**
     * Use SYSTAB_0 to get #replicas, and to guess #threads
     */
    Ndb_table_guard ndbtab_g(ndb, "sys", "SYSTAB_0");
    const NdbDictionary::Table *tab = ndbtab_g.get_table();
    if (tab) {
      no_replicas = tab->getReplicaCount();

      /**
       * Guess #threads
       */
      {
        const Uint32 frags = tab->getFragmentCount();
        Uint32 node = 0;
        Uint32 cnt = 0;
        for (Uint32 i = 0; i < frags; i++) {
          Uint32 replicas[4];
          if (tab->getFragmentNodes(i, replicas, NDB_ARRAY_SIZE(replicas))) {
            if (node == replicas[0] || node == 0) {
              node = replicas[0];
              cnt++;
            }
          }
        }
        no_threads = cnt;  // No of primary replica on 1-node
      }
    }
  }

  const unsigned usable_nodes = no_replicas * no_nodegroups;
  const uint max_replicas = 8 * usable_nodes * no_threads;

  reported_frags = usable_nodes * no_threads;  // Start with 1 frag per threads
  Uint32 replicas = reported_frags * no_replicas;

  /**
   * Loop until requested replicas, and not exceed max-replicas
   */
  while (reported_frags < requested_frags &&
         (replicas + usable_nodes * no_threads * no_replicas) <= max_replicas) {
    reported_frags += usable_nodes * no_threads;
    replicas += usable_nodes * no_threads * no_replicas;
  }

  return (reported_frags < requested_frags);
}

static bool parsePartitionBalance(
    THD *thd, const NDB_Modifier *mod,
    NdbDictionary::Object::PartitionBalance *part_bal) {
  if (mod->m_found == false) return false;  // OK

  NdbDictionary::Object::PartitionBalance ret =
      NdbDictionary::Table::getPartitionBalance(mod->m_val_str.str);

  if (ret == 0) {
    DBUG_PRINT("info",
               ("PartitionBalance: %s not supported", mod->m_val_str.str));
    /**
     * Comment section contains a partition balance we cannot
     * recognize, we will print warning about this and will
     * not change the comment string.
     */
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG), 4500,
                        "Comment contains non-supported fragment"
                        " count type",
                        "NDB");
    return false;
  }

  if (part_bal) {
    *part_bal = ret;
  }
  return true;
}

/*
  Forward declaration of the utility functions used
  when creating partitioned tables
*/
static int create_table_set_up_partition_info(partition_info *part_info,
                                              NdbDictionary::Table &,
                                              Ndb_table_map &);

/**
  @brief Check that any table modifiers specified in the table COMMENT= matches
  the NDB table properties.

  @note Traditionally this function was used to augment the create info of a
  table but has since been superseded by implementation of the new data
  dictionary.

  @note The values for PARTITION_BALANCE and FULLY_REPLICATED are only
  checked when READ_BACKUP is set
*/
void ha_ndbcluster::append_create_info(String *) {
  if (DBUG_EVALUATE_IF("ndb_append_create_info_unsync", true, false)) {
    // Trigger all warnings by using non default modifier values
    const char *unsync_props =
        "NDB_TABLE=NOLOGGING=1,READ_BACKUP=0,"
        "PARTITION_BALANCE=FOR_RA_BY_LDM_X_3,FULLY_REPLICATED=1";
    table_share->comment.str =
        strdup_root(&table_share->mem_root, unsync_props);
    table_share->comment.length = strlen(unsync_props);
  }

  if (DBUG_EVALUATE_IF("ndb_append_create_info_unparse", true, false)) {
    // Test failure to load comment by using unparsable comment,
    const char *unparse_props = "NDB_TABLE=UNPARSABLE=1";
    table_share->comment.str =
        strdup_root(&table_share->mem_root, unparse_props);
    table_share->comment.length = strlen(unparse_props);
  }

  if (table_share->comment.length == 0) {
    return;
  }

  THD *thd = current_thd;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  // Load table definition from NDB
  Ndb_table_guard ndbtab_g(thd_ndb->ndb, table_share->db.str,
                           table_share->table_name.str);
  const NdbDictionary::Table *tab = ndbtab_g.get_table();
  if (!tab) {
    // Could not load table from NDB, push error and skip further checks
    thd_ndb->push_ndb_error_warning(ndbtab_g.getNdbError());
    return;
  }

  /* Parse the current comment string */
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix, ndb_table_modifiers);
  if (table_modifiers.loadComment(table_share->comment.str,
                                  table_share->comment.length) == -1) {
    thd_ndb->push_warning(ER_ILLEGAL_HA_CREATE_OPTION, "%s",
                          table_modifiers.getErrMsg());
    return;
  }

  /*
     Check for differences between COMMENT= and NDB table properties
  */
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  if (mod_nologging->m_found) {
    const bool comment_logged_table = !mod_nologging->m_val_bool;
    if (tab->getLogging() != comment_logged_table) {
      thd_ndb->push_warning(4502,
                            "Table property is not the same as in comment for "
                            "NOLOGGING property");
    }
  }

  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  if (mod_read_backup->m_found) {
    const bool comment_read_backup = mod_read_backup->m_val_bool;
    if (tab->getReadBackupFlag() != comment_read_backup) {
      thd_ndb->push_warning(4502,
                            "Table property is not the same as in comment for "
                            "READ_BACKUP property");
    }

    const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");
    if (mod_frags->m_found) {
      NdbDictionary::Object::PartitionBalance comment_part_bal =
          g_default_partition_balance;
      if (parsePartitionBalance(thd, mod_frags, &comment_part_bal)) {
        if (tab->getPartitionBalance() != comment_part_bal) {
          thd_ndb->push_warning(4501,
                                "Table property is not the same as in comment "
                                "for PARTITION_BALANCE property");
        }
      }
    }

    const NDB_Modifier *mod_fully_replicated =
        table_modifiers.get("FULLY_REPLICATED");
    if (mod_fully_replicated->m_found) {
      const bool comment_fully_replicated = mod_fully_replicated->m_val_bool;
      if (tab->getFullyReplicated() != comment_fully_replicated) {
        thd_ndb->push_warning(
            4502,
            "Table property is not the same as in comment for "
            "FULLY_REPLICATED property");
      }
    }
  }
}

static bool drop_table_and_related(THD *thd, Ndb *ndb,
                                   NdbDictionary::Dictionary *dict,
                                   const char *dbname,
                                   const NdbDictionary::Table *table,
                                   int drop_flags, bool skip_related);

/**
  @brief Create a table in NDB
  @param path                  Path for table (in filesystem encoded charset).
  @param table_arg             Pointer to TABLE object describing the table to
                               be created, the exact same information is
                               available as handler::table which is used
                               throughout this function.
  @param create_info           HA_CREATE_INFO describing table.
  @param table_def             dd::Table object describing the table
                               to be created. This object can be
                               updated and will be persisted in the
                               data dictionary when function returns
  @retval 0 Success
  @retval != 0 Error

  @note Warnings and error should be reported before returning from this
        function. When this is done properly the error code HA_ERR_GENERIC
        can be used to avoid that ha_ndbcluster::print_error() reports
        another error.
*/
int ha_ndbcluster::create(const char *path [[maybe_unused]],
                          TABLE *table_arg [[maybe_unused]],
                          HA_CREATE_INFO *create_info, dd::Table *table_def) {
  THD *thd = current_thd;
  NDBTAB tab;
  uint pk_length = 0;
  bool use_disk = false;
  Ndb_fk_list fk_list_for_truncate;

  // Verify default value for "single user mode" of the table
  assert(tab.getSingleUserMode() == NdbDictionary::Table::SingleUserModeLocked);

  DBUG_TRACE;

  /* Create a map from stored field number to column number */
  Ndb_table_map table_map(table);

  /*
    CREATE TEMPORARY TABLE is not supported in NDB since there is no
    guarantee that the table "is visible only to the current session,
    and is dropped automatically when the session is closed". Support
    for temporary tables is turned off with HTON_TEMPORARY_NOT_SUPPORTED so
    crash in debug if mysqld tries to create such a table anyway.
  */
  assert(!(create_info->options & HA_LEX_CREATE_TMP_TABLE));

  const char *dbname = table_share->db.str;
  const char *tabname = table_share->table_name.str;

  ndb_log_info("Creating table '%s.%s'", dbname, tabname);

  Ndb_schema_dist_client schema_dist_client(thd);

  if (check_ndb_connection(thd)) return HA_ERR_NO_CONNECTION;

  Ndb_create_helper create(thd, tabname);
  Ndb *ndb = get_thd_ndb(thd)->ndb;
  NDBDICT *dict = ndb->getDictionary();

  if (create_info->table_options & HA_OPTION_CREATE_FROM_ENGINE) {
    // This is the final step of table discovery, the table already exists
    // in NDB and it has already been added to local DD by
    // calling ha_discover() and thus ndbcluster_discover()
    // Just finish this process by setting up the binlog for this table
    const int setup_result =
        ndbcluster_binlog_setup_table(thd, ndb, dbname, tabname, table_def);
    if (setup_result != 0) {
      if (setup_result == HA_ERR_TABLE_EXIST) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_TABLE_EXISTS_ERROR,
            "Failed to setup replication of table %s.%s", dbname, tabname);
      }
      return create.failed_warning_already_pushed();
    }
    return 0;
  }

  /*
    Check if the create table is part of a copying alter table.
    Note, this has to be done after the check for auto-discovering
    tables since a table being altered might not be known to the
    mysqld issuing the alter statement.
   */
  if (thd_sql_command(thd) == SQLCOM_ALTER_TABLE) {
    DBUG_PRINT("info", ("Detected copying ALTER TABLE"));

    // Check that the table name is a temporary name
    assert(ndb_name_is_temp(tabname));

    if (!is_copying_alter_table_allowed(thd)) {
      DBUG_PRINT("info", ("Refusing implicit copying alter table"));
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "Implicit copying alter", "ndb_allow_copying_alter_table=0",
               "ALGORITHM=COPY to force the alter");
      return HA_WRONG_CREATE_OPTION;
    }

    /*
      Renaming a table and at the same time doing some other change
      is currently not supported, see Bug #16021021 ALTER ... RENAME
      FAILS TO RENAME ON PARTICIPANT MYSQLD.

      Refuse such ALTER TABLE .. RENAME already when trying to
      create the destination table.
    */
    const uint flags = thd->lex->alter_info->flags;
    if (flags & Alter_info::ALTER_RENAME && flags & ~Alter_info::ALTER_RENAME) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), thd->query().str);
      return ER_NOT_SUPPORTED_YET;
    }
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  if (!(thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT) ||
        thd_ndb->has_required_global_schema_lock("ha_ndbcluster::create"))) {
    return HA_ERR_NO_CONNECTION;
  }

  // Set database name to use while creating the table in NDB
  const Ndb_dbname_guard dbname_guard(ndb, dbname);
  if (dbname_guard.change_database_failed()) {
    return create.failed_in_NDB(ndb->getNdbError());
  }

  if (thd_ndb->check_option(Thd_ndb::CREATE_UTIL_TABLE)) {
    // Creating ndbcluster util table. This is done in order to install the
    // table definition in DD using SQL. Apply special settings for the table
    // and return
    DBUG_PRINT("info", ("Creating ndbcluster util table"));

    if (thd_ndb->check_option(Thd_ndb::CREATE_UTIL_TABLE_HIDDEN)) {
      // Mark the util table as hidden in DD
      ndb_dd_table_mark_as_hidden(table_def);
    }

    // Table already created in NDB, check that it exist and fail create
    // otherwise
    Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
    if (!ndbtab_g.get_table()) {
      return create.failed_internal_error(
          "The util table does not already exist in NDB");
    }

    // Update table definition with the table id and version of the NDB table
    const NdbDictionary::Table *const ndbtab = ndbtab_g.get_table();
    const Ndb_dd_handle dd_handle{ndbtab->getObjectId(),
                                  ndbtab->getObjectVersion()};
    ndb_dd_table_set_spi_and_version(table_def, dd_handle);

    return create.succeeded();
  }

  if (ndb_name_is_temp(tabname)) {
    // Creating table with temporary name, table will only be access by this
    // MySQL Server -> skip schema distribution
    DBUG_PRINT("info", ("Creating table with temporary name"));

    ndbcluster::ndbrequire(is_prefix(tabname, "#sql2") == 0);

    // Checking if there is no table with given temporary name in NDB
    // If such a table exists, it will be dropped to allow name reuse
    Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
    const NDBTAB *ndbtab = ndbtab_g.get_table();
    constexpr int flag = NdbDictionary::Dictionary::DropTableCascadeConstraints;

    if (ndbtab != nullptr) {
      thd_ndb->push_warning(
          "The temporary named table %s.%s already exists, it will be removed",
          tabname, dbname);
      if (ndb->getDictionary()->dropTableGlobal(*ndbtab, flag) != 0) {
        thd_ndb->push_warning(
            "Attempt to drop temporary named table %s.%s failed", dbname,
            tabname);
        return create.failed_in_NDB(ndb->getDictionary()->getNdbError());
      }
    }

  } else {
    // Prepare schema distribution
    if (!schema_dist_client.prepare(dbname, tabname)) {
      // Failed to prepare schema distributions
      DBUG_PRINT("info", ("Schema distribution failed to initialize"));
      return HA_ERR_NO_CONNECTION;
    }

    std::string invalid_identifier;
    if (!schema_dist_client.check_identifier_limits(invalid_identifier)) {
      // Check of db or table name limits failed
      my_error(ER_TOO_LONG_IDENT, MYF(0), invalid_identifier.c_str());
      return HA_WRONG_CREATE_OPTION;
    }
  }

  if (thd_sql_command(thd) == SQLCOM_TRUNCATE) {
    Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
    if (!ndbtab_g.get_table()) {
      ERR_RETURN(ndbtab_g.getNdbError());
    }

    /* save the foreign key information in fk_list */
    if (!retrieve_foreign_key_list_from_ndb(dict, ndbtab_g.get_table(),
                                            &fk_list_for_truncate)) {
      ERR_RETURN(dict->getNdbError());
    }

    DBUG_PRINT("info", ("Dropping and re-creating table for TRUNCATE"));
    const int drop_result = drop_table_impl(
        thd, thd_ndb->ndb, &schema_dist_client, dbname, tabname);
    if (drop_result) {
      return drop_result;
    }
  }

  DBUG_PRINT("info", ("Start parse of table modifiers, comment = %s",
                      create_info->comment.str));
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix, ndb_table_modifiers);
  if (table_modifiers.loadComment(create_info->comment.str,
                                  create_info->comment.length) == -1) {
    thd_ndb->push_warning(ER_ILLEGAL_HA_CREATE_OPTION, "%s",
                          table_modifiers.getErrMsg());
    return create.failed_illegal_create_option(
        "Syntax error in COMMENT modifier");
  }
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");
  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier *mod_fully_replicated =
      table_modifiers.get("FULLY_REPLICATED");
  NdbDictionary::Object::PartitionBalance part_bal =
      g_default_partition_balance;
  if (parsePartitionBalance(thd, mod_frags, &part_bal) == false) {
    /**
     * unable to parse => modifier which is not found
     */
    mod_frags = table_modifiers.notfound();
  } else if (ndbd_support_partition_balance(ndb->getMinDbNodeVersion()) == 0) {
    return create.failed_illegal_create_option(
        "PARTITION_BALANCE not supported by current data node versions");
  }

  /* Verify we can support read backup table property if set */
  if ((mod_read_backup->m_found || opt_ndb_read_backup) &&
      ndbd_support_read_backup(ndb->getMinDbNodeVersion()) == 0) {
    return create.failed_illegal_create_option(
        "READ_BACKUP not supported by current data node versions");
  }

  /*
    ROW_FORMAT=[DEFAULT|FIXED|DYNAMIC|COMPRESSED|REDUNDANT|COMPACT]

    Only DEFAULT, FIXED or DYNAMIC supported
  */
  if (!(create_info->row_type == ROW_TYPE_DEFAULT ||
        create_info->row_type == ROW_TYPE_FIXED ||
        create_info->row_type == ROW_TYPE_DYNAMIC)) {
    /* Unsupported row format requested */
    std::string err_message;
    err_message.append("ROW_FORMAT=");
    switch (create_info->row_type) {
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
        assert(false);
        break;
    }
    return create.failed_illegal_create_option(err_message.c_str());
  }

  /* Verify we can support fully replicated table property if set */
  if ((mod_fully_replicated->m_found || opt_ndb_fully_replicated) &&
      ndbd_support_fully_replicated(ndb->getMinDbNodeVersion()) == 0) {
    return create.failed_illegal_create_option(
        "FULLY_REPLICATED not supported by current data node versions");
  }

  // Read mysql.ndb_replication settings for this table, if any
  uint32 binlog_flags;
  const st_conflict_fn_def *conflict_fn = nullptr;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  uint num_args = MAX_CONFLICT_ARGS;

  Ndb_binlog_client binlog_client(thd, dbname, tabname);
  if (binlog_client.read_replication_info(ndb, dbname, tabname, ::server_id,
                                          &binlog_flags, &conflict_fn, args,
                                          &num_args)) {
    return HA_WRONG_CREATE_OPTION;
  }

  // Use mysql.ndb_replication settings when creating table
  if (conflict_fn != nullptr) {
    switch (conflict_fn->type) {
      case CFT_NDB_EPOCH:
      case CFT_NDB_EPOCH_TRANS:
      case CFT_NDB_EPOCH2:
      case CFT_NDB_EPOCH2_TRANS: {
        /* Default 6 extra Gci bits allows 2^6 == 64
         * epochs / saveGCP, a comfortable default
         */
        Uint32 numExtraGciBits = 6;
        Uint32 numExtraAuthorBits = 1;

        if ((num_args == 1) && (args[0].type == CFAT_EXTRA_GCI_BITS)) {
          numExtraGciBits = args[0].extraGciBits;
        }
        DBUG_PRINT("info", ("Setting ExtraRowGciBits to %u, "
                            "ExtraAuthorBits to %u",
                            numExtraGciBits, numExtraAuthorBits));

        tab.setExtraRowGciBits(numExtraGciBits);
        tab.setExtraRowAuthorBits(numExtraAuthorBits);
      }
      default:
        break;
    }
  }

  Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
  if (!schema_trans.begin_trans()) {
    return create.failed_warning_already_pushed();
  }

  if (tab.setName(tabname)) {
    return create.failed_oom("Failed to set table name");
  }

  if (THDVAR(thd, table_temporary)) {
#ifdef DOES_NOT_WORK_CURRENTLY
    tab.setTemporary(true);
#endif
    DBUG_PRINT("info", ("table_temporary set"));
    tab.setLogging(false);
  } else if (THDVAR(thd, table_no_logging)) {
    DBUG_PRINT("info", ("table_no_logging set"));
    tab.setLogging(false);
  }
  if (mod_nologging->m_found) {
    DBUG_PRINT("info", ("tab.setLogging(%u)", (!mod_nologging->m_val_bool)));
    tab.setLogging(!mod_nologging->m_val_bool);
  }

  {
    bool use_fully_replicated;
    bool use_read_backup;

    if (mod_fully_replicated->m_found) {
      use_fully_replicated = mod_fully_replicated->m_val_bool;
    } else {
      use_fully_replicated = opt_ndb_fully_replicated;
    }

    if (mod_read_backup->m_found) {
      use_read_backup = mod_read_backup->m_val_bool;
    } else if (use_fully_replicated) {
      use_read_backup = true;
    } else {
      use_read_backup = opt_ndb_read_backup;
    }

    if (use_fully_replicated) {
      /* Fully replicated table */
      if (mod_read_backup->m_found && !mod_read_backup->m_val_bool) {
        /**
         * Cannot mix FULLY_REPLICATED=1 and READ_BACKUP=0 since
         * FULLY_REPLICATED=1 implies READ_BACKUP=1.
         */
        return create.failed_illegal_create_option(
            "READ_BACKUP=0 cannot be used for fully replicated tables");
      }
      tab.setReadBackupFlag(true);
      tab.setFullyReplicated(true);
    } else if (use_read_backup) {
      tab.setReadBackupFlag(true);
    }
  }
  tab.setRowChecksum(opt_ndb_row_checksum);

  {
    /*
      Save the serialized table definition for this table as
      extra metadata of the table in the dictionary of NDB
    */

    dd::sdi_t sdi;
    if (!ndb_sdi_serialize(thd, table_def, dbname, sdi)) {
      return create.failed_internal_error(
          "Failed to serialize dictionary information");
    }

    const int result = tab.setExtraMetadata(2,  // version 2 for sdi
                                            sdi.c_str(), (Uint32)sdi.length());
    if (result != 0) {
      return create.failed_internal_error("Failed to set extra metadata");
    }
  }

  /*
    ROW_FORMAT=[DEFAULT|FIXED|DYNAMIC|etc.]

    Controls whether the NDB table will be created with a "varpart reference",
    thus allowing columns to be added inplace at a later time.
    It's possible to turn off "varpart reference" with ROW_FORMAT=FIXED, this
    will save datamemory in NDB at the cost of not being able to add
    columns inplace. Any other value enables "varpart reference".
  */
  if (create_info->row_type == ROW_TYPE_FIXED) {
    // CREATE TABLE .. ROW_FORMAT=FIXED
    DBUG_PRINT("info", ("Turning off 'varpart reference'"));
    tab.setForceVarPart(false);
    assert(ndb_dd_table_is_using_fixed_row_format(table_def));
  } else {
    tab.setForceVarPart(true);
    assert(!ndb_dd_table_is_using_fixed_row_format(table_def));
  }

  /*
     TABLESPACE=

     Controls whether the NDB table have corresponding tablespace. It's
     possible for a table to have tablespace although no columns are on disk.
  */
  if (create_info->tablespace) {
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
    restore_record(table, s->default_values);
    old_map = tmp_use_all_columns(table, table->read_set);
  }

  for (uint i = 0; i < table->s->fields; i++) {
    Field *const field = table->field[i];
    DBUG_PRINT("info", ("name: %s, type: %u, pack_length: %d, stored: %d",
                        field->field_name, field->real_type(),
                        field->pack_length(), field->stored_in_db));
    if ((field->auto_flags & Field::NEXT_NUMBER) &&
        !ndb_name_is_temp(tabname)) {
      uint64 max_field_memory;
      switch (field->pack_length()) {
        case 1:
          if (field->is_unsigned()) {
            max_field_memory = UINT_MAX8;
          } else {
            max_field_memory = INT_MAX8;
          }
          break;
        case 2:
          if (field->is_unsigned()) {
            max_field_memory = UINT_MAX16;
          } else {
            max_field_memory = INT_MAX16;
          }
          break;
        case 3:
          if (field->is_unsigned()) {
            max_field_memory = UINT_MAX24;
          } else {
            max_field_memory = INT_MAX24;
          }
          break;
        case 4:
          if (field->is_unsigned()) {
            max_field_memory = UINT_MAX32;
          } else {
            max_field_memory = INT_MAX32;
          }
          break;
        case 8:
        default:
          if (field->is_unsigned()) {
            max_field_memory = UINT_MAX64;
          } else {
            max_field_memory = INT_MAX64;
          }
          break;
      }
      unsigned int autoinc_prefetch = THDVAR(thd, autoincrement_prefetch_sz);
      // If autoincrement_prefetch_sz is greater than the max value of the
      // column than the first insert query in the second mysqld will fail.
      if (max_field_memory < (uint64)autoinc_prefetch) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_WRONG_FIELD_SPEC,
            "Max value for column %s in table %s.%s is less than "
            "autoincrement prefetch size. Please decrease "
            "ndb_autoincrement_prefetch_sz",
            field->field_name, dbname, tabname);
      }
    }
    if (field->stored_in_db) {
      NdbDictionary::Column col;
      const int create_column_result =
          create_ndb_column(thd, col, field, create_info);
      if (create_column_result) {
        return create_column_result;
      }

      // Turn on use_disk if the column is configured to be on disk
      if (col.getStorageType() == NDBCOL::StorageTypeDisk) {
        use_disk = true;
      }

      if (tab.addColumn(col)) {
        return create.failed_oom("Failed to add column");
      }
      if (col.getPrimaryKey()) pk_length += (field->pack_length() + 3) / 4;
    }
  }

  tmp_restore_column_map(table->read_set, old_map);
  if (use_disk) {
    if (mod_nologging->m_found && mod_nologging->m_val_bool) {
      // Setting NOLOGGING=1 on a disk table isn't permitted.
      return create.failed_illegal_create_option(
          "NOLOGGING=1 on table with fields using STORAGE DISK");
    }
    tab.setLogging(true);
    tab.setTemporary(false);

    if (create_info->tablespace) {
      tab.setTablespaceName(create_info->tablespace);
    } else {
      // It's not possible to create a table which uses disk without
      // also specifying a tablespace name
      return create.failed_missing_create_option(
          "TABLESPACE option must be specified when using STORAGE DISK");
    }
  }

  // Save the table level storage media setting
  switch (create_info->storage_media) {
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

  DBUG_PRINT("info", ("Table %s is %s stored with tablespace %s", tabname,
                      (use_disk) ? "disk" : "memory",
                      (use_disk) ? tab.getTablespaceName() : "N/A"));

  for (uint i = 0; i < table_share->keys; i++) {
    const KEY *key_info = table->key_info + i;
    const KEY_PART_INFO *key_part = key_info->key_part;
    const KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
    for (; key_part != end; key_part++) {
      if (key_part->field->field_storage_type() == HA_SM_DISK) {
        thd_ndb->push_warning(ER_ILLEGAL_HA_CREATE_OPTION,
                              "Cannot create index on DISK column '%s'. Alter "
                              "it in a way to use STORAGE MEMORY.",
                              key_part->field->field_name);
        return create.failed_illegal_create_option("index on DISK column");
      }
      table_map.getColumn(tab, key_part->fieldnr - 1)
          ->setStorageType(NdbDictionary::Column::StorageTypeMemory);
    }
  }

  // No primary key, create shadow key as 64 bit, auto increment
  if (table_share->primary_key == MAX_KEY) {
    DBUG_PRINT("info", ("Generating shadow key"));
    NdbDictionary::Column col;
    if (col.setName("$PK")) {
      return create.failed_oom("Failed to set name for shadow key");
    }
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(false);
    col.setPrimaryKey(true);
    col.setAutoIncrement(true);
    col.setDefaultValue(nullptr, 0);
    if (tab.addColumn(col)) {
      return create.failed_oom("Failed to add column for shadow key");
    }
    pk_length += 2;
  }

  // Make sure that blob tables don't have too big part size
  for (uint i = 0; i < table_share->fields; i++) {
    const Field *const field = table->field[i];
    if (!field->stored_in_db) continue;

    /**
     * The extra +7 concists
     * 2 - words from pk in blob table
     * 5 - from extra words added by tup/dict??
     */

    switch (field->real_type()) {
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VECTOR:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_JSON: {
        NdbDictionary::Column *column = table_map.getColumn(tab, i);
        unsigned size = pk_length + (column->getPartSize() + 3) / 4 + 7;
        unsigned ndb_max = MAX_BLOB_ROW_SIZE;

        if (size > ndb_max && (pk_length + 7) < ndb_max) {
          size = ndb_max - pk_length - 7;
          column->setPartSize(4 * size);
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
  assert(create_info->max_rows == table_share->max_rows);
  assert(create_info->min_rows == table_share->min_rows);

  {
    ha_rows max_rows = create_info->max_rows;
    ha_rows min_rows = create_info->min_rows;
    if (max_rows < min_rows) max_rows = min_rows;
    if (max_rows != (ha_rows)0) /* default setting, don't set fragmentation */
    {
      tab.setMaxRows(max_rows);
      tab.setMinRows(min_rows);
    }
  }

  // Check partition info
  {
    const int setup_partinfo_result =
        create_table_set_up_partition_info(table->part_info, tab, table_map);
    if (setup_partinfo_result) {
      return setup_partinfo_result;
    }
  }

  if (tab.getFullyReplicated() &&
      (tab.getFragmentType() != NDBTAB::HashMapPartition ||
       !tab.getDefaultNoPartitionsFlag())) {
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
      !mod_frags->m_found &&        // Let PARTITION_BALANCE override max_rows
      !tab.getFullyReplicated() &&  // Ignore max_rows for fully replicated
      (create_info->max_rows != 0 || create_info->min_rows != 0)) {
    ulonglong rows = create_info->max_rows >= create_info->min_rows
                         ? create_info->max_rows
                         : create_info->min_rows;
    uint no_fragments = get_no_fragments(rows);
    uint reported_frags = no_fragments;
    if (adjusted_frag_count(ndb, no_fragments, reported_frags)) {
      push_warning(current_thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                   "Ndb might have problems storing the max amount "
                   "of rows specified");
    }
    tab.setFragmentCount(reported_frags);
    tab.setDefaultNoPartitionsFlag(false);
    tab.setFragmentData(nullptr, 0);
    tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
  }

  // Check for HashMap
  if (tab.getFragmentType() == NDBTAB::HashMapPartition &&
      tab.getDefaultNoPartitionsFlag()) {
    /**
     * Default partitioning
     */
    tab.setFragmentCount(0);
    tab.setFragmentData(nullptr, 0);
    tab.setPartitionBalance(part_bal);
  } else if (tab.getFragmentType() == NDBTAB::HashMapPartition) {
    NdbDictionary::HashMap hm;
    if (dict->getDefaultHashMap(hm, tab.getFragmentCount()) == -1) {
      if (dict->initDefaultHashMap(hm, tab.getFragmentCount()) == -1) {
        return create.failed_in_NDB(dict->getNdbError());
      }

      if (dict->createHashMap(hm) == -1) {
        return create.failed_in_NDB(dict->getNdbError());
      }
    }
  }

  // Create the table in NDB
  if (dict->createTable(tab) != 0) {
    return create.failed_in_NDB(dict->getNdbError());
  }

  DBUG_PRINT("info",
             ("Table '%s.%s' created in NDB, id: %d, version: %d", dbname,
              tabname, tab.getObjectId(), tab.getObjectVersion()));

  // Update table definition with the table id and version of the newly
  // created table, the caller will then save this information in the DD
  ndb_dd_table_set_spi_and_version(table_def, tab.getObjectId(),
                                   tab.getObjectVersion());

  // Create secondary indexes
  if (create_indexes(thd, table, &tab) != 0) {
    return create.failed_warning_already_pushed();
  }

  if (thd_sql_command(thd) != SQLCOM_TRUNCATE) {
    const int create_fks_result = create_fks(thd, ndb, dbname, tabname);
    if (create_fks_result != 0) {
      return create_fks_result;
    }
  }

  if (thd->lex->sql_command == SQLCOM_ALTER_TABLE ||
      thd->lex->sql_command == SQLCOM_DROP_INDEX ||
      thd->lex->sql_command == SQLCOM_CREATE_INDEX) {
    // Copy foreign keys from the old NDB table (which still exists)
    const int copy_fk_result =
        copy_fk_for_offline_alter(thd, ndb, dbname, tabname);
    if (copy_fk_result != 0) {
      return copy_fk_result;
    }
  }

  if (!fk_list_for_truncate.empty()) {
    // create foreign keys from the list extracted from old table
    const int recreate_fk_result = recreate_fk_for_truncate(
        thd, ndb, dbname, tabname, &fk_list_for_truncate);
    if (recreate_fk_result != 0) {
      return recreate_fk_result;
    }
  }

  // All schema objects created, commit NDB schema transaction
  if (!schema_trans.commit_trans()) {
    return create.failed_warning_already_pushed();
  }

  // Log the commit in the Ndb_DDL_transaction_ctx
  Ndb_DDL_transaction_ctx *ddl_ctx = nullptr;
  if (thd_sql_command(thd) != SQLCOM_TRUNCATE) {
    ddl_ctx = thd_ndb->get_ddl_transaction_ctx(true);
    ddl_ctx->log_create_table(dbname, tabname);
  }

  if (DBUG_EVALUATE_IF("ndb_create_open_fail", true, false)) {
    // The table has been successfully created in NDB, emulate
    // failure to open the table by dropping the table from NDB
    Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
    assert(ndbtab_g.get_table());
    (void)drop_table_and_related(thd, ndb, dict, dbname, ndbtab_g.get_table(),
                                 0,       // drop_flags
                                 false);  // skip_related
  }

  Ndb_table_guard ndbtab_g(ndb, dbname, tabname);
  const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
  if (ndbtab == nullptr) {
    // Failed to open the newly created table from NDB, since the
    // table is apparently not in NDB it can't be dropped.
    // However an NDB error must have occurred since the table can't
    // be opened and as such the NDB error can be returned here
    return create.failed_in_NDB(ndbtab_g.getNdbError());
  }

  // Check if the DD table object has the correct number of partitions.
  // Correct the number of partitions in the DD table object in case of
  // a mismatch
  const bool check_partition_count_result = ndb_dd_table_check_partition_count(
      table_def, ndbtab->getPartitionCount());
  if (!check_partition_count_result) {
    ndb_dd_table_fix_partition_count(table_def, ndbtab->getPartitionCount());
  }

  // Acquire or create reference to NDB_SHARE
  NDB_SHARE *share =
      NDB_SHARE::acquire_or_create_reference(dbname, tabname, "create");
  if (!share) {
    // Failed to create the NDB_SHARE instance for this table, most likely OOM.
    return create.failed_oom("Failed to acquire NDB_SHARE");
  }

  // Guard for the temporary share.
  // This will release the share automatically when it goes out of scope.
  Ndb_share_temp_ref ndb_share_guard(share, "create");

  if (ndb_name_is_temp(tabname)) {
    // Temporary named table created OK
    return create.succeeded();  // All OK
  }

  // Check that NDB and DD metadata matches
  assert(Ndb_metadata::compare(thd, ndb, dbname, ndbtab, table_def));

  // Apply the mysql.ndb_replication settings
  if (binlog_client.apply_replication_info(ndb, share, ndbtab, conflict_fn,
                                           args, num_args, binlog_flags) != 0) {
    // Failed to apply replication settings
    return create.failed_warning_already_pushed();
  }

  if (binlog_client.table_should_have_event(share, ndbtab)) {
    if (binlog_client.create_event(ndb, ndbtab, share)) {
      // Failed to create event for this table
      return create.failed_internal_error("Failed to create event");
    }

    if (binlog_client.table_should_have_event_op(share)) {
      if (binlog_client.create_event_op(share, table_def, ndbtab) != 0) {
        // Failed to create event operation for this table
        return create.failed_internal_error("Failed to create event operation");
      }
    }
  }

  bool schema_dist_result;
  if (thd_sql_command(thd) == SQLCOM_TRUNCATE) {
    schema_dist_result = schema_dist_client.truncate_table(
        dbname, tabname, ndbtab->getObjectId(), ndbtab->getObjectVersion());
  } else {
    assert(thd_sql_command(thd) == SQLCOM_CREATE_TABLE);
    schema_dist_result = schema_dist_client.create_table(
        dbname, tabname, ndbtab->getObjectId(), ndbtab->getObjectVersion());
    if (schema_dist_result) {
      // Mark the stmt as distributed in Ndb_DDL_transaction_ctx.
      assert(ddl_ctx != nullptr);
      ddl_ctx->mark_last_stmt_as_distributed();
    }
  }
  if (!schema_dist_result) {
    // Failed to distribute the create/truncate of this table to the
    // other MySQL Servers, fail the CREATE/TRUNCATE
    return create.failed_internal_error("Failed to distribute table");
  }

  return create.succeeded();  // All OK
}

int ha_ndbcluster::create_index(THD *thd, const char *name, const KEY *key_info,
                                NDB_INDEX_TYPE idx_type,
                                const NdbDictionary::Table *ndbtab) const {
  int error = 0;
  char unique_name[FN_LEN + 1];
  static const char *unique_suffix = "$unique";
  DBUG_TRACE;
  DBUG_PRINT("enter", ("name: %s", name));

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX) {
    strxnmov(unique_name, FN_LEN, name, unique_suffix, NullS);
    DBUG_PRINT("info", ("unique_name: '%s'", unique_name));
  }

  switch (idx_type) {
    case PRIMARY_KEY_INDEX:
      // Do nothing, already created
      break;
    case PRIMARY_KEY_ORDERED_INDEX:
      error = create_index_in_NDB(thd, name, key_info, ndbtab, false);
      break;
    case UNIQUE_ORDERED_INDEX:
      if (!(error = create_index_in_NDB(thd, name, key_info, ndbtab, false)))
        error = create_index_in_NDB(thd, unique_name, key_info, ndbtab,
                                    true /* unique */);
      break;
    case UNIQUE_INDEX:
      if (check_index_fields_not_null(key_info)) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_NULL_COLUMN_IN_INDEX,
            "Ndb does not support unique index on NULL valued attributes, "
            "index access with NULL value will become full table scan");
      }
      error = create_index_in_NDB(thd, unique_name, key_info, ndbtab,
                                  true /* unique */);
      break;
    case ORDERED_INDEX:
      if (key_info->algorithm == HA_KEY_ALG_HASH) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_ILLEGAL_HA_CREATE_OPTION,
            ER_THD(thd, ER_ILLEGAL_HA_CREATE_OPTION), ndbcluster_hton_name,
            "Ndb does not support non-unique "
            "hash based indexes");
        error = HA_ERR_UNSUPPORTED;
        break;
      }
      error = create_index_in_NDB(thd, name, key_info, ndbtab, false);
      break;
    default:
      assert(false);
      break;
  }

  return error;
}

// Maximum index size supported by the index statistics implementation in the
// kernel. The limitation comes from the size of a column in the
// ndb_index_stat_sample table
static constexpr unsigned MAX_INDEX_SIZE_STAT = 3056;

/**
  @brief Create an index in NDB.
*/
int ha_ndbcluster::create_index_in_NDB(THD *thd, const char *name,
                                       const KEY *key_info,
                                       const NdbDictionary::Table *ndbtab,
                                       bool unique) const {
  Ndb *ndb = get_thd_ndb(thd)->ndb;
  NdbDictionary::Dictionary *dict = ndb->getDictionary();

  DBUG_TRACE;
  DBUG_PRINT("enter", ("name: %s, unique: %d ", name, unique));

  char index_name[FN_LEN + 1];
  ndb_protect_char(name, index_name, sizeof(index_name) - 1, '/');
  DBUG_PRINT("info", ("index_name: %s ", index_name));

  NdbDictionary::Index ndb_index(index_name);
  if (unique)
    ndb_index.setType(NdbDictionary::Index::UniqueHashIndex);
  else {
    ndb_index.setType(NdbDictionary::Index::OrderedIndex);
    ndb_index.setLogging(false);
  }

  if (!ndbtab->getLogging()) {
    ndb_index.setLogging(false);
  }

  if (ndbtab->getTemporary()) {
    ndb_index.setTemporary(true);
  }

  if (ndb_index.setTable(ndbtab->getName())) {
    // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
    return HA_ERR_OUT_OF_MEM;
  }

  KEY_PART_INFO *key_part = key_info->key_part;
  KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
  uint key_store_length = 0;
  for (; key_part != end; key_part++) {
    Field *field = key_part->field;
    if (field->field_storage_type() == HA_SM_DISK) {
      my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION,
                      "Cannot create index on DISK column '%s'. Alter it "
                      "in a way to use STORAGE MEMORY.",
                      MYF(0), field->field_name);
      return HA_ERR_UNSUPPORTED;
    }
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    if (ndb_index.addColumnName(field->field_name)) {
      // Can only fail due to memory -> return HA_ERR_OUT_OF_MEM
      return HA_ERR_OUT_OF_MEM;
    }

    if (!unique) {
      // Calculate the size of ordered indexes. This is used later to see if it
      // exceeds the maximum size supported by the NDB index statistics
      // implementation
      if (key_part->store_length != 0) {
        key_store_length += key_part->store_length;
      } else {
        // The store length hasn't been computed for this key_part. This is the
        // case for CREATE INDEX/ALTER TABLE. The below is taken from
        // KEY_PART_INFO::init_from_field() in sql/table.cc
        key_store_length += key_part->length;
        if (field->is_nullable()) {
          key_store_length += HA_KEY_NULL_LENGTH;
        }
        if (field->type() == MYSQL_TYPE_BLOB ||
            field->type() == MYSQL_TYPE_VECTOR ||
            field->real_type() == MYSQL_TYPE_VARCHAR ||
            field->type() == MYSQL_TYPE_GEOMETRY) {
          key_store_length += HA_KEY_BLOB_LENGTH;
        }
      }
    }
  }

  if (!unique && key_store_length > MAX_INDEX_SIZE_STAT) {
    // The ordered index size exceeds the maximum size supported by NDB. Allow
    // the index to be created with a warning
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                        "Specified key '%s' was too long (max = %d bytes); "
                        "statistics will not be generated",
                        index_name, MAX_INDEX_SIZE_STAT);
  }

  if (dict->createIndex(ndb_index, *ndbtab)) ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  return 0;
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

int ha_ndbcluster::truncate(dd::Table *table_def) {
  DBUG_TRACE;

  /* Table should have been opened */
  assert(m_table);

  /* Fill in create_info from the open table */
  HA_CREATE_INFO create_info;
  update_create_info_from_table(&create_info, table);

  // Close the table, will always return 0
  (void)close();

  // Call ha_ndbcluster::create which will detect that this is a
  // truncate and thus drop the table before creating it again.
  const int truncate_error =
      create(table->s->normalized_path.str, table, &create_info, table_def);

  // Open the table again even if the truncate failed, the caller
  // expect the table to be open. Report any error during open.
  const int open_error = open(table->s->normalized_path.str, 0, 0, table_def);

  if (truncate_error) return truncate_error;
  return open_error;
}

int ha_ndbcluster::prepare_inplace__add_index(THD *thd, KEY *key_info,
                                              uint num_of_keys) const {
  int error = 0;
  DBUG_TRACE;

  for (uint idx = 0; idx < num_of_keys; idx++) {
    KEY *key = key_info + idx;
    KEY_PART_INFO *key_part = key->key_part;
    KEY_PART_INFO *end = key_part + key->user_defined_key_parts;
    // Add fields to key_part struct
    for (; key_part != end; key_part++)
      key_part->field = table->field[key_part->fieldnr];
    // Check index type
    // Create index in ndb
    const NDB_INDEX_TYPE idx_type =
        get_index_type_from_key(idx, key_info, false);
    if ((error =
             create_index(thd, key_info[idx].name, key, idx_type, m_table))) {
      break;
    }
  }
  return error;
}

/*
   Prepare drop of indexes
*/
void ha_ndbcluster::prepare_inplace__drop_index(uint index_num) {
  DBUG_TRACE;

  // Release index statistics if the index has a physical NDB ordered index
  const NDB_INDEX_TYPE index_type = m_index[index_num].type;
  if (index_type == PRIMARY_KEY_ORDERED_INDEX ||
      index_type == UNIQUE_ORDERED_INDEX || index_type == ORDERED_INDEX) {
    const NdbDictionary::Index *index = m_index[index_num].index;
    if (!index) {
      // NOTE! This is very unusual, the index should have been loaded when
      // table was opened otherwise an error would have been returned
      assert(index);
      return;
    }
    ndb_index_stat_free(m_share, index->getObjectId(),
                        index->getObjectVersion());
  }
}

extern void ndb_fk_util_resolve_mock_tables(THD *thd, Ndb *ndb,
                                            const char *new_parent_db,
                                            const char *new_parent_name);

extern int ndb_fk_util_rename_foreign_keys(
    THD *thd, NdbDictionary::Dictionary *dict,
    const NdbDictionary::Table *renamed_table,
    const std::string &old_table_name, const std::string &new_db_name,
    const std::string &new_table_name);

int rename_table_impl(THD *thd, Ndb *ndb,
                      Ndb_schema_dist_client *schema_dist_client,
                      const NdbDictionary::Table *orig_tab,
                      dd::Table *to_table_def, const char *from, const char *to,
                      const char *old_dbname, const char *old_tabname,
                      const char *new_dbname, const char *new_tabname,
                      bool real_rename, const char *real_rename_db,
                      const char *real_rename_name, bool drop_events,
                      bool create_events, bool commit_alter) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("real_rename: %d", real_rename));
  DBUG_PRINT("info", ("real_rename_db: '%s'", real_rename_db));
  DBUG_PRINT("info", ("real_rename_name: '%s'", real_rename_name));
  // Verify default values of real_rename related parameters
  assert(real_rename ||
         (real_rename_db == nullptr && real_rename_name == nullptr));

  DBUG_PRINT("info", ("drop_events: %d", drop_events));
  DBUG_PRINT("info", ("create_events: %d", create_events));
  DBUG_PRINT("info", ("commit_alter: %d", commit_alter));

  DBUG_EXECUTE_IF("ndb_simulate_alter_failure_rename1", {
    // Simulate failure during copy alter first rename
    // when renaming old table to a temp name
    if (!ndb_name_is_temp(old_tabname) && ndb_name_is_temp(new_tabname)) {
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Simulated : Failed to rename original table to a temp name.");
      DBUG_SET("-d,ndb_simulate_alter_failure_rename1");
      return ER_INTERNAL_ERROR;
    }
  });

  DBUG_EXECUTE_IF("ndb_simulate_alter_failure_rename2", {
    // Simulate failure during during copy alter second rename
    // when renaming new table with temp name to original name
    if (ndb_name_is_temp(old_tabname) && !ndb_name_is_temp(new_tabname)) {
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Simulated : Failed to rename new table to target name.");
      DBUG_SET("-d,ndb_simulate_alter_failure_rename2");
      return ER_INTERNAL_ERROR;
    }
  });

  DBUG_EXECUTE_IF("ndb_simulate_crash_during_alter_table_rename1", {
    // Force mysqld crash during copy alter first rename
    // before renaming old table to a temp name
    if (!ndb_name_is_temp(old_tabname) && ndb_name_is_temp(new_tabname)) {
      DBUG_SUICIDE();
    }
  });

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock("ha_ndbcluster::rename_table"))
    return HA_ERR_NO_CONNECTION;

  NDBDICT *dict = ndb->getDictionary();
  NDBDICT::List index_list;
  if (my_strcasecmp(system_charset_info, new_dbname, old_dbname)) {
    // NOTE! This is backwards compatibility code for preserving the old
    // index name format during a rename table.
    // When moving tables between databases the indexes need to be
    // recreated, save list of indexes before rename to allow
    // them to be recreated afterwards
    dict->listIndexes(index_list, *orig_tab);
  }

  // Change current database to that of target table
  const Ndb_dbname_guard dbname_guard(ndb, new_dbname);
  if (dbname_guard.change_database_failed()) {
    ERR_RETURN(ndb->getNdbError());
  }

  const int ndb_table_id = orig_tab->getObjectId();
  const int ndb_table_version = orig_tab->getObjectVersion();

  Ndb_share_temp_ref share(old_dbname, old_tabname, "rename_table_impl");
  if (real_rename) {
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
    if (!schema_dist_client->rename_table_prepare(
            real_rename_db, real_rename_name, ndb_table_id, ndb_table_version,
            to)) {
      // Failed to distribute the prepare rename of this table to the
      // other MySQL Servers, just log error and continue
      // NOTE! Actually it's no point in continuing trying to rename since
      // the participants will most likley not know what the new name of
      // the table is.
      ndb_log_error("Failed to distribute prepare rename for '%s'",
                    real_rename_name);
    }
  }
  NDB_SHARE_KEY *old_key = share->key;  // Save current key
  NDB_SHARE_KEY *new_key = NDB_SHARE::create_key(to);
  (void)NDB_SHARE::rename_share(share, new_key);

  Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx(false);
  const bool rollback_in_progress =
      (ddl_ctx != nullptr && ddl_ctx->rollback_in_progress());
  std::string orig_sdi;
  if (!rollback_in_progress) {
    // Backup the original sdi in case if we have to rollback
    Uint32 version;
    void *unpacked_data;
    Uint32 unpacked_len;
    const int get_result =
        orig_tab->getExtraMetadata(version, &unpacked_data, &unpacked_len);
    if (get_result != 0) {
      my_printf_error(ER_INTERNAL_ERROR,
                      "Failed to read extra metadata during rename table, "
                      "error: %d",
                      MYF(0), get_result);
      return HA_ERR_INTERNAL_ERROR;
    }
    orig_sdi.assign(static_cast<const char *>(unpacked_data), unpacked_len);
    free(unpacked_data);
  }

  NdbDictionary::Table new_tab = *orig_tab;
  new_tab.setName(new_tabname);

  {
    dd::sdi_t sdi;
    if (rollback_in_progress) {
      // This is a rollback. Fetch the original sdi from the DDL context log.
      ddl_ctx->get_original_sdi_for_rename(sdi);
    } else {
      // This is an actual rename and not a rollback of the rename
      // Create a new serialized table definition for the table to be
      // renamed since it contains the table name.
      assert(to_table_def != nullptr);
      if (!ndb_sdi_serialize(thd, to_table_def, new_dbname, sdi)) {
        my_error(ER_INTERNAL_ERROR, MYF(0), "Table def. serialization failed");
        return HA_ERR_INTERNAL_ERROR;
      }
    }

    const int set_result =
        new_tab.setExtraMetadata(2,  // version 2 for sdi
                                 sdi.c_str(), (Uint32)sdi.length());
    if (set_result != 0) {
      my_printf_error(ER_INTERNAL_ERROR,
                      "Failed to set extra metadata during rename table, "
                      "error: %d",
                      MYF(0), set_result);
      return HA_ERR_INTERNAL_ERROR;
    }
  }

  if (dict->alterTableGlobal(*orig_tab, new_tab) != 0) {
    const NdbError ndb_error = dict->getNdbError();
    // Rename the share back to old_key
    (void)NDB_SHARE::rename_share(share, old_key);
    // Release the unused new_key
    NDB_SHARE::free_key(new_key);
    ERR_RETURN(ndb_error);
  }
  // Release the unused old_key
  NDB_SHARE::free_key(old_key);

  // Load the altered table
  Ndb_table_guard ndbtab_g(ndb, new_dbname, new_tabname);
  const NDBTAB *ndbtab = ndbtab_g.get_table();
  if (!ndbtab) {
    ERR_RETURN(ndbtab_g.getNdbError());
  }

  if (!rollback_in_progress) {
    // This is an actual rename and not a rollback of the rename
    // Fetch the new table version and write it to the table definition,
    // the caller will then save it into DD
    // The id should still be the same as before the rename
    assert(ndbtab->getObjectId() == ndb_table_id);
    // The version should have been changed by the rename
    assert(ndbtab->getObjectVersion() != ndb_table_version);

    ndb_dd_table_set_spi_and_version(to_table_def, ndb_table_id,
                                     ndbtab->getObjectVersion());

    // Log the rename in the Ndb_DDL_transaction_ctx object
    ddl_ctx =
        (ddl_ctx == nullptr) ? thd_ndb->get_ddl_transaction_ctx(true) : ddl_ctx;
    ddl_ctx->log_rename_table(old_dbname, old_tabname, new_dbname, new_tabname,
                              from, to, orig_sdi);
  }

  ndb_fk_util_resolve_mock_tables(thd, ndb, new_dbname, new_tabname);

  /* handle old table */
  if (drop_events) {
    Ndb_binlog_client::drop_events_for_table(thd, ndb, old_dbname, old_tabname);
  }

  Ndb_binlog_client binlog_client(thd, new_dbname, new_tabname);

  if (create_events) {
    Ndb_table_guard ndbtab_g2(ndb, new_dbname, new_tabname);
    const NDBTAB *ndbtab = ndbtab_g2.get_table();
    if (!ndbtab) {
      ERR_RETURN(ndbtab_g2.getNdbError());
    }

    // NOTE! Should check error and fail the rename
    (void)binlog_client.read_and_apply_replication_info(ndb, share, ndbtab,
                                                        ::server_id);

    if (binlog_client.table_should_have_event(share, ndbtab)) {
      if (binlog_client.create_event(ndb, ndbtab, share)) {
        // Failed to create event for this table, fail the rename
        // NOTE! Should cover whole function with schema transaction to cleanup
        my_printf_error(ER_INTERNAL_ERROR,
                        "Failed to to create event for table '%s'", MYF(0),
                        share->key_string());
        return ER_INTERNAL_ERROR;
      }

      if (binlog_client.table_should_have_event_op(share)) {
        // NOTE! Simple renames performs the rename without recreating the event
        // operation, thus the check if share have event operation  below.
        if (share->have_event_operation() == false &&
            binlog_client.create_event_op(share, to_table_def, ndbtab) != 0) {
          // Failed to create event for this table, fail the rename
          // NOTE! Should cover whole function with schema transaction to
          // cleanup
          my_printf_error(ER_INTERNAL_ERROR,
                          "Failed to create event operation for table '%s'",
                          MYF(0), share->key_string());
          return ER_INTERNAL_ERROR;
        }
      }
    }
  }

  if (real_rename) {
    /*
      This is a real rename - either the final phase of a copy alter involving
      a table rename or a simple rename. In either case, the generated names
      of foreign keys has to be renamed.
     */
    int error;
    if ((error = ndb_fk_util_rename_foreign_keys(
             thd, dict, ndbtab, real_rename_name, new_dbname, new_tabname))) {
      return error;
    }

    /*
      Commit of "real" rename table on participant i.e make the participant
      extract the original table name which it got in prepare.

      NOTE! The tricky thing also here is that the NDB_SHARE haven't yet been
      renamed on the participant and thus you have to use the original
      table name when communicating with the participant, otherwise it
      will not find the share where the final table name has been stashed.

      Also note that this has to be written to the participant binlog only if
      it is not a part of an copy alter. In case of this rename being a part
      of or done through a copy alter, the query will be written to the
      participant's binlog during the distribution of that ALTER. This should
      also be skipped if the rename is part of a rollback.
    */
    const bool log_on_participant = !(commit_alter || rollback_in_progress);
    if (schema_dist_client->rename_table(
            real_rename_db, real_rename_name, ndb_table_id, ndb_table_version,
            new_dbname, new_tabname, log_on_participant)) {
      // Schema distribution succeeded.
      if (!rollback_in_progress) {
        // Log this in the DDL Context if this is not a rollback
        assert(ddl_ctx != nullptr);
        ddl_ctx->mark_last_stmt_as_distributed();
      }
    } else {
      // Failed to distribute the rename of this table to the
      // other MySQL Servers, just log error and continue
      ndb_log_error("Failed to distribute rename for '%s'", real_rename_name);
    }

    DBUG_EXECUTE_IF("ndb_simulate_failure_after_table_rename", {
      // Simulate failure after the table has been renamed.
      // This can either be an ALTER RENAME (with COPY or INPLACE algorithm)
      // or a simple RENAME.
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Simulated : Failed after renaming the table.");
      DBUG_SET("-d,ndb_simulate_failure_after_table_rename");
      return ER_INTERNAL_ERROR;
    });
  }

  if (commit_alter) {
    // Final phase of offline alter table.

    // Check that NDB and DD metadata matches if this isn't a rollback
    assert(rollback_in_progress ||
           Ndb_metadata::compare(thd, ndb, new_dbname, ndbtab, to_table_def));

    /* Skip logging on participant if this is a rollback.
       Note : Regardless of the outcome, during rollback, we always have to
              schema distribute an ALTER COPY to update the table version in
              connected servers' DD. So we don't have to log this in the DDL
              context. */
    if (!schema_dist_client->alter_table(new_dbname, new_tabname, ndb_table_id,
                                         ndb_table_version,
                                         !rollback_in_progress)) {
      // Failed to distribute the alter of this table to the
      // other MySQL Servers, just log error and continue
      ndb_log_error("Failed to distribute 'ALTER TABLE %s'", new_tabname);
    }
  }

  for (unsigned i = 0; i < index_list.count; i++) {
    NDBDICT::List::Element &index_el = index_list.elements[i];
    // NOTE! This is backwards compatibility code for preserving the old
    // index name format during a rename table, the actual table rename is done
    // inplace and then these indexes are renamed separately.
    // Recreate any indexes not stored in the system database
    if (my_strcasecmp(system_charset_info, index_el.database,
                      NDB_SYSTEM_DATABASE)) {
      // Get old index
      const NdbDictionary::Index *index =
          dict->getIndexGlobal(index_el.name, *orig_tab);
      if (!index) {
        // Could not open the old index, push warning and then skip
        // create new and drop old index.
        thd_ndb->push_ndb_error_warning(dict->getNdbError());
        thd_ndb->push_warning("Failed to move index with old name");
        assert(false);
      } else {
        DBUG_PRINT("info", ("Creating index %s/%s", index_el.database,
                            index->getName()));
        // Create the same "old" index on new tab
        dict->createIndex(*index, new_tab);
        DBUG_PRINT("info", ("Dropping index %s/%s", index_el.database,
                            index->getName()));
        // Drop old index
        dict->dropIndexGlobal(*index);
      }
    }
  }
  return 0;
}

static bool check_table_id_and_version(const dd::Table *table_def,
                                       const NdbDictionary::Table *ndbtab) {
  DBUG_TRACE;

  Ndb_dd_handle dd_handle = ndb_dd_table_get_spi_and_version(table_def);
  if (!dd_handle.valid()) {
    return false;
  }

  // Check that the id and version from DD
  // matches the id and version of the NDB table
  Ndb_dd_handle curr_handle{ndbtab->getObjectId(), ndbtab->getObjectVersion()};
  if (curr_handle != dd_handle) {
    return false;
  }

  return true;
}

/**
  Rename a table in NDB and on the participating mysqld(s)
*/

int ha_ndbcluster::rename_table(const char *from, const char *to,
                                const dd::Table *from_table_def,
                                dd::Table *to_table_def) {
  THD *thd = current_thd;

  DBUG_TRACE;
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));

  char old_dbname[FN_HEADLEN];
  char old_tabname[FN_HEADLEN];
  char new_dbname[FN_HEADLEN];
  char new_tabname[FN_HEADLEN];
  ndb_set_dbname(from, old_dbname);
  ndb_set_tabname(from, old_tabname);
  ndb_set_dbname(to, new_dbname);
  ndb_set_tabname(to, new_tabname);

  DBUG_PRINT("info", ("old_tabname: '%s'", old_tabname));
  DBUG_PRINT("info", ("new_tabname: '%s'", new_tabname));

  if (check_ndb_connection(thd)) return HA_ERR_NO_CONNECTION;

  Ndb_schema_dist_client schema_dist_client(thd);

  {
    // Prepare schema distribution, find the names which will be used in this
    // rename by looking at the parameters and the lex structures.
    const char *prepare_dbname;
    const char *prepare_tabname;
    switch (thd_sql_command(thd)) {
      case SQLCOM_CREATE_INDEX:
      case SQLCOM_DROP_INDEX:
      case SQLCOM_ALTER_TABLE:
        prepare_dbname = thd->lex->query_block->get_table_list()->db;
        prepare_tabname = thd->lex->query_block->get_table_list()->table_name;
        break;

      case SQLCOM_RENAME_TABLE:
        prepare_dbname = old_dbname;
        prepare_tabname = old_tabname;
        break;

      default:
        ndb_log_error(
            "INTERNAL ERROR: Unexpected sql command: %u "
            "using rename_table",
            thd_sql_command(thd));
        abort();
        break;
    }

    if (!schema_dist_client.prepare_rename(prepare_dbname, prepare_tabname,
                                           new_dbname, new_tabname)) {
      return HA_ERR_NO_CONNECTION;
    }
  }

  std::string invalid_identifier;
  if (!schema_dist_client.check_identifier_limits(invalid_identifier)) {
    // Check of db or table name limits failed
    my_error(ER_TOO_LONG_IDENT, MYF(0), invalid_identifier.c_str());
    return HA_WRONG_CREATE_OPTION;
  }

  // Open the table which is to be renamed(aka. the old)
  Ndb *ndb = get_thd_ndb(thd)->ndb;
  Ndb_table_guard ndbtab_g(ndb, old_dbname, old_tabname);
  const NDBTAB *orig_tab = ndbtab_g.get_table();
  if (orig_tab == nullptr) {
    ERR_RETURN(ndbtab_g.getNdbError());
  }
  DBUG_PRINT("info", ("NDB table name: '%s'", orig_tab->getName()));

  // Check that id and version of the table to be renamed
  // matches the id and version of the NDB table
  if (!check_table_id_and_version(from_table_def, orig_tab)) {
    return HA_ERR_INTERNAL_ERROR;
  }

  // Magically detect if this is a rename or some form of alter
  // and decide which actions need to be performed
  const bool old_is_temp = ndb_name_is_temp(old_tabname);
  const bool new_is_temp = ndb_name_is_temp(new_tabname);

  switch (thd_sql_command(thd)) {
    case SQLCOM_DROP_INDEX:
    case SQLCOM_CREATE_INDEX:
      DBUG_PRINT("info", ("CREATE or DROP INDEX as copying ALTER"));
      [[fallthrough]];
    case SQLCOM_ALTER_TABLE:
      DBUG_PRINT("info", ("SQLCOM_ALTER_TABLE"));

      if (!new_is_temp && !old_is_temp) {
        /*
          This is a rename directly from real to real which occurs:
          1) when the ALTER is "simple" RENAME i.e only consists of RENAME
             and/or enable/disable indexes
          2) as part of inplace ALTER .. RENAME
         */
        DBUG_PRINT("info", ("simple rename detected"));
        return rename_table_impl(thd, ndb, &schema_dist_client, orig_tab,
                                 to_table_def, from, to, old_dbname,
                                 old_tabname, new_dbname, new_tabname,
                                 true,         // real_rename
                                 old_dbname,   // real_rename_db
                                 old_tabname,  // real_rename_name
                                 true,         // drop_events
                                 true,         // create events
                                 false);       // commit_alter
      }

      // Make sure that inplace was not requested
      assert(thd->lex->alter_info->requested_algorithm !=
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

      if (new_is_temp) {
        if (Ndb_dist_priv_util::is_privilege_table(old_dbname, old_tabname)) {
          // Special case allowing the legacy distributed privilege tables
          // to be migrated to local shadow tables.  Do not drop the table from
          // NdbDictionary or publish this change via schema distribution.
          // Rename the share.
          ndb_log_info("Migrating legacy privilege table: Rename %s to %s",
                       old_tabname, new_tabname);
          Ndb_share_temp_ref share(old_dbname, old_tabname,
                                   "rename_table__for_local_shadow");
          // privilege tables never have an event
          assert(!share->have_event_operation());
          NDB_SHARE_KEY *old_key = share->key;  // Save current key
          NDB_SHARE_KEY *new_key = NDB_SHARE::create_key(to);
          (void)NDB_SHARE::rename_share(share, new_key);
          NDB_SHARE::free_key(old_key);
          return 0;
        }

        /*
          This is an alter table which renames real name to temp name.
          ie. step 3) per above and is the first of
          two rename_table() calls. Drop events from the table.
        */
        DBUG_PRINT("info", ("real -> temp"));
        return rename_table_impl(thd, ndb, &schema_dist_client, orig_tab,
                                 to_table_def, from, to, old_dbname,
                                 old_tabname, new_dbname, new_tabname,
                                 false,    // real_rename
                                 nullptr,  // real_rename_db
                                 nullptr,  // real_rename_name
                                 true,     // drop_events
                                 false,    // create events
                                 false);   // commit_alter
      }

      if (old_is_temp) {
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

          Important here is to remember to rename the table also
          on all partiticipants so they will find the table when
          the alter is completed. This is slightly problematic since
          their table is renamed directly from real to real name, while
          the mysqld who performs the alter renames from temp to real
          name. Fortunately it's possible to lookup the original table
          name via THD.
        */
        const char *orig_name =
            thd->lex->query_block->get_table_list()->table_name;
        const char *orig_db = thd->lex->query_block->get_table_list()->db;
        if (thd->lex->alter_info->flags & Alter_info::ALTER_RENAME &&
            (my_strcasecmp(system_charset_info, orig_db, new_dbname) ||
             my_strcasecmp(system_charset_info, orig_name, new_tabname))) {
          DBUG_PRINT("info", ("ALTER with RENAME detected"));
          /*
            Use the original table name when communicating with participant
          */
          const char *real_rename_db = orig_db;
          const char *real_rename_name = orig_name;

          return rename_table_impl(thd, ndb, &schema_dist_client, orig_tab,
                                   to_table_def, from, to, old_dbname,
                                   old_tabname, new_dbname, new_tabname,
                                   true,  // real_rename
                                   real_rename_db, real_rename_name,
                                   false,  // drop_events
                                   true,   // create events
                                   true);  // commit_alter
        }

        return rename_table_impl(thd, ndb, &schema_dist_client, orig_tab,
                                 to_table_def, from, to, old_dbname,
                                 old_tabname, new_dbname, new_tabname,
                                 false,    // real_rename
                                 nullptr,  // real_rename_db
                                 nullptr,  // real_rename_name
                                 false,    // drop_events
                                 true,     // create events
                                 true);    // commit_alter
      }
      break;

    case SQLCOM_RENAME_TABLE:
      DBUG_PRINT("info", ("SQLCOM_RENAME_TABLE"));

      return rename_table_impl(thd, ndb, &schema_dist_client, orig_tab,
                               to_table_def, from, to, old_dbname, old_tabname,
                               new_dbname, new_tabname,
                               true,         // real_rename
                               old_dbname,   // real_rename_db
                               old_tabname,  // real_rename_name
                               true,         // drop_events
                               true,         // create events
                               false);       // commit_alter
      break;

    default:
      ndb_log_error("Unexpected rename case detected, sql_command: %d",
                    thd_sql_command(thd));
      abort();
      break;
  }

  // Never reached
  return HA_ERR_UNSUPPORTED;
}

// Declare adapter functions for Dummy_table_util function
extern bool ndb_fk_util_build_list(THD *, NdbDictionary::Dictionary *,
                                   const NdbDictionary::Table *, List<char> &);
extern void ndb_fk_util_drop_list(THD *, Ndb *ndb, NdbDictionary::Dictionary *,
                                  List<char> &);
extern bool ndb_fk_util_drop_table(THD *, Ndb *ndb, const char *db_name,
                                   const NdbDictionary::Table *);
extern bool ndb_fk_util_is_mock_name(const char *table_name);

/**
  Delete table and its related objects from NDB.
*/

static bool drop_table_and_related(THD *thd, Ndb *ndb,
                                   NdbDictionary::Dictionary *dict,
                                   const char *dbname,
                                   const NdbDictionary::Table *table,
                                   int drop_flags, bool skip_related) {
  DBUG_TRACE;
  DBUG_PRINT(
      "enter",
      ("cascade_constraints: %d dropdb: %d skip_related: %d",
       static_cast<bool>(drop_flags & NDBDICT::DropTableCascadeConstraints),
       static_cast<bool>(drop_flags &
                         NDBDICT::DropTableCascadeConstraintsDropDB),
       skip_related));

  /*
    Build list of objects which should be dropped after the table
    unless the caller ask to skip dropping related
  */
  List<char> drop_list;
  if (!skip_related && !ndb_fk_util_build_list(thd, dict, table, drop_list)) {
    return false;
  }

  // Drop the table
  if (dict->dropTableGlobal(*table, drop_flags) != 0) {
    const NdbError &ndb_err = dict->getNdbError();
    if (ndb_err.code == 21080 &&
        thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
      /*
        Drop was not allowed because table is still referenced by
        foreign key(s). Since foreign_key_checks=0 the problem is
        worked around by creating a mock table, recreating the foreign
        key(s) to point at the mock table and finally dropping
        the requested table.
      */
      if (!ndb_fk_util_drop_table(thd, ndb, dbname, table)) {
        return false;
      }
    } else {
      return false;
    }
  }

  // Drop objects which should be dropped after table
  ndb_fk_util_drop_list(thd, ndb, dict, drop_list);

  return true;
}

/*
  @brief Drop a table in NDB. This includes dropping the table and related
  objects and events, finally distribute the drop to schema dist participants.

  @note Success is reported when dropping a table which doesn't exist in NDB.

  @return zero on successful drop and the last NDB error code if an error
  occurs.
*/
int drop_table_impl(THD *thd, Ndb *ndb,
                    Ndb_schema_dist_client *schema_dist_client, const char *db,
                    const char *table_name) {
  DBUG_TRACE;

  // Acquire NDB_SHARE.
  // NOTE! The NDB_SHARE might not exist. This may happen when
  // drop_database_impl() retrieves a list of tables from NDB which are dropped.
  // Since those tables are normally not known the MySQL Server (they haven't
  // yet been synced/discovered or failed to install) there will not be any
  // NDB_SHARE either.
  NDB_SHARE *share =
      NDB_SHARE::acquire_reference(db, table_name, "delete_table");

  bool skip_related = false;
  int drop_flags = 0;
  // Copying alter can leave temporary named table which is parent of old FKs
  if ((thd_sql_command(thd) == SQLCOM_ALTER_TABLE ||
       thd_sql_command(thd) == SQLCOM_DROP_INDEX ||
       thd_sql_command(thd) == SQLCOM_CREATE_INDEX) &&
      ndb_name_is_temp(table_name)) {
    DBUG_PRINT("info", ("Using cascade constraints for ALTER of temp table"));
    drop_flags |= NDBDICT::DropTableCascadeConstraints;
    // Cascade constraint is used and related will be dropped anyway
    skip_related = true;
  }

  if (thd_sql_command(thd) == SQLCOM_DROP_DB) {
    DBUG_PRINT("info", ("Using cascade constraints DB for drop database"));
    drop_flags |= NDBDICT::DropTableCascadeConstraintsDropDB;
  }

  if (thd_sql_command(thd) == SQLCOM_TRUNCATE) {
    DBUG_PRINT("info", ("Deleting table for TRUNCATE, skip dropping related"));
    skip_related = true;
  }

  // Drop the table from NDB
  NDBDICT *dict = ndb->getDictionary();
  int ndb_table_id = 0;
  int ndb_table_version = 0;
  uint retries = 100;
  while (true) {
    Ndb_table_guard ndbtab_g(ndb, db, table_name);
    const NDBTAB *ndbtab = ndbtab_g.get_table();
    if (ndbtab == nullptr) {
      // Table not found
      break;
    }

    if (drop_table_and_related(thd, ndb, dict, db, ndbtab, drop_flags,
                               skip_related)) {
      // Table successfully dropped from NDB
      ndb_table_id = ndbtab->getObjectId();
      ndb_table_version = ndbtab->getObjectVersion();
      break;
    }

    // An error has occurred. Examine the failure and retry if possible
    if (--retries && dict->getNdbError().status == NdbError::TemporaryError &&
        !thd_killed(thd)) {
      ndb_trans_retry_sleep();
      continue;
    }

    if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT) {
      // Invalidate the object and retry
      ndbtab_g.invalidate();
      continue;
    }

    // Some other error has occurred, do not retry
    break;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  const int dict_error_code = dict->getNdbError().code;
  // Check if an error has occurred. Note that if the table didn't exist in NDB
  // (denoted by error codes 709 or 723), it's considered a success
  if (dict_error_code && dict_error_code != 709 && dict_error_code != 723) {
    // The drop table failed, just release the share reference and return error
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    if (share) {
      NDB_SHARE::release_reference(share, "delete_table");
    }
    return dict_error_code;
  }

  // Drop the event(s) for the table
  Ndb_binlog_client::drop_events_for_table(thd, ndb, db, table_name);

  if (share) {
    // Wait for binlog thread to detect the dropped table
    // and release it's event operations
    ndbcluster_binlog_wait_synch_drop_table(thd, share);
  }

  // Distribute the drop table.
  // Skip logging in participant if this is a rollback
  Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx(false);
  bool log_on_participant =
      (ddl_ctx == nullptr || !ddl_ctx->rollback_in_progress());
  if (!ndb_name_is_temp(table_name) &&
      thd_sql_command(thd) != SQLCOM_TRUNCATE &&
      thd_sql_command(thd) != SQLCOM_DROP_DB && schema_dist_client != nullptr) {
    if (!schema_dist_client->drop_table(db, table_name, ndb_table_id,
                                        ndb_table_version,
                                        log_on_participant)) {
      // Failed to distribute the drop of this table to the
      // other MySQL Servers, just push warning and continue
      thd_ndb->push_warning("Failed to distribute 'DROP TABLE %s'", table_name);
    }
  }

  if (share) {
    NDB_SHARE::mark_share_dropped_and_release(share, "delete_table");
  }

  return 0;
}

// This function is only used in the special case where a legacy distributed
// privilege table has been altered from NDB to another engine
static void clear_legacy_privilege_table_from_dictionary_cache(
    Ndb *ndb, const char *db, const char *table_name) {
  Ndb_table_guard ndb_tab_g(ndb, db, table_name);
  const NdbDictionary::Table *tab = ndb_tab_g.get_table();
  if (tab) {
    ndb_tab_g.invalidate();

    const NdbDictionary::Dictionary *dict = ndb->getDictionary();
    NdbDictionary::Dictionary::List index_list;
    dict->listIndexes(index_list, *tab);
    for (unsigned i = 0; i < index_list.count; i++) {
      const NdbDictionary::Index *index =
          dict->getIndexGlobal(index_list.elements[i].name, *tab);
      dict->removeIndexGlobal(*index, 1 /*invalidate=true*/);
    }
  }
}

int ha_ndbcluster::delete_table(const char *path, const dd::Table *) {
  THD *thd = current_thd;

  DBUG_TRACE;
  DBUG_PRINT("enter", ("path: %s", path));

  // Never called on an open handler
  assert(m_table == nullptr);

  char dbname[FN_HEADLEN];
  char tabname[FN_HEADLEN];
  ndb_set_dbname(path, dbname);
  ndb_set_tabname(path, tabname);

  if (check_ndb_connection(thd)) {
    return HA_ERR_NO_CONNECTION;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  // Set database name to use while dropping table from NDB
  const Ndb_dbname_guard dbname_guard(thd_ndb->ndb, dbname);
  if (dbname_guard.change_database_failed()) {
    thd_ndb->push_ndb_error_warning(thd_ndb->ndb->getNdbError());
    return HA_ERR_NO_CONNECTION;
  }

  if (ndb_name_is_temp(tabname)) {
    const char *orig_table_name =
        thd->lex->query_block->get_table_list()->table_name;
    if (thd_sql_command(thd) == SQLCOM_ALTER_TABLE &&
        Ndb_dist_priv_util::is_privilege_table(dbname, orig_table_name)) {
      ndb_log_info("Migrating legacy privilege table: Drop %s (%s)",
                   orig_table_name, tabname);
      // Special case allowing the legacy distributed privilege tables
      // to be migrated to local shadow tables. Do not drop the table from
      // NdbDictionary or publish this change via schema distribution.
      // Mark the share as dropped, then clear the table from the dictionary
      // cache.
      NDB_SHARE *share =
          NDB_SHARE::acquire_reference(dbname, tabname, "delete_local_shadow");
      NDB_SHARE::mark_share_dropped_and_release(share, "delete_local_shadow");
      clear_legacy_privilege_table_from_dictionary_cache(thd_ndb->ndb, dbname,
                                                         orig_table_name);
      return 0;
    }

    if (!thd_ndb->has_required_global_schema_lock(
            "ha_ndbcluster::delete_table")) {
      return HA_ERR_NO_CONNECTION;
    }

    /* This the final phase of a copy alter. Delay the drop of the table with
       temp name until after commit so that when required, a rollback would be
       possible. Log it in the ddl_ctx and return. It will be dropped after
       the commit succeeds. */
    assert(thd_sql_command(thd) == SQLCOM_ALTER_TABLE ||
           thd_sql_command(thd) == SQLCOM_DROP_INDEX ||
           thd_sql_command(thd) == SQLCOM_CREATE_INDEX);
    Ndb_DDL_transaction_ctx *ddl_ctx = thd_ndb->get_ddl_transaction_ctx(true);
    ddl_ctx->log_drop_temp_table(dbname, tabname);
    return 0;
  }

  Ndb_schema_dist_client schema_dist_client(thd);
  if (!schema_dist_client.prepare(dbname, tabname)) {
    /* Don't allow delete table unless schema distribution is ready */
    return HA_ERR_NO_CONNECTION;
  }

  /* Drop table in NDB and on the other mysqld(s) */
  const int drop_result =
      drop_table_impl(thd, thd_ndb->ndb, &schema_dist_client, dbname, tabname);
  return drop_result;
}

void ha_ndbcluster::get_auto_increment(ulonglong offset, ulonglong increment,
                                       ulonglong, ulonglong *first_value,
                                       ulonglong *nb_reserved_values) {
  Uint64 auto_value;
  THD *thd = current_thd;
  DBUG_TRACE;
  Ndb *ndb = get_thd_ndb(thd)->ndb;
  uint retries = NDB_AUTO_INCREMENT_RETRIES;
  for (;;) {
    NDB_SHARE::Tuple_id_range_guard g(m_share);
    if ((m_skip_auto_increment &&
         ndb->readAutoIncrementValue(m_table, g.range, auto_value)) ||
        ndb->getAutoIncrementValue(m_table, g.range, auto_value,
                                   Uint32(m_autoincrement_prefetch), increment,
                                   offset)) {
      if (--retries && !thd_killed(thd) &&
          ndb->getNdbError().status == NdbError::TemporaryError) {
        ndb_trans_retry_sleep();
        continue;
      }
      const NdbError err = ndb->getNdbError();
      ndb_log_error("Error %d in ::get_auto_increment(): %s", err.code,
                    err.message);
      *first_value = ~(ulonglong)0;
      return;
    }
    break;
  }
  *first_value = (longlong)auto_value;
  /* From the point of view of MySQL, NDB reserves one row at a time */
  *nb_reserved_values = 1;
}

ha_ndbcluster::ha_ndbcluster(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg),
      m_thd_ndb(nullptr),
      m_active_cursor(nullptr),
      m_ndb_record(nullptr),
      m_ndb_hidden_key_record(nullptr),
      m_key_fields(nullptr),
      m_part_info(nullptr),
      m_user_defined_partitioning(false),
      m_use_partition_pruning(false),
      m_sorted(false),
      m_use_write(false),
      m_ignore_dup_key(false),
      m_ignore_no_key(false),
      m_read_before_write_removal_possible(false),
      m_read_before_write_removal_used(false),
      m_rows_updated(0),
      m_rows_deleted(0),
      m_rows_to_insert((ha_rows)1),
      m_delete_cannot_batch(false),
      m_update_cannot_batch(false),
      m_skip_auto_increment(true),
      m_is_bulk_delete(false),
      m_blobs_row_total_size(0),
      m_dupkey((uint)-1),
      m_autoincrement_prefetch(DEFAULT_AUTO_PREFETCH),
      m_pushed_join_member(nullptr),
      m_pushed_join_operation(-1),
      m_disable_pushed_join(false),
      m_active_query(nullptr),
      m_pushed_operation(nullptr),
      m_cond(this),
      m_multi_cursor(nullptr) {
  DBUG_TRACE;

  stats.records = ~(ha_rows)0;  // uninitialized
  stats.block_size = 1024;
}

ha_ndbcluster::~ha_ndbcluster() {
  DBUG_TRACE;

  // Double check that the share has been released already. It should be
  // acquired in open() and released in close().
  assert(m_share == nullptr);

  // Double check that the NDB table's metadata has been released already. It
  // should be loaded in open() and released in close(). NOTE! The m_table
  // pointer serves as an indicator whether the table is open or closed.
  assert(m_table == nullptr);

  release_blobs_buffer();

  // Check for open cursor/transaction
  assert(m_thd_ndb == nullptr);

  DBUG_PRINT("info", ("Deleting pushed joins"));
  assert(m_active_query == nullptr);
  assert(m_active_cursor == nullptr);
  if (m_pushed_join_operation == PUSHED_ROOT) {
    delete m_pushed_join_member;  // Also delete QueryDef
  }
  m_pushed_join_member = nullptr;
}

/**
  Return extra handler specific text for EXPLAIN.
*/
std::string ha_ndbcluster::explain_extra() const {
  std::string str("");

  const TABLE *const pushed_root = member_of_pushed_join();
  if (pushed_root) {
    if (pushed_root == table) {
      const uint pushed_count = number_of_pushed_joins();
      str += std::string(", activating pushed join of ") +
             std::to_string(pushed_count) + " tables";
    } else {
      str += std::string(", child of ") + parent_of_pushed_join()->alias +
             " in pushed join";
    }
  }

  if (pushed_cond != nullptr) {
    str += ", with pushed condition: " + ItemToString(pushed_cond);
  }
  return str;
}

/**
  @brief Open a table for further use
  - check that table exists in NDB
  - fetch metadata for this table from NDB

  @param path                  Path for table (in filesystem encoded charset).
  @param mode_unused           Unused argument
  @param test_if_locked_unused Unused argument
  @param table_def             Table definition

  @return handler error code, 0 on success.
*/
int ha_ndbcluster::open(const char *path [[maybe_unused]],
                        int mode_unused [[maybe_unused]],
                        uint test_if_locked_unused [[maybe_unused]],
                        const dd::Table *table_def) {
  THD *thd = current_thd;
  DBUG_TRACE;

  const char *dbname = table_share->db.str;
  const char *tabname = table_share->table_name.str;
  DBUG_PRINT("info", ("Opening table '%s.%s'", dbname, tabname));

  if (check_ndb_connection(thd) != 0) {
    return HA_ERR_NO_CONNECTION;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  // Set database name to use while opening table from NDB
  const Ndb_dbname_guard dbname_guard(thd_ndb->ndb, dbname);
  if (dbname_guard.change_database_failed()) {
    thd_ndb->push_ndb_error_warning(thd_ndb->ndb->getNdbError());
    return HA_ERR_NO_CONNECTION;
  }

  if (open_table_set_key_fields()) {
    release_key_fields();
    return HA_ERR_OUT_OF_MEM;
  }

  if (ndb_binlog_is_read_only())
    m_share = open_share_before_schema_sync(thd, dbname, tabname);
  else
    m_share = NDB_SHARE::acquire_for_handler(dbname, tabname, this);

  if (m_share == nullptr) {
    // The NDB_SHARE should have been created by CREATE TABLE, during
    // schema synchronization or by auto discovery. Push warning explaining
    // the problem and return a sensible error.
    thd_ndb->push_warning("Could not open NDB_SHARE for '%s.%s'", dbname,
                          tabname);
    release_key_fields();
    return HA_ERR_NO_CONNECTION;
  }

  // Init table lock structure
  thr_lock_data_init(&m_share->lock, &m_lock, (void *)nullptr);

  int res;
  if ((res = get_metadata(thd_ndb->ndb, dbname, tabname, table_def))) {
    release_key_fields();
    release_ndb_share();
    return res;
  }

  // Read fresh stats from NDB (one roundtrip) and update "constant variables"
  if ((res = update_stats(thd, true)) || (res = info(HA_STATUS_CONST))) {
    release_key_fields();
    release_ndb_share();
    NdbDictionary::Dictionary *const dict = thd_ndb->ndb->getDictionary();
    release_metadata(dict, false);
    return res;
  }

  return 0;
}

/* Set up key-related data structures for open().
   Returns false on success; true on failed memory allocation.
*/
bool ha_ndbcluster::open_table_set_key_fields() {
  KEY *key;
  KEY_PART_INFO *key_part_info;
  uint key_parts, i, j;
  char *bitmap_array;

  if (table_share->primary_key != MAX_KEY) {
    /*
      Setup ref_length to make room for the whole
      primary key to be written in the ref variable
    */
    key = table->key_info + table_share->primary_key;
    ref_length = key->key_length;
  } else {
    if (m_user_defined_partitioning) {
      /* Add space for partid in ref */
      ref_length += sizeof(m_part_id);
    }
  }
  DBUG_PRINT("info", ("ref_length: %d", ref_length));

  uint extra_hidden_keys = table_share->primary_key != MAX_KEY ? 0 : 1;
  uint n_keys = table_share->keys + extra_hidden_keys;
  uint ptr_size = sizeof(MY_BITMAP *) * (n_keys + 1 /* null termination */);
  uint map_size = sizeof(MY_BITMAP) * n_keys;
  m_key_fields = (MY_BITMAP **)my_malloc(PSI_INSTRUMENT_ME, ptr_size + map_size,
                                         MYF(MY_WME + MY_ZEROFILL));
  if (!m_key_fields) return true;  // alloc failed

  bitmap_array = ((char *)m_key_fields) + ptr_size;
  for (i = 0; i < n_keys; i++) {
    const bool is_hidden_key = (i == table_share->keys);
    m_key_fields[i] = (MY_BITMAP *)bitmap_array;
    if (is_hidden_key || (i == table_share->primary_key)) {
      // Primary key, initialize bitmap to use the preallocated buffer
      ndb_bitmap_init(m_key_fields[i], m_pk_bitmap_buf, table_share->fields);
      // Setup pointer to the primary key bitmap
      m_pk_bitmap_p = m_key_fields[i];
    } else {
      // Other key, initialize bitmap with dynamically allocated buffer
      if (bitmap_init(m_key_fields[i], nullptr, table_share->fields)) {
        // Failed to initialize bitmap (only occurs if buffer alloc fails)
        m_key_fields[i] = nullptr;
        return true;
      }
    }
    if (!is_hidden_key) {
      key = table->key_info + i;
      key_part_info = key->key_part;
      key_parts = key->user_defined_key_parts;
      for (j = 0; j < key_parts; j++, key_part_info++)
        bitmap_set_bit(m_key_fields[i], key_part_info->fieldnr - 1);
    } else {
      const uint field_no = table_share->fields;
      // Set bit for hidden key. Use raw bit fiddling since 'field_no' is larger
      // than size of bitmap and thus triggers assert using bitmap_set_bit
      ((uchar *)m_pk_bitmap_buf.buf())[field_no >> 3] |= (1 << (field_no & 7));
    }
    bitmap_array += sizeof(MY_BITMAP);
  }
  m_key_fields[i] = nullptr;
  return false;
}

/* Handle open() before schema distribution is ready and schema synchronization
   has completed. As a rule, the user who wants to use the table will have
   to wait, but there are some exceptions.
*/
NDB_SHARE *ha_ndbcluster::open_share_before_schema_sync(
    THD *thd, const char *dbname, const char *tabname) const {
  /* Migrating distributed privilege tables. Create the NDB_SHARE. It will be
     opened, renamed, and then dropped, as the table is migrated.
  */
  if (Ndb_dist_priv_util::is_privilege_table(dbname, tabname)) {
    return NDB_SHARE::create_for_handler(dbname, tabname, this);
  }

  /* Running CHECK TABLE FOR UPGRADE in a server upgrade thread.
   */
  if (thd->system_thread == SYSTEM_THREAD_SERVER_UPGRADE)
    return NDB_SHARE::create_for_handler(dbname, tabname, this);

  /* User must wait until schema distribution is ready.
   */
  get_thd_ndb(thd)->push_warning(
      "Can't open table '%s.%s' from NDB, schema distribution is not ready",
      dbname, tabname);
  return nullptr;
}

/*
 * Support for OPTIMIZE TABLE
 * reclaims unused space of deleted rows
 * and updates index statistics
 */
int ha_ndbcluster::optimize(THD *thd, HA_CHECK_OPT *) {
  const uint delay = (uint)THDVAR(thd, optimization_delay);

  const int error = ndb_optimize_table(thd, delay);

  // Read fresh stats from NDB (one roundtrip)
  const int stats_error = update_stats(thd, true);

  return (error) ? error : stats_error;
}

int ha_ndbcluster::ndb_optimize_table(THD *thd, uint delay) const {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NDBDICT *dict = ndb->getDictionary();
  int result = 0, error = 0;

  DBUG_TRACE;
  NdbDictionary::OptimizeTableHandle th;
  if ((error = dict->optimizeTable(*m_table, th))) {
    DBUG_PRINT("info", ("Optimze table %s returned error %d",
                        m_table->getName(), error));
    ERR_RETURN(ndb->getNdbError());
  }
  while ((result = th.next()) == 1) {
    if (thd->killed) return -1;
    ndb_milli_sleep(delay);
  }
  if (result == -1 || th.close() == -1) {
    DBUG_PRINT("info",
               ("Optimize table %s did not complete", m_table->getName()));
    ERR_RETURN(ndb->getNdbError());
  }
  for (uint i = 0; i < MAX_KEY; i++) {
    if (thd->killed) return -1;
    if (m_index[i].type != UNDEFINED_INDEX) {
      NdbDictionary::OptimizeIndexHandle ih;
      const NdbDictionary::Index *index = m_index[i].index;
      if (index) {
        if ((error = dict->optimizeIndex(*index, ih))) {
          DBUG_PRINT("info",
                     ("Optimze index %s returned %d", index->getName(), error));
          ERR_RETURN(ndb->getNdbError());
        }
        while ((result = ih.next()) == 1) {
          if (thd->killed) return -1;
          ndb_milli_sleep(delay);
        }
        if (result == -1 || ih.close() == -1) {
          DBUG_PRINT("info",
                     ("Optimize index %s did not complete", index->getName()));
          ERR_RETURN(ndb->getNdbError());
        }
      }

      const NdbDictionary::Index *unique_index = m_index[i].unique_index;
      if (unique_index) {
        if ((error = dict->optimizeIndex(*unique_index, ih))) {
          DBUG_PRINT("info", ("Optimze unique index %s returned %d",
                              unique_index->getName(), error));
          ERR_RETURN(ndb->getNdbError());
        }
        while ((result = ih.next()) == 1) {
          if (thd->killed) return -1;
          ndb_milli_sleep(delay);
        }
        if (result == -1 || ih.close() == -1) {
          DBUG_PRINT("info", ("Optimize index %s did not complete",
                              unique_index->getName()));
          ERR_RETURN(ndb->getNdbError());
        }
      }
    }
  }
  return 0;
}

int ha_ndbcluster::analyze(THD *thd, HA_CHECK_OPT *) {
  DBUG_TRACE;

  // Read fresh stats from NDB (one roundtrip)
  int error = update_stats(thd, true);

  // analyze index if index stat is enabled
  if (error == 0 && THDVAR(nullptr, index_stat_enable) &&
      THDVAR(thd, index_stat_enable)) {
    error = analyze_index();
  }

  // handle any errors
  if (error != 0) {
    // Push the NDB error into stack before returning
    Ndb *ndb = get_thd_ndb(thd)->ndb;
    const NdbError &ndberr = ndb->getNdbError(error);
    my_error(ER_GET_ERRMSG, MYF(0), error, ndberr.message, "NDB");
    return HA_ADMIN_FAILED;
  }
  return 0;
}

int ha_ndbcluster::analyze_index() {
  DBUG_TRACE;

  uint inx_list[MAX_INDEXES];
  uint inx_count = 0;

  for (uint inx = 0; inx < table_share->keys; inx++) {
    NDB_INDEX_TYPE idx_type = get_index_type(inx);

    if ((idx_type == PRIMARY_KEY_ORDERED_INDEX ||
         idx_type == UNIQUE_ORDERED_INDEX || idx_type == ORDERED_INDEX)) {
      if (inx_count < MAX_INDEXES) inx_list[inx_count++] = inx;
    }
  }

  if (inx_count != 0) {
    int err = ndb_index_stat_analyze(inx_list, inx_count);
    if (err != 0) return err;
  }
  return 0;
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

void ha_ndbcluster::set_part_info(partition_info *part_info, bool early) {
  DBUG_TRACE;
  m_part_info = part_info;
  if (!early) {
    m_use_partition_pruning = false;
    if (!(m_part_info->part_type == partition_type::HASH &&
          m_part_info->list_of_part_fields &&
          !m_part_info->is_sub_partitioned())) {
      /*
        PARTITION BY HASH, RANGE and LIST plus all subpartitioning variants
        all use MySQL defined partitioning. PARTITION BY KEY uses NDB native
        partitioning scheme.
      */
      m_use_partition_pruning = true;
      m_user_defined_partitioning = true;
    }
    if (m_part_info->part_type == partition_type::HASH &&
        m_part_info->list_of_part_fields &&
        m_part_info->num_full_part_fields == 0) {
      /*
        CREATE TABLE t (....) ENGINE NDB PARTITION BY KEY();
        where no primary key is defined uses a hidden key as partition field
        and this makes it impossible to use any partition pruning. Partition
        pruning requires partitioning based on real fields, also the lack of
        a primary key means that all accesses to tables are based on either
        full table scans or index scans and they can never be pruned those
        scans given that the hidden key is unknown. In write_row, update_row,
        and delete_row the normal hidden key handling will fix things.
      */
      m_use_partition_pruning = false;
    }
    DBUG_PRINT("info",
               ("m_use_partition_pruning = %d", m_use_partition_pruning));
  }
}

inline void ha_ndbcluster::release_ndb_share() {
  if (m_share) {
    NDB_SHARE::release_for_handler(m_share, this);
    m_share = nullptr;
  }
}

inline void ha_ndbcluster::release_key_fields() {
  if (m_key_fields) {
    MY_BITMAP **inx_bitmap;
    for (inx_bitmap = m_key_fields;
         (inx_bitmap != nullptr) && ((*inx_bitmap) != nullptr); inx_bitmap++) {
      if ((*inx_bitmap)->bitmap == m_pk_bitmap_buf.buf()) {
        // Don't free bitmap when it's using m_pk_bitmap_buf as buffer
        continue;
      }

      bitmap_free(*inx_bitmap);
    }
    my_free(m_key_fields);
    m_key_fields = nullptr;
  }
}

/**
  Close an open ha_ndbcluster instance.

  @note This function is called in several different contexts:
   - By same thread which opened or have used NDB before. In this
     case both THD and Thd_ndb is available.
   - By thread which executes FLUSH TABLES or RESET BINARY LOGS AND GTIDS
     and thus closes all  cached table definitions (and thus the related
     ha_ndbcluster instance).
     In this case only THD is available since thread hasn't used NDB before.
   - By thread handling SIGHUP which closes all cached table definitions. In
     this case there isn't even a THD available (this is intentional).

  @note Since neither THD or Thd_ndb can't be assumed to be available (see
  above explanation) an implementation has been chosen that does not rely on
  any of them.

  @return Function always return 0 and shouldn't really fail. Return code
  from this function is normally not checked by caller.
*/

int ha_ndbcluster::close(void) {
  DBUG_TRACE;

  release_key_fields();
  release_ndb_share();

  // During a FLUSH TABLE or when called from SIGHUP signal handler, the NDB
  // table definitions in the "global dict cache" should be invalidated.
  const THD *thd = current_thd;
  const bool invalidate_dict_cache =
      thd == nullptr /* SIGHUP */ || thd_sql_command(thd) == SQLCOM_FLUSH;

  /*
    Release NDB table and index definitions from global dict cache in NdbApi.

    NOTE! This uses the global Ndb object (g_ndb) acting as a facade for
    calling things which are kind of static "factory functions" for releasing
    resources. Those functions are thread safe and can thus be called
    from any thread. Since using the global Ndb object, care must be taken in
    the future not to call functions which are not thread safe. An alternative
    solution to avoid using the global Ndb object is to store the dict pointer
    in ha_ndbcluster when the table is opened.
  */
  NdbDictionary::Dictionary *const dict_factory = g_ndb->getDictionary();
  release_metadata(dict_factory, invalidate_dict_cache);

  return 0;
}

int ha_ndbcluster::check_ndb_connection(THD *thd) const {
  DBUG_TRACE;
  if (!check_ndb_in_thd(thd, true /* allow_recycle */))
    return HA_ERR_NO_CONNECTION;
  return 0;
}

static int ndbcluster_close_connection(handlerton *, THD *thd) {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  DBUG_TRACE;
  if (thd_ndb) {
    Thd_ndb::release(thd_ndb);
    thd_set_thd_ndb(thd, nullptr);
  }
  return 0;
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
        than zero or not. All failures are mapped to the
        ER_NO_SUCH_TABLE error
*/

static int ndbcluster_discover(handlerton *, THD *thd, const char *db,
                               const char *name, uchar **frmblob,
                               size_t *frmlen) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));

  Ndb *ndb = check_ndb_in_thd(thd);
  if (ndb == nullptr) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                        "Failed to discover table '%s' from NDB, could not "
                        "connect to storage engine",
                        name);
    return 1;
  }
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (thd_ndb->check_option(Thd_ndb::CREATE_UTIL_TABLE)) {
    DBUG_PRINT("exit", ("Simulate that table does not exist in NDB"));
    return 1;
  }

  Ndb_table_guard ndbtab_g(ndb, db, name);
  const NDBTAB *ndbtab = ndbtab_g.get_table();
  if (ndbtab == nullptr) {
    // Could not open the table from NDB
    const NdbError &err = ndbtab_g.getNdbError();
    if (err.code == 709 || err.code == 723) {
      // Got the normal 'No such table existed'
      DBUG_PRINT("info", ("No such table, error: %u", err.code));
      return 1;
    }
    if (err.code == NDB_ERR_CLUSTER_FAILURE) {
      // Cluster failure occurred and it's not really possible to tell if table
      // exists or not. Let caller proceed without any warnings as subsequent
      // attempt to create table in NDB should also fail.
      DBUG_PRINT("info", ("Cluster failure detected"));
      return 1;
    }

    thd_ndb->push_ndb_error_warning(err);
    thd_ndb->push_warning("Failed to discover table '%s' from NDB", name);
    return 1;
  }

  DBUG_PRINT("info", ("Found table '%s'", ndbtab->getName()));

  // Magically detect which context this function is called in by
  // checking which kind of metadata locks are held on the table name.
  if (!thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE, db, name,
                                                    MDL_EXCLUSIVE)) {
    // No exclusive MDL lock, this is ha_check_if_table_exists, just
    // return a dummy frmblob to indicate that table exists
    DBUG_PRINT("info", ("return dummy exists for ha_check_if_table_exists()"));
    *frmlen = 37;
    *frmblob = (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, *frmlen, MYF(0));
    return 0;  // Table exists
  }

  DBUG_PRINT("info", ("table exists, check if it can also be discovered"));

  // 2) Assume that exclusive MDL lock is held on the table at this point
  assert(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE, db, name,
                                                      MDL_EXCLUSIVE));

  // Don't allow discover unless schema distribution is ready and
  // "schema synchronization" have completed(which currently can be
  // checked using ndb_binlog_is_read_only()). The user who wants to use
  // this table simply has to wait
  if (!Ndb_schema_dist::is_ready(thd) || ndb_binlog_is_read_only()) {
    // Can't discover, schema distribution is not ready
    thd_ndb->push_warning(
        "Failed to discover table '%s' from NDB, schema "
        "distribution is not ready",
        name);
    my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
    return 1;
  }

  // Function to install table in DD
  const auto install_in_dd = [](Thd_ndb *thd_ndb,
                                const NdbDictionary::Table *ndbtab,
                                const char *db, const char *name) {
    Uint32 version;
    void *unpacked_data;
    Uint32 unpacked_len;
    if (ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_len) != 0) {
      thd_ndb->push_warning(
          "Failed to discover table '%s' from NDB, could not "
          "get extra metadata",
          name);
      my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
      return 1;
    }

    ndb_log_info("Attempting to install table %s.%s in DD", db, name);

    Ndb_dd_client dd_client(thd_ndb->get_thd());
    if (version == 1) {
      // Upgrade the "old" metadata and install the table into DD,
      // don't use force_overwrite since this function would never
      // have been called unless the table didn't exist
      if (!dd_client.migrate_table(
              db, name, static_cast<const unsigned char *>(unpacked_data),
              unpacked_len, false)) {
        thd_ndb->push_warning(
            "Failed to discover table '%s' from NDB, could "
            "not upgrade table with extra metadata version 1",
            name);
        my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
        free(unpacked_data);
        return 1;
      }
    } else {
      // Assign the unpacked data to sdi_t(which is string data type)
      dd::sdi_t sdi;
      sdi.assign(static_cast<const char *>(unpacked_data), unpacked_len);
      const std::string tablespace_name =
          ndb_table_tablespace_name(thd_ndb->ndb->getDictionary(), ndbtab);
      if (!tablespace_name.empty()) {
        // Acquire IX MDL on tablespace
        if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
          thd_ndb->push_warning(
              "Failed to discover table '%s' from NDB, could "
              "not acquire metadata lock on tablespace '%s'",
              name, tablespace_name.c_str());
          my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
          free(unpacked_data);
          return 1;
        }
      }
      // Install the table into DD, use force_overwrite since this function
      // may be called both for non existent table as well as for metadata
      // version mismatch
      if (!dd_client.install_table(
              db, name, sdi, ndbtab->getObjectId(), ndbtab->getObjectVersion(),
              ndbtab->getPartitionCount(), tablespace_name, true)) {
        // Table existed in NDB but it could not be inserted into DD
        thd_ndb->push_warning(
            "Failed to discover table '%s' from NDB, could "
            "not install table in DD",
            name);
        my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
        free(unpacked_data);
        return 1;
      }
    }

#ifndef NDEBUG
    // Run metadata check except if this is discovery during a DROP TABLE
    if (thd_ndb->sql_command() != SQLCOM_DROP_TABLE) {
      const dd::Table *dd_table;
      assert(dd_client.get_table(db, name, &dd_table) &&
             Ndb_metadata::compare(thd_ndb->get_thd(), thd_ndb->ndb, db, ndbtab,
                                   dd_table));
    }
#endif

    dd_client.commit();
    free(unpacked_data);
    ndb_log_info("Successfully installed table %s.%s in DD", db, name);
    return 0;
  };

  // Since installing table in DD  requires commit it's not allowed to
  // discover while in an active transaction.
  if (thd->in_active_multi_stmt_transaction()) {
    if (thd_ndb->get_applier()) {
      // Special case for replica applier which will rollback transaction,
      // install in DD and return error plus warning to retry.
      trans_rollback_stmt(thd);
      trans_rollback(thd);

      // Install table
      const int ret = install_in_dd(thd_ndb, ndbtab, db, name);
      if (ret != 0) {
        ndbtab_g.invalidate();
        return ret;
      }
      // Push warning to make applier retry transaction.
      thd_ndb->push_warning(ER_REPLICA_SILENT_RETRY_TRANSACTION,
                            "Transaction rolled back due to discovery, retry");
      my_error(ER_TABLE_DEF_CHANGED, MYF(0), db, name);
      return 1;
    }
    thd_ndb->push_warning(
        "Failed to discover table '%s' from NDB, not allowed in "
        "active transaction",
        name);
    my_error(ER_TABLE_DEF_CHANGED, MYF(0), db, name);
    return 1;
  }

  const int ret = install_in_dd(thd_ndb, ndbtab, db, name);
  if (ret != 0) {
    ndbtab_g.invalidate();
    return ret;
  }

  // Don't return any sdi in order to indicate that table definitions exists
  // and has been installed into DD
  DBUG_PRINT("info", ("no sdi returned for ha_create_table_from_engine() "
                      "since the table definition is already installed"));
  *frmlen = 0;
  *frmblob = nullptr;

  return 0;
}

/**
  Check if a table exists in NDB.
*/
static int ndbcluster_table_exists_in_engine(handlerton *, THD *thd,
                                             const char *db, const char *name) {
  Ndb *ndb;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s  name: %s", db, name));

  if (!(ndb = check_ndb_in_thd(thd))) return HA_ERR_NO_CONNECTION;

  // ignore temporary named tables left behind by copy alter
  // if the temporary named table exists, it will be removed before creating
  if (ndb_name_is_temp(name)) {
    return HA_ERR_NO_SUCH_TABLE;
  }

  const Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (thd_ndb->check_option(Thd_ndb::CREATE_UTIL_TABLE)) {
    DBUG_PRINT("exit", ("Simulate that table does not exist in NDB"));
    return HA_ERR_NO_SUCH_TABLE;
  }

  NDBDICT *dict = ndb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0) {
    const NdbError &ndb_err = dict->getNdbError();
    if (ndb_err.code == NDB_ERR_CLUSTER_FAILURE) {
      // Cluster failure occurred and it's not really possible to tell if table
      // exists or not. Let caller proceed without any warnings as subsequent
      // attempt to create table in NDB should also fail.
      DBUG_PRINT("info", ("Cluster failure detected"));
      return HA_ERR_NO_SUCH_TABLE;
    }
    thd_ndb->push_ndb_error_warning(ndb_err);
    return HA_ERR_NO_SUCH_TABLE;
  }

  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    if (my_strcasecmp(table_alias_charset, elmt.database, db)) continue;
    if (my_strcasecmp(table_alias_charset, elmt.name, name)) continue;
    DBUG_PRINT("info", ("Found table"));
    return HA_ERR_TABLE_EXIST;
  }
  return HA_ERR_NO_SUCH_TABLE;
}

/**
  @brief Drop a database from NDB.

  The function is called when the MySQL Server has already dropped all tables in
  the database, this means there are normally not  much to do except
  double-checking for remaining NDB tables left in the database and drop those.
  One example of when this function would need to drop something is in the rare
  case when the NDB table couldn't be installed in DD.

  There is no particular database object to remove from NDB, rather the
  database exists in NDB as long as there are tables whose name
  contains the database.

  @note Since function is called this late in the process there is not much to
  do in case an error occurs, just continue best effort and push warnings to
  indicate what happened.
*/
static int drop_database_impl(THD *thd,
                              Ndb_schema_dist_client &schema_dist_client,
                              const char *dbname) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s'", dbname));

  if (!check_ndb_in_thd(thd, true /* allow_recycle */))
    return HA_ERR_NO_CONNECTION;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  // List all user tables in NDB
  Ndb *ndb = thd_ndb->ndb;
  NDBDICT *dict = ndb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to list tables in NDB");
    return -1;
  }

  for (uint i = 0; i < list.count; i++) {
    const NdbDictionary::Dictionary::List::Element &el = list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", el.database, el.name));

    if (my_strcasecmp(system_charset_info, el.database, dbname) != 0) {
      // Skip tables that belong to other databases
      continue;
    }

    if (ndb_name_is_blob_prefix(el.name) || ndb_fk_util_is_mock_name(el.name)) {
      // Skip blob part and mock tables, they are dropped when their respective
      // table is dropped.
      continue;
    }

    DBUG_PRINT("info", ("Table '%s' must be dropped", el.name));

    if (drop_table_impl(thd, ndb, &schema_dist_client, dbname, el.name) != 0) {
      // Failed to drop table, the NDB error has already been pushed as warning
      thd_ndb->push_warning("Failed to drop table '%s.%s'", dbname, el.name);
    }
  }

  // Invalidate all table definitions in NdbApi for the dropped database
  dict->invalidateDbGlobal(dbname);
  return 0;
}

static void ndbcluster_drop_database(handlerton *, char *path) {
  THD *thd = current_thd;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("path: '%s'", path));

  char db[FN_REFLEN];
  ndb_set_dbname(path, db);
  Ndb_schema_dist_client schema_dist_client(thd);

  if (!schema_dist_client.prepare(db, "")) {
    /* Don't allow drop database unless schema distribution is ready */
    return;
  }

  if (drop_database_impl(thd, schema_dist_client, db) != 0) {
    return;
  }

  if (!schema_dist_client.drop_db(db)) {
    // NOTE! There is currently no way to report an error from this
    // function, just log an error and proceed
    ndb_log_error("Failed to distribute 'DROP DATABASE %s'", db);
  }
}

/**
  Check if the given table is a system table which is
  supported to store in NDB

*/
static bool is_supported_system_table(const char *, const char *, bool) {
  /*
    It is not currently supported to store any standard system tables
    in NDB.
  */
  return false;
}

Ndb_index_stat_thread ndb_index_stat_thread;
Ndb_metadata_change_monitor ndb_metadata_change_monitor_thread;

//
// Functionality used for delaying MySQL Server startup until
// connection to NDB and setup (of index stat plus binlog) has completed
//
static bool wait_setup_completed(ulong max_wait_seconds) {
  DBUG_TRACE;

  const auto timeout_time =
      std::chrono::steady_clock::now() + std::chrono::seconds(max_wait_seconds);

  while (std::chrono::steady_clock::now() < timeout_time) {
    if (ndb_binlog_is_initialized() &&
        ndb_index_stat_thread.is_setup_complete()) {
      return true;
    }
    ndb_milli_sleep(100);
  }

  // Timer expired
  return false;
}

/*
  Function installed as server hook to be called just before
  connections are allowed. Wait for --ndb-wait-setup= seconds
  for ndbcluster connect to NDB and complete setup.
*/
static int ndb_wait_setup_server_startup(void *) {
  DBUG_TRACE;
  ndbcluster_hton->notify_alter_table = ndbcluster_notify_alter_table;
  ndbcluster_hton->notify_exclusive_mdl = ndbcluster_notify_exclusive_mdl;

  // Signal components that server is started
  ndb_index_stat_thread.set_server_started();
  ndbcluster_binlog_set_server_started();
  ndb_metadata_change_monitor_thread.set_server_started();

  // Wait for connection to NDB and thread(s) setup
  if (wait_setup_completed(opt_ndb_wait_setup) == false) {
    ndb_log_error(
        "Tables not available after %lu seconds. Consider "
        "increasing --ndb-wait-setup value",
        opt_ndb_wait_setup);
  }
  return 0;  // NOTE! return value ignored by caller
}

/*
  Run "ALTER TABLE x ENGINE=INNODB" on all privilege tables stored in NDB.
  The callback context does not provide a THD, so we must create one.
  Returns false on success.
*/
static bool upgrade_migrate_privilege_tables() {
  /*
    Setup THD object
  */
  auto ndb_create_thd = [](void *stackptr) -> THD * {
    THD *thd = new THD;
    thd->thread_stack = reinterpret_cast<char *>(stackptr);
    thd->store_globals();

    thd->init_query_mem_roots();
    thd->set_command(COM_DAEMON);
    thd->security_context()->skip_grants();

    CHARSET_INFO *charset_connection =
        get_charset_by_csname("utf8mb3", MY_CS_PRIMARY, MYF(MY_WME));
    thd->variables.character_set_client = charset_connection;
    thd->variables.character_set_results = charset_connection;
    thd->variables.collation_connection = charset_connection;
    thd->update_charset();

    return thd;
  };

  int stack_base = 0;
  std::unique_ptr<THD> temp_thd(ndb_create_thd(&stack_base));
  Ndb *ndb = check_ndb_in_thd(temp_thd.get());

  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  std::unordered_set<std::string> ndb_tables;
  if (!ndb_get_table_names_in_schema(dict, "mysql", &ndb_tables)) return true;

  Ndb_privilege_upgrade_connection conn(temp_thd.get());
  for (const auto &table_name : ndb_tables)
    if (Ndb_dist_priv_util::is_privilege_table("mysql", table_name.c_str()))
      if (conn.migrate_privilege_table(table_name.c_str())) return true;

  return false;
}

/*
  Function installed as server hook that runs after DD upgrades.
*/
static int ndb_dd_upgrade_hook(void *) {
  if (!ndb_connection_is_ready(g_ndb_cluster_connection,
                               opt_ndb_wait_connected)) {
    ndb_log_error("Timeout waiting for connection to NDB.");
    return 1;
  }

  if (upgrade_migrate_privilege_tables()) {
    ndb_log_error("Failed to migrate privilege tables.");
    return 1;
  }

  return 0;
}

static Ndb_server_hooks ndb_server_hooks;

/**
  Callback handling the notification of ALTER TABLE start and end
  on the given key. The function locks or unlocks the GSL thus
  preventing concurrent modification to any other object in
  the cluster.

  @param thd                Thread context.
  @param mdl_key            MDL key identifying table which is going to be
                            or was ALTERed.
  @param notification       Indicates whether this is pre-ALTER TABLE or
                            post-ALTER TABLE notification.

  @note This is an additional notification that spans the duration
        of the whole ALTER TABLE thus avoiding the need for an expensive
        abort of the ALTER late in the process when upgrade to X
        metadata lock happens.

  @note This callback is called in addition to notify_exclusive_mdl()
        which means that during an ALTER TABLE we will get two different
        calls to take and release GSL.

  @see notify_alter_table() in handler.h
*/

static bool ndbcluster_notify_alter_table(THD *thd,
                                          const MDL_key *mdl_key
                                          [[maybe_unused]],
                                          ha_notification_type notification) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("db: '%s', name: '%s'", mdl_key->db_name(), mdl_key->name()));

  bool victimized;
  bool result;
  do {
    result = ndb_gsl_lock(thd, notification == HA_NOTIFY_PRE_EVENT,
                          false /* record_gsl */, &victimized);
    if (result && thd_killed(thd)) {
      // Failed to acuire GSL and THD is killed -> give up!
      return true;
    }
    if (result && victimized == false) {
      /*
        Failed to acquire GSL and not 'victimzed' -> ignore error to lock GSL
        and let execution continue until one of ha_ndbcluster's DDL functions
        use Thd_ndb::has_required_global_schema_lock() to verify if the GSL is
        taken or not. This allows users to work with non NDB objects although
        failure to lock GSL occurs(for example because connection to NDB is
        not available).
      */
      return false;
    }
  } while (victimized);
  return result;
}

/**
  Callback handling the notification about acquisition or after
  release of exclusive metadata lock on object represented by
  key. The function locks or unlocks the GSL thus preventing
  concurrent modification to any other object in the cluster

  @param thd                Thread context.
  @param mdl_key            MDL key identifying object on which exclusive
                            lock is to be acquired/was released.
  @param notification       Indicates whether this is pre-acquire or
                            post-release notification.
  @param victimized        'true' if locking failed as we were chosen
                            as a victim in order to avoid possible deadlocks.

  @see notify_exclusive_mdl() in handler.h
*/

static bool ndbcluster_notify_exclusive_mdl(THD *thd, const MDL_key *mdl_key,
                                            ha_notification_type notification,
                                            bool *victimized) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("namespace: %u, db: '%s', name: '%s'", mdl_key->mdl_namespace(),
              mdl_key->db_name(), mdl_key->name()));

  // If the MDL being acquired is on a Schema or a Tablespace, record the GSL
  // acquire so that it can be used to detect any possible deadlocks
  const bool record_gsl = mdl_key->mdl_namespace() == MDL_key::TABLESPACE ||
                          mdl_key->mdl_namespace() == MDL_key::SCHEMA;
  const bool result = ndb_gsl_lock(thd, notification == HA_NOTIFY_PRE_EVENT,
                                   record_gsl, victimized);
  if (result && *victimized == false) {
    /*
      Failed to acquire GSL and not 'victimzed' -> ignore error to lock GSL
      and let execution continue until one of ha_ndbcluster's DDL functions
      use Thd_ndb::has_required_global_schema_lock() to verify if the GSL is
      taken or not. This allows users to work with non NDB objects although
      failure to lock GSL occurs(for example because connection to NDB is not
      available).
    */
    return false;
  }

  return result;
}

/**
  Check if types of child and parent columns in foreign key are compatible.

  @param child_column_type  Child column type description.
  @param parent_column_type Parent column type description.

  @return True if types are compatible, False if not.
*/

static bool ndbcluster_check_fk_column_compat(
    const Ha_fk_column_type *child_column_type,
    const Ha_fk_column_type *parent_column_type, bool /* check_charsets */) {
  NDBCOL child_col, parent_col;

  create_ndb_fk_fake_column(child_col, *child_column_type);
  create_ndb_fk_fake_column(parent_col, *parent_column_type);

  return child_col.isBindable(parent_col) != -1;
}

/* Version in composite numerical format */
static Uint32 ndb_version = NDB_VERSION_D;
static MYSQL_SYSVAR_UINT(version,     /* name */
                         ndb_version, /* var */
                         PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY |
                             PLUGIN_VAR_NOPERSIST,
                         "Compile version for ndbcluster",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0,       /* default */
                         0,       /* min */
                         0,       /* max */
                         0        /* block */
);

/* Version in ndb-Y.Y.Y[-status] format */
static char *ndb_version_string = const_cast<char *>(NDB_NDB_VERSION_STRING);
static MYSQL_SYSVAR_STR(version_string,     /* name */
                        ndb_version_string, /* var */
                        PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_NOPERSIST,
                        "Compile version string for ndbcluster",
                        nullptr, /* check func. */
                        nullptr, /* update func. */
                        nullptr  /* default */
);

extern int ndb_dictionary_is_mysqld;

Uint32 recv_thread_num_cpus;
static int ndb_recv_thread_cpu_mask_check_str(const char *str);
static int ndb_recv_thread_cpu_mask_update();
handlerton *ndbcluster_hton;

/*
  Handle failure from ndbcluster_init() by printing error
  message(s) and request the MySQL Server to shutdown.

  NOTE! This is done to avoid the current undefined behaviour which occurs
  when an error return code from plugin's init() function just disables
  the plugin.
*/

static int ndbcluster_init_abort(const char *error) {
  ndb_log_error("%s", error);
  ndb_log_error("Failed to initialize ndbcluster, aborting!");
  ndb_log_error("Use --skip-ndbcluster to start without ndbcluster.");
  // flush all the buffered messages before exiting
  ndb_log_flush_buffered_messages();
  DBUG_EXECUTE("ndbcluster_init_fail1",
               ndb_log_error("ndbcluster_init_abort1"););
  DBUG_EXECUTE("ndbcluster_init_fail2",
               ndb_log_error("ndbcluster_init_abort2"););

  // Terminate things which cause server shutdown hang
  ndbcluster_binlog_end();

  // Release resources which will not be released in other ways
  // (since ndbcluster_end() will not be called)
  ndb_server_hooks.unregister_all();
  Ndb_replica::deinit();

  // Use server service to ask for server shutdown
  Ndb_mysql_services services;
  if (services.request_mysql_server_shutdown()) {
    // The shutdown failed -> abort the server.
    ndb_log_error("Failed to request shutdown, aborting...");
    abort();
  }

  return 1;  // Error
}

/*
  Initialize the ndbcluster storage engine part of the "ndbcluster plugin"

  NOTE! As this is the init() function for a storage engine plugin,
  the function is passed a pointer to the handlerton and not
  the "ndbcluster plugin"
*/

static int ndbcluster_init(void *handlerton_ptr) {
  DBUG_TRACE;
  assert(!ndbcluster_inited);

  handlerton *hton = static_cast<handlerton *>(handlerton_ptr);

  if (unlikely(opt_initialize)) {
    /* Don't schema-distribute 'mysqld --initialize' of data dictionary */
    ndb_log_info("'--initialize' -> ndbcluster plugin disabled");
    hton->state = SHOW_OPTION_DISABLED;
    assert(!ha_storage_engine_is_enabled(hton));
    return 0;  // Return before init will disable ndbcluster-SE.
  }

  /* Check const alignment */
  static_assert(DependencyTracker::InvalidTransactionId ==
                Ndb_binlog_extra_row_info::InvalidTransactionId);

  if (global_system_variables.binlog_format == BINLOG_FORMAT_STMT) {
    /* Set global to mixed - note that this is not the default,
     * but the current global value
     */
    global_system_variables.binlog_format = BINLOG_FORMAT_MIXED;
    ndb_log_info(
        "Changed global value of binlog_format from STATEMENT to MIXED");
  }

  std::function<bool()> start_channel_func = []() -> bool {
    DBUG_EXECUTE_IF("ndb_replica_change_t1_version", {
      //  Change DD version of t1, this forces the applier to reinstall table
      Ndb_dd_client dd_client(current_thd);
      assert(dd_client.change_version_for_table("test", "t1", 37));
    });

    if (!wait_setup_completed(opt_ndb_wait_setup)) {
      ndb_log_error(
          "Replica: Connection to NDB not ready after %lu seconds. "
          "Consider increasing --ndb-wait-setup value",
          opt_ndb_wait_setup);
      // Continue and fail with better error when starting to use NDB
    }
    return true;
  };

  if (Ndb_replica::init(start_channel_func, &g_default_channel_stats)) {
    return ndbcluster_init_abort("Failed to initialize NDB Replica");
  }

  if (ndb_index_stat_thread.init() ||
      DBUG_EVALUATE_IF("ndbcluster_init_fail1", true, false)) {
    return ndbcluster_init_abort("Failed to initialize NDB Index Stat");
  }

  if (ndb_metadata_change_monitor_thread.init()) {
    return ndbcluster_init_abort(
        "Failed to initialize NDB Metadata Change Monitor");
  }

  ndb_dictionary_is_mysqld = 1;

  ndbcluster_hton = hton;
  hton->state = SHOW_OPTION_YES;
  hton->db_type = DB_TYPE_NDBCLUSTER;
  hton->close_connection = ndbcluster_close_connection;
  hton->commit = ndbcluster_commit;
  hton->rollback = ndbcluster_rollback;
  hton->create = ndbcluster_create_handler;         /* Create a new handler */
  hton->drop_database = ndbcluster_drop_database;   /* Drop a database */
  hton->panic = ndbcluster_end;                     /* Panic call */
  hton->show_status = ndbcluster_show_status;       /* Show status */
  hton->get_tablespace = ndbcluster_get_tablespace; /* Get ts for old ver */
  hton->alter_tablespace =
      ndbcluster_alter_tablespace; /* Tablespace and logfile group */
  hton->get_tablespace_statistics =
      ndbcluster_get_tablespace_statistics;           /* Provide data to I_S */
  hton->partition_flags = ndbcluster_partition_flags; /* Partition flags */
  if (!ndbcluster_binlog_init(hton))
    return ndbcluster_init_abort("Failed to initialize NDB Binlog");
  hton->flags = HTON_TEMPORARY_NOT_SUPPORTED | HTON_NO_BINLOG_ROW_OPT |
                HTON_SUPPORTS_FOREIGN_KEYS | HTON_SUPPORTS_ATOMIC_DDL;
  hton->discover = ndbcluster_discover;
  hton->table_exists_in_engine = ndbcluster_table_exists_in_engine;
  hton->push_to_engine = ndbcluster_push_to_engine;
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

  hton->foreign_keys_flags = HTON_FKS_WITH_SUPPORTING_HASH_KEYS |
                             HTON_FKS_WITH_ANY_PREFIX_SUPPORTING_KEYS;

  hton->check_fk_column_compat = ndbcluster_check_fk_column_compat;
  hton->pre_dd_shutdown = ndbcluster_pre_dd_shutdown;

  // notify_alter_table and notify_exclusive_mdl will be registered latter
  // SO, that GSL will not be held unnecessary for non-NDB tables.
  hton->post_ddl = ndbcluster_post_ddl;

  // Initialize NdbApi
  ndb_init_internal(1);

  if (!ndb_server_hooks.register_server_hooks(ndb_wait_setup_server_startup,
                                              ndb_dd_upgrade_hook)) {
    return ndbcluster_init_abort("Failed to register server start hook");
  }

  // Initialize NDB_SHARE factory
  NDB_SHARE::initialize(table_alias_charset);

  /* allocate connection resources and connect to cluster */
  const uint global_opti_node_select =
      THDVAR(nullptr, optimized_node_selection);
  if (ndbcluster_connect(
          opt_ndb_wait_connected, opt_ndb_cluster_connection_pool,
          opt_connection_pool_nodeids_str, (global_opti_node_select & 1),
          opt_ndb_connectstring, opt_ndb_nodeid,
          opt_ndb_recv_thread_activation_threshold,
          opt_ndb_data_node_neighbour)) {
    return ndbcluster_init_abort("Failed to initialize connection(s)");
  }

  /* Translate recv thread cpu mask if set */
  if (ndb_recv_thread_cpu_mask_check_str(opt_ndb_recv_thread_cpu_mask) == 0) {
    if (recv_thread_num_cpus) {
      if (ndb_recv_thread_cpu_mask_update()) {
        return ndbcluster_init_abort(
            "Failed to lock receive thread(s) to CPU(s)");
      }
    }
  }

  /* start the ndb injector thread */
  if (ndbcluster_binlog_start()) {
    return ndbcluster_init_abort("Failed to start NDB Binlog");
  }

  // Create index statistics thread
  if (ndb_index_stat_thread.start() ||
      DBUG_EVALUATE_IF("ndbcluster_init_fail2", true, false)) {
    return ndbcluster_init_abort("Failed to start NDB Index Stat");
  }

  // Create metadata change monitor thread
  if (ndb_metadata_change_monitor_thread.start()) {
    return ndbcluster_init_abort("Failed to start NDB Metadata Change Monitor");
  }

  if (ndb_pfs_init()) {
    return ndbcluster_init_abort("Failed to init pfs");
  }

  // Mysql client not available. So, pusing the warning to log file
  if (opt_ndb_slave_conflict_role != SCR_NONE) {
    push_deprecated_warn(nullptr, "ndb_slave_conflict_role",
                         "ndb_applier_conflict_role");
  }

  /*
    If user sets both deprecated and new variable use
    the value in new variable.
  */
  if (opt_ndb_applier_conflict_role != SCR_NONE) {
    /*
      new variable opt_ndb_applier_conflict_role is introduced only to identify
      and report above deprecated warning during startup. And, the plugin code
      only uses opt_ndb_slave_conflict_role internally. So, when ever
      opt_ndb_applier_conflict_role is updated, opt_ndb_slave_conflict_role
      needs to be set to the same value.
    */
    opt_ndb_slave_conflict_role = opt_ndb_applier_conflict_role;
  }

  ndbcluster_inited = 1;

  return 0;  // OK
}

static int ndbcluster_end(handlerton *, ha_panic_function) {
  DBUG_TRACE;

  // Unregister all server hooks
  ndb_server_hooks.unregister_all();
  Ndb_replica::deinit();

  if (!ndbcluster_inited) return 0;
  ndbcluster_inited = 0;

  // Stop threads started by ndbcluster_init() except the
  // ndb_metadata_change_monitor_thread. This is stopped and deinited in the
  // ndbcluster_pre_dd_shutdown() function
  ndb_index_stat_thread.stop();
  ndbcluster_binlog_end();

  NDB_SHARE::deinitialize();

  ndb_index_stat_end();
  ndbcluster_disconnect();

  ndb_index_stat_thread.deinit();

  ndb_pfs_deinit();

  // Cleanup NdbApi
  ndb_end_internal(1);

  return 0;
}

/*
  Deinitialize the ndbcluster storage engine part of the "ndbcluster plugin"

  NOTE! As this is the deinit() function for a storage engine plugin,
  the function is passed a pointer to the handlerton and not
  the "ndbcluster plugin"
*/

static int ndbcluster_deinit(void *) { return 0; }

void ha_ndbcluster::print_error(int error, myf errflag) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));

  if (error == HA_ERR_GENERIC) {
    // This error code is used to indicate that the error already has been
    // handled and reported in other parts of ha_ndbcluster, thus it can be
    // safely ignored here. NOTE! HA_ERR_GENERIC is not used elsewhere in
    // ha_ndbcluster and should not be used for any other purpose in the future

    // Verify that error has been reported already
    assert(current_thd->get_stmt_da()->is_error());

    return;
  }

  if (error == HA_ERR_NO_PARTITION_FOUND) {
    m_part_info->print_no_partition_found(current_thd, table);
    return;
  }

  if (error == HA_ERR_NO_CONNECTION) {
    if (current_thd->get_stmt_da()->is_error()) {
      // Error has been printed already
      return;
    }
    handler::print_error(NDB_ERR_CLUSTER_FAILURE, errflag);
    return;
  }

  handler::print_error(error, errflag);
}

/* Determine roughly how many records are in the range specified */
ha_rows ha_ndbcluster::records_in_range(uint inx, key_range *min_key,
                                        key_range *max_key) {
  const KEY *const key_info = table->key_info + inx;
  const uint key_length = key_info->key_length;
  const NDB_INDEX_TYPE idx_type = get_index_type(inx);

  DBUG_TRACE;

  if (idx_type == UNDEFINED_INDEX) return HA_POS_ERROR;  // index is offline

  if (key_info->flags & HA_NOSAME) {  // 0)
    // Is a potential single row lookup operation.
    assert(idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX ||
           idx_type == UNIQUE_ORDERED_INDEX ||
           idx_type == PRIMARY_KEY_ORDERED_INDEX);
    /**
     * Read from PRIMARY or UNIQUE index with full key
     * will return (at most) a single record, iff:
     * 0) Index has flag NOSAME (-> A unique key)
     * 1) Both min and max keys are fully specified.
     * 2) Min and max keys are equal.
     * 3) There are no NULL values in key (NULLs are not unique)
     */
    if ((min_key && min_key->length == key_length) &&  // 1a)
        (max_key && max_key->length == key_length) &&  // 1b)
        (min_key->key == max_key->key ||               // 2)
         memcmp(min_key->key, max_key->key, key_length) == 0) &&
        !check_null_in_key(key_info, min_key->key, key_length))  // 3)
      return 1;

    // Prevent partial read of hash indexes by returning HA_POS_ERROR
    if (idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX)
      return HA_POS_ERROR;
  }
  // An UNIQUE_INDEX or PRIMARY_KEY_INDEX would have completed above
  assert(idx_type == PRIMARY_KEY_ORDERED_INDEX ||
         idx_type == UNIQUE_ORDERED_INDEX || idx_type == ORDERED_INDEX);
  {
    THD *thd = current_thd;
    const bool index_stat_enable =
        ndb_index_stat_get_enable(nullptr) && ndb_index_stat_get_enable(thd);

    if (index_stat_enable) {
      ha_rows rows = HA_POS_ERROR;
      int err = ndb_index_stat_get_rir(inx, min_key, max_key, &rows);
      if (err == 0) {
        /**
         * optmizer thinks that all values < 2 are exact...but
         * but we don't provide exact statistics
         */
        if (rows < 2) rows = 2;
        return rows;
      }
      if (err != 0 &&
          /* no stats is not unexpected error */
          err != NdbIndexStat::NoIndexStats &&
          /* warning was printed at first error */
          err != NdbIndexStat::MyHasError &&
          /* stats thread aborted request */
          err != NdbIndexStat::MyAbortReq) {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANT_GET_STAT, /* pun? */
                            "index stats (RIR) for key %s:"
                            " unexpected error %d",
                            key_info->name, err);
      }
      /*fall through*/
    }
  }

  /* Use simple heuristics to estimate fraction
     of 'stats.record' returned from range.
  */
  do {
    if (stats.records == ~(ha_rows)0 || stats.records == 0) {
      // Read fresh stats from NDB (one roundtrip) only if 'use_exact_count'
      THD *thd = current_thd;
      if (update_stats(thd, THDVAR(thd, use_exact_count))) break;
    }

    Uint64 rows;
    Uint64 table_rows = stats.records;
    size_t eq_bound_len = 0;
    size_t min_key_length = (min_key) ? min_key->length : 0;
    size_t max_key_length = (max_key) ? max_key->length : 0;

    // Might have an closed/open range bound:
    // Low range open
    if (!min_key_length) {
      rows = (!max_key_length)
                 ? table_rows        // No range was specified
                 : table_rows / 10;  // -oo .. <high range> -> 10% selectivity
    }
    // High range open
    else if (!max_key_length) {
      rows = table_rows / 10;  // <low range>..oo -> 10% selectivity
    } else {
      size_t bounds_len = std::min(min_key_length, max_key_length);
      uint eq_bound_len = 0;
      uint eq_bound_offs = 0;

      KEY_PART_INFO *key_part = key_info->key_part;
      KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
      for (; key_part != end; key_part++) {
        uint part_length = key_part->store_length;
        if (eq_bound_offs + part_length > bounds_len ||
            memcmp(&min_key->key[eq_bound_offs], &max_key->key[eq_bound_offs],
                   part_length)) {
          break;
        }
        eq_bound_len += key_part->length;
        eq_bound_offs += part_length;
      }

      if (!eq_bound_len) {
        rows = table_rows / 20;  // <low range>..<high range> -> 5%
      } else {
        // Has an equality range on a leading part of 'key_length':
        // - Assume reduced selectivity for non-unique indexes
        //   by decreasing 'eq_fraction' by 20%
        // - Assume equal selectivity for all eq_parts in key.

        double eq_fraction = (double)(eq_bound_len) / key_length;
        if (idx_type == ORDERED_INDEX)  // Non-unique index -> less selectivity
          eq_fraction /= 1.20;
        if (eq_fraction >= 1.0)  // Exact match -> 1 row
          return 1;

        rows =
            (Uint64)((double)table_rows / pow((double)table_rows, eq_fraction));
        if (rows > (table_rows / 50))  // EQ-range: Max 2% of rows
          rows = (table_rows / 50);

        if (min_key_length > eq_bound_offs) rows /= 2;
        if (max_key_length > eq_bound_offs) rows /= 2;
      }
    }

    // Make sure that EQ is preferred even if row-count is low
    if (eq_bound_len && rows < 2)  // At least 2 rows as not exact
      rows = 2;
    else if (rows < 3)
      rows = 3;
    return std::min(rows, table_rows);
  } while (0);

  return 10; /* Poor guess when you don't know anything */
}

ulonglong ha_ndbcluster::table_flags(void) const {
  THD *thd = current_thd;
  ulonglong f = HA_NULL_IN_KEY | HA_AUTO_PART_KEY | HA_NO_PREFIX_CHAR_KEYS |
                HA_CAN_GEOMETRY | HA_CAN_BIT_FIELD |
                HA_PRIMARY_KEY_REQUIRED_FOR_POSITION | HA_PARTIAL_COLUMN_READ |
                HA_HAS_OWN_BINLOGGING | HA_BINLOG_ROW_CAPABLE |
                HA_COUNT_ROWS_INSTANT | HA_READ_BEFORE_WRITE_REMOVAL |
                HA_GENERATED_COLUMNS | 0;

  /*
    To allow for logging of NDB tables during stmt based logging;
    flag cabablity, but also turn off flag for OWN_BINLOGGING
  */
  if (thd->variables.binlog_format == BINLOG_FORMAT_STMT)
    f = (f | HA_BINLOG_STMT_CAPABLE) & ~HA_HAS_OWN_BINLOGGING;

  /*
     Allow MySQL Server to decide that STATEMENT logging should be used
     during TRUNCATE TABLE, thus writing the truncate query to the binlog
     in STATEMENT format. Basically this is shortcutting the logic
     in THD::decide_logging_format() to not handle the truncated
     table as a "no_replicate" table.
  */
  if (thd_sql_command(thd) == SQLCOM_TRUNCATE)
    f = (f | HA_BINLOG_STMT_CAPABLE) & ~HA_HAS_OWN_BINLOGGING;

  /**
   * To maximize join pushability we want const-table
   * optimization blocked if 'ndb_join_pushdown= on'
   */
  if (THDVAR(thd, join_pushdown)) f = f | HA_BLOCK_CONST_TABLE;

  return f;
}

const char *ha_ndbcluster::table_type() const { return ("NDBCLUSTER"); }
uint ha_ndbcluster::max_supported_keys() const { return MAX_KEY; }
uint ha_ndbcluster::max_supported_key_parts() const {
  return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY;
}
uint ha_ndbcluster::max_supported_key_length() const {
  return NDB_MAX_KEY_SIZE;
}
uint ha_ndbcluster::max_supported_key_part_length(HA_CREATE_INFO *create_info
                                                  [[maybe_unused]]) const {
  return NDB_MAX_KEY_SIZE;
}
bool ha_ndbcluster::low_byte_first() const {
#ifdef WORDS_BIGENDIAN
  return false;
#else
  return true;
#endif
}

/**
   @brief Update statistics for the open table.

   The function will either use cached table stats from NDB_SHARE or read fresh
   table stats from NDB and update the cache, this is controlled by the
   do_read_stat argument.

   Those table stats will then be used to update the
   Thd_ndb::Trans_table::Stats::records value as well as values in
   handler::stats.

   @param thd           The THD pointer
   @param do_read_stat  Read fresh stats from NDB and update cache.

   @return 0 on success
 */
int ha_ndbcluster::update_stats(THD *thd, bool do_read_stat) {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  DBUG_TRACE;
  DBUG_PRINT("enter", ("read_stat: %d", do_read_stat));

  Ndb_table_stats table_stats;
  if (!do_read_stat) {
    // Just use the cached stats from NDB_SHARE without reading from NDB
    table_stats = m_share->cached_stats.get_table_stats();
  } else {
    // Count number of table stat fetches
    thd_ndb->m_fetch_table_stats++;
    // Count one execute for fetch of stats
    thd_ndb->m_execute_count++;

    // Request stats from NDB
    NdbError ndb_error;
    if (ndb_get_table_statistics(thd, thd_ndb->ndb, m_table, &table_stats,
                                 ndb_error)) {
      if (ndb_error.classification == NdbError::SchemaError) {
        // Updating stats for table failed due to a schema error. Mark the NDB
        // table def as invalid, this will cause also all index defs to be
        // invalidate on close
        m_table->setStatusInvalid();
      }
      return ndb_to_mysql_error(&ndb_error);
    }

    // Update cached stats in NDB_SHARE with fresh data
    m_share->cached_stats.save_table_stats(table_stats);
  }

  int active_rows = 0;  // Active uncommitted rows
  if (m_trans_table_stats) {
    // There is an active statement or transaction

    // Use also the uncommitted rows when updating stats.records further down
    active_rows = m_trans_table_stats->uncommitted_rows;
    DBUG_PRINT("info", ("active_rows: %d", active_rows));

    // Update "records" for the "active" statement or transaction
    m_trans_table_stats->table_rows = table_stats.row_count;
  }
  // Update values in handler::stats (another "records")
  stats.mean_rec_length = static_cast<ulong>(table_stats.row_size);
  stats.data_file_length = table_stats.fragment_memory;
  stats.records = table_stats.row_count + active_rows;
  stats.max_data_file_length = table_stats.fragment_extent_space;
  stats.delete_length = table_stats.fragment_extent_free_space;

  DBUG_PRINT("exit", ("stats.records: %llu  "
                      "table_stats.row_count: %llu  "
                      "no_uncommitted_rows_count: %d "
                      "table_stats.fragment_extent_space: %llu  "
                      "table_stats.fragment_extent_free_space: %llu",
                      stats.records, table_stats.row_count, active_rows,
                      table_stats.fragment_extent_space,
                      table_stats.fragment_extent_free_space));
  return 0;
}

void ha_ndbcluster::check_read_before_write_removal() {
  DBUG_TRACE;

  /* Must have determined that rbwr is possible */
  assert(m_read_before_write_removal_possible);
  m_read_before_write_removal_used = true;

  /* Can't use on table with hidden primary key */
  assert(table_share->primary_key != MAX_KEY);

  /* Index must be unique */
  DBUG_PRINT("info", ("using index %d", active_index));
  const KEY *key = table->key_info + active_index;
  ndbcluster::ndbrequire((key->flags & HA_NOSAME));
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
enum multi_range_types {
  enum_unique_range,        /// Range converted to key operation
  enum_empty_unique_range,  /// No data found (in key operation)
  enum_ordered_range,       /// Normal ordered index scan range
  enum_skip_range           /// Empty range (eg. partition pruning)
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

static inline ulong multi_range_buffer_size(const HANDLER_BUFFER *buffer) {
  const size_t buf_size = buffer->buffer_end - buffer->buffer;
  assert(buf_size < ULONG_MAX);
  return (ulong)buf_size;
}

/* Return the needed size of the fixed array at start of HANDLER_BUFFER. */
static ulong multi_range_fixed_size(int num_ranges) {
  if (num_ranges > MRR_MAX_RANGES) num_ranges = MRR_MAX_RANGES;
  return num_ranges * sizeof(char *);
}

/* Return max number of ranges so that fixed part will still fit in buffer. */
static int multi_range_max_ranges(int num_ranges, ulong bufsize) {
  if (num_ranges > MRR_MAX_RANGES) num_ranges = MRR_MAX_RANGES;
  if (num_ranges * sizeof(char *) > bufsize)
    num_ranges = bufsize / sizeof(char *);
  return num_ranges;
}

/* Return the size in HANDLER_BUFFER of a variable-sized entry. */
static ulong multi_range_entry_size(bool use_keyop, ulong reclength) {
  /* Space for type byte. */
  ulong len = 1;
  if (use_keyop) len += reclength;
  return len;
}

/*
  Return the maximum size of a variable-sized entry in HANDLER_BUFFER.

  Actual size may depend on key values (whether the actual value can be
  converted to a hash key operation or needs to be done as an ordered index
  scan).
*/
static ulong multi_range_max_entry(NDB_INDEX_TYPE keytype, ulong reclength) {
  return multi_range_entry_size(keytype != ORDERED_INDEX, reclength);
}

static uchar &multi_range_entry_type(uchar *p) { return *p; }

/* Find the start of the next entry in HANDLER_BUFFER. */
static uchar *multi_range_next_entry(uchar *p, ulong reclength) {
  bool use_keyop = multi_range_entry_type(p) < enum_ordered_range;
  return p + multi_range_entry_size(use_keyop, reclength);
}

/* Get pointer to row data (for range converted to key operation). */
static uchar *multi_range_row(uchar *p) {
  assert(multi_range_entry_type(p) == enum_unique_range);
  return p + 1;
}

/* Get and put upper layer custom char *, use memcpy() for unaligned access. */
static char *multi_range_get_custom(HANDLER_BUFFER *buffer, int range_no) {
  assert(range_no < MRR_MAX_RANGES);
  char *res;
  memcpy(&res, buffer->buffer + range_no * sizeof(char *), sizeof(char *));
  return res;
}

static void multi_range_put_custom(HANDLER_BUFFER *buffer, int range_no,
                                   char *custom) {
  assert(range_no < MRR_MAX_RANGES);
  // memcpy() required for unaligned access.
  memcpy(buffer->buffer + range_no * sizeof(char *), &custom, sizeof(char *));
}

/*
  This is used to check if an ordered index scan is needed for a range in
  a multi range read.
  If a scan is not needed, we use a faster primary/unique key operation
  instead.
*/
static bool read_multi_needs_scan(NDB_INDEX_TYPE cur_index_type,
                                  const KEY *key_info, const KEY_MULTI_RANGE *r,
                                  bool is_pushed) {
  if (cur_index_type == ORDERED_INDEX || is_pushed) return true;
  if (cur_index_type == PRIMARY_KEY_INDEX) return false;
  if (cur_index_type == UNIQUE_INDEX) {  // a 'UNIQUE ... USING HASH' index
    // UNIQUE_INDEX is used iff optimizer set HA_MRR_NO_NULL_ENDPOINTS.
    // Assert that there are no NULL values in key as promised.
    assert(!check_null_in_key(key_info, r->start_key.key, r->start_key.length));
    return false;
  }
  assert(cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
         cur_index_type == UNIQUE_ORDERED_INDEX);
  if (r->start_key.length != key_info->key_length ||
      r->start_key.flag != HA_READ_KEY_EXACT)
    return true;  // Not exact match, need scan
  if (cur_index_type == UNIQUE_ORDERED_INDEX &&
      check_null_in_key(key_info, r->start_key.key, r->start_key.length))
    return true;  // Can't use for NULL values
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

ha_rows ha_ndbcluster::multi_range_read_info_const(
    uint keyno, RANGE_SEQ_IF *seq, void *seq_init_param, uint n_ranges,
    uint *bufsz, uint *flags, bool *force_default_mrr, Cost_estimate *cost) {
  ha_rows rows;
  uint def_flags = *flags;
  uint def_bufsz = *bufsz;

  DBUG_TRACE;

  /* Get cost/flags/mem_usage of default MRR implementation */
  rows = handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                              n_ranges, &def_bufsz, &def_flags,
                                              force_default_mrr, cost);
  if (unlikely(rows == HA_POS_ERROR)) {
    return rows;
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is
    an order to use the default MRR implementation.
    Also, if multi_range_read_info_const() detected that "DS_MRR" cannot
    be used (E.g. Using a multi-valued index for non-equality ranges), we
    are mandated to use the default implementation. Else, make a choice
    based on requested *flags, handler capabilities, cost and mrr* flags
    of @@optimizer_switch.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) || *force_default_mrr ||
      choose_mrr_impl(keyno, n_ranges, rows, bufsz, flags, cost)) {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags = def_flags;
    *bufsz = def_bufsz;
    assert(*flags & HA_MRR_USE_DEFAULT_IMPL);
  } else {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("NDB-MRR implementation choosen"));
    assert(!(*flags & HA_MRR_USE_DEFAULT_IMPL));
  }
  return rows;
}

/*
  Get cost and other information about MRR scan over some sequence of ranges

  SYNOPSIS
    See handler::multi_range_read_info.
*/

ha_rows ha_ndbcluster::multi_range_read_info(uint keyno, uint n_ranges,
                                             uint n_rows, uint *bufsz,
                                             uint *flags, Cost_estimate *cost) {
  ha_rows res;
  uint def_flags = *flags;
  uint def_bufsz = *bufsz;

  DBUG_TRACE;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res = handler::multi_range_read_info(keyno, n_ranges, n_rows, &def_bufsz,
                                       &def_flags, cost);
  if (unlikely(res == HA_POS_ERROR)) {
    /* Default implementation can't perform MRR scan => we can't either */
    return res;
  }
  assert(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, n_ranges, n_rows, bufsz, flags, cost)) {
    /* Default implementation is chosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags = def_flags;
    *bufsz = def_bufsz;
    assert(*flags & HA_MRR_USE_DEFAULT_IMPL);
  } else {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("NDB-MRR implementation choosen"));
    assert(!(*flags & HA_MRR_USE_DEFAULT_IMPL));
  }
  return res;
}

/**
  Internals: Choose between Default MRR implementation and
                    native ha_ndbcluster MRR

  Make the choice between using Default MRR implementation and
  ha_ndbcluster-MRR. This function contains common functionality factored out of
  multi_range_read_info() and multi_range_read_info_const(). The function
  assumes that the default MRR implementation's applicability requirements are
  satisfied.

  @param keyno       Index number
  @param n_ranges    Number of ranges/keys (i.e. intervals) in the range
  sequence.
  @param n_rows      E(full rows to be retrieved)
  @param bufsz  OUT  If DS-MRR is chosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is chosen, flags of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is chosen, cost of DS-MRR scan
                     else the value is not modified

  @retval true   Default MRR implementation should be used
  @retval false  NDB-MRR implementation should be used
*/

bool ha_ndbcluster::choose_mrr_impl(uint keyno, uint n_ranges, ha_rows n_rows,
                                    uint *bufsz, uint *flags,
                                    Cost_estimate *cost [[maybe_unused]]) {
  THD *thd = current_thd;
  NDB_INDEX_TYPE key_type = get_index_type(keyno);

  get_read_set(true, keyno);  // read_set needed for uses_blob_value()

  /* Disable MRR on blob read and on NULL lookup in unique index. */
  if (!thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR) ||
      uses_blob_value(table->read_set) ||
      (key_type == UNIQUE_INDEX && has_null_in_unique_index(keyno) &&
       !(*flags & HA_MRR_NO_NULL_ENDPOINTS))) {
    /* Use the default implementation, don't modify args: See comments  */
    return true;
  }

  /**
   * Calculate *bufsz, fallback to default MRR if we can't allocate
   * suffient buffer space for NDB-MRR
   */
  {
    uint save_bufsize = *bufsz;
    ulong reclength = table_share->reclength;
    uint entry_size = multi_range_max_entry(key_type, reclength);
    uint min_total_size = entry_size + multi_range_fixed_size(1);
    DBUG_PRINT("info", ("MRR bufsize suggested=%u want=%u limit=%d",
                        save_bufsize, (uint)(n_rows + 1) * entry_size,
                        (*flags & HA_MRR_LIMITS) != 0));
    if (save_bufsize < min_total_size) {
      if (*flags & HA_MRR_LIMITS) {
        /* Too small buffer limit for native NDB-MRR. */
        return true;
      }
      *bufsz = min_total_size;
    } else {
      uint max_ranges = (n_ranges > 0) ? n_ranges : MRR_MAX_RANGES;
      *bufsz = std::min(
          save_bufsize,
          (uint)(n_rows * entry_size + multi_range_fixed_size(max_ranges)));
    }
    DBUG_PRINT("info", ("MRR bufsize set to %u", *bufsz));
  }

  /**
   * Cost based MRR optimization is known to be incorrect.
   * Disabled -> always use NDB-MRR whenever possible
   */
  *flags &= ~HA_MRR_USE_DEFAULT_IMPL;
  *flags |= HA_MRR_SUPPORT_SORTED;

  return false;
}

int ha_ndbcluster::multi_range_read_init(RANGE_SEQ_IF *seq_funcs,
                                         void *seq_init_param, uint n_ranges,
                                         uint mode, HANDLER_BUFFER *buffer) {
  DBUG_TRACE;

  /*
    If supplied buffer is smaller than needed for just one range, we cannot do
    multi_range_read.
  */
  const ulong bufsize = multi_range_buffer_size(buffer);

  if (mode & HA_MRR_USE_DEFAULT_IMPL ||
      bufsize < multi_range_fixed_size(1) +
                    multi_range_max_entry(get_index_type(active_index),
                                          table_share->reclength) ||
      (m_pushed_join_operation == PUSHED_ROOT && !m_disable_pushed_join &&
       !m_pushed_join_member->get_query_def().isScanQuery()) ||
      m_delete_cannot_batch || m_update_cannot_batch) {
    m_disable_multi_read = true;
    return handler::multi_range_read_init(seq_funcs, seq_init_param, n_ranges,
                                          mode, buffer);
  }

  /**
   * There may still be an open m_multi_cursor from the previous mrr access on
   * this handler. Close it now to free up resources for this NdbScanOperation.
   */
  if (int error = close_scan()) {
    return error;
  }

  m_disable_multi_read = false;

  mrr_is_output_sorted = (mode & HA_MRR_SORTED);
  /*
    Copy arguments into member variables
  */
  multi_range_buffer = buffer;
  mrr_funcs = *seq_funcs;
  mrr_iter = mrr_funcs.init(seq_init_param, n_ranges, mode);
  ranges_in_seq = n_ranges;
  m_range_res = mrr_funcs.next(mrr_iter, &mrr_cur_range);
  const bool mrr_need_range_assoc = !(mode & HA_MRR_NO_ASSOCIATION);
  if (mrr_need_range_assoc) {
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
  first_running_range = 0;
  first_unstarted_range = 0;

  return 0;
}

int ha_ndbcluster::multi_range_start_retrievals(uint starting_range) {
  KEY *key_info = table->key_info + active_index;
  ulong reclength = table_share->reclength;
  const NdbOperation *op;
  NDB_INDEX_TYPE cur_index_type = get_index_type(active_index);
  const NdbOperation *oplist[MRR_MAX_RANGES];
  uint num_keyops = 0;
  NdbTransaction *trans = m_thd_ndb->trans;
  int error;
  const bool is_pushed =
      check_if_pushable(NdbQueryOperationDef::OrderedIndexScan, active_index);

  DBUG_TRACE;

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

  assert(cur_index_type != UNDEFINED_INDEX);
  assert(m_multi_cursor == nullptr);
  assert(m_active_query == nullptr);

  const NdbOperation::LockMode lm = get_ndb_lock_mode(m_lock.type);
  const uchar *end_of_buffer = multi_range_buffer->buffer_end;

  /*
    Normally we should have sufficient buffer for the whole fixed_sized part.
    But we need to make sure we do not crash if upper layer gave us a _really_
    small buffer.

    We already checked (in multi_range_read_init()) that we got enough buffer
    for at least one range.
  */
  uint min_entry_size =
      multi_range_entry_size(!read_multi_needs_scan(cur_index_type, key_info,
                                                    &mrr_cur_range, is_pushed),
                             reclength);
  const ulong bufsize = multi_range_buffer_size(multi_range_buffer);
  int max_range =
      multi_range_max_ranges(ranges_in_seq, bufsize - min_entry_size);
  assert(max_range > 0);
  uchar *row_buf =
      multi_range_buffer->buffer + multi_range_fixed_size(max_range);
  m_multi_range_result_ptr = row_buf;

  int range_no = 0;
  int mrr_range_no = starting_range;
  bool any_real_read = false;

  if (m_read_before_write_removal_possible) check_read_before_write_removal();

  for (; !m_range_res;
       range_no++, m_range_res = mrr_funcs.next(mrr_iter, &mrr_cur_range)) {
    if (range_no >= max_range) break;
    bool need_scan = read_multi_needs_scan(cur_index_type, key_info,
                                           &mrr_cur_range, is_pushed);
    if (row_buf + multi_range_entry_size(!need_scan, reclength) > end_of_buffer)
      break;
    if (need_scan) {
      if (range_no > NdbIndexScanOperation::MaxRangeNo) break;
      /*
        Check how much KEYINFO data we already used for index bounds, and
        split the MRR here if it exceeds a certain limit. This way we avoid
        overloading the TC block in the ndb kernel.

        The limit used is based on the value MAX_KEY_SIZE_IN_WORDS.
      */
      if (m_multi_cursor && m_multi_cursor->getCurrentKeySize() >= 1000) break;
    }

    mrr_range_no++;
    multi_range_put_custom(multi_range_buffer, range_no, mrr_cur_range.ptr);

    part_id_range part_spec;
    if (m_use_partition_pruning) {
      get_partition_set(table, table->record[0], active_index,
                        &mrr_cur_range.start_key, &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part: %u  part_spec.end_part: %u",
                          part_spec.start_part, part_spec.end_part));
      /*
        If partition pruning has found no partition in set
        we can skip this scan
      */
      if (part_spec.start_part > part_spec.end_part) {
        /*
          We can skip this range since the key won't fit into any
          partition
        */
        multi_range_entry_type(row_buf) = enum_skip_range;
        row_buf = multi_range_next_entry(row_buf, reclength);
        continue;
      }
      if (!trans && (part_spec.start_part == part_spec.end_part))
        if (unlikely(!(trans = start_transaction_part_id(part_spec.start_part,
                                                         error))))
          return error;
    }

    if (need_scan) {
      if (!trans) {
        // ToDo see if we can use start_transaction_key here instead
        if (!m_use_partition_pruning) {
          get_partition_set(table, table->record[0], active_index,
                            &mrr_cur_range.start_key, &part_spec);
          if (part_spec.start_part == part_spec.end_part) {
            if (unlikely(!(trans = start_transaction_part_id(
                               part_spec.start_part, error))))
              return error;
          } else if (unlikely(!(trans = start_transaction(error))))
            return error;
        } else if (unlikely(!(trans = start_transaction(error))))
          return error;
      }

      any_real_read = true;
      DBUG_PRINT("info", ("any_real_read= true"));

      /* Create the scan operation for the first scan range. */
      if (check_if_pushable(NdbQueryOperationDef::OrderedIndexScan,
                            active_index)) {
        assert(!m_read_before_write_removal_used);
        if (!m_active_query) {
          const int error = create_pushed_join();
          if (unlikely(error)) return error;

          NdbQuery *const query = m_active_query;
          if (mrr_is_output_sorted &&
              query->getQueryOperation((uint)PUSHED_ROOT)
                  ->setOrdering(NdbQueryOptions::ScanOrdering_ascending))
            ERR_RETURN(query->getNdbError());
        }
      }  // check_if_pushable()
      else if (!m_multi_cursor) {
        /* Do a multi-range index scan for ranges not done by primary/unique
         * key. */
        NdbScanOperation::ScanOptions options;
        NdbInterpretedCode code(m_table);

        options.optionsPresent = NdbScanOperation::ScanOptions::SO_SCANFLAGS |
                                 NdbScanOperation::ScanOptions::SO_PARALLEL;

        options.scan_flags =
            NdbScanOperation::SF_ReadRangeNo | NdbScanOperation::SF_MultiRange;

        if (lm == NdbOperation::LM_Read)
          options.scan_flags |= NdbScanOperation::SF_KeyInfo;
        if (mrr_is_output_sorted)
          options.scan_flags |= NdbScanOperation::SF_OrderByFull;

        options.parallel = DEFAULT_PARALLELISM;

        NdbOperation::GetValueSpec gets[2];
        if (table_share->primary_key == MAX_KEY)
          get_hidden_fields_scan(&options, gets);

        generate_scan_filter(&code, &options);
        get_read_set(true, active_index);

        /* Define scan */
        NdbIndexScanOperation *scanOp =
            trans->scanIndex(m_index[active_index].ndb_record_key, m_ndb_record,
                             lm, m_table_map->get_column_mask(table->read_set),
                             nullptr, /* All bounds specified below */
                             &options, sizeof(NdbScanOperation::ScanOptions));

        if (!scanOp) ERR_RETURN(trans->getNdbError());

        m_multi_cursor = scanOp;

        /* Can't have blobs in multi range read */
        assert(!uses_blob_value(table->read_set));

        /* We set m_next_row=0 to m that no row was fetched from the scan yet.
         */
        m_next_row = nullptr;
      }

      Ndb::PartitionSpec ndbPartitionSpec;
      const Ndb::PartitionSpec *ndbPartSpecPtr = nullptr;

      /* If this table uses user-defined partitioning, use MySQLD provided
       * partition info as pruning info
       * Otherwise, scan range pruning is performed automatically by
       * NDBAPI based on distribution key values.
       */
      if (m_use_partition_pruning && m_user_defined_partitioning &&
          (part_spec.start_part == part_spec.end_part)) {
        DBUG_PRINT(
            "info",
            ("Range on user-def-partitioned table can be pruned to part %u",
             part_spec.start_part));
        ndbPartitionSpec.type = Ndb::PartitionSpec::PS_USER_DEFINED;
        ndbPartitionSpec.UserDefined.partitionId = part_spec.start_part;
        ndbPartSpecPtr = &ndbPartitionSpec;
      }

      /* Include this range in the ordered index scan. */
      NdbIndexScanOperation::IndexBound bound;
      compute_index_bounds(bound, key_info, &mrr_cur_range.start_key,
                           &mrr_cur_range.end_key, 0);
      bound.range_no = range_no;

      const NdbRecord *key_rec = m_index[active_index].ndb_record_key;
      if (m_active_query) {
        DBUG_PRINT("info", ("setBound:%d, for pushed join", bound.range_no));
        if (m_active_query->setBound(key_rec, &bound)) {
          ERR_RETURN(trans->getNdbError());
        }
      } else {
        if (m_multi_cursor->setBound(
                m_index[active_index].ndb_record_key, bound,
                ndbPartSpecPtr,  // Only for user-def tables
                sizeof(Ndb::PartitionSpec))) {
          ERR_RETURN(trans->getNdbError());
        }
      }

      multi_range_entry_type(row_buf) = enum_ordered_range;
      row_buf = multi_range_next_entry(row_buf, reclength);
    } else {
      multi_range_entry_type(row_buf) = enum_unique_range;

      if (!trans) {
        assert(active_index != MAX_KEY);
        if (unlikely(!(trans = start_transaction_key(
                           active_index, mrr_cur_range.start_key.key, error))))
          return error;
      }

      if (m_read_before_write_removal_used) {
        DBUG_PRINT("info", ("m_read_before_write_removal_used == true"));

        /* Key will later be returned as result record.
         * Save it in 'row_buf' from where it will later retrieved.
         */
        key_restore(multi_range_row(row_buf),
                    pointer_cast<const uchar *>(mrr_cur_range.start_key.key),
                    key_info, key_info->key_length);

        op = nullptr;  // read_before_write_removal
      } else {
        any_real_read = true;
        DBUG_PRINT("info", ("any_real_read= true"));

        /* Convert to primary/unique key operation. */
        Uint32 partitionId;
        Uint32 *ppartitionId = nullptr;

        if (m_user_defined_partitioning &&
            (cur_index_type == PRIMARY_KEY_ORDERED_INDEX ||
             cur_index_type == PRIMARY_KEY_INDEX)) {
          partitionId = part_spec.start_part;
          ppartitionId = &partitionId;
        }

        /**
         * 'Pushable codepath' is incomplete and expected not
         * to be produced as make_join_pushed() handle
         * AT_MULTI_UNIQUE_KEY as non-pushable.
         */
        if (m_pushed_join_operation == PUSHED_ROOT && !m_disable_pushed_join &&
            !m_pushed_join_member->get_query_def().isScanQuery()) {
          op = nullptr;   // Avoid compiler warning
          assert(false);  // FIXME: Incomplete code, should not be executed
          assert(lm == NdbOperation::LM_CommittedRead);
          const int error = pk_unique_index_read_key_pushed(
              active_index, mrr_cur_range.start_key.key);
          if (unlikely(error)) {
            return error;
          }
        } else {
          if (m_pushed_join_operation == PUSHED_ROOT &&
              !m_disable_pushed_join) {
            DBUG_PRINT("info",
                       ("Cannot push join due to incomplete implementation."));
            m_thd_ndb->push_warning(
                "Prepared pushed join could not be executed"
                ", not implemented for UNIQUE KEY 'multi range read'");
            m_thd_ndb->m_pushed_queries_dropped++;
          }
          if (!(op = pk_unique_index_read_key(
                    active_index, mrr_cur_range.start_key.key,
                    multi_range_row(row_buf), lm, ppartitionId)))
            ERR_RETURN(trans->getNdbError());
        }
      }
      oplist[num_keyops++] = op;
      row_buf = multi_range_next_entry(row_buf, reclength);
    }
  }

  if (m_active_query != nullptr &&
      m_pushed_join_member->get_query_def().isScanQuery()) {
    m_thd_ndb->m_scan_count++;
    if (mrr_is_output_sorted) {
      m_thd_ndb->m_sorted_scan_count++;
    }

    bool prunable = false;
    if (unlikely(m_active_query->isPrunable(prunable) != 0))
      ERR_RETURN(m_active_query->getNdbError());
    if (prunable) m_thd_ndb->m_pruned_scan_count++;

    DBUG_PRINT("info",
               ("Is MRR scan-query pruned to 1 partition? :%u", prunable));
    assert(!m_multi_cursor);
  }
  if (m_multi_cursor) {
    DBUG_PRINT("info", ("Is MRR scan pruned to 1 partition? :%u",
                        m_multi_cursor->getPruned()));
    m_thd_ndb->m_scan_count++;
    m_thd_ndb->m_pruned_scan_count += (m_multi_cursor->getPruned() ? 1 : 0);
    if (mrr_is_output_sorted) {
      m_thd_ndb->m_sorted_scan_count++;
    }
  }

  if (any_real_read && execute_no_commit_ie(m_thd_ndb, trans))
    ERR_RETURN(trans->getNdbError());

  if (!m_range_res) {
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
    multi_range_buffer->end_of_used_area = multi_range_buffer->buffer_end;
  } else
    multi_range_buffer->end_of_used_area = row_buf;

  first_running_range = first_range_in_batch = starting_range;
  first_unstarted_range = mrr_range_no;
  m_current_range_no = 0;

  /*
    Now we need to inspect all ranges that were converted to key operations.

    We need to check for any error (in particular NoDataFound), and remember
    the status, since the operation pointer may no longer be valid when we
    actually get to it in multi_range_next_entry() (we may have done further
    execute()'s in a different handler object during joins eg.)
  */
  row_buf = m_multi_range_result_ptr;
  uint op_idx = 0;
  for (uint r = first_range_in_batch; r < first_unstarted_range; r++) {
    uchar &type_loc = multi_range_entry_type(row_buf);
    row_buf = multi_range_next_entry(row_buf, reclength);
    if (type_loc >= enum_ordered_range) continue;

    assert(op_idx < MRR_MAX_RANGES);
    if ((op = oplist[op_idx++]) == nullptr)
      continue;  // read_before_write_removal

    const NdbError &error = op->getNdbError();
    if (error.code != 0) {
      if (error.classification == NdbError::NoDataFound)
        type_loc = enum_empty_unique_range;
      else {
        /*
          Some operation error that did not cause transaction
          rollback, but was unexpected when performing these
          lookups.
          Return error to caller, expecting caller to rollback
          transaction.
        */
        ERR_RETURN(error);
      }
    }
  }

  return 0;
}

int ha_ndbcluster::multi_range_read_next(char **range_info) {
  DBUG_TRACE;

  if (m_disable_multi_read) {
    return handler::multi_range_read_next(range_info);
  }

  for (;;) {
    /* for each range (we should have remembered the number) */
    while (first_running_range < first_unstarted_range) {
      uchar *row_buf = m_multi_range_result_ptr;
      int expected_range_no = first_running_range - first_range_in_batch;

      switch (multi_range_entry_type(row_buf)) {
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
          m_multi_range_result_ptr = multi_range_next_entry(
              m_multi_range_result_ptr, table_share->reclength);

          /*
            Clear m_active_cursor; it is used as a flag in update_row() /
            delete_row() to know whether the current tuple is from a scan
            or pk operation.
          */
          m_active_cursor = nullptr;

          /* Return the record. */
          *range_info =
              multi_range_get_custom(multi_range_buffer, expected_range_no);
          memcpy(table->record[0], multi_range_row(row_buf),
                 table_share->stored_rec_length);

          if (unlikely(!m_cond.check_condition())) {
            continue;  // 'False', move to next range
          }
          if (table->has_gcol()) {
            update_generated_read_fields(table->record[0], table);
          }
          assert(pushed_cond == nullptr ||
                 const_cast<Item *>(pushed_cond)->val_int());
          return 0;

        case enum_ordered_range:
          /* An index scan range. */
          {
            int res;
            if ((res = read_multi_range_fetch_next()) != 0) {
              *range_info =
                  multi_range_get_custom(multi_range_buffer, expected_range_no);
              first_running_range++;
              m_multi_range_result_ptr = multi_range_next_entry(
                  m_multi_range_result_ptr, table_share->reclength);
              return res;
            }
          }
          if (!m_next_row) {
            /*
              The whole scan is done, and the cursor has been closed.
              So nothing more for this range. Move to next.
            */
            break;
          } else {
            int current_range_no = m_current_range_no;
            /*
              For a sorted index scan, we will receive rows in increasing
              range_no order, so we can return ranges in order, pausing when
              range_no indicate that the currently processed range
              (first_running_range) is done.

              But for unsorted scan, we may receive a high range_no from one
              fragment followed by a low range_no from another fragment. So we
              need to process all index scan ranges together.
            */
            if (!mrr_is_output_sorted ||
                expected_range_no == current_range_no) {
              *range_info =
                  multi_range_get_custom(multi_range_buffer, current_range_no);
              /* Copy out data from the new row. */
              const int ignore = unpack_record_and_set_generated_fields(
                  table->record[0], m_next_row);
              /*
                Mark that we have used this row, so we need to fetch a new
                one on the next call.
              */
              m_next_row = nullptr;

              if (unlikely(ignore)) {
                /* Not a valid row, continue with next row */
                break;
              }
              /*
                Set m_active_cursor; it is used as a flag in update_row() /
                delete_row() to know whether the current tuple is from a scan or
                pk operation.
              */
              m_active_cursor = m_multi_cursor;

              assert(pushed_cond == nullptr ||
                     const_cast<Item *>(pushed_cond)->val_int());
              return 0;
            }

            if (current_range_no > expected_range_no) {
              /* Nothing more in scan for this range. Move to next. */
              break;
            }

            /*
              Should not happen. Ranges should be returned from NDB API in
              the order we requested them.
            */
            assert(0);
            break;  // Attempt to carry on
          }

        default:
          assert(0);
      }
      /* At this point the current range is done, proceed to next. */
      first_running_range++;
      m_multi_range_result_ptr = multi_range_next_entry(
          m_multi_range_result_ptr, table_share->reclength);
    }

    if (m_range_res)  // mrr_funcs.next() has consumed all ranges.
      return HA_ERR_END_OF_FILE;

    /*
      Read remaining ranges
    */
    int res;
    if ((res = multi_range_start_retrievals(first_running_range))) return res;

  }  // for(;;)
}

/*
  Fetch next row from the ordered index cursor in multi range scan.

  We keep the next row in m_next_row, and the range_no of the
  next row in m_current_range_no. This is used in sorted index scan
  to correctly interleave rows from primary/unique key operations with
  rows from the scan.
*/
int ha_ndbcluster::read_multi_range_fetch_next() {
  DBUG_TRACE;

  if (m_active_query) {
    DBUG_PRINT("info",
               ("read_multi_range_fetch_next from pushed join, m_next_row:%p",
                m_next_row));
    if (!m_next_row) {
      int res = fetch_next_pushed();
      if (res == NdbQuery::NextResult_gotRow) {
        m_current_range_no = m_active_query->getRangeNo();
      } else if (res == NdbQuery::NextResult_scanComplete) {
        /* We have fetched the last row from the scan. */
        m_active_query->close(false);
        m_active_query = nullptr;
        m_next_row = nullptr;
        return 0;
      } else {
        /* An error. */
        return res;
      }
    }
  } else if (m_multi_cursor) {
    if (!m_next_row) {
      NdbIndexScanOperation *cursor = m_multi_cursor;
      int res = fetch_next(cursor);
      if (res == 0) {
        m_current_range_no = cursor->get_range_no();
      } else if (res == 1) {
        /* We have fetched the last row from the scan. */
        cursor->close(false, true);
        m_active_cursor = nullptr;
        m_multi_cursor = nullptr;
        m_next_row = nullptr;
        return 0;
      } else {
        /* An error. */
        return res;
      }
    }
  }
  return 0;
}

/**
 * Use whatever conditions got pushed to the table, either as part of a
 * pushed join or not. Update the FILTER AccessPath with the remaining
 * conditions not pushed, possibly clearing the entire FILTER condition.
 * In such cases the FILTER AccessPath may later be entirely eliminated.
 */
void accept_pushed_conditions(const TABLE *table, AccessPath *filter) {
  if (table == nullptr) return;
  ha_ndbcluster *const handler = dynamic_cast<ha_ndbcluster *>(table->file);
  if (handler == nullptr) return;

  // Is a NDB table
  const Item *remainder;
  assert(handler->pushed_cond == nullptr);
  if (handler->m_cond.use_cond_push(handler->pushed_cond, remainder) == 0) {
    if (handler->pushed_cond != nullptr) {  // Something was pushed
      assert(filter->filter().condition != nullptr);
      filter->filter().condition = const_cast<Item *>(remainder);

      // To get correct explain output: (Does NOT affect what is executed)
      // Need to set the QEP_TAB condition as well. Note that QEP_TABs
      // are not 'executed' any longer -> affects only explain output.
      // Can be removed when/if the 'traditional' explain is rewritten
      // to not use the QEP_TAB's
      QEP_TAB *qep_tab = table->reginfo.qep_tab;
      if (qep_tab != nullptr) {
        // The Hypergraph-optimizer do not construct QEP_TAB's
        qep_tab->set_condition(const_cast<Item *>(remainder));
        qep_tab->set_condition_optim();
      }
    }
  }
}

/**
 * 'path' is a basic access path, referring 'table'. If it was pushed
 * as a child in a pushed join a 'PushedJoinRefAccessPath' has to be
 * constructed, replacing the original 'path' for this table.
 *
 * Returns the complete table_map for all tables being members of the
 * same pushed join as 'table'.
 */
static void accept_pushed_child_joins(THD *thd, AccessPath *path, TABLE *table,
                                      Index_lookup *ref, bool is_unique) {
  const TABLE *const pushed_join_root = table->file->member_of_pushed_join();
  if (pushed_join_root == nullptr) return;
  if (pushed_join_root == table) return;

  assert(path->type == AccessPath::EQ_REF || path->type == AccessPath::REF);
  assert(is_unique == (path->type == AccessPath::EQ_REF));

  // Is a 'child' in the pushed join. As it receive its result rows
  // as the result of navigating its parents, it need a special
  // AccessPath operation for retrieving the results.
  AccessPath *pushedJoinRef =
      NewPushedJoinRefAccessPath(thd, table, ref,
                                 /*ordered=*/false, is_unique,
                                 /*count_examined_rows=*/true);
  // Keep the rows/cost statistics.
  CopyBasicProperties(*path, pushedJoinRef);
  *path = std::move(*pushedJoinRef);
}

#ifndef NDEBUG
/**
 * Check if any tables within the (sub-)path is a member of a pushed join
 */
static bool has_pushed_members(AccessPath *path, const JOIN *join) {
  bool has_pushed_joins = false;
  auto func = [&has_pushed_joins](AccessPath *subpath, const JOIN *) {
    const TABLE *table = GetBasicTable(subpath);
    if (table != nullptr && table->file->member_of_pushed_join()) {
      has_pushed_joins = true;
      return true;
    }
    return false;  // -> Allow walker to continue
  };
  WalkAccessPaths(path, join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK, func);
  return has_pushed_joins;
}
#endif

/**
 * Check if any tables within the (sub-)path is a member of
 * a pushed join not entirely inside this path.
 * (Check the 'Query_scope')
 */
static bool has_pushed_members_outside_of_branch(AccessPath *path,
                                                 const JOIN *join) {
  table_map branch_map(0);
  table_map pushed_map(0);

  auto func = [&branch_map, &pushed_map](AccessPath *subpath, const JOIN *) {
    const TABLE *table = GetBasicTable(subpath);
    if (table == nullptr) return false;
    if (table->pos_in_table_list == nullptr) return false;

    // Is referring a TABLE: Collect table_map of tables in path,
    // and any pushed joins having table(s) within path.
    const table_map map = table->pos_in_table_list->map();
    branch_map |= map;
    if ((pushed_map & map) == 0) {  // Not already seen
      pushed_map |= table->file->tables_in_pushed_join();
    }
    return false;  // -> Allow walker to continue
  };
  WalkAccessPaths(path, join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK, func);
  return ((pushed_map & ~branch_map) != 0);
}

/**
 * Walk through the AccessPath three, possibly modify it as needed to adapt it
 * to query elements which got pushed down to the NDB engine.
 */
static void fixup_pushed_access_paths(THD *thd, AccessPath *path,
                                      const JOIN *join, AccessPath *filter) {
  /**
   * Define the lambda function which modify the Accesspath to take advantage
   * of whatever we pushed to the NDB engine.
   */
  auto fixupFunc = [thd, filter](AccessPath *subpath, const JOIN *join) {
    /**
     * Note that for most of the cases handled below, we manually handle the
     * child-walk, and 'return true' which will stop the 'upper' callee from
     * walking further down.
     */
    switch (subpath->type) {
      /**
       * Note that REF / EQ_REF are the only TABLE referring AccessPath types
       * which may be pushed as a 'child' in a pushed join. Such 'child' needs
       * changes of their AccessPath done by accept_pushed_child_joins()
       */
      case AccessPath::REF: {
        const auto &param = subpath->ref();
        accept_pushed_conditions(param.table, filter);
        accept_pushed_child_joins(thd, subpath, param.table, param.ref,
                                  /*is_unique=*/false);
        return true;
      }
      case AccessPath::EQ_REF: {
        const auto &param = subpath->eq_ref();
        accept_pushed_conditions(param.table, filter);
        accept_pushed_child_joins(thd, subpath, param.table, param.ref,
                                  /*is_unique=*/true);
        return true;
      }
      /**
       * Other AccessPath types referring a TABLE may have a pushed condition
       * and/or being a root in a set of pushed joins.
       */
      default: {
        const TABLE *table = GetBasicTable(subpath);
        if (table != nullptr) {  // Is referring a TABLE
          accept_pushed_conditions(table, filter);
          // Can not push as a ChildJoin:
          assert(table->file->member_of_pushed_join() == nullptr ||
                 table->file->member_of_pushed_join() == table);
          return true;
        }
        break;
      }
      /**
       * FILTERs may be pushed down to the engine when accessing the TABLE.
       * Possibly eliminating the entire FILTER operation.
       */
      case AccessPath::FILTER: {
        auto &param = subpath->filter();
        fixup_pushed_access_paths(thd, param.child, join, /*filter=*/subpath);

        if (param.condition == nullptr) {
          // Entire FILTER condition was pushed down.
          // Remove the FILTER operation, keep the estimated rows/cost.
          // (Used for explain only, query plan is already decided)
          param.child->set_num_output_rows(subpath->num_output_rows());
          param.child->set_cost(subpath->cost());
          *subpath = std::move(*param.child);
        }
        return true;
      }
      /**
       * HASH_JOINs may gracefully start writing to temporary 'chunks-files',
       * or use 'spill_to_disk' strategy for INNER-joins if the join_buffer
       * is too small. (We can never guarantee that we can avoid this too-small
       * situation, so need to protect against any ill effects of it,
       * or handle it).
       * If written to such temporary files, we need to ensure that entire
       * pushed join is part of what is being written - We can't go back later
       * and re-establish the parent-child relations between parent rows in
       * a temporary file, and child rows still not fetched.
       *
       * 1) If pushed, the inner 'HASH-build' branch need to contain all
       *    tables pushed as part of it. Such that the complete pushed join
       *    is evaluated and written to the chunk files. This is handled by
       *    creating a separate HASH-scope for this branch.
       *    (Just assert it below.)
       * 2) If the outer 'probe' branch of the hash join does not contain all
       *    tables pushed as part of it, we disable 'spill_to_disk' as
       *    described above. We might then need to read the outer part
       *    multiple times.
       */
      case AccessPath::HASH_JOIN: {
        auto &param = subpath->hash_join();
        assert(!has_pushed_members_outside_of_branch(param.inner, join));

        if (has_pushed_members_outside_of_branch(param.outer, join)) {
          // The pushed join is not contained within 'probe'. Only pushed if
          // INNER- or CROSS-joined, and require 'spill_to_disk' to be disabled.
          // Note that this controls only the spill of the probe input.

          // If this table is part of a pushed join query, rows from the
          // dependent child table(s) has to be read while we are positioned
          // on the rows from the pushed ancestors which the child depends on.
          // Thus, we can not allow rows from a 'pushed join' to
          // 'spill_to_disk'.
          param.allow_spill_to_disk = false;
        }
        break;
      }

#ifndef NDEBUG
      // Below, debug only: Assert Query_scope containment.
      // For some operations this is stricter than what was set up by
      // ndb_pushed_builder_ctx::construct(), where only a Join_scope was
      // constructed. (See further below)
      case AccessPath::AGGREGATE: {
        assert(!has_pushed_members_outside_of_branch(subpath->aggregate().child,
                                                     join));
        break;
      }
      case AccessPath::TEMPTABLE_AGGREGATE: {
        assert(!has_pushed_members_outside_of_branch(
            subpath->temptable_aggregate().subquery_path, join));
        break;
      }
      case AccessPath::STREAM: {
        assert(!has_pushed_members_outside_of_branch(subpath->stream().child,
                                                     join));
        break;
      }
      case AccessPath::MATERIALIZE: {
        for (const MaterializePathParameters::Operand &operand :
             subpath->materialize().param->m_operands) {
          AccessPath *subquery = operand.subquery_path;
          assert(!has_pushed_members_outside_of_branch(subquery, join));
        }
        break;
      }
      case AccessPath::WEEDOUT: {
        assert(!has_pushed_members_outside_of_branch(subpath->weedout().child,
                                                     join));
        break;
      }

      /////////////////
      // For asserts below we only constructed a Join_scope in
      // ndb_pushed_builder_ctx::construct(), thus we do allow 'upper'
      // references '..OutsideOfBranch'. Never seen it being taken advantage of
      // though. It seems to always behave as if a Query_scope was constructed.
      // Would like to investigate, and add as testcase, if any of the
      // (too strict) asserts are hit below.
      case AccessPath::SORT: {
        if (has_pushed_members(subpath->sort().child, join)) {
          // No indirect sort: An indirect sort will first build a temporary
          // sorted 'key-set', then use it to fetch the rows one-by-one. We do
          // not want this to be a pushed operation, as that would have
          // retrieved (and wasted) the rows already, effectively fetching them
          // twice.
          assert(subpath->sort().filesort->m_sort_param.using_addon_fields());
        }
        assert(
            !has_pushed_members_outside_of_branch(subpath->sort().child, join));
        break;
      }
#endif
    }
    // AccessPaths not needing attention:
    return false;  // -> Allow walker to continue
  };

  WalkAccessPaths(path, join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
                  fixupFunc);
}

/**
 * Try to find parts of queries which can be pushed down to
 * storage engines for faster execution. This is typically
 * conditions which can filter out result rows on the SE,
 * and/or entire joins between tables.
 *
 * @param  thd         Thread context
 * @param  root_path   The AccessPath for the entire query.
 * @param  join        The JOIN struct built for the main query.
 *
 * @return Possible error code, '0' if no errors.
 */
int ndbcluster_push_to_engine(THD *thd, AccessPath *root_path, JOIN *join) {
  DBUG_TRACE;
  ndb_pushed_builder_ctx pushed_builder(thd, root_path, join);

  /**
   * Investigate what could be pushed down as entire joins first.
   * Note that we also handle condition pushdowns for the tables which
   * are members of pushed join. There are mutual dependencies between
   * such join- and condition pushdowns, so they need to be handled together
   */
  if (THDVAR(thd, join_pushdown)) {  // Enabled 'ndb_join_pushdown'
    const int error = pushed_builder.make_pushed_join();
    if (unlikely(error)) {
      return error;
    }
  }

  /**
   * For those tables not being join-pushed we may still be able to
   * push any conditions on the table. (There are less restrictions on whether
   * a condition could be pushed when not being part of a pushed join.)
   */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)) {
    const uint count = pushed_builder.m_table_count;
    for (uint tab_no = 0; tab_no < count; tab_no++) {
      pushed_table &table = pushed_builder.m_tables[tab_no];
      const Item *cond = table.get_condition();
      if (cond == nullptr) continue;

      handler *const ha = table.get_table()->file;
      if (ha->member_of_pushed_join() != nullptr &&
          ha->member_of_pushed_join() != table.get_table()) {
        // Condition already pushed as part of pushed join child -> skip
        continue;
      }

      ha_ndbcluster *const ndb_handler = dynamic_cast<ha_ndbcluster *>(ha);
      if (ndb_handler == nullptr) continue;

      const enum_access_type jt = table.get_access_type();
      if ((jt == AT_PRIMARY_KEY || jt == AT_UNIQUE_KEY ||
           jt == AT_OTHER) &&  // CONST or SYSTEM
          !ha->member_of_pushed_join()) {
        /*
          It is of limited value to push a condition to a single row
          access method if not member of a pushed join, so we skip cond_push()
          for these. The exception is if we are member of a pushed join, where
          execution of entire join branches may be eliminated. (above)
        */
        continue;
      }

      /*
        In a 'Select' any tables preceding this table, and in its 'query_scope'
        can be referred as a const-table from the pushed conditions.
      */
      table_map const_expr_tables(0);
      if (thd->lex->sql_command == SQLCOM_SELECT) {
        ndb_table_map query_scope = table.get_tables_in_all_query_scopes();
        for (uint i = 0; i < tab_no; i++) {
          if (query_scope.contain(i)) {
            const TABLE *const_table = pushed_builder.m_tables[i].get_table();
            if (const_table != nullptr &&
                const_table->pos_in_table_list != nullptr) {
              const_expr_tables |= const_table->pos_in_table_list->map();
            }
          }
        }
      }
      /* Prepare push of condition to handler, possibly leaving a remainder */
      ndb_handler->m_cond.prep_cond_push(cond, const_expr_tables, table_map(0));
    }
  }
  // Modify the AccessPath structure to reflect pushed execution.
  fixup_pushed_access_paths(thd, root_path, join, /*filter=*/nullptr);
  return 0;
}

/**
 * First level of filtering tables which *maybe* may be part of
 * a pushed query: Returning 'false' will eliminate this table
 * from being a part of a pushed join.
 * A 'reason' for rejecting this table is required if 'false'
 * is returned.
 */
bool ha_ndbcluster::maybe_pushable_join(const char *&reason) const {
  reason = nullptr;
  if (uses_blob_value(table->read_set)) {
    reason = "select list can't contain BLOB columns";
    return false;
  }
  if (m_user_defined_partitioning) {
    reason = "has user defined partioning";
    return false;
  }

  // Pushed operations may not set locks.
  const NdbOperation::LockMode lockMode = get_ndb_lock_mode(m_lock.type);
  switch (lockMode) {
    case NdbOperation::LM_CommittedRead:
      return true;

    case NdbOperation::LM_Read:
    case NdbOperation::LM_Exclusive:
      reason = "lock modes other than 'read committed' not implemented";
      return false;

    default:  // Other lock modes not used by handler.
      assert(false);
      return false;
  }

  return true;
}

/**
 * Check if this table access operation (and a number of succeeding operations)
 * can be pushed to the cluster and executed there. This requires that there
 * is an NdbQueryDefiniton and that it still matches the corresponds to the
 * type of operation that we intend to execute. (The MySQL server will
 * sometimes change its mind and replace a scan with a lookup or vice versa
 * as it works its way into the nested loop join.)
 *
 * @param type This is the operation type that the server want to execute.
 * @param idx  Index used whenever relevant for operation type
 * @return True if the operation may be pushed.
 */
bool ha_ndbcluster::check_if_pushable(int type,  // NdbQueryOperationDef::Type,
                                      uint idx) const {
  if (m_disable_pushed_join) {
    // We will likely re-enable and (try to) execute it later, not just yet
    DBUG_PRINT("info", ("Push disabled (HA_EXTRA_KEYREAD)"));
    return false;
  }
  if (m_pushed_join_operation == PUSHED_ROOT &&
      m_pushed_join_member != nullptr) {
    const char *reason = nullptr;
    // We had prepared a pushed join for this table, can it be executed?
    if (!m_pushed_join_member->match_definition(
            type, (idx < MAX_KEY) ? &m_index[idx] : nullptr, reason)) {
      m_thd_ndb->push_warning("Prepared pushed join could not be executed, %s",
                              reason);
      m_thd_ndb->m_pushed_queries_dropped++;
      return false;
    }
    return true;  // Ok to execute it
  }
  return false;
}

int ha_ndbcluster::create_pushed_join(const NdbQueryParamValue *keyFieldParams,
                                      uint paramCnt) {
  DBUG_TRACE;
  assert(m_pushed_join_member && m_pushed_join_operation == PUSHED_ROOT);

  /**
   * Generate the pushed condition code, keep it in 'm_cond' for later fetch.
   * Note that pushed condition may refer subqueries, which will now be
   * executed in order to include their result row(s) in the pushed condition
   * code. As a subquery execute will also send any other query requests we
   * may have queued up for this NdbTransaction, we need to perform this step
   * before we 'make_query_instance' below - Else that query would have been
   * executed now as well.
   */
  for (uint i = 0; i < m_pushed_join_member->get_operation_count(); i++) {
    const TABLE *const tab = m_pushed_join_member->get_table(i);
    ha_ndbcluster *handler = static_cast<ha_ndbcluster *>(tab->file);
    handler->m_cond.build_cond_push();
  }

  NdbQuery *const query = m_pushed_join_member->make_query_instance(
      m_thd_ndb->trans, keyFieldParams, paramCnt);

  if (unlikely(query == nullptr)) ERR_RETURN(m_thd_ndb->trans->getNdbError());

  // Bind to instantiated NdbQueryOperations.
  for (uint i = 0; i < m_pushed_join_member->get_operation_count(); i++) {
    const TABLE *const tab = m_pushed_join_member->get_table(i);
    ha_ndbcluster *handler = static_cast<ha_ndbcluster *>(tab->file);

    assert(handler->m_pushed_join_operation == (int)i);
    NdbQueryOperation *const op = query->getQueryOperation(i);
    handler->m_pushed_operation = op;
    handler->get_read_set(false, handler->active_index);

    /**
     * Fetch the generated (above) 'pushed condition' code:
     *
     * Note that for lookup_operations there is a hard limit on ~32K
     * on the size of the unfragmented sendSignal() used to send the
     * LQH_KEYREQ from SPJ -> LQH. (SCANFRAGREQ's are fragmented, and thus
     * can be larger). As the generated code is embedded in the LQH_KEYREQ,
     * we have to limit the 'code' size in a lookup_operation.
     *
     * Also see bug#27397802 ENTIRE CLUSTER CRASHES WITH "MESSAGE TOO
     *    BIG IN SENDSIGNAL" WITH BIG IN CLAUSE.
     *
     * There is a limited gain of pushing a condition for a lookup:
     *
     * 1) A lookup operation returns at most a single row, so the gain
     *    of adding a filter is limited.
     * 2) The filter could be big (Large IN-lists...) and have to be
     *    included in the AttrInfo of every LQH_KEYREQ. So the overhead
     *    could exceed the gain.
     *
     * So we allow only 'small' filters (64 words) to be pushed for EQ_REF's.
     */
    const NdbInterpretedCode &code = handler->m_cond.get_interpreter_code();
    const uint codeSize = code.getWordsUsed();
    if (codeSize > 0) {
      const NdbQueryOperationDef::Type type =
          op->getQueryOperationDef().getType();
      const bool isLookup = (type == NdbQueryOperationDef::PrimaryKeyAccess ||
                             type == NdbQueryOperationDef::UniqueIndexAccess);
      if (isLookup && codeSize >= 64) {
        // Too large, not pushed. Let ha_ndbcluster evaluate the condition
        handler->m_cond.set_condition(handler->pushed_cond);
      } else if (op->setInterpretedCode(code)) {
        // Failed to set generated code, let ha_ndbcluster evaluate
        handler->m_cond.set_condition(handler->pushed_cond);
      }
    }

    // Bind to result buffers
    int res = op->setResultRowRef(
        handler->m_ndb_record, handler->_m_next_row,
        handler->m_table_map->get_column_mask(tab->read_set));
    if (unlikely(res)) ERR_RETURN(query->getNdbError());

    // We clear 'm_next_row' to say that no row was fetched from the query yet.
    handler->_m_next_row = nullptr;
  }

  assert(m_active_query == nullptr);
  m_active_query = query;
  m_thd_ndb->m_pushed_queries_executed++;

  return 0;
}

/**
 * Check if this table access operation is part of a pushed join operation
 * which is actively executing.
 */
bool ha_ndbcluster::check_is_pushed() const {
  if (m_pushed_join_member == nullptr) return false;

  handler *root = m_pushed_join_member->get_table(PUSHED_ROOT)->file;
  return (static_cast<ha_ndbcluster *>(root)->m_active_query);
}

uint ha_ndbcluster::number_of_pushed_joins() const {
  if (m_pushed_join_member == nullptr)
    return 0;
  else
    return m_pushed_join_member->get_operation_count();
}

const TABLE *ha_ndbcluster::member_of_pushed_join() const {
  if (m_pushed_join_member == nullptr)
    return nullptr;
  else
    return m_pushed_join_member->get_table(PUSHED_ROOT);
}

const TABLE *ha_ndbcluster::parent_of_pushed_join() const {
  if (m_pushed_join_operation > PUSHED_ROOT) {
    assert(m_pushed_join_member != nullptr);
    uint parent_ix = m_pushed_join_member->get_query_def()
                         .getQueryOperation(m_pushed_join_operation)
                         ->getParentOperation(0)
                         ->getOpNo();
    return m_pushed_join_member->get_table(parent_ix);
  }
  return nullptr;
}

table_map ha_ndbcluster::tables_in_pushed_join() const {
  if (!member_of_pushed_join()) {
    return 0;
  }

  table_map map = 0;
  for (uint i = 0; i < m_pushed_join_member->get_operation_count(); ++i) {
    map |= m_pushed_join_member->get_table(i)->pos_in_table_list->map();
  }
  return map;
}

/*
  Condition pushdown
*/
/**
  Condition pushdown

  Push a condition to ndbcluster storage engine for evaluation
  during table and index scans. The conditions will be cleared
  by calling handler::extra(HA_EXTRA_RESET) or handler::reset().

  The current implementation supports arbitrary AND/OR nested conditions
  with comparisons between columns and constants (including constant
  expressions and function calls) and the following comparison operators:
  =, !=, >, >=, <, <=, "is null", and "is not null".

  If the condition consist of multiple AND/OR'ed 'boolean terms',
  parts of it may be pushed, and other parts will be returned as a
  'remainder condition', which the server has to evaluate.

  handler::pushed_cond will be assigned the (part of) the condition
  which we accepted to be pushed down.

  Note that this handler call has been partly deprecated by
  handlerton::push_to_engine(), which does both join- and
  condition pushdown for the entire query AccessPath.
  The only remaining intended usage for ::cond_push() is simple
  update and delete queries, where the join part is not relevant.

  @param cond          Condition to be pushed down.

  @retval Return the 'remainder' condition, consisting of the AND'ed
          sum of boolean terms which could not be pushed. A nullptr
          is returned if entire condition was supported.
*/
const Item *ha_ndbcluster::cond_push(const Item *cond) {
  DBUG_TRACE;
  assert(pushed_cond == nullptr);
  assert(cond != nullptr);
  DBUG_EXECUTE("where", print_where(ha_thd(), cond, table_share->table_name.str,
                                    QT_ORDINARY););
  m_cond.prep_cond_push(cond, table_map(0), table_map(0));

  const Item *remainder;
  if (unlikely(m_cond.use_cond_push(pushed_cond, remainder) != 0))
    return cond;  // Failed to accept pushed condition, entire 'cond' is
                  // remainder

  return remainder;
}

/*
  Implements the SHOW ENGINE NDB STATUS command.
*/
bool ndbcluster_show_status(handlerton *, THD *thd, stat_print_fn *stat_print,
                            enum ha_stat_type stat_type) {
  char buf[IO_SIZE];
  uint buflen;
  DBUG_TRACE;

  if (stat_type != HA_ENGINE_STATUS) {
    return false;
  }

  Ndb *ndb = check_ndb_in_thd(thd);
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  struct st_ndb_status ns;
  if (ndb)
    update_status_variables(thd_ndb, &ns, thd_ndb->connection);
  else
    update_status_variables(nullptr, &ns, g_ndb_cluster_connection);

  buflen = (uint)snprintf(buf, sizeof(buf),
                          "cluster_node_id=%ld, "
                          "connected_host=%s, "
                          "connected_port=%ld, "
                          "number_of_data_nodes=%ld, "
                          "number_of_ready_data_nodes=%ld, "
                          "connect_count=%ld",
                          ns.cluster_node_id, ns.connected_host,
                          ns.connected_port, ns.number_of_data_nodes,
                          ns.number_of_ready_data_nodes, ns.connect_count);
  if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                 STRING_WITH_LEN("connection"), buf, buflen))
    return true;

  if (ndb) {
    Ndb::Free_list_usage tmp;
    tmp.m_name = nullptr;
    while (ndb->get_free_list_usage(&tmp)) {
      buflen =
          (uint)snprintf(buf, sizeof(buf), "created=%u, free=%u, sizeof=%u",
                         tmp.m_created, tmp.m_free, tmp.m_sizeof);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     tmp.m_name, (uint)strlen(tmp.m_name), buf, buflen))
        return true;
    }
  }

  buflen = (uint)ndbcluster_show_status_binlog(buf, sizeof(buf));
  if (buflen) {
    if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                   STRING_WITH_LEN("binlog"), buf, buflen))
      return true;
  }

  return false;
}

int ha_ndbcluster::get_default_num_partitions(HA_CREATE_INFO *create_info) {
  THD *thd = current_thd;

  if (check_ndb_connection(thd)) {
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return -1;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  ha_rows max_rows, min_rows;
  if (create_info) {
    max_rows = create_info->max_rows;
    min_rows = create_info->min_rows;
  } else {
    max_rows = table_share->max_rows;
    min_rows = table_share->min_rows;
  }
  uint no_fragments =
      get_no_fragments(max_rows >= min_rows ? max_rows : min_rows);
  uint reported_frags;
  adjusted_frag_count(thd_ndb->ndb, no_fragments, reported_frags);
  return reported_frags;
}

uint32 ha_ndbcluster::calculate_key_hash_value(Field **field_array) {
  Uint32 hash_value;
  struct Ndb::Key_part_ptr key_data[MAX_REF_PARTS];
  struct Ndb::Key_part_ptr *key_data_ptr = &key_data[0];
  Uint32 i = 0;
  int ret_val;
  Uint32 tmp[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  void *buf = (void *)&tmp[0];
  DBUG_TRACE;

  do {
    Field *field = *field_array;
    uint len = field->data_length();
    assert(!field->is_real_null());
    if (field->real_type() == MYSQL_TYPE_VARCHAR)
      len += field->get_length_bytes();
    key_data[i].ptr = field->field_ptr();
    key_data[i++].len = len;
  } while (*(++field_array));
  key_data[i].ptr = nullptr;
  if ((ret_val = Ndb::computeHash(&hash_value, m_table, key_data_ptr, buf,
                                  sizeof(tmp)))) {
    DBUG_PRINT("info", ("ret_val = %d", ret_val));
    assert(false);
    abort();
  }
  return m_table->getPartitionId(hash_value);
}

enum ndb_distribution_enum {
  NDB_DISTRIBUTION_KEYHASH = 0,
  NDB_DISTRIBUTION_LINHASH = 1
};
static const char *distribution_names[] = {"KEYHASH", "LINHASH", NullS};
static TYPELIB distribution_typelib = {array_elements(distribution_names) - 1,
                                       "", distribution_names, nullptr};
static ulong opt_ndb_distribution;
static MYSQL_SYSVAR_ENUM(distribution,         /* name */
                         opt_ndb_distribution, /* var */
                         PLUGIN_VAR_RQCMDARG,
                         "Default distribution for new tables in NDB",
                         nullptr,                  /* check func. */
                         nullptr,                  /* update func. */
                         NDB_DISTRIBUTION_KEYHASH, /* default */
                         &distribution_typelib     /* typelib */
);

/**
  Setup auto partitioning scheme for tables that didn't define any
  partitioning. Use PARTITION BY KEY() in this case which translates into
  partition by primary key if a primary key exists and partition by hidden
  key otherwise.

  @param part_info    Partition info struct to setup

*/
void ha_ndbcluster::set_auto_partitions(partition_info *part_info) {
  DBUG_TRACE;
  part_info->list_of_part_fields = true;
  part_info->part_type = partition_type::HASH;
  switch (opt_ndb_distribution) {
    case NDB_DISTRIBUTION_KEYHASH:
      part_info->linear_hash_ind = false;
      break;
    case NDB_DISTRIBUTION_LINHASH:
      part_info->linear_hash_ind = true;
      break;
    default:
      assert(false);
      break;
  }
}

enum row_type ha_ndbcluster::get_partition_row_type(const dd::Table *, uint) {
  return table_share->real_row_type;
}

/*
  Partitioning setup.

  Check how many fragments the user wants defined and which node groups to put
  those into. Create mappings.

*/
static int create_table_set_up_partition_info(partition_info *part_info,
                                              NdbDictionary::Table &ndbtab,
                                              Ndb_table_map &colIdMap) {
  DBUG_TRACE;

  if (part_info->part_type == partition_type::HASH &&
      part_info->list_of_part_fields == true) {
    Field **fields = part_info->part_field_array;

    DBUG_PRINT("info", ("Using HashMapPartition fragmentation type"));
    ndbtab.setFragmentType(NDBTAB::HashMapPartition);

    for (uint i = 0; i < part_info->part_field_list.elements; i++) {
      assert(fields[i]->stored_in_db);
      NDBCOL *col = colIdMap.getColumn(ndbtab, fields[i]->field_index());
      DBUG_PRINT("info", ("setting dist key on %s", col->getName()));
      col->setPartitionKey(true);
    }
  } else {
    auto partition_type_description = [](partition_type pt) {
      switch (pt) {
        case partition_type::RANGE:
          return "PARTITION BY RANGE";
        case partition_type::HASH:
          return "PARTITION BY HASH";
        case partition_type::LIST:
          return "PARTITION BY LIST";
        default:
          assert(false);
          return "PARTITION BY <type>";
      }
    };

    push_warning_printf(
        current_thd, Sql_condition::SL_WARNING,
        ER_WARN_DEPRECATED_ENGINE_SYNTAX_NO_REPLACEMENT,
        ER_THD(current_thd, ER_WARN_DEPRECATED_ENGINE_SYNTAX_NO_REPLACEMENT),
        partition_type_description(part_info->part_type), ndbcluster_hton_name);

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
    if (part_info->part_type == partition_type::RANGE) {
      // Translate and check values for RANGE partitions to NDB format
      const uint parts = part_info->num_parts;
      std::unique_ptr<int32[]> range_data(new (std::nothrow) int32[parts]);
      if (!range_data) {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), parts * sizeof(int32));
        return 1;
      }
      for (uint i = 0; i < parts; i++) {
        longlong range_val = part_info->range_int_array[i];
        if (part_info->part_expr->unsigned_flag)
          range_val -= 0x8000000000000000ULL;
        if (range_val < INT_MIN32 || range_val >= INT_MAX32) {
          if ((i != parts - 1) || (range_val != LLONG_MAX)) {
            my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
            return 1;
          }
          range_val = INT_MAX32;
        }
        range_data[i] = (int32)range_val;
      }
      ndbtab.setRangeListData(range_data.get(), parts);
    } else if (part_info->part_type == partition_type::LIST) {
      // Translate and check values for LIST partitions to NDB format
      const uint values = part_info->num_list_values;
      std::unique_ptr<int32[]> list_data(new (std::nothrow) int32[values * 2]);
      if (!list_data) {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
                 values * 2 * sizeof(int32));
        return 1;
      }
      for (uint i = 0; i < values; i++) {
        const LIST_PART_ENTRY *list_entry = &part_info->list_array[i];
        longlong list_val = list_entry->list_value;
        if (part_info->part_expr->unsigned_flag)
          list_val -= 0x8000000000000000ULL;
        if (list_val < INT_MIN32 || list_val > INT_MAX32) {
          my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
          return 1;
        }
        list_data[i * 2] = (int32)list_val;
        list_data[i * 2 + 1] = list_entry->partition_id;
      }
      ndbtab.setRangeListData(list_data.get(), values * 2);
    }

    DBUG_PRINT("info", ("Using UserDefined fragmentation type"));
    ndbtab.setFragmentType(NDBTAB::UserDefined);
  }

  const bool use_default_num_parts = part_info->use_default_num_partitions;
  ndbtab.setDefaultNoPartitionsFlag(use_default_num_parts);
  ndbtab.setLinearFlag(part_info->linear_hash_ind);

  if (ndbtab.getFragmentType() == NDBTAB::HashMapPartition &&
      use_default_num_parts) {
    /**
     * Skip below for default partitioning, this removes the need to undo
     * these settings later in ha_ndbcluster::create.
     */
    return 0;
  }

  {
    // Count number of fragments to use for the table and
    // build array describing which nodegroup should store each
    // partition(each partition is mapped to one fragment in the table).
    const uint tot_parts = part_info->get_tot_partitions();
    std::unique_ptr<uint32[]> frag_data(new (std::nothrow) uint32[tot_parts]);
    if (!frag_data) {
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), tot_parts * sizeof(uint32));
      return 1;
    }
    uint fd_index = 0;
    partition_element *part_elem;
    List_iterator<partition_element> part_it(part_info->partitions);
    while ((part_elem = part_it++)) {
      if (!part_info->is_sub_partitioned()) {
        frag_data[fd_index++] = part_elem->nodegroup_id;
      } else {
        partition_element *subpart_elem;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        while ((subpart_elem = sub_it++)) {
          frag_data[fd_index++] = subpart_elem->nodegroup_id;
        }
      }
    }

    // Double check number of partitions vs. fragments
    assert(tot_parts == fd_index);

    ndbtab.setFragmentCount(fd_index);
    ndbtab.setFragmentData(frag_data.get(), fd_index);
    ndbtab.setPartitionBalance(
        NdbDictionary::Object::PartitionBalance_Specific);
  }
  return 0;
}

class NDB_ALTER_DATA : public inplace_alter_handler_ctx {
 public:
  NDB_ALTER_DATA(THD *thd, Ndb *ndb, const char *dbname,
                 const NdbDictionary::Table *table)
      : dictionary(ndb->getDictionary()),
        old_table(table),
        new_table(new NdbDictionary::Table(*table)),
        table_id(table->getObjectId()),
        old_table_version(table->getObjectVersion()),
        schema_dist_client(thd),
        dbname_guard(ndb, dbname) {}
  ~NDB_ALTER_DATA() { delete new_table; }
  NdbDictionary::Dictionary *const dictionary;
  const NdbDictionary::Table *old_table;
  NdbDictionary::Table *new_table;
  const Uint32 table_id;
  const Uint32 old_table_version;
  Ndb_schema_dist_client schema_dist_client;
  // The dbname_guard will set database used by the Ndb object for the
  // lifetime of NDB_ALTER_DATA
  const Ndb_dbname_guard dbname_guard;
};

/*
  Utility function to use when reporting that inplace alter
  is not supported.
*/

static inline enum_alter_inplace_result inplace_unsupported(
    Alter_inplace_info *alter_info, const char *reason) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("%s", reason));
  alter_info->unsupported_reason = reason;

  THD *const thd = current_thd;
  if (!is_copying_alter_table_allowed(thd)) {
    // Query will return an error since copying alter table is not allowed, push
    // the reason as warning in order to allow user to see it with SHOW WARNINGS
    Thd_ndb *const thd_ndb = get_thd_ndb(thd);
    thd_ndb->push_warning(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON,
                          "Reason: '%s'", reason);
  }

  return HA_ALTER_INPLACE_NOT_SUPPORTED;
}

/*
  Check if the table was defined when the default COLUMN_FORMAT
  was FIXED and will now be become DYNAMIC.
  Warn the user if the ALTER TABLE isn't defined to be INPLACE
  and the column which will change isn't about to be dropped.
*/
static void inplace_check_implicit_column_format_change(
    const TABLE *const table, const TABLE *const altered_table,
    const Alter_inplace_info *const ha_alter_info) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table version: %lu", table->s->mysql_version));

  /* Find the old fields */
  for (uint i = 0; i < table->s->fields; i++) {
    const Field *field = table->field[i];

    /*
      Find fields that are not part of the primary key
      and that have a default COLUMN_FORMAT.
    */
    if ((!field->is_flag_set(PRI_KEY_FLAG)) &&
        field->column_format() == COLUMN_FORMAT_TYPE_DEFAULT) {
      DBUG_PRINT("info", ("Found old non-pk field %s", field->field_name));
      bool modified_explicitly = false;
      bool dropped = false;
      /*
        If the field is dropped or
        modified with an explicit COLUMN_FORMAT (FIXED or DYNAMIC)
        we don't need to warn the user about that field.
      */
      const Alter_inplace_info::HA_ALTER_FLAGS alter_flags =
          ha_alter_info->handler_flags;
      if (alter_flags & Alter_inplace_info::DROP_COLUMN ||
          alter_flags & Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT) {
        if (alter_flags & Alter_inplace_info::DROP_COLUMN) dropped = true;
        /* Find the fields in modified table */
        for (uint j = 0; j < altered_table->s->fields; j++) {
          const Field *field2 = altered_table->field[j];
          if (!my_strcasecmp(system_charset_info, field->field_name,
                             field2->field_name)) {
            dropped = false;
            if (field2->column_format() != COLUMN_FORMAT_TYPE_DEFAULT) {
              modified_explicitly = true;
            }
          }
        }
        if (dropped)
          DBUG_PRINT("info", ("Field %s is to be dropped", field->field_name));
        if (modified_explicitly)
          DBUG_PRINT("info",
                     ("Field  %s is modified with explicit COLUMN_FORMAT",
                      field->field_name));
      }
      if (!dropped && !modified_explicitly) {
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
}

static bool inplace_check_table_storage_changed(
    ha_storage_media new_table_storage, ha_storage_media old_table_storage) {
  if (new_table_storage == HA_SM_DEFAULT) {
    new_table_storage = HA_SM_MEMORY;
  }
  if (old_table_storage == HA_SM_DEFAULT) {
    old_table_storage = HA_SM_MEMORY;
  }
  if (new_table_storage != old_table_storage) {
    return true;
  }
  return false;
}

static bool inplace_check_column_has_index(TABLE *tab, uint field_idx,
                                           uint start_field, uint end_field) {
  /**
   * Check all indexes to determine if column has index instead of checking
   *   field->flags (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG
   *   since field->flags appears to only be set on first column in
   *   multi-part index
   */
  for (uint j = start_field; j < end_field; j++) {
    KEY *key_info = tab->key_info + j;
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->user_defined_key_parts;
    for (; key_part != end; key_part++) {
      if (key_part->field->field_index() == field_idx) {
        return true;
      }
    }
  }
  return false;
}

enum_alter_inplace_result ha_ndbcluster::supported_inplace_ndb_column_change(
    uint field_idx, TABLE *altered_table, Alter_inplace_info *ha_alter_info,
    bool table_storage_changed, bool index_on_column) const {
  DBUG_TRACE;

  HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  Field *old_field = table->field[field_idx];
  const NDBCOL *old_col = m_table_map->getColumn(field_idx);
  Field *new_field = altered_table->field[field_idx];
  NDBCOL new_col;

  // Don't allow INPLACE COMMENT NDB_COLUMN= changes
  const char *reason = nullptr;
  std::string_view old_comment{old_field->comment.str,
                               old_field->comment.length};
  std::string_view new_comment{new_field->comment.str,
                               new_field->comment.length};
  if (inplace_ndb_column_comment_changed(old_comment, new_comment, &reason)) {
    return inplace_unsupported(ha_alter_info, reason);
  }

  /*
    Create the new NdbDictionary::Column to be able to analyse if
    the storage type or format has changed. Note that the default
    storage format here needs to be based on whatever value it was before
    because some previous ALTER TABLE might have changed it implicitly
    to support add column or disk storage.
   */
  create_ndb_column(nullptr, new_col, new_field, create_info,
                    old_col->getDynamic());

  if (index_on_column) {
    /**
     * Index columns are stored in memory. Impose it on the new_col
     * being created just now, in order to make the check 'if
     * getStorageType() == getStorageType()' further down correct.
     * Continue to keep an index column in memory even though it is an
     * implicit disk column and the index is being dropped now (This
     * will avoid the cost of moving it back to disk by copy alter).
     */
    new_col.setStorageType(NdbDictionary::Column::StorageTypeMemory);
  } else {
    if (old_field->field_storage_type() == HA_SM_DEFAULT &&
        table_storage_changed &&
        new_col.getStorageType() != old_col->getStorageType()) {
      return inplace_unsupported(ha_alter_info,
                                 "Column storage media is changed due "
                                 "to change in table storage media");
    }

    if (old_field->field_storage_type() != new_field->field_storage_type() &&
        new_col.getStorageType() != old_col->getStorageType()) {
      return inplace_unsupported(ha_alter_info,
                                 "Column storage media is changed");
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
    new_col.setStorageType(old_col->getStorageType());
  }

  // Check if we are adding an index to a disk stored column
  if (new_field->is_flag_set(FIELD_IN_ADD_INDEX) &&
      new_col.getStorageType() == NdbDictionary::Column::StorageTypeDisk) {
    return inplace_unsupported(ha_alter_info,
                               "Add/drop index is not supported for disk "
                               "stored column");
  }

  if (index_on_column && new_field->field_storage_type() == HA_SM_DISK) {
    return inplace_unsupported(ha_alter_info,
                               "Changing COLUMN_STORAGE "
                               "to disk (Explicit STORAGE DISK) "
                               "on index column).");
  }

  /*
    If a field has a specified storage type (not default)
    and the column changes storage type this is not allowed
   */
  if (new_field->field_storage_type() != HA_SM_DEFAULT &&
      old_col->getStorageType() != new_col.getStorageType()) {
    return inplace_unsupported(ha_alter_info,
                               "Column storage media is changed");
  }

  // Check if type is changed
  if (new_col.getType() != old_col->getType()) {
    DBUG_PRINT("info",
               ("Detected unsupported type change for field %s : "
                "field types : old %u new %u "
                "ndb column types : old %u new %u ",
                old_field->field_name, old_field->real_type(),
                new_field->real_type(), old_col->getType(), new_col.getType()));
    return inplace_unsupported(ha_alter_info,
                               "Altering field type is not supported");
  }

  Alter_inplace_info::HA_ALTER_FLAGS alter_flags = ha_alter_info->handler_flags;
  bool altering_column =
      (alter_flags & (Alter_inplace_info::ALTER_COLUMN_DEFAULT |
                      Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE |
                      Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT));

  /*
    Check if format is changed.
    If we are modifying some column with the column format
    being specified (not using default value) and
    we see a change of format on the Field level or
    NdbDictionary::Column level, then the change is not supported.
   */
  if (altering_column &&
      new_field->column_format() != COLUMN_FORMAT_TYPE_DEFAULT &&
      (new_field->column_format() != old_field->column_format() ||
       new_col.getDynamic() != old_col->getDynamic())) {
    DBUG_PRINT("info", ("Detected unsupported format change for field %s : "
                        "field format : old %u new %u "
                        "ndb column format : old %u  new %u ",
                        old_field->field_name, old_field->column_format(),
                        new_field->column_format(), old_col->getDynamic(),
                        new_col.getDynamic()));
    return inplace_unsupported(ha_alter_info, "Altering column format");
  }

  return HA_ALTER_INPLACE_SHARED_LOCK;
}

enum_alter_inplace_result ha_ndbcluster::supported_inplace_field_change(
    Alter_inplace_info *ha_alter_info, Field *old_field, Field *new_field,
    bool field_fk_reference, bool index_on_column) const {
  DBUG_TRACE;

  // Check for definition change
  if (!old_field->eq_def(new_field)) {
    return inplace_unsupported(ha_alter_info,
                               "Altering field definition is "
                               "not supported");
  }

  // Check max display length
  if (new_field->max_display_length() != old_field->max_display_length()) {
    return inplace_unsupported(ha_alter_info,
                               "Altering field display length is "
                               "not supported");
  }

  // Check if nullable change
  if (new_field->is_nullable() != old_field->is_nullable()) {
    return inplace_unsupported(ha_alter_info,
                               "Altering if field is nullable is "
                               "not supported");
  }

  // Check if auto_increment change
  if (new_field->auto_flags != old_field->auto_flags) {
    return inplace_unsupported(ha_alter_info,
                               "Altering field auto_increment "
                               "is not supported");
  }

  // Check that BLOB fields are not modified
  if ((old_field->is_flag_set(BLOB_FLAG) ||
       new_field->is_flag_set(BLOB_FLAG)) &&
      !old_field->eq_def(new_field)) {
    return inplace_unsupported(ha_alter_info,
                               "Altering BLOB field is not supported");
  }

  // Check that default value is not added or removed
  if (old_field->is_flag_set(NO_DEFAULT_VALUE_FLAG) !=
      new_field->is_flag_set(NO_DEFAULT_VALUE_FLAG)) {
    return inplace_unsupported(
        ha_alter_info, "Adding or removing default value is not supported");
  }

  const enum enum_field_types mysql_type = old_field->real_type();
  char old_buf[MAX_ATTR_DEFAULT_VALUE_SIZE];
  char new_buf[MAX_ATTR_DEFAULT_VALUE_SIZE];

  if ((!old_field->is_flag_set(PRI_KEY_FLAG)) &&
      type_supports_default_value(mysql_type)) {
    if (!old_field->is_flag_set(NO_DEFAULT_VALUE_FLAG)) {
      ptrdiff_t src_offset = old_field->table->default_values_offset();
      if ((!old_field->is_real_null(src_offset)) ||
          (old_field->is_flag_set(NOT_NULL_FLAG))) {
        DBUG_PRINT("info", ("Checking default value hasn't changed "
                            "for field %s",
                            old_field->field_name));
        memset(old_buf, 0, MAX_ATTR_DEFAULT_VALUE_SIZE);
        get_default_value(old_buf, old_field);
        memset(new_buf, 0, MAX_ATTR_DEFAULT_VALUE_SIZE);
        get_default_value(new_buf, new_field);
        if (memcmp(old_buf, new_buf, MAX_ATTR_DEFAULT_VALUE_SIZE)) {
          return inplace_unsupported(ha_alter_info,
                                     "Altering default value is "
                                     "not supported");
        }
      }
    }
  }

  // Check if the field is renamed
  if ((new_field->is_flag_set(FIELD_IS_RENAMED)) ||
      (strcmp(old_field->field_name, new_field->field_name) != 0)) {
    DBUG_PRINT("info", ("Detected field %s is renamed %s",
                        old_field->field_name, new_field->field_name));
    if (field_fk_reference) {
      DBUG_PRINT("info", ("Detected unsupported rename field %s being "
                          "reference from a foreign key",
                          old_field->field_name));
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), "ALTER TABLE",
               "Altering name of a field being referenced from a foreign key "
               "is not supported",
               "dropping foreign key first");
      return HA_ALTER_ERROR;
    }
    if (index_on_column) {
      // Renaming column that is part of an index is not supported
      return inplace_unsupported(
          ha_alter_info,
          "Renaming column that is part of an index is not supported");
    }
  }

  return HA_ALTER_INPLACE_SHARED_LOCK;
}

/*
  Alter_inplace_info flags indicate a column has been modified
  check if supported field type change is found, if BLOB type is found
  or if default value has really changed.
*/
enum_alter_inplace_result ha_ndbcluster::supported_inplace_column_change(
    NdbDictionary::Dictionary *dict, TABLE *altered_table, uint field_position,
    Field *old_field, Alter_inplace_info *ha_alter_info) const {
  DBUG_TRACE;

  HA_CREATE_INFO *create_info = ha_alter_info->create_info;

  const bool is_table_storage_changed = inplace_check_table_storage_changed(
      create_info->storage_media, table_share->default_storage_media);

  DBUG_PRINT("info", ("Checking if supported column change for field %s",
                      old_field->field_name));

  Field *new_field = altered_table->field[field_position];

  // Ignore if old and new fields are virtual
  if (old_field->is_virtual_gcol() && new_field->is_virtual_gcol()) {
    return HA_ALTER_INPLACE_INSTANT;
  }

  // When either the new or the old field is a generated column,
  // the following conversions cannot be done inplace
  //  - non generated column to a generated column (both stored and virtual)
  //    and vice versa
  //  - generated stored column to a virtual column and vice versa
  // Changing a column generation expression is also not supported inplace
  // but check_inplace_alter_supported() handles that later by looking into
  // HA_ALTER_FLAGS
  if ((old_field->is_gcol() != new_field->is_gcol()) ||
      (old_field->gcol_info && (old_field->gcol_info->get_field_stored() !=
                                new_field->gcol_info->get_field_stored()))) {
    return inplace_unsupported(
        ha_alter_info,
        "Unsupported change involving generated stored/virtual column");
  }

  const bool is_index_on_column =
      inplace_check_column_has_index(table, field_position, 0, table->s->keys);

  // Check if storage type or format are changed from Ndb's point of view
  const enum_alter_inplace_result ndb_column_change_result =
      supported_inplace_ndb_column_change(
          field_position, altered_table, ha_alter_info,
          is_table_storage_changed, is_index_on_column);

  if (ndb_column_change_result == HA_ALTER_INPLACE_NOT_SUPPORTED ||
      ndb_column_change_result == HA_ALTER_ERROR) {
    return ndb_column_change_result;
  }

  const bool field_fk_reference =
      has_fk_dependency(dict, m_table->getColumn(field_position));

  // Check if table field properties are changed
  const enum_alter_inplace_result field_change_result =
      supported_inplace_field_change(ha_alter_info, old_field, new_field,
                                     field_fk_reference, is_index_on_column);

  if (field_change_result == HA_ALTER_INPLACE_NOT_SUPPORTED ||
      field_change_result == HA_ALTER_ERROR) {
    return field_change_result;
  }

  return HA_ALTER_INPLACE_SHARED_LOCK;
}

enum_alter_inplace_result ha_ndbcluster::check_inplace_alter_supported(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  THD *thd = current_thd;
  HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  Alter_info *alter_info = ha_alter_info->alter_info;
  /*
    Save the Alter_inplace_info::HA_ALTER_FLAGS set by the server.
    Note that some of the flags are set if conservatively and after
    double checking for real changes some flags can be reset.
   */
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags = ha_alter_info->handler_flags;
  const Alter_inplace_info::HA_ALTER_FLAGS supported =
      Alter_inplace_info::ADD_INDEX | Alter_inplace_info::DROP_INDEX |
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
      Alter_inplace_info::ALTER_INDEX_COMMENT |
      Alter_inplace_info::ALTER_COLUMN_NAME | Alter_inplace_info::RENAME_INDEX;

  const Alter_inplace_info::HA_ALTER_FLAGS not_supported = ~supported;

  Alter_inplace_info::HA_ALTER_FLAGS add_column =
      Alter_inplace_info::ADD_VIRTUAL_COLUMN |
      Alter_inplace_info::ADD_STORED_BASE_COLUMN;

  const Alter_inplace_info::HA_ALTER_FLAGS adding =
      Alter_inplace_info::ADD_INDEX | Alter_inplace_info::ADD_UNIQUE_INDEX;

  const Alter_inplace_info::HA_ALTER_FLAGS dropping =
      Alter_inplace_info::DROP_INDEX | Alter_inplace_info::DROP_UNIQUE_INDEX;

  enum_alter_inplace_result result = HA_ALTER_INPLACE_SHARED_LOCK;

  DBUG_TRACE;

  if (alter_flags & Alter_inplace_info::DROP_COLUMN) {
    return inplace_unsupported(ha_alter_info, "Dropping column");
  }

  if (alter_flags & Alter_inplace_info::ALTER_STORED_COLUMN_ORDER) {
    return inplace_unsupported(ha_alter_info, "Altering column order");
  }

  partition_info *part_info = altered_table->part_info;
  const NDBTAB *old_tab = m_table;

  if (THDVAR(thd, use_copying_alter_table) &&
      (alter_info->requested_algorithm ==
       Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT)) {
    // Usage of copying alter has been forced and user has not specified
    // any ALGORITHM=, don't allow inplace
    return inplace_unsupported(ha_alter_info,
                               "ndb_use_copying_alter_table is set");
  }

  DBUG_PRINT("info", ("Passed alter flags 0x%llx", alter_flags));
  DBUG_PRINT("info", ("Supported 0x%llx", supported));
  DBUG_PRINT("info", ("Not supported 0x%llx", not_supported));
  DBUG_PRINT("info", ("alter_flags & not_supported 0x%llx",
                      alter_flags & not_supported));

  bool max_rows_changed = false;
  bool comment_changed = false;

  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) {
    DBUG_PRINT("info", ("Some create options changed"));
    if (create_info->used_fields & HA_CREATE_USED_AUTO &&
        create_info->auto_increment_value != stats.auto_increment_value) {
      DBUG_PRINT("info", ("The AUTO_INCREMENT value changed"));

      /* Check that no other create option changed */
      if (create_info->used_fields ^ ~HA_CREATE_USED_AUTO) {
        return inplace_unsupported(ha_alter_info,
                                   "Not only AUTO_INCREMENT value "
                                   "changed");
      }
    }

    /* Check that ROW_FORMAT didn't change */
    if (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT &&
        create_info->row_type != table_share->real_row_type) {
      return inplace_unsupported(ha_alter_info, "ROW_FORMAT changed");
    }

    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS) {
      DBUG_PRINT("info", ("The MAX_ROWS value changed"));

      max_rows_changed = true;

      const ulonglong curr_max_rows = table_share->max_rows;
      if (curr_max_rows == 0) {
        // Don't support setting MAX_ROWS on a table without MAX_ROWS
        return inplace_unsupported(ha_alter_info,
                                   "setting MAX_ROWS on table "
                                   "without MAX_ROWS");
      }
    }
    if (create_info->used_fields & HA_CREATE_USED_COMMENT) {
      DBUG_PRINT("info", ("The COMMENT string changed"));
      comment_changed = true;
    }

    if (create_info->used_fields & HA_CREATE_USED_TABLESPACE) {
      // Changing TABLESPACE is not supported by inplace alter
      return inplace_unsupported(ha_alter_info,
                                 "Adding or changing TABLESPACE");
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG) {
    DBUG_PRINT("info", ("Reorganize partitions"));
    /*
      sql_partition.cc tries to compute what is going on
      and sets flags...that we clear
    */
    if (part_info->use_default_num_partitions) {
      DBUG_PRINT("info", ("Using default number of partitions, "
                          "clear some flags"));
      alter_flags = alter_flags & ~Alter_inplace_info::COALESCE_PARTITION;
      alter_flags = alter_flags & ~Alter_inplace_info::ADD_PARTITION;
    }
  }

  Ndb *ndb = get_thd_ndb(thd)->ndb;
  NDBDICT *dict = ndb->getDictionary();
  NdbDictionary::Table new_tab = *old_tab;

  /**
   * Check whether altering column properties can be performed inplace
   * by comparing all old and new fields
   */
  for (uint i = 0; i < table->s->fields; i++) {
    Field *field = table->field[i];
    const enum_alter_inplace_result column_change_result =
        supported_inplace_column_change(dict, altered_table, i, field,
                                        ha_alter_info);

    switch (column_change_result) {
      case HA_ALTER_ERROR:
      case HA_ALTER_INPLACE_NOT_SUPPORTED:
        return column_change_result;
        break;
      default:
        // Save the highest lock requirement
        result = std::min(result, column_change_result);
        break;
    }

    /*
       If we are changing a field name then change the
       corresponding column name in the temporary Ndb table.
    */
    if (alter_flags & Alter_inplace_info::ALTER_COLUMN_NAME) {
      Field *new_field = altered_table->field[i];
      if (strcmp(field->field_name, new_field->field_name) != 0 &&
          !field->is_virtual_gcol()) {
        NDBCOL *ndbCol = new_tab.getColumn(new_field->field_index());
        ndbCol->setName(new_field->field_name);
      }
    }
  }
  if (!(alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN)) {
    if (alter_flags & Alter_inplace_info::ALTER_COLUMN_DEFAULT) {
      // We didn't find that the default value has changed, remove flag
      DBUG_PRINT("info", ("No change of default value found, ignoring flag"));
      alter_flags &= ~Alter_inplace_info::ALTER_COLUMN_DEFAULT;
    }
    if (alter_flags & Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE) {
      // We didn't find that the storage type has changed, remove flag
      DBUG_PRINT("info", ("No change of storage type found, ignoring flag"));
      alter_flags &= ~Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE;
    }
    if (alter_flags & Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT) {
      // We didn't find that the storage format has changed, remove flag
      DBUG_PRINT("info", ("No change of storage format found, ignoring flag"));
      alter_flags &= ~Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT;
    }
    if (alter_flags & Alter_inplace_info::ALTER_STORED_COLUMN_TYPE) {
      // We didn't find that the storage of the column has changed, remove flag
      DBUG_PRINT("info", ("No change of storage type, ignoring flag"));
      alter_flags &= ~Alter_inplace_info::ALTER_STORED_COLUMN_TYPE;
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_COLUMN_NAME) {
    /*
      Check that we are only renaming column
    */
    if (alter_flags & ~Alter_inplace_info::ALTER_COLUMN_NAME) {
      return inplace_unsupported(ha_alter_info,
                                 "Only rename column exclusively can be "
                                 "performed inplace");
    }
  }

  if (alter_flags & Alter_inplace_info::RENAME_INDEX) {
    /*
     Server sets same flag(Alter_inplace_info::RENAME_INDEX) for ALTER INDEX and
     RENAME INDEX. RENAME INDEX cannot be done inplace and only the ALTER INDEX
     which changes the visibility of the index can be done inplace.
    */

    if (alter_info->flags & Alter_info::ALTER_RENAME_INDEX) {
      return inplace_unsupported(ha_alter_info,
                                 "Rename index can not be performed inplace");
    }
  }

  if (alter_flags & Alter_inplace_info::ADD_PK_INDEX) {
    return inplace_unsupported(ha_alter_info, "Adding primary key");
  }

  if (alter_flags & Alter_inplace_info::DROP_PK_INDEX) {
    return inplace_unsupported(ha_alter_info, "Dropping primary key");
  }

  // Catch all for everything not supported, should ideally have been caught
  // already and returned a clear text message.
  if (alter_flags & not_supported) {
    if (alter_info->requested_algorithm ==
        Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_ALTER_INFO,
                          "Detected unsupported change: "
                          "HA_ALTER_FLAGS = 0x%llx",
                          alter_flags & not_supported);
    return inplace_unsupported(ha_alter_info, "Detected unsupported change");
  }

  if (alter_flags & Alter_inplace_info::ALTER_COLUMN_NAME ||
      alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN ||
      alter_flags & Alter_inplace_info::ADD_PARTITION ||
      alter_flags & Alter_inplace_info::ALTER_TABLE_REORG || max_rows_changed ||
      comment_changed) {
    result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
    if (alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN) {
      NDBCOL col;

      /*
        Check that we are only adding columns
      */
      /*
        HA_COLUMN_DEFAULT_VALUE & HA_COLUMN_STORAGE & HA_COLUMN_FORMAT
        are set if they are specified in an later cmd
        even if they're no change. This is probably a bug
        conclusion: add them to add_column-mask, so that we silently "accept"
        them In case of someone trying to change a column, the HA_CHANGE_COLUMN
        would be set which we don't support, so we will still return
        HA_ALTER_NOT_SUPPORTED in those cases
      */
      add_column |= Alter_inplace_info::ALTER_COLUMN_DEFAULT;
      add_column |= Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE;
      add_column |= Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT;
      if (alter_flags & ~add_column) {
        return inplace_unsupported(ha_alter_info,
                                   "Only add column exclusively can be "
                                   "performed online");
      }
      /*
        Check for extra fields for hidden primary key
        or user defined partitioning
      */
      if (table_share->primary_key == MAX_KEY ||
          part_info->part_type != partition_type::HASH ||
          !part_info->list_of_part_fields) {
        return inplace_unsupported(ha_alter_info,
                                   "Found hidden primary key or "
                                   "user defined partitioning");
      }

      /* Find the new fields */
      for (uint i = table->s->fields; i < altered_table->s->fields; i++) {
        Field *field = altered_table->field[i];
        if (field->is_virtual_gcol()) {
          DBUG_PRINT("info",
                     ("Field %s is VIRTUAL; not adding.", field->field_name));
          continue;
        }
        DBUG_PRINT("info", ("Found new field %s", field->field_name));
        DBUG_PRINT("info", ("storage_type %i, column_format %i",
                            (uint)field->field_storage_type(),
                            (uint)field->column_format()));
        if (!field->is_flag_set(NO_DEFAULT_VALUE_FLAG)) {
          ptrdiff_t src_offset =
              field->table->s->default_values - field->table->record[0];
          if (/*
                Check that column doesn't have non NULL specified
                as default value.
               */
              (!field->is_real_null(src_offset)) ||
              (field->is_flag_set(NOT_NULL_FLAG)) ||
              /*
                 Check that column doesn't have
                 DEFAULT/ON INSERT/UPDATE CURRENT_TIMESTAMP as default
                 or AUTO_INCREMENT.
              */
              ((field->has_insert_default_datetime_value_expression() ||
                field->has_update_default_datetime_value_expression()) ||
               ((field->auto_flags & Field::NEXT_NUMBER) != 0))) {
            return inplace_unsupported(
                ha_alter_info,
                "Adding column with non-null default value "
                "is not supported online");
          }
        }
        /* Create new field to check if it can be added */
        const int create_column_result = create_ndb_column(
            thd, col, field, create_info, true /* use_dynamic_as_default */);
        if (create_column_result) {
          DBUG_PRINT("info", ("Failed to create NDB column, error %d",
                              create_column_result));
          return HA_ALTER_ERROR;
        }
        if (new_tab.addColumn(col)) {
          DBUG_PRINT("info", ("Failed to add NDB column to table"));
          return HA_ALTER_ERROR;
        }
      }
    }

    if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG) {
      const ulonglong curr_max_rows = table_share->max_rows;
      if (curr_max_rows != 0) {
        // No inplace REORGANIZE PARTITION for table with MAX_ROWS
        return inplace_unsupported(ha_alter_info,
                                   "REORGANIZE of table "
                                   "with MAX_ROWS");
      }
      new_tab.setFragmentCount(0);
      new_tab.setFragmentData(nullptr, 0);
    } else if (alter_flags & Alter_inplace_info::ADD_PARTITION) {
      DBUG_PRINT("info", ("Adding partition (%u)", part_info->num_parts));
      new_tab.setFragmentCount(part_info->num_parts);
      new_tab.setPartitionBalance(
          NdbDictionary::Object::PartitionBalance_Specific);
      if (new_tab.getFullyReplicated()) {
        // No add partition on fully replicated table
        return inplace_unsupported(
            ha_alter_info, "Can't add partition to fully replicated table");
      }
    }

    if (comment_changed) {
      const char *unsupported_reason;
      if (inplace_parse_comment(&new_tab, old_tab, create_info, thd, ndb,
                                &unsupported_reason, max_rows_changed)) {
        return inplace_unsupported(ha_alter_info, unsupported_reason);
      }
    }

    if (max_rows_changed) {
      ulonglong rows = create_info->max_rows;
      uint no_fragments = get_no_fragments(rows);
      uint reported_frags = no_fragments;
      if (adjusted_frag_count(ndb, no_fragments, reported_frags)) {
        push_warning(current_thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                     "Ndb might have problems storing the max amount "
                     "of rows specified");
      }
      if (reported_frags < old_tab->getFragmentCount()) {
        return inplace_unsupported(ha_alter_info,
                                   "Online reduction in number of "
                                   "fragments not supported");
      } else if (rows == 0) {
        /* Dont support setting MAX_ROWS to 0 inplace */
        return inplace_unsupported(ha_alter_info,
                                   "Setting MAX_ROWS to 0 is "
                                   "not supported online");
      }
      new_tab.setFragmentCount(reported_frags);
      new_tab.setDefaultNoPartitionsFlag(false);
      new_tab.setFragmentData(nullptr, 0);
      new_tab.setPartitionBalance(
          NdbDictionary::Object::PartitionBalance_Specific);
    }

    if (dict->supportedAlterTable(*old_tab, new_tab)) {
      DBUG_PRINT(
          "info",
          ("Adding column(s) or add/reorganize partition supported online"));
    } else {
      return inplace_unsupported(
          ha_alter_info,
          "Adding column(s) or add/reorganize partition not supported online");
    }
  }

  /*
    Check that we are not adding multiple indexes
  */
  if (alter_flags & adding) {
    if (((altered_table->s->keys - table->s->keys) != 1) ||
        (alter_flags & dropping)) {
      return inplace_unsupported(ha_alter_info,
                                 "Only one index can be added online");
    }
  }

  /*
    Check that we are not dropping multiple indexes
  */
  if (alter_flags & dropping) {
    if (((table->s->keys - altered_table->s->keys) != 1) ||
        (alter_flags & adding)) {
      return inplace_unsupported(ha_alter_info,
                                 "Only one index can be dropped online");
    }
  }

  // All unsupported cases should have returned directly
  assert(result != HA_ALTER_INPLACE_NOT_SUPPORTED);
  DBUG_PRINT("info", ("Inplace alter is supported"));
  return result;
}

enum_alter_inplace_result ha_ndbcluster::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  DBUG_TRACE;

  const enum_alter_inplace_result result =
      check_inplace_alter_supported(altered_table, ha_alter_info);

  if (result == HA_ALTER_INPLACE_NOT_SUPPORTED) {
    /*
      The ALTER TABLE is not supported inplace and will fall back
      to use copying ALTER TABLE. If --ndb-default-column-format is dynamic
      by default, the table was created by an older MySQL version and the
      algorithm for the alter table is not inplace -> check for
      implicit changes and print warnings.
    */
    if ((opt_ndb_default_column_format == NDB_DEFAULT_COLUMN_FORMAT_DYNAMIC) &&
        (table->s->mysql_version < NDB_VERSION_DYNAMIC_IS_DEFAULT) &&
        (ha_alter_info->alter_info->requested_algorithm !=
         Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)) {
      inplace_check_implicit_column_format_change(table, altered_table,
                                                  ha_alter_info);
    }
  }
  return result;
}

bool ha_ndbcluster::inplace_parse_comment(NdbDictionary::Table *new_tab,
                                          const NdbDictionary::Table *old_tab,
                                          HA_CREATE_INFO *create_info, THD *thd,
                                          Ndb *ndb, const char **reason,
                                          bool &max_rows_changed,
                                          bool *partition_balance_in_comment) {
  DBUG_TRACE;
  NDB_Modifiers table_modifiers(ndb_table_modifier_prefix, ndb_table_modifiers);
  if (table_modifiers.loadComment(create_info->comment.str,
                                  create_info->comment.length) == -1) {
    // The comment has already been parsed in update_comment_info() and syntax
    // error would have failed the command, crash here in debug if syntax error
    // occurs anyway
    assert(false);
    *reason = "Syntax error in COMMENT modifier";
    return true;
  }
  const NDB_Modifier *mod_nologging = table_modifiers.get("NOLOGGING");
  const NDB_Modifier *mod_frags = table_modifiers.get("PARTITION_BALANCE");
  const NDB_Modifier *mod_read_backup = table_modifiers.get("READ_BACKUP");
  const NDB_Modifier *mod_fully_replicated =
      table_modifiers.get("FULLY_REPLICATED");

  NdbDictionary::Object::PartitionBalance part_bal =
      g_default_partition_balance;
  if (parsePartitionBalance(thd, mod_frags, &part_bal) == false) {
    /**
     * unable to parse => modifier which is not found
     */
    mod_frags = table_modifiers.notfound();
  } else if (ndbd_support_partition_balance(ndb->getMinDbNodeVersion()) == 0) {
    *reason = "PARTITION_BALANCE not supported by current data node versions";
    return true;
  }

  if (mod_nologging->m_found) {
    if (new_tab->getLogging() != (!mod_nologging->m_val_bool)) {
      *reason = "Cannot alter NOLOGGING inplace";
      return true;
    }
    new_tab->setLogging(!mod_nologging->m_val_bool);
  }

  if (mod_read_backup->m_found) {
    if (ndbd_support_read_backup(ndb->getMinDbNodeVersion()) == 0) {
      *reason = "READ_BACKUP not supported by current data node versions";
      return true;
    }
    if (old_tab->getFullyReplicated() && (!mod_read_backup->m_val_bool)) {
      *reason = "READ_BACKUP off with FULLY_REPLICATED on";
      return true;
    }
    new_tab->setReadBackupFlag(mod_read_backup->m_val_bool);
  }

  if (mod_fully_replicated->m_found) {
    if (ndbd_support_fully_replicated(ndb->getMinDbNodeVersion()) == 0) {
      *reason = "FULLY_REPLICATED not supported by current data node versions";
      return true;
    }
    if (old_tab->getFullyReplicated() != mod_fully_replicated->m_val_bool) {
      *reason = "Turning FULLY_REPLICATED on after create";
      return true;
    }
  }
  /**
   * We will not silently change tables during ALTER TABLE to use read
   * backup or fully replicated. We will only use this configuration
   * variable to affect new tables. For ALTER TABLE one has to set these
   * properties explicitly.
   */
  if (mod_frags->m_found) {
    if (max_rows_changed) {
      max_rows_changed = false;
    }
    new_tab->setFragmentCount(0);
    new_tab->setFragmentData(nullptr, 0);
    new_tab->setPartitionBalance(part_bal);
    if (partition_balance_in_comment != nullptr) {
      *partition_balance_in_comment = true;
    }
    DBUG_PRINT("info", ("parse_comment_changes: PartitionBalance: %s",
                        new_tab->getPartitionBalanceString()));
  } else {
    part_bal = old_tab->getPartitionBalance();
  }
  if (old_tab->getFullyReplicated()) {
    if (part_bal != old_tab->getPartitionBalance()) {
      // Can't change partition balance inplace for fully replicated table
      *reason = "Changing PARTITION_BALANCE with FULLY_REPLICATED on";
      return true;
    }
    max_rows_changed = false;
  }
  return false;
}

static bool inplace_ndb_column_comment_changed(std::string_view old_comment,
                                               std::string_view new_comment,
                                               const char **reason) {
  if (old_comment == new_comment) return false;

  NDB_Modifiers old_modifiers(ndb_column_modifier_prefix, ndb_column_modifiers);
  NDB_Modifiers new_modifiers(ndb_column_modifier_prefix, ndb_column_modifiers);

  if (old_modifiers.loadComment(old_comment.data(), old_comment.length()) ==
      -1) {
    *reason = "Syntax error in old COMMENT modifier";
    return true;
  }
  if (new_modifiers.loadComment(new_comment.data(), new_comment.length()) ==
      -1) {
    *reason = "Syntax error in new COMMENT modifier";
    return true;
  }

  *reason = "NDB_COLUMN= comment changed";

  const NDB_Modifier *old_max_blob_part =
      old_modifiers.get("MAX_BLOB_PART_SIZE");
  const NDB_Modifier *new_max_blob_part =
      new_modifiers.get("MAX_BLOB_PART_SIZE");
  if (new_max_blob_part->m_found != old_max_blob_part->m_found) return true;
  if (old_max_blob_part->m_found && new_max_blob_part->m_found) {
    return old_max_blob_part->m_val_bool != new_max_blob_part->m_val_bool;
  }

  const NDB_Modifier *old_blob_inline_size =
      old_modifiers.get("BLOB_INLINE_SIZE");
  const NDB_Modifier *new_blob_inline_size =
      new_modifiers.get("BLOB_INLINE_SIZE");
  if (new_blob_inline_size->m_found != old_blob_inline_size->m_found)
    return true;
  if (old_blob_inline_size->m_found && new_blob_inline_size->m_found) {
    std::string_view old_val{old_blob_inline_size->m_val_str.str};
    std::string_view new_val{new_blob_inline_size->m_val_str.str};
    return old_val != new_val;
  }

  // did not change
  reason = nullptr;
  return false;
}

/**
   Return index of the key in the list of keys in table
   @table Table where to find the key
   @key_info Pointer to the key to find
*/
static uint index_of_key_in_table(const TABLE *table, const KEY *key_info) {
  for (uint i = 0; i < table->s->keys; i++) {
    if (key_info == table->key_info + i) {
      return i;
    }
  }
  // Inconsistency in list of keys or invalid key_ptr passed
  abort();
  return 0;
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
bool ha_ndbcluster::prepare_inplace_alter_table(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info, const dd::Table *,
    dd::Table *) {
  HA_CREATE_INFO *create_info = ha_alter_info->create_info;

  const Alter_inplace_info::HA_ALTER_FLAGS alter_flags =
      ha_alter_info->handler_flags;
  DBUG_PRINT("info", ("alter_flags: 0x%llx", alter_flags));

  const Alter_inplace_info::HA_ALTER_FLAGS adding =
      Alter_inplace_info::ADD_INDEX | Alter_inplace_info::ADD_UNIQUE_INDEX;

  DBUG_TRACE;

  THD *thd = current_thd;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock(
          "ha_ndbcluster::prepare_inplace_alter_table"))
    return true;

  const char *dbname = table->s->db.str;
  const char *tabname = table->s->table_name.str;

  Ndb *ndb = thd_ndb->ndb;
  NDB_ALTER_DATA *alter_data =
      new (*THR_MALLOC) NDB_ALTER_DATA(thd, ndb, dbname, m_table);
  if (!alter_data) return true;

  if (alter_data->dbname_guard.change_database_failed()) {
    thd_ndb->set_ndb_error(ndb->getNdbError(), "Failed to change database");
    ::destroy_at(alter_data);
    return true;
  }

  ha_alter_info->handler_ctx = alter_data;

  const NDBTAB *const old_tab = alter_data->old_table;
  NdbDictionary::Table *const new_tab = alter_data->new_table;

  if (!alter_data->schema_dist_client.prepare(dbname, tabname)) {
    // Release alter_data early as there is nothing to abort
    ::destroy_at(alter_data);
    ha_alter_info->handler_ctx = nullptr;
    print_error(HA_ERR_NO_CONNECTION, MYF(0));
    return true;
  }

  bool max_rows_changed = false;
  bool partition_balance_in_comment = false;
  bool comment_changed = false;
  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) {
    if (create_info->used_fields & HA_CREATE_USED_MAX_ROWS)
      max_rows_changed = true;
    if (create_info->used_fields & HA_CREATE_USED_COMMENT) {
      DBUG_PRINT("info", ("The COMMENT string changed"));
      comment_changed = true;
    }
  }

  // Pin the NDB_SHARE of the altered table
  NDB_SHARE::acquire_reference_on_existing(m_share, "inplace_alter");

  NDBDICT *dict = ndb->getDictionary();
  if (dict->beginSchemaTrans() == -1) {
    thd_ndb->set_ndb_error(dict->getNdbError(),
                           "Failed to start schema transaction");
    goto err;
  }

  if (alter_flags & adding) {
    KEY *key_info;
    KEY *key;
    uint *idx_p;
    uint *idx_end_p;
    KEY_PART_INFO *key_part;
    KEY_PART_INFO *part_end;
    DBUG_PRINT("info", ("Adding indexes"));
    key_info = (KEY *)thd->alloc(sizeof(KEY) * ha_alter_info->index_add_count);
    key = key_info;
    for (idx_p = ha_alter_info->index_add_buffer,
        idx_end_p = idx_p + ha_alter_info->index_add_count;
         idx_p < idx_end_p; idx_p++, key++) {
      /* Copy the KEY struct. */
      *key = ha_alter_info->key_info_buffer[*idx_p];
      /* Fix the key parts. */
      part_end = key->key_part + key->user_defined_key_parts;
      for (key_part = key->key_part; key_part < part_end; key_part++)
        key_part->field = table->field[key_part->fieldnr];
    }
    if (int error = prepare_inplace__add_index(
            thd, key_info, ha_alter_info->index_add_count)) {
      /*
        Exchange the key_info for the error message. If we exchange
        key number by key name in the message later, we need correct info.
      */
      KEY *save_key_info = table->key_info;
      table->key_info = key_info;
      table->file->print_error(error, MYF(0));
      table->key_info = save_key_info;
      goto abort;
    }
  }

  if (alter_flags & (Alter_inplace_info::DROP_INDEX |
                     Alter_inplace_info::DROP_UNIQUE_INDEX)) {
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      const KEY *key_info = ha_alter_info->index_drop_buffer[i];
      prepare_inplace__drop_index(index_of_key_in_table(table, key_info));
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_COLUMN_NAME) {
    DBUG_PRINT("info", ("Finding renamed field"));
    /* Find the renamed field */
    for (uint i = 0; i < table->s->fields; i++) {
      Field *old_field = table->field[i];
      Field *new_field = altered_table->field[i];
      if (strcmp(old_field->field_name, new_field->field_name) != 0) {
        DBUG_PRINT("info", ("Found field %s renamed to %s",
                            old_field->field_name, new_field->field_name));
        NdbDictionary::Column *ndbCol =
            new_tab->getColumn(new_field->field_index());
        ndbCol->setName(new_field->field_name);
      }
    }
  }

  if (alter_flags & Alter_inplace_info::ADD_STORED_BASE_COLUMN) {
    NDBCOL col;

    /* Find the new fields */
    for (uint i = table->s->fields; i < altered_table->s->fields; i++) {
      Field *field = altered_table->field[i];
      if (!field->stored_in_db) continue;

      DBUG_PRINT("info", ("Found new field %s", field->field_name));
      if (create_ndb_column(thd, col, field, create_info,
                            true /* use_dynamic_as_default */) != 0) {
        // Failed to create column in NDB
        goto abort;
      }

      /*
        If the user has not specified the field format
        make it dynamic to enable online add attribute
      */
      if (field->column_format() == COLUMN_FORMAT_TYPE_DEFAULT &&
          create_info->row_type == ROW_TYPE_DEFAULT && col.getDynamic()) {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "Converted FIXED field '%s' to DYNAMIC "
                            "to enable online ADD COLUMN",
                            field->field_name);
      }
      new_tab->addColumn(col);
    }
  }

  if (comment_changed) {
    // Parse comment again, this is done in order to extract "max_rows_changed"
    // and "partition_balance_in_comment"
    const char *unsupported_reason;
    if (inplace_parse_comment(new_tab, old_tab, create_info, thd, ndb,
                              &unsupported_reason, max_rows_changed,
                              &partition_balance_in_comment)) {
      // The comment has been parsed earlier, thus syntax error or unsupported
      // ALTER should have been detected already and failed the command (this
      // function should actually not be called in such case) -> don't push any
      // warnings or set error
      goto abort;
    }
  }

  if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG ||
      alter_flags & Alter_inplace_info::ADD_PARTITION || max_rows_changed ||
      partition_balance_in_comment) {
    if (alter_flags & Alter_inplace_info::ALTER_TABLE_REORG) {
      new_tab->setFragmentCount(0);
      new_tab->setFragmentData(nullptr, 0);
    } else if (alter_flags & Alter_inplace_info::ADD_PARTITION) {
      partition_info *part_info = altered_table->part_info;
      DBUG_PRINT("info", ("Adding partition (%u)", part_info->num_parts));
      new_tab->setFragmentCount(part_info->num_parts);
      new_tab->setPartitionBalance(
          NdbDictionary::Object::PartitionBalance_Specific);
    } else if (max_rows_changed) {
      ulonglong rows = create_info->max_rows;
      uint no_fragments = get_no_fragments(rows);
      uint reported_frags = no_fragments;
      if (adjusted_frag_count(ndb, no_fragments, reported_frags)) {
        assert(false); /* Checked above */
      }
      if (reported_frags < old_tab->getFragmentCount()) {
        assert(false);
        return false;
      }
      /* Note we don't set the ndb table's max_rows param, as that
       * is considered a 'real' change
       */
      // new_tab->setMaxRows(create_info->max_rows);
      new_tab->setFragmentCount(reported_frags);
      new_tab->setDefaultNoPartitionsFlag(false);
      new_tab->setFragmentData(nullptr, 0);
      new_tab->setPartitionBalance(
          NdbDictionary::Object::PartitionBalance_Specific);
    }

    if (dict->prepareHashMap(*old_tab, *new_tab) == -1) {
      thd_ndb->set_ndb_error(dict->getNdbError(), "Failed to prepare hash map");
      goto abort;
    }
  }

  if (alter_flags & Alter_inplace_info::ADD_FOREIGN_KEY) {
    const int create_fks_result = create_fks(thd, ndb, dbname, tabname);
    if (create_fks_result != 0) {
      table->file->print_error(create_fks_result, MYF(0));
      goto abort;
    }
  }

  return false;
abort:
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort) == -1) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to abort NDB schema transaction");
  }

err:
  return true;
}

static int inplace__set_sdi_and_alter_in_ndb(THD *thd,
                                             const NDB_ALTER_DATA *alter_data,
                                             dd::Table *new_table_def,
                                             const char *schema_name) {
  DBUG_TRACE;

  // Function alters table in NDB, requires database to be set
  assert(Ndb_dbname_guard::check_dbname(get_thd_ndb(thd)->ndb, schema_name));

  ndb_dd_fix_inplace_alter_table_def(new_table_def,
                                     alter_data->old_table->getName());

  dd::sdi_t sdi;
  if (!ndb_sdi_serialize(thd, new_table_def, schema_name, sdi)) {
    return 1;
  }

  NdbDictionary::Table *new_tab = alter_data->new_table;
  const int set_result =
      new_tab->setExtraMetadata(2,  // version 2 for frm
                                sdi.c_str(), (Uint32)sdi.length());
  if (set_result != 0) {
    my_printf_error(ER_GET_ERRMSG,
                    "Failed to set extra metadata during"
                    "inplace alter table, error: %d",
                    MYF(0), set_result);
    return 2;
  }

  NdbDictionary::Dictionary *dict = alter_data->dictionary;
  if (dict->alterTableGlobal(*alter_data->old_table, *new_tab)) {
    DBUG_PRINT("info",
               ("Inplace alter of table %s failed", new_tab->getName()));
    const NdbError ndberr = dict->getNdbError();
    const int error = ndb_to_mysql_error(&ndberr);
    my_error(ER_GET_ERRMSG, MYF(0), error, ndberr.message, "NDBCLUSTER");
    return error;
  }

  return 0;
}

bool ha_ndbcluster::inplace_alter_table(TABLE *,
                                        Alter_inplace_info *ha_alter_info,
                                        const dd::Table *,
                                        dd::Table *new_table_def) {
  DBUG_TRACE;
  int error = 0;
  THD *thd = current_thd;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  NDB_ALTER_DATA *alter_data = (NDB_ALTER_DATA *)ha_alter_info->handler_ctx;
  NDBDICT *dict = alter_data->dictionary;
  const Alter_inplace_info::HA_ALTER_FLAGS alter_flags =
      ha_alter_info->handler_flags;

  if (!thd_ndb->has_required_global_schema_lock(
          "ha_ndbcluster::inplace_alter_table")) {
    return true;
  }

  bool auto_increment_value_changed = false;
  if (alter_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) {
    if (create_info->auto_increment_value !=
        table->file->stats.auto_increment_value)
      auto_increment_value_changed = true;
  }

  if (alter_flags & (Alter_inplace_info::DROP_INDEX |
                     Alter_inplace_info::DROP_UNIQUE_INDEX)) {
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      const KEY *key_info = ha_alter_info->index_drop_buffer[i];

      if ((error = inplace__drop_index(
               dict, index_of_key_in_table(table, key_info)))) {
        print_error(error, MYF(0));
        goto abort;
      }
    }
  }

  if (alter_flags & Alter_inplace_info::DROP_FOREIGN_KEY) {
    if ((error = inplace__drop_fks(thd, thd_ndb->ndb, table->s->db.str,
                                   table->s->table_name.str)) != 0) {
      print_error(error, MYF(0));
      goto abort;
    }
  }

  assert(m_table != nullptr);

  error = inplace__set_sdi_and_alter_in_ndb(thd, alter_data, new_table_def,
                                            table->s->db.str);
  if (!error) {
    /*
     * Alter successful, commit schema transaction
     */
    if (dict->endSchemaTrans() == -1) {
      error = ndb_to_mysql_error(&dict->getNdbError());
      DBUG_PRINT("info",
                 ("Failed to commit schema transaction, error %u", error));
      table->file->print_error(error, MYF(0));
      goto err;
    }
    if (auto_increment_value_changed)
      error = set_auto_inc_val(thd_ndb->ndb, create_info->auto_increment_value);
    if (error) {
      DBUG_PRINT("info", ("Failed to set auto_increment value"));
      goto err;
    }
  } else  // if (error)
  {
  abort:
    if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort) ==
        -1) {
      DBUG_PRINT("info", ("Failed to abort schema transaction"));
      ERR_PRINT(dict->getNdbError());
    }
  }

err:
  return error ? true : false;
}

bool ha_ndbcluster::commit_inplace_alter_table(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit,
    const dd::Table *, dd::Table *new_table_def) {
  DBUG_TRACE;

  if (!commit) return abort_inplace_alter_table(altered_table, ha_alter_info);

  THD *thd = current_thd;
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb->has_required_global_schema_lock(
          "ha_ndbcluster::commit_inplace_alter_table")) {
    return true;  // Error
  }

  const char *dbname = table->s->db.str;
  const char *tabname = table->s->table_name.str;
  NDB_ALTER_DATA *alter_data = (NDB_ALTER_DATA *)ha_alter_info->handler_ctx;
  const Uint32 table_id = alter_data->table_id;
  const Uint32 table_version = alter_data->old_table_version;
  bool abort = false;  // OK

  // Pass pointer to table_def for usage by schema dist participant
  // in the binlog thread of this mysqld.
  m_share->inplace_alter_new_table_def = new_table_def;

  Ndb_schema_dist_client &schema_dist_client = alter_data->schema_dist_client;
  if (!schema_dist_client.alter_table_inplace_prepare(dbname, tabname, table_id,
                                                      table_version)) {
    // Failed to distribute the prepare of this alter table to the
    // other MySQL Servers, just log error and continue
    ndb_log_error("Failed to distribute inplace alter table prepare for '%s'",
                  tabname);
    assert(false);  // Catch in debug
  }

  // The pointer to new table_def is not valid anymore
  m_share->inplace_alter_new_table_def = nullptr;

  // Fetch the new table version and write it to the table definition.
  // The caller will then save it into DD
  {
    Ndb_table_guard ndbtab_g(thd_ndb->ndb, dbname, tabname);
    const NDBTAB *ndbtab = ndbtab_g.get_table();

    if (DBUG_EVALUATE_IF("ndb_missing_table_in_inplace_alter", true, false)) {
      ndbtab = nullptr;
    }

    if (ndbtab == nullptr) {
      // Since the schema transaction has already been committed at this
      // point we cannot properly abort (Bug#30302405),
      // but write an error in the log instead.
      // Local DD will need to be synchronised with new schema in cluster
      const NdbError &err = ndbtab_g.getNdbError();
      ndb_log_error(
          "Failed to complete inplace alter table commit for '%s', "
          "table not found, error %u: %s",
          tabname, err.code, err.message);
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Failed to complete inplace alter table commit, "
               "table not found");
      abort = true;  // ERROR
    } else {
      // The id should still be the same as before the alter
      assert((Uint32)ndbtab->getObjectId() == table_id);
      // The version should have been changed by the alter
      assert((Uint32)ndbtab->getObjectVersion() != table_version);

      ndb_dd_table_set_spi_and_version(new_table_def, ndbtab->getObjectId(),
                                       ndbtab->getObjectVersion());

      // Also check and correct the partition count if required.
      const bool check_partition_count_result =
          ndb_dd_table_check_partition_count(new_table_def,
                                             ndbtab->getPartitionCount());
      if (!check_partition_count_result) {
        ndb_dd_table_fix_partition_count(new_table_def,
                                         ndbtab->getPartitionCount());
      }

      // Check that NDB and DD metadata matches
      assert(Ndb_metadata::compare(thd, thd_ndb->ndb, dbname, ndbtab,
                                   new_table_def));
    }
  }

  if (!abort) {
    // Unpin the NDB_SHARE of the altered table
    NDB_SHARE::release_reference(m_share, "inplace_alter");
  }
  // else abort_inplace_alter_table will unpin the NDB_SHARE

  return abort;
}

bool ha_ndbcluster::abort_inplace_alter_table(
    TABLE *, Alter_inplace_info *ha_alter_info) {
  DBUG_TRACE;

  NDB_ALTER_DATA *alter_data = (NDB_ALTER_DATA *)ha_alter_info->handler_ctx;
  if (!alter_data) {
    // Could not find any alter_data, nothing to abort or already aborted
    return false;  // OK
  }

  NDBDICT *dict = alter_data->dictionary;
  if (dict->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort) == -1) {
    DBUG_PRINT("info", ("Failed to abort schema transaction"));
    ERR_PRINT(dict->getNdbError());
  }

  // NOTE! There is nothing informing participants that the prepared
  // schema distribution has been aborted

  ::destroy_at(alter_data);
  ha_alter_info->handler_ctx = nullptr;

  // Unpin the NDB_SHARE of the altered table
  NDB_SHARE::release_reference(m_share, "inplace_alter");

  return false;  // OK
}

void ha_ndbcluster::notify_table_changed(Alter_inplace_info *alter_info) {
  DBUG_TRACE;

  // Tell particpants that alter has completed and it's time to use
  // the new event operations
  const char *db = table->s->db.str;
  const char *name = table->s->table_name.str;
  NDB_ALTER_DATA *alter_data =
      static_cast<NDB_ALTER_DATA *>(alter_info->handler_ctx);
  Ndb_schema_dist_client &schema_dist_client = alter_data->schema_dist_client;
  if (!schema_dist_client.alter_table_inplace_commit(
          db, name, alter_data->table_id, alter_data->old_table_version)) {
    // Failed to distribute the commit of this alter table to the
    // other MySQL Servers, just log error and continue
    ndb_log_error("Failed to distribute inplace alter table commit of '%s'",
                  name);
  }

  ::destroy_at(alter_data);
  alter_info->handler_ctx = nullptr;
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

static int ndbcluster_get_tablespace(THD *thd, LEX_CSTRING db_name,
                                     LEX_CSTRING table_name,
                                     LEX_CSTRING *tablespace_name) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("db_name: %s, table_name: %s", db_name.str, table_name.str));
  assert(tablespace_name != nullptr);

  Ndb *ndb = check_ndb_in_thd(thd);
  if (ndb == nullptr) return HA_ERR_NO_CONNECTION;

  Ndb_table_guard ndbtab_g(ndb, db_name.str, table_name.str);
  const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
  if (ndbtab == nullptr) {
    ERR_RETURN(ndbtab_g.getNdbError());
  }

  Uint32 id;
  if (ndbtab->getTablespace(&id)) {
    NDBDICT *dict = ndb->getDictionary();
    NdbDictionary::Tablespace ts = dict->getTablespace(id);
    if (ndb_dict_check_NDB_error(dict)) {
      const char *tablespace = ts.getName();
      assert(tablespace);
      const size_t tablespace_len = strlen(tablespace);
      DBUG_PRINT("info", ("Found tablespace '%s'", tablespace));
      lex_string_strmake(thd->mem_root, tablespace_name, tablespace,
                         tablespace_len);
    }
  }

  return 0;
}

static bool create_tablespace_in_NDB(st_alter_tablespace *alter_info,
                                     NdbDictionary::Dictionary *dict,
                                     const Thd_ndb *thd_ndb, int &object_id,
                                     int &object_version) {
  NdbDictionary::Tablespace ndb_ts;
  ndb_ts.setName(alter_info->tablespace_name);
  ndb_ts.setExtentSize(static_cast<Uint32>(alter_info->extent_size));
  ndb_ts.setDefaultLogfileGroup(alter_info->logfile_group_name);
  NdbDictionary::ObjectId objid;
  if (dict->createTablespace(ndb_ts, &objid)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to create tablespace '%s' in NDB",
                          alter_info->tablespace_name);
    my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "TABLESPACE");
    return false;
  }
  object_id = objid.getObjectId();
  object_version = objid.getObjectVersion();
  if (dict->getWarningFlags() & NdbDictionary::Dictionary::WarnExtentRoundUp) {
    thd_ndb->push_warning("Extent size rounded up to kernel page size");
  }
  return true;
}

static bool create_datafile_in_NDB(st_alter_tablespace *alter_info,
                                   NdbDictionary::Dictionary *dict,
                                   const Thd_ndb *thd_ndb) {
  NdbDictionary::Datafile ndb_df;
  ndb_df.setPath(alter_info->data_file_name);
  ndb_df.setSize(alter_info->initial_size);
  ndb_df.setTablespace(alter_info->tablespace_name);
  if (dict->createDatafile(ndb_df)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to create datafile '%s' in NDB",
                          alter_info->data_file_name);
    if (alter_info->ts_cmd_type == CREATE_TABLESPACE) {
      my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "DATAFILE");
    } else {
      my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "CREATE DATAFILE FAILED");
    }
    return false;
  }
  if (dict->getWarningFlags() &
      NdbDictionary::Dictionary::WarnDatafileRoundUp) {
    thd_ndb->push_warning("Datafile size rounded up to extent size");
  } else if (dict->getWarningFlags() &
             NdbDictionary::Dictionary::WarnDatafileRoundDown) {
    thd_ndb->push_warning("Datafile size rounded down to extent size");
  }
  return true;
}

static bool drop_datafile_from_NDB(const char *tablespace_name,
                                   const char *datafile_name,
                                   NdbDictionary::Dictionary *dict,
                                   const Thd_ndb *thd_ndb) {
  NdbDictionary::Tablespace ts = dict->getTablespace(tablespace_name);
  if (ndb_dict_check_NDB_error(dict)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to get tablespace '%s' from NDB",
                          tablespace_name);
    my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "DROP DATAFILE FAILED");
    return false;
  }
  NdbDictionary::Datafile df = dict->getDatafile(0, datafile_name);
  if (ndb_dict_check_NDB_error(dict)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to get datafile '%s' from NDB",
                          datafile_name);
    my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "DROP DATAFILE FAILED");
    return false;
  }

  NdbDictionary::ObjectId objid;
  df.getTablespaceId(&objid);
  if (ts.getObjectId() == objid.getObjectId() &&
      strcmp(df.getPath(), datafile_name) == 0) {
    if (dict->dropDatafile(df)) {
      thd_ndb->push_ndb_error_warning(dict->getNdbError());
      thd_ndb->push_warning("Failed to drop datafile '%s' from NDB",
                            datafile_name);
      my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "DROP DATAFILE FAILED");
      return false;
    }
  } else {
    my_error(ER_WRONG_FILE_NAME, MYF(0), datafile_name);
    return false;
  }
  return true;
}

static bool drop_tablespace_from_NDB(const char *tablespace_name,
                                     NdbDictionary::Dictionary *dict,
                                     const Thd_ndb *thd_ndb, int &object_id,
                                     int &object_version) {
  NdbDictionary::Tablespace ts = dict->getTablespace(tablespace_name);
  if (ndb_dict_check_NDB_error(dict)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to get tablespace '%s' from NDB",
                          tablespace_name);
    my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "TABLESPACE");
    return false;
  }
  object_id = ts.getObjectId();
  object_version = ts.getObjectVersion();
  if (dict->dropTablespace(ts)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to drop tablespace '%s' from NDB",
                          tablespace_name);
    my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "TABLESPACE");
    return false;
  }
  return true;
}

static bool create_logfile_group_in_NDB(st_alter_tablespace *alter_info,
                                        NdbDictionary::Dictionary *dict,
                                        const Thd_ndb *thd_ndb, int &object_id,
                                        int &object_version) {
  NdbDictionary::LogfileGroup ndb_lg;
  ndb_lg.setName(alter_info->logfile_group_name);
  ndb_lg.setUndoBufferSize(static_cast<Uint32>(alter_info->undo_buffer_size));
  NdbDictionary::ObjectId objid;
  if (dict->createLogfileGroup(ndb_lg, &objid)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to create logfile group '%s' in NDB",
                          alter_info->logfile_group_name);
    my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
    return false;
  }
  object_id = objid.getObjectId();
  object_version = objid.getObjectVersion();
  if (dict->getWarningFlags() &
      NdbDictionary::Dictionary::WarnUndobufferRoundUp) {
    thd_ndb->push_warning("Undo buffer size rounded up to kernel page size");
  }
  return true;
}

static bool create_undofile_in_NDB(st_alter_tablespace *alter_info,
                                   NdbDictionary::Dictionary *dict,
                                   const Thd_ndb *thd_ndb) {
  NdbDictionary::Undofile ndb_uf;
  ndb_uf.setPath(alter_info->undo_file_name);
  ndb_uf.setSize(alter_info->initial_size);
  ndb_uf.setLogfileGroup(alter_info->logfile_group_name);
  if (dict->createUndofile(ndb_uf)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to create undofile '%s' in NDB",
                          alter_info->undo_file_name);
    if (alter_info->ts_cmd_type == CREATE_LOGFILE_GROUP) {
      my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "UNDOFILE");
    } else {
      my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "CREATE UNDOFILE FAILED");
    }
    return false;
  }
  if (dict->getWarningFlags() &
      NdbDictionary::Dictionary::WarnUndofileRoundDown) {
    thd_ndb->push_warning("Undofile size rounded down to kernel page size");
  }
  return true;
}

static bool drop_logfile_group_from_NDB(const char *logfile_group_name,
                                        NdbDictionary::Dictionary *dict,
                                        const Thd_ndb *thd_ndb, int &object_id,
                                        int &object_version) {
  NdbDictionary::LogfileGroup lg = dict->getLogfileGroup(logfile_group_name);
  if (ndb_dict_check_NDB_error(dict)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to get logfile group '%s' from NDB",
                          logfile_group_name);
    my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
    return false;
  }
  object_id = lg.getObjectId();
  object_version = lg.getObjectVersion();
  if (dict->dropLogfileGroup(lg)) {
    thd_ndb->push_ndb_error_warning(dict->getNdbError());
    thd_ndb->push_warning("Failed to drop logfile group '%s' from NDB",
                          logfile_group_name);
    my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
    return false;
  }
  return true;
}

/**
  Create, drop or alter tablespace or logfile group

  @param          thd         Thread context.
  @param          alter_info  Description of tablespace and specific
                              operation on it.
  @param [in,out] new_ts_def  dd::Tablespace object describing new version
                              of tablespace. Engines which support atomic DDL
                              can adjust this object. The updated information
                              will be saved to the data-dictionary.

  @note  The new_ts_def object is only altered for tablespace DDL. This
         object is then saved to the DD by the caller. In case of logfile
         groups, a new DD object is created and saved to the DD all within
         the scope of this function

  @return Operation status.
    @retval == 0  Success.
    @retval != 0  Error, only a subset of handler error codes (i.e those
                  that start with HA_) can be returned. Special case seems
                  to be 1 which is to be used when my_error() already has
                  been called to set the MySQL error code.
*/

static int ndbcluster_alter_tablespace(handlerton *, THD *thd,
                                       st_alter_tablespace *alter_info,
                                       const dd::Tablespace *,
                                       dd::Tablespace *new_ts_def) {
  DBUG_TRACE;

  Ndb *ndb = check_ndb_in_thd(thd, true);
  if (ndb == nullptr) {
    return HA_ERR_NO_CONNECTION;
  }
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  Ndb_schema_dist_client schema_dist_client(thd);
  const Thd_ndb *thd_ndb = get_thd_ndb(thd);

  // Function should be called with GSL held
  if (!thd_ndb->has_required_global_schema_lock(
          "ndbcluster_alter_tablespace")) {
    return HA_ERR_NO_CONNECTION;
  }

  switch (alter_info->ts_cmd_type) {
    case CREATE_TABLESPACE: {
      if (DBUG_EVALUATE_IF("ndb_skip_create_tablespace_in_NDB", true, false)) {
        // Force mismatch by skipping creation of the tablespace in NDB
        ndb_dd_disk_data_set_object_id_and_version(new_ts_def, 0, 0);
        ndb_dd_disk_data_set_object_type(new_ts_def, object_type::TABLESPACE);
        return 0;
      }

      if (alter_info->extent_size >= (Uint64(1) << 32)) {
        thd_ndb->push_warning("Value specified for EXTENT_SIZE was too large");
        my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
        return 1;
      }

      if (alter_info->max_size > 0) {
        thd_ndb->push_warning(
            "MAX_SIZE cannot be set to a value greater than 0");
        my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
        return 1;
      }

      if (!schema_dist_client.prepare("", alter_info->tablespace_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
      if (!schema_trans.begin_trans()) {
        return HA_ERR_NO_CONNECTION;
      }

      int object_id, object_version;
      if (!create_tablespace_in_NDB(alter_info, dict, thd_ndb, object_id,
                                    object_version)) {
        return 1;
      }

      if (!create_datafile_in_NDB(alter_info, dict, thd_ndb)) {
        return 1;
      }

      if (!schema_trans.commit_trans()) {
        my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "TABLESPACE");
        return 1;
      }
      DBUG_PRINT("info",
                 ("Successfully created tablespace '%s' and datafile "
                  "'%s' in NDB",
                  alter_info->tablespace_name, alter_info->data_file_name));

      // Set se_private_id and se_private_data for the tablespace
      ndb_dd_disk_data_set_object_id_and_version(new_ts_def, object_id,
                                                 object_version);
      ndb_dd_disk_data_set_object_type(new_ts_def, object_type::TABLESPACE);

      if (!schema_dist_client.create_tablespace(alter_info->tablespace_name,
                                                object_id, object_version)) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute CREATE TABLESPACE '%s'",
                              alter_info->tablespace_name);
      }
      break;
    }
    case ALTER_TABLESPACE: {
      if (!schema_dist_client.prepare("", alter_info->tablespace_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      switch (alter_info->ts_alter_tablespace_type) {
        case ALTER_TABLESPACE_ADD_FILE: {
          if (alter_info->max_size > 0) {
            thd_ndb->push_warning(
                "MAX_SIZE cannot be set to a value greater than "
                "0");
            my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
            return 1;
          }

          Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
          if (!schema_trans.begin_trans()) {
            return HA_ERR_NO_CONNECTION;
          }

          if (!create_datafile_in_NDB(alter_info, dict, thd_ndb)) {
            return 1;
          }

          if (!schema_trans.commit_trans()) {
            my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0),
                     "CREATE DATAFILE FAILED");
            return 1;
          }
          DBUG_PRINT("info", ("Successfully created datafile '%s' in NDB",
                              alter_info->data_file_name));
          break;
        }
        case ALTER_TABLESPACE_DROP_FILE: {
          Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
          if (!schema_trans.begin_trans()) {
            return HA_ERR_NO_CONNECTION;
          }

          if (!drop_datafile_from_NDB(alter_info->tablespace_name,
                                      alter_info->data_file_name, dict,
                                      thd_ndb)) {
            return 1;
          }

          if (!schema_trans.commit_trans()) {
            my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "DROP DATAFILE FAILED");
            return 1;
          }
          DBUG_PRINT("info", ("Successfully dropped datafile '%s' from NDB",
                              alter_info->data_file_name));
          break;
        }
        default: {
          DBUG_PRINT("error", ("Unsupported alter tablespace type: %d",
                               alter_info->ts_alter_tablespace_type));
          return HA_ADMIN_NOT_IMPLEMENTED;
        }
      }

      NdbDictionary::Tablespace ts =
          dict->getTablespace(alter_info->tablespace_name);
      if (ndb_dict_check_NDB_error(dict)) {
        // Failed to get tablespace from NDB, push warnings and continue
        thd_ndb->push_ndb_error_warning(dict->getNdbError());
        thd_ndb->push_warning("Failed to get tablespace '%s' from NDB",
                              alter_info->tablespace_name);
        thd_ndb->push_warning("Failed to distribute ALTER TABLESPACE '%s'",
                              alter_info->tablespace_name);
        break;
      }

      if (!schema_dist_client.alter_tablespace(alter_info->tablespace_name,
                                               ts.getObjectId(),
                                               ts.getObjectVersion())) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute ALTER TABLESPACE '%s'",
                              alter_info->tablespace_name);
      }
      break;
    }
    case CREATE_LOGFILE_GROUP: {
      if (alter_info->undo_file_name == nullptr) {
        thd_ndb->push_warning("REDO files in LOGFILE GROUP are not supported");
        return HA_ADMIN_NOT_IMPLEMENTED;
      }

      if (alter_info->undo_buffer_size >= (Uint64(1) << 32)) {
        thd_ndb->push_warning(
            "Size specified for UNDO_BUFFER_SIZE was too "
            "large");
        my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
        return 1;
      }

      if (!schema_dist_client.prepare("", alter_info->logfile_group_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
      if (!schema_trans.begin_trans()) {
        return HA_ERR_NO_CONNECTION;
      }

      int object_id, object_version;
      if (!create_logfile_group_in_NDB(alter_info, dict, thd_ndb, object_id,
                                       object_version)) {
        return 1;
      }

      if (!create_undofile_in_NDB(alter_info, dict, thd_ndb)) {
        return 1;
      }

      // Add Logfile Group entry to the DD as a tablespace
      Ndb_dd_client dd_client(thd);
      std::vector<std::string> undofile_names = {alter_info->undo_file_name};
      if (!dd_client.install_logfile_group(
              alter_info->logfile_group_name, undofile_names, object_id,
              object_version, false /* force_overwrite */) ||
          DBUG_EVALUATE_IF("ndb_dd_client_install_logfile_group_fail", true,
                           false)) {
        thd_ndb->push_warning("Logfile group '%s' could not be stored in DD",
                              alter_info->logfile_group_name);
        my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
        return 1;
      }

      // Objects created in NDB and DD. Time to commit NDB schema transaction
      if (!schema_trans.commit_trans()) {
        if (DBUG_EVALUATE_IF("ndb_dd_client_lfg_force_commit", true, false)) {
          // Force commit of logfile group creation in DD when creation in NDB
          // has failed leading to a mismatch
          dd_client.commit();
          return 0;
        }
        my_error(ER_CREATE_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
        return 1;
      }
      DBUG_PRINT("info",
                 ("Successfully created logfile group '%s' and undofile "
                  "'%s' in NDB",
                  alter_info->logfile_group_name, alter_info->undo_file_name));

      // NDB schema transaction committed successfully, safe to commit in DD
      dd_client.commit();

      if (!schema_dist_client.create_logfile_group(
              alter_info->logfile_group_name, object_id, object_version)) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute CREATE LOGFILE GROUP '%s'",
                              alter_info->logfile_group_name);
      }
      break;
    }
    case ALTER_LOGFILE_GROUP: {
      if (alter_info->undo_file_name == nullptr) {
        thd_ndb->push_warning("REDO files in LOGFILE GROUP are not supported");
        return HA_ADMIN_NOT_IMPLEMENTED;
      }

      if (!schema_dist_client.prepare("", alter_info->logfile_group_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
      if (!schema_trans.begin_trans()) {
        return HA_ERR_NO_CONNECTION;
      }

      if (!create_undofile_in_NDB(alter_info, dict, thd_ndb)) {
        return 1;
      }

      // Update Logfile Group entry in the DD
      Ndb_dd_client dd_client(thd);
      if (!dd_client.install_undo_file(alter_info->logfile_group_name,
                                       alter_info->undo_file_name) ||
          DBUG_EVALUATE_IF("ndb_dd_client_install_undo_file_fail", true,
                           false)) {
        thd_ndb->push_warning(
            "Undofile '%s' could not be added to logfile "
            "group '%s' in DD",
            alter_info->undo_file_name, alter_info->logfile_group_name);
        my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "CREATE UNDOFILE FAILED");
        return 1;
      }

      // Objects created in NDB and DD. Time to commit NDB schema transaction
      if (!schema_trans.commit_trans()) {
        my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), "CREATE UNDOFILE FAILED");
        return 1;
      }
      DBUG_PRINT("info", ("Successfully created undofile '%s' in NDB",
                          alter_info->undo_file_name));

      // NDB schema transaction committed successfully, safe to commit in DD
      dd_client.commit();

      NdbDictionary::LogfileGroup ndb_lg =
          dict->getLogfileGroup(alter_info->logfile_group_name);
      if (ndb_dict_check_NDB_error(dict)) {
        // Failed to get logfile group from NDB, push warnings and continue
        thd_ndb->push_ndb_error_warning(dict->getNdbError());
        thd_ndb->push_warning("Failed to get logfile group '%s' from NDB",
                              alter_info->logfile_group_name);
        thd_ndb->push_warning("Failed to distribute ALTER LOGFILE GROUP '%s'",
                              alter_info->logfile_group_name);
        break;
      }
      if (!schema_dist_client.alter_logfile_group(
              alter_info->logfile_group_name, ndb_lg.getObjectId(),
              ndb_lg.getObjectVersion())) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute ALTER LOGFILE GROUP '%s'",
                              alter_info->logfile_group_name);
      }
      break;
    }
    case DROP_TABLESPACE: {
      if (!schema_dist_client.prepare("", alter_info->tablespace_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
      if (!schema_trans.begin_trans()) {
        return HA_ERR_NO_CONNECTION;
      }

      int object_id, object_version;
      if (!drop_tablespace_from_NDB(alter_info->tablespace_name, dict, thd_ndb,
                                    object_id, object_version)) {
        return 1;
      }
      if (!schema_trans.commit_trans()) {
        my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "TABLESPACE");
        return 1;
      }
      DBUG_PRINT("info", ("Successfully dropped tablespace '%s' from NDB",
                          alter_info->tablespace_name));

      if (!schema_dist_client.drop_tablespace(alter_info->tablespace_name,
                                              object_id, object_version)) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute DROP TABLESPACE '%s'",
                              alter_info->tablespace_name);
      }
      break;
    }
    case DROP_LOGFILE_GROUP: {
      if (!schema_dist_client.prepare("", alter_info->logfile_group_name)) {
        return HA_ERR_NO_CONNECTION;
      }

      Ndb_schema_trans_guard schema_trans(thd_ndb, dict);
      if (!schema_trans.begin_trans()) {
        return HA_ERR_NO_CONNECTION;
      }

      int object_id, object_version;
      if (!drop_logfile_group_from_NDB(alter_info->logfile_group_name, dict,
                                       thd_ndb, object_id, object_version)) {
        return 1;
      }

      // Drop Logfile Group entry from the DD
      Ndb_dd_client dd_client(thd);
      if (!dd_client.drop_logfile_group(alter_info->logfile_group_name) ||
          DBUG_EVALUATE_IF("ndb_dd_client_drop_logfile_group_fail", true,
                           false)) {
        thd_ndb->push_warning("Logfile group '%s' could not be dropped from DD",
                              alter_info->logfile_group_name);
        my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
        return 1;
      }

      // Objects dropped from NDB and DD. Time to commit NDB schema transaction
      if (!schema_trans.commit_trans()) {
        my_error(ER_DROP_FILEGROUP_FAILED, MYF(0), "LOGFILE GROUP");
        return 1;
      }
      DBUG_PRINT("info", ("Successfully dropped logfile group '%s' from NDB",
                          alter_info->logfile_group_name));

      // NDB schema transaction committed successfully, safe to commit in DD
      dd_client.commit();

      if (!schema_dist_client.drop_logfile_group(alter_info->logfile_group_name,
                                                 object_id, object_version)) {
        // Schema distribution failed, just push a warning and continue
        thd_ndb->push_warning("Failed to distribute DROP LOGFILE GROUP '%s'",
                              alter_info->logfile_group_name);
      }
      break;
    }
    case CHANGE_FILE_TABLESPACE:
    case ALTER_ACCESS_MODE_TABLESPACE: {
      return HA_ADMIN_NOT_IMPLEMENTED;
    }
    default: {
      // Unexpected, crash in debug
      assert(false);
      return HA_ADMIN_NOT_IMPLEMENTED;
    }
  }

  return 0;
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

static bool ndbcluster_get_tablespace_statistics(
    const char *tablespace_name, const char *file_name,
    const dd::Properties &ts_se_private_data, ha_tablespace_statistics *stats) {
  DBUG_TRACE;

  // Find out type of object. The type is stored in se_private_data
  enum object_type type;

  if (!ndb_dd_disk_data_get_object_type(ts_se_private_data, type)) {
    my_printf_error(ER_INTERNAL_ERROR, "Could not get object type", MYF(0));
    return true;
  }

  THD *thd = current_thd;
  Ndb *ndb = check_ndb_in_thd(thd);
  if (!ndb) {
    // No connection to NDB
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return true;
  }
  Thd_ndb *thd_ndb = get_thd_ndb(thd);

  if (type == object_type::LOGFILE_GROUP) {
    NdbDictionary::Dictionary *dict = ndb->getDictionary();

    /* Find a node which is alive. NDB's view of an undo file
     * is actually a composite of the stats found across all
     * data nodes. However this does not fit well with the
     * server's view which thinks of it as a single file.
     * Since the stats of interest don't vary across the data
     * nodes, using the first available data node is acceptable.
     */
    NdbDictionary::Undofile uf = dict->getUndofile(-1, file_name);
    if (ndb_dict_check_NDB_error(dict)) {
      thd_ndb->set_ndb_error(dict->getNdbError(), "Could not get undo file");
      return true;
    }

    NdbDictionary::LogfileGroup lfg =
        dict->getLogfileGroup(uf.getLogfileGroup());
    if (ndb_dict_check_NDB_error(dict)) {
      thd_ndb->set_ndb_error(dict->getNdbError(),
                             "Could not get logfile group");
      return true;
    }

    /* Check if logfile group name matches tablespace name.
     * Failure means that the NDB dictionary has gone out
     * of sync with the DD
     */
    if (strcmp(lfg.getName(), tablespace_name) != 0) {
      my_error(ER_TABLESPACE_MISSING, MYF(0), tablespace_name);
      assert(false);
      return true;
    }

    // Populate statistics
    stats->m_id = uf.getObjectId();
    stats->m_type = "UNDO LOG";
    stats->m_logfile_group_name = lfg.getName();
    stats->m_logfile_group_number = lfg.getObjectId();
    stats->m_total_extents = uf.getSize() / 4;
    stats->m_extent_size = 4;
    stats->m_initial_size = uf.getSize();
    stats->m_maximum_size = uf.getSize();
    stats->m_version = uf.getObjectVersion();
    std::stringstream extra;
    extra << "UNDO_BUFFER_SIZE=" << lfg.getUndoBufferSize();
    stats->m_extra = extra.str().c_str();

    return false;
  }

  if (type == object_type::TABLESPACE) {
    NdbDictionary::Dictionary *dict = ndb->getDictionary();

    /* Find a node which is alive. NDB's view of a data file
     * is actually a composite of the stats found across all
     * data nodes. However this does not fit well with the
     * server's view which thinks of it as a single file.
     * Since the stats of interest don't vary across the data
     * nodes, using the first available data node is acceptable.
     */
    NdbDictionary::Datafile df = dict->getDatafile(-1, file_name);
    if (ndb_dict_check_NDB_error(dict)) {
      thd_ndb->set_ndb_error(dict->getNdbError(), "Could not get data file");
      return true;
    }

    NdbDictionary::Tablespace ts = dict->getTablespace(df.getTablespace());
    if (ndb_dict_check_NDB_error(dict)) {
      thd_ndb->set_ndb_error(dict->getNdbError(), "Could not get tablespace");
      return true;
    }

    /* Check if tablespace name from NDB matches tablespace name
     * from DD. Failure means that the NDB dictionary has gone out
     * of sync with the DD
     */
    if (strcmp(ts.getName(), tablespace_name) != 0) {
      my_error(ER_TABLESPACE_MISSING, MYF(0), tablespace_name);
      assert(false);
      return true;
    }

    // Populate statistics
    stats->m_id = df.getObjectId();
    stats->m_type = "DATAFILE";
    stats->m_logfile_group_name = ts.getDefaultLogfileGroup();
    stats->m_logfile_group_number = ts.getDefaultLogfileGroupId();
    stats->m_free_extents = df.getFree() / ts.getExtentSize();
    stats->m_total_extents = df.getSize() / ts.getExtentSize();
    stats->m_extent_size = ts.getExtentSize();
    stats->m_initial_size = df.getSize();
    stats->m_maximum_size = df.getSize();
    stats->m_version = df.getObjectVersion();
    stats->m_row_format = "FIXED";

    return false;
  }

  // Should never reach here
  assert(false);
  return true;
}

/**
  Return number of partitions used by NDB table.

  This function is used while opening table which exist in DD, the number of
  partitions are then loaded from DD into table_share. For upgrade, when
  value is not in DD, there is an extra step in the upgrade code that fills the
  value by fetching from NDB.

  @param path normalized path(same as open) to the table

  @param[out] num_parts Number of partitions

  @retval false for success
*/
bool ha_ndbcluster::get_num_parts(const char *path [[maybe_unused]],
                                  uint *num_parts) {
  if (table_share->m_part_info == nullptr) {
    // Bootstrap privilege table migration
    // The part_info is not set during generic server upgrade of "legacy
    // privilege tables", use hardcoded value to allow open before
    // altering to other engine.
    //
    // The hardcoded value is also set for tables with triggers. These tables
    // are handled separately during generic server upgrades.
    *num_parts = 0;
    return false;
  }

  *num_parts = table_share->m_part_info->num_parts;
  DBUG_PRINT("exit", ("num_parts: %u", *num_parts));
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

bool ha_ndbcluster::upgrade_table(THD *thd, const char *db_name,
                                  const char *table_name, dd::Table *dd_table) {
  Ndb *ndb = check_ndb_in_thd(thd);

  if (!ndb) {
    // No connection to NDB
    my_error(HA_ERR_NO_CONNECTION, MYF(0));
    return true;
  }

  Ndb_table_guard ndbtab_g(ndb, db_name, table_name);
  const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
  if (ndbtab == nullptr) {
    return true;
  }

  // Set object id and version
  ndb_dd_table_set_spi_and_version(dd_table, ndbtab->getObjectId(),
                                   ndbtab->getObjectVersion());

  /*
    Detect and set row format for the table. This is done here
    since the row format of a table is determined by the
    'varpart_reference' which isn't available earlier upstream
  */
  ndb_dd_table_set_row_format(dd_table, ndbtab->getForceVarPart());

  // Set the previous mysql version of the table i.e. mysql version
  // from which the table is being upgraded
  ndb_dd_table_set_previous_mysql_version(dd_table, table->s->mysql_version);

  // Set foreign key information in the table
  if (!ndb_dd_upgrade_foreign_keys(dd_table, ndb, db_name, ndbtab)) {
    return true;
  }

  return false;
}

/*
  @brief Shut down background tasks accessing DD or InnoDB before shutting down.

  @param  hton  Handlerton of the SE

*/
static void ndbcluster_pre_dd_shutdown(handlerton *hton [[maybe_unused]]) {
  // Stop and deinitialize the ndb_metadata_change_monitor thread
  ndb_metadata_change_monitor_thread.stop();
  ndb_metadata_change_monitor_thread.deinit();
  // Notify ndb_binlog that the ndb_purger need to be stopped
  ndbcluster_binlog_pre_dd_shutdown();
}

static int show_ndb_status(THD *thd, SHOW_VAR *var, char *) {
  if (!check_ndb_in_thd(thd)) return -1;

  struct st_ndb_status *st;
  SHOW_VAR *st_var;
  {
    // Allocate memory in current MEM_ROOT
    char *mem = (char *)(*THR_MALLOC)
                    ->Alloc(sizeof(struct st_ndb_status) +
                            sizeof(ndb_status_vars_dynamic));
    st = new (mem) st_ndb_status;
    st_var = (SHOW_VAR *)(mem + sizeof(struct st_ndb_status));
    memcpy(st_var, &ndb_status_vars_dynamic, sizeof(ndb_status_vars_dynamic));
    int i = 0;
    SHOW_VAR *tmp = &(ndb_status_vars_dynamic[0]);
    for (; tmp->value; tmp++, i++)
      st_var[i].value = mem + (tmp->value - (char *)&g_ndb_status);
  }
  {
    Thd_ndb *thd_ndb = get_thd_ndb(thd);
    Ndb_cluster_connection *c = thd_ndb->connection;
    update_status_variables(thd_ndb, st, c);
  }
  var->type = SHOW_ARRAY;
  var->value = (char *)st_var;
  return 0;
}

/*
   Array defining the status variables which can be returned by
   the ndbcluster plugin in a SHOW STATUS or performance_schema query.

   The list consist of functions as well as further sub arrays. Functions
   are used when the array first need to be populated before its values
   can be read.
*/

static SHOW_VAR ndb_status_vars[] = {
    {"Ndb", (char *)&show_ndb_status, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&show_ndb_status_injector, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&ndb_status_vars_replica, SHOW_ARRAY, SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&show_ndb_status_server_api, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ndb_index_stat", (char *)&show_ndb_status_index_stat, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&show_ndb_metadata_check, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&show_ndb_metadata_synced, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ndb", (char *)&show_ndb_metadata_excluded_count, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

static MYSQL_SYSVAR_ULONG(extra_logging,         /* name */
                          opt_ndb_extra_logging, /* var */
                          PLUGIN_VAR_OPCMDARG,
                          "Turn on more logging in the error log.",
                          nullptr, /* check func. */
                          nullptr, /* update func. */
                          1,       /* default */
                          0,       /* min */
                          0,       /* max */
                          0        /* block */
);

static MYSQL_SYSVAR_ULONG(wait_connected,         /* name */
                          opt_ndb_wait_connected, /* var */
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Time (in seconds) to wait for connection to "
                          "cluster.",
                          nullptr,             /* check func. */
                          nullptr,             /* update func. */
                          120,                 /* default */
                          0,                   /* min */
                          ONE_YEAR_IN_SECONDS, /* max */
                          0                    /* block */
);

static MYSQL_SYSVAR_ULONG(wait_setup,         /* name */
                          opt_ndb_wait_setup, /* var */
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Time (in seconds) to wait for setup to "
                          "complete (0 = no wait)",
                          nullptr,             /* check func. */
                          nullptr,             /* update func. */
                          120,                 /* default */
                          0,                   /* min */
                          ONE_YEAR_IN_SECONDS, /* max */
                          0                    /* block */
);

static MYSQL_SYSVAR_ULONG(replica_batch_size,         /* name */
                          opt_ndb_replica_batch_size, /* var */
                          PLUGIN_VAR_OPCMDARG,
                          "Batch size in bytes for the replica applier.",
                          nullptr,                    /* check func */
                          nullptr,                    /* update func */
                          DEFAULT_REPLICA_BATCH_SIZE, /* default */
                          0,                          /* min */
                          2UL * 1024 * 1024 * 1024,   /* max */
                          0                           /* block */
);

static MYSQL_SYSVAR_UINT(replica_blob_write_batch_bytes,         /* name */
                         opt_ndb_replica_blob_write_batch_bytes, /* var */
                         PLUGIN_VAR_OPCMDARG,
                         "Specifies the byte size of batched blob writes "
                         "for the replica applier. 0 == No limit.",
                         nullptr,                    /* check func */
                         nullptr,                    /* update func */
                         DEFAULT_REPLICA_BATCH_SIZE, /* default */
                         0,                          /* min */
                         2UL * 1024 * 1024 * 1024,   /* max */
                         0                           /* block */
);

static const int MAX_CLUSTER_CONNECTIONS = 63;

static MYSQL_SYSVAR_UINT(
    cluster_connection_pool,         /* name */
    opt_ndb_cluster_connection_pool, /* var */
    PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
    "Pool of cluster connections to be used by mysql server.",
    nullptr,                 /* check func. */
    nullptr,                 /* update func. */
    1,                       /* default */
    1,                       /* min */
    MAX_CLUSTER_CONNECTIONS, /* max */
    0                        /* block */
);

static MYSQL_SYSVAR_STR(
    cluster_connection_pool_nodeids, /* name */
    opt_connection_pool_nodeids_str, /* var */
    PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
    "Comma separated list of nodeids to use for the cluster connection pool. "
    "Overrides node id specified in --ndb-connectstring. First nodeid "
    "must be equal to --ndb-nodeid(if specified).",
    nullptr, /* check func. */
    nullptr, /* update func. */
    nullptr  /* default */
);

static MYSQL_SYSVAR_STR(tls_search_path,         /* name */
                        opt_ndb_tls_search_path, /* var */
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Directory containing NDB Cluster TLS Private Keys",
                        NULL, NULL, NDB_TLS_SEARCH_PATH);

static const char *tls_req_levels[] = {"relaxed", "strict", NullS};

static TYPELIB mgm_tls_typelib = {array_elements(tls_req_levels) - 1, "",
                                  tls_req_levels, nullptr};

static MYSQL_SYSVAR_ENUM(mgm_tls, opt_ndb_mgm_tls_level,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                         "MGM TLS Requirement level", nullptr, nullptr, 0,
                         &mgm_tls_typelib);

static const int MIN_ACTIVATION_THRESHOLD = 0;
static const int MAX_ACTIVATION_THRESHOLD = 16;

static int ndb_recv_thread_activation_threshold_check(
    THD *, SYS_VAR *, void *, struct st_mysql_value *value) {
  long long int_buf;
  int val = (int)value->val_int(value, &int_buf);
  int new_val = (int)int_buf;

  if (val != 0 || new_val < MIN_ACTIVATION_THRESHOLD ||
      new_val > MAX_ACTIVATION_THRESHOLD) {
    return 1;
  }
  opt_ndb_recv_thread_activation_threshold = new_val;
  return 0;
}

static void ndb_recv_thread_activation_threshold_update(THD *, SYS_VAR *,
                                                        void *, const void *) {
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
char ndb_recv_thread_cpu_mask_option_buf
    [ndb_recv_thread_cpu_mask_option_buf_size];
Uint16 recv_thread_cpuid_array[1 * MAX_CLUSTER_CONNECTIONS];

static int ndb_recv_thread_cpu_mask_check(THD *, SYS_VAR *, void *,
                                          struct st_mysql_value *value) {
  char buf[ndb_recv_thread_cpu_mask_option_buf_size];
  int len = sizeof(buf);
  const char *str = value->val_str(value, buf, &len);

  return ndb_recv_thread_cpu_mask_check_str(str);
}

static int ndb_recv_thread_cpu_mask_check_str(const char *str) {
  unsigned i;
  SparseBitmask bitmask;

  recv_thread_num_cpus = 0;
  if (str == nullptr) {
    /* Setting to empty string is interpreted as remove locking to CPU */
    return 0;
  }

  if (parse_mask(str, bitmask) < 0) {
    ndb_log_info(
        "Trying to set ndb_recv_thread_cpu_mask to"
        " illegal value = %s, ignored",
        str);
    goto error;
  }
  for (i = bitmask.find(0); i != SparseBitmask::NotFound;
       i = bitmask.find(i + 1)) {
    if (recv_thread_num_cpus == 1 * MAX_CLUSTER_CONNECTIONS) {
      ndb_log_info(
          "Trying to set too many CPU's in "
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

static int ndb_recv_thread_cpu_mask_update() {
  return ndb_set_recv_thread_cpu(recv_thread_cpuid_array, recv_thread_num_cpus);
}

static void ndb_recv_thread_cpu_mask_update_func(THD *, SYS_VAR *, void *,
                                                 const void *) {
  (void)ndb_recv_thread_cpu_mask_update();
}

static MYSQL_SYSVAR_STR(
    recv_thread_cpu_mask,         /* name */
    opt_ndb_recv_thread_cpu_mask, /* var */
    PLUGIN_VAR_RQCMDARG,
    "CPU mask for locking receiver threads to specific CPU, specified "
    " as hexadecimal as e.g. 0x33, one CPU is used per receiver thread.",
    ndb_recv_thread_cpu_mask_check,       /* check func. */
    ndb_recv_thread_cpu_mask_update_func, /* update func. */
    ndb_recv_thread_cpu_mask_option_buf);

static MYSQL_SYSVAR_STR(
    index_stat_option,         /* name */
    opt_ndb_index_stat_option, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Comma-separated tunable options for ndb index statistics",
    ndb_index_stat_option_check,  /* check func. */
    ndb_index_stat_option_update, /* update func. */
    ndb_index_stat_option_buf);

ulong opt_ndb_report_thresh_binlog_epoch_slip;
static MYSQL_SYSVAR_ULONG(
    report_thresh_binlog_epoch_slip,         /* name */
    opt_ndb_report_thresh_binlog_epoch_slip, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Threshold for Binlog injector thread consumption lag, "
    "before reporting the Event buffer status' message with reason "
    "BUFFERED_EPOCHS_OVER_THRESHOLD. "
    "The lag is defined as the number of epochs completely buffered in "
    "the event buffer, but not consumed by the Binlog injector thread yet.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    10,      /* default */
    0,       /* min */
    256,     /* max */
    0        /* block */
);

ulong opt_ndb_report_thresh_binlog_mem_usage;
static MYSQL_SYSVAR_ULONG(
    report_thresh_binlog_mem_usage,         /* name */
    opt_ndb_report_thresh_binlog_mem_usage, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Threshold on percentage of free memory before reporting binlog "
    "status. E.g. 10 means that if amount of available memory for "
    "receiving binlog data from the storage nodes goes below 10%, "
    "a status message will be sent to the cluster log.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    10,      /* default */
    0,       /* min */
    100,     /* max */
    0        /* block */
);

ulonglong opt_ndb_eventbuffer_max_alloc;
static MYSQL_SYSVAR_ULONGLONG(eventbuffer_max_alloc,         /* name */
                              opt_ndb_eventbuffer_max_alloc, /* var */
                              PLUGIN_VAR_RQCMDARG,
                              "Maximum amount of memory (in bytes) that can be "
                              "allocated for buffering events by the NdbApi.",
                              nullptr,   /* check func. */
                              nullptr,   /* update func. */
                              0,         /* default */
                              0,         /* min */
                              INT_MAX64, /* max */
                              0          /* block */
);

uint opt_ndb_eventbuffer_free_percent;
static MYSQL_SYSVAR_UINT(eventbuffer_free_percent,         /* name */
                         opt_ndb_eventbuffer_free_percent, /* var */
                         PLUGIN_VAR_RQCMDARG,
                         "Percentage of free memory that should be available "
                         "in event buffer before resuming buffering "
                         "after the max_alloc limit is hit.",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         20,      /* default */
                         1,       /* min */
                         99,      /* max */
                         0        /* block */
);

static MYSQL_SYSVAR_ULONG(
    row_checksum,         /* name */
    opt_ndb_row_checksum, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Create tables with a row checksum, this checks for HW issues at the"
    "expense of performance",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1,       /* default */
    0,       /* min */
    1,       /* max */
    0        /* block */
);

static MYSQL_SYSVAR_BOOL(
    fully_replicated,         /* name */
    opt_ndb_fully_replicated, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Create tables that are fully replicated by default. This enables reading"
    " from any data node when using ReadCommitted. This is great for read"
    " scalability but hampers write scalability",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

bool opt_ndb_metadata_check;
static MYSQL_SYSVAR_BOOL(
    metadata_check,         /* name */
    opt_ndb_metadata_check, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Enable the automatic detection of NDB metadata changes to be synchronized "
    "with the DD",
    nullptr, /* check func. */
    nullptr, /* update func. */
    true     /* default */
);

ulong opt_ndb_metadata_check_interval;
static void metadata_check_interval_update(THD *, SYS_VAR *, void *var_ptr,
                                           const void *save) {
  const ulong updated_interval = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = updated_interval;
  ndb_metadata_change_monitor_thread.set_check_interval(updated_interval);
}
static MYSQL_SYSVAR_ULONG(metadata_check_interval,         /* name */
                          opt_ndb_metadata_check_interval, /* var */
                          PLUGIN_VAR_RQCMDARG,
                          "Interval of time (in seconds) at which a check is "
                          "done to see if there are "
                          "NDB metadata changes to be synchronized",
                          nullptr,                        /* check func. */
                          metadata_check_interval_update, /* update func. */
                          60,                             /* default */
                          0,                              /* min */
                          ONE_YEAR_IN_SECONDS,            /* max */
                          0                               /* block */
);

bool opt_ndb_metadata_sync;
static void metadata_sync_update(THD *, SYS_VAR *, void *var_ptr,
                                 const void *save) {
  *static_cast<bool *>(var_ptr) = *static_cast<const ulong *>(save);
  ndb_metadata_change_monitor_thread.signal_metadata_sync_enabled();
}
static MYSQL_SYSVAR_BOOL(
    metadata_sync,         /* name */
    opt_ndb_metadata_sync, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Triggers immediate synchronization of all changes between NDB Dictionary "
    "and MySQL server. Setting this option results in the values of "
    "ndb_metadata_check and ndb_metadata_check_interval being ignored. "
    "Automatically resets to false when the synchronization has completed",
    nullptr,              /* check func. */
    metadata_sync_update, /* update func. */
    false                 /* default */
);

static MYSQL_SYSVAR_BOOL(
    read_backup,         /* name */
    opt_ndb_read_backup, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Create tables with Read Backup flag set. Enables those tables to be"
    " read from backup replicas as well as from primary replicas. Delays"
    " commit acknowledge of write transactions to accomplish this.",
    nullptr, /* check func.  */
    nullptr, /* update func. */
    1        /* default      */
);

static void ndb_data_node_neighbour_update_func(THD *, SYS_VAR *, void *var_ptr,
                                                const void *save) {
  const ulong data_node_neighbour = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = data_node_neighbour;
  ndb_set_data_node_neighbour(data_node_neighbour);
}

static MYSQL_SYSVAR_ULONG(
    data_node_neighbour,         /* name */
    opt_ndb_data_node_neighbour, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "My closest data node, if 0 no closest neighbour, used to select"
    " an appropriate data node to contact to run a transaction at.",
    nullptr,                             /* check func.  */
    ndb_data_node_neighbour_update_func, /* update func. */
    0,                                   /* default      */
    0,                                   /* min          */
    MAX_NDB_NODES,                       /* max          */
    0                                    /* block        */
);

bool opt_ndb_log_update_as_write;
static MYSQL_SYSVAR_BOOL(
    log_update_as_write,         /* name */
    opt_ndb_log_update_as_write, /* var */
    PLUGIN_VAR_OPCMDARG,
    "For efficiency log only after image as a write event. "
    "Ignore before image. This may cause compatibility problems if "
    "replicating to other storage engines than ndbcluster.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

bool opt_ndb_log_update_minimal;
static MYSQL_SYSVAR_BOOL(
    log_update_minimal,         /* name */
    opt_ndb_log_update_minimal, /* var */
    PLUGIN_VAR_OPCMDARG,
    "For efficiency, log updates in a minimal format"
    "Log only the primary key value(s) in the before "
    "image. Log only the changed columns in the after "
    "image. This may cause compatibility problems if "
    "replicating to other storage engines than ndbcluster.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

bool opt_ndb_log_updated_only;
static MYSQL_SYSVAR_BOOL(
    log_updated_only,         /* name */
    opt_ndb_log_updated_only, /* var */
    PLUGIN_VAR_OPCMDARG,
    "For efficiency log only updated columns. Columns are considered "
    "as \"updated\" even if they are updated with the same value. "
    "This may cause compatibility problems if "
    "replicating to other storage engines than ndbcluster.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

bool opt_ndb_log_empty_update;
static MYSQL_SYSVAR_BOOL(log_empty_update,         /* name */
                         opt_ndb_log_empty_update, /* var */
                         PLUGIN_VAR_OPCMDARG,
                         "Normally empty updates are filtered away "
                         "before they are logged. However, for read tracking "
                         "in conflict resolution a hidden pesudo attribute is "
                         "set which will result in an empty update along with "
                         "special flags set. For this to work empty updates "
                         "have to be allowed.",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

static int ndb_log_orig_check(THD *thd, SYS_VAR *sys_var, void *save,
                              st_mysql_value *value) {
  int r = check_func_bool(thd, sys_var, save, value);
  if (r == 0) {
    if (!opt_log_replica_updates) {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WRONG_VALUE_FOR_VAR,
                          "Variable 'ndb_log_orig' can't be changed when "
                          "'log_replica_updates' is OFF");
      return 1;
    }
  }
  return r;
}

bool opt_ndb_log_orig;
static MYSQL_SYSVAR_BOOL(
    log_orig,         /* name */
    opt_ndb_log_orig, /* var */
    PLUGIN_VAR_OPCMDARG,
    "Log originating server id and epoch in ndb_binlog_index. Each epoch "
    "may in this case have multiple rows in ndb_binlog_index, one for "
    "each originating epoch.",
    ndb_log_orig_check, /* check func. */
    nullptr,            /* update func. */
    0                   /* default */
);

bool opt_ndb_log_bin;
static MYSQL_SYSVAR_BOOL(
    log_bin,         /* name */
    opt_ndb_log_bin, /* var */
    PLUGIN_VAR_OPCMDARG,
    "Log NDB tables in the binary log. Option only has meaning if "
    "the binary log has been turned on for the server.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0        /* default */
);

bool opt_ndb_log_binlog_index;
static MYSQL_SYSVAR_BOOL(
    log_binlog_index,         /* name */
    opt_ndb_log_binlog_index, /* var */
    PLUGIN_VAR_OPCMDARG,
    "Insert mapping between epochs and binlog positions into the "
    "ndb_binlog_index table.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

bool opt_ndb_log_empty_epochs;
static MYSQL_SYSVAR_BOOL(log_empty_epochs,                 /* name */
                         opt_ndb_log_empty_epochs,         /* var */
                         PLUGIN_VAR_OPCMDARG, "", nullptr, /* check func. */
                         nullptr,                          /* update func. */
                         0                                 /* default */
);

static int ndb_log_apply_status_check(THD *thd, SYS_VAR *sys_var, void *save,
                                      st_mysql_value *value) {
  int r = check_func_bool(thd, sys_var, save, value);
  if (r == 0) {
    if (!opt_log_replica_updates) {
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE_FOR_VAR,
          "Variable 'ndb_log_apply_status' can't be changed when "
          "'log_replica_updates' is OFF");
      return 1;
    }
  }
  return r;
}

bool opt_ndb_log_apply_status;
static MYSQL_SYSVAR_BOOL(
    log_apply_status,         /* name */
    opt_ndb_log_apply_status, /* var */
    PLUGIN_VAR_OPCMDARG,
    "Log ndb_apply_status updates from Master in the Binlog",
    ndb_log_apply_status_check, /* check func. */
    nullptr,                    /* update func. */
    0                           /* default */
);

bool opt_ndb_log_transaction_id;
static MYSQL_SYSVAR_BOOL(log_transaction_id,         /* name */
                         opt_ndb_log_transaction_id, /* var  */
                         PLUGIN_VAR_OPCMDARG,
                         "Log Ndb transaction identities per row in the Binlog",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

bool opt_ndb_log_trx_compression;
static MYSQL_SYSVAR_BOOL(log_transaction_compression, /* name */
                         opt_ndb_log_trx_compression, /* var */
                         PLUGIN_VAR_OPCMDARG, "Compress the Ndb Binlog",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

uint opt_ndb_log_trx_compression_level_zstd;
static MYSQL_SYSVAR_UINT(log_transaction_compression_level_zstd, /* name */
                         opt_ndb_log_trx_compression_level_zstd, /* var */
                         PLUGIN_VAR_OPCMDARG,
                         "Compression level for ZSTD transaction "
                         "compression in the NDB Binlog.",
                         nullptr,                        /* check func. */
                         nullptr,                        /* update func. */
                         DEFAULT_ZSTD_COMPRESSION_LEVEL, /* default */
                         1,                              /* min */
                         22,                             /* max */
                         0);

ulong opt_ndb_log_purge_rate;
static MYSQL_SYSVAR_ULONG(
    log_purge_rate,         /* name */
    opt_ndb_log_purge_rate, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Rate of rows to delete when purging rows from ndb_binlog_index.",
    nullptr,     /* check func. */
    nullptr,     /* update func. */
    8192,        /* default */
    1,           /* min */
    1024 * 1024, /* max */
    0            /* block */
);

// Overrides --binlog-cache-size for the ndb binlog thread
ulong opt_ndb_log_cache_size;
static void fix_ndb_log_cache_size(THD *thd, SYS_VAR *, void *val_ptr,
                                   const void *checked) {
  ulong new_size = *static_cast<const ulong *>(checked);

  // Cap the max value in the same way as other binlog cache size variables
  if (new_size > max_binlog_cache_size) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX,
        "Option ndb_log_cache_size (%lu) is greater than max_binlog_cache_size "
        "(%lu); setting ndb_log_cache_size equal to max_binlog_cache_size.",
        (ulong)new_size, (ulong)max_binlog_cache_size);
    new_size = static_cast<ulong>(max_binlog_cache_size);
  }
  *(static_cast<ulong *>(val_ptr)) = new_size;
}

static MYSQL_SYSVAR_ULONG(
    log_cache_size,         /* name */
    opt_ndb_log_cache_size, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Size of the binary log transaction cache used by NDB binlog",
    nullptr,                /* check func. */
    fix_ndb_log_cache_size, /* update func. */
    64 * 1024 * 1024,       /* default */
    IO_SIZE,                /* min */
    ULONG_MAX,              /* max */
    IO_SIZE                 /* block */
);

bool opt_ndb_clear_apply_status;
static MYSQL_SYSVAR_BOOL(
    clear_apply_status,         /* name */
    opt_ndb_clear_apply_status, /* var  */
    PLUGIN_VAR_OPCMDARG,
    "Whether RESET REPLICA will clear all entries in ndb_apply_status",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1        /* default */
);

bool opt_ndb_applier_allow_skip_epoch;
static MYSQL_SYSVAR_BOOL(applier_allow_skip_epoch,         /* name */
                         opt_ndb_applier_allow_skip_epoch, /* var */
                         PLUGIN_VAR_OPCMDARG,
                         "Should replication applier be "
                         "allowed to skip epochs",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

bool opt_ndb_schema_dist_upgrade_allowed;
static MYSQL_SYSVAR_BOOL(
    schema_dist_upgrade_allowed,         /* name */
    opt_ndb_schema_dist_upgrade_allowed, /* var  */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
    "Allow schema distribution table upgrade when connecting to NDB. Use this "
    "variable to defer this change until all MySQL Servers connected to the "
    "cluster have been upgrade to same version. NOTE! The schema distribution "
    "functionality might be slightly degraded until the change has been "
    "performed.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    true     /* default */
);

int opt_ndb_schema_dist_timeout;
static MYSQL_SYSVAR_INT(
    schema_dist_timeout,         /* name */
    opt_ndb_schema_dist_timeout, /* var  */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
    "Controls how many seconds it takes before timeout is detected during "
    "schema distribution. Timeout might indicate that activity on the other "
    "MySQL Server(s) are high or are somehow prevented from acquiring the "
    "necessary resources at this time.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    120,     /* default */
    5,       /* min */
    1200,    /* max */
    0        /* block */
);

ulong opt_ndb_schema_dist_lock_wait_timeout;
static MYSQL_SYSVAR_ULONG(
    schema_dist_lock_wait_timeout,         /* name */
    opt_ndb_schema_dist_lock_wait_timeout, /* var */
    PLUGIN_VAR_RQCMDARG,
    "Time (in seconds) during schema distribution to wait for a lock before "
    "returning an error. This setting allows avoiding that the binlog "
    "injector thread waits too long while handling schema operations.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    30,      /* default */
    0,       /* min */
    1200,    /* max */
    0        /* block */
);

static MYSQL_SYSVAR_STR(connectstring,         /* name */
                        opt_ndb_connectstring, /* var */
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Connect string for ndbcluster.",
                        nullptr, /* check func. */
                        nullptr, /* update func. */
                        nullptr  /* default */
);

bool opt_ndb_log_fail_terminate;
static MYSQL_SYSVAR_BOOL(log_fail_terminate,         /* name */
                         opt_ndb_log_fail_terminate, /* var  */
                         PLUGIN_VAR_OPCMDARG,
                         "Terminate mysqld if complete logging of all found "
                         "row events is not possible",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

bool opt_ndb_log_trans_dependency;
static MYSQL_SYSVAR_BOOL(log_transaction_dependency,   /* name */
                         opt_ndb_log_trans_dependency, /* var */
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
                         "Enable transaction dependency extraction for NDB "
                         "changes written to the binlog.",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0        /* default */
);

static MYSQL_SYSVAR_STR(mgmd_host,             /* name */
                        opt_ndb_connectstring, /* var */
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Same as --ndb-connectstring",
                        nullptr, /* check func. */
                        nullptr, /* update func. */
                        nullptr  /* default */
);

static MYSQL_SYSVAR_UINT(
    nodeid,         /* name */
    opt_ndb_nodeid, /* var */
    PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
    "Set nodeid for this node. Overrides node id specified "
    "in --ndb-connectstring.",
    nullptr,      /* check func. */
    nullptr,      /* update func. */
    0,            /* default */
    0,            /* min */
    MAX_NODES_ID, /* max */
    0             /* block */
);

static bool checkSlaveConflictRoleChange(enum_slave_conflict_role old_role,
                                         enum_slave_conflict_role new_role,
                                         const char **failure_cause) {
  if (old_role == new_role) return true;

  /**
   * Initial role is SCR_NONE
   * Allowed transitions :
   *   SCR_NONE -> SCR_PASS
   *   SCR_NONE -> SCR_PRIMARY
   *   SCR_NONE -> SCR_SECONDARY
   *   SCR_PRIMARY -> SCR_NONE
   *   SCR_PRIMARY -> SCR_SECONDARY
   *   SCR_SECONDARY -> SCR_NONE
   *   SCR_SECONDARY -> SCR_PRIMARY
   *   SCR_PASS -> SCR_NONE
   *
   * Disallowed transitions
   *   SCR_PASS -> SCR_PRIMARY
   *   SCR_PASS -> SCR_SECONDARY
   *   SCR_PRIMARY -> SCR_PASS
   *   SCR_SECONDARY -> SCR_PASS
   */
  bool bad_transition = false;
  *failure_cause = "Internal error";

  switch (old_role) {
    case SCR_NONE:
      break;
    case SCR_PRIMARY:
    case SCR_SECONDARY:
      bad_transition = (new_role == SCR_PASS);
      break;
    case SCR_PASS:
      bad_transition =
          ((new_role == SCR_PRIMARY) || (new_role == SCR_SECONDARY));
      break;
    default:
      assert(false);
      return false;
  }

  if (bad_transition) {
    *failure_cause = "Invalid role change.";
    return false;
  }

  /* Don't allow changing role while any Ndb_replica channel is started */
  if (ndb_replica->num_started_channels() > 0) {
    *failure_cause =
        "Cannot change role while Replica SQL "
        "thread is running.  Use STOP REPLICA first.";
    return false;
  }

  return true;
}

static const char *slave_conflict_role_names[] = {"NONE", "SECONDARY",
                                                  "PRIMARY", "PASS", NullS};

static TYPELIB slave_conflict_role_typelib = {
    array_elements(slave_conflict_role_names) - 1, "",
    slave_conflict_role_names, nullptr};

/**
 * slave_conflict_role_check_func.
 *
 * Perform most validation of a role change request.
 * Inspired by sql_plugin.cc::check_func_enum()
 */
static int slave_conflict_role_check_func(THD *thd, SYS_VAR *, void *save,
                                          st_mysql_value *value) {
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  long long tmp;
  long result;
  int length;

  do {
    if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING) {
      length = sizeof(buff);
      if (!(str = value->val_str(value, buff, &length))) break;
      if ((result = (long)find_type(str, &slave_conflict_role_typelib, 0) - 1) <
          0)
        break;
    } else {
      if (value->val_int(value, &tmp)) break;
      if (tmp < 0 ||
          tmp >= static_cast<long long>(slave_conflict_role_typelib.count))
        break;
      result = (long)tmp;
    }

    const char *failure_cause_str = nullptr;
    if (!checkSlaveConflictRoleChange(
            (enum_slave_conflict_role)opt_ndb_slave_conflict_role,
            (enum_slave_conflict_role)result, &failure_cause_str)) {
      char msgbuf[256];
      snprintf(
          msgbuf, sizeof(msgbuf), "Role change from %s to %s failed : %s",
          get_type(&slave_conflict_role_typelib, opt_ndb_slave_conflict_role),
          get_type(&slave_conflict_role_typelib, result), failure_cause_str);

      thd->raise_error_printf(ER_ERROR_WHEN_EXECUTING_COMMAND,
                              "SET GLOBAL ndb_slave_conflict_role", msgbuf);

      break;
    }

    /* Ok */
    *(long *)save = result;
    return 0;
  } while (0);
  /* Error */
  return 1;
}

/**
 * applier_conflict_role_check_func.
 *
 * Perform most validation of a role change request.
 * Inspired by sql_plugin.cc::check_func_enum()
 */
static int applier_conflict_role_check_func(THD *thd, SYS_VAR *, void *save,
                                            st_mysql_value *value) {
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  long result;
  int length;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING) {
    length = sizeof(buff);
    if (!(str = value->val_str(value, buff, &length))) return 1;
    if ((result = static_cast<long>(
             find_type(str, &slave_conflict_role_typelib, 0) - 1)) < 0)
      return 1;
  } else {
    long long tmp;
    if (value->val_int(value, &tmp)) return 1;
    if (tmp < 0 ||
        tmp >= static_cast<long long>(slave_conflict_role_typelib.count))
      return 1;
    result = static_cast<long>(tmp);
  }

  const char *failure_cause_str = nullptr;
  if (!checkSlaveConflictRoleChange(
          (enum_slave_conflict_role)opt_ndb_applier_conflict_role,
          (enum_slave_conflict_role)result, &failure_cause_str)) {
    char msgbuf[256];
    snprintf(
        msgbuf, sizeof(msgbuf), "Role change from %s to %s failed : %s",
        get_type(&slave_conflict_role_typelib, opt_ndb_applier_conflict_role),
        get_type(&slave_conflict_role_typelib, result), failure_cause_str);

    thd->raise_error_printf(ER_ERROR_WHEN_EXECUTING_COMMAND,
                            "SET GLOBAL ndb_applier_conflict_role", msgbuf);

    return 1;
  }

  /* Ok */
  *(long *)save = result;
  return 0;
}

/**
 * applier_conflict_role_update_func
 *
 * Perform actual change of role, using saved 'long' enum value
 * prepared by the update func above.
 *
 * Inspired by sql_plugin.cc::update_func_long()
 */
static void applier_conflict_role_update_func(THD *, SYS_VAR *, void *tgt,
                                              const void *save) {
  *(long *)tgt = *static_cast<const long *>(save);
  opt_ndb_slave_conflict_role = *static_cast<const long *>(save);
}

/**
 * slave_conflict_role_update_func
 *
 * Perform actual change of role, using saved 'long' enum value
 * prepared by the update func above.
 *
 * Inspired by sql_plugin.cc::update_func_long()
 */
static void slave_conflict_role_update_func(THD *thd, SYS_VAR *, void *tgt,
                                            const void *save) {
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                      ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX),
                      "ndb_slave_conflict_role", "ndb_applier_conflict_role");
  *(long *)tgt = *static_cast<const long *>(save);
  opt_ndb_applier_conflict_role = *static_cast<const long *>(save);
}

static MYSQL_SYSVAR_ENUM(
    slave_conflict_role,         /* Name */
    opt_ndb_slave_conflict_role, /* Var */
    PLUGIN_VAR_RQCMDARG,
    "Role for applier to play in asymmetric conflict algorithms. "
    "This variable is deprecated and will be removed in a future release. Use "
    "ndb_applier_conflict_role instead",
    slave_conflict_role_check_func,  /* Check func */
    slave_conflict_role_update_func, /* Update func */
    SCR_NONE,                        /* Default value */
    &slave_conflict_role_typelib     /* typelib */
);

/*
  opt_ndb_applier_conflict_role is a hack and is used only to identify if the
  user sets slave_conflict_role during startup. pugin code only uses
  opt_ndb_slave_conflict_role. So, if one of the variables is changed then
  other variable also need to be set to the same value.
*/
static MYSQL_SYSVAR_ENUM(
    applier_conflict_role,         /* Name */
    opt_ndb_applier_conflict_role, /* Var */
    PLUGIN_VAR_RQCMDARG,
    "Role for applier to play in asymmetric conflict algorithms.",
    applier_conflict_role_check_func,  /* Check func */
    applier_conflict_role_update_func, /* Update func */
    SCR_NONE,                          /* Default value */
    &slave_conflict_role_typelib       /* typelib */
);

#ifndef NDEBUG

static void dbg_check_shares_update(THD *, SYS_VAR *, void *, const void *) {
  NDB_SHARE::dbg_check_shares_update();
}

static MYSQL_THDVAR_UINT(dbg_check_shares, /* name */
                         PLUGIN_VAR_RQCMDARG,
                         "Debug, only...check that no shares are lingering...",
                         nullptr,                 /* check func */
                         dbg_check_shares_update, /* update func */
                         0,                       /* default */
                         0,                       /* min */
                         1,                       /* max */
                         0                        /* block */
);

#endif

static SYS_VAR *system_variables[] = {
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
    MYSQL_SYSVAR(replica_batch_size),
    MYSQL_SYSVAR(optimization_delay),
    MYSQL_SYSVAR(index_stat_enable),
    MYSQL_SYSVAR(index_stat_option),
    MYSQL_SYSVAR(tls_search_path),
    MYSQL_SYSVAR(mgm_tls),
    MYSQL_SYSVAR(table_no_logging),
    MYSQL_SYSVAR(table_temporary),
    MYSQL_SYSVAR(log_bin),
    MYSQL_SYSVAR(log_binlog_index),
    MYSQL_SYSVAR(log_empty_epochs),
    MYSQL_SYSVAR(log_apply_status),
    MYSQL_SYSVAR(log_transaction_id),
    MYSQL_SYSVAR(log_transaction_compression),
    MYSQL_SYSVAR(log_transaction_compression_level_zstd),
    MYSQL_SYSVAR(log_purge_rate),
    MYSQL_SYSVAR(log_cache_size),
    MYSQL_SYSVAR(log_fail_terminate),
    MYSQL_SYSVAR(log_transaction_dependency),
    MYSQL_SYSVAR(clear_apply_status),
    MYSQL_SYSVAR(schema_dist_upgrade_allowed),
    MYSQL_SYSVAR(schema_dist_timeout),
    MYSQL_SYSVAR(schema_dist_lock_wait_timeout),
    MYSQL_SYSVAR(connectstring),
    MYSQL_SYSVAR(mgmd_host),
    MYSQL_SYSVAR(nodeid),
    MYSQL_SYSVAR(blob_read_batch_bytes),
    MYSQL_SYSVAR(blob_write_batch_bytes),
    MYSQL_SYSVAR(replica_blob_write_batch_bytes),
    MYSQL_SYSVAR(deferred_constraints),
    MYSQL_SYSVAR(join_pushdown),
    MYSQL_SYSVAR(log_exclusive_reads),
    MYSQL_SYSVAR(read_backup),
    MYSQL_SYSVAR(data_node_neighbour),
    MYSQL_SYSVAR(fully_replicated),
    MYSQL_SYSVAR(row_checksum),
#ifndef NDEBUG
    MYSQL_SYSVAR(dbg_check_shares),
#endif
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(version_string),
    MYSQL_SYSVAR(show_foreign_key_mock_tables),
    MYSQL_SYSVAR(slave_conflict_role),
    MYSQL_SYSVAR(applier_conflict_role),
    MYSQL_SYSVAR(default_column_format),
    MYSQL_SYSVAR(metadata_check),
    MYSQL_SYSVAR(metadata_check_interval),
    MYSQL_SYSVAR(metadata_sync),
    MYSQL_SYSVAR(applier_allow_skip_epoch),
    nullptr};

struct st_mysql_storage_engine ndbcluster_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

extern struct st_mysql_plugin ndbinfo_plugin;

mysql_declare_plugin(ndbcluster){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &ndbcluster_storage_engine,
    ndbcluster_hton_name,
    PLUGIN_AUTHOR_ORACLE,
    "Clustered, fault-tolerant tables",
    PLUGIN_LICENSE_GPL,
    ndbcluster_init,   /* plugin init */
    nullptr,           /* plugin check uninstall */
    ndbcluster_deinit, /* plugin deinit */
    0x0100,            /* plugin version */
    ndb_status_vars,   /* status variables */
    system_variables,  /* system variables */
    nullptr,           /* config options */
    PLUGIN_OPT_DEFAULT_OFF | PLUGIN_OPT_DEPENDENT_EXTRA_PLUGINS /* flags */
},
    ndbinfo_plugin,
    ndb_transid_mysql_connection_map_table mysql_declare_plugin_end;
