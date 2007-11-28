/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef RPL_RLI_H
#define RPL_RLI_H

#include "rpl_tblmap.h"
#include "rpl_reporting.h"
#include "rpl_utility.h"

struct RPL_TABLE_LIST;
class Master_info;

/****************************************************************************

  Replication SQL Thread

  Relay_log_info contains:
    - the current relay log
    - the current relay log offset
    - master log name
    - master log sequence corresponding to the last update
    - misc information specific to the SQL thread

  Relay_log_info is initialized from the slave.info file if such
  exists.  Otherwise, data members are intialized with defaults. The
  initialization is done with init_relay_log_info() call.

  The format of slave.info file:

  relay_log_name
  relay_log_pos
  master_log_name
  master_log_pos

  To clean up, call end_relay_log_info()

*****************************************************************************/

class Relay_log_info : public Slave_reporting_capability
{
public:
  /**
     Flags for the state of the replication.
   */
  enum enum_state_flag {
    /** The replication thread is inside a statement */
    IN_STMT,

    /** Flag counter.  Should always be last */
    STATE_FLAGS_COUNT
  };

  /*
    If flag set, then rli does not store its state in any info file.
    This is the case only when we execute BINLOG SQL commands inside
    a client, non-replication thread.
  */
  bool no_storage;

  /*
    If true, events with the same server id should be replicated. This
    field is set on creation of a relay log info structure by copying
    the value of ::replicate_same_server_id and can be overridden if
    necessary. For example of when this is done, check sql_binlog.cc,
    where the BINLOG statement can be used to execute "raw" events.
   */
  bool replicate_same_server_id;

  /*** The following variables can only be read when protect by data lock ****/

  /*
    info_fd - file descriptor of the info file. set only during
    initialization or clean up - safe to read anytime
    cur_log_fd - file descriptor of the current read  relay log
  */
  File info_fd,cur_log_fd;

  /*
    Protected with internal locks.
    Must get data_lock when resetting the logs.
  */
  MYSQL_BIN_LOG relay_log;
  LOG_INFO linfo;
  IO_CACHE cache_buf,*cur_log;

  /* The following variables are safe to read any time */

  /* IO_CACHE of the info file - set only during init or end */
  IO_CACHE info_file;

  /*
    When we restart slave thread we need to have access to the previously
    created temporary tables. Modified only on init/end and by the SQL
    thread, read only by SQL thread.
  */
  TABLE *save_temporary_tables;

  /*
    standard lock acquistion order to avoid deadlocks:
    run_lock, data_lock, relay_log.LOCK_log, relay_log.LOCK_index
  */
  pthread_mutex_t data_lock,run_lock;

  /*
    start_cond is broadcast when SQL thread is started
    stop_cond - when stopped
    data_cond - when data protected by data_lock changes
  */
  pthread_cond_t start_cond, stop_cond, data_cond;

  /* parent Master_info structure */
  Master_info *mi;

  /*
    Needed to deal properly with cur_log getting closed and re-opened with
    a different log under our feet
  */
  uint32 cur_log_old_open_count;
  
  /*
    Let's call a group (of events) :
      - a transaction
      or
      - an autocommiting query + its associated events (INSERT_ID,
    TIMESTAMP...)
    We need these rli coordinates :
    - relay log name and position of the beginning of the group we currently are
    executing. Needed to know where we have to restart when replication has
    stopped in the middle of a group (which has been rolled back by the slave).
    - relay log name and position just after the event we have just
    executed. This event is part of the current group.
    Formerly we only had the immediately above coordinates, plus a 'pending'
    variable, but this dealt wrong with the case of a transaction starting on a
    relay log and finishing (commiting) on another relay log. Case which can
    happen when, for example, the relay log gets rotated because of
    max_binlog_size.
  */
  char group_relay_log_name[FN_REFLEN];
  ulonglong group_relay_log_pos;
  char event_relay_log_name[FN_REFLEN];
  ulonglong event_relay_log_pos;
  ulonglong future_event_relay_log_pos;

