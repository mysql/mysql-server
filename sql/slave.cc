/* Copyright (C) 2000-2003 MySQL AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"

#ifdef HAVE_REPLICATION

#include <mysql.h>
#include <myisam.h>
#include "slave.h"
#include "sql_repl.h"
#include "repl_failsafe.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <sql_common.h>

bool use_slave_mask = 0;
MY_BITMAP slave_error_mask;

typedef bool (*CHECK_KILLED_FUNC)(THD*,void*);

volatile bool slave_sql_running = 0, slave_io_running = 0;
char* slave_load_tmpdir = 0;
MASTER_INFO *active_mi;
HASH replicate_do_table, replicate_ignore_table;
DYNAMIC_ARRAY replicate_wild_do_table, replicate_wild_ignore_table;
bool do_table_inited = 0, ignore_table_inited = 0;
bool wild_do_table_inited = 0, wild_ignore_table_inited = 0;
bool table_rules_on= 0, replicate_same_server_id;
ulonglong relay_log_space_limit = 0;

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.

  TODO: move the vars below under MASTER_INFO
*/

int disconnect_slave_event_count = 0, abort_slave_event_count = 0;
int events_till_abort = -1;
static int events_till_disconnect = -1;

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL} SLAVE_THD_TYPE;

static int process_io_rotate(MASTER_INFO* mi, Rotate_log_event* rev);
static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev);
static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli);
static inline bool io_slave_killed(THD* thd,MASTER_INFO* mi);
static inline bool sql_slave_killed(THD* thd,RELAY_LOG_INFO* rli);
static int count_relay_log_space(RELAY_LOG_INFO* rli);
static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type);
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings);
static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings);
static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg);
static int request_table_dump(MYSQL* mysql, const char* db, const char* table);
static int create_table_from_dump(THD* thd, MYSQL *mysql, const char* db,
				  const char* table_name, bool overwrite);
static int get_master_version_and_clock(MYSQL* mysql, MASTER_INFO* mi);

/*
  Find out which replications threads are running

  SYNOPSIS
    init_thread_mask()
    mask		Return value here
    mi			master_info for slave
    inverse		If set, returns which threads are not running

  IMPLEMENTATION
    Get a bit mask for which threads are running so that we can later restart
    these threads.

  RETURN
    mask	If inverse == 0, running threads
		If inverse == 1, stopped threads    
*/

void init_thread_mask(int* mask,MASTER_INFO* mi,bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli.slave_running;
  register int tmp_mask=0;
  if (set_io)
    tmp_mask |= SLAVE_IO;
  if (set_sql)
    tmp_mask |= SLAVE_SQL;
  if (inverse)
    tmp_mask^= (SLAVE_IO | SLAVE_SQL);
  *mask = tmp_mask;
}


/*
  lock_slave_threads()
*/

void lock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_lock(&mi->run_lock);
  pthread_mutex_lock(&mi->rli.run_lock);
}


/*
  unlock_slave_threads()
*/

void unlock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_unlock(&mi->rli.run_lock);
  pthread_mutex_unlock(&mi->run_lock);
}


/* Initialize slave structures */

int init_slave()
{
  DBUG_ENTER("init_slave");

  /*
    This is called when mysqld starts. Before client connections are
    accepted. However bootstrap may conflict with us if it does START SLAVE.
    So it's safer to take the lock.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  /*
    TODO: re-write this to interate through the list of files
    for multi-master
  */
  active_mi= new MASTER_INFO;

  /*
    If master_host is not specified, try to read it from the master_info file.
    If master_host is specified, create the master_info file if it doesn't
    exists.
  */
  if (!active_mi)
  {
    sql_print_error("Failed to allocate memory for the master info structure");
    goto err;
  }
    
  if (init_master_info(active_mi,master_info_file,relay_log_info_file,
		       !master_host))
  {
    sql_print_error("Failed to initialize the master info structure");
    goto err;
  }

  if (server_id && !master_host && active_mi->host[0])
    master_host= active_mi->host;

  /* If server id is not set, start_slave_thread() will say it */

  if (master_host && !opt_skip_slave_start)
  {
    if (start_slave_threads(1 /* need mutex */,
			    0 /* no wait for start*/,
			    active_mi,
			    master_info_file,
			    relay_log_info_file,
			    SLAVE_IO | SLAVE_SQL))
    {
      sql_print_error("Failed to create slave threads");
      goto err;
    }
  }
  pthread_mutex_unlock(&LOCK_active_mi);
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&LOCK_active_mi);
  DBUG_RETURN(1);
}


static void free_table_ent(TABLE_RULE_ENT* e)
{
  my_free((gptr) e, MYF(0));
}


static byte* get_table_key(TABLE_RULE_ENT* e, uint* len,
			   my_bool not_used __attribute__((unused)))
{
  *len = e->key_len;
  return (byte*)e->db;
}


/*
  Open the given relay log

  SYNOPSIS
    init_relay_log_pos()
    rli			Relay information (will be initialized)
    log			Name of relay log file to read from. NULL = First log
    pos			Position in relay log file 
    need_data_lock	Set to 1 if this functions should do mutex locks
    errmsg		Store pointer to error message here
    look_for_description_event 
                        1 if we should look for such an event. We only need
                        this when the SQL thread starts and opens an existing
                        relay log and has to execute it (possibly from an offset
                        >4); then we need to read the first event of the relay
                        log to be able to parse the events we have to execute.

  DESCRIPTION
  - Close old open relay log files.
  - If we are using the same relay log as the running IO-thread, then set
    rli->cur_log to point to the same IO_CACHE entry.
  - If not, open the 'log' binary file.

  TODO
    - check proper initialization of group_master_log_name/group_master_log_pos

  RETURN VALUES
    0	ok
    1	error.  errmsg is set to point to the error message
*/

int init_relay_log_pos(RELAY_LOG_INFO* rli,const char* log,
		       ulonglong pos, bool need_data_lock,
		       const char** errmsg,
                       bool look_for_description_event)
{
  DBUG_ENTER("init_relay_log_pos");
  DBUG_PRINT("info", ("pos=%lu", pos));

  *errmsg=0;
  pthread_mutex_t *log_lock=rli->relay_log.get_log_lock();
  
  if (need_data_lock)
    pthread_mutex_lock(&rli->data_lock);

  /*
    Slave threads are not the only users of init_relay_log_pos(). CHANGE MASTER
    is, too, and init_slave() too; these 2 functions allocate a description
    event in init_relay_log_pos, which is not freed by the terminating SQL slave
    thread as that thread is not started by these functions. So we have to free
    the description_event here, in case, so that there is no memory leak in
    running, say, CHANGE MASTER.
  */
  delete rli->relay_log.description_event_for_exec;
  /*
    By default the relay log is in binlog format 3 (4.0).
    Even if format is 4, this will work enough to read the first event
    (Format_desc) (remember that format 4 is just lenghtened compared to format
    3; format 3 is a prefix of format 4). 
  */
  rli->relay_log.description_event_for_exec= new
    Format_description_log_event(3);
  
  pthread_mutex_lock(log_lock);
  
  /* Close log file and free buffers if it's already open */
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    my_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  
  rli->group_relay_log_pos = rli->event_relay_log_pos = pos;

  /*
    Test to see if the previous run was with the skip of purging
    If yes, we do not purge when we restart
  */
  if (rli->relay_log.find_log_pos(&rli->linfo, NullS, 1))
  {
    *errmsg="Could not find first log during relay log initialization";
    goto err;
  }

  if (log && rli->relay_log.find_log_pos(&rli->linfo, log, 1))
  {
    *errmsg="Could not find target log during relay log initialization";
    goto err;
  }
  strmake(rli->group_relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->group_relay_log_name)-1);
  strmake(rli->event_relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->event_relay_log_name)-1);
  if (rli->relay_log.is_active(rli->linfo.log_file_name))
  {
    /*
      The IO thread is using this log file.
      In this case, we will use the same IO_CACHE pointer to
      read data as the IO thread is using to write data.
    */
    my_b_seek((rli->cur_log=rli->relay_log.get_log_file()), (off_t)0);
    if (check_binlog_magic(rli->cur_log,errmsg))
      goto err;
    rli->cur_log_old_open_count=rli->relay_log.get_open_count();
  }
  else
  {
    /*
      Open the relay log and set rli->cur_log to point at this one
    */
    if ((rli->cur_log_fd=open_binlog(&rli->cache_buf,
				     rli->linfo.log_file_name,errmsg)) < 0)
      goto err;
    rli->cur_log = &rli->cache_buf;
  }
  /*
    In all cases, check_binlog_magic() has been called so we're at offset 4 for
    sure.
  */
  if (pos > BIN_LOG_HEADER_SIZE) /* If pos<=4, we stay at 4 */
  {
    Log_event* ev;
    while (look_for_description_event) 
    {
      /*
        Read the possible Format_description_log_event; if position was 4, no need, it will
        be read naturally.
      */
      DBUG_PRINT("info",("looking for a Format_description_log_event"));

      if (my_b_tell(rli->cur_log) >= pos)
        break;

      /*
        Because of we have rli->data_lock and log_lock, we can safely read an
        event
      */
      if (!(ev=Log_event::read_log_event(rli->cur_log,0,
                                         rli->relay_log.description_event_for_exec)))
      {
        DBUG_PRINT("info",("could not read event, rli->cur_log->error=%d",
                           rli->cur_log->error));
        if (rli->cur_log->error) /* not EOF */
        {
          *errmsg= "I/O error reading event at position 4";
          goto err;
        }
        break;
      }
      else if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
      {
        DBUG_PRINT("info",("found Format_description_log_event"));
        delete rli->relay_log.description_event_for_exec;
        rli->relay_log.description_event_for_exec= (Format_description_log_event*) ev;
        /*
          As ev was returned by read_log_event, it has passed is_valid(), so
          my_malloc() in ctor worked, no need to check again.
        */
        /*
          Ok, we found a Format_description event. But it is not sure that this
          describes the whole relay log; indeed, one can have this sequence
          (starting from position 4):
          Format_desc (of slave)
          Rotate (of master)
          Format_desc (of master)
          So the Format_desc which really describes the rest of the relay log is
          the 3rd event (it can't be further than that, because we rotate the
          relay log when we queue a Rotate event from the master).
          But what describes the Rotate is the first Format_desc.
          So what we do is:
          go on searching for Format_description events, until you exceed the
          position (argument 'pos') or until you find another event than Rotate
          or Format_desc.
        */
      }
      else 
      {
        DBUG_PRINT("info",("found event of another type=%d",
                           ev->get_type_code()));
        look_for_description_event= (ev->get_type_code() == ROTATE_EVENT);
        delete ev;
      }
    }
    my_b_seek(rli->cur_log,(off_t)pos);
#ifndef DBUG_OFF
  {
    char llbuf1[22], llbuf2[22];
    DBUG_PRINT("info", ("my_b_tell(rli->cur_log)=%s rli->event_relay_log_pos=%s",
                        llstr(my_b_tell(rli->cur_log),llbuf1), 
                        llstr(rli->event_relay_log_pos,llbuf2)));
  }
#endif

  }

err:
  /*
    If we don't purge, we can't honour relay_log_space_limit ;
    silently discard it
  */
  if (!relay_log_purge)
    rli->log_space_limit= 0;
  pthread_cond_broadcast(&rli->data_cond);
  
  pthread_mutex_unlock(log_lock);

  if (need_data_lock)
    pthread_mutex_unlock(&rli->data_lock);
  if (!rli->relay_log.description_event_for_exec->is_valid() && !*errmsg)
    *errmsg= "Invalid Format_description log event; could be out of memory";

  DBUG_RETURN ((*errmsg) ? 1 : 0);
}


/*
  Init functio to set up array for errors that should be skipped for slave

  SYNOPSIS
    init_slave_skip_errors()
    arg		List of errors numbers to skip, separated with ','

  NOTES
    Called from get_options() in mysqld.cc on start-up
*/

void init_slave_skip_errors(const char* arg)
{
  const char *p;
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
    return;
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
}


void st_relay_log_info::inc_group_relay_log_pos(ulonglong log_pos,
						bool skip_lock)  
{
  if (!skip_lock)
    pthread_mutex_lock(&data_lock);
  inc_event_relay_log_pos();
  group_relay_log_pos= event_relay_log_pos;
  strmake(group_relay_log_name,event_relay_log_name,
	  sizeof(group_relay_log_name)-1);

  notify_group_relay_log_name_update();
        
  /*
    If the slave does not support transactions and replicates a transaction,
    users should not trust group_master_log_pos (which they can display with
    SHOW SLAVE STATUS or read from relay-log.info), because to compute
    group_master_log_pos the slave relies on log_pos stored in the master's
    binlog, but if we are in a master's transaction these positions are always
    the BEGIN's one (excepted for the COMMIT), so group_master_log_pos does
    not advance as it should on the non-transactional slave (it advances by
    big leaps, whereas it should advance by small leaps).
  */
  /*
    In 4.x we used the event's len to compute the positions here. This is
    wrong if the event was 3.23/4.0 and has been converted to 5.0, because
    then the event's len is not what is was in the master's binlog, so this
    will make a wrong group_master_log_pos (yes it's a bug in 3.23->4.0
    replication: Exec_master_log_pos is wrong). Only way to solve this is to
    have the original offset of the end of the event the relay log. This is
    what we do in 5.0: log_pos has become "end_log_pos" (because the real use
    of log_pos in 4.0 was to compute the end_log_pos; so better to store
    end_log_pos instead of begin_log_pos.
    If we had not done this fix here, the problem would also have appeared
    when the slave and master are 5.0 but with different event length (for
    example the slave is more recent than the master and features the event
    UID). It would give false MASTER_POS_WAIT, false Exec_master_log_pos in
    SHOW SLAVE STATUS, and so the user would do some CHANGE MASTER using this
    value which would lead to badly broken replication.
    Even the relay_log_pos will be corrupted in this case, because the len is
    the relay log is not "val".
    With the end_log_pos solution, we avoid computations involving lengthes.
  */
  DBUG_PRINT("info", ("log_pos=%lld group_master_log_pos=%lld",
		      log_pos,group_master_log_pos));
  if (log_pos) // 3.23 binlogs don't have log_posx
  {
#if MYSQL_VERSION_ID < 50000
    /*
      If the event was converted from a 3.23 format, get_event_len() has
      grown by 6 bytes (at least for most events, except LOAD DATA INFILE
      which is already a big problem for 3.23->4.0 replication); 6 bytes is
      the difference between the header's size in 4.0 (LOG_EVENT_HEADER_LEN)
      and the header's size in 3.23 (OLD_HEADER_LEN). Note that using
      mi->old_format will not help if the I/O thread has not started yet.
      Yes this is a hack but it's just to make 3.23->4.x replication work;
      3.23->5.0 replication is working much better.
    */
    group_master_log_pos= log_pos -
      (mi->old_format ? (LOG_EVENT_HEADER_LEN - OLD_HEADER_LEN) : 0);
#else
    group_master_log_pos= log_pos;
#endif /* MYSQL_VERSION_ID < 5000 */
  }
  pthread_cond_broadcast(&data_cond);
  if (!skip_lock)
    pthread_mutex_unlock(&data_lock);
}


