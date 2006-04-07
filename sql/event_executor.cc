/* Copyright (C) 2004-2005 MySQL AB

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

#include "event_priv.h"
#include "event.h"
#include "sp.h"

#define WAIT_STATUS_READY         0
#define WAIT_STATUS_EMPTY_QUEUE   1
#define WAIT_STATUS_NEW_TOP_EVENT 2
#define WAIT_STATUS_STOP_EXECUTOR 3


/* 
  Make this define DBUG_FAULTY_THR to be able to put breakpoints inside
  code used by the scheduler's thread(s). In this case user connections
  are not possible because the scheduler thread code is ran inside the
  main thread (no spawning takes place. If you want to debug client 
  connection then start with --one-thread and make the define
  DBUG_FAULTY_THR !
*/
#define DBUG_FAULTY_THR2

extern  ulong thread_created;
extern const char *my_localhost;
extern pthread_attr_t connection_attrib;

pthread_mutex_t LOCK_event_arrays,              // mutex for when working with the queue
                LOCK_workers_count,             // mutex for when inc/dec uint workers_count
                LOCK_evex_running;              // mutes for managing bool evex_is_running

static pthread_mutex_t LOCK_evex_main_thread;   // mutex for when working with the queue
bool scheduler_main_thread_running= false;

bool evex_is_running= false;

ulonglong evex_main_thread_id= 0;
ulong opt_event_executor;
my_bool event_executor_running_global_var;
static my_bool evex_mutexes_initted= FALSE;
static uint workers_count;

static int
evex_load_events_from_db(THD *thd);

bool
evex_print_warnings(THD *thd, Event_timed *et);

/*
  TODO Andrey: Check for command line option whether to start
               the main thread or not.
*/

pthread_handler_t
event_executor_worker(void *arg);

pthread_handler_t
event_executor_main(void *arg);


/*
   Returns the seconds difference of 2 TIME structs

   SYNOPSIS
     evex_time_diff()
      a - TIME struct 1
      b - TIME struct 2
   
   Returns:
    the seconds difference
*/

static int
evex_time_diff(TIME *a, TIME *b)
{
  return sec_since_epoch_TIME(a) - sec_since_epoch_TIME(b);
}


/*
   Inits the mutexes used by the scheduler module

   SYNOPSIS
     evex_init_mutexes()
          
   NOTES
    The mutexes are :
      LOCK_event_arrays
      LOCK_workers_count
      LOCK_evex_running
*/

static void
evex_init_mutexes()
{
  if (evex_mutexes_initted)
    return;

  evex_mutexes_initted= TRUE;
  pthread_mutex_init(&LOCK_event_arrays, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_workers_count, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_evex_running, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_evex_main_thread, MY_MUTEX_INIT_FAST);

  event_executor_running_global_var= opt_event_executor;
}

extern TABLE_FIELD_W_TYPE mysql_db_table_fields[];
extern time_t mysql_db_table_last_check;

/*
  Opens mysql.db and mysql.user and checks whether
  1. mysql.db has column Event_priv at column 20 (0 based);
  2. mysql.user has column Event_priv at column 29 (0 based);
  
  Synopsis
    evex_check_system_tables()
*/

void
evex_check_system_tables()
{
  THD *thd= current_thd;
  TABLE_LIST tables;
  Open_tables_state backup;

  /* thd is 0x0 during boot of the server. Later it's !=0x0 */
  if (!thd)
    return;

  thd->reset_n_backup_open_tables_state(&backup);
  
  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "db";
  tables.lock_type= TL_READ;

  if (simple_open_n_lock_tables(thd, &tables))
    sql_print_error("Cannot open mysql.db");
  else
  {
    table_check_intact(tables.table, MYSQL_DB_FIELD_COUNT, mysql_db_table_fields,
                       &mysql_db_table_last_check,ER_CANNOT_LOAD_FROM_TABLE);
    close_thread_tables(thd);
  }

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "user";
  tables.lock_type= TL_READ;

  if (simple_open_n_lock_tables(thd, &tables))
    sql_print_error("Cannot open mysql.db");
  else
  {
    if (tables.table->s->fields < 29 ||
      strncmp(tables.table->field[29]->field_name,
                STRING_WITH_LEN("Event_priv")))
      sql_print_error("mysql.user has no `Event_priv` column at position 29");

    close_thread_tables(thd);
  }

  thd->restore_backup_open_tables_state(&backup);
}


