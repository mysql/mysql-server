/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
  @addtogroup Replication
  @{

  @file

  @brief Code to run the io thread and the sql thread on the
  replication slave.
*/

#include "sql_priv.h"
#include "my_global.h"
#include "slave.h"
#include "sql_parse.h"                         // execute_init_command
#include "sql_table.h"                         // mysql_rm_table
#include "rpl_mi.h"
#include "rpl_rli.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include "transaction.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <sql_common.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <mysys_err.h>
#include "rpl_handler.h"
#include <signal.h>
#include <mysql.h>
#include <myisam.h>

#include "sql_base.h"                           // close_thread_tables
#include "tztime.h"                             // struct Time_zone
#include "log_event.h"                          // Rotate_log_event,
                                                // Create_file_log_event,
                                                // Format_description_log_event

#ifdef HAVE_REPLICATION

#include "rpl_tblmap.h"
#include "debug_sync.h"

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
 

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL} SLAVE_THD_TYPE;

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
static Log_event* next_event(Relay_log_info* rli);
static int queue_event(Master_info* mi,const char* buf,ulong event_len);
static int terminate_slave_thread(THD *thd,
                                  mysql_mutex_t *term_lock,
                                  mysql_cond_t *term_cond,
                                  volatile uint *slave_running,
                                  bool skip_lock);
static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info);

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

void init_thread_mask(int* mask,Master_info* mi,bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli.slave_running;
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
  mysql_mutex_lock(&mi->rli.run_lock);
  DBUG_VOID_RETURN;
}


/*
  unlock_slave_threads()
*/

void unlock_slave_threads(Master_info* mi)
{
  DBUG_ENTER("unlock_slave_threads");

  //TODO: see if we can do this without dual mutex
  mysql_mutex_unlock(&mi->rli.run_lock);
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

  if (PSI_server == NULL)
    return;

  count= array_elements(all_slave_threads);
  PSI_server->register_thread(category, all_slave_threads, count);
}
#endif /* HAVE_PSI_INTERFACE */

/* Initialize slave structures */

int init_slave()
{
  DBUG_ENTER("init_slave");
  int error= 0;

#ifdef HAVE_PSI_INTERFACE
  init_slave_psi_keys();
#endif

  /*
    This is called when mysqld starts. Before client connections are
    accepted. However bootstrap may conflict with us if it does START SLAVE.
    So it's safer to take the lock.
  */
  mysql_mutex_lock(&LOCK_active_mi);
  /*
    TODO: re-write this to interate through the list of files
    for multi-master
  */
  active_mi= new Master_info(relay_log_recovery);

  if (pthread_key_create(&RPL_MASTER_INFO, NULL))
    goto err;

  /*
    If --slave-skip-errors=... was not used, the string value for the
    system variable has not been set up yet. Do it now.
  */
  if (!use_slave_mask)
  {
    print_slave_skip_errors();
  }

  /*
    If master_host is not specified, try to read it from the master_info file.
    If master_host is specified, create the master_info file if it doesn't
    exists.
  */
  if (!active_mi)
  {
    sql_print_error("Failed to allocate memory for the master info structure");
    error= 1;
    goto err;
  }

  if (init_master_info(active_mi,master_info_file,relay_log_info_file,
                       1, (SLAVE_IO | SLAVE_SQL)))
  {
    sql_print_error("Failed to initialize the master info structure");
    error= 1;
    goto err;
  }

  /* If server id is not set, start_slave_thread() will say it */

  if (active_mi->host[0] && !opt_skip_slave_start)
  {
    if (start_slave_threads(1 /* need mutex */,
                            0 /* no wait for start*/,
                            active_mi,
                            master_info_file,
                            relay_log_info_file,
                            SLAVE_IO | SLAVE_SQL))
    {
      sql_print_error("Failed to create slave threads");
      error= 1;
      goto err;
    }
  }

err:
  mysql_mutex_unlock(&LOCK_active_mi);
  DBUG_RETURN(error);
}

/*
  Updates the master info based on the information stored in the
  relay info and ignores relay logs previously retrieved by the IO 
  thread, which thus starts fetching again based on to the  
  group_master_log_pos and group_master_log_name. Eventually, the old
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
 
  Relay_log_info *rli= &mi->rli;
  if (rli->group_master_log_name[0])
  {
    mi->master_log_pos= max(BIN_LOG_HEADER_SIZE,
                             rli->group_master_log_pos);
    strmake(mi->master_log_name, rli->group_master_log_name,
            sizeof(mi->master_log_name)-1);
 
    sql_print_warning("Recovery from master pos %ld and file %s.",
                      (ulong) mi->master_log_pos, mi->master_log_name);
 
    strmake(rli->group_relay_log_name, rli->relay_log.get_log_fname(),
            sizeof(rli->group_relay_log_name)-1);
    strmake(rli->event_relay_log_name, rli->relay_log.get_log_fname(),
            sizeof(mi->rli.event_relay_log_name)-1);
 
    rli->group_relay_log_pos= rli->event_relay_log_pos= BIN_LOG_HEADER_SIZE;
  }

  DBUG_RETURN(0);
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

  /* Make @@slave_skip_errors show the nice human-readable value.  */
  opt_slave_skip_errors= slave_skip_error_names;

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

/*
  Init function to set up array for errors that should be skipped for slave

  SYNOPSIS
    init_slave_skip_errors()
    arg         List of errors numbers to skip, separated with ','

  NOTES
    Called from get_options() in mysqld.cc on start-up
*/

void init_slave_skip_errors(const char* arg)
{
  const char *p;
  DBUG_ENTER("init_slave_skip_errors");

  if (bitmap_init(&slave_error_mask,0,MAX_SLAVE_ERROR,0))
  {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(1);
  }
  use_slave_mask = 1;
  for (;my_isspace(system_charset_info,*arg);++arg)
    /* empty */;
  if (!my_strnncoll(system_charset_info,(uchar*)arg,4,(const uchar*)"all",4))
  {
    bitmap_set_all(&slave_error_mask);
    print_slave_skip_errors();
    DBUG_VOID_RETURN;
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
  /* Convert slave skip errors bitmap into a printable string. */
  print_slave_skip_errors();
  DBUG_VOID_RETURN;
}

static void set_thd_in_use_temporary_tables(Relay_log_info *rli)
{
  TABLE *table;

  for (table= rli->save_temporary_tables ; table ; table= table->next)
    table->in_use= rli->sql_thd;
}

int terminate_slave_threads(Master_info* mi,int thread_mask,bool skip_lock)
{
  DBUG_ENTER("terminate_slave_threads");

  if (!mi->inited)
    DBUG_RETURN(0); /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  mysql_mutex_t *sql_lock = &mi->rli.run_lock, *io_lock = &mi->run_lock;
  mysql_mutex_t *log_lock= mi->rli.relay_log.get_log_lock();

  if (thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating SQL thread"));
    mi->rli.abort_slave=1;
    if ((error=terminate_slave_thread(mi->rli.sql_thd, sql_lock,
                                      &mi->rli.stop_cond,
                                      &mi->rli.slave_running,
                                      skip_lock)) &&
        !force_all)
      DBUG_RETURN(error);

    mysql_mutex_lock(log_lock);

    DBUG_PRINT("info",("Flushing relay-log info file."));
    if (current_thd)
      thd_proc_info(current_thd, "Flushing relay-log info file.");
    if (flush_relay_log_info(&mi->rli))
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);
    
    if (my_sync(mi->rli.info_fd, MYF(MY_WME)))
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);

    mysql_mutex_unlock(log_lock);
  }
  if (thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating IO thread"));
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->io_thd, io_lock,
                                      &mi->stop_cond,
                                      &mi->slave_running,
                                      skip_lock)) &&
        !force_all)
      DBUG_RETURN(error);

    mysql_mutex_lock(log_lock);

    DBUG_PRINT("info",("Flushing relay log and master info file."));
    if (current_thd)
      thd_proc_info(current_thd, "Flushing relay log and master info files.");
    if (flush_master_info(mi, TRUE, FALSE))
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);

    if (mi->rli.relay_log.is_open() &&
        my_sync(mi->rli.relay_log.get_log_file()->file, MYF(MY_WME)))
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);

    if (my_sync(mi->fd, MYF(MY_WME)))
      DBUG_RETURN(ER_ERROR_DURING_FLUSH_LOGS);

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
   unless @c skip_lock is true in which case the mutex should be owned
   by the caller of this function and will remain acquired after
   return from the function.

   @param term_lock
          Associated lock to use when waiting for @c term_cond

   @param term_cond
          Condition that is signalled when the thread has terminated

   @param slave_running
          Pointer to predicate to check for slave thread termination

   @param skip_lock
          If @c true the lock will not be acquired before waiting on
          the condition. In this case, it is assumed that the calling
          function acquires the lock before calling this function.

   @retval 0 All OK ER_SLAVE_NOT_RUNNING otherwise.

   @note  If the executing thread has to acquire term_lock (skip_lock
          is false), the negative running status does not represent
          any issue therefore no error is reported.

 */