void st_relay_log_info::close_temporary_tables()
{
  TABLE *table,*next;

  for (table=save_temporary_tables ; table ; table=next)
  {
    next=table->next;
    /*
      Don't ask for disk deletion. For now, anyway they will be deleted when
      slave restarts, but it is a better intention to not delete them.
    */
    close_temporary(table, 0);
  }
  save_temporary_tables= 0;
  slave_open_temp_tables= 0;
}

/*
  purge_relay_logs()

  NOTES
    Assumes to have a run lock on rli and that no slave thread are running.
*/

int purge_relay_logs(RELAY_LOG_INFO* rli, THD *thd, bool just_reset,
		     const char** errmsg)
{
  int error=0;
  DBUG_ENTER("purge_relay_logs");

  /*
    Even if rli->inited==0, we still try to empty rli->master_log_* variables.
    Indeed, rli->inited==0 does not imply that they already are empty.
    It could be that slave's info initialization partly succeeded : 
    for example if relay-log.info existed but *relay-bin*.*
    have been manually removed, init_relay_log_info reads the old 
    relay-log.info and fills rli->master_log_*, then init_relay_log_info
    checks for the existence of the relay log, this fails and
    init_relay_log_info leaves rli->inited to 0.
    In that pathological case, rli->master_log_pos* will be properly reinited
    at the next START SLAVE (as RESET SLAVE or CHANGE
    MASTER, the callers of purge_relay_logs, will delete bogus *.info files
    or replace them with correct files), however if the user does SHOW SLAVE
    STATUS before START SLAVE, he will see old, confusing rli->master_log_*.
    In other words, we reinit rli->master_log_* for SHOW SLAVE STATUS 
    to display fine in any case.
  */

  rli->group_master_log_name[0]= 0;
  rli->group_master_log_pos= 0;

  if (!rli->inited)
  {
    DBUG_PRINT("info", ("rli->inited == 0"));
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(rli->slave_running == 0);
  DBUG_ASSERT(rli->mi->slave_running == 0);

  rli->slave_skip_counter=0;
  pthread_mutex_lock(&rli->data_lock);
  if (rli->relay_log.reset_logs(thd))
  {
    *errmsg = "Failed during log reset";
    error=1;
    goto err;
  }
  /* Save name of used relay log file */
  strmake(rli->group_relay_log_name, rli->relay_log.get_log_fname(),
	  sizeof(rli->group_relay_log_name)-1);
  strmake(rli->event_relay_log_name, rli->relay_log.get_log_fname(),
 	  sizeof(rli->event_relay_log_name)-1);
  rli->group_relay_log_pos= rli->event_relay_log_pos= BIN_LOG_HEADER_SIZE;
  if (count_relay_log_space(rli))
  {
    *errmsg= "Error counting relay log space";
    goto err;
  }
  if (!just_reset)
    error= init_relay_log_pos(rli, rli->group_relay_log_name, rli->group_relay_log_pos,
  			      0 /* do not need data lock */, errmsg, 0);
  
err:
#ifndef DBUG_OFF
  char buf[22];
#endif  
  DBUG_PRINT("info",("log_space_total: %s",llstr(rli->log_space_total,buf)));
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);
}


int terminate_slave_threads(MASTER_INFO* mi,int thread_mask,bool skip_lock)
{
  if (!mi->inited)
    return 0; /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  pthread_mutex_t *sql_lock = &mi->rli.run_lock, *io_lock = &mi->run_lock;
  pthread_mutex_t *sql_cond_lock,*io_cond_lock;
  DBUG_ENTER("terminate_slave_threads");

  sql_cond_lock=sql_lock;
  io_cond_lock=io_lock;
  
  if (skip_lock)
  {
    sql_lock = io_lock = 0;
  }
  if ((thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL)) && mi->slave_running)
  {
    DBUG_PRINT("info",("Terminating IO thread"));
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->io_thd,io_lock,
				      io_cond_lock,
				      &mi->stop_cond,
				      &mi->slave_running)) &&
	!force_all)
      DBUG_RETURN(error);
  }
  if ((thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL)) && mi->rli.slave_running)
  {
    DBUG_PRINT("info",("Terminating SQL thread"));
    DBUG_ASSERT(mi->rli.sql_thd != 0) ;
    mi->rli.abort_slave=1;
    if ((error=terminate_slave_thread(mi->rli.sql_thd,sql_lock,
				      sql_cond_lock,
				      &mi->rli.stop_cond,
				      &mi->rli.slave_running)) &&
	!force_all)
      DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}


int terminate_slave_thread(THD* thd, pthread_mutex_t* term_lock,
			   pthread_mutex_t *cond_lock,
			   pthread_cond_t* term_cond,
			   volatile bool* slave_running)
{
  if (term_lock)
  {
    pthread_mutex_lock(term_lock);
    if (!*slave_running)
    {
      pthread_mutex_unlock(term_lock);
      return ER_SLAVE_NOT_RUNNING;
    }
  }
  DBUG_ASSERT(thd != 0);
  /*
    Is is criticate to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */
  THD_CHECK_SENTRY(thd);

  while (*slave_running)			// Should always be true
  {
    KICK_SLAVE(thd);
    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect againts it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
    pthread_cond_timedwait(term_cond, cond_lock, &abstime);
  }
  if (term_lock)
    pthread_mutex_unlock(term_lock);
  return 0;
}


int start_slave_thread(pthread_handler h_func, pthread_mutex_t *start_lock,
		       pthread_mutex_t *cond_lock,
		       pthread_cond_t *start_cond,
		       volatile bool *slave_running,
		       volatile ulong *slave_run_id,
		       MASTER_INFO* mi,
                       bool high_priority)
{
  pthread_t th;
  ulong start_id;
  DBUG_ASSERT(mi->inited);
  DBUG_ENTER("start_slave_thread");

  if (start_lock)
    pthread_mutex_lock(start_lock);
  if (!server_id)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    sql_print_error("Server id not set, will not start slave");
    DBUG_RETURN(ER_BAD_SLAVE);
  }
  
  if (*slave_running)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_MUST_STOP);
  }
  start_id= *slave_run_id;
  DBUG_PRINT("info",("Creating new slave thread"));
  if (high_priority)
    my_pthread_attr_setprio(&connection_attrib,CONNECT_PRIOR);
  if (pthread_create(&th, &connection_attrib, h_func, (void*)mi))
  {
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_THREAD);
  }
  if (start_cond && cond_lock) // caller has cond_lock
  {
    THD* thd = current_thd;
    while (start_id == *slave_run_id)
    {
      DBUG_PRINT("sleep",("Waiting for slave thread to start"));
      const char* old_msg = thd->enter_cond(start_cond,cond_lock,
					    "Waiting for slave thread to start");
      pthread_cond_wait(start_cond,cond_lock);
      thd->exit_cond(old_msg);
      pthread_mutex_lock(cond_lock); // re-acquire it as exit_cond() released
      if (thd->killed)
	DBUG_RETURN(thd->killed_errno());
    }
  }
  if (start_lock)
    pthread_mutex_unlock(start_lock);
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
			MASTER_INFO* mi, const char* master_info_fname,
			const char* slave_info_fname, int thread_mask)
{
  pthread_mutex_t *lock_io=0,*lock_sql=0,*lock_cond_io=0,*lock_cond_sql=0;
  pthread_cond_t* cond_io=0,*cond_sql=0;
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
    error=start_slave_thread(handle_slave_io,lock_io,lock_cond_io,
			     cond_io,
			     &mi->slave_running, &mi->slave_run_id,
			     mi, 1); //high priority, to read the most possible
  if (!error && (thread_mask & SLAVE_SQL))
  {
    error=start_slave_thread(handle_slave_sql,lock_sql,lock_cond_sql,
			     cond_sql,
			     &mi->rli.slave_running, &mi->rli.slave_run_id,
			     mi, 0);
    if (error)
      terminate_slave_threads(mi, thread_mask & SLAVE_IO, 0);
  }
  DBUG_RETURN(error);
}


void init_table_rule_hash(HASH* h, bool* h_inited)
{
  hash_init(h, system_charset_info,TABLE_RULE_HASH_SIZE,0,0,
	    (hash_get_key) get_table_key,
	    (hash_free_key) free_table_ent, 0);
  *h_inited = 1;
}


void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited)
{
  my_init_dynamic_array(a, sizeof(TABLE_RULE_ENT*), TABLE_RULE_ARR_SIZE,
		     TABLE_RULE_ARR_SIZE);
  *a_inited = 1;
}


static TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len)
{
  uint i;
  const char* key_end = key + len;
  
  for (i = 0; i < a->elements; i++)
    {
      TABLE_RULE_ENT* e ;
      get_dynamic(a, (gptr)&e, i);
      if (!my_wildcmp(system_charset_info, key, key_end, 
                            (const char*)e->db,
			    (const char*)(e->db + e->key_len),
			    '\\',wild_one,wild_many))
	return e;
    }
  
  return 0;
}


/*
  Checks whether tables match some (wild_)do_table and (wild_)ignore_table
  rules (for replication)

  SYNOPSIS
    tables_ok()
    thd             thread (SQL slave thread normally)
    tables          list of tables to check

  NOTES
    Note that changing the order of the tables in the list can lead to
    different results. Note also the order of precedence of the do/ignore 
    rules (see code below). For that reason, users should not set conflicting 
    rules because they may get unpredicted results (precedence order is
    explained in the manual).
    If no table of the list is marked "updating" (so far this can only happen
    if the statement is a multi-delete (SQLCOM_DELETE_MULTI) and the "tables"
    is the tables in the FROM): then we always return 0, because there is no
    reason we play this statement on this slave if it updates nothing. In the
    case of SQLCOM_DELETE_MULTI, there will be a second call to tables_ok(),
    with tables having "updating==TRUE" (those after the DELETE), so this
    second call will make the decision (because
    all_tables_not_ok() = !tables_ok(1st_list) && !tables_ok(2nd_list)).

    Thought which arose from a question of a big customer "I want to include all
    tables like "abc.%" except the "%.EFG"". This can't be done now. If we
    supported Perl regexps we could do it with this pattern: /^abc\.(?!EFG)/
    (I could not find an equivalent in the regex library MySQL uses).

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

int tables_ok(THD* thd, TABLE_LIST* tables)
{
  bool some_tables_updating= 0;
  DBUG_ENTER("tables_ok");

  for (; tables; tables= tables->next_global)
  {
    char hash_key[2*NAME_LEN+2];
    char *end;
    uint len;

    if (!tables->updating) 
      continue;
    some_tables_updating= 1;
    end= strmov(hash_key, tables->db ? tables->db : thd->db);
    *end++= '.';
    len= (uint) (strmov(end, tables->real_name) - hash_key);
    if (do_table_inited) // if there are any do's
    {
      if (hash_search(&replicate_do_table, (byte*) hash_key, len))
	DBUG_RETURN(1);
    }
    if (ignore_table_inited) // if there are any ignores
    {
      if (hash_search(&replicate_ignore_table, (byte*) hash_key, len))
	DBUG_RETURN(0); 
    }
    if (wild_do_table_inited && find_wild(&replicate_wild_do_table,
					  hash_key, len))
      DBUG_RETURN(1);
    if (wild_ignore_table_inited && find_wild(&replicate_wild_ignore_table,
					      hash_key, len))
      DBUG_RETURN(0);
  }

  /*
    If no table was to be updated, ignore statement (no reason we play it on
    slave, slave is supposed to replicate _changes_ only).
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  DBUG_RETURN(some_tables_updating &&
              !do_table_inited && !wild_do_table_inited);
}


/*
  Checks whether a db matches wild_do_table and wild_ignore_table
  rules (for replication)

  SYNOPSIS
    db_ok_with_wild_table()
    db		name of the db to check.
		Is tested with check_db_name() before calling this function.

  NOTES
    Here is the reason for this function.
    We advise users who want to exclude a database 'db1' safely to do it
    with replicate_wild_ignore_table='db1.%' instead of binlog_ignore_db or
    replicate_ignore_db because the two lasts only check for the selected db,
    which won't work in that case:
    USE db2;
    UPDATE db1.t SET ... #this will be replicated and should not
    whereas replicate_wild_ignore_table will work in all cases.
    With replicate_wild_ignore_table, we only check tables. When
    one does 'DROP DATABASE db1', tables are not involved and the
    statement will be replicated, while users could expect it would not (as it
    rougly means 'DROP db1.first_table, DROP db1.second_table...').
    In other words, we want to interpret 'db1.%' as "everything touching db1".
    That is why we want to match 'db1' against 'db1.%' wild table rules.

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated
 */

int db_ok_with_wild_table(const char *db)
{
  char hash_key[NAME_LEN+2];
  char *end;
  int len;
  end= strmov(hash_key, db);
  *end++= '.';
  len= end - hash_key ;
  if (wild_do_table_inited && find_wild(&replicate_wild_do_table,
                                        hash_key, len))
    return 1;
  if (wild_ignore_table_inited && find_wild(&replicate_wild_ignore_table,
                                            hash_key, len))
    return 0;
  
  /*
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  return !wild_do_table_inited;
}


int add_table_rule(HASH* h, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  (void)my_hash_insert(h, (byte*)e);
  return 0;
}


/*
  Add table expression with wildcards to dynamic array
*/

int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  insert_dynamic(a, (gptr)&e);
  return 0;
}


static void free_string_array(DYNAMIC_ARRAY *a)
{
  uint i;
  for (i = 0; i < a->elements; i++)
    {
      char* p;
      get_dynamic(a, (gptr) &p, i);
      my_free(p, MYF(MY_WME));
    }
  delete_dynamic(a);
}


#ifdef NOT_USED_YET
static int end_slave_on_walk(MASTER_INFO* mi, gptr /*unused*/)
{
  end_master_info(mi);
  return 0;
}
#endif


/*
  Free all resources used by slave

  SYNOPSIS
    end_slave()
*/

void end_slave()
{
  /*
    This is called when the server terminates, in close_connections().
    It terminates slave threads. However, some CHANGE MASTER etc may still be
    running presently. If a START SLAVE was in progress, the mutex lock below
    will make us wait until slave threads have started, and START SLAVE
    returns, then we terminate them here.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    /*
      TODO: replace the line below with
      list_walk(&master_list, (list_walk_action)end_slave_on_walk,0);
      once multi-master code is ready.
    */
    terminate_slave_threads(active_mi,SLAVE_FORCE_ALL);
    end_master_info(active_mi);
    if (do_table_inited)
      hash_free(&replicate_do_table);
    if (ignore_table_inited)
      hash_free(&replicate_ignore_table);
    if (wild_do_table_inited)
      free_string_array(&replicate_wild_do_table);
    if (wild_ignore_table_inited)
      free_string_array(&replicate_wild_ignore_table);
    delete active_mi;
    active_mi= 0;
  }
  pthread_mutex_unlock(&LOCK_active_mi);
}