/*
   Inits the scheduler. Called on server start and every time the scheduler
   is started with switching the event_scheduler global variable to TRUE 

   SYNOPSIS
     init_events()

   NOTES
    Inits the mutexes used by the scheduler. Done at server start.
*/

int
init_events()
{
  pthread_t th;
  DBUG_ENTER("init_events");

  DBUG_PRINT("info",("Starting events main thread"));

  evex_check_system_tables();

  evex_init_mutexes();

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  
  if (event_executor_running_global_var)
  {
#ifndef DBUG_FAULTY_THR
    /* TODO Andrey: Change the error code returned! */
    if (pthread_create(&th, &connection_attrib, event_executor_main,(void*)NULL))
      DBUG_RETURN(ER_SLAVE_THREAD);
#else
    event_executor_main(NULL);
#endif
  }

  DBUG_RETURN(0);
}


/*
   Cleans up scheduler memory. Called on server shutdown.

   SYNOPSIS
     shutdown_events()

   NOTES
    Destroys the mutexes.
*/

void
shutdown_events()
{
  DBUG_ENTER("shutdown_events");

  if (evex_mutexes_initted)
  {
    evex_mutexes_initted= FALSE;
    VOID(pthread_mutex_lock(&LOCK_evex_running));
    VOID(pthread_mutex_unlock(&LOCK_evex_running));

    pthread_mutex_destroy(&LOCK_event_arrays);
    pthread_mutex_destroy(&LOCK_workers_count);
    pthread_mutex_destroy(&LOCK_evex_running);
    pthread_mutex_destroy(&LOCK_evex_main_thread);
  }
  DBUG_VOID_RETURN;
}


/*
   Inits an scheduler thread handler, both the main and a worker

   SYNOPSIS
     init_event_thread()
       thd - the THD of the thread. Has to be allocated by the caller.

   NOTES
      1. The host of the thead is my_localhost
      2. thd->net is initted with NULL - no communication.

  Returns
     0  - OK
    -1  - Error
*/

