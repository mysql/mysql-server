/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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


#include "semisync_master.h"
#include "sql_class.h"                          // THD
#include "semisync_master_ack_receiver.h"

ReplSemiSyncMaster repl_semisync;
Ack_receiver ack_receiver;

/* The places at where semisync waits for binlog ACKs. */
enum enum_wait_point {
  WAIT_AFTER_SYNC,
  WAIT_AFTER_COMMIT
};

static ulong rpl_semi_sync_master_wait_point= WAIT_AFTER_COMMIT;

static bool SEMI_SYNC_DUMP= true;

thread_local_key_t THR_RPL_SEMI_SYNC_DUMP;

static inline bool is_semi_sync_dump()
{
  /*
    The key is only set for semisync dump threads, so it just checks if
    the key is not NULL.
  */
  return my_get_thread_local(THR_RPL_SEMI_SYNC_DUMP) != NULL;
}

C_MODE_START

int repl_semi_report_binlog_update(Binlog_storage_param *param,
				   const char *log_file,
				   my_off_t log_pos)
{
  int  error= 0;

  if (repl_semisync.getMasterEnabled())
  {
    /*
      Let us store the binlog file name and the position, so that
      we know how long to wait for the binlog to the replicated to
      the slave in synchronous replication.
    */
    error= repl_semisync.writeTranxInBinlog(log_file,
                                            log_pos);
  }

  return error;
}

int repl_semi_report_binlog_sync(Binlog_storage_param *param,
                                 const char *log_file,
                                 my_off_t log_pos)
{
  if (rpl_semi_sync_master_wait_point == WAIT_AFTER_SYNC)
    return repl_semisync.commitTrx(log_file, log_pos);
  return 0;
}

int repl_semi_report_before_dml(Trans_param *param, int& out)
{
  return 0;
}

int repl_semi_request_commit(Trans_param *param)
{
  return 0;
}

int repl_semi_report_before_commit(Trans_param *param)
{
  return 0;
}

int repl_semi_report_before_rollback(Trans_param *param)
{
  return 0;
}

int repl_semi_report_commit(Trans_param *param)
{

  bool is_real_trans= param->flags & TRANS_IS_REAL_TRANS;

  if (rpl_semi_sync_master_wait_point == WAIT_AFTER_COMMIT &&
      is_real_trans && param->log_pos)
  {
    const char *binlog_name= param->log_file;
    return repl_semisync.commitTrx(binlog_name, param->log_pos);
  }
  return 0;
}

int repl_semi_report_rollback(Trans_param *param)
{
  return repl_semi_report_commit(param);
}

int repl_semi_binlog_dump_start(Binlog_transmit_param *param,
				 const char *log_file,
				 my_off_t log_pos)
{
  long long semi_sync_slave= 0;

  /*
    semi_sync_slave will be 0 if the user variable doesn't exist. Otherwise, it
    will be set to the value of the user variable.
    'rpl_semi_sync_slave = 0' means that it is not a semisync slave.
  */
  get_user_var_int("rpl_semi_sync_slave", &semi_sync_slave, NULL);

  if (semi_sync_slave != 0)
  {
    if (ack_receiver.add_slave(current_thd))
    {
      sql_print_error("Failed to register slave to semi-sync ACK receiver "
                      "thread.");
      return -1;
    }

    my_set_thread_local(THR_RPL_SEMI_SYNC_DUMP, &SEMI_SYNC_DUMP);

    /* One more semi-sync slave */
    repl_semisync.add_slave();

    /* Tell server it will observe the transmission.*/
    param->set_observe_flag();

    /*
      Let's assume this semi-sync slave has already received all
      binlog events before the filename and position it requests.
    */
    repl_semisync.handleAck(param->server_id, log_file, log_pos);
  }
  else
    param->set_dont_observe_flag();

  sql_print_information("Start %s binlog_dump to slave (server_id: %d), pos(%s, %lu)",
			semi_sync_slave != 0 ? "semi-sync" : "asynchronous",
			param->server_id, log_file, (unsigned long)log_pos);

  return 0;
}