  /* 
     Original log name and position of the group we're currently executing
     (whose coordinates are group_relay_log_name/pos in the relay log)
     in the master's binlog. These concern the *group*, because in the master's
     binlog the log_pos that comes with each event is the position of the
     beginning of the group.
  */
  char group_master_log_name[FN_REFLEN];
  volatile my_off_t group_master_log_pos;

  /*
    Handling of the relay_log_space_limit optional constraint.
    ignore_log_space_limit is used to resolve a deadlock between I/O and SQL
    threads, the SQL thread sets it to unblock the I/O thread and make it
    temporarily forget about the constraint.
  */
  ulonglong log_space_limit,log_space_total;
  bool ignore_log_space_limit;

  /*
    When it commits, InnoDB internally stores the master log position it has
    processed so far; the position to store is the one of the end of the
    committing event (the COMMIT query event, or the event if in autocommit
    mode).
  */
#if MYSQL_VERSION_ID < 40100
  ulonglong future_master_log_pos;
#else
  ulonglong future_group_master_log_pos;
#endif

  time_t last_master_timestamp;

  void clear_until_condition();

  /*
    Needed for problems when slave stops and we want to restart it
    skipping one or more events in the master log that have caused
    errors, and have been manually applied by DBA already.
  */
  volatile uint32 slave_skip_counter;
  volatile ulong abort_pos_wait;	/* Incremented on change master */
  volatile ulong slave_run_id;		/* Incremented on slave start */
  pthread_mutex_t log_space_lock;
  pthread_cond_t log_space_cond;
  THD * sql_thd;
#ifndef DBUG_OFF
  int events_till_abort;
#endif  

  /* if not set, the value of other members of the structure are undefined */
  bool inited;
  volatile bool abort_slave;
  volatile uint slave_running;

  /* 
     Condition and its parameters from START SLAVE UNTIL clause.
     
     UNTIL condition is tested with is_until_satisfied() method that is 
     called by exec_relay_log_event(). is_until_satisfied() caches the result
     of the comparison of log names because log names don't change very often;
     this cache is invalidated by parts of code which change log names with
     notify_*_log_name_updated() methods. (They need to be called only if SQL
     thread is running).
   */
  
  enum {UNTIL_NONE= 0, UNTIL_MASTER_POS, UNTIL_RELAY_POS} until_condition;
  char until_log_name[FN_REFLEN];
  ulonglong until_log_pos;
  /* extension extracted from log_name and converted to int */
  ulong until_log_name_extension;   
  /* 
     Cached result of comparison of until_log_name and current log name
     -2 means unitialised, -1,0,1 are comarison results 
  */
  enum 
  { 
    UNTIL_LOG_NAMES_CMP_UNKNOWN= -2, UNTIL_LOG_NAMES_CMP_LESS= -1,
    UNTIL_LOG_NAMES_CMP_EQUAL= 0, UNTIL_LOG_NAMES_CMP_GREATER= 1
  } until_log_names_cmp_result;

  char cached_charset[6];
  /*
    trans_retries varies between 0 to slave_transaction_retries and counts how
    many times the slave has retried the present transaction; gets reset to 0
    when the transaction finally succeeds. retried_trans is a cumulative
    counter: how many times the slave has retried a transaction (any) since
    slave started.
  */
  ulong trans_retries, retried_trans;

  /*
    If the end of the hot relay log is made of master's events ignored by the
    slave I/O thread, these two keep track of the coords (in the master's
    binlog) of the last of these events seen by the slave I/O thread. If not,
    ign_master_log_name_end[0] == 0.
    As they are like a Rotate event read/written from/to the relay log, they
    are both protected by rli->relay_log.LOCK_log.
  */
  char ign_master_log_name_end[FN_REFLEN];
  ulonglong ign_master_log_pos_end;

  Relay_log_info();
  ~Relay_log_info();

  /*
    Invalidate cached until_log_name and group_relay_log_name comparison 
    result. Should be called after any update of group_realy_log_name if
    there chances that sql_thread is running.
  */
  inline void notify_group_relay_log_name_update()
  {
    if (until_condition==UNTIL_RELAY_POS)
      until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_UNKNOWN;
  }