static int
terminate_slave_thread(THD *thd,
                       mysql_mutex_t *term_lock,
                       mysql_cond_t *term_cond,
                       volatile uint *slave_running,
                       bool skip_lock)
{
  DBUG_ENTER("terminate_slave_thread");
  if (!skip_lock)
  {
    mysql_mutex_lock(term_lock);
  }
  else
  {
    mysql_mutex_assert_owner(term_lock);
  }
  if (!*slave_running)
  {
    if (!skip_lock)
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

  if (!skip_lock)
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

  DBUG_ASSERT(mi->inited);

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
    while (start_id == *slave_run_id)
    {
      DBUG_PRINT("sleep",("Waiting for slave thread to start"));
      const char *old_msg= thd->enter_cond(start_cond, cond_lock,
                                           "Waiting for slave thread to start");
      /*
        It is not sufficient to test this at loop bottom. We must test
        it after registering the mutex in enter_cond(). If the kill
        happens after testing of thd->killed and before the mutex is
        registered, we could otherwise go waiting though thd->killed is
        set.
      */
      if (!thd->killed)
        mysql_cond_wait(start_cond, cond_lock);
      thd->exit_cond(old_msg);
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

int start_slave_threads(bool need_slave_mutex, bool wait_for_start,
                        Master_info* mi, const char* master_info_fname,
                        const char* slave_info_fname, int thread_mask)
{
  mysql_mutex_t *lock_io=0, *lock_sql=0, *lock_cond_io=0, *lock_cond_sql=0;
  mysql_cond_t* cond_io=0, *cond_sql=0;
  int error=0;
  DBUG_ENTER("start_slave_threads");

  if (need_slave_mutex)
  {
    lock_io = &mi->run_lock;
    lock_sql = &mi->rli.run_lock;
  }
  if (wait_for_start)
  {
    cond_io = &mi->start_cond;
    cond_sql = &mi->rli.start_cond;
    lock_cond_io = &mi->run_lock;
    lock_cond_sql = &mi->rli.run_lock;
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
    error= start_slave_thread(
#ifdef HAVE_PSI_INTERFACE
                              key_thread_slave_sql,
#endif
                              handle_slave_sql, lock_sql, lock_cond_sql,
                              cond_sql,
                              &mi->rli.slave_running, &mi->rli.slave_run_id,
                              mi);
    if (error)
      terminate_slave_threads(mi, thread_mask & SLAVE_IO, !need_slave_mutex);
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
    end_master_info(active_mi);
    delete active_mi;
    active_mi= 0;
  }
  mysql_mutex_unlock(&LOCK_active_mi);
}

static bool io_slave_killed(THD* thd, Master_info* mi)
{
  DBUG_ENTER("io_slave_killed");

  DBUG_ASSERT(mi->io_thd == thd);
  DBUG_ASSERT(mi->slave_running); // tracking buffer overrun
  DBUG_RETURN(mi->abort_slave || abort_loop || thd->killed);
}

/**
   The function analyzes a possible killed status and makes
   a decision whether to accept it or not.
   Normally upon accepting the sql thread goes to shutdown.
   In the event of deffering decision @rli->last_event_start_time waiting
   timer is set to force the killed status be accepted upon its expiration.

   @param thd   pointer to a THD instance
   @param rli   pointer to Relay_log_info instance

   @return TRUE the killed status is recognized, FALSE a possible killed
           status is deferred.
*/
static bool sql_slave_killed(THD* thd, Relay_log_info* rli)
{
  bool ret= FALSE;
  DBUG_ENTER("sql_slave_killed");

  DBUG_ASSERT(rli->sql_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);// tracking buffer overrun
  if (abort_loop || thd->killed || rli->abort_slave)
  {
    /*
      The transaction should always be binlogged if OPTION_KEEP_LOG is set
      (it implies that something can not be rolled back). And such case
      should be regarded similarly as modifing a non-transactional table
      because retrying of the transaction will lead to an error or inconsistency
      as well.
      Example: OPTION_KEEP_LOG is set if a temporary table is created or dropped.
    */
    if ((thd->transaction.all.modified_non_trans_table ||
         (thd->variables.option_bits & OPTION_KEEP_LOG))
        && rli->is_in_group())
    {
      char msg_stopped[]=
        "... Slave SQL Thread stopped with incomplete event group "
        "having non-transactional changes. "
        "If the group consists solely of row-based events, you can try "
        "to restart the slave with --slave-exec-mode=IDEMPOTENT, which "
        "ignores duplicate key, key not found, and similar errors (see "
        "documentation for details).";

      if (rli->abort_slave)
      {
        DBUG_PRINT("info", ("Request to stop slave SQL Thread received while "
                            "applying a group that has non-transactional "
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

        if (ret == 0)
        {
          rli->report(WARNING_LEVEL, 0,
                      "Request to stop slave SQL Thread received while "
                      "applying a group that has non-transactional "
                      "changes; waiting for completion of the group ... ");
        }
        else
        {
          rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                      ER(ER_SLAVE_FATAL_ERROR), msg_stopped);
        }
      }
      else
      {
        ret= TRUE;
        rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR),
                    msg_stopped);
      }
    }
    else
    {
      ret= TRUE;
    }
  }
  if (ret)
    rli->last_event_start_time= 0;
  
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

int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
                                 const char *default_val)
{
  uint length;
  DBUG_ENTER("init_strvar_from_file");

  if ((length=my_b_gets(f,var, max_size)))
  {
    char* last_p = var + length -1;
    if (*last_p == '\n')
      *last_p = 0; // if we stopped on newline, kill it
    else
    {
      /*
        If we truncated a line or stopped on last char, remove all chars
        up to and including newline.
      */
      int c;
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF)) ;
    }
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  char buf[32];
  DBUG_ENTER("init_intvar_from_file");


  if (my_b_gets(f, buf, sizeof(buf)))
  {
    *var = atoi(buf);
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val)
{
  char buf[16];
  DBUG_ENTER("init_floatvar_from_file");


  if (my_b_gets(f, buf, sizeof(buf)))
  {
    if (sscanf(buf, "%f", var) != 1)
      DBUG_RETURN(1);
    else
      DBUG_RETURN(0);
  }
  else if (default_val != 0.0)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


/**
   A master info read method

   This function is called from @c init_master_info() along with
   relatives to restore some of @c active_mi members.
   Particularly, this function is responsible for restoring
   IGNORE_SERVER_IDS list of servers whose events the slave is
   going to ignore (to not log them in the relay log).
   Items being read are supposed to be decimal output of values of a
   type shorter or equal of @c long and separated by the single space.

   @param arr         @c DYNAMIC_ARRAY pointer to storage for servers id
   @param f           @c IO_CACHE pointer to the source file

   @retval 0         All OK
   @retval non-zero  An error
*/

int init_dynarray_intvar_from_file(DYNAMIC_ARRAY* arr, IO_CACHE* f)
{
  int ret= 0;
  char buf[16 * (sizeof(long)*4 + 1)]; // static buffer to use most of times
  char *buf_act= buf; // actual buffer can be dynamic if static is short
  char *token, *last;
  uint num_items;     // number of items of `arr'
  size_t read_size;
  DBUG_ENTER("init_dynarray_intvar_from_file");

  if ((read_size= my_b_gets(f, buf_act, sizeof(buf))) == 0)
  {
    return 0; // no line in master.info
  }
  if (read_size + 1 == sizeof(buf) && buf[sizeof(buf) - 2] != '\n')
  {
    /*
      short read happend; allocate sufficient memory and make the 2nd read
    */
    char buf_work[(sizeof(long)*3 + 1)*16];
    memcpy(buf_work, buf, sizeof(buf_work));
    num_items= atoi(strtok_r(buf_work, " ", &last));
    size_t snd_size;
    /*
      max size lower bound approximate estimation bases on the formula:
      (the items number + items themselves) * 
          (decimal size + space) - 1 + `\n' + '\0'
    */
    size_t max_size= (1 + num_items) * (sizeof(long)*3 + 1) + 1;
    buf_act= (char*) my_malloc(max_size, MYF(MY_WME));
    memcpy(buf_act, buf, read_size);
    snd_size= my_b_gets(f, buf_act + read_size, max_size - read_size);
    if (snd_size == 0 ||
        ((snd_size + 1 == max_size - read_size) &&  buf_act[max_size - 2] != '\n'))
    {
      /*
        failure to make the 2nd read or short read again
      */
      ret= 1;
      goto err;
    }
  }
  token= strtok_r(buf_act, " ", &last);
  if (token == NULL)
  {
    ret= 1;
    goto err;
  }
  num_items= atoi(token);
  for (uint i=0; i < num_items; i++)
  {
    token= strtok_r(NULL, " ", &last);
    if (token == NULL)
    {
      ret= 1;
      goto err;
    }
    else
    {
      ulong val= atol(token);
      insert_dynamic(arr, (uchar *) &val);
    }
  }
err:
  if (buf_act != buf)
    my_free(buf_act);
  DBUG_RETURN(ret);
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
    Free old description_event_for_queue (that is needed if we are in
    a reconnection).
  */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;

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
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(1, mysql->server_version);
      break;
    case '4':
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(3, mysql->server_version);
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
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(4, mysql->server_version);
      break;
    }
  }

  /*
     This does not mean that a 5.0 slave will be able to read a 6.0 master; but
     as we don't know yet, we don't want to forbid this for now. If a 5.0 slave
     can't read a 6.0 master, this will show up when the slave can't read some
     events sent by the master, and there will be error messages.
  */

  if (errmsg)
    goto err;

  /* as we are here, we tried to allocate the event */
  if (!mi->rli.relay_log.description_event_for_queue)
  {
    errmsg= "default Format_description_log_event";
    err_code= ER_SLAVE_CREATE_EVENT_FAILURE;
    sprintf(err_buff, ER(err_code), errmsg);
    goto err;
  }

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
    mi->clock_diff_with_master=
      (long) (time((time_t*) 0) - strtoul(master_row[0], 0, 10));
  }
  else if (check_io_slave_killed(mi->io_thd, mi, NULL))
    goto slave_killed_err;
  else if (is_network_error(mysql_errno(mysql)))
  {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "Get master clock failed with error: %s", mysql_error(mysql));
    goto network_err;
  }
  else 
  {
    mi->clock_diff_with_master= 0; /* The "most sensible" value */
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
        !mi->rli.replicate_same_server_id)
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
    if (check_io_slave_killed(mi->io_thd, mi, NULL))
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
  if (mi->master_id == 0 && mi->ignore_server_ids.elements > 0)
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
    else if (check_io_slave_killed(mi->io_thd, mi, NULL))
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
    else if (check_io_slave_killed(mi->io_thd, mi, NULL))
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

    if (mysql_real_query(mysql, query, strlen(query))
        && !check_io_slave_killed(mi->io_thd, mi, NULL))
    {
      errmsg= "The slave I/O thread stops because SET @master_heartbeat_period "
        "on master failed.";
      err_code= ER_SLAVE_FATAL_ERROR;
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      mysql_free_result(mysql_store_result(mysql));
      goto err;
    }
    mysql_free_result(mysql_store_result(mysql));
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
  const char *save_proc_info;
  THD* thd = mi->io_thd;
  DBUG_ENTER("wait_for_relay_log_space");

  mysql_mutex_lock(&rli->log_space_lock);
  save_proc_info= thd->enter_cond(&rli->log_space_cond,
                                  &rli->log_space_lock,
                                  "\
Waiting for the slave SQL thread to free enough relay log space");
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
      rotate_relay_log(rli->mi);
      rli->sql_force_rotate_relay= false;
    }

    rli->ignore_log_space_limit= false;
  }

  thd->exit_cond(save_proc_info);
  DBUG_RETURN(slave_killed);
}


