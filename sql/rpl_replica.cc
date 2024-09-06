/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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
  @addtogroup Replication
  @{

  @file sql/rpl_replica.cc

  @brief Code to run the io thread and the sql thread on the
  replication slave.
*/

#include "sql/rpl_replica.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/compression.h"
#include "include/mutex_lock.h"
#include "m_string.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_memory_bits.h"
#include "mysql/components/services/bits/psi_stage_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/status_var.h"
#include "mysql/strings/int2str.h"
#include "sql/changestreams/apply/replication_thread_status.h"
#include "sql/rpl_channel_service_interface.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "errmsg.h"  // CR_*
#include "lex_string.h"
#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_bitmap.h"   // MY_BITMAP
#include "my_byteorder.h"
#include "my_command.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_thread_local.h"  // thread_local_key_t
#include "mysql.h"            // MYSQL
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/debug_vars.h"
#include "mysql/my_loglevel.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/thread_type.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "pfs_thread_provider.h"
#include "prealloced_array.h"
#include "sql-common/net_ns.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/auto_thd.h"
#include "sql/binlog.h"
#include "sql/binlog_reader.h"
#include "sql/clone_handler.h"  // is_provisioning
#include "sql/current_thd.h"
#include "sql/debug_sync.h"   // DEBUG_SYNC
#include "sql/derror.h"       // ER_THD
#include "sql/dynamic_ids.h"  // Server_ids
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/log.h"
#include "sql/log_event.h"  // Rotate_log_event
#include "sql/mdl.h"
#include "sql/mysqld.h"              // ER
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/rpl_applier_reader.h"
#include "sql/rpl_async_conn_failover.h"
#include "sql/rpl_async_conn_failover_configuration_propagation.h"
#include "sql/rpl_filter.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_handler.h"  // RUN_HOOK
#include "sql/rpl_info.h"
#include "sql/rpl_info_factory.h"  // Rpl_info_factory
#include "sql/rpl_info_handler.h"
#include "sql/rpl_io_monitor.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"  // Multisource_info
#include "sql/rpl_mta_submode.h"
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/rpl_replica_until_options.h"
#include "sql/rpl_reporting.h"
#include "sql/rpl_rli.h"      // Relay_log_info
#include "sql/rpl_rli_pdb.h"  // Slave_worker
#include "sql/rpl_trx_boundary_parser.h"
#include "sql/rpl_utility.h"
#include "sql/sql_backup_lock.h"  // is_instance_backup_locked
#include "sql/sql_class.h"        // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"   // execute_init_command
#include "sql/sql_plugin.h"  // opt_plugin_dir_ptr
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/transaction.h"  // trans_begin
#include "sql/transaction_info.h"
#include "sql_common.h"  // end_server
#include "sql_string.h"
#include "str2int.h"
#include "string_with_len.h"
#include "strmake.h"
#include "typelib.h"
#ifndef NDEBUG
#include "rpl_debug_points.h"
#endif
#include "scope_guard.h"

struct mysql_cond_t;
struct mysql_mutex_t;

using mysql::binlog::event::Binary_log_event;
using mysql::binlog::event::checksum_crc32;
using mysql::binlog::event::enum_binlog_checksum_alg;
using mysql::binlog::event::Log_event_footer;
using mysql::binlog::event::Log_event_type;
using mysql::binlog::event::Log_event_type_helper;
using std::max;
using std::min;

#define FLAGSTR(V, F) ((V) & (F) ? #F " " : "")

/*
  a parameter of sql_slave_killed() to defer the killed status
*/
#define SLAVE_WAIT_GROUP_DONE 60
bool use_slave_mask = false;
MY_BITMAP slave_error_mask;
char slave_skip_error_names[SHOW_VAR_FUNC_BUFF_SIZE];

char *replica_load_tmpdir = nullptr;
bool replicate_same_server_id;
ulonglong relay_log_space_limit = 0;

const char *relay_log_index = nullptr;
const char *relay_log_basename = nullptr;

/*
  MTS load-ballancing parameter.
  Max length of one MTS Worker queue. The value also determines the size
  of Relay_log_info::gaq (see @c slave_start_workers()).
  It can be set to any value in [1, ULONG_MAX - 1] range.
*/
const ulong mts_slave_worker_queue_len_max = 16384;

/*
  MTS load-ballancing parameter.
  Time unit in microsecs to sleep by MTS Coordinator to avoid extra thread
  signalling in the case of Worker queues are close to be filled up.
*/
const ulong mts_coordinator_basic_nap = 5;

/*
  MTS load-ballancing parameter.
  Percent of Worker queue size at which Worker is considered to become
  hungry.

  C enqueues --+                   . underrun level
               V                   "
   +----------+-+------------------+--------------+
   | empty    |.|::::::::::::::::::|xxxxxxxxxxxxxx| ---> Worker dequeues
   +----------+-+------------------+--------------+

   Like in the above diagram enqueuing to the x-d area would indicate
   actual underrruning by Worker.
*/
const ulong mts_worker_underrun_level = 10;

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.
*/
static thread_local Master_info *RPL_MASTER_INFO = nullptr;

/**
  Encapsulates the messages and thread stages used for a specific call
  to try_to_reconnect.  Different Reconnect_messages objects may be
  used by the caller of try_to_reconnect in order to make the errors
  and stages include text that describes the reason for the reconnect.
*/
struct Reconnect_messages {
  /// Stage used while waiting to reconnect
  PSI_stage_info &stage_waiting_to_reconnect;
  /// Error reported in case the thread is killed while waiting
  std::string error_killed_while_waiting;
  /// Stage used while reconnecting
  PSI_stage_info &stage_reconnecting;
  /// Description of the condition that caused the thread to reconnect
  std::string triggering_error;
  /**
    The string representation of the enum_server_command that had been
    sent to the source before condition that caused the thread to
    reconnect happened.
  */
  std::string triggering_command;
};

static Reconnect_messages reconnect_messages_after_failed_registration{
    stage_replica_waiting_to_reconnect_after_failed_registration_on_source,
    "Replica I/O thread killed while waiting to reconnect after a failed "
    "registration on source",
    stage_replica_reconnecting_after_failed_registration_on_source,
    "failed registering on source, reconnecting to try again, "
    "log '%s' at position %s",
    "COM_REGISTER_REPLICA"};

static Reconnect_messages reconnect_messages_after_failed_dump{
    stage_replica_waiting_to_reconnect_after_failed_binlog_dump_request,
    "Replica I/O thread killed while retrying source dump",
    stage_replica_reconnecting_after_failed_binlog_dump_request,
    "failed dump request, reconnecting to try again, log '%s' at position "
    "%s",
    "COM_BINLOG_DUMP"};

static Reconnect_messages reconnect_messages_after_failed_event_read{
    stage_replica_waiting_to_reconnect_after_failed_event_read,
    "Replica I/O thread killed while waiting to reconnect after a failed read",
    stage_replica_reconnecting_after_failed_event_read,
    "Replica I/O thread: Failed reading log event, reconnecting to retry, "
    "log '%s' at position %s",
    ""};

enum enum_slave_apply_event_and_update_pos_retval {
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK = 0,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR = 1,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR = 2,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR = 3,
  SLAVE_APPLY_EVENT_RETRY = 4,
  SLAVE_APPLY_EVENT_UNTIL_REACHED = 5,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_MAX
};

static int process_io_rotate(Master_info *mi, Rotate_log_event *rev);

/// @brief Checks whether relay log space will be exceeded after queueing
/// additional 'queued_size' bytes. If yes, function will
/// request relay log purge, rotate the relay log and wait for notification
/// from coordinator
/// @retval true Event may be queued by the receiver
/// @retval false Failed to reclaim required relay log space
static inline bool wait_for_relay_log_space(Relay_log_info *rli,
                                            std::size_t queued_size);
/// @brief Checks whether relay log space limit will be exceeded after queueing
/// additional 'queued_size' bytes
/// @param rli Pointer to connection metadata object for the considered channel
/// @param queued_size Number of bytes we want to queue
/// @retval true Limit exceeded
/// @retval false Current size + 'queued_size' is within limits
static inline bool exceeds_relay_log_limit(Relay_log_info *rli,
                                           std::size_t queued_size);
static inline bool io_slave_killed(THD *thd, Master_info *mi);
static inline bool monitor_io_replica_killed(THD *thd, Master_info *mi);
static inline bool is_autocommit_off(THD *thd);
static void print_replica_skip_errors(void);
static int safe_connect(THD *thd, MYSQL *mysql, Master_info *mi,
                        const std::string &host = std::string(),
                        const uint port = 0);
static int safe_reconnect(THD *thd, MYSQL *mysql, Master_info *mi,
                          bool suppress_warnings,
                          const std::string &host = std::string(),
                          const uint port = 0);
static int get_master_version_and_clock(MYSQL *mysql, Master_info *mi);
static int get_master_uuid(MYSQL *mysql, Master_info *mi);
int io_thread_init_commands(MYSQL *mysql, Master_info *mi);
static int terminate_slave_thread(THD *thd, mysql_mutex_t *term_lock,
                                  mysql_cond_t *term_cond,
                                  std::atomic<uint> *slave_running,
                                  ulong *stop_wait_timeout, bool need_lock_term,
                                  bool force = false);
static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info);
static int mts_event_coord_cmp(LOG_POS_COORD *id1, LOG_POS_COORD *id2);

static int check_slave_sql_config_conflict(const Relay_log_info *rli);
static void group_replication_cleanup_after_clone();

static void check_replica_configuration_restrictions();
static bool check_replica_configuration_errors(Master_info *mi,
                                               int thread_mask);
/*
  Applier thread InnoDB priority.
  When two transactions conflict inside InnoDB, the one with
  greater priority wins.

  @param thd       Thread handler for slave
  @param priority  Thread priority
*/
static void set_thd_tx_priority(THD *thd, int priority) {
  DBUG_TRACE;
  assert(thd->system_thread == SYSTEM_THREAD_SLAVE_SQL ||
         thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER);

  thd->thd_tx_priority = priority;
  DBUG_EXECUTE_IF("dbug_set_high_prio_sql_thread",
                  { thd->thd_tx_priority = 1; });
}

/**
  Set for the thread options about the memory and size limits when
  transactions collect write sets.

  @param thd          Thread handler
  @param ignore_limit  if the memory limits should be ignored
  @param allow_drop_write_set if this thread does not require WS to always be
  logged
*/
static void set_thd_write_set_options(THD *thd, bool ignore_limit,
                                      bool allow_drop_write_set) {
  DBUG_TRACE;
  thd->get_transaction()
      ->get_transaction_write_set_ctx()
      ->set_local_ignore_write_set_memory_limit(ignore_limit);
  thd->get_transaction()
      ->get_transaction_write_set_ctx()
      ->set_local_allow_drop_write_set(allow_drop_write_set);
}

/*
  Function to set the slave's max_allowed_packet based on the value
  of replica_max_allowed_packet.

    @in_param    thd    Thread handler for slave
    @in_param    mysql  MySQL connection handle
*/

static void set_replica_max_allowed_packet(THD *thd, MYSQL *mysql) {
  DBUG_TRACE;
  // thd and mysql must be valid
  assert(thd && mysql);

  thd->variables.max_allowed_packet = replica_max_allowed_packet;
  /*
    Adding MAX_LOG_EVENT_HEADER_LEN to the max_packet_size on the I/O
    thread and the mysql->option max_allowed_packet, since a
    replication event can become this much  larger than
    the corresponding packet (query) sent from client to master.
  */
  thd->get_protocol_classic()->set_max_packet_size(replica_max_allowed_packet +
                                                   MAX_LOG_EVENT_HEADER);
  /*
    Skipping the setting of mysql->net.max_packet size to slave
    max_allowed_packet since this is done during mysql_real_connect.
  */
  mysql->options.max_allowed_packet =
      replica_max_allowed_packet + MAX_LOG_EVENT_HEADER;
}

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_key key_memory_rli_mta_coor;

static PSI_thread_key key_thread_replica_io, key_thread_replica_sql,
    key_thread_replica_worker, key_thread_replica_monitor_io;

static PSI_thread_info all_slave_threads[] = {
    {&key_thread_replica_io, "replica_io", "rpl_rca_io", PSI_FLAG_THREAD_SYSTEM,
     0, PSI_DOCUMENT_ME},
    {&key_thread_replica_sql, "replica_sql", "rpl_rca_sql",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME},
    {&key_thread_replica_worker, "replica_worker", "rpl_rca_wkr",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME},
    {&key_thread_replica_monitor_io, "replica_monitor", "rpl_rca_mon",
     PSI_FLAG_SINGLETON | PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME}};

static PSI_memory_info all_slave_memory[] = {{&key_memory_rli_mta_coor,
                                              "Relay_log_info::mta_coor", 0, 0,
                                              PSI_DOCUMENT_ME}};

#endif /* HAVE_PSI_INTERFACE */

/* Initialize slave structures */

int ReplicaInitializer::get_initialization_code() const { return m_init_code; }

ReplicaInitializer::ReplicaInitializer(bool opt_initialize,
                                       bool opt_skip_replica_start,
                                       Rpl_channel_filters &filters,
                                       char **replica_skip_erors)
    : m_opt_initialize_replica(!opt_initialize),
      m_opt_skip_replica_start(opt_initialize),
      m_thread_mask(REPLICA_SQL | REPLICA_IO) {
  if (m_opt_initialize_replica) {
    // Make @@replica_skip_errors show the nice human-readable value.
    set_replica_skip_errors(replica_skip_erors);
    /*
      Group replication filters should be discarded before init_replica(),
      otherwise the pre-configured filters will be referenced by group
      replication channels.
    */
    filters.discard_group_replication_filters();

    /*
      init_replica() must be called after the thread keys are created.
    */

    if (server_id != 0) {
      m_init_code = init_replica();
    }

    start_replication_threads(opt_skip_replica_start);

    /*
      If the user specifies a per-channel replication filter through a
      command-line option (or in a configuration file) for a slave
      replication channel which does not exist as of now (i.e not
      present in slave info tables yet), then the per-channel
      replication filter is discarded with a warning.
      If the user specifies a per-channel replication filter through
      a command-line option (or in a configuration file) for group
      replication channels 'group_replication_recovery' and
      'group_replication_applier' which is disallowed, then the
      per-channel replication filter is discarded with a warning.
    */
    filters.discard_all_unattached_filters();
  }
}

void ReplicaInitializer::print_channel_info() const {
#ifndef NDEBUG
  /* @todo: Print it for all the channels */
  {
    Master_info *default_mi;
    default_mi = channel_map.get_default_channel_mi();
    if (default_mi && default_mi->rli) {
      DBUG_PRINT(
          "info",
          ("init group source %s %lu  group replica %s %lu event %s %lu\n",
           default_mi->rli->get_group_master_log_name(),
           (ulong)default_mi->rli->get_group_master_log_pos(),
           default_mi->rli->get_group_relay_log_name(),
           (ulong)default_mi->rli->get_group_relay_log_pos(),
           default_mi->rli->get_event_relay_log_name(),
           (ulong)default_mi->rli->get_event_relay_log_pos()));
    }
  }
#endif
}

void ReplicaInitializer::start_replication_threads(bool skip_replica_start) {
  if (!m_opt_skip_replica_start && !skip_replica_start) {
    start_threads();
  }
}

void ReplicaInitializer::start_threads() {
  /*
    Loop through the channel_map and start replica threads for each channel.
  */
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    Master_info *mi = it->second;

    /* If server id is not set, start_slave_thread() will say it */
    if (Master_info::is_configured(mi) && mi->rli->inited) {
      /* same as in start_slave() cache the global var values into rli's
       * members */
      mi->rli->opt_replica_parallel_workers = opt_mts_replica_parallel_workers;
      mi->rli->checkpoint_group = opt_mta_checkpoint_group;
      if (mts_parallel_option == MTS_PARALLEL_TYPE_DB_NAME)
        mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_DB_NAME;
      else
        mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_LOGICAL_CLOCK;

      if (mi->is_source_connection_auto_failover())
        m_thread_mask |= SLAVE_MONITOR;

      mi->set_applier_metric_collection_status(
          opt_collect_replica_applier_metrics);

      if (start_slave_threads(true /*need_lock_slave=true*/,
                              false /*wait_for_start=false*/, mi,
                              m_thread_mask)) {
        LogErr(ERROR_LEVEL, ER_FAILED_TO_START_REPLICA_THREAD,
               mi->get_channel());
      }
    } else {
      LogErr(INFORMATION_LEVEL, ER_FAILED_TO_START_REPLICA_THREAD,
             mi->get_channel());
    }
  }
}

void ReplicaInitializer::init_replica_psi_keys() {
#ifdef HAVE_PSI_INTERFACE
  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(all_slave_threads));
  mysql_thread_register(category, all_slave_threads, count);

  count = static_cast<int>(array_elements(all_slave_memory));
  mysql_memory_register(category, all_slave_memory, count);
#endif  // HAVE_PSI_INTERFACE
}

int ReplicaInitializer::init_replica() {
  DBUG_TRACE;
  int error = 0;

#ifdef HAVE_PSI_INTERFACE
  init_replica_psi_keys();
#endif

  /*
    This is called when mysqld starts. Before client connections are
    accepted. However bootstrap may conflict with us if it does START REPLICA.
    So it's safer to take the lock.
  */
  channel_map.wrlock();

  Scope_guard channel_map_guard([&error]() {
    channel_map.unlock();
    if (error)
      LogErr(INFORMATION_LEVEL, ER_REPLICA_NOT_STARTED_ON_SOME_CHANNELS);
  });

  RPL_MASTER_INFO = nullptr;

  /*
    Create slave info objects by reading repositories of individual
    channels and add them into channel_map
  */
  if ((error = Rpl_info_factory::create_slave_info_objects(
           INFO_REPOSITORY_TABLE, INFO_REPOSITORY_TABLE, m_thread_mask,
           &channel_map)))
    LogErr(ERROR_LEVEL,
           ER_RPL_REPLICA_FAILED_TO_CREATE_OR_RECOVER_INFO_REPOSITORIES);

  group_replication_cleanup_after_clone();

  print_channel_info();

  check_replica_configuration_restrictions();

  if (check_slave_sql_config_conflict(nullptr)) {
    error = 1;
    return error;
  }
  return error;
}

/**
   Function to start a slave for all channels.
   Used in Multisource replication.
   @param[in]        thd           THD object of the client.

   @retval false success
   @retval true error

    @todo  It is good to continue to start other channels
           when a slave start failed for other channels.

    @todo  The problem with the below code is if the slave is already
           stared, we would have multiple warnings
           "Replica was already running" for each channel.
           A nice warning message would be to add
           "Replica for channel '%s' was already running"
           but error messages are in different languages and cannot be tampered
           with so, we have to handle it case by case basis, whether
           only default channel exists or not and properly continue with
           starting other channels if one channel fails clearly giving
           an error message by displaying failed channels.
*/
bool start_slave(THD *thd) {
  DBUG_TRACE;
  Master_info *mi;
  bool channel_configured, error = false;

  if (channel_map.get_num_instances() == 1) {
    mi = channel_map.get_default_channel_mi();
    assert(mi);
    if (start_slave(thd, &thd->lex->replica_connection, &thd->lex->mi,
                    thd->lex->replica_thd_opt, mi, true))
      return true;
  } else {
    /*
      Users cannot start more than one channel's applier thread
      if sql_replica_skip_counter > 0. It throws an error to the session.
    */
    mysql_mutex_lock(&LOCK_sql_replica_skip_counter);
    /* sql_replica_skip_counter > 0 && !(START REPLICA IO_THREAD) */
    if (sql_replica_skip_counter > 0 &&
        !(thd->lex->replica_thd_opt & REPLICA_IO)) {
      my_error(ER_REPLICA_CHANNEL_SQL_SKIP_COUNTER, MYF(0));
      mysql_mutex_unlock(&LOCK_sql_replica_skip_counter);
      return true;
    }
    mysql_mutex_unlock(&LOCK_sql_replica_skip_counter);

    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      mi = it->second;

      channel_configured =
          mi &&                      // Master_info exists
          (mi->inited || mi->reset)  // It is inited or was reset
          && mi->host[0];            // host is set

      if (channel_configured) {
        if (start_slave(thd, &thd->lex->replica_connection, &thd->lex->mi,
                        thd->lex->replica_thd_opt, mi, true)) {
          LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_START_REPLICA_FOR_CHANNEL,
                 mi->get_channel());
          error = true;
        }
      }
    }
  }
  if (!error) {
    /* no error */
    my_ok(thd);
  }
  return error;
}

/**
   Function to stop a slave for all channels.
   Used in Multisource replication.
   @param[in]        thd           THD object of the client.

   @retval           0             success
   @retval           1             error

    @todo  It is good to continue to stop other channels
           when a slave start failed for other channels.
*/
int stop_slave(THD *thd) {
  DBUG_TRACE;
  bool push_temp_table_warning = true;
  Master_info *mi = nullptr;
  int error = 0;

  if (channel_map.get_num_instances() == 1) {
    mi = channel_map.get_default_channel_mi();

    assert(!strcmp(mi->get_channel(), channel_map.get_default_channel()));

    error = stop_slave(thd, mi, true, false /*for_one_channel*/,
                       &push_temp_table_warning);
  } else {
    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      mi = it->second;

      if (Master_info::is_configured(mi)) {
        if (stop_slave(thd, mi, true, false /*for_one_channel*/,
                       &push_temp_table_warning)) {
          LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_STOP_REPLICA_FOR_CHANNEL,
                 mi->get_channel());
          error = 1;
        }
      }
    }
  }

  if (!error) {
    /* no error */
    my_ok(thd);
  }

  return error;
}

/**
  Entry point to the START REPLICA command. The function
  decides to start replication threads on several channels
  or a single given channel.

  @param[in]   thd        the client thread carrying the command.

  @retval      false      ok
  @retval      true       not ok.
*/
bool start_slave_cmd(THD *thd) {
  DBUG_TRACE;

  Master_info *mi;
  LEX *lex = thd->lex;
  bool res = true; /* default, an error */

  DEBUG_SYNC(thd, "begin_start_replica");

  channel_map.wrlock();

  DEBUG_SYNC(thd, "after_locking_channel_map_in_start_replica");

  if (!is_slave_configured()) {
    my_error(ER_REPLICA_CONFIGURATION, MYF(0));
    goto err;
  }

  if (!lex->mi.for_channel) {
    /*
      If replica_until options are provided when multiple channels exist
      without explicitly providing FOR CHANNEL clause, error out.
    */
    if (lex->mi.replica_until && channel_map.get_num_instances() > 1) {
      my_error(ER_REPLICA_MULTIPLE_CHANNELS_CMD, MYF(0));
      goto err;
    }

    res = start_slave(thd);
  } else {
    mi = channel_map.get_mi(lex->mi.channel);

    /*
      If the channel being used is a group replication channel we need to
      disable this command here as, in some cases, group replication does not
      support them.

      For channel group_replication_applier we disable START REPLICA [IO_THREAD]
      command.

      For channel group_replication_recovery we disable START REPLICA command
      and its two thread variants.
    */
    if (mi &&
        channel_map.is_group_replication_channel_name(mi->get_channel()) &&
        ((!thd->lex->replica_thd_opt ||
          (thd->lex->replica_thd_opt & REPLICA_IO)) ||
         (!(channel_map.is_group_replication_applier_channel_name(
              mi->get_channel())) &&
          (thd->lex->replica_thd_opt & REPLICA_SQL)))) {
      const char *command = "START REPLICA FOR CHANNEL";
      if (thd->lex->replica_thd_opt & REPLICA_IO)
        command = "START REPLICA IO_THREAD FOR CHANNEL";
      else if (thd->lex->replica_thd_opt & REPLICA_SQL)
        command = "START REPLICA SQL_THREAD FOR CHANNEL";

      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0), command,
               mi->get_channel());

      goto err;
    }
    /*
      START REPLICA for channel group_replication_applier is disallowed while
      Group Replication is running.
    */
    if (mi &&
        channel_map.is_group_replication_applier_channel_name(
            mi->get_channel()) &&
        is_group_replication_running()) {
      const char *command =
          "START REPLICA FOR CHANNEL while Group Replication is running";
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0), command,
               mi->get_channel());
      goto err;
    }

    if (mi)
      res = start_slave(thd, &thd->lex->replica_connection, &thd->lex->mi,
                        thd->lex->replica_thd_opt, mi, true);
    else if (strcmp(channel_map.get_default_channel(), lex->mi.channel))
      my_error(ER_REPLICA_CHANNEL_DOES_NOT_EXIST, MYF(0), lex->mi.channel);

    if (!res) my_ok(thd);
  }
err:
  channel_map.unlock();
  return res;
}

/**
  Entry point for the STOP REPLICA command. This function stops replication
  threads for all channels or a single channel based on the  command
  options supplied.

  @param[in]     thd         the client thread.

  @retval        false       ok
  @retval        true        not ok.
*/
bool stop_slave_cmd(THD *thd) {
  DBUG_TRACE;

  Master_info *mi;
  bool push_temp_table_warning = true;
  LEX *lex = thd->lex;
  bool res = true; /*default, an error */

  channel_map.rdlock();

  if (!is_slave_configured()) {
    my_error(ER_REPLICA_CONFIGURATION, MYF(0));
    channel_map.unlock();
    return true;
  }

  MDL_lock_guard backup_sentry{thd};
  /* During provisioning we stop replica after acquiring backup lock. */
  if (!Clone_handler::is_provisioning() &&
      (!thd->lex->replica_thd_opt ||
       (thd->lex->replica_thd_opt & REPLICA_SQL))) {
    if (backup_sentry.lock(MDL_key::BACKUP_LOCK, MDL_INTENTION_EXCLUSIVE)) {
      my_error(ER_RPL_CANT_STOP_REPLICA_WHILE_LOCKED_BACKUP, MYF(0));
      channel_map.unlock();
      return true;
    }
  }

  if (!lex->mi.for_channel)
    res = stop_slave(thd);
  else {
    mi = channel_map.get_mi(lex->mi.channel);

    /*
      If the channel being used is a group replication channel we need to
      disable this command here as, in some cases, group replication does not
      support them.

      For channel group_replication_applier we disable STOP REPLICA [IO_THREAD]
      command.

      For channel group_replication_recovery we disable STOP REPLICA command
      and its two thread variants.
    */
    if (mi &&
        channel_map.is_group_replication_channel_name(mi->get_channel()) &&
        ((!thd->lex->replica_thd_opt ||
          (thd->lex->replica_thd_opt & REPLICA_IO)) ||
         (!(channel_map.is_group_replication_applier_channel_name(
              mi->get_channel())) &&
          (thd->lex->replica_thd_opt & REPLICA_SQL)))) {
      const char *command = "STOP REPLICA FOR CHANNEL";
      if (thd->lex->replica_thd_opt & REPLICA_IO)
        command = "STOP REPLICA IO_THREAD FOR CHANNEL";
      else if (thd->lex->replica_thd_opt & REPLICA_SQL)
        command = "STOP REPLICA SQL_THREAD FOR CHANNEL";

      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0), command,
               mi->get_channel());

      channel_map.unlock();
      return true;
    }
    /*
      STOP REPLICA for channel group_replication_applier is disallowed while
      Group Replication is running.
    */
    if (mi &&
        channel_map.is_group_replication_applier_channel_name(
            mi->get_channel()) &&
        is_group_replication_running()) {
      const char *command =
          "STOP REPLICA FOR CHANNEL while Group Replication is running";
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0), command,
               mi->get_channel());
      channel_map.unlock();
      return true;
    }

    if (mi)
      res = stop_slave(thd, mi, true /*net report */, true /*for_one_channel*/,
                       &push_temp_table_warning);
    else if (strcmp(channel_map.get_default_channel(), lex->mi.channel))
      my_error(ER_REPLICA_CHANNEL_DOES_NOT_EXIST, MYF(0), lex->mi.channel);
  }

  channel_map.unlock();

  DBUG_EXECUTE_IF("stop_replica_dont_release_backup_lock", {
    rpl_replica_debug_point(DBUG_RPL_S_STOP_SLAVE_BACKUP_LOCK, thd);
  });

  return res;
}

enum enum_read_rotate_from_relay_log_status {
  FOUND_ROTATE,
  NOT_FOUND_ROTATE,
  ERROR
};

/**
   Parse the given relay log and identify the rotate event from the master.
   Ignore the Format description event, Previous_gtid log event, ignorable
   event and Stop event within the relay log as they are generated by slave.
   When a rotate event is found check if it is a rotate that is originated from
   the master based on the server_id. Ignore the event if the rotate is from
   slave or if it is a fake rotate event. If any other events are encountered
   apart from the above events generate an error. From the rotate event
   extract the master's binary log name and position.

   @param filename
          Relay log name which needs to be parsed.

   @param[out] source_log_file
          Set the source_log_file to the log file name that is extracted from
          rotate event. The source_log_file should contain string of len
          FN_REFLEN.

   @param[out] master_log_pos
          Set the master_log_pos to the log position extracted from rotate
          event.

   @retval FOUND_ROTATE: When rotate event is found in the relay log
   @retval NOT_FOUND_ROTATE: When rotate event is not found in the relay log
   @retval ERROR: On error
 */
static enum_read_rotate_from_relay_log_status read_rotate_from_relay_log(
    char *filename, char *source_log_file, my_off_t *master_log_pos) {
  DBUG_TRACE;

  Relaylog_file_reader relaylog_file_reader(opt_replica_sql_verify_checksum);
  if (relaylog_file_reader.open(filename)) {
    LogErr(ERROR_LEVEL, ER_RPL_RECOVERY_ERROR,
           relaylog_file_reader.get_error_str());
    return ERROR;
  }

  Log_event *ev = nullptr;
  bool done = false;
  enum_read_rotate_from_relay_log_status ret = NOT_FOUND_ROTATE;
  while (!done && (ev = relaylog_file_reader.read_event_object()) != nullptr) {
    DBUG_PRINT("info", ("Read event of type %s", ev->get_type_str()));
    switch (ev->get_type_code()) {
      case mysql::binlog::event::FORMAT_DESCRIPTION_EVENT:
        break;
      case mysql::binlog::event::ROTATE_EVENT:
        /*
          Check for rotate event from the master. Ignore the ROTATE event if it
          is a fake rotate event with server_id=0.
         */
        if (ev->server_id && ev->server_id != ::server_id) {
          Rotate_log_event *rotate_ev = (Rotate_log_event *)ev;
          assert(FN_REFLEN >= rotate_ev->ident_len + 1);
          memcpy(source_log_file, rotate_ev->new_log_ident,
                 rotate_ev->ident_len + 1);
          *master_log_pos = rotate_ev->pos;
          ret = FOUND_ROTATE;
          done = true;
        }
        break;
      case mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT:
      case mysql::binlog::event::IGNORABLE_LOG_EVENT:
      case mysql::binlog::event::STOP_EVENT:
        break;
      default:
        LogErr(ERROR_LEVEL, ER_RPL_RECOVERY_NO_ROTATE_EVENT_FROM_SOURCE);
        ret = ERROR;
        done = true;
        break;
    }
    delete ev;
  }
  if (relaylog_file_reader.has_fatal_error()) {
    LogErr(ERROR_LEVEL, ER_RPL_RECOVERY_ERROR_READ_RELAY_LOG, -1);
    return ERROR;
  }
  return ret;
}

/**
   Reads relay logs one by one starting from the first relay log. Looks for
   the first rotate event from the master. If rotate is not found in the relay
   log search continues to next relay log. If rotate event from master is
   found then the extracted source_log_file and master_log_pos are used to set
   rli->group_master_log_name and rli->group_master_log_pos. If an error has
   occurred the error code is returned back.

   @param rli
          Relay_log_info object to read relay log files and to set
          group_master_log_name and group_master_log_pos.

   @retval 0 Success - Rotate event was found
   @retval 1 Failure - Found some events replicated but no rotate event was
   found
   @retval 2 When no rotate event from master was found. This can happen when
             slave server was restarted immediately after executing CHANGE
   REPLICATION SOURCE
 */
static int find_first_relay_log_with_rotate_from_master(Relay_log_info *rli) {
  DBUG_TRACE;
  int error = 0;
  Log_info linfo;
  bool got_rotate_from_master = false;
  int pos;
  char source_log_file[FN_REFLEN];
  my_off_t master_log_pos = 0;

  if (channel_map.is_group_replication_channel_name(rli->get_channel())) {
    LogErr(INFORMATION_LEVEL,
           ER_RPL_RECOVERY_SKIPPED_GROUP_REPLICATION_CHANNEL);
    goto err;
  }

  for (pos = rli->relay_log.find_log_pos(&linfo, nullptr, true); !pos;
       pos = rli->relay_log.find_next_log(&linfo, true)) {
    switch (read_rotate_from_relay_log(linfo.log_file_name, source_log_file,
                                       &master_log_pos)) {
      case ERROR:
        error = 1;
        break;
      case FOUND_ROTATE:
        got_rotate_from_master = true;
        break;
      case NOT_FOUND_ROTATE:
        break;
    }
    if (error || got_rotate_from_master) break;
  }
  if (pos == LOG_INFO_IO) {
    error = 1;
    LogErr(ERROR_LEVEL, ER_RPL_RECOVERY_IO_ERROR_READING_RELAY_LOG_INDEX);
    goto err;
  }
  if (pos == LOG_INFO_EOF) {
    error = 2;
    LogErr(WARNING_LEVEL, ER_RPL_RECOVERY_NO_ROTATE_EVENT_FROM_SOURCE);
    LogErr(WARNING_LEVEL, ER_WARN_RPL_RECOVERY_NO_ROTATE_EVENT_FROM_SOURCE_EOF,
           rli->mi->get_channel());
    goto err;
  }
  if (!error && got_rotate_from_master) {
    rli->set_group_master_log_name(source_log_file);
    rli->set_group_master_log_pos(master_log_pos);
  }
err:
  return error;
}

/*
  Updates the master info based on the information stored in the
  relay info and ignores relay logs previously retrieved by the IO
  thread, which thus starts fetching again based on to the
  master_log_pos and master_log_name. Eventually, the old
  relay logs will be purged by the normal purge mechanism.

  When GTID's are enabled the "Retrieved GTID" set should be cleared
  so that partial read events are discarded and they are
  fetched once again

  @param mi    pointer to Master_info instance
*/
static void recover_relay_log(Master_info *mi) {
  Relay_log_info *rli = mi->rli;

  // If GTID ONLY is enable the receiver doesn't care about these positions
  if (!mi->is_gtid_only_mode()) {
    // Set Receiver Thread's positions as per the recovered Applier Thread.
    mi->set_master_log_pos(std::max<ulonglong>(
        BIN_LOG_HEADER_SIZE, rli->get_group_master_log_pos()));
    mi->set_master_log_name(rli->get_group_master_log_name());

    LogErr(WARNING_LEVEL, ER_RPL_RECOVERY_FILE_SOURCE_POS_INFO,
           (ulong)mi->get_master_log_pos(), mi->get_master_log_name(),
           mi->get_for_channel_str(), rli->get_group_relay_log_pos(),
           rli->get_group_relay_log_name());
  } else {
    LogErr(WARNING_LEVEL, ER_RPL_RELAY_LOG_RECOVERY_GTID_ONLY);
  }

  // Start with a fresh relay log.
  rli->set_group_relay_log_name(rli->relay_log.get_log_fname());
  rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
  /*
    Clear the retrieved GTID set so that events that are written partially
    will be fetched again.
  */
  if (global_gtid_mode.get() == Gtid_mode::ON &&
      !channel_map.is_group_replication_channel_name(rli->get_channel())) {
    rli->get_tsid_lock()->wrlock();
    (const_cast<Gtid_set *>(rli->get_gtid_set()))->clear_set_and_tsid_map();
    rli->get_tsid_lock()->unlock();
  }
}

/*
  Updates the master info based on the information stored in the
  relay info and ignores relay logs previously retrieved by the IO
  thread, which thus starts fetching again based on to the
  master_log_pos and master_log_name. Eventually, the old
  relay logs will be purged by the normal purge mechanism.

  There can be a special case where rli->group_master_log_name and
  rli->group_master_log_pos are not initialized, as the sql thread was never
  started at all. In those cases all the existing relay logs are parsed
  starting from the first one and the initial rotate event that was received
  from the master is identified. From the rotate event master_log_name and
  master_log_pos are extracted and they are set to rli->group_master_log_name
  and rli->group_master_log_pos.

  In the feature, we should improve this routine in order to avoid throwing
  away logs that are safely stored in the disk. Note also that this recovery
  routine relies on the correctness of the relay-log.info and only tolerates
  coordinate problems in master.info.

  In this function, there is no need for a mutex as the caller
  (i.e. init_replica) already has one acquired.

  Specifically, the following structures are updated:

  1 - mi->master_log_pos  <-- rli->group_master_log_pos
  2 - mi->master_log_name <-- rli->group_master_log_name
  3 - It moves the relay log to the new relay log file, by
      rli->group_relay_log_pos  <-- BIN_LOG_HEADER_SIZE;
      rli->event_relay_log_pos  <-- BIN_LOG_HEADER_SIZE;
      rli->group_relay_log_name <-- rli->relay_log.get_log_fname();
      rli->event_relay_log_name <-- rli->relay_log.get_log_fname();

   If there is an error, it returns (1), otherwise returns (0).
 */
int init_recovery(Master_info *mi) {
  DBUG_TRACE;

  int error = 0;
  Relay_log_info *rli = mi->rli;
  char *group_master_log_name = nullptr;

  /*
    This is not idempotent and a crash after this function and before
    the recovery is actually done may lead the system to an inconsistent
    state.

    This may happen because the gap is not persitent stored anywhere
    and eventually old relay log files will be removed and further
    calculations on the gaps will be impossible.

    We need to improve this. /Alfranio.
  */
  error = mts_recovery_groups(rli);
  if (rli->mts_recovery_group_cnt) return error;

  group_master_log_name = const_cast<char *>(rli->get_group_master_log_name());
  if (!error) {
    bool run_relay_log_recovery = true;
    if (!group_master_log_name[0] && !rli->mi->is_gtid_only_mode()) {
      if (rli->replicate_same_server_id) {
        error = 1;
        LogErr(ERROR_LEVEL,
               ER_RPL_RECOVERY_REPLICATE_SAME_SERVER_ID_REQUIRES_POSITION);
        return error;
      }
      error = find_first_relay_log_with_rotate_from_master(rli);
      if (error == 2) {
        // No events from the master on relay log - skip relay log recovery
        run_relay_log_recovery = false;
        error = 0;
      } else if (error)
        return error;
    }
    if (run_relay_log_recovery) recover_relay_log(mi);
  }
  return error;
}

/*
  Relay log recovery in the case of MTS, is handled by the following function.
  Gaps in MTS execution are filled using implicit execution of
  START REPLICA UNTIL SQL_AFTER_MTS_GAPS call. Once slave reaches a consistent
  gapless state receiver thread's positions are initialized to applier thread's
  positions and the old relay logs are discarded. This completes the recovery
  process.

  @param mi    pointer to Master_info instance.

  @retval 0 success
  @retval 1 error
*/
static inline int fill_mts_gaps_and_recover(Master_info *mi) {
  DBUG_TRACE;
  Relay_log_info *rli = mi->rli;
  int recovery_error = 0;
  rli->is_relay_log_recovery = false;
  Until_mts_gap *until_mg = new Until_mts_gap(rli);
  rli->set_until_option(until_mg);
  rli->until_condition = Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS;
  until_mg->init();
  rli->channel_mts_submode = (mts_parallel_option == MTS_PARALLEL_TYPE_DB_NAME)
                                 ? MTS_PARALLEL_TYPE_DB_NAME
                                 : MTS_PARALLEL_TYPE_LOGICAL_CLOCK;
  LogErr(INFORMATION_LEVEL, ER_RPL_MTA_RECOVERY_STARTING_COORDINATOR);
  recovery_error = start_slave_thread(
      key_thread_replica_sql, handle_slave_sql, &rli->run_lock, &rli->run_lock,
      &rli->start_cond, &rli->slave_running, &rli->slave_run_id, mi);

  if (recovery_error) {
    LogErr(WARNING_LEVEL, ER_RPL_MTA_RECOVERY_FAILED_TO_START_COORDINATOR);
    goto err;
  }
  mysql_mutex_lock(&rli->run_lock);
  mysql_cond_wait(&rli->stop_cond, &rli->run_lock);
  mysql_mutex_unlock(&rli->run_lock);
  if (rli->until_condition != Relay_log_info::UNTIL_DONE) {
    LogErr(WARNING_LEVEL, ER_RPL_MTA_AUTOMATIC_RECOVERY_FAILED);
    goto err;
  }
  rli->clear_until_option();
  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&rli->data_lock);
  recover_relay_log(mi);

  if (mi->flush_info(true) ||
      rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT)) {
    recovery_error = 1;
    mysql_mutex_unlock(&mi->data_lock);
    mysql_mutex_unlock(&rli->data_lock);
    goto err;
  }
  rli->inited = true;
  rli->error_on_rli_init_info = false;
  mysql_mutex_unlock(&mi->data_lock);
  mysql_mutex_unlock(&rli->data_lock);
  LogErr(INFORMATION_LEVEL, ER_RPL_MTA_RECOVERY_SUCCESSFUL);
  return recovery_error;
err:
  /*
    If recovery failed means we failed to initialize rli object in the case
    of MTS. We should not allow the START REPLICA command to work as we do in
    the case of STS. i.e if init_recovery call fails then we set inited=0.
  */
  rli->end_info();
  rli->inited = false;
  rli->error_on_rli_init_info = true;
  rli->clear_until_option();
  return recovery_error;
}

int load_mi_and_rli_from_repositories(
    Master_info *mi, bool ignore_if_no_info, int thread_mask,
    bool skip_received_gtid_set_and_relaylog_recovery, bool force_load) {
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);
  int init_error = 0;
  enum_return_check check_return = ERROR_CHECKING_REPOSITORY;
  THD *thd = current_thd;

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  /*
    When info tables are used and autocommit= 0 we force a new
    transaction start to avoid table access deadlocks when START REPLICA
    is executed after RESET REPLICA.
  */
  if (is_autocommit_off(thd)) {
    if (trans_begin(thd)) {
      init_error = 1;
      goto end;
    }
  }

  /*
    This takes care of the startup dependency between the master_info
    and relay_info. It initializes the master info if the REPLICA_IO
    thread is being started and the relay log info if either the
    REPLICA_SQL thread is being started or was not initialized as it is
    required by the REPLICA_IO thread.
  */
  check_return = mi->check_info();
  if (check_return == ERROR_CHECKING_REPOSITORY) {
    init_error = 1;
    goto end;
  }

  if (!ignore_if_no_info || check_return != REPOSITORY_DOES_NOT_EXIST) {
    if ((thread_mask & REPLICA_IO) != 0) {
      if (!mi->inited || force_load) {
        if (mi->mi_init_info()) {
          init_error = 1;
        }
      }
    }
  }

  check_return = mi->rli->check_info();
  if (check_return == ERROR_CHECKING_REPOSITORY) {
    init_error = 1;
    goto end;
  }
  if (!ignore_if_no_info || check_return != REPOSITORY_DOES_NOT_EXIST) {
    if ((thread_mask & REPLICA_SQL) != 0 || !(mi->rli->inited)) {
      if (!mi->rli->inited || force_load) {
        if (mi->rli->rli_init_info(
                skip_received_gtid_set_and_relaylog_recovery)) {
          init_error = 1;
        } else {
          /*
            During rli_init_info() above, the relay log is opened (if rli was
            not initialized yet). The function below expects the relay log to be
            opened to get its coordinates and store as the last flushed relay
            log coordinates from I/O thread point of view.
          */
          mi->update_flushed_relay_log_info();
        }
      } else {
        // Even if we skip rli_init_info we must check if gaps exist to maintain
        // the server behavior in commands like CHANGE REPLICATION SOURCE
        if (mi->rli->recovery_parallel_workers ? mts_recovery_groups(mi->rli)
                                               : 0)
          init_error = 1;
      }
    }
  }

  DBUG_EXECUTE_IF("enable_mta_worker_failure_init",
                  { DBUG_SET("+d,mta_worker_thread_init_fails"); });
end:
  /*
    When info tables are used and autocommit= 0 we force transaction
    commit to avoid table access deadlocks when START REPLICA is executed
    after RESET REPLICA.
  */
  if (is_autocommit_off(thd))
    if (trans_commit(thd)) init_error = 1;

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  /*
    Handling MTS Relay-log recovery after successful initialization of mi and
    rli objects.

    MTS Relay-log recovery is handled by SSUG command. In order to start the
    slave applier thread rli needs to be inited and mi->rli->data_lock should
    be in released state. Hence we do the MTS recovery at this point of time
    where both conditions are satisfied.
  */
  if (!init_error && mi->rli->is_relay_log_recovery &&
      mi->rli->mts_recovery_group_cnt)
    init_error = fill_mts_gaps_and_recover(mi);
  return init_error;
}

void end_info(Master_info *mi) {
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);

  /*
    The previous implementation was not acquiring locks.  We do the same here.
    However, this is quite strange.
  */
  mi->end_info();
  mi->rli->end_info();
}

void clear_info(Master_info *mi) {
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);

  /*
    Reset errors (the idea is that we forget about the
    old master).
  */
  mi->clear_error();
  mi->rli->clear_error();
  if (mi->rli->workers_array_initialized) {
    for (size_t i = 0; i < mi->rli->get_worker_count(); i++) {
      mi->rli->get_worker(i)->clear_error();
    }
  }
  mi->rli->clear_sql_delay();

  end_info(mi);
}

int remove_info(Master_info *mi) {
  int error = 1;
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);

  /*
    The previous implementation was not acquiring locks.
    We do the same here. However, this is quite strange.
  */
  clear_info(mi);

  if (mi->remove_info() || Rpl_info_factory::reset_workers(mi->rli) ||
      mi->rli->remove_info())
    goto err;

  error = 0;

err:
  return error;
}

bool reset_info(Master_info *mi) {
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);

  clear_info(mi);

  if (mi->remove_info() || Rpl_info_factory::reset_workers(mi->rli))
    return true;

  MUTEX_LOCK(mi_lock, &mi->data_lock);
  MUTEX_LOCK(rli_lock, &mi->rli->data_lock);

  mi->init_master_log_pos();
  mi->master_uuid[0] = 0;

  if (mi->reset && mi->flush_info(true)) {
    my_error(ER_CONNECTION_METADATA, MYF(0));
    return true;
  }

  bool have_relay_log_data_to_persist =              // Only want to keep
      (!mi->rli->is_privilege_checks_user_null() ||  // if PCU is not null
       mi->rli->is_row_format_required() ||          // or RRF is 1
       Relay_log_info::PK_CHECK_STREAM !=            // or RTPKC != STREAM
           mi->rli->get_require_table_primary_key_check());

  if ((have_relay_log_data_to_persist && mi->rli->clear_info()) ||
      (!have_relay_log_data_to_persist && mi->rli->remove_info())) {
    my_error(ER_CONNECTION_METADATA, MYF(0));
    return true;
  }

  return false;
}

int flush_master_info(Master_info *mi, bool force, bool need_lock,
                      bool do_flush_relay_log, bool skip_repo_persistence) {
  DBUG_TRACE;
  assert(mi != nullptr && mi->rli != nullptr);
  DBUG_EXECUTE_IF("fail_to_flush_source_info", { return 1; });

  if (skip_repo_persistence && !do_flush_relay_log) {
    return 0;
  }

  /*
    With the appropriate recovery process, we will not need to flush
    the content of the current log.

    For now, we flush the relay log BEFORE the master.info file, because
    if we crash, we will get a duplicate event in the relay log at restart.
    If we change the order, there might be missing events.

    If we don't do this and the slave server dies when the relay log has
    some parts (its last kilobytes) in memory only, with, say, from master's
    position 100 to 150 in memory only (not on disk), and with position 150
    in master.info, there will be missing information. When the slave restarts,
    the I/O thread will fetch binlogs from 150, so in the relay log we will
    have "[0, 100] U [150, infinity[" and nobody will notice it, so the SQL
    thread will jump from 100 to 150, and replication will silently break.
  */
  mysql_mutex_t *log_lock = mi->rli->relay_log.get_log_lock();
  mysql_mutex_t *data_lock = &mi->data_lock;

  if (need_lock) {
    mysql_mutex_lock(log_lock);
    mysql_mutex_lock(data_lock);
  } else {
    mysql_mutex_assert_owner(log_lock);
    mysql_mutex_assert_owner(&mi->data_lock);
  }

  int err = 0;
  /*
    We can skip flushing the relay log when this function is called from
    queue_event(), as after_write_to_relay_log() will already flush it.
  */
  if (do_flush_relay_log) err |= mi->rli->flush_current_log();

  if (!skip_repo_persistence) err |= mi->flush_info(force);

  if (need_lock) {
    mysql_mutex_unlock(data_lock);
    mysql_mutex_unlock(log_lock);
  }

  return err;
}

/**
  Convert slave skip errors bitmap into a printable string.
*/

static void print_replica_skip_errors(void) {
  /*
    To be safe, we want 10 characters of room in the buffer for a number
    plus terminators. Also, we need some space for constant strings.
    10 characters must be sufficient for a number plus {',' | '...'}
    plus a NUL terminator. That is a max 6 digit number.
  */
  const size_t MIN_ROOM = 10;
  DBUG_TRACE;
  assert(sizeof(slave_skip_error_names) > MIN_ROOM);
  assert(MAX_SLAVE_ERROR <= 999999);  // 6 digits

  if (!use_slave_mask || bitmap_is_clear_all(&slave_error_mask)) {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("OFF"));
    /* purecov: end */
  } else if (bitmap_is_set_all(&slave_error_mask)) {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("ALL"));
    /* purecov: end */
  } else {
    char *buff = slave_skip_error_names;
    char *bend = buff + sizeof(slave_skip_error_names);
    int errnum;

    for (errnum = 0; errnum < MAX_SLAVE_ERROR; errnum++) {
      if (bitmap_is_set(&slave_error_mask, errnum)) {
        if (buff + MIN_ROOM >= bend) break; /* purecov: tested */
        buff = longlong10_to_str(errnum, buff, -10);
        *buff++ = ',';
      }
    }
    if (buff != slave_skip_error_names) buff--;  // Remove last ','
    /*
      The range for client side error is [2000-2999]
      so if the errnum doesn't lie in that and if less
      than MAX_SLAVE_ERROR[10000] we enter the if loop.
    */
    if (errnum < MAX_SLAVE_ERROR &&
        (errnum < CR_MIN_ERROR || errnum > CR_MAX_ERROR)) {
      /* Couldn't show all errors */
      buff = my_stpcpy(buff, "..."); /* purecov: tested */
    }
    *buff = 0;
  }
  DBUG_PRINT("init", ("error_names: '%s'", slave_skip_error_names));
}

/**
 Change arg to the string with the nice, human-readable skip error values.
   @param replica_skip_errors_ptr
          The pointer to be changed
*/
void set_replica_skip_errors(char **replica_skip_errors_ptr) {
  DBUG_TRACE;
  print_replica_skip_errors();
  *replica_skip_errors_ptr = slave_skip_error_names;
}

/**
  Init function to set up array for errors that should be skipped for slave
*/
static void init_replica_skip_errors() {
  DBUG_TRACE;
  assert(!use_slave_mask);  // not already initialized

  if (bitmap_init(&slave_error_mask, nullptr, MAX_SLAVE_ERROR)) {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(MYSQLD_ABORT_EXIT);
  }
  use_slave_mask = true;
}

static void add_replica_skip_errors(const uint *errors, uint n_errors) {
  DBUG_TRACE;
  assert(errors);
  assert(use_slave_mask);

  for (uint i = 0; i < n_errors; i++) {
    const uint err_code = errors[i];
    /*
      The range for client side error is [2000-2999]
      so if the err_code doesn't lie in that and if less
      than MAX_SLAVE_ERROR[14000] we enter the if loop.
    */
    if (err_code < MAX_SLAVE_ERROR &&
        (err_code < CR_MIN_ERROR || err_code > CR_MAX_ERROR))
      bitmap_set_bit(&slave_error_mask, err_code);
  }
}

/*
  Add errors that should be skipped for slave

  SYNOPSIS
    add_replica_skip_errors()
    arg         List of errors numbers to be added to skip, separated with ','

  NOTES
    Called from get_options() in mysqld.cc on start-up
*/

void add_replica_skip_errors(const char *arg) {
  const char *p = nullptr;
  /*
    ALL is only valid when nothing else is provided.
  */
  const uchar SKIP_ALL[] = "all";
  size_t SIZE_SKIP_ALL = strlen((const char *)SKIP_ALL) + 1;
  /*
    IGNORE_DDL_ERRORS can be combined with other parameters
    but must be the first one provided.
  */
  const uchar SKIP_DDL_ERRORS[] = "ddl_exist_errors";
  size_t SIZE_SKIP_DDL_ERRORS = strlen((const char *)SKIP_DDL_ERRORS);
  DBUG_TRACE;

  // initialize mask if not done yet
  if (!use_slave_mask) init_replica_skip_errors();

  for (; my_isspace(system_charset_info, *arg); ++arg) /* empty */
    ;
  if (!my_strnncoll(system_charset_info, pointer_cast<const uchar *>(arg),
                    SIZE_SKIP_ALL, SKIP_ALL, SIZE_SKIP_ALL)) {
    bitmap_set_all(&slave_error_mask);
    return;
  }
  if (!my_strnncoll(system_charset_info, pointer_cast<const uchar *>(arg),
                    SIZE_SKIP_DDL_ERRORS, SKIP_DDL_ERRORS,
                    SIZE_SKIP_DDL_ERRORS)) {
    // DDL errors to be skipped for relaxed 'exist' handling
    const uint ddl_errors[] = {
        // error codes with create/add <schema object>
        ER_DB_CREATE_EXISTS, ER_TABLE_EXISTS_ERROR, ER_DUP_KEYNAME,
        ER_MULTIPLE_PRI_KEY,
        // error codes with change/rename <schema object>
        ER_BAD_FIELD_ERROR, ER_NO_SUCH_TABLE, ER_DUP_FIELDNAME,
        // error codes with drop <schema object>
        ER_DB_DROP_EXISTS, ER_BAD_TABLE_ERROR, ER_CANT_DROP_FIELD_OR_KEY};

    add_replica_skip_errors(ddl_errors,
                            sizeof(ddl_errors) / sizeof(ddl_errors[0]));
    /*
      After processing the SKIP_DDL_ERRORS, the pointer is
      increased to the position after the comma.
    */
    if (strlen(arg) > SIZE_SKIP_DDL_ERRORS + 1) arg += SIZE_SKIP_DDL_ERRORS + 1;
  }
  for (p = arg; *p;) {
    long err_code;
    if (!(p = str2int(p, 10, 0, LONG_MAX, &err_code))) break;
    if (err_code < MAX_SLAVE_ERROR)
      bitmap_set_bit(&slave_error_mask, (uint)err_code);
    while (!my_isdigit(system_charset_info, *p) && *p) p++;
  }
}

static void set_thd_in_use_temporary_tables(Relay_log_info *rli) {
  TABLE *table;

  for (table = rli->save_temporary_tables; table; table = table->next) {
    table->in_use = rli->info_thd;
    if (table->file != nullptr) {
      /*
        Since we are stealing opened temporary tables from one thread to
        another, we need to let the performance schema know that, for aggregates
        per thread to work properly.
      */
      table->file->unbind_psi();
      table->file->rebind_psi();
    }
  }
}

int terminate_slave_threads(Master_info *mi, int thread_mask,
                            ulong stop_wait_timeout, bool need_lock_term) {
  DBUG_TRACE;

  if (!mi->inited) return 0; /* successfully do nothing */
  int error, force_all = (thread_mask & SLAVE_FORCE_ALL);
  mysql_mutex_t *sql_lock{&mi->rli->run_lock}, *io_lock{&mi->run_lock};
  mysql_mutex_t *log_lock = mi->rli->relay_log.get_log_lock();
  /*
    Set it to a variable, so the value is shared by both stop methods.
    This guarantees that the user defined value for the timeout value is for
    the time the 2 threads take to shutdown, and not the time of each thread
    stop operation.
  */
  ulong total_stop_wait_timeout = stop_wait_timeout;

  if (thread_mask & (REPLICA_SQL | SLAVE_FORCE_ALL)) {
    DBUG_PRINT("info", ("Terminating SQL thread"));
    mi->rli->abort_slave = true;

    DEBUG_SYNC(current_thd,
               "terminate_replica_threads_after_set_abort_replica");

    if ((error = terminate_slave_thread(
             mi->rli->info_thd, sql_lock, &mi->rli->stop_cond,
             &mi->rli->slave_running, &total_stop_wait_timeout,
             need_lock_term)) &&
        !force_all) {
      if (error == 1) {
        return ER_STOP_REPLICA_SQL_THREAD_TIMEOUT;
      }
      return error;
    }

    DBUG_PRINT("info", ("Flushing applier metadata."));
    if (current_thd)
      THD_STAGE_INFO(current_thd, stage_flushing_applier_metadata);

    /*
      Flushes the relay log info regardless of the sync_relay_log_info option.
    */
    if (mi->rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT)) {
      return ER_ERROR_DURING_FLUSH_LOGS;
    }
  }

  /*
    Only stops the monitoring thread if this is the only failover channel
    running.
  */
  if ((thread_mask & (SLAVE_MONITOR | SLAVE_FORCE_ALL)) &&
      channel_map.get_number_of_connection_auto_failover_channels_running() ==
          1) {
    DBUG_PRINT("info", ("Terminating Monitor IO thread"));
    if ((error = Source_IO_monitor::get_instance()
                     ->terminate_monitoring_process()) &&
        !force_all) {
      if (error == 1) {
        return ER_STOP_REPLICA_MONITOR_IO_THREAD_TIMEOUT;
      }
      return error;
    }
  }

  if (thread_mask & (REPLICA_IO | SLAVE_FORCE_ALL)) {
    DBUG_PRINT("info", ("Terminating IO thread"));
    mi->abort_slave = true;
    DBUG_EXECUTE_IF("pause_after_queue_event",
                    { rpl_replica_debug_point(DBUG_RPL_S_PAUSE_QUEUE_EV); });
    /*
      If the I/O thread is running and waiting for disk space,
      the signal above will not make it to stop.
    */
    bool io_waiting_disk_space =
        mi->slave_running && mi->info_thd->is_waiting_for_disk_space();

    /*
      If we are shutting down the server and the I/O thread is waiting for
      disk space, tell the terminate_slave_thread to forcefully kill the I/O
      thread by sending a KILL_CONNECTION signal that will be listened by
      my_write function.
    */
    bool force_io_stop =
        io_waiting_disk_space && (thread_mask & SLAVE_FORCE_ALL);

    // If not shutting down, let the user to decide to abort I/O thread or wait
    if (io_waiting_disk_space && !force_io_stop) {
      LogErr(WARNING_LEVEL, ER_STOP_REPLICA_IO_THREAD_DISK_SPACE,
             mi->get_channel());
      DBUG_EXECUTE_IF("simulate_io_thd_wait_for_disk_space", {
        rpl_replica_debug_point(DBUG_RPL_S_IO_WAIT_FOR_SPACE);
      });
    }

    if ((error = terminate_slave_thread(
             mi->info_thd, io_lock, &mi->stop_cond, &mi->slave_running,
             &total_stop_wait_timeout, need_lock_term, force_io_stop)) &&
        !force_all) {
      if (error == 1) {
        return ER_STOP_REPLICA_IO_THREAD_TIMEOUT;
      }
      return error;
    }

#ifndef NDEBUG
    if (force_io_stop) {
      if (DBUG_EVALUATE_IF("simulate_io_thd_wait_for_disk_space", 1, 0)) {
        DBUG_SET("-d,simulate_io_thd_wait_for_disk_space");
      }
    }
#endif

    mysql_mutex_lock(log_lock);

    DBUG_PRINT("info", ("Flushing relay log and source info repository."));
    if (current_thd)
      THD_STAGE_INFO(current_thd,
                     stage_flushing_applier_and_connection_metadata);

    /*
      Flushes the master info regardless of the sync_source_info option and
      GTID_ONLY = 0 for this channel
    */
    if (!mi->is_gtid_only_mode()) {
      mysql_mutex_lock(&mi->data_lock);
      if (mi->flush_info(true)) {
        mysql_mutex_unlock(&mi->data_lock);
        mysql_mutex_unlock(log_lock);
        return ER_ERROR_DURING_FLUSH_LOGS;
      }
      mysql_mutex_unlock(&mi->data_lock);
    }
    /*
      Flushes the relay log regardless of the sync_relay_log option.
    */
    if (mi->rli->relay_log.is_open() &&
        mi->rli->relay_log.flush_and_sync(true)) {
      mysql_mutex_unlock(log_lock);
      return ER_ERROR_DURING_FLUSH_LOGS;
    }

    mysql_mutex_unlock(log_lock);
  }
  return 0;
}

/**
   Wait for a slave thread to terminate.

   This function is called after requesting the thread to terminate
   (by setting @c abort_slave member of @c Relay_log_info or @c
   Master_info structure to 1). Termination of the thread is
   controlled with the the predicate <code>*slave_running</code>.

   Function will acquire @c term_lock before waiting on the condition
   unless @c need_lock_term is false in which case the mutex should be
   owned by the caller of this function and will remain acquired after
   return from the function.

   @param thd
          Current session.
   @param term_lock
          Associated lock to use when waiting for @c term_cond

   @param term_cond
          Condition that is signalled when the thread has terminated

   @param slave_running
          Pointer to predicate to check for slave thread termination

   @param stop_wait_timeout
          A pointer to a variable that denotes the time the thread has
          to stop before we time out and throw an error.

   @param need_lock_term
          If @c false the lock will not be acquired before waiting on
          the condition. In this case, it is assumed that the calling
          function acquires the lock before calling this function.

   @param force
          Force the slave thread to stop by sending a KILL_CONNECTION
          signal to it. This is used to forcefully stop the I/O thread
          when it is waiting for disk space and the server is shutting
          down.

   @retval 0 All OK, 1 on "STOP REPLICA" command timeout,
   ER_REPLICA_CHANNEL_NOT_RUNNING otherwise.

   @note  If the executing thread has to acquire term_lock
          (need_lock_term is true, the negative running status does not
          represent any issue therefore no error is reported.

 */
static int terminate_slave_thread(THD *thd, mysql_mutex_t *term_lock,
                                  mysql_cond_t *term_cond,
                                  std::atomic<uint> *slave_running,
                                  ulong *stop_wait_timeout, bool need_lock_term,
                                  bool force) {
  DBUG_TRACE;
  if (need_lock_term) {
    mysql_mutex_lock(term_lock);
  } else {
    mysql_mutex_assert_owner(term_lock);
  }
  if (!*slave_running) {
    if (need_lock_term) {
      /*
        if run_lock (term_lock) is acquired locally then either
        slave_running status is fine
      */
      mysql_mutex_unlock(term_lock);
      return 0;
    } else {
      return ER_REPLICA_CHANNEL_NOT_RUNNING;
    }
  }
  assert(thd != nullptr);
  THD_CHECK_SENTRY(thd);

  /*
    Is is critical to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */

  while (*slave_running)  // Should always be true
  {
    DBUG_PRINT("loop", ("killing replica thread"));

    mysql_mutex_lock(&thd->LOCK_thd_data);
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
#ifndef _WIN32
    int err [[maybe_unused]] = pthread_kill(thd->real_id, SIGALRM);
    assert(err != EINVAL);
#endif
    if (force)
      thd->awake(THD::KILL_CONNECTION);
    else
      thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    DBUG_EXECUTE_IF("block_on_thread_stop_after_awake", {
      rpl_replica_debug_point(DBUG_RPL_R_WAIT_AFTER_AWAKE_ON_THREAD_STOP);
    });

    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect against it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(&abstime, 2);
#ifndef NDEBUG
    int error =
#endif
        mysql_cond_timedwait(term_cond, term_lock, &abstime);
    if ((*stop_wait_timeout) >= 2)
      (*stop_wait_timeout) = (*stop_wait_timeout) - 2;
    else if (*slave_running) {
      if (need_lock_term) mysql_mutex_unlock(term_lock);
      return 1;
    }
    assert(error == ETIMEDOUT || error == 0);
  }

  assert(*slave_running == 0);

  if (need_lock_term) mysql_mutex_unlock(term_lock);
  return 0;
}

bool start_slave_thread(PSI_thread_key thread_key, my_start_routine h_func,
                        mysql_mutex_t *start_lock, mysql_mutex_t *cond_lock,
                        mysql_cond_t *start_cond,
                        std::atomic<uint> *slave_running,
                        std::atomic<ulong> *slave_run_id, Master_info *mi) {
  bool is_error = false;
  my_thread_handle th;
  ulong start_id;
  DBUG_TRACE;

  if (start_lock) mysql_mutex_lock(start_lock);
  if (!server_id) {
    if (start_cond) mysql_cond_broadcast(start_cond);
    LogErr(ERROR_LEVEL, ER_RPL_SERVER_ID_MISSING, mi->get_for_channel_str());
    my_error(ER_BAD_REPLICA, MYF(0));
    goto err;
  }

  if (*slave_running) {
    if (start_cond) mysql_cond_broadcast(start_cond);
    my_error(ER_REPLICA_CHANNEL_MUST_STOP, MYF(0), mi->get_channel());
    goto err;
  }
  start_id = *slave_run_id;
  DBUG_PRINT("info", ("Creating new replica thread"));
  if (mysql_thread_create(thread_key, &th, &connection_attrib, h_func,
                          (void *)mi)) {
    LogErr(ERROR_LEVEL, ER_RPL_CANT_CREATE_REPLICA_THREAD,
           mi->get_for_channel_str());
    my_error(ER_REPLICA_THREAD, MYF(0));
    goto err;
  }
  if (start_cond && cond_lock)  // caller has cond_lock
  {
    THD *thd = current_thd;
    while (start_id == *slave_run_id && thd != nullptr) {
      DBUG_PRINT("sleep", ("Waiting for replica thread to start"));
      PSI_stage_info saved_stage = {0, "", 0, ""};
      thd->ENTER_COND(start_cond, cond_lock,
                      &stage_waiting_for_replica_thread_to_start, &saved_stage);
      /*
        It is not sufficient to test this at loop bottom. We must test
        it after registering the mutex in enter_cond(). If the kill
        happens after testing of thd->killed and before the mutex is
        registered, we could otherwise go waiting though thd->killed is
        set.
      */
      if (!thd->killed) mysql_cond_wait(start_cond, cond_lock);
      mysql_mutex_unlock(cond_lock);
      thd->EXIT_COND(&saved_stage);
      mysql_mutex_lock(cond_lock);  // re-acquire it
      if (thd->killed) {
        my_error(thd->killed, MYF(0));
        goto err;
      }
    }
  }

  goto end;
err:
  is_error = true;
end:

  if (start_lock) mysql_mutex_unlock(start_lock);
  return is_error;
}

/*
  start_slave_threads()

  NOTES
    SLAVE_FORCE_ALL is not implemented here on purpose since it does not make
    sense to do that for starting a slave--we always care if it actually
    started the threads that were not previously running
*/

bool start_slave_threads(bool need_lock_slave, bool wait_for_start,
                         Master_info *mi, int thread_mask) {
  mysql_mutex_t *lock_io{nullptr}, *lock_sql{nullptr}, *lock_cond_io{nullptr},
      *lock_cond_sql{nullptr};
  mysql_cond_t *cond_io{nullptr}, *cond_sql{nullptr};
  bool is_error{false};
  DBUG_TRACE;
  DBUG_EXECUTE_IF("uninitialized_source-info_structure", mi->inited = false;);

  if (!mi->inited || !mi->rli->inited) {
    int error = (!mi->inited ? ER_REPLICA_CM_INIT_REPOSITORY
                             : ER_REPLICA_AM_INIT_REPOSITORY);
    Rpl_info *info = (!mi->inited ? mi : static_cast<Rpl_info *>(mi->rli));
    const char *prefix = current_thd ? ER_THD_NONCONST(current_thd, error)
                                     : ER_DEFAULT_NONCONST(error);
    info->report(ERROR_LEVEL,
                 (!mi->inited ? ER_SERVER_REPLICA_CM_INIT_REPOSITORY
                              : ER_SERVER_REPLICA_AM_INIT_REPOSITORY),
                 prefix, nullptr);
    my_error(error, MYF(0));
    return true;
  }

  if (check_replica_configuration_errors(mi, thread_mask)) return true;

  /**
    SQL AFTER MTS GAPS has no effect when GTID_MODE=ON and SOURCE_AUTO_POS=1
    as no gaps information was collected.
  **/
  if (global_gtid_mode.get() == Gtid_mode::ON && mi->is_auto_position() &&
      mi->rli->until_condition == Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS) {
    if (current_thd) {
      push_warning_printf(
          current_thd, Sql_condition::SL_WARNING,
          ER_WARN_SQL_AFTER_MTS_GAPS_GAP_NOT_CALCULATED,
          ER_THD(current_thd, ER_WARN_SQL_AFTER_MTS_GAPS_GAP_NOT_CALCULATED),
          mi->get_channel());
    }
  }

  if (need_lock_slave) {
    lock_io = &mi->run_lock;
    lock_sql = &mi->rli->run_lock;
  }
  if (wait_for_start) {
    cond_io = &mi->start_cond;
    cond_sql = &mi->rli->start_cond;
    lock_cond_io = &mi->run_lock;
    lock_cond_sql = &mi->rli->run_lock;
  }

  if (thread_mask & REPLICA_IO)
    is_error = start_slave_thread(key_thread_replica_io, handle_slave_io,
                                  lock_io, lock_cond_io, cond_io,
                                  &mi->slave_running, &mi->slave_run_id, mi);

  if (!is_error && (thread_mask & (REPLICA_IO | SLAVE_MONITOR)) &&
      mi->is_source_connection_auto_failover() &&
      !Source_IO_monitor::get_instance()->is_monitoring_process_running()) {
    is_error = Source_IO_monitor::get_instance()->launch_monitoring_process(
        key_thread_replica_monitor_io);

    if (is_error)
      terminate_slave_threads(mi, thread_mask & (REPLICA_IO | SLAVE_MONITOR),
                              rpl_stop_replica_timeout, need_lock_slave);
  }

  if (!is_error && (thread_mask & REPLICA_SQL)) {
    /*
      MTS-recovery gaps gathering is placed onto common execution path
      for either START-SLAVE and --skip-start-replica= 0
    */
    if (mi->rli->recovery_parallel_workers != 0) {
      if (mts_recovery_groups(mi->rli)) {
        is_error = true;
        my_error(ER_MTA_RECOVERY_FAILURE, MYF(0));
      }
    }
    if (!is_error)
      is_error = start_slave_thread(
          key_thread_replica_sql, handle_slave_sql, lock_sql, lock_cond_sql,
          cond_sql, &mi->rli->slave_running, &mi->rli->slave_run_id, mi);
    if (is_error)
      terminate_slave_threads(mi, thread_mask & (REPLICA_IO | SLAVE_MONITOR),
                              rpl_stop_replica_timeout, need_lock_slave);
  }
  return is_error;
}

/*
  Release slave threads at time of executing shutdown.

  SYNOPSIS
    end_slave()
*/

void end_slave() {
  DBUG_TRACE;

  Master_info *mi = nullptr;

  /*
    This is called when the server terminates, in close_connections().
    It terminates slave threads. However, some CHANGE REPLICATION SOURCE etc may
    still be running presently. If a START REPLICA was in progress, the mutex
    lock below will make us wait until slave threads have started, and START
    REPLICA returns, then we terminate them here.
  */
  channel_map.wrlock();

  /* traverse through the map and terminate the threads */
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;

    if (mi)
      terminate_slave_threads(mi, SLAVE_FORCE_ALL, rpl_stop_replica_timeout);
  }
  channel_map.unlock();
}

/**
   Free all resources used by slave threads at time of executing shutdown.
   The routine must be called after all possible users of channel_map
   have left.

*/
void delete_slave_info_objects() {
  DBUG_TRACE;

  Master_info *mi = nullptr;

  channel_map.wrlock();

  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;

    if (mi) {
      mi->channel_wrlock();
      end_info(mi);
      if (mi->rli) delete mi->rli;
      delete mi;
      it->second = nullptr;
    }
  }

  // Clean other types of channel
  for (mi_map::iterator it = channel_map.begin(GROUP_REPLICATION_CHANNEL);
       it != channel_map.end(GROUP_REPLICATION_CHANNEL); it++) {
    mi = it->second;

    if (mi) {
      mi->channel_wrlock();
      end_info(mi);
      if (mi->rli) delete mi->rli;
      delete mi;
      it->second = nullptr;
    }
  }

  channel_map.unlock();
}

/**
   Check if in multi-statement transaction mode

   @param thd    THD object

   @retval true  Success
   @retval false Failure
*/
static bool is_autocommit_off(THD *thd) {
  DBUG_TRACE;
  return (thd && thd->in_multi_stmt_transaction_mode());
}

static bool monitor_io_replica_killed(THD *thd, Master_info *mi) {
  return Source_IO_monitor::get_instance()->is_monitor_killed(thd, mi);
}

static bool io_slave_killed(THD *thd, Master_info *mi) {
  DBUG_TRACE;

  assert(mi->info_thd == thd);
  assert(mi->slave_running);  // tracking buffer overrun
  return mi->abort_slave || connection_events_loop_aborted() || thd->killed;
}

/**
   The function analyzes a possible killed status and makes
   a decision whether to accept it or not.
   Normally upon accepting the sql thread goes to shutdown.
   In the event of deferring decision @c rli->last_event_start_time waiting
   timer is set to force the killed status be accepted upon its expiration.

   Notice Multi-Threaded-Slave behaves similarly in that when it's being
   stopped and the current group of assigned events has not yet scheduled
   completely, Coordinator defers to accept to leave its read-distribute
   state. The above timeout ensures waiting won't last endlessly, and in
   such case an error is reported.

   @param thd   pointer to a THD instance
   @param rli   pointer to Relay_log_info instance

   @return true the killed status is recognized, false a possible killed
           status is deferred.
*/
bool sql_slave_killed(THD *thd, Relay_log_info *rli) {
  bool is_parallel_warn = false;

  DBUG_TRACE;

  assert(rli->info_thd == thd);
  assert(rli->slave_running == 1);
  if (rli->sql_thread_kill_accepted) return true;
  DBUG_EXECUTE_IF("stop_when_mta_in_group", rli->abort_slave = 1;
                  DBUG_SET("-d,stop_when_mta_in_group");
                  DBUG_SET("-d,simulate_stop_when_mta_in_group");
                  return false;);
  if (connection_events_loop_aborted() || thd->killed || rli->abort_slave) {
    rli->sql_thread_kill_accepted = true;
    is_parallel_warn =
        (rli->is_parallel_exec() && (rli->is_mts_in_group() || thd->killed));
    /*
      Slave can execute stop being in one of two MTS or Single-Threaded mode.
      The modes define different criteria to accept the stop.
      In particular that relates to the concept of groupping.
      Killed Coordinator thread expects the worst so it warns on
      possible consistency issue.
    */
    if (is_parallel_warn || (!rli->is_parallel_exec() &&
                             thd->get_transaction()->cannot_safely_rollback(
                                 Transaction_ctx::SESSION) &&
                             rli->is_in_group())) {
      char msg_stopped[] =
          "... Replica SQL Thread stopped with incomplete event group "
          "having non-transactional changes. "
          "If the group consists solely of row-based events, you can try "
          "to restart the replica with --replica-exec-mode=IDEMPOTENT, which "
          "ignores duplicate key, key not found, and similar errors (see "
          "documentation for details).";
      char msg_stopped_mts[] =
          "... The replica coordinator and worker threads are stopped, "
          "possibly "
          "leaving data in inconsistent state. A restart should "
          "restore consistency automatically, although using non-transactional "
          "storage for data or info tables or DDL queries could lead to "
          "problems. "
          "In such cases you have to examine your data (see documentation for "
          "details).";

      if (rli->abort_slave) {
        DBUG_PRINT("info",
                   ("Request to stop replica SQL Thread received while "
                    "applying an MTA group or a group that "
                    "has non-transactional "
                    "changes; waiting for completion of the group ... "));

        /*
          Slave sql thread shutdown in face of unfinished group modified
          Non-trans table is handled via a timer. The slave may eventually
          give out to complete the current group and in that case there
          might be issues at consequent slave restart, see the error message.
          WL#2975 offers a robust solution requiring to store the last exectuted
          event's coordinates along with the group's coordianates
          instead of waiting with @c last_event_start_time the timer.
        */

        if (rli->last_event_start_time == 0)
          rli->last_event_start_time = time(nullptr);
        rli->sql_thread_kill_accepted =
            difftime(time(nullptr), rli->last_event_start_time) <=
                    SLAVE_WAIT_GROUP_DONE
                ? false
                : true;

        DBUG_EXECUTE_IF("stop_replica_middle_group",
                        DBUG_EXECUTE_IF("incomplete_group_in_relay_log",
                                        rli->sql_thread_kill_accepted =
                                            true;););  // time is over

        if (!rli->sql_thread_kill_accepted && !rli->reported_unsafe_warning) {
          rli->report(
              WARNING_LEVEL, 0,
              !is_parallel_warn
                  ? "Request to stop replica SQL Thread received while "
                    "applying a group that has non-transactional "
                    "changes; waiting for completion of the group ... "
                  : "Coordinator thread of multi-threaded replica is being "
                    "stopped in the middle of assigning a group of events; "
                    "deferring to exit until the group completion ... ");
          rli->reported_unsafe_warning = true;
        }
      }
      if (rli->sql_thread_kill_accepted) {
        rli->last_event_start_time = 0;
        if (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP) {
          rli->mts_group_status = Relay_log_info::MTS_KILLED_GROUP;
        }
        if (is_parallel_warn)
          rli->report(!rli->is_error()
                          ? ERROR_LEVEL
                          : WARNING_LEVEL,  // an error was reported by Worker
                      ER_MTA_INCONSISTENT_DATA,
                      ER_THD(thd, ER_MTA_INCONSISTENT_DATA), msg_stopped_mts);
        else
          rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                      ER_THD(thd, ER_REPLICA_FATAL_ERROR), msg_stopped);
      }
    }
  }
  return rli->sql_thread_kill_accepted;
}

bool net_request_file(NET *net, const char *fname) {
  DBUG_TRACE;
  return net_write_command(net, 251, pointer_cast<const uchar *>(fname),
                           strlen(fname), pointer_cast<const uchar *>(""), 0);
}

/*
  From other comments and tests in code, it looks like
  sometimes Query_log_event and Load_log_event can have db == 0
  (see rewrite_db() above for example)
  (cases where this happens are unclear; it may be when the master is 3.23).
*/

const char *print_slave_db_safe(const char *db) {
  DBUG_TRACE;

  return (db ? db : "");
}

bool is_network_error(uint errorno) {
  return errorno == CR_CONNECTION_ERROR || errorno == CR_CONN_HOST_ERROR ||
         errorno == CR_SERVER_GONE_ERROR || errorno == CR_SERVER_LOST ||
         errorno == ER_CON_COUNT_ERROR || errorno == ER_SERVER_SHUTDOWN ||
         errorno == ER_NET_READ_INTERRUPTED ||
         errorno == ER_NET_WRITE_INTERRUPTED;
}

enum enum_command_status {
  COMMAND_STATUS_OK,
  COMMAND_STATUS_ERROR,
  COMMAND_STATUS_ALLOWED_ERROR
};
/**
  Execute an initialization query for the IO thread.

  If there is an error, then this function calls mysql_free_result;
  otherwise the MYSQL object holds the result after this call.  If
  there is an error other than allowed_error, then this function
  prints a message and returns -1.

  @param mi Master_info object.
  @param query Query string.
  @param allowed_error Allowed error code, or 0 if no errors are allowed.
  @param[out] master_res If this is not NULL and there is no error, then
  mysql_store_result() will be called and the result stored in this pointer.
  @param[out] master_row If this is not NULL and there is no error, then
  mysql_fetch_row() will be called and the result stored in this pointer.

  @retval COMMAND_STATUS_OK No error.
  @retval COMMAND_STATUS_ALLOWED_ERROR There was an error and the
  error code was 'allowed_error'.
  @retval COMMAND_STATUS_ERROR There was an error and the error code
  was not 'allowed_error'.
*/
static enum_command_status io_thread_init_command(
    Master_info *mi, const char *query, int allowed_error,
    MYSQL_RES **master_res = nullptr, MYSQL_ROW *master_row = nullptr) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("IO thread initialization command: '%s'", query));
  MYSQL *mysql = mi->mysql;
  int ret = mysql_real_query(mysql, query, static_cast<ulong>(strlen(query)));
  if (io_slave_killed(mi->info_thd, mi)) {
    LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_IO_THREAD_WAS_KILLED,
           mi->get_for_channel_str(), query);
    mysql_free_result(mysql_store_result(mysql));
    return COMMAND_STATUS_ERROR;
  }
  if (ret != 0) {
    uint err{mysql_errno(mysql)};
    mysql_free_result(mysql_store_result(mysql));
    if (is_network_error(err)) mi->set_network_error();
    if (!err || (int)err != allowed_error) {
      mi->report(is_network_error(err) ? WARNING_LEVEL : ERROR_LEVEL, err,
                 "The replica IO thread stops because the initialization query "
                 "'%s' failed with error '%s'.",
                 query, mysql_error(mysql));
      return COMMAND_STATUS_ERROR;
    }
    return COMMAND_STATUS_ALLOWED_ERROR;
  }
  if (master_res != nullptr) {
    if ((*master_res = mysql_store_result(mysql)) == nullptr) {
      uint err{mysql_errno(mysql)};
      if (is_network_error(err)) mi->set_network_error();
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "The replica IO thread stops because the initialization query "
                 "'%s' did not return any result.",
                 query);
      return COMMAND_STATUS_ERROR;
    }
    if (master_row != nullptr) {
      if ((*master_row = mysql_fetch_row(*master_res)) == nullptr) {
        uint err{mysql_errno(mysql)};
        if (is_network_error(err)) mi->set_network_error();
        mysql_free_result(*master_res);
        mi->report(
            WARNING_LEVEL, mysql_errno(mysql),
            "The replica IO thread stops because the initialization query "
            "'%s' did not return any row.",
            query);
        return COMMAND_STATUS_ERROR;
      }
    }
  } else
    assert(master_row == nullptr);
  return COMMAND_STATUS_OK;
}

/**
  Set user variables after connecting to the master.

  @param  mysql MYSQL to request uuid from master.
  @param  mi    Master_info to set master_uuid

  @return 0: Success, 1: Fatal error, 2: Transient network error.
 */
int io_thread_init_commands(MYSQL *mysql, Master_info *mi) {
  char query[256];
  int ret = 0;
  DBUG_EXECUTE_IF("fake_5_5_version_replica", return ret;);

  mi->reset_network_error();

  sprintf(query, "SET @slave_uuid = '%s', @replica_uuid = '%s'", server_uuid,
          server_uuid);
  if (mysql_real_query(mysql, query, static_cast<ulong>(strlen(query))) &&
      !check_io_slave_killed(mi->info_thd, mi, nullptr))
    goto err;

  mysql_free_result(mysql_store_result(mysql));
  return ret;

err:
  if (mysql_errno(mysql) && is_network_error(mysql_errno(mysql))) {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "The initialization command '%s' failed with the following"
               " error: '%s'.",
               query, mysql_error(mysql));
    mi->set_network_error();
    ret = 2;
  } else {
    char errmsg[512];
    const char *errmsg_fmt =
        "The replica I/O thread stops because a fatal error is encountered "
        "when it tries to send query to source(query: %s).";

    sprintf(errmsg, errmsg_fmt, query);
    mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
               ER_THD(current_thd, ER_REPLICA_FATAL_ERROR), errmsg);
    ret = 1;
  }
  mysql_free_result(mysql_store_result(mysql));
  return ret;
}

/**
  Get master's uuid on connecting.

  @param  mysql MYSQL to request uuid from master.
  @param  mi    Master_info to set master_uuid

  @return 0: Success, 1: Fatal error, 2: Transient network error.
*/
static int get_master_uuid(MYSQL *mysql, Master_info *mi) {
  const char *errmsg;
  MYSQL_RES *master_res = nullptr;
  MYSQL_ROW master_row = nullptr;
  int ret = 0;
  char query_buf[] = "SELECT @@GLOBAL.SERVER_UUID";

  mi->reset_network_error();

  DBUG_EXECUTE_IF("dbug.return_null_SOURCE_UUID", {
    mi->master_uuid[0] = 0;
    return 0;
  };);

  DBUG_EXECUTE_IF("dbug.before_get_SOURCE_UUID",
                  { rpl_replica_debug_point(DBUG_RPL_S_BEFORE_MASTER_UUID); };);

  DBUG_EXECUTE_IF("dbug.simulate_busy_io",
                  { rpl_replica_debug_point(DBUG_RPL_S_SIMULATE_BUSY_IO); };);
#ifndef NDEBUG
  DBUG_EXECUTE_IF("dbug.simulate_no_such_var_server_uuid", {
    query_buf[strlen(query_buf) - 1] = '_';  // corrupt the last char
  });
#endif
  if (!mysql_real_query(mysql, STRING_WITH_LEN(query_buf)) &&
      (master_res = mysql_store_result(mysql)) &&
      (master_row = mysql_fetch_row(master_res))) {
    if (!strcmp(::server_uuid, master_row[0]) &&
        !mi->rli->replicate_same_server_id) {
      errmsg =
          "The replica I/O thread stops because source and replica have equal "
          "MySQL server UUIDs; these UUIDs must be different for "
          "replication to work.";
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(current_thd, ER_REPLICA_FATAL_ERROR), errmsg);
      // Fatal error
      ret = 1;
    } else {
      if (mi->master_uuid[0] != 0) {
        if (strcmp(mi->master_uuid, master_row[0])) {
          bool is_host_port_unchanged{false};
          char new_source_uuid[UUID_LENGTH + 1];
          strncpy(new_source_uuid, master_row[0], UUID_LENGTH);
          new_source_uuid[UUID_LENGTH] = 0;
          if (!mi->m_uuid_from_host.empty() && mi->m_uuid_from_port != 0) {
            if (mi->m_uuid_from_host.compare(mi->host) == 0 &&
                mi->m_uuid_from_port == mi->port) {
              is_host_port_unchanged = true;
            }
          }
          if (is_host_port_unchanged) {
            LogErr(WARNING_LEVEL,
                   ER_RPL_REPLICA_SOURCE_UUID_HAS_CHANGED_HOST_PORT_UNCHANGED,
                   mi->host, mi->port, mi->master_uuid, new_source_uuid);
          } else {
            LogErr(INFORMATION_LEVEL,
                   ER_RPL_REPLICA_SOURCE_UUID_HOST_PORT_HAS_CHANGED,
                   mi->m_uuid_from_host.c_str(), mi->m_uuid_from_port,
                   mi->master_uuid, mi->host, mi->port, new_source_uuid);
          }
        } else {
          if (!mi->m_uuid_from_host.empty() && mi->m_uuid_from_port != 0 &&
              mi->m_uuid_from_host.compare(mi->host) != 0 &&
              mi->m_uuid_from_port != mi->port) {
            LogErr(WARNING_LEVEL, ER_RPL_REPLICA_SOURCE_UUID_HAS_NOT_CHANGED,
                   mi->m_uuid_from_host.c_str(), mi->m_uuid_from_port, mi->host,
                   mi->port, mi->master_uuid);
          }
        }
      }
      strncpy(mi->master_uuid, master_row[0], UUID_LENGTH);
      mi->master_uuid[UUID_LENGTH] = 0;
      mi->m_uuid_from_host.assign(mi->host);
      mi->m_uuid_from_port = mi->port;
    }
  } else if (mysql_errno(mysql) != ER_UNKNOWN_SYSTEM_VARIABLE) {
    if (is_network_error(mysql_errno(mysql))) {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get source SERVER_UUID failed with error: %s",
                 mysql_error(mysql));
      mi->set_network_error();
      ret = 2;
    } else {
      /* Fatal error */
      errmsg =
          "The replica I/O thread stops because a fatal error is encountered "
          "when it tries to get the value of SERVER_UUID variable from source.";
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(current_thd, ER_REPLICA_FATAL_ERROR), errmsg);
      ret = 1;
    }
  } else {
    mi->master_uuid[0] = 0;
    mi->report(
        WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
        "Unknown system variable 'SERVER_UUID' on source. "
        "A probable cause is that the variable is not supported on the "
        "source (version: %s), even though it is on the replica (version: %s)",
        mysql->server_version, server_version);
  }

  if (master_res) mysql_free_result(master_res);
  return ret;
}

/*
  Note that we rely on the master's version (3.23, 4.0.14 etc) instead of
  relying on the binlog's version. This is not perfect: imagine an upgrade
  of the master without waiting that all slaves are in sync with the master;
  then a slave could be fooled about the binlog's format. This is what happens
  when people upgrade a 3.23 master to 4.0 without doing
  RESET BINARY LOGS AND GTIDS: 4.0 slaves are fooled.
  So we do this only to distinguish between 3.23 and more
  recent masters (it's too late to change things for 3.23).

  RETURNS
  0       ok
  1       error
  2       transient network problem, the caller should try to reconnect
*/

static int get_master_version_and_clock(MYSQL *mysql, Master_info *mi) {
  char err_buff[MAX_SLAVE_ERRMSG];
  const char *errmsg = nullptr;
  int err_code = 0;
  int version_number = 0;
  version_number = atoi(mysql->server_version);

  MYSQL_RES *master_res = nullptr;
  MYSQL_ROW master_row;
  DBUG_TRACE;

  DBUG_EXECUTE_IF("unrecognized_source_version", { version_number = 1; };);

  mi->reset_network_error();

  if (!my_isdigit(&my_charset_bin, *mysql->server_version) ||
      version_number < 5) {
    errmsg = "Source reported unrecognized MySQL version";
    err_code = ER_REPLICA_FATAL_ERROR;
    sprintf(err_buff, ER_THD_NONCONST(current_thd, err_code), errmsg);
    goto err;
  }

  mysql_mutex_lock(mi->rli->relay_log.get_log_lock());
  mysql_mutex_lock(&mi->data_lock);
  mi->set_mi_description_event(new Format_description_log_event());
  /* as we are here, we tried to allocate the event */
  if (mi->get_mi_description_event() == nullptr) {
    mysql_mutex_unlock(&mi->data_lock);
    mysql_mutex_unlock(mi->rli->relay_log.get_log_lock());
    errmsg = "default Format_description_log_event";
    err_code = ER_REPLICA_CREATE_EVENT_FAILURE;
    sprintf(err_buff, ER_THD_NONCONST(current_thd, err_code), errmsg);
    goto err;
  }

  /*
    FD_q's (A) is set initially from RL's (A): FD_q.(A) := RL.(A).
    It's necessary to adjust FD_q.(A) at this point because in the following
    course FD_q is going to be dumped to RL.
    Generally FD_q is derived from a received FD_m (roughly FD_q := FD_m)
    in queue_event and the master's (A) is installed.
    At one step with the assignment the Relay-Log's checksum alg is set to
    a new value: RL.(A) := FD_q.(A). If the slave service is stopped
    the last time assigned RL.(A) will be passed over to the restarting
    service (to the current execution point).
    RL.A is a "codec" to verify checksum in queue_event() almost all the time
    the first fake Rotate event.
    Starting from this point IO thread will executes the following checksum
    warmup sequence  of actions:

    FD_q.A := RL.A,
    A_m^0 := master.@@global.binlog_checksum,
    {queue_event(R_f): verifies(R_f, A_m^0)},
    {queue_event(FD_m): verifies(FD_m, FD_m.A), dump(FD_q), rotate(RL),
                        FD_q := FD_m, RL.A := FD_q.A)}

    See legends definition on MYSQL_BIN_LOG::relay_log_checksum_alg
    docs lines (binlog.h).
    In above A_m^0 - the value of master's
    @@binlog_checksum determined in the upcoming handshake (stored in
    mi->checksum_alg_before_fd).


    After the warm-up sequence IO gets to "normal" checksum verification mode
    to use RL.A in

    {queue_event(E_m): verifies(E_m, RL.A)}

    until it has received a new FD_m.
  */
  mi->get_mi_description_event()->common_footer->checksum_alg =
      mi->rli->relay_log.relay_log_checksum_alg;

  assert(mi->get_mi_description_event()->common_footer->checksum_alg !=
         mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);
  assert(mi->rli->relay_log.relay_log_checksum_alg !=
         mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);

  mysql_mutex_unlock(&mi->data_lock);
  mysql_mutex_unlock(mi->rli->relay_log.get_log_lock());

  /*
    Compare the master and slave's clock. Do not die if master's clock is
    unavailable (very old master not supporting UNIX_TIMESTAMP()?).
  */

  DBUG_EXECUTE_IF("dbug.before_get_UNIX_TIMESTAMP", {
    rpl_replica_debug_point(DBUG_RPL_S_BEFORE_UNIX_TIMESTAMP);
  };);

  master_res = nullptr;
  if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT UNIX_TIMESTAMP()")) &&
      (master_res = mysql_store_result(mysql)) &&
      (master_row = mysql_fetch_row(master_res))) {
    mysql_mutex_lock(&mi->data_lock);
    mi->clock_diff_with_master =
        (long)(time((time_t *)nullptr) - strtoul(master_row[0], nullptr, 10));
    DBUG_EXECUTE_IF("dbug.mta.force_clock_diff_eq_0",
                    mi->clock_diff_with_master = 0;);
    mysql_mutex_unlock(&mi->data_lock);
  } else if (check_io_slave_killed(mi->info_thd, mi, nullptr))
    goto slave_killed_err;
  else if (is_network_error(mysql_errno(mysql))) {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "Get source clock failed with error: %s", mysql_error(mysql));
    goto network_err;
  } else {
    mysql_mutex_lock(&mi->data_lock);
    mi->clock_diff_with_master = 0; /* The "most sensible" value */
    mysql_mutex_unlock(&mi->data_lock);
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_SECONDS_BEHIND_SOURCE_DUBIOUS,
           mysql_error(mysql), mysql_errno(mysql));
  }
  if (master_res) {
    mysql_free_result(master_res);
    master_res = nullptr;
  }

  /*
    Check that the master's server id and ours are different. Because if they
    are equal (which can result from a simple copy of master's datadir to slave,
    thus copying some my.cnf), replication will work but all events will be
    skipped.
    Do not die if SELECT @@SERVER_ID fails on master (very old master?).
    Note: we could have put a @@SERVER_ID in the previous SELECT
    UNIX_TIMESTAMP() instead, but this would not have worked on 3.23 masters.
  */
  DBUG_EXECUTE_IF("dbug.before_get_SERVER_ID",
                  { rpl_replica_debug_point(DBUG_RPL_S_BEFORE_SERVER_ID); };);
  master_res = nullptr;
  master_row = nullptr;
  DBUG_EXECUTE_IF("get_source_server_id.ER_NET_READ_INTERRUPTED", {
    DBUG_SET("+d,inject_ER_NET_READ_INTERRUPTED");
    DBUG_SET(
        "-d,get_source_server_id."
        "ER_NET_READ_INTERRUPTED");
  });
  if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT @@GLOBAL.SERVER_ID")) &&
      (master_res = mysql_store_result(mysql)) &&
      (master_row = mysql_fetch_row(master_res))) {
    if ((::server_id ==
         (mi->master_id = strtoul(master_row[0], nullptr, 10))) &&
        !mi->rli->replicate_same_server_id) {
      errmsg =
          "The replica I/O thread stops because source and replica have equal "
          "MySQL server ids; these ids must be different for replication to "
          "work (or the --replicate-same-server-id option must be used on "
          "replica but this does not always make sense; please check the "
          "manual before using it).";
      err_code = ER_REPLICA_FATAL_ERROR;
      sprintf(err_buff, ER_THD(current_thd, ER_REPLICA_FATAL_ERROR), errmsg);
      goto err;
    }
  } else if (mysql_errno(mysql) != ER_UNKNOWN_SYSTEM_VARIABLE) {
    if (check_io_slave_killed(mi->info_thd, mi, nullptr))
      goto slave_killed_err;
    else if (is_network_error(mysql_errno(mysql))) {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get source SERVER_ID failed with error: %s",
                 mysql_error(mysql));
      goto network_err;
    }
    /* Fatal error */
    errmsg =
        "The replica I/O thread stops because a fatal error is encountered "
        "when it try to get the value of SERVER_ID variable from source.";
    err_code = mysql_errno(mysql);
    sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
    goto err;
  } else {
    mi->report(WARNING_LEVEL, ER_SERVER_UNKNOWN_SYSTEM_VARIABLE,
               "Unknown system variable 'SERVER_ID' on source, maybe it "
               "is a *VERY OLD SOURCE*.");
  }
  if (master_res) {
    mysql_free_result(master_res);
    master_res = nullptr;
  }
  if (mi->master_id == 0 && mi->ignore_server_ids->dynamic_ids.size() > 0) {
    errmsg =
        "Replica configured with server id filtering could not detect the "
        "source "
        "server id.";
    err_code = ER_REPLICA_FATAL_ERROR;
    sprintf(err_buff, ER_THD(current_thd, ER_REPLICA_FATAL_ERROR), errmsg);
    goto err;
  }

  if (mi->heartbeat_period != 0.0) {
    char llbuf[22];
    const char query_format[] =
        "SET @master_heartbeat_period = %s, @source_heartbeat_period = %s";
    char query[sizeof(query_format) - 2 * 2 + 2 * sizeof(llbuf) + 1];
    /*
       the period is an ulonglong of nano-secs.
    */
    llstr((ulonglong)(mi->heartbeat_period * 1000000000UL), llbuf);
    sprintf(query, query_format, llbuf, llbuf);

    if (mysql_real_query(mysql, query, static_cast<ulong>(strlen(query)))) {
      if (check_io_slave_killed(mi->info_thd, mi, nullptr))
        goto slave_killed_err;

      if (is_network_error(mysql_errno(mysql))) {
        mi->report(
            WARNING_LEVEL, mysql_errno(mysql),
            "SET @source_heartbeat_period to source failed with error: %s",
            mysql_error(mysql));
        mysql_free_result(mysql_store_result(mysql));
        goto network_err;
      } else {
        /* Fatal error */
        errmsg =
            "The replica I/O thread stops because a fatal error is encountered "
            " when it tries to SET @source_heartbeat_period on source.";
        err_code = ER_REPLICA_FATAL_ERROR;
        sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
        mysql_free_result(mysql_store_result(mysql));
        goto err;
      }
    }
    mysql_free_result(mysql_store_result(mysql));
  }

  /*
    Querying if master is capable to checksum and notifying it about own
    CRC-awareness. The master's side instant value of @@global.binlog_checksum
    is stored in the dump thread's uservar area as well as cached locally
    to become known in consensus by master and slave.
  */
  if (DBUG_EVALUATE_IF("simulate_replica_unaware_checksum", 0, 1)) {
    int rc;
    // Set both variables, so that it works equally on both old and new
    // source server.
    const char query[] =
        "SET @master_binlog_checksum = @@global.binlog_checksum, "
        "@source_binlog_checksum = @@global.binlog_checksum";
    master_res = nullptr;
    // initially undefined
    mi->checksum_alg_before_fd =
        mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;
    /*
      @c checksum_alg_before_fd is queried from master in this block.
      If master is old checksum-unaware the value stays undefined.
      Once the first FD will be received its alg descriptor will replace
      the being queried one.
    */
    rc = mysql_real_query(mysql, query, static_cast<ulong>(strlen(query)));
    if (rc != 0) {
      mi->checksum_alg_before_fd =
          mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
      if (check_io_slave_killed(mi->info_thd, mi, nullptr))
        goto slave_killed_err;

      if (mysql_errno(mysql) == ER_UNKNOWN_SYSTEM_VARIABLE) {
        // this is tolerable as OM -> NS is supported
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "Notifying source by %s failed with "
                   "error: %s",
                   query, mysql_error(mysql));
      } else {
        if (is_network_error(mysql_errno(mysql))) {
          mi->report(WARNING_LEVEL, mysql_errno(mysql),
                     "Notifying source by %s failed with "
                     "error: %s",
                     query, mysql_error(mysql));
          mysql_free_result(mysql_store_result(mysql));
          goto network_err;
        } else {
          errmsg =
              "The replica I/O thread stops because a fatal error is "
              "encountered "
              "when it tried to SET @source_binlog_checksum on source.";
          err_code = ER_REPLICA_FATAL_ERROR;
          sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
          mysql_free_result(mysql_store_result(mysql));
          goto err;
        }
      }
    } else {
      mysql_free_result(mysql_store_result(mysql));
      // Read back the user variable that we just set, to verify that
      // the source recognized the checksum algorithm.
      if (!mysql_real_query(
              mysql, STRING_WITH_LEN("SELECT @source_binlog_checksum")) &&
          (master_res = mysql_store_result(mysql)) &&
          (master_row = mysql_fetch_row(master_res)) &&
          (master_row[0] != nullptr)) {
        mi->checksum_alg_before_fd = static_cast<enum_binlog_checksum_alg>(
            find_type(master_row[0], &binlog_checksum_typelib, 1) - 1);

        DBUG_EXECUTE_IF("undefined_algorithm_on_replica",
                        mi->checksum_alg_before_fd =
                            mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;);
        if (mi->checksum_alg_before_fd ==
            mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF) {
          errmsg =
              "The replica I/O thread was stopped because a fatal error is "
              "encountered "
              "The checksum algorithm used by source is unknown to replica.";
          err_code = ER_REPLICA_FATAL_ERROR;
          sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
          mysql_free_result(mysql_store_result(mysql));
          goto err;
        }

        // valid outcome is either of
        assert(mi->checksum_alg_before_fd ==
                   mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF ||
               mi->checksum_alg_before_fd ==
                   mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32);
      } else if (check_io_slave_killed(mi->info_thd, mi, nullptr))
        goto slave_killed_err;
      else if (is_network_error(mysql_errno(mysql))) {
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "Get source BINLOG_CHECKSUM failed with error: %s",
                   mysql_error(mysql));
        goto network_err;
      } else {
        errmsg =
            "The replica I/O thread stops because a fatal error is encountered "
            "when it tried to SELECT @source_binlog_checksum.";
        err_code = ER_REPLICA_FATAL_ERROR;
        sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
        mysql_free_result(mysql_store_result(mysql));
        goto err;
      }
    }
    if (master_res) {
      mysql_free_result(master_res);
      master_res = nullptr;
    }
  } else
    mi->checksum_alg_before_fd = mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;

  if (DBUG_EVALUATE_IF("bug32442749_simulate_null_checksum", 1, 0)) {
    const char query[] = "SET @source_binlog_checksum= NULL";
    int rc = mysql_real_query(mysql, query, static_cast<ulong>(strlen(query)));
    if (rc != 0) {
      errmsg =
          "The replica I/O thread stops because a fatal error is encountered "
          "when it tried to SET @source_binlog_checksum.";
      err_code = ER_REPLICA_FATAL_ERROR;
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      mysql_free_result(mysql_store_result(mysql));
      goto err;
    }
    mysql_free_result(mysql_store_result(mysql));
  }

  if (DBUG_EVALUATE_IF("simulate_replica_unaware_gtid", 0, 1)) {
    auto master_gtid_mode = Gtid_mode::OFF;
    auto slave_gtid_mode = global_gtid_mode.get();
    switch (io_thread_init_command(mi, "SELECT @@GLOBAL.GTID_MODE",
                                   ER_UNKNOWN_SYSTEM_VARIABLE, &master_res,
                                   &master_row)) {
      case COMMAND_STATUS_ERROR:
        return 2;
      case COMMAND_STATUS_ALLOWED_ERROR:
        // master is old and does not have @@GLOBAL.GTID_MODE
        master_gtid_mode = Gtid_mode::OFF;
        break;
      case COMMAND_STATUS_OK: {
        const char *master_gtid_mode_string = master_row[0];
        DBUG_EXECUTE_IF("simulate_source_has_gtid_mode_on_something",
                        { master_gtid_mode_string = "on_something"; });
        DBUG_EXECUTE_IF("simulate_source_has_gtid_mode_off_something",
                        { master_gtid_mode_string = "off_something"; });
        DBUG_EXECUTE_IF("simulate_source_has_unknown_gtid_mode",
                        { master_gtid_mode_string = "Krakel Spektakel"; });
        bool error;
        std::tie(error, master_gtid_mode) =
            Gtid_mode::from_string(master_gtid_mode_string);
        if (error) {
          mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                     "The replica IO thread stops because the source has "
                     "an unknown @@GLOBAL.GTID_MODE '%s'.",
                     master_gtid_mode_string);
          mysql_free_result(master_res);
          return 1;
        }
        mysql_free_result(master_res);
        break;
      }
    }
    if ((slave_gtid_mode == Gtid_mode::OFF &&
         master_gtid_mode >= Gtid_mode::ON_PERMISSIVE) ||
        (slave_gtid_mode == Gtid_mode::ON &&
         master_gtid_mode <= Gtid_mode::OFF_PERMISSIVE &&
         mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() ==
             Assign_gtids_to_anonymous_transactions_info::enum_type::
                 AGAT_OFF)) {
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 "The replication receiver thread cannot start because "
                 "the source has GTID_MODE = %.192s and this server has "
                 "GTID_MODE = %.192s.",
                 Gtid_mode::to_string(master_gtid_mode),
                 Gtid_mode::to_string(slave_gtid_mode));
      return 1;
    }
    if (mi->is_auto_position() && master_gtid_mode != Gtid_mode::ON) {
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 "The replication receiver thread cannot start in "
                 "AUTO_POSITION mode: the source has GTID_MODE = %.192s "
                 "instead of ON.",
                 Gtid_mode::to_string(master_gtid_mode));
      return 1;
    }
  }

err:
  if (errmsg) {
    if (master_res) mysql_free_result(master_res);
    assert(err_code != 0);
    mi->report(ERROR_LEVEL, err_code, "%s", err_buff);
    return 1;
  }

  return 0;

network_err:
  if (master_res) mysql_free_result(master_res);
  mi->set_network_error();
  return 2;

slave_killed_err:
  if (master_res) mysql_free_result(master_res);
  return 2;
}

static bool exceeds_relay_log_limit(Relay_log_info *rli,
                                    std::size_t queued_size) {
  return (rli->log_space_limit != 0 &&
          rli->log_space_limit < rli->log_space_total + queued_size);
}

static bool wait_for_relay_log_space(Relay_log_info *rli,
                                     std::size_t queued_size) {
  bool slave_killed = false;
  Master_info *mi = rli->mi;
  PSI_stage_info old_stage;
  THD *thd = mi->info_thd;
  DBUG_TRACE;

  // from now on, until the time is_receiver_waiting_for_rl_space is
  // cleared, every rotation made by coordinator and executed
  // outside of a transaction, will purge the currently rotated log
  rli->is_receiver_waiting_for_rl_space.store(true);

  // rotate now to avoid deadlock with FLUSH RELAY LOGS, which calls
  // rotate_relay_log with a default locking order, see rotate_relay_log
  // Before rotation, is_receiver_waiting_for_rl_space is already set, so
  // after exiting the rotate_relay_log, coordinator executing rotation
  // requested here will see the correct value of the
  // 'is_receiver_waiting_for_rl_space' and will purge applied logs
  // with force option
  rotate_relay_log(mi, true, true, true);

  // capture the log name to which we rotated:
  mysql_mutex_lock(rli->relay_log.get_log_lock());
  std::string receiver_log = rli->relay_log.get_log_fname();
  mysql_mutex_unlock(rli->relay_log.get_log_lock());

  mysql_mutex_lock(&rli->log_space_lock);
  thd->ENTER_COND(&rli->log_space_cond, &rli->log_space_lock,
                  &stage_waiting_for_relay_log_space, &old_stage);
  while (exceeds_relay_log_limit(rli, queued_size) &&
         !(slave_killed = io_slave_killed(thd, mi)) &&
         rli->coordinator_log_after_purge != receiver_log) {
    mysql_cond_wait(&rli->log_space_cond, &rli->log_space_lock);
  }
  mysql_mutex_unlock(&rli->log_space_lock);
  thd->EXIT_COND(&old_stage);

  rli->is_receiver_waiting_for_rl_space.store(false);

  return slave_killed;
}

/*
  Builds a Rotate and writes it to relay log.

  The caller must hold mi->data_lock.

  @param thd pointer to I/O Thread's Thd.
  @param mi  point to I/O Thread metadata class.

  @param force_flush_mi_info when true, do not respect sync period and flush
                             information.
                             when false, flush will only happen if it is time to
                             flush.

  @return 0 if everything went fine, 1 otherwise.
*/
static int write_rotate_to_master_pos_into_relay_log(THD *thd, Master_info *mi,
                                                     bool force_flush_mi_info) {
  Relay_log_info *rli = mi->rli;
  int error = 0;
  DBUG_TRACE;

  assert(thd == mi->info_thd);
  mysql_mutex_assert_owner(rli->relay_log.get_log_lock());

  DBUG_PRINT("info", ("writing a Rotate event to the relay log"));
  Rotate_log_event *ev = new Rotate_log_event(mi->get_master_log_name(), 0,
                                              mi->get_master_log_pos(),
                                              Rotate_log_event::DUP_NAME);

  DBUG_EXECUTE_IF("fail_generating_rotate_event_on_write_rotate_to_source_pos",
                  {
                    if (likely((bool)ev)) {
                      delete ev;
                      ev = nullptr;
                    }
                  });

  if (likely((bool)ev)) {
    if (mi->get_mi_description_event() != nullptr)
      ev->common_footer->checksum_alg =
          mi->get_mi_description_event()->common_footer->checksum_alg;

    ev->server_id = 0;  // don't be ignored by slave SQL thread
    if (unlikely(rli->relay_log.write_event(ev, mi) != 0))
      mi->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_WRITE_FAILURE,
                 ER_THD(thd, ER_REPLICA_RELAY_LOG_WRITE_FAILURE),
                 "failed to write a Rotate event"
                 " to the relay log, SHOW REPLICA STATUS may be"
                 " inaccurate");
    mysql_mutex_lock(&mi->data_lock);
    if (flush_master_info(mi, force_flush_mi_info, false, false,
                          mi->is_gtid_only_mode())) {
      error = 1;
      LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_FLUSH_CONNECTION_METADATA_REPOS);
    }
    mysql_mutex_unlock(&mi->data_lock);
    delete ev;
  } else {
    error = 1;
    mi->report(ERROR_LEVEL, ER_REPLICA_CREATE_EVENT_FAILURE,
               ER_THD(thd, ER_REPLICA_CREATE_EVENT_FAILURE),
               "Rotate_event (out of memory?),"
               " SHOW REPLICA STATUS may be inaccurate");
  }

  return error;
}

/*
  Builds a Rotate from the ignored events' info and writes it to relay log.

  @param thd pointer to I/O Thread's Thd.
  @param mi  point to I/O Thread metadata class.

  @return 0 if everything went fine, 1 otherwise.
*/
static int write_ignored_events_info_to_relay_log(THD *thd, Master_info *mi) {
  Relay_log_info *rli = mi->rli;
  mysql_mutex_t *end_pos_lock = rli->relay_log.get_binlog_end_pos_lock();
  int error = 0;
  DBUG_TRACE;

  assert(thd == mi->info_thd);
  mysql_mutex_lock(rli->relay_log.get_log_lock());
  mysql_mutex_lock(end_pos_lock);

  if (rli->ign_master_log_name_end[0]) {
    DBUG_PRINT("info", ("writing a Rotate event to track down ignored events"));
    /*
      If the ignored events' info still hold, they should have same info as
      the mi->get_master_log_[name|pos].
    */
    assert(strcmp(rli->ign_master_log_name_end, mi->get_master_log_name()) ==
           0);
    assert(rli->ign_master_log_pos_end == mi->get_master_log_pos());

    /* Avoid the applier to get the ignored event' info by rli->ign* */
    rli->ign_master_log_name_end[0] = 0;
    /* can unlock before writing as the relay log will soon have our Rotate */
    mysql_mutex_unlock(end_pos_lock);

    /* Generate the rotate based on mi position */
    error = write_rotate_to_master_pos_into_relay_log(
        thd, mi, false /* force_flush_mi_info */);
  } else
    mysql_mutex_unlock(end_pos_lock);

  mysql_mutex_unlock(rli->relay_log.get_log_lock());
  return error;
}

static int register_slave_on_master(MYSQL *mysql, Master_info *mi,
                                    bool *suppress_warnings) {
  uchar buf[1024], *pos = buf;
  size_t report_host_len = 0, report_user_len = 0, report_password_len = 0;
  DBUG_TRACE;

  *suppress_warnings = false;
  if (report_host) report_host_len = strlen(report_host);
  if (report_host_len > HOSTNAME_LENGTH) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_REPORT_HOST_TOO_LONG, report_host_len,
           HOSTNAME_LENGTH, mi->get_for_channel_str());
    return 0;
  }

  if (report_user) report_user_len = strlen(report_user);
  if (report_user_len > USERNAME_LENGTH) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_REPORT_USER_TOO_LONG, report_user_len,
           USERNAME_LENGTH, mi->get_for_channel_str());
    return 0;
  }

  if (report_password) report_password_len = strlen(report_password);
  if (report_password_len > MAX_PASSWORD_LENGTH) {
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_REPORT_PASSWORD_TOO_LONG,
           report_password_len, MAX_PASSWORD_LENGTH, mi->get_for_channel_str());
    return 0;
  }

  int4store(pos, server_id);
  pos += 4;
  pos = net_store_data(pos, (uchar *)report_host, report_host_len);
  pos = net_store_data(pos, (uchar *)report_user, report_user_len);
  pos = net_store_data(pos, (uchar *)report_password, report_password_len);
  int2store(pos, (uint16)report_port);
  pos += 2;
  /*
    Fake rpl_recovery_rank, which was removed in BUG#13963,
    so that this server can register itself on old servers,
    see BUG#49259.
   */
  int4store(pos, /* rpl_recovery_rank */ 0);
  pos += 4;
  /* The master will fill in master_id */
  int4store(pos, 0);
  pos += 4;

  if (simple_command(mysql, COM_REGISTER_SLAVE, buf, (size_t)(pos - buf), 0)) {
    uint err{mysql_errno(mysql)};
    if (err == ER_NET_READ_INTERRUPTED) {
      *suppress_warnings = true;  // Suppress reconnect warning
    } else if (!check_io_slave_killed(mi->info_thd, mi, nullptr)) {
      std::stringstream ss;
      ss << mysql_error(mysql) << " (Errno: " << err << ")";
      mi->report(ERROR_LEVEL, ER_REPLICA_SOURCE_COM_FAILURE,
                 ER_THD(current_thd, ER_REPLICA_SOURCE_COM_FAILURE),
                 "COM_REGISTER_REPLICA", ss.str().c_str());
    }
    if (is_network_error(err)) mi->set_network_error();
    return 1;
  }

  DBUG_EXECUTE_IF("simulate_register_replica_killed", {
    mi->abort_slave = 1;
    return 1;
  };);
  return 0;
}

/**
    Function that fills the metadata required for SHOW REPLICA STATUS.
    This function shall be used in two cases:
     1) SHOW REPLICA STATUS FOR ALL CHANNELS
     2) SHOW REPLICA STATUS for a channel

     @param[in,out]  field_list        field_list to fill the metadata
     @param[in]      io_gtid_set_size  the size to be allocated to store
                                       the retrieved gtid set
     @param[in]      sql_gtid_set_size the size to be allocated to store
                                       the executed gtid set

     @todo  return a bool after adding catching the exceptions to the
            push_back() methods for field_list.
*/

static void show_slave_status_metadata(mem_root_deque<Item *> *field_list,
                                       int io_gtid_set_size,
                                       int sql_gtid_set_size) {
  field_list->push_back(new Item_empty_string("Replica_IO_State", 14));
  field_list->push_back(
      new Item_empty_string("Source_Host", HOSTNAME_LENGTH + 1));
  field_list->push_back(
      new Item_empty_string("Source_User", USERNAME_LENGTH + 1));
  field_list->push_back(new Item_return_int("Source_Port", 7, MYSQL_TYPE_LONG));
  field_list->push_back(
      new Item_return_int("Connect_Retry", 10, MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Source_Log_File", FN_REFLEN));
  field_list->push_back(
      new Item_return_int("Read_Source_Log_Pos", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Relay_Log_File", FN_REFLEN));
  field_list->push_back(
      new Item_return_int("Relay_Log_Pos", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(
      new Item_empty_string("Relay_Source_Log_File", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Replica_IO_Running", 3));
  field_list->push_back(new Item_empty_string("Replica_SQL_Running", 3));
  field_list->push_back(new Item_empty_string("Replicate_Do_DB", 20));
  field_list->push_back(new Item_empty_string("Replicate_Ignore_DB", 20));
  field_list->push_back(new Item_empty_string("Replicate_Do_Table", 20));
  field_list->push_back(new Item_empty_string("Replicate_Ignore_Table", 23));
  field_list->push_back(new Item_empty_string("Replicate_Wild_Do_Table", 24));
  field_list->push_back(
      new Item_empty_string("Replicate_Wild_Ignore_Table", 28));
  field_list->push_back(new Item_return_int("Last_Errno", 4, MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Last_Error", 20));
  field_list->push_back(
      new Item_return_int("Skip_Counter", 10, MYSQL_TYPE_LONG));
  field_list->push_back(
      new Item_return_int("Exec_Source_Log_Pos", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(
      new Item_return_int("Relay_Log_Space", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Until_Condition", 6));
  field_list->push_back(new Item_empty_string("Until_Log_File", FN_REFLEN));
  field_list->push_back(
      new Item_return_int("Until_Log_Pos", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Source_SSL_Allowed", 7));
  field_list->push_back(new Item_empty_string("Source_SSL_CA_File", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Source_SSL_CA_Path", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Source_SSL_Cert", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Source_SSL_Cipher", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Source_SSL_Key", FN_REFLEN));
  field_list->push_back(
      new Item_return_int("Seconds_Behind_Source", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(
      new Item_empty_string("Source_SSL_Verify_Server_Cert", 3));
  field_list->push_back(
      new Item_return_int("Last_IO_Errno", 4, MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Last_IO_Error", 20));
  field_list->push_back(
      new Item_return_int("Last_SQL_Errno", 4, MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Last_SQL_Error", 20));
  field_list->push_back(
      new Item_empty_string("Replicate_Ignore_Server_Ids", FN_REFLEN));
  field_list->push_back(
      new Item_return_int("Source_Server_Id", sizeof(ulong), MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Source_UUID", UUID_LENGTH));
  field_list->push_back(
      new Item_empty_string("Source_Info_File", 2 * FN_REFLEN));
  field_list->push_back(new Item_return_int("SQL_Delay", 10, MYSQL_TYPE_LONG));
  field_list->push_back(
      new Item_return_int("SQL_Remaining_Delay", 8, MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Replica_SQL_Running_State", 20));
  field_list->push_back(
      new Item_return_int("Source_Retry_Count", 10, MYSQL_TYPE_LONGLONG));
  field_list->push_back(
      new Item_empty_string("Source_Bind", HOSTNAME_LENGTH + 1));
  field_list->push_back(new Item_empty_string("Last_IO_Error_Timestamp", 20));
  field_list->push_back(new Item_empty_string("Last_SQL_Error_Timestamp", 20));
  field_list->push_back(new Item_empty_string("Source_SSL_Crl", FN_REFLEN));
  field_list->push_back(new Item_empty_string("Source_SSL_Crlpath", FN_REFLEN));
  field_list->push_back(
      new Item_empty_string("Retrieved_Gtid_Set", io_gtid_set_size));
  field_list->push_back(
      new Item_empty_string("Executed_Gtid_Set", sql_gtid_set_size));
  field_list->push_back(
      new Item_return_int("Auto_Position", sizeof(ulong), MYSQL_TYPE_LONG));
  field_list->push_back(new Item_empty_string("Replicate_Rewrite_DB", 24));
  field_list->push_back(
      new Item_empty_string("Channel_Name", CHANNEL_NAME_LENGTH));
  field_list->push_back(new Item_empty_string("Source_TLS_Version", FN_REFLEN));
  field_list->push_back(
      new Item_empty_string("Source_public_key_path", FN_REFLEN));
  field_list->push_back(new Item_return_int("Get_Source_public_key",
                                            sizeof(ulong), MYSQL_TYPE_LONG));
  field_list->push_back(
      new Item_empty_string("Network_Namespace", NAME_LEN + 1));
}

/**
    Send the data to the client of a Master_info during show_slave_status()
    This function has to be called after calling show_slave_status_metadata().
    Just before sending the data, thd->get_protocol() is prepared to (re)send;

    @param[in]     thd         client thread
    @param[in]     mi          the master info. In the case of multisource
                               replication, this master info corresponds to a
                                channel.

    @param[in]     io_gtid_set_buffer    buffer related to Retrieved GTID set
                                          for each channel.
    @param[in]     sql_gtid_set_buffer   buffer related to Executed GTID set
                                           for each channel.
    @retval        0     success
    @retval        1     Error
*/

static bool show_slave_status_send_data(THD *thd, Master_info *mi,
                                        char *io_gtid_set_buffer,
                                        char *sql_gtid_set_buffer) {
  DBUG_TRACE;

  Protocol *protocol = thd->get_protocol();
  char *slave_sql_running_state = nullptr;
  Rpl_filter *rpl_filter = mi->rli->rpl_filter;

  DBUG_PRINT("info", ("host is set: '%s'", mi->host));

  protocol->start_row();

  /*
    slave_running can be accessed without run_lock but not other
    non-volatile members like mi->info_thd or rli->info_thd, for
    them either info_thd_lock or run_lock hold is required.
  */
  mysql_mutex_lock(&mi->info_thd_lock);
  protocol->store(mi->info_thd ? mi->info_thd->proc_info_session(thd) : "",
                  &my_charset_bin);
  mysql_mutex_unlock(&mi->info_thd_lock);

  mysql_mutex_lock(&mi->rli->info_thd_lock);
  slave_sql_running_state = const_cast<char *>(
      mi->rli->info_thd ? mi->rli->info_thd->proc_info_session(thd) : "");
  mysql_mutex_unlock(&mi->rli->info_thd_lock);

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  mysql_mutex_lock(&mi->err_lock);
  mysql_mutex_lock(&mi->rli->err_lock);

  DEBUG_SYNC(thd, "wait_after_lock_active_mi_and_rli_data_lock_is_acquired");
  protocol->store(mi->host, &my_charset_bin);
  protocol->store(mi->get_user(), &my_charset_bin);
  protocol->store((uint32)mi->port);
  protocol->store((uint32)mi->connect_retry);
  protocol->store(mi->get_master_log_name_info(), &my_charset_bin);
  protocol->store((ulonglong)mi->get_master_log_pos_info());
  protocol->store(mi->rli->get_group_relay_log_name() +
                      dirname_length(mi->rli->get_group_relay_log_name()),
                  &my_charset_bin);
  protocol->store((ulonglong)mi->rli->get_group_relay_log_pos());
  protocol->store(mi->rli->get_group_master_log_name_info(), &my_charset_bin);
  protocol->store(
      mi->slave_running == MYSQL_SLAVE_RUN_CONNECT
          ? "Yes"
          : (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT ? "Connecting"
                                                              : "No"),
      &my_charset_bin);
  protocol->store(mi->rli->slave_running ? "Yes" : "No", &my_charset_bin);

  /*
    Acquire the read lock, because the filter may be modified by
    CHANGE REPLICATION FILTER when slave is not running.
  */
  rpl_filter->rdlock();
  store(protocol, rpl_filter->get_do_db());
  store(protocol, rpl_filter->get_ignore_db());

  char buf[256];
  String tmp(buf, sizeof(buf), &my_charset_bin);
  rpl_filter->get_do_table(&tmp);
  protocol->store(&tmp);
  rpl_filter->get_ignore_table(&tmp);
  protocol->store(&tmp);
  rpl_filter->get_wild_do_table(&tmp);
  protocol->store(&tmp);
  rpl_filter->get_wild_ignore_table(&tmp);
  protocol->store(&tmp);

  protocol->store(mi->rli->last_error().number);
  protocol->store(mi->rli->last_error().message, &my_charset_bin);
  protocol->store((uint32)mi->rli->slave_skip_counter);
  protocol->store((ulonglong)mi->rli->get_group_master_log_pos_info());
  protocol->store((ulonglong)mi->rli->log_space_total);

  const char *until_type = "";

  switch (mi->rli->until_condition) {
    case Relay_log_info::UNTIL_NONE:
      until_type = "None";
      break;
    case Relay_log_info::UNTIL_MASTER_POS:
      until_type = "Source";
      break;
    case Relay_log_info::UNTIL_RELAY_POS:
      until_type = "Relay";
      break;
    case Relay_log_info::UNTIL_SQL_BEFORE_GTIDS:
      until_type = "SQL_BEFORE_GTIDS";
      break;
    case Relay_log_info::UNTIL_SQL_AFTER_GTIDS:
      until_type = "SQL_AFTER_GTIDS";
      break;
    case Relay_log_info::UNTIL_SQL_VIEW_ID:
      until_type = "SQL_VIEW_ID";
      break;
    case Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS:
      until_type = "SQL_AFTER_MTS_GAPS";
      break;
    case Relay_log_info::UNTIL_DONE:
      until_type = "DONE";
      break;
    default:
      assert(0);
  }
  protocol->store(until_type, &my_charset_bin);
  protocol->store(mi->rli->get_until_log_name(), &my_charset_bin);
  protocol->store((ulonglong)mi->rli->get_until_log_pos());

  protocol->store(mi->ssl ? "Yes" : "No", &my_charset_bin);
  protocol->store(mi->ssl_ca, &my_charset_bin);
  protocol->store(mi->ssl_capath, &my_charset_bin);
  protocol->store(mi->ssl_cert, &my_charset_bin);
  protocol->store(mi->ssl_cipher, &my_charset_bin);
  protocol->store(mi->ssl_key, &my_charset_bin);

  /*
     The pseudo code to compute Seconds_Behind_Source:
     if (SQL thread is running)
     {
       if (SQL thread processed all the available relay log)
       {
         if (IO thread is running)
            print 0;
         else
            print NULL;
       }
        else
          compute Seconds_Behind_Source;
      }
      else
       print NULL;
  */

  if (mi->rli->slave_running) {
    /*
       Check if SQL thread is at the end of relay log
       Checking should be done using two conditions
       condition1: compare the log positions and
       condition2: compare the file names (to handle rotation case)
    */
    if ((mi->get_master_log_pos() == mi->rli->get_group_master_log_pos()) &&
        (!strcmp(mi->get_master_log_name(),
                 mi->rli->get_group_master_log_name()))) {
      if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
        protocol->store(0LL);
      else
        protocol->store_null();
    } else {
      long time_diff = ((long)(time(nullptr) - mi->rli->last_master_timestamp) -
                        mi->clock_diff_with_master);
      /*
        Apparently on some systems time_diff can be <0. Here are possible
        reasons related to MySQL:
        - the master is itself a slave of another master whose time is ahead.
        - somebody used an explicit SET TIMESTAMP on the master.
        Possible reason related to granularity-to-second of time functions
        (nothing to do with MySQL), which can explain a value of -1:
        assume the master's and slave's time are perfectly synchronized, and
        that at slave's connection time, when the master's timestamp is read,
        it is at the very end of second 1, and (a very short time later) when
        the slave's timestamp is read it is at the very beginning of second
        2. Then the recorded value for master is 1 and the recorded value for
        slave is 2. At SHOW REPLICA STATUS time, assume that the difference
        between timestamp of slave and rli->last_master_timestamp is 0
        (i.e. they are in the same second), then we get 0-(2-1)=-1 as a result.
        This confuses users, so we don't go below 0: hence the max().

        last_master_timestamp == 0 (an "impossible" timestamp 1970) is a
        special marker to say "consider we have caught up".
      */
      protocol->store(
          (longlong)(mi->rli->last_master_timestamp ? max(0L, time_diff) : 0));
    }
  } else {
    protocol->store_null();
  }
  protocol->store(mi->ssl_verify_server_cert ? "Yes" : "No", &my_charset_bin);

  // Last_IO_Errno
  protocol->store(mi->last_error().number);
  // Last_IO_Error
  protocol->store(mi->last_error().message, &my_charset_bin);
  // Last_SQL_Errno
  protocol->store(mi->rli->last_error().number);
  // Last_SQL_Error
  protocol->store(mi->rli->last_error().message, &my_charset_bin);
  // Replicate_Ignore_Server_Ids
  {
    char buff[FN_REFLEN];
    ulong i, cur_len;
    for (i = 0, buff[0] = 0, cur_len = 0;
         i < mi->ignore_server_ids->dynamic_ids.size(); i++) {
      ulong s_id, slen;
      char sbuff[FN_REFLEN];
      s_id = mi->ignore_server_ids->dynamic_ids[i];
      slen = sprintf(sbuff, (i == 0 ? "%lu" : ", %lu"), s_id);
      if (cur_len + slen + 4 > FN_REFLEN) {
        /*
          break the loop whenever remained space could not fit
          ellipses on the next cycle
        */
        sprintf(buff + cur_len, "...");
        break;
      }
      cur_len += sprintf(buff + cur_len, "%s", sbuff);
    }
    protocol->store(buff, &my_charset_bin);
  }
  // Source_Server_id
  protocol->store((uint32)mi->master_id);
  protocol->store(mi->master_uuid, &my_charset_bin);
  // Master_info_file
  protocol->store(mi->get_description_info(), &my_charset_bin);
  // SQL_Delay
  protocol->store((uint32)mi->rli->get_sql_delay());
  // SQL_Remaining_Delay
  if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name) {
    time_t t = time(nullptr), sql_delay_end = mi->rli->get_sql_delay_end();
    protocol->store((uint32)(t < sql_delay_end ? sql_delay_end - t : 0));
  } else
    protocol->store_null();
  // Replica_SQL_Running_State
  protocol->store(slave_sql_running_state, &my_charset_bin);
  // Source_Retry_Count
  protocol->store((ulonglong)mi->retry_count);
  // Source_Bind
  protocol->store(mi->bind_addr, &my_charset_bin);
  // Last_IO_Error_Timestamp
  protocol->store(mi->last_error().timestamp, &my_charset_bin);
  // Last_SQL_Error_Timestamp
  protocol->store(mi->rli->last_error().timestamp, &my_charset_bin);
  // Source_Ssl_Crl
  protocol->store(mi->ssl_crl, &my_charset_bin);
  // Source_Ssl_Crlpath
  protocol->store(mi->ssl_crlpath, &my_charset_bin);
  // Retrieved_Gtid_Set
  protocol->store(io_gtid_set_buffer, &my_charset_bin);
  // Executed_Gtid_Set
  protocol->store(sql_gtid_set_buffer, &my_charset_bin);
  // Auto_Position
  protocol->store(mi->is_auto_position() ? 1 : 0);
  // Replicate_Rewrite_DB
  rpl_filter->get_rewrite_db(&tmp);
  protocol->store(&tmp);
  // channel_name
  protocol->store(mi->get_channel(), &my_charset_bin);
  // Source_TLS_Version
  protocol->store(mi->tls_version, &my_charset_bin);
  // Source_public_key_path
  protocol->store(mi->public_key_path, &my_charset_bin);
  // Get_Source_public_key
  protocol->store(mi->get_public_key ? 1 : 0);

  protocol->store(mi->network_namespace_str(), &my_charset_bin);

  rpl_filter->unlock();
  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  return false;
}

/**
   Method to the show the replication status in all channels.

   @param[in]       thd        the client thread

   @retval          0           success
   @retval          1           Error

*/
bool show_slave_status(THD *thd) {
  Protocol *protocol = thd->get_protocol();
  int sql_gtid_set_size = 0, io_gtid_set_size = 0;
  Master_info *mi = nullptr;
  char *sql_gtid_set_buffer = nullptr;
  char **io_gtid_set_buffer_array;
  /*
    We need the maximum size of the retrieved gtid set (i.e io_gtid_set_size).
    This size is needed to reserve the place in show_slave_status_metadata().
    So, we travel all the mi's and find out the maximum size of io_gtid_set_size
    and pass it through show_slave_status_metadata()
  */
  int max_io_gtid_set_size = io_gtid_set_size;
  uint idx;
  uint num_io_gtid_sets;
  bool ret = true;

  DBUG_TRACE;

  channel_map.assert_some_lock();

  num_io_gtid_sets = channel_map.get_num_instances();

  io_gtid_set_buffer_array =
      (char **)my_malloc(key_memory_show_replica_status_io_gtid_set,
                         num_io_gtid_sets * sizeof(char *), MYF(MY_WME));

  if (io_gtid_set_buffer_array == nullptr) return true;

  global_tsid_lock->wrlock();

  const Gtid_set *sql_gtid_set = gtid_state->get_executed_gtids();
  sql_gtid_set_size = sql_gtid_set->to_string(&sql_gtid_set_buffer);

  global_tsid_lock->unlock();

  idx = 0;
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;
    /*
      The following statement is needed because, when mi->host[0]=0
      we don't alloc memory for retried_gtid_set. However, we try
      to free it at the end, causing a crash. To be on safeside,
      we initialize it to NULL, so that my_free() takes care of it.
    */
    io_gtid_set_buffer_array[idx] = nullptr;

    if (Master_info::is_configured(mi)) {
      const Gtid_set *io_gtid_set = mi->rli->get_gtid_set();
      mi->rli->get_tsid_lock()->wrlock();

      /*
         @todo: a single memory allocation improves speed,
         instead of doing it for each loop
      */

      if ((io_gtid_set_size =
               io_gtid_set->to_string(&io_gtid_set_buffer_array[idx])) < 0) {
        my_eof(thd);
        my_free(sql_gtid_set_buffer);

        for (uint i = 0; i < idx - 1; i++) {
          my_free(io_gtid_set_buffer_array[i]);
        }
        my_free(io_gtid_set_buffer_array);

        mi->rli->get_tsid_lock()->unlock();
        return true;
      } else
        max_io_gtid_set_size = max_io_gtid_set_size > io_gtid_set_size
                                   ? max_io_gtid_set_size
                                   : io_gtid_set_size;

      mi->rli->get_tsid_lock()->unlock();
    }
    idx++;
  }

  mem_root_deque<Item *> field_list(thd->mem_root);
  show_slave_status_metadata(&field_list, max_io_gtid_set_size,
                             sql_gtid_set_size);

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    goto err;
  }

  /* Run through each mi */

  idx = 0;
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;

    if (Master_info::is_configured(mi)) {
      if (show_slave_status_send_data(thd, mi, io_gtid_set_buffer_array[idx],
                                      sql_gtid_set_buffer))
        goto err;

      if (protocol->end_row()) goto err;
    }
    idx++;
  }

  ret = false;
err:
  my_eof(thd);
  for (uint i = 0; i < num_io_gtid_sets; i++) {
    my_free(io_gtid_set_buffer_array[i]);
  }
  my_free(io_gtid_set_buffer_array);
  my_free(sql_gtid_set_buffer);

  return ret;
}

/**
  Execute a SHOW REPLICA STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the IO thread.

  @retval false success
  @retval true failure

  Currently, show replica status works for a channel too, in multisource
  replication. But using performance schema tables is better.

*/
bool show_slave_status(THD *thd, Master_info *mi) {
  Protocol *protocol = thd->get_protocol();
  char *sql_gtid_set_buffer = nullptr, *io_gtid_set_buffer = nullptr;
  int sql_gtid_set_size = 0, io_gtid_set_size = 0;
  DBUG_TRACE;

  if (mi != nullptr) {
    global_tsid_lock->wrlock();
    const Gtid_set *sql_gtid_set = gtid_state->get_executed_gtids();
    sql_gtid_set_size = sql_gtid_set->to_string(&sql_gtid_set_buffer);
    global_tsid_lock->unlock();

    mi->rli->get_tsid_lock()->wrlock();
    const Gtid_set *io_gtid_set = mi->rli->get_gtid_set();
    io_gtid_set_size = io_gtid_set->to_string(&io_gtid_set_buffer);
    mi->rli->get_tsid_lock()->unlock();

    if (sql_gtid_set_size < 0 || io_gtid_set_size < 0) {
      my_eof(thd);
      my_free(sql_gtid_set_buffer);
      my_free(io_gtid_set_buffer);
      return true;
    }
  }

  /* Fill the metadata required for show replica status. */

  mem_root_deque<Item *> field_list(thd->mem_root);
  show_slave_status_metadata(&field_list, io_gtid_set_size, sql_gtid_set_size);

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    my_free(sql_gtid_set_buffer);
    my_free(io_gtid_set_buffer);
    return true;
  }

  if (Master_info::is_configured(mi)) {
    if (show_slave_status_send_data(thd, mi, io_gtid_set_buffer,
                                    sql_gtid_set_buffer))
      return true;

    if (protocol->end_row()) {
      my_free(sql_gtid_set_buffer);
      my_free(io_gtid_set_buffer);
      return true;
    }
  }
  my_eof(thd);
  my_free(sql_gtid_set_buffer);
  my_free(io_gtid_set_buffer);
  return false;
}

/**
  Entry point for SHOW REPLICA STATUS command. Function displays
  the slave status for all channels or for a single channel
  based on the FOR CHANNEL  clause.

  @param[in]       thd          the client thread.

  @retval          false        ok
  @retval          true         not ok
*/
bool show_slave_status_cmd(THD *thd) {
  Master_info *mi = nullptr;
  LEX *lex = thd->lex;
  bool res;

  DBUG_TRACE;

  channel_map.rdlock();

  if (!lex->mi.for_channel)
    res = show_slave_status(thd);
  else {
    mi = channel_map.get_mi(lex->mi.channel);

    /*
      When mi is NULL, that means the channel doesn't exist, SSS
      will throw an error.
    */
    if (mi == nullptr) {
      my_error(ER_REPLICA_CHANNEL_DOES_NOT_EXIST, MYF(0), lex->mi.channel);
      channel_map.unlock();
      return true;
    }

    /*
      If the channel being used is a group replication applier channel we
      need to disable the SHOW REPLICA STATUS command as its output is not
      compatible with this command.
    */
    if (channel_map.is_group_replication_applier_channel_name(
            mi->get_channel())) {
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
               "SHOW REPLICA STATUS", mi->get_channel());
      channel_map.unlock();
      return true;
    }

    res = show_slave_status(thd, mi);
  }

  channel_map.unlock();

  return res;
}

void set_slave_thread_options(THD *thd) {
  DBUG_TRACE;
  /*
     It's nonsense to constrain the slave threads with max_join_size; if a
     query succeeded on master, we HAVE to execute it. So set
     OPTION_BIG_SELECTS. Setting max_join_size to HA_POS_ERROR is not enough
     (and it's not needed if we have OPTION_BIG_SELECTS) because an INSERT
     SELECT examining more than 4 billion rows would still fail (yes, because
     when max_join_size is 4G, OPTION_BIG_SELECTS is automatically set, but
     only for client threads.
  */
  ulonglong options = thd->variables.option_bits | OPTION_BIG_SELECTS;
  if (opt_log_replica_updates)
    options |= OPTION_BIN_LOG;
  else
    options &= ~OPTION_BIN_LOG;
  thd->variables.option_bits = options;
  thd->variables.completion_type = 0;

  /* Do not track GTIDs for slave threads to avoid performance issues. */
  thd->variables.session_track_gtids = SESSION_TRACK_GTIDS_OFF;
  thd->rpl_thd_ctx.session_gtids_ctx()
      .update_tracking_activeness_from_session_variable(thd);

  /*
    Set autocommit= 1 when info tables are used and autocommit == 0 to
    avoid trigger asserts on mysql_execute_command(THD *thd) caused by
    info tables updates which do not commit, like Rotate, Stop and
    skipped events handling.
  */
  if ((thd->variables.option_bits & OPTION_NOT_AUTOCOMMIT)) {
    thd->variables.option_bits |= OPTION_AUTOCOMMIT;
    thd->variables.option_bits &= ~OPTION_NOT_AUTOCOMMIT;
    thd->server_status |= SERVER_STATUS_AUTOCOMMIT;
  }

  /*
    Set thread InnoDB high priority.
  */
  DBUG_EXECUTE_IF("dbug_set_high_prio_sql_thread", {
    if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL ||
        thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER)
      thd->thd_tx_priority = 1;
  });
}

void set_slave_thread_default_charset(THD *thd, Relay_log_info const *rli) {
  DBUG_TRACE;

  thd->variables.character_set_client =
      global_system_variables.character_set_client;
  thd->variables.collation_connection =
      global_system_variables.collation_connection;
  thd->variables.collation_server = global_system_variables.collation_server;
  thd->update_charset();

  /*
    We use a const cast here since the conceptual (and externally
    visible) behavior of the function is to set the default charset of
    the thread.  That the cache has to be invalidated is a secondary
    effect.
   */
  const_cast<Relay_log_info *>(rli)->cached_charset_invalidate();
}

/*
  init_replica_thread()
*/

int init_replica_thread(THD *thd, SLAVE_THD_TYPE thd_type) {
  DBUG_TRACE;
#if !defined(NDEBUG)
  int simulate_error = 0;
#endif
  thd->system_thread = (thd_type == SLAVE_THD_WORKER)
                           ? SYSTEM_THREAD_SLAVE_WORKER
                       : (thd_type == SLAVE_THD_SQL) ? SYSTEM_THREAD_SLAVE_SQL
                                                     : SYSTEM_THREAD_SLAVE_IO;
  thd->get_protocol_classic()->init_net(nullptr);
  thd->slave_thread = true;
  thd->enable_slow_log = opt_log_slow_replica_statements;
  set_slave_thread_options(thd);

  /*
    Replication threads are:
    - background threads in the server, not user sessions,
    - yet still assigned a PROCESSLIST_ID,
      for historical reasons (displayed in SHOW PROCESSLIST).
  */
  thd->set_new_thread_id();

#ifdef HAVE_PSI_THREAD_INTERFACE
  /*
    Populate the PROCESSLIST_ID in the instrumentation.
  */
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_id)(psi, thd->thread_id());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  DBUG_EXECUTE_IF("simulate_io_replica_error_on_init",
                  simulate_error |= (1 << SLAVE_THD_IO););
  DBUG_EXECUTE_IF("simulate_sql_replica_error_on_init",
                  simulate_error |= (1 << SLAVE_THD_SQL););
  thd->store_globals();
#if !defined(NDEBUG)
  if (simulate_error & (1 << thd_type)) {
    return -1;
  }
#endif

  if (thd_type == SLAVE_THD_SQL) {
    THD_STAGE_INFO(thd, stage_waiting_for_the_next_event_in_relay_log);
    thd->set_command(
        COM_QUERY);  // the SQL thread does not use the server protocol
  } else {
    THD_STAGE_INFO(thd, stage_waiting_for_source_update);
  }
  thd->set_time();
  /* Do not use user-supplied timeout value for system threads. */
  thd->variables.lock_wait_timeout = LONG_TIMEOUT;
  return 0;
}

/**
  Sleep for a given amount of time or until killed.

  @param thd        Thread context of the current thread.
  @param seconds    The number of seconds to sleep.
  @param func       Function object to check if the thread has been killed.
  @param info       The Rpl_info object associated with this sleep.

  @retval True if the thread has been killed, false otherwise.
*/
template <typename killed_func, typename rpl_info>
static inline bool slave_sleep(THD *thd, time_t seconds, killed_func func,
                               rpl_info info) {
  bool ret;
  struct timespec abstime;
  mysql_mutex_t *lock = &info->sleep_lock;
  mysql_cond_t *cond = &info->sleep_cond;

  /* Absolute system time at which the sleep time expires. */
  set_timespec(&abstime, seconds);

  mysql_mutex_lock(lock);
  thd->ENTER_COND(cond, lock, nullptr, nullptr);

  while (!(ret = func(thd, info))) {
    int error = mysql_cond_timedwait(cond, lock, &abstime);
    if (is_timeout(error)) break;
  }

  mysql_mutex_unlock(lock);
  thd->EXIT_COND(nullptr);

  return ret;
}

/**
  Callback function for mysql_binlog_open().

  Sets gtid data in the command packet.

  @param rpl              Replication stream information.
  @param packet_gtid_set  Pointer to command packet where gtid
                          data should be stored.
*/
static void fix_gtid_set(MYSQL_RPL *rpl, uchar *packet_gtid_set) {
  Gtid_set *gtid_set = (Gtid_set *)rpl->gtid_set_arg;

  gtid_set->encode(packet_gtid_set, rpl->flags & MYSQL_RPL_SKIP_TAGGED_GTIDS);
}

static int request_dump(THD *thd, MYSQL *mysql, MYSQL_RPL *rpl, Master_info *mi,
                        bool *suppress_warnings) {
  DBUG_TRACE;
  enum_server_command command =
      mi->is_auto_position() ? COM_BINLOG_DUMP_GTID : COM_BINLOG_DUMP;
  /*
    Note: binlog_flags is always 0.  However, in versions up to 5.6
    RC, the master would check the lowest bit and do something
    unexpected if it was set; in early versions of 5.6 it would also
    use the two next bits.  Therefore, for backward compatibility,
    if we ever start to use the flags, we should leave the three
    lowest bits unused.
  */
  uint binlog_flags = 0;
  binlog_flags |= USE_HEARTBEAT_EVENT_V2;

  *suppress_warnings = false;
  if (RUN_HOOK(binlog_relay_io, before_request_transmit,
               (thd, mi, binlog_flags)))
    return 1;

  rpl->server_id = server_id;
  rpl->flags = binlog_flags;

  Tsid_map tsid_map(nullptr); /* No lock needed */
  /*
    Note: should be declared at the same level as the mysql_binlog_open() call,
    as the latter might call fix_gtid_set() which in turns calls
    gtid_executed->encode().
  */
  Gtid_set gtid_executed(&tsid_map);

  if (command == COM_BINLOG_DUMP_GTID) {
    // get set of GTIDs
    mi->rli->get_tsid_lock()->wrlock();

    if (gtid_executed.add_gtid_set(mi->rli->get_gtid_set()) !=
        RETURN_STATUS_OK) {
      mi->rli->get_tsid_lock()->unlock();
      return 1;
    }
    mi->rli->get_tsid_lock()->unlock();

    global_tsid_lock->wrlock();
    gtid_state->dbug_print();

    if (gtid_executed.add_gtid_set(gtid_state->get_executed_gtids()) !=
        RETURN_STATUS_OK) {
      global_tsid_lock->unlock();
      return 1;
    }
    global_tsid_lock->unlock();

    rpl->file_name = nullptr; /* No need to set rpl.file_name_length */
    rpl->start_position = 4;
    rpl->flags |= MYSQL_RPL_GTID;
    if (mysql_get_server_version(mysql) < MYSQL_TAGGED_GTIDS_VERSION_SUPPORT) {
      rpl->flags |= MYSQL_RPL_SKIP_TAGGED_GTIDS;
    }
    DBUG_EXECUTE_IF("com_binlog_dump_gtids_force_skipping_tagged_gtids",
                    { rpl->flags |= MYSQL_RPL_SKIP_TAGGED_GTIDS; });
    rpl->gtid_set_encoded_size = gtid_executed.get_encoded_length(
        rpl->flags & MYSQL_RPL_SKIP_TAGGED_GTIDS);
    rpl->fix_gtid_set = fix_gtid_set;
    rpl->gtid_set_arg = (void *)&gtid_executed;
  } else {
    rpl->file_name_length = 0;
    rpl->file_name = mi->get_master_log_name();
    rpl->start_position = DBUG_EVALUATE_IF("request_source_log_pos_3", 3,
                                           mi->get_master_log_pos());
  }
  if (mysql_binlog_open(mysql, rpl)) {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    uint err{mysql_errno(mysql)};
    if (err == ER_NET_READ_INTERRUPTED)
      *suppress_warnings = true;  // Suppress reconnect warning
    else
      LogErr(ERROR_LEVEL, ER_RPL_REPLICA_ERROR_RETRYING,
             Command_names::str_global(command).c_str(), err,
             mysql_error(mysql), mi->connect_retry);
    if (is_network_error(err)) mi->set_network_error();
    return 1;
  }

  return 0;
}

/**
  Read one event from the master.

  @param mysql               MySQL connection.
  @param rpl                 Replication stream information.
  @param mi                  Master connection information.
  @param suppress_warnings   true when a normal net read timeout has caused us
                             to try a reconnect. We do not want to print
                             anything to the error log in this case because
                             this an abnormal event in an idle server.

  @retval 'packet_error'     Error.
  @retval  number            Length of packet.
*/

static ulong read_event(MYSQL *mysql, MYSQL_RPL *rpl, Master_info *mi,
                        bool *suppress_warnings) {
  DBUG_TRACE;

  *suppress_warnings = false;

  if (mysql_binlog_fetch(mysql, rpl)) {
    uint err{mysql_errno(mysql)};
    if (err == ER_NET_READ_INTERRUPTED) {
      /*
        We are trying a normal reconnect after a read timeout;
        we suppress prints to .err file as long as the reconnect
        happens without problems
      */
      *suppress_warnings = true;
    } else if (!mi->abort_slave) {
      LogErr(ERROR_LEVEL, ER_RPL_REPLICA_ERROR_READING_FROM_SERVER,
             mi->get_for_channel_str(), mysql_error(mysql), err);
    }
    if (is_network_error(err)) mi->set_network_error();
    return packet_error;
  }

  /* Check if eof packet */
  if (rpl->size == 0) {
    LogErr(SYSTEM_LEVEL, ER_RPL_REPLICA_DUMP_THREAD_KILLED_BY_SOURCE,
           mi->get_for_channel_str(), ::server_uuid, mysql_error(mysql));
    return packet_error;
  }

  DBUG_PRINT("exit", ("len: %lu  net->read_pos[4]: %d", rpl->size,
                      mysql->net.read_pos[4]));
  return rpl->size - 1;
}

/**
  If this is a lagging slave (specified with CHANGE REPLICATION SOURCE TO
  SOURCE_DELAY = X), delays accordingly. Also unlocks rli->data_lock.

  Design note: this is the place to unlock rli->data_lock. The lock
  must be held when reading delay info from rli, but it should not be
  held while sleeping.

  @param ev Event that is about to be executed.

  @param thd The sql thread's THD object.

  @param rli The sql thread's Relay_log_info structure.

  @retval 0 If the delay timed out and the event shall be executed.

  @retval nonzero If the delay was interrupted and the event shall be skipped.
*/
static int sql_delay_event(Log_event *ev, THD *thd, Relay_log_info *rli) {
  time_t sql_delay = rli->get_sql_delay();

  DBUG_TRACE;
  mysql_mutex_assert_owner(&rli->data_lock);
  assert(!rli->belongs_to_client());

  if (sql_delay) {
    auto type = ev->get_type_code();
    time_t sql_delay_end = 0;

    if (rli->commit_timestamps_status == Relay_log_info::COMMIT_TS_UNKNOWN &&
        Log_event_type_helper::is_any_gtid_event(type)) {
      if (static_cast<Gtid_log_event *>(ev)->has_commit_timestamps &&
          DBUG_EVALUATE_IF("sql_delay_without_timestamps", 0, 1)) {
        rli->commit_timestamps_status = Relay_log_info::COMMIT_TS_FOUND;
      } else {
        rli->commit_timestamps_status = Relay_log_info::COMMIT_TS_NOT_FOUND;
      }
    }

    if (rli->commit_timestamps_status == Relay_log_info::COMMIT_TS_FOUND) {
      if (Log_event_type_helper::is_any_gtid_event(type)) {
        /*
          Calculate when we should execute the event.
          The immediate master timestamp is expressed in microseconds.
          Delayed replication is defined in seconds.
          Hence convert immediate_commit_timestamp to seconds here.
        */
        sql_delay_end = ceil((static_cast<Gtid_log_event *>(ev)
                                  ->immediate_commit_timestamp) /
                             1000000.00) +
                        sql_delay;
      }
    } else {
      /*
        the immediate master does not support commit timestamps
        in Gtid_log_events
      */
      if (type != mysql::binlog::event::ROTATE_EVENT &&
          type != mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
          type != mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
        // Calculate when we should execute the event.
        sql_delay_end = ev->common_header->when.tv_sec +
                        rli->mi->clock_diff_with_master + sql_delay;
      }
    }
    if (sql_delay_end != 0) {
      // The current time.
      time_t now = time(nullptr);
      // The amount of time we will have to sleep before executing the event.
      time_t nap_time = 0;

      if (sql_delay_end > now) {
        nap_time = sql_delay_end - now;

        DBUG_PRINT(
            "info",
            ("sql_delay= %lu "
             "now= %ld "
             "sql_delay_end= %ld "
             "nap_time= %ld",
             static_cast<unsigned long>(sql_delay), static_cast<long>(now),
             static_cast<long>(sql_delay_end), static_cast<long>(nap_time)));
        DBUG_PRINT("info", ("delaying replication event %lu secs",
                            static_cast<unsigned long>(nap_time)));
        rli->start_sql_delay(sql_delay_end);
        mysql_mutex_unlock(&rli->data_lock);
        return slave_sleep(thd, nap_time, sql_slave_killed, rli);
      } else {
        DBUG_PRINT("info",
                   ("sql_delay= %lu "
                    "now= %ld "
                    "sql_delay_end= %ld ",
                    static_cast<unsigned long>(sql_delay),
                    static_cast<long>(now), static_cast<long>(sql_delay_end)));
      }
    }
  }
  mysql_mutex_unlock(&rli->data_lock);
  return 0;
}

/**
  Applies the given event and advances the relay log position.

  This is needed by the sql thread to execute events from the binlog,
  and by clients executing BINLOG statements.  Conceptually, this
  function does:

  @code
    ev->apply_event(rli);
    ev->update_pos(rli);
  @endcode

  It also does the following maintenance:

   - Initializes the thread's server_id and time; and the event's
     thread.

   - If !rli->belongs_to_client() (i.e., if it belongs to the slave
     sql thread instead of being used for executing BINLOG
     statements), it does the following things: (1) skips events if it
     is needed according to the server id or slave_skip_counter; (2)
     unlocks rli->data_lock; (3) sleeps if required by 'CHANGE REPLICATION
  SOURCE TO SOURCE_DELAY=X'; (4) maintains the running state of the sql thread
  (rli->thread_state).

   - Reports errors as needed.

  @param ptr_ev a pointer to a reference to the event to apply.

  @param thd The client thread that executes the event (i.e., the
  slave sql thread if called from a replication slave, or the client
  thread if called to execute a BINLOG statement).

  @param rli The relay log info (i.e., the slave's rli if called from
  a replication slave, or the client's thd->rli_fake if called to
  execute a BINLOG statement).

  @note MTS can store NULL to @c ptr_ev location to indicate
        the event is taken over by a Worker.

  @retval SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK
          OK.

  @retval SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR
          Error calling ev->apply_event().

  @retval SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR
          No error calling ev->apply_event(), but error calling
          ev->update_pos().

  @retval SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR
          append_item_to_jobs() failed, thread was killed while waiting
          for successful enqueue on worker.
*/
static enum enum_slave_apply_event_and_update_pos_retval
apply_event_and_update_pos(Log_event **ptr_ev, THD *thd, Relay_log_info *rli) {
  int exec_res = 0;
  bool skip_event = false;
  Log_event *ev = *ptr_ev;
  Log_event::enum_skip_reason reason = Log_event::EVENT_SKIP_NOT;

  DBUG_TRACE;

  DBUG_PRINT("exec_event",
             ("%s(type_code: %d; server_id: %d)", ev->get_type_str(),
              ev->get_type_code(), ev->server_id));
  DBUG_PRINT("info",
             ("thd->options: %s%s; rli->last_event_start_time: %lu",
              FLAGSTR(thd->variables.option_bits, OPTION_NOT_AUTOCOMMIT),
              FLAGSTR(thd->variables.option_bits, OPTION_BEGIN),
              (ulong)rli->last_event_start_time));

  /*
    Execute the event to change the database and update the binary
    log coordinates, but first we set some data that is needed for
    the thread.

    The event will be executed unless it is supposed to be skipped.

    Queries originating from this server must be skipped.  Low-level
    events (Format_description_log_event, Rotate_log_event,
    Stop_log_event) from this server must also be skipped. But for
    those we don't want to modify 'group_master_log_pos', because
    these events did not exist on the master.
    Format_description_log_event is not completely skipped.

    Skip queries specified by the user in 'slave_skip_counter'.  We
    can't however skip events that has something to do with the log
    files themselves.

    Filtering on own server id is extremely important, to ignore
    execution of events created by the creation/rotation of the relay
    log (remember that now the relay log starts with its Format_desc,
    has a Rotate etc).
  */
  /*
     Set the unmasked and actual server ids from the event
   */
  thd->server_id = ev->server_id;  // use the original server id for logging
  thd->unmasked_server_id = ev->common_header->unmasked_server_id;
  thd->set_time();  // time the query
  thd->lex->set_current_query_block(nullptr);
  if (!ev->common_header->when.tv_sec)
    my_micro_time_to_timeval(my_micro_time(), &ev->common_header->when);
  ev->thd = thd;  // because up to this point, ev->thd == 0

  if (!(rli->is_mts_recovery() &&
        bitmap_is_set(&rli->recovery_groups, rli->mts_recovery_index))) {
    reason = ev->shall_skip(rli);
  }
#ifndef NDEBUG
  if (rli->is_mts_recovery()) {
    DBUG_PRINT("mta",
               ("Mta is recovering %d, number of bits set %d, "
                "bitmap is set %d, index %lu.\n",
                rli->is_mts_recovery(), bitmap_bits_set(&rli->recovery_groups),
                bitmap_is_set(&rli->recovery_groups, rli->mts_recovery_index),
                rli->mts_recovery_index));
  }
#endif
  if (reason == Log_event::EVENT_SKIP_COUNT) {
    --rli->slave_skip_counter;
    skip_event = true;
  }

  if (reason == Log_event::EVENT_SKIP_NOT) {
    // Sleeps if needed, and unlocks rli->data_lock.
    if (sql_delay_event(ev, thd, rli))
      return SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK;

    // Setting positions for STA. Worker positions are set on
    // slave_worker_exec_job_group
    rli->set_group_source_log_start_end_pos(ev);
    exec_res = ev->apply_event(rli);

    DBUG_EXECUTE_IF("simulate_stop_when_mta_in_group",
                    if (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP &&
                        rli->curr_group_seen_begin)
                        DBUG_SET("+d,stop_when_mta_in_group"););

    if (!exec_res && (ev->worker != rli)) {
      if (ev->worker) {
        Slave_job_item item = {
            ev,
            rli->get_event_start_pos(),
            {'\0'},
            rli->get_applier_metrics().is_after_metrics_breakpoint()};
        if (rli->get_event_relay_log_name())
          strcpy(item.event_relay_log_name, rli->get_event_relay_log_name());
        Slave_job_item *job_item = &item;
        Slave_worker *w = (Slave_worker *)ev->worker;
        // specially marked group typically with OVER_MAX_DBS_IN_EVENT_MTS db:s
        bool need_sync = ev->is_mts_group_isolated();

        // all events except BEGIN-query must be marked with a non-NULL Worker
        assert(((Slave_worker *)ev->worker) == rli->last_assigned_worker);

        DBUG_PRINT("Log_event::apply_event:",
                   ("-> job item data %p to W_%lu", job_item->data, w->id));

        // Reset mts in-group state
        if (rli->mts_group_status == Relay_log_info::MTS_END_GROUP) {
          // CGAP cleanup
          rli->curr_group_assigned_parts.clear();
          // reset the B-group and Gtid-group marker
          rli->curr_group_seen_begin = rli->curr_group_seen_gtid = false;
          rli->last_assigned_worker = nullptr;
        }
        /*
           Storing GAQ index of the group that the event belongs to
           in the event. Deferred events are handled similarly below.
        */
        ev->mts_group_idx = rli->gaq->assigned_group_index;

        bool append_item_to_jobs_error = false;
        if (rli->curr_group_da.size() > 0) {
          /*
            the current event sorted out which partition the current group
            belongs to. It's time now to processed deferred array events.
          */
          for (uint i = 0; i < rli->curr_group_da.size(); i++) {
            Slave_job_item da_item = rli->curr_group_da[i];
            DBUG_PRINT("mta", ("Assigning job %llu to worker %lu",
                               (da_item.data)->common_header->log_pos, w->id));
            da_item.data->mts_group_idx =
                rli->gaq->assigned_group_index;  // similarly to above
            if (!append_item_to_jobs_error)
              append_item_to_jobs_error = append_item_to_jobs(&da_item, w, rli);
            if (append_item_to_jobs_error) delete da_item.data;
          }
          rli->curr_group_da.clear();
        }
        if (append_item_to_jobs_error)
          return SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR;

        DBUG_PRINT("mta", ("Assigning job %llu to worker %lu\n",
                           job_item->data->common_header->log_pos, w->id));

        /* Notice `ev' instance can be destroyed after `append()' */
        if (append_item_to_jobs(job_item, w, rli))
          return SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR;
        if (need_sync) {
          /*
            combination of over-max db:s and end of the current group
            forces to wait for the assigned groups completion by assigned
            to the event worker.
            Indeed MTS group status could be safely set to MTS_NOT_IN_GROUP
            after wait_() returns.
            No need to know a possible error out of synchronization call.
          */
          (void)rli->current_mts_submode->wait_for_workers_to_finish(rli);
        }
      }
      *ptr_ev = nullptr;  // announcing the event is passed to w-worker
    }
  } else
    mysql_mutex_unlock(&rli->data_lock);

  DBUG_PRINT("info", ("apply_event error = %d", exec_res));
  if (exec_res == 0) {
    /*
      Positions are not updated here when an XID is processed. To make
      a slave crash-safe, positions must be updated while processing a
      XID event and as such do not need to be updated here again.

      However, if the event needs to be skipped, this means that it
      will not be processed and then positions need to be updated here.

      DDL:s that are not yet committed, as indicated by
      @c has_ddl_committed flag, visit the block.

      See sql/rpl_rli.h for further details.
    */
    int error = 0;
    if (*ptr_ev &&
        ((ev->get_type_code() != mysql::binlog::event::XID_EVENT &&
          !is_committed_ddl(*ptr_ev)) ||
         skip_event ||
         (rli->is_mts_recovery() && !is_any_gtid_event(ev) &&
          (ev->ends_group() || !rli->mts_recovery_group_seen_begin) &&
          bitmap_is_set(&rli->recovery_groups, rli->mts_recovery_index)))) {
#ifndef NDEBUG
      /*
        This only prints information to the debug trace.

        TODO: Print an informational message to the error log?
      */
      static const char *const explain[] = {
          // EVENT_SKIP_NOT,
          "not skipped",
          // EVENT_SKIP_IGNORE,
          "skipped because event should be ignored",
          // EVENT_SKIP_COUNT
          "skipped because event skip counter was non-zero"};
      DBUG_PRINT("info",
                 ("OPTION_BEGIN: %d; IN_STMT: %d",
                  static_cast<bool>(thd->variables.option_bits & OPTION_BEGIN),
                  rli->get_flag(Relay_log_info::IN_STMT)));
      DBUG_PRINT("skip_event",
                 ("%s event was %s", ev->get_type_str(), explain[reason]));
#endif

      error = ev->update_pos(rli);
      /*
        Slave skips an event if the slave_skip_counter is greater than zero.
        We have to free thd's mem_root here after we update the positions
        in the repository table if the event is a skipped event.
        Otherwise, imagine a situation where slave_skip_counter is big number
        and slave is skipping the events and updating the repository.
        All the memory used while these operations are going on is never
        freed unless slave starts executing the events (after slave_skip_counter
        becomes zero).

        Hence we free thd's mem_root here if it is a skipped event.
        (freeing mem_root generally happens from Query_log_event::do_apply_event
        or Rows_log_event::do_apply_event when they find the end of
        the group event).
      */
      if (skip_event) thd->mem_root->ClearForReuse();

#ifndef NDEBUG
      DBUG_PRINT("info", ("update_pos error = %d", error));
      if (!rli->belongs_to_client()) {
        char buf[22];
        DBUG_PRINT("info",
                   ("group %s %s", llstr(rli->get_group_relay_log_pos(), buf),
                    rli->get_group_relay_log_name()));
        DBUG_PRINT("info",
                   ("event %s %s", llstr(rli->get_event_relay_log_pos(), buf),
                    rli->get_event_relay_log_name()));
      }
#endif
    } else {
      /*
        INTVAR_EVENT, RAND_EVENT, USER_VAR_EVENT and ROWS_QUERY_LOG_EVENT are
        deferred event. It means ev->worker is NULL.
      */
      assert(*ptr_ev == ev || rli->is_parallel_exec() ||
             (!ev->worker &&
              (ev->get_type_code() == mysql::binlog::event::INTVAR_EVENT ||
               ev->get_type_code() == mysql::binlog::event::RAND_EVENT ||
               ev->get_type_code() == mysql::binlog::event::USER_VAR_EVENT ||
               ev->get_type_code() ==
                   mysql::binlog::event::ROWS_QUERY_LOG_EVENT)));

      rli->inc_event_relay_log_pos();
    }

    if (!error && rli->is_mts_recovery() &&
        ev->get_type_code() != mysql::binlog::event::ROTATE_EVENT &&
        ev->get_type_code() != mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
        ev->get_type_code() != mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
      if (ev->starts_group()) {
        rli->mts_recovery_group_seen_begin = true;
      } else if ((ev->ends_group() || !rli->mts_recovery_group_seen_begin) &&
                 !is_any_gtid_event(ev)) {
        rli->mts_recovery_index++;
        if (--rli->mts_recovery_group_cnt == 0) {
          rli->mts_recovery_index = 0;
          LogErr(INFORMATION_LEVEL, ER_RPL_MTA_RECOVERY_COMPLETE,
                 rli->get_for_channel_str(), rli->get_group_relay_log_name(),
                 rli->get_group_relay_log_pos(),
                 rli->get_group_master_log_name(),
                 rli->get_group_master_log_pos());
          /*
             Few tests wait for UNTIL_SQL_AFTER_MTS_GAPS completion.
             Due to existing convention the status won't change
             prior to slave restarts.
             So making of UNTIL_SQL_AFTER_MTS_GAPS completion is done here,
             and only in the debug build to make the test to catch the change
             despite a faulty design of UNTIL checking before execution.
          */
          if (rli->until_condition ==
              Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS) {
            rli->until_condition = Relay_log_info::UNTIL_DONE;
          }
          // reset the Worker tables to remove last slave session time info
          if ((error = rli->mts_finalize_recovery())) {
            (void)Rpl_info_factory::reset_workers(rli);
          }
        }
        rli->mts_recovery_group_seen_begin = false;
        if (!error)
          error = rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT);
      }
    }

    if (error) {
      /*
        The update should not fail, so print an error message and
        return an error code.

        TODO: Replace this with a decent error message when merged
        with BUG#24954 (which adds several new error message).
      */
      char buf[22];
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "It was not possible to update the positions"
                  " of the relay log information: the replica may"
                  " be in an inconsistent state."
                  " Stopped in %s position %s",
                  rli->get_group_relay_log_name(),
                  llstr(rli->get_group_relay_log_pos(), buf));
      return SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR;
    }
  }

  return exec_res ? SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR
                  : SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK;
}

/**
  Let the worker applying the current group to rollback and gracefully
  finish its work before.

  @param rli The slave's relay log info.

  @param ev a pointer to the event on hold before applying this rollback
  procedure.

  @retval false The rollback succeeded.

  @retval true  There was an error while injecting events.
*/
static bool coord_handle_partial_binlogged_transaction(Relay_log_info *rli,
                                                       const Log_event *ev) {
  DBUG_TRACE;
  /*
    This function is called holding the rli->data_lock.
    We must return it still holding this lock, except in the case of returning
    error.
  */
  mysql_mutex_assert_owner(&rli->data_lock);
  THD *thd = rli->info_thd;

  if (!rli->curr_group_seen_begin) {
    DBUG_PRINT("info", ("Injecting QUERY(BEGIN) to rollback worker"));
    Log_event *begin_event = new Query_log_event(thd, STRING_WITH_LEN("BEGIN"),
                                                 true,  /* using_trans */
                                                 false, /* immediate */
                                                 true,  /* suppress_use */
                                                 0,     /* error */
                                                 true /* ignore_command */);
    ((Query_log_event *)begin_event)->db = "";
    begin_event->common_header->data_written = 0;
    begin_event->server_id = ev->server_id;
    /*
      We must be careful to avoid SQL thread increasing its position
      farther than the event that triggered this QUERY(BEGIN).
    */
    begin_event->common_header->log_pos = ev->common_header->log_pos;
    begin_event->future_event_relay_log_pos = ev->future_event_relay_log_pos;

    if (apply_event_and_update_pos(&begin_event, thd, rli) !=
        SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK) {
      delete begin_event;
      return true;
    }
    mysql_mutex_lock(&rli->data_lock);
  }

  DBUG_PRINT("info", ("Injecting QUERY(ROLLBACK) to rollback worker"));
  Log_event *rollback_event = new Query_log_event(
      thd, STRING_WITH_LEN("ROLLBACK"), true, /* using_trans */
      false,                                  /* immediate */
      true,                                   /* suppress_use */
      0,                                      /* error */
      true /* ignore_command */);
  ((Query_log_event *)rollback_event)->db = "";
  rollback_event->common_header->data_written = 0;
  rollback_event->server_id = ev->server_id;
  /*
    We must be careful to avoid SQL thread increasing its position
    farther than the event that triggered this QUERY(ROLLBACK).
  */
  rollback_event->common_header->log_pos = ev->common_header->log_pos;
  rollback_event->future_event_relay_log_pos = ev->future_event_relay_log_pos;

  ((Query_log_event *)rollback_event)->rollback_injected_by_coord = true;

  if (apply_event_and_update_pos(&rollback_event, thd, rli) !=
      SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK) {
    delete rollback_event;
    return true;
  }
  mysql_mutex_lock(&rli->data_lock);

  return false;
}

/**
  Top-level function for executing the next event in the relay log.
  This is called from the SQL thread.

  This function reads the event from the relay log, executes it, and
  advances the relay log position.  It also handles errors, etc.

  This function may fail to apply the event for the following reasons:

   - The position specified by the UNTIL condition of the START REPLICA
     command is reached.

   - It was not possible to read the event from the log.

   - The slave is killed.

   - An error occurred when applying the event, and the event has been
     tried slave_trans_retries times.  If the event has been retried
     fewer times, 0 is returned.

   - init_info or init_relay_log_pos failed. (These are called
     if a failure occurs when applying the event.)

   - An error occurred when updating the binlog position.

  @retval 0 The event was applied.

  @retval 1 The event was not applied.
*/
static int exec_relay_log_event(THD *thd, Relay_log_info *rli,
                                Rpl_applier_reader *applier_reader,
                                Log_event *in) {
  DBUG_TRACE;

  /*
     We acquire this mutex since we need it for all operations except
     event execution. But we will release it in places where we will
     wait for something for example inside of next_event().
   */
  mysql_mutex_lock(&rli->data_lock);

  Log_event *ev = in;
  Log_event **ptr_ev = nullptr;
  RLI_current_event_raii rli_c_ev(rli, ev);

  if (ev != nullptr) {
    /*
      To avoid assigned event groups exceeding rli->checkpoint_group, it
      need force to compute checkpoint.
    */
    bool force = rli->rli_checkpoint_seqno >= rli->checkpoint_group;
    if (force || rli->is_time_for_mta_checkpoint()) {
      mysql_mutex_unlock(&rli->data_lock);
      if (mta_checkpoint_routine(rli, force)) {
        delete ev;
        return 1;
      }
      mysql_mutex_lock(&rli->data_lock);
    }
  }

  /*
    It should be checked after calling mta_checkpoint_routine(), because that
    function could be interrupted by kill while 'force' is true.
  */
  if (sql_slave_killed(thd, rli)) {
    mysql_mutex_unlock(&rli->data_lock);
    delete ev;

    LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_ERROR_READING_RELAY_LOG_EVENTS,
           rli->get_for_channel_str(), "replica SQL thread was killed");
    return 1;
  }

  if (ev) {
    enum enum_slave_apply_event_and_update_pos_retval exec_res;

    ptr_ev = &ev;
    /*
      Even if we don't execute this event, we keep the master timestamp,
      so that seconds behind master shows correct delta (there are events
      that are not replayed, so we keep falling behind).

      If it is an artificial event, or a relay log event (IO thread generated
      event) or ev->when is set to 0, or a FD from master, or a heartbeat
      event with server_id '0' then  we don't update the last_master_timestamp.

      In case of parallel execution last_master_timestamp is only updated when
      a job is taken out of GAQ. Thus when last_master_timestamp is 0 (which
      indicates that GAQ is empty, all slave workers are waiting for events from
      the Coordinator), we need to initialize it with a timestamp from the first
      event to be executed in parallel.
    */
    if ((!rli->is_parallel_exec() || rli->last_master_timestamp == 0) &&
        !(ev->is_artificial_event() || ev->is_relay_log_event() ||
          ev->get_type_code() ==
              mysql::binlog::event::FORMAT_DESCRIPTION_EVENT ||
          ev->server_id == 0)) {
      rli->last_master_timestamp =
          ev->common_header->when.tv_sec + (time_t)ev->exec_time;
      assert(rli->last_master_timestamp >= 0);
    }

    if (rli->is_until_satisfied_before_dispatching_event(ev)) {
      /*
        Setting abort_slave flag because we do not want additional message about
        error in query execution to be printed.
      */
      rli->abort_slave = true;
      mysql_mutex_unlock(&rli->data_lock);
      delete ev;
      return SLAVE_APPLY_EVENT_UNTIL_REACHED;
    }

    {
      /**
        The following failure injecion works in cooperation
        with tests setting @@global.debug= 'd,incomplete_group_in_relay_log'.
        Xid or Commit events are not executed to force the slave sql read
        hanging if the relay log does not have any more events.
       */
      DBUG_EXECUTE_IF(
          "incomplete_group_in_relay_log",
          if ((ev->get_type_code() == mysql::binlog::event::XID_EVENT) ||
              ((ev->get_type_code() == mysql::binlog::event::QUERY_EVENT) &&
               strcmp("COMMIT", ((Query_log_event *)ev)->query) == 0)) {
            rli->abort_slave = 1;
            mysql_mutex_unlock(&rli->data_lock);
            delete ev;
            rli->inc_event_relay_log_pos();
            return 0;
          };);
    }

    /*
      GTID protocol will put a FORMAT_DESCRIPTION_EVENT from the master with
      log_pos != 0 after each (re)connection if auto positioning is enabled.
      This means that the SQL thread might have already started to apply the
      current group but, as the IO thread had to reconnect, it left this
      group incomplete and will start it again from the beginning.
      So, before applying this FORMAT_DESCRIPTION_EVENT, we must let the
      worker roll back the current group and gracefully finish its work,
      before starting to apply the new (complete) copy of the group.
    */
    if (ev->get_type_code() == mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
        ev->server_id != ::server_id && ev->common_header->log_pos != 0 &&
        rli->is_parallel_exec() && rli->curr_group_seen_gtid) {
      if (coord_handle_partial_binlogged_transaction(rli, ev))
        /*
          In the case of an error, coord_handle_partial_binlogged_transaction
          will not try to get the rli->data_lock again.
        */
        return 1;
    }

    DBUG_EXECUTE_IF("wait_on_exec_relay_log_event", {
      if (ev->get_type_code() == mysql::binlog::event::WRITE_ROWS_EVENT) {
        const char act[] =
            "now SIGNAL signal.waiting_on_event_execution "
            "WAIT_FOR signal.can_continue_execution";
        assert(opt_debug_sync_timeout > 0);
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      }
    };);

    /* ptr_ev can change to NULL indicating MTS coorinator passed to a Worker */
    exec_res = apply_event_and_update_pos(ptr_ev, thd, rli);
    /*
      Note: the above call to apply_event_and_update_pos executes
      mysql_mutex_unlock(&rli->data_lock);
    */

    /* For deferred events, the ptr_ev is set to NULL
        in Deferred_log_events::add() function.
        Hence deferred events won't be deleted here.
        They will be deleted in Deferred_log_events::rewind() function.
    */
    if (*ptr_ev) {
      assert(*ptr_ev == ev);  // event remains to belong to Coordinator

      DBUG_EXECUTE_IF("dbug.calculate_sbm_after_previous_gtid_log_event", {
        if (ev->get_type_code() ==
            mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
          rpl_replica_debug_point(DBUG_RPL_S_SBM_AFTER_PREVIOUS_GTID_EV, thd);
        }
      };);
      DBUG_EXECUTE_IF("dbug.calculate_sbm_after_fake_rotate_log_event", {
        if (ev->get_type_code() == mysql::binlog::event::ROTATE_EVENT &&
            ev->is_artificial_event()) {
          rpl_replica_debug_point(DBUG_RPL_S_SBM_AFTER_FAKE_ROTATE_EV, thd);
        }
      };);
      /*
        Format_description_log_event should not be deleted because it will be
        used to read info about the relay log's format; it will be deleted when
        the SQL thread does not need it, i.e. when this thread terminates.
        ROWS_QUERY_LOG_EVENT is destroyed at the end of the current statement
        clean-up routine.
      */
      if (ev->get_type_code() !=
              mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
          ev->get_type_code() != mysql::binlog::event::ROWS_QUERY_LOG_EVENT) {
        DBUG_PRINT("info", ("Deleting the event after it has been executed"));
        delete ev;
        /*
          Raii guard is explicitly instructed to invalidate
          otherwise bogus association of the execution context with the being
          destroyed above event.
        */
        ev = rli->current_event = nullptr;
      }
    }

    /*
      exec_res == SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR
                  update_log_pos failed: this should not happen, so we
                  don't retry.
      exec_res == SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR
                  append_item_to_jobs() failed, this happened because
                  thread was killed while waiting for enqueue on worker.
    */
    if (exec_res >= SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR) {
      delete ev;
      return 1;
    }

    if (slave_trans_retries) {
      int temp_err = 0;
      bool silent = false;
      if (exec_res && !is_mts_worker(thd) /* no reexecution in MTS mode */ &&
          (temp_err = rli->has_temporary_error(thd, 0, &silent)) &&
          !thd->get_transaction()->cannot_safely_rollback(
              Transaction_ctx::SESSION)) {
        const char *errmsg;
        /*
          We were in a transaction which has been rolled back because of a
          temporary error;
          let's seek back to BEGIN log event and retry it all again.
          Note, if lock wait timeout (innodb_lock_wait_timeout exceeded)
          there is no rollback since 5.0.13 (ref: manual).
          We have to not only seek but also
          a) init_info(), to seek back to hot relay log's start for later
          (for when we will come back to this hot log after re-processing the
          possibly existing old logs where BEGIN is: applier_reader will
          then need the cache to be at position 0 (see comments at beginning of
          init_info()).
          b) init_relay_log_pos(), because the BEGIN may be an older relay log.
        */
        if (rli->trans_retries < slave_trans_retries) {
          /*
            The transactions has to be rolled back before
            load_mi_and_rli_from_repositories is called. Because
            load_mi_and_rli_from_repositories will start a new
            transaction.
          */
          rli->cleanup_context(thd, true);
          /*
            Temporary error status is both unneeded and harmful for following
            open-and-lock slave system tables but store its number first for
            monitoring purposes.
          */
          uint temp_trans_errno = thd->get_stmt_da()->mysql_errno();
          thd->clear_error();
          applier_reader->close();
          /*
             We need to figure out if there is a test case that covers
             this part. \Alfranio.
          */
          if (load_mi_and_rli_from_repositories(rli->mi, false, REPLICA_SQL,
                                                false, true))
            LogErr(ERROR_LEVEL,
                   ER_RPL_REPLICA_FAILED_TO_INIT_CONNECTION_METADATA_STRUCTURE,
                   rli->get_for_channel_str());
          else if (applier_reader->open(&errmsg))
            LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_INIT_RELAY_LOG_POSITION,
                   rli->get_for_channel_str(), errmsg);
          else {
            exec_res = SLAVE_APPLY_EVENT_RETRY;
            /* chance for concurrent connection to get more locks */
            slave_sleep(thd,
                        min<ulong>(rli->trans_retries, MAX_SLAVE_RETRY_PAUSE),
                        sql_slave_killed, rli);
            mysql_mutex_lock(&rli->data_lock);  // because of SHOW STATUS
            if (!silent) {
              rli->trans_retries++;
              if (rli->is_processing_trx()) {
                rli->retried_processing(temp_trans_errno,
                                        ER_THD_NONCONST(thd, temp_trans_errno),
                                        rli->trans_retries);
              }
            }
            rli->retried_trans++;
            mysql_mutex_unlock(&rli->data_lock);
#ifndef NDEBUG
            if (rli->trans_retries == 2 || rli->trans_retries == 6)
              DBUG_EXECUTE_IF("rpl_ps_tables_worker_retry", {
                rpl_replica_debug_point(DBUG_RPL_S_PS_TABLE_WORKER_RETRY);
              };);

#endif
            DBUG_PRINT("info", ("Replica retries transaction "
                                "rli->trans_retries: %lu",
                                rli->trans_retries));
          }
        } else {
          thd->fatal_error();
          rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                      "Replica SQL thread retried transaction %lu time(s) "
                      "in vain, giving up. Consider raising the value of "
                      "the replica_transaction_retries variable.",
                      rli->trans_retries);
        }
      } else if ((exec_res && !temp_err) ||
                 (opt_using_transactions &&
                  rli->get_group_relay_log_pos() ==
                      rli->get_event_relay_log_pos())) {
        /*
          Only reset the retry counter if the entire group succeeded
          or failed with a non-transient error.  On a successful
          event, the execution will proceed as usual; in the case of a
          non-transient error, the slave will stop with an error.
         */
        rli->trans_retries = 0;  // restart from fresh
        DBUG_PRINT("info", ("Resetting retry counter, rli->trans_retries: %lu",
                            rli->trans_retries));
      }
    }
    if (exec_res) {
      delete ev;
      /* Raii object is explicitly updated 'cos this branch doesn't end func */
      rli->current_event = nullptr;
    } else if (rli->is_until_satisfied_after_dispatching_event()) {
      mysql_mutex_lock(&rli->data_lock);
      rli->abort_slave = true;
      mysql_mutex_unlock(&rli->data_lock);
      return SLAVE_APPLY_EVENT_UNTIL_REACHED;
    }
    return exec_res;
  }

  /*
    It is impossible to read next event to finish the event group whenever a
    read event error happens. So MTS group status is set to MTS_KILLED_GROUP to
    force stop.
  */
  if (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP)
    rli->mts_group_status = Relay_log_info::MTS_KILLED_GROUP;

  mysql_mutex_unlock(&rli->data_lock);
  rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
              ER_THD(thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
              "Could not parse relay log event entry. The possible reasons "
              "are: the source's "
              "binary log is corrupted (you can check this by running "
              "'mysqlbinlog' on the "
              "binary log), the replica's relay log is corrupted (you can "
              "check this by running "
              "'mysqlbinlog' on the relay log), a network problem, the server "
              "was unable to "
              "fetch a keyring key required to open an encrypted relay log "
              "file, or a bug in "
              "the source's or replica's MySQL code. If you want to check the "
              "source's binary "
              "log or replica's relay log, you will be able to know their "
              "names by issuing "
              "'SHOW REPLICA STATUS' on this replica.");

  return SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR;
}

static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info) {
  if (io_slave_killed(thd, mi)) {
    if (info)
      LogErr(INFORMATION_LEVEL, ER_RPL_IO_THREAD_KILLED, info,
             mi->get_for_channel_str());
    return true;
  }
  return false;
}

/**
  @brief Try to reconnect slave IO thread.

  @details Terminates current connection to master, sleeps for
  @c mi->connect_retry msecs and initiates new connection with
  @c safe_reconnect(). Variable pointed by @c retry_count is increased -
  if it exceeds @c mi->retry_count then connection is not re-established
  and function signals error.

  Unless @c suppress_warnings is true, a warning is written to the
  server error log when reconnecting. The warning message, the
  messages used to report errors, and the thread stages, are taken
  from @c rm. In case @c mi->retry_count is exceeded, no messages are
  added to the log.

  @param[in]     thd                 Thread context.
  @param[in]     mysql               MySQL connection.
  @param[in]     mi                  Master connection information.
  @param[in,out] retry_count         Number of attempts to reconnect.
  @param[in]     suppress_warnings   true when a normal net read timeout
                                     has caused to reconnecting.
  @param[in]     messages            Error/warning messages and thread stage
                                     information. See class Reconnect_messages.

  @retval        0                   OK.
  @retval        1                   There was an error.
*/

static int try_to_reconnect(THD *thd, MYSQL *mysql, Master_info *mi,
                            uint *retry_count, bool suppress_warnings,
                            const Reconnect_messages &messages) {
  mi->slave_running = MYSQL_SLAVE_RUN_NOT_CONNECT;
  THD_STAGE_INFO(thd, messages.stage_waiting_to_reconnect);
  DBUG_EXECUTE_IF("hang_in_stage_replica_waiting_to_reconnect", {
    while (!io_slave_killed(thd, mi)) my_sleep(100000);  // 0.1 second
  });
  thd->clear_active_vio();
  end_server(mysql);
  if ((*retry_count)++) {
    if (*retry_count > mi->retry_count) return 1;  // Don't retry forever
    slave_sleep(thd, mi->connect_retry, io_slave_killed, mi);
  }
  if (check_io_slave_killed(thd, mi,
                            messages.error_killed_while_waiting.c_str()))
    return 1;
  THD_STAGE_INFO(thd, messages.stage_reconnecting);
  DBUG_EXECUTE_IF("hang_in_stage_replica_reconnecting", {
    while (!io_slave_killed(thd, mi)) my_sleep(100000);  // 0.1 second
  });
  if (!suppress_warnings) {
    char llbuff[22];
    /*
      Raise a warining during registering on master/requesting dump.
      Log a message reading event.
    */
    if (messages.triggering_command.length()) {
      char buf[256];
      snprintf(buf, sizeof(buf), messages.triggering_error.c_str(),
               mi->get_io_rpl_log_name(),
               llstr(mi->get_master_log_pos(), llbuff));

      mi->report(WARNING_LEVEL, ER_REPLICA_SOURCE_COM_FAILURE,
                 ER_THD(thd, ER_REPLICA_SOURCE_COM_FAILURE),
                 messages.triggering_command.c_str(), buf);
    } else {
      LogErr(INFORMATION_LEVEL, ER_REPLICA_RECONNECT_FAILED,
             mi->get_io_rpl_log_name(), llstr(mi->get_master_log_pos(), llbuff),
             mi->get_for_channel_str());
    }
  }
  if (safe_reconnect(thd, mysql, mi, true) || io_slave_killed(thd, mi)) {
    LogErr(INFORMATION_LEVEL, ER_REPLICA_KILLED_AFTER_RECONNECT);
    return 1;
  }
  return 0;
}

/**
  Slave IO thread entry point.

  @param arg Pointer to Master_info struct that holds information for
  the IO thread.

  @return Always 0.
*/
extern "C" void *handle_slave_io(void *arg) {
  THD *thd{nullptr};  // needs to be first for thread_stack
  bool thd_added{false};
  MYSQL *mysql;
  Master_info *mi = (Master_info *)arg;
  Relay_log_info *rli = mi->rli;
  char llbuff[22];
  uint retry_count;
  bool suppress_warnings;
  int ret;
  Async_conn_failover_manager::SourceQuorumStatus quorum_status{
      Async_conn_failover_manager::SourceQuorumStatus::no_error};
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  {
    DBUG_TRACE;

    assert(mi->inited);
    mysql = nullptr;

    mysql_mutex_lock(&mi->run_lock);

    /* Inform waiting threads that slave has started */
    mi->slave_run_id++;

    thd = new THD;  // note that constructor of THD uses DBUG_ !
    THD_CHECK_SENTRY(thd);
    mi->info_thd = thd;

#ifdef HAVE_PSI_THREAD_INTERFACE
    // save the instrumentation for IO thread in mi->info_thd
    struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
    thd_set_psi(mi->info_thd, psi);
#endif
    mysql_thread_set_psi_THD(thd);

    thd->thread_stack = (char *)&thd;  // remember where our stack is
    mi->clear_error();
    mi->slave_running = 1;
    if (init_replica_thread(thd, SLAVE_THD_IO)) {
      mysql_cond_broadcast(&mi->start_cond);
      mysql_mutex_unlock(&mi->run_lock);
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                 "Failed during replica I/O thread initialization ");
      goto err;
    }

    thd_manager->add_thd(thd);
    thd_added = true;

    mi->abort_slave = false;
    mysql_mutex_unlock(&mi->run_lock);
    mysql_cond_broadcast(&mi->start_cond);

  connect_init:
    DBUG_PRINT("source_info",
               ("log_file_name: '%s'  position: %s", mi->get_master_log_name(),
                llstr(mi->get_master_log_pos(), llbuff)));

    /* This must be called before run any binlog_relay_io hooks */
    RPL_MASTER_INFO = mi;

    if (RUN_HOOK(binlog_relay_io, thread_start, (thd, mi))) {
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                 "Failed to run 'thread_start' hook");
      goto err;
    }

    retry_count = 0;
    if (!(mi->mysql = mysql = mysql_init(nullptr))) {
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(thd, ER_REPLICA_FATAL_ERROR), "error in mysql_init()");
      goto err;
    }

    THD_STAGE_INFO(thd, stage_connecting_to_source);

    if (safe_connect(thd, mysql, mi)) {
      goto err;
    }

  connected:

    /*
      When using auto positioning, the slave IO thread will always start reading
      a transaction from the beginning of the transaction (transaction's first
      event). So, we have to reset the transaction boundary parser after
      (re)connecting.
      If not using auto positioning, the Relay_log_info::rli_init_info() took
      care of putting the mi->transaction_parser in the correct state when
      initializing Received_gtid_set from relay log during slave server starts,
      as the IO thread might had stopped in the middle of a transaction.
    */
    if (mi->is_auto_position()) {
      mi->transaction_parser.reset();
      mi->clear_queueing_trx(true /* need_lock*/);
    }

    mi->reset_network_error();

    DBUG_EXECUTE_IF("dbug.before_get_running_status_yes", {
      rpl_replica_debug_point(DBUG_RPL_S_BEFORE_RUNNING_STATUS, thd);
    };);
    DBUG_EXECUTE_IF("dbug.calculate_sbm_after_previous_gtid_log_event", {
      /* Fake that thread started 3 minutes ago */
      thd->start_time.tv_sec -= 180;
    };);
    DBUG_EXECUTE_IF("dbug.calculate_sbm_after_fake_rotate_log_event", {
      /* Fake that thread started 3 minutes ago */
      thd->start_time.tv_sec -= 180;
    };);
    mysql_mutex_lock(&mi->run_lock);
    mi->slave_running = MYSQL_SLAVE_RUN_CONNECT;
    mysql_mutex_unlock(&mi->run_lock);

    THD_STAGE_INFO(thd, stage_checking_source_version);
    ret = get_master_version_and_clock(mysql, mi);
    if (!ret) {
      ret = get_master_uuid(mysql, mi);
    }
    if (!ret) ret = io_thread_init_commands(mysql, mi);

    quorum_status = Async_conn_failover_manager::SourceQuorumStatus::no_error;
    if (!ret && mi->is_source_connection_auto_failover()) {
      quorum_status =
          Async_conn_failover_manager::get_source_quorum_status(mysql, mi);
      switch (quorum_status) {
        case Async_conn_failover_manager::SourceQuorumStatus::fatal_error:
        case Async_conn_failover_manager::SourceQuorumStatus::no_quorum_error:
          ret = 1;
          break;
        case Async_conn_failover_manager::SourceQuorumStatus::
            transient_network_error:
          ret = 2;
          break;
        case Async_conn_failover_manager::SourceQuorumStatus::no_error:
        default:
          break;
      }
    }

    if (DBUG_EVALUATE_IF("simulate_reconnect_after_failed_registration", 1,
                         0)) {
      ret = 2;
    }

    switch (ret) {
      case 0: {
        if (mi && mi->is_auto_position()) {
          LogErr(SYSTEM_LEVEL,
                 ER_RPL_REPLICA_CONNECTED_TO_SOURCE_RPL_STARTED_GTID_BASED,
                 mi->get_for_channel_str(), mi->get_user(), mi->host, mi->port,
                 mi->master_uuid, mi->master_id);
        } else {
          LogErr(SYSTEM_LEVEL,
                 ER_RPL_REPLICA_CONNECTED_TO_SOURCE_RPL_STARTED_FILE_BASED,
                 mi->get_for_channel_str(), mi->get_user(), mi->host, mi->port,
                 mi->master_uuid, mi->master_id, mi->get_io_rpl_log_name(),
                 llstr(mi->get_master_log_pos(), llbuff));
        }
        break;
      }
      case 1:
        /* Fatal error */
        goto err;
        break;
      case 2: {
        if (check_io_slave_killed(
                mi->info_thd, mi,
                "Replica I/O thread killed "
                "while calling get_master_version_and_clock(...)"))
          goto err;
        suppress_warnings = false;
        /* Try to reconnect because the error was caused by a transient network
         * problem */
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages_after_failed_registration))
          goto err;
        goto connected;
        break;
      }
    }

    /*
      Register ourselves with the master.
    */
    THD_STAGE_INFO(thd, stage_registering_replica_on_source);
    if (register_slave_on_master(mysql, mi, &suppress_warnings)) {
      if (!check_io_slave_killed(thd, mi,
                                 "Replica I/O thread killed "
                                 "while registering replica on source")) {
        LogErr(ERROR_LEVEL, ER_RPL_REPLICA_IO_THREAD_CANT_REGISTER_ON_SOURCE);
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages_after_failed_registration))
          goto err;
      } else
        goto err;
      goto connected;
    }

    DBUG_PRINT("info", ("Starting reading binary log from source"));
    while (!io_slave_killed(thd, mi)) {
      MYSQL_RPL rpl;

      THD_STAGE_INFO(thd, stage_requesting_binlog_dump);
      if (request_dump(thd, mysql, &rpl, mi, &suppress_warnings) ||
          DBUG_EVALUATE_IF("simulate_reconnect_after_failed_binlog_dump", 1,
                           0)) {
        LogErr(ERROR_LEVEL, ER_RPL_REPLICA_ERROR_REQUESTING_BINLOG_DUMP,
               mi->get_for_channel_str());
        if (check_io_slave_killed(thd, mi,
                                  "Replica I/O thread killed while "
                                  "requesting source dump") ||
            try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages_after_failed_dump))
          goto err;
        goto connected;
      }
      const char *event_buf;

      assert(mi->last_error().number == 0);

      while (!io_slave_killed(thd, mi)) {
        ulong event_len;
        /*
           We say "waiting" because read_event() will wait if there's nothing to
           read. But if there's something to read, it will not wait. The
           important thing is to not confuse users by saying "reading" whereas
           we're in fact receiving nothing.
        */
        THD_STAGE_INFO(thd, stage_waiting_for_source_to_send_event);
        event_len = read_event(mysql, &rpl, mi, &suppress_warnings);
        if (check_io_slave_killed(thd, mi,
                                  "Replica I/O thread killed while "
                                  "reading event"))
          goto err;

        if (event_len == packet_error ||
            DBUG_EVALUATE_IF("simulate_reconnect_after_failed_event_read", 1,
                             0)) {
          uint mysql_error_number = mysql_errno(mysql);
          switch (mysql_error_number) {
            case CR_NET_PACKET_TOO_LARGE:
              LogErr(ERROR_LEVEL,
                     ER_RPL_LOG_ENTRY_EXCEEDS_REPLICA_MAX_ALLOWED_PACKET,
                     replica_max_allowed_packet);
              mi->report(ERROR_LEVEL, ER_SERVER_NET_PACKET_TOO_LARGE, "%s",
                         "Got a packet bigger than "
                         "'replica_max_allowed_packet' bytes");
              goto err;
            case ER_SOURCE_FATAL_ERROR_READING_BINLOG:
              mi->report(ERROR_LEVEL,
                         ER_SERVER_SOURCE_FATAL_ERROR_READING_BINLOG,
                         ER_THD(thd, ER_SOURCE_FATAL_ERROR_READING_BINLOG),
                         mysql_error_number, mysql_error(mysql));
              goto err;
            case ER_OUT_OF_RESOURCES:
              LogErr(ERROR_LEVEL, ER_RPL_REPLICA_STOPPING_AS_SOURCE_OOM);
              mi->report(ERROR_LEVEL, ER_SERVER_OUT_OF_RESOURCES, "%s",
                         ER_THD(thd, ER_SERVER_OUT_OF_RESOURCES));
              goto err;
          }
          if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                               reconnect_messages_after_failed_event_read))
            goto err;
          goto connected;
        }  // if (event_len == packet_error)

        retry_count = 0;  // ok event, reset retry counter
        THD_STAGE_INFO(thd, stage_queueing_source_event_to_the_relay_log);
        event_buf = (const char *)mysql->net.read_pos + 1;
        [[maybe_unused]] auto &ev_type = event_buf[EVENT_TYPE_OFFSET];
        DBUG_PRINT("info", ("IO thread received event of type %s",
                            Log_event::get_type_str(ev_type)));
        if (RUN_HOOK(binlog_relay_io, after_read_event,
                     (thd, mi, (const char *)mysql->net.read_pos + 1, event_len,
                      &event_buf, &event_len))) {
          mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                     ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                     "Failed to run 'after_read_event' hook");
          goto err;
        }

        /* XXX: 'synced' should be updated by queue_event to indicate
           whether event has been synced to disk */
        bool synced = false;
#ifndef NDEBUG
        bool was_in_trx = false;
        if (mi->is_queueing_trx()) {
          was_in_trx = true;
          DBUG_EXECUTE_IF("rpl_ps_tables_queue", {
            rpl_replica_debug_point(DBUG_RPL_S_PS_TABLE_QUEUE);
          };);
        }
#endif
        std::size_t queued_size = event_len;
        if (Log_event_type_helper::is_any_gtid_event(
                static_cast<Log_event_type>(event_buf[EVENT_TYPE_OFFSET]))) {
          mysql_mutex_lock(rli->relay_log.get_log_lock());
          Gtid_log_event gtid_ev(event_buf, mi->get_mi_description_event());
          mysql_mutex_unlock(rli->relay_log.get_log_lock());
          if (!gtid_ev.is_valid()) {
            mi->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_WRITE_FAILURE,
                       ER_THD(thd, ER_REPLICA_RELAY_LOG_WRITE_FAILURE),
                       "could not queue event from source");
            goto err;
          }
          queued_size = gtid_ev.get_trx_length();
        }
        // allow waiting only if we are outside of a transaction
        if (rli->log_space_limit && exceeds_relay_log_limit(rli, queued_size) &&
            !mi->transaction_parser.is_inside_transaction()) {
          if (wait_for_relay_log_space(rli, queued_size)) {
            LogErr(
                ERROR_LEVEL,
                ER_RPL_REPLICA_IO_THREAD_ABORTED_WAITING_FOR_RELAY_LOG_SPACE);
            goto err;
          }
        }

        QUEUE_EVENT_RESULT queue_res = queue_event(mi, event_buf, event_len);
        if (queue_res == QUEUE_EVENT_ERROR_QUEUING) {
          mi->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_WRITE_FAILURE,
                     ER_THD(thd, ER_REPLICA_RELAY_LOG_WRITE_FAILURE),
                     "could not queue event from source");
          goto err;
        }

#ifndef NDEBUG
        if (was_in_trx && !mi->is_queueing_trx()) {
          DBUG_EXECUTE_IF("rpl_ps_tables",
                          { rpl_replica_debug_point(DBUG_RPL_S_PS_TABLES); };);
        }
#endif
        if (RUN_HOOK(binlog_relay_io, after_queue_event,
                     (thd, mi, event_buf, event_len, synced))) {
          mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                     ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                     "Failed to run 'after_queue_event' hook");
          goto err;
        }

        /* The event was queued, but there was a failure flushing master info */
        if (queue_res == QUEUE_EVENT_ERROR_FLUSHING_INFO) {
          mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                     ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                     "Failed to flush connection metadata.");
          goto err;
        }

        assert(queue_res == QUEUE_EVENT_OK);
        /*
          Pause the IO thread execution and wait for
          'continue_after_queue_event' signal to continue IO thread
          execution.
        */
        DBUG_EXECUTE_IF("pause_after_queue_event", {
          rpl_replica_debug_point(DBUG_RPL_S_PAUSE_AFTER_QUEUE_EV);
        };);

        /*
          See if the relay logs take too much space.
        */
#ifndef NDEBUG
        {
          char llbuf1[22], llbuf2[22];
          DBUG_PRINT("info", ("log_space_limit=%s log_space_total=%s ",
                              llstr(rli->log_space_limit, llbuf1),
                              llstr(rli->log_space_total, llbuf2)));
        }
#endif

        DBUG_EXECUTE_IF("rpl_set_relay_log_limits", {
          rli->log_space_limit = 10;
          rli->log_space_total = 20;
        };);

        DBUG_EXECUTE_IF("flush_after_reading_user_var_event", {
          if (ev_type == mysql::binlog::event::USER_VAR_EVENT)
            rpl_replica_debug_point(DBUG_RPL_S_FLUSH_AFTER_USERV_EV);
        });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_gtid_log_event",
            if (Log_event_type_helper::is_assigned_gtid_event(
                    static_cast<Log_event_type>(ev_type))) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_query_log_event",
            if (ev_type == mysql::binlog::event::QUERY_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_user_var_log_event",
            if (ev_type == mysql::binlog::event::USER_VAR_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_table_map_event",
            if (ev_type == mysql::binlog::event::TABLE_MAP_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_xid_log_event",
            if (ev_type == mysql::binlog::event::XID_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_write_rows_log_event",
            if (ev_type == mysql::binlog::event::WRITE_ROWS_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF(
            "stop_io_after_reading_unknown_event",
            if (ev_type >= mysql::binlog::event::ENUM_END_EVENT) {
              thd->killed = THD::KILLED_NO_VALUE;
            });
        DBUG_EXECUTE_IF("stop_io_after_queuing_event",
                        { thd->killed = THD::KILLED_NO_VALUE; });
        /*
          After event is flushed to relay log file, memory used
          by thread's mem_root is not required any more.
          Hence adding ClearorReuse() to do the
          cleanup, otherwise a long running IO thread can
          cause OOM error.
        */
        thd->mem_root->ClearForReuse();
      }
    }

    // error = 0;
  err:
    /*
      If source_connection_auto_failover (async connection failover) is
      enabled, this server is not a Group Replication SECONDARY and
      Replica IO thread is not killed but failed due to network error, a
      connection to another source is attempted.
    */
    if (mi->is_source_connection_auto_failover() &&
        !is_group_replication_member_secondary() && !io_slave_killed(thd, mi) &&
        (mi->is_network_error() ||
         quorum_status !=
             Async_conn_failover_manager::SourceQuorumStatus::no_error)) {
      DBUG_EXECUTE_IF("async_conn_failover_crash", DBUG_SUICIDE(););

      /*
        Channel connection details (host, port) values can change after
        call to Async_conn_failover_manager::do_auto_conn_failover() function
        to get the next available sender.
      */
      std::string old_user(mi->get_user());
      std::string old_host(mi->host);
      uint old_port = mi->port;

      /*
        Get the sender to connect to.
        If there is a STOP REPLICA ongoing for any channel, that is, a
        channel_map lock cannot be acquired by this channel IO thread,
        then this channel IO thread does skip the next sender selection.
      */
      Async_conn_failover_manager::DoAutoConnFailoverError update_source_error =
          Async_conn_failover_manager::DoAutoConnFailoverError::retriable_error;
      if (!channel_map.tryrdlock()) {
        update_source_error =
            Async_conn_failover_manager::do_auto_conn_failover(mi, false);
        channel_map.unlock();
      }
      DBUG_EXECUTE_IF("replica_retry_count_exceed", {
        if (Async_conn_failover_manager::DoAutoConnFailoverError::no_error ==
            update_source_error) {
          rpl_replica_debug_point(DBUG_RPL_S_RETRY_COUNT_EXCEED, thd);
        }
      });

      if (Async_conn_failover_manager::DoAutoConnFailoverError::
              no_sources_error != update_source_error) {
        /* Wait before reconnect to avoid resources starvation. */
        my_sleep(1000000);

        /* After waiting, recheck that a STOP REPLICA did not happen. */
        if (!check_io_slave_killed(
                thd, mi,
                "Replica I/O thread killed while "
                "attempting asynchronous connection failover")) {
          /* Reconnect. */
          if (mysql) {
            LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_NEXT_FAILOVER_CHANNEL_SELECTED,
                   mi->retry_count, old_user.c_str(), old_host.c_str(),
                   old_port, mi->get_for_channel_str(), mi->get_user(),
                   mi->host, mi->port);
            thd->clear_active_vio();
            mysql_close(mysql);
            mi->mysql = nullptr;
            mysql = nullptr;
          }
          goto connect_init;
        }
      }
    }

    // print the current replication position
    LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_IO_THREAD_EXITING,
           mi->get_for_channel_str(), mi->get_io_rpl_log_name(),
           llstr(mi->get_master_log_pos(), llbuff));
    /* At this point the I/O thread will not try to reconnect anymore. */
    mi->atomic_is_stopping = true;
    (void)RUN_HOOK(binlog_relay_io, thread_stop, (thd, mi));
    /*
      Pause the IO thread and wait for 'continue_to_stop_io_thread'
      signal to continue to shutdown the IO thread.
    */
    DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
      rpl_replica_debug_point(DBUG_RPL_S_PAUSE_AFTER_IO_STOP, thd);
    };);

    thd->reset_query();
    thd->reset_db(NULL_CSTR);
    if (mysql) {
      /*
        Here we need to clear the active VIO before closing the
        connection with the master.  The reason is that THD::awake()
        might be called from terminate_slave_thread() because somebody
        issued a STOP REPLICA.  If that happends, the shutdown_active_vio()
        can be called in the middle of closing the VIO associated with
        the 'mysql' object, causing a crash.
      */
      thd->clear_active_vio();
      mysql_close(mysql);
      mi->mysql = nullptr;
      mysql = nullptr;
    }
    write_ignored_events_info_to_relay_log(thd, mi);
    THD_STAGE_INFO(thd, stage_waiting_for_replica_mutex_on_exit);
    mysql_mutex_lock(&mi->run_lock);
    /*
      Clean information used to start replica in order to avoid
      security issues.
    */
    mi->reset_start_info();
    /* Forget the relay log's format */
    mysql_mutex_lock(rli->relay_log.get_log_lock());
    mi->set_mi_description_event(nullptr);
    mysql_mutex_unlock(rli->relay_log.get_log_lock());

    // destructor will not free it, because net.vio is 0
    thd->get_protocol_classic()->end_net();

    thd->release_resources();
    THD_CHECK_SENTRY(thd);
    if (thd_added) thd_manager->remove_thd(thd);

    mi->abort_slave = false;
    mi->slave_running = 0;
    mi->atomic_is_stopping = false;
    mysql_mutex_lock(&mi->info_thd_lock);
    mi->info_thd = nullptr;
    mysql_mutex_unlock(&mi->info_thd_lock);

    /*
      The thd can only be destructed after indirect references
      through mi->info_thd are cleared: mi->info_thd= NULL.

      For instance, user thread might be issuing show_slave_status
      and attempting to read mi->info_thd->proc_info().
      Therefore thd must only be deleted after info_thd is set
      to NULL.
    */
    mysql_thread_set_psi_THD(nullptr);
    delete thd;

    /*
      Note: the order of the two following calls (first broadcast, then unlock)
      is important. Otherwise a killer_thread can execute between the calls and
      delete the mi structure leading to a crash! (see BUG#25306 for details)
     */
    mysql_cond_broadcast(&mi->stop_cond);  // tell the world we are done
    DBUG_EXECUTE_IF("simulate_replica_delay_at_terminate_bug38694", sleep(5););
    mysql_mutex_unlock(&mi->run_lock);
  }
  my_thread_end();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(nullptr);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  my_thread_exit(nullptr);
  return (nullptr);  // Avoid compiler warnings
}

/*
  Check the temporary directory used by commands like
  LOAD DATA INFILE.
 */
static int check_temp_dir(char *tmp_file, const char *channel_name) {
  int fd;
  MY_DIR *dirp;
  char tmp_dir[FN_REFLEN];
  size_t tmp_dir_size;

  DBUG_TRACE;

  /*
    Get the directory from the temporary file.
  */
  dirname_part(tmp_dir, tmp_file, &tmp_dir_size);

  /*
    Check if the directory exists.
   */
  if (!(dirp = my_dir(tmp_dir, MYF(MY_WME)))) return 1;
  my_dirend(dirp);

  /*
    Check permissions to create a file.
   */
  // append the server UUID to the temp file name.
  constexpr uint size_of_tmp_file_name = 768;
  static_assert(size_of_tmp_file_name >= FN_REFLEN + TEMP_FILE_MAX_LEN, "");
  char *unique_tmp_file_name = (char *)my_malloc(
      key_memory_rpl_replica_check_temp_dir, size_of_tmp_file_name, MYF(0));
  /*
    In the case of Multisource replication, the file create
    sometimes fail because of there is a race that a second SQL
    thread might create the same file and the creation fails.
    TO overcome this, we add a channel name to get a unique file name.
  */

  /* @TODO: dangerous. Prevent this buffer flow */
  snprintf(unique_tmp_file_name, size_of_tmp_file_name, "%s%s%s", tmp_file,
           channel_name, server_uuid);
  if ((fd = mysql_file_create(key_file_misc, unique_tmp_file_name, CREATE_MODE,
                              O_WRONLY | O_EXCL | O_NOFOLLOW, MYF(MY_WME))) < 0)
    return 1;

  /*
    Clean up.
   */
  mysql_file_close(fd, MYF(0));

  mysql_file_delete(key_file_misc, unique_tmp_file_name, MYF(0));
  my_free(unique_tmp_file_name);
  return 0;
}

/*
  Worker thread for the parallel execution of the replication events.
*/
extern "C" {
static void *handle_slave_worker(void *arg) {
  THD *thd; /* needs to be first for thread_stack */
  bool thd_added = false;
  int error = 0;
  Slave_worker *w = (Slave_worker *)arg;
  Relay_log_info *rli = w->c_rli;
  ulong purge_cnt = 0;
  ulonglong purge_size = 0;
  struct slave_job_item _item, *job_item = &_item;
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi;
#endif

  my_thread_init();
  DBUG_TRACE;

  thd = new THD;
  if (!thd) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_INITIALIZE_REPLICA_WORKER,
           rli->get_for_channel_str());
    goto err;
  }
  mysql_mutex_lock(&w->info_thd_lock);
  w->info_thd = thd;
  mysql_mutex_unlock(&w->info_thd_lock);
  thd->thread_stack = (char *)&thd;

#ifdef HAVE_PSI_THREAD_INTERFACE
  // save the instrumentation for worker thread in w->info_thd
  psi = PSI_THREAD_CALL(get_thread)();
  thd_set_psi(w->info_thd, psi);
#endif
  mysql_thread_set_psi_THD(thd);

  if (init_replica_thread(thd, SLAVE_THD_WORKER)) {
    // todo make SQL thread killed
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_CANT_INITIALIZE_REPLICA_WORKER,
           rli->get_for_channel_str());
    goto err;
  }
  thd->rli_slave = w;
  thd->init_query_mem_roots();

  if (channel_map.is_group_replication_applier_channel_name(
          rli->get_channel())) {
    thd->rpl_thd_ctx.set_rpl_channel_type(GR_APPLIER_CHANNEL);
  } else if (channel_map.is_group_replication_recovery_channel_name(
                 rli->get_channel())) {
    thd->rpl_thd_ctx.set_rpl_channel_type(GR_RECOVERY_CHANNEL);
  } else {
    thd->rpl_thd_ctx.set_rpl_channel_type(RPL_STANDARD_CHANNEL);
  }

  w->set_filter(rli->rpl_filter);

  if ((w->deferred_events_collecting = w->rpl_filter->is_on()))
    w->deferred_events = new Deferred_log_events();
  assert(thd->rli_slave->info_thd == thd);

  /* Set applier thread InnoDB priority */
  set_thd_tx_priority(thd, rli->get_thd_tx_priority());
  /* Set write set related options */
  set_thd_write_set_options(thd, rli->get_ignore_write_set_memory_limit(),
                            rli->get_allow_drop_write_set());

  thd->variables.require_row_format = rli->is_row_format_required();

  if (Relay_log_info::PK_CHECK_STREAM !=
      rli->get_require_table_primary_key_check())
    thd->variables.sql_require_primary_key =
        (rli->get_require_table_primary_key_check() ==
         Relay_log_info::PK_CHECK_ON);
  w->set_require_table_primary_key_check(
      rli->get_require_table_primary_key_check());

  thd->variables.sql_generate_invisible_primary_key = false;
  if (thd->rpl_thd_ctx.get_rpl_channel_type() != GR_APPLIER_CHANNEL &&
      thd->rpl_thd_ctx.get_rpl_channel_type() != GR_RECOVERY_CHANNEL &&
      Relay_log_info::PK_CHECK_GENERATE ==
          rli->get_require_table_primary_key_check()) {
    thd->variables.sql_generate_invisible_primary_key = true;
  }

  thd->variables.restrict_fk_on_non_standard_key = false;

  thd_manager->add_thd(thd);
  thd_added = true;

  if (w->update_is_transactional()) {
    rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                "Error checking if the worker repository is transactional.");
    goto err;
  }

  if (rli->get_commit_order_manager() != nullptr)
    rli->get_commit_order_manager()->init_worker_context(
        *w);  // Initialize worker context within Commit_order_manager

  mysql_mutex_lock(&w->jobs_lock);
  w->running_status = Slave_worker::RUNNING;
  mysql_cond_signal(&w->jobs_cond);

  mysql_mutex_unlock(&w->jobs_lock);

  assert(thd->is_slave_error == 0);

  // No need to report anything, all error handling will be performed in the
  // slave SQL thread.
  if (!rli->check_privilege_checks_user())
    rli->initialize_security_context(w->info_thd);  // Worker security context
                                                    // initialization with
                                                    // `PRIVILEGE_CHECKS_USER`

  while (!error) {
    error = slave_worker_exec_job_group(w, rli);
  }

  /*
     Cleanup after an error requires clear_error() go first.
     Otherwise assert(!all) in binlog_rollback()
  */
  thd->clear_error();
  w->cleanup_context(thd, error);

  mysql_mutex_lock(&w->jobs_lock);

  while (w->jobs.de_queue(job_item)) {
    purge_cnt++;
    purge_size += job_item->data->common_header->data_written;
    assert(job_item->data);
    delete job_item->data;
  }

  assert(w->jobs.get_length() == 0);

  mysql_mutex_unlock(&w->jobs_lock);

  mysql_mutex_lock(&rli->pending_jobs_lock);
  rli->pending_jobs -= purge_cnt;
  rli->mts_pending_jobs_size -= purge_size;
  assert(rli->mts_pending_jobs_size < rli->mts_pending_jobs_size_max);

  mysql_mutex_unlock(&rli->pending_jobs_lock);

  /*
     In MTS case cleanup_after_session() has be called explicitly.
     TODO: to make worker thd be deleted before Slave_worker instance.
  */
  if (thd->rli_slave) {
    w->cleanup_after_session();
    thd->rli_slave = nullptr;
  }
  mysql_mutex_lock(&w->jobs_lock);
  w->running_status = Slave_worker::NOT_RUNNING;
  mysql_cond_signal(&w->jobs_cond);  // famous last goodbye

  mysql_mutex_unlock(&w->jobs_lock);

err:

  if (thd) {
    /*
       The slave code is very bad. Notice that it is missing
       several clean up calls here. I've just added what was
       necessary to avoid valgrind errors.

       /Alfranio
    */
    thd->get_protocol_classic()->end_net();

    /*
      to avoid close_temporary_tables() closing temp tables as those
      are Coordinator's burden.
    */
    thd->system_thread = NON_SYSTEM_THREAD;
    thd->release_resources();

    THD_CHECK_SENTRY(thd);
    if (thd_added) thd_manager->remove_thd(thd);
    mysql_thread_set_psi_THD(nullptr);
    delete thd;
  }

  my_thread_end();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(nullptr);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  my_thread_exit(nullptr);
  return nullptr;
}
}  // extern "C"

/**
   Orders jobs by comparing relay log information.
*/

int mts_event_coord_cmp(LOG_POS_COORD *id1, LOG_POS_COORD *id2) {
  longlong filecmp = strcmp(id1->file_name, id2->file_name);
  longlong poscmp = id1->pos - id2->pos;
  return (filecmp < 0
              ? -1
              : (filecmp > 0 ? 1 : (poscmp < 0 ? -1 : (poscmp > 0 ? 1 : 0))));
}

bool mts_recovery_groups(Relay_log_info *rli) {
  Log_event *ev = nullptr;
  bool is_error = false;
  bool flag_group_seen_begin = false;
  uint recovery_group_cnt = 0;
  bool not_reached_commit = true;

  // Value-initialization, to avoid compiler warnings on push_back.
  Slave_job_group job_worker = Slave_job_group();

  Log_info linfo;
  my_off_t offset = 0;
  MY_BITMAP *groups = &rli->recovery_groups;
  THD *thd = current_thd;

  DBUG_TRACE;

  assert(rli->replica_parallel_workers == 0);

  /*
     Although mts_recovery_groups() is reentrant it returns
     early if the previous invocation raised any bit in
     recovery_groups bitmap.
  */
  if (rli->is_mts_recovery()) return false;

  /*
    The process of relay log recovery for the multi threaded applier
    is focused on marking transactions as already executed so they are
    skipped when the SQL thread applies them.
    This is important as the position stored for the last executed relay log
    position may be behind what transactions workers already handled.
    When GTID_MODE=ON however we can use the old relay log position, even if
    stale as applied transactions will be skipped due to GTIDs auto skip
    feature.
  */
  if (global_gtid_mode.get() == Gtid_mode::ON && rli->mi &&
      rli->mi->is_auto_position()) {
    rli->mts_recovery_group_cnt = 0;
    return false;
  }

  /*
    Save relay log position to compare with worker's position.
  */
  LOG_POS_COORD cp = {const_cast<char *>(rli->get_group_master_log_name()),
                      rli->get_group_master_log_pos()};

  /*
    Gathers information on valuable workers and stores it in
    above_lwm_jobs in asc ordered by the master binlog coordinates.
  */
  Prealloced_array<Slave_job_group, 16> above_lwm_jobs(PSI_NOT_INSTRUMENTED);
  above_lwm_jobs.reserve(rli->recovery_parallel_workers);

  /*
    When info tables are used and autocommit= 0 we force a new
    transaction start to avoid table access deadlocks when START REPLICA
    is executed after STOP REPLICA with MTS enabled.
  */
  if (is_autocommit_off(thd))
    if (trans_begin(thd)) goto err;

  for (uint id = 0; id < rli->recovery_parallel_workers; id++) {
    Slave_worker *worker =
        Rpl_info_factory::create_worker(INFO_REPOSITORY_TABLE, id, rli, true);

    if (!worker) {
      if (is_autocommit_off(thd)) trans_rollback(thd);
      goto err;
    }

    LOG_POS_COORD w_last = {
        const_cast<char *>(worker->get_group_master_log_name()),
        worker->get_group_master_log_pos()};
    if (mts_event_coord_cmp(&w_last, &cp) > 0) {
      /*
        Inserts information into a dynamic array for further processing.
        The jobs/workers are ordered by the last checkpoint positions
        workers have seen.
      */
      job_worker.worker = worker;
      job_worker.checkpoint_log_pos = worker->checkpoint_master_log_pos;
      job_worker.checkpoint_log_name = worker->checkpoint_master_log_name;

      above_lwm_jobs.push_back(job_worker);
    } else {
      /*
        Deletes the worker because its jobs are included in the latest
        checkpoint.
      */
      delete worker;
    }
  }

  /*
    When info tables are used and autocommit= 0 we force transaction
    commit to avoid table access deadlocks when START REPLICA is executed
    after STOP REPLICA with MTS enabled.
  */
  if (is_autocommit_off(thd))
    if (trans_commit(thd)) goto err;

  /*
    In what follows, the group Recovery Bitmap is constructed.

     seek(lwm);

     while(w= next(above_lwm_w))
       do
         read G
         if G == w->last_comm
           w.B << group_cnt++;
           RB |= w.B;
            break;
         else
           group_cnt++;
        while(!eof);
        continue;
  */
  assert(!rli->recovery_groups_inited);

  if (!above_lwm_jobs.empty()) {
    bitmap_init(groups, nullptr, MTS_MAX_BITS_IN_GROUP);
    rli->recovery_groups_inited = true;
    bitmap_clear_all(groups);
  }
  rli->mts_recovery_group_cnt = 0;
  for (Slave_job_group *jg = above_lwm_jobs.begin(); jg != above_lwm_jobs.end();
       ++jg) {
    Slave_worker *w = jg->worker;
    LOG_POS_COORD w_last = {const_cast<char *>(w->get_group_master_log_name()),
                            w->get_group_master_log_pos()};

    LogErr(INFORMATION_LEVEL,
           ER_RPL_MTA_GROUP_RECOVERY_APPLIER_METADATA_FOR_WORKER, w->id,
           w->get_group_relay_log_name(), w->get_group_relay_log_pos(),
           w->get_group_master_log_name(), w->get_group_master_log_pos());

    recovery_group_cnt = 0;
    not_reached_commit = true;
    if (rli->relay_log.find_log_pos(&linfo, rli->get_group_relay_log_name(),
                                    true)) {
      LogErr(ERROR_LEVEL, ER_RPL_ERROR_LOOKING_FOR_LOG,
             rli->get_group_relay_log_name());
      goto err;
    }
    offset = rli->get_group_relay_log_pos();

    Relaylog_file_reader relaylog_file_reader(opt_replica_sql_verify_checksum);

    while (not_reached_commit) {
      if (relaylog_file_reader.open(linfo.log_file_name, offset)) {
        LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
               relaylog_file_reader.get_error_str());
        goto err;
      }

      while (not_reached_commit &&
             (ev = relaylog_file_reader.read_event_object())) {
        assert(ev->is_valid());

        if (ev->get_type_code() == mysql::binlog::event::ROTATE_EVENT ||
            ev->get_type_code() ==
                mysql::binlog::event::FORMAT_DESCRIPTION_EVENT ||
            ev->get_type_code() ==
                mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
          delete ev;
          ev = nullptr;
          continue;
        }

        DBUG_PRINT(
            "mta",
            ("Event Recoverying relay log info "
             "group_mster_log_name %s, event_source_log_pos %llu type code %u.",
             linfo.log_file_name, ev->common_header->log_pos,
             ev->get_type_code()));

        if (ev->starts_group()) {
          flag_group_seen_begin = true;
        } else if ((ev->ends_group() || !flag_group_seen_begin) &&
                   !is_any_gtid_event(ev)) {
          int ret = 0;
          LOG_POS_COORD ev_coord = {
              const_cast<char *>(rli->get_group_master_log_name()),
              ev->common_header->log_pos};
          flag_group_seen_begin = false;
          recovery_group_cnt++;

          LogErr(INFORMATION_LEVEL, ER_RPL_MTA_GROUP_RECOVERY_APPLIER_METADATA,
                 rli->get_group_master_log_name_info(),
                 ev->common_header->log_pos);
          if ((ret = mts_event_coord_cmp(&ev_coord, &w_last)) == 0) {
#ifndef NDEBUG
            for (uint i = 0; i <= w->worker_checkpoint_seqno; i++) {
              if (bitmap_is_set(&w->group_executed, i))
                DBUG_PRINT("mta", ("Bit %u is set.", i));
              else
                DBUG_PRINT("mta", ("Bit %u is not set.", i));
            }
#endif
            DBUG_PRINT("mta",
                       ("Doing a shift ini(%lu) end(%lu).",
                        (w->worker_checkpoint_seqno + 1) - recovery_group_cnt,
                        w->worker_checkpoint_seqno));

            for (uint i = (w->worker_checkpoint_seqno + 1) - recovery_group_cnt,
                      j = 0;
                 i <= w->worker_checkpoint_seqno; i++, j++) {
              if (bitmap_is_set(&w->group_executed, i)) {
                DBUG_PRINT("mta", ("Setting bit %u.", j));
                bitmap_test_and_set(groups, j);
              }
            }
            not_reached_commit = false;
          } else
            assert(ret < 0);
        }
        delete ev;
        ev = nullptr;
      }

      relaylog_file_reader.close();
      offset = BIN_LOG_HEADER_SIZE;
      if (not_reached_commit && rli->relay_log.find_next_log(&linfo, true)) {
        LogErr(ERROR_LEVEL, ER_RPL_CANT_FIND_FOLLOWUP_FILE,
               linfo.log_file_name);
        goto err;
      }
    }

    rli->mts_recovery_group_cnt =
        (rli->mts_recovery_group_cnt < recovery_group_cnt
             ? recovery_group_cnt
             : rli->mts_recovery_group_cnt);
  }

  assert(!rli->recovery_groups_inited ||
         rli->mts_recovery_group_cnt <= groups->n_bits);

  goto end;
err:
  is_error = true;
end:

  for (Slave_job_group *jg = above_lwm_jobs.begin(); jg != above_lwm_jobs.end();
       ++jg) {
    delete jg->worker;
  }

  if (rli->mts_recovery_group_cnt == 0) rli->clear_mts_recovery_groups();

  return is_error;
}

bool mta_checkpoint_routine(Relay_log_info *rli, bool force) {
  ulong cnt;
  bool error = false;
  time_t ts = 0;

  DBUG_TRACE;

#ifndef NDEBUG
  if (DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0)) {
    if (!rli->gaq->count_done(rli)) return false;
  }
  DBUG_EXECUTE_IF("mta_checkpoint", {
    rpl_replica_debug_point(DBUG_RPL_S_MTS_CHECKPOINT_START, rli->info_thd);
  };);
#endif

  /*
    rli->checkpoint_group can have two possible values due to
    two possible status of the last (being scheduled) group.
  */
  assert(!rli->gaq->full() ||
         ((rli->rli_checkpoint_seqno == rli->checkpoint_group - 1 &&
           (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP ||
            rli->mts_group_status == Relay_log_info::MTS_KILLED_GROUP)) ||
          rli->rli_checkpoint_seqno == rli->checkpoint_group));

  do {
    if (!is_mts_db_partitioned(rli)) mysql_mutex_lock(&rli->mts_gaq_LOCK);

    cnt = rli->gaq->move_queue_head(&rli->workers);

    if (!is_mts_db_partitioned(rli)) mysql_mutex_unlock(&rli->mts_gaq_LOCK);
#ifndef NDEBUG
    if (DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0) &&
        cnt != opt_mta_checkpoint_period)
      LogErr(ERROR_LEVEL, ER_RPL_MTA_CHECKPOINT_PERIOD_DIFFERS_FROM_CNT);
#endif
  } while (!sql_slave_killed(rli->info_thd, rli) && cnt == 0 && force &&
           !DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0) &&
           (my_sleep(rli->mts_coordinator_basic_nap), 1));
  /*
    This checks how many consecutive jobs where processed.
    If this value is different than zero the checkpoint
    routine can proceed. Otherwise, there is nothing to be
    done.
  */
  if (cnt == 0) goto end;

  /*
     The workers have completed  cnt jobs from the gaq. This means that we
     should increment C->jobs_done by cnt.
   */
  if (!is_mts_worker(rli->info_thd) && !is_mts_db_partitioned(rli)) {
    DBUG_PRINT("info", ("jobs_done this itr=%ld", cnt));
    static_cast<Mts_submode_logical_clock *>(rli->current_mts_submode)
        ->jobs_done += cnt;
  }

  mysql_mutex_lock(&rli->data_lock);

  /*
    "Coordinator::commit_positions"

    rli->gaq->lwm has been updated in move_queue_head() and
    to contain all but rli->group_master_log_name which
    is altered solely by Coordinator at special checkpoints.
  */
  rli->set_group_master_log_pos(rli->gaq->lwm.group_master_log_pos);
  rli->set_group_relay_log_pos(rli->gaq->lwm.group_relay_log_pos);
  DBUG_PRINT(
      "mta",
      ("New checkpoint %llu %llu %s", rli->gaq->lwm.group_master_log_pos,
       rli->gaq->lwm.group_relay_log_pos, rli->gaq->lwm.group_relay_log_name));

  if (rli->gaq->lwm.group_relay_log_name[0] != 0)
    rli->set_group_relay_log_name(rli->gaq->lwm.group_relay_log_name);

  /*
     todo: uncomment notifies when UNTIL will be supported

     rli->notify_group_master_log_name_update();
     rli->notify_group_relay_log_name_update();

     Todo: optimize with if (wait_flag) broadcast
         waiter: set wait_flag; waits....; drops wait_flag;
  */

  error = rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT);

  mysql_cond_broadcast(&rli->data_cond);
  mysql_mutex_unlock(&rli->data_lock);

  /*
    We need to ensure that this is never called at this point when
    cnt is zero. This value means that the checkpoint information
    will be completely reset.
  */

  /*
    Update the rli->last_master_timestamp for reporting correct
    Seconds_behind_source.

    If GAQ is empty, set it to zero.
    Else, update it with the timestamp of the first job of the Slave_job_queue
    which was assigned in the Log_event::get_slave_worker() function.
  */
  ts = rli->gaq->empty()
           ? 0
           : reinterpret_cast<Slave_job_group *>(rli->gaq->head_queue())->ts;
  rli->reset_notified_checkpoint(cnt, ts, true);
  /* end-of "Coordinator::commit_positions" */

end:
  error = error || rli->info_thd->killed != THD::NOT_KILLED;
#ifndef NDEBUG
  if (DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0)) DBUG_SUICIDE();
  DBUG_EXECUTE_IF("mta_checkpoint", {
    rpl_replica_debug_point(DBUG_RPL_S_MTS_CHECKPOINT_END, rli->info_thd);
  };);
#endif
  set_timespec_nsec(&rli->last_clock, 0);

  return error;
}

/**
   Instantiation of a Slave_worker and forking out a single Worker thread.

   @param  rli  Coordinator's Relay_log_info pointer
   @param  i    identifier of the Worker

   @return 0 suppress or 1 if fails
*/
static int slave_start_single_worker(Relay_log_info *rli, ulong i) {
  int error = 0;
  my_thread_handle th;
  Slave_worker *w = nullptr;

  mysql_mutex_assert_owner(&rli->run_lock);

  if (!(w = Rpl_info_factory::create_worker(INFO_REPOSITORY_TABLE, i, rli,
                                            false))) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_WORKER_THREAD_CREATION_FAILED,
           rli->get_for_channel_str());
    error = 1;
    goto err;
  }

  if (w->init_worker(rli, i)) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_WORKER_THREAD_CREATION_FAILED,
           rli->get_for_channel_str());
    error = 1;
    goto err;
  }

  // We assume that workers are added in sequential order here.
  assert(i == rli->workers.size());
  if (i >= rli->workers.size()) rli->workers.resize(i + 1);
  rli->workers[i] = w;

  if (DBUG_EVALUATE_IF("mta_worker_thread_fails", i == 1, 0) ||
      (error = mysql_thread_create(key_thread_replica_worker, &th,
                                   &connection_attrib, handle_slave_worker,
                                   (void *)w))) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_WORKER_THREAD_CREATION_FAILED_WITH_ERRNO,
           rli->get_for_channel_str(), error);
    error = 1;
    goto err;
  }

  mysql_mutex_lock(&w->jobs_lock);
  if (w->running_status == Slave_worker::NOT_RUNNING)
    mysql_cond_wait(&w->jobs_cond, &w->jobs_lock);
  mysql_mutex_unlock(&w->jobs_lock);

err:
  if (error && w) {
    // Free the current submode object
    delete w->current_mts_submode;
    w->current_mts_submode = nullptr;
    delete w;
    /*
      Any failure after array inserted must follow with deletion
      of just created item.
    */
    if (rli->workers.size() == i + 1) rli->workers.erase(i);
  }
  return error;
}

/**
   Initialization of the central rli members for Coordinator's role,
   communication channels such as Assigned Partition Hash (APH),
   and starting the Worker pool.

   @param rli             Pointer to Coordinator's Relay_log_info instance.
   @param n               Number of configured Workers in the upcoming session.
   @param[out] mts_inited If the initialization processed was started.

   @return 0         success
           non-zero  as failure
*/
static int slave_start_workers(Relay_log_info *rli, ulong n, bool *mts_inited) {
  int error = 0;
  /**
    gtid_monitoring_info must be cleared when MTS is enabled or
    workers_copy_pfs has elements
  */
  bool clear_gtid_monitoring_info = false;
  int64_t order_commit_wait_count{0};
  int64_t order_commit_waited_time{0};

  mysql_mutex_assert_owner(&rli->run_lock);

  if (n == 0 && rli->mts_recovery_group_cnt == 0) {
    rli->workers.clear();
    rli->clear_processing_trx();
    goto end;
  }

  *mts_inited = true;

  /*
    The requested through argument number of Workers can be different
     from the previous time which ended with an error. Thereby
     the effective number of configured Workers is max of the two.
  */
  rli->init_workers(max(n, rli->recovery_parallel_workers));

  rli->last_assigned_worker = nullptr;  // associated with curr_group_assigned

  /*
     GAQ  queue holds seqno:s of scheduled groups. C polls workers in
     @c opt_mta_checkpoint_period to update GAQ (see @c next_event())
     The length of GAQ is set to be equal to checkpoint_group.
     Notice, the size matters for mta_checkpoint_routine's progress loop.
  */

  rli->gaq = new Slave_committed_queue(rli->checkpoint_group, n);
  if (!rli->gaq->inited) return 1;

  // length of WQ is actually constant though can be made configurable
  rli->mts_slave_worker_queue_len_max = mts_slave_worker_queue_len_max;
  rli->mts_pending_jobs_size = 0;
  rli->mts_pending_jobs_size_max = ::opt_mts_pending_jobs_size_max;
  rli->mts_wq_underrun_w_id = MTS_WORKER_UNDEF;
  rli->mts_wq_excess_cnt = 0;
  rli->mts_wq_oversize = false;
  rli->mts_coordinator_basic_nap = mts_coordinator_basic_nap;
  rli->mts_worker_underrun_level = mts_worker_underrun_level;
  rli->curr_group_seen_begin = rli->curr_group_seen_gtid = false;
  rli->curr_group_isolated = false;
  rli->rli_checkpoint_seqno = 0;
  rli->mta_coordinator_has_waited_stat = time(nullptr);
  rli->mts_group_status = Relay_log_info::MTS_NOT_IN_GROUP;
  clear_gtid_monitoring_info = true;

  if (init_hash_workers(rli))  // MTS: mapping_db_to_worker
  {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_FAILED_TO_INIT_PARTITIONS_HASH);
    error = 1;
    goto err;
  }

  for (uint i = 0; i < n; i++) {
    if ((error = slave_start_single_worker(rli, i))) goto err;
    rli->replica_parallel_workers++;
  }

end:
  /*
    Free the buffer that was being used to report worker's status through
    the table performance_schema.table_replication_applier_status_by_worker
    between stop replica and next start replica.
  */
  for (int i = static_cast<int>(rli->workers_copy_pfs.size()) - 1; i >= 0;
       i--) {
    // Don't loose the stats on commit order waits
    order_commit_wait_count += rli->workers_copy_pfs[i]
                                   ->get_worker_metrics()
                                   .get_number_of_waits_on_commit_order();
    order_commit_waited_time += rli->workers_copy_pfs[i]
                                    ->get_worker_metrics()
                                    .get_wait_time_on_commit_order();
    delete rli->workers_copy_pfs[i];
    if (!clear_gtid_monitoring_info) clear_gtid_monitoring_info = true;
  }
  rli->workers_copy_pfs.clear();
  rli->get_applier_metrics().inc_commit_order_wait_stored_metrics(
      order_commit_wait_count, order_commit_waited_time);

  // Effective end of the recovery right now when there is no gaps
  if (!error && rli->mts_recovery_group_cnt == 0) {
    if ((error = rli->mts_finalize_recovery()))
      (void)Rpl_info_factory::reset_workers(rli);
    if (!error)
      error = rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT);
  }

err:
  if (clear_gtid_monitoring_info) rli->clear_gtid_monitoring_info();
  return error;
}

/*
   Ending Worker threads.

   Not in case Coordinator is killed itself, it first waits for
   Workers have finished their assignments, and then updates checkpoint.
   Workers are notified with setting KILLED status
   and waited for their acknowledgment as specified by
   worker's running_status.
   Coordinator finalizes with its MTS running status to reset few objects.
*/
static void slave_stop_workers(Relay_log_info *rli, bool *mts_inited) {
  THD *thd = rli->info_thd;

  if (!*mts_inited)
    return;
  else if (rli->replica_parallel_workers == 0)
    goto end;

  /*
    If request for stop replica is received notify worker
    to stop.
  */
  // Initialize worker exit count and max_updated_index to 0 during each stop.
  rli->exit_counter = 0;
  rli->max_updated_index = (rli->until_condition != Relay_log_info::UNTIL_NONE)
                               ? rli->mts_groups_assigned
                               : 0;
  if (!rli->workers.empty()) {
    for (int i = static_cast<int>(rli->workers.size()) - 1; i >= 0; i--) {
      Slave_worker *w = rli->workers[i];
      struct slave_job_item item = {nullptr, 0, {'\0'}, false};
      struct slave_job_item *job_item = &item;
      mysql_mutex_lock(&w->jobs_lock);

      if (w->running_status != Slave_worker::RUNNING) {
        mysql_mutex_unlock(&w->jobs_lock);
        continue;
      }

      w->running_status = Slave_worker::STOP;
      (void)set_max_updated_index_on_stop(w, job_item);
      mysql_cond_signal(&w->jobs_cond);

      mysql_mutex_unlock(&w->jobs_lock);

      DBUG_PRINT("info", ("Notifying worker %lu%s to exit, thd %p", w->id,
                          w->get_for_channel_str(), w->info_thd));
    }
  }
  thd_proc_info(thd, "Waiting for workers to exit");

  for (Slave_worker **it = rli->workers.begin(); it != rli->workers.end();
       ++it) {
    Slave_worker *w = *it;
    mysql_mutex_lock(&w->jobs_lock);
    while (w->running_status != Slave_worker::NOT_RUNNING) {
      PSI_stage_info old_stage;
      assert(w->running_status == Slave_worker::ERROR_LEAVING ||
             w->running_status == Slave_worker::STOP ||
             w->running_status == Slave_worker::STOP_ACCEPTED);

      thd->ENTER_COND(&w->jobs_cond, &w->jobs_lock,
                      &stage_replica_waiting_workers_to_exit, &old_stage);
      mysql_cond_wait(&w->jobs_cond, &w->jobs_lock);
      mysql_mutex_unlock(&w->jobs_lock);
      thd->EXIT_COND(&old_stage);
      mysql_mutex_lock(&w->jobs_lock);
    }
    mysql_mutex_unlock(&w->jobs_lock);
  }

  for (Slave_worker **it = rli->workers.begin(); it != rli->workers.end();
       ++it) {
    Slave_worker *w = *it;

    /*
      Make copies for reporting through the performance schema tables.
      This is preserved until the next START REPLICA.
    */
    Slave_worker *worker_copy = new Slave_worker(
        nullptr,
#ifdef HAVE_PSI_INTERFACE
        &key_relay_log_info_run_lock, &key_relay_log_info_data_lock,
        &key_relay_log_info_sleep_lock, &key_relay_log_info_thd_lock,
        &key_relay_log_info_data_cond, &key_relay_log_info_start_cond,
        &key_relay_log_info_stop_cond, &key_relay_log_info_sleep_cond,
#endif
        w->id, rli->get_channel());
    worker_copy->copy_values_for_PFS(w->id, w->running_status, w->info_thd,
                                     w->last_error(),
                                     w->get_gtid_monitoring_info());
    worker_copy->copy_worker_metrics(w);
    rli->workers_copy_pfs.push_back(worker_copy);
  }

  /// @todo: consider to propagate an error out of the function
  if (thd->killed == THD::NOT_KILLED) (void)mta_checkpoint_routine(rli, false);

  {
    MUTEX_LOCK(lock, &rli->data_lock);
    while (!rli->workers.empty()) {
      Slave_worker *w = rli->workers.back();
      // Free the current submode object
      delete w->current_mts_submode;
      w->current_mts_submode = nullptr;
      rli->workers.pop_back();
      delete w;
    }
  }

  assert(rli->pending_jobs == 0);
  assert(rli->mts_pending_jobs_size == 0);

end:
  rli->mts_group_status = Relay_log_info::MTS_NOT_IN_GROUP;
  destroy_hash_workers(rli);
  delete rli->gaq;

  // Destroy buffered events of the current group prior to exit.
  for (uint i = 0; i < rli->curr_group_da.size(); i++)
    delete rli->curr_group_da[i].data;
  rli->curr_group_da.clear();  // GCDA

  rli->curr_group_assigned_parts.clear();  // GCAP
  rli->deinit_workers();
  rli->workers_array_initialized = false;
  rli->replica_parallel_workers = 0;

  *mts_inited = false;
}

/**
  Processes the outcome of applying an event, logs it properly if it's an error
  and return the proper error code to trigger.

  @return the error code to bubble up in the execution stack.
 */
static int report_apply_event_error(THD *thd, Relay_log_info *rli) {
  DBUG_TRACE;
  longlong slave_errno = 0;

  /*
    retrieve as much info as possible from the thd and, error
    codes and warnings and print this to the error log as to
    allow the user to locate the error
  */
  uint32 const last_errno = rli->last_error().number;

  if (thd->is_error()) {
    char const *const errmsg = thd->get_stmt_da()->message_text();

    DBUG_PRINT("info", ("thd->get_stmt_da()->get_mysql_errno()=%d; "
                        "rli->last_error.number=%d",
                        thd->get_stmt_da()->mysql_errno(), last_errno));
    if (last_errno == 0) {
      /*
        This function is reporting an error which was not reported
        while executing exec_relay_log_event().
      */
      rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(), "%s", errmsg);
    } else if (last_errno != thd->get_stmt_da()->mysql_errno()) {
      /*
       * An error was reported while executing exec_relay_log_event()
       * however the error code differs from what is in the thread.
       * This function prints out more information to help finding
       * what caused the problem.
       */
      LogErr(ERROR_LEVEL, ER_RPL_REPLICA_ADDITIONAL_ERROR_INFO_FROM_DA, errmsg,
             thd->get_stmt_da()->mysql_errno());
    }
  }

  /* Print any warnings issued */
  Diagnostics_area::Sql_condition_iterator it =
      thd->get_stmt_da()->sql_conditions();
  const Sql_condition *err;
  /*
    Added controlled slave thread cancel for replication
    of user-defined variables.
  */
  bool udf_error = false;
  while ((err = it++)) {
    if (err->mysql_errno() == ER_CANT_OPEN_LIBRARY) udf_error = true;
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_ERROR_INFO_FROM_DA,
           err->message_text(), err->mysql_errno());
  }
  if (udf_error)
    slave_errno = ER_RPL_REPLICA_ERROR_LOADING_USER_DEFINED_LIBRARY;
  else
    slave_errno = ER_RPL_REPLICA_ERROR_RUNNING_QUERY;

  return slave_errno;
}

/**
  Slave SQL thread entry point.

  @param arg Pointer to Relay_log_info object that holds information
  for the SQL thread.

  @return Always 0.
*/
extern "C" void *handle_slave_sql(void *arg) {
  THD *thd; /* needs to be first for thread_stack */
  bool thd_added = false;
  bool main_loop_error = false;
  char llbuff[22], llbuff1[22];
  char saved_log_name[FN_REFLEN];
  char saved_master_log_name[FN_REFLEN];
  my_off_t saved_log_pos = 0;
  my_off_t saved_master_log_pos = 0;
  my_off_t saved_skip = 0;

  Relay_log_info *rli = ((Master_info *)arg)->rli;
  const char *errmsg;
  longlong slave_errno = 0;
  bool mts_inited = false;
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  Commit_order_manager *commit_order_mngr = nullptr;
  Rpl_applier_reader applier_reader(rli);
  Relay_log_info::enum_priv_checks_status priv_check_status =
      Relay_log_info::enum_priv_checks_status::SUCCESS;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  {
    DBUG_TRACE;

    assert(rli->inited);
    mysql_mutex_lock(&rli->run_lock);
    assert(!rli->slave_running);
    errmsg = nullptr;

    thd = new THD;  // note that constructor of THD uses DBUG_ !
    thd->thread_stack = (char *)&thd;  // remember where our stack is
    mysql_mutex_lock(&rli->info_thd_lock);
    rli->info_thd = thd;

#ifdef HAVE_PSI_THREAD_INTERFACE
    // save the instrumentation for SQL thread in rli->info_thd
    struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
    thd_set_psi(rli->info_thd, psi);
#endif
    mysql_thread_set_psi_THD(thd);

    if (rli->channel_mts_submode != MTS_PARALLEL_TYPE_DB_NAME)
      rli->current_mts_submode = new Mts_submode_logical_clock();
    else
      rli->current_mts_submode = new Mts_submode_database();

    // Only use replica preserve commit order if more than 1 worker exists
    if (opt_replica_preserve_commit_order && !rli->is_parallel_exec() &&
        rli->opt_replica_parallel_workers > 1)
      commit_order_mngr =
          new Commit_order_manager(rli->opt_replica_parallel_workers);

    rli->set_commit_order_manager(commit_order_mngr);

    if (channel_map.is_group_replication_applier_channel_name(
            rli->get_channel())) {
      thd->rpl_thd_ctx.set_rpl_channel_type(GR_APPLIER_CHANNEL);
    } else if (channel_map.is_group_replication_recovery_channel_name(
                   rli->get_channel())) {
      thd->rpl_thd_ctx.set_rpl_channel_type(GR_RECOVERY_CHANNEL);
    } else {
      thd->rpl_thd_ctx.set_rpl_channel_type(RPL_STANDARD_CHANNEL);
    }

    mysql_mutex_unlock(&rli->info_thd_lock);

    /* Inform waiting threads that slave has started */
    rli->slave_run_id++;
    rli->slave_running = 1;
    rli->reported_unsafe_warning = false;
    rli->sql_thread_kill_accepted = false;
    rli->last_event_start_time = 0;

    if (init_replica_thread(thd, SLAVE_THD_SQL)) {
      /*
        TODO: this is currently broken - slave start and change replication
        source will be stuck if we fail here
      */
      mysql_cond_broadcast(&rli->start_cond);
      mysql_mutex_unlock(&rli->run_lock);
      rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                  ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                  "Failed during replica thread initialization");
      goto err;
    }
    thd->init_query_mem_roots();

    if ((rli->deferred_events_collecting = rli->rpl_filter->is_on()))
      rli->deferred_events = new Deferred_log_events();
    thd->rli_slave = rli;
    assert(thd->rli_slave->info_thd == thd);

    thd->temporary_tables = rli->save_temporary_tables;  // restore temp tables
    set_thd_in_use_temporary_tables(
        rli);  // (re)set sql_thd in use for saved temp tables
    /* Set applier thread InnoDB priority */
    set_thd_tx_priority(thd, rli->get_thd_tx_priority());

    /* Set write set related options */
    set_thd_write_set_options(thd, rli->get_ignore_write_set_memory_limit(),
                              rli->get_allow_drop_write_set());

    thd->variables.require_row_format = rli->is_row_format_required();

    if (Relay_log_info::PK_CHECK_STREAM !=
        rli->get_require_table_primary_key_check())
      thd->variables.sql_require_primary_key =
          (rli->get_require_table_primary_key_check() ==
           Relay_log_info::PK_CHECK_ON);

    thd->variables.sql_generate_invisible_primary_key = false;
    if (thd->rpl_thd_ctx.get_rpl_channel_type() != GR_APPLIER_CHANNEL &&
        thd->rpl_thd_ctx.get_rpl_channel_type() != GR_RECOVERY_CHANNEL &&
        Relay_log_info::PK_CHECK_GENERATE ==
            rli->get_require_table_primary_key_check()) {
      thd->variables.sql_generate_invisible_primary_key = true;
    }

    thd->variables.restrict_fk_on_non_standard_key = false;

    rli->transaction_parser.reset();

    thd_manager->add_thd(thd);
    thd_added = true;

    rli->get_applier_metrics().start_applier_timer();

    if (RUN_HOOK(binlog_relay_io, applier_start, (thd, rli->mi))) {
      mysql_cond_broadcast(&rli->start_cond);
      mysql_mutex_unlock(&rli->run_lock);
      rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                  ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                  "Failed to run 'applier_start' hook");
      goto err;
    }

    /* MTS: starting the worker pool */
    if (slave_start_workers(rli, rli->opt_replica_parallel_workers,
                            &mts_inited) != 0) {
      mysql_cond_broadcast(&rli->start_cond);
      mysql_mutex_unlock(&rli->run_lock);
      rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                  ER_THD(thd, ER_REPLICA_FATAL_ERROR),
                  "Failed during replica workers initialization");
      goto err;
    }
    /*
      We are going to set slave_running to 1. Assuming slave I/O thread is
      alive and connected, this is going to make Seconds_Behind_Source be 0
      i.e. "caught up". Even if we're just at start of thread. Well it's ok, at
      the moment we start we can think we are caught up, and the next second we
      start receiving data so we realize we are not caught up and
      Seconds_Behind_Source grows. No big deal.
    */
    rli->abort_slave = false;

    /*
      Reset errors for a clean start (otherwise, if the master is idle, the SQL
      thread may execute no Query_log_event, so the error will remain even
      though there's no problem anymore). Do not reset the master timestamp
      (imagine the slave has caught everything, the STOP REPLICA and START
      REPLICA: as we are not sure that we are going to receive a query, we want
      to remember the last master timestamp (to say how many seconds behind we
      are now. But the master timestamp is reset by RESET REPLICA & CHANGE
      MASTER.
    */
    rli->clear_error();
    if (rli->workers_array_initialized) {
      for (size_t i = 0; i < rli->get_worker_count(); i++) {
        rli->get_worker(i)->clear_error();
      }
    }

    if (rli->update_is_transactional() ||
        DBUG_EVALUATE_IF("simulate_update_is_transactional_error", true,
                         false)) {
      mysql_cond_broadcast(&rli->start_cond);
      mysql_mutex_unlock(&rli->run_lock);
      rli->report(
          ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
          ER_THD(thd, ER_REPLICA_FATAL_ERROR),
          "Error checking if the relay log repository is transactional.");
      goto err;
    }

    if (!rli->is_transactional())
      rli->report(
          WARNING_LEVEL, 0,
          "If a crash happens this configuration does not guarantee that "
          "the relay "
          "log info will be consistent");

    mysql_cond_broadcast(&rli->start_cond);
    mysql_mutex_unlock(&rli->run_lock);

    DEBUG_SYNC(thd, "after_start_replica");

    rli->trans_retries = 0;  // start from "no error"
    DBUG_PRINT("info", ("rli->trans_retries: %lu", rli->trans_retries));

    if (applier_reader.open(&errmsg)) {
      rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR, "%s", errmsg);
      goto err;
    }

    THD_CHECK_SENTRY(thd);
    assert(rli->info_thd == thd);

    DBUG_PRINT("source_info", ("log_file_name: %s  position: %s",
                               rli->get_group_master_log_name(),
                               llstr(rli->get_group_master_log_pos(), llbuff)));

    if (check_temp_dir(rli->slave_patternload_file, rli->get_channel())) {
      rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                  "Unable to use replica's temporary directory %s - %s",
                  replica_load_tmpdir, thd->get_stmt_da()->message_text());
      goto err;
    }

    priv_check_status = rli->check_privilege_checks_user();
    if (!!priv_check_status) {
      rli->report_privilege_check_error(ERROR_LEVEL, priv_check_status,
                                        false /* to client*/);
      rli->set_privilege_checks_user_corrupted(true);
      goto err;
    }
    priv_check_status =
        rli->initialize_applier_security_context();  // Applier security context
                                                     // initialization with
                                                     // `PRIVILEGE_CHECKS_USER`
    if (!!priv_check_status) {
      rli->report_privilege_check_error(ERROR_LEVEL, priv_check_status,
                                        false /* to client*/);
      goto err;
    }

    if (rli->is_privilege_checks_user_null())
      LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_SQL_THREAD_STARTING,
             rli->get_for_channel_str(), rli->get_rpl_log_name(),
             llstr(rli->get_group_master_log_pos_info(), llbuff),
             rli->get_group_relay_log_name(),
             llstr(rli->get_group_relay_log_pos(), llbuff1));
    else
      LogErr(INFORMATION_LEVEL,
             ER_RPL_REPLICA_SQL_THREAD_STARTING_WITH_PRIVILEGE_CHECKS,
             rli->get_for_channel_str(), rli->get_rpl_log_name(),
             llstr(rli->get_group_master_log_pos_info(), llbuff),
             rli->get_group_relay_log_name(),
             llstr(rli->get_group_relay_log_pos(), llbuff1),
             rli->get_privilege_checks_username().c_str(),
             rli->get_privilege_checks_hostname().c_str(),
             opt_always_activate_granted_roles == 0 ? "DEFAULT" : "ALL");

    /* execute init_replica variable */
    if (opt_init_replica.length) {
      execute_init_command(thd, &opt_init_replica, &LOCK_sys_init_replica);
      if (thd->is_slave_error) {
        rli->report(ERROR_LEVEL, ER_SERVER_REPLICA_INIT_QUERY_FAILED,
                    ER_THD(current_thd, ER_SERVER_REPLICA_INIT_QUERY_FAILED),
                    thd->get_stmt_da()->mysql_errno(),
                    thd->get_stmt_da()->message_text());
        goto err;
      }
    }

    /*
      First check until condition - probably there is nothing to execute. We
      do not want to wait for next event in this case.
    */
    mysql_mutex_lock(&rli->data_lock);
    if (rli->slave_skip_counter) {
      strmake(saved_log_name, rli->get_group_relay_log_name(), FN_REFLEN - 1);
      strmake(saved_master_log_name, rli->get_group_master_log_name(),
              FN_REFLEN - 1);
      saved_log_pos = rli->get_group_relay_log_pos();
      saved_master_log_pos = rli->get_group_master_log_pos();
      saved_skip = rli->slave_skip_counter;
    }
    if (rli->is_until_satisfied_at_start_slave()) {
      mysql_mutex_unlock(&rli->data_lock);
      goto err;
    }
    mysql_mutex_unlock(&rli->data_lock);

    /* Read queries from the IO/THREAD until this thread is killed */

    while (!main_loop_error && !sql_slave_killed(thd, rli)) {
      Log_event *ev = nullptr;
      THD_STAGE_INFO(thd, stage_reading_event_from_the_relay_log);
      assert(rli->info_thd == thd);
      THD_CHECK_SENTRY(thd);
      if (saved_skip && rli->slave_skip_counter == 0) {
        LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_SKIP_COUNTER_EXECUTED,
               (ulong)saved_skip, saved_log_name, (ulong)saved_log_pos,
               saved_master_log_name, (ulong)saved_master_log_pos,
               rli->get_group_relay_log_name(),
               (ulong)rli->get_group_relay_log_pos(),
               rli->get_group_master_log_name_info(),
               (ulong)rli->get_group_master_log_pos_info());
        saved_skip = 0;
      }

      // read next event
      mysql_mutex_lock(&rli->data_lock);
      ev = applier_reader.read_next_event();
      mysql_mutex_unlock(&rli->data_lock);

      // set additional context as needed by the scheduler before execution
      // takes place
      if (ev != nullptr && rli->is_parallel_exec() &&
          rli->current_mts_submode != nullptr) {
        if (rli->current_mts_submode->set_multi_threaded_applier_context(*rli,
                                                                         *ev)) {
          goto err;
        }
      }

      // try to execute the event
      switch (exec_relay_log_event(thd, rli, &applier_reader, ev)) {
        case SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK:
          /** success, we read the next event. */
          /** fall through */
        case SLAVE_APPLY_EVENT_UNTIL_REACHED:
          /** this will make the main loop abort in the next iteration */
          /** fall through */
        case SLAVE_APPLY_EVENT_RETRY:
          /** single threaded applier has to retry.
              Next iteration reads the same event. */
          break;

        case SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR:
          /** fall through */
        case SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR:
          /** fall through */
        case SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR:
          main_loop_error = true;
          break;

        default:
          /* This shall never happen. */
          assert(0); /* purecov: inspected */
          break;
      }
    }
  err:

    // report error
    if (main_loop_error == true && !sql_slave_killed(thd, rli))
      slave_errno = report_apply_event_error(thd, rli);

    /* At this point the SQL thread will not try to work anymore. */
    rli->atomic_is_stopping = true;
    (void)RUN_HOOK(
        binlog_relay_io, applier_stop,
        (thd, rli->mi, rli->is_error() || !rli->sql_thread_kill_accepted));

    slave_stop_workers(rli, &mts_inited);  // stopping worker pool
    /* Thread stopped. Print the current replication position to the log */
    if (slave_errno)
      LogErr(ERROR_LEVEL, slave_errno, rli->get_rpl_log_name(),
             llstr(rli->get_group_master_log_pos_info(), llbuff));
    else
      LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_SQL_THREAD_EXITING,
             rli->get_for_channel_str(), rli->get_rpl_log_name(),
             llstr(rli->get_group_master_log_pos_info(), llbuff));

    delete rli->current_mts_submode;
    rli->current_mts_submode = nullptr;
    rli->clear_mts_recovery_groups();

    /*
      Some events set some playgrounds, which won't be cleared because thread
      stops. Stopping of this thread may not be known to these events ("stop"
      request is detected only by the present function, not by events), so we
      must "proactively" clear playgrounds:
    */
    thd->clear_error();
    rli->cleanup_context(thd, true);
    /*
      Some extra safety, which should not been needed (normally, event deletion
      should already have done these assignments (each event which sets these
      variables is supposed to set them to 0 before terminating)).
    */
    thd->set_catalog(NULL_CSTR);
    thd->reset_query();
    thd->reset_db(NULL_CSTR);

    /*
      Pause the SQL thread and wait for 'continue_to_stop_sql_thread'
      signal to continue to shutdown the SQL thread.
    */
    DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
      rpl_replica_debug_point(DBUG_RPL_S_AFTER_SQL_STOP, thd);
    };);

    THD_STAGE_INFO(thd, stage_waiting_for_replica_mutex_on_exit);
    mysql_mutex_lock(&rli->run_lock);
    /* We need data_lock, at least to wake up any waiting source_pos_wait() */
    mysql_mutex_lock(&rli->data_lock);
    applier_reader.close();
    assert(rli->slave_running == 1);  // tracking buffer overrun
    /* When source_pos_wait() wakes up it will check this and terminate */
    rli->slave_running = 0;
    rli->atomic_is_stopping = false;

    rli->get_applier_metrics().stop_applier_timer();

    /* Forget the relay log's format */
    if (rli->set_rli_description_event(nullptr)) {
#ifndef NDEBUG
      bool set_rli_description_event_failed = false;
#endif
      assert(set_rli_description_event_failed);
    }
    /* Wake up source_pos_wait() */
    DBUG_PRINT("info",
               ("Signaling possibly waiting source_pos_wait() functions"));
    mysql_cond_broadcast(&rli->data_cond);
    mysql_mutex_unlock(&rli->data_lock);
    /* we die so won't remember charset - re-update them on next thread start */
    rli->cached_charset_invalidate();
    rli->save_temporary_tables = thd->temporary_tables;

    /*
      TODO: see if we can do this conditionally in next_event() instead
      to avoid unneeded position re-init
    */
    thd->temporary_tables =
        nullptr;  // remove temptation from destructor to close them
    // destructor will not free it, because we are weird
    thd->get_protocol_classic()->end_net();
    assert(rli->info_thd == thd);
    THD_CHECK_SENTRY(thd);
    mysql_mutex_lock(&rli->info_thd_lock);
    rli->info_thd = nullptr;
    if (commit_order_mngr) {
      rli->set_commit_order_manager(nullptr);
      delete commit_order_mngr;
    }

    mysql_mutex_unlock(&rli->info_thd_lock);
    set_thd_in_use_temporary_tables(
        rli);  // (re)set info_thd in use for saved temp tables

    thd->release_resources();
    THD_CHECK_SENTRY(thd);
    if (thd_added) thd_manager->remove_thd(thd);

    /*
      The thd can only be destructed after indirect references
      through mi->rli->info_thd are cleared: mi->rli->info_thd= NULL.

      For instance, user thread might be issuing show_slave_status
      and attempting to read mi->rli->info_thd->proc_info().
      Therefore thd must only be deleted after info_thd is set
      to NULL.
    */
    mysql_thread_set_psi_THD(nullptr);
    delete thd;

    /*
     Note: the order of the broadcast and unlock calls below (first broadcast,
     then unlock) is important. Otherwise a killer_thread can execute between
     the calls and delete the mi structure leading to a crash! (see BUG#25306
     for details)
    */
    mysql_cond_broadcast(&rli->stop_cond);
    DBUG_EXECUTE_IF("simulate_replica_delay_at_terminate_bug38694", sleep(5););
    mysql_mutex_unlock(&rli->run_lock);  // tell the world we are done
  }
  my_thread_end();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(nullptr);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  my_thread_exit(nullptr);
  return nullptr;  // Avoid compiler warnings
}

/**
  Used by the slave IO thread when it receives a rotate event from the
  master.

  Updates the master info with the place in the next binary log where
  we should start reading.  Rotate the relay log to avoid mixed-format
  relay logs.

  @param mi master_info for the slave
  @param rev The rotate log event read from the master

  @note The caller must hold mi->data_lock before invoking this function.

  @retval 0 ok
  @retval 1 error
*/
static int process_io_rotate(Master_info *mi, Rotate_log_event *rev) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(mi->rli->relay_log.get_log_lock());

  if (unlikely(!rev->is_valid())) return 1;

  /*
    Master will send a FD event immediately after the Roate event, so don't log
    the current FD event.
  */
  int ret = rotate_relay_log(mi, false, false, true);

  mysql_mutex_lock(&mi->data_lock);
  /* Safe copy as 'rev' has been "sanitized" in Rotate_log_event's ctor */
  memcpy(const_cast<char *>(mi->get_master_log_name()), rev->new_log_ident,
         rev->ident_len + 1);
  mi->set_master_log_pos(rev->pos);
  DBUG_PRINT("info",
             ("new (source_log_name, source_log_pos): ('%s', %lu)",
              mi->get_master_log_name(), (ulong)mi->get_master_log_pos()));
  mysql_mutex_unlock(&mi->data_lock);

  return ret;
}

int heartbeat_queue_event(bool is_valid, Master_info *&mi,
                          std::string binlog_name, uint64_t position,
                          unsigned long &inc_pos, bool &do_flush_mi) {
  if (!is_valid) {
    mi->report(ERROR_LEVEL, ER_REPLICA_HEARTBEAT_FAILURE,
               ER_THD(current_thd, ER_REPLICA_HEARTBEAT_FAILURE),
               "heartbeat event content seems corrupted");
    return 1;
  }
  {
    MUTEX_LOCK(lock, &mi->data_lock);
    mi->received_heartbeats++;
    mi->last_heartbeat = my_getsystime() / 10;
    std::string mi_log_filename{
        mi->get_master_log_name() != nullptr ? mi->get_master_log_name() : ""};

    /*
      compare local and event's versions of log_file, log_pos.

      Heartbeat is sent only after an event corresponding to the coordinates
      the heartbeat carries.
      Slave can not have a difference in coordinates except in the
      special case when mi->get_master_log_name(), mi->get_master_log_pos()
      have never been updated by Rotate event i.e when slave does not have
      any history with the master (and thereafter mi->get_master_log_pos() is
      NULL).

      TODO: handling `when' for SHOW REPLICA STATUS' ends behind
    */
    if (mi_log_filename.compare(binlog_name) != 0 ||
        DBUG_EVALUATE_IF("simulate_heartbeart_bogus_data_error", true, false)) {
      std::ostringstream oss;
      oss << "Replication heartbeat event contained the filename '"
          << binlog_name << "' which is different from '" << mi_log_filename
          << "' that was specified in earlier Rotate events.";
      mi->report(ERROR_LEVEL, ER_REPLICA_HEARTBEAT_FAILURE,
                 ER_THD(current_thd, ER_REPLICA_HEARTBEAT_FAILURE),
                 oss.str().c_str());
      return 1;
    } else if (mi->get_master_log_pos() > position ||
               DBUG_EVALUATE_IF("fail_heartbeat_event_lock_leak_testing", 1,
                                0)) {
      std::ostringstream oss;
      oss << "Replication heartbeat event contained the position " << position
          << " which is smaller than the position " << mi->get_master_log_pos()
          << " that was computed from earlier events received in the stream. "
          << "The filename is '" << mi_log_filename << "'.";
      mi->report(ERROR_LEVEL, ER_REPLICA_HEARTBEAT_FAILURE,
                 ER_THD(current_thd, ER_REPLICA_HEARTBEAT_FAILURE),
                 oss.str().c_str());
      return 1;
    }
    /*
      During GTID protocol, if the master skips transactions,
      a heartbeat event is sent to the slave at the end of last
      skipped transaction to update coordinates.

      I/O thread receives the heartbeat event and updates mi
      only if the received heartbeat position is greater than
      mi->get_master_log_pos(). This event is written to the
      relay log as an ignored Rotate event. SQL thread reads
      the rotate event only to update the coordinates corresponding
      to the last skipped transaction. Note that,
      we update only the positions and not the file names, as a ROTATE
      EVENT from the master prior to this will update the file name.
    */
    if ((mi->is_auto_position() == false ||
         mi->get_master_log_pos() >= position || mi_log_filename.empty()))
      return 0;

    DBUG_EXECUTE_IF("reached_heart_beat_queue_event",
                    { rpl_replica_debug_point(DBUG_RPL_S_HEARTBEAT_EV); };);
    mi->set_master_log_pos(position);

    /*
       Put this heartbeat event in the relay log as a Rotate Event.
    */
    inc_pos = 0;
  }
  if (write_rotate_to_master_pos_into_relay_log(mi->info_thd, mi, false
                                                /* force_flush_mi_info */))
    return 0;
  do_flush_mi = false; /* write_rotate_... above flushed master info */
  return 0;
}

/**
  Store an event received from the master connection into the relay
  log.

  @param mi The Master_info object representing this connection.
  @param buf Pointer to the event data.
  @param event_len Length of event data.
  @param do_flush_mi True to flush master info after successfully queuing the
                     event.

  @retval QUEUE_EVENT_OK                  on success.
  @retval QUEUE_EVENT_ERROR_QUEUING       if there was an error while queuing.
  @retval QUEUE_EVENT_ERROR_FLUSHING_INFO if there was an error while
                                          flushing master info.

  @todo Make this a member of Master_info.
*/
QUEUE_EVENT_RESULT queue_event(Master_info *mi, const char *buf,
                               ulong event_len, bool do_flush_mi) {
  QUEUE_EVENT_RESULT res = QUEUE_EVENT_OK;
  ulong inc_pos = 0;
  Relay_log_info *rli = mi->rli;
  mysql_mutex_t *log_lock = rli->relay_log.get_log_lock();
  ulong s_id;
  int lock_count = 0;

  DBUG_EXECUTE_IF("wait_in_the_middle_of_trx", {
    /*
      See `gr_flush_relay_log_no_split_trx.test`
      1) Add a debug sync point that holds and makes the applier thread to
         wait, in the middle of a transaction -
         `signal.rpl_requested_for_a_flush`.
    */
    DBUG_SET("-d,wait_in_the_middle_of_trx");
    const char dbug_wait[] = "now WAIT_FOR signal.rpl_requested_for_a_flush";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(dbug_wait)));
  });

  /*
    inside get_master_version_and_clock()
    Show-up of FD:s affects checksum_alg at once because
    that changes FD_queue.
  */
  enum_binlog_checksum_alg checksum_alg =
      mi->checksum_alg_before_fd !=
              mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF
          ? mi->checksum_alg_before_fd
          : mi->rli->relay_log.relay_log_checksum_alg;

  const char *save_buf =
      nullptr;  // needed for checksumming the fake Rotate event
  char rot_buf[LOG_EVENT_HEADER_LEN + Binary_log_event::ROTATE_HEADER_LEN +
               FN_REFLEN];
  Gtid gtid = {0, 0};
  ulonglong immediate_commit_timestamp = 0;
  ulonglong original_commit_timestamp = 0;
  bool info_error{false};
  mysql::binlog::event::Log_event_basic_info log_event_info;
  ulonglong compressed_transaction_bytes = 0;
  ulonglong uncompressed_transaction_bytes = 0;
  auto compression_type = mysql::binlog::event::compression::type::NONE;
  Log_event_type event_type = (Log_event_type)buf[EVENT_TYPE_OFFSET];

  assert(checksum_alg == mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF ||
         checksum_alg == mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF ||
         checksum_alg == mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32);

  DBUG_TRACE;

  /*
    Pause the IO thread execution and wait for 'continue_queuing_event'
    signal to continue IO thread execution.
  */
  DBUG_EXECUTE_IF("pause_on_queuing_event",
                  { rpl_replica_debug_point(DBUG_RPL_S_PAUSE_QUEUING); };);

  /*
    FD_queue checksum alg description does not apply in a case of
    FD itself. The one carries both parts of the checksum data.
  */
  if (event_type == mysql::binlog::event::FORMAT_DESCRIPTION_EVENT) {
    checksum_alg = Log_event_footer::get_checksum_alg(buf, event_len);
  }

  // does not hold always because of old binlog can work with NM
  // assert(checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);

  // should hold unless manipulations with RL. Tests that do that
  // will have to refine the clause.
  assert(mi->rli->relay_log.relay_log_checksum_alg !=
         mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);

  // Emulate the network corruption
  DBUG_EXECUTE_IF(
      "corrupt_queue_event",
      if (event_type != mysql::binlog::event::FORMAT_DESCRIPTION_EVENT) {
        char *debug_event_buf_c = const_cast<char *>(buf);
        int debug_cor_pos = rand() % (event_len - BINLOG_CHECKSUM_LEN);
        debug_event_buf_c[debug_cor_pos] = ~debug_event_buf_c[debug_cor_pos];
        DBUG_PRINT("info",
                   ("Corrupt the event at queue_event: byte on position %d",
                    debug_cor_pos));
        DBUG_SET("");
      });
  mysql::binlog::event::debug::debug_checksum_test =
      DBUG_EVALUATE_IF("simulate_checksum_test_failure", true, false);
  if (Log_event_footer::event_checksum_test(
          const_cast<uchar *>(pointer_cast<const uchar *>(buf)), event_len,
          checksum_alg)) {
    mi->report(ERROR_LEVEL, ER_NETWORK_READ_EVENT_CHECKSUM_FAILURE, "%s",
               ER_THD(current_thd, ER_NETWORK_READ_EVENT_CHECKSUM_FAILURE));
    goto err;
  }

  /*
    From now, and up to finishing queuing the event, no other thread is allowed
    to write to the relay log, or to rotate it.
  */
  mysql_mutex_lock(log_lock);
  assert(lock_count == 0);
  lock_count = 1;

  if (mi->get_mi_description_event() == nullptr) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_QUEUE_EVENT_FAILED_INVALID_CONFIGURATION,
           mi->get_channel());
    goto err;
  }

  /*
    Simulate an unknown ignorable log event by rewriting a Xid
    log event before queuing it into relay log.
  */
  DBUG_EXECUTE_IF(
      "simulate_unknown_ignorable_log_event_with_xid",
      if (event_type == mysql::binlog::event::XID_EVENT) {
        uchar *ev_buf = const_cast<uchar *>(pointer_cast<const uchar *>(buf));
        /* Overwrite the log event type with an unknown type. */
        ev_buf[EVENT_TYPE_OFFSET] = mysql::binlog::event::ENUM_END_EVENT + 1;
        /* Set LOG_EVENT_IGNORABLE_F for the log event. */
        int2store(ev_buf + FLAGS_OFFSET,
                  uint2korr(ev_buf + FLAGS_OFFSET) | LOG_EVENT_IGNORABLE_F);
        /* Recalc event's CRC */
        ha_checksum ev_crc = checksum_crc32(0L, nullptr, 0);
        ev_crc = checksum_crc32(ev_crc, (const uchar *)ev_buf,
                                event_len - BINLOG_CHECKSUM_LEN);
        int4store(&ev_buf[event_len - BINLOG_CHECKSUM_LEN], ev_crc);
        /*
          We will skip writing this event to the relay log in order to let
          the startup procedure to not finding it and assuming this transaction
          is incomplete.
          But we have to keep the unknown ignorable error to let the
          "stop_io_after_reading_unknown_event" debug point to work after
          "queuing" this event.
        */
        mysql_mutex_lock(&mi->data_lock);
        mi->set_master_log_pos(mi->get_master_log_pos() + event_len);
        lock_count = 2;
        goto end;
      });

  /*
    This transaction parser is used to ensure that the GTID of the transaction
    (if it has one) will only be added to the Retrieved_Gtid_Set after the
    last event of the transaction be queued.
    It will also be used to avoid rotating the relay log in the middle of
    a transaction.
  */
  std::tie(info_error, log_event_info) = extract_log_event_basic_info(
      buf, event_len, mi->get_mi_description_event());
  if (info_error || mi->transaction_parser.feed_event(log_event_info, true)) {
    /*
      The transaction parser detected a problem while changing state and threw
      a warning message. We are taking care of avoiding transaction boundary
      issues, but it can happen.

      Transaction boundary errors might happen mostly because of bad master
      positioning in 'CHANGE REPLICATION SOURCE TO' (or bad manipulation of
      master.info) when GTID auto positioning is off. Errors can also happen
      when using cross-version replication, replicating from a master that
      supports more event types than this slave.

      The IO thread will keep working and queuing events regardless of the
      transaction parser error, but we will throw another warning message to
      log the relay log file and position of the parser error to help
      forensics.
    */
    LogErr(WARNING_LEVEL,
           ER_RPL_REPLICA_IO_THREAD_DETECTED_UNEXPECTED_EVENT_SEQUENCE,
           mi->get_master_log_name(), mi->get_master_log_pos());
  }

  // When the receiver connects, one of the header events (typically a rotate)
  // in one of the first relay logs it writes will be a "parallelization
  // barrier", i.e., the coordinator will wait for workers to finish and then
  // the coordinator applies the event. The relay log containing this barrier
  // is the "metrics breakpoint". See
  // Applier_metrics_interface::is_after_metrics_breakpoint for details.
  if (Log_event::get_mts_execution_mode(
          false, event_type, log_event_info.server_id,
          log_event_info.log_pos) == Log_event::EVENT_EXEC_SYNC) {
    mi->rli->get_applier_metrics().set_metrics_breakpoint(
        rli->relay_log.get_log_fname());
  }

  switch (event_type) {
    case mysql::binlog::event::STOP_EVENT:
      /*
        We needn't write this event to the relay log. Indeed, it just indicates
        a master server shutdown. The only thing this does is cleaning. But
        cleaning is already done on a per-master-thread basis (as the master
        server is shutting down cleanly, it has written all DROP TEMPORARY TABLE
        prepared statements' deletion are TODO only when we binlog prep stmts).

        We don't even increment mi->get_master_log_pos(), because we may be just
        after a Rotate event. Btw, in a few milliseconds we are going to have a
        Start event from the next binlog (unless the master is presently running
        without --log-bin).
      */
      do_flush_mi = false;
      goto end;
    case mysql::binlog::event::ROTATE_EVENT: {
      Format_description_log_event *fde = mi->get_mi_description_event();
      enum_binlog_checksum_alg fde_checksum_alg = fde->footer()->checksum_alg;
      if (fde_checksum_alg != checksum_alg)
        fde->footer()->checksum_alg = checksum_alg;
      Rotate_log_event rev(buf, fde);
      fde->footer()->checksum_alg = fde_checksum_alg;

      if (unlikely(process_io_rotate(mi, &rev))) {
        // This error will be reported later at handle_slave_io().
        goto err;
      }
      /*
         Checksum special cases for the fake Rotate (R_f) event caused by the
         protocol of events generation and serialization in RL where Rotate of
         master is queued right next to FD of slave. Since it's only FD that
         carries the alg desc of FD_s has to apply to R_m. Two special rules
         apply only to the first R_f which comes in before any FD_m. The 2nd R_f
         should be compatible with the FD_s that must have taken over the last
         seen FD_m's (A).

         RSC_1: If OM \and fake Rotate \and slave is configured to
                to compute checksum for its first FD event for RL
                the fake Rotate gets checksummed here.
      */
      if (uint4korr(&buf[0]) == 0 &&
          checksum_alg == mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF &&
          mi->rli->relay_log.relay_log_checksum_alg !=
              mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF) {
        ha_checksum rot_crc = checksum_crc32(0L, nullptr, 0);
        event_len += BINLOG_CHECKSUM_LEN;
        memcpy(rot_buf, buf, event_len - BINLOG_CHECKSUM_LEN);
        int4store(&rot_buf[EVENT_LEN_OFFSET],
                  uint4korr(rot_buf + EVENT_LEN_OFFSET) + BINLOG_CHECKSUM_LEN);
        rot_crc = checksum_crc32(rot_crc, (const uchar *)rot_buf,
                                 event_len - BINLOG_CHECKSUM_LEN);
        int4store(&rot_buf[event_len - BINLOG_CHECKSUM_LEN], rot_crc);
        assert(event_len == uint4korr(&rot_buf[EVENT_LEN_OFFSET]));
        assert(mi->get_mi_description_event()->common_footer->checksum_alg ==
               mi->rli->relay_log.relay_log_checksum_alg);
        /* the first one */
        assert(mi->checksum_alg_before_fd !=
               mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);
        save_buf = buf;
        buf = rot_buf;
      } else
        /*
          RSC_2: If NM \and fake Rotate \and slave does not compute checksum
          the fake Rotate's checksum is stripped off before relay-logging.
        */
        if (uint4korr(&buf[0]) == 0 &&
            checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF &&
            mi->rli->relay_log.relay_log_checksum_alg ==
                mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF) {
          event_len -= BINLOG_CHECKSUM_LEN;
          memcpy(rot_buf, buf, event_len);
          int4store(
              &rot_buf[EVENT_LEN_OFFSET],
              uint4korr(rot_buf + EVENT_LEN_OFFSET) - BINLOG_CHECKSUM_LEN);
          assert(event_len == uint4korr(&rot_buf[EVENT_LEN_OFFSET]));
          assert(mi->get_mi_description_event()->common_footer->checksum_alg ==
                 mi->rli->relay_log.relay_log_checksum_alg);
          /* the first one */
          assert(mi->checksum_alg_before_fd !=
                 mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);
          save_buf = buf;
          buf = rot_buf;
        }
      /*
        Now the I/O thread has just changed its mi->get_master_log_name(), so
        incrementing mi->get_master_log_pos() is nonsense.
      */
      inc_pos = 0;
      break;
    }
    case mysql::binlog::event::FORMAT_DESCRIPTION_EVENT: {
      /*
        Create an event, and save it (when we rotate the relay log, we will have
        to write this event again).
      */
      /*
        We are the only thread which reads/writes mi_description_event.
        The relay_log struct does not move (though some members of it can
        change), so we needn't any lock (no rli->data_lock, no log lock).
      */
      // mark it as undefined that is irrelevant anymore
      mi->checksum_alg_before_fd =
          mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;
      Format_description_log_event *new_fdle;
      Log_event *ev = nullptr;
      if (binlog_event_deserialize(reinterpret_cast<const unsigned char *>(buf),
                                   event_len, mi->get_mi_description_event(),
                                   true, &ev) != Binlog_read_error::SUCCESS) {
        // This error will be reported later at handle_slave_io().
        goto err;
      }

      new_fdle = dynamic_cast<Format_description_log_event *>(ev);
      if (new_fdle->common_footer->checksum_alg ==
          mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF)
        new_fdle->common_footer->checksum_alg =
            mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;

      mi->set_mi_description_event(new_fdle);

      /* installing new value of checksum Alg for relay log */
      mi->rli->relay_log.relay_log_checksum_alg =
          new_fdle->common_footer->checksum_alg;

      /*
         Though this does some conversion to the slave's format, this will
         preserve the master's binlog format version, and number of event types.
      */
      /*
         If the event was not requested by the slave (the slave did not ask for
         it), i.e. has end_log_pos=0, we do not increment
         mi->get_master_log_pos()
      */
      inc_pos = uint4korr(buf + LOG_POS_OFFSET) ? event_len : 0;
      DBUG_PRINT("info", ("binlog format is now %d",
                          mi->get_mi_description_event()->binlog_version));

    } break;

    case mysql::binlog::event::HEARTBEAT_LOG_EVENT: {
      /*
        HB (heartbeat) cannot come before RL (Relay)
      */
      Heartbeat_log_event hb(buf, mi->get_mi_description_event());
      std::string mi_log_filename{mi->get_master_log_name() != nullptr
                                      ? mi->get_master_log_name()
                                      : ""};
      if (heartbeat_queue_event(hb.is_valid(), mi, hb.get_log_ident(),
                                hb.header()->log_pos, inc_pos, do_flush_mi))
        goto err;
      else
        goto end;
    } break;

    case mysql::binlog::event::HEARTBEAT_LOG_EVENT_V2: {
      /*
        HB (heartbeat) cannot come before RL (Relay)
      */
      Heartbeat_log_event_v2 hb(buf, mi->get_mi_description_event());
      auto hb_log_filename = hb.get_log_filename();
      auto hb_log_position = hb.get_log_position() == 0 ? hb.header()->log_pos
                                                        : hb.get_log_position();
      std::string mi_log_filename{mi->get_master_log_name() != nullptr
                                      ? mi->get_master_log_name()
                                      : ""};
      if (heartbeat_queue_event(hb.is_valid(), mi, hb_log_filename,
                                hb_log_position, inc_pos, do_flush_mi))
        goto err;
      else
        goto end;
    } break;
    case mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT: {
      /*
        This event does not have any meaning for the slave and
        was just sent to show the slave the master is making
        progress and avoid possible deadlocks.
        So at this point, the event is replaced by a rotate
        event what will make the slave to update what it knows
        about the master's coordinates.
      */
      inc_pos = 0;
      mysql_mutex_lock(&mi->data_lock);
      mi->set_master_log_pos(mi->get_master_log_pos() + event_len);
      mysql_mutex_unlock(&mi->data_lock);

      if (write_rotate_to_master_pos_into_relay_log(
              mi->info_thd, mi, true /* force_flush_mi_info */))
        goto err;

      do_flush_mi = false; /* write_rotate_... above flushed master info */
      goto end;
    } break;

    case mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT: {
      mysql::binlog::event::Transaction_payload_event tpe(
          buf, mi->get_mi_description_event());
      compression_type = tpe.get_compression_type();
      compressed_transaction_bytes = tpe.get_payload_size();
      uncompressed_transaction_bytes = tpe.get_uncompressed_size();
      auto gtid_monitoring_info = mi->get_gtid_monitoring_info();
      gtid_monitoring_info->update(compression_type,
                                   compressed_transaction_bytes,
                                   uncompressed_transaction_bytes);
      inc_pos = event_len;
      mi->m_queueing_transaction_size =
          mi->m_queueing_transaction_gtid_event_size +
          uncompressed_transaction_bytes;
      break;
    }

    case mysql::binlog::event::GTID_LOG_EVENT:
    case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
      /*
        This can happen if the master uses GTID_MODE=OFF_PERMISSIVE, and
        sends GTID events to the slave. A possible scenario is that user
        does not follow the upgrade procedure for GTIDs, and creates a
        topology like A->B->C, where A uses GTID_MODE=ON_PERMISSIVE, B
        uses GTID_MODE=OFF_PERMISSIVE, and C uses GTID_MODE=OFF.  Each
        connection is allowed, but the master A will generate GTID
        transactions which will be sent through B to C.  Then C will hit
        this error.
      */
      if (global_gtid_mode.get() == Gtid_mode::OFF) {
        mi->report(
            ERROR_LEVEL, ER_CANT_REPLICATE_GTID_WITH_GTID_MODE_OFF,
            ER_THD(current_thd, ER_CANT_REPLICATE_GTID_WITH_GTID_MODE_OFF),
            mi->get_master_log_name(), mi->get_master_log_pos());
        goto err;
      }
      Gtid_log_event gtid_ev(buf, mi->get_mi_description_event());
      if (!gtid_ev.is_valid()) goto err;
      rli->get_tsid_lock()->rdlock();
      gtid.sidno = gtid_ev.get_sidno(rli->get_gtid_set()->get_tsid_map());
      rli->get_tsid_lock()->unlock();
      if (gtid.sidno < 0) goto err;
      gtid.gno = gtid_ev.get_gno();
      original_commit_timestamp = gtid_ev.original_commit_timestamp;
      immediate_commit_timestamp = gtid_ev.immediate_commit_timestamp;
      compressed_transaction_bytes = uncompressed_transaction_bytes =
          gtid_ev.get_trx_length() - gtid_ev.get_event_length();

      inc_pos = event_len;
      mi->m_queueing_transaction_size = gtid_ev.get_trx_length();
      mi->m_queueing_transaction_gtid_event_size = gtid_ev.get_event_length();
    } break;

    case mysql::binlog::event::ANONYMOUS_GTID_LOG_EVENT: {
      /*
        This cannot normally happen, because the master has a check that
        prevents it from sending anonymous events when auto_position is
        enabled.  However, the master could be something else than
        mysqld, which could contain bugs that we have no control over.
        So we need this check on the slave to be sure that whoever is on
        the other side of the protocol does not break the protocol.
      */
      if (mi->is_auto_position()) {
        mi->report(
            ERROR_LEVEL, ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION,
            ER_THD(current_thd, ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION),
            mi->get_master_log_name(), mi->get_master_log_pos());
        goto err;
      }
      /*
        This can happen if the master uses GTID_MODE=ON_PERMISSIVE, and
        sends an anonymous event to the slave. A possible scenario is
        that user does not follow the upgrade procedure for GTIDs, and
        creates a topology like A->B->C, where A uses
        GTID_MODE=OFF_PERMISSIVE, B uses GTID_MODE=ON_PERMISSIVE, and C
        uses GTID_MODE=ON.  Each connection is allowed, but the master A
        will generate anonymous transactions which will be sent through
        B to C.  Then C will hit this error.
        There is a special case where on the slave
        ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS= LOCAL/UUID in that case it is
        possible to replicate from a GTID_MODE=OFF master to a GTID_MODE=ON
        slave
      */
      else if (mi->rli->m_assign_gtids_to_anonymous_transactions_info
                   .get_type() == Assign_gtids_to_anonymous_transactions_info::
                                      enum_type::AGAT_OFF) {
        if (global_gtid_mode.get() == Gtid_mode::ON) {
          mi->report(ERROR_LEVEL, ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON,
                     ER_THD(current_thd,
                            ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON),
                     mi->get_master_log_name(), mi->get_master_log_pos());
          goto err;
        }
      }
      /*
       save the original_commit_timestamp and the immediate_commit_timestamp to
       be later used for monitoring
      */
      Gtid_log_event anon_gtid_ev(buf, mi->get_mi_description_event());
      original_commit_timestamp = anon_gtid_ev.original_commit_timestamp;
      immediate_commit_timestamp = anon_gtid_ev.immediate_commit_timestamp;
      compressed_transaction_bytes = uncompressed_transaction_bytes =
          anon_gtid_ev.get_trx_length() - anon_gtid_ev.get_event_length();

      mi->m_queueing_transaction_size = anon_gtid_ev.get_trx_length();
      mi->m_queueing_transaction_gtid_event_size =
          anon_gtid_ev.get_event_length();
    }
      [[fallthrough]];
    default:
      inc_pos = event_len;
      break;
  }

  /*
    Simulate an unknown ignorable log event by rewriting the write_rows log
    event and previous_gtids log event before writing them in relay log.
  */
  DBUG_EXECUTE_IF(
      "simulate_unknown_ignorable_log_event",
      if (event_type == mysql::binlog::event::WRITE_ROWS_EVENT ||
          event_type == mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
        char *event_buf = const_cast<char *>(buf);
        /* Overwrite the log event type with an unknown type. */
        event_buf[EVENT_TYPE_OFFSET] = mysql::binlog::event::ENUM_END_EVENT + 1;
        /* Set LOG_EVENT_IGNORABLE_F for the log event. */
        int2store(event_buf + FLAGS_OFFSET,
                  uint2korr(event_buf + FLAGS_OFFSET) | LOG_EVENT_IGNORABLE_F);
      });

  /*
     If this event is originating from this server, don't queue it.
     We don't check this for 3.23 events because it's simpler like this; 3.23
     will be filtered anyway by the SQL slave thread which also tests the
     server id (we must also keep this test in the SQL thread, in case somebody
     upgrades a 4.0 slave which has a not-filtered relay log).

     ANY event coming from ourselves can be ignored: it is obvious for queries;
     for STOP_EVENT/ROTATE_EVENT/START_EVENT: these cannot come from ourselves
     (--log-replica-updates would not log that) unless this slave is also its
     direct master (an unsupported, useless setup!).
  */

  s_id = uint4korr(buf + SERVER_ID_OFFSET);

  /*
    If server_id_bits option is set we need to mask out irrelevant bits
    when checking server_id, but we still put the full unmasked server_id
    into the Relay log so that it can be accessed when applying the event
  */
  s_id &= opt_server_id_mask;

  if ((s_id == ::server_id && !mi->rli->replicate_same_server_id) ||
      /*
        the following conjunction deals with IGNORE_SERVER_IDS, if set
        If the master is on the ignore list, execution of
        format description log events and rotate events is necessary.
      */
      (mi->ignore_server_ids->dynamic_ids.size() > 0 &&
       mi->shall_ignore_server_id(s_id) &&
       /* everything is filtered out from non-master */
       (s_id != mi->master_id ||
        /* for the master meta information is necessary */
        (event_type != mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
         event_type != mysql::binlog::event::ROTATE_EVENT)))) {
    /*
      Do not write it to the relay log.
      a) We still want to increment mi->get_master_log_pos(), so that we won't
      re-read this event from the master if the slave IO thread is now
      stopped/restarted (more efficient if the events we are ignoring are big
      LOAD DATA INFILE).
      b) We want to record that we are skipping events, for the information of
      the slave SQL thread, otherwise that thread may let
      rli->group_relay_log_pos stay too small if the last binlog's event is
      ignored.
      But events which were generated by this slave and which do not exist in
      the master's binlog (i.e. Format_desc, Rotate & Stop) should not increment
      mi->get_master_log_pos().
      If the event is originated remotely and is being filtered out by
      IGNORE_SERVER_IDS it increments mi->get_master_log_pos()
      as well as rli->group_relay_log_pos.
    */
    if (!(s_id == ::server_id && !mi->rli->replicate_same_server_id) ||
        (event_type != mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
         event_type != mysql::binlog::event::ROTATE_EVENT &&
         event_type != mysql::binlog::event::STOP_EVENT)) {
      rli->relay_log.lock_binlog_end_pos();
      mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
      memcpy(rli->ign_master_log_name_end, mi->get_master_log_name(),
             FN_REFLEN);
      assert(rli->ign_master_log_name_end[0]);
      rli->ign_master_log_pos_end = mi->get_master_log_pos();
      // the slave SQL thread needs to re-check
      rli->relay_log.update_binlog_end_pos(false /*need_lock*/);
      rli->relay_log.unlock_binlog_end_pos();
    }
    DBUG_PRINT(
        "info",
        ("source_log_pos: %lu, event originating from %u server, ignored",
         (ulong)mi->get_master_log_pos(), uint4korr(buf + SERVER_ID_OFFSET)));
  } else {
    bool is_error = false;
    DBUG_EXECUTE_IF("simulate_truncated_relay_log_event", { event_len -= 5; });
    /* write the event to the relay log */
    if (likely(rli->relay_log.write_buffer(buf, event_len, mi) == 0)) {
      DBUG_SIGNAL_WAIT_FOR(current_thd,
                           "pause_on_queue_event_after_write_buffer",
                           "receiver_reached_pause_on_queue_event",
                           "receiver_continue_queuing_event");
      mysql_mutex_lock(&mi->data_lock);
      lock_count = 2;
      mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
      DBUG_PRINT("info",
                 ("source_log_pos: %lu", (ulong)mi->get_master_log_pos()));

      /*
        If we are starting an anonymous transaction, we will discard
        the GTID of the partial transaction that was not finished (if
        there is one) when calling mi->started_queueing().
      */
#ifndef NDEBUG
      if (event_type == mysql::binlog::event::ANONYMOUS_GTID_LOG_EVENT) {
        if (!mi->get_queueing_trx_gtid()->is_empty()) {
          DBUG_PRINT("info",
                     ("Discarding Gtid(%d, %" PRId64 ") as the transaction "
                      "wasn't complete and we found an "
                      "ANONYMOUS_GTID_LOG_EVENT.",
                      mi->get_queueing_trx_gtid()->sidno,
                      mi->get_queueing_trx_gtid()->gno));
        }
      }
#endif

      /*
        We have to mark this GTID (either anonymous or not) as started
        to be queued.

        Also, if this event is a GTID_LOG_EVENT, we have to store its GTID to
        add to the Retrieved_Gtid_Set later, when the last event of the
        transaction be queued. The call to mi->started_queueing() will save
        the GTID to be used later.
      */
      if (Log_event_type_helper::is_any_gtid_event(event_type)) {
        // set the timestamp for the start time of queueing this transaction
        mi->started_queueing(gtid, original_commit_timestamp,
                             immediate_commit_timestamp);
        auto gtid_monitoring_info = mi->get_gtid_monitoring_info();
        gtid_monitoring_info->update(
            mysql::binlog::event::compression::type::NONE,
            compressed_transaction_bytes, uncompressed_transaction_bytes);
      }
    } else {
      /*
        We failed to write the event and didn't updated slave positions.

        We have to "rollback" the transaction parser state, or else, when
        restarting the I/O thread without GTID auto positing the parser
        would assume the failed event as queued.
      */
      mi->transaction_parser.rollback();
      is_error = true;
    }

    if (save_buf != nullptr) buf = save_buf;
    if (is_error) {
      // This error will be reported later at handle_slave_io().
      goto err;
    }
  }
  goto end;

err:
  res = QUEUE_EVENT_ERROR_QUEUING;

end:
  if (res == QUEUE_EVENT_OK && do_flush_mi) {
    /*
      Take a ride in the already locked LOCK_log to flush master info.

      JAG: TODO: Notice that we could only flush master info if we are
                 not in the middle of a transaction. Having a proper
                 relay log recovery can allow us to do this.
    */
    if (lock_count == 1) {
      mysql_mutex_lock(&mi->data_lock);
      lock_count = 2;
    }

    if (flush_master_info(mi, false /*force*/, lock_count == 0 /*need_lock*/,
                          false /*flush_relay_log*/, mi->is_gtid_only_mode()))
      res = QUEUE_EVENT_ERROR_FLUSHING_INFO;
    if (mi->is_gtid_only_mode()) {
      mi->update_flushed_relay_log_info();
    }
  }
  if (lock_count >= 2) mysql_mutex_unlock(&mi->data_lock);
  if (lock_count >= 1) mysql_mutex_unlock(log_lock);
  DBUG_PRINT("info", ("queue result: %d", res));
  return res;
}

/**
  Hook to detach the active VIO before closing a connection handle.

  The client API might close the connection (and associated data)
  in case it encounters a unrecoverable (network) error. This hook
  is called from the client code before the VIO handle is deleted
  allows the thread to detach the active vio so it does not point
  to freed memory.

  Other calls to THD::clear_active_vio throughout this module are
  redundant due to the hook but are left in place for illustrative
  purposes.
*/

void slave_io_thread_detach_vio() {
  THD *thd = current_thd;
  if (thd && thd->slave_thread) thd->clear_active_vio();
}

/*
  Set network namespace if channel is using network namespace and connect
  to master.

  @param  thd                THD context
  @param  mysql              MYSQL connection handler
  @param  mi                 Master info corresponding to this channel.
  @param  reconnect          Reconnect if true
  @param  suppress_warnings  suppress warnings if required.

  @retval 0   ok.
  @retval 1   not ok.
*/
static int connect_to_master_via_namespace(THD *thd, MYSQL *mysql,
                                           Master_info *mi, bool reconnect,
                                           bool suppress_warnings,
                                           const std::string &host,
                                           const uint port) {
  if (mi->is_set_network_namespace()) {
#ifdef HAVE_SETNS
    if (set_network_namespace(mi->network_namespace)) {
      std::stringstream ss;
      ss << "failed to set network namespace '";
      ss << mi->network_namespace;
      ss << "'";
      mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                 ER_THD(thd, ER_REPLICA_FATAL_ERROR), ss.str().c_str());
      return 1;
    }
#else
    // Network namespace not supported by the platform. Report error.
    LogErr(ERROR_LEVEL, ER_NETWORK_NAMESPACES_NOT_SUPPORTED);
    mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
               ER_THD(thd, ER_REPLICA_FATAL_ERROR),
               ER_THD(thd, ER_NETWORK_NAMESPACES_NOT_SUPPORTED));
    return 1;
#endif
    // Save default value of network namespace
    // Set network namespace before sockets be created
  }
  int connect_res = connect_to_master(thd, mysql, mi, reconnect,
                                      suppress_warnings, host, port);
  // we can get killed during safe_connect
#ifdef HAVE_SETNS
  if (mi->is_set_network_namespace()) {
    // Restore original network namespace used to be before connection has
    // been created
    (void)restore_original_network_namespace();
  }
#endif
  return connect_res;
}

/*
  Try to connect until successful or slave killed

  SYNPOSIS
    safe_connect()
    thd                 Thread handler for slave
    mysql               MySQL connection handle
    mi                  Replication handle

  RETURN
    0   ok
    #   Error
*/
static int safe_connect(THD *thd, MYSQL *mysql, Master_info *mi,
                        const std::string &host, const uint port) {
  DBUG_TRACE;

  return connect_to_master_via_namespace(thd, mysql, mi,
                                         /*reconnect=*/false,
                                         /*suppress_warnings=*/false, host,
                                         port);
}

int connect_to_master(THD *thd, MYSQL *mysql, Master_info *mi, bool reconnect,
                      bool suppress_warnings, const std::string &host,
                      const uint port, bool is_io_thread) {
  int last_errno = -2;  // impossible error
  ulong err_count = 0;
  char llbuff[22];
  char password[MAX_PASSWORD_LENGTH + 1];
  size_t password_size = sizeof(password);
  DBUG_TRACE;
  set_replica_max_allowed_packet(thd, mysql);
  ulong client_flag = CLIENT_REMEMBER_OPTIONS;

  /* Always reset public key to remove cached copy */
  mysql_reset_server_public_key();

  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *)&replica_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *)&replica_net_timeout);

  if (mi->bind_addr[0]) {
    DBUG_PRINT("info", ("bind_addr: %s", mi->bind_addr));
    mysql_options(mysql, MYSQL_OPT_BIND, mi->bind_addr);
  }

  /* By default the channel is not configured to use SSL */
  enum mysql_ssl_mode ssl_mode = SSL_MODE_DISABLED;
  if (mi->ssl) {
    /* The channel is configured to use SSL */
    mysql_options(mysql, MYSQL_OPT_SSL_KEY,
                  mi->ssl_key[0] ? mi->ssl_key : nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CERT,
                  mi->ssl_cert[0] ? mi->ssl_cert : nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CA,
                  mi->ssl_ca[0] ? mi->ssl_ca : nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CAPATH,
                  mi->ssl_capath[0] ? mi->ssl_capath : nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CIPHER,
                  mi->ssl_cipher[0] ? mi->ssl_cipher : nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CRL,
                  mi->ssl_crl[0] ? mi->ssl_crl : nullptr);
    mysql_options(mysql, MYSQL_OPT_TLS_VERSION,
                  mi->tls_version[0] ? mi->tls_version : nullptr);
    mysql_options(mysql, MYSQL_OPT_TLS_CIPHERSUITES,
                  mi->tls_ciphersuites.first
                      ? nullptr
                      : mi->tls_ciphersuites.second.c_str());
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH,
                  mi->ssl_crlpath[0] ? mi->ssl_crlpath : nullptr);
    if (mi->ssl_verify_server_cert)
      ssl_mode = SSL_MODE_VERIFY_IDENTITY;
    else if (mi->ssl_ca[0] || mi->ssl_capath[0])
      ssl_mode = SSL_MODE_VERIFY_CA;
    else
      ssl_mode = SSL_MODE_REQUIRED;
  }
  mysql_options(mysql, MYSQL_OPT_SSL_MODE, &ssl_mode);

  mysql_options(mysql, MYSQL_OPT_COMPRESSION_ALGORITHMS,
                opt_replica_compressed_protocol ? COMPRESSION_ALGORITHM_ZLIB
                                                : mi->compression_algorithm);
  mysql_options(mysql, MYSQL_OPT_ZSTD_COMPRESSION_LEVEL,
                &mi->zstd_compression_level);
  /*
    If server's default charset is not supported (like utf16, utf32) as client
    charset, then set client charset to 'latin1' (default client charset).
  */
  if (is_supported_parser_charset(default_charset_info))
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  else {
    LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_CANT_USE_CHARSET,
           default_charset_info->csname, default_client_charset_info->csname);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME,
                  default_client_charset_info->csname);
  }

  if (mi->is_start_plugin_auth_configured()) {
    DBUG_PRINT("info", ("Slaving is using MYSQL_DEFAULT_AUTH %s",
                        mi->get_start_plugin_auth()));
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, mi->get_start_plugin_auth());
  }

  if (mi->is_start_plugin_dir_configured()) {
    DBUG_PRINT("info", ("Slaving is using MYSQL_PLUGIN_DIR %s",
                        mi->get_start_plugin_dir()));
    mysql_options(mysql, MYSQL_PLUGIN_DIR, mi->get_start_plugin_dir());
  }
  /* Set MYSQL_PLUGIN_DIR in case master asks for an external authentication
     plugin */
  else if (opt_plugin_dir_ptr && *opt_plugin_dir_ptr)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir_ptr);

  if (mi->public_key_path[0]) {
    /* Set public key path */
    DBUG_PRINT("info", ("Set source's public key path"));
    mysql_options(mysql, MYSQL_SERVER_PUBLIC_KEY, mi->public_key_path);
  }

  /* Get public key from master */
  DBUG_PRINT("info", ("Set preference to get public key from source"));
  mysql_options(mysql, MYSQL_OPT_GET_SERVER_PUBLIC_KEY, &mi->get_public_key);

  if (is_io_thread && !mi->is_start_user_configured())
    LogErr(WARNING_LEVEL, ER_RPL_REPLICA_INSECURE_CHANGE_SOURCE);

  if (mi->get_password(password, &password_size)) {
    mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
               ER_THD(thd, ER_REPLICA_FATAL_ERROR),
               "Unable to configure password when attempting to "
               "connect to the source server. Connection attempt "
               "terminated.");
    return 1;
  }

  const char *user = mi->get_user();
  if (user == nullptr || user[0] == 0) {
    mi->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
               ER_THD(thd, ER_REPLICA_FATAL_ERROR),
               "Invalid (empty) username when attempting to "
               "connect to the source server. Connection attempt "
               "terminated.");
    return 1;
  }

  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", "mysqld");
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "_client_role",
                 "binary_log_listener");
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "_client_replication_channel_name", mi->get_channel());

  const char *tmp_host = host.empty() ? mi->host : host.c_str();
  uint tmp_port = (port == 0) ? mi->port : port;

  bool replica_was_killed{false};
  bool connected{false};

  while (!connected) {
    replica_was_killed = is_io_thread ? io_slave_killed(thd, mi)
                                      : monitor_io_replica_killed(thd, mi);
    if (replica_was_killed) {
      LogErr(INFORMATION_LEVEL, ER_RPL_REPLICA_IO_THREAD_KILLED,
             mi->get_for_channel_str());
      break;
    }

    if (reconnect) {
      connected = !mysql_reconnect(mysql);
    } else {
      // Set this each time mysql_real_connect() is called to make a connection
      mysql_extension_set_server_extn(mysql, &mi->server_extn);

      connected = mysql_real_connect(mysql, tmp_host, user, password, nullptr,
                                     tmp_port, nullptr, client_flag);
    }
    if (connected) break;

    /*
       SHOW REPLICA STATUS will display the number of retries which
       would be real retry counts instead of mi->retry_count for
       each connection attempt by 'Last_IO_Error' entry.
    */
    last_errno = mysql_errno(mysql);
    suppress_warnings = false;
    if (is_io_thread) {
      mi->report(ERROR_LEVEL, last_errno,
                 "Error %s to source '%s@%s:%d'."
                 " This was attempt %lu/%lu, with a delay of %d seconds between"
                 " attempts. Message: %s",
                 (reconnect ? "reconnecting" : "connecting"), mi->get_user(),
                 tmp_host, tmp_port, err_count + 1, mi->retry_count,
                 mi->connect_retry, mysql_error(mysql));
    }

    /*
      By default we try forever. The reason is that failure will trigger
      master election, so if the user did not set mi->retry_count we
      do not want to have election triggered on the first failure to
      connect
    */
    if (++err_count == mi->retry_count) {
      if (is_network_error(last_errno) && is_io_thread) mi->set_network_error();
      replica_was_killed = true;
      break;
    }
    slave_sleep(thd, mi->connect_retry,
                is_io_thread ? io_slave_killed : monitor_io_replica_killed, mi);
  }

  if (!replica_was_killed) {
    if (is_io_thread) {
      mi->clear_error();  // clear possible left over reconnect error
      mi->reset_network_error();
    }

    if (reconnect) {
      if (!suppress_warnings)
        LogErr(SYSTEM_LEVEL,
               ER_RPL_REPLICA_CONNECTED_TO_SOURCE_REPLICATION_RESUMED,
               mi->get_for_channel_str(), mi->get_user(), tmp_host, tmp_port,
               mi->get_io_rpl_log_name(),
               llstr(mi->get_master_log_pos(), llbuff));
    } else {
      query_logger.general_log_print(thd, COM_CONNECT_OUT, "%s@%s:%d",
                                     mi->get_user(), tmp_host, tmp_port);
    }

    thd->set_active_vio(mysql->net.vio);
  }
  DBUG_PRINT("exit", ("replica_was_killed: %d", replica_was_killed));
  return replica_was_killed;
}

/*
  safe_reconnect()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    mi->retry_count times
*/

static int safe_reconnect(THD *thd, MYSQL *mysql, Master_info *mi,
                          bool suppress_warnings, const std::string &host,
                          const uint port) {
  DBUG_TRACE;
  return connect_to_master_via_namespace(thd, mysql, mi,
                                         /*reconnect=*/true, suppress_warnings,
                                         host, port);
}

int rotate_relay_log(Master_info *mi, bool log_master_fd, bool need_lock,
                     bool need_log_space_lock) {
  DBUG_TRACE;

  Relay_log_info *rli = mi->rli;

  if (need_lock)
    mysql_mutex_lock(rli->relay_log.get_log_lock());
  else
    mysql_mutex_assert_owner(rli->relay_log.get_log_lock());
  DBUG_EXECUTE_IF("crash_before_rotate_relaylog", DBUG_SUICIDE(););

  int error = 0;

  /*
     We need to test inited because otherwise, new_file() will attempt to lock
     LOCK_log, which may not be inited (if we're not a slave).
  */
  if (!rli->inited) {
    DBUG_PRINT("info", ("rli->inited == 0"));
    goto end;
  }

  if (log_master_fd)
    error =
        rli->relay_log.new_file_without_locking(mi->get_mi_description_event());
  else
    error = rli->relay_log.new_file_without_locking(nullptr);
  if (error != 0) goto end;

  /*
    We harvest now, because otherwise BIN_LOG_HEADER_SIZE will not immediately
    be counted, so imagine a succession of FLUSH LOGS  and assume the slave
    threads are started:
    relay_log_space decreases by the size of the deleted relay log, but does
    not increase, so flush-after-flush we may become negative, which is wrong.
    Even if this will be corrected as soon as a query is replicated on the
    slave (because the I/O thread will then call harvest_bytes_written() which
    will harvest all these BIN_LOG_HEADER_SIZE we forgot), it may give strange
    output in SHOW REPLICA STATUS meanwhile. So we harvest now.
    If the log is closed, then this will just harvest the last writes, probably
    0 as they probably have been harvested.
  */
  rli->relay_log.harvest_bytes_written(rli, need_log_space_lock);
end:
  if (need_lock) mysql_mutex_unlock(rli->relay_log.get_log_lock());
  return error;
}

/**
  flushes the relay logs of a replication channel.

  @param[in]         mi      Master_info corresponding to the
                             channel.
  @param[in]         thd     the client thread carrying the command.

  @retval            1       fail
  @retval            0       ok
  @retval            -1      deferred flush
*/
int flush_relay_logs(Master_info *mi, THD *thd) {
  DBUG_TRACE;
  int error = 0;

  if (mi) {
    Relay_log_info *rli = mi->rli;
    if (rli->inited) {
      // Rotate immediately if one is true:
      if ((!is_group_replication_plugin_loaded() ||  // GR is disabled
           !mi->transaction_parser
                .is_inside_transaction() ||  // not inside a transaction
           !channel_map.is_group_replication_applier_channel_name(
               mi->get_channel()) ||  // channel isn't GR applier channel
           !mi->slave_running) &&     // the I/O thread isn't running
          DBUG_EVALUATE_IF(
              "deferred_flush_relay_log",
              !channel_map.is_group_replication_applier_channel_name(
                  mi->get_channel()),
              true)) {
        if (rotate_relay_log(mi)) error = 1;
      }
      // Postpone the rotate action, delegating it to the I/O thread
      else {
        channel_map.unlock();
        mi->request_rotate(thd);
        channel_map.rdlock();
        error = -1;
      }
    }
  }
  return error;
}

/**
   Entry point for FLUSH RELAYLOGS command or to flush relaylogs for
   the FLUSH LOGS command.
   FLUSH LOGS or FLUSH RELAYLOGS needs to flush the relaylogs of all
   the replciaiton channels in multisource replication.
   FLUSH RELAYLOGS FOR CHANNEL flushes only the relaylogs pertaining to
   a channel.

   @param[in]         thd              the client thread carrying the command.

   @retval            true             fail
   @retval            false            success
*/
bool flush_relay_logs_cmd(THD *thd) {
  DBUG_TRACE;
  Master_info *mi = nullptr;
  LEX *lex = thd->lex;
  bool error = false;

  channel_map.rdlock();

  /*
     lex->mi.channel is NULL, for FLUSH LOGS or when the client thread
     is not present. (See tmp_thd in  the caller).
     When channel is not provided, lex->mi.for_channel is false.
  */
  if (!lex->mi.channel || !lex->mi.for_channel) {
    bool flush_was_deferred{false};
    enum_channel_type channel_types[] = {SLAVE_REPLICATION_CHANNEL,
                                         GROUP_REPLICATION_CHANNEL};

    for (auto channel_type : channel_types) {
      mi_map already_processed;

      do {
        flush_was_deferred = false;

        for (mi_map::iterator it = channel_map.begin(channel_type);
             it != channel_map.end(channel_type); it++) {
          if (already_processed.find(it->first) != already_processed.end())
            continue;

          mi = it->second;
          already_processed.insert(std::make_pair(it->first, mi));

          int flush_status = flush_relay_logs(mi, thd);
          flush_was_deferred = (flush_status == -1);
          error = (flush_status == 1);

          if (flush_status != 0) break;
        }
      } while (flush_was_deferred);
    }
  } else {
    mi = channel_map.get_mi(lex->mi.channel);

    if (mi) {
      error = (flush_relay_logs(mi, thd) == 1);
    } else {
      if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL ||
          thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER) {
        /*
          Log warning on SQL or worker threads.
        */
        LogErr(WARNING_LEVEL, ER_RPL_REPLICA_INCORRECT_CHANNEL,
               lex->mi.channel);
      } else {
        /*
          Return error on client sessions.
        */
        error = true;
        my_error(ER_REPLICA_CHANNEL_DOES_NOT_EXIST, MYF(0), lex->mi.channel);
      }
    }
  }

  channel_map.unlock();

  return error;
}

bool reencrypt_relay_logs() {
  DBUG_TRACE;

  Master_info *mi;
  channel_map.rdlock();

  enum_channel_type channel_types[] = {SLAVE_REPLICATION_CHANNEL,
                                       GROUP_REPLICATION_CHANNEL};
  for (auto channel_type : channel_types) {
    for (mi_map::iterator it = channel_map.begin(channel_type);
         it != channel_map.end(channel_type); it++) {
      mi = it->second;
      if (mi != nullptr) {
        Relay_log_info *rli = mi->rli;
        if (rli != nullptr && rli->inited && rli->relay_log.reencrypt_logs()) {
          channel_map.unlock();
          return true;
        }
      }
    }
  }

  channel_map.unlock();

  return false;
}

/**
  a copy of active_mi->rli->slave_skip_counter, for showing in SHOW GLOBAL
  VARIABLES, INFORMATION_SCHEMA.GLOBAL_VARIABLES and @@sql_replica_skip_counter
  without taking all the mutexes needed to access
  active_mi->rli->slave_skip_counter properly.
*/
uint sql_replica_skip_counter;

/**
   Executes a START REPLICA statement.

  @param thd                 Pointer to THD object for the client thread
                             executing the statement.

   @param connection_param   Connection parameters for starting threads

   @param master_param       Master parameters used for starting threads

   @param thread_mask_input  The thread mask that identifies which threads to
                             start. If 0 is passed (start no thread) then this
                             parameter is ignored and all stopped threads are
                             started

   @param mi                 Pointer to Master_info object for the slave's IO
                             thread.

   @param set_mts_settings   If true, the channel uses the server MTS
                             configured settings when starting the applier
                             thread.

   @retval false success
   @retval true error
*/
bool start_slave(THD *thd, LEX_REPLICA_CONNECTION *connection_param,
                 LEX_SOURCE_INFO *master_param, int thread_mask_input,
                 Master_info *mi, bool set_mts_settings) {
  bool is_error = false;
  int thread_mask;

  DBUG_TRACE;

  /*
    START REPLICA command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.slave_master_info' and
    'mysql.slave_relay_log_info' replication repository tables.
  */
  thd->set_skip_readonly_check();
  Security_context *sctx = thd->security_context();
  if (!sctx->check_access(SUPER_ACL) &&
      !sctx->has_global_grant(STRING_WITH_LEN("REPLICATION_SLAVE_ADMIN"))
           .first) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or REPLICATION_SLAVE_ADMIN");
    return true;
  }

  mi->channel_wrlock();

  if (connection_param->user || connection_param->password) {
    if (!thd->get_ssl()) {
      push_warning(thd, Sql_condition::SL_NOTE, ER_INSECURE_PLAIN_TEXT,
                   ER_THD(thd, ER_INSECURE_PLAIN_TEXT));
    }
  }

  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  // Get a mask of _stopped_ threads
  init_thread_mask(&thread_mask, mi, true /* inverse */);
  /*
    Below we will start all stopped threads.  But if the user wants to
    start only one thread, do as if the other thread was running (as we
    don't want to touch the other thread), so set the bit to 0 for the
    other thread
  */
  if (thread_mask_input) {
    thread_mask &= thread_mask_input;
  }
  if (thread_mask)  // some threads are stopped, start them
  {
    if (load_mi_and_rli_from_repositories(mi, false, thread_mask)) {
      is_error = true;
      my_error(ER_CONNECTION_METADATA, MYF(0));
    } else if (*mi->host || !(thread_mask & REPLICA_IO)) {
      // If the all threads are stopped and the metrics are disabled
      // we can reset the statistics in case they ran in the past and are
      // again enabled in the future.
      // If metrics are enabled, don't reset anything to save
      // ongoing metrics from before the stop
      if ((thread_mask & REPLICA_IO) != 0 && (thread_mask & REPLICA_SQL) != 0 &&
          !opt_collect_replica_applier_metrics) {
        mi->rli->get_applier_metrics().reset();
      }

      mi->set_applier_metric_collection_status(
          opt_collect_replica_applier_metrics);

      /*
        If we will start IO thread we need to take care of possible
        options provided through the START REPLICA if there is any.
      */
      if (thread_mask & REPLICA_IO) {
        if (connection_param->user) {
          mi->set_start_user_configured(true);
          mi->set_user(connection_param->user);
        }
        if (connection_param->password) {
          mi->set_start_user_configured(true);
          mi->set_password(connection_param->password);
        }
        if (connection_param->plugin_auth)
          mi->set_plugin_auth(connection_param->plugin_auth);
        if (connection_param->plugin_dir)
          mi->set_plugin_dir(connection_param->plugin_dir);
      }

      /*
        If we will start SQL thread we will care about UNTIL options If
        not and they are specified we will ignore them and warn user
        about this fact.
      */
      if (thread_mask & REPLICA_SQL) {
        /*
          sql_replica_skip_counter only effects the applier thread which is
          first started. So after sql_replica_skip_counter is copied to
          rli->slave_skip_counter, it is reset to 0.
        */
        mysql_mutex_lock(&LOCK_sql_replica_skip_counter);
        if (mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() !=
                Assign_gtids_to_anonymous_transactions_info::enum_type::
                    AGAT_OFF ||
            global_gtid_mode.get() != Gtid_mode::ON)
          mi->rli->slave_skip_counter = sql_replica_skip_counter;
        sql_replica_skip_counter = 0;
        mysql_mutex_unlock(&LOCK_sql_replica_skip_counter);
        /*
          To cache the MTS system var values and used them in the following
          runtime. The system vars can change meanwhile but having no other
          effects.
          It also allows the per channel definition of this variables.
        */
        if (set_mts_settings) {
          mi->rli->opt_replica_parallel_workers =
              opt_mts_replica_parallel_workers;
          if (mi->is_gtid_only_mode() &&
              opt_mts_replica_parallel_workers == 0) {
            mi->rli->opt_replica_parallel_workers = 1;
          }
          if (mts_parallel_option == MTS_PARALLEL_TYPE_DB_NAME)
            mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_DB_NAME;
          else
            mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_LOGICAL_CLOCK;

#ifndef NDEBUG
          if (!DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0))
#endif
            mi->rli->checkpoint_group = opt_mta_checkpoint_group;
        }

        int slave_errno = mi->rli->init_until_option(thd, master_param);
        if (slave_errno) {
          my_error(slave_errno, MYF(0));
          is_error = true;
        }

        if (!is_error) is_error = check_slave_sql_config_conflict(mi->rli);
      } else if (master_param->pos || master_param->relay_log_pos ||
                 master_param->gtid)
        push_warning(thd, Sql_condition::SL_NOTE, ER_UNTIL_COND_IGNORED,
                     ER_THD(thd, ER_UNTIL_COND_IGNORED));

      if (!is_error)
        is_error =
            start_slave_threads(false /*need_lock_slave=false*/,
                                true /*wait_for_start=true*/, mi, thread_mask);
    } else {
      is_error = true;
      my_error(ER_BAD_REPLICA, MYF(0));
    }
  } else {
    /* no error if all threads are already started, only a warning */
    push_warning_printf(
        thd, Sql_condition::SL_NOTE, ER_REPLICA_CHANNEL_WAS_RUNNING,
        ER_THD(thd, ER_REPLICA_CHANNEL_WAS_RUNNING), mi->get_channel());
  }

  /*
    Clean up start information if there was an attempt to start
    the IO thread to avoid any security issue.
  */
  if (is_error && (thread_mask & REPLICA_IO) == REPLICA_IO)
    mi->reset_start_info();

  unlock_slave_threads(mi);

  mi->channel_unlock();

  return is_error;
}

/**
  Execute a STOP REPLICA statement.

  @param thd              Pointer to THD object for the client thread executing
                          the statement.

  @param mi               Pointer to Master_info object for the slave's IO
                          thread.

  @param net_report       If true, saves the exit status into Diagnostics_area.

  @param for_one_channel  If the method is being invoked only for one channel

  @param push_temp_tables_warning  If it should push a "have temp tables
                                   warning" once having open temp tables. This
                                   avoids multiple warnings when there is more
                                   than one channel with open temp tables.
                                   This parameter can be removed when the
                                   warning is issued with per-channel
                                   information.

  @retval 0 success
  @retval 1 error
*/
int stop_slave(THD *thd, Master_info *mi, bool net_report, bool for_one_channel,
               bool *push_temp_tables_warning) {
  DBUG_TRACE;

  int slave_errno;
  if (!thd) thd = current_thd;

  /*
    STOP REPLICA command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.slave_master_info' and
    'mysql.slave_relay_log_info' replication repository tables.
  */
  thd->set_skip_readonly_check();

  Security_context *sctx = thd->security_context();
  if (!sctx->check_access(SUPER_ACL) &&
      !sctx->has_global_grant(STRING_WITH_LEN("REPLICATION_SLAVE_ADMIN"))
           .first) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or REPLICATION_SLAVE_ADMIN");
    return 1;
  }

  mi->channel_wrlock();

  THD_STAGE_INFO(thd, stage_killing_replica);
  int thread_mask;
  lock_slave_threads(mi);

  DBUG_EXECUTE_IF("simulate_hold_run_locks_on_stop_replica",
                  my_sleep(10000000););

  // Get a mask of _running_ threads
  init_thread_mask(&thread_mask, mi, false /* not inverse*/);

  /*
    Below we will stop all running threads.
    But if the user wants to stop only one thread, do as if the other thread
    was stopped (as we don't want to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (thd->lex->replica_thd_opt) {
    thread_mask &= thd->lex->replica_thd_opt;

    /*
      If we are stopping IO thread, we also need to consider
      IO Monitor thread.
    */
    if ((thread_mask & REPLICA_IO) &&
        mi->is_source_connection_auto_failover()) {
      thread_mask |= SLAVE_MONITOR;
    }
  }

  if (thread_mask) {
    slave_errno =
        terminate_slave_threads(mi, thread_mask, rpl_stop_replica_timeout,
                                false /*need_lock_term=false*/);
  } else {
    // no error if both threads are already stopped, only a warning
    slave_errno = 0;
    push_warning_printf(
        thd, Sql_condition::SL_NOTE, ER_REPLICA_CHANNEL_WAS_NOT_RUNNING,
        ER_THD(thd, ER_REPLICA_CHANNEL_WAS_NOT_RUNNING), mi->get_channel());
  }

  /*
    If the slave has open temp tables and there is a following CHANGE
    REPLICATION SOURCE there is a possibility that the temporary tables are left
    open forever. Though we dont restrict failover here, we do warn users. In
    future, we should have a command to delete open temp tables the slave has
    replicated. See WL#7441 regarding this command.
  */

  if (mi->rli->atomic_channel_open_temp_tables && *push_temp_tables_warning) {
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO,
                 ER_THD(thd, ER_WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO));
    *push_temp_tables_warning = false;
  }

  unlock_slave_threads(mi);

  mi->channel_unlock();

  if (slave_errno) {
    if ((slave_errno == ER_STOP_REPLICA_SQL_THREAD_TIMEOUT) ||
        (slave_errno == ER_STOP_REPLICA_MONITOR_IO_THREAD_TIMEOUT) ||
        (slave_errno == ER_STOP_REPLICA_IO_THREAD_TIMEOUT)) {
      push_warning(thd, Sql_condition::SL_NOTE, slave_errno,
                   ER_THD_NONCONST(thd, slave_errno));

      /*
        If new slave_errno is added in the if() condition above then make sure
        that there are no % in the error message or change the logging API
        to use verbatim() to avoid % substitutions.
      */
      longlong log_errno = (slave_errno == ER_STOP_REPLICA_SQL_THREAD_TIMEOUT)
                               ? ER_RPL_REPLICA_SQL_THREAD_STOP_CMD_EXEC_TIMEOUT
                               : ER_RPL_REPLICA_IO_THREAD_STOP_CMD_EXEC_TIMEOUT;
      LogErr(WARNING_LEVEL, log_errno);
    }
    if (net_report) my_error(slave_errno, MYF(0));
    return 1;
  } else if (net_report && for_one_channel)
    my_ok(thd);

  return 0;
}

/**
  Execute a RESET REPLICA (for all channels), used in Multisource replication.
  If resetting of a particular channel fails, it exits out.

  @param[in]  thd  THD object of the client.

  @retval     0    success
  @retval     1    error
 */

int reset_slave(THD *thd) {
  DBUG_TRACE;

  channel_map.assert_some_wrlock();

  Master_info *mi = nullptr;
  int result = 0;
  mi_map::iterator it, gr_channel_map_it;
  if (thd->lex->reset_replica_info.all) {
    /* First do reset_slave for default channel */
    mi = channel_map.get_default_channel_mi();
    if (mi && reset_slave(thd, mi, thd->lex->reset_replica_info.all)) return 1;
    /* Do while iteration for rest of the channels */
    it = channel_map.begin();
    while (it != channel_map.end()) {
      if (!it->first.compare(channel_map.get_default_channel())) {
        it++;
        continue;
      }
      mi = it->second;
      assert(mi);
      if ((result = reset_slave(thd, mi, thd->lex->reset_replica_info.all)))
        break;
      it = channel_map.begin();
    }
    /* RESET group replication specific channels */
    gr_channel_map_it = channel_map.begin(GROUP_REPLICATION_CHANNEL);
    while (gr_channel_map_it != channel_map.end(GROUP_REPLICATION_CHANNEL)) {
      mi = gr_channel_map_it->second;
      assert(mi);
      /*
        We cannot RESET a group replication channel while the group
        replication is running.
      */
      if (is_group_replication_running()) {
        my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
                 "RESET REPLICA ALL FOR CHANNEL", mi->get_channel());
        return 1;
      }
      if ((result = reset_slave(thd, mi, thd->lex->reset_replica_info.all)))
        break;
      gr_channel_map_it = channel_map.begin(GROUP_REPLICATION_CHANNEL);
    }
  } else {
    it = channel_map.begin();
    while (it != channel_map.end()) {
      mi = it->second;
      assert(mi);
      if ((result = reset_slave(thd, mi, thd->lex->reset_replica_info.all)))
        break;
      it++;
    }
    /*
      RESET group replication specific channels.

      We cannot RESET a group replication channel while the group
      replication is running.
    */
    gr_channel_map_it = channel_map.begin(GROUP_REPLICATION_CHANNEL);
    while (gr_channel_map_it != channel_map.end(GROUP_REPLICATION_CHANNEL)) {
      mi = gr_channel_map_it->second;
      assert(mi);
      if (is_group_replication_running()) {
        my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
                 "RESET REPLICA FOR CHANNEL", mi->get_channel());
        return 1;
      }
      if ((result = reset_slave(thd, mi, thd->lex->reset_replica_info.all)))
        break;
      gr_channel_map_it++;
    }
  }
  return result;
}

/**
  Execute a RESET REPLICA statement.
  Locks slave threads and unlocks the slave threads after executing
  reset replica.
  The method also takes the mi->channel_wrlock; if this {mi} object
  is deleted (when the parameter reset_all is true) its destructor unlocks
  the lock. In case of error, the method shall always unlock the
  mi channel lock.

  @param thd        Pointer to THD object of the client thread executing the
                    statement.

  @param mi         Pointer to Master_info object for the slave.

  @param reset_all  Do a full reset or only clean master info structures

  @retval 0   success
  @retval !=0 error
*/
int reset_slave(THD *thd, Master_info *mi, bool reset_all) {
  int thread_mask = 0, error = 0;
  const char *errmsg = "Unknown error occurred while reseting replica";
  DBUG_TRACE;

  bool is_default_channel =
      strcmp(mi->get_channel(), channel_map.get_default_channel()) == 0;

  /*
    RESET REPLICA command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.slave_master_info' and
    'mysql.slave_relay_log_info' replication repository tables.
  */
  thd->set_skip_readonly_check();
  mi->channel_wrlock();

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask, mi, false /* not inverse */,
                   true /* ignore_monitor_thread */);
  if (thread_mask)  // We refuse if any slave thread is running
  {
    my_error(ER_REPLICA_CHANNEL_MUST_STOP, MYF(0), mi->get_channel());
    error = ER_REPLICA_CHANNEL_MUST_STOP;
    unlock_slave_threads(mi);
    mi->channel_unlock();
    goto err;
  }

  ha_reset_slave(thd);

  // delete relay logs, clear relay log coordinates
  if ((error = mi->rli->purge_relay_logs(thd, &errmsg,
                                         reset_all && !is_default_channel))) {
    my_error(ER_RELAY_LOG_FAIL, MYF(0), errmsg);
    error = ER_RELAY_LOG_FAIL;
    unlock_slave_threads(mi);
    mi->channel_unlock();
    goto err;
  }

  // Reset statistics
  mi->rli->get_applier_metrics().reset();

  for (size_t idx = 0; idx < mi->rli->get_worker_count(); idx++) {
    mi->rli->get_worker(idx)->get_worker_metrics().reset();
  }

  assert(!mi->rli || !mi->rli->slave_running);  // none writes in rli table
  if ((reset_all && remove_info(mi)) ||  // Removes all repository information.
      (!reset_all && reset_info(mi))) {  // Resets log names, positions, etc,
                                         // but keeps configuration information
                                         // needed for a re-connection.
    error = ER_UNKNOWN_ERROR;
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    unlock_slave_threads(mi);
    mi->channel_unlock();
    goto err;
  }
  unlock_slave_threads(mi);

  (void)RUN_HOOK(binlog_relay_io, after_reset_slave, (thd, mi));

  /*
     RESET REPLICA ALL deletes the channels(except default channel), so their mi
     and rli objects are removed. For default channel, its mi and rli are
     deleted and recreated to keep in clear status.
  */
  if (reset_all) {
    bool is_default =
        !strcmp(mi->get_channel(), channel_map.get_default_channel());

    rpl_acf_configuration_handler->delete_channel_status(
        mi->get_channel(),
        Rpl_acf_status_configuration::SOURCE_CONNECTION_AUTO_FAILOVER);

    // delete_mi will call mi->channel_unlock in case it succeeds
    if (channel_map.delete_mi(mi->get_channel())) {
      mi->channel_unlock();
      error = ER_UNKNOWN_ERROR;
      my_error(ER_UNKNOWN_ERROR, MYF(0));
      goto err;
    }

    if (is_default) {
      if (!Rpl_info_factory::create_mi_and_rli_objects(
              INFO_REPOSITORY_TABLE, INFO_REPOSITORY_TABLE,
              channel_map.get_default_channel(), &channel_map)) {
        error = ER_CONNECTION_METADATA;
        my_message(ER_CONNECTION_METADATA, ER_THD(thd, ER_CONNECTION_METADATA),
                   MYF(0));
      }
    }
  } else {
    mi->channel_unlock();
  }

err:
  return error;
}

/**
  Entry function for RESET REPLICA command. Function either resets
  the slave for all channels or for a single channel.
  When RESET REPLICA ALL is given, the slave_info_objects (mi, rli & workers)
  are destroyed.

  @param[in]           thd          the client thread with the command.

  @retval              false        OK
  @retval              true         not OK
*/
bool reset_slave_cmd(THD *thd) {
  DBUG_TRACE;

  Master_info *mi;
  LEX *lex = thd->lex;
  bool res = true;  // default, an error

  channel_map.wrlock();

  if (!is_slave_configured()) {
    my_error(ER_REPLICA_CONFIGURATION, MYF(0));
    channel_map.unlock();
    return res = true;
  }

  if (!lex->mi.for_channel)
    res = reset_slave(thd);
  else {
    mi = channel_map.get_mi(lex->mi.channel);
    /*
      If the channel being used is a group replication channel and
      group_replication is still running we need to disable RESET REPLICA [ALL]
      command.
    */
    if (mi &&
        channel_map.is_group_replication_applier_channel_name(
            mi->get_channel()) &&
        is_group_replication_running()) {
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
               "RESET REPLICA [ALL] FOR CHANNEL", mi->get_channel());
      channel_map.unlock();
      return true;
    }

    if (mi)
      res = reset_slave(thd, mi, thd->lex->reset_replica_info.all);
    else if (strcmp(channel_map.get_default_channel(), lex->mi.channel))
      my_error(ER_REPLICA_CHANNEL_DOES_NOT_EXIST, MYF(0), lex->mi.channel);
  }

  channel_map.unlock();

  return res;
}

/**
  This function checks if the given CHANGE REPLICATION SOURCE command
  has any receive option being set or changed.

  - used in change_master().

  @param  lex_mi structure that holds all options given on the
          change replication source command.

  @retval false No change replication source receive options were found.
  @retval true  At least one receive option was found.
*/
static bool have_change_replication_source_receive_option(
    const LEX_SOURCE_INFO *lex_mi) {
  bool have_receive_option = false;

  DBUG_TRACE;

  /* Check if *at least one* receive option is given the command*/
  if (lex_mi->host || lex_mi->user || lex_mi->password ||
      lex_mi->log_file_name || lex_mi->pos || lex_mi->bind_addr ||
      lex_mi->network_namespace || lex_mi->port || lex_mi->connect_retry ||
      lex_mi->server_id || lex_mi->ssl != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_verify_server_cert != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->heartbeat_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->retry_count_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_key || lex_mi->ssl_cert || lex_mi->ssl_ca ||
      lex_mi->ssl_capath || lex_mi->tls_version ||
      lex_mi->tls_ciphersuites != LEX_SOURCE_INFO::UNSPECIFIED ||
      lex_mi->ssl_cipher || lex_mi->ssl_crl || lex_mi->ssl_crlpath ||
      lex_mi->repl_ignore_server_ids_opt == LEX_SOURCE_INFO::LEX_MI_ENABLE ||
      lex_mi->public_key_path ||
      lex_mi->get_public_key != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->zstd_compression_level || lex_mi->compression_algorithm ||
      lex_mi->require_row_format != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    have_receive_option = true;

  return have_receive_option;
}

/**
  This function checks if the given CHANGE REPLICATION SOURCE command
  has any execute option being set or changed.

  - used in change_master().

  @param  lex_mi structure that holds all options given on the
          change replication source command.

  @param[out] need_relay_log_purge
              - If relay_log_file/relay_log_pos options are used,
                we won't delete relaylogs. We set this boolean flag to false.
              - If relay_log_file/relay_log_pos options are NOT used,
                we return the boolean flag UNCHANGED.
              - Used in change_receive_options() and change_master().

  @retval false No change replication source execute option.
  @retval true  At least one execute option was there.
*/
static bool have_change_replication_source_execute_option(
    const LEX_SOURCE_INFO *lex_mi, bool *need_relay_log_purge) {
  bool have_execute_option = false;

  DBUG_TRACE;

  /* Check if *at least one* execute option is given on change replication
   * source command*/
  if (lex_mi->relay_log_name || lex_mi->relay_log_pos ||
      lex_mi->sql_delay != -1 || lex_mi->privilege_checks_username != nullptr ||
      lex_mi->privilege_checks_none ||
      lex_mi->require_row_format != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->require_table_primary_key_check !=
          LEX_SOURCE_INFO::LEX_MI_PK_CHECK_UNCHANGED)
    have_execute_option = true;

  if (lex_mi->relay_log_name || lex_mi->relay_log_pos)
    *need_relay_log_purge = false;

  return have_execute_option;
}

/**
   This function checks if the given CHANGE REPLICATION SOURCE command has
   any option that affect both the receiver and the applier.

   - used in change_master().

  @param  lex_mi structure that holds all options given on the
          change replication source command.

  @retval false no option that affects both applier and receiver was found
  @retval true  At least one option affects both the applier and receiver.
*/
static bool have_change_replication_source_applier_and_receive_option(
    const LEX_SOURCE_INFO *lex_mi) {
  bool have_applier_receive_option = false;

  DBUG_TRACE;

  /* Check if *at least one* receive option is given to change rep source*/
  if (lex_mi->assign_gtids_to_anonymous_transactions_type !=
          LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED ||
      lex_mi->auto_position != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->m_source_connection_auto_failover !=
          LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->m_gtid_only != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    have_applier_receive_option = true;

  return have_applier_receive_option;
}

/**
   This function checks all possible cases in which compression algorithm,
   compression level can be configured for a channel.

   - used in change_receive_options

   @param  lex_mi      pointer to structure holding all options specified
                       as part of change replication source to statement
   @param  mi          pointer to structure holding all options specified
                       as part of change replication source to statement after
   performing necessary checks

   @retval false    in case of success
   @retval true     in case of failures
*/
static bool change_master_set_compression(THD *, const LEX_SOURCE_INFO *lex_mi,
                                          Master_info *mi) {
  DBUG_TRACE;

  if (lex_mi->compression_algorithm) {
    if (validate_compression_attributes(lex_mi->compression_algorithm,
                                        lex_mi->channel, false))
      return true;
    assert(sizeof(mi->compression_algorithm) >
           strlen(lex_mi->compression_algorithm));
    strcpy(mi->compression_algorithm, lex_mi->compression_algorithm);
  }
  /* level specified */
  if (lex_mi->zstd_compression_level) {
    /* vaildate compression level */
    if (!is_zstd_compression_level_valid(lex_mi->zstd_compression_level)) {
      my_error(ER_CHANGE_SOURCE_WRONG_COMPRESSION_LEVEL_CLIENT, MYF(0),
               lex_mi->zstd_compression_level, lex_mi->channel);
      return true;
    }
    mi->zstd_compression_level = lex_mi->zstd_compression_level;
  }
  return false;
}

/**
   This function is called if the change replication source command had at least
  one receive option. This function then sets or alters the receive option(s)
   given in the command. The execute options are handled in the function
   change_execute_options()

   - used in change_master().
   - Receiver threads should be stopped when this function is called.

  @param thd    Pointer to THD object for the client thread executing the
                statement.

  @param lex_mi structure that holds all change replication source options given
  on the change replication source command. Coming from the an executing
  statement or set directly this shall contain connection settings like
  hostname, user, password and other settings like the number of connection
  retries.

  @param mi     Pointer to Master_info object belonging to the replica channel
                to be configured

  @retval 0    no error i.e., success.
  @retval !=0  error.
*/
static int change_receive_options(THD *thd, LEX_SOURCE_INFO *lex_mi,
                                  Master_info *mi) {
  int ret = 0; /* return value. Set if there is an error. */

  DBUG_TRACE;

  /*
    If the user specified host or port without binlog or position,
    reset binlog's name to FIRST and position to 4.
  */

  if ((lex_mi->host && strcmp(lex_mi->host, mi->host)) ||
      (lex_mi->port && lex_mi->port != mi->port)) {
    /*
      This is necessary because the primary key, i.e. host or port, has
      changed.

      The repository does not support direct changes on the primary key,
      so the row is dropped and re-inserted with a new primary key. If we
      don't do that, the master info repository we will end up with several
      rows.
    */
    if (mi->clean_info()) {
      ret = 1;
      goto err;
    }
    mi->master_uuid[0] = 0;
    mi->master_id = 0;
  }

  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name &&
      !lex_mi->pos) {
    char *var_master_log_name = nullptr;
    var_master_log_name = const_cast<char *>(mi->get_master_log_name());
    var_master_log_name[0] = '\0';
    mi->set_master_log_pos(BIN_LOG_HEADER_SIZE);
  }

  if (lex_mi->log_file_name) mi->set_master_log_name(lex_mi->log_file_name);
  if (lex_mi->pos) {
    mi->set_master_log_pos(lex_mi->pos);
  }

  if (lex_mi->log_file_name && !lex_mi->pos)
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARN_ONLY_SOURCE_LOG_FILE_NO_POS,
                 ER_THD(thd, ER_WARN_ONLY_SOURCE_LOG_FILE_NO_POS));

  DBUG_PRINT("info", ("source_log_pos: %lu", (ulong)mi->get_master_log_pos()));

  if (lex_mi->user || lex_mi->password) {
    if (!thd->get_ssl()) {
      push_warning(thd, Sql_condition::SL_NOTE, ER_INSECURE_PLAIN_TEXT,
                   ER_THD(thd, ER_INSECURE_PLAIN_TEXT));
    }
    push_warning(thd, Sql_condition::SL_NOTE, ER_INSECURE_CHANGE_SOURCE,
                 ER_THD(thd, ER_INSECURE_CHANGE_SOURCE));
  }

  if (lex_mi->user) mi->set_user(lex_mi->user);
  if (lex_mi->password) mi->set_password(lex_mi->password);
  if (lex_mi->host) strmake(mi->host, lex_mi->host, sizeof(mi->host) - 1);
  if (lex_mi->bind_addr)
    strmake(mi->bind_addr, lex_mi->bind_addr, sizeof(mi->bind_addr) - 1);

  if (lex_mi->network_namespace)
    strmake(mi->network_namespace, lex_mi->network_namespace,
            sizeof(mi->network_namespace) - 1);
  /*
    Setting channel's port number explicitly to '0' should be allowed.
    Eg: 'group_replication_recovery' channel (*after recovery is done*)
    or 'group_replication_applier' channel wants to set the port number
    to '0' as there is no actual network usage on these channels.
  */
  if (lex_mi->port || lex_mi->port_opt == LEX_SOURCE_INFO::LEX_MI_ENABLE)
    mi->port = lex_mi->port;
  if (lex_mi->connect_retry) mi->connect_retry = lex_mi->connect_retry;
  if (lex_mi->retry_count_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    mi->retry_count = lex_mi->retry_count;

  if (lex_mi->heartbeat_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    mi->heartbeat_period = lex_mi->heartbeat_period;
  else if (lex_mi->host || lex_mi->port) {
    /*
      If the user specified host or port or both without heartbeat_period,
      we use default value for heartbeat_period. By default, We want to always
      have heartbeat enabled when we switch master unless
      source_heartbeat_period is explicitly set to zero (heartbeat disabled).

      Here is the default value for heartbeat period if CHANGE REPLICATION
      SOURCE did not specify it.  (no data loss in conversion as hb period has a
      max)
    */
    mi->heartbeat_period = std::min<float>(REPLICA_MAX_HEARTBEAT_PERIOD,
                                           (replica_net_timeout / 2.0f));
    assert(mi->heartbeat_period > (float)0.001 || mi->heartbeat_period == 0);

    // counter is cleared if master is CHANGED.
    mi->received_heartbeats = 0;
    // clear timestamp of last heartbeat as well.
    mi->last_heartbeat = 0;
  }

  /*
    reset the last time server_id list if the current CHANGE REPLICATION SOURCE
    is mentioning IGNORE_SERVER_IDS= (...)
  */
  if (lex_mi->repl_ignore_server_ids_opt == LEX_SOURCE_INFO::LEX_MI_ENABLE)
    mi->ignore_server_ids->dynamic_ids.clear();
  for (size_t i = 0; i < lex_mi->repl_ignore_server_ids.size(); i++) {
    ulong s_id = lex_mi->repl_ignore_server_ids[i];
    if (s_id == ::server_id && replicate_same_server_id) {
      ret = ER_REPLICA_IGNORE_SERVER_IDS;
      my_error(ER_REPLICA_IGNORE_SERVER_IDS, MYF(0), static_cast<int>(s_id));
      goto err;
    } else {
      // Keep the array sorted, ignore duplicates.
      mi->ignore_server_ids->dynamic_ids.insert_unique(s_id);
    }
  }

  if (lex_mi->ssl != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    mi->ssl = (lex_mi->ssl == LEX_SOURCE_INFO::LEX_MI_ENABLE);

  if (lex_mi->ssl_verify_server_cert != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    mi->ssl_verify_server_cert =
        (lex_mi->ssl_verify_server_cert == LEX_SOURCE_INFO::LEX_MI_ENABLE);

  if (lex_mi->public_key_path)
    strmake(mi->public_key_path, lex_mi->public_key_path,
            sizeof(mi->public_key_path) - 1);

  if (lex_mi->get_public_key != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    mi->get_public_key =
        (lex_mi->get_public_key == LEX_SOURCE_INFO::LEX_MI_ENABLE);

  if (lex_mi->ssl_ca)
    strmake(mi->ssl_ca, lex_mi->ssl_ca, sizeof(mi->ssl_ca) - 1);
  if (lex_mi->ssl_capath)
    strmake(mi->ssl_capath, lex_mi->ssl_capath, sizeof(mi->ssl_capath) - 1);
  if (lex_mi->tls_version)
    strmake(mi->tls_version, lex_mi->tls_version, sizeof(mi->tls_version) - 1);

  if (LEX_SOURCE_INFO::SPECIFIED_NULL == lex_mi->tls_ciphersuites) {
    mi->tls_ciphersuites.first = true;
    mi->tls_ciphersuites.second.clear();
  } else if (LEX_SOURCE_INFO::SPECIFIED_STRING == lex_mi->tls_ciphersuites) {
    mi->tls_ciphersuites.first = false;
    mi->tls_ciphersuites.second.assign(lex_mi->tls_ciphersuites_string);
  }

  if (lex_mi->ssl_cert)
    strmake(mi->ssl_cert, lex_mi->ssl_cert, sizeof(mi->ssl_cert) - 1);
  if (lex_mi->ssl_cipher)
    strmake(mi->ssl_cipher, lex_mi->ssl_cipher, sizeof(mi->ssl_cipher) - 1);
  if (lex_mi->ssl_key)
    strmake(mi->ssl_key, lex_mi->ssl_key, sizeof(mi->ssl_key) - 1);
  if (lex_mi->ssl_crl)
    strmake(mi->ssl_crl, lex_mi->ssl_crl, sizeof(mi->ssl_crl) - 1);
  if (lex_mi->ssl_crlpath)
    strmake(mi->ssl_crlpath, lex_mi->ssl_crlpath, sizeof(mi->ssl_crlpath) - 1);

  ret = change_master_set_compression(thd, lex_mi, mi);
  if (ret) goto err;

err:
  return ret;
}

/**
   This function is called if the change replication source command had at least
  one execute option. This function then sets or alters the execute option(s)
   given in the command. The receive options are handled in the function
   change_receive_options()

   - used in change_master().
   - Execute threads should be stopped before this function is called.

  @param lex_mi structure that holds all change replication source options given
  on the change replication source command.

  @param mi     Pointer to Master_info object belonging to the replica channel
                that will be configured

  @return       false if the execute options were successfully set and true,
                otherwise.
*/
static bool change_execute_options(LEX_SOURCE_INFO *lex_mi, Master_info *mi) {
  DBUG_TRACE;

  if (lex_mi->privilege_checks_username != nullptr ||
      lex_mi->privilege_checks_none) {
    Relay_log_info::enum_priv_checks_status error{
        mi->rli->set_privilege_checks_user(
            lex_mi->privilege_checks_username,
            lex_mi->privilege_checks_none ? nullptr
                                          : lex_mi->privilege_checks_hostname)};
    if (!!error) {
      mi->rli->report_privilege_check_error(
          ERROR_LEVEL, error, true /* to client*/, mi->rli->get_channel(),
          lex_mi->privilege_checks_username, lex_mi->privilege_checks_hostname);
      return true;
    }
  }

  if (lex_mi->require_row_format != LEX_SOURCE_INFO::LEX_MI_UNCHANGED) {
    mi->rli->set_require_row_format(lex_mi->require_row_format ==
                                    LEX_SOURCE_INFO::LEX_MI_ENABLE);
  }

  if (lex_mi->require_table_primary_key_check !=
      LEX_SOURCE_INFO::LEX_MI_PK_CHECK_UNCHANGED) {
    switch (lex_mi->require_table_primary_key_check) {
      case (LEX_SOURCE_INFO::LEX_MI_PK_CHECK_STREAM):
        mi->rli->set_require_table_primary_key_check(
            Relay_log_info::PK_CHECK_STREAM);
        break;
      case (LEX_SOURCE_INFO::LEX_MI_PK_CHECK_ON):
        mi->rli->set_require_table_primary_key_check(
            Relay_log_info::PK_CHECK_ON);
        break;
      case (LEX_SOURCE_INFO::LEX_MI_PK_CHECK_OFF):
        mi->rli->set_require_table_primary_key_check(
            Relay_log_info::PK_CHECK_OFF);
        break;
      case (LEX_SOURCE_INFO::LEX_MI_PK_CHECK_GENERATE):
        if (channel_map.is_group_replication_channel_name(lex_mi->channel)) {
          my_error(ER_REQUIRE_TABLE_PRIMARY_KEY_CHECK_GENERATE_WITH_GR, MYF(0));
          return true;
        }
        mi->rli->set_require_table_primary_key_check(
            Relay_log_info::PK_CHECK_GENERATE);
        break;

      default:     /* purecov: tested */
        assert(0); /* purecov: tested */
        break;
    }
  }

  if (lex_mi->relay_log_name) {
    char relay_log_name[FN_REFLEN];
    mi->rli->relay_log.make_log_name(relay_log_name, lex_mi->relay_log_name);
    mi->rli->set_group_relay_log_name(relay_log_name);
    mi->rli->is_group_master_log_pos_invalid = true;
  }

  if (lex_mi->relay_log_pos) {
    mi->rli->set_group_relay_log_pos(lex_mi->relay_log_pos);
    mi->rli->is_group_master_log_pos_invalid = true;
  }

  if (lex_mi->sql_delay != -1) mi->rli->set_sql_delay(lex_mi->sql_delay);

  return false;
}

/**
   This function is called if the change replication source command had at
   least one option that affects both the receiver and applier parts.
   Pure execute option(s) are handled in change_execute_options()
   The receive options are handled in the function change_receive_options()

   - used in change_master().
   - Both receiver and applier threads should be stopped on invocation

  @param lex_mi structure that holds all change replication source options

  @param mi     Pointer to Master_info object belonging to the replica channel
                to be configured

  @return       false if successfully set, true otherwise.
*/
static bool change_applier_receiver_options(THD *thd, LEX_SOURCE_INFO *lex_mi,
                                            Master_info *mi) {
  if (lex_mi->m_source_connection_auto_failover !=
      LEX_SOURCE_INFO::LEX_MI_UNCHANGED) {
    if (lex_mi->m_source_connection_auto_failover ==
        LEX_SOURCE_INFO::LEX_MI_ENABLE) {
      mi->set_source_connection_auto_failover();
      /*
        Send replication channel SOURCE_CONNECTION_AUTO_FAILOVER attribute of
        CHANGE REPLICATION SOURCE command status to group replication group
        members.
      */
      if (rpl_acf_configuration_handler->send_channel_status_and_version_data(
              mi->get_channel(),
              Rpl_acf_status_configuration::SOURCE_CONNECTION_AUTO_FAILOVER,
              1)) {
        my_error(ER_GRP_RPL_FAILOVER_CHANNEL_STATUS_PROPAGATION, MYF(0),
                 mi->get_channel());
        mi->unset_source_connection_auto_failover();
        return true;
      }

      /*
        If IO thread is running and the monitoring thread is not, start
        the monitoring thread.
      */
      if (mi->slave_running &&
          !Source_IO_monitor::get_instance()->is_monitoring_process_running()) {
        if (Source_IO_monitor::get_instance()->launch_monitoring_process(
                key_thread_replica_monitor_io)) {
          my_error(ER_STARTING_REPLICA_MONITOR_IO_THREAD, MYF(0));
          return true;
        }
      }
    } else {
      /*
        If this is the only channel with source_connection_auto_failover,
        then stop the monitoring thread.
      */
      if (mi->is_source_connection_auto_failover() && mi->slave_running &&
          channel_map
                  .get_number_of_connection_auto_failover_channels_running() ==
              1) {
        if (Source_IO_monitor::get_instance()->terminate_monitoring_process()) {
          my_error(ER_STOP_REPLICA_MONITOR_IO_THREAD_TIMEOUT, MYF(0));
          return true;
        }
      }
      mi->unset_source_connection_auto_failover();
      /*
        Send replication channel SOURCE_CONNECTION_AUTO_FAILOVER attribute of
        CHANGE REPLICATION SOURCE command status to group replication group
        members.
      */
      if (rpl_acf_configuration_handler->send_channel_status_and_version_data(
              mi->get_channel(),
              Rpl_acf_status_configuration::SOURCE_CONNECTION_AUTO_FAILOVER,
              0)) {
        my_error(ER_GRP_RPL_FAILOVER_CHANNEL_STATUS_PROPAGATION, MYF(0),
                 mi->get_channel());
        return true;
      }
    }
  }

  if (lex_mi->auto_position != LEX_SOURCE_INFO::LEX_MI_UNCHANGED) {
    mi->set_auto_position(
        (lex_mi->auto_position == LEX_SOURCE_INFO::LEX_MI_ENABLE));
  }

  if (lex_mi->assign_gtids_to_anonymous_transactions_type !=
      LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED) {
    if (lex_mi->assign_gtids_to_anonymous_transactions_type >
        LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_OFF) {
      push_warning(
          thd, Sql_condition::SL_NOTE,
          ER_USING_ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_AS_LOCAL_OR_UUID,
          ER_THD(
              thd,
              ER_USING_ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_AS_LOCAL_OR_UUID));
    }

    switch (lex_mi->assign_gtids_to_anonymous_transactions_type) {
      case (LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_LOCAL):
        mi->rli->m_assign_gtids_to_anonymous_transactions_info.set_info(
            Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_LOCAL,
            ::server_uuid);
        break;
      case (LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UUID):
        if (mi->rli->m_assign_gtids_to_anonymous_transactions_info.set_info(
                Assign_gtids_to_anonymous_transactions_info::enum_type::
                    AGAT_UUID,
                lex_mi->assign_gtids_to_anonymous_transactions_manual_uuid))
          return true;
        break;
      case (LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_OFF):
        mi->rli->m_assign_gtids_to_anonymous_transactions_info.set_info(
            Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF,
            "");
        break;
      default:
        assert(0);
        break;
    }
  }

  if (lex_mi->m_gtid_only != LEX_SOURCE_INFO::LEX_MI_UNCHANGED) {
    mi->set_gtid_only_mode(
        (lex_mi->m_gtid_only == LEX_SOURCE_INFO::LEX_MI_ENABLE));
  }

  return false;
}

/**
  This function validates that change replication source options are
  valid according to the current GTID_MODE.
  This method assumes it will only be called when GTID_MODE != ON

  @param lex_mi structure that holds all change replication source options

  @param mi     Pointer to Master_info object belonging to the replica channel
                to be configured

  @return       false if the configuration is valid
                true  some configuration option is invalid with GTID_MODE
*/
static int validate_gtid_option_restrictions(const LEX_SOURCE_INFO *lex_mi,
                                             Master_info *mi) {
  int error = 0;

  /*
    CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1 requires
      GTID_MODE != OFF
  */
  if (global_gtid_mode.get() == Gtid_mode::OFF) {
    if (lex_mi->auto_position == LEX_SOURCE_INFO::LEX_MI_ENABLE) {
      error = ER_AUTO_POSITION_REQUIRES_GTID_MODE_NOT_OFF;
      my_error(ER_AUTO_POSITION_REQUIRES_GTID_MODE_NOT_OFF, MYF(0));
      return error;
    }
  }

  /*
    CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS != OFF
      requires GTID_MODE = ON
  */
  if (lex_mi->assign_gtids_to_anonymous_transactions_type >
      LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_OFF) {
    error = ER_ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_REQUIRES_GTID_MODE_ON;
    my_error(ER_ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_REQUIRES_GTID_MODE_ON,
             MYF(0));
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO GTID_ONLY= 1 requires
      GTID_MODE = ON
  */
  if (lex_mi->m_gtid_only == LEX_SOURCE_INFO::LEX_MI_ENABLE) {
    error = ER_CHANGE_REPLICATION_SOURCE_NO_OPTIONS_FOR_GTID_ONLY;
    my_error(ER_CHANGE_REPLICATION_SOURCE_NO_OPTIONS_FOR_GTID_ONLY, MYF(0),
             mi->get_channel());
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO SOURCE_CONNECTION_AUTO_FAILOVER = 1 requires
      GTID_MODE = ON
  */
  if (lex_mi->m_source_connection_auto_failover ==
      LEX_SOURCE_INFO::LEX_MI_ENABLE) {
    error = ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF;
    my_error(ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF, MYF(0));
    return error;
  }

  if (channel_map.is_group_replication_channel_name(lex_mi->channel)) {
    error = ER_CHANGE_REP_SOURCE_GR_CHANNEL_WITH_GTID_MODE_NOT_ON;
    my_error(error, MYF(0));
    return error;
  }

  return error;
}

/**
  This is an helper method for boolean vars like
    SOURCE_AUTO_POSITION
    REQUIRE_ROW_FORMAT
    SOURCE_CONNECTION_AUTO_FAILOVER
  It tells if the variable is already enabled or will be by the command

  @param base_value the current variable value
  @param option_value the configuration input value (UNCHANGED,ENABLED,DISABLE)

  @return true if the option was already enable or will be. false otherwise
*/
bool is_option_enabled_or_will_be(bool base_value, int option_value) {
  bool var_enabled = base_value;
  switch (option_value) {
    case LEX_SOURCE_INFO::LEX_MI_ENABLE:
      var_enabled = true;
      break;
    case LEX_SOURCE_INFO::LEX_MI_DISABLE:
      var_enabled = false;
      break;
    case LEX_SOURCE_INFO::LEX_MI_UNCHANGED:
      break;
    default:
      assert(0);
      break;
  }
  return var_enabled;
}

/**
  This method evaluates if the different options given to
    CHANGE REPLICATION SOURCE TO
  are compatible with the current configuration and with one another.

  Example: SOURCE_CONNECTION_AUTO_FAILOVER = 1 requires
  SOURCE_AUTO_POSITION to be already enabled or to be enabled on this command.

  @param lex_mi structure that holds all change replication source options given
                on the command

  @param mi     Pointer to Master_info object for the channel that holds the
                the configuration

  @return 0     if no issues are found
          != 0  the error number associated to the issue, if one is found
*/
int evaluate_inter_option_dependencies(const LEX_SOURCE_INFO *lex_mi,
                                       Master_info *mi) {
  int error = 0;

  /**
    We first define the variables used and then we group the checks for
    readability
  */
  bool is_or_will_auto_position_be_enabled = is_option_enabled_or_will_be(
      mi->is_auto_position(), lex_mi->auto_position);
  bool will_auto_position_be_disable =
      mi->is_auto_position() &&
      lex_mi->auto_position == LEX_SOURCE_INFO::LEX_MI_DISABLE;

  bool is_or_will_require_row_format_be_enabled = is_option_enabled_or_will_be(
      mi->rli->is_row_format_required(), lex_mi->require_row_format);
  bool will_require_row_format_be_disable =
      mi->rli->is_row_format_required() &&
      lex_mi->require_row_format == LEX_SOURCE_INFO::LEX_MI_DISABLE;

  bool is_or_will_source_connection_auto_failover_be_enabled =
      is_option_enabled_or_will_be(mi->is_source_connection_auto_failover(),
                                   lex_mi->m_source_connection_auto_failover);

  bool is_or_will_gtid_only_be_enabled = is_option_enabled_or_will_be(
      mi->is_gtid_only_mode(), lex_mi->m_gtid_only);
  bool will_gtid_only_mode_be_disable =
      mi->is_gtid_only_mode() &&
      lex_mi->m_gtid_only == LEX_SOURCE_INFO::LEX_MI_DISABLE;

  auto assign_gtids_to_anonymous_transactions_type =
      mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type();
  switch (lex_mi->assign_gtids_to_anonymous_transactions_type) {
    case LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_OFF:
      assign_gtids_to_anonymous_transactions_type =
          Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF;
      break;
    case LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_LOCAL:
      assign_gtids_to_anonymous_transactions_type =
          Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_LOCAL;
      break;
    case LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UUID:
      assign_gtids_to_anonymous_transactions_type =
          Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_UUID;
      break;
    case LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED:
      break;
    default:
      assert(0);
      break;
  }

  /* Check phase - enabling options */

  /*
    We cannot specify auto position and set either the coordinates
    on source or replica. If we try to do so, an error message is
    printed out.
  */
  if (lex_mi->log_file_name != nullptr || lex_mi->pos != 0 ||
      lex_mi->relay_log_name != nullptr || lex_mi->relay_log_pos != 0) {
    if (lex_mi->auto_position == LEX_SOURCE_INFO::LEX_MI_ENABLE ||
        (lex_mi->auto_position != LEX_SOURCE_INFO::LEX_MI_DISABLE &&
         mi->is_auto_position())) {
      error = ER_BAD_REPLICA_AUTO_POSITION;
      my_error(error, MYF(0));
      return error;
    }
  }

  /*
   CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS != OFF
   requires
      SOURCE_AUTO_POSITION = 0
  */
  if (assign_gtids_to_anonymous_transactions_type !=
          Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF &&
      is_or_will_auto_position_be_enabled) {
    error = ER_CANT_COMBINE_ANONYMOUS_TO_GTID_AND_AUTOPOSITION;
    my_error(error, MYF(0));
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO GTID_ONLY = 1 requires
      SOURCE_AUTO_POSITION = 1
      REQUIRE_ROW_FORMAT = 1
   */
  if (lex_mi->m_gtid_only == LEX_SOURCE_INFO::LEX_MI_ENABLE &&
      (!is_or_will_auto_position_be_enabled ||
       !is_or_will_require_row_format_be_enabled)) {
    error = ER_CHANGE_REPLICATION_SOURCE_NO_OPTIONS_FOR_GTID_ONLY;
    my_error(error, MYF(0), mi->get_channel());
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO SOURCE_CONNECTION_AUTO_FAILOVER = 1 requires
      SOURCE_AUTO_POSITION = 1
  */
  if (lex_mi->m_source_connection_auto_failover ==
          LEX_SOURCE_INFO::LEX_MI_ENABLE &&
      !is_or_will_auto_position_be_enabled) {
    error = ER_RPL_ASYNC_RECONNECT_AUTO_POSITION_OFF;
    my_error(error, MYF(0));
    return error;
  }

  /*
    We need to check if there is an empty source_host. Otherwise
    change replication source succeeds, a master.info file is created containing
    empty source_host string and when issuing: start replica; an error
    is thrown stating that the server is not configured as replica.
    (See BUG#28796).
  */
  if (lex_mi->host && !*lex_mi->host) {
    error = ER_WRONG_ARGUMENTS;
    my_error(error, MYF(0), "SOURCE_HOST");
    return error;
  }

  /*
    Changing source_connection_auto_failover option is not allowed on group
    secondary member.
  */
  if (lex_mi->m_source_connection_auto_failover !=
          LEX_SOURCE_INFO::LEX_MI_UNCHANGED &&
      is_group_replication_member_secondary()) {
    error = ER_OPERATION_NOT_ALLOWED_ON_GR_SECONDARY;
    my_error(error, MYF(0));
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS != OFF
    can't use the same value as the group replication name or view change uuid
  */
  if (lex_mi->assign_gtids_to_anonymous_transactions_type >
      LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_OFF) {
    std::string group_name = get_group_replication_group_name();
    if (group_name.length() > 0) {
      bool is_same = false;
      auto type = lex_mi->assign_gtids_to_anonymous_transactions_type;
      if (type == LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_LOCAL)
        if (!(group_name.compare(::server_uuid))) is_same = true;
      if (type == LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UUID)
        if (!(group_name.compare(
                lex_mi->assign_gtids_to_anonymous_transactions_manual_uuid)))
          is_same = true;
      if (is_same) {
        error = ER_CANT_USE_SAME_UUID_AS_GROUP_NAME;
        my_error(error, MYF(0));
        return error;
      }

      std::string view_change_uuid;
      if (get_group_replication_view_change_uuid(view_change_uuid)) {
        /* purecov: begin inspected */
        error = ER_GRP_RPL_VIEW_CHANGE_UUID_FAIL_GET_VARIABLE;
        my_error(error, MYF(0));
        return error;
        /* purecov: end */
      } else {
        if (type == LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_LOCAL)
          if (!(view_change_uuid.compare(::server_uuid))) is_same = true;
        if (type == LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UUID)
          if (!(view_change_uuid.compare(
                  lex_mi->assign_gtids_to_anonymous_transactions_manual_uuid)))
            is_same = true;
        if (is_same) {
          error = ER_CANT_USE_SAME_UUID_AS_VIEW_CHANGE_UUID;
          my_error(error, MYF(0));
          return error;
        }
      }
    }
  }

  /* Check phase - disabling options */

  /*
    CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS
      auto_position cannot be disable if either source_connection_auto_failover
      option is enabled or getting enabled in current CHANGE REPLICATION SOURCE
    statement.
  */
  if (will_auto_position_be_disable &&
      is_or_will_source_connection_auto_failover_be_enabled) {
    error = ER_DISABLE_AUTO_POSITION_REQUIRES_ASYNC_RECONNECT_OFF;
    my_error(error, MYF(0));
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 cannot be done when
      GTID_ONLY = 1
  */
  if (will_auto_position_be_disable && is_or_will_gtid_only_be_enabled) {
    error = ER_CHANGE_REP_SOURCE_CANT_DISABLE_AUTO_POSITION_WITH_GTID_ONLY;
    my_error(error, MYF(0), mi->get_channel());
    return error;
  }
  /*
    CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0 cannot be done when
      GTID_ONLY = 1
  */
  if (will_require_row_format_be_disable && is_or_will_gtid_only_be_enabled) {
    error = ER_CHANGE_REP_SOURCE_CANT_DISABLE_REQ_ROW_FORMAT_WITH_GTID_ONLY;
    my_error(error, MYF(0), mi->get_channel());
    return error;
  }

  /*
    CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 when
    source positions in relation to the source are invalid.
    This requires `SOURCE_LOG_FILE` and `SOURCE_LOG_POS`
    The message varies if you are also disabling `GTID_ONLY`
  */
  if (will_auto_position_be_disable) {
    if (mi->is_receiver_position_info_invalid()) {
      if (lex_mi->log_file_name == nullptr || lex_mi->pos == 0) {
        if (will_gtid_only_mode_be_disable) {
          error = ER_CHANGE_REP_SOURCE_CANT_DISABLE_GTID_ONLY_WITHOUT_POSITIONS;
          my_error(error, MYF(0), mi->get_channel());
        } else {
          error = ER_CHANGE_REP_SOURCE_CANT_DISABLE_AUTO_POS_WITHOUT_POSITIONS;
          my_error(error, MYF(0), mi->get_channel());
        }
        return error;
      }
    }
  }

  /*
    Emitting error after user executes CHANGE REPLICATION
    SOURCE TO IGNORE_SERVER_IDS if GTID_MODE=ON.
   */
  if (lex_mi->repl_ignore_server_ids.size() > 0 &&
      global_gtid_mode.get() == Gtid_mode::ON) {
    error = ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED;
    my_error(error, MYF(0),
             "CHANGE REPLICATION SOURCE TO ... IGNORE_SERVER_IDS='...'"
             " when @@GLOBAL.GTID_MODE = ON",
             mi->get_channel());
    return error;
  }
  return error;
}

/**
  Log a warning in case GTID_ONLY or SOURCE AUTO POSITION are disabled
  and the server contains invalid positions.

  @param thd the associated thread object

  @param lex_mi structure that holds all change replication source options given
                on the command

  @param mi     Pointer to Master_info object
*/
static void log_invalid_position_warning(THD *thd,
                                         const LEX_SOURCE_INFO *lex_mi,
                                         Master_info *mi) {
  if (lex_mi->m_gtid_only == LEX_SOURCE_INFO::LEX_MI_DISABLE ||
      lex_mi->auto_position == LEX_SOURCE_INFO::LEX_MI_DISABLE) {
    if (mi->is_receiver_position_info_invalid() ||
        mi->rli->is_applier_source_position_info_invalid()) {
      push_warning_printf(
          thd, Sql_condition::SL_WARNING,
          ER_WARN_C_DISABLE_GTID_ONLY_WITH_SOURCE_AUTO_POS_INVALID_POS,
          ER_THD(thd,
                 ER_WARN_C_DISABLE_GTID_ONLY_WITH_SOURCE_AUTO_POS_INVALID_POS),
          mi->get_channel());
      LogErr(WARNING_LEVEL,
             ER_WARN_L_DISABLE_GTID_ONLY_WITH_SOURCE_AUTO_POS_INVALID_POS,
             mi->get_channel());
    }
  }
}

/**
  This method aggregates the validation checks made for the command
    CHANGE REPLICATION SOURCE

  @param thd    Pointer to THD object for the client thread executing the
                statement.

  @param lex_mi structure that holds all change replication source options given
                on the command

  @param mi     Pointer to Master_info object for the channel that holds the
                the configuration

  @param thread_mask  The thread mask identifying which threads are running

  @return A pair of booleans <return_value, remove_mta_info>
          return_value: true if an error occurred, false otherwise
          remove_mta_info: if true remove MTA worker info
*/
static std::pair<bool, bool> validate_change_replication_source_options(
    THD *thd, const LEX_SOURCE_INFO *lex_mi, Master_info *mi, int thread_mask) {
  bool mta_remove_worker_info = false;
  if ((thread_mask & REPLICA_SQL) == 0)  // If execute threads are stopped
  {
    if (mi->rli->mts_recovery_group_cnt) {
      /*
        Change-Master can't be done if there is a mts group gap.
        That requires mts-recovery which START REPLICA provides.
      */
      assert(mi->rli->recovery_parallel_workers);
      my_error(ER_MTA_CHANGE_SOURCE_CANT_RUN_WITH_GAPS, MYF(0));
      return std::make_pair(true, mta_remove_worker_info);
    } else {
      /*
        Lack of mts group gaps makes Workers info stale regardless of
        need_relay_log_purge computation. We set the mta_remove_worker_info
        flag here and call reset_workers() later to delete the worker info
        in mysql.slave_worker_info table.
      */
      if (mi->rli->recovery_parallel_workers) mta_remove_worker_info = true;
    }
  }

  /*
    When give a warning?
    CHANGE REPLICATION SOURCE command is used in three ways:
    a) To change a connection configuration but remain connected to
       the same master.
    b) To change positions in binary or relay log(eg: SOURCE_LOG_POS).
    c) To change the master you are replicating from.
    We give a warning in cases b and c.
  */
  if ((lex_mi->host || lex_mi->port || lex_mi->log_file_name || lex_mi->pos ||
       lex_mi->relay_log_name || lex_mi->relay_log_pos) &&
      (mi->rli->atomic_channel_open_temp_tables > 0))
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO,
                 ER_THD(thd, ER_WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO));

  /**
    Although this check is redone when the user is set, we do an early
    check here to avoid failures in the middle of configuration
  */
  Relay_log_info::enum_priv_checks_status priv_check_error;
  priv_check_error = mi->rli->check_privilege_checks_user(
      lex_mi->privilege_checks_username,
      lex_mi->privilege_checks_none ? nullptr
                                    : lex_mi->privilege_checks_hostname);
  if (!!priv_check_error) {
    mi->rli->report_privilege_check_error(
        ERROR_LEVEL, priv_check_error, true /* to client*/,
        mi->rli->get_channel(), lex_mi->privilege_checks_username,
        lex_mi->privilege_checks_hostname);
    return std::make_pair(true, mta_remove_worker_info);
  }
  return std::make_pair(false, mta_remove_worker_info);
}

/**
  This method aggregates the the instantiation of options for the command
  CHANGE REPLICATION SOURCE

  @param thd    Pointer to THD object for the client thread executing the
                statement.

  @param lex_mi structure that holds all change replication source options given
                on the command

  @param mi     Pointer to Master_info object belonging to the replica channel
                to be configured

  @param have_both_receive_execute_option the command will change options that
                                          affect both the applier and receiver

  @param have_execute_option the command will change applier related options

  @param have_receive_option the command will change receiver related options

  @return returns true if an error occurred, false otherwise
*/
static bool update_change_replication_source_options(
    THD *thd, LEX_SOURCE_INFO *lex_mi, Master_info *mi,
    bool have_both_receive_execute_option, bool have_execute_option,
    bool have_receive_option) {
  if (have_both_receive_execute_option) {
    if (change_applier_receiver_options(thd, lex_mi, mi)) {
      return true;
    }
  }

  if (channel_map.is_group_replication_channel_name(lex_mi->channel)) {
    mi->set_auto_position(true);
    mi->rli->set_require_row_format(true);
    mi->set_gtid_only_mode(true);
  }

  if (have_execute_option && change_execute_options(lex_mi, mi)) return true;

  if (have_receive_option) {
    if (change_receive_options(thd, lex_mi, mi)) {
      return true;
    }
  }

  return false;
}

/**
  Execute a CHANGE REPLICATION SOURCE statement.

  Apart from changing the receive/execute configurations/positions,
  this function also does the following:
  - May leave replicated open temporary table after warning.
  - Purges relay logs if no threads running and no relay log file/pos options.
  - Delete worker info in mysql.slave_worker_info table if applier not running.

  @param thd            Pointer to THD object for the client thread executing
                        the statement.

  @param mi             Pointer to Master_info object belonging to the slave's
                        IO thread.

  @param lex_mi         Lex information with master connection data.
                        Coming from the an executing statement or set directly
                        this shall contain connection settings like hostname,
                        user, password and other settings like the number of
                        connection retries.

  @param preserve_logs  If the decision of purging the logs should be always be
                        false even if no relay log name/position is given to
                        the method. The preserve_logs parameter will not be
                        respected when the relay log info repository is not
                        initialized.

  @retval 0   success
  @retval !=0 error
*/
int change_master(THD *thd, Master_info *mi, LEX_SOURCE_INFO *lex_mi,
                  bool preserve_logs) {
  int error = 0;

  /* Do we have at least one receive related (IO thread) option? */
  bool have_receive_option = false;
  /* Do we have at least one execute related (SQL/coord/worker) option? */
  bool have_execute_option = false;
  /* Do we have at least one option that relates to receival and execution? */
  bool have_both_receive_execute_option = false;
  /** Is there a an error during validation */
  bool validation_error = false;
  /* If there are no mts gaps, we delete the rows in this table. */
  bool mta_remove_worker_info = false;
  /* used as a bit mask to indicate running slave threads. */
  int thread_mask;
  /*
    Relay logs are purged only if both receive and execute threads are
    stopped before executing CHANGE REPLICATION SOURCE and
    relay_log_file/relay_log_pos options are not used.
  */
  bool need_relay_log_purge = true;

  /*
    We want to save the old receive configurations so that we can use them to
    print the changes in these configurations (from-to form). This is used in
    LogErr() later.
  */
  char saved_host[HOSTNAME_LENGTH + 1], saved_bind_addr[HOSTNAME_LENGTH + 1];
  uint saved_port = 0;
  char saved_log_name[FN_REFLEN];
  my_off_t saved_log_pos = 0;

  DBUG_TRACE;

  /*
    CHANGE REPLICATION SOURCE command should ignore 'read-only' and
    'super_read_only' options so that it can update 'mysql.slave_master_info'
    replication repository tables.
  */
  thd->set_skip_readonly_check();
  mi->channel_wrlock();
  /*
    When we change replication source, we first decide which thread is running
    and which is not. We dont want this assumption to break while we change
    replication source.

    Suppose we decide that receiver thread is running and thus it is
    safe to change receive related options in mi. By this time if
    the receive thread is started, we may have a race condition between
    the client thread and receiver thread.
  */
  lock_slave_threads(mi);

  /*
    Get a bit mask for the slave threads that are running.
    Since the third argument is 0, thread_mask after the function
    returns stands for running threads.
  */
  init_thread_mask(&thread_mask, mi, false);

  if (thread_mask) /* If any thread is running */
  {
    /*
      Prior to WL#6120, we imposed the condition that STOP REPLICA is required
      before CHANGE REPLICATION SOURCE. Since the slave threads die on STOP
      REPLICA, it was fine if we purged relay logs.

      Now that we do allow CHANGE REPLICATION SOURCE with a running
      receiver/applier thread, we need to make sure that the relay logs are
      purged only if both receiver and applier threads are stopped otherwise we
      could lose events.

      The idea behind purging relay logs if both the threads are stopped is to
      keep consistency with the old behavior. If the user/application is doing
      a CHANGE REPLICATION SOURCE without stopping any one thread, the relay log
      purge should be controlled via the 'relay_log_purge' option.
    */
    need_relay_log_purge = false;
  }

  /* Check if at least one receive option is given on change replication source
   */
  have_receive_option = have_change_replication_source_receive_option(lex_mi);

  /* Check if at least one execute option is given on change replication source
   */
  have_execute_option = have_change_replication_source_execute_option(
      lex_mi, &need_relay_log_purge);
  /* Check if at least one execute option affects both the applier and receiver
   */
  have_both_receive_execute_option =
      have_change_replication_source_applier_and_receive_option(lex_mi);

  /* If either:
      + An option affects both the applier and receiver and one of the threads
       is running
      + There are receiver and applier options and both threads are running
     Then tell the user the replica must stop
   */
  if ((have_both_receive_execute_option &&
       ((thread_mask & REPLICA_IO) || (thread_mask & REPLICA_SQL))) ||
      (have_receive_option && have_execute_option &&
       (thread_mask & REPLICA_IO) && (thread_mask & REPLICA_SQL))) {
    error = ER_REPLICA_CHANNEL_MUST_STOP;
    my_error(ER_REPLICA_CHANNEL_MUST_STOP, MYF(0), mi->get_channel());
    goto err;
  }

  /* With receiver thread running, we dont allow changing receive options. */
  if (have_receive_option && (thread_mask & REPLICA_IO)) {
    error = ER_REPLICA_CHANNEL_IO_THREAD_MUST_STOP;
    my_error(ER_REPLICA_CHANNEL_IO_THREAD_MUST_STOP, MYF(0), mi->get_channel());
    goto err;
  }

  /* With an execute thread running, we don't allow changing execute options. */
  if (have_execute_option && (thread_mask & REPLICA_SQL)) {
    error = ER_REPLICA_CHANNEL_SQL_THREAD_MUST_STOP;
    my_error(ER_REPLICA_CHANNEL_SQL_THREAD_MUST_STOP, MYF(0),
             mi->get_channel());
    goto err;
  }

  /* If GTID_MODE is different from ON check if some options are invalid
     We hold channel_map lock for the duration of the CHANGE REPLICATION SOURCE.
     This is important since it prevents that a concurrent
     connection changes to GTID_MODE=OFF between this check and the
     point where AUTO_POSITION is stored in the table and in mi.
  */
  if (global_gtid_mode.get() != Gtid_mode::ON) {
    if ((error = validate_gtid_option_restrictions(lex_mi, mi))) {
      goto err;
    }
  }

  if ((error = evaluate_inter_option_dependencies(lex_mi, mi))) {
    goto err;
  }

  if (need_relay_log_purge && /* If we should purge the logs for this channel */
      preserve_logs &&        /* And we were asked to keep them */
      mi->rli->inited)        /* And the channel was initialized properly */
  {
    need_relay_log_purge = false;
  }

  THD_STAGE_INFO(thd, stage_changing_source);

  int thread_mask_stopped_threads;

  /*
    Before load_mi_and_rli_from_repositories() call, get a bit mask to indicate
    stopped threads in thread_mask_stopped_threads. Since the third argguement
    is 1, thread_mask when the function returns stands for stopped threads.
  */

  init_thread_mask(&thread_mask_stopped_threads, mi, true);

  if (load_mi_and_rli_from_repositories(mi, false, thread_mask_stopped_threads,
                                        need_relay_log_purge)) {
    error = ER_CONNECTION_METADATA;
    my_error(ER_CONNECTION_METADATA, MYF(0));
    goto err;
  }

  std::tie(validation_error, mta_remove_worker_info) =
      validate_change_replication_source_options(thd, lex_mi, mi, thread_mask);

  if (validation_error) {
    error = 1;
    goto err;
  }

  /*
    Validation operations should be above this comment
    Try to use the validate_change_replication_source_options method

    Changes to variables should be below this comment
    Try to use the update_change_replication_source_options method
   */

  if (have_receive_option) {
    strmake(saved_host, mi->host, HOSTNAME_LENGTH);
    strmake(saved_bind_addr, mi->bind_addr, HOSTNAME_LENGTH);
    saved_port = mi->port;
    strmake(saved_log_name, mi->get_master_log_name(), FN_REFLEN - 1);
    saved_log_pos = mi->get_master_log_pos();
  }

  if (update_change_replication_source_options(
          thd, lex_mi, mi, have_both_receive_execute_option,
          have_execute_option, have_receive_option)) {
    error = 1;
    goto err;
  }

  /*
    If user didn't specify neither host nor port nor any log name nor any log
    pos, i.e. he specified only user/password/source_connect_retry,
    source_delay, he probably  wants replication to resume from where it had
    left, i.e. from the coordinates of the **SQL** thread (imagine the case
    where the I/O is ahead of the SQL; restarting from the coordinates of the
    I/O would lose some events which is probably unwanted when you are just
    doing minor changes like changing source_connect_retry). Note: coordinates
    of the SQL thread must be read before the block which resets them.
  */
  if (need_relay_log_purge) {
    /*
      A side-effect is that if only the I/O thread was started, this thread may
      restart from ''/4 after the CHANGE REPLICATION SOURCE. That's a minor
      problem (it is a much more unlikely situation than the one we are fixing
      here).
    */
    if (!lex_mi->host && !lex_mi->port && !lex_mi->log_file_name &&
        !lex_mi->pos && !mi->rli->is_applier_source_position_info_invalid()) {
      /*
        Sometimes mi->rli->master_log_pos == 0 (it happens when the SQL thread
        is not initialized), so we use a max(). What happens to
        mi->rli->master_log_pos during the initialization stages of replication
        is not 100% clear, so we guard against problems using max().
      */
      mi->set_master_log_pos(max<ulonglong>(
          BIN_LOG_HEADER_SIZE, mi->rli->get_group_master_log_pos()));
      mi->set_master_log_name(mi->rli->get_group_master_log_name());
    }
  }

  if (have_receive_option)
    LogErr(SYSTEM_LEVEL, ER_REPLICA_CHANGE_SOURCE_TO_EXECUTED,
           mi->get_for_channel_str(true), saved_host, saved_port,
           saved_log_name, (ulong)saved_log_pos, saved_bind_addr, mi->host,
           mi->port, mi->get_master_log_name(), (ulong)mi->get_master_log_pos(),
           mi->bind_addr);

  /* If the receiver is stopped, flush master_info to disk. */
  if ((thread_mask & REPLICA_IO) == 0 && flush_master_info(mi, true)) {
    error = ER_RELAY_LOG_INIT;
    my_error(ER_RELAY_LOG_INIT, MYF(0),
             "Failed to flush connection metadata repository");
    goto err;
  }

  if ((thread_mask & REPLICA_SQL) == 0) /* Applier module is not executing */
  {
    if (need_relay_log_purge) {
      /*
        'if (need_relay_log_purge)' implicitly means that all slave threads are
        stopped and there is no use of relay_log_file/relay_log_pos options.
        We need not check these here again.
      */

      /* purge_relay_log() returns pointer to an error message here. */
      const char *errmsg = nullptr;
      /*
        purge_relay_log() assumes that we have run_lock and no slave threads
        are running.
      */
      THD_STAGE_INFO(thd, stage_purging_old_relay_logs);
      if (mi->rli->purge_relay_logs(thd, &errmsg)) {
        error = ER_RELAY_LOG_FAIL;
        my_error(ER_RELAY_LOG_FAIL, MYF(0), errmsg);
        goto err;
      }

      /*
        Coordinates in rli were spoilt by purge_relay_logs(),
        so restore them to good values. If we left them to ''/0, that would
        work. But that would fail in the case of 2 successive CHANGE REPLICATION
        SOURCE (without a START REPLICA in between): because first one would set
        the coords in mi to the good values of those in rli, then set those i>n
        rli to ''/0, then second CHANGE REPLICATION SOURCE would set the coords
        in mi to those of rli, i.e. to ''/0: we have lost all copies of the
        original good coordinates. That's why we always save good coords in rli.
*/
      if (!mi->is_receiver_position_info_invalid()) {
        mi->rli->set_group_master_log_pos(mi->get_master_log_pos());
        mi->rli->set_group_master_log_name(mi->get_master_log_name());
        DBUG_PRINT("info", ("source_log_pos: %llu", mi->get_master_log_pos()));
      }
    } else {
      const char *errmsg = nullptr;
      if (mi->rli->is_group_relay_log_name_invalid(&errmsg)) {
        error = ER_RELAY_LOG_INIT;
        my_error(ER_RELAY_LOG_INIT, MYF(0), errmsg);
        goto err;
      }
    }

    char *var_group_master_log_name =
        const_cast<char *>(mi->rli->get_group_master_log_name());

    if (!var_group_master_log_name[0] &&  // uninitialized case
        !mi->rli->is_applier_source_position_info_invalid())
      mi->rli->set_group_master_log_pos(0);

    mi->rli->abort_pos_wait++; /* for SOURCE_POS_WAIT() to abort */

    /* Clear the errors, for a clean start */
    mi->rli->clear_error();
    if (mi->rli->workers_array_initialized) {
      for (size_t i = 0; i < mi->rli->get_worker_count(); i++) {
        mi->rli->get_worker(i)->clear_error();
      }
    }

    /*
      If we don't write new coordinates to disk now, then old will remain in
      relay-log.info until START REPLICA is issued; but if mysqld is shutdown
      before START REPLICA, then old will remain in relay-log.info, and will be
      the in-memory value at restart (thus causing errors, as the old relay log
      does not exist anymore).

      Notice that the rli table is available exclusively as slave is not
      running.
    */
    if (mi->rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT |
                            Relay_log_info::RLI_FLUSH_IGNORE_GTID_ONLY)) {
      error = ER_RELAY_LOG_INIT;
      my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush relay info file.");
      goto err;
    }

  } /* end 'if (thread_mask & REPLICA_SQL == 0)' */

  log_invalid_position_warning(thd, lex_mi, mi);

  if (mta_remove_worker_info)
    if (Rpl_info_factory::reset_workers(mi->rli)) {
      error = ER_MTA_RESET_WORKERS;
      my_error(ER_MTA_RESET_WORKERS, MYF(0));
      goto err;
    }
err:

  unlock_slave_threads(mi);
  mi->channel_unlock();
  return error;
}

/**
   This function is first called when the Master_info object
   corresponding to a channel in a multisourced slave does not
   exist. But before a new channel is created, certain
   conditions have to be met. The below function apriorily
   checks if all such conditions are met. If all the
   conditions are met then it creates a channel i.e
   mi<->rli

   @param[in,out]  mi                When new {mi,rli} are created,
                                     the reference is stored in *mi
   @param[in]      channel           The channel on which the change
                                     master was introduced.
*/
int add_new_channel(Master_info **mi, const char *channel) {
  DBUG_TRACE;

  int error = 0;
  Ident_name_check ident_check_status;

  /*
    Return if max num of replication channels exceeded already.
  */

  if (!channel_map.is_valid_channel_count()) {
    error = ER_REPLICA_MAX_CHANNELS_EXCEEDED;
    my_error(ER_REPLICA_MAX_CHANNELS_EXCEEDED, MYF(0));
    goto err;
  }

  /*
    Now check the sanity of the channel name. It's length etc. The channel
    identifier is similar to table names. So, use  check_table_function.
  */
  if (channel) {
    ident_check_status = check_table_name(channel, strlen(channel));
  } else
    ident_check_status = Ident_name_check::WRONG;

  if (ident_check_status != Ident_name_check::OK) {
    error = ER_REPLICA_CHANNEL_NAME_INVALID_OR_TOO_LONG;
    my_error(ER_REPLICA_CHANNEL_NAME_INVALID_OR_TOO_LONG, MYF(0));
    goto err;
  }

  if (!((*mi) = Rpl_info_factory::create_mi_and_rli_objects(
            INFO_REPOSITORY_TABLE, INFO_REPOSITORY_TABLE, channel,
            &channel_map))) {
    error = ER_CONNECTION_METADATA;
    my_error(ER_CONNECTION_METADATA, MYF(0));
    goto err;
  }

err:

  return error;
}

/**
   Method used to check if the user is trying to update any other option for
   the change replication source apart from the SOURCE_USER and SOURCE_PASSWORD.
   In case user tries to update any other parameter apart from these two,
   this method will return error.

   @param  lex_mi structure that holds all change replication source options
   given on the change replication source command.

   @retval true - The CHANGE REPLICATION SOURCE is updating a unsupported
   parameter for the recovery channel.

   @retval false - Everything is fine. The CHANGE REPLICATION SOURCE can execute
   with the given option(s) for the recovery channel.
*/
static bool is_invalid_change_master_for_group_replication_recovery(
    const LEX_SOURCE_INFO *lex_mi) {
  DBUG_TRACE;
  bool have_extra_option_received = false;

  /* Check if *at least one* receive/execute option is given on change
   * replication source command*/
  if (lex_mi->host || lex_mi->log_file_name || lex_mi->pos ||
      lex_mi->bind_addr || lex_mi->port || lex_mi->connect_retry ||
      lex_mi->server_id ||
      lex_mi->auto_position != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_verify_server_cert != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->heartbeat_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->retry_count_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_key || lex_mi->ssl_cert || lex_mi->ssl_ca ||
      lex_mi->ssl_capath || lex_mi->tls_version ||
      lex_mi->tls_ciphersuites != LEX_SOURCE_INFO::UNSPECIFIED ||
      lex_mi->ssl_cipher || lex_mi->ssl_crl || lex_mi->ssl_crlpath ||
      lex_mi->repl_ignore_server_ids_opt == LEX_SOURCE_INFO::LEX_MI_ENABLE ||
      lex_mi->relay_log_name || lex_mi->relay_log_pos ||
      lex_mi->sql_delay != -1 || lex_mi->public_key_path ||
      lex_mi->get_public_key != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->zstd_compression_level || lex_mi->compression_algorithm ||
      lex_mi->require_row_format != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->m_source_connection_auto_failover !=
          LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->assign_gtids_to_anonymous_transactions_type !=
          LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED ||
      lex_mi->m_gtid_only != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    have_extra_option_received = true;

  return have_extra_option_received;
}

/**
   Method used to check if the user is trying to update any other option for
   the change replication source apart from the PRIVILEGE_CHECKS_USER.
   In case user tries to update any other parameter apart from this one, this
   method will return error.

   @param  lex_mi structure that holds all change replication source options
   given on the change replication source command.

   @retval true - The CHANGE REPLICATION SOURCE is updating a unsupported
   parameter for the recovery channel.

   @retval false - Everything is fine. The CHANGE REPLICATION SOURCE can execute
   with the given option(s) for the recovery channel.
*/
static bool is_invalid_change_master_for_group_replication_applier(
    const LEX_SOURCE_INFO *lex_mi) {
  DBUG_TRACE;
  bool have_extra_option_received = false;

  /* Check if *at least one* receive/execute option is given on change
   * replication source command*/
  if (lex_mi->host || lex_mi->user || lex_mi->password ||
      lex_mi->log_file_name || lex_mi->pos || lex_mi->bind_addr ||
      lex_mi->port || lex_mi->connect_retry || lex_mi->server_id ||
      lex_mi->auto_position != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_verify_server_cert != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->heartbeat_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->retry_count_opt != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->ssl_key || lex_mi->ssl_cert || lex_mi->ssl_ca ||
      lex_mi->ssl_capath || lex_mi->tls_version || lex_mi->ssl_cipher ||
      lex_mi->ssl_crl || lex_mi->ssl_crlpath ||
      lex_mi->repl_ignore_server_ids_opt == LEX_SOURCE_INFO::LEX_MI_ENABLE ||
      lex_mi->relay_log_name || lex_mi->relay_log_pos ||
      lex_mi->sql_delay != -1 || lex_mi->public_key_path ||
      lex_mi->get_public_key != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->zstd_compression_level || lex_mi->compression_algorithm ||
      lex_mi->require_row_format != LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->m_source_connection_auto_failover !=
          LEX_SOURCE_INFO::LEX_MI_UNCHANGED ||
      lex_mi->assign_gtids_to_anonymous_transactions_type !=
          LEX_SOURCE_INFO::LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED ||
      lex_mi->m_gtid_only != LEX_SOURCE_INFO::LEX_MI_UNCHANGED)
    have_extra_option_received = true;

  return have_extra_option_received;
}

/**
  Entry point for the CHANGE REPLICATION SOURCE command. Function
  decides to create a new channel or create an existing one.

  @param[in]        thd        the client thread that issued the command.

  @retval           true       fail
  @retval           false      success.
*/
bool change_master_cmd(THD *thd) {
  DBUG_TRACE;

  Master_info *mi = nullptr;
  LEX *lex = thd->lex;
  bool res = false;

  channel_map.wrlock();

  /* The slave must have been initialized to allow CHANGE REPLICATION SOURCE
   * statements */
  if (!is_slave_configured()) {
    my_error(ER_REPLICA_CONFIGURATION, MYF(0));
    res = true;
    goto err;
  }

  if (channel_map.is_group_replication_applier_channel_name(lex->mi.channel)) {
    /*
      If the chosen name is for group_replication_applier channel we allow the
      channel creation based on the check as to which field is being updated.
    */
    LEX_SOURCE_INFO *lex_mi = &thd->lex->mi;
    if (is_invalid_change_master_for_group_replication_applier(lex_mi)) {
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
               "CHANGE REPLICATION SOURCE with the given parameters",
               lex->mi.channel);
      res = true;
      goto err;
    }

    /*
      group_replication_applier channel only has the SQL thread, the IO thread
      job is done by GR pipeline, which queues events into the relay log after
      going through certification.
      Thence for CHANGE REPLICATION SOURCE execution pre-conditions we need to
      check if the full GR stack is stopped.
    */
    if (is_group_replication_running()) {
      my_error(ER_GRP_OPERATION_NOT_ALLOWED_GR_MUST_STOP, MYF(0));
      res = true;
      goto err;
    }
  }

  // If the channel being used is group_replication_recovery we allow the
  // channel creation based on the check as to which field is being updated.
  if (channel_map.is_group_replication_recovery_channel_name(lex->mi.channel)) {
    LEX_SOURCE_INFO *lex_mi = &thd->lex->mi;
    if (is_invalid_change_master_for_group_replication_recovery(lex_mi)) {
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
               "CHANGE REPLICATION SOURCE with the given parameters",
               lex->mi.channel);
      res = true;
      goto err;
    }
  }

  /*
    Error out if number of replication channels are > 1 if FOR CHANNEL
    clause is not provided in the CHANGE REPLICATION SOURCE command.
  */
  if (!lex->mi.for_channel && channel_map.get_num_instances() > 1) {
    my_error(ER_REPLICA_MULTIPLE_CHANNELS_CMD, MYF(0));
    res = true;
    goto err;
  }

  /* Get the Master_info of the channel */
  mi = channel_map.get_mi(lex->mi.channel);

  /* create a new channel if doesn't exist */
  if (!mi && strcmp(lex->mi.channel, channel_map.get_default_channel())) {
    /* The mi will be returned holding mi->channel_lock for writing */
    if (add_new_channel(&mi, lex->mi.channel)) goto err;
  }

  if (mi) {
    bool configure_filters = !Master_info::is_configured(mi);

    if (!(res = change_master(thd, mi, &thd->lex->mi))) {
      /*
        If the channel was just created or not configured before this
        "CHANGE REPLICATION SOURCE", we need to configure rpl_filter for it.
      */
      if (configure_filters) {
        if ((res = Rpl_info_factory::configure_channel_replication_filters(
                 mi->rli, lex->mi.channel)))
          goto err;
      }

      my_ok(thd);
    }
  } else {
    /*
       Even default channel does not exist. So issue a previous
       backward compatible  error message (till 5.6).
       @TODO: This error message shall be improved.
    */
    my_error(ER_REPLICA_CONFIGURATION, MYF(0));
  }

err:
  channel_map.unlock();

  return res;
}

/**
  Check if there is any slave SQL config conflict.

  @param[in] rli The slave's rli object.

  @return 0 is returned if there is no conflict, otherwise 1 is returned.
 */
static int check_slave_sql_config_conflict(const Relay_log_info *rli) {
  int channel_mts_submode, replica_parallel_workers;

  if (rli) {
    channel_mts_submode = rli->channel_mts_submode;
    replica_parallel_workers = rli->opt_replica_parallel_workers;
  } else {
    /*
      When the slave is first initialized, we collect the values from the
      command line options
    */
    channel_mts_submode = mts_parallel_option;
    replica_parallel_workers = opt_mts_replica_parallel_workers;
  }

  if (opt_replica_preserve_commit_order && replica_parallel_workers > 0) {
    if (channel_mts_submode == MTS_PARALLEL_TYPE_DB_NAME) {
      my_error(ER_DONT_SUPPORT_REPLICA_PRESERVE_COMMIT_ORDER, MYF(0),
               "when replica_parallel_type is DATABASE");
      return ER_DONT_SUPPORT_REPLICA_PRESERVE_COMMIT_ORDER;
    }
  }

  if (rli) {
    const char *channel = const_cast<Relay_log_info *>(rli)->get_channel();
    if (replica_parallel_workers > 0 &&
        (channel_mts_submode != MTS_PARALLEL_TYPE_LOGICAL_CLOCK ||
         (channel_mts_submode == MTS_PARALLEL_TYPE_LOGICAL_CLOCK &&
          !opt_replica_preserve_commit_order)) &&
        channel_map.is_group_replication_applier_channel_name(channel)) {
      my_error(ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED, MYF(0),
               "START REPLICA SQL_THREAD when REPLICA_PARALLEL_WORKERS > 0 "
               "and REPLICA_PARALLEL_TYPE != LOGICAL_CLOCK "
               "or REPLICA_PRESERVE_COMMIT_ORDER != ON",
               channel);
      return ER_REPLICA_CHANNEL_OPERATION_NOT_ALLOWED;
    }
  }

  return 0;
}

/**
  Purge Group Replication channels relay logs after this server being a
  recipient of clone.
*/
static void group_replication_cleanup_after_clone() {
  if (clone_startup && get_server_state() == SERVER_BOOTING) {
    channel_map.assert_some_wrlock();
    Auto_THD thd;

    Master_info *mi = channel_map.get_mi("group_replication_applier");
    if (nullptr != mi) reset_slave(thd.thd, mi, false);

    mi = channel_map.get_mi("group_replication_recovery");
    if (nullptr != mi) reset_slave(thd.thd, mi, false);
  }
}

/**
  @} (end of group Replication)
*/

/**
  Checks the current replica configuration against the server GTID mode
  If some incompatibility is found a warning is logged.
*/
static void check_replica_configuration_restrictions() {
  std::string group_name = get_group_replication_group_name();
  if (global_gtid_mode.get() != Gtid_mode::ON || group_name.length() > 0) {
    for (auto it : channel_map) {
      Master_info *mi = it.second;
      if (mi != nullptr) {
        if (global_gtid_mode.get() != Gtid_mode::ON) {
          // Check if a channel has SOURCE_AUTO POSITION
          if (global_gtid_mode.get() == Gtid_mode::OFF &&
              mi->is_auto_position()) {
            LogErr(WARNING_LEVEL,
                   ER_RPL_REPLICA_AUTO_POSITION_IS_1_AND_GTID_MODE_IS_OFF,
                   mi->get_channel(), mi->get_channel());
          }
          // Check if a channel has SOURCE_CONNECTION_AUTO_FAILOVER
          if (mi->is_source_connection_auto_failover()) {
            LogErr(WARNING_LEVEL, ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF_CHANNEL,
                   mi->get_channel(), mi->get_channel());
          }
          // Check if a channel has GTID_ONLY
          if (mi->is_gtid_only_mode()) {
            LogErr(WARNING_LEVEL,
                   ER_WARN_REPLICA_GTID_ONLY_AND_GTID_MODE_NOT_ON,
                   mi->get_channel());
          }
          // Check if a channel has ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS
          if (mi->rli->m_assign_gtids_to_anonymous_transactions_info
                  .get_type() > Assign_gtids_to_anonymous_transactions_info::
                                    enum_type::AGAT_OFF) {
            std::string assign_gtid_type;
            if (mi->rli->m_assign_gtids_to_anonymous_transactions_info
                    .get_type() == Assign_gtids_to_anonymous_transactions_info::
                                       enum_type::AGAT_LOCAL)
              assign_gtid_type.assign("LOCAL");
            else
              assign_gtid_type.assign("a UUID");
            LogErr(
                WARNING_LEVEL,
                ER_REPLICA_ANON_TO_GTID_IS_LOCAL_OR_UUID_AND_GTID_MODE_NOT_ON,
                mi->get_channel(), assign_gtid_type.data(),
                Gtid_mode::to_string(global_gtid_mode.get()));
          }
        } else {
          // No checks needed if mode is OFF
          if (mi->rli->m_assign_gtids_to_anonymous_transactions_info
                  .get_type() ==
              Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF)
            continue;

          /*
            Check if one of the channels with
              ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS
            does not have the same UUID as Group Replication
          */
          if (!(group_name.compare(
                  mi->rli->m_assign_gtids_to_anonymous_transactions_info
                      .get_value()))) {
            LogErr(WARNING_LEVEL,
                   ER_REPLICA_ANONYMOUS_TO_GTID_UUID_SAME_AS_GROUP_NAME,
                   mi->get_channel(),
                   mi->rli->m_assign_gtids_to_anonymous_transactions_info
                       .get_value()
                       .c_str());
          }
          /*
            Check if one of the channels with
              ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS
            does not have the same UUID as group_replication_view_change_uuid
          */
          std::string view_change_uuid;
          if (get_group_replication_view_change_uuid(view_change_uuid)) {
            /* purecov: begin inspected */
            LogErr(WARNING_LEVEL,
                   ER_WARN_GRP_RPL_VIEW_CHANGE_UUID_FAIL_GET_VARIABLE);
            /* purecov: end */
          }

          if (!(view_change_uuid.compare(
                  mi->rli->m_assign_gtids_to_anonymous_transactions_info
                      .get_value()))) {
            LogErr(
                WARNING_LEVEL,
                ER_WARN_REPLICA_ANONYMOUS_TO_GTID_UUID_SAME_AS_VIEW_CHANGE_UUID,
                mi->get_channel(),
                mi->rli->m_assign_gtids_to_anonymous_transactions_info
                    .get_value()
                    .c_str());
          }
        }
      }
    }
  }
}

/**
  Checks the current replica configuration when starting a replication thread
  If some incompatibility is found an error is thrown.

  @param mi  pointer to the source info repository object
  @param thread_mask what replication threads are running

  @return true if an error occurs, false otherwise
*/
static bool check_replica_configuration_errors(Master_info *mi,
                                               int thread_mask) {
  if (global_gtid_mode.get() != Gtid_mode::ON) {
    if (mi->is_auto_position() && (thread_mask & REPLICA_IO) &&
        global_gtid_mode.get() == Gtid_mode::OFF) {
      my_error(ER_CANT_USE_AUTO_POSITION_WITH_GTID_MODE_OFF, MYF(0),
               mi->get_for_channel_str());
      return true;
    }

    if (mi->is_source_connection_auto_failover()) {
      my_error(ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF, MYF(0));
      return true;
    }

    if (mi->is_gtid_only_mode()) {
      my_error(ER_CANT_USE_GTID_ONLY_WITH_GTID_MODE_NOT_ON, MYF(0),
               mi->get_for_channel_str());
      return true;
    }

    if ((mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() >
         Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF)) {
      /*
        This function may be called either during server start (when
        --skip-start-replica is not used) or during START REPLICA. The error
        should only be generated during START REPLICA. During server start, an
        error has already been written to the log for this case (in
        init_replica).
      */
      if (current_thd)
        my_error(ER_CANT_USE_ANONYMOUS_TO_GTID_WITH_GTID_MODE_NOT_ON, MYF(0),
                 mi->get_for_channel_str());
      return true;
    }
  }

  if (mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() >
      Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF) {
    std::string group_name = get_group_replication_group_name();
    if ((group_name.length() > 0) &&
        !(group_name.compare(
            mi->rli->m_assign_gtids_to_anonymous_transactions_info
                .get_value()))) {
      my_error(ER_ANONYMOUS_TO_GTID_UUID_SAME_AS_GROUP_NAME, MYF(0),
               mi->get_channel());
      return true;
    }
    std::string view_change_uuid;
    if (get_group_replication_view_change_uuid(view_change_uuid)) {
      /* purecov: begin inspected */
      my_error(ER_GRP_RPL_VIEW_CHANGE_UUID_FAIL_GET_VARIABLE, MYF(0));
      return true;
      /* purecov: end */
    } else {
      if (!(view_change_uuid.compare(
              mi->rli->m_assign_gtids_to_anonymous_transactions_info
                  .get_value()))) {
        my_error(ER_ANONYMOUS_TO_GTID_UUID_SAME_AS_VIEW_CHANGE_UUID, MYF(0),
                 mi->get_channel());
        return true;
      }
    }
    if (mi->rli->until_condition == Relay_log_info::UNTIL_SQL_BEFORE_GTIDS ||
        mi->rli->until_condition == Relay_log_info::UNTIL_SQL_AFTER_GTIDS) {
      my_error(ER_CANT_SET_SQL_AFTER_OR_BEFORE_GTIDS_WITH_ANONYMOUS_TO_GTID,
               MYF(0));
      return true;
    }
  }
  // Emit error when IGNORE_SERVER_IDS are configured along with
  // GTID_MODE = ON on server start
  if (mi != nullptr && mi->is_ignore_server_ids_configured() &&
      global_gtid_mode.get() == Gtid_mode::ON) {
    if (current_thd)
      my_error(ER_START_REPLICA_CHANNEL_INVALID_CONFIGURATION, MYF(0),
               mi->rli->get_channel(),
               "IGNORE_SERVER_IDS are configured along "
               "with GTID MODE = ON");

    LogErr(ERROR_LEVEL, ER_START_REPLICA_CHANNEL_INVALID_CONFIGURATION_LOG,
           mi->rli->get_channel(),
           "IGNORE_SERVER_IDS are configured along "
           "with GTID MODE = ON");
    return true;
  }

  return false;
}

void enable_applier_metric_collection() {
  opt_collect_replica_applier_metrics = true;
}

void disable_applier_metric_collection() {
  opt_collect_replica_applier_metrics = false;
}