int repl_semi_binlog_dump_end(Binlog_transmit_param *param)
{
  bool semi_sync_slave= is_semi_sync_dump();

  sql_print_information("Stop %s binlog_dump to slave (server_id: %d)",
                        semi_sync_slave ? "semi-sync" : "asynchronous",
                        param->server_id);
  if (semi_sync_slave)
  {
    ack_receiver.remove_slave(current_thd);
    /* One less semi-sync slave */
    repl_semisync.remove_slave();
    my_set_thread_local(THR_RPL_SEMI_SYNC_DUMP, NULL);
  }
  return 0;
}

int repl_semi_reserve_header(Binlog_transmit_param *param,
			     unsigned char *header,
			     unsigned long size, unsigned long *len)
{
  if (is_semi_sync_dump())
    *len +=  repl_semisync.reserveSyncHeader(header, size);
  return 0;
}

int repl_semi_before_send_event(Binlog_transmit_param *param,
                                unsigned char *packet, unsigned long len,
                                const char *log_file, my_off_t log_pos)
{
  if (!is_semi_sync_dump())
    return 0;

  return repl_semisync.updateSyncHeader(packet,
					log_file,
					log_pos,
					param->server_id);
}

int repl_semi_after_send_event(Binlog_transmit_param *param,
                               const char *event_buf, unsigned long len,
                               const char * skipped_log_file,
                               my_off_t skipped_log_pos)
{
  if (is_semi_sync_dump())
  {
    if(skipped_log_pos>0)
      repl_semisync.skipSlaveReply(event_buf, param->server_id,
                                   skipped_log_file, skipped_log_pos);
    else
    {
      THD *thd= current_thd;
      /*
        Possible errors in reading slave reply are ignored deliberately
        because we do not want dump thread to quit on this. Error
        messages are already reported.
      */
      (void) repl_semisync.readSlaveReply(
        thd->get_protocol_classic()->get_net(),
        param->server_id, event_buf);
      thd->clear_error();
    }
  }
  return 0;
}

int repl_semi_reset_master(Binlog_transmit_param *param)
{
  if (repl_semisync.resetMaster())
    return 1;
  return 0;
}

C_MODE_END

/*
  semisync system variables
 */
static void fix_rpl_semi_sync_master_timeout(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val);

static void fix_rpl_semi_sync_master_trace_level(MYSQL_THD thd,
					  SYS_VAR *var,
					  void *ptr,
					  const void *val);

static void fix_rpl_semi_sync_master_wait_no_slave(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val);

static void fix_rpl_semi_sync_master_enabled(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val);

static void fix_rpl_semi_sync_master_wait_for_slave_count(MYSQL_THD thd,
                                                          SYS_VAR *var,
                                                          void *ptr,
                                                          const void *val);

static MYSQL_SYSVAR_BOOL(enabled, rpl_semi_sync_master_enabled,
  PLUGIN_VAR_OPCMDARG,
 "Enable semi-synchronous replication master (disabled by default). ",
  NULL, 			// check
  &fix_rpl_semi_sync_master_enabled,	// update
  0);

static MYSQL_SYSVAR_ULONG(timeout, rpl_semi_sync_master_timeout,
  PLUGIN_VAR_OPCMDARG,
 "The timeout value (in ms) for semi-synchronous replication in the master",
  NULL, 			// check
  fix_rpl_semi_sync_master_timeout,	// update
  10000, 0, ~0UL, 1);

static MYSQL_SYSVAR_BOOL(wait_no_slave, rpl_semi_sync_master_wait_no_slave,
  PLUGIN_VAR_OPCMDARG,
 "Wait until timeout when no semi-synchronous replication slave available (enabled by default). ",
  NULL, 			// check
  &fix_rpl_semi_sync_master_wait_no_slave,  // update
  1);

static MYSQL_SYSVAR_ULONG(trace_level, rpl_semi_sync_master_trace_level,
  PLUGIN_VAR_OPCMDARG,
 "The tracing level for semi-sync replication.",
  NULL,				  // check
  &fix_rpl_semi_sync_master_trace_level, // update
  32, 0, ~0UL, 1);