static bool io_slave_killed(THD* thd, MASTER_INFO* mi)
{
  DBUG_ASSERT(mi->io_thd == thd);
  DBUG_ASSERT(mi->slave_running == 1); // tracking buffer overrun
  return mi->abort_slave || abort_loop || thd->killed;
}


static bool sql_slave_killed(THD* thd, RELAY_LOG_INFO* rli)
{
  DBUG_ASSERT(rli->sql_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);// tracking buffer overrun
  return rli->abort_slave || abort_loop || thd->killed;
}


/*
  Writes an error message to rli->last_slave_error and rli->last_slave_errno
  (which will be displayed by SHOW SLAVE STATUS), and prints it to stderr.

  SYNOPSIS
    slave_print_error()
    rli		
    err_code    The error code
    msg         The error message (usually related to the error code, but can
                contain more information).
    ...         (this is printf-like format, with % symbols in msg)

  RETURN VALUES
    void
*/

void slave_print_error(RELAY_LOG_INFO* rli, int err_code, const char* msg, ...)
{
  va_list args;
  va_start(args,msg);
  my_vsnprintf(rli->last_slave_error,
	       sizeof(rli->last_slave_error), msg, args);
  rli->last_slave_errno = err_code;
  /* If the error string ends with '.', do not add a ',' it would be ugly */
  if (rli->last_slave_error[0] && 
      (*(strend(rli->last_slave_error)-1) == '.'))
    sql_print_error("Slave: %s Error_code: %d", rli->last_slave_error,
                    err_code);
  else
    sql_print_error("Slave: %s, Error_code: %d", rli->last_slave_error,
                    err_code);

}

/*
  skip_load_data_infile()

  NOTES
    This is used to tell a 3.23 master to break send_file()
*/

void skip_load_data_infile(NET *net)
{
  (void)net_request_file(net, "/dev/null");
  (void)my_net_read(net);				// discard response
  (void)net_write_command(net, 0, "", 0, "", 0);	// Send ok
}


bool net_request_file(NET* net, const char* fname)
{
  DBUG_ENTER("net_request_file");
  DBUG_RETURN(net_write_command(net, 251, fname, strlen(fname), "", 0));
}


const char *rewrite_db(const char* db)
{
  if (replicate_rewrite_db.is_empty() || !db)
    return db;
  I_List_iterator<i_string_pair> it(replicate_rewrite_db);
  i_string_pair* tmp;

  while ((tmp=it++))
  {
    if (!strcmp(tmp->key, db))
      return tmp->val;
  }
  return db;
}

/*
  From other comments and tests in code, it looks like
  sometimes Query_log_event and Load_log_event can have db == 0
  (see rewrite_db() above for example)
  (cases where this happens are unclear; it may be when the master is 3.23).
*/

const char *print_slave_db_safe(const char* db)
{
  return (db ? rewrite_db(db) : "");
}

/*
  Checks whether a db matches some do_db and ignore_db rules
  (for logging or replication)

  SYNOPSIS
    db_ok()
    db              name of the db to check
    do_list         either binlog_do_db or replicate_do_db
    ignore_list     either binlog_ignore_db or replicate_ignore_db

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list )
{
  if (do_list.is_empty() && ignore_list.is_empty())
    return 1; // ok to replicate if the user puts no constraints

  /*
    If the user has specified restrictions on which databases to replicate
    and db was not selected, do not replicate.
  */
  if (!db)
    return 0;

  if (!do_list.is_empty()) // if the do's are not empty
  {
    I_List_iterator<i_string> it(do_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 1; // match
    }
    return 0;
  }
  else // there are some elements in the don't, otherwise we cannot get here
  {
    I_List_iterator<i_string> it(ignore_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 0; // match
    }
    return 1;
  }
}


static int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
				 const char *default_val)
{
  uint length;
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
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF));
    }
    return 0;
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    return 0;
  }
  return 1;
}


static int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  char buf[32];
  
  if (my_b_gets(f, buf, sizeof(buf))) 
  {
    *var = atoi(buf);
    return 0;
  }
  else if (default_val)
  {
    *var = default_val;
    return 0;
  }
  return 1;
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
*/

static int get_master_version_and_clock(MYSQL* mysql, MASTER_INFO* mi)
{
  const char* errmsg= 0;

  /*
    Free old description_event_for_queue (that is needed if we are in
    a reconnection).
  */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;
  
  if (!my_isdigit(&my_charset_bin,*mysql->server_version))
    errmsg = "Master reported unrecognized MySQL version";
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
  {
    sql_print_error(errmsg);
    return 1;
  }

  /* as we are here, we tried to allocate the event */
  if (!mi->rli.relay_log.description_event_for_queue)
  {
    sql_print_error("Slave I/O thread failed to create a default Format_description_log_event");
    return 1;
  }

  /*
    Compare the master and slave's clock. Do not die if master's clock is
    unavailable (very old master not supporting UNIX_TIMESTAMP()?).
  */
  MYSQL_RES *master_res= 0;
  MYSQL_ROW master_row;
  
  if (!mysql_real_query(mysql, "SELECT UNIX_TIMESTAMP()", 23) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    mi->clock_diff_with_master= 
      (long) (time((time_t*) 0) - strtoul(master_row[0], 0, 10));
  }
  else
  {
    mi->clock_diff_with_master= 0; /* The "most sensible" value */
    sql_print_error("Warning: \"SELECT UNIX_TIMESTAMP()\" failed on master, \
do not trust column Seconds_Behind_Master of SHOW SLAVE STATUS");
  }
  if (master_res)
    mysql_free_result(master_res);      
 
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
  if (!mysql_real_query(mysql, "SHOW VARIABLES LIKE 'SERVER_ID'", 31) &&
      (master_res= mysql_store_result(mysql)))
  {
    if ((master_row= mysql_fetch_row(master_res)) &&
        (::server_id == strtoul(master_row[1], 0, 10)) &&
        !replicate_same_server_id)
      errmsg= "The slave I/O thread stops because master and slave have equal \
MySQL server ids; these ids must be different for replication to work (or \
the --replicate-same-server-id option must be used on slave but this does \
not always make sense; please check the manual before using it).";
    mysql_free_result(master_res);
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
  */
  if (!mysql_real_query(mysql, "SELECT @@GLOBAL.COLLATION_SERVER", 32) &&
      (master_res= mysql_store_result(mysql)))
  {
    if ((master_row= mysql_fetch_row(master_res)) &&
        strcmp(master_row[0], global_system_variables.collation_server->name))
      errmsg= "The slave I/O thread stops because master and slave have \
different values for the COLLATION_SERVER global variable. The values must \
be equal for replication to work";
    mysql_free_result(master_res);
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
  */
  if (!mysql_real_query(mysql, "SELECT @@GLOBAL.TIME_ZONE", 25) &&
      (master_res= mysql_store_result(mysql)))
  {
    if ((master_row= mysql_fetch_row(master_res)) &&
        strcmp(master_row[0], 
               global_system_variables.time_zone->get_name()->ptr()))
      errmsg= "The slave I/O thread stops because master and slave have \
different values for the TIME_ZONE global variable. The values must \
be equal for replication to work";
    mysql_free_result(master_res);
  }

  if (errmsg)
  {
    sql_print_error(errmsg);
    return 1;
  }

  return 0;
}

/*
  Used by fetch_master_table (used by LOAD TABLE tblname FROM MASTER and LOAD
  DATA FROM MASTER). Drops the table (if 'overwrite' is true) and recreates it
  from the dump. Honours replication inclusion/exclusion rules.

  RETURN VALUES
    0           success
    1           error
*/

static int create_table_from_dump(THD* thd, MYSQL *mysql, const char* db,
				  const char* table_name, bool overwrite)
{
  ulong packet_len;
  char *query;
  char* save_db;
  Vio* save_vio;
  HA_CHECK_OPT check_opt;
  TABLE_LIST tables;
  int error= 1;
  handler *file;
  ulong save_options;
  NET *net= &mysql->net;
  DBUG_ENTER("create_table_from_dump");  

  packet_len= my_net_read(net); // read create table statement
  if (packet_len == packet_error)
  {
    send_error(thd, ER_MASTER_NET_READ);
    DBUG_RETURN(1);
  }
  if (net->read_pos[0] == 255) // error from master
  {
    char *err_msg; 
    err_msg= (char*) net->read_pos + ((mysql->server_capabilities &
				       CLIENT_PROTOCOL_41) ?
				      3+SQLSTATE_LENGTH+1 : 3);
    net_printf(thd, ER_MASTER, err_msg);
    DBUG_RETURN(1);
  }
  thd->command = COM_TABLE_DUMP;
  thd->query_length= packet_len;
  /* Note that we should not set thd->query until the area is initalized */
  if (!(query = thd->strmake((char*) net->read_pos, packet_len)))
  {
    sql_print_error("create_table_from_dump: out of memory");
    net_printf(thd, ER_GET_ERRNO, "Out of memory");
    DBUG_RETURN(1);
  }
  thd->query= query;
  thd->query_error = 0;
  thd->net.no_send_ok = 1;

  bzero((char*) &tables,sizeof(tables));
  tables.db = (char*)db;
  tables.alias= tables.real_name= (char*)table_name;

  /* Drop the table if 'overwrite' is true */
  if (overwrite && mysql_rm_table(thd,&tables,1,0)) /* drop if exists */
  {
    send_error(thd);
    sql_print_error("create_table_from_dump: failed to drop the table");
    goto err;
  }

  /* Create the table. We do not want to log the "create table" statement */
  save_options = thd->options;
  thd->options &= ~(ulong) (OPTION_BIN_LOG);
  thd->proc_info = "Creating table from master dump";
  // save old db in case we are creating in a different database
  save_db = thd->db;
  thd->db = (char*)db;
  mysql_parse(thd, thd->query, packet_len); // run create table
  thd->db = save_db;		// leave things the way the were before
  thd->options = save_options;
  
  if (thd->query_error)
    goto err;			// mysql_parse took care of the error send

  thd->proc_info = "Opening master dump table";
  tables.lock_type = TL_WRITE;
  if (!open_ltable(thd, &tables, TL_WRITE))
  {
    send_error(thd,0,0);			// Send error from open_ltable
    sql_print_error("create_table_from_dump: could not open created table");
    goto err;
  }
  
  file = tables.table->file;
  thd->proc_info = "Reading master dump table data";
  /* Copy the data file */
  if (file->net_read_dump(net))
  {
    net_printf(thd, ER_MASTER_NET_READ);
    sql_print_error("create_table_from_dump: failed in\
 handler::net_read_dump()");
    goto err;
  }

  check_opt.init();
  check_opt.flags|= T_VERY_SILENT | T_CALC_CHECKSUM | T_QUICK;
  thd->proc_info = "Rebuilding the index on master dump table";
  /*
    We do not want repair() to spam us with messages
    just send them to the error log, and report the failure in case of
    problems.
  */
  save_vio = thd->net.vio;
  thd->net.vio = 0;
  /* Rebuild the index file from the copied data file (with REPAIR) */
  error=file->repair(thd,&check_opt) != 0;
  thd->net.vio = save_vio;
  if (error)
    net_printf(thd, ER_INDEX_REBUILD,tables.table->real_name);

err:
  close_thread_tables(thd);
  thd->net.no_send_ok = 0;
  DBUG_RETURN(error); 
}


int fetch_master_table(THD *thd, const char *db_name, const char *table_name,
		       MASTER_INFO *mi, MYSQL *mysql, bool overwrite)
{
  int error= 1;
  const char *errmsg=0;
  bool called_connected= (mysql != NULL);
  DBUG_ENTER("fetch_master_table");
  DBUG_PRINT("enter", ("db_name: '%s'  table_name: '%s'",
		       db_name,table_name));

  if (!called_connected)
  { 
    if (!(mysql = mysql_init(NULL)))
    {
      send_error(thd);			// EOM
      DBUG_RETURN(1);
    }
    if (connect_to_master(thd, mysql, mi))
    {
      net_printf(thd, ER_CONNECT_TO_MASTER, mysql_error(mysql));
      mysql_close(mysql);
      DBUG_RETURN(1);
    }
    if (thd->killed)
      goto err;
  }

  if (request_table_dump(mysql, db_name, table_name))
  {
    error= ER_UNKNOWN_ERROR;
    errmsg= "Failed on table dump request";
    goto err;
  }
  if (create_table_from_dump(thd, mysql, db_name,
			     table_name, overwrite))
    goto err;    // create_table_from_dump have sent the error already
  error = 0;

 err:
  thd->net.no_send_ok = 0; // Clear up garbage after create_table_from_dump
  if (!called_connected)
    mysql_close(mysql);
  if (errmsg && thd->vio_ok())
    send_error(thd, error, errmsg);
  DBUG_RETURN(test(error));			// Return 1 on error
}


void end_master_info(MASTER_INFO* mi)
{
  DBUG_ENTER("end_master_info");

  if (!mi->inited)
    DBUG_VOID_RETURN;
  end_relay_log_info(&mi->rli);
  if (mi->fd >= 0)
  {
    end_io_cache(&mi->file);
    (void)my_close(mi->fd, MYF(MY_WME));
    mi->fd = -1;
  }
  mi->inited = 0;

  DBUG_VOID_RETURN;
}


int init_relay_log_info(RELAY_LOG_INFO* rli, const char* info_fname)
{
  char fname[FN_REFLEN+128];
  int info_fd;
  const char* msg = 0;
  int error = 0;
  DBUG_ENTER("init_relay_log_info");

  if (rli->inited)				// Set if this function called
    DBUG_RETURN(0);
  fn_format(fname, info_fname, mysql_data_home, "", 4+32);
  pthread_mutex_lock(&rli->data_lock);
  info_fd = rli->info_fd;
  rli->cur_log_fd = -1;
  rli->slave_skip_counter=0;
  rli->abort_pos_wait=0;
  rli->log_space_limit= relay_log_space_limit;
  rli->log_space_total= 0;

  // TODO: make this work with multi-master
  if (!opt_relay_logname)
  {
    char tmp[FN_REFLEN];
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(tmp,glob_hostname,FN_REFLEN-5);
    strmov(strcend(tmp,'.'),"-relay-bin");
    opt_relay_logname=my_strdup(tmp,MYF(MY_WME));
  }

  /*
    The relay log will now be opened, as a SEQ_READ_APPEND IO_CACHE.
    Note that the I/O thread flushes it to disk after writing every event, in
    flush_master_info(mi, 1).
  */

  /*
    For the maximum log size, we choose max_relay_log_size if it is
    non-zero, max_binlog_size otherwise. If later the user does SET
    GLOBAL on one of these variables, fix_max_binlog_size and
    fix_max_relay_log_size will reconsider the choice (for example
    if the user changes max_relay_log_size to zero, we have to
    switch to using max_binlog_size for the relay log) and update
    rli->relay_log.max_size (and mysql_bin_log.max_size).
  */

  if (open_log(&rli->relay_log, glob_hostname, opt_relay_logname,
	       "-relay-bin", opt_relaylog_index_name,
	       LOG_BIN, 1 /* read_append cache */,
	       0 /* starting from 5.0 we want relay logs to have auto events */,
               max_relay_log_size ? max_relay_log_size : max_binlog_size))
  {
    pthread_mutex_unlock(&rli->data_lock);
    sql_print_error("Failed in open_log() called from init_relay_log_info()");
    DBUG_RETURN(1);
  }

  /* if file does not exist */
  if (access(fname,F_OK))
  {
    /*
      If someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (info_fd >= 0)
      my_close(info_fd, MYF(MY_WME));
    if ((info_fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0)
    {
      sql_print_error("Failed to create a new relay log info file (\
file '%s', errno %d)", fname, my_errno);
      msg= current_thd->net.last_error;
      goto err;
    }
    if (init_io_cache(&rli->info_file, info_fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME))) 
    {
      sql_print_error("Failed to create a cache on relay log info file '%s'",
		      fname);
      msg= current_thd->net.last_error;
      goto err;
    }

    /* Init relay log with first entry in the relay index file */
    if (init_relay_log_pos(rli,NullS,BIN_LOG_HEADER_SIZE,0 /* no data lock */,
			   &msg, 0))
    {
      sql_print_error("Failed to open the relay log 'FIRST' (relay_log_pos 4)");
      goto err;
    }
    rli->group_master_log_name[0]= 0;
    rli->group_master_log_pos= 0;		
    rli->info_fd= info_fd;
  }
  else // file exists
  {
    if (info_fd >= 0)
      reinit_io_cache(&rli->info_file, READ_CACHE, 0L,0,0);
    else 
    {
      int error=0;
      if ((info_fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0)
      {
        sql_print_error("\
Failed to open the existing relay log info file '%s' (errno %d)",
			fname, my_errno);
        error= 1;
      }
      else if (init_io_cache(&rli->info_file, info_fd,
                             IO_SIZE*2, READ_CACHE, 0L, 0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on relay log info file '%s'",
			fname);
        error= 1;
      }
      if (error)
      {
        if (info_fd >= 0)
          my_close(info_fd, MYF(0));
        rli->info_fd= -1;
        rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
        pthread_mutex_unlock(&rli->data_lock);
        DBUG_RETURN(1);
      }
    }
         
    rli->info_fd = info_fd;
    int relay_log_pos, master_log_pos;
    if (init_strvar_from_file(rli->group_relay_log_name,
			      sizeof(rli->group_relay_log_name),
                              &rli->info_file, "") ||
       init_intvar_from_file(&relay_log_pos,
			     &rli->info_file, BIN_LOG_HEADER_SIZE) ||
       init_strvar_from_file(rli->group_master_log_name,
			     sizeof(rli->group_master_log_name),
                             &rli->info_file, "") ||
       init_intvar_from_file(&master_log_pos, &rli->info_file, 0))
    {
      msg="Error reading slave log configuration";
      goto err;
    }
    strmake(rli->event_relay_log_name,rli->group_relay_log_name,
            sizeof(rli->event_relay_log_name)-1);
    rli->group_relay_log_pos= rli->event_relay_log_pos= relay_log_pos;
    rli->group_master_log_pos= master_log_pos;

    if (init_relay_log_pos(rli,
			   rli->group_relay_log_name,
			   rli->group_relay_log_pos,
			   0 /* no data lock*/,
			   &msg, 0))
    {
      char llbuf[22];
      sql_print_error("Failed to open the relay log '%s' (relay_log_pos %s)",
		      rli->group_relay_log_name,
		      llstr(rli->group_relay_log_pos, llbuf));
      goto err;
    }
  }

#ifndef DBUG_OFF
  {
    char llbuf1[22], llbuf2[22];
    DBUG_PRINT("info", ("my_b_tell(rli->cur_log)=%s rli->event_relay_log_pos=%s",
                        llstr(my_b_tell(rli->cur_log),llbuf1), 
                        llstr(rli->event_relay_log_pos,llbuf2)));
    DBUG_ASSERT(rli->event_relay_log_pos >= BIN_LOG_HEADER_SIZE);
    DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->event_relay_log_pos);
  }
#endif

  /*
    Now change the cache from READ to WRITE - must do this
    before flush_relay_log_info
  */
  reinit_io_cache(&rli->info_file, WRITE_CACHE,0L,0,1);
  if ((error= flush_relay_log_info(rli)))
    sql_print_error("Failed to flush relay log info file");
  if (count_relay_log_space(rli))
  {
    msg="Error counting relay log space";
    goto err;
  }
  rli->inited= 1;
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);

err:
  sql_print_error(msg);
  end_io_cache(&rli->info_file);
  if (info_fd >= 0)
    my_close(info_fd, MYF(0));
  rli->info_fd= -1;
  rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(1);
}


static inline int add_relay_log(RELAY_LOG_INFO* rli,LOG_INFO* linfo)
{
  MY_STAT s;
  DBUG_ENTER("add_relay_log");
  if (!my_stat(linfo->log_file_name,&s,MYF(0)))
  {
    sql_print_error("log %s listed in the index, but failed to stat",
		    linfo->log_file_name);
    DBUG_RETURN(1);
  }
  rli->log_space_total += s.st_size;
#ifndef DBUG_OFF
  char buf[22];
  DBUG_PRINT("info",("log_space_total: %s", llstr(rli->log_space_total,buf)));
#endif  
  DBUG_RETURN(0);
}


static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli)
{
  bool slave_killed=0;
  MASTER_INFO* mi = rli->mi;
  const char *save_proc_info;
  THD* thd = mi->io_thd;

  DBUG_ENTER("wait_for_relay_log_space");

  pthread_mutex_lock(&rli->log_space_lock);
  save_proc_info= thd->enter_cond(&rli->log_space_cond,
				  &rli->log_space_lock, 
				  "\
Waiting for the slave SQL thread to free enough relay log space");
  while (rli->log_space_limit < rli->log_space_total &&
	 !(slave_killed=io_slave_killed(thd,mi)) &&
         !rli->ignore_log_space_limit)
    pthread_cond_wait(&rli->log_space_cond, &rli->log_space_lock);
  thd->exit_cond(save_proc_info);
  DBUG_RETURN(slave_killed);
}