static int
init_event_thread(THD* thd)
{
  DBUG_ENTER("init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->master_access= 0;
  thd->security_ctx->db_access= 0;
  thd->security_ctx->host_or_ip= (char*)my_localhost;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->slave_thread= 0;
  thd->options|= OPTION_AUTO_IS_NULL;
  thd->client_capabilities= CLIENT_LOCAL_FILES;
  thd->real_id=pthread_self();
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->thread_id= thread_id++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

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

  thd->proc_info= "Initialized";
  thd->version= refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}


/*
  This function waits till the time next event in the queue should be 
  executed.

  Returns
    WAIT_STATUS_READY          There is an event to be executed right now
    WAIT_STATUS_EMPTY_QUEUE    No events or the last event was dropped.
    WAIT_STATUS_NEW_TOP_EVENT  New event has entered the queue and scheduled
                               on top. Restart ticking.
    WAIT_STATUS_STOP_EXECUTOR  The thread was killed or SET global event_scheduler=0;
*/

static int
executor_wait_till_next_event_exec(THD *thd)
{
  Event_timed *et;
  TIME time_now;
  int t2sleep;

  DBUG_ENTER("executor_wait_till_next_event_exec");
  /*
    now let's see how much time to sleep, we know there is at least 1
    element in the queue.
  */
  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  if (!evex_queue_num_elements(EVEX_EQ_NAME))
  {
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));
    DBUG_RETURN(WAIT_STATUS_EMPTY_QUEUE);
  }
  et= evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*);
  DBUG_ASSERT(et);
  if (et->status == MYSQL_EVENT_DISABLED)
  {
    DBUG_PRINT("evex main thread",("Now it is disabled-exec no more"));
    if (et->dropped)
      et->drop(thd);
    delete et;
    evex_queue_delete_element(&EVEX_EQ_NAME, 0);// 0 is top, internally 1
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));
    sql_print_information("Event found disabled, dropping.");
    DBUG_RETURN(1);
  }

  DBUG_PRINT("evex main thread",("computing time to sleep till next exec"));
  /* set the internal clock of thd */
  thd->end_time();
  my_tz_UTC->gmt_sec_to_TIME(&time_now, thd->query_start());
  t2sleep= evex_time_diff(&et->execute_at, &time_now);
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));

  t2sleep*=20;
  DBUG_PRINT("evex main thread",("unlocked LOCK_event_arrays"));
  if (t2sleep > 0)
  {
    ulonglong modified= et->modified;
    /*
      We sleep t2sleep seconds but we check every second whether this thread
      has been killed, or there is a new candidate
    */
    while (t2sleep-- && !thd->killed && event_executor_running_global_var &&
           evex_queue_num_elements(EVEX_EQ_NAME) &&
           (evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*) == et &&
            evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*)->modified ==
             modified))
    {
      DBUG_PRINT("evex main thread",("will sleep a bit more."));
      my_sleep(50000);
    }
    DBUG_PRINT("info",("saved_modified=%llu current=%llu", modified,
               evex_queue_num_elements(EVEX_EQ_NAME)? 
               evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*)->modified:
               (ulonglong)~0));
  }

  int ret= WAIT_STATUS_READY;
  if (!evex_queue_num_elements(EVEX_EQ_NAME))
    ret= WAIT_STATUS_EMPTY_QUEUE;
  else if (evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*) != et)
    ret= WAIT_STATUS_NEW_TOP_EVENT;
  if (thd->killed && event_executor_running_global_var)
    ret= WAIT_STATUS_STOP_EXECUTOR;

  DBUG_RETURN(ret);
}


/*
   The main scheduler thread. Inits the priority queue on start and
   destroys it on thread shutdown. Forks child threads for every event
   execution. Sleeps between thread forking and does not do a busy wait.

   SYNOPSIS
     event_executor_main() 
       arg  unused

   NOTES
      1. The host of the thead is my_localhost
      2. thd->net is initted with NULL - no communication.
  
*/