static const char *wait_point_names[]= {"AFTER_SYNC", "AFTER_COMMIT", NullS};
static TYPELIB wait_point_typelib= {
  array_elements(wait_point_names) - 1,
  "",
  wait_point_names,
  NULL
};
static MYSQL_SYSVAR_ENUM(
  wait_point,                      /* name     */
  rpl_semi_sync_master_wait_point, /* var      */
  PLUGIN_VAR_OPCMDARG,             /* flags    */
  "Semisync can wait for slave ACKs at one of two points,"
  "AFTER_SYNC or AFTER_COMMIT. AFTER_SYNC is the default value."
  "AFTER_SYNC means that semisynchronous replication waits just after the "
  "binary log file is flushed, but before the engine commits, and so "
  "guarantees that no other sessions can see the data before replicated to "
  "slave. AFTER_COMMIT means that semisynchronous replication waits just "
  "after the engine commits. Other sessions may see the data before it is "
  "replicated, even though the current session is still waiting for the commit "
  "to end successfully.",
  NULL,                            /* check()  */
  NULL,                            /* update() */
  WAIT_AFTER_SYNC,                 /* default  */
  &wait_point_typelib              /* typelib  */
);

static MYSQL_SYSVAR_UINT(wait_for_slave_count,   /* name  */
  rpl_semi_sync_master_wait_for_slave_count,     /* var   */
  PLUGIN_VAR_OPCMDARG,                           /* flags */
  "How many slaves the events should be replicated to. Semisynchronous "
  "replication master will wait until all events of the transaction are "
  "replicated to at least rpl_semi_sync_master_wait_for_slave_count slaves",
  NULL,                                           /* check() */
  &fix_rpl_semi_sync_master_wait_for_slave_count, /* update */
  1, 1, 65535, 1);

static SYS_VAR* semi_sync_master_system_vars[]= {
  MYSQL_SYSVAR(enabled),
  MYSQL_SYSVAR(timeout),
  MYSQL_SYSVAR(wait_no_slave),
  MYSQL_SYSVAR(trace_level),
  MYSQL_SYSVAR(wait_point),
  MYSQL_SYSVAR(wait_for_slave_count),
  NULL,
};
static void fix_rpl_semi_sync_master_timeout(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val)
{
  *(unsigned long *)ptr= *(unsigned long *)val;
  repl_semisync.setWaitTimeout(rpl_semi_sync_master_timeout);
  return;
}

static void fix_rpl_semi_sync_master_trace_level(MYSQL_THD thd,
					  SYS_VAR *var,
					  void *ptr,
					  const void *val)
{
  *(unsigned long *)ptr= *(unsigned long *)val;
  repl_semisync.setTraceLevel(rpl_semi_sync_master_trace_level);
  ack_receiver.setTraceLevel(rpl_semi_sync_master_trace_level);
  return;
}

static void fix_rpl_semi_sync_master_enabled(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val)
{
  *(char *)ptr= *(char *)val;
  if (rpl_semi_sync_master_enabled)
  {
    if (repl_semisync.enableMaster() != 0)
      rpl_semi_sync_master_enabled = false;
    else if (ack_receiver.start())
    {
      repl_semisync.disableMaster();
      rpl_semi_sync_master_enabled = false;
    }
  }
  else
  {
    if (repl_semisync.disableMaster() != 0)
      rpl_semi_sync_master_enabled = true;
    ack_receiver.stop();
  }

  return;
}

static void fix_rpl_semi_sync_master_wait_for_slave_count(MYSQL_THD thd,
                                                          SYS_VAR *var,
                                                          void *ptr,
                                                          const void *val)
{
  (void) repl_semisync.setWaitSlaveCount(*(unsigned int*) val);
  return;
}

static void fix_rpl_semi_sync_master_wait_no_slave(MYSQL_THD thd,
				      SYS_VAR *var,
				      void *ptr,
				      const void *val)
{
  if (rpl_semi_sync_master_wait_no_slave != *(char *)val)
  {
    *(char *)ptr= *(char *)val;
    repl_semisync.set_wait_no_slave(val);
  }
  return;
}

Trans_observer trans_observer = {
  sizeof(Trans_observer),		// len

  repl_semi_report_before_dml,      //before_dml
  repl_semi_report_before_commit,   // before_commit
  repl_semi_report_before_rollback, // before_rollback
  repl_semi_report_commit,	// after_commit
  repl_semi_report_rollback,	// after_rollback
};

