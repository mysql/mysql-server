/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/**
  @addtogroup Replication
  @{

  @file

  @brief Code to run the io thread and the sql thread on the
  replication slave.
*/

#include "sql_priv.h"
#include "my_global.h"
#include "rpl_slave.h"
#include "sql_parse.h"                         // execute_init_command
#include "sql_table.h"                         // mysql_rm_table
#include "rpl_mi.h"
#include "rpl_rli.h"
#include "rpl_filter.h"
#include "rpl_info_factory.h"
#include "transaction.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <sql_common.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <mysys_err.h>
#include "rpl_handler.h"
#include "rpl_info_dummy.h"
#include <signal.h>
#include <mysql.h>
#include <myisam.h>

#include "sql_base.h"                           // close_thread_tables
#include "tztime.h"                             // struct Time_zone
#include "log_event.h"                          // Rotate_log_event,
                                                // Create_file_log_event,
                                                // Format_description_log_event
#include "dynamic_ids.h"
#include "rpl_rli_pdb.h"
#include "global_threads.h"

#ifdef HAVE_REPLICATION

#include "rpl_tblmap.h"
#include "debug_sync.h"

using std::min;
using std::max;

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

#define MAX_SLAVE_RETRY_PAUSE 5
/*
  a parameter of sql_slave_killed() to defer the killed status
*/
#define SLAVE_WAIT_GROUP_DONE 60
bool use_slave_mask = 0;
MY_BITMAP slave_error_mask;
char slave_skip_error_names[SHOW_VAR_FUNC_BUFF_SIZE];

char* slave_load_tmpdir = 0;
Master_info *active_mi= 0;
my_bool replicate_same_server_id;
ulonglong relay_log_space_limit = 0;

const char *relay_log_index= 0;
const char *relay_log_basename= 0;

/*
  MTS load-ballancing parameter.
  Max length of one MTS Worker queue. The value also determines the size
  of Relay_log_info::gaq (see @c slave_start_workers()).
  It can be set to any value in [1, ULONG_MAX - 1] range.
*/
const ulong mts_slave_worker_queue_len_max= 16384;

/*
  Statistics go to the error log every # of seconds when --log-warnings > 1
*/
const long mts_online_stat_period= 60 * 2;


/*
  MTS load-ballancing parameter.
  Time unit in microsecs to sleep by MTS Coordinator to avoid extra thread
  signalling in the case of Worker queues are close to be filled up.
*/
const ulong mts_coordinator_basic_nap= 5;

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
const ulong mts_worker_underrun_level= 10;

Slave_job_item * de_queue(Slave_jobs_queue *jobs, Slave_job_item *ret);
bool append_item_to_jobs(slave_job_item *job_item,
                         Slave_worker *w, Relay_log_info *rli);

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.

  TODO: move the vars below under Master_info
*/

int disconnect_slave_event_count = 0, abort_slave_event_count = 0;

static pthread_key(Master_info*, RPL_MASTER_INFO);

enum enum_slave_reconnect_actions
{
  SLAVE_RECON_ACT_REG= 0,
  SLAVE_RECON_ACT_DUMP= 1,
  SLAVE_RECON_ACT_EVENT= 2,
  SLAVE_RECON_ACT_MAX
};

enum enum_slave_reconnect_messages
{
  SLAVE_RECON_MSG_WAIT= 0,
  SLAVE_RECON_MSG_KILLED_WAITING= 1,
  SLAVE_RECON_MSG_AFTER= 2,
  SLAVE_RECON_MSG_FAILED= 3,
  SLAVE_RECON_MSG_COMMAND= 4,
  SLAVE_RECON_MSG_KILLED_AFTER= 5,
  SLAVE_RECON_MSG_MAX
};

static const char *reconnect_messages[SLAVE_RECON_ACT_MAX][SLAVE_RECON_MSG_MAX]=
{
  {
    "Waiting to reconnect after a failed registration on master",
    "Slave I/O thread killed while waitnig to reconnect after a failed \
registration on master",
    "Reconnecting after a failed registration on master",
    "failed registering on master, reconnecting to try again, \
log '%s' at position %s",
    "COM_REGISTER_SLAVE",
    "Slave I/O thread killed during or after reconnect"
  },
  {
    "Waiting to reconnect after a failed binlog dump request",
    "Slave I/O thread killed while retrying master dump",
    "Reconnecting after a failed binlog dump request",
    "failed dump request, reconnecting to try again, log '%s' at position %s",
    "COM_BINLOG_DUMP",
    "Slave I/O thread killed during or after reconnect"
  },
  {
    "Waiting to reconnect after a failed master event read",
    "Slave I/O thread killed while waiting to reconnect after a failed read",
    "Reconnecting after a failed master event read",
    "Slave I/O thread: Failed reading log event, reconnecting to retry, \
log '%s' at position %s",
    "",
    "Slave I/O thread killed during or after a reconnect done to recover from \
failed read"
  }
};

enum enum_slave_apply_event_and_update_pos_retval
{
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK= 0,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR= 1,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR= 2,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR= 3,
  SLAVE_APPLY_EVENT_AND_UPDATE_POS_MAX
};


static int process_io_rotate(Master_info* mi, Rotate_log_event* rev);
static int process_io_create_file(Master_info* mi, Create_file_log_event* cev);
static bool wait_for_relay_log_space(Relay_log_info* rli);
static inline bool io_slave_killed(THD* thd,Master_info* mi);
static inline bool sql_slave_killed(THD* thd,Relay_log_info* rli);
static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type);
static void print_slave_skip_errors(void);
static int safe_connect(THD* thd, MYSQL* mysql, Master_info* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, Master_info* mi,
                          bool suppress_warnings);
static int connect_to_master(THD* thd, MYSQL* mysql, Master_info* mi,
                             bool reconnect, bool suppress_warnings);
static int get_master_version_and_clock(MYSQL* mysql, Master_info* mi);
static int get_master_uuid(MYSQL *mysql, Master_info *mi);
int io_thread_init_commands(MYSQL *mysql, Master_info *mi);
static Log_event* next_event(Relay_log_info* rli);
static int queue_event(Master_info* mi,const char* buf,ulong event_len);
static int terminate_slave_thread(THD *thd,
                                  mysql_mutex_t *term_lock,
                                  mysql_cond_t *term_cond,
                                  volatile uint *slave_running,
                                  bool need_lock_term);
static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info);
int slave_worker_exec_job(Slave_worker * w, Relay_log_info *rli);
static int mts_event_coord_cmp(LOG_POS_COORD *id1, LOG_POS_COORD *id2);

/*
  Find out which replications threads are running

  SYNOPSIS
    init_thread_mask()
    mask                Return value here
    mi                  master_info for slave
    inverse             If set, returns which threads are not running

  IMPLEMENTATION
    Get a bit mask for which threads are running so that we can later restart
    these threads.

  RETURN
    mask        If inverse == 0, running threads
                If inverse == 1, stopped threads
*/

void init_thread_mask(int* mask, Master_info* mi, bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli->slave_running;
  register int tmp_mask=0;
  DBUG_ENTER("init_thread_mask");

  if (set_io)
    tmp_mask |= SLAVE_IO;
  if (set_sql)
    tmp_mask |= SLAVE_SQL;
  if (inverse)
    tmp_mask^= (SLAVE_IO | SLAVE_SQL);
  *mask = tmp_mask;
  DBUG_VOID_RETURN;
}


/*
  lock_slave_threads()
*/

void lock_slave_threads(Master_info* mi)
{
  DBUG_ENTER("lock_slave_threads");

  //TODO: see if we can do this without dual mutex
  mysql_mutex_lock(&mi->run_lock);
  mysql_mutex_lock(&mi->rli->run_lock);
  DBUG_VOID_RETURN;
}


/*
  unlock_slave_threads()
*/

void unlock_slave_threads(Master_info* mi)
{
  DBUG_ENTER("unlock_slave_threads");

  //TODO: see if we can do this without dual mutex
  mysql_mutex_unlock(&mi->rli->run_lock);
  mysql_mutex_unlock(&mi->run_lock);
  DBUG_VOID_RETURN;
}

#ifdef HAVE_PSI_INTERFACE
static PSI_thread_key key_thread_slave_io, key_thread_slave_sql;

static PSI_thread_info all_slave_threads[]=
{
  { &key_thread_slave_io, "slave_io", PSI_FLAG_GLOBAL},
  { &key_thread_slave_sql, "slave_sql", PSI_FLAG_GLOBAL}
};

static void init_slave_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= array_elements(all_slave_threads);
  mysql_thread_register(category, all_slave_threads, count);
}
#endif /* HAVE_PSI_INTERFACE */

/* Initialize slave structures */

int init_slave()
{
  DBUG_ENTER("init_slave");
  int error= 0;
  int thread_mask= SLAVE_SQL | SLAVE_IO;
  Relay_log_info* rli= NULL;

#ifdef HAVE_PSI_INTERFACE
  init_slave_psi_keys();
#endif

  /*
    This is called when mysqld starts. Before client connections are
    accepted. However bootstrap may conflict with us if it does START SLAVE.
    So it's safer to take the lock.
  */
  mysql_mutex_lock(&LOCK_active_mi);

  if (pthread_key_create(&RPL_MASTER_INFO, NULL))
    DBUG_RETURN(1);

  if ((error= Rpl_info_factory::create_coordinators(opt_mi_repository_id, &active_mi,
                                                    opt_rli_repository_id, &rli)))
  {
    sql_print_error("Failed to create or recover replication info repository.");
    error= 1;
    goto err;
  }

  /*
    This is the startup routine and as such we try to
    configure both the SLAVE_SQL and SLAVE_IO.
  */
  if (global_init_info(active_mi, true, thread_mask))
  {
    sql_print_error("Failed to initialize the master info structure");
    error= 1;
    goto err;
  }

  DBUG_PRINT("info", ("init group master %s %lu  group relay %s %lu event %s %lu\n",
    rli->get_group_master_log_name(),
    (ulong) rli->get_group_master_log_pos(),
    rli->get_group_relay_log_name(),
    (ulong) rli->get_group_relay_log_pos(),
    rli->get_event_relay_log_name(),
    (ulong) rli->get_event_relay_log_pos()));

  /* If server id is not set, start_slave_thread() will say it */
  if (active_mi->host[0] && !opt_skip_slave_start)
  {
    /* same as in start_slave() cache the global var values into rli's members */
    active_mi->rli->opt_slave_parallel_workers= opt_mts_slave_parallel_workers;
    active_mi->rli->checkpoint_group= opt_mts_checkpoint_group;
    if (start_slave_threads(true/*need_lock_slave=true*/,
                            false/*wait_for_start=false*/,
                            active_mi,
                            thread_mask))
    {
      sql_print_error("Failed to create slave threads");
      error= 1;
      goto err;
    }
  }

err:
  mysql_mutex_unlock(&LOCK_active_mi);
  if (error)
    sql_print_information("Check error log for additional messages. "
                          "You will not be able to start replication until "
                          "the issue is resolved and the server restarted.");
  DBUG_RETURN(error);
}

/*
  Updates the master info based on the information stored in the
  relay info and ignores relay logs previously retrieved by the IO
  thread, which thus starts fetching again based on to the
  master_log_pos and master_log_name. Eventually, the old
  relay logs will be purged by the normal purge mechanism.

  In the feature, we should improve this routine in order to avoid throwing
  away logs that are safely stored in the disk. Note also that this recovery
  routine relies on the correctness of the relay-log.info and only tolerates
  coordinate problems in master.info.

  In this function, there is no need for a mutex as the caller
  (i.e. init_slave) already has one acquired.

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
int init_recovery(Master_info* mi, const char** errmsg)
{
  DBUG_ENTER("init_recovery");

  int error= 0;
  Relay_log_info *rli= mi->rli;
  char *group_master_log_name= NULL;

  if (rli->recovery_parallel_workers)
  {
    /*
      This is not idempotent and a crash after this function and before
      the recovery is actually done may lead the system to an inconsistent
      state.

      This may happen because the gap is not persitent stored anywhere
      and eventually old relay log files will be removed and further
      calculations on the gaps will be impossible.

      We need to improve this. /Alfranio.
    */
    error= mts_recovery_groups(rli);
    if (rli->mts_recovery_group_cnt)
    {
      error= 1;
      sql_print_error("--relay-log-recovery cannot be executed when the slave "
                        "was stopped with an error or killed in MTS mode; "
                        "consider using RESET SLAVE or restart the server "
                        "with --relay-log-recovery = 0 followed by "
                        "START SLAVE UNTIL SQL_AFTER_MTS_GAPS");
    }
  }

  group_master_log_name= const_cast<char *>(rli->get_group_master_log_name());
  if (!error && group_master_log_name[0])
  {
    mi->set_master_log_pos(max<ulonglong>(BIN_LOG_HEADER_SIZE,
                                               rli->get_group_master_log_pos()));
    mi->set_master_log_name(rli->get_group_master_log_name());

    sql_print_warning("Recovery from master pos %ld and file %s.",
                      (ulong) mi->get_master_log_pos(), mi->get_master_log_name());

    rli->set_group_relay_log_name(rli->relay_log.get_log_fname());
    rli->set_event_relay_log_name(rli->relay_log.get_log_fname());
    rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
    rli->set_event_relay_log_pos(BIN_LOG_HEADER_SIZE);
  }

  DBUG_RETURN(error);
}

int global_init_info(Master_info* mi, bool ignore_if_no_info, int thread_mask)
{
  DBUG_ENTER("init_info");
  DBUG_ASSERT(mi != NULL && mi->rli != NULL);
  int init_error= 0;
  enum_return_check check_return= ERROR_CHECKING_REPOSITORY;

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  /*
    This takes care of the startup dependency between the master_info
    and relay_info. It initializes the master info if the SLAVE_IO
    thread is being started and the relay log info if either the
    SLAVE_SQL thread is being started or was not initialized as it is
    required by the SLAVE_IO thread.
  */
  check_return= mi->check_info();
  if (check_return == ERROR_CHECKING_REPOSITORY)
    goto end;

  if (!(ignore_if_no_info && check_return == REPOSITORY_DOES_NOT_EXIST))
  {
    if ((thread_mask & SLAVE_IO) != 0 && mi->mi_init_info())
      init_error= 1;
  }

  check_return= mi->rli->check_info();
  if (check_return == ERROR_CHECKING_REPOSITORY)
    goto end;
  if (!(ignore_if_no_info && check_return == REPOSITORY_DOES_NOT_EXIST))
  {
    if (((thread_mask & SLAVE_SQL) != 0 || !(mi->rli->inited))
        && mi->rli->rli_init_info())
      init_error= 1;
  }

end:
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(check_return == ERROR_CHECKING_REPOSITORY || init_error);
}

void end_info(Master_info* mi)
{
  DBUG_ENTER("end_info");
  DBUG_ASSERT(mi != NULL && mi->rli != NULL);

  /*
    The previous implementation was not acquiring locks.  We do the same here.
    However, this is quite strange.
  */
  mi->end_info();
  mi->rli->end_info();

  DBUG_VOID_RETURN;
}

int remove_info(Master_info* mi)
{
  int error= 1;
  DBUG_ENTER("remove_info");
  DBUG_ASSERT(mi != NULL && mi->rli != NULL);

  /*
    The previous implementation was not acquiring locks.
    We do the same here. However, this is quite strange.
  */
  /*
    Reset errors (the idea is that we forget about the
    old master).
  */
  mi->clear_error();
  mi->rli->clear_error();
  mi->rli->clear_until_condition();
  mi->rli->clear_sql_delay();

  mi->end_info();
  mi->rli->end_info();

  if (mi->remove_info() || Rpl_info_factory::reset_workers(mi->rli) ||
      mi->rli->remove_info())
    goto err;

  error= 0;

err:
  DBUG_RETURN(error);
}

int flush_master_info(Master_info* mi, bool force)
{
  DBUG_ENTER("flush_master_info");
  DBUG_ASSERT(mi != NULL && mi->rli != NULL);
  /*
    The previous implementation was not acquiring locks.
    We do the same here. However, this is quite strange.
  */
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
  mysql_mutex_t *log_lock= mi->rli->relay_log.get_log_lock();

  mysql_mutex_lock(log_lock);

  int err=  (mi->rli->flush_current_log() ||
             mi->flush_info(force));

  mysql_mutex_unlock(log_lock);

  DBUG_RETURN (err);
}

/**
  Convert slave skip errors bitmap into a printable string.
*/

static void print_slave_skip_errors(void)
{
  /*
    To be safe, we want 10 characters of room in the buffer for a number
    plus terminators. Also, we need some space for constant strings.
    10 characters must be sufficient for a number plus {',' | '...'}
    plus a NUL terminator. That is a max 6 digit number.
  */
  const size_t MIN_ROOM= 10;
  DBUG_ENTER("print_slave_skip_errors");
  DBUG_ASSERT(sizeof(slave_skip_error_names) > MIN_ROOM);
  DBUG_ASSERT(MAX_SLAVE_ERROR <= 999999); // 6 digits

  if (!use_slave_mask || bitmap_is_clear_all(&slave_error_mask))
  {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("OFF"));
    /* purecov: end */
  }
  else if (bitmap_is_set_all(&slave_error_mask))
  {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("ALL"));
    /* purecov: end */
  }
  else
  {
    char *buff= slave_skip_error_names;
    char *bend= buff + sizeof(slave_skip_error_names);
    int  errnum;

    for (errnum= 0; errnum < MAX_SLAVE_ERROR; errnum++)
    {
      if (bitmap_is_set(&slave_error_mask, errnum))
      {
        if (buff + MIN_ROOM >= bend)
          break; /* purecov: tested */
        buff= int10_to_str(errnum, buff, 10);
        *buff++= ',';
      }
    }
    if (buff != slave_skip_error_names)
      buff--; // Remove last ','
    if (errnum < MAX_SLAVE_ERROR)
    {
      /* Couldn't show all errors */
      buff= strmov(buff, "..."); /* purecov: tested */
    }
    *buff=0;
  }
  DBUG_PRINT("init", ("error_names: '%s'", slave_skip_error_names));
  DBUG_VOID_RETURN;
}

/**
 Change arg to the string with the nice, human-readable skip error values.
   @param slave_skip_errors_ptr
          The pointer to be changed
*/
void set_slave_skip_errors(char** slave_skip_errors_ptr)
{
  DBUG_ENTER("set_slave_skip_errors");
  print_slave_skip_errors();
  *slave_skip_errors_ptr= slave_skip_error_names;
  DBUG_VOID_RETURN;
}

/**
  Init function to set up array for errors that should be skipped for slave
*/
static void init_slave_skip_errors()
{
  DBUG_ENTER("init_slave_skip_errors");
  DBUG_ASSERT(!use_slave_mask); // not already initialized

  if (bitmap_init(&slave_error_mask,0,MAX_SLAVE_ERROR,0))
  {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(1);
  }
  use_slave_mask = 1;
  DBUG_VOID_RETURN;
}

static void add_slave_skip_errors(const uint* errors, uint n_errors)
{
  DBUG_ENTER("add_slave_skip_errors");
  DBUG_ASSERT(errors);
  DBUG_ASSERT(use_slave_mask);

  for (uint i = 0; i < n_errors; i++)
  {
    const uint err_code = errors[i];
    if (err_code < MAX_SLAVE_ERROR)
       bitmap_set_bit(&slave_error_mask, err_code);
  }
  DBUG_VOID_RETURN;
}

/*
  Add errors that should be skipped for slave

  SYNOPSIS
    add_slave_skip_errors()
    arg         List of errors numbers to be added to skip, separated with ','

  NOTES
    Called from get_options() in mysqld.cc on start-up
*/

void add_slave_skip_errors(const char* arg)
{
  const char *p= NULL;
  /*
    ALL is only valid when nothing else is provided.
  */
  const uchar SKIP_ALL[]= "all";
  size_t SIZE_SKIP_ALL= strlen((const char *) SKIP_ALL) + 1;
  /*
    IGNORE_DDL_ERRORS can be combined with other parameters
    but must be the first one provided.
  */
  const uchar SKIP_DDL_ERRORS[]= "ddl_exist_errors";
  size_t SIZE_SKIP_DDL_ERRORS= strlen((const char *) SKIP_DDL_ERRORS);
  DBUG_ENTER("add_slave_skip_errors");

  // initialize mask if not done yet
  if (!use_slave_mask)
    init_slave_skip_errors();

  for (; my_isspace(system_charset_info,*arg); ++arg)
    /* empty */;
  if (!my_strnncoll(system_charset_info, (uchar*)arg, SIZE_SKIP_ALL,
                    SKIP_ALL, SIZE_SKIP_ALL))
  {
    bitmap_set_all(&slave_error_mask);
    DBUG_VOID_RETURN;
  }
  if (!my_strnncoll(system_charset_info, (uchar*)arg, SIZE_SKIP_DDL_ERRORS,
                    SKIP_DDL_ERRORS, SIZE_SKIP_DDL_ERRORS))
  {
    // DDL errors to be skipped for relaxed 'exist' handling
    const uint ddl_errors[] = {
      // error codes with create/add <schema object>
      ER_DB_CREATE_EXISTS, ER_TABLE_EXISTS_ERROR, ER_DUP_KEYNAME,
      ER_MULTIPLE_PRI_KEY,
      // error codes with change/rename <schema object>
      ER_BAD_FIELD_ERROR, ER_NO_SUCH_TABLE, ER_DUP_FIELDNAME,
      // error codes with drop <schema object>
      ER_DB_DROP_EXISTS, ER_BAD_TABLE_ERROR, ER_CANT_DROP_FIELD_OR_KEY
    };

    add_slave_skip_errors(ddl_errors,
                          sizeof(ddl_errors)/sizeof(ddl_errors[0]));
    /*
      After processing the SKIP_DDL_ERRORS, the pointer is
      increased to the position after the comma.
    */
    if (strlen(arg) > SIZE_SKIP_DDL_ERRORS + 1)
      arg+= SIZE_SKIP_DDL_ERRORS + 1;
  }
  for (p= arg ; *p; )
  {
    long err_code;
    if (!(p= str2int(p, 10, 0, LONG_MAX, &err_code)))
      break;
    if (err_code < MAX_SLAVE_ERROR)
       bitmap_set_bit(&slave_error_mask,(uint)err_code);
    while (!my_isdigit(system_charset_info,*p) && *p)
      p++;
  }
  DBUG_VOID_RETURN;
}

static void set_thd_in_use_temporary_tables(Relay_log_info *rli)
{
  TABLE *table;

  for (table= rli->save_temporary_tables ; table ; table= table->next)
  {
    table->in_use= rli->info_thd;
    if (table->file != NULL)
    {
      /*
        Since we are stealing opened temporary tables from one thread to another,
        we need to let the performance schema know that,
        for aggregates per thread to work properly.
      */
      table->file->unbind_psi();
      table->file->rebind_psi();
    }
  }
}

int terminate_slave_threads(Master_info* mi,int thread_mask,bool need_lock_term)
{
  DBUG_ENTER("terminate_slave_threads");

  if (!mi->inited)
    DBUG_RETURN(0); /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  mysql_mutex_t *sql_lock = &mi->rli->run_lock, *io_lock = &mi->run_lock;
  mysql_mutex_t *log_lock= mi->rli->relay_log.get_log_lock();

  if (thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating SQL thread"));
    mi->rli->abort_slave= 1;
    if ((error=terminate_slave_thread(mi->rli->info_thd, sql_lock,
                                      &mi->rli->stop_cond,
                                      &mi->rli->slave_running,
                                      need_lock_term)) &&
        !force_all)
      DBUG_RETURN(error);

    mysql_mutex_lock(log_lock);

    DBUG_PRINT("info",("Flushing relay-log info file."));
    if (current_thd)
      THD_STAGE_INFO(current_thd, stage_flushing_relay_log_info_file);

    /*
      Flushes the relay log info regardles of the sync_relay_log_info option.
    */
    if (mi->rli->flush_info(TRUE))
    {
      mysql_mutex_unlock(log_lock);
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);
    }

    mysql_mutex_unlock(log_lock);
  }
  if (thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating IO thread"));
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->info_thd,io_lock,
                                      &mi->stop_cond,
                                      &mi->slave_running,
                                      need_lock_term)) &&
        !force_all)
      DBUG_RETURN(error);

    mysql_mutex_lock(log_lock);

    DBUG_PRINT("info",("Flushing relay log and master info repository."));
    if (current_thd)
      THD_STAGE_INFO(current_thd, stage_flushing_relay_log_and_master_info_repository);

    /*
      Flushes the master info regardles of the sync_master_info option.
    */
    if (mi->flush_info(TRUE))
    {
      mysql_mutex_unlock(log_lock);
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);
    }

    /*
      Flushes the relay log regardles of the sync_relay_log option.
    */
    if (mi->rli->relay_log.is_open() &&
        mi->rli->relay_log.flush_and_sync(true))
    {
      mysql_mutex_unlock(log_lock);
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);
    }

    mysql_mutex_unlock(log_lock);
  }
  DBUG_RETURN(0);
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

   @param term_lock
          Associated lock to use when waiting for @c term_cond

   @param term_cond
          Condition that is signalled when the thread has terminated

   @param slave_running
          Pointer to predicate to check for slave thread termination

   @param need_lock_term
          If @c false the lock will not be acquired before waiting on
          the condition. In this case, it is assumed that the calling
          function acquires the lock before calling this function.

   @retval 0 All OK ER_SLAVE_NOT_RUNNING otherwise.

   @note  If the executing thread has to acquire term_lock
          (need_lock_term is true, the negative running status does not
          represent any issue therefore no error is reported.

 */
static int
terminate_slave_thread(THD *thd,
                       mysql_mutex_t *term_lock,
                       mysql_cond_t *term_cond,
                       volatile uint *slave_running,
                       bool need_lock_term)
{
  DBUG_ENTER("terminate_slave_thread");
  if (need_lock_term)
  {
    mysql_mutex_lock(term_lock);
  }
  else
  {
    mysql_mutex_assert_owner(term_lock);
  }
  if (!*slave_running)
  {
    if (need_lock_term)
    {
      /*
        if run_lock (term_lock) is acquired locally then either
        slave_running status is fine
      */
      mysql_mutex_unlock(term_lock);
      DBUG_RETURN(0);
    }
    else
    {
      DBUG_RETURN(ER_SLAVE_NOT_RUNNING);
    }
  }
  DBUG_ASSERT(thd != 0);
  THD_CHECK_SENTRY(thd);

  /*
    Is is critical to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */

  while (*slave_running)                        // Should always be true
  {
    int error;
    DBUG_PRINT("loop", ("killing slave thread"));

    mysql_mutex_lock(&thd->LOCK_thd_data);
#ifndef DONT_USE_THR_ALARM
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
    int err __attribute__((unused))= pthread_kill(thd->real_id, thr_client_alarm);
    DBUG_ASSERT(err != EINVAL);
#endif
    thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect againts it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
    error= mysql_cond_timedwait(term_cond, term_lock, &abstime);
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(*slave_running == 0);

  if (need_lock_term)
    mysql_mutex_unlock(term_lock);
  DBUG_RETURN(0);
}


int start_slave_thread(
#ifdef HAVE_PSI_INTERFACE
                       PSI_thread_key thread_key,
#endif
                       pthread_handler h_func, mysql_mutex_t *start_lock,
                       mysql_mutex_t *cond_lock,
                       mysql_cond_t *start_cond,
                       volatile uint *slave_running,
                       volatile ulong *slave_run_id,
                       Master_info* mi)
{
  pthread_t th;
  ulong start_id;
  DBUG_ENTER("start_slave_thread");

  if (start_lock)
    mysql_mutex_lock(start_lock);
  if (!server_id)
  {
    if (start_cond)
      mysql_cond_broadcast(start_cond);
    if (start_lock)
      mysql_mutex_unlock(start_lock);
    sql_print_error("Server id not set, will not start slave");
    DBUG_RETURN(ER_BAD_SLAVE);
  }

  if (*slave_running)
  {
    if (start_cond)
      mysql_cond_broadcast(start_cond);
    if (start_lock)
      mysql_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_MUST_STOP);
  }
  start_id= *slave_run_id;
  DBUG_PRINT("info",("Creating new slave thread"));
  if (mysql_thread_create(thread_key,
                          &th, &connection_attrib, h_func, (void*)mi))
  {
    if (start_lock)
      mysql_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_THREAD);
  }
  if (start_cond && cond_lock) // caller has cond_lock
  {
    THD* thd = current_thd;
    while (start_id == *slave_run_id && thd != NULL)
    {
      DBUG_PRINT("sleep",("Waiting for slave thread to start"));
      PSI_stage_info saved_stage= {0, "", 0};
      thd->ENTER_COND(start_cond, cond_lock,
                      & stage_waiting_for_slave_thread_to_start,
                      & saved_stage);
      /*
        It is not sufficient to test this at loop bottom. We must test
        it after registering the mutex in enter_cond(). If the kill
        happens after testing of thd->killed and before the mutex is
        registered, we could otherwise go waiting though thd->killed is
        set.
      */
      if (!thd->killed)
        mysql_cond_wait(start_cond, cond_lock);
      thd->EXIT_COND(& saved_stage);
      mysql_mutex_lock(cond_lock); // re-acquire it as exit_cond() released
      if (thd->killed)
      {
        if (start_lock)
          mysql_mutex_unlock(start_lock);
        DBUG_RETURN(thd->killed_errno());
      }
    }
  }
  if (start_lock)
    mysql_mutex_unlock(start_lock);
  DBUG_RETURN(0);
}


/*
  start_slave_threads()

  NOTES
    SLAVE_FORCE_ALL is not implemented here on purpose since it does not make
    sense to do that for starting a slave--we always care if it actually
    started the threads that were not previously running
*/

int start_slave_threads(bool need_lock_slave, bool wait_for_start,
                        Master_info* mi, int thread_mask)
{
  mysql_mutex_t *lock_io=0, *lock_sql=0, *lock_cond_io=0, *lock_cond_sql=0;
  mysql_cond_t* cond_io=0, *cond_sql=0;
  int error=0;
  DBUG_ENTER("start_slave_threads");

  if (!mi->inited || !mi->rli->inited)
  {
    if (!mi->inited)
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                 ER(ER_SLAVE_FATAL_ERROR),
                 "Failed to initialize the master info structure");
    else
      mi->rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                      ER(ER_SLAVE_FATAL_ERROR),
                      "Failed to initialize the master info structure");
    DBUG_RETURN(ER_SLAVE_FATAL_ERROR);
  }

  if (need_lock_slave)
  {
    lock_io = &mi->run_lock;
    lock_sql = &mi->rli->run_lock;
  }
  if (wait_for_start)
  {
    cond_io = &mi->start_cond;
    cond_sql = &mi->rli->start_cond;
    lock_cond_io = &mi->run_lock;
    lock_cond_sql = &mi->rli->run_lock;
  }

  if (thread_mask & SLAVE_IO)
    error= start_slave_thread(
#ifdef HAVE_PSI_INTERFACE
                              key_thread_slave_io,
#endif
                              handle_slave_io, lock_io, lock_cond_io,
                              cond_io,
                              &mi->slave_running, &mi->slave_run_id,
                              mi);
  if (!error && (thread_mask & SLAVE_SQL))
  {
    /*
      MTS-recovery gaps gathering is placed onto common execution path
      for either START-SLAVE and --skip-start-slave= 0 
    */
    if (mi->rli->recovery_parallel_workers != 0)
      error= mts_recovery_groups(mi->rli);
    if (!error)
      error= start_slave_thread(
#ifdef HAVE_PSI_INTERFACE
                                key_thread_slave_sql,
#endif
                                handle_slave_sql, lock_sql, lock_cond_sql,
                                cond_sql,
                                &mi->rli->slave_running, &mi->rli->slave_run_id,
                                mi);
    if (error)
      terminate_slave_threads(mi, thread_mask & SLAVE_IO, need_lock_slave);
  }
  DBUG_RETURN(error);
}

/*
  Release slave threads at time of executing shutdown.

  SYNOPSIS
    end_slave()
*/

void end_slave()
{
  DBUG_ENTER("end_slave");

  /*
    This is called when the server terminates, in close_connections().
    It terminates slave threads. However, some CHANGE MASTER etc may still be
    running presently. If a START SLAVE was in progress, the mutex lock below
    will make us wait until slave threads have started, and START SLAVE
    returns, then we terminate them here.
  */
  mysql_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    /*
      TODO: replace the line below with
      list_walk(&master_list, (list_walk_action)end_slave_on_walk,0);
      once multi-master code is ready.
    */
    terminate_slave_threads(active_mi,SLAVE_FORCE_ALL);
  }
  mysql_mutex_unlock(&LOCK_active_mi);
  DBUG_VOID_RETURN;
}

/**
   Free all resources used by slave threads at time of executing shutdown.
   The routine must be called after all possible users of @c active_mi
   have left.

   SYNOPSIS
     close_active_mi()

*/
void close_active_mi()
{
  mysql_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    end_info(active_mi);
    if (active_mi->rli)
      delete active_mi->rli;
    delete active_mi;
    active_mi= 0;
  }
  mysql_mutex_unlock(&LOCK_active_mi);
}

static bool io_slave_killed(THD* thd, Master_info* mi)
{
  DBUG_ENTER("io_slave_killed");

  DBUG_ASSERT(mi->info_thd == thd);
  DBUG_ASSERT(mi->slave_running); // tracking buffer overrun
  DBUG_RETURN(mi->abort_slave || abort_loop || thd->killed);
}