static int count_relay_log_space(RELAY_LOG_INFO* rli)
{
  LOG_INFO linfo;
  DBUG_ENTER("count_relay_log_space");
  rli->log_space_total= 0;
  if (rli->relay_log.find_log_pos(&linfo, NullS, 1))
  {
    sql_print_error("Could not find first log while counting relay log space");
    DBUG_RETURN(1);
  }
  do
  {
    if (add_relay_log(rli,&linfo))
      DBUG_RETURN(1);
  } while (!rli->relay_log.find_next_log(&linfo, 1));
  /* 
     As we have counted everything, including what may have written in a
     preceding write, we must reset bytes_written, or we may count some space 
     twice.
  */
  rli->relay_log.reset_bytes_written();
  DBUG_RETURN(0);
}


void init_master_info_with_options(MASTER_INFO* mi)
{
  mi->master_log_name[0] = 0;
  mi->master_log_pos = BIN_LOG_HEADER_SIZE;		// skip magic number
  
  if (master_host)
    strmake(mi->host, master_host, sizeof(mi->host) - 1);
  if (master_user)
    strmake(mi->user, master_user, sizeof(mi->user) - 1);
  if (master_password)
    strmake(mi->password, master_password, MAX_PASSWORD_LENGTH);
  mi->port = master_port;
  mi->connect_retry = master_connect_retry;
  
  mi->ssl= master_ssl;
  if (master_ssl_ca)
    strmake(mi->ssl_ca, master_ssl_ca, sizeof(mi->ssl_ca)-1);
  if (master_ssl_capath)
    strmake(mi->ssl_capath, master_ssl_capath, sizeof(mi->ssl_capath)-1);
  if (master_ssl_cert)
    strmake(mi->ssl_cert, master_ssl_cert, sizeof(mi->ssl_cert)-1);
  if (master_ssl_cipher)
    strmake(mi->ssl_cipher, master_ssl_cipher, sizeof(mi->ssl_cipher)-1);
  if (master_ssl_key)
    strmake(mi->ssl_key, master_ssl_key, sizeof(mi->ssl_key)-1);
}

static void clear_slave_error(RELAY_LOG_INFO* rli)
{
  /* Clear the errors displayed by SHOW SLAVE STATUS */
  rli->last_slave_error[0]= 0;
  rli->last_slave_errno= 0;
}

void clear_slave_error_timestamp(RELAY_LOG_INFO* rli)
{
  rli->last_master_timestamp= 0;
  clear_slave_error(rli);
}

/*
    Reset UNTIL condition for RELAY_LOG_INFO
   SYNOPSYS
    clear_until_condition()
      rli - RELAY_LOG_INFO structure where UNTIL condition should be reset
 */
void clear_until_condition(RELAY_LOG_INFO* rli)
{
  rli->until_condition= RELAY_LOG_INFO::UNTIL_NONE;
  rli->until_log_name[0]= 0;
  rli->until_log_pos= 0;
}


#define LINES_IN_MASTER_INFO_WITH_SSL 14


int init_master_info(MASTER_INFO* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file)
{
  int fd,error;
  char fname[FN_REFLEN+128];
  DBUG_ENTER("init_master_info");

  if (mi->inited)
  {
    /*
      We have to reset read position of relay-log-bin as we may have
      already been reading from 'hotlog' when the slave was stopped
      last time. If this case pos_in_file would be set and we would
      get a crash when trying to read the signature for the binary
      relay log.
    */
    my_b_seek(mi->rli.cur_log, (my_off_t) 0);
    DBUG_RETURN(0);
  }

  mi->mysql=0;
  mi->file_id=1;
  fn_format(fname, master_info_fname, mysql_data_home, "", 4+32);

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */

  pthread_mutex_lock(&mi->data_lock);
  fd = mi->fd;

  /* does master.info exist ? */
  
  if (access(fname,F_OK))
  {
    if (abort_if_no_master_info_file)
    {
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(0);
    }
    /*
      if someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (fd >= 0)
      my_close(fd, MYF(MY_WME));
    if ((fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
    {
      sql_print_error("Failed to create a new master info file (\
file '%s', errno %d)", fname, my_errno);
      goto err;
    }
    if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME)))
    {
      sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
      goto err;
    }

    mi->fd = fd;
    init_master_info_with_options(mi);

  }
  else // file exists
  {
    if (fd >= 0)
      reinit_io_cache(&mi->file, READ_CACHE, 0L,0,0);
    else 
    {
      if ((fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
      {
        sql_print_error("Failed to open the existing master info file (\
file '%s', errno %d)", fname, my_errno);
        goto err;
      }
      if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,
                        0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
        goto err;
      }
    }

    mi->fd = fd;
    int port, connect_retry, master_log_pos, ssl= 0, lines;
    char *first_non_digit;
    
    /*
       Starting from 4.1.x master.info has new format. Now its
       first line contains number of lines in file. By reading this 
       number we will be always distinguish to which version our 
       master.info corresponds to. We can't simply count lines in 
       file since versions before 4.1.x could generate files with more
       lines than needed.
       If first line doesn't contain a number or contain number less than 
       14 then such file is treated like file from pre 4.1.1 version.
       There is no ambiguity when reading an old master.info, as before 
       4.1.1, the first line contained the binlog's name, which is either
       empty or has an extension (contains a '.'), so can't be confused 
       with an integer.

       So we're just reading first line and trying to figure which version 
       is this.
    */
    
    /* 
       The first row is temporarily stored in mi->master_log_name, 
       if it is line count and not binlog name (new format) it will be 
       overwritten by the second row later.
    */
    if (init_strvar_from_file(mi->master_log_name,
			      sizeof(mi->master_log_name), &mi->file,
			      ""))
      goto errwithmsg;
    
    lines= strtoul(mi->master_log_name, &first_non_digit, 10);

    if (mi->master_log_name[0]!='\0' && 
        *first_non_digit=='\0' && lines >= LINES_IN_MASTER_INFO_WITH_SSL)
    {                                          // Seems to be new format
      if (init_strvar_from_file(mi->master_log_name,     
            sizeof(mi->master_log_name), &mi->file, ""))
        goto errwithmsg;
    }
    else
      lines= 7;
    
    if (init_intvar_from_file(&master_log_pos, &mi->file, 4) ||
	init_strvar_from_file(mi->host, sizeof(mi->host), &mi->file,
			      master_host) ||
	init_strvar_from_file(mi->user, sizeof(mi->user), &mi->file,
			      master_user) || 
        init_strvar_from_file(mi->password, SCRAMBLED_PASSWORD_CHAR_LENGTH+1,
                              &mi->file, master_password) ||
	init_intvar_from_file(&port, &mi->file, master_port) ||
	init_intvar_from_file(&connect_retry, &mi->file,
			      master_connect_retry))
      goto errwithmsg;

    /* 
       If file has ssl part use it even if we have server without 
       SSL support. But these option will be ignored later when 
       slave will try connect to master, so in this case warning 
       is printed.
     */
    if (lines >= LINES_IN_MASTER_INFO_WITH_SSL && 
        (init_intvar_from_file(&ssl, &mi->file, master_ssl) ||
         init_strvar_from_file(mi->ssl_ca, sizeof(mi->ssl_ca), 
                               &mi->file, master_ssl_ca) ||
         init_strvar_from_file(mi->ssl_capath, sizeof(mi->ssl_capath), 
                               &mi->file, master_ssl_capath) ||
         init_strvar_from_file(mi->ssl_cert, sizeof(mi->ssl_cert),
                               &mi->file, master_ssl_cert) ||
         init_strvar_from_file(mi->ssl_cipher, sizeof(mi->ssl_cipher),
                               &mi->file, master_ssl_cipher) ||
         init_strvar_from_file(mi->ssl_key, sizeof(mi->ssl_key),
                              &mi->file, master_ssl_key)))
      goto errwithmsg;
#ifndef HAVE_OPENSSL
    if (ssl)
      sql_print_error("SSL information in the master info file "
                      "('%s') are ignored because this MySQL slave was compiled "
                      "without SSL support.", fname);
#endif /* HAVE_OPENSSL */
    
    /*
      This has to be handled here as init_intvar_from_file can't handle
      my_off_t types
    */
    mi->master_log_pos= (my_off_t) master_log_pos;
    mi->port= (uint) port;
    mi->connect_retry= (uint) connect_retry;
    mi->ssl= (my_bool) ssl;
  }
  DBUG_PRINT("master_info",("log_file_name: %s  position: %ld",
			    mi->master_log_name,
			    (ulong) mi->master_log_pos));

  mi->rli.mi = mi;
  if (init_relay_log_info(&mi->rli, slave_info_fname))
    goto err;

  mi->inited = 1;
  // now change cache READ -> WRITE - must do this before flush_master_info
  reinit_io_cache(&mi->file, WRITE_CACHE,0L,0,1);
  if ((error=test(flush_master_info(mi, 1))))
    sql_print_error("Failed to flush master info file");
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);
  