Binlog_storage_observer storage_observer = {
  sizeof(Binlog_storage_observer), // len

  repl_semi_report_binlog_update, // report_update
  repl_semi_report_binlog_sync,   // after_sync
};

Binlog_transmit_observer transmit_observer = {
  sizeof(Binlog_transmit_observer), // len

  repl_semi_binlog_dump_start,	// start
  repl_semi_binlog_dump_end,	// stop
  repl_semi_reserve_header,	// reserve_header
  repl_semi_before_send_event,	// before_send_event
  repl_semi_after_send_event,	// after_send_event
  repl_semi_reset_master,	// reset
};


#define SHOW_FNAME(name)			\
  rpl_semi_sync_master_show_##name

#define DEF_SHOW_FUNC(name, show_type)					\
  static  int SHOW_FNAME(name)(MYSQL_THD thd, SHOW_VAR *var, char *buff) \
  {									\
    repl_semisync.setExportStats();					\
    var->type= show_type;						\
    var->value= (char *)&rpl_semi_sync_master_##name;				\
    return 0;								\
  }

DEF_SHOW_FUNC(status, SHOW_BOOL)
DEF_SHOW_FUNC(clients, SHOW_LONG)
DEF_SHOW_FUNC(wait_sessions, SHOW_LONG)
DEF_SHOW_FUNC(trx_wait_time, SHOW_LONGLONG)
DEF_SHOW_FUNC(trx_wait_num, SHOW_LONGLONG)
DEF_SHOW_FUNC(net_wait_time, SHOW_LONGLONG)
DEF_SHOW_FUNC(net_wait_num, SHOW_LONGLONG)
DEF_SHOW_FUNC(avg_net_wait_time, SHOW_LONG)
DEF_SHOW_FUNC(avg_trx_wait_time, SHOW_LONG)