pthread_handler_t
event_executor_main(void *arg)
{
  THD *thd;			/* needs to be first for thread_stack */
  uint i=0, j=0;
  my_ulonglong cnt= 0;
  
  DBUG_ENTER("event_executor_main");
  DBUG_PRINT("event_executor_main", ("EVEX thread started"));
  
  pthread_mutex_lock(&LOCK_evex_main_thread);
  if (!scheduler_main_thread_running)
    scheduler_main_thread_running= true;
  else 
  {
    DBUG_PRINT("event_executor_main", ("already running. thd_id=%d",
                                      evex_main_thread_id));
    pthread_mutex_unlock(&LOCK_evex_main_thread);
    my_thread_end();
    pthread_exit(0);
    DBUG_RETURN(0);                               // Can't return anything here    
  }
  pthread_mutex_unlock(&LOCK_evex_main_thread);

  /* init memory root */
  init_alloc_root(&evex_mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  /* needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff*/
  my_thread_init();

  if (sizeof(my_time_t) != sizeof(time_t))
  {
    sql_print_error("SCHEDULER: sizeof(my_time_t) != sizeof(time_t) ."
                    "The scheduler will not work correctly. Stopping.");
    DBUG_ASSERT(0);
    goto err_no_thd;
  }
  
  /* note that contructor of THD uses DBUG_ ! */
  if (!(thd = new THD))                         
  {
    sql_print_error("SCHEDULER: Cannot create THD for the main thread.");
    goto err_no_thd;
  }
  thd->thread_stack = (char*)&thd;              // remember where our stack is

  pthread_detach_this_thread();

  if (init_event_thread(thd))
    goto finish;

  /*
    make this thread visible it has no vio -> show processlist won't see it
    unless it's marked as system thread
  */
  thd->system_thread= 1;

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  DBUG_PRINT("EVEX main thread", ("Initing events_queue"));

  /*
    eventually manifest that we are running, not to crashe because of
    usage of non-initialized memory structures.
  */
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  evex_queue_init(&EVEX_EQ_NAME);
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));  
  evex_is_running= true;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  thd->security_ctx->user= my_strdup("event_scheduler", MYF(0));

  if (evex_load_events_from_db(thd))
    goto finish;

  evex_main_thread_id= thd->thread_id;

  sql_print_information("SCHEDULER: Main thread started");
  while (!thd->killed)
  {
    TIME time_now;
    Event_timed *et;

    cnt++;
    DBUG_PRINT("info", ("EVEX External Loop %d thd->k", cnt));

    thd->proc_info = "Sleeping";
    if (!event_executor_running_global_var)
    {
      sql_print_information("SCHEDULER: Asked to stop.");
      break;
    }

    if (!evex_queue_num_elements(EVEX_EQ_NAME))
    {
      my_sleep(100000);// sleep 0.1s
      continue;
    }

restart_ticking:
    switch (executor_wait_till_next_event_exec(thd)) {
    case WAIT_STATUS_READY:                     // time to execute the event on top
      DBUG_PRINT("evex main thread",("time to execute an event"));
      break;
    case WAIT_STATUS_EMPTY_QUEUE:               // no more events
      DBUG_PRINT("evex main thread",("no more events"));
      continue;
      break;
    case WAIT_STATUS_NEW_TOP_EVENT:             // new event on top in the queue
      DBUG_PRINT("evex main thread",("restart ticking"));
      goto restart_ticking;
    case WAIT_STATUS_STOP_EXECUTOR:
      sql_print_information("SCHEDULER: Asked to stop.");
      goto finish;
      break;
    default:
      DBUG_ASSERT(0);
    }


    VOID(pthread_mutex_lock(&LOCK_event_arrays));
    thd->end_time();
    my_tz_UTC->gmt_sec_to_TIME(&time_now, thd->query_start());

    if (!evex_queue_num_elements(EVEX_EQ_NAME))
    {
      VOID(pthread_mutex_unlock(&LOCK_event_arrays));
      DBUG_PRINT("evex main thread",("empty queue"));
      continue;
    }
    et= evex_queue_first_element(&EVEX_EQ_NAME, Event_timed*);
    DBUG_PRINT("evex main thread",("got event from the queue"));
      
    if (!et->execute_at_null && my_time_compare(&time_now,&et->execute_at) == -1)
    {
      DBUG_PRINT("evex main thread",("still not the time for execution"));
      VOID(pthread_mutex_unlock(&LOCK_event_arrays));
      continue;
    } 
      
    DBUG_PRINT("evex main thread",("it's right time"));
    if (et->status == MYSQL_EVENT_ENABLED)
    {
      int fork_ret_code;

      DBUG_PRINT("evex main thread", ("[%10s] this exec at [%llu]", et->name.str,
                               TIME_to_ulonglong_datetime(&et->execute_at)));
      et->mark_last_executed(thd);
      if (et->compute_next_execution_time())
      {
        sql_print_error("SCHEDULER: Error while computing time of %s.%s . "
                        "Disabling after execution.",
                        et->dbname.str, et->name.str);
        et->status= MYSQL_EVENT_DISABLED;
      }
      DBUG_PRINT("evex main thread", ("[%10s] next exec at [%llu]", et->name.str,
                               TIME_to_ulonglong_datetime(&et->execute_at)));

      et->update_fields(thd);
#ifndef DBUG_FAULTY_THR
      thread_safe_increment(workers_count, &LOCK_workers_count);
      switch ((fork_ret_code= et->spawn_now(event_executor_worker))) {
      case EVENT_EXEC_CANT_FORK:
        thread_safe_decrement(workers_count, &LOCK_workers_count);
        sql_print_error("SCHEDULER: Problem while trying to create a thread");
        UNLOCK_MUTEX_AND_BAIL_OUT(LOCK_event_arrays, finish);
      case EVENT_EXEC_ALREADY_EXEC:
        thread_safe_decrement(workers_count, &LOCK_workers_count);
        sql_print_information("SCHEDULER: %s.%s in execution. Skip this time.",
                              et->dbname.str, et->name.str);
        break;
      default:
        DBUG_ASSERT(!fork_ret_code);
        if (fork_ret_code)
          thread_safe_decrement(workers_count, &LOCK_workers_count);
        break;
      }
#else
      event_executor_worker((void *) et);
#endif
      /*
        1. For one-time event : year is > 0 and expression is 0
        2. For recurring, expression is != -=> check execute_at_null in this case
      */
      if ((et->execute_at.year && !et->expression) || et->execute_at_null)
         et->flags |= EVENT_EXEC_NO_MORE;

      if ((et->flags & EVENT_EXEC_NO_MORE) || et->status == MYSQL_EVENT_DISABLED)
        evex_queue_delete_element(&EVEX_EQ_NAME, 0);// 0 is top, internally 1
      else
        evex_queue_first_updated(&EVEX_EQ_NAME);
    }
    DBUG_PRINT("evex main thread",("unlocking"));
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  }/* while */
finish:

  /* First manifest that this thread does not work and then destroy */
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;
  evex_main_thread_id= 0;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));


  /*
    TODO: A better will be with a conditional variable
  */
  /* 
    Read workers_count without lock, no need for locking.
    In the worst case we have to wait 1sec more.
  */
  sql_print_information("SCHEDULER: Stopping. Waiting for worker threads to finish.");
  while (1)
  {
    VOID(pthread_mutex_lock(&LOCK_workers_count));
    if (!workers_count)
    {
      VOID(pthread_mutex_unlock(&LOCK_workers_count));
      break;
    }
    VOID(pthread_mutex_unlock(&LOCK_workers_count));
    my_sleep(1000000);// 1s
  }

  /*
    First we free all objects ...
    Lock because a DROP DATABASE could be running in parallel and it locks on these
  */
  sql_print_information("SCHEDULER: Emptying the queue.");
  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  for (i= 0; i < evex_queue_num_elements(EVEX_EQ_NAME); ++i)
  {
    Event_timed *et= evex_queue_element(&EVEX_EQ_NAME, i, Event_timed*);
    et->free_sp();
    delete et;
  }
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  /* ... then we can thrash the whole queue at once */
  evex_queue_destroy(&EVEX_EQ_NAME);

  thd->proc_info = "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  THD_CHECK_SENTRY(thd);

  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  thread_running--;