/**
   The function analyzes a possible killed status and makes
   a decision whether to accept it or not.
   Normally upon accepting the sql thread goes to shutdown.
   In the event of deferring decision @rli->last_event_start_time waiting
   timer is set to force the killed status be accepted upon its expiration.

   Notice Multi-Threaded-Slave behaves similarly in that when it's being
   stopped and the current group of assigned events has not yet scheduled 
   completely, Coordinator defers to accept to leave its read-distribute
   state. The above timeout ensures waiting won't last endlessly, and in
   such case an error is reported.

   @param thd   pointer to a THD instance
   @param rli   pointer to Relay_log_info instance

   @return TRUE the killed status is recognized, FALSE a possible killed
           status is deferred.
*/
static bool sql_slave_killed(THD* thd, Relay_log_info* rli)
{
  bool ret= FALSE;
  bool is_parallel_warn= FALSE;

  DBUG_ENTER("sql_slave_killed");

  DBUG_ASSERT(rli->info_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);
  if (abort_loop || thd->killed || rli->abort_slave)
  {
    is_parallel_warn= (rli->is_parallel_exec() && 
                       (rli->is_mts_in_group() || thd->killed));
    /*
      Slave can execute stop being in one of two MTS or Single-Threaded mode.
      The modes define different criteria to accept the stop.
      In particular that relates to the concept of groupping.
      Killed Coordinator thread expects the worst so it warns on
      possible consistency issue.
    */
    if (is_parallel_warn ||
        (!rli->is_parallel_exec() &&
         thd->transaction.all.cannot_safely_rollback() && rli->is_in_group()))
    {
      char msg_stopped[]=
        "... Slave SQL Thread stopped with incomplete event group "
        "having non-transactional changes. "
        "If the group consists solely of row-based events, you can try "
        "to restart the slave with --slave-exec-mode=IDEMPOTENT, which "
        "ignores duplicate key, key not found, and similar errors (see "
        "documentation for details).";
      char msg_stopped_mts[]=
        "... The slave coordinator and worker threads are stopped, possibly "
        "leaving data in inconsistent state. A restart should "
        "restore consistency automatically, although using non-transactional "
        "storage for data or info tables or DDL queries could lead to problems. "
        "In such cases you have to examine your data (see documentation for "
        "details).";

      ret= TRUE;
      if (rli->abort_slave)
      {
        DBUG_PRINT("info", ("Request to stop slave SQL Thread received while "
                            "applying an MTS group or a group that "
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
          rli->last_event_start_time= my_time(0);
        ret= difftime(my_time(0), rli->last_event_start_time) <=
          SLAVE_WAIT_GROUP_DONE ? FALSE : TRUE;

        DBUG_EXECUTE_IF("stop_slave_middle_group", 
                        DBUG_EXECUTE_IF("incomplete_group_in_relay_log",
                                        ret= TRUE;);); // time is over

        if (!ret && !rli->reported_unsafe_warning)
        {
          rli->report(WARNING_LEVEL, 0,
                      !is_parallel_warn ?
                      "Request to stop slave SQL Thread received while "
                      "applying a group that has non-transactional "
                      "changes; waiting for completion of the group ... "
                      :
                      "Coordinator thread of multi-threaded slave is being "
                      "stopped in the middle of assigning a group of events; "
                      "deferring to exit until the group completion ... ");
          rli->reported_unsafe_warning= true;
        }
      }
      if (ret)
      {
        if (is_parallel_warn)
          rli->report(!rli->is_error() ? ERROR_LEVEL :
                      WARNING_LEVEL,    // an error was reported by Worker
                      ER_MTS_INCONSISTENT_DATA,
                      ER(ER_MTS_INCONSISTENT_DATA),
                      msg_stopped_mts);
        else
          rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                      ER(ER_SLAVE_FATAL_ERROR), msg_stopped);
      }
    }
    else
    {
      ret= TRUE;
    }
  }
  if (ret)
  {
    rli->last_event_start_time= 0;
    if (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP)
    {
      rli->mts_group_status= Relay_log_info::MTS_KILLED_GROUP;
    }
  }
  
  DBUG_RETURN(ret);
}


/*
  skip_load_data_infile()

  NOTES
    This is used to tell a 3.23 master to break send_file()
*/

void skip_load_data_infile(NET *net)
{
  DBUG_ENTER("skip_load_data_infile");

  (void)net_request_file(net, "/dev/null");
  (void)my_net_read(net);                               // discard response
  (void)net_write_command(net, 0, (uchar*) "", 0, (uchar*) "", 0); // ok
  DBUG_VOID_RETURN;
}


bool net_request_file(NET* net, const char* fname)
{
  DBUG_ENTER("net_request_file");
  DBUG_RETURN(net_write_command(net, 251, (uchar*) fname, strlen(fname),
                                (uchar*) "", 0));
}

/*
  From other comments and tests in code, it looks like
  sometimes Query_log_event and Load_log_event can have db == 0
  (see rewrite_db() above for example)
  (cases where this happens are unclear; it may be when the master is 3.23).
*/

const char *print_slave_db_safe(const char* db)
{
  DBUG_ENTER("*print_slave_db_safe");

  DBUG_RETURN((db ? db : ""));
}

/*
  Check if the error is caused by network.
  @param[in]   errorno   Number of the error.
  RETURNS:
  TRUE         network error
  FALSE        not network error
*/

bool is_network_error(uint errorno)
{ 
  if (errorno == CR_CONNECTION_ERROR || 
      errorno == CR_CONN_HOST_ERROR ||
      errorno == CR_SERVER_GONE_ERROR ||
      errorno == CR_SERVER_LOST ||
      errorno == ER_CON_COUNT_ERROR ||
      errorno == ER_SERVER_SHUTDOWN)
    return TRUE;

  return FALSE;   
}


/**
  Execute an initialization query for the IO thread.

  If there is an error, then this function calls mysql_free_result;
  otherwise the MYSQL object holds the result after this call.  If
  there is an error other than allowed_error, then this function
  prints a message and returns -1.

  @param mysql MYSQL object.
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
enum enum_command_status
{ COMMAND_STATUS_OK, COMMAND_STATUS_ERROR, COMMAND_STATUS_ALLOWED_ERROR };
static enum_command_status
io_thread_init_command(Master_info *mi, const char *query, int allowed_error,
                       MYSQL_RES **master_res= NULL,
                       MYSQL_ROW *master_row= NULL)
{
  DBUG_ENTER("io_thread_init_command");
  DBUG_PRINT("info", ("IO thread initialization command: '%s'", query));
  MYSQL *mysql= mi->mysql;
  int ret= mysql_real_query(mysql, query, strlen(query));
  if (io_slave_killed(mi->info_thd, mi))
  {
    sql_print_information("The slave IO thread was killed while executing "
                          "initialization query '%s'", query);
    mysql_free_result(mysql_store_result(mysql));
    DBUG_RETURN(COMMAND_STATUS_ERROR);
  }
  if (ret != 0)
  {
    int err= mysql_errno(mysql);
    mysql_free_result(mysql_store_result(mysql));
    if (!err || err != allowed_error)
    {
      mi->report(is_network_error(err) ? WARNING_LEVEL : ERROR_LEVEL, err,
                 "The slave IO thread stops because the initialization query "
                 "'%s' failed with error '%s'.",
                 query, mysql_error(mysql));
      DBUG_RETURN(COMMAND_STATUS_ERROR);
    }
    DBUG_RETURN(COMMAND_STATUS_ALLOWED_ERROR);
  }
  if (master_res != NULL)
  {
    if ((*master_res= mysql_store_result(mysql)) == NULL)
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "The slave IO thread stops because the initialization query "
                 "'%s' did not return any result.",
                 query);
      DBUG_RETURN(COMMAND_STATUS_ERROR);
    }
    if (master_row != NULL)
    {
      if ((*master_row= mysql_fetch_row(*master_res)) == NULL)
      {
        mysql_free_result(*master_res);
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "The slave IO thread stops because the initialization query "
                   "'%s' did not return any row.",
                   query);
        DBUG_RETURN(COMMAND_STATUS_ERROR);
      }
    }
  }
  else
    DBUG_ASSERT(master_row == NULL);
  DBUG_RETURN(COMMAND_STATUS_OK);
}


/**
  Set user variables after connecting to the master.

  @param  mysql MYSQL to request uuid from master.
  @param  mi    Master_info to set master_uuid

  @return 0: Success, 1: Fatal error, 2: Network error.
 */
int io_thread_init_commands(MYSQL *mysql, Master_info *mi)
{
  char query[256];
  int ret= 0;

  sprintf(query, "SET @slave_uuid= '%s'", server_uuid);
  if (mysql_real_query(mysql, query, strlen(query))
      && !check_io_slave_killed(mi->info_thd, mi, NULL))
    goto err;

  mysql_free_result(mysql_store_result(mysql));
  return ret;

err:
  if (mysql_errno(mysql) && is_network_error(mysql_errno(mysql)))
  {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "The initialization command '%s' failed with the following"
               " error: '%s'.", query, mysql_error(mysql));
    ret= 2;
  }
  else
  {
    char errmsg[512];
    const char *errmsg_fmt=
      "The slave I/O thread stops because a fatal error is encountered "
      "when it tries to send query to master(query: %s).";

    sprintf(errmsg, errmsg_fmt, query);
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR),
               errmsg);
    ret= 1;
  }
  mysql_free_result(mysql_store_result(mysql));
  return ret;
}

/**
  Get master's uuid on connecting.

  @param  mysql MYSQL to request uuid from master.
  @param  mi    Master_info to set master_uuid

  @return 0: Success, 1: Fatal error, 2: Network error.
*/
static int get_master_uuid(MYSQL *mysql, Master_info *mi)
{
  const char *errmsg;
  MYSQL_RES *master_res= NULL;
  MYSQL_ROW master_row= NULL;
  int ret= 0;

  DBUG_EXECUTE_IF("dbug.before_get_MASTER_UUID",
                  {
                    const char act[]= "now wait_for signal.get_master_uuid";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  if (!mysql_real_query(mysql,
                        STRING_WITH_LEN("SHOW VARIABLES LIKE 'SERVER_UUID'")) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    if (!strcmp(::server_uuid, master_row[1]) &&
        !mi->rli->replicate_same_server_id)
    {
      errmsg= "The slave I/O thread stops because master and slave have equal "
              "MySQL server UUIDs; these UUIDs must be different for "
              "replication to work.";
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR),
                 errmsg);
      // Fatal error
      ret= 1;
    }
    else
    {
      if (mi->master_uuid[0] != 0 && strcmp(mi->master_uuid, master_row[1]))
        sql_print_warning("The master's UUID has changed, although this should"
                          " not happen unless you have changed it manually."
                          " The old UUID was %s.",
                          mi->master_uuid);
      strncpy(mi->master_uuid, master_row[1], UUID_LENGTH);
      mi->master_uuid[UUID_LENGTH]= 0;
    }
  }
  else if (mysql_errno(mysql))
  {
    if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master SERVER_UUID failed with error: %s",
                 mysql_error(mysql));
      ret= 2;
    }
    else
    {
      /* Fatal error */
      errmsg= "The slave I/O thread stops because a fatal error is encountered "
        "when it tries to get the value of SERVER_UUID variable from master.";
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR),
                 errmsg);
      ret= 1;
    }
  }
  else if (!master_row && master_res)
  {
    mi->report(WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
               "Unknown system variable 'SERVER_UUID' on master, "
               "maybe it is a *VERY OLD MASTER*.");
  }

  if (master_res)
    mysql_free_result(master_res);
  return ret;
}

/*
  Note that we rely on the master's version (3.23, 4.0.14 etc) instead of
  relying on the binlog's version. This is not perfect: imagine an upgrade
  of the master without waiting that all slaves are in sync with the master;
  then a slave could be fooled about the binlog's format. This is what happens
  when people upgrade a 3.23 master to 4.0 without doing RESET MASTER: 4.0
  slaves are fooled. So we do this only to distinguish between 3.23 and more
  recent masters (it's too late to change things for 3.23).

  RETURNS
  0       ok
  1       error
  2       transient network problem, the caller should try to reconnect
*/

static int get_master_version_and_clock(MYSQL* mysql, Master_info* mi)
{
  char err_buff[MAX_SLAVE_ERRMSG];
  const char* errmsg= 0;
  int err_code= 0;
  MYSQL_RES *master_res= 0;
  MYSQL_ROW master_row;
  DBUG_ENTER("get_master_version_and_clock");

  /*
    Free old mi_description_event (that is needed if we are in
    a reconnection).
  */
  mysql_mutex_lock(&mi->data_lock);
  mi->set_mi_description_event(NULL);

  if (!my_isdigit(&my_charset_bin,*mysql->server_version))
  {
    errmsg = "Master reported unrecognized MySQL version";
    err_code= ER_SLAVE_FATAL_ERROR;
    sprintf(err_buff, ER(err_code), errmsg);
  }
  else
  {
    /*
      Note the following switch will bug when we have MySQL branch 30 ;)
    */
    switch (*mysql->server_version)
    {
    case '0':
    case '1':
    case '2':
      errmsg = "Master reported unrecognized MySQL version";
      err_code= ER_SLAVE_FATAL_ERROR;
      sprintf(err_buff, ER(err_code), errmsg);
      break;
    case '3':
      mi->set_mi_description_event(new
        Format_description_log_event(1, mysql->server_version));
      break;
    case '4':
      mi->set_mi_description_event(new
        Format_description_log_event(3, mysql->server_version));
      break;
    default:
      /*
        Master is MySQL >=5.0. Give a default Format_desc event, so that we can
        take the early steps (like tests for "is this a 3.23 master") which we
        have to take before we receive the real master's Format_desc which will
        override this one. Note that the Format_desc we create below is garbage
        (it has the format of the *slave*); it's only good to help know if the
        master is 3.23, 4.0, etc.
      */
      mi->set_mi_description_event(new
        Format_description_log_event(4, mysql->server_version));
      break;
    }
  }

  /*
     This does not mean that a 5.0 slave will be able to read a 5.5 master; but
     as we don't know yet, we don't want to forbid this for now. If a 5.0 slave
     can't read a 5.5 master, this will show up when the slave can't read some
     events sent by the master, and there will be error messages.
  */

  if (errmsg)
    goto err;

  /* as we are here, we tried to allocate the event */
  if (mi->get_mi_description_event() == NULL)
  {
    mysql_mutex_unlock(&mi->data_lock);
    errmsg= "default Format_description_log_event";
    err_code= ER_SLAVE_CREATE_EVENT_FAILURE;
    sprintf(err_buff, ER(err_code), errmsg);
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
  mi->get_mi_description_event()->checksum_alg=
    mi->rli->relay_log.relay_log_checksum_alg;

  DBUG_ASSERT(mi->get_mi_description_event()->checksum_alg !=
              BINLOG_CHECKSUM_ALG_UNDEF);
  DBUG_ASSERT(mi->rli->relay_log.relay_log_checksum_alg !=
              BINLOG_CHECKSUM_ALG_UNDEF); 

  mysql_mutex_unlock(&mi->data_lock);

  /*
    Compare the master and slave's clock. Do not die if master's clock is
    unavailable (very old master not supporting UNIX_TIMESTAMP()?).
  */

  DBUG_EXECUTE_IF("dbug.before_get_UNIX_TIMESTAMP",
                  {
                    const char act[]=
                      "now "
                      "wait_for signal.get_unix_timestamp";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  master_res= NULL;
  if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT UNIX_TIMESTAMP()")) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    mysql_mutex_lock(&mi->data_lock);
    mi->clock_diff_with_master=
      (long) (time((time_t*) 0) - strtoul(master_row[0], 0, 10));
    mysql_mutex_unlock(&mi->data_lock);
  }
  else if (check_io_slave_killed(mi->info_thd, mi, NULL))
    goto slave_killed_err;
  else if (is_network_error(mysql_errno(mysql)))
  {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "Get master clock failed with error: %s", mysql_error(mysql));
    goto network_err;
  }
  else 
  {
    mysql_mutex_lock(&mi->data_lock);
    mi->clock_diff_with_master= 0; /* The "most sensible" value */
    mysql_mutex_unlock(&mi->data_lock);
    sql_print_warning("\"SELECT UNIX_TIMESTAMP()\" failed on master, "
                      "do not trust column Seconds_Behind_Master of SHOW "
                      "SLAVE STATUS. Error: %s (%d)",
                      mysql_error(mysql), mysql_errno(mysql));
  }
  if (master_res)
  {
    mysql_free_result(master_res);
    master_res= NULL;
  }

  /*
    Check that the master's server id and ours are different. Because if they
    are equal (which can result from a simple copy of master's datadir to slave,
    thus copying some my.cnf), replication will work but all events will be
    skipped.
    Do not die if SHOW VARIABLES LIKE 'SERVER_ID' fails on master (very old
    master?).
    Note: we could have put a @@SERVER_ID in the previous SELECT
    UNIX_TIMESTAMP() instead, but this would not have worked on 3.23 masters.
  */
  DBUG_EXECUTE_IF("dbug.before_get_SERVER_ID",
                  {
                    const char act[]=
                      "now "
                      "wait_for signal.get_server_id";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, 
                                                       STRING_WITH_LEN(act)));
                  };);
  master_res= NULL;
  master_row= NULL;
  if (!mysql_real_query(mysql,
                        STRING_WITH_LEN("SHOW VARIABLES LIKE 'SERVER_ID'")) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    if ((::server_id == (mi->master_id= strtoul(master_row[1], 0, 10))) &&
        !mi->rli->replicate_same_server_id)
    {
      errmsg= "The slave I/O thread stops because master and slave have equal \
MySQL server ids; these ids must be different for replication to work (or \
the --replicate-same-server-id option must be used on slave but this does \
not always make sense; please check the manual before using it).";
      err_code= ER_SLAVE_FATAL_ERROR;
      sprintf(err_buff, ER(err_code), errmsg);
      goto err;
    }
  }
  else if (mysql_errno(mysql))
  {
    if (check_io_slave_killed(mi->info_thd, mi, NULL))
      goto slave_killed_err;
    else if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master SERVER_ID failed with error: %s", mysql_error(mysql));
      goto network_err;
    }
    /* Fatal error */
    errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of SERVER_ID variable from master.";
    err_code= mysql_errno(mysql);
    sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
    goto err;
  }
  else if (!master_row && master_res)
  {
    mi->report(WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
               "Unknown system variable 'SERVER_ID' on master, \
maybe it is a *VERY OLD MASTER*.");
  }
  if (master_res)
  {
    mysql_free_result(master_res);
    master_res= NULL;
  }
  if (mi->master_id == 0 && mi->ignore_server_ids->dynamic_ids.elements > 0)
  {
    errmsg= "Slave configured with server id filtering could not detect the master server id.";
    err_code= ER_SLAVE_FATAL_ERROR;
    sprintf(err_buff, ER(err_code), errmsg);
    goto err;
  }

  /*
    Check that the master's global character_set_server and ours are the same.
    Not fatal if query fails (old master?).
    Note that we don't check for equality of global character_set_client and
    collation_connection (neither do we prevent their setting in
    set_var.cc). That's because from what I (Guilhem) have tested, the global
    values of these 2 are never used (new connections don't use them).
    We don't test equality of global collation_database either as it's is
    going to be deprecated (made read-only) in 4.1 very soon.
    The test is only relevant if master < 5.0.3 (we'll test only if it's older
    than the 5 branch; < 5.0.3 was alpha...), as >= 5.0.3 master stores
    charset info in each binlog event.
    We don't do it for 3.23 because masters <3.23.50 hang on
    SELECT @@unknown_var (BUG#7965 - see changelog of 3.23.50). So finally we
    test only if master is 4.x.
  */

  /* redundant with rest of code but safer against later additions */
  if (*mysql->server_version == '3')
    goto err;

  if (*mysql->server_version == '4')
  {
    master_res= NULL;
    if (!mysql_real_query(mysql,
                          STRING_WITH_LEN("SELECT @@GLOBAL.COLLATION_SERVER")) &&
        (master_res= mysql_store_result(mysql)) &&
        (master_row= mysql_fetch_row(master_res)))
    {
      if (strcmp(master_row[0], global_system_variables.collation_server->name))
      {
        errmsg= "The slave I/O thread stops because master and slave have \
different values for the COLLATION_SERVER global variable. The values must \
be equal for the Statement-format replication to work";
        err_code= ER_SLAVE_FATAL_ERROR;
        sprintf(err_buff, ER(err_code), errmsg);
        goto err;
      }
    }
    else if (check_io_slave_killed(mi->info_thd, mi, NULL))
      goto slave_killed_err;
    else if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master COLLATION_SERVER failed with error: %s", mysql_error(mysql));
      goto network_err;
    }
    else if (mysql_errno(mysql) != ER_UNKNOWN_SYSTEM_VARIABLE)
    {
      /* Fatal error */
      errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of COLLATION_SERVER global variable from master.";
      err_code= mysql_errno(mysql);
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      goto err;
    }
    else
      mi->report(WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
                 "Unknown system variable 'COLLATION_SERVER' on master, \
maybe it is a *VERY OLD MASTER*. *NOTE*: slave may experience \
inconsistency if replicated data deals with collation.");

    if (master_res)
    {
      mysql_free_result(master_res);
      master_res= NULL;
    }
  }

  /*
    Perform analogous check for time zone. Theoretically we also should
    perform check here to verify that SYSTEM time zones are the same on
    slave and master, but we can't rely on value of @@system_time_zone
    variable (it is time zone abbreviation) since it determined at start
    time and so could differ for slave and master even if they are really
    in the same system time zone. So we are omiting this check and just
    relying on documentation. Also according to Monty there are many users
    who are using replication between servers in various time zones. Hence
    such check will broke everything for them. (And now everything will
    work for them because by default both their master and slave will have
    'SYSTEM' time zone).
    This check is only necessary for 4.x masters (and < 5.0.4 masters but
    those were alpha).
  */
  if (*mysql->server_version == '4')
  {
    master_res= NULL;
    if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT @@GLOBAL.TIME_ZONE")) &&
        (master_res= mysql_store_result(mysql)) &&
        (master_row= mysql_fetch_row(master_res)))
    {
      if (strcmp(master_row[0],
                 global_system_variables.time_zone->get_name()->ptr()))
      {
        errmsg= "The slave I/O thread stops because master and slave have \
different values for the TIME_ZONE global variable. The values must \
be equal for the Statement-format replication to work";
        err_code= ER_SLAVE_FATAL_ERROR;
        sprintf(err_buff, ER(err_code), errmsg);
        goto err;
      }
    }
    else if (check_io_slave_killed(mi->info_thd, mi, NULL))
      goto slave_killed_err;
    else if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master TIME_ZONE failed with error: %s", mysql_error(mysql));
      goto network_err;
    } 
    else
    {
      /* Fatal error */
      errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of TIME_ZONE global variable from master.";
      err_code= mysql_errno(mysql);
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      goto err;
    }
    if (master_res)
    {
      mysql_free_result(master_res);
      master_res= NULL;
    }
  }

  if (mi->heartbeat_period != 0.0)
  {
    char llbuf[22];
    const char query_format[]= "SET @master_heartbeat_period= %s";
    char query[sizeof(query_format) - 2 + sizeof(llbuf)];
    /* 
       the period is an ulonglong of nano-secs. 
    */
    llstr((ulonglong) (mi->heartbeat_period*1000000000UL), llbuf);
    sprintf(query, query_format, llbuf);

    if (mysql_real_query(mysql, query, strlen(query)))
    {
      if (check_io_slave_killed(mi->info_thd, mi, NULL))
        goto slave_killed_err;

      if (is_network_error(mysql_errno(mysql)))
      {
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "SET @master_heartbeat_period to master failed with error: %s",
                   mysql_error(mysql));
        mysql_free_result(mysql_store_result(mysql));
        goto network_err;
      }
      else
      {
        /* Fatal error */
        errmsg= "The slave I/O thread stops because a fatal error is encountered "
          " when it tries to SET @master_heartbeat_period on master.";
        err_code= ER_SLAVE_FATAL_ERROR;
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
  if (DBUG_EVALUATE_IF("simulate_slave_unaware_checksum", 0, 1))
  {
    int rc;
    const char query[]= "SET @master_binlog_checksum= @@global.binlog_checksum";
    master_res= NULL;
    mi->checksum_alg_before_fd= BINLOG_CHECKSUM_ALG_UNDEF; //initially undefined
    /*
      @c checksum_alg_before_fd is queried from master in this block.
      If master is old checksum-unaware the value stays undefined.
      Once the first FD will be received its alg descriptor will replace
      the being queried one.
    */
    rc= mysql_real_query(mysql, query, strlen(query));
    if (rc != 0)
    {
      mi->checksum_alg_before_fd= BINLOG_CHECKSUM_ALG_OFF;
      if (check_io_slave_killed(mi->info_thd, mi, NULL))
        goto slave_killed_err;

      if (mysql_errno(mysql) == ER_UNKNOWN_SYSTEM_VARIABLE)
      {
        // this is tolerable as OM -> NS is supported
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "Notifying master by %s failed with "
                   "error: %s", query, mysql_error(mysql));
      }
      else
      {
        if (is_network_error(mysql_errno(mysql)))
        {
          mi->report(WARNING_LEVEL, mysql_errno(mysql),
                     "Notifying master by %s failed with "
                     "error: %s", query, mysql_error(mysql));
          mysql_free_result(mysql_store_result(mysql));
          goto network_err;
        }
        else
        {
          errmsg= "The slave I/O thread stops because a fatal error is encountered "
            "when it tried to SET @master_binlog_checksum on master.";
          err_code= ER_SLAVE_FATAL_ERROR;
          sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
          mysql_free_result(mysql_store_result(mysql));
          goto err;
        }
      }
    }
    else
    {
      mysql_free_result(mysql_store_result(mysql));
      if (!mysql_real_query(mysql,
                            STRING_WITH_LEN("SELECT @master_binlog_checksum")) &&
          (master_res= mysql_store_result(mysql)) &&
          (master_row= mysql_fetch_row(master_res)) &&
          (master_row[0] != NULL))
      {
        mi->checksum_alg_before_fd= (uint8)
          find_type(master_row[0], &binlog_checksum_typelib, 1) - 1;
        
       DBUG_EXECUTE_IF("undefined_algorithm_on_slave",
        mi->checksum_alg_before_fd = BINLOG_CHECKSUM_ALG_UNDEF;);
       if(mi->checksum_alg_before_fd == BINLOG_CHECKSUM_ALG_UNDEF) 
       {
         errmsg= "The slave I/O thread was stopped because a fatal error is encountered "
                 "The checksum algorithm used by master is unknown to slave.";
         err_code= ER_SLAVE_FATAL_ERROR;
         sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
         mysql_free_result(mysql_store_result(mysql));
         goto err;
       }

        // valid outcome is either of
        DBUG_ASSERT(mi->checksum_alg_before_fd == BINLOG_CHECKSUM_ALG_OFF ||
                    mi->checksum_alg_before_fd == BINLOG_CHECKSUM_ALG_CRC32);
      }
      else if (check_io_slave_killed(mi->info_thd, mi, NULL))
        goto slave_killed_err;
      else if (is_network_error(mysql_errno(mysql)))
      {
        mi->report(WARNING_LEVEL, mysql_errno(mysql),
                   "Get master BINLOG_CHECKSUM failed with error: %s", mysql_error(mysql));
        goto network_err;
      }
      else
      {
        errmsg= "The slave I/O thread stops because a fatal error is encountered "
          "when it tried to SELECT @master_binlog_checksum.";
        err_code= ER_SLAVE_FATAL_ERROR;
        sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
        mysql_free_result(mysql_store_result(mysql));
        goto err;
      }
    }
    if (master_res)
    {
      mysql_free_result(master_res);
      master_res= NULL;
    }
  }
  else
    mi->checksum_alg_before_fd= BINLOG_CHECKSUM_ALG_OFF;

  if (DBUG_EVALUATE_IF("simulate_slave_unaware_gtid", 0, 1))
  {
    switch (io_thread_init_command(mi, "SELECT @@GLOBAL.GTID_MODE",
                                   ER_UNKNOWN_SYSTEM_VARIABLE,
                                   &master_res, &master_row))
    {
    case COMMAND_STATUS_ERROR:
      DBUG_RETURN(2);
    case COMMAND_STATUS_ALLOWED_ERROR:
      // master is old and does not have @@GLOBAL.GTID_MODE
      mi->master_gtid_mode= 0;
      break;
    case COMMAND_STATUS_OK:
      int typelib_index= find_type(master_row[0], &gtid_mode_typelib, 1);
      mysql_free_result(master_res);
      if (typelib_index == 0)
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                   "The slave IO thread stops because the master has "
                   "an unknown GTID_MODE.");
        DBUG_RETURN(1);
      }
      mi->master_gtid_mode= typelib_index - 1;
      break;
    }
    if (mi->master_gtid_mode > gtid_mode + 1 ||
        gtid_mode > mi->master_gtid_mode + 1)
    {
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                 "The slave IO thread stops because the master has GTID_MODE "
                 "%s and this server has GTID_MODE %s",
                 gtid_mode_names[mi->master_gtid_mode],
                 gtid_mode_names[gtid_mode]);
      DBUG_RETURN(1);
    }
    if (mi->is_auto_position() && mi->master_gtid_mode != 3)
    {
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                 "The slave IO thread stops because the master has GTID_MODE "
                 "%s and we are trying to connect using MASTER_AUTO_POSITION.",
                 gtid_mode_names[mi->master_gtid_mode]);
      DBUG_RETURN(1);
    }
  }

err:
  if (errmsg)
  {
    if (master_res)
      mysql_free_result(master_res);
    DBUG_ASSERT(err_code != 0);
    mi->report(ERROR_LEVEL, err_code, "%s", err_buff);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);

network_err:
  if (master_res)
    mysql_free_result(master_res);
  DBUG_RETURN(2);

slave_killed_err:
  if (master_res)
    mysql_free_result(master_res);
  DBUG_RETURN(2);
}

static bool wait_for_relay_log_space(Relay_log_info* rli)
{
  bool slave_killed=0;
  Master_info* mi = rli->mi;
  PSI_stage_info old_stage;
  THD* thd = mi->info_thd;
  DBUG_ENTER("wait_for_relay_log_space");

  mysql_mutex_lock(&rli->log_space_lock);
  thd->ENTER_COND(&rli->log_space_cond,
                  &rli->log_space_lock,
                  &stage_waiting_for_relay_log_space,
                  &old_stage);
  while (rli->log_space_limit < rli->log_space_total &&
         !(slave_killed=io_slave_killed(thd,mi)) &&
         !rli->ignore_log_space_limit)
    mysql_cond_wait(&rli->log_space_cond, &rli->log_space_lock);

  /* 
    Makes the IO thread read only one event at a time
    until the SQL thread is able to purge the relay 
    logs, freeing some space.

    Therefore, once the SQL thread processes this next 
    event, it goes to sleep (no more events in the queue),
    sets ignore_log_space_limit=true and wakes the IO thread. 
    However, this event may have been enough already for 
    the SQL thread to purge some log files, freeing 
    rli->log_space_total .

    This guarantees that the SQL and IO thread move
    forward only one event at a time (to avoid deadlocks), 
    when the relay space limit is reached. It also 
    guarantees that when the SQL thread is prepared to
    rotate (to be able to purge some logs), the IO thread
    will know about it and will rotate.

    NOTE: The ignore_log_space_limit is only set when the SQL
          thread sleeps waiting for events.

   */
  if (rli->ignore_log_space_limit)
  {
#ifndef DBUG_OFF
    {
      char llbuf1[22], llbuf2[22];
      DBUG_PRINT("info", ("log_space_limit=%s "
                          "log_space_total=%s "
                          "ignore_log_space_limit=%d "
                          "sql_force_rotate_relay=%d", 
                        llstr(rli->log_space_limit,llbuf1),
                        llstr(rli->log_space_total,llbuf2),
                        (int) rli->ignore_log_space_limit,
                        (int) rli->sql_force_rotate_relay));
    }
#endif
    if (rli->sql_force_rotate_relay)
    {
      mysql_mutex_lock(&mi->data_lock);
      rotate_relay_log(mi);
      mysql_mutex_unlock(&mi->data_lock);
      rli->sql_force_rotate_relay= false;
    }

    rli->ignore_log_space_limit= false;
  }

  thd->EXIT_COND(&old_stage);
  DBUG_RETURN(slave_killed);
}


/*
  Builds a Rotate from the ignored events' info and writes it to relay log.

  The caller must hold mi->data_lock before invoking this function.

  @param thd pointer to I/O Thread's Thd.
  @param mi  point to I/O Thread metadata class.

  @return 0 if everything went fine, 1 otherwise.
*/
static int write_ignored_events_info_to_relay_log(THD *thd, Master_info *mi)
{
  Relay_log_info *rli= mi->rli;
  mysql_mutex_t *log_lock= rli->relay_log.get_log_lock();
  int error= 0;
  DBUG_ENTER("write_ignored_events_info_to_relay_log");

  DBUG_ASSERT(thd == mi->info_thd);
  mysql_mutex_assert_owner(&mi->data_lock);
  mysql_mutex_lock(log_lock);
  if (rli->ign_master_log_name_end[0])
  {
    DBUG_PRINT("info",("writing a Rotate event to track down ignored events"));
    Rotate_log_event *ev= new Rotate_log_event(rli->ign_master_log_name_end,
                                               0, rli->ign_master_log_pos_end,
                                               Rotate_log_event::DUP_NAME);
    if (mi->get_mi_description_event() != NULL)
      ev->checksum_alg= mi->get_mi_description_event()->checksum_alg;
    
    rli->ign_master_log_name_end[0]= 0;
    /* can unlock before writing as slave SQL thd will soon see our Rotate */
    mysql_mutex_unlock(log_lock);
    if (likely((bool)ev))
    {
      ev->server_id= 0; // don't be ignored by slave SQL thread
      if (unlikely(rli->relay_log.append_event(ev, mi) != 0))
        mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                   ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                   "failed to write a Rotate event"
                   " to the relay log, SHOW SLAVE STATUS may be"
                   " inaccurate");
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
      if (flush_master_info(mi, TRUE))
      {
        error= 1;
        sql_print_error("Failed to flush master info file.");
      }
      delete ev;
    }
    else
    {
      error= 1;
      mi->report(ERROR_LEVEL, ER_SLAVE_CREATE_EVENT_FAILURE,
                 ER(ER_SLAVE_CREATE_EVENT_FAILURE),
                 "Rotate_event (out of memory?),"
                 " SHOW SLAVE STATUS may be inaccurate");
    }
  }
  else
    mysql_mutex_unlock(log_lock);

  DBUG_RETURN(error);
}


