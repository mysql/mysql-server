/* Copyright (C) 2007 Google Inc.
   Copyright (C) 2008 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include "semisync_slave.h"
#include <mysql.h>

ReplSemiSyncSlave repl_semisync;

/*
  indicate whether or not the slave should send a reply to the master.

  This is set to true in repl_semi_slave_read_event if the current
  event read is the last event of a transaction. And the value is
  checked in repl_semi_slave_queue_event.
*/
bool semi_sync_need_reply= false;

C_MODE_START

int repl_semi_reset_slave(Binlog_relay_IO_param *param)
{
  // TODO: reset semi-sync slave status here
  return 0;
}

int repl_semi_slave_request_dump(Binlog_relay_IO_param *param,
				 uint32 flags)
{
  MYSQL *mysql= param->mysql;
  MYSQL_RES *res= 0;
  MYSQL_ROW row;
  const char *query;

  if (!repl_semisync.getSlaveEnabled())
    return 0;

  /* Check if master server has semi-sync plugin installed */
  query= "SHOW VARIABLES LIKE 'rpl_semi_sync_master_enabled'";
  if (mysql_real_query(mysql, query, strlen(query)) ||
      !(res= mysql_store_result(mysql)))
  {
    sql_print_error("Execution failed on master: %s", query);
    return 1;
  }

  row= mysql_fetch_row(res);
  if (!row)
  {
    /* Master does not support semi-sync */
    sql_print_warning("Master server does not support semi-sync, "
                      "fallback to asynchronous replication");
    rpl_semi_sync_slave_status= 0;
    mysql_free_result(res);
    return 0;
  }
  mysql_free_result(res);

  /*
    Tell master dump thread that we want to do semi-sync
    replication
  */
  query= "SET @rpl_semi_sync_slave= 1";
  if (mysql_real_query(mysql, query, strlen(query)))
  {
    sql_print_error("Set 'rpl_semi_sync_slave=1' on master failed");
    return 1;
  }
  mysql_free_result(mysql_store_result(mysql));
  rpl_semi_sync_slave_status= 1;
  return 0;
}

int repl_semi_slave_read_event(Binlog_relay_IO_param *param,
			       const char *packet, unsigned long len,
			       const char **event_buf, unsigned long *event_len)
{
  if (rpl_semi_sync_slave_status)
    return repl_semisync.slaveReadSyncHeader(packet, len,
					     &semi_sync_need_reply,
					     event_buf, event_len);
  *event_buf= packet;
  *event_len= len;
  return 0;
}

int repl_semi_slave_queue_event(Binlog_relay_IO_param *param,
				const char *event_buf,
				unsigned long event_len,
				uint32 flags)
{
  if (rpl_semi_sync_slave_status && semi_sync_need_reply)
  {
    /*
      We deliberately ignore the error in slaveReply, such error
      should not cause the slave IO thread to stop, and the error
      messages are already reported.
    */
    (void) repl_semisync.slaveReply(param->mysql,
                                    param->master_log_name,
                                    param->master_log_pos);
  }
  return 0;
}

int repl_semi_slave_io_start(Binlog_relay_IO_param *param)
{
  return repl_semisync.slaveStart(param);
}

int repl_semi_slave_io_end(Binlog_relay_IO_param *param)
{
  return repl_semisync.slaveStop(param);
}

C_MODE_END

static void fix_rpl_semi_sync_slave_enabled(MYSQL_THD thd,
					    SYS_VAR *var,
					    void *ptr,
					    const void *val)
{
  *(char *)ptr= *(char *)val;
  repl_semisync.setSlaveEnabled(rpl_semi_sync_slave_enabled != 0);
  return;
}

static void fix_rpl_semi_sync_trace_level(MYSQL_THD thd,
					  SYS_VAR *var,
					  void *ptr,
					  const void *val)
{
  *(unsigned long *)ptr= *(unsigned long *)val;
  repl_semisync.setTraceLevel(rpl_semi_sync_slave_trace_level);
  return;
}

/* plugin system variables */
static MYSQL_SYSVAR_BOOL(enabled, rpl_semi_sync_slave_enabled,
  PLUGIN_VAR_OPCMDARG,
 "Enable semi-synchronous replication slave (disabled by default). ",
  NULL,				   // check
  &fix_rpl_semi_sync_slave_enabled, // update
  0);

static MYSQL_SYSVAR_ULONG(trace_level, rpl_semi_sync_slave_trace_level,
  PLUGIN_VAR_OPCMDARG,
 "The tracing level for semi-sync replication.",
  NULL,				  // check
  &fix_rpl_semi_sync_trace_level, // update
  32, 0, ~0UL, 1);

static SYS_VAR* semi_sync_slave_system_vars[]= {
  MYSQL_SYSVAR(enabled),
  MYSQL_SYSVAR(trace_level),
  NULL,
};


/* plugin status variables */
static SHOW_VAR semi_sync_slave_status_vars[]= {
  {"Rpl_semi_sync_slave_status",
   (char*) &rpl_semi_sync_slave_status,    SHOW_BOOL},
  {NULL, NULL, SHOW_BOOL},
};

Binlog_relay_IO_observer relay_io_observer = {
  sizeof(Binlog_relay_IO_observer), // len

  repl_semi_slave_io_start,	// start
  repl_semi_slave_io_end,	// stop
  repl_semi_slave_request_dump,	// request_transmit
  repl_semi_slave_read_event,	// after_read_event
  repl_semi_slave_queue_event,	// after_queue_event
  repl_semi_reset_slave,	// reset
};

static int semi_sync_slave_plugin_init(void *p)
{
  if (repl_semisync.initObject())
    return 1;
  if (register_binlog_relay_io_observer(&relay_io_observer, p))
    return 1;
  return 0;
}

static int semi_sync_slave_plugin_deinit(void *p)
{
  if (unregister_binlog_relay_io_observer(&relay_io_observer, p))
    return 1;
  return 0;
}


struct Mysql_replication semi_sync_slave_plugin= {
  MYSQL_REPLICATION_INTERFACE_VERSION
};

/*
  Plugin library descriptor
*/
mysql_declare_plugin(semi_sync_slave)
{
  MYSQL_REPLICATION_PLUGIN,
  &semi_sync_slave_plugin,
  "rpl_semi_sync_slave",
  "He Zhenxing",
  "Semi-synchronous replication slave",
  PLUGIN_LICENSE_GPL,
  semi_sync_slave_plugin_init, /* Plugin Init */
  semi_sync_slave_plugin_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  semi_sync_slave_status_vars,	/* status variables */
  semi_sync_slave_system_vars,	/* system variables */
  NULL,                         /* config options */
  0,                            /* flags */
}
mysql_declare_plugin_end;