#ifndef DBUG_FAULTY_THR
  THD_CHECK_SENTRY(thd);
  delete thd;
#endif
  pthread_mutex_unlock(&LOCK_thread_count);


err_no_thd:
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  event_executor_running_global_var= false;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  free_root(&evex_mem_root, MYF(0));
  sql_print_information("SCHEDULER: Stopped.");

#ifndef DBUG_FAULTY_THR
  pthread_mutex_lock(&LOCK_evex_main_thread);
  scheduler_main_thread_running= false;
  pthread_mutex_unlock(&LOCK_evex_main_thread);

  my_thread_end();
  pthread_exit(0);
#endif
  DBUG_RETURN(0);                               // Can't return anything here
}


/*
   Function that executes an event in a child thread. Setups the 
   environment for the event execution and cleans after that.

   SYNOPSIS
     event_executor_worker()
       arg  The Event_timed object to be processed
*/

pthread_handler_t
event_executor_worker(void *event_void)
{
  THD *thd; /* needs to be first for thread_stack */
  Event_timed *event = (Event_timed *) event_void;
  MEM_ROOT worker_mem_root;

  DBUG_ENTER("event_executor_worker");

  init_alloc_root(&worker_mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

#ifndef DBUG_FAULTY_THR
  my_thread_init();

  if (!(thd = new THD)) /* note that contructor of THD uses DBUG_ ! */
  {
    sql_print_error("SCHEDULER: Cannot create a THD structure in an worker.");
    goto err_no_thd;
  }
  thd->thread_stack = (char*)&thd;              // remember where our stack is
  thd->mem_root= &worker_mem_root;

  pthread_detach_this_thread();
  
  if (init_event_thread(thd))
    goto err;

  thd->init_for_queries();

  /* make this thread visible it has no vio -> show processlist needs this flag */
  thd->system_thread= 1;

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
#else
  thd= current_thd;
#endif

  thd->enable_slow_log= TRUE;
  {
    int ret;
    sql_print_information("SCHEDULER: Executing event %s.%s of %s [EXPR:%d]",
                          event->dbname.str, event->name.str,
                          event->definer.str, (int) event->expression);

    ret= event->execute(thd, &worker_mem_root);

    evex_print_warnings(thd, event);
    sql_print_information("SCHEDULER: Executed event %s.%s of %s  [EXPR:%d]. "
                          "RetCode=%d",  event->dbname.str, event->name.str,
                          event->definer.str, (int) event->expression, ret);
    if (ret == EVEX_COMPILE_ERROR)
      sql_print_information("SCHEDULER: COMPILE ERROR for event %s.%s of",
                            event->dbname.str, event->name.str,
                            event->definer.str);
    else if (ret == EVEX_MICROSECOND_UNSUP)
      sql_print_information("SCHEDULER: MICROSECOND is not supported");
  }
  event->spawn_thread_finish(thd);


err:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
#ifndef DBUG_FAULTY_THR
  thread_count--;
  thread_running--;
  /*
    Some extra safety, which should not been needed (normally, event deletion
    should already have done these assignments (each event which sets these
    variables is supposed to set them to 0 before terminating)).
  */
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thd->proc_info = "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  THD_CHECK_SENTRY(thd);

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  THD_CHECK_SENTRY(thd);
  delete thd;
#endif
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

err_no_thd:

  free_root(&worker_mem_root, MYF(0));
  thread_safe_decrement(workers_count, &LOCK_workers_count);

#ifndef DBUG_FAULTY_THR
  my_thread_end();
  pthread_exit(0);
#endif
  DBUG_RETURN(0); // Can't return anything here
}


/*
   Loads all ENABLED events from mysql.event into the prioritized
   queue. Called during scheduler main thread initialization. Compiles
   the events. Creates Event_timed instances for every ENABLED event
   from mysql.event.

   SYNOPSIS
     evex_load_events_from_db()
       thd - Thread context. Used for memory allocation in some cases.
     
   RETURNS
     0  OK
    !0  Error

   NOTES
     Reports the error to the console
*/

static int
evex_load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  int ret= -1;
  uint count= 0;

  DBUG_ENTER("evex_load_events_from_db");  

  if ((ret= evex_open_event_table(thd, TL_READ, &table)))
  {
    sql_print_error("SCHEDULER: Table mysql.event is damaged. Can not open.");
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  }

  VOID(pthread_mutex_lock(&LOCK_event_arrays));

  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_timed *et;
    if (!(et= new Event_timed))
    {
      DBUG_PRINT("evex_load_events_from_db", ("Out of memory"));
      ret= -1;
      goto end;
    }
    DBUG_PRINT("evex_load_events_from_db", ("Loading event from row."));
    
    if ((ret= et->load_from_row(&evex_mem_root, table)))
    {
      sql_print_error("SCHEDULER: Error while loading from mysql.event. "
                      "Table probably corrupted");
      goto end;
    }
    if (et->status != MYSQL_EVENT_ENABLED)
    {
      DBUG_PRINT("evex_load_events_from_db",("%s is disabled",et->name.str));
      delete et;
      continue;
    }

    DBUG_PRINT("evex_load_events_from_db",
            ("Event %s loaded from row. Time to compile", et->name.str));
    
    switch (ret= et->compile(thd, &evex_mem_root)) {
    case EVEX_MICROSECOND_UNSUP:
      sql_print_error("SCHEDULER: mysql.event is tampered. MICROSECOND is not "
                      "supported but found in mysql.event");
      goto end;
    case EVEX_COMPILE_ERROR:
      sql_print_error("SCHEDULER: Error while compiling %s.%s. Aborting load.",
                      et->dbname.str, et->name.str);
      goto end;
    default:
      break;
    }

    /* let's find when to be executed */
    if (et->compute_next_execution_time())
    {
      sql_print_error("SCHEDULER: Error while computing execution time of %s.%s."
                      " Skipping", et->dbname.str, et->name.str);
      continue;
    }
    
    DBUG_PRINT("evex_load_events_from_db", ("Adding to the exec list."));

    evex_queue_insert(&EVEX_EQ_NAME, (EVEX_PTOQEL) et);
    DBUG_PRINT("evex_load_events_from_db", ("%p %*s",
                et, et->name.length,et->name.str));
    count++;
  }

  ret= 0;