int register_slave_on_master(MYSQL* mysql, Master_info *mi,
                             bool *suppress_warnings)
{
  uchar buf[1024], *pos= buf;
  uint report_host_len=0, report_user_len=0, report_password_len=0;
  DBUG_ENTER("register_slave_on_master");

  *suppress_warnings= FALSE;
  if (report_host)
    report_host_len= strlen(report_host);
  if (report_host_len > HOSTNAME_LENGTH)
  {
    sql_print_warning("The length of report_host is %d. "
                      "It is larger than the max length(%d), so this "
                      "slave cannot be registered to the master.",
                      report_host_len, HOSTNAME_LENGTH);
    DBUG_RETURN(0);
  }

  if (report_user)
    report_user_len= strlen(report_user);
  if (report_user_len > USERNAME_LENGTH)
  {
    sql_print_warning("The length of report_user is %d. "
                      "It is larger than the max length(%d), so this "
                      "slave cannot be registered to the master.",
                      report_user_len, USERNAME_LENGTH);
    DBUG_RETURN(0);
  }

  if (report_password)
    report_password_len= strlen(report_password);
  if (report_password_len > MAX_PASSWORD_LENGTH)
  {
    sql_print_warning("The length of report_password is %d. "
                      "It is larger than the max length(%d), so this "
                      "slave cannot be registered to the master.",
                      report_password_len, MAX_PASSWORD_LENGTH);
    DBUG_RETURN(0);
  }

  int4store(pos, server_id); pos+= 4;
  pos= net_store_data(pos, (uchar*) report_host, report_host_len);
  pos= net_store_data(pos, (uchar*) report_user, report_user_len);
  pos= net_store_data(pos, (uchar*) report_password, report_password_len);
  int2store(pos, (uint16) report_port); pos+= 2;
  /* 
    Fake rpl_recovery_rank, which was removed in BUG#13963,
    so that this server can register itself on old servers,
    see BUG#49259.
   */
  int4store(pos, /* rpl_recovery_rank */ 0);    pos+= 4;
  /* The master will fill in master_id */
  int4store(pos, 0);                    pos+= 4;

  if (simple_command(mysql, COM_REGISTER_SLAVE, buf, (size_t) (pos- buf), 0))
  {
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      *suppress_warnings= TRUE;                 // Suppress reconnect warning
    }
    else if (!check_io_slave_killed(mi->info_thd, mi, NULL))
    {
      char buf[256];
      my_snprintf(buf, sizeof(buf), "%s (Errno: %d)", mysql_error(mysql), 
                  mysql_errno(mysql));
      mi->report(ERROR_LEVEL, ER_SLAVE_MASTER_COM_FAILURE,
                 ER(ER_SLAVE_MASTER_COM_FAILURE), "COM_REGISTER_SLAVE", buf);
    }
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  Execute a SHOW SLAVE STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the IO thread.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_slave_status(THD* thd, Master_info* mi)
{
  // TODO: fix this for multi-master
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  char *slave_sql_running_state= NULL;
  char *sql_gtid_set_buffer= NULL, *io_gtid_set_buffer= NULL;
  int sql_gtid_set_size= 0, io_gtid_set_size= 0;
  DBUG_ENTER("show_slave_status");
 
  if (mi != NULL)
  { 
    global_sid_lock->wrlock();
    const Gtid_set* sql_gtid_set= gtid_state->get_logged_gtids();
    const Gtid_set* io_gtid_set= mi->rli->get_gtid_set();
    if ((sql_gtid_set_size= sql_gtid_set->to_string(&sql_gtid_set_buffer)) < 0 ||
        (io_gtid_set_size= io_gtid_set->to_string(&io_gtid_set_buffer)) < 0)
    {
      my_eof(thd);
      my_free(sql_gtid_set_buffer);
      my_free(io_gtid_set_buffer);
      global_sid_lock->unlock();
      DBUG_RETURN(true);
    }
    global_sid_lock->unlock();
  }

  field_list.push_back(new Item_empty_string("Slave_IO_State",
                                                     14));
  field_list.push_back(new Item_empty_string("Master_Host", mi != NULL ?
                                                     sizeof(mi->host) : 0));
  field_list.push_back(new Item_empty_string("Master_User", mi != NULL ?
                                                     mi->get_user_size() : 0));
  field_list.push_back(new Item_return_int("Master_Port", 7,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Connect_Retry", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Master_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_return_int("Read_Master_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Relay_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_return_int("Relay_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Relay_Master_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_empty_string("Slave_IO_Running", 3));
  field_list.push_back(new Item_empty_string("Slave_SQL_Running", 3));
  field_list.push_back(new Item_empty_string("Replicate_Do_DB", 20));
  field_list.push_back(new Item_empty_string("Replicate_Ignore_DB", 20));
  field_list.push_back(new Item_empty_string("Replicate_Do_Table", 20));
  field_list.push_back(new Item_empty_string("Replicate_Ignore_Table", 23));
  field_list.push_back(new Item_empty_string("Replicate_Wild_Do_Table", 24));
  field_list.push_back(new Item_empty_string("Replicate_Wild_Ignore_Table",
                                             28));
  field_list.push_back(new Item_return_int("Last_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_Error", 20));
  field_list.push_back(new Item_return_int("Skip_Counter", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Exec_Master_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_return_int("Relay_Log_Space", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Until_Condition", 6));
  field_list.push_back(new Item_empty_string("Until_Log_File", FN_REFLEN));
  field_list.push_back(new Item_return_int("Until_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Master_SSL_Allowed", 7));
  field_list.push_back(new Item_empty_string("Master_SSL_CA_File", mi != NULL ?
                                             sizeof(mi->ssl_ca) : 0));
  field_list.push_back(new Item_empty_string("Master_SSL_CA_Path", mi != NULL ?
                                             sizeof(mi->ssl_capath) : 0));
  field_list.push_back(new Item_empty_string("Master_SSL_Cert", mi != NULL ?
                                             sizeof(mi->ssl_cert) : 0));
  field_list.push_back(new Item_empty_string("Master_SSL_Cipher", mi != NULL ?
                                             sizeof(mi->ssl_cipher) : 0));
  field_list.push_back(new Item_empty_string("Master_SSL_Key", mi != NULL ?
                                             sizeof(mi->ssl_key) : 0));
  field_list.push_back(new Item_return_int("Seconds_Behind_Master", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Master_SSL_Verify_Server_Cert",
                                             3));
  field_list.push_back(new Item_return_int("Last_IO_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_IO_Error", 20));
  field_list.push_back(new Item_return_int("Last_SQL_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_SQL_Error", 20));
  field_list.push_back(new Item_empty_string("Replicate_Ignore_Server_Ids",
                                             FN_REFLEN));
  field_list.push_back(new Item_return_int("Master_Server_Id", sizeof(ulong),
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Master_UUID", UUID_LENGTH));
  field_list.push_back(new Item_empty_string("Master_Info_File",
                                             2 * FN_REFLEN));
  field_list.push_back(new Item_return_int("SQL_Delay", 10, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("SQL_Remaining_Delay", 8, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Slave_SQL_Running_State", 20));
  field_list.push_back(new Item_return_int("Master_Retry_Count", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Master_Bind", mi != NULL ?
                                             sizeof(mi->bind_addr) : 0));
  field_list.push_back(new Item_empty_string("Last_IO_Error_Timestamp", 20));
  field_list.push_back(new Item_empty_string("Last_SQL_Error_Timestamp", 20));
  field_list.push_back(new Item_empty_string("Master_SSL_Crl", mi != NULL ?
                                             sizeof(mi->ssl_crl) : 0));
  field_list.push_back(new Item_empty_string("Master_SSL_Crlpath", mi != NULL ?
                                             sizeof(mi->ssl_crlpath) : 0));
  field_list.push_back(new Item_empty_string("Retrieved_Gtid_Set",
                                             io_gtid_set_size));
  field_list.push_back(new Item_empty_string("Executed_Gtid_Set",
                                             sql_gtid_set_size));
  field_list.push_back(new Item_return_int("Auto_Position", sizeof(ulong),
                                           MYSQL_TYPE_LONG));

  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    my_free(sql_gtid_set_buffer);
    my_free(io_gtid_set_buffer);
    DBUG_RETURN(true);
  }

  if (mi != NULL && mi->host[0])
  {
    DBUG_PRINT("info",("host is set: '%s'", mi->host));
    String *packet= &thd->packet;
    protocol->prepare_for_resend();

    /*
      slave_running can be accessed without run_lock but not other
      non-volotile members like mi->info_thd, which is guarded by the mutex.
    */
    mysql_mutex_lock(&mi->run_lock);
    protocol->store(mi->info_thd ? mi->info_thd->get_proc_info() : "", &my_charset_bin);
    mysql_mutex_unlock(&mi->run_lock);

    mysql_mutex_lock(&mi->rli->run_lock);
    slave_sql_running_state= const_cast<char *>(mi->rli->info_thd ? mi->rli->info_thd->get_proc_info() : "");
    mysql_mutex_unlock(&mi->rli->run_lock);

    mysql_mutex_lock(&mi->data_lock);
    mysql_mutex_lock(&mi->rli->data_lock);
    mysql_mutex_lock(&mi->err_lock);
    mysql_mutex_lock(&mi->rli->err_lock);

    protocol->store(mi->host, &my_charset_bin);
    protocol->store(mi->get_user(), &my_charset_bin);
    protocol->store((uint32) mi->port);
    protocol->store((uint32) mi->connect_retry);
    protocol->store(mi->get_master_log_name(), &my_charset_bin);
    protocol->store((ulonglong) mi->get_master_log_pos());
    protocol->store(mi->rli->get_group_relay_log_name() +
                    dirname_length(mi->rli->get_group_relay_log_name()),
                    &my_charset_bin);
    protocol->store((ulonglong) mi->rli->get_group_relay_log_pos());
    protocol->store(mi->rli->get_group_master_log_name(), &my_charset_bin);
    protocol->store(mi->slave_running == MYSQL_SLAVE_RUN_CONNECT ?
                    "Yes" : (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT ?
                             "Connecting" : "No"), &my_charset_bin);
    protocol->store(mi->rli->slave_running ? "Yes":"No", &my_charset_bin);
    protocol->store(rpl_filter->get_do_db());
    protocol->store(rpl_filter->get_ignore_db());

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
    protocol->store((uint32) mi->rli->slave_skip_counter);
    protocol->store((ulonglong) mi->rli->get_group_master_log_pos());
    protocol->store((ulonglong) mi->rli->log_space_total);

    const char *until_type= "";

    switch (mi->rli->until_condition)
    {
    case Relay_log_info::UNTIL_NONE:
      until_type= "None";
      break;
    case Relay_log_info::UNTIL_MASTER_POS:
      until_type= "Master";
      break;
    case Relay_log_info::UNTIL_RELAY_POS:
      until_type= "Relay";
      break;
    case Relay_log_info::UNTIL_SQL_BEFORE_GTIDS:
      until_type= "SQL_BEFORE_GTIDS";
      break;
    case Relay_log_info::UNTIL_SQL_AFTER_GTIDS:
      until_type= "SQL_AFTER_GTIDS";
      break;
    case Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS:
      until_type= "SQL_AFTER_MTS_GAPS";
#ifndef DBUG_OFF
    case Relay_log_info::UNTIL_DONE:
      until_type= "DONE";
      break;
#endif
    default:
      DBUG_ASSERT(0);
    }
    protocol->store(until_type, &my_charset_bin);
    protocol->store(mi->rli->until_log_name, &my_charset_bin);
    protocol->store((ulonglong) mi->rli->until_log_pos);

#ifdef HAVE_OPENSSL
    protocol->store(mi->ssl? "Yes":"No", &my_charset_bin);
#else
    protocol->store(mi->ssl? "Ignored":"No", &my_charset_bin);
#endif
    protocol->store(mi->ssl_ca, &my_charset_bin);
    protocol->store(mi->ssl_capath, &my_charset_bin);
    protocol->store(mi->ssl_cert, &my_charset_bin);
    protocol->store(mi->ssl_cipher, &my_charset_bin);
    protocol->store(mi->ssl_key, &my_charset_bin);

    /*
      The pseudo code to compute Seconds_Behind_Master:
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
            compute Seconds_Behind_Master;
        }
        else
          print NULL;
    */
    if (mi->rli->slave_running)
    {
      /* Check if SQL thread is at the end of relay log
           Checking should be done using two conditions
           condition1: compare the log positions and
           condition2: compare the file names (to handle rotation case)
      */
      if ((mi->get_master_log_pos() == mi->rli->get_group_master_log_pos()) &&
           (!strcmp(mi->get_master_log_name(), mi->rli->get_group_master_log_name())))
      {
        if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
          protocol->store(0LL);
        else
          protocol->store_null();
      }
      else
      {
        long time_diff= ((long)(time(0) - mi->rli->last_master_timestamp)
                                - mi->clock_diff_with_master);
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
        slave is 2. At SHOW SLAVE STATUS time, assume that the difference
        between timestamp of slave and rli->last_master_timestamp is 0
        (i.e. they are in the same second), then we get 0-(2-1)=-1 as a result.
        This confuses users, so we don't go below 0: hence the max().

        last_master_timestamp == 0 (an "impossible" timestamp 1970) is a
        special marker to say "consider we have caught up".
      */
        protocol->store((longlong)(mi->rli->last_master_timestamp ?
                                   max(0L, time_diff) : 0));
      }
    }
    else
    {
      protocol->store_null();
    }
    protocol->store(mi->ssl_verify_server_cert? "Yes":"No", &my_charset_bin);

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
      for (i= 0, buff[0]= 0, cur_len= 0;
           i < mi->ignore_server_ids->dynamic_ids.elements; i++)
      {
        ulong s_id, slen;
        char sbuff[FN_REFLEN];
        get_dynamic(&(mi->ignore_server_ids->dynamic_ids), (uchar*) &s_id, i);
        slen= sprintf(sbuff, (i == 0 ? "%lu" : ", %lu"), s_id);
        if (cur_len + slen + 4 > FN_REFLEN)
        {
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
    // Master_Server_id
    protocol->store((uint32) mi->master_id);
    protocol->store(mi->master_uuid, &my_charset_bin);
    // Master_Info_File
    protocol->store(mi->get_description_info(), &my_charset_bin);
    // SQL_Delay
    protocol->store((uint32) mi->rli->get_sql_delay());
    // SQL_Remaining_Delay
    if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name)
    {
      time_t t= my_time(0), sql_delay_end= mi->rli->get_sql_delay_end();
      protocol->store((uint32)(t < sql_delay_end ? sql_delay_end - t : 0));
    }
    else
      protocol->store_null();
    // Slave_SQL_Running_State
    protocol->store(slave_sql_running_state, &my_charset_bin);
    // Master_Retry_Count
    protocol->store((ulonglong) mi->retry_count);
    // Master_Bind
    protocol->store(mi->bind_addr, &my_charset_bin);
    // Last_IO_Error_Timestamp
    protocol->store(mi->last_error().timestamp, &my_charset_bin);
    // Last_SQL_Error_Timestamp
    protocol->store(mi->rli->last_error().timestamp, &my_charset_bin);
    // Master_Ssl_Crl
    protocol->store(mi->ssl_ca, &my_charset_bin);
    // Master_Ssl_Crlpath
    protocol->store(mi->ssl_capath, &my_charset_bin);
    // Retrieved_Gtid_Set
    protocol->store(io_gtid_set_buffer, &my_charset_bin);
    // Executed_Gtid_Set
    protocol->store(sql_gtid_set_buffer, &my_charset_bin);
    // Auto_Position
    protocol->store(mi->is_auto_position() ? 1 : 0);

    mysql_mutex_unlock(&mi->rli->err_lock);
    mysql_mutex_unlock(&mi->err_lock);
    mysql_mutex_unlock(&mi->rli->data_lock);
    mysql_mutex_unlock(&mi->data_lock);

    if (my_net_write(&thd->net, (uchar*) thd->packet.ptr(), packet->length()))
    {
      my_free(sql_gtid_set_buffer);
      my_free(io_gtid_set_buffer);
      DBUG_RETURN(true);
    }
  }
  my_eof(thd);
  my_free(sql_gtid_set_buffer);
  my_free(io_gtid_set_buffer);
  DBUG_RETURN(false);
}


void set_slave_thread_options(THD* thd)
{
  DBUG_ENTER("set_slave_thread_options");
  /*
     It's nonsense to constrain the slave threads with max_join_size; if a
     query succeeded on master, we HAVE to execute it. So set
     OPTION_BIG_SELECTS. Setting max_join_size to HA_POS_ERROR is not enough
     (and it's not needed if we have OPTION_BIG_SELECTS) because an INSERT
     SELECT examining more than 4 billion rows would still fail (yes, because
     when max_join_size is 4G, OPTION_BIG_SELECTS is automatically set, but
     only for client threads.
  */
  ulonglong options= thd->variables.option_bits | OPTION_BIG_SELECTS;
  if (opt_log_slave_updates)
    options|= OPTION_BIN_LOG;
  else
    options&= ~OPTION_BIN_LOG;
  thd->variables.option_bits= options;
  thd->variables.completion_type= 0;
  DBUG_VOID_RETURN;
}

void set_slave_thread_default_charset(THD* thd, Relay_log_info const *rli)
{
  DBUG_ENTER("set_slave_thread_default_charset");

  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();

  /*
    We use a const cast here since the conceptual (and externally
    visible) behavior of the function is to set the default charset of
    the thread.  That the cache has to be invalidated is a secondary
    effect.
   */
  const_cast<Relay_log_info*>(rli)->cached_charset_invalidate();
  DBUG_VOID_RETURN;
}

/*
  init_slave_thread()
*/

static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type)
{
  DBUG_ENTER("init_slave_thread");
#if !defined(DBUG_OFF)
  int simulate_error= 0;
#endif
  thd->system_thread= (thd_type == SLAVE_THD_WORKER) ? 
    SYSTEM_THREAD_SLAVE_WORKER : (thd_type == SLAVE_THD_SQL) ?
    SYSTEM_THREAD_SLAVE_SQL : SYSTEM_THREAD_SLAVE_IO;
  thd->security_ctx->skip_grants();
  my_net_init(&thd->net, 0);
/*
  Adding MAX_LOG_EVENT_HEADER_LEN to the max_allowed_packet on all
  slave threads, since a replication event can become this much larger
  than the corresponding packet (query) sent from client to master.
*/
  thd->variables.max_allowed_packet= slave_max_allowed_packet;
  thd->slave_thread = 1;
  thd->enable_slow_log= opt_log_slow_slave_statements;
  set_slave_thread_options(thd);
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  mysql_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  mysql_mutex_unlock(&LOCK_thread_count);

  DBUG_EXECUTE_IF("simulate_io_slave_error_on_init",
                  simulate_error|= (1 << SLAVE_THD_IO););
  DBUG_EXECUTE_IF("simulate_sql_slave_error_on_init",
                  simulate_error|= (1 << SLAVE_THD_SQL););
#if !defined(DBUG_OFF)
  if (init_thr_lock() || thd->store_globals() || simulate_error & (1<< thd_type))
#else
  if (init_thr_lock() || thd->store_globals())
#endif
  {
    DBUG_RETURN(-1);
  }

  if (thd_type == SLAVE_THD_SQL)
  {
    THD_STAGE_INFO(thd, stage_waiting_for_the_next_event_in_relay_log);
  }
  else
  {
    THD_STAGE_INFO(thd, stage_waiting_for_master_update);
  }
  thd->set_time();
  /* Do not use user-supplied timeout value for system threads. */
  thd->variables.lock_wait_timeout= LONG_TIMEOUT;
  DBUG_RETURN(0);
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
static inline bool slave_sleep(THD *thd, time_t seconds,
                               killed_func func, rpl_info info)
{
  bool ret;
  struct timespec abstime;
  mysql_mutex_t *lock= &info->sleep_lock;
  mysql_cond_t *cond= &info->sleep_cond;

  /* Absolute system time at which the sleep time expires. */
  set_timespec(abstime, seconds);

  mysql_mutex_lock(lock);
  thd->ENTER_COND(cond, lock, NULL, NULL);

  while (! (ret= func(thd, info)))
  {
    int error= mysql_cond_timedwait(cond, lock, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
  }

  /* Implicitly unlocks the mutex. */
  thd->EXIT_COND(NULL);

  return ret;
}

static int request_dump(THD *thd, MYSQL* mysql, Master_info* mi,
                        bool *suppress_warnings)
{
  DBUG_ENTER("request_dump");

  const int BINLOG_NAME_INFO_SIZE= strlen(mi->get_master_log_name());
  int error= 1;
  size_t command_size= 0;
  enum_server_command command= mi->is_auto_position() ?
    COM_BINLOG_DUMP_GTID : COM_BINLOG_DUMP;
  uchar* command_buffer= NULL;
  ushort binlog_flags= 0;

  if (RUN_HOOK(binlog_relay_io,
               before_request_transmit,
               (thd, mi, binlog_flags)))
    goto err;

  *suppress_warnings= false;
  if (command == COM_BINLOG_DUMP_GTID)
  {
    // get set of GTIDs
    Sid_map sid_map(NULL/*no lock needed*/);
    Gtid_set gtid_executed(&sid_map);
    global_sid_lock->wrlock();
    gtid_state->dbug_print();
    if (gtid_executed.add_gtid_set(mi->rli->get_gtid_set()) != RETURN_STATUS_OK ||
        gtid_executed.add_gtid_set(gtid_state->get_logged_gtids()) !=
        RETURN_STATUS_OK)
    {
      global_sid_lock->unlock();
      goto err;
    }
    global_sid_lock->unlock();
     
    // allocate buffer
    size_t encoded_data_size= gtid_executed.get_encoded_length();
    size_t allocation_size= 
      ::BINLOG_FLAGS_INFO_SIZE + ::BINLOG_SERVER_ID_INFO_SIZE +
      ::BINLOG_NAME_SIZE_INFO_SIZE + BINLOG_NAME_INFO_SIZE +
      ::BINLOG_POS_INFO_SIZE + ::BINLOG_DATA_SIZE_INFO_SIZE +
      encoded_data_size + 1;
    if (!(command_buffer= (uchar *) my_malloc(allocation_size, MYF(MY_WME))))
      goto err;
    uchar* ptr_buffer= command_buffer;

    DBUG_PRINT("info", ("Do I know something about the master? (binary log's name %s - auto position %d).",
               mi->get_master_log_name(), mi->is_auto_position()));
    /*
      Note: binlog_flags is always 0.  However, in versions up to 5.6
      RC, the master would check the lowest bit and do something
      unexpected if it was set; in early versions of 5.6 it would also
      use the two next bits.  Therefore, for backward compatibility,
      if we ever start to use the flags, we should leave the three
      lowest bits unused.
    */
    int2store(ptr_buffer, binlog_flags);
    ptr_buffer+= ::BINLOG_FLAGS_INFO_SIZE;
    int4store(ptr_buffer, server_id);
    ptr_buffer+= ::BINLOG_SERVER_ID_INFO_SIZE;
    int4store(ptr_buffer, BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= ::BINLOG_NAME_SIZE_INFO_SIZE;
    memset(ptr_buffer, 0, BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= BINLOG_NAME_INFO_SIZE;
    int8store(ptr_buffer, 4);
    ptr_buffer+= ::BINLOG_POS_INFO_SIZE;

    int4store(ptr_buffer, encoded_data_size);
    ptr_buffer+= ::BINLOG_DATA_SIZE_INFO_SIZE;
    gtid_executed.encode(ptr_buffer);
    ptr_buffer+= encoded_data_size;

    command_size= ptr_buffer - command_buffer;
    DBUG_ASSERT(command_size == (allocation_size - 1));
  }
  else
  {
    size_t allocation_size= ::BINLOG_POS_OLD_INFO_SIZE +
      BINLOG_NAME_INFO_SIZE + ::BINLOG_FLAGS_INFO_SIZE +
      ::BINLOG_SERVER_ID_INFO_SIZE + 1;
    if (!(command_buffer= (uchar *) my_malloc(allocation_size, MYF(MY_WME))))
      goto err;
    uchar* ptr_buffer= command_buffer;
  
    int4store(ptr_buffer, mi->get_master_log_pos());
    ptr_buffer+= ::BINLOG_POS_OLD_INFO_SIZE;
    // See comment regarding binlog_flags above.
    int2store(ptr_buffer, binlog_flags);
    ptr_buffer+= ::BINLOG_FLAGS_INFO_SIZE;
    int4store(ptr_buffer, server_id);
    ptr_buffer+= ::BINLOG_SERVER_ID_INFO_SIZE;
    memcpy(ptr_buffer, mi->get_master_log_name(), BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= BINLOG_NAME_INFO_SIZE;

    command_size= ptr_buffer - command_buffer;
    DBUG_ASSERT(command_size == (allocation_size - 1));
  }

  if (simple_command(mysql, command, command_buffer, command_size, 1))
  {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
      *suppress_warnings= true;                 // Suppress reconnect warning
    else
      sql_print_error("Error on %s: %d  %s, will retry in %d secs",
                      command_name[command].str,
                      mysql_errno(mysql), mysql_error(mysql),
                      mi->connect_retry);
    goto err;
  }
  error= 0;

err:
  my_free(command_buffer);
  DBUG_RETURN(error);
}


/*
  Read one event from the master

  SYNOPSIS
    read_event()
    mysql               MySQL connection
    mi                  Master connection information
    suppress_warnings   TRUE when a normal net read timeout has caused us to
                        try a reconnect.  We do not want to print anything to
                        the error log in this case because this a anormal
                        event in an idle server.

    RETURN VALUES
    'packet_error'      Error
    number              Length of packet
*/

static ulong read_event(MYSQL* mysql, Master_info *mi, bool* suppress_warnings)
{
  ulong len;
  DBUG_ENTER("read_event");

  *suppress_warnings= FALSE;
  /*
    my_real_read() will time us out
    We check if we were told to die, and if not, try reading again
  */
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(mi->events_until_exit--))
    DBUG_RETURN(packet_error);
#endif

  len = cli_safe_read(mysql);
  if (len == packet_error || (long) len < 1)
  {
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      /*
        We are trying a normal reconnect after a read timeout;
        we suppress prints to .err file as long as the reconnect
        happens without problems
      */
      *suppress_warnings= TRUE;
    }
    else
      sql_print_error("Error reading packet from server: %s ( server_errno=%d)",
                      mysql_error(mysql), mysql_errno(mysql));
    DBUG_RETURN(packet_error);
  }

  /* Check if eof packet */
  if (len < 8 && mysql->net.read_pos[0] == 254)
  {
    sql_print_information("Slave: received end packet from server, apparent "
                          "master shutdown: %s",
                     mysql_error(mysql));
     DBUG_RETURN(packet_error);
  }

  DBUG_PRINT("exit", ("len: %lu  net->read_pos[4]: %d",
                      len, mysql->net.read_pos[4]));
  DBUG_RETURN(len - 1);
}


/**
  If this is a lagging slave (specified with CHANGE MASTER TO MASTER_DELAY = X), delays accordingly. Also unlocks rli->data_lock.

  Design note: this is the place to unlock rli->data_lock. The lock
  must be held when reading delay info from rli, but it should not be
  held while sleeping.

  @param ev Event that is about to be executed.

  @param thd The sql thread's THD object.

  @param rli The sql thread's Relay_log_info structure.

  @retval 0 If the delay timed out and the event shall be executed.

  @retval nonzero If the delay was interrupted and the event shall be skipped.
*/
static int sql_delay_event(Log_event *ev, THD *thd, Relay_log_info *rli)
{
  long sql_delay= rli->get_sql_delay();

  DBUG_ENTER("sql_delay_event");
  mysql_mutex_assert_owner(&rli->data_lock);
  DBUG_ASSERT(!rli->belongs_to_client());

  int type= ev->get_type_code();
  if (sql_delay && type != ROTATE_EVENT &&
      type != FORMAT_DESCRIPTION_EVENT && type != START_EVENT_V3)
  {
    // The time when we should execute the event.
    time_t sql_delay_end=
      ev->when.tv_sec + rli->mi->clock_diff_with_master + sql_delay;
    // The current time.
    time_t now= my_time(0);
    // The time we will have to sleep before executing the event.
    unsigned long nap_time= 0;
    if (sql_delay_end > now)
      nap_time= sql_delay_end - now;

    DBUG_PRINT("info", ("sql_delay= %lu "
                        "ev->when= %lu "
                        "rli->mi->clock_diff_with_master= %lu "
                        "now= %ld "
                        "sql_delay_end= %ld "
                        "nap_time= %ld",
                        sql_delay, (long) ev->when.tv_sec,
                        rli->mi->clock_diff_with_master,
                        (long)now, (long)sql_delay_end, (long)nap_time));

    if (sql_delay_end > now)
    {
      DBUG_PRINT("info", ("delaying replication event %lu secs",
                          nap_time));
      rli->start_sql_delay(sql_delay_end);
      mysql_mutex_unlock(&rli->data_lock);
      DBUG_RETURN(slave_sleep(thd, nap_time, sql_slave_killed, rli));
    }
  }

  mysql_mutex_unlock(&rli->data_lock);

  DBUG_RETURN(0);
}

/**
   a sort_dynamic function on ulong type
   returns as specified by @c qsort_cmp
*/
int ulong_cmp(ulong *id1, ulong *id2)
{
  return *id1 < *id2? -1 : (*id1 > *id2? 1 : 0);
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

  It also does the following maintainance:

   - Initializes the thread's server_id and time; and the event's
     thread.

   - If !rli->belongs_to_client() (i.e., if it belongs to the slave
     sql thread instead of being used for executing BINLOG
     statements), it does the following things: (1) skips events if it
     is needed according to the server id or slave_skip_counter; (2)
     unlocks rli->data_lock; (3) sleeps if required by 'CHANGE MASTER
     TO MASTER_DELAY=X'; (4) maintains the running state of the sql
     thread (rli->thread_state).

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
enum enum_slave_apply_event_and_update_pos_retval
apply_event_and_update_pos(Log_event** ptr_ev, THD* thd, Relay_log_info* rli)
{
  int exec_res= 0;
  bool skip_event= FALSE;
  Log_event *ev= *ptr_ev;
  Log_event::enum_skip_reason reason= Log_event::EVENT_SKIP_NOT;

  DBUG_ENTER("apply_event_and_update_pos");

  DBUG_PRINT("exec_event",("%s(type_code: %d; server_id: %d)",
                           ev->get_type_str(), ev->get_type_code(),
                           ev->server_id));
  DBUG_PRINT("info", ("thd->options: %s%s; rli->last_event_start_time: %lu",
                      FLAGSTR(thd->variables.option_bits, OPTION_NOT_AUTOCOMMIT),
                      FLAGSTR(thd->variables.option_bits, OPTION_BEGIN),
                      (ulong) rli->last_event_start_time));

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
  thd->server_id = ev->server_id; // use the original server id for logging
  thd->unmasked_server_id = ev->unmasked_server_id;
  thd->set_time();                            // time the query
  thd->lex->current_select= 0;
  if (!ev->when.tv_sec)
    my_micro_time_to_timeval(my_micro_time(), &ev->when);
  ev->thd = thd; // because up to this point, ev->thd == 0

  if (!(rli->is_mts_recovery() && bitmap_is_set(&rli->recovery_groups,
                                                rli->mts_recovery_index)))
  {
    reason= ev->shall_skip(rli);
  }
#ifndef DBUG_OFF
  if (rli->is_mts_recovery())
  {
    DBUG_PRINT("mts", ("Mts is recovering %d, number of bits set %d, "
                       "bitmap is set %d, index %lu.\n",
                       rli->is_mts_recovery(),
                       bitmap_bits_set(&rli->recovery_groups),
                       bitmap_is_set(&rli->recovery_groups,
                                     rli->mts_recovery_index),
                       rli->mts_recovery_index));
  }
#endif
  if (reason == Log_event::EVENT_SKIP_COUNT)
  {
    sql_slave_skip_counter= --rli->slave_skip_counter;
    skip_event= TRUE;
  }
  if (reason == Log_event::EVENT_SKIP_NOT)
  {
    // Sleeps if needed, and unlocks rli->data_lock.
    if (sql_delay_event(ev, thd, rli))
      DBUG_RETURN(SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK);

    exec_res= ev->apply_event(rli);

    if (!exec_res && (ev->worker != rli))
    {
      if (ev->worker)
      {
        Slave_job_item item= {ev}, *job_item= &item;
        Slave_worker *w= (Slave_worker *) ev->worker;
        // specially marked group typically with OVER_MAX_DBS_IN_EVENT_MTS db:s
        bool need_sync= ev->is_mts_group_isolated();

        // all events except BEGIN-query must be marked with a non-NULL Worker
        DBUG_ASSERT(((Slave_worker*) ev->worker) == rli->last_assigned_worker);

        DBUG_PRINT("Log_event::apply_event:",
                   ("-> job item data %p to W_%lu", job_item->data, w->id));

        // Reset mts in-group state
        if (rli->mts_group_status == Relay_log_info::MTS_END_GROUP)
        {
          // CGAP cleanup
          for (uint i= rli->curr_group_assigned_parts.elements; i > 0; i--)
            delete_dynamic_element(&rli->
                                   curr_group_assigned_parts, i - 1);
          // reset the B-group and Gtid-group marker
          rli->curr_group_seen_begin= rli->curr_group_seen_gtid= false;
          rli->last_assigned_worker= NULL;
        }

        bool append_item_to_jobs_error= false;
        if (rli->curr_group_da.elements > 0)
        {
          /*
            the current event sorted out which partion the current group belongs to.
            It's time now to processed deferred array events.
          */
          for (uint i= 0; i < rli->curr_group_da.elements; i++)
          { 
            Slave_job_item da_item;
            get_dynamic(&rli->curr_group_da, (uchar*) &da_item.data, i);
            DBUG_PRINT("mts", ("Assigning job %llu to worker %lu",
                      ((Log_event* )da_item.data)->log_pos, w->id));
            if (!append_item_to_jobs_error)
              append_item_to_jobs_error= append_item_to_jobs(&da_item, w, rli);
            if (append_item_to_jobs_error)
              delete static_cast<Log_event*>(da_item.data);
          }
          if (rli->curr_group_da.elements > rli->curr_group_da.max_element)
          {
            // reallocate to less mem
            rli->curr_group_da.elements= rli->curr_group_da.max_element;
            rli->curr_group_da.max_element= 0;
            freeze_size(&rli->curr_group_da); // restores max_element
          }
          rli->curr_group_da.elements= 0;
        }
        if (append_item_to_jobs_error)
          DBUG_RETURN(SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR);

        DBUG_PRINT("mts", ("Assigning job %llu to worker %lu\n",
                   ((Log_event* )job_item->data)->log_pos, w->id));

        /* Notice `ev' instance can be destoyed after `append()' */
        if (append_item_to_jobs(job_item, w, rli))
          DBUG_RETURN(SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPEND_JOB_ERROR);
        if (need_sync)
        {
          /*
            combination of over-max db:s and end of the current group
            forces to wait for the assigned groups completion by assigned
            to the event worker.
            Indeed MTS group status could be safely set to MTS_NOT_IN_GROUP
            after wait_() returns.
            No need to know a possible error out of synchronization call.
          */
          (void) wait_for_workers_to_finish(rli);
        }

      }
      *ptr_ev= NULL; // announcing the event is passed to w-worker

      if (log_warnings > 1 &&
          rli->is_parallel_exec() && rli->mts_events_assigned % 1024 == 1)
      {
        time_t my_now= my_time(0);

        if ((my_now - rli->mts_last_online_stat) >=
            mts_online_stat_period)
        {
          sql_print_information("Multi-threaded slave statistics: "
                                "seconds elapsed = %lu; "
                                "events assigned = %llu; "
                                "worker queues filled over overrun level = %lu; "
                                "waited due a Worker queue full = %lu; "
                                "waited due the total size = %lu; "
                                "slept when Workers occupied = %lu ",
                                static_cast<unsigned long>
                                (my_now - rli->mts_last_online_stat),
                                rli->mts_events_assigned,
                                rli->mts_wq_overrun_cnt,
                                rli->mts_wq_overfill_cnt,
                                rli->wq_size_waits_cnt,
                                rli->mts_wq_no_underrun_cnt);
          rli->mts_last_online_stat= my_now;
        }
      }
    }
  }
  else
    mysql_mutex_unlock(&rli->data_lock);
 
  DBUG_PRINT("info", ("apply_event error = %d", exec_res));
  if (exec_res == 0)
  {
    /*
      Positions are not updated here when an XID is processed. To make
      a slave crash-safe, positions must be updated while processing a
      XID event and as such do not need to be updated here again.

      However, if the event needs to be skipped, this means that it
      will not be processed and then positions need to be updated here.

      See sql/rpl_rli.h for further details.
    */
    int error= 0;
    if (*ptr_ev &&
        (ev->get_type_code() != XID_EVENT ||
         skip_event || (rli->is_mts_recovery() && !is_gtid_event(ev) &&
         (ev->ends_group() || !rli->mts_recovery_group_seen_begin) &&
          bitmap_is_set(&rli->recovery_groups, rli->mts_recovery_index))))
    {
#ifndef DBUG_OFF
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
        "skipped because event skip counter was non-zero"
      };
      DBUG_PRINT("info", ("OPTION_BEGIN: %d; IN_STMT: %d",
                          test(thd->variables.option_bits & OPTION_BEGIN),
                          rli->get_flag(Relay_log_info::IN_STMT)));
      DBUG_PRINT("skip_event", ("%s event was %s",
                                ev->get_type_str(), explain[reason]));
#endif

      error= ev->update_pos(rli);

#ifndef DBUG_OFF
      DBUG_PRINT("info", ("update_pos error = %d", error));
      if (!rli->belongs_to_client())
      {
        char buf[22];
        DBUG_PRINT("info", ("group %s %s",
                            llstr(rli->get_group_relay_log_pos(), buf),
                            rli->get_group_relay_log_name()));
        DBUG_PRINT("info", ("event %s %s",
                            llstr(rli->get_event_relay_log_pos(), buf),
                            rli->get_event_relay_log_name()));
      }
#endif
    }
    else
    {
      DBUG_ASSERT(*ptr_ev == ev || rli->is_parallel_exec() ||
		  (!ev->worker &&
		   (ev->get_type_code() == INTVAR_EVENT ||
		    ev->get_type_code() == RAND_EVENT ||
		    ev->get_type_code() == USER_VAR_EVENT)));

      rli->inc_event_relay_log_pos();
    }

    if (!error && rli->is_mts_recovery() &&
        ev->get_type_code() != ROTATE_EVENT &&
        ev->get_type_code() != FORMAT_DESCRIPTION_EVENT &&
        ev->get_type_code() != PREVIOUS_GTIDS_LOG_EVENT)
    {
      if (ev->starts_group())
      {
        rli->mts_recovery_group_seen_begin= true;
      }
      else if ((ev->ends_group() || !rli->mts_recovery_group_seen_begin) &&
               !is_gtid_event(ev))
      {
        rli->mts_recovery_index++;
        if (--rli->mts_recovery_group_cnt == 0)
        {
          rli->mts_recovery_index= 0;
          sql_print_information("Slave: MTS Recovery has completed at "
                                "relay log %s, position %llu "
                                "master log %s, position %llu.",
                                rli->get_group_relay_log_name(),
                                rli->get_group_relay_log_pos(),
                                rli->get_group_master_log_name(),
                                rli->get_group_master_log_pos());
#ifndef DBUG_OFF
          /* 
             Few tests wait for UNTIL_SQL_AFTER_MTS_GAPS completion.
             Due to exisiting convention the status won't change 
             prior to slave restarts.
             So making of UNTIL_SQL_AFTER_MTS_GAPS completion isdone here,
             and only in the debug build to make the test to catch the change
             despite a faulty design of UNTIL checking before execution.
          */
          if (rli->until_condition == Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS)
          {
            rli->until_condition= Relay_log_info::UNTIL_DONE;
          }
#endif
          // reset the Worker tables to remove last slave session time info
          if ((error= rli->mts_finalize_recovery()))
          {
            (void) Rpl_info_factory::reset_workers(rli);
          }
        }
        rli->mts_recovery_group_seen_begin= false;
        if (!error)
          error= rli->flush_info(true);
      }
    }

    if (error)
    {
      /*
        The update should not fail, so print an error message and
        return an error code.
        
        TODO: Replace this with a decent error message when merged
        with BUG#24954 (which adds several new error message).
      */
      char buf[22];
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "It was not possible to update the positions"
                  " of the relay log information: the slave may"
                  " be in an inconsistent state."
                  " Stopped in %s position %s",
                  rli->get_group_relay_log_name(),
                  llstr(rli->get_group_relay_log_pos(), buf));
      DBUG_RETURN(SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR);
    }
  }

  DBUG_RETURN(exec_res ? SLAVE_APPLY_EVENT_AND_UPDATE_POS_APPLY_ERROR
                       : SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK);
}


/**
  Top-level function for executing the next event in the relay log.
  This is called from the SQL thread.

  This function reads the event from the relay log, executes it, and
  advances the relay log position.  It also handles errors, etc.

  This function may fail to apply the event for the following reasons:

   - The position specfied by the UNTIL condition of the START SLAVE
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
static int exec_relay_log_event(THD* thd, Relay_log_info* rli)
{
  DBUG_ENTER("exec_relay_log_event");

  /*
     We acquire this mutex since we need it for all operations except
     event execution. But we will release it in places where we will
     wait for something for example inside of next_event().
   */
  mysql_mutex_lock(&rli->data_lock);

  Log_event *ev = next_event(rli), **ptr_ev;

  DBUG_ASSERT(rli->info_thd==thd);

  if (sql_slave_killed(thd,rli))
  {
    mysql_mutex_unlock(&rli->data_lock);
    delete ev;
    DBUG_RETURN(1);
  }
  if (ev)
  {
    enum enum_slave_apply_event_and_update_pos_retval exec_res;

    ptr_ev= &ev;
    /*
      Even if we don't execute this event, we keep the master timestamp,
      so that seconds behind master shows correct delta (there are events
      that are not replayed, so we keep falling behind).

      If it is an artificial event, or a relay log event (IO thread generated
      event) or ev->when is set to 0, or a FD from master, we don't update the
      last_master_timestamp.
    */
    if (!(rli->is_parallel_exec() ||
          ev->is_artificial_event() || ev->is_relay_log_event() ||
          (ev->when.tv_sec == 0) || ev->get_type_code() == FORMAT_DESCRIPTION_EVENT))
    {
      rli->last_master_timestamp= ev->when.tv_sec + (time_t) ev->exec_time;
      DBUG_ASSERT(rli->last_master_timestamp >= 0);
    }

    /*
      This tests if the position of the beginning of the current event
      hits the UNTIL barrier.
      MTS: since the master and the relay-group coordinates change 
      asynchronously logics of rli->is_until_satisfied() can't apply.
      A special UNTIL_SQL_AFTER_MTS_GAPS is still deployed here
      temporarily (see is_until_satisfied todo).
    */
    if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
        rli->is_until_satisfied(thd, ev))
    {
      /*
        Setting abort_slave flag because we do not want additional message about
        error in query execution to be printed.
      */
      rli->abort_slave= 1;
      mysql_mutex_unlock(&rli->data_lock);
      delete ev;
      DBUG_RETURN(1);
    }

    { /**
         The following failure injecion works in cooperation with tests 
         setting @@global.debug= 'd,incomplete_group_in_relay_log'.
         Xid or Commit events are not executed to force the slave sql
         read hanging if the realy log does not have any more events.
      */
      DBUG_EXECUTE_IF("incomplete_group_in_relay_log",
                      if ((ev->get_type_code() == XID_EVENT) ||
                          ((ev->get_type_code() == QUERY_EVENT) &&
                           strcmp("COMMIT", ((Query_log_event *) ev)->query) == 0))
                      {
                        DBUG_ASSERT(thd->transaction.all.cannot_safely_rollback());
                        rli->abort_slave= 1;
                        mysql_mutex_unlock(&rli->data_lock);
                        delete ev;
                        rli->inc_event_relay_log_pos();
                        DBUG_RETURN(0);
                      };);
    }

    /* ptr_ev can change to NULL indicating MTS coorinator passed to a Worker */
    exec_res= apply_event_and_update_pos(ptr_ev, thd, rli);
    /*
      Note: the above call to apply_event_and_update_pos executes
      mysql_mutex_unlock(&rli->data_lock);
    */

    if (*ptr_ev)
    {
      DBUG_ASSERT(*ptr_ev == ev); // event remains to belong to Coordinator

      /*
        Format_description_log_event should not be deleted because it will be
        used to read info about the relay log's format; it will be deleted when
        the SQL thread does not need it, i.e. when this thread terminates.
        ROWS_QUERY_LOG_EVENT is destroyed at the end of the current statement
        clean-up routine.
      */
      if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT &&
          ev->get_type_code() != ROWS_QUERY_LOG_EVENT)
      {
        DBUG_PRINT("info", ("Deleting the event after it has been executed"));
        delete ev;
        ev= NULL;
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
    if (exec_res >= SLAVE_APPLY_EVENT_AND_UPDATE_POS_UPDATE_POS_ERROR)
    {
      delete ev;
      DBUG_RETURN(1);
    }

    if (slave_trans_retries)
    {
      int UNINIT_VAR(temp_err);
      bool silent= false;
      if (exec_res && !is_mts_worker(thd) /* no reexecution in MTS mode */ &&
          (temp_err= rli->has_temporary_error(thd, 0, &silent)) &&
          !thd->transaction.all.cannot_safely_rollback())
      {
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
          possibly existing old logs where BEGIN is: check_binlog_magic() will
          then need the cache to be at position 0 (see comments at beginning of
          init_info()).
          b) init_relay_log_pos(), because the BEGIN may be an older relay log.
        */
        if (rli->trans_retries < slave_trans_retries)
        {
          /*
             We need to figure out if there is a test case that covers
             this part. \Alfranio.
          */
          if (global_init_info(rli->mi, false, SLAVE_SQL))
            sql_print_error("Failed to initialize the master info structure");
          else if (rli->init_relay_log_pos(rli->get_group_relay_log_name(),
                                           rli->get_group_relay_log_pos(),
                                           true/*need_data_lock=true*/,
                                           &errmsg, 1))
            sql_print_error("Error initializing relay log position: %s",
                            errmsg);
          else
          {
            exec_res= SLAVE_APPLY_EVENT_AND_UPDATE_POS_OK;
            rli->cleanup_context(thd, 1);
            /* chance for concurrent connection to get more locks */
            slave_sleep(thd, min<ulong>(rli->trans_retries, MAX_SLAVE_RETRY_PAUSE),
                        sql_slave_killed, rli);
            mysql_mutex_lock(&rli->data_lock); // because of SHOW STATUS
            if (!silent)
              rli->trans_retries++;
            
            rli->retried_trans++;
            mysql_mutex_unlock(&rli->data_lock);
            DBUG_PRINT("info", ("Slave retries transaction "
                                "rli->trans_retries: %lu", rli->trans_retries));
          }
        }
        else
        {
          thd->is_fatal_error= 1;
          rli->report(ERROR_LEVEL, thd->get_stmt_da()->sql_errno(),
                      "Slave SQL thread retried transaction %lu time(s) "
                      "in vain, giving up. Consider raising the value of "
                      "the slave_transaction_retries variable.", rli->trans_retries);
        }
      }
      else if ((exec_res && !temp_err) ||
               (opt_using_transactions &&
                rli->get_group_relay_log_pos() == rli->get_event_relay_log_pos()))
      {
        /*
          Only reset the retry counter if the entire group succeeded
          or failed with a non-transient error.  On a successful
          event, the execution will proceed as usual; in the case of a
          non-transient error, the slave will stop with an error.
         */
        rli->trans_retries= 0; // restart from fresh
        DBUG_PRINT("info", ("Resetting retry counter, rli->trans_retries: %lu",
                            rli->trans_retries));
      }
    }
    if (exec_res)
      delete ev;
    DBUG_RETURN(exec_res);
  }
  mysql_mutex_unlock(&rli->data_lock);
  rli->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_READ_FAILURE,
              ER(ER_SLAVE_RELAY_LOG_READ_FAILURE), "\
Could not parse relay log event entry. The possible reasons are: the master's \
binary log is corrupted (you can check this by running 'mysqlbinlog' on the \
binary log), the slave's relay log is corrupted (you can check this by running \
'mysqlbinlog' on the relay log), a network problem, or a bug in the master's \
or slave's MySQL code. If you want to check the master's binary log or slave's \
relay log, you will be able to know their names by issuing 'SHOW SLAVE STATUS' \
on this slave.\
");
  DBUG_RETURN(1);
}

static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info)
{
  if (io_slave_killed(thd, mi))
  {
    if (info && log_warnings)
      sql_print_information("%s", info);
    return TRUE;
  }
  return FALSE;
}

/**
  @brief Try to reconnect slave IO thread.

  @details Terminates current connection to master, sleeps for
  @c mi->connect_retry msecs and initiates new connection with
  @c safe_reconnect(). Variable pointed by @c retry_count is increased -
  if it exceeds @c mi->retry_count then connection is not re-established
  and function signals error.
  Unless @c suppres_warnings is TRUE, a warning is put in the server error log
  when reconnecting. The warning message and messages used to report errors
  are taken from @c messages array. In case @c mi->retry_count is exceeded,
  no messages are added to the log.

  @param[in]     thd                 Thread context.
  @param[in]     mysql               MySQL connection.
  @param[in]     mi                  Master connection information.
  @param[in,out] retry_count         Number of attempts to reconnect.
  @param[in]     suppress_warnings   TRUE when a normal net read timeout 
                                     has caused to reconnecting.
  @param[in]     messages            Messages to print/log, see 
                                     reconnect_messages[] array.

  @retval        0                   OK.
  @retval        1                   There was an error.
*/

static int try_to_reconnect(THD *thd, MYSQL *mysql, Master_info *mi,
                            uint *retry_count, bool suppress_warnings,
                            const char *messages[SLAVE_RECON_MSG_MAX])
{
  mi->slave_running= MYSQL_SLAVE_RUN_NOT_CONNECT;
  thd->proc_info= messages[SLAVE_RECON_MSG_WAIT];
#ifdef SIGNAL_WITH_VIO_CLOSE  
  thd->clear_active_vio();
#endif
  end_server(mysql);
  if ((*retry_count)++)
  {
    if (*retry_count > mi->retry_count)
      return 1;                             // Don't retry forever
    slave_sleep(thd, mi->connect_retry, io_slave_killed, mi);
  }
  if (check_io_slave_killed(thd, mi, messages[SLAVE_RECON_MSG_KILLED_WAITING]))
    return 1;
  thd->proc_info = messages[SLAVE_RECON_MSG_AFTER];
  if (!suppress_warnings) 
  {
    char buf[256], llbuff[22];
    my_snprintf(buf, sizeof(buf), messages[SLAVE_RECON_MSG_FAILED], 
                mi->get_io_rpl_log_name(), llstr(mi->get_master_log_pos(),
                llbuff));
    /* 
      Raise a warining during registering on master/requesting dump.
      Log a message reading event.
    */
    if (messages[SLAVE_RECON_MSG_COMMAND][0])
    {
      mi->report(WARNING_LEVEL, ER_SLAVE_MASTER_COM_FAILURE,
                 ER(ER_SLAVE_MASTER_COM_FAILURE), 
                 messages[SLAVE_RECON_MSG_COMMAND], buf);
    }
    else
    {
      sql_print_information("%s", buf);
    }
  }
  if (safe_reconnect(thd, mysql, mi, 1) || io_slave_killed(thd, mi))
  {
    if (log_warnings)
      sql_print_information("%s", messages[SLAVE_RECON_MSG_KILLED_AFTER]);
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
pthread_handler_t handle_slave_io(void *arg)
{
  THD *thd= NULL; // needs to be first for thread_stack
  bool thd_added= false;
  MYSQL *mysql;
  Master_info *mi = (Master_info*)arg;
  Relay_log_info *rli= mi->rli;
  char llbuff[22];
  uint retry_count;
  bool suppress_warnings;
  int ret;
  int binlog_version;
#ifndef DBUG_OFF
  uint retry_count_reg= 0, retry_count_dump= 0, retry_count_event= 0;
#endif
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_io");

  DBUG_ASSERT(mi->inited);
  mysql= NULL ;
  retry_count= 0;

  mysql_mutex_lock(&mi->run_lock);
  /* Inform waiting threads that slave has started */
  mi->slave_run_id++;

#ifndef DBUG_OFF
  mi->events_until_exit = disconnect_slave_event_count;
#endif

  thd= new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);
  mi->info_thd = thd;

  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd; // remember where our stack is
  mi->clear_error();
  if (init_slave_thread(thd, SLAVE_THD_IO))
  {
    mysql_cond_broadcast(&mi->start_cond);
    mysql_mutex_unlock(&mi->run_lock);
    sql_print_error("Failed during slave I/O thread initialization");
    goto err;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  add_global_thread(thd);
  thd_added= true;
  mysql_mutex_unlock(&LOCK_thread_count);

  mi->slave_running = 1;
  mi->abort_slave = 0;
  mysql_mutex_unlock(&mi->run_lock);
  mysql_cond_broadcast(&mi->start_cond);

  DBUG_PRINT("master_info",("log_file_name: '%s'  position: %s",
                            mi->get_master_log_name(),
                            llstr(mi->get_master_log_pos(), llbuff)));

  /* This must be called before run any binlog_relay_io hooks */
  my_pthread_setspecific_ptr(RPL_MASTER_INFO, mi);

  if (RUN_HOOK(binlog_relay_io, thread_start, (thd, mi)))
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR), "Failed to run 'thread_start' hook");
    goto err;
  }

  if (!(mi->mysql = mysql = mysql_init(NULL)))
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR), "error in mysql_init()");
    goto err;
  }

  THD_STAGE_INFO(thd, stage_connecting_to_master);
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
  {
    sql_print_information("Slave I/O thread: connected to master '%s@%s:%d',"
                          "replication started in log '%s' at position %s",
                          mi->get_user(), mi->host, mi->port,
			  mi->get_io_rpl_log_name(),
			  llstr(mi->get_master_log_pos(), llbuff));
  /*
    Assign the max_packet_size with the bigger one of the values of
    'max_packet_size' and 'opt_binlog_rows_event_max_size'. So that
    slave I/O thread can replicate large row event from master.
    Adding MAX_LOG_EVENT_HEADER_LEN to the max_packet_size on the I/O
    thread, since a replication event can become this much larger than
    the corresponding packet (query) sent from client to master.
  */
    thd->net.max_packet_size= slave_max_allowed_packet;
    mysql->net.max_packet_size= thd->net.max_packet_size+= MAX_LOG_EVENT_HEADER;
  }
  else
  {
    sql_print_information("Slave I/O thread killed while connecting to master");
    goto err;
  }

connected:

    DBUG_EXECUTE_IF("dbug.before_get_running_status_yes",
                    {
                      const char act[]=
                        "now "
                        "wait_for signal.io_thread_let_running";
                      DBUG_ASSERT(opt_debug_sync_timeout > 0);
                      DBUG_ASSERT(!debug_sync_set_action(thd, 
                                                         STRING_WITH_LEN(act)));
                    };);
  mysql_mutex_lock(&mi->run_lock);
  mi->slave_running= MYSQL_SLAVE_RUN_CONNECT;
  mysql_mutex_unlock(&mi->run_lock);

  thd->slave_net = &mysql->net;
  THD_STAGE_INFO(thd, stage_checking_master_version);
  ret= get_master_version_and_clock(mysql, mi);
  if (!ret)
    ret= get_master_uuid(mysql, mi);
  if (!ret)
    ret= io_thread_init_commands(mysql, mi);

  if (ret == 1)
    /* Fatal error */
    goto err;

  if (ret == 2) 
  { 
    if (check_io_slave_killed(mi->info_thd, mi, "Slave I/O thread killed"
                              "while calling get_master_version_and_clock(...)"))
      goto err;
    suppress_warnings= FALSE;
    /* Try to reconnect because the error was caused by a transient network problem */
    if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
      goto err;
    goto connected;
  } 

  mysql_mutex_lock(&mi->data_lock);
  binlog_version= mi->get_mi_description_event()->binlog_version;
  mysql_mutex_unlock(&mi->data_lock);

  if (binlog_version > 1)
  {
    /*
      Register ourselves with the master.
    */
    THD_STAGE_INFO(thd, stage_registering_slave_on_master);
    if (register_slave_on_master(mysql, mi, &suppress_warnings))
    {
      if (!check_io_slave_killed(thd, mi, "Slave I/O thread killed "
                                "while registering slave on master"))
      {
        sql_print_error("Slave I/O thread couldn't register on master");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
          goto err;
      }
      else
        goto err;
      goto connected;
    }
    DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_REG", 
      if (!retry_count_reg)
      {
        retry_count_reg++;
        sql_print_information("Forcing to reconnect slave I/O thread");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
          goto err;
        goto connected;
      });
  }

  DBUG_PRINT("info",("Starting reading binary log from master"));
  while (!io_slave_killed(thd,mi))
  {
    THD_STAGE_INFO(thd, stage_requesting_binlog_dump);
    if (request_dump(thd, mysql, mi, &suppress_warnings))
    {
      sql_print_error("Failed on request_dump()");
      if (check_io_slave_killed(thd, mi, "Slave I/O thread killed while \
requesting master dump") ||
          try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                           reconnect_messages[SLAVE_RECON_ACT_DUMP]))
        goto err;
      goto connected;
    }
    DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_DUMP", 
      if (!retry_count_dump)
      {
        retry_count_dump++;
        sql_print_information("Forcing to reconnect slave I/O thread");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_DUMP]))
          goto err;
        goto connected;
      });
    const char *event_buf;

    DBUG_ASSERT(mi->last_error().number == 0);
    while (!io_slave_killed(thd,mi))
    {
      ulong event_len;
      /*
         We say "waiting" because read_event() will wait if there's nothing to
         read. But if there's something to read, it will not wait. The
         important thing is to not confuse users by saying "reading" whereas
         we're in fact receiving nothing.
      */
      THD_STAGE_INFO(thd, stage_waiting_for_master_to_send_event);
      event_len= read_event(mysql, mi, &suppress_warnings);
      if (check_io_slave_killed(thd, mi, "Slave I/O thread killed while \
reading event"))
        goto err;
      DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_EVENT",
        if (!retry_count_event)
        {
          retry_count_event++;
          sql_print_information("Forcing to reconnect slave I/O thread");
          if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                               reconnect_messages[SLAVE_RECON_ACT_EVENT]))
            goto err;
          goto connected;
        });

      if (event_len == packet_error)
      {
        uint mysql_error_number= mysql_errno(mysql);
        switch (mysql_error_number) {
        case CR_NET_PACKET_TOO_LARGE:
          sql_print_error("\
Log entry on master is longer than slave_max_allowed_packet (%lu) on \
slave. If the entry is correct, restart the server with a higher value of \
slave_max_allowed_packet",
                         slave_max_allowed_packet);
          mi->report(ERROR_LEVEL, ER_NET_PACKET_TOO_LARGE,
                     "%s", "Got a packet bigger than 'slave_max_allowed_packet' bytes");
          goto err;
        case ER_MASTER_FATAL_ERROR_READING_BINLOG:
          mi->report(ERROR_LEVEL, ER_MASTER_FATAL_ERROR_READING_BINLOG,
                     ER(ER_MASTER_FATAL_ERROR_READING_BINLOG),
                     mysql_error_number, mysql_error(mysql));
          goto err;
        case ER_OUT_OF_RESOURCES:
          sql_print_error("\
Stopping slave I/O thread due to out-of-memory error from master");
          mi->report(ERROR_LEVEL, ER_OUT_OF_RESOURCES,
                     "%s", ER(ER_OUT_OF_RESOURCES));
          goto err;
        }
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_EVENT]))
          goto err;
        goto connected;
      } // if (event_len == packet_error)

      retry_count=0;                    // ok event, reset retry counter
      THD_STAGE_INFO(thd, stage_queueing_master_event_to_the_relay_log);
      event_buf= (const char*)mysql->net.read_pos + 1;
      DBUG_PRINT("info", ("IO thread received event of type %s", Log_event::get_type_str((Log_event_type)event_buf[EVENT_TYPE_OFFSET])));
      if (RUN_HOOK(binlog_relay_io, after_read_event,
                   (thd, mi,(const char*)mysql->net.read_pos + 1,
                    event_len, &event_buf, &event_len)))
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                   ER(ER_SLAVE_FATAL_ERROR),
                   "Failed to run 'after_read_event' hook");
        goto err;
      }

      /* XXX: 'synced' should be updated by queue_event to indicate
         whether event has been synced to disk */
      bool synced= 0;
      if (queue_event(mi, event_buf, event_len))
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                   ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                   "could not queue event from master");
        goto err;
      }

      if (RUN_HOOK(binlog_relay_io, after_queue_event,
                   (thd, mi, event_buf, event_len, synced)))
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                   ER(ER_SLAVE_FATAL_ERROR),
                   "Failed to run 'after_queue_event' hook");
        goto err;
      }

      mysql_mutex_lock(&mi->data_lock);
      if (flush_master_info(mi, FALSE))
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                   ER(ER_SLAVE_FATAL_ERROR),
                   "Failed to flush master info.");
        mysql_mutex_unlock(&mi->data_lock);
        goto err;
      }
      mysql_mutex_unlock(&mi->data_lock);

      /*
        See if the relay logs take too much space.
        We don't lock mi->rli->log_space_lock here; this dirty read saves time
        and does not introduce any problem:
        - if mi->rli->ignore_log_space_limit is 1 but becomes 0 just after (so
        the clean value is 0), then we are reading only one more event as we
        should, and we'll block only at the next event. No big deal.
        - if mi->rli->ignore_log_space_limit is 0 but becomes 1 just after (so
        the clean value is 1), then we are going into wait_for_relay_log_space()
        for no reason, but this function will do a clean read, notice the clean
        value and exit immediately.
      */