errwithmsg:
  sql_print_error("Error reading master configuration");
  
err:
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&mi->file);
  }
  mi->fd= -1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(1);
}


int register_slave_on_master(MYSQL* mysql)
{
  char buf[1024], *pos= buf;
  uint report_host_len, report_user_len=0, report_password_len=0;

  if (!report_host)
    return 0;
  report_host_len= strlen(report_host);
  if (report_user)
    report_user_len= strlen(report_user);
  if (report_password)
    report_password_len= strlen(report_password);
  /* 30 is a good safety margin */
  if (report_host_len + report_user_len + report_password_len + 30 >
      sizeof(buf))
    return 0;					// safety

  int4store(pos, server_id); pos+= 4;
  pos= net_store_data(pos, report_host, report_host_len); 
  pos= net_store_data(pos, report_user, report_user_len);
  pos= net_store_data(pos, report_password, report_password_len);
  int2store(pos, (uint16) report_port); pos+= 2;
  int4store(pos, rpl_recovery_rank);	pos+= 4;
  /* The master will fill in master_id */
  int4store(pos, 0);			pos+= 4;

  if (simple_command(mysql, COM_REGISTER_SLAVE, (char*) buf,
			(uint) (pos- buf), 0))
  {
    sql_print_error("Error on COM_REGISTER_SLAVE: %d '%s'",
		    mysql_errno(mysql),
		    mysql_error(mysql));
    return 1;
  }
  return 0;
}


/*
  Builds a String from a HASH of TABLE_RULE_ENT. Cannot be used for any other 
  hash, as it assumes that the hash entries are TABLE_RULE_ENT.

  SYNOPSIS
    table_rule_ent_hash_to_str()
    s               pointer to the String to fill
    h               pointer to the HASH to read

  RETURN VALUES
    none
*/

void table_rule_ent_hash_to_str(String* s, HASH* h)
{
  s->length(0);
  for (uint i=0 ; i < h->records ; i++)
  {
    TABLE_RULE_ENT* e= (TABLE_RULE_ENT*) hash_element(h, i);
    if (s->length())
      s->append(',');
    s->append(e->db,e->key_len);
  }
}

/*
  Mostly the same thing as above
*/

void table_rule_ent_dynamic_array_to_str(String* s, DYNAMIC_ARRAY* a)
{
  s->length(0);
  for (uint i=0 ; i < a->elements ; i++)
  {
    TABLE_RULE_ENT* e;
    get_dynamic(a, (gptr)&e, i);
    if (s->length())
      s->append(',');
    s->append(e->db,e->key_len);
  }
}

int show_master_info(THD* thd, MASTER_INFO* mi)
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
  
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(-1);

  if (mi->host[0])
  {
    DBUG_PRINT("info",("host is set: '%s'", mi->host));
    String *packet= &thd->packet;
    protocol->prepare_for_resend();
  
    pthread_mutex_lock(&mi->data_lock);
    pthread_mutex_lock(&mi->rli.data_lock);

    protocol->store(mi->io_thd ? mi->io_thd->proc_info : "", &my_charset_bin);
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
    protocol->store(mi->slave_running ? "Yes":"No", &my_charset_bin);
    protocol->store(mi->rli.slave_running ? "Yes":"No", &my_charset_bin);
    protocol->store(&replicate_do_db);
    protocol->store(&replicate_ignore_db);
    /*
      We can't directly use some protocol->store for 
      replicate_*_table,
      as Protocol doesn't know the TABLE_RULE_ENT struct.
      We first build Strings and then pass them to protocol->store.
    */
    char buf[256];
    String tmp(buf, sizeof(buf), &my_charset_bin);
    table_rule_ent_hash_to_str(&tmp, &replicate_do_table);
    protocol->store(&tmp);
    table_rule_ent_hash_to_str(&tmp, &replicate_ignore_table);
    protocol->store(&tmp);
    table_rule_ent_dynamic_array_to_str(&tmp, &replicate_wild_do_table);
    protocol->store(&tmp);
    table_rule_ent_dynamic_array_to_str(&tmp, &replicate_wild_ignore_table);
    protocol->store(&tmp);

    protocol->store((uint32) mi->rli.last_slave_errno);
    protocol->store(mi->rli.last_slave_error, &my_charset_bin);
    protocol->store((uint32) mi->rli.slave_skip_counter);
    protocol->store((ulonglong) mi->rli.group_master_log_pos);
    protocol->store((ulonglong) mi->rli.log_space_total);

    protocol->store(
      mi->rli.until_condition==RELAY_LOG_INFO::UNTIL_NONE ? "None": 
        ( mi->rli.until_condition==RELAY_LOG_INFO::UNTIL_MASTER_POS? "Master":
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

    if (mi->rli.last_master_timestamp)
    {
      long tmp= (long)((time_t)time((time_t*) 0)
                               - mi->rli.last_master_timestamp)
        - mi->clock_diff_with_master;
      /*
        Apparently on some systems tmp can be <0. Here are possible reasons
        related to MySQL:
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
        This confuses users, so we don't go below 0.
      */
      protocol->store((longlong)(max(0, tmp)));
    }
    else
      protocol->store_null();

    pthread_mutex_unlock(&mi->rli.data_lock);
    pthread_mutex_unlock(&mi->data_lock);
  
    if (my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
      DBUG_RETURN(-1);
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


bool flush_master_info(MASTER_INFO* mi, bool flush_relay_log_cache)
{
  IO_CACHE* file = &mi->file;
  char lbuf[22];
  DBUG_ENTER("flush_master_info");
  DBUG_PRINT("enter",("master_pos: %ld", (long) mi->master_log_pos));

  /*
    Flush the relay log to disk. If we don't do it, then the relay log while
    have some part (its last kilobytes) in memory only, so if the slave server
    dies now, with, say, from master's position 100 to 150 in memory only (not
    on disk), and with position 150 in master.info, then when the slave
    restarts, the I/O thread will fetch binlogs from 150, so in the relay log
    we will have "[0, 100] U [150, infinity[" and nobody will notice it, so the
    SQL thread will jump from 100 to 150, and replication will silently break.

    When we come to this place in code, relay log may or not be initialized;
    the caller is responsible for setting 'flush_relay_log_cache' accordingly.
  */
  if (flush_relay_log_cache)
    flush_io_cache(mi->rli.relay_log.get_log_file());

  /*
    We flushed the relay log BEFORE the master.info file, because if we crash
    now, we will get a duplicate event in the relay log at restart. If we
    flushed in the other order, we would get a hole in the relay log.
    And duplicate is better than hole (with a duplicate, in later versions we
    can add detection and scrap one event; with a hole there's nothing we can
    do).
  */

  /*
     In certain cases this code may create master.info files that seems 
     corrupted, because of extra lines filled with garbage in the end 
     file (this happens if new contents take less space than previous 
     contents of file). But because of number of lines in the first line 
     of file we don't care about this garbage.
  */
  
  my_b_seek(file, 0L);
  my_b_printf(file, "%u\n%s\n%s\n%s\n%s\n%s\n%d\n%d\n%d\n%s\n%s\n%s\n%s\n%s\n",
	      LINES_IN_MASTER_INFO_WITH_SSL,
              mi->master_log_name, llstr(mi->master_log_pos, lbuf),
	      mi->host, mi->user,
	      mi->password, mi->port, mi->connect_retry,
              (int)(mi->ssl), mi->ssl_ca, mi->ssl_capath, mi->ssl_cert,
              mi->ssl_cipher, mi->ssl_key);
  flush_io_cache(file);
  DBUG_RETURN(0);
}


st_relay_log_info::st_relay_log_info()
  :info_fd(-1), cur_log_fd(-1), save_temporary_tables(0),
   cur_log_old_open_count(0), group_master_log_pos(0), log_space_total(0),
   ignore_log_space_limit(0), last_master_timestamp(0), slave_skip_counter(0),
   abort_pos_wait(0), slave_run_id(0), sql_thd(0), last_slave_errno(0),
   inited(0), abort_slave(0), slave_running(0), until_condition(UNTIL_NONE),
   until_log_pos(0)
{
  group_relay_log_name[0]= event_relay_log_name[0]=
    group_master_log_name[0]= 0;
  last_slave_error[0]=0; until_log_name[0]= 0;

  bzero((char*) &info_file, sizeof(info_file));
  bzero((char*) &cache_buf, sizeof(cache_buf));
  pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&log_space_lock, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&data_cond, NULL);
  pthread_cond_init(&start_cond, NULL);
  pthread_cond_init(&stop_cond, NULL);
  pthread_cond_init(&log_space_cond, NULL);
  relay_log.init_pthread_objects();
}


st_relay_log_info::~st_relay_log_info()
{
  pthread_mutex_destroy(&run_lock);
  pthread_mutex_destroy(&data_lock);
  pthread_mutex_destroy(&log_space_lock);
  pthread_cond_destroy(&data_cond);
  pthread_cond_destroy(&start_cond);
  pthread_cond_destroy(&stop_cond);
  pthread_cond_destroy(&log_space_cond);
}

/*
  Waits until the SQL thread reaches (has executed up to) the
  log/position or timed out.

  SYNOPSIS
    wait_for_pos()
    thd             client thread that sent SELECT MASTER_POS_WAIT
    log_name        log name to wait for
    log_pos         position to wait for 
    timeout         timeout in seconds before giving up waiting

  NOTES
    timeout is longlong whereas it should be ulong ; but this is
    to catch if the user submitted a negative timeout.

  RETURN VALUES
    -2          improper arguments (log_pos<0)
                or slave not running, or master info changed
                during the function's execution,
                or client thread killed. -2 is translated to NULL by caller
    -1          timed out
    >=0         number of log events the function had to wait
                before reaching the desired log/position
 */

int st_relay_log_info::wait_for_pos(THD* thd, String* log_name,
                                    longlong log_pos,
                                    longlong timeout)
{
  if (!inited)
    return -1;
  int event_count = 0;
  ulong init_abort_pos_wait;
  int error=0;
  struct timespec abstime; // for timeout checking
  set_timespec(abstime,timeout);

  DBUG_ENTER("wait_for_pos");
  DBUG_PRINT("enter",("group_master_log_name: '%s'  pos: %lu timeout: %ld",
                      group_master_log_name, (ulong) group_master_log_pos, 
                      (long) timeout));

  pthread_mutex_lock(&data_lock);
  const char *msg= thd->enter_cond(&data_cond, &data_lock,
                                   "Waiting for the slave SQL thread to "
                                   "advance position");
  /* 
     This function will abort when it notices that some CHANGE MASTER or
     RESET MASTER has changed the master info.
     To catch this, these commands modify abort_pos_wait ; We just monitor
     abort_pos_wait and see if it has changed.
     Why do we have this mechanism instead of simply monitoring slave_running
     in the loop (we do this too), as CHANGE MASTER/RESET SLAVE require that
     the SQL thread be stopped?
     This is becasue if someones does:
     STOP SLAVE;CHANGE MASTER/RESET SLAVE; START SLAVE;
     the change may happen very quickly and we may not notice that
     slave_running briefly switches between 1/0/1.
  */
  init_abort_pos_wait= abort_pos_wait;

  /*
    We'll need to 
    handle all possible log names comparisons (e.g. 999 vs 1000).
    We use ulong for string->number conversion ; this is no 
    stronger limitation than in find_uniq_filename in sql/log.cc
  */
  ulong log_name_extension;
  char log_name_tmp[FN_REFLEN]; //make a char[] from String
  char *end= strmake(log_name_tmp, log_name->ptr(), min(log_name->length(),
							FN_REFLEN-1));
  char *p= fn_ext(log_name_tmp);
  char *p_end;
  if (!*p || log_pos<0)   
  {
    error= -2; //means improper arguments
    goto err;
  }
  // Convert 0-3 to 4
  log_pos= max(log_pos, BIN_LOG_HEADER_SIZE);
  /* p points to '.' */
  log_name_extension= strtoul(++p, &p_end, 10);
  /*
    p_end points to the first invalid character.
    If it equals to p, no digits were found, error.
    If it contains '\0' it means conversion went ok.
  */
  if (p_end==p || *p_end)
  {
    error= -2;
    goto err;
  }    

  /* The "compare and wait" main loop */
  while (!thd->killed &&
         init_abort_pos_wait == abort_pos_wait &&
         slave_running)
  {
    bool pos_reached;
    int cmp_result= 0;

    /*
      group_master_log_name can be "", if we are just after a fresh
      replication start or after a CHANGE MASTER TO MASTER_HOST/PORT
      (before we have executed one Rotate event from the master) or
      (rare) if the user is doing a weird slave setup (see next
      paragraph).  If group_master_log_name is "", we assume we don't
      have enough info to do the comparison yet, so we just wait until
      more data. In this case master_log_pos is always 0 except if
      somebody (wrongly) sets this slave to be a slave of itself
      without using --replicate-same-server-id (an unsupported
      configuration which does nothing), then group_master_log_pos
      will grow and group_master_log_name will stay "".
    */
    if (*group_master_log_name)
    {
      char *basename= (group_master_log_name +
                       dirname_length(group_master_log_name));
      /*
        First compare the parts before the extension.
        Find the dot in the master's log basename,
        and protect against user's input error :
        if the names do not match up to '.' included, return error
      */
      char *q= (char*)(fn_ext(basename)+1);
      if (strncmp(basename, log_name_tmp, (int)(q-basename)))
      {
        error= -2;
        break;
      }
      // Now compare extensions.
      char *q_end;
      ulong group_master_log_name_extension= strtoul(q, &q_end, 10);
      if (group_master_log_name_extension < log_name_extension)
        cmp_result= -1 ;
      else
        cmp_result= (group_master_log_name_extension > log_name_extension) ? 1 : 0 ;

      pos_reached= ((!cmp_result && group_master_log_pos >= (ulonglong)log_pos) ||
                    cmp_result > 0);
      if (pos_reached || thd->killed)
        break;
    }

    //wait for master update, with optional timeout.
    
    DBUG_PRINT("info",("Waiting for master update"));
    /*
      We are going to pthread_cond_(timed)wait(); if the SQL thread stops it
      will wake us up.
    */
    if (timeout > 0)
    {
      /*
        Note that pthread_cond_timedwait checks for the timeout
        before for the condition ; i.e. it returns ETIMEDOUT 
        if the system time equals or exceeds the time specified by abstime
        before the condition variable is signaled or broadcast, _or_ if
        the absolute time specified by abstime has already passed at the time
        of the call.
        For that reason, pthread_cond_timedwait will do the "timeoutting" job
        even if its condition is always immediately signaled (case of a loaded
        master).
      */
      error=pthread_cond_timedwait(&data_cond, &data_lock, &abstime);
    }
    else
      pthread_cond_wait(&data_cond, &data_lock);
    DBUG_PRINT("info",("Got signal of master update or timed out"));
    if (error == ETIMEDOUT || error == ETIME)
    {
      error= -1;
      break;
    }
    error=0;
    event_count++;
    DBUG_PRINT("info",("Testing if killed or SQL thread not running"));
  }

err:
  thd->exit_cond(msg);
  DBUG_PRINT("exit",("killed: %d  abort: %d  slave_running: %d \
improper_arguments: %d  timed_out: %d",
                     thd->killed_errno(),
                     (int) (init_abort_pos_wait != abort_pos_wait),
                     (int) slave_running,
                     (int) (error == -2),
                     (int) (error == -1)));
  if (thd->killed || init_abort_pos_wait != abort_pos_wait ||
      !slave_running) 
  {
    error= -2;
  }
  DBUG_RETURN( error ? error : event_count );
}

void set_slave_thread_options(THD* thd)
{
  thd->options = ((opt_log_slave_updates) ? OPTION_BIN_LOG:0) |
    OPTION_AUTO_IS_NULL;
}

/*
  init_slave_thread()
*/

static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type)
{
  DBUG_ENTER("init_slave_thread");
  thd->system_thread = (thd_type == SLAVE_THD_SQL) ?
    SYSTEM_THREAD_SLAVE_SQL : SYSTEM_THREAD_SLAVE_IO; 
  thd->host_or_ip= "";
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->slave_thread = 1;
  set_slave_thread_options(thd);
  /* 
     It's nonsense to constrain the slave threads with max_join_size; if a
     query succeeded on master, we HAVE to execute it. So set
     OPTION_BIG_SELECTS. Setting max_join_size to HA_POS_ERROR is not enough
     (and it's not needed if we have OPTION_BIG_SELECTS) because an INSERT
     SELECT examining more than 4 billion rows would still fail (yes, because
     when max_join_size is 4G, OPTION_BIG_SELECTS is automatically set, but
     only for client threads.
  */
  thd->options = ((opt_log_slave_updates) ? OPTION_BIN_LOG:0) |
    OPTION_AUTO_IS_NULL | OPTION_BIG_SELECTS;
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  thd->real_id=pthread_self();
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    delete thd;
    DBUG_RETURN(-1);
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  if (thd_type == SLAVE_THD_SQL)
    thd->proc_info= "Waiting for the next event in relay log";
  else
    thd->proc_info= "Waiting for master update";
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}


static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg)
{
  int nap_time;
  thr_alarm_t alarmed;
  thr_alarm_init(&alarmed);
  time_t start_time= time((time_t*) 0);
  time_t end_time= start_time+sec;

  while ((nap_time= (int) (end_time - start_time)) > 0)
  {
    ALARM alarm_buff;
    /*
      The only reason we are asking for alarm is so that
      we will be woken up in case of murder, so if we do not get killed,
      set the alarm so it goes off after we wake up naturally
    */
    thr_alarm(&alarmed, 2 * nap_time, &alarm_buff);
    sleep(nap_time);
    thr_end_alarm(&alarmed);
    
    if ((*thread_killed)(thd,thread_killed_arg))
      return 1;
    start_time=time((time_t*) 0);
  }
  return 0;
}


static int request_dump(MYSQL* mysql, MASTER_INFO* mi,
			bool *suppress_warnings)
{
  char buf[FN_REFLEN + 10];
  int len;
  int binlog_flags = 0; // for now
  char* logname = mi->master_log_name;
  DBUG_ENTER("request_dump");

  // TODO if big log files: Change next to int8store()
  int4store(buf, (longlong) mi->master_log_pos);
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
      *suppress_warnings= 1;			// Suppress reconnect warning
    else
      sql_print_error("Error on COM_BINLOG_DUMP: %d  %s, will retry in %d secs",
		      mysql_errno(mysql), mysql_error(mysql),
		      master_connect_retry);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


static int request_table_dump(MYSQL* mysql, const char* db, const char* table)
{
  char buf[1024];
  char * p = buf;
  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if (table_len + db_len > sizeof(buf) - 2)
  {
    sql_print_error("request_table_dump: Buffer overrun");
    return 1;
  } 
  
  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);
  
  if (simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
  {
    sql_print_error("request_table_dump: Error sending the table dump \
command");
    return 1;
  }

  return 0;
}


/*
  Read one event from the master
  
  SYNOPSIS
    read_event()
    mysql		MySQL connection
    mi			Master connection information
    suppress_warnings	TRUE when a normal net read timeout has caused us to
			try a reconnect.  We do not want to print anything to
			the error log in this case because this a anormal
			event in an idle server.

    RETURN VALUES
    'packet_error'	Error
    number		Length of packet
*/

static ulong read_event(MYSQL* mysql, MASTER_INFO *mi, bool* suppress_warnings)
{
  ulong len;

  *suppress_warnings= 0;
  /*
    my_real_read() will time us out
    We check if we were told to die, and if not, try reading again

    TODO:  Move 'events_till_disconnect' to the MASTER_INFO structure
  */
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(events_till_disconnect--))
    return packet_error;      
#endif
  
  len = net_safe_read(mysql);
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
      sql_print_error("Error reading packet from server: %s (\
server_errno=%d)",
		      mysql_error(mysql), mysql_errno(mysql));
    return packet_error;
  }

  /* Check if eof packet */
  if (len < 8 && mysql->net.read_pos[0] == 254)
  {
     sql_print_error("Slave: received end packet from server, apparent\
 master shutdown: %s",
		     mysql_error(mysql));
     return packet_error;
  }
  
  DBUG_PRINT("info",( "len=%u, net->read_pos[4] = %d\n",
		      len, mysql->net.read_pos[4]));
  return len - 1;   
}