end:
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  end_read_record(&read_record_info);
  
  /* Force close to free memory */
  thd->version--;  

  close_thread_tables(thd);
  if (!ret)
    sql_print_information("SCHEDULER: Loaded %d event%s", count, (count == 1)?"":"s");
  DBUG_PRINT("info", ("Status code %d. Loaded %d event(s)", ret, count));

  DBUG_RETURN(ret);
}


/*
   The update method of the global variable event_scheduler.
   If event_scheduler is switched from 0 to 1 then the scheduler main
   thread is started.

   SYNOPSIS
     event_executor_worker()
       thd - Thread context (unused)
       car - the new value

   Returns
     0  OK (always)
*/

bool
sys_var_event_executor::update(THD *thd, set_var *var)
{
  /* here start the thread if not running. */
  DBUG_ENTER("sys_var_event_executor::update");
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  *value= var->save_result.ulong_value;

  DBUG_PRINT("new_value", ("%d", *value));
  if ((my_bool) *value && !evex_is_running)
  {
    VOID(pthread_mutex_unlock(&LOCK_evex_running));
    init_events();
  } else 
    VOID(pthread_mutex_unlock(&LOCK_evex_running));

  DBUG_RETURN(0);
}


extern LEX_STRING warning_level_names[];

typedef void (*sql_print_xxx_func)(const char *format, ...);
static sql_print_xxx_func sql_print_xxx_handlers[3] =
{
  sql_print_information,
  sql_print_warning,
  sql_print_error
};


