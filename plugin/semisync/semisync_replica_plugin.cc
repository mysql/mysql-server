/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2008, 2026, Oracle and/or its affiliates.

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

#include <assert.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "my_macros.h"
#include "plugin/semisync/semisync_replica.h"
#include "sql/current_thd.h"
#include "sql/derror.h"       // ER_THD
#include "sql/raii/sentry.h"  // raii::Sentry
#include "sql/sql_error.h"
#include "sql/sql_lex.h"  // thd->lex

ReplSemiSyncReplica *repl_semisync = nullptr;

/*
  indicate whether or not the replica should send a reply to the source.

  This is set to true in repl_semi_replica_read_event if the current
  event read is the last event of a transaction. And the value is
  checked in repl_semi_replica_queue_event.
*/
bool semi_sync_need_reply = false;

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static int repl_semi_apply_replica(Binlog_relay_IO_param *, Trans_param *,
                                   int &) {
  // TODO: implement
  return 0;
}

static int repl_semi_reset_replica(Binlog_relay_IO_param *) {
  // TODO: reset semi-sync replica status here
  return 0;
}

/**
  Send a query to source server to determine if it supports semisync.

  This checks if rpl_semi_sync_source_enabled is defined on the source.

  @param mysql Existing connection to the source server.

  @retval 1 Source supports semisync.

  @retval 0 Source does not support semisync.

  @retval -1 Error occurred while checking if source supports semisync.
  This function reports an error to the log in this case.
*/
static int has_source_semisync(MYSQL *mysql) {
  /* Check if source server has semi-sync plugin installed */
  std::string query = "SELECT @@global.rpl_semi_sync_source_enabled";
  if (mysql_real_query(mysql, query.c_str(),
                       static_cast<ulong>(query.length()))) {
    uint mysql_error = mysql_errno(mysql);
    if (mysql_error == ER_UNKNOWN_SYSTEM_VARIABLE)
      return 0;
    else {
      LogPluginErr(ERROR_LEVEL, ER_SEMISYNC_EXECUTION_FAILED_ON_SOURCE,
                   query.c_str(), mysql_error);
      return -1;
    }
  }
  /* Mandatory ritual required to reset the connection state */
  MYSQL_RES *res = mysql_store_result(mysql);
  (void)mysql_fetch_row(res);
  mysql_free_result(res);

  return 1;
}

static int repl_semi_replica_request_dump(Binlog_relay_IO_param *param,
                                          uint32) {
  MYSQL *mysql = param->mysql;

  if (!repl_semisync->getReplicaEnabled()) return 0;

  int source_state = has_source_semisync(mysql);
  if (source_state == 0) {
    /* Source does not support semi-sync */
    LogPluginErr(WARNING_LEVEL, ER_SEMISYNC_NOT_SUPPORTED_BY_SOURCE);
    rpl_semi_sync_replica_status = 0;
    return 0;
  }
  if (source_state == -1) return 1;

  /*
    Tell source dump thread that we want to do semi-sync
    replication
  */
  const char *query = "SET @rpl_semi_sync_replica = 1";
  if (mysql_real_query(mysql, query, static_cast<ulong>(strlen(query)))) {
    LogPluginErr(ERROR_LEVEL, ER_SEMISYNC_REPLICA_SET_FAILED);
    return 1;
  }
  mysql_free_result(mysql_store_result(mysql));
  rpl_semi_sync_replica_status = 1;
  return 0;
}

static int repl_semi_replica_read_event(Binlog_relay_IO_param *,
                                        const char *packet, unsigned long len,
                                        const char **event_buf,
                                        unsigned long *event_len) {
  if (rpl_semi_sync_replica_status)
    return repl_semisync->replicaReadSyncHeader(
        packet, len, &semi_sync_need_reply, event_buf, event_len);
  *event_buf = packet;
  *event_len = len;
  return 0;
}

static int repl_semi_replica_queue_event(Binlog_relay_IO_param *param,
                                         const char *, unsigned long, uint32) {
  if (rpl_semi_sync_replica_status && semi_sync_need_reply) {
    /*
      We deliberately ignore the error in replicaReply, such error
      should not cause the replica IO thread to stop, and the error
      messages are already reported.
    */
    (void)repl_semisync->replicaReply(param->mysql, param->master_log_name,
                                      param->master_log_pos);
  }
  return 0;
}

static int repl_semi_replica_io_start(Binlog_relay_IO_param *param) {
  return repl_semisync->replicaStart(param);
}

static int repl_semi_replica_io_end(Binlog_relay_IO_param *param) {
  return repl_semisync->replicaStop(param);
}