#ifndef DBUG_OFF
      {
        char llbuf1[22], llbuf2[22];
        DBUG_PRINT("info", ("log_space_limit=%s log_space_total=%s \
ignore_log_space_limit=%d",
                            llstr(rli->log_space_limit,llbuf1),
                            llstr(rli->log_space_total,llbuf2),
                            (int) rli->ignore_log_space_limit));
      }
#endif

      if (rli->log_space_limit && rli->log_space_limit <
          rli->log_space_total &&
          !rli->ignore_log_space_limit)
        if (wait_for_relay_log_space(rli))
        {
          sql_print_error("Slave I/O thread aborted while waiting for relay \
log space");
          goto err;
        }
    }
  }

  // error = 0;
err:
  // print the current replication position
  sql_print_information("Slave I/O thread exiting, read up to log '%s', position %s",
                  mi->get_io_rpl_log_name(), llstr(mi->get_master_log_pos(), llbuff));
  (void) RUN_HOOK(binlog_relay_io, thread_stop, (thd, mi));
  thd->reset_query();
  thd->reset_db(NULL, 0);
  if (mysql)
  {
    /*
      Here we need to clear the active VIO before closing the
      connection with the master.  The reason is that THD::awake()
      might be called from terminate_slave_thread() because somebody
      issued a STOP SLAVE.  If that happends, the close_active_vio()
      can be called in the middle of closing the VIO associated with
      the 'mysql' object, causing a crash.
    */
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->clear_active_vio();
#endif
    mysql_close(mysql);
    mi->mysql=0;
  }
  mysql_mutex_lock(&mi->data_lock);
  write_ignored_events_info_to_relay_log(thd, mi);
  mysql_mutex_unlock(&mi->data_lock);
  THD_STAGE_INFO(thd, stage_waiting_for_slave_mutex_on_exit);
  mysql_mutex_lock(&mi->run_lock);
  /*
    Clean information used to start slave in order to avoid
    security issues.
  */
  mi->reset_start_info();
  /* Forget the relay log's format */
  mysql_mutex_lock(&mi->data_lock);
  mi->set_mi_description_event(NULL);
  mysql_mutex_unlock(&mi->data_lock);

  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0

  thd->release_resources();
  mysql_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  if (thd_added)
    remove_global_thread(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  delete thd;

  mi->abort_slave= 0;
  mi->slave_running= 0;
  mi->info_thd= 0;
  /*
    Note: the order of the two following calls (first broadcast, then unlock)
    is important. Otherwise a killer_thread can execute between the calls and
    delete the mi structure leading to a crash! (see BUG#25306 for details)
   */ 
  mysql_cond_broadcast(&mi->stop_cond);       // tell the world we are done
  DBUG_EXECUTE_IF("simulate_slave_delay_at_terminate_bug38694", sleep(5););
  mysql_mutex_unlock(&mi->run_lock);
  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  ERR_remove_state(0);
  pthread_exit(0);
  return(0);                                    // Avoid compiler warnings
}

/*
  Check the temporary directory used by commands like
  LOAD DATA INFILE.
 */