/*
  Builds a Rotate from the ignored events' info and writes it to relay log.

  SYNOPSIS
  write_ignored_events_info_to_relay_log()
    thd             pointer to I/O thread's thd
    mi

  DESCRIPTION
    Slave I/O thread, going to die, must leave a durable trace of the
    ignored events' end position for the use of the slave SQL thread, by
    calling this function. Only that thread can call it (see assertion).
 */
static void write_ignored_events_info_to_relay_log(THD *thd, Master_info *mi)
{
  Relay_log_info *rli= &mi->rli;
  mysql_mutex_t *log_lock= rli->relay_log.get_log_lock();
  DBUG_ENTER("write_ignored_events_info_to_relay_log");

  DBUG_ASSERT(thd == mi->io_thd);
  mysql_mutex_lock(log_lock);
  if (rli->ign_master_log_name_end[0])
  {
    DBUG_PRINT("info",("writing a Rotate event to track down ignored events"));
    Rotate_log_event *ev= new Rotate_log_event(rli->ign_master_log_name_end,
                                               0, rli->ign_master_log_pos_end,
                                               Rotate_log_event::DUP_NAME);
    rli->ign_master_log_name_end[0]= 0;
    /* can unlock before writing as slave SQL thd will soon see our Rotate */
    mysql_mutex_unlock(log_lock);
    if (likely((bool)ev))
    {
      ev->server_id= 0; // don't be ignored by slave SQL thread
      if (unlikely(rli->relay_log.append(ev)))
        mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                   ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                   "failed to write a Rotate event"
                   " to the relay log, SHOW SLAVE STATUS may be"
                   " inaccurate");
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
      if (flush_master_info(mi, TRUE, TRUE))
        sql_print_error("Failed to flush master info file");
      delete ev;
    }
    else
      mi->report(ERROR_LEVEL, ER_SLAVE_CREATE_EVENT_FAILURE,
                 ER(ER_SLAVE_CREATE_EVENT_FAILURE),
                 "Rotate_event (out of memory?),"
                 " SHOW SLAVE STATUS may be inaccurate");
  }
  else
    mysql_mutex_unlock(log_lock);
  DBUG_VOID_RETURN;
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
    else if (!check_io_slave_killed(mi->io_thd, mi, NULL))
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
bool show_master_info(THD* thd, Master_info* mi)
{
  // TODO: fix this for multi-master
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_master_info");

  field_list.push_back(new Item_empty_string("Slave_IO_State",
                                                     14));
  field_list.push_back(new Item_empty_string("Master_Host",
                                                     sizeof(mi->host)));
  field_list.push_back(new Item_empty_string("Master_User",
                                                     sizeof(mi->user)));
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
  field_list.push_back(new Item_empty_string("Master_SSL_CA_File",
                                             sizeof(mi->ssl_ca)));
  field_list.push_back(new Item_empty_string("Master_SSL_CA_Path",
                                             sizeof(mi->ssl_capath)));
  field_list.push_back(new Item_empty_string("Master_SSL_Cert",
                                             sizeof(mi->ssl_cert)));
  field_list.push_back(new Item_empty_string("Master_SSL_Cipher",
                                             sizeof(mi->ssl_cipher)));
  field_list.push_back(new Item_empty_string("Master_SSL_Key",
                                             sizeof(mi->ssl_key)));
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

  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  if (mi->host[0])
  {
    DBUG_PRINT("info",("host is set: '%s'", mi->host));
    String *packet= &thd->packet;
    protocol->prepare_for_resend();

    /*
      slave_running can be accessed without run_lock but not other
      non-volotile members like mi->io_thd, which is guarded by the mutex.
    */
    mysql_mutex_lock(&mi->run_lock);
    protocol->store(mi->io_thd ? mi->io_thd->proc_info : "", &my_charset_bin);
    mysql_mutex_unlock(&mi->run_lock);

    mysql_mutex_lock(&mi->data_lock);
    mysql_mutex_lock(&mi->rli.data_lock);
    mysql_mutex_lock(&mi->err_lock);
    mysql_mutex_lock(&mi->rli.err_lock);
    protocol->store(mi->host, &my_charset_bin);
    protocol->store(mi->user, &my_charset_bin);
    protocol->store((uint32) mi->port);
    protocol->store((uint32) mi->connect_retry);
    protocol->store(mi->master_log_name, &my_charset_bin);
    protocol->store((ulonglong) mi->master_log_pos);
    protocol->store(mi->rli.group_relay_log_name +
                    dirname_length(mi->rli.group_relay_log_name),
                    &my_charset_bin);
    protocol->store((ulonglong) mi->rli.group_relay_log_pos);
    protocol->store(mi->rli.group_master_log_name, &my_charset_bin);
    protocol->store(mi->slave_running == MYSQL_SLAVE_RUN_CONNECT ?
                    "Yes" : (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT ?
                             "Connecting" : "No"), &my_charset_bin);
    protocol->store(mi->rli.slave_running ? "Yes":"No", &my_charset_bin);
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

    protocol->store(mi->rli.last_error().number);
    protocol->store(mi->rli.last_error().message, &my_charset_bin);
    protocol->store((uint32) mi->rli.slave_skip_counter);
    protocol->store((ulonglong) mi->rli.group_master_log_pos);
    protocol->store((ulonglong) mi->rli.log_space_total);

    protocol->store(
      mi->rli.until_condition==Relay_log_info::UNTIL_NONE ? "None":
        ( mi->rli.until_condition==Relay_log_info::UNTIL_MASTER_POS? "Master":
          "Relay"), &my_charset_bin);
    protocol->store(mi->rli.until_log_name, &my_charset_bin);
    protocol->store((ulonglong) mi->rli.until_log_pos);

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
      Seconds_Behind_Master: if SQL thread is running and I/O thread is
      connected, we can compute it otherwise show NULL (i.e. unknown).
    */
    if ((mi->slave_running == MYSQL_SLAVE_RUN_CONNECT) &&
        mi->rli.slave_running)
    {
      long time_diff= ((long)(time(0) - mi->rli.last_master_timestamp)
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
      protocol->store((longlong)(mi->rli.last_master_timestamp ?
                                 max(0, time_diff) : 0));
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
    protocol->store(mi->rli.last_error().number);
    // Last_SQL_Error
    protocol->store(mi->rli.last_error().message, &my_charset_bin);
    // Replicate_Ignore_Server_Ids
    {
      char buff[FN_REFLEN];
      ulong i, cur_len;
      for (i= 0, buff[0]= 0, cur_len= 0;
           i < mi->ignore_server_ids.elements; i++)
      {
        ulong s_id, slen;
        char sbuff[FN_REFLEN];
        get_dynamic(&mi->ignore_server_ids, (uchar*) &s_id, i);
        slen= sprintf(sbuff, (i==0? "%lu" : ", %lu"), s_id);
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

    mysql_mutex_unlock(&mi->rli.err_lock);
    mysql_mutex_unlock(&mi->err_lock);
    mysql_mutex_unlock(&mi->rli.data_lock);
    mysql_mutex_unlock(&mi->data_lock);

    if (my_net_write(&thd->net, (uchar*) thd->packet.ptr(), packet->length()))
      DBUG_RETURN(TRUE);
  }
  my_eof(thd);
  DBUG_RETURN(FALSE);
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
  thd->system_thread = (thd_type == SLAVE_THD_SQL) ?
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
    thd->cleanup();
    DBUG_RETURN(-1);
  }

  if (thd_type == SLAVE_THD_SQL)
    thd_proc_info(thd, "Waiting for the next event in relay log");
  else
    thd_proc_info(thd, "Waiting for master update");
  thd->set_time();
  /* Do not use user-supplied timeout value for system threads. */
  thd->variables.lock_wait_timeout= LONG_TIMEOUT;
  DBUG_RETURN(0);
}

/*
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
  const char *old_proc_info;

  mysql_mutex_t *lock= &info->sleep_lock;
  mysql_cond_t *cond= &info->sleep_cond;

  /* Absolute system time at which the sleep time expires. */
  set_timespec(abstime, seconds);
  mysql_mutex_lock(lock);
  old_proc_info= thd->enter_cond(cond, lock, thd->proc_info);

  while (! (ret= func(thd, info)))
  {
    int error= mysql_cond_timedwait(cond, lock, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
  }
  /* Implicitly unlocks the mutex. */
  thd->exit_cond(old_proc_info);
  return ret;
}


static int request_dump(THD *thd, MYSQL* mysql, Master_info* mi,
			bool *suppress_warnings)
{
  uchar buf[FN_REFLEN + 10];
  int len;
  ushort binlog_flags = 0; // for now
  char* logname = mi->master_log_name;
  DBUG_ENTER("request_dump");
  
  *suppress_warnings= FALSE;

  if (RUN_HOOK(binlog_relay_io,
               before_request_transmit,
               (thd, mi, binlog_flags)))
    DBUG_RETURN(1);
  
  // TODO if big log files: Change next to int8store()
  int4store(buf, (ulong) mi->master_log_pos);
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  len = (uint) strlen(logname);
  memcpy(buf + 10, logname,len);
  if (simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
  {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
      *suppress_warnings= TRUE;                 // Suppress reconnect warning
    else
      sql_print_error("Error on COM_BINLOG_DUMP: %d  %s, will retry in %d secs",
                      mysql_errno(mysql), mysql_error(mysql),
                      mi->connect_retry);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
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
  if (disconnect_slave_event_count && !(mi->events_till_disconnect--))
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

/*
  Check if the current error is of temporary nature of not.
  Some errors are temporary in nature, such as
  ER_LOCK_DEADLOCK and ER_LOCK_WAIT_TIMEOUT.  Ndb also signals
  that the error is temporary by pushing a warning with the error code
  ER_GET_TEMPORARY_ERRMSG, if the originating error is temporary.
*/
static int has_temporary_error(THD *thd)
{
  DBUG_ENTER("has_temporary_error");

  DBUG_EXECUTE_IF("all_errors_are_temporary_errors",
                  if (thd->stmt_da->is_error())
                  {
                    thd->clear_error();
                    my_error(ER_LOCK_DEADLOCK, MYF(0));
                  });

  /*
    If there is no message in THD, we can't say if it's a temporary
    error or not. This is currently the case for Incident_log_event,
    which sets no message. Return FALSE.
  */
  if (!thd->is_error())
    DBUG_RETURN(0);

  /*
    Temporary error codes:
    currently, InnoDB deadlock detected by InnoDB or lock
    wait timeout (innodb_lock_wait_timeout exceeded
  */
  if (thd->stmt_da->sql_errno() == ER_LOCK_DEADLOCK ||
      thd->stmt_da->sql_errno() == ER_LOCK_WAIT_TIMEOUT)
    DBUG_RETURN(1);

#ifdef HAVE_NDB_BINLOG
  /*
    currently temporary error set in ndbcluster
  */
  List_iterator_fast<MYSQL_ERROR> it(thd->warning_info->warn_list());
  MYSQL_ERROR *err;
  while ((err= it++))
  {
    DBUG_PRINT("info", ("has condition %d %s", err->get_sql_errno(),
                        err->get_message_text()));
    switch (err->get_sql_errno())
    {
    case ER_GET_TEMPORARY_ERRMSG:
      DBUG_RETURN(1);
    default:
      break;
    }
  }
#endif
  DBUG_RETURN(0);
}


/**
  Applies the given event and advances the relay log position.

  In essence, this function does:

  @code
    ev->apply_event(rli);
    ev->update_pos(rli);
  @endcode

  But it also does some maintainance, such as skipping events if
  needed and reporting errors.

  If the @c skip flag is set, then it is tested whether the event
  should be skipped, by looking at the slave_skip_counter and the
  server id.  The skip flag should be set when calling this from a
  replication thread but not set when executing an explicit BINLOG
  statement.

  @retval 0 OK.

  @retval 1 Error calling ev->apply_event().

  @retval 2 No error calling ev->apply_event(), but error calling
  ev->update_pos().
*/
int apply_event_and_update_pos(Log_event* ev, THD* thd, Relay_log_info* rli)
{
  int exec_res= 0;

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

  thd->server_id = ev->server_id; // use the original server id for logging
  thd->set_time();                            // time the query
  thd->lex->current_select= 0;
  if (!ev->when)
    ev->when= my_time(0);
  ev->thd = thd; // because up to this point, ev->thd == 0

  int reason= ev->shall_skip(rli);
  if (reason == Log_event::EVENT_SKIP_COUNT)
    sql_slave_skip_counter= --rli->slave_skip_counter;
  mysql_mutex_unlock(&rli->data_lock);
  if (reason == Log_event::EVENT_SKIP_NOT)
    exec_res= ev->apply_event(rli);

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

  DBUG_PRINT("info", ("apply_event error = %d", exec_res));
  if (exec_res == 0)
  {
    int error= ev->update_pos(rli);
#ifdef HAVE_purify
    if (!rli->is_fake)
#endif
    {
#ifndef DBUG_OFF
      char buf[22];
#endif
      DBUG_PRINT("info", ("update_pos error = %d", error));
      DBUG_PRINT("info", ("group %s %s",
                          llstr(rli->group_relay_log_pos, buf),
                          rli->group_relay_log_name));
      DBUG_PRINT("info", ("event %s %s",
                          llstr(rli->event_relay_log_pos, buf),
                          rli->event_relay_log_name));
    }
    /*
      The update should not fail, so print an error message and
      return an error code.

      TODO: Replace this with a decent error message when merged
      with BUG#24954 (which adds several new error message).
    */
    if (error)
    {
      char buf[22];
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "It was not possible to update the positions"
                  " of the relay log information: the slave may"
                  " be in an inconsistent state."
                  " Stopped in %s position %s",
                  rli->group_relay_log_name,
                  llstr(rli->group_relay_log_pos, buf));
      DBUG_RETURN(2);
    }
  }

  DBUG_RETURN(exec_res ? 1 : 0);
}


/**
  Top-level function for executing the next event from the relay log.

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

   - init_master_info or init_relay_log_pos failed. (These are called
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

  Log_event * ev = next_event(rli);

  DBUG_ASSERT(rli->sql_thd==thd);

  if (sql_slave_killed(thd,rli))
  {
    mysql_mutex_unlock(&rli->data_lock);
    delete ev;
    DBUG_RETURN(1);
  }
  if (ev)
  {
    int exec_res;

    /*
      This tests if the position of the beginning of the current event
      hits the UNTIL barrier.
    */
    if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
        rli->is_until_satisfied(thd, ev))
    {
      char buf[22];
      sql_print_information("Slave SQL thread stopped because it reached its"
                            " UNTIL position %s", llstr(rli->until_pos(), buf));
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
                        DBUG_ASSERT(thd->transaction.all.modified_non_trans_table);
                        rli->abort_slave= 1;
                        mysql_mutex_unlock(&rli->data_lock);
                        delete ev;
                        rli->inc_event_relay_log_pos();
                        DBUG_RETURN(0);
                      };);
    }

    exec_res= apply_event_and_update_pos(ev, thd, rli);

    /*
      Format_description_log_event should not be deleted because it will be
      used to read info about the relay log's format; it will be deleted when
      the SQL thread does not need it, i.e. when this thread terminates.
    */
    if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT &&
        !rli->is_deferred_event(ev))
    {
      DBUG_PRINT("info", ("Deleting the event after it has been executed"));
      delete ev;
    }

    /*
      update_log_pos failed: this should not happen, so we don't
      retry.
    */
    if (exec_res == 2)
      DBUG_RETURN(1);

    if (slave_trans_retries)
    {
      int UNINIT_VAR(temp_err);
      if (exec_res && (temp_err= has_temporary_error(thd)))
      {
        const char *errmsg;
        /*
          We were in a transaction which has been rolled back because of a
          temporary error;
          let's seek back to BEGIN log event and retry it all again.
	  Note, if lock wait timeout (innodb_lock_wait_timeout exceeded)
	  there is no rollback since 5.0.13 (ref: manual).
          We have to not only seek but also
          a) init_master_info(), to seek back to hot relay log's start for later
          (for when we will come back to this hot log after re-processing the
          possibly existing old logs where BEGIN is: check_binlog_magic() will
          then need the cache to be at position 0 (see comments at beginning of
          init_master_info()).
          b) init_relay_log_pos(), because the BEGIN may be an older relay log.
        */
        if (rli->trans_retries < slave_trans_retries)
        {
          if (init_master_info(rli->mi, 0, 0, 0, SLAVE_SQL))
            sql_print_error("Failed to initialize the master info structure");
          else if (init_relay_log_pos(rli,
                                      rli->group_relay_log_name,
                                      rli->group_relay_log_pos,
                                      1, &errmsg, 1))
            sql_print_error("Error initializing relay log position: %s",
                            errmsg);
          else
          {
            exec_res= 0;
            rli->cleanup_context(thd, 1);
            /* chance for concurrent connection to get more locks */
            slave_sleep(thd, min(rli->trans_retries, MAX_SLAVE_RETRY_PAUSE),
                       sql_slave_killed, rli);
            mysql_mutex_lock(&rli->data_lock); // because of SHOW STATUS
            rli->trans_retries++;
            rli->retried_trans++;
            mysql_mutex_unlock(&rli->data_lock);
            DBUG_PRINT("info", ("Slave retries transaction "
                                "rli->trans_retries: %lu", rli->trans_retries));
          }
        }
        else
          sql_print_error("Slave SQL thread retried transaction %lu time(s) "
                          "in vain, giving up. Consider raising the value of "
                          "the slave_transaction_retries variable.",
                          slave_trans_retries);
      }
      else if ((exec_res && !temp_err) ||
               (opt_using_transactions &&
                rli->group_relay_log_pos == rli->event_relay_log_pos))
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
    if (info && global_system_variables.log_warnings)
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
  if it exceeds @c master_retry_count then connection is not re-established
  and function signals error.
  Unless @c suppres_warnings is TRUE, a warning is put in the server error log
  when reconnecting. The warning message and messages used to report errors
  are taken from @c messages array. In case @c master_retry_count is exceeded,
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
    if (*retry_count > master_retry_count)
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
                IO_RPL_LOG_NAME, llstr(mi->master_log_pos, llbuff));
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
    if (global_system_variables.log_warnings)
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
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql;
  Master_info *mi = (Master_info*)arg;
  Relay_log_info *rli= &mi->rli;
  char llbuff[22];
  uint retry_count;
  bool suppress_warnings;
  int ret;
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
  mi->events_till_disconnect = disconnect_slave_event_count;
#endif

  thd= new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);
  mi->io_thd = thd;

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
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  mi->slave_running = 1;
  mi->abort_slave = 0;
  mysql_mutex_unlock(&mi->run_lock);
  mysql_cond_broadcast(&mi->start_cond);

  DBUG_PRINT("master_info",("log_file_name: '%s'  position: %s",
                            mi->master_log_name,
                            llstr(mi->master_log_pos,llbuff)));

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

  thd_proc_info(thd, "Connecting to master");
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
  {
    sql_print_information("Slave I/O thread: connected to master '%s@%s:%d',"
                          "replication started in log '%s' at position %s",
                          mi->user, mi->host, mi->port,
			  IO_RPL_LOG_NAME,
			  llstr(mi->master_log_pos,llbuff));
  /*
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

  // TODO: the assignment below should be under mutex (5.0)
  mi->slave_running= MYSQL_SLAVE_RUN_CONNECT;
  thd->slave_net = &mysql->net;
  thd_proc_info(thd, "Checking master version");
  ret= get_master_version_and_clock(mysql, mi);
  if (ret == 1)
    /* Fatal error */
    goto err;

  if (ret == 2) 
  { 
    if (check_io_slave_killed(mi->io_thd, mi, "Slave I/O thread killed"
                              "while calling get_master_version_and_clock(...)"))
      goto err;
    suppress_warnings= FALSE;
    /* Try to reconnect because the error was caused by a transient network problem */
    if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
      goto err;
    goto connected;
  } 

  if (mi->rli.relay_log.description_event_for_queue->binlog_version > 1)
  {
    /*
      Register ourselves with the master.
    */
    thd_proc_info(thd, "Registering slave on master");
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
    thd_proc_info(thd, "Requesting binlog dump");
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
      thd_proc_info(thd, "Waiting for master to send event");
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
      thd_proc_info(thd, "Queueing master event to the relay log");
      event_buf= (const char*)mysql->net.read_pos + 1;
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

      if (flush_master_info(mi, TRUE, TRUE))
      {
        sql_print_error("Failed to flush master info file");
        goto err;
      }
      /*
        See if the relay logs take too much space.
        We don't lock mi->rli.log_space_lock here; this dirty read saves time
        and does not introduce any problem:
        - if mi->rli.ignore_log_space_limit is 1 but becomes 0 just after (so
        the clean value is 0), then we are reading only one more event as we
        should, and we'll block only at the next event. No big deal.
        - if mi->rli.ignore_log_space_limit is 0 but becomes 1 just after (so
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
                  IO_RPL_LOG_NAME, llstr(mi->master_log_pos,llbuff));
  RUN_HOOK(binlog_relay_io, thread_stop, (thd, mi));
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
  write_ignored_events_info_to_relay_log(thd, mi);
  thd_proc_info(thd, "Waiting for slave mutex on exit");
  mysql_mutex_lock(&mi->run_lock);

  /* Forget the relay log's format */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;
  // TODO: make rpl_status part of Master_info
  change_rpl_status(RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE);
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0
  mysql_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  mysql_mutex_unlock(&LOCK_thread_count);
  mi->abort_slave= 0;
  mi->slave_running= 0;
  mi->io_thd= 0;
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
  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
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
  if ((fd= mysql_file_create(key_file_misc,
                             tmp_file, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0)
  DBUG_RETURN(1);

  /*
    Clean up.
   */
  mysql_file_close(fd, MYF(0));
  mysql_file_delete(key_file_misc, tmp_file, MYF(0));

  DBUG_RETURN(0);
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
  char llbuff[22],llbuff1[22];
  char saved_log_name[FN_REFLEN];
  char saved_master_log_name[FN_REFLEN];
  my_off_t UNINIT_VAR(saved_log_pos);
  my_off_t UNINIT_VAR(saved_master_log_pos);
  my_off_t saved_skip= 0;

  Relay_log_info* rli = &((Master_info*)arg)->rli;
  const char *errmsg;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_sql");

  DBUG_ASSERT(rli->inited);
  mysql_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
  errmsg= 0;
#ifndef DBUG_OFF
  rli->events_till_abort = abort_slave_event_count;
#endif

  thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->thread_stack = (char*)&thd; // remember where our stack is
  rli->sql_thd= thd;
  
  /* Inform waiting threads that slave has started */
  rli->slave_run_id++;
  rli->slave_running = 1;

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
  thd->init_for_queries();
  thd->rli_slave= rli;
  if ((rli->deferred_events_collecting= rpl_filter->is_on()))
  {
    rli->deferred_events= new Deferred_log_events(rli);
  }

  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  set_thd_in_use_temporary_tables(rli);   // (re)set sql_thd in use for saved temp tables
  mysql_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  /*
    We are going to set slave_running to 1. Assuming slave I/O thread is
    alive and connected, this is going to make Seconds_Behind_Master be 0
    i.e. "caught up". Even if we're just at start of thread. Well it's ok, at
    the moment we start we can think we are caught up, and the next second we
    start receiving data so we realize we are not caught up and
    Seconds_Behind_Master grows. No big deal.
  */
  rli->abort_slave = 0;
  mysql_mutex_unlock(&rli->run_lock);
  mysql_cond_broadcast(&rli->start_cond);

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

  //tell the I/O thread to take relay_log_space_limit into account from now on
  mysql_mutex_lock(&rli->log_space_lock);
  rli->ignore_log_space_limit= 0;
  mysql_mutex_unlock(&rli->log_space_lock);
  rli->trans_retries= 0; // start from "no error"
  DBUG_PRINT("info", ("rli->trans_retries: %lu", rli->trans_retries));

  if (init_relay_log_pos(rli,
                         rli->group_relay_log_name,
                         rli->group_relay_log_pos,
                         1 /*need data lock*/, &errmsg,
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
                        llstr(rli->event_relay_log_pos,llbuf2)));
    DBUG_ASSERT(rli->event_relay_log_pos >= BIN_LOG_HEADER_SIZE);
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
    DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->event_relay_log_pos);
#endif
  }
#endif
  DBUG_ASSERT(rli->sql_thd == thd);

  DBUG_PRINT("master_info",("log_file_name: %s  position: %s",
                            rli->group_master_log_name,
                            llstr(rli->group_master_log_pos,llbuff)));
  if (global_system_variables.log_warnings)
    sql_print_information("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", RPL_LOG_NAME,
                    llstr(rli->group_master_log_pos,llbuff),rli->group_relay_log_name,
                    llstr(rli->group_relay_log_pos,llbuff1));

  if (check_temp_dir(rli->slave_patternload_file))
  {
    rli->report(ERROR_LEVEL, thd->stmt_da->sql_errno(), 
                "Unable to use slave's temporary directory %s - %s", 
                slave_load_tmpdir, thd->stmt_da->message());
    goto err;
  }

  /* execute init_slave variable */
  if (opt_init_slave.length)
  {
    execute_init_command(thd, &opt_init_slave, &LOCK_sys_init_slave);
    if (thd->is_slave_error)
    {
      rli->report(ERROR_LEVEL, thd->stmt_da->sql_errno(),
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
    strmake(saved_log_name, rli->group_relay_log_name, FN_REFLEN - 1);
    strmake(saved_master_log_name, rli->group_master_log_name, FN_REFLEN - 1);
    saved_log_pos= rli->group_relay_log_pos;
    saved_master_log_pos= rli->group_master_log_pos;
    saved_skip= rli->slave_skip_counter;
  }
  if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
      rli->is_until_satisfied(thd, NULL))
  {
    char buf[22];
    sql_print_information("Slave SQL thread stopped because it reached its"
                          " UNTIL position %s", llstr(rli->until_pos(), buf));
    mysql_mutex_unlock(&rli->data_lock);
    goto err;
  }
  mysql_mutex_unlock(&rli->data_lock);

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    thd_proc_info(thd, "Reading event from the relay log");
    DBUG_ASSERT(rli->sql_thd == thd);
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
        rli->group_relay_log_name, (ulong) rli->group_relay_log_pos,
        rli->group_master_log_name, (ulong) rli->group_master_log_pos);
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
          char const *const errmsg= thd->stmt_da->message();

          DBUG_PRINT("info",
                     ("thd->stmt_da->sql_errno()=%d; rli->last_error.number=%d",
                      thd->stmt_da->sql_errno(), last_errno));
          if (last_errno == 0)
          {
            /*
 	      This function is reporting an error which was not reported
 	      while executing exec_relay_log_event().
 	    */ 
            rli->report(ERROR_LEVEL, thd->stmt_da->sql_errno(), "%s", errmsg);
          }
          else if (last_errno != thd->stmt_da->sql_errno())
          {
            /*
             * An error was reported while executing exec_relay_log_event()
             * however the error code differs from what is in the thread.
             * This function prints out more information to help finding
             * what caused the problem.
             */  
            sql_print_error("Slave (additional info): %s Error_code: %d",
                            errmsg, thd->stmt_da->sql_errno());
          }
        }

        /* Print any warnings issued */
        List_iterator_fast<MYSQL_ERROR> it(thd->warning_info->warn_list());
        MYSQL_ERROR *err;
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
            "position %s", RPL_LOG_NAME, llstr(rli->group_master_log_pos, 
            llbuff));
        else
          sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s", RPL_LOG_NAME, llstr(rli->group_master_log_pos, llbuff));
      }
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_information("Slave SQL thread exiting, replication stopped in log "
                        "'%s' at position %s",
                        RPL_LOG_NAME, llstr(rli->group_master_log_pos,llbuff));

 err:

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
  thd_proc_info(thd, "Waiting for slave mutex on exit");
  mysql_mutex_lock(&rli->run_lock);
  /* We need data_lock, at least to wake up any waiting master_pos_wait() */
  mysql_mutex_lock(&rli->data_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  /* When master_pos_wait() wakes up it will check this and terminate */
  rli->slave_running= 0;
  /* Forget the relay log's format */
  delete rli->relay_log.description_event_for_exec;
  rli->relay_log.description_event_for_exec= 0;
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
  DBUG_ASSERT(rli->sql_thd == thd);
  THD_CHECK_SENTRY(thd);
  rli->sql_thd= 0;
  set_thd_in_use_temporary_tables(rli);  // (re)set sql_thd in use for saved temp tables
  mysql_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  mysql_mutex_unlock(&LOCK_thread_count);
 /*
  Note: the order of the broadcast and unlock calls below (first broadcast, then unlock)
  is important. Otherwise a killer_thread can execute between the calls and
  delete the mi structure leading to a crash! (see BUG#25306 for details)
 */ 
  mysql_cond_broadcast(&rli->stop_cond);
  DBUG_EXECUTE_IF("simulate_slave_delay_at_terminate_bug38694", sleep(5););
  mysql_mutex_unlock(&rli->run_lock);  // tell the world we are done

  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
}