/* plugin status variables */
static SHOW_VAR semi_sync_master_status_vars[]= {
  {"Rpl_semi_sync_master_status",
   (char*) &SHOW_FNAME(status),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_clients",
   (char*) &SHOW_FNAME(clients),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_yes_tx",
   (char*) &rpl_semi_sync_master_yes_transactions,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_no_tx",
   (char*) &rpl_semi_sync_master_no_transactions,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_wait_sessions",
   (char*) &SHOW_FNAME(wait_sessions),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_no_times",
   (char*) &rpl_semi_sync_master_off_times,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_timefunc_failures",
   (char*) &rpl_semi_sync_master_timefunc_fails,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_wait_pos_backtraverse",
   (char*) &rpl_semi_sync_master_wait_pos_backtraverse,
   SHOW_LONG, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_tx_wait_time",
   (char*) &SHOW_FNAME(trx_wait_time),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_tx_waits",
   (char*) &SHOW_FNAME(trx_wait_num),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_tx_avg_wait_time",
   (char*) &SHOW_FNAME(avg_trx_wait_time),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_net_wait_time",
   (char*) &SHOW_FNAME(net_wait_time),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_net_waits",
   (char*) &SHOW_FNAME(net_wait_num),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {"Rpl_semi_sync_master_net_avg_wait_time",
   (char*) &SHOW_FNAME(avg_net_wait_time),
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {NULL, NULL, SHOW_LONG, SHOW_SCOPE_GLOBAL},
};

#ifdef HAVE_PSI_INTERFACE

PSI_mutex_key key_ss_mutex_LOCK_binlog_;
PSI_mutex_key key_ss_mutex_Ack_receiver_mutex;

static PSI_mutex_info all_semisync_mutexes[]=
{
  { &key_ss_mutex_LOCK_binlog_, "LOCK_binlog_", 0},
  { &key_ss_mutex_Ack_receiver_mutex, "Ack_receiver::m_mutex", 0}
};

PSI_cond_key key_ss_cond_COND_binlog_send_;
PSI_cond_key key_ss_cond_Ack_receiver_cond;

static PSI_cond_info all_semisync_conds[]=
{
  { &key_ss_cond_COND_binlog_send_, "COND_binlog_send_", 0},
  { &key_ss_cond_Ack_receiver_cond, "Ack_receiver::m_cond", 0}
};

PSI_thread_key key_ss_thread_Ack_receiver_thread;

static PSI_thread_info all_semisync_threads[]=
{
  {&key_ss_thread_Ack_receiver_thread, "Ack_receiver", PSI_FLAG_GLOBAL}
};
#endif /* HAVE_PSI_INTERFACE */

PSI_stage_info stage_waiting_for_semi_sync_ack_from_slave=
{ 0, "Waiting for semi-sync ACK from slave", 0};

PSI_stage_info stage_waiting_for_semi_sync_slave=
{ 0, "Waiting for semi-sync slave connection", 0};

PSI_stage_info stage_reading_semi_sync_ack=
{ 0, "Reading semi-sync ACK from slave", 0};

/* Always defined. */
PSI_memory_key key_ss_memory_TranxNodeAllocator_block;

#ifdef HAVE_PSI_INTERFACE
PSI_stage_info *all_semisync_stages[]=
{
  & stage_waiting_for_semi_sync_ack_from_slave,
  & stage_waiting_for_semi_sync_slave,
  & stage_reading_semi_sync_ack
};

PSI_memory_info all_semisync_memory[]=
{
  {&key_ss_memory_TranxNodeAllocator_block, "TranxNodeAllocator::block", 0}
};

static void init_semisync_psi_keys(void)
{
  const char* category= "semisync";
  int count;

  count= array_elements(all_semisync_mutexes);
  mysql_mutex_register(category, all_semisync_mutexes, count);

  count= array_elements(all_semisync_conds);
  mysql_cond_register(category, all_semisync_conds, count);

  count= array_elements(all_semisync_stages);
  mysql_stage_register(category, all_semisync_stages, count);

  count= array_elements(all_semisync_memory);
  mysql_memory_register(category, all_semisync_memory, count);

  count= array_elements(all_semisync_threads);
  mysql_thread_register(category, all_semisync_threads, count);
}
#endif /* HAVE_PSI_INTERFACE */

static int semi_sync_master_plugin_init(void *p)
{
#ifdef HAVE_PSI_INTERFACE
  init_semisync_psi_keys();
#endif

  my_create_thread_local_key(&THR_RPL_SEMI_SYNC_DUMP, NULL);

  if (repl_semisync.initObject())
    return 1;
  if (ack_receiver.init())
    return 1;
  if (register_trans_observer(&trans_observer, p))
    return 1;
  if (register_binlog_storage_observer(&storage_observer, p))
    return 1;
  if (register_binlog_transmit_observer(&transmit_observer, p))
    return 1;
  return 0;
}

static int semi_sync_master_plugin_deinit(void *p)
{
  ack_receiver.stop();
  my_delete_thread_local_key(THR_RPL_SEMI_SYNC_DUMP);

  if (unregister_trans_observer(&trans_observer, p))
  {
    sql_print_error("unregister_trans_observer failed");
    return 1;
  }
  if (unregister_binlog_storage_observer(&storage_observer, p))
  {
    sql_print_error("unregister_binlog_storage_observer failed");
    return 1;
  }
  if (unregister_binlog_transmit_observer(&transmit_observer, p))
  {
    sql_print_error("unregister_binlog_transmit_observer failed");
    return 1;
  }

  sql_print_information("unregister_replicator OK");
  return 0;
}

struct Mysql_replication semi_sync_master_plugin= {
  MYSQL_REPLICATION_INTERFACE_VERSION
};

/*
  Plugin library descriptor
*/
mysql_declare_plugin(semi_sync_master)
{
  MYSQL_REPLICATION_PLUGIN,
  &semi_sync_master_plugin,
  "rpl_semi_sync_master",
  "He Zhenxing",
  "Semi-synchronous replication master",
  PLUGIN_LICENSE_GPL,
  semi_sync_master_plugin_init, /* Plugin Init */
  semi_sync_master_plugin_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  semi_sync_master_status_vars,	/* status variables */
  semi_sync_master_system_vars,	/* system variables */
  NULL,                         /* config options */
  0,                            /* flags */
}
mysql_declare_plugin_end;