static 
int check_temp_dir(char* tmp_file)
{
  int fd;
  MY_DIR *dirp;
  char tmp_dir[FN_REFLEN];
  size_t tmp_dir_size;

  DBUG_ENTER("check_temp_dir");

  /*
    Get the directory from the temporary file.
  */
  dirname_part(tmp_dir, tmp_file, &tmp_dir_size);

  /*
    Check if the directory exists.
   */
  if (!(dirp=my_dir(tmp_dir,MYF(MY_WME))))
    DBUG_RETURN(1);
  my_dirend(dirp);

  /*
    Check permissions to create a file.
   */
  //append the server UUID to the temp file name.
  char *unique_tmp_file_name= (char*)my_malloc((FN_REFLEN+TEMP_FILE_MAX_LEN)*sizeof(char), MYF(0));
  sprintf(unique_tmp_file_name, "%s%s", tmp_file, server_uuid);
  if ((fd= mysql_file_create(key_file_misc,
                             unique_tmp_file_name, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0)
  DBUG_RETURN(1);

  /*
    Clean up.
   */
  mysql_file_close(fd, MYF(0));

  mysql_file_delete(key_file_misc, unique_tmp_file_name, MYF(0));
  my_free(unique_tmp_file_name);
  DBUG_RETURN(0);
}

/*
  Worker thread for the parallel execution of the replication events.
*/
pthread_handler_t handle_slave_worker(void *arg)
{
  THD *thd;                     /* needs to be first for thread_stack */
  bool thd_added= false;
  int error= 0;
  Slave_worker *w= (Slave_worker *) arg;
  Relay_log_info* rli= w->c_rli;
  ulong purge_cnt= 0;
  ulonglong purge_size= 0;
  struct slave_job_item _item, *job_item= &_item;

  my_thread_init();
  DBUG_ENTER("handle_slave_worker");

  thd= new THD;
  if (!thd)
  {
    sql_print_error("Failed during slave worker initialization");
    goto err;
  }
  w->info_thd= thd;
  thd->thread_stack = (char*)&thd;
  
  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_WORKER))
  {
    // todo make SQL thread killed
    sql_print_error("Failed during slave worker initialization");
    goto err;
  }
  thd->init_for_queries(w);

  mysql_mutex_lock(&LOCK_thread_count);
  add_global_thread(thd);
  thd_added= true;
  mysql_mutex_unlock(&LOCK_thread_count);

  if (w->update_is_transactional())
  {
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                "Error checking if the worker repository is transactional.");
    goto err;
  }

  mysql_mutex_lock(&w->jobs_lock);
  w->running_status= Slave_worker::RUNNING;
  mysql_cond_signal(&w->jobs_cond);

  mysql_mutex_unlock(&w->jobs_lock);

  DBUG_ASSERT(thd->is_slave_error == 0);

  while (!error)
  {
      error= slave_worker_exec_job(w, rli);
  }

  /* 
     Cleanup after an error requires clear_error() go first.
     Otherwise assert(!all) in binlog_rollback()
  */
  thd->clear_error();
  w->cleanup_context(thd, error);

  mysql_mutex_lock(&w->jobs_lock);

  while(de_queue(&w->jobs, job_item))
  {
    purge_cnt++;
    purge_size += ((Log_event*) (job_item->data))->data_written;
    DBUG_ASSERT(job_item->data);
    delete static_cast<Log_event*>(job_item->data);
  }

  DBUG_ASSERT(w->jobs.len == 0);

  mysql_mutex_unlock(&w->jobs_lock);

  mysql_mutex_lock(&rli->pending_jobs_lock);
  rli->pending_jobs -= purge_cnt;
  rli->mts_pending_jobs_size -= purge_size;
  DBUG_ASSERT(rli->mts_pending_jobs_size < rli->mts_pending_jobs_size_max);

  mysql_mutex_unlock(&rli->pending_jobs_lock);

  /*
     In MTS case cleanup_after_session() has be called explicitly.
     TODO: to make worker thd be deleted before Slave_worker instance.
  */
  if (thd->rli_slave)
  {
    w->cleanup_after_session();
    thd->rli_slave= NULL;
  }
  mysql_mutex_lock(&w->jobs_lock);

  w->running_status= Slave_worker::NOT_RUNNING;
  if (log_warnings > 1)
    sql_print_information("Worker %lu statistics: "
                          "events processed = %lu "
                          "hungry waits = %lu "
                          "priv queue overfills = %llu ",
                          w->id, w->events_done, w->wq_size_waits_cnt,
                          w->jobs.waited_overfill);
  mysql_cond_signal(&w->jobs_cond);  // famous last goodbye

  mysql_mutex_unlock(&w->jobs_lock);

err:

  if (thd)
  {
    /*
       The slave code is very bad. Notice that it is missing
       several clean up calls here. I've just added what was
       necessary to avoid valgrind errors.
 
       /Alfranio
    */
    DBUG_ASSERT(thd->net.buff != 0);
    net_end(&thd->net);

    /*
      to avoid close_temporary_tables() closing temp tables as those
      are Coordinator's burden.
    */
    thd->system_thread= NON_SYSTEM_THREAD;
    thd->release_resources();

    mysql_mutex_lock(&LOCK_thread_count);
    THD_CHECK_SENTRY(thd);
    if (thd_added)
      remove_global_thread(thd);
    mysql_mutex_unlock(&LOCK_thread_count);
    delete thd;
  }

  my_thread_end();
  ERR_remove_state(0);
  pthread_exit(0);
  DBUG_RETURN(0); 
}

/**
   Orders jobs by comparing relay log information.
*/

int mts_event_coord_cmp(LOG_POS_COORD *id1, LOG_POS_COORD *id2)
{
  longlong filecmp= strcmp(id1->file_name, id2->file_name);
  longlong poscmp= id1->pos - id2->pos;
  return (filecmp < 0  ? -1 : (filecmp > 0  ?  1 :
         (poscmp  < 0  ? -1 : (poscmp  > 0  ?  1 : 0))));
}

int mts_recovery_groups(Relay_log_info *rli)
{ 
  Log_event *ev= NULL;
  const char *errmsg= NULL;
  bool error= FALSE;
  bool flag_group_seen_begin= FALSE;
  uint recovery_group_cnt= 0;
  bool not_reached_commit= true;
  DYNAMIC_ARRAY above_lwm_jobs;
  Slave_job_group job_worker;
  IO_CACHE log;
  File file;
  LOG_INFO linfo;
  my_off_t offset= 0;
  MY_BITMAP *groups= &rli->recovery_groups;

  DBUG_ENTER("mts_recovery_groups");

  DBUG_ASSERT(rli->slave_parallel_workers == 0);

  /* 
     Although mts_recovery_groups() is reentrant it returns
     early if the previous invocation raised any bit in 
     recovery_groups bitmap.
  */
  if (rli->is_mts_recovery())
    DBUG_RETURN(0);

  /*
    Save relay log position to compare with worker's position.
  */
  LOG_POS_COORD cp=
  {
    (char *) rli->get_group_master_log_name(),
    rli->get_group_master_log_pos()
  };

  Format_description_log_event fdle(BINLOG_VERSION), *p_fdle= &fdle;

  if (!p_fdle->is_valid())
    DBUG_RETURN(TRUE);

  /*
    Gathers information on valuable workers and stores it in 
    above_lwm_jobs in asc ordered by the master binlog coordinates.
  */
  my_init_dynamic_array(&above_lwm_jobs, sizeof(Slave_job_group),
                        rli->recovery_parallel_workers,
                        rli->recovery_parallel_workers);

  for (uint id= 0; id < rli->recovery_parallel_workers; id++)
  {
    Slave_worker *worker=
      Rpl_info_factory::create_worker(opt_rli_repository_id, id, rli, true);

    if (!worker)
    {
      error= TRUE;
      goto err;
    }

    LOG_POS_COORD w_last= { const_cast<char*>(worker->get_group_master_log_name()),
                            worker->get_group_master_log_pos() };
    if (mts_event_coord_cmp(&w_last, &cp) > 0)
    {
      /*
        Inserts information into a dynamic array for further processing.
        The jobs/workers are ordered by the last checkpoint positions
        workers have seen.
      */
      job_worker.worker= worker;
      job_worker.checkpoint_log_pos= worker->checkpoint_master_log_pos;
      job_worker.checkpoint_log_name= worker->checkpoint_master_log_name;

      insert_dynamic(&above_lwm_jobs, (uchar*) &job_worker);
    }
    else
    {
      /*
        Deletes the worker because its jobs are included in the latest
        checkpoint.
      */
      delete worker;
    }
  }

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
  DBUG_ASSERT(!rli->recovery_groups_inited);

  if (above_lwm_jobs.elements != 0)
  {
    bitmap_init(groups, NULL, MTS_MAX_BITS_IN_GROUP, FALSE);
    rli->recovery_groups_inited= true;
    bitmap_clear_all(groups);
  }
  rli->mts_recovery_group_cnt= 0;
  for (uint it_job= 0; it_job < above_lwm_jobs.elements; it_job++)
  {
    Slave_worker *w= ((Slave_job_group *)
                      dynamic_array_ptr(&above_lwm_jobs, it_job))->worker;
    LOG_POS_COORD w_last= { const_cast<char*>(w->get_group_master_log_name()),
                            w->get_group_master_log_pos() };
    bool checksum_detected= FALSE;

    sql_print_information("Slave: MTS group recovery relay log info based on Worker-Id %lu, "
                          "group_relay_log_name %s, group_relay_log_pos %llu "
                          "group_master_log_name %s, group_master_log_pos %llu",
                          w->id,
                          w->get_group_relay_log_name(),
                          w->get_group_relay_log_pos(),
                          w->get_group_master_log_name(),
                          w->get_group_master_log_pos());

    recovery_group_cnt= 0;
    not_reached_commit= true;
    if (rli->relay_log.find_log_pos(&linfo, rli->get_group_relay_log_name(), 1))
    {
      error= TRUE;
      sql_print_error("Error looking for %s.", rli->get_group_relay_log_name());
      goto err;
    }
    offset= rli->get_group_relay_log_pos();
    for (int checking= 0 ; not_reached_commit; checking++)
    {
      if ((file= open_binlog_file(&log, linfo.log_file_name, &errmsg)) < 0)
      {
        error= TRUE;
        sql_print_error("%s", errmsg);
        goto err;
      }
      /*
        Looking for the actual relay checksum algorithm that is present in
        a FD at head events of the relay log.
      */
      if (!checksum_detected)
      {
        int i= 0;
        while (i < 4 && (ev= Log_event::read_log_event(&log,
               (mysql_mutex_t*) 0, p_fdle, 0)))
        {
          if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
          {
            p_fdle->checksum_alg= ev->checksum_alg;
            checksum_detected= TRUE;
          }
          delete ev;
          i++;
        }
        if (!checksum_detected)
        {
          error= TRUE;
          sql_print_error("%s", "malformed or very old relay log which "
                          "does not have FormatDescriptor");
          goto err;
        }
      }

      my_b_seek(&log, offset);

      while (not_reached_commit &&
             (ev= Log_event::read_log_event(&log, 0, p_fdle,
                                            opt_slave_sql_verify_checksum)))
      {
        DBUG_ASSERT(ev->is_valid());

        if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
          p_fdle->checksum_alg= ev->checksum_alg;

        if (ev->get_type_code() == ROTATE_EVENT ||
            ev->get_type_code() == FORMAT_DESCRIPTION_EVENT ||
            ev->get_type_code() == PREVIOUS_GTIDS_LOG_EVENT)
        {
          delete ev;
          ev= NULL;
          continue;
        }

        DBUG_PRINT("mts", ("Event Recoverying relay log info "
                   "group_mster_log_name %s, event_master_log_pos %llu type code %u.",
                   linfo.log_file_name, ev->log_pos, ev->get_type_code()));

        if (ev->starts_group())
        {
          flag_group_seen_begin= true;
        }
        else if ((ev->ends_group() || !flag_group_seen_begin) &&
                 !is_gtid_event(ev))
        {
          int ret= 0;
          LOG_POS_COORD ev_coord= { (char *) rli->get_group_master_log_name(),
                                      ev->log_pos };
          flag_group_seen_begin= false;
          recovery_group_cnt++;

          sql_print_information("Slave: MTS group recovery relay log info "
                                "group_master_log_name %s, "
                                "event_master_log_pos %llu.",
                                rli->get_group_master_log_name(), ev->log_pos);
          if ((ret= mts_event_coord_cmp(&ev_coord, &w_last)) == 0)
          {
#ifndef DBUG_OFF
            for (uint i= 0; i <= w->checkpoint_seqno; i++)
            {
              if (bitmap_is_set(&w->group_executed, i))
                DBUG_PRINT("mts", ("Bit %u is set.", i));
              else
                DBUG_PRINT("mts", ("Bit %u is not set.", i));
            }
#endif
            DBUG_PRINT("mts",
                       ("Doing a shift ini(%lu) end(%lu).",
                       (w->checkpoint_seqno + 1) - recovery_group_cnt,
                        w->checkpoint_seqno));

            for (uint i= (w->checkpoint_seqno + 1) - recovery_group_cnt,
                 j= 0; i <= w->checkpoint_seqno; i++, j++)
            {
              if (bitmap_is_set(&w->group_executed, i))
              {
                DBUG_PRINT("mts", ("Setting bit %u.", j));
                bitmap_fast_test_and_set(groups, j);
              }
            }
            not_reached_commit= false;
          }
          else
            DBUG_ASSERT(ret < 0);
        }
        delete ev;
        ev= NULL;
      }
      end_io_cache(&log);
      mysql_file_close(file, MYF(MY_WME));
      offset= BIN_LOG_HEADER_SIZE;
      if (not_reached_commit && rli->relay_log.find_next_log(&linfo, 1))
      {
         error= TRUE;
         sql_print_error("Error looking for file after %s.", linfo.log_file_name);
         goto err;
      }
    }

    rli->mts_recovery_group_cnt= (rli->mts_recovery_group_cnt < recovery_group_cnt ?
      recovery_group_cnt : rli->mts_recovery_group_cnt);
  }

  DBUG_ASSERT(!rli->recovery_groups_inited ||
              rli->mts_recovery_group_cnt <= groups->n_bits);

err:
  
  for (uint it_job= 0; it_job < above_lwm_jobs.elements; it_job++)
  {
    get_dynamic(&above_lwm_jobs, (uchar *) &job_worker, it_job);
    delete job_worker.worker;
  }

  delete_dynamic(&above_lwm_jobs);
  if (rli->recovery_groups_inited && rli->mts_recovery_group_cnt == 0)
  {
    bitmap_free(groups);
    rli->recovery_groups_inited= false;
  }

  DBUG_RETURN(error ? ER_MTS_RECOVERY_FAILURE : 0);
}

/**
   Processing rli->gaq to find out the low-water-mark (lwm) coordinates
   which is stored into the cental recovery table.

   @param rli            pointer to Relay-log-info of Coordinator
   @param period         period of processing GAQ, normally derived from
                         @c mts_checkpoint_period
   @param force          if TRUE then hang in a loop till some progress
   @param need_data_lock False if rli->data_lock mutex is aquired by
                         the caller.

   @return FALSE success, TRUE otherwise
*/
bool mts_checkpoint_routine(Relay_log_info *rli, ulonglong period,
                            bool force, bool need_data_lock)
{
  ulong cnt;
  bool error= FALSE;
  struct timespec curr_clock;

  DBUG_ENTER("checkpoint_routine");

#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0))
  {
    if (!rli->gaq->count_done(rli))
      DBUG_RETURN(FALSE);
  }
#endif

  /*
    rli->checkpoint_group can have two possible values due to
    two possible status of the last (being scheduled) group. 
  */
  DBUG_ASSERT(!rli->gaq->full() ||
              ((rli->checkpoint_seqno == rli->checkpoint_group -1 &&
                rli->mts_group_status == Relay_log_info::MTS_IN_GROUP) ||
               rli->checkpoint_seqno == rli->checkpoint_group));

  /*
    Currently, the checkpoint routine is being called by the SQL Thread.
    For that reason, this function is called call from appropriate points
    in the SQL Thread's execution path and the elapsed time is calculated
    here to check if it is time to execute it.
  */
  set_timespec_nsec(curr_clock, 0);
  ulonglong diff= diff_timespec(curr_clock, rli->last_clock);
  if (!force && diff < period)
  {
    /*
      We do not need to execute the checkpoint now because
      the time elapsed is not enough.
    */
    DBUG_RETURN(FALSE);
  }

  do
  {
    cnt= rli->gaq->move_queue_head(&rli->workers);
#ifndef DBUG_OFF
    if (DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0) &&
        cnt != opt_mts_checkpoint_period)
      sql_print_error("This an error cnt != mts_checkpoint_period");
#endif
  } while (!sql_slave_killed(rli->info_thd, rli) &&
           cnt == 0 && force &&
           !DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0) &&
           (my_sleep(rli->mts_coordinator_basic_nap), 1));
  /*
    This checks how many consecutive jobs where processed.
    If this value is different than zero the checkpoint
    routine can proceed. Otherwise, there is nothing to be
    done.
  */      
  if (cnt == 0)
    goto end;


  /* TODO: 
     to turn the least occupied selection in terms of jobs pieces
  */
  for (uint i= 0; i < rli->workers.elements; i++)
  {
    Slave_worker *w_i;
    get_dynamic(&rli->workers, (uchar *) &w_i, i);
    set_dynamic(&rli->least_occupied_workers, (uchar*) &w_i->jobs.len, w_i->id);
  };
  sort_dynamic(&rli->least_occupied_workers, (qsort_cmp) ulong_cmp);

  if (need_data_lock)
    mysql_mutex_lock(&rli->data_lock);
  else
    mysql_mutex_assert_owner(&rli->data_lock);

  /*
    "Coordinator::commit_positions" {

    rli->gaq->lwm has been updated in move_queue_head() and 
    to contain all but rli->group_master_log_name which 
    is altered solely by Coordinator at special checkpoints.
  */
  rli->set_group_master_log_pos(rli->gaq->lwm.group_master_log_pos);
  rli->set_group_relay_log_pos(rli->gaq->lwm.group_relay_log_pos);
  DBUG_PRINT("mts", ("New checkpoint %llu %llu %s",
             rli->gaq->lwm.group_master_log_pos,
             rli->gaq->lwm.group_relay_log_pos,
             rli->gaq->lwm.group_relay_log_name));

  if (rli->gaq->lwm.group_relay_log_name[0] != 0)
    rli->set_group_relay_log_name(rli->gaq->lwm.group_relay_log_name);

  /* 
     todo: uncomment notifies when UNTIL will be supported

     rli->notify_group_master_log_name_update();
     rli->notify_group_relay_log_name_update();

     Todo: optimize with if (wait_flag) broadcast
         waiter: set wait_flag; waits....; drops wait_flag;
  */

  error= rli->flush_info(TRUE);

  mysql_cond_broadcast(&rli->data_cond);
  if (need_data_lock)
    mysql_mutex_unlock(&rli->data_lock);

  /*
    We need to ensure that this is never called at this point when
    cnt is zero. This value means that the checkpoint information
    will be completely reset.
  */
  rli->reset_notified_checkpoint(cnt, rli->gaq->lwm.ts, need_data_lock);

  /* end-of "Coordinator::"commit_positions" */

end:
#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0))
    DBUG_SUICIDE();
#endif
  set_timespec_nsec(rli->last_clock, 0);
  
  DBUG_RETURN(error);
}

/**
   Instantiation of a Slave_worker and forking out a single Worker thread.
   
   @param  rli  Coordinator's Relay_log_info pointer
   @param  i    identifier of the Worker

   @return 0 suppress or 1 if fails
*/
int slave_start_single_worker(Relay_log_info *rli, ulong i)
{
  int error= 0;
  pthread_t th;
  Slave_worker *w= NULL;

  mysql_mutex_assert_owner(&rli->run_lock);

  if (!(w=
        Rpl_info_factory::create_worker(opt_rli_repository_id, i, rli, false)))
  {
    sql_print_error("Failed during slave worker thread create");
    error= 1;
    goto err;
  }

  if (w->init_worker(rli, i))
  {
    sql_print_error("Failed during slave worker thread create");
    error= 1;
    goto err;
  }
  set_dynamic(&rli->workers, (uchar*) &w, i);

  if (DBUG_EVALUATE_IF("mts_worker_thread_fails", i == 1, 0) ||
      pthread_create(&th, &connection_attrib, handle_slave_worker,
                     (void*) w))
  {
    sql_print_error("Failed during slave worker thread create");
    error= 1;
    goto err;
  }
  
  mysql_mutex_lock(&w->jobs_lock);
  if (w->running_status == Slave_worker::NOT_RUNNING)
    mysql_cond_wait(&w->jobs_cond, &w->jobs_lock);
  mysql_mutex_unlock(&w->jobs_lock);
  // Least occupied inited with zero
  insert_dynamic(&rli->least_occupied_workers, (uchar*) &w->jobs.len);

err:
  if (error && w)
  {
    delete w;
    /*
      Any failure after dynarray inserted must follow with deletion
      of just created item.
    */
    if (rli->workers.elements == i + 1)
      delete_dynamic_element(&rli->workers, i);
  }
  return error;
}

/**
   Initialization of the central rli members for Coordinator's role,
   communication channels such as Assigned Partition Hash (APH),
   and starting the Worker pool.

   @param  n   Number of configured Workers in the upcoming session.

   @return 0         success
           non-zero  as failure
*/
int slave_start_workers(Relay_log_info *rli, ulong n, bool *mts_inited)
{
  uint i;
  int error= 0;

  mysql_mutex_assert_owner(&rli->run_lock);

  if (n == 0 && rli->mts_recovery_group_cnt == 0)
  {
    reset_dynamic(&rli->workers);
    goto end;
  }

  *mts_inited= true;

  /*
    The requested through argument number of Workers can be different 
     from the previous time which ended with an error. Thereby
     the effective number of configured Workers is max of the two.
  */
  rli->init_workers(max(n, rli->recovery_parallel_workers));

  // CGAP dynarray holds id:s of partitions of the Current being executed Group
  my_init_dynamic_array(&rli->curr_group_assigned_parts,
                        sizeof(db_worker_hash_entry*),
                        SLAVE_INIT_DBS_IN_GROUP, 1);
  rli->last_assigned_worker= NULL;     // associated with curr_group_assigned
  my_init_dynamic_array(&rli->curr_group_da, sizeof(Log_event*), 8, 2);
  // Least_occupied_workers array to hold items size of Slave_jobs_queue::len
  my_init_dynamic_array(&rli->least_occupied_workers, sizeof(ulong), n, 0); 

  /* 
     GAQ  queue holds seqno:s of scheduled groups. C polls workers in 
     @c opt_mts_checkpoint_period to update GAQ (see @c next_event())
     The length of GAQ is set to be equal to checkpoint_group.
     Notice, the size matters for mts_checkpoint_routine's progress loop.
  */

  rli->gaq= new Slave_committed_queue(rli->get_group_master_log_name(),
                                      sizeof(Slave_job_group),
                                      rli->checkpoint_group, n);
  if (!rli->gaq->inited)
    return 1;

  // length of WQ is actually constant though can be made configurable
  rli->mts_slave_worker_queue_len_max= mts_slave_worker_queue_len_max;
  rli->mts_pending_jobs_size= 0;
  rli->mts_pending_jobs_size_max= ::opt_mts_pending_jobs_size_max;
  rli->mts_wq_underrun_w_id= MTS_WORKER_UNDEF;
  rli->mts_wq_excess_cnt= 0;
  rli->mts_wq_overrun_cnt= 0;
  rli->mts_wq_oversize= FALSE;
  rli->mts_coordinator_basic_nap= mts_coordinator_basic_nap;
  rli->mts_worker_underrun_level= mts_worker_underrun_level;
  rli->curr_group_seen_begin= rli->curr_group_seen_gtid= false;
  rli->curr_group_isolated= FALSE;
  rli->checkpoint_seqno= 0;
  rli->mts_last_online_stat= my_time(0);
  rli->mts_group_status= Relay_log_info::MTS_NOT_IN_GROUP;
  /*
    dyn memory to consume by Coordinator per event
  */
  init_alloc_root(&rli->mts_coor_mem_root, NAME_LEN,
                  (MAX_DBS_IN_EVENT_MTS / 2) * NAME_LEN);

  if (init_hash_workers(n))  // MTS: mapping_db_to_worker
  {
    sql_print_error("Failed to init partitions hash");
    error= 1;
    goto err;
  }

  for (i= 0; i < n; i++)
  {
    if ((error= slave_start_single_worker(rli, i)))
      goto err;
  }

end:
  rli->slave_parallel_workers= n;
  // Effective end of the recovery right now when there is no gaps
  if (!error && rli->mts_recovery_group_cnt == 0)
  {
    if ((error= rli->mts_finalize_recovery()))
      (void) Rpl_info_factory::reset_workers(rli);
    if (!error)
      error= rli->flush_info(TRUE);
  }

err:
  return error;
}

/* 
   Ending Worker threads.

   Not in case Coordinator is killed itself, it first waits for
   Workers have finished their assignements, and then updates checkpoint. 
   Workers are notified with setting KILLED status
   and waited for their acknowledgment as specified by
   worker's running_status.
   Coordinator finalizes with its MTS running status to reset few objects.
*/
void slave_stop_workers(Relay_log_info *rli, bool *mts_inited)
{
  int i;
  THD *thd= rli->info_thd;

  mysql_mutex_assert_owner(&rli->run_lock);

  if (!*mts_inited) 
    return;
  else if (rli->slave_parallel_workers == 0)
    goto end;

  /*
    In case of the "soft" graceful stop Coordinator
    guaranteed Workers were assigned with full groups so waiting
    will be resultful.
    "Hard" stop with KILLing Coordinator or erroring out by a Worker
    can't wait for Workers' completion because those may not receive
    commit-events of last assigned groups.
  */
  if (rli->mts_group_status != Relay_log_info::MTS_KILLED_GROUP &&
      thd->killed == THD::NOT_KILLED)
  {
    DBUG_ASSERT(rli->mts_group_status != Relay_log_info::MTS_IN_GROUP ||
                thd->is_error());

#ifndef DBUG_OFF
    if (DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0))
    {
      sql_print_error("This is not supposed to happen at this point...");
      DBUG_SUICIDE();
    }
#endif
    // No need to know a possible error out of synchronization call.
    (void) wait_for_workers_to_finish(rli);
    /*
      At this point the coordinator has been stopped and the checkpoint
      routine is executed to eliminate possible gaps.
    */
    (void) mts_checkpoint_routine(rli, 0, false, true/*need_data_lock=true*/); // TODO: ALFRANIO ERROR
  }
  for (i= rli->workers.elements - 1; i >= 0; i--)
  {
    Slave_worker *w;
    get_dynamic((DYNAMIC_ARRAY*)&rli->workers, (uchar*) &w, i);
    
    mysql_mutex_lock(&w->jobs_lock);
    
    if (w->running_status != Slave_worker::RUNNING)
    {
      mysql_mutex_unlock(&w->jobs_lock);
      continue;
    }

    w->running_status= Slave_worker::KILLED;
    mysql_cond_signal(&w->jobs_cond);

    mysql_mutex_unlock(&w->jobs_lock);

    if (log_warnings > 1)
      sql_print_information("Notifying Worker %lu to exit, thd %p", w->id,
                            w->info_thd);
  }

  thd_proc_info(thd, "Waiting for workers to exit");

  for (i= rli->workers.elements - 1; i >= 0; i--)
  {
    Slave_worker *w= NULL;
    get_dynamic((DYNAMIC_ARRAY*)&rli->workers, (uchar*) &w, i);

    mysql_mutex_lock(&w->jobs_lock);
    while (w->running_status != Slave_worker::NOT_RUNNING)
    {
      PSI_stage_info old_stage;
      DBUG_ASSERT(w->running_status == Slave_worker::KILLED ||
                  w->running_status == Slave_worker::ERROR_LEAVING);

      thd->ENTER_COND(&w->jobs_cond, &w->jobs_lock,
                      &stage_slave_waiting_workers_to_exit, &old_stage);
      mysql_cond_wait(&w->jobs_cond, &w->jobs_lock);
      thd->EXIT_COND(&old_stage);
      mysql_mutex_lock(&w->jobs_lock);
    }
    mysql_mutex_unlock(&w->jobs_lock);

    delete_dynamic_element(&rli->workers, i);
    delete w;
  }

  if (log_warnings > 1)
    sql_print_information("Total MTS session statistics: "
                          "events processed = %llu; "
                          "worker queues filled over overrun level = %lu; "
                          "waited due a Worker queue full = %lu; "
                          "waited due the total size = %lu; "
                          "slept when Workers occupied = %lu ",
                          rli->mts_events_assigned, rli->mts_wq_overrun_cnt,
                          rli->mts_wq_overfill_cnt, rli->wq_size_waits_cnt,
                          rli->mts_wq_no_underrun_cnt);

  DBUG_ASSERT(rli->pending_jobs == 0);
  DBUG_ASSERT(rli->mts_pending_jobs_size == 0);

end:
  rli->mts_group_status= Relay_log_info::MTS_NOT_IN_GROUP;
  destroy_hash_workers(rli);
  delete rli->gaq;
  delete_dynamic(&rli->least_occupied_workers);    // least occupied

  // Destroy buffered events of the current group prior to exit.
  for (uint i= 0; i < rli->curr_group_da.elements; i++)
    delete *(Log_event**) dynamic_array_ptr(&rli->curr_group_da, i);
  delete_dynamic(&rli->curr_group_da);             // GCDA

  delete_dynamic(&rli->curr_group_assigned_parts); // GCAP
  rli->deinit_workers();
  rli->slave_parallel_workers= 0;
  free_root(&rli->mts_coor_mem_root, MYF(0));
  *mts_inited= false;
}


/**
  Slave SQL thread entry point.

  @param arg Pointer to Relay_log_info object that holds information
  for the SQL thread.

  @return Always 0.
*/
pthread_handler_t handle_slave_sql(void *arg)
{
  THD *thd;                     /* needs to be first for thread_stack */
  bool thd_added= false;
  char llbuff[22],llbuff1[22];
  char saved_log_name[FN_REFLEN];
  char saved_master_log_name[FN_REFLEN];
  my_off_t saved_log_pos= 0;
  my_off_t saved_master_log_pos= 0;
  my_off_t saved_skip= 0;

  Relay_log_info* rli = ((Master_info*)arg)->rli;
  const char *errmsg;
  bool mts_inited= false;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_sql");

  DBUG_ASSERT(rli->inited);
  mysql_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
  errmsg= 0;
#ifndef DBUG_OFF
  rli->events_until_exit = abort_slave_event_count;
#endif

  thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->thread_stack = (char*)&thd; // remember where our stack is
  rli->info_thd= thd;
  
  /* Inform waiting threads that slave has started */
  rli->slave_run_id++;
  rli->slave_running = 1;
  rli->reported_unsafe_warning= false;

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_SQL))
  {
    /*
      TODO: this is currently broken - slave start and change master
      will be stuck if we fail here
    */
    mysql_cond_broadcast(&rli->start_cond);
    mysql_mutex_unlock(&rli->run_lock);
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Failed during slave thread initialization");
    goto err;
  }
  thd->init_for_queries(rli);
  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  set_thd_in_use_temporary_tables(rli);   // (re)set sql_thd in use for saved temp tables

  mysql_mutex_lock(&LOCK_thread_count);
  add_global_thread(thd);
  thd_added= true;
  mysql_mutex_unlock(&LOCK_thread_count);

  /* MTS: starting the worker pool */
  if (slave_start_workers(rli, rli->opt_slave_parallel_workers, &mts_inited) != 0)
  {
    mysql_cond_broadcast(&rli->start_cond);
    mysql_mutex_unlock(&rli->run_lock);
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Failed during slave workers initialization");
    goto err;
  }
  /*
    We are going to set slave_running to 1. Assuming slave I/O thread is
    alive and connected, this is going to make Seconds_Behind_Master be 0
    i.e. "caught up". Even if we're just at start of thread. Well it's ok, at
    the moment we start we can think we are caught up, and the next second we
    start receiving data so we realize we are not caught up and
    Seconds_Behind_Master grows. No big deal.
  */
  rli->abort_slave = 0;

  /*
    Reset errors for a clean start (otherwise, if the master is idle, the SQL
    thread may execute no Query_log_event, so the error will remain even
    though there's no problem anymore). Do not reset the master timestamp
    (imagine the slave has caught everything, the STOP SLAVE and START SLAVE:
    as we are not sure that we are going to receive a query, we want to
    remember the last master timestamp (to say how many seconds behind we are
    now.
    But the master timestamp is reset by RESET SLAVE & CHANGE MASTER.
  */
  rli->clear_error();

  if (rli->update_is_transactional())
  {
    mysql_cond_broadcast(&rli->start_cond);
    mysql_mutex_unlock(&rli->run_lock);
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Error checking if the relay log repository is transactional.");
    goto err;
  }

  if (!rli->is_transactional())
    rli->report(WARNING_LEVEL, 0,
    "If a crash happens this configuration does not guarantee that the relay "
    "log info will be consistent");

  mysql_mutex_unlock(&rli->run_lock);
  mysql_cond_broadcast(&rli->start_cond);

  DEBUG_SYNC(thd, "after_start_slave");

  //tell the I/O thread to take relay_log_space_limit into account from now on
  mysql_mutex_lock(&rli->log_space_lock);
  rli->ignore_log_space_limit= 0;
  mysql_mutex_unlock(&rli->log_space_lock);
  rli->trans_retries= 0; // start from "no error"
  DBUG_PRINT("info", ("rli->trans_retries: %lu", rli->trans_retries));

  if (rli->init_relay_log_pos(rli->get_group_relay_log_name(),
                              rli->get_group_relay_log_pos(),
                              true/*need_data_lock=true*/, &errmsg,
                              1 /*look for a description_event*/))
  { 
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Error initializing relay log position: %s", errmsg);
    goto err;
  }
  THD_CHECK_SENTRY(thd);