int check_expected_error(THD* thd, RELAY_LOG_INFO* rli, int expected_error)
{
  switch (expected_error) {
  case ER_NET_READ_ERROR:
  case ER_NET_ERROR_ON_WRITE:  
  case ER_SERVER_SHUTDOWN:  
  case ER_NEW_ABORTING_CONNECTION:
    return 1;
  default:
    return 0;
  }
}

/*
     Check if condition stated in UNTIL clause of START SLAVE is reached.
   SYNOPSYS
     st_relay_log_info::is_until_satisfied()
   DESCRIPTION
     Checks if UNTIL condition is reached. Uses caching result of last 
     comparison of current log file name and target log file name. So cached 
     value should be invalidated if current log file name changes 
     (see st_relay_log_info::notify_... functions).
     
     This caching is needed to avoid of expensive string comparisons and 
     strtol() conversions needed for log names comparison. We don't need to
     compare them each time this function is called, we only need to do this 
     when current log name changes. If we have UNTIL_MASTER_POS condition we 
     need to do this only after Rotate_log_event::exec_event() (which is 
     rare, so caching gives real benifit), and if we have UNTIL_RELAY_POS 
     condition then we should invalidate cached comarison value after 
     inc_group_relay_log_pos() which called for each group of events (so we
     have some benefit if we have something like queries that use 
     autoincrement or if we have transactions).
     
     Should be called ONLY if until_condition != UNTIL_NONE !
   RETURN VALUE
     true - condition met or error happened (condition seems to have 
            bad log file name)
     false - condition not met
*/

bool st_relay_log_info::is_until_satisfied()
{
  const char *log_name;
  ulonglong log_pos;

  DBUG_ASSERT(until_condition != UNTIL_NONE);
  
  if (until_condition == UNTIL_MASTER_POS)
  {
    log_name= group_master_log_name;
    log_pos= group_master_log_pos;
  }
  else
  { /* until_condition == UNTIL_RELAY_POS */
    log_name= group_relay_log_name;
    log_pos= group_relay_log_pos;
  }
  
  if (until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_UNKNOWN)
  {
    /*
      We have no cached comparison results so we should compare log names
      and cache result.
      If we are after RESET SLAVE, and the SQL slave thread has not processed
      any event yet, it could be that group_master_log_name is "". In that case,
      just wait for more events (as there is no sensible comparison to do).
    */

    if (*log_name)
    {
      const char *basename= log_name + dirname_length(log_name);
      
      const char *q= (const char*)(fn_ext(basename)+1);
      if (strncmp(basename, until_log_name, (int)(q-basename)) == 0)
      {
        /* Now compare extensions. */
        char *q_end;
        ulong log_name_extension= strtoul(q, &q_end, 10);
        if (log_name_extension < until_log_name_extension)
          until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_LESS;
        else
          until_log_names_cmp_result= 
            (log_name_extension > until_log_name_extension) ? 
            UNTIL_LOG_NAMES_CMP_GREATER : UNTIL_LOG_NAMES_CMP_EQUAL ;
      }
      else  
      {
        /* Probably error so we aborting */
        sql_print_error("Slave SQL thread is stopped because UNTIL "
                        "condition is bad.");
        return TRUE;
      }
    }
    else
      return until_log_pos == 0;
  }
    
  return ((until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_EQUAL && 
           log_pos >= until_log_pos) ||
          until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_GREATER);
}


static int exec_relay_log_event(THD* thd, RELAY_LOG_INFO* rli)
{
  /*
     We acquire this mutex since we need it for all operations except
     event execution. But we will release it in places where we will 
     wait for something for example inside of next_event().
   */
  pthread_mutex_lock(&rli->data_lock);
  
  if (rli->until_condition!=RELAY_LOG_INFO::UNTIL_NONE && 
      rli->is_until_satisfied()) 
  {
    sql_print_error("Slave SQL thread stopped because it reached its"
                    " UNTIL position %ld", (long) rli->until_pos());
    /* 
      Setting abort_slave flag because we do not want additional message about
      error in query execution to be printed.
    */
    rli->abort_slave= 1;
    pthread_mutex_unlock(&rli->data_lock);
    return 1;
  }
  
  Log_event * ev = next_event(rli);
  
  DBUG_ASSERT(rli->sql_thd==thd);
  
  if (sql_slave_killed(thd,rli))
  {
    pthread_mutex_unlock(&rli->data_lock);
    delete ev;
    return 1;
  }
  if (ev)
  {
    int type_code = ev->get_type_code();
    int exec_res;

    /*
      Queries originating from this server must be skipped.
      Low-level events (Format_desc, Rotate, Stop) from this server
      must also be skipped. But for those we don't want to modify
      group_master_log_pos, because these events did not exist on the master.
      Format_desc is not completely skipped.
      Skip queries specified by the user in slave_skip_counter.
      We can't however skip events that has something to do with the
      log files themselves.
      Filtering on own server id is extremely important, to ignore execution of
      events created by the creation/rotation of the relay log (remember that
      now the relay log starts with its Format_desc, has a Rotate etc).
    */
    
    DBUG_PRINT("info",("type_code=%d, server_id=%d",type_code,ev->server_id));
    
    if ((ev->server_id == (uint32) ::server_id &&
         !replicate_same_server_id &&
         type_code != FORMAT_DESCRIPTION_EVENT) ||
 	(rli->slave_skip_counter && 
         type_code != ROTATE_EVENT && type_code != STOP_EVENT &&
         type_code != START_EVENT_V3 && type_code!= FORMAT_DESCRIPTION_EVENT))
    {
      DBUG_PRINT("info", ("event skipped"));
      if (thd->options & OPTION_BEGIN)
        rli->inc_event_relay_log_pos();
      else
      {
        rli->inc_group_relay_log_pos((type_code == ROTATE_EVENT || 
                                      type_code == STOP_EVENT ||
                                      type_code == FORMAT_DESCRIPTION_EVENT) ?
                                     LL(0) : ev->log_pos,
                                     1/* skip lock*/);
        flush_relay_log_info(rli);
      }
      
      /*
 	Protect against common user error of setting the counter to 1
 	instead of 2 while recovering from an insert which used auto_increment,
 	rand or user var.
      */
      if (rli->slave_skip_counter && 
 	  !((type_code == INTVAR_EVENT ||
             type_code == RAND_EVENT || 
             type_code == USER_VAR_EVENT) &&
 	    rli->slave_skip_counter == 1) &&
          /*
            The events from ourselves which have something to do with the relay
            log itself must be skipped, true, but they mustn't decrement
            rli->slave_skip_counter, because the user is supposed to not see
            these events (they are not in the master's binlog) and if we
            decremented, START SLAVE would for example decrement when it sees
            the Rotate, so the event which the user probably wanted to skip
            would not be skipped.
          */
          !(ev->server_id == (uint32) ::server_id &&
            (type_code == ROTATE_EVENT || type_code == STOP_EVENT ||
             type_code == START_EVENT_V3 || type_code == FORMAT_DESCRIPTION_EVENT)))
        --rli->slave_skip_counter;
      pthread_mutex_unlock(&rli->data_lock);
      delete ev;     
      return 0;					// avoid infinite update loops
    } 
    pthread_mutex_unlock(&rli->data_lock);
  
    thd->server_id = ev->server_id; // use the original server id for logging
    thd->set_time();				// time the query
    thd->lex->current_select= 0;
    if (!ev->when)
      ev->when = time(NULL);
    ev->thd = thd;
    exec_res = ev->exec_event(rli);
    DBUG_ASSERT(rli->sql_thd==thd);
    /* 
       Format_description_log_event should not be deleted because it will be
       used to read info about the relay log's format; it will be deleted when
       the SQL thread does not need it, i.e. when this thread terminates.
    */
    if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT)
    {
      DBUG_PRINT("info", ("Deleting the event after it has been executed"));
      delete ev;
    }
    return exec_res;
  }
  else
  {
    pthread_mutex_unlock(&rli->data_lock);
    slave_print_error(rli, 0, "\
Could not parse relay log event entry. The possible reasons are: the master's \
binary log is corrupted (you can check this by running 'mysqlbinlog' on the \
binary log), the slave's relay log is corrupted (you can check this by running \
'mysqlbinlog' on the relay log), a network problem, or a bug in the master's \
or slave's MySQL code. If you want to check the master's binary log or slave's \
relay log, you will be able to know their names by issuing 'SHOW SLAVE STATUS' \
on this slave.\
");
    return 1;
  }
}


/* Slave I/O Thread entry point */