/*
  process_io_create_file()
*/

static int process_io_create_file(Master_info* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD *thd = mi->io_thd;
  NET *net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

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
        if (unlikely(mi->rli.relay_log.append(&xev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Exec_load event to relay log");
          goto err;
        }
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
        break;
      }
      if (unlikely(cev_not_written))
      {
        cev->block = net->read_pos;
        cev->block_len = num_bytes;
        if (unlikely(mi->rli.relay_log.append(cev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Create_file event to relay log");
          goto err;
        }
        cev_not_written=0;
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
      }
      else
      {
        aev.block = net->read_pos;
        aev.block_len = num_bytes;
        aev.log_pos = cev->log_pos;
        if (unlikely(mi->rli.relay_log.append(&aev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Append_block event to relay log");
          goto err;
        }
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total) ;
      }
    }
  }
  error=0;
err:
  DBUG_RETURN(error);
}


/*
  Start using a new binary log on the master

  SYNOPSIS
    process_io_rotate()
    mi                  master_info for the slave
    rev                 The rotate log event read from the binary log

  DESCRIPTION
    Updates the master info with the place in the next binary
    log where we should start reading.
    Rotate the relay log to avoid mixed-format relay logs.

  NOTES
    We assume we already locked mi->data_lock

  RETURN VALUES
    0           ok
    1           Log event is illegal

*/

static int process_io_rotate(Master_info *mi, Rotate_log_event *rev)
{
  DBUG_ENTER("process_io_rotate");
  mysql_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);

  /* Safe copy as 'rev' has been "sanitized" in Rotate_log_event's ctor */
  memcpy(mi->master_log_name, rev->new_log_ident, rev->ident_len+1);
  mi->master_log_pos= rev->pos;
  DBUG_PRINT("info", ("master_log_pos: '%s' %lu",
                      mi->master_log_name, (ulong) mi->master_log_pos));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    mi->events_till_disconnect++;
#endif

  /*
    If description_event_for_queue is format <4, there is conversion in the
    relay log to the slave's format (4). And Rotate can mean upgrade or
    nothing. If upgrade, it's to 5.0 or newer, so we will get a Format_desc, so
    no need to reset description_event_for_queue now. And if it's nothing (same
    master version as before), no need (still using the slave's format).
  */
  if (mi->rli.relay_log.description_event_for_queue->binlog_version >= 4)
  {
    delete mi->rli.relay_log.description_event_for_queue;
    /* start from format 3 (MySQL 4.0) again */
    mi->rli.relay_log.description_event_for_queue= new
      Format_description_log_event(3);
  }
  /*
    Rotate the relay log makes binlog format detection easier (at next slave
    start or mysqlbinlog)
  */
  DBUG_RETURN(rotate_relay_log(mi) /* will take the right mutexes */);
}

/*
  Reads a 3.23 event and converts it to the slave's format. This code was
  copied from MySQL 4.0.
*/
static int queue_binlog_ver_1_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  bool ignore_event= 0;
  char *tmp_buf = 0;
  Relay_log_info *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_1_event");

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
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
                                            mi->rli.relay_log.description_event_for_queue);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free(tmp_buf);
    DBUG_RETURN(1);
  }

  mysql_mutex_lock(&mi->data_lock);
  ev->log_pos= mi->master_log_pos; /* 3.23 events don't contain log_pos */
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    ignore_event= 1;
    inc_pos= event_len;
    break;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      mysql_mutex_unlock(&mi->data_lock);
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
    mi->master_log_pos += inc_pos;
    DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
    mysql_mutex_unlock(&mi->data_lock);
    my_free(tmp_buf);
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
    if (unlikely(rli->relay_log.append(ev)))
    {
      delete ev;
      mysql_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  }
  delete ev;
  mi->master_log_pos+= inc_pos;
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
  mysql_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(0);
}