#ifndef DBUG_OFF
  {
    char llbuf1[22], llbuf2[22];
    DBUG_PRINT("info", ("my_b_tell(rli->cur_log)=%s rli->event_relay_log_pos=%s",
                        llstr(my_b_tell(rli->cur_log),llbuf1),
                        llstr(rli->get_event_relay_log_pos(),llbuf2)));
    DBUG_ASSERT(rli->get_event_relay_log_pos() >= BIN_LOG_HEADER_SIZE);
    /*
      Wonder if this is correct. I (Guilhem) wonder if my_b_tell() returns the
      correct position when it's called just after my_b_seek() (the questionable
      stuff is those "seek is done on next read" comments in the my_b_seek()
      source code).
      The crude reality is that this assertion randomly fails whereas
      replication seems to work fine. And there is no easy explanation why it
      fails (as we my_b_seek(rli->event_relay_log_pos) at the very end of
      init_relay_log_pos() called above). Maybe the assertion would be
      meaningful if we held rli->data_lock between the my_b_seek() and the
      DBUG_ASSERT().
    */
#ifdef SHOULD_BE_CHECKED
    DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->get_event_relay_log_pos());
#endif
  }
#endif
  DBUG_ASSERT(rli->info_thd == thd);

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  /* engine specific hook, to be made generic */
  if (ndb_wait_setup_func && ndb_wait_setup_func(opt_ndb_wait_setup))
  {
    sql_print_warning("Slave SQL thread : NDB : Tables not available after %lu"
                      " seconds.  Consider increasing --ndb-wait-setup value",
                      opt_ndb_wait_setup);
  }
#endif

  DBUG_PRINT("master_info",("log_file_name: %s  position: %s",
                            rli->get_group_master_log_name(),
                            llstr(rli->get_group_master_log_pos(),llbuff)));
  if (log_warnings)
    sql_print_information("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", rli->get_rpl_log_name(),
                    llstr(rli->get_group_master_log_pos(),llbuff),rli->get_group_relay_log_name(),
                    llstr(rli->get_group_relay_log_pos(),llbuff1));

  if (check_temp_dir(rli->slave_patternload_file))
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->sql_errno(), 
                "Unable to use slave's temporary directory %s - %s", 
                slave_load_tmpdir, thd->get_stmt_da()->message());
    goto err;
  }

  /* execute init_slave variable */
  if (opt_init_slave.length)
  {
    execute_init_command(thd, &opt_init_slave, &LOCK_sys_init_slave);
    if (thd->is_slave_error)
    {
      rli->report(ERROR_LEVEL, thd->get_stmt_da()->sql_errno(),
                  "Slave SQL thread aborted. Can't execute init_slave query");
      goto err;
    }
  }

  /*
    First check until condition - probably there is nothing to execute. We
    do not want to wait for next event in this case.
  */
  mysql_mutex_lock(&rli->data_lock);
  if (rli->slave_skip_counter)
  {
    strmake(saved_log_name, rli->get_group_relay_log_name(), FN_REFLEN - 1);
    strmake(saved_master_log_name, rli->get_group_master_log_name(), FN_REFLEN - 1);
    saved_log_pos= rli->get_group_relay_log_pos();
    saved_master_log_pos= rli->get_group_master_log_pos();
    saved_skip= rli->slave_skip_counter;
  }
  if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
      rli->is_until_satisfied(thd, NULL))
  {
    mysql_mutex_unlock(&rli->data_lock);
    goto err;
  }
  mysql_mutex_unlock(&rli->data_lock);

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    THD_STAGE_INFO(thd, stage_reading_event_from_the_relay_log);
    DBUG_ASSERT(rli->info_thd == thd);
    THD_CHECK_SENTRY(thd);

    if (saved_skip && rli->slave_skip_counter == 0)
    {
      sql_print_information("'SQL_SLAVE_SKIP_COUNTER=%ld' executed at "
        "relay_log_file='%s', relay_log_pos='%ld', master_log_name='%s', "
        "master_log_pos='%ld' and new position at "
        "relay_log_file='%s', relay_log_pos='%ld', master_log_name='%s', "
        "master_log_pos='%ld' ",
        (ulong) saved_skip, saved_log_name, (ulong) saved_log_pos,
        saved_master_log_name, (ulong) saved_master_log_pos,
        rli->get_group_relay_log_name(), (ulong) rli->get_group_relay_log_pos(),
        rli->get_group_master_log_name(), (ulong) rli->get_group_master_log_pos());
      saved_skip= 0;
    }
    
    if (exec_relay_log_event(thd,rli))
    {
      DBUG_PRINT("info", ("exec_relay_log_event() failed"));
      // do not scare the user if SQL thread was simply killed or stopped
      if (!sql_slave_killed(thd,rli))
      {
        /*
          retrieve as much info as possible from the thd and, error
          codes and warnings and print this to the error log as to
          allow the user to locate the error
        */
        uint32 const last_errno= rli->last_error().number;

        if (thd->is_error())
        {
          char const *const errmsg= thd->get_stmt_da()->message();

          DBUG_PRINT("info",
                     ("thd->get_stmt_da()->sql_errno()=%d; "
                      "rli->last_error.number=%d",
                      thd->get_stmt_da()->sql_errno(), last_errno));
          if (last_errno == 0)
          {
            /*
 	      This function is reporting an error which was not reported
 	      while executing exec_relay_log_event().
 	    */ 
            rli->report(ERROR_LEVEL, thd->get_stmt_da()->sql_errno(),
                        "%s", errmsg);
          }
          else if (last_errno != thd->get_stmt_da()->sql_errno())
          {
            /*
             * An error was reported while executing exec_relay_log_event()
             * however the error code differs from what is in the thread.
             * This function prints out more information to help finding
             * what caused the problem.
             */  
            sql_print_error("Slave (additional info): %s Error_code: %d",
                            errmsg, thd->get_stmt_da()->sql_errno());
          }
        }

        /* Print any warnings issued */
        Diagnostics_area::Sql_condition_iterator it=
          thd->get_stmt_da()->sql_conditions();
        const Sql_condition *err;
        /*
          Added controlled slave thread cancel for replication
          of user-defined variables.
        */
        bool udf_error = false;
        while ((err= it++))
        {
          if (err->get_sql_errno() == ER_CANT_OPEN_LIBRARY)
            udf_error = true;
          sql_print_warning("Slave: %s Error_code: %d", err->get_message_text(), err->get_sql_errno());
        }
        if (udf_error)
          sql_print_error("Error loading user-defined library, slave SQL "
            "thread aborted. Install the missing library, and restart the "
            "slave SQL thread with \"SLAVE START\". We stopped at log '%s' "
            "position %s", rli->get_rpl_log_name(),
            llstr(rli->get_group_master_log_pos(), llbuff));
        else
          sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s", rli->get_rpl_log_name(),
llstr(rli->get_group_master_log_pos(), llbuff));
      }
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_information("Slave SQL thread exiting, replication stopped in log "
                        "'%s' at position %s",
                        rli->get_rpl_log_name(),
                        llstr(rli->get_group_master_log_pos(), llbuff));

 err:

  mysql_mutex_lock(&rli->run_lock);
  slave_stop_workers(rli, &mts_inited); // stopping worker pool
  if (rli->recovery_groups_inited)
  {
    bitmap_free(&rli->recovery_groups);
    rli->recovery_groups_inited= false;
  }

  /*
    Some events set some playgrounds, which won't be cleared because thread
    stops. Stopping of this thread may not be known to these events ("stop"
    request is detected only by the present function, not by events), so we
    must "proactively" clear playgrounds:
  */
  thd->clear_error();
  rli->cleanup_context(thd, 1);
  /*
    Some extra safety, which should not been needed (normally, event deletion
    should already have done these assignments (each event which sets these
    variables is supposed to set them to 0 before terminating)).
  */
  thd->catalog= 0;
  thd->reset_query();
  thd->reset_db(NULL, 0);

  THD_STAGE_INFO(thd, stage_waiting_for_slave_mutex_on_exit);
  /* We need data_lock, at least to wake up any waiting master_pos_wait() */
  mysql_mutex_lock(&rli->data_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  /* When master_pos_wait() wakes up it will check this and terminate */
  rli->slave_running= 0;
  /* Forget the relay log's format */
  rli->set_rli_description_event(NULL);
  /* Wake up master_pos_wait() */
  mysql_mutex_unlock(&rli->data_lock);
  DBUG_PRINT("info",("Signaling possibly waiting master_pos_wait() functions"));
  mysql_cond_broadcast(&rli->data_cond);
  rli->ignore_log_space_limit= 0; /* don't need any lock */
  /* we die so won't remember charset - re-update them on next thread start */
  rli->cached_charset_invalidate();
  rli->save_temporary_tables = thd->temporary_tables;

  /*
    TODO: see if we can do this conditionally in next_event() instead
    to avoid unneeded position re-init
  */
  thd->temporary_tables = 0; // remove tempation from destructor to close them
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  DBUG_ASSERT(rli->info_thd == thd);
  THD_CHECK_SENTRY(thd);
  rli->info_thd= 0;
  set_thd_in_use_temporary_tables(rli);  // (re)set info_thd in use for saved temp tables

  thd->release_resources();
  mysql_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  if (thd_added)
    remove_global_thread(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  delete thd;
 /*
  Note: the order of the broadcast and unlock calls below (first broadcast, then unlock)
  is important. Otherwise a killer_thread can execute between the calls and
  delete the mi structure leading to a crash! (see BUG#25306 for details)
 */ 
  mysql_cond_broadcast(&rli->stop_cond);
  DBUG_EXECUTE_IF("simulate_slave_delay_at_terminate_bug38694", sleep(5););
  mysql_mutex_unlock(&rli->run_lock);  // tell the world we are done

  DBUG_LEAVE;                            // Must match DBUG_ENTER()
  my_thread_end();
  ERR_remove_state(0);
  pthread_exit(0);
  return 0;                             // Avoid compiler warnings
}


/*
  process_io_create_file()
*/

static int process_io_create_file(Master_info* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD *thd = mi->info_thd;
  NET *net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

  mysql_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!cev->is_valid()))
    DBUG_RETURN(1);

  if (!rpl_filter->db_ok(cev->db))
  {
    skip_load_data_infile(net);
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(cev->inited_from_old);
  thd->file_id = cev->file_id = mi->file_id++;
  thd->server_id = cev->server_id;
  cev_not_written = 1;

  if (unlikely(net_request_file(net,cev->fname)))
  {
    sql_print_error("Slave I/O: failed requesting download of '%s'",
                    cev->fname);
    goto err;
  }

  /*
    This dummy block is so we could instantiate Append_block_log_event
    once and then modify it slightly instead of doing it multiple times
    in the loop
  */
  {
    Append_block_log_event aev(thd,0,0,0,0);

    for (;;)
    {
      if (unlikely((num_bytes=my_net_read(net)) == packet_error))
      {
        sql_print_error("Network read error downloading '%s' from master",
                        cev->fname);
        goto err;
      }
      if (unlikely(!num_bytes)) /* eof */
      {
	/* 3.23 master wants it */
        net_write_command(net, 0, (uchar*) "", 0, (uchar*) "", 0);
        /*
          If we wrote Create_file_log_event, then we need to write
          Execute_load_log_event. If we did not write Create_file_log_event,
          then this is an empty file and we can just do as if the LOAD DATA
          INFILE had not existed, i.e. write nothing.
        */
        if (unlikely(cev_not_written))
          break;
        Execute_load_log_event xev(thd,0,0);
        xev.log_pos = cev->log_pos;
        if (unlikely(mi->rli->relay_log.append_event(&xev, mi) != 0))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Exec_load event to relay log");
          goto err;
        }
        mi->rli->relay_log.harvest_bytes_written(&mi->rli->log_space_total);
        break;
      }
      if (unlikely(cev_not_written))
      {
        cev->block = net->read_pos;
        cev->block_len = num_bytes;
        if (unlikely(mi->rli->relay_log.append_event(cev, mi) != 0))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Create_file event to relay log");
          goto err;
        }
        cev_not_written=0;
        mi->rli->relay_log.harvest_bytes_written(&mi->rli->log_space_total);
      }
      else
      {
        aev.block = net->read_pos;
        aev.block_len = num_bytes;
        aev.log_pos = cev->log_pos;
        if (unlikely(mi->rli->relay_log.append_event(&aev, mi) != 0))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Append_block event to relay log");
          goto err;
        }
        mi->rli->relay_log.harvest_bytes_written(&mi->rli->log_space_total);
      }
    }
  }
  error=0;
err:
  DBUG_RETURN(error);
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
static int process_io_rotate(Master_info *mi, Rotate_log_event *rev)
{
  DBUG_ENTER("process_io_rotate");
  mysql_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);

  /* Safe copy as 'rev' has been "sanitized" in Rotate_log_event's ctor */
  memcpy(const_cast<char *>(mi->get_master_log_name()),
         rev->new_log_ident, rev->ident_len + 1);
  mi->set_master_log_pos(rev->pos);
  DBUG_PRINT("info", ("new (master_log_name, master_log_pos): ('%s', %lu)",
                      mi->get_master_log_name(), (ulong) mi->get_master_log_pos()));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    mi->events_until_exit++;
#endif

  /*
    If mi_description_event is format <4, there is conversion in the
    relay log to the slave's format (4). And Rotate can mean upgrade or
    nothing. If upgrade, it's to 5.0 or newer, so we will get a Format_desc, so
    no need to reset mi_description_event now. And if it's nothing (same
    master version as before), no need (still using the slave's format).
  */
  Format_description_log_event *old_fdle= mi->get_mi_description_event();
  if (old_fdle->binlog_version >= 4)
  {
    DBUG_ASSERT(old_fdle->checksum_alg ==
                mi->rli->relay_log.relay_log_checksum_alg);
    Format_description_log_event *new_fdle= new
      Format_description_log_event(3);
    new_fdle->checksum_alg= mi->rli->relay_log.relay_log_checksum_alg;
    mi->set_mi_description_event(new_fdle);
  }
  /*
    Rotate the relay log makes binlog format detection easier (at next slave
    start or mysqlbinlog)
  */
  int ret= rotate_relay_log(mi);
  DBUG_RETURN(ret);
}

/**
  Reads a 3.23 event and converts it to the slave's format. This code was
  copied from MySQL 4.0.

  @note The caller must hold mi->data_lock before invoking this function.
*/
static int queue_binlog_ver_1_event(Master_info *mi, const char *buf,
                                    ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  bool ignore_event= 0;
  char *tmp_buf = 0;
  Relay_log_info *rli= mi->rli;
  DBUG_ENTER("queue_binlog_ver_1_event");

  mysql_mutex_assert_owner(&mi->data_lock);

  /*
    If we get Load event, we need to pass a non-reusable buffer
    to read_log_event, so we do a trick
  */
  if (buf[EVENT_TYPE_OFFSET] == LOAD_EVENT)
  {
    if (unlikely(!(tmp_buf=(char*)my_malloc(event_len+1,MYF(MY_WME)))))
    {
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                 ER(ER_SLAVE_FATAL_ERROR), "Memory allocation failed");
      DBUG_RETURN(1);
    }
    memcpy(tmp_buf,buf,event_len);
    /*
      Create_file constructor wants a 0 as last char of buffer, this 0 will
      serve as the string-termination char for the file's name (which is at the
      end of the buffer)
      We must increment event_len, otherwise the event constructor will not see
      this end 0, which leads to segfault.
    */
    tmp_buf[event_len++]=0;
    int4store(tmp_buf+EVENT_LEN_OFFSET, event_len);
    buf = (const char*)tmp_buf;
  }
  /*
    This will transform LOAD_EVENT into CREATE_FILE_EVENT, ask the master to
    send the loaded file, and write it to the relay log in the form of
    Append_block/Exec_load (the SQL thread needs the data, as that thread is not
    connected to the master).
  */
  Log_event *ev=
    Log_event::read_log_event(buf, event_len, &errmsg,
                              mi->get_mi_description_event(), 0);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free((char*) tmp_buf);
    DBUG_RETURN(1);
  }

  mi->set_master_log_pos(ev->log_pos); /* 3.23 events don't contain log_pos */
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    ignore_event= 1;
    inc_pos= event_len;
    break;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      DBUG_RETURN(1);
    }
    inc_pos= 0;
    break;
  case CREATE_FILE_EVENT:
    /*
      Yes it's possible to have CREATE_FILE_EVENT here, even if we're in
      queue_old_event() which is for 3.23 events which don't comprise
      CREATE_FILE_EVENT. This is because read_log_event() above has just
      transformed LOAD_EVENT into CREATE_FILE_EVENT.
    */
  {
    /* We come here when and only when tmp_buf != 0 */
    DBUG_ASSERT(tmp_buf != 0);
    inc_pos=event_len;
    ev->log_pos+= inc_pos;
    int error = process_io_create_file(mi,(Create_file_log_event*)ev);
    delete ev;
    mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
    DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));
    my_free((char*)tmp_buf);
    DBUG_RETURN(error);
  }
  default:
    inc_pos= event_len;
    break;
  }
  if (likely(!ignore_event))
  {
    if (ev->log_pos)
      /*
         Don't do it for fake Rotate events (see comment in
      Log_event::Log_event(const char* buf...) in log_event.cc).
      */
      ev->log_pos+= event_len; /* make log_pos be the pos of the end of the event */
    if (unlikely(rli->relay_log.append_event(ev, mi) != 0))
    {
      delete ev;
      DBUG_RETURN(1);
    }
    rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  }
  delete ev;
  mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));
  DBUG_RETURN(0);
}

/**
  Reads a 4.0 event and converts it to the slave's format. This code was copied
  from queue_binlog_ver_1_event(), with some affordable simplifications.

  @note The caller must hold mi->data_lock before invoking this function.
*/
static int queue_binlog_ver_3_event(Master_info *mi, const char *buf,
                                    ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  char *tmp_buf = 0;
  Relay_log_info *rli= mi->rli;
  DBUG_ENTER("queue_binlog_ver_3_event");

  mysql_mutex_assert_owner(&mi->data_lock);

  /* read_log_event() will adjust log_pos to be end_log_pos */
  Log_event *ev=
    Log_event::read_log_event(buf, event_len, &errmsg,
                              mi->get_mi_description_event(), 0);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free((char*) tmp_buf);
    DBUG_RETURN(1);
  }
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    goto err;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      DBUG_RETURN(1);
    }
    inc_pos= 0;
    break;
  default:
    inc_pos= event_len;
    break;
  }

  if (unlikely(rli->relay_log.append_event(ev, mi) != 0))
  {
    delete ev;
    DBUG_RETURN(1);
  }
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  delete ev;
  mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
err:
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));
  DBUG_RETURN(0);
}

/*
  queue_old_event()

  Writes a 3.23 or 4.0 event to the relay log, after converting it to the 5.0
  (exactly, slave's) format. To do the conversion, we create a 5.0 event from
  the 3.23/4.0 bytes, then write this event to the relay log.

  TODO:
    Test this code before release - it has to be tested on a separate
    setup with 3.23 master or 4.0 master
*/

static int queue_old_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  DBUG_ENTER("queue_old_event");

  mysql_mutex_assert_owner(&mi->data_lock);

  switch (mi->get_mi_description_event()->binlog_version)
  {
  case 1:
      DBUG_RETURN(queue_binlog_ver_1_event(mi,buf,event_len));
  case 3:
      DBUG_RETURN(queue_binlog_ver_3_event(mi,buf,event_len));
  default: /* unsupported format; eg version 2 */
    DBUG_PRINT("info",("unsupported binlog format %d in queue_old_event()",
                       mi->get_mi_description_event()->binlog_version));
    DBUG_RETURN(1);
  }
}

/*
  queue_event()

  If the event is 3.23/4.0, passes it to queue_old_event() which will convert
  it. Otherwise, writes a 5.0 (or newer) event to the relay log. Then there is
  no format conversion, it's pure read/write of bytes.
  So a 5.0.0 slave's relay log can contain events in the slave's format or in
  any >=5.0.0 format.
*/

static int queue_event(Master_info* mi,const char* buf, ulong event_len)
{
  int error= 0;
  String error_msg;
  ulong inc_pos= 0;
  Relay_log_info *rli= mi->rli;
  mysql_mutex_t *log_lock= rli->relay_log.get_log_lock();
  ulong s_id;
  bool unlock_data_lock= TRUE;
  /*
    FD_q must have been prepared for the first R_a event
    inside get_master_version_and_clock()
    Show-up of FD:s affects checksum_alg at once because
    that changes FD_queue.
  */
  uint8 checksum_alg= mi->checksum_alg_before_fd != BINLOG_CHECKSUM_ALG_UNDEF ? 
    mi->checksum_alg_before_fd :
    mi->rli->relay_log.relay_log_checksum_alg;

  char *save_buf= NULL; // needed for checksumming the fake Rotate event
  char rot_buf[LOG_EVENT_HEADER_LEN + ROTATE_HEADER_LEN + FN_REFLEN];
  Gtid gtid= { 0, 0 };
  Log_event_type event_type= (Log_event_type)buf[EVENT_TYPE_OFFSET];

  DBUG_ASSERT(checksum_alg == BINLOG_CHECKSUM_ALG_OFF || 
              checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF || 
              checksum_alg == BINLOG_CHECKSUM_ALG_CRC32); 

  DBUG_ENTER("queue_event");
  /*
    FD_queue checksum alg description does not apply in a case of
    FD itself. The one carries both parts of the checksum data.
  */
  if (event_type == FORMAT_DESCRIPTION_EVENT)
  {
    checksum_alg= get_checksum_alg(buf, event_len);
  }
  else if (event_type == START_EVENT_V3)
  {
    // checksum behaviour is similar to the pre-checksum FD handling
    mi->checksum_alg_before_fd= BINLOG_CHECKSUM_ALG_UNDEF;
    mysql_mutex_lock(&mi->data_lock);
    mi->get_mi_description_event()->checksum_alg=
      mi->rli->relay_log.relay_log_checksum_alg= checksum_alg=
      BINLOG_CHECKSUM_ALG_OFF;
    mysql_mutex_unlock(&mi->data_lock);
  }

  // does not hold always because of old binlog can work with NM 
  // DBUG_ASSERT(checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);

  // should hold unless manipulations with RL. Tests that do that
  // will have to refine the clause.
  DBUG_ASSERT(mi->rli->relay_log.relay_log_checksum_alg !=
              BINLOG_CHECKSUM_ALG_UNDEF);
              
  // Emulate the network corruption
  DBUG_EXECUTE_IF("corrupt_queue_event",
    if (event_type != FORMAT_DESCRIPTION_EVENT)
    {
      char *debug_event_buf_c = (char*) buf;
      int debug_cor_pos = rand() % (event_len - BINLOG_CHECKSUM_LEN);
      debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
      DBUG_PRINT("info", ("Corrupt the event at queue_event: byte on position %d", debug_cor_pos));
      DBUG_SET("");
    }
  );
                                              
  if (event_checksum_test((uchar *) buf, event_len, checksum_alg))
  {
    error= ER_NETWORK_READ_EVENT_CHECKSUM_FAILURE;
    unlock_data_lock= FALSE;
    goto err;
  }

  mysql_mutex_lock(&mi->data_lock);

  if (mi->get_mi_description_event()->binlog_version < 4 &&
      event_type != FORMAT_DESCRIPTION_EVENT /* a way to escape */)
  {
    int ret= queue_old_event(mi,buf,event_len);
    mysql_mutex_unlock(&mi->data_lock);
    DBUG_RETURN(ret);
  }

  switch (event_type) {
  case STOP_EVENT:
    /*
      We needn't write this event to the relay log. Indeed, it just indicates a
      master server shutdown. The only thing this does is cleaning. But
      cleaning is already done on a per-master-thread basis (as the master
      server is shutting down cleanly, it has written all DROP TEMPORARY TABLE
      prepared statements' deletion are TODO only when we binlog prep stmts).

      We don't even increment mi->get_master_log_pos(), because we may be just after
      a Rotate event. Btw, in a few milliseconds we are going to have a Start
      event from the next binlog (unless the master is presently running
      without --log-bin).
    */
    goto err;
  case ROTATE_EVENT:
  {
    Rotate_log_event rev(buf, checksum_alg != BINLOG_CHECKSUM_ALG_OFF ?
                         event_len - BINLOG_CHECKSUM_LEN : event_len,
                         mi->get_mi_description_event());

    if (unlikely(process_io_rotate(mi, &rev)))
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
      goto err;
    }
    /* 
       Checksum special cases for the fake Rotate (R_f) event caused by the protocol
       of events generation and serialization in RL where Rotate of master is 
       queued right next to FD of slave.
       Since it's only FD that carries the alg desc of FD_s has to apply to R_m.
       Two special rules apply only to the first R_f which comes in before any FD_m.
       The 2nd R_f should be compatible with the FD_s that must have taken over
       the last seen FD_m's (A).
       
       RSC_1: If OM \and fake Rotate \and slave is configured to
              to compute checksum for its first FD event for RL
              the fake Rotate gets checksummed here.
    */
    if (uint4korr(&buf[0]) == 0 && checksum_alg == BINLOG_CHECKSUM_ALG_OFF &&
        mi->rli->relay_log.relay_log_checksum_alg != BINLOG_CHECKSUM_ALG_OFF)
    {
      ha_checksum rot_crc= my_checksum(0L, NULL, 0);
      event_len += BINLOG_CHECKSUM_LEN;
      memcpy(rot_buf, buf, event_len - BINLOG_CHECKSUM_LEN);
      int4store(&rot_buf[EVENT_LEN_OFFSET],
                uint4korr(rot_buf + EVENT_LEN_OFFSET) + BINLOG_CHECKSUM_LEN);
      rot_crc= my_checksum(rot_crc, (const uchar *) rot_buf,
                           event_len - BINLOG_CHECKSUM_LEN);
      int4store(&rot_buf[event_len - BINLOG_CHECKSUM_LEN], rot_crc);
      DBUG_ASSERT(event_len == uint4korr(&rot_buf[EVENT_LEN_OFFSET]));
      DBUG_ASSERT(mi->get_mi_description_event()->checksum_alg ==
                  mi->rli->relay_log.relay_log_checksum_alg);
      /* the first one */
      DBUG_ASSERT(mi->checksum_alg_before_fd != BINLOG_CHECKSUM_ALG_UNDEF);
      save_buf= (char *) buf;
      buf= rot_buf;
    }
    else
      /*
        RSC_2: If NM \and fake Rotate \and slave does not compute checksum
        the fake Rotate's checksum is stripped off before relay-logging.
      */
      if (uint4korr(&buf[0]) == 0 && checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
          mi->rli->relay_log.relay_log_checksum_alg == BINLOG_CHECKSUM_ALG_OFF)
      {
        event_len -= BINLOG_CHECKSUM_LEN;
        memcpy(rot_buf, buf, event_len);
        int4store(&rot_buf[EVENT_LEN_OFFSET],
                  uint4korr(rot_buf + EVENT_LEN_OFFSET) - BINLOG_CHECKSUM_LEN);
        DBUG_ASSERT(event_len == uint4korr(&rot_buf[EVENT_LEN_OFFSET]));
        DBUG_ASSERT(mi->get_mi_description_event()->checksum_alg ==
                    mi->rli->relay_log.relay_log_checksum_alg);
        /* the first one */
        DBUG_ASSERT(mi->checksum_alg_before_fd != BINLOG_CHECKSUM_ALG_UNDEF);
        save_buf= (char *) buf;
        buf= rot_buf;
      }
    /*
      Now the I/O thread has just changed its mi->get_master_log_name(), so
      incrementing mi->get_master_log_pos() is nonsense.
    */
    inc_pos= 0;
    break;
  }
  case FORMAT_DESCRIPTION_EVENT:
  {
    /*
      Create an event, and save it (when we rotate the relay log, we will have
      to write this event again).
    */
    /*
      We are the only thread which reads/writes mi_description_event.
      The relay_log struct does not move (though some members of it can
      change), so we needn't any lock (no rli->data_lock, no log lock).
    */
    const char* errmsg;
    // mark it as undefined that is irrelevant anymore
    mi->checksum_alg_before_fd= BINLOG_CHECKSUM_ALG_UNDEF;
    Format_description_log_event *new_fdle=
      (Format_description_log_event*)
      Log_event::read_log_event(buf, event_len, &errmsg,
                                mi->get_mi_description_event(), 1);
    if (new_fdle == NULL)
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
      goto err;
    }
    if (new_fdle->checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF)
      new_fdle->checksum_alg= BINLOG_CHECKSUM_ALG_OFF;
    mi->set_mi_description_event(new_fdle);

    /* installing new value of checksum Alg for relay log */
    mi->rli->relay_log.relay_log_checksum_alg= new_fdle->checksum_alg;

    /*
       Though this does some conversion to the slave's format, this will
       preserve the master's binlog format version, and number of event types.
    */
    /*
       If the event was not requested by the slave (the slave did not ask for
       it), i.e. has end_log_pos=0, we do not increment mi->get_master_log_pos()
    */
    inc_pos= uint4korr(buf+LOG_POS_OFFSET) ? event_len : 0;
    DBUG_PRINT("info",("binlog format is now %d",
                       mi->get_mi_description_event()->binlog_version));

  }
  break;

  case HEARTBEAT_LOG_EVENT:
  {
    /*
      HB (heartbeat) cannot come before RL (Relay)
    */
    char  llbuf[22];
    Heartbeat_log_event hb(buf,
                           mi->rli->relay_log.relay_log_checksum_alg
                           != BINLOG_CHECKSUM_ALG_OFF ?
                           event_len - BINLOG_CHECKSUM_LEN : event_len,
                           mi->get_mi_description_event());
    if (!hb.is_valid())
    {
      error= ER_SLAVE_HEARTBEAT_FAILURE;
      error_msg.append(STRING_WITH_LEN("inconsistent heartbeat event content;"));
      error_msg.append(STRING_WITH_LEN("the event's data: log_file_name "));
      error_msg.append(hb.get_log_ident(), (uint) strlen(hb.get_log_ident()));
      error_msg.append(STRING_WITH_LEN(" log_pos "));
      llstr(hb.log_pos, llbuf);
      error_msg.append(llbuf, strlen(llbuf));
      goto err;
    }
    mi->received_heartbeats++;
    mi->last_heartbeat= my_time(0);


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
    if (mi->is_auto_position()  && mi->get_master_log_pos() < hb.log_pos
        &&  mi->get_master_log_name() != NULL)
    {

      DBUG_ASSERT(memcmp(const_cast<char*>(mi->get_master_log_name()),
                         hb.get_log_ident(), hb.get_ident_len()) == 0);

      mi->set_master_log_pos(hb.log_pos);

      /*
         Put this heartbeat event in the relay log as a Rotate Event.
      */
      inc_pos= 0;
      memcpy(rli->ign_master_log_name_end, mi->get_master_log_name(),
             FN_REFLEN);
      rli->ign_master_log_pos_end = mi->get_master_log_pos();

      if (write_ignored_events_info_to_relay_log(mi->info_thd, mi))
        goto err;
    }

    /* 
       compare local and event's versions of log_file, log_pos.
       
       Heartbeat is sent only after an event corresponding to the corrdinates
       the heartbeat carries.
       Slave can not have a difference in coordinates except in the only
       special case when mi->get_master_log_name(), mi->get_master_log_pos() have never
       been updated by Rotate event i.e when slave does not have any history
       with the master (and thereafter mi->get_master_log_pos() is NULL).

       TODO: handling `when' for SHOW SLAVE STATUS' snds behind
    */
    if ((memcmp(const_cast<char *>(mi->get_master_log_name()),
                hb.get_log_ident(), hb.get_ident_len())
         && mi->get_master_log_name() != NULL)
        || ((mi->get_master_log_pos() != hb.log_pos && gtid_mode == 0) || 
            /*
              When Gtid mode is on only monotocity can be claimed.
              Todo: enhance HB event with the skipped events size
              and to convert HB.pos  == MI.pos to HB.pos - HB.skip_size == MI.pos
            */
            (mi->get_master_log_pos() > hb.log_pos)))
    {
      /* missed events of heartbeat from the past */
      error= ER_SLAVE_HEARTBEAT_FAILURE;
      error_msg.append(STRING_WITH_LEN("heartbeat is not compatible with local info;"));
      error_msg.append(STRING_WITH_LEN("the event's data: log_file_name "));
      error_msg.append(hb.get_log_ident(), (uint) strlen(hb.get_log_ident()));
      error_msg.append(STRING_WITH_LEN(" log_pos "));
      llstr(hb.log_pos, llbuf);
      error_msg.append(llbuf, strlen(llbuf));
      goto err;
    }
    goto skip_relay_logging;
  }
  break;

  case PREVIOUS_GTIDS_LOG_EVENT:
  {
    if (gtid_mode == 0)
    {
      error= ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF;
      goto err;
    }
    /*
      This event does not have any meaning for the slave and
      was just sent to show the slave the master is making
      progress and avoid possible deadlocks.
      So at this point, the event is replaced by a rotate
      event what will make the slave to update what it knows
      about the master's coordinates.
    */
    inc_pos= 0;
    mi->set_master_log_pos(mi->get_master_log_pos() + event_len);
    memcpy(rli->ign_master_log_name_end, mi->get_master_log_name(), FN_REFLEN);
    rli->ign_master_log_pos_end= mi->get_master_log_pos();

    if (write_ignored_events_info_to_relay_log(mi->info_thd, mi))
      goto err;

    goto skip_relay_logging;
  }
  break;

  case GTID_LOG_EVENT:
  {
    if (gtid_mode == 0)
    {
      error= ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF;
      goto err;
    }
    global_sid_lock->rdlock();
    Gtid_log_event gtid_ev(buf, checksum_alg != BINLOG_CHECKSUM_ALG_OFF ?
                           event_len - BINLOG_CHECKSUM_LEN : event_len,
                           mi->get_mi_description_event());
    gtid.sidno= gtid_ev.get_sidno(false);
    global_sid_lock->unlock();
    if (gtid.sidno < 0)
      goto err;
    gtid.gno= gtid_ev.get_gno();
    inc_pos= event_len;
  }
  break;

  case ANONYMOUS_GTID_LOG_EVENT:

  default:
    inc_pos= event_len;
  break;
  }

  /*
     If this event is originating from this server, don't queue it.
     We don't check this for 3.23 events because it's simpler like this; 3.23
     will be filtered anyway by the SQL slave thread which also tests the
     server id (we must also keep this test in the SQL thread, in case somebody
     upgrades a 4.0 slave which has a not-filtered relay log).

     ANY event coming from ourselves can be ignored: it is obvious for queries;
     for STOP_EVENT/ROTATE_EVENT/START_EVENT: these cannot come from ourselves
     (--log-slave-updates would not log that) unless this slave is also its
     direct master (an unsupported, useless setup!).
  */

  mysql_mutex_lock(log_lock);
  s_id= uint4korr(buf + SERVER_ID_OFFSET);

  /*
    If server_id_bits option is set we need to mask out irrelevant bits
    when checking server_id, but we still put the full unmasked server_id
    into the Relay log so that it can be accessed when applying the event
  */
  s_id&= opt_server_id_mask;

  if ((s_id == ::server_id && !mi->rli->replicate_same_server_id) ||
      /*
        the following conjunction deals with IGNORE_SERVER_IDS, if set
        If the master is on the ignore list, execution of
        format description log events and rotate events is necessary.
      */
      (mi->ignore_server_ids->dynamic_ids.elements > 0 &&
       mi->shall_ignore_server_id(s_id) &&
       /* everything is filtered out from non-master */
       (s_id != mi->master_id ||
        /* for the master meta information is necessary */
        (event_type != FORMAT_DESCRIPTION_EVENT &&
         event_type != ROTATE_EVENT))))
  {
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
        (event_type != FORMAT_DESCRIPTION_EVENT &&
         event_type != ROTATE_EVENT &&
         event_type != STOP_EVENT))
    {
      mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
      memcpy(rli->ign_master_log_name_end, mi->get_master_log_name(), FN_REFLEN);
      DBUG_ASSERT(rli->ign_master_log_name_end[0]);
      rli->ign_master_log_pos_end= mi->get_master_log_pos();
    }
    rli->relay_log.signal_update(); // the slave SQL thread needs to re-check
    DBUG_PRINT("info", ("master_log_pos: %lu, event originating from %u server, ignored",
                        (ulong) mi->get_master_log_pos(), uint4korr(buf + SERVER_ID_OFFSET)));
  }
  else
  {
    /* write the event to the relay log */
    if (likely(rli->relay_log.append_buffer(buf, event_len, mi) == 0))
    {
      mi->set_master_log_pos(mi->get_master_log_pos() + inc_pos);
      DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);

      if (event_type == GTID_LOG_EVENT)
      {
        global_sid_lock->rdlock();
        int ret= rli->add_logged_gtid(gtid.sidno, gtid.gno);
        global_sid_lock->unlock();
        if (ret != 0)
          goto err;
      }
    }
    else
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
    }
    rli->ign_master_log_name_end[0]= 0; // last event is not ignored
    if (save_buf != NULL)
      buf= save_buf;
  }
  mysql_mutex_unlock(log_lock);