extern "C" pthread_handler_decl(handle_slave_io,arg)
{
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql;
  MASTER_INFO *mi = (MASTER_INFO*)arg; 
  char llbuff[22];
  uint retry_count;
  
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_io");

#ifndef DBUG_OFF
slave_begin:
#endif  
  DBUG_ASSERT(mi->inited);
  mysql= NULL ;
  retry_count= 0;

  pthread_mutex_lock(&mi->run_lock);
  /* Inform waiting threads that slave has started */
  mi->slave_run_id++;

#ifndef DBUG_OFF  
  mi->events_till_abort = abort_slave_event_count;
#endif  
  
  thd= new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_IO))
  {
    pthread_cond_broadcast(&mi->start_cond);
    pthread_mutex_unlock(&mi->run_lock);
    sql_print_error("Failed during slave I/O thread initialization");
    goto err;
  }
  mi->io_thd = thd;
  thd->thread_stack = (char*)&thd; // remember where our stack is
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  mi->slave_running = 1;
  mi->abort_slave = 0;
  pthread_mutex_unlock(&mi->run_lock);
  pthread_cond_broadcast(&mi->start_cond);
  
  DBUG_PRINT("master_info",("log_file_name: '%s'  position: %s",
			    mi->master_log_name,
			    llstr(mi->master_log_pos,llbuff)));
  
  if (!(mi->mysql = mysql = mysql_init(NULL)))
  {
    sql_print_error("Slave I/O thread: error in mysql_init()");
    goto err;
  }
  

  thd->proc_info = "Connecting to master";
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
    sql_print_error("Slave I/O thread: connected to master '%s@%s:%d',\
  replication started in log '%s' at position %s", mi->user,
		    mi->host, mi->port,
		    IO_RPL_LOG_NAME,
		    llstr(mi->master_log_pos,llbuff));
  else
  {
    sql_print_error("Slave I/O thread killed while connecting to master");
    goto err;
  }

connected:

  thd->slave_net = &mysql->net;
  thd->proc_info = "Checking master version";
  if (get_master_version_and_clock(mysql, mi))
    goto err;

  if (mi->rli.relay_log.description_event_for_queue->binlog_version > 1)
  {
    /*
      Register ourselves with the master.
      If fails, this is not fatal - we just print the error message and go
      on with life.
    */
    thd->proc_info = "Registering slave on master";
    if (register_slave_on_master(mysql) ||  update_slave_list(mysql, mi))
      goto err;
  }
  
  DBUG_PRINT("info",("Starting reading binary log from master"));
  while (!io_slave_killed(thd,mi))
  {
    bool suppress_warnings= 0;    
    thd->proc_info = "Requesting binlog dump";
    if (request_dump(mysql, mi, &suppress_warnings))
    {
      sql_print_error("Failed on request_dump()");
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while requesting master \
dump");
	goto err;
      }
	  
      thd->proc_info= "Waiting to reconnect after a failed binlog dump request";
#ifdef SIGNAL_WITH_VIO_CLOSE
      thd->clear_active_vio();
#endif
      end_server(mysql);
      /*
	First time retry immediately, assuming that we can recover
	right away - if first time fails, sleep between re-tries
	hopefuly the admin can fix the problem sometime
      */
      if (retry_count++)
      {
	if (retry_count > master_retry_count)
	  goto err;				// Don't retry forever
	safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		   (void*)mi);
      }
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while retrying master \
dump");
	goto err;
      }

      thd->proc_info = "Reconnecting after a failed binlog dump request";
      if (!suppress_warnings)
	sql_print_error("Slave I/O thread: failed dump request, \
reconnecting to try again, log '%s' at postion %s", IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
      if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	  io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed during or \
after reconnect");
	goto err;
      }

      goto connected;
    }

    while (!io_slave_killed(thd,mi))
    {
      bool suppress_warnings= 0;    
      /* 
         We say "waiting" because read_event() will wait if there's nothing to
         read. But if there's something to read, it will not wait. The important
         thing is to not confuse users by saying "reading" whereas we're in fact
         receiving nothing.
      */
      thd->proc_info = "Waiting for master to send event";
      ulong event_len = read_event(mysql, mi, &suppress_warnings);
      if (io_slave_killed(thd,mi))
      {
	if (global_system_variables.log_warnings)
	  sql_print_error("Slave I/O thread killed while reading event");
	goto err;
      }
	  	  
      if (event_len == packet_error)
      {
	uint mysql_error_number= mysql_errno(mysql);
	if (mysql_error_number == ER_NET_PACKET_TOO_LARGE)
	{
	  sql_print_error("\
Log entry on master is longer than max_allowed_packet (%ld) on \
slave. If the entry is correct, restart the server with a higher value of \
max_allowed_packet",
			  thd->variables.max_allowed_packet);
	  goto err;
	}
	if (mysql_error_number == ER_MASTER_FATAL_ERROR_READING_BINLOG)
	{
	  sql_print_error(ER(mysql_error_number), mysql_error_number,
			  mysql_error(mysql));
	  goto err;
	}
	thd->proc_info = "Waiting to reconnect after a failed master event read";
#ifdef SIGNAL_WITH_VIO_CLOSE
        thd->clear_active_vio();
#endif
	end_server(mysql);
	if (retry_count++)
	{
	  if (retry_count > master_retry_count)
	    goto err;				// Don't retry forever
	  safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		     (void*) mi);
	}	    
	if (io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed while waiting to \
reconnect after a failed read");
	  goto err;
	}
	thd->proc_info = "Reconnecting after a failed master event read";
	if (!suppress_warnings)
	  sql_print_error("Slave I/O thread: Failed reading log event, \
reconnecting to retry, log '%s' position %s", IO_RPL_LOG_NAME,
			  llstr(mi->master_log_pos, llbuff));
	if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	    io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed during or after a \
reconnect done to recover from failed read");
	  goto err;
	}
	goto connected;
      } // if (event_len == packet_error)
	  
      retry_count=0;			// ok event, reset retry counter
      thd->proc_info = "Queueing master event to the relay log";
      if (queue_event(mi,(const char*)mysql->net.read_pos + 1,
		      event_len))
      {
	sql_print_error("Slave I/O thread could not queue event from master");
	goto err;
      }
      flush_master_info(mi, 1); /* sure that we can flush the relay log */
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
                            llstr(mi->rli.log_space_limit,llbuf1),
                            llstr(mi->rli.log_space_total,llbuf2),
                            (int) mi->rli.ignore_log_space_limit)); 
      }
#endif

      if (mi->rli.log_space_limit && mi->rli.log_space_limit <
	  mi->rli.log_space_total &&
          !mi->rli.ignore_log_space_limit)
	if (wait_for_relay_log_space(&mi->rli))
	{
	  sql_print_error("Slave I/O thread aborted while waiting for relay \
log space");
	  goto err;
	}
      // TODO: check debugging abort code
#ifndef DBUG_OFF
      if (abort_slave_event_count && !--events_till_abort)
      {
	sql_print_error("Slave I/O thread: debugging abort");
	goto err;
      }
#endif
    } 
  }

  // error = 0;
err:
  // print the current replication position
  sql_print_error("Slave I/O thread exiting, read up to log '%s', position %s",
		  IO_RPL_LOG_NAME, llstr(mi->master_log_pos,llbuff));
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query = thd->db = 0; // extra safety
  thd->query_length = 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (mysql)
  {
    mysql_close(mysql);
    mi->mysql=0;
  }
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&mi->run_lock);
  mi->slave_running = 0;
  mi->io_thd = 0;
  /* Forget the relay log's format */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;
  // TODO: make rpl_status part of MASTER_INFO
  change_rpl_status(RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE);
  mi->abort_slave = 0; // TODO: check if this is needed
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_broadcast(&mi->stop_cond);	// tell the world we are done
  pthread_mutex_unlock(&mi->run_lock);
#ifndef DBUG_OFF
  if (abort_slave_event_count && !events_till_abort)
    goto slave_begin;
#endif  
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


/* Slave SQL Thread entry point */

extern "C" pthread_handler_decl(handle_slave_sql,arg)
{
  THD *thd;			/* needs to be first for thread_stack */
  char llbuff[22],llbuff1[22];
  RELAY_LOG_INFO* rli = &((MASTER_INFO*)arg)->rli; 
  const char *errmsg;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_sql");

#ifndef DBUG_OFF
slave_begin:  
#endif  

  DBUG_ASSERT(rli->inited);
  pthread_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
  errmsg= 0;
#ifndef DBUG_OFF  
  rli->events_till_abort = abort_slave_event_count;
#endif  

  thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->thread_stack = (char*)&thd; // remember where our stack is
  
  /* Inform waiting threads that slave has started */
  rli->slave_run_id++;

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_SQL))
  {
    /*
      TODO: this is currently broken - slave start and change master
      will be stuck if we fail here
    */
    pthread_cond_broadcast(&rli->start_cond);
    pthread_mutex_unlock(&rli->run_lock);
    sql_print_error("Failed during slave thread initialization");
    goto err;
  }
  thd->init_for_queries();
  rli->sql_thd= thd;
  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  rli->slave_running = 1;
  rli->abort_slave = 0;
  pthread_mutex_unlock(&rli->run_lock);
  pthread_cond_broadcast(&rli->start_cond);

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
  clear_slave_error(rli);

  //tell the I/O thread to take relay_log_space_limit into account from now on
  pthread_mutex_lock(&rli->log_space_lock);
  rli->ignore_log_space_limit= 0;
  pthread_mutex_unlock(&rli->log_space_lock);

  if (init_relay_log_pos(rli,
			 rli->group_relay_log_name,
			 rli->group_relay_log_pos,
			 1 /*need data lock*/, &errmsg,
                         1 /*look for a description_event*/))
  {
    sql_print_error("Error initializing relay log position: %s",
		    errmsg);
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
    sql_print_error("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", RPL_LOG_NAME,
		    llstr(rli->group_master_log_pos,llbuff),rli->group_relay_log_name,
		    llstr(rli->group_relay_log_pos,llbuff1));

  /* execute init_slave variable */
  if (sys_init_slave.value_length)
  {
    execute_init_command(thd, &sys_init_slave, &LOCK_sys_init_slave);
    if (thd->query_error)
    {
      sql_print_error("\
Slave SQL thread aborted. Can't execute init_slave query");
      goto err;
    }
  }

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    thd->proc_info = "Reading event from the relay log"; 
    DBUG_ASSERT(rli->sql_thd == thd);
    THD_CHECK_SENTRY(thd);
    if (exec_relay_log_event(thd,rli))
    {
      // do not scare the user if SQL thread was simply killed or stopped
      if (!sql_slave_killed(thd,rli))
        sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s",
		      RPL_LOG_NAME, llstr(rli->group_master_log_pos, llbuff));
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_error("Slave SQL thread exiting, replication stopped in log \
 '%s' at position %s",
		  RPL_LOG_NAME, llstr(rli->group_master_log_pos,llbuff));

 err:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  /*
    Some extra safety, which should not been needed (normally, event deletion
    should already have done these assignments (each event which sets these
    variables is supposed to set them to 0 before terminating)).
  */
  thd->query= thd->db= thd->catalog= 0; 
  thd->query_length = 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&rli->run_lock);
  /* We need data_lock, at least to wake up any waiting master_pos_wait() */
  pthread_mutex_lock(&rli->data_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  /* When master_pos_wait() wakes up it will check this and terminate */
  rli->slave_running= 0; 
  /* Forget the relay log's format */
  delete rli->relay_log.description_event_for_exec;
  rli->relay_log.description_event_for_exec= 0;
  /* Wake up master_pos_wait() */
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_PRINT("info",("Signaling possibly waiting master_pos_wait() functions"));
  pthread_cond_broadcast(&rli->data_cond);
  rli->ignore_log_space_limit= 0; /* don't need any lock */
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
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_broadcast(&rli->stop_cond);
  // tell the world we are done
  pthread_mutex_unlock(&rli->run_lock);
#ifndef DBUG_OFF // TODO: reconsider the code below
  if (abort_slave_event_count && !rli->events_till_abort)
    goto slave_begin;
#endif  
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


/*
  process_io_create_file()
*/

static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD *thd = mi->io_thd;
  NET *net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

  if (unlikely(!cev->is_valid()))
    DBUG_RETURN(1);
  /*
    TODO: fix to honor table rules, not only db rules
  */
  if (!db_ok(cev->db, replicate_do_db, replicate_ignore_db))
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
	net_write_command(net, 0, "", 0, "", 0);/* 3.23 master wants it */
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
	  sql_print_error("Slave I/O: error writing Exec_load event to \
relay log");
	  goto err;
	}
	mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
	break;
      }
      if (unlikely(cev_not_written))
      {
	cev->block = (char*)net->read_pos;
	cev->block_len = num_bytes;
	if (unlikely(mi->rli.relay_log.append(cev)))
	{
	  sql_print_error("Slave I/O: error writing Create_file event to \
relay log");
	  goto err;
	}
	cev_not_written=0;
	mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
      }
      else
      {
	aev.block = (char*)net->read_pos;
	aev.block_len = num_bytes;
	aev.log_pos = cev->log_pos;
	if (unlikely(mi->rli.relay_log.append(&aev)))
	{
	  sql_print_error("Slave I/O: error writing Append_block event to \
relay log");
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
    mi			master_info for the slave
    rev			The rotate log event read from the binary log

  DESCRIPTION
    Updates the master info with the place in the next binary
    log where we should start reading.
    Rotate the relay log to avoid mixed-format relay logs.

  NOTES
    We assume we already locked mi->data_lock

  RETURN VALUES
    0		ok
    1	        Log event is illegal

*/

static int process_io_rotate(MASTER_INFO *mi, Rotate_log_event *rev)
{
  DBUG_ENTER("process_io_rotate");
  safe_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);

  memcpy(mi->master_log_name, rev->new_log_ident, rev->ident_len+1);
  mi->master_log_pos= rev->pos;
  DBUG_PRINT("info", ("master_log_pos: '%s' %d",
		      mi->master_log_name, (ulong) mi->master_log_pos));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    events_till_disconnect++;
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
  rotate_relay_log(mi); /* will take the right mutexes */
  DBUG_RETURN(0);
}

/*
  Reads a 3.23 event and converts it to the slave's format. This code was copied
  from MySQL 4.0.
*/
static int queue_binlog_ver_1_event(MASTER_INFO *mi, const char *buf,
			   ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  bool ignore_event= 0;
  char *tmp_buf = 0;
  RELAY_LOG_INFO *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_1_event");

  /*
    If we get Load event, we need to pass a non-reusable buffer
    to read_log_event, so we do a trick
  */
  if (buf[EVENT_TYPE_OFFSET] == LOAD_EVENT)
  {
    if (unlikely(!(tmp_buf=(char*)my_malloc(event_len+1,MYF(MY_WME)))))
    {
      sql_print_error("Slave I/O: out of memory for Load event");
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
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&mi->data_lock);
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
      pthread_mutex_unlock(&mi->data_lock);
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
    DBUG_ASSERT(tmp_buf);
    inc_pos=event_len;
    ev->log_pos+= inc_pos;
    int error = process_io_create_file(mi,(Create_file_log_event*)ev);
    delete ev;
    mi->master_log_pos += inc_pos;
    DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
    pthread_mutex_unlock(&mi->data_lock);
    my_free((char*)tmp_buf, MYF(0));
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
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  }
  delete ev;
  mi->master_log_pos+= inc_pos;
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(0);
}

/*
  Reads a 4.0 event and converts it to the slave's format. This code was copied
  from queue_binlog_ver_1_event(), with some affordable simplifications.
*/
static int queue_binlog_ver_3_event(MASTER_INFO *mi, const char *buf,
			   ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  char *tmp_buf = 0;
  RELAY_LOG_INFO *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_3_event");

  /* read_log_event() will adjust log_pos to be end_log_pos */
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
                                            mi->rli.relay_log.description_event_for_queue);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
		    errmsg);
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&mi->data_lock);
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    goto err;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
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
    pthread_mutex_unlock(&mi->data_lock);
    DBUG_RETURN(1);
  }
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  delete ev;
  mi->master_log_pos+= inc_pos;