/*
  Reads a 4.0 event and converts it to the slave's format. This code was copied
  from queue_binlog_ver_1_event(), with some affordable simplifications.
*/
static int queue_binlog_ver_3_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  char *tmp_buf = 0;
  Relay_log_info *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_3_event");

  /* read_log_event() will adjust log_pos to be end_log_pos */
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
                                            mi->rli.relay_log.description_event_for_queue);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free(tmp_buf);
    DBUG_RETURN(1);
  }
  mysql_mutex_lock(&mi->data_lock);
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    goto err;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      mysql_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    inc_pos= 0;
    break;
  default:
    inc_pos= event_len;
    break;
  }
  if (unlikely(rli->relay_log.append(ev)))
  {
    delete ev;
    mysql_mutex_unlock(&mi->data_lock);
    DBUG_RETURN(1);
  }
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  delete ev;
  mi->master_log_pos+= inc_pos;
err:
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
  mysql_mutex_unlock(&mi->data_lock);
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

  switch (mi->rli.relay_log.description_event_for_queue->binlog_version)
  {
  case 1:
      DBUG_RETURN(queue_binlog_ver_1_event(mi,buf,event_len));
  case 3:
      DBUG_RETURN(queue_binlog_ver_3_event(mi,buf,event_len));
  default: /* unsupported format; eg version 2 */
    DBUG_PRINT("info",("unsupported binlog format %d in queue_old_event()",
                       mi->rli.relay_log.description_event_for_queue->binlog_version));
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
  ulong inc_pos;
  Relay_log_info *rli= &mi->rli;
  mysql_mutex_t *log_lock= rli->relay_log.get_log_lock();
  ulong s_id;
  DBUG_ENTER("queue_event");

  LINT_INIT(inc_pos);

  if (mi->rli.relay_log.description_event_for_queue->binlog_version<4 &&
      buf[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT /* a way to escape */)
    DBUG_RETURN(queue_old_event(mi,buf,event_len));

  LINT_INIT(inc_pos);
  mysql_mutex_lock(&mi->data_lock);

  switch (buf[EVENT_TYPE_OFFSET]) {
  case STOP_EVENT:
    /*
      We needn't write this event to the relay log. Indeed, it just indicates a
      master server shutdown. The only thing this does is cleaning. But
      cleaning is already done on a per-master-thread basis (as the master
      server is shutting down cleanly, it has written all DROP TEMPORARY TABLE
      prepared statements' deletion are TODO only when we binlog prep stmts).

      We don't even increment mi->master_log_pos, because we may be just after
      a Rotate event. Btw, in a few milliseconds we are going to have a Start
      event from the next binlog (unless the master is presently running
      without --log-bin).
    */
    goto err;
  case ROTATE_EVENT:
  {
    Rotate_log_event rev(buf,event_len,mi->rli.relay_log.description_event_for_queue);
    if (unlikely(process_io_rotate(mi,&rev)))
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
      goto err;
    }
    /*
      Now the I/O thread has just changed its mi->master_log_name, so
      incrementing mi->master_log_pos is nonsense.
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
      We are the only thread which reads/writes description_event_for_queue.
      The relay_log struct does not move (though some members of it can
      change), so we needn't any lock (no rli->data_lock, no log lock).
    */
    Format_description_log_event* tmp;
    const char* errmsg;
    if (!(tmp= (Format_description_log_event*)
          Log_event::read_log_event(buf, event_len, &errmsg,
                                    mi->rli.relay_log.description_event_for_queue)))
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
      goto err;
    }
    delete mi->rli.relay_log.description_event_for_queue;
    mi->rli.relay_log.description_event_for_queue= tmp;
    /*
       Though this does some conversion to the slave's format, this will
       preserve the master's binlog format version, and number of event types.
    */
    /*
       If the event was not requested by the slave (the slave did not ask for
       it), i.e. has end_log_pos=0, we do not increment mi->master_log_pos
    */
    inc_pos= uint4korr(buf+LOG_POS_OFFSET) ? event_len : 0;
    DBUG_PRINT("info",("binlog format is now %d",
                       mi->rli.relay_log.description_event_for_queue->binlog_version));

  }
  break;

  case HEARTBEAT_LOG_EVENT:
  {
    /*
      HB (heartbeat) cannot come before RL (Relay)
    */
    char  llbuf[22];
    Heartbeat_log_event hb(buf, event_len, mi->rli.relay_log.description_event_for_queue);
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
    /* 
       compare local and event's versions of log_file, log_pos.
       
       Heartbeat is sent only after an event corresponding to the corrdinates
       the heartbeat carries.
       Slave can not have a difference in coordinates except in the only
       special case when mi->master_log_name, master_log_pos have never
       been updated by Rotate event i.e when slave does not have any history
       with the master (and thereafter mi->master_log_pos is NULL).

       TODO: handling `when' for SHOW SLAVE STATUS' snds behind
    */
    if ((memcmp(mi->master_log_name, hb.get_log_ident(), hb.get_ident_len())
         && mi->master_log_name != NULL)
        || mi->master_log_pos != hb.log_pos)
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
  if ((s_id == ::server_id && !mi->rli.replicate_same_server_id) ||
      /*
        the following conjunction deals with IGNORE_SERVER_IDS, if set
        If the master is on the ignore list, execution of
        format description log events and rotate events is necessary.
      */
      (mi->ignore_server_ids.elements > 0 &&
       mi->shall_ignore_server_id(s_id) &&
       /* everything is filtered out from non-master */
       (s_id != mi->master_id ||
        /* for the master meta information is necessary */
        (buf[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT &&
         buf[EVENT_TYPE_OFFSET] != ROTATE_EVENT))))
  {
    /*
      Do not write it to the relay log.
      a) We still want to increment mi->master_log_pos, so that we won't
      re-read this event from the master if the slave IO thread is now
      stopped/restarted (more efficient if the events we are ignoring are big
      LOAD DATA INFILE).
      b) We want to record that we are skipping events, for the information of
      the slave SQL thread, otherwise that thread may let
      rli->group_relay_log_pos stay too small if the last binlog's event is
      ignored.
      But events which were generated by this slave and which do not exist in
      the master's binlog (i.e. Format_desc, Rotate & Stop) should not increment
      mi->master_log_pos.
      If the event is originated remotely and is being filtered out by
      IGNORE_SERVER_IDS it increments mi->master_log_pos
      as well as rli->group_relay_log_pos.
    */
    if (!(s_id == ::server_id && !mi->rli.replicate_same_server_id) ||
        (buf[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT &&
         buf[EVENT_TYPE_OFFSET] != ROTATE_EVENT &&
         buf[EVENT_TYPE_OFFSET] != STOP_EVENT))
    {
      mi->master_log_pos+= inc_pos;
      memcpy(rli->ign_master_log_name_end, mi->master_log_name, FN_REFLEN);
      DBUG_ASSERT(rli->ign_master_log_name_end[0]);
      rli->ign_master_log_pos_end= mi->master_log_pos;
    }
    rli->relay_log.signal_update(); // the slave SQL thread needs to re-check
    DBUG_PRINT("info", ("master_log_pos: %lu, event originating from %u server, ignored",
                        (ulong) mi->master_log_pos, uint4korr(buf + SERVER_ID_OFFSET)));
  }
  else
  {
    /* write the event to the relay log */
    if (likely(!(rli->relay_log.appendv(buf,event_len,0))))
    {
      mi->master_log_pos+= inc_pos;
      DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
    }
    else
    {
      error= ER_SLAVE_RELAY_LOG_WRITE_FAILURE;
    }
    rli->ign_master_log_name_end[0]= 0; // last event is not ignored
  }
  mysql_mutex_unlock(log_lock);

skip_relay_logging:
  
err:
  mysql_mutex_unlock(&mi->data_lock);
  DBUG_PRINT("info", ("error: %d", error));
  if (error)
    mi->report(ERROR_LEVEL, error, ER(error), 
               (error == ER_SLAVE_RELAY_LOG_WRITE_FAILURE)?
               "could not queue event from master" :
               error_msg.ptr());
  DBUG_RETURN(error);
}


void end_relay_log_info(Relay_log_info* rli)
{
  DBUG_ENTER("end_relay_log_info");

  if (!rli->inited)
    DBUG_VOID_RETURN;
  if (rli->info_fd >= 0)
  {
    end_io_cache(&rli->info_file);
    mysql_file_close(rli->info_fd, MYF(MY_WME));
    rli->info_fd = -1;
  }
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    mysql_file_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  rli->inited = 0;
  rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  /*
    Delete the slave's temporary tables from memory.
    In the future there will be other actions than this, to ensure persistance
    of slave's temp tables after shutdown.
  */
  rli->close_temporary_tables();
  DBUG_VOID_RETURN;
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
    master_retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, Master_info* mi,
                             bool reconnect, bool suppress_warnings)
{
  int slave_was_killed;
  int last_errno= -2;                           // impossible error
  ulong err_count=0;
  char llbuff[22];
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  mi->events_till_disconnect = disconnect_slave_event_count;
#endif
  ulong client_flag= CLIENT_REMEMBER_OPTIONS;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;                /* We will use compression */

  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);

#ifdef HAVE_OPENSSL
  if (mi->ssl)
  {
    mysql_ssl_set(mysql,
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0,
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &mi->ssl_verify_server_cert);
  }
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

  /* Set MYSQL_PLUGIN_DIR in case master asks for an external authentication plugin */
  if (opt_plugin_dir_ptr && *opt_plugin_dir_ptr)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir_ptr);

  /* we disallow empty users */
  if (mi->user == NULL || mi->user[0] == 0)
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR),
               "Invalid (empty) username when attempting to "
               "connect to the master server. Connection attempt "
               "terminated.");
    DBUG_RETURN(1);
  }
  while (!(slave_was_killed = io_slave_killed(thd,mi)) &&
         (reconnect ? mysql_reconnect(mysql) != 0 :
          mysql_real_connect(mysql, mi->host, mi->user, mi->password, 0,
                             mi->port, 0, client_flag) == 0))
  {
    /* Don't repeat last error */
    if ((int)mysql_errno(mysql) != last_errno)
    {
      last_errno=mysql_errno(mysql);
      suppress_warnings= 0;
      mi->report(ERROR_LEVEL, last_errno,
                 "error %s to master '%s@%s:%d'"
                 " - retry-time: %d  retries: %lu",
                 (reconnect ? "reconnecting" : "connecting"),
                 mi->user, mi->host, mi->port,
                 mi->connect_retry, master_retry_count);
    }
    /*
      By default we try forever. The reason is that failure will trigger
      master election, so if the user did not set master_retry_count we
      do not want to have election triggered on the first failure to
      connect
    */
    if (++err_count == master_retry_count)
    {
      slave_was_killed=1;
      if (reconnect)
        change_rpl_status(RPL_ACTIVE_SLAVE,RPL_LOST_SOLDIER);
      break;
    }
    slave_sleep(thd,mi->connect_retry,io_slave_killed, mi);
  }

  if (!slave_was_killed)
  {
    mi->clear_error(); // clear possible left over reconnect error
    if (reconnect)
    {
      if (!suppress_warnings && global_system_variables.log_warnings)
        sql_print_information("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->user,
                        mi->host, mi->port,
                        IO_RPL_LOG_NAME,
                        llstr(mi->master_log_pos,llbuff));
    }
    else
    {
      change_rpl_status(RPL_IDLE_SLAVE,RPL_ACTIVE_SLAVE);
      general_log_print(thd, COM_CONNECT_OUT, "%s@%s:%d",
                        mi->user, mi->host, mi->port);
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
    master_retry_count times
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

#ifdef HAVE_OPENSSL
  if (mi->ssl)
  {
    mysql_ssl_set(mysql,
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0,
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &mi->ssl_verify_server_cert);
  }
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

  if (mi->user == NULL
      || mi->user[0] == 0
      || io_slave_killed(thd, mi)
      || !mysql_real_connect(mysql, mi->host, mi->user, mi->password, 0,
                             mi->port, 0, 0))
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
  Store the file and position where the execute-slave thread are in the
  relay log.

  SYNOPSIS
    flush_relay_log_info()
    rli                 Relay log information

  NOTES
    - As this is only called by the slave thread or on STOP SLAVE, with the
      log_lock grabbed and the slave thread stopped, we don't need to have 
      a lock here.
    - If there is an active transaction, then we don't update the position
      in the relay log.  This is to ensure that we re-execute statements
      if we die in the middle of an transaction that was rolled back.
    - As a transaction never spans binary logs, we don't have to handle the
      case where we do a relay-log-rotation in the middle of the transaction.
      If this would not be the case, we would have to ensure that we
      don't delete the relay log file where the transaction started when
      we switch to a new relay log file.

  TODO
    - Change the log file information to a binary format to avoid calling
      longlong2str.

  RETURN VALUES
    0   ok
    1   write error
*/

bool flush_relay_log_info(Relay_log_info* rli)
{
  bool error=0;
  DBUG_ENTER("flush_relay_log_info");

  if (unlikely(rli->no_storage))
    DBUG_RETURN(0);

  IO_CACHE *file = &rli->info_file;
  char buff[FN_REFLEN*2+22*2+4], *pos;

  my_b_seek(file, 0L);
  pos=strmov(buff, rli->group_relay_log_name);
  *pos++='\n';
  pos=longlong2str(rli->group_relay_log_pos, pos, 10);
  *pos++='\n';
  pos=strmov(pos, rli->group_master_log_name);
  *pos++='\n';
  pos=longlong2str(rli->group_master_log_pos, pos, 10);
  *pos='\n';
  if (my_b_write(file, (uchar*) buff, (size_t) (pos-buff)+1))
    error=1;
  if (flush_io_cache(file))
    error=1;
  if (sync_relayloginfo_period &&
      !error &&
      ++(rli->sync_counter) >= sync_relayloginfo_period)
  {
    if (my_sync(rli->info_fd, MYF(MY_WME)))
      error=1;
    rli->sync_counter= 0;
  }
  /* 
    Flushing the relay log is done by the slave I/O thread 
    or by the user on STOP SLAVE. 
   */
  DBUG_RETURN(error);
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
  if ((rli->cur_log_fd=open_binlog(cur_log,rli->event_relay_log_name,
                                   errmsg)) <0)
    DBUG_RETURN(0);
  /*
    We want to start exactly where we was before:
    relay_log_pos       Current log pos
    pending             Number of bytes already processed from the event
  */
  rli->event_relay_log_pos= max(rli->event_relay_log_pos, BIN_LOG_HEADER_SIZE);
  my_b_seek(cur_log,rli->event_relay_log_pos);
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
  THD* thd = rli->sql_thd;
  DBUG_ENTER("next_event");

  DBUG_ASSERT(thd != 0);

#ifndef DBUG_OFF
  if (abort_slave_event_count && !rli->events_till_abort--)
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
      /* This is an assertion which sometimes fails, let's try to track it */
      char llbuf1[22], llbuf2[22];
      DBUG_PRINT("info", ("my_b_tell(cur_log)=%s rli->event_relay_log_pos=%s",
                          llstr(my_b_tell(cur_log),llbuf1),
                          llstr(rli->event_relay_log_pos,llbuf2)));
      DBUG_ASSERT(my_b_tell(cur_log) >= BIN_LOG_HEADER_SIZE);
      DBUG_ASSERT(my_b_tell(cur_log) == rli->event_relay_log_pos);
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
    if ((ev=Log_event::read_log_event(cur_log,0,
                                      rli->relay_log.description_event_for_exec)))

    {
      DBUG_ASSERT(thd==rli->sql_thd);
      /*
        read it while we have a lock, to avoid a mutex lock in
        inc_event_relay_log_pos()
      */
      rli->future_event_relay_log_pos= my_b_tell(cur_log);
      if (hot_log)
        mysql_mutex_unlock(log_lock);
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->sql_thd);
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
        time_t save_timestamp= rli->last_master_timestamp;
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
          rli->sql_force_rotate_relay= !rli->is_in_group();

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
        rli->relay_log.wait_for_update_relay_log(rli->sql_thd);
        // re-acquire data lock since we released it earlier
        mysql_mutex_lock(&rli->data_lock);
        rli->last_master_timestamp= save_timestamp;
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
             rli->group_relay_log_pos == rli->event_relay_log_pos
             && !strcmp(rli->group_relay_log_name,rli->event_relay_log_name)))
        {
          errmsg = "Error purging processed logs";
          goto err;
        }
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
        rli->event_relay_log_pos = BIN_LOG_HEADER_SIZE;
        strmake(rli->event_relay_log_name,rli->linfo.log_file_name,
                sizeof(rli->event_relay_log_name)-1);
        flush_relay_log_info(rli);
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
        if (global_system_variables.log_warnings)
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
      if (global_system_variables.log_warnings)
        sql_print_information("next log '%s' is not active",
                              rli->linfo.log_file_name);
#endif
      // open_binlog() will check the magic header
      if ((rli->cur_log_fd=open_binlog(cur_log,rli->linfo.log_file_name,
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
      my_b_seek(cur_log,rli->event_relay_log_pos);
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;                                    // To end of function
    }
  }
  if (!errmsg && global_system_variables.log_warnings)
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
  Relay_log_info* rli= &mi->rli;
  int error= 0;

  DBUG_EXECUTE_IF("crash_before_rotate_relaylog", DBUG_SUICIDE(););

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
  if ((error= rli->relay_log.new_file()))
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
    rli->relay_log.description_event_for_exec->server_version_split;

  DBUG_ASSERT(sizeof(rli->relay_log.description_event_for_exec->server_version_split) == 3);

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
                      rli->relay_log.description_event_for_exec->server_version,
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
  if (active_mi && active_mi->rli.sql_thd == thd)
  {
    Relay_log_info *rli= &active_mi->rli;
    DBUG_EXECUTE_IF("simulate_bug33029", return TRUE;);
    return rpl_master_has_bug(rli, 33029, FALSE, NULL, NULL);
  }
  return FALSE;
}

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif

/**
  @} (end of group Replication)
*/

#endif /* HAVE_REPLICATION */