/*
  Prints the stack of infos, warnings, errors from thd to
  the console so it can be fetched by the logs-into-tables and
  checked later.

  Synopsis
    evex_print_warnings
      thd    - thread used during the execution of the event
      et     - the event itself

  Returns
    0 - OK (always)   

*/

bool
evex_print_warnings(THD *thd, Event_timed *et)
{
  MYSQL_ERROR *err;
  DBUG_ENTER("evex_show_warnings");
  char msg_buf[1024];
  char prefix_buf[512];
  String prefix(prefix_buf, sizeof(prefix_buf), system_charset_info);
  prefix.length(0);

  List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
  while ((err= it++))
  {
    String err_msg(msg_buf, sizeof(msg_buf), system_charset_info);
    /* set it to 0 or we start adding at the end. That's the trick ;) */
    err_msg.length(0);
    if (!prefix.length())
    {
      prefix.append("SCHEDULER: [");

      append_identifier(thd,&prefix,et->definer_user.str,et->definer_user.length);
      prefix.append('@');
      append_identifier(thd,&prefix,et->definer_host.str,et->definer_host.length);
      prefix.append("][", 2);
      append_identifier(thd,&prefix, et->dbname.str, et->dbname.length);
      prefix.append('.');
      append_identifier(thd,&prefix, et->name.str, et->name.length);
      prefix.append("] ", 2);
    }

    err_msg.append(prefix);
    err_msg.append(err->msg, strlen(err->msg), system_charset_info);
    err_msg.append("]");
    DBUG_ASSERT(err->level < 3);
    (sql_print_xxx_handlers[err->level])("%*s", err_msg.length(), err_msg.c_ptr());
  }


  DBUG_RETURN(FALSE);
}