err:
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
  pthread_mutex_unlock(&mi->data_lock);
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

static int queue_old_event(MASTER_INFO *mi, const char *buf,
			   ulong event_len)
{
  switch (mi->rli.relay_log.description_event_for_queue->binlog_version)
  {
  case 1:
      return queue_binlog_ver_1_event(mi,buf,event_len);
  case 3:
      return queue_binlog_ver_3_event(mi,buf,event_len);
  default: /* unsupported format; eg version 2 */
    DBUG_PRINT("info",("unsupported binlog format %d in queue_old_event()",
                       mi->rli.relay_log.description_event_for_queue->binlog_version));  
    return 1;
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

int queue_event(MASTER_INFO* mi,const char* buf, ulong event_len)
{
  int error= 0;
  ulong inc_pos;
  RELAY_LOG_INFO *rli= &mi->rli;
  DBUG_ENTER("queue_event");

  if (mi->rli.relay_log.description_event_for_queue->binlog_version<4 &&
      buf[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT /* a way to escape */)
    DBUG_RETURN(queue_old_event(mi,buf,event_len));

  pthread_mutex_lock(&mi->data_lock);

  /*
    TODO: figure out if other events in addition to Rotate
    require special processing.
    Guilhem 2003-06 : I don't think so.
  */
  switch (buf[EVENT_TYPE_OFFSET]) {
  case STOP_EVENT:
    /*
      We needn't write this event to the relay log. Indeed, it just indicates a
      master server shutdown. The only thing this does is cleaning. But
      cleaning is already done on a per-master-thread basis (as the master
      server is shutting down cleanly, it has written all DROP TEMPORARY TABLE
      and DO RELEASE_LOCK; prepared statements' deletion are TODO).
      
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
      error= 1;
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
      We are the only thread which reads/writes description_event_for_queue. The
      relay_log struct does not move (though some members of it can change), so
      we needn't any lock (no rli->data_lock, no log lock).
    */
    Format_description_log_event* tmp;
    const char* errmsg;
    if (!(tmp= (Format_description_log_event*)
          Log_event::read_log_event(buf, event_len, &errmsg,
                                    mi->rli.relay_log.description_event_for_queue)))
    {
      error= 2;
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

  if ((uint4korr(buf + SERVER_ID_OFFSET) == ::server_id) &&
      !replicate_same_server_id)
  {
    /*
      Do not write it to the relay log.
      We still want to increment, so that we won't re-read this event from the
      master if the slave IO thread is now stopped/restarted (more efficient if
      the events we are ignoring are big LOAD DATA INFILE).
      But events which were generated by this slave and which do not exist in
      the master's binlog (i.e. Format_desc, Rotate & Stop) should not increment
      mi->master_log_pos.
    */
    if (buf[EVENT_TYPE_OFFSET]!=FORMAT_DESCRIPTION_EVENT &&
        buf[EVENT_TYPE_OFFSET]!=ROTATE_EVENT &&
        buf[EVENT_TYPE_OFFSET]!=STOP_EVENT)
      mi->master_log_pos+= inc_pos;
    DBUG_PRINT("info", ("master_log_pos: %d, event originating from the same server, ignored", (ulong) mi->master_log_pos));
  }  
  else
  {
    /* write the event to the relay log */
    if (likely(!(rli->relay_log.appendv(buf,event_len,0))))
    {
      mi->master_log_pos+= inc_pos;
      DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
    }
    else
      error=3;
  }

err:
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_PRINT("info", ("error=%d", error));
  DBUG_RETURN(error);
}


void end_relay_log_info(RELAY_LOG_INFO* rli)
{
  DBUG_ENTER("end_relay_log_info");

  if (!rli->inited)
    DBUG_VOID_RETURN;
  if (rli->info_fd >= 0)
  {
    end_io_cache(&rli->info_file);
    (void) my_close(rli->info_fd, MYF(MY_WME));
    rli->info_fd = -1;
  }
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    (void)my_close(rli->cur_log_fd, MYF(MY_WME));
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

/*
  Try to connect until successful or slave killed

  SYNPOSIS
    safe_connect()
    thd			Thread handler for slave
    mysql		MySQL connection handle
    mi			Replication handle

  RETURN
    0	ok
    #	Error
*/

static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  return connect_to_master(thd, mysql, mi, 0, 0);
}


/*
  SYNPOSIS
    connect_to_master()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    master_retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings)
{
  int slave_was_killed;
  int last_errno= -2;				// impossible error
  ulong err_count=0;
  char llbuff[22];
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  events_till_disconnect = disconnect_slave_event_count;
#endif
  ulong client_flag= CLIENT_REMEMBER_OPTIONS;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;		/* We will use compression */

  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);
 
#ifdef HAVE_OPENSSL
  if (mi->ssl)
    mysql_ssl_set(mysql, 
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0, 
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

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
      sql_print_error("Slave I/O thread: error %s to master \
'%s@%s:%d': \
Error: '%s'  errno: %d  retry-time: %d  retries: %d",
		      (reconnect ? "reconnecting" : "connecting"),
		      mi->user,mi->host,mi->port,
		      mysql_error(mysql), last_errno,
		      mi->connect_retry,
		      master_retry_count);
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
    safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
	       (void*)mi);
  }

  if (!slave_was_killed)
  {
    if (reconnect)
    { 
      if (!suppress_warnings && global_system_variables.log_warnings)
	sql_print_error("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->user,
			mi->host, mi->port,
			IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
    }
    else
    {
      change_rpl_status(RPL_IDLE_SLAVE,RPL_ACTIVE_SLAVE);
      mysql_log.write(thd, COM_CONNECT_OUT, "%s@%s:%d",
		      mi->user, mi->host, mi->port);
    }
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif      
  }
  DBUG_PRINT("exit",("slave_was_killed: %d", slave_was_killed));
  DBUG_RETURN(slave_was_killed);
}


/*
  safe_reconnect()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    master_retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings)
{
  DBUG_ENTER("safe_reconnect");
  DBUG_RETURN(connect_to_master(thd, mysql, mi, 1, suppress_warnings));
}


/*
  Store the file and position where the execute-slave thread are in the
  relay log.

  SYNOPSIS
    flush_relay_log_info()
    rli			Relay log information

  NOTES
    - As this is only called by the slave thread, we don't need to
      have a lock on this.
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
    0	ok
    1	write error
*/

bool flush_relay_log_info(RELAY_LOG_INFO* rli)
{
  bool error=0;
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
  if (my_b_write(file, (byte*) buff, (ulong) (pos-buff)+1))
    error=1;
  if (flush_io_cache(file))
    error=1;
  /* Flushing the relay log is done by the slave I/O thread */
  return error;
}


/*
  Called when we notice that the current "hot" log got rotated under our feet.
*/

static IO_CACHE *reopen_relay_log(RELAY_LOG_INFO *rli, const char **errmsg)
{
  DBUG_ASSERT(rli->cur_log != &rli->cache_buf);
  DBUG_ASSERT(rli->cur_log_fd == -1);
  DBUG_ENTER("reopen_relay_log");

  IO_CACHE *cur_log = rli->cur_log=&rli->cache_buf;
  if ((rli->cur_log_fd=open_binlog(cur_log,rli->event_relay_log_name,
				   errmsg)) <0)
    DBUG_RETURN(0);
  /*
    We want to start exactly where we was before:
    relay_log_pos	Current log pos
    pending		Number of bytes already processed from the event
  */
  rli->event_relay_log_pos= max(rli->event_relay_log_pos, BIN_LOG_HEADER_SIZE);
  my_b_seek(cur_log,rli->event_relay_log_pos);
  DBUG_RETURN(cur_log);
}


Log_event* next_event(RELAY_LOG_INFO* rli)
{
  Log_event* ev;
  IO_CACHE* cur_log = rli->cur_log;
  pthread_mutex_t *log_lock = rli->relay_log.get_log_lock(); 
  const char* errmsg=0;
  THD* thd = rli->sql_thd;
  
  DBUG_ENTER("next_event");
  DBUG_ASSERT(thd != 0);

  /*
    For most operations we need to protect rli members with data_lock,
    so we assume calling function acquired this mutex for us and we will
    hold it for the most of the loop below However, we will release it
    whenever it is worth the hassle,  and in the cases when we go into a
    pthread_cond_wait() with the non-data_lock mutex
  */
  safe_mutex_assert_owner(&rli->data_lock);
  
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
      pthread_mutex_lock(log_lock);

      /*
	Reading xxx_file_id is safe because the log will only
	be rotated when we hold relay_log.LOCK_log
      */
      if (rli->relay_log.get_open_count() != rli->cur_log_old_open_count)
      {
	// The master has switched to a new log file; Reopen the old log file
	cur_log=reopen_relay_log(rli, &errmsg);
	pthread_mutex_unlock(log_lock);
	if (!cur_log)				// No more log files
	  goto err;
	hot_log=0;				// Using old binary log
      }
    }

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
      A problem: the description event may be in a previous relay log. So if the
      slave has been shutdown meanwhile, we would have to look in old relay
      logs, which may even have been deleted. So we need to write this
      description event at the beginning of the relay log.
      When the relay log is created when the I/O thread starts, easy: the master
      will send the description event and we will queue it.
      But if the relay log is created by new_file(): then the solution is:
      MYSQL_LOG::open() will write the buffered description event.
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
	pthread_mutex_unlock(log_lock);
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->sql_thd);
    if (opt_reckless_slave)			// For mysql-test
      cur_log->error = 0;
    if (cur_log->error < 0)
    {
      errmsg = "slave SQL thread aborted because of I/O error";
      if (hot_log)
	pthread_mutex_unlock(log_lock);
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
	DBUG_ASSERT(rli->relay_log.get_open_count() == rli->cur_log_old_open_count);
	/*
	  We can, and should release data_lock while we are waiting for
	  update. If we do not, show slave status will block
	*/
	pthread_mutex_unlock(&rli->data_lock);

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
          and reads more events.
          The SQL thread decides when the I/O thread should take log_space_limit
          into account again : ignore_log_space_limit is reset to 0 
          in purge_first_log (when the SQL thread purges the just-read relay
          log), and also when the SQL thread starts. We should also reset
          ignore_log_space_limit to 0 when the user does RESET SLAVE, but in
          fact, no need as RESET SLAVE requires that the slave
          be stopped, and the SQL thread sets ignore_log_space_limit to 0 when
          it stops.
        */
        pthread_mutex_lock(&rli->log_space_lock);
        // prevent the I/O thread from blocking next times
        rli->ignore_log_space_limit= 1; 
        /*
          If the I/O thread is blocked, unblock it.
          Ok to broadcast after unlock, because the mutex is only destroyed in
          ~st_relay_log_info(), i.e. when rli is destroyed, and rli will not be
          destroyed before we exit the present function.
        */
        pthread_mutex_unlock(&rli->log_space_lock);
        pthread_cond_broadcast(&rli->log_space_cond);
        // Note that wait_for_update unlocks lock_log !
        rli->relay_log.wait_for_update(rli->sql_thd, 1);
        // re-acquire data lock since we released it earlier
        pthread_mutex_lock(&rli->data_lock);
	continue;
      }
      /*
	If the log was not hot, we need to move to the next log in
	sequence. The next log could be hot or cold, we deal with both
	cases separately after doing some common initialization
      */
      end_io_cache(cur_log);
      DBUG_ASSERT(rli->cur_log_fd >= 0);
      my_close(rli->cur_log_fd, MYF(MY_WME));
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
        pthread_mutex_lock(log_lock);
      if (rli->relay_log.is_active(rli->linfo.log_file_name))
      {
#ifdef EXTRA_DEBUG
	if (global_system_variables.log_warnings)
	  sql_print_error("next log '%s' is currently active",
			  rli->linfo.log_file_name);
#endif	  
	rli->cur_log= cur_log= rli->relay_log.get_log_file();
	rli->cur_log_old_open_count= rli->relay_log.get_open_count();
	DBUG_ASSERT(rli->cur_log_fd == -1);
	  
	/*
	  Read pointer has to be at the start since we are the only
	  reader.
          We must keep the LOCK_log to read the 4 first bytes, as this is a hot
          log (same as when we call read_log_event() above: for a hot log we
          take the mutex).
	*/
	if (check_binlog_magic(cur_log,&errmsg))
        {
          if (!hot_log) pthread_mutex_unlock(log_lock);
	  goto err;
        }
        if (!hot_log) pthread_mutex_unlock(log_lock);
	continue;
      }
      if (!hot_log) pthread_mutex_unlock(log_lock);
      /*
	if we get here, the log was not hot, so we will have to open it
	ourselves. We are sure that the log is still not hot now (a log can get
	from hot to cold, but not from cold to hot). No need for LOCK_log.
      */
#ifdef EXTRA_DEBUG
      if (global_system_variables.log_warnings)
	sql_print_error("next log '%s' is not active",
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
	pthread_mutex_unlock(log_lock);
      sql_print_error("Slave SQL thread: I/O error reading \
event(errno: %d  cur_log->error: %d)",
		      my_errno,cur_log->error);
      // set read position to the beginning of the event
      my_b_seek(cur_log,rli->event_relay_log_pos);
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;					// To end of function
    }
  }
  if (!errmsg && global_system_variables.log_warnings)
    errmsg = "slave SQL thread was killed";

err:
  if (errmsg)
    sql_print_error("Error reading relay log event: %s", errmsg);
  DBUG_RETURN(0);
}

/*
  Rotate a relay log (this is used only by FLUSH LOGS; the automatic rotation
  because of size is simpler because when we do it we already have all relevant
  locks; here we don't, so this function is mainly taking locks). 
  Returns nothing as we cannot catch any error (MYSQL_LOG::new_file() is void).
*/

void rotate_relay_log(MASTER_INFO* mi)
{
  DBUG_ENTER("rotate_relay_log");
  RELAY_LOG_INFO* rli= &mi->rli;

  /* We don't lock rli->run_lock. This would lead to deadlocks. */
  pthread_mutex_lock(&mi->run_lock);

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
  rli->relay_log.new_file(1);

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
  pthread_mutex_unlock(&mi->run_lock);
  DBUG_VOID_RETURN;
}


#ifdef __GNUC__
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif


#endif /* HAVE_REPLICATION */