skip_relay_logging:
  
err:
  if (unlock_data_lock)
    mysql_mutex_unlock(&mi->data_lock);
  DBUG_PRINT("info", ("error: %d", error));
  if (error)
    mi->report(ERROR_LEVEL, error, ER(error), 
               (error == ER_SLAVE_RELAY_LOG_WRITE_FAILURE)?
               "could not queue event from master" :
               error_msg.ptr());
  DBUG_RETURN(error);
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

extern "C" void slave_io_thread_detach_vio()
{
#ifdef SIGNAL_WITH_VIO_CLOSE
  THD *thd= current_thd;
  if (thd && thd->slave_thread)
    thd->clear_active_vio();
#endif
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

static int safe_connect(THD* thd, MYSQL* mysql, Master_info* mi)
{
  DBUG_ENTER("safe_connect");

  DBUG_RETURN(connect_to_master(thd, mysql, mi, 0, 0));
}


/*
  SYNPOSIS
    connect_to_master()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    mi->retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, Master_info* mi,
                             bool reconnect, bool suppress_warnings)
{
  int slave_was_killed= 0;
  int last_errno= -2;                           // impossible error
  ulong err_count=0;
  char llbuff[22];
  char password[MAX_PASSWORD_LENGTH + 1];
  int password_size= sizeof(password);
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  mi->events_until_exit = disconnect_slave_event_count;
#endif
  ulong client_flag= CLIENT_REMEMBER_OPTIONS;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;                /* We will use compression */

  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);

  if (mi->bind_addr[0])
  {
    DBUG_PRINT("info",("bind_addr: %s", mi->bind_addr));
    mysql_options(mysql, MYSQL_OPT_BIND, mi->bind_addr);
  }

#ifdef HAVE_OPENSSL
  if (mi->ssl)
  {
    mysql_ssl_set(mysql,
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0,
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
    mysql_options(mysql, MYSQL_OPT_SSL_CRL, 
                  mi->ssl_crl[0] ? mi->ssl_crl : 0);
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, 
                  mi->ssl_crlpath[0] ? mi->ssl_crlpath : 0);
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &mi->ssl_verify_server_cert);
  }
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

  if (mi->is_start_plugin_auth_configured())
  {
    DBUG_PRINT("info", ("Slaving is using MYSQL_DEFAULT_AUTH %s",
                        mi->get_start_plugin_auth()));
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, mi->get_start_plugin_auth());
  }
  
  if (mi->is_start_plugin_dir_configured())
  {
    DBUG_PRINT("info", ("Slaving is using MYSQL_PLUGIN_DIR %s",
                        mi->get_start_plugin_dir()));
    mysql_options(mysql, MYSQL_PLUGIN_DIR, mi->get_start_plugin_dir());
  }
  /* Set MYSQL_PLUGIN_DIR in case master asks for an external authentication plugin */
  else if (opt_plugin_dir_ptr && *opt_plugin_dir_ptr)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir_ptr);
  
  if (!mi->is_start_user_configured())
    sql_print_warning("%s", ER(ER_INSECURE_CHANGE_MASTER));

  if (mi->get_password(password, &password_size))
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR),
               "Unable to configure password when attempting to "
               "connect to the master server. Connection attempt "
               "terminated.");
    DBUG_RETURN(1);
  }

  const char* user= mi->get_user();
  if (user == NULL || user[0] == 0)
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR),
               "Invalid (empty) username when attempting to "
               "connect to the master server. Connection attempt "
               "terminated.");
    DBUG_RETURN(1);
  }

  while (!(slave_was_killed = io_slave_killed(thd,mi))
         && (reconnect ? mysql_reconnect(mysql) != 0 :
             mysql_real_connect(mysql, mi->host, user,
                                password, 0, mi->port, 0, client_flag) == 0))
  {
    /*
       SHOW SLAVE STATUS will display the number of retries which
       would be real retry counts instead of mi->retry_count for
       each connection attempt by 'Last_IO_Error' entry.
    */
    last_errno=mysql_errno(mysql);
    suppress_warnings= 0;
    mi->report(ERROR_LEVEL, last_errno,
               "error %s to master '%s@%s:%d'"
               " - retry-time: %d  retries: %lu",
               (reconnect ? "reconnecting" : "connecting"),
               mi->get_user(), mi->host, mi->port,
               mi->connect_retry, err_count + 1);
    /*
      By default we try forever. The reason is that failure will trigger
      master election, so if the user did not set mi->retry_count we
      do not want to have election triggered on the first failure to
      connect
    */
    if (++err_count == mi->retry_count)
    {
      slave_was_killed=1;
      break;
    }
    slave_sleep(thd, mi->connect_retry, io_slave_killed, mi);
  }

  if (!slave_was_killed)
  {
    mi->clear_error(); // clear possible left over reconnect error
    if (reconnect)
    {
      if (!suppress_warnings && log_warnings)
        sql_print_information("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->get_user(),
                        mi->host, mi->port,
                        mi->get_io_rpl_log_name(),
                        llstr(mi->get_master_log_pos(),llbuff));
    }
    else
    {
      general_log_print(thd, COM_CONNECT_OUT, "%s@%s:%d",
                        mi->get_user(), mi->host, mi->port);
    }
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif
  }
  mysql->reconnect= 1;
  DBUG_PRINT("exit",("slave_was_killed: %d", slave_was_killed));
  DBUG_RETURN(slave_was_killed);
}


/*
  safe_reconnect()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    mi->retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, Master_info* mi,
                          bool suppress_warnings)
{
  DBUG_ENTER("safe_reconnect");
  DBUG_RETURN(connect_to_master(thd, mysql, mi, 1, suppress_warnings));
}


MYSQL *rpl_connect_master(MYSQL *mysql)
{
  THD *thd= current_thd;
  char password[MAX_PASSWORD_LENGTH + 1];
  int password_size= sizeof(password);
  Master_info *mi= my_pthread_getspecific_ptr(Master_info*, RPL_MASTER_INFO);
  if (!mi)
  {
    sql_print_error("'rpl_connect_master' must be called in slave I/O thread context.");
    return NULL;
  }

  bool allocated= false;
  
  if (!mysql)
  {
    if(!(mysql= mysql_init(NULL)))
    {
      sql_print_error("rpl_connect_master: failed in mysql_init()");
      return NULL;
    }
    allocated= true;
  }

  /*
    XXX: copied from connect_to_master, this function should not
    change the slave status, so we cannot use connect_to_master
    directly
    
    TODO: make this part a seperate function to eliminate duplication
  */
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);

  if (mi->bind_addr[0])
  {
    DBUG_PRINT("info",("bind_addr: %s", mi->bind_addr));
    mysql_options(mysql, MYSQL_OPT_BIND, mi->bind_addr);
  }

#ifdef HAVE_OPENSSL
  if (mi->ssl)
  {
    mysql_ssl_set(mysql,
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0,
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
    mysql_options(mysql, MYSQL_OPT_SSL_CRL, 
                  mi->ssl_crl[0] ? mi->ssl_crl : 0);
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, 
                  mi->ssl_crlpath[0] ? mi->ssl_crlpath : 0);
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &mi->ssl_verify_server_cert);
  }
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

  if (mi->is_start_plugin_auth_configured())
  {
    DBUG_PRINT("info", ("Slaving is using MYSQL_DEFAULT_AUTH %s",
                        mi->get_start_plugin_auth()));
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, mi->get_start_plugin_auth());
  }

  if (mi->is_start_plugin_dir_configured())
  {
    DBUG_PRINT("info", ("Slaving is using MYSQL_PLUGIN_DIR %s",
                        mi->get_start_plugin_dir()));
    mysql_options(mysql, MYSQL_PLUGIN_DIR, mi->get_start_plugin_dir());
  }
  /* Set MYSQL_PLUGIN_DIR in case master asks for an external authentication plugin */
  else if (opt_plugin_dir_ptr && *opt_plugin_dir_ptr)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir_ptr);

  if (!mi->is_start_user_configured())
    sql_print_warning("%s", ER(ER_INSECURE_CHANGE_MASTER));

  const char *user= mi->get_user();
  if (user == NULL
      || user[0] == 0
      || mi->get_password(password, &password_size)
      || io_slave_killed(thd, mi)
      || !mysql_real_connect(mysql, mi->host, user,
                             password, 0, mi->port, 0, 0))
  {
    if (!io_slave_killed(thd, mi))
      sql_print_error("rpl_connect_master: error connecting to master: %s (server_error: %d)",
                      mysql_error(mysql), mysql_errno(mysql));
    
    if (allocated)
      mysql_close(mysql);                       // this will free the object
    return NULL;
  }
  return mysql;
}

/*
  Called when we notice that the current "hot" log got rotated under our feet.
*/

static IO_CACHE *reopen_relay_log(Relay_log_info *rli, const char **errmsg)
{
  DBUG_ENTER("reopen_relay_log");
  DBUG_ASSERT(rli->cur_log != &rli->cache_buf);
  DBUG_ASSERT(rli->cur_log_fd == -1);

  IO_CACHE *cur_log = rli->cur_log=&rli->cache_buf;
  if ((rli->cur_log_fd=open_binlog_file(cur_log,rli->get_event_relay_log_name(),
                                        errmsg)) <0)
    DBUG_RETURN(0);
  /*
    We want to start exactly where we was before:
    relay_log_pos       Current log pos
    pending             Number of bytes already processed from the event
  */
  rli->set_event_relay_log_pos(max<ulonglong>(rli->get_event_relay_log_pos(),
                                              BIN_LOG_HEADER_SIZE));
  my_b_seek(cur_log,rli->get_event_relay_log_pos());
  DBUG_RETURN(cur_log);
}


/**
  Reads next event from the relay log.  Should be called from the
  slave IO thread.

  @param rli Relay_log_info structure for the slave IO thread.

  @return The event read, or NULL on error.  If an error occurs, the
  error is reported through the sql_print_information() or
  sql_print_error() functions.
*/
static Log_event* next_event(Relay_log_info* rli)
{
  Log_event* ev;
  IO_CACHE* cur_log = rli->cur_log;
  mysql_mutex_t *log_lock = rli->relay_log.get_log_lock();
  const char* errmsg=0;
  THD* thd = rli->info_thd;
  DBUG_ENTER("next_event");

  DBUG_ASSERT(thd != 0);

#ifndef DBUG_OFF
  if (abort_slave_event_count && !rli->events_until_exit--)
    DBUG_RETURN(0);
#endif

  /*
    For most operations we need to protect rli members with data_lock,
    so we assume calling function acquired this mutex for us and we will
    hold it for the most of the loop below However, we will release it
    whenever it is worth the hassle,  and in the cases when we go into a
    mysql_cond_wait() with the non-data_lock mutex
  */
  mysql_mutex_assert_owner(&rli->data_lock);

  while (!sql_slave_killed(thd,rli))
  {
    /*
      We can have two kinds of log reading:
      hot_log:
        rli->cur_log points at the IO_CACHE of relay_log, which
        is actively being updated by the I/O thread. We need to be careful
        in this case and make sure that we are not looking at a stale log that
        has already been rotated. If it has been, we reopen the log.

      The other case is much simpler:
        We just have a read only log that nobody else will be updating.
    */
    bool hot_log;
    if ((hot_log = (cur_log != &rli->cache_buf)))
    {
      DBUG_ASSERT(rli->cur_log_fd == -1); // foreign descriptor
      mysql_mutex_lock(log_lock);

      /*
        Reading xxx_file_id is safe because the log will only
        be rotated when we hold relay_log.LOCK_log
      */
      if (rli->relay_log.get_open_count() != rli->cur_log_old_open_count)
      {
        // The master has switched to a new log file; Reopen the old log file
        cur_log=reopen_relay_log(rli, &errmsg);
        mysql_mutex_unlock(log_lock);
        if (!cur_log)                           // No more log files
          goto err;
        hot_log=0;                              // Using old binary log
      }
    }
    /* 
      As there is no guarantee that the relay is open (for example, an I/O
      error during a write by the slave I/O thread may have closed it), we
      have to test it.
    */
    if (!my_b_inited(cur_log))
      goto err;
#ifndef DBUG_OFF
    {
      DBUG_PRINT("info", ("assertion skip %lu file pos %lu event relay log pos %lu file %s\n",
        (ulong) rli->slave_skip_counter, (ulong) my_b_tell(cur_log),
        (ulong) rli->get_event_relay_log_pos(),
        rli->get_event_relay_log_name()));

      /* This is an assertion which sometimes fails, let's try to track it */
      char llbuf1[22], llbuf2[22];
      DBUG_PRINT("info", ("my_b_tell(cur_log)=%s rli->event_relay_log_pos=%s",
                          llstr(my_b_tell(cur_log),llbuf1),
                          llstr(rli->get_event_relay_log_pos(),llbuf2)));

      DBUG_ASSERT(my_b_tell(cur_log) >= BIN_LOG_HEADER_SIZE);
      DBUG_ASSERT(my_b_tell(cur_log) == rli->get_event_relay_log_pos() || rli->is_parallel_exec());

      DBUG_PRINT("info", ("next_event group master %s %lu group relay %s %lu event %s %lu\n",
        rli->get_group_master_log_name(),
        (ulong) rli->get_group_master_log_pos(),
        rli->get_group_relay_log_name(),
        (ulong) rli->get_group_relay_log_pos(),
        rli->get_event_relay_log_name(),
        (ulong) rli->get_event_relay_log_pos()));
    }
#endif
    /*
      Relay log is always in new format - if the master is 3.23, the
      I/O thread will convert the format for us.
      A problem: the description event may be in a previous relay log. So if
      the slave has been shutdown meanwhile, we would have to look in old relay
      logs, which may even have been deleted. So we need to write this
      description event at the beginning of the relay log.
      When the relay log is created when the I/O thread starts, easy: the
      master will send the description event and we will queue it.
      But if the relay log is created by new_file(): then the solution is:
      MYSQL_BIN_LOG::open() will write the buffered description event.
    */
    if ((ev= Log_event::read_log_event(cur_log, 0,
                                       rli->get_rli_description_event(),
                                       opt_slave_sql_verify_checksum)))
    {
      DBUG_ASSERT(thd==rli->info_thd);
      /*
        read it while we have a lock, to avoid a mutex lock in
        inc_event_relay_log_pos()
      */
      rli->set_future_event_relay_log_pos(my_b_tell(cur_log));
      ev->future_event_relay_log_pos= rli->get_future_event_relay_log_pos();

      if (hot_log)
        mysql_mutex_unlock(log_lock);

      /* 
         MTS checkpoint in the successful read branch 
      */
      bool force= (rli->checkpoint_seqno > (rli->checkpoint_group - 1));
      if (rli->is_parallel_exec() && (opt_mts_checkpoint_period != 0 || force))
      {
        ulonglong period= static_cast<ulonglong>(opt_mts_checkpoint_period * 1000000ULL);
        mysql_mutex_unlock(&rli->data_lock);
        /*
          At this point the coordinator has is delegating jobs to workers and
          the checkpoint routine must be periodically invoked.
        */
        (void) mts_checkpoint_routine(rli, period, force, true/*need_data_lock=true*/); // TODO: ALFRANIO ERROR
        DBUG_ASSERT(!force ||
                    (force && (rli->checkpoint_seqno <= (rli->checkpoint_group - 1))) ||
                    sql_slave_killed(thd, rli));
        mysql_mutex_lock(&rli->data_lock);
      }
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->info_thd);
    if (opt_reckless_slave)                     // For mysql-test
      cur_log->error = 0;
    if (cur_log->error < 0)
    {
      errmsg = "slave SQL thread aborted because of I/O error";
      if (hot_log)
        mysql_mutex_unlock(log_lock);
      goto err;
    }
    if (!cur_log->error) /* EOF */
    {
      /*
        On a hot log, EOF means that there are no more updates to
        process and we must block until I/O thread adds some and
        signals us to continue
      */
      if (hot_log)
      {
        /*
          We say in Seconds_Behind_Master that we have "caught up". Note that
          for example if network link is broken but I/O slave thread hasn't
          noticed it (slave_net_timeout not elapsed), then we'll say "caught
          up" whereas we're not really caught up. Fixing that would require
          internally cutting timeout in smaller pieces in network read, no
          thanks. Another example: SQL has caught up on I/O, now I/O has read
          a new event and is queuing it; the false "0" will exist until SQL
          finishes executing the new event; it will be look abnormal only if
          the events have old timestamps (then you get "many", 0, "many").

          Transient phases like this can be fixed with implemeting
          Heartbeat event which provides the slave the status of the
          master at time the master does not have any new update to send.
          Seconds_Behind_Master would be zero only when master has no
          more updates in binlog for slave. The heartbeat can be sent
          in a (small) fraction of slave_net_timeout. Until it's done
          rli->last_master_timestamp is temporarely (for time of
          waiting for the following event) reset whenever EOF is
          reached.
        */

        /* shows zero while it is sleeping (and until the next event
           is about to be executed).  Note, in MTS case
           Seconds_Behind_Master resetting follows slightly different
           schema where reaching EOF is not enough.  The status
           parameter is updated per some number of processed group of
           events. The number can't be greater than
           @@global.slave_checkpoint_group and anyway SBM updating
           rate does not exceed @@global.slave_checkpoint_period.
           Notice that SBM is set to a new value after processing the
           terminal event (e.g Commit) of a group.  Coordinator resets
           SBM when notices no more groups left neither to read from
           Relay-log nor to process by Workers.
        */
        if (!rli->is_parallel_exec())
          rli->last_master_timestamp= 0;

        DBUG_ASSERT(rli->relay_log.get_open_count() ==
                    rli->cur_log_old_open_count);

        if (rli->ign_master_log_name_end[0])
        {
          /* We generate and return a Rotate, to make our positions advance */
          DBUG_PRINT("info",("seeing an ignored end segment"));
          ev= new Rotate_log_event(rli->ign_master_log_name_end,
                                   0, rli->ign_master_log_pos_end,
                                   Rotate_log_event::DUP_NAME);
          rli->ign_master_log_name_end[0]= 0;
          mysql_mutex_unlock(log_lock);
          if (unlikely(!ev))
          {
            errmsg= "Slave SQL thread failed to create a Rotate event "
              "(out of memory?), SHOW SLAVE STATUS may be inaccurate";
            goto err;
          }
          ev->server_id= 0; // don't be ignored by slave SQL thread
          DBUG_RETURN(ev);
        }

        /*
          We can, and should release data_lock while we are waiting for
          update. If we do not, show slave status will block
        */
        mysql_mutex_unlock(&rli->data_lock);

        /*
          Possible deadlock :
          - the I/O thread has reached log_space_limit
          - the SQL thread has read all relay logs, but cannot purge for some
          reason:
            * it has already purged all logs except the current one
            * there are other logs than the current one but they're involved in
            a transaction that finishes in the current one (or is not finished)
          Solution :
          Wake up the possibly waiting I/O thread, and set a boolean asking
          the I/O thread to temporarily ignore the log_space_limit
          constraint, because we do not want the I/O thread to block because of
          space (it's ok if it blocks for any other reason (e.g. because the
          master does not send anything). Then the I/O thread stops waiting
          and reads one more event and starts honoring log_space_limit again.

          If the SQL thread needs more events to be able to rotate the log (it
          might need to finish the current group first), then it can ask for one
          more at a time. Thus we don't outgrow the relay log indefinitely,
          but rather in a controlled manner, until the next rotate.

          When the SQL thread starts it sets ignore_log_space_limit to false. 
          We should also reset ignore_log_space_limit to 0 when the user does 
          RESET SLAVE, but in fact, no need as RESET SLAVE requires that the slave
          be stopped, and the SQL thread sets ignore_log_space_limit to 0 when
          it stops.
        */
        mysql_mutex_lock(&rli->log_space_lock);

        /* 
          If we have reached the limit of the relay space and we
          are going to sleep, waiting for more events:

          1. If outside a group, SQL thread asks the IO thread 
             to force a rotation so that the SQL thread purges 
             logs next time it processes an event (thus space is
             freed).

          2. If in a group, SQL thread asks the IO thread to 
             ignore the limit and queues yet one more event 
             so that the SQL thread finishes the group and 
             is are able to rotate and purge sometime soon.
         */
        if (rli->log_space_limit && 
            rli->log_space_limit < rli->log_space_total)
        {
          /* force rotation if not in an unfinished group */
          if (!rli->is_parallel_exec())
          {
            rli->sql_force_rotate_relay= !rli->is_in_group();
          }
          else
          {
            rli->sql_force_rotate_relay=
              (rli->mts_group_status != Relay_log_info::MTS_IN_GROUP);
          }
          /* ask for one more event */
          rli->ignore_log_space_limit= true;
        }

        /*
          If the I/O thread is blocked, unblock it.  Ok to broadcast
          after unlock, because the mutex is only destroyed in
          ~Relay_log_info(), i.e. when rli is destroyed, and rli will
          not be destroyed before we exit the present function.
        */
        mysql_mutex_unlock(&rli->log_space_lock);
        mysql_cond_broadcast(&rli->log_space_cond);
        // Note that wait_for_update_relay_log unlocks lock_log !

        if (rli->is_parallel_exec() && (opt_mts_checkpoint_period != 0 ||
            DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0)))
        {
          int ret= 0;
          struct timespec waittime;
          ulonglong period= static_cast<ulonglong>(opt_mts_checkpoint_period * 1000000ULL);
          ulong signal_cnt= rli->relay_log.signal_cnt;

          mysql_mutex_unlock(log_lock);
          do
          {
            /*
              At this point the coordinator has no job to delegate to workers.
              However, workers are executing their assigned jobs and as such
              the checkpoint routine must be periodically invoked.
            */
            (void) mts_checkpoint_routine(rli, period, false, true/*need_data_lock=true*/); // TODO: ALFRANIO ERROR
            mysql_mutex_lock(log_lock);
            // More to the empty relay-log all assigned events done so reset it.
            if (rli->gaq->empty())
              rli->last_master_timestamp= 0;

            if (DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0))
              period= 10000000ULL;

            set_timespec_nsec(waittime, period);
            ret= rli->relay_log.wait_for_update_relay_log(thd, &waittime);
          } while ((ret == ETIMEDOUT || ret == ETIME) /* todo:remove */ &&
                   signal_cnt == rli->relay_log.signal_cnt && !thd->killed);
        }
        else
        {
          rli->relay_log.wait_for_update_relay_log(thd, NULL);
        }
        
        // re-acquire data lock since we released it earlier
        mysql_mutex_lock(&rli->data_lock);
        continue;
      }
      /*
        If the log was not hot, we need to move to the next log in
        sequence. The next log could be hot or cold, we deal with both
        cases separately after doing some common initialization
      */
      end_io_cache(cur_log);
      DBUG_ASSERT(rli->cur_log_fd >= 0);
      mysql_file_close(rli->cur_log_fd, MYF(MY_WME));
      rli->cur_log_fd = -1;

      if (relay_log_purge)
      {
        /*
          purge_first_log will properly set up relay log coordinates in rli.
          If the group's coordinates are equal to the event's coordinates
          (i.e. the relay log was not rotated in the middle of a group),
          we can purge this relay log too.
          We do ulonglong and string comparisons, this may be slow but
          - purging the last relay log is nice (it can save 1GB of disk), so we
          like to detect the case where we can do it, and given this,
          - I see no better detection method
          - purge_first_log is not called that often
        */
        if (rli->relay_log.purge_first_log
            (rli,
             rli->get_group_relay_log_pos() == rli->get_event_relay_log_pos()
             && !strcmp(rli->get_group_relay_log_name(),rli->get_event_relay_log_name())))
        {
          errmsg = "Error purging processed logs";
          goto err;
        }
        DBUG_PRINT("info", ("next_event group master %s %lu  group relay %s %lu event %s %lu\n",
          rli->get_group_master_log_name(),
          (ulong) rli->get_group_master_log_pos(),
          rli->get_group_relay_log_name(),
          (ulong) rli->get_group_relay_log_pos(),
          rli->get_event_relay_log_name(),
          (ulong) rli->get_event_relay_log_pos()));
      }
      else
      {
        /*
          If hot_log is set, then we already have a lock on
          LOCK_log.  If not, we have to get the lock.

          According to Sasha, the only time this code will ever be executed
          is if we are recovering from a bug.
        */
        if (rli->relay_log.find_next_log(&rli->linfo, !hot_log))
        {
          errmsg = "error switching to the next log";
          goto err;
        }
        rli->set_event_relay_log_pos(BIN_LOG_HEADER_SIZE);
        rli->set_event_relay_log_name(rli->linfo.log_file_name);
        /*
          We may update the worker here but this is not extremlly
          necessary. /Alfranio
        */
        rli->flush_info();
      }

      /* Reset the relay-log-change-notified status of  Slave Workers */
      if (rli->is_parallel_exec())
      {
        DBUG_PRINT("info", ("next_event: MTS group relay log changes to %s %lu\n",
                            rli->get_group_relay_log_name(),
                            (ulong) rli->get_group_relay_log_pos()));
        rli->reset_notified_relay_log_change();
      }

      /*
        Now we want to open this next log. To know if it's a hot log (the one
        being written by the I/O thread now) or a cold log, we can use
        is_active(); if it is hot, we use the I/O cache; if it's cold we open
        the file normally. But if is_active() reports that the log is hot, this
        may change between the test and the consequence of the test. So we may
        open the I/O cache whereas the log is now cold, which is nonsense.
        To guard against this, we need to have LOCK_log.
      */

      DBUG_PRINT("info",("hot_log: %d",hot_log));
      if (!hot_log) /* if hot_log, we already have this mutex */
        mysql_mutex_lock(log_lock);
      if (rli->relay_log.is_active(rli->linfo.log_file_name))
      {
#ifdef EXTRA_DEBUG
        if (log_warnings)
          sql_print_information("next log '%s' is currently active",
                                rli->linfo.log_file_name);
#endif
        rli->cur_log= cur_log= rli->relay_log.get_log_file();
        rli->cur_log_old_open_count= rli->relay_log.get_open_count();
        DBUG_ASSERT(rli->cur_log_fd == -1);

        /*
           When the SQL thread is [stopped and] (re)started the
           following may happen:

           1. Log was hot at stop time and remains hot at restart

              SQL thread reads again from hot_log (SQL thread was
              reading from the active log when it was stopped and the
              very same log is still active on SQL thread restart).

              In this case, my_b_seek is performed on cur_log, while
              cur_log points to relay_log.get_log_file();

           2. Log was hot at stop time but got cold before restart

              The log was hot when SQL thread stopped, but it is not
              anymore when the SQL thread restarts.

              In this case, the SQL thread reopens the log, using
              cache_buf, ie, cur_log points to &cache_buf, and thence
              its coordinates are reset.

           3. Log was already cold at stop time

              The log was not hot when the SQL thread stopped, and, of
              course, it will not be hot when it restarts.

              In this case, the SQL thread opens the cold log again,
              using cache_buf, ie, cur_log points to &cache_buf, and
              thence its coordinates are reset.

           4. Log was hot at stop time, DBA changes to previous cold
              log and restarts SQL thread

              The log was hot when the SQL thread was stopped, but the
              user changed the coordinates of the SQL thread to
              restart from a previous cold log.

              In this case, at start time, cur_log points to a cold
              log, opened using &cache_buf as cache, and coordinates
              are reset. However, as it moves on to the next logs, it
              will eventually reach the hot log. If the hot log is the
              same at the time the SQL thread was stopped, then
              coordinates were not reset - the cur_log will point to
              relay_log.get_log_file(), and not a freshly opened
              IO_CACHE through cache_buf. For this reason we need to
              deploy a my_b_seek before calling check_binlog_magic at
              this point of the code (see: BUG#55263 for more
              details).
          
          NOTES: 
            - We must keep the LOCK_log to read the 4 first bytes, as
              this is a hot log (same as when we call read_log_event()
              above: for a hot log we take the mutex).

            - Because of scenario #4 above, we need to have a
              my_b_seek here. Otherwise, we might hit the assertion
              inside check_binlog_magic.
        */

        my_b_seek(cur_log, (my_off_t) 0);
        if (check_binlog_magic(cur_log,&errmsg))
        {
          if (!hot_log)
            mysql_mutex_unlock(log_lock);
          goto err;
        }
        if (!hot_log)
          mysql_mutex_unlock(log_lock);
        continue;
      }
      if (!hot_log)
        mysql_mutex_unlock(log_lock);
      /*
        if we get here, the log was not hot, so we will have to open it
        ourselves. We are sure that the log is still not hot now (a log can get
        from hot to cold, but not from cold to hot). No need for LOCK_log.
      */
#ifdef EXTRA_DEBUG
      if (log_warnings)
        sql_print_information("next log '%s' is not active",
                              rli->linfo.log_file_name);
#endif
      // open_binlog_file() will check the magic header
      if ((rli->cur_log_fd=open_binlog_file(cur_log,rli->linfo.log_file_name,
                                            &errmsg)) <0)
        goto err;
    }
    else
    {
      /*
        Read failed with a non-EOF error.
        TODO: come up with something better to handle this error
      */
      if (hot_log)
        mysql_mutex_unlock(log_lock);
      sql_print_error("Slave SQL thread: I/O error reading \
event(errno: %d  cur_log->error: %d)",
                      my_errno,cur_log->error);
      // set read position to the beginning of the event
      my_b_seek(cur_log,rli->get_event_relay_log_pos());
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;                                    // To end of function
    }
  }
  if (!errmsg && log_warnings)
  {
    sql_print_information("Error reading relay log event: %s",
                          "slave SQL thread was killed");
    DBUG_RETURN(0);
  }

err:
  if (errmsg)
    sql_print_error("Error reading relay log event: %s", errmsg);
  DBUG_RETURN(0);
}

/*
  Rotate a relay log (this is used only by FLUSH LOGS; the automatic rotation
  because of size is simpler because when we do it we already have all relevant
  locks; here we don't, so this function is mainly taking locks).
  Returns nothing as we cannot catch any error (MYSQL_BIN_LOG::new_file()
  is void).
*/