int repl_semi_replica_sql_start(Binlog_relay_IO_param *) { return 0; }

static int repl_semi_replica_sql_stop(Binlog_relay_IO_param *, bool) {
  return 0;
}

static void fix_rpl_semi_sync_replica_enabled(MYSQL_THD, SYS_VAR *, void *ptr,
                                              const void *val) {
  *static_cast<char *>(ptr) = *static_cast<const char *>(val);
  repl_semisync->setReplicaEnabled(rpl_semi_sync_replica_enabled != 0);
}

static void fix_rpl_semi_sync_trace_level(MYSQL_THD, SYS_VAR *, void *ptr,
                                          const void *val) {
  *static_cast<unsigned long *>(ptr) = *static_cast<const unsigned long *>(val);
  repl_semisync->setTraceLevel(rpl_semi_sync_replica_trace_level);
}

/* plugin system variables */
static MYSQL_SYSVAR_BOOL(
    enabled, rpl_semi_sync_replica_enabled, PLUGIN_VAR_OPCMDARG,
    "Enable semi-synchronous replication on this replica (disabled by "
    "default). ",
    nullptr,                             // check
    &fix_rpl_semi_sync_replica_enabled,  // update
    0);

static MYSQL_SYSVAR_ULONG(trace_level, rpl_semi_sync_replica_trace_level,
                          PLUGIN_VAR_OPCMDARG,
                          "The tracing level for semi-sync replication.",
                          nullptr,                         // check
                          &fix_rpl_semi_sync_trace_level,  // update
                          32, 0, ~0UL, 1);

static SYS_VAR *semi_sync_replica_system_vars[] = {
    MYSQL_SYSVAR(enabled),
    MYSQL_SYSVAR(trace_level),
    nullptr,
};

/* plugin status variables */
static SHOW_VAR semi_sync_replica_status_vars[] = {
    {"Rpl_semi_sync_replica_status", (char *)&rpl_semi_sync_replica_status,
     SHOW_BOOL, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_BOOL, SHOW_SCOPE_GLOBAL},
};

Binlog_relay_IO_observer relay_io_observer = {
    sizeof(Binlog_relay_IO_observer),  // len

    repl_semi_replica_io_start,      // start
    repl_semi_replica_io_end,        // stop
    repl_semi_replica_sql_start,     // start sql thread
    repl_semi_replica_sql_stop,      // stop sql thread
    repl_semi_replica_request_dump,  // request_transmit
    repl_semi_replica_read_event,    // after_read_event
    repl_semi_replica_queue_event,   // after_queue_event
    repl_semi_reset_replica,         // reset
    repl_semi_apply_replica          // apply
};

static int semi_sync_replica_plugin_init(void *p) {
  // Initialize error logging service.
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  bool success = false;
  raii::Sentry<> logging_service_guard{[&]() {
    if (!success) deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  }};

  repl_semisync = new ReplSemiSyncReplica();
  if (repl_semisync->initObject()) return 1;
  if (register_binlog_relay_io_observer(&relay_io_observer, p)) return 1;

  success = true;
  return 0;
}

static int semi_sync_replica_plugin_check_uninstall(void *) {
  int ret = rpl_semi_sync_replica_status ? 1 : 0;
  if (ret) {
    my_error(
        ER_PLUGIN_CANNOT_BE_UNINSTALLED, MYF(0), "rpl_semi_sync_replica",
        "Stop any active semisynchronous I/O threads on this replica first.");
  }
  return ret;
}

static int semi_sync_replica_plugin_deinit(void *p) {
  if (unregister_binlog_relay_io_observer(&relay_io_observer, p)) return 1;
  delete repl_semisync;
  repl_semisync = nullptr;
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

struct Mysql_replication semi_sync_replica_plugin = {
    MYSQL_REPLICATION_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(semi_sync_replica){
    MYSQL_REPLICATION_PLUGIN,
    &semi_sync_replica_plugin,
    "rpl_semi_sync_replica",
    PLUGIN_AUTHOR_ORACLE,
    "Replica-side semi-synchronous replication.",
    PLUGIN_LICENSE_GPL,
    semi_sync_replica_plugin_init,            /* Plugin Init */
    semi_sync_replica_plugin_check_uninstall, /* Plugin Check uninstall */
    semi_sync_replica_plugin_deinit,          /* Plugin Deinit */
    0x0100 /* 1.0 */,
    semi_sync_replica_status_vars, /* status variables */
    semi_sync_replica_system_vars, /* system variables */
    nullptr,                       /* config options */
    0,                             /* flags */
} mysql_declare_plugin_end;