  /*
    The same as previous but for group_master_log_name. 
  */
  inline void notify_group_master_log_name_update()
  {
    if (until_condition==UNTIL_MASTER_POS)
      until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_UNKNOWN;
  }
  
  inline void inc_event_relay_log_pos()
  {
    event_relay_log_pos= future_event_relay_log_pos;
  }

  void inc_group_relay_log_pos(ulonglong log_pos,
			       bool skip_lock=0);

  int wait_for_pos(THD* thd, String* log_name, longlong log_pos, 
		   longlong timeout);
  void close_temporary_tables();

  /* Check if UNTIL condition is satisfied. See slave.cc for more. */
  bool is_until_satisfied();
  inline ulonglong until_pos()
  {
    return ((until_condition == UNTIL_MASTER_POS) ? group_master_log_pos :
	    group_relay_log_pos);
  }

  RPL_TABLE_LIST *tables_to_lock;           /* RBR: Tables to lock  */
  uint tables_to_lock_count;        /* RBR: Count of tables to lock */
  table_mapping m_table_map;      /* RBR: Mapping table-id to table */

  inline table_def *get_tabledef(TABLE *tbl)
  {
    table_def *td= 0;
    for (TABLE_LIST *ptr= tables_to_lock; ptr && !td; ptr= ptr->next_global)
      if (ptr->table == tbl)
        td= &((RPL_TABLE_LIST *)ptr)->m_tabledef;
    return (td);
  }

  /*
    Last charset (6 bytes) seen by slave SQL thread is cached here; it helps
    the thread save 3 get_charset() per Query_log_event if the charset is not
    changing from event to event (common situation).
    When the 6 bytes are equal to 0 is used to mean "cache is invalidated".
  */
  void cached_charset_invalidate();
  bool cached_charset_compare(char *charset) const;

  void cleanup_context(THD *, bool);
  void clear_tables_to_lock();

  /*
    Used by row-based replication to detect that it should not stop at
    this event, but give it a chance to send more events. The time
    where the last event inside a group started is stored here. If the
    variable is zero, we are not in a group (but may be in a
    transaction).
   */
  time_t last_event_start_time;

  /**
    Helper function to do after statement completion.

    This function is called from an event to complete the group by
    either stepping the group position, if the "statement" is not
    inside a transaction; or increase the event position, if the
    "statement" is inside a transaction.

    @param event_log_pos
    Master log position of the event. The position is recorded in the
    relay log info and used to produce information for <code>SHOW
    SLAVE STATUS</code>.

    @param event_creation_time
    Timestamp for the creation of the event on the master side. The
    time stamp is recorded in the relay log info and used to compute
    the <code>Seconds_behind_master</code> field.
  */
  void stmt_done(my_off_t event_log_pos,
                 time_t event_creation_time);


  /**
     Set the value of a replication state flag.

     @param flag Flag to set
   */
  void set_flag(enum_state_flag flag)
  {
    m_flags |= (1UL << flag);
  }

  /**
     Get the value of a replication state flag.

     @param flag Flag to get value of

     @return @c true if the flag was set, @c false otherwise.
   */
  bool get_flag(enum_state_flag flag)
  {
    return m_flags & (1UL << flag);
  }

  /**
     Clear the value of a replication state flag.

     @param flag Flag to clear
   */
  void clear_flag(enum_state_flag flag)
  {
    m_flags &= ~(1UL << flag);
  }

  /**
     Is the replication inside a group?

     Replication is inside a group if either:
     - The OPTION_BEGIN flag is set, meaning we're inside a transaction
     - The RLI_IN_STMT flag is set, meaning we're inside a statement

     @retval true Replication thread is currently inside a group
     @retval false Replication thread is currently not inside a group
   */
  bool is_in_group() const {
    return (sql_thd->options & OPTION_BEGIN) ||
      (m_flags & (1UL << IN_STMT));
  }

private:
  uint32 m_flags;
};


// Defined in rpl_rli.cc
int init_relay_log_info(Relay_log_info* rli, const char* info_fname);


#endif /* RPL_RLI_H */