int rotate_relay_log(Master_info* mi)
{
  DBUG_ENTER("rotate_relay_log");

  mysql_mutex_assert_owner(&mi->data_lock);
  DBUG_EXECUTE_IF("crash_before_rotate_relaylog", DBUG_SUICIDE(););

  Relay_log_info* rli= mi->rli;
  int error= 0;

  /*
     We need to test inited because otherwise, new_file() will attempt to lock
     LOCK_log, which may not be inited (if we're not a slave).
  */
  if (!rli->inited)
  {
    DBUG_PRINT("info", ("rli->inited == 0"));
    goto end;
  }

  /* If the relay log is closed, new_file() will do nothing. */
  error= rli->relay_log.new_file(mi->get_mi_description_event());
  if (error != 0)
    goto end;

  /*
    We harvest now, because otherwise BIN_LOG_HEADER_SIZE will not immediately
    be counted, so imagine a succession of FLUSH LOGS  and assume the slave
    threads are started:
    relay_log_space decreases by the size of the deleted relay log, but does
    not increase, so flush-after-flush we may become negative, which is wrong.
    Even if this will be corrected as soon as a query is replicated on the
    slave (because the I/O thread will then call harvest_bytes_written() which
    will harvest all these BIN_LOG_HEADER_SIZE we forgot), it may give strange
    output in SHOW SLAVE STATUS meanwhile. So we harvest now.
    If the log is closed, then this will just harvest the last writes, probably
    0 as they probably have been harvested.
  */
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
end:
  DBUG_RETURN(error);
}


/**
   Detects, based on master's version (as found in the relay log), if master
   has a certain bug.
   @param rli Relay_log_info which tells the master's version
   @param bug_id Number of the bug as found in bugs.mysql.com
   @param report bool report error message, default TRUE

   @param pred Predicate function that will be called with @c param to
   check for the bug. If the function return @c true, the bug is present,
   otherwise, it is not.

   @param param  State passed to @c pred function.

   @return TRUE if master has the bug, FALSE if it does not.
*/
bool rpl_master_has_bug(const Relay_log_info *rli, uint bug_id, bool report,
                        bool (*pred)(const void *), const void *param)
{
  struct st_version_range_for_one_bug {
    uint        bug_id;
    const uchar introduced_in[3]; // first version with bug
    const uchar fixed_in[3];      // first version with fix
  };
  static struct st_version_range_for_one_bug versions_for_all_bugs[]=
  {
    {24432, { 5, 0, 24 }, { 5, 0, 38 } },
    {24432, { 5, 1, 12 }, { 5, 1, 17 } },
    {33029, { 5, 0,  0 }, { 5, 0, 58 } },
    {33029, { 5, 1,  0 }, { 5, 1, 12 } },
    {37426, { 5, 1,  0 }, { 5, 1, 26 } },
  };
  const uchar *master_ver=
    rli->get_rli_description_event()->server_version_split;

  DBUG_ASSERT(sizeof(rli->get_rli_description_event()->server_version_split) == 3);

  for (uint i= 0;
       i < sizeof(versions_for_all_bugs)/sizeof(*versions_for_all_bugs);i++)
  {
    const uchar *introduced_in= versions_for_all_bugs[i].introduced_in,
      *fixed_in= versions_for_all_bugs[i].fixed_in;
    if ((versions_for_all_bugs[i].bug_id == bug_id) &&
        (memcmp(introduced_in, master_ver, 3) <= 0) &&
        (memcmp(fixed_in,      master_ver, 3) >  0) &&
        (pred == NULL || (*pred)(param)))
    {
      if (!report)
	return TRUE;
      // a short message for SHOW SLAVE STATUS (message length constraints)
      my_printf_error(ER_UNKNOWN_ERROR, "master may suffer from"
                      " http://bugs.mysql.com/bug.php?id=%u"
                      " so slave stops; check error log on slave"
                      " for more info", MYF(0), bug_id);
      // a verbose message for the error log
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "According to the master's version ('%s'),"
                  " it is probable that master suffers from this bug:"
                  " http://bugs.mysql.com/bug.php?id=%u"
                  " and thus replicating the current binary log event"
                  " may make the slave's data become different from the"
                  " master's data."
                  " To take no risk, slave refuses to replicate"
                  " this event and stops."
                  " We recommend that all updates be stopped on the"
                  " master and slave, that the data of both be"
                  " manually synchronized,"
                  " that master's binary logs be deleted,"
                  " that master be upgraded to a version at least"
                  " equal to '%d.%d.%d'. Then replication can be"
                  " restarted.",
                  rli->get_rli_description_event()->server_version,
                  bug_id,
                  fixed_in[0], fixed_in[1], fixed_in[2]);
      return TRUE;
    }
  }
  return FALSE;
}

/**
   BUG#33029, For all 5.0 up to 5.0.58 exclusive, and 5.1 up to 5.1.12
   exclusive, if one statement in a SP generated AUTO_INCREMENT value
   by the top statement, all statements after it would be considered
   generated AUTO_INCREMENT value by the top statement, and a
   erroneous INSERT_ID value might be associated with these statement,
   which could cause duplicate entry error and stop the slave.

   Detect buggy master to work around.
 */
bool rpl_master_erroneous_autoinc(THD *thd)
{
  if (active_mi != NULL && active_mi->rli->info_thd == thd)
  {
    Relay_log_info *rli= active_mi->rli;
    DBUG_EXECUTE_IF("simulate_bug33029", return TRUE;);
    return rpl_master_has_bug(rli, 33029, FALSE, NULL, NULL);
  }
  return FALSE;
}

/**
  a copy of active_mi->rli->slave_skip_counter, for showing in SHOW VARIABLES,
  INFORMATION_SCHEMA.GLOBAL_VARIABLES and @@sql_slave_skip_counter without
  taking all the mutexes needed to access active_mi->rli->slave_skip_counter
  properly.
*/
uint sql_slave_skip_counter;

/**
  Execute a START SLAVE statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave's IO thread.

  @param net_report If true, saves the exit status into Diagnostics_area.

  @retval 0 success
  @retval 1 error
*/
int start_slave(THD* thd , Master_info* mi,  bool net_report)
{
  int slave_errno= 0;
  int thread_mask;
  DBUG_ENTER("start_slave");

  if (check_access(thd, SUPER_ACL, any_db, NULL, NULL, 0, 0))
    DBUG_RETURN(1);

  if (thd->lex->slave_connection.user ||
      thd->lex->slave_connection.password)
  {
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
    if (thd->vio_ok() && !thd->net.vio->ssl_arg)
      push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                   ER_INSECURE_PLAIN_TEXT,
                   ER(ER_INSECURE_PLAIN_TEXT));
#endif
#if !defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                 ER_INSECURE_PLAIN_TEXT,
                 ER(ER_INSECURE_PLAIN_TEXT));
#endif
  }

  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  // Get a mask of _stopped_ threads
  init_thread_mask(&thread_mask,mi,1 /* inverse */);
  /*
    Below we will start all stopped threads.  But if the user wants to
    start only one thread, do as if the other thread was running (as we
    don't wan't to touch the other thread), so set the bit to 0 for the
    other thread
  */
  if (thd->lex->slave_thd_opt)
    thread_mask&= thd->lex->slave_thd_opt;
  if (thread_mask) //some threads are stopped, start them
  {
    if (global_init_info(mi, false, thread_mask))
      slave_errno=ER_MASTER_INFO;
    else if (server_id_supplied && *mi->host)
    {
      /*
        If we will start IO thread we need to take care of possible
        options provided through the START SLAVE if there is any.
      */
      if (thread_mask & SLAVE_IO)
      {
        if (thd->lex->slave_connection.user)
        {
          mi->set_start_user_configured(true);
          mi->set_user(thd->lex->slave_connection.user);
        }
        if (thd->lex->slave_connection.password)
        {
          mi->set_start_user_configured(true);
          mi->set_password(thd->lex->slave_connection.password,
                           strlen(thd->lex->slave_connection.password));
        }
        if (thd->lex->slave_connection.plugin_auth)
          mi->set_plugin_auth(thd->lex->slave_connection.plugin_auth);
        if (thd->lex->slave_connection.plugin_dir)
          mi->set_plugin_dir(thd->lex->slave_connection.plugin_dir);
      }
 
      /*
        If we will start SQL thread we will care about UNTIL options If
        not and they are specified we will ignore them and warn user
        about this fact.
      */
      if (thread_mask & SLAVE_SQL)
      {
        /*
          To cache the MTS system var values and used them in the following
          runtime. The system var:s can change meanwhile but having no other
          effects.
        */
        mi->rli->opt_slave_parallel_workers= opt_mts_slave_parallel_workers;
#ifndef DBUG_OFF
        if (!DBUG_EVALUATE_IF("check_slave_debug_group", 1, 0))
#endif
          mi->rli->checkpoint_group= opt_mts_checkpoint_group;

        mysql_mutex_lock(&mi->rli->data_lock);

        if (thd->lex->mi.pos)
        {
          if (thd->lex->mi.relay_log_pos)
            slave_errno= ER_BAD_SLAVE_UNTIL_COND;
          mi->rli->until_condition= Relay_log_info::UNTIL_MASTER_POS;
          mi->rli->until_log_pos= thd->lex->mi.pos;
          /*
             We don't check thd->lex->mi.log_file_name for NULL here
             since it is checked in sql_yacc.yy
          */
          strmake(mi->rli->until_log_name, thd->lex->mi.log_file_name,
                  sizeof(mi->rli->until_log_name)-1);
        }
        else if (thd->lex->mi.relay_log_pos)
        {
          if (thd->lex->mi.pos)
            slave_errno= ER_BAD_SLAVE_UNTIL_COND;
          mi->rli->until_condition= Relay_log_info::UNTIL_RELAY_POS;
          mi->rli->until_log_pos= thd->lex->mi.relay_log_pos;
          strmake(mi->rli->until_log_name, thd->lex->mi.relay_log_name,
                  sizeof(mi->rli->until_log_name)-1);
        }
        else if (thd->lex->mi.gtid)
        {
          global_sid_lock->wrlock();
          mi->rli->clear_until_condition();
          if (mi->rli->until_sql_gtids.add_gtid_text(thd->lex->mi.gtid)
              != RETURN_STATUS_OK)
            slave_errno= ER_BAD_SLAVE_UNTIL_COND;
          else {
            mi->rli->until_condition=
              LEX_MASTER_INFO::UNTIL_SQL_BEFORE_GTIDS == thd->lex->mi.gtid_until_condition
              ? Relay_log_info::UNTIL_SQL_BEFORE_GTIDS
              : Relay_log_info::UNTIL_SQL_AFTER_GTIDS;
          }
          global_sid_lock->unlock();
        }
        else if (thd->lex->mi.until_after_gaps)
        {
            mi->rli->until_condition= Relay_log_info::UNTIL_SQL_AFTER_MTS_GAPS;
            mi->rli->opt_slave_parallel_workers=
              mi->rli->recovery_parallel_workers;
        }
        else
          mi->rli->clear_until_condition();

        if (mi->rli->until_condition == Relay_log_info::UNTIL_MASTER_POS ||
            mi->rli->until_condition == Relay_log_info::UNTIL_RELAY_POS)
        {
          /* Preparing members for effective until condition checking */
          const char *p= fn_ext(mi->rli->until_log_name);
          char *p_end;
          if (*p)
          {
            //p points to '.'
            mi->rli->until_log_name_extension= strtoul(++p,&p_end, 10);
            /*
              p_end points to the first invalid character. If it equals
              to p, no digits were found, error. If it contains '\0' it
              means  conversion went ok.
            */
            if (p_end==p || *p_end)
              slave_errno=ER_BAD_SLAVE_UNTIL_COND;
          }
          else
            slave_errno=ER_BAD_SLAVE_UNTIL_COND;

          /* mark the cached result of the UNTIL comparison as "undefined" */
          mi->rli->until_log_names_cmp_result=
            Relay_log_info::UNTIL_LOG_NAMES_CMP_UNKNOWN;

          /* Issuing warning then started without --skip-slave-start */
          if (!opt_skip_slave_start)
            push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                         ER_MISSING_SKIP_SLAVE,
                         ER(ER_MISSING_SKIP_SLAVE));
          if (mi->rli->opt_slave_parallel_workers != 0)
          {
            mi->rli->opt_slave_parallel_workers= 0;
            push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                                ER_MTS_FEATURE_IS_NOT_SUPPORTED,
                                ER(ER_MTS_FEATURE_IS_NOT_SUPPORTED),
                                "UNTIL condtion",
                                "Slave is started in the sequential execution mode.");
          }
        }

        mysql_mutex_unlock(&mi->rli->data_lock);

        /* MTS technical limitation no support of trans retry */
        if (mi->rli->opt_slave_parallel_workers != 0 && slave_trans_retries != 0)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                              ER_MTS_FEATURE_IS_NOT_SUPPORTED,
                              ER(ER_MTS_FEATURE_IS_NOT_SUPPORTED),
                              "slave_transaction_retries",
                              "In the event of a transient failure, the slave will "
                              "not retry the transaction and will stop.");
        }
      }
      else if (thd->lex->mi.pos || thd->lex->mi.relay_log_pos || thd->lex->mi.gtid)
        push_warning(thd, Sql_condition::WARN_LEVEL_NOTE, ER_UNTIL_COND_IGNORED,
                     ER(ER_UNTIL_COND_IGNORED));

      if (!slave_errno)
        slave_errno = start_slave_threads(false/*need_lock_slave=false*/,
                                          true/*wait_for_start=true*/,
                                          mi,
                                          thread_mask);
    }
    else
      slave_errno = ER_BAD_SLAVE;
  }
  else
  {
    /* no error if all threads are already started, only a warning */
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE, ER_SLAVE_WAS_RUNNING,
                 ER(ER_SLAVE_WAS_RUNNING));
  }

  /*
    Clean up start information if there was an attempt to start
    the IO thread to avoid any security issue.
  */
  if (slave_errno &&
      (thread_mask & SLAVE_IO) == SLAVE_IO)
    mi->reset_start_info();

  unlock_slave_threads(mi);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    DBUG_RETURN(1);
  }
  else if (net_report)
    my_ok(thd);

  DBUG_RETURN(0);
}


/**
  Execute a STOP SLAVE statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave's IO thread.

  @param net_report If true, saves the exit status into Diagnostics_area.

  @retval 0 success
  @retval 1 error
*/
int stop_slave(THD* thd, Master_info* mi, bool net_report )
{
  DBUG_ENTER("stop_slave");
  
  int slave_errno;
  if (!thd)
    thd = current_thd;

  if (check_access(thd, SUPER_ACL, any_db, NULL, NULL, 0, 0))
    DBUG_RETURN(1);
  THD_STAGE_INFO(thd, stage_killing_slave);
  int thread_mask;
  lock_slave_threads(mi);
  // Get a mask of _running_ threads
  init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
  /*
    Below we will stop all running threads.
    But if the user wants to stop only one thread, do as if the other thread
    was stopped (as we don't wan't to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (thd->lex->slave_thd_opt)
    thread_mask &= thd->lex->slave_thd_opt;

  if (thread_mask)
  {
    slave_errno= terminate_slave_threads(mi,thread_mask,
                                         false/*need_lock_term=false*/);
  }
  else
  {
    //no error if both threads are already stopped, only a warning
    slave_errno= 0;
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE, ER_SLAVE_WAS_NOT_RUNNING,
                 ER(ER_SLAVE_WAS_NOT_RUNNING));
  }
  unlock_slave_threads(mi);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    DBUG_RETURN(1);
  }
  else if (net_report)
    my_ok(thd);

  DBUG_RETURN(0);
}


/**
  Execute a RESET SLAVE statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave.

  @retval 0 success
  @retval 1 error
*/
int reset_slave(THD *thd, Master_info* mi)
{
  int thread_mask= 0, error= 0;
  uint sql_errno=ER_UNKNOWN_ERROR;
  const char* errmsg= "Unknown error occured while reseting slave";
  DBUG_ENTER("reset_slave");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /* not inverse */);
  if (thread_mask) // We refuse if any slave thread is running
  {
    sql_errno= ER_SLAVE_MUST_STOP;
    error=1;
    goto err;
  }

  ha_reset_slave(thd);

  // delete relay logs, clear relay log coordinates
  if ((error= mi->rli->purge_relay_logs(thd,
                                        1 /* just reset */,
                                        &errmsg)))
  {
    sql_errno= ER_RELAY_LOG_FAIL;
    goto err;
  }

  /* Clear master's log coordinates and associated information */
  DBUG_ASSERT(!mi->rli || !mi->rli->slave_running); // none writes in rli table
  mi->clear_in_memory_info(thd->lex->reset_slave_info.all);

  if (remove_info(mi))
  {
    error= 1;
    goto err;
  }

  (void) RUN_HOOK(binlog_relay_io, after_reset_slave, (thd, mi));
err:
  unlock_slave_threads(mi);
  if (error)
    my_error(sql_errno, MYF(0), errmsg);
  DBUG_RETURN(error);
}

/**
  Execute a CHANGE MASTER statement. MTS workers info tables data are removed
  in the successful branch (i.e. there are no gaps in the execution history).

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object belonging to the slave's IO
  thread.

  @retval FALSE success
  @retval TRUE error
*/
bool change_master(THD* thd, Master_info* mi)
{
  int thread_mask;
  const char* errmsg= 0;
  bool need_relay_log_purge= 1;
  char *var_master_log_name= NULL, *var_group_master_log_name= NULL;
  bool ret= false;
  char saved_host[HOSTNAME_LENGTH + 1], saved_bind_addr[HOSTNAME_LENGTH + 1];
  uint saved_port= 0;
  char saved_log_name[FN_REFLEN];
  my_off_t saved_log_pos= 0;
  my_bool save_relay_log_purge= relay_log_purge;
  bool mts_remove_workers= false;

  DBUG_ENTER("change_master");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /*not inverse*/);
  LEX_MASTER_INFO* lex_mi= &thd->lex->mi;
  if (thread_mask) // We refuse if any slave thread is running
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    ret= true;
    goto err;
  }
  thread_mask= SLAVE_IO | SLAVE_SQL;

  THD_STAGE_INFO(thd, stage_changing_master);
  /* 
    We need to check if there is an empty master_host. Otherwise
    change master succeeds, a master.info file is created containing 
    empty master_host string and when issuing: start slave; an error
    is thrown stating that the server is not configured as slave.
    (See BUG#28796).
  */
  if(lex_mi->host && !*lex_mi->host) 
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "MASTER_HOST");
    unlock_slave_threads(mi);
    DBUG_RETURN(TRUE);
  }
  if (global_init_info(mi, false, thread_mask))
  {
    my_message(ER_MASTER_INFO, ER(ER_MASTER_INFO), MYF(0));
    ret= true;
    goto err;
  }
  if (mi->rli->mts_recovery_group_cnt)
  {
    /*
      Change-Master can't be done if there is a mts group gap.
      That requires mts-recovery which START SLAVE provides.
    */
    DBUG_ASSERT(mi->rli->recovery_parallel_workers);

    my_message(ER_MTS_CHANGE_MASTER_CANT_RUN_WITH_GAPS,
               ER(ER_MTS_CHANGE_MASTER_CANT_RUN_WITH_GAPS), MYF(0));
    ret= true;
    goto err;
  }
  else
  {
    /*
      Lack of mts group gaps makes Workers info stale
      regardless of need_relay_log_purge computation.
    */
    if (mi->rli->recovery_parallel_workers)
      mts_remove_workers= true;
  }
  /*
    We cannot specify auto position and set either the coordinates
    on master or slave. If we try to do so, an error message is
    printed out.
  */
  if (lex_mi->log_file_name != NULL || lex_mi->pos != 0 || 
      lex_mi->relay_log_name != NULL || lex_mi->relay_log_pos != 0)
  {
    if (lex_mi->auto_position == LEX_MASTER_INFO::LEX_MI_ENABLE ||
        (lex_mi->auto_position != LEX_MASTER_INFO::LEX_MI_DISABLE &&
         mi->is_auto_position()))
    {
      my_message(ER_BAD_SLAVE_AUTO_POSITION,
                 ER(ER_BAD_SLAVE_AUTO_POSITION), MYF(0));
      ret= true;
      goto err;
    }
  }

  // CHANGE MASTER TO MASTER_AUTO_POSITION = 1 requires GTID_MODE = ON
  if (lex_mi->auto_position == LEX_MASTER_INFO::LEX_MI_ENABLE && gtid_mode != 3)
  {
    my_message(ER_AUTO_POSITION_REQUIRES_GTID_MODE_ON,
               ER(ER_AUTO_POSITION_REQUIRES_GTID_MODE_ON), MYF(0));
    ret= true;
    goto err;
  }

  /*
    Data lock not needed since we have already stopped the running threads,
    and we have the hold on the run locks which will keep all threads that
    could possibly modify the data structures from running
  */

  /*
    Before processing the command, save the previous state.
  */
  strmake(saved_host, mi->host, HOSTNAME_LENGTH);
  strmake(saved_bind_addr, mi->bind_addr, HOSTNAME_LENGTH);
  saved_port= mi->port;
  strmake(saved_log_name, mi->get_master_log_name(), FN_REFLEN - 1);
  saved_log_pos= mi->get_master_log_pos();

  /*
    If the user specified host or port without binlog or position,
    reset binlog's name to FIRST and position to 4.
  */

  if ((lex_mi->host && strcmp(lex_mi->host, mi->host)) ||
      (lex_mi->port && lex_mi->port != mi->port))
  {
    /*
      This is necessary because the primary key, i.e. host or port, has
      changed.

      The repository does not support direct changes on the primary key,
      so the row is dropped and re-inserted with a new primary key. If we
      don't do that, the master info repository we will end up with several
      rows.
    */
    if (mi->clean_info())
    {
      ret= true;
      goto err;
    }
    mi->master_uuid[0]= 0;
    mi->master_id= 0;
  }

  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
  {
    var_master_log_name= const_cast<char*>(mi->get_master_log_name());
    var_master_log_name[0]= '\0';
    mi->set_master_log_pos(BIN_LOG_HEADER_SIZE);
  }

  if (lex_mi->log_file_name)
    mi->set_master_log_name(lex_mi->log_file_name);
  if (lex_mi->pos)
  {
    mi->set_master_log_pos(lex_mi->pos);
  }
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));

  if (lex_mi->user || lex_mi->password)
  {
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
    if (thd->vio_ok() && !thd->net.vio->ssl_arg)
      push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                   ER_INSECURE_PLAIN_TEXT,
                   ER(ER_INSECURE_PLAIN_TEXT));
#endif
#if !defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                 ER_INSECURE_PLAIN_TEXT,
                 ER(ER_INSECURE_PLAIN_TEXT));
#endif
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                 ER_INSECURE_CHANGE_MASTER,
                 ER(ER_INSECURE_CHANGE_MASTER));
  }

  if (lex_mi->user)
    mi->set_user(lex_mi->user);

  if (lex_mi->password)
  {
    if (mi->set_password(lex_mi->password, strlen(lex_mi->password)))
    {
      /*
        After implementing WL#5769, we should create a better error message
        to denote that the call may have failed due to an error while trying
        to encrypt/store the password in a secure key store.
      */
      my_message(ER_MASTER_INFO, ER(ER_MASTER_INFO), MYF(0));
      ret= false;
      goto err;
    }
  }
  if (lex_mi->host)
    strmake(mi->host, lex_mi->host, sizeof(mi->host)-1);
  if (lex_mi->bind_addr)
    strmake(mi->bind_addr, lex_mi->bind_addr, sizeof(mi->bind_addr)-1);
  if (lex_mi->port)
    mi->port = lex_mi->port;
  if (lex_mi->connect_retry)
    mi->connect_retry = lex_mi->connect_retry;
  if (lex_mi->retry_count_opt !=  LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->retry_count = lex_mi->retry_count;
  if (lex_mi->heartbeat_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->heartbeat_period = lex_mi->heartbeat_period;
  else
    mi->heartbeat_period= min<float>(SLAVE_MAX_HEARTBEAT_PERIOD,
                                     (slave_net_timeout/2.0));
  mi->received_heartbeats= LL(0); // counter lives until master is CHANGEd
  /*
    reset the last time server_id list if the current CHANGE MASTER 
    is mentioning IGNORE_SERVER_IDS= (...)
  */
  if (lex_mi->repl_ignore_server_ids_opt == LEX_MASTER_INFO::LEX_MI_ENABLE)
    reset_dynamic(&(mi->ignore_server_ids->dynamic_ids));
  for (uint i= 0; i < lex_mi->repl_ignore_server_ids.elements; i++)
  {
    ulong s_id;
    get_dynamic(&lex_mi->repl_ignore_server_ids, (uchar*) &s_id, i);
    if (s_id == ::server_id && replicate_same_server_id)
    {
      my_error(ER_SLAVE_IGNORE_SERVER_IDS, MYF(0), static_cast<int>(s_id));
      ret= TRUE;
      goto err;
    }
    else
    {
      if (bsearch((const ulong *) &s_id,
                  mi->ignore_server_ids->dynamic_ids.buffer,
                  mi->ignore_server_ids->dynamic_ids.elements, sizeof(ulong),
                  (int (*) (const void*, const void*))
                  change_master_server_id_cmp) == NULL)
        insert_dynamic(&(mi->ignore_server_ids->dynamic_ids), (uchar*) &s_id);
    }
  }
  sort_dynamic(&(mi->ignore_server_ids->dynamic_ids), (qsort_cmp) change_master_server_id_cmp);

  if (lex_mi->ssl != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->ssl= (lex_mi->ssl == LEX_MASTER_INFO::LEX_MI_ENABLE);

  if (lex_mi->sql_delay != -1)
    mi->rli->set_sql_delay(lex_mi->sql_delay);

  if (lex_mi->ssl_verify_server_cert != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->ssl_verify_server_cert=
      (lex_mi->ssl_verify_server_cert == LEX_MASTER_INFO::LEX_MI_ENABLE);

  if (lex_mi->ssl_ca)
    strmake(mi->ssl_ca, lex_mi->ssl_ca, sizeof(mi->ssl_ca)-1);
  if (lex_mi->ssl_capath)
    strmake(mi->ssl_capath, lex_mi->ssl_capath, sizeof(mi->ssl_capath)-1);
  if (lex_mi->ssl_cert)
    strmake(mi->ssl_cert, lex_mi->ssl_cert, sizeof(mi->ssl_cert)-1);
  if (lex_mi->ssl_cipher)
    strmake(mi->ssl_cipher, lex_mi->ssl_cipher, sizeof(mi->ssl_cipher)-1);
  if (lex_mi->ssl_key)
    strmake(mi->ssl_key, lex_mi->ssl_key, sizeof(mi->ssl_key)-1);
  if (lex_mi->ssl_crl)
    strmake(mi->ssl_crl, lex_mi->ssl_crl, sizeof(mi->ssl_crl)-1);
  if (lex_mi->ssl_crlpath)
    strmake(mi->ssl_crlpath, lex_mi->ssl_crlpath, sizeof(mi->ssl_crlpath)-1);
#ifndef HAVE_OPENSSL
  if (lex_mi->ssl || lex_mi->ssl_ca || lex_mi->ssl_capath ||
      lex_mi->ssl_cert || lex_mi->ssl_cipher || lex_mi->ssl_key ||
      lex_mi->ssl_verify_server_cert || lex_mi->ssl_crl || lex_mi->ssl_crlpath)
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
                 ER_SLAVE_IGNORED_SSL_PARAMS, ER(ER_SLAVE_IGNORED_SSL_PARAMS));
#endif

  if (lex_mi->relay_log_name)
  {
    need_relay_log_purge= 0;
    char relay_log_name[FN_REFLEN];

    mi->rli->relay_log.make_log_name(relay_log_name, lex_mi->relay_log_name);
    mi->rli->set_group_relay_log_name(relay_log_name);
    mi->rli->set_event_relay_log_name(relay_log_name);
  }

  if (lex_mi->relay_log_pos)
  {
    need_relay_log_purge= 0;
    mi->rli->set_group_relay_log_pos(lex_mi->relay_log_pos);
    mi->rli->set_event_relay_log_pos(lex_mi->relay_log_pos);
  }

  /*
    If user did specify neither host nor port nor any log name nor any log
    pos, i.e. he specified only user/password/master_connect_retry, he probably
    wants replication to resume from where it had left, i.e. from the
    coordinates of the **SQL** thread (imagine the case where the I/O is ahead
    of the SQL; restarting from the coordinates of the I/O would lose some
    events which is probably unwanted when you are just doing minor changes
    like changing master_connect_retry).
    A side-effect is that if only the I/O thread was started, this thread may
    restart from ''/4 after the CHANGE MASTER. That's a minor problem (it is a
    much more unlikely situation than the one we are fixing here).
    Note: coordinates of the SQL thread must be read here, before the
    'if (need_relay_log_purge)' block which resets them.
  */
  if (!lex_mi->host && !lex_mi->port &&
      !lex_mi->log_file_name && !lex_mi->pos &&
      need_relay_log_purge)
   {
     /*
       Sometimes mi->rli->master_log_pos == 0 (it happens when the SQL thread is
       not initialized), so we use a max().
       What happens to mi->rli->master_log_pos during the initialization stages
       of replication is not 100% clear, so we guard against problems using
       max().
      */
     mi->set_master_log_pos(max<ulonglong>(BIN_LOG_HEADER_SIZE,
                                           mi->rli->get_group_master_log_pos()));
     mi->set_master_log_name(mi->rli->get_group_master_log_name());
  }

  /*
    Sets if the slave should connect to the master and look for
    GTIDs.
  */
  if (lex_mi->auto_position != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->set_auto_position(
      (lex_mi->auto_position == LEX_MASTER_INFO::LEX_MI_ENABLE));

  /*
    Relay log's IO_CACHE may not be inited, if rli->inited==0 (server was never
    a slave before).
  */
  if (flush_master_info(mi, true))
  {
    my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush master info file");
    ret= TRUE;
    goto err;
  }
  if (need_relay_log_purge)
  {
    relay_log_purge= 1;
    THD_STAGE_INFO(thd, stage_purging_old_relay_logs);
    if (mi->rli->purge_relay_logs(thd,
                                  0 /* not only reset, but also reinit */,
                                  &errmsg))
    {
      my_error(ER_RELAY_LOG_FAIL, MYF(0), errmsg);
      ret= TRUE;
      goto err;
    }
  }
  else
  {
    const char* msg;
    relay_log_purge= 0;
    /* Relay log is already initialized */
    
    if (mi->rli->init_relay_log_pos(mi->rli->get_group_relay_log_name(),
                                    mi->rli->get_group_relay_log_pos(),
                                    true/*need_data_lock=true*/,
                                    &msg, 0))
    {
      my_error(ER_RELAY_LOG_INIT, MYF(0), msg);
      ret= TRUE;
      goto err;
    }
  }
  relay_log_purge= save_relay_log_purge;

  /*
    Coordinates in rli were spoilt by the 'if (need_relay_log_purge)' block,
    so restore them to good values. If we left them to ''/0, that would work;
    but that would fail in the case of 2 successive CHANGE MASTER (without a
    START SLAVE in between): because first one would set the coords in mi to
    the good values of those in rli, the set those in rli to ''/0, then
    second CHANGE MASTER would set the coords in mi to those of rli, i.e. to
    ''/0: we have lost all copies of the original good coordinates.
    That's why we always save good coords in rli.
  */
  if (need_relay_log_purge)
  {
    mi->rli->set_group_master_log_pos(mi->get_master_log_pos());
    DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->get_master_log_pos()));
    mi->rli->set_group_master_log_name(mi->get_master_log_name());
  }
  var_group_master_log_name=  const_cast<char *>(mi->rli->get_group_master_log_name());
  if (!var_group_master_log_name[0]) // uninitialized case
    mi->rli->set_group_master_log_pos(0);

  mysql_mutex_lock(&mi->rli->data_lock);
  mi->rli->abort_pos_wait++; /* for MASTER_POS_WAIT() to abort */
  /* Clear the errors, for a clean start */
  mi->rli->clear_error();
  mi->rli->clear_until_condition();

  sql_print_information("'CHANGE MASTER TO executed'. "
    "Previous state master_host='%s', master_port= %u, master_log_file='%s', "
    "master_log_pos= %ld, master_bind='%s'. "
    "New state master_host='%s', master_port= %u, master_log_file='%s', "
    "master_log_pos= %ld, master_bind='%s'.", 
    saved_host, saved_port, saved_log_name, (ulong) saved_log_pos,
    saved_bind_addr, mi->host, mi->port, mi->get_master_log_name(),
    (ulong) mi->get_master_log_pos(), mi->bind_addr);

  /*
    If we don't write new coordinates to disk now, then old will remain in
    relay-log.info until START SLAVE is issued; but if mysqld is shutdown
    before START SLAVE, then old will remain in relay-log.info, and will be the
    in-memory value at restart (thus causing errors, as the old relay log does
    not exist anymore).

    Notice that the rli table is available exclusively as slave is not
    running.
  */
  DBUG_ASSERT(!mi->rli->slave_running);
  if ((ret= mi->rli->flush_info(true)))
    my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush relay info file.");
  mysql_cond_broadcast(&mi->data_cond);
  mysql_mutex_unlock(&mi->rli->data_lock);

err:
  unlock_slave_threads(mi);
  if (ret == FALSE)
  {
    if (!mts_remove_workers)
      my_ok(thd);
    else
      if (!Rpl_info_factory::reset_workers(mi->rli))
        my_ok(thd);
      else
        my_error(ER_MTS_RESET_WORKERS, MYF(0));
  }
  DBUG_RETURN(ret);
}
/**
  @} (end of group Replication)
*/
#endif /* HAVE_REPLICATION */
