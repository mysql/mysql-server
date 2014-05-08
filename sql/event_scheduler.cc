/* Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "unireg.h"
#include "event_scheduler.h"
#include "events.h"
#include "event_data_objects.h"
#include "event_queue.h"
#include "event_db_repository.h"
#include "sql_connect.h"         // init_new_connection_handler_thread
#include "sql_acl.h"             // SUPER_ACL
#include "global_threads.h"

/**
  @addtogroup Event_Scheduler
  @{
*/

#ifdef __GNUC__
#if __GNUC__ >= 2
#define SCHED_FUNC __FUNCTION__
#endif
#else
#define SCHED_FUNC "<unknown>"
#endif

#define LOCK_DATA()       lock_data(SCHED_FUNC, __LINE__)
#define UNLOCK_DATA()     unlock_data(SCHED_FUNC, __LINE__)
#define COND_STATE_WAIT(mythd, abstime, stage) \
        cond_wait(mythd, abstime, stage, SCHED_FUNC, __FILE__, __LINE__)

extern pthread_attr_t connection_attrib;


Event_db_repository *Event_worker_thread::db_repository;


static
const LEX_STRING scheduler_states_names[] =
{
  { C_STRING_WITH_LEN("INITIALIZED") },
  { C_STRING_WITH_LEN("RUNNING") },
  { C_STRING_WITH_LEN("STOPPING") }
};

struct scheduler_param {
  THD *thd;
  Event_scheduler *scheduler;
};


/*
  Prints the stack of infos, warnings, errors from thd to
  the console so it can be fetched by the logs-into-tables and
  checked later.

  SYNOPSIS
    evex_print_warnings
      thd  Thread used during the execution of the event
      et   The event itself
*/

void
Event_worker_thread::print_warnings(THD *thd, Event_job_data *et)
{
  const Sql_condition *err;
  DBUG_ENTER("evex_print_warnings");
  if (thd->get_stmt_da()->is_warning_info_empty())
    DBUG_VOID_RETURN;

  char msg_buf[10 * STRING_BUFFER_USUAL_SIZE];
  char prefix_buf[5 * STRING_BUFFER_USUAL_SIZE];
  String prefix(prefix_buf, sizeof(prefix_buf), system_charset_info);
  prefix.length(0);
  prefix.append("Event Scheduler: [");

  prefix.append(et->definer.str, et->definer.length, system_charset_info);
  prefix.append("][", 2);
  prefix.append(et->dbname.str, et->dbname.length, system_charset_info);
  prefix.append('.');
  prefix.append(et->name.str, et->name.length, system_charset_info);
  prefix.append("] ", 2);

  Diagnostics_area::Sql_condition_iterator it=
    thd->get_stmt_da()->sql_conditions();
  while ((err= it++))
  {
    String err_msg(msg_buf, sizeof(msg_buf), system_charset_info);
    /* set it to 0 or we start adding at the end. That's the trick ;) */
    err_msg.length(0);
    err_msg.append(prefix);
    err_msg.append(err->get_message_text(),
                   err->get_message_octet_length(), system_charset_info);
    DBUG_ASSERT(err->get_level() < 3);
    (sql_print_message_handlers[err->get_level()])("%*s", err_msg.length(),
                                                   err_msg.c_ptr());
  }
  DBUG_VOID_RETURN;
}


/*
  Performs post initialization of structures in a new thread.

  SYNOPSIS
    post_init_event_thread()
      thd  Thread

  NOTES
      Before this is called, one should not do any DBUG_XXX() calls.

*/

bool
post_init_event_thread(THD *thd)
{
  (void) init_new_connection_handler_thread();
  if (init_thr_lock() || thd->store_globals())
  {
    return TRUE;
  }

  inc_thread_running();
  mysql_mutex_lock(&LOCK_thread_count);
  add_global_thread(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  return FALSE;
}


/*
  Cleans up the THD and the threaded environment of the thread.

  SYNOPSIS
    deinit_event_thread()
      thd  Thread
*/

void
deinit_event_thread(THD *thd)
{
  thd->proc_info= "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net);
  DBUG_PRINT("exit", ("Event thread finishing"));

  dec_thread_running();
  thd->release_resources();
  remove_global_thread(thd);
  delete thd;
}


/*
  Performs pre- mysql_thread_create() initialisation of THD. Do this
  in the thread that will pass THD to the child thread. In the
  child thread call post_init_event_thread().

  SYNOPSIS
    pre_init_event_thread()
      thd  The THD of the thread. Has to be allocated by the caller.

  NOTES
    1. The host of the thead is my_localhost
    2. thd->net is initted with NULL - no communication.
*/

void
pre_init_event_thread(THD* thd)
{
  DBUG_ENTER("pre_init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->master_access= 0;
  thd->security_ctx->db_access= 0;
  thd->security_ctx->host_or_ip= (char*)my_localhost;
  my_net_init(&thd->net, NULL);
  thd->security_ctx->set_user((char*)"event_scheduler");
  thd->net.read_timeout= slave_net_timeout;
  thd->slave_thread= 0;
  thd->variables.option_bits|= OPTION_AUTO_IS_NULL;
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;
  mysql_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  mysql_mutex_unlock(&LOCK_thread_count);

  /*
    Guarantees that we will see the thread in SHOW PROCESSLIST though its
    vio is NULL.
  */

  thd->proc_info= "Initialized";
  thd->set_time();

  /* Do not use user-supplied timeout value for system threads. */
  thd->variables.lock_wait_timeout= LONG_TIMEOUT;

  DBUG_VOID_RETURN;
}


/*
  Function that executes the scheduler,

  SYNOPSIS
    event_scheduler_thread()
      arg  Pointer to `struct scheduler_param`

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_scheduler_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd= (THD *) ((struct scheduler_param *) arg)->thd;
  Event_scheduler *scheduler= ((struct scheduler_param *) arg)->scheduler;
  bool res;

  thd->thread_stack= (char *)&thd;              // remember where our stack is

  mysql_thread_set_psi_id(thd->thread_id);

  res= post_init_event_thread(thd);

  DBUG_ENTER("event_scheduler_thread");
  my_free(arg);
  if (!res)
    scheduler->run(thd);
  else
  {
    thd->proc_info= "Clearing";
    net_end(&thd->net);
    delete thd;
  }

  DBUG_LEAVE;                               // Against gcc warnings
  my_thread_end();
  return 0;
}


/**
  Function that executes an event in a child thread. Setups the
  environment for the event execution and cleans after that.

  SYNOPSIS
    event_worker_thread()
      arg  The Event_job_data object to be processed

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_worker_thread(void *arg)
{
  THD *thd;
  Event_queue_element_for_exec *event= (Event_queue_element_for_exec *)arg;

  thd= event->thd;

  mysql_thread_set_psi_id(thd->thread_id);

  Event_worker_thread worker_thread;
  worker_thread.run(thd, event);

  my_thread_end();
  return 0;                                     // Can't return anything here
}


/**
  Function that executes an event in a child thread. Setups the
  environment for the event execution and cleans after that.

  SYNOPSIS
    Event_worker_thread::run()
      thd    Thread context
      event  The Event_queue_element_for_exec object to be processed
*/

void
Event_worker_thread::run(THD *thd, Event_queue_element_for_exec *event)
{
  /* needs to be first for thread_stack */
  char my_stack;
  Event_job_data job_data;
  bool res;

  thd->thread_stack= &my_stack;                // remember where our stack is
  res= post_init_event_thread(thd);

  DBUG_ENTER("Event_worker_thread::run");
  DBUG_PRINT("info", ("Time is %ld, THD: 0x%lx", (long) my_time(0), (long) thd));

  if (res)
    goto end;

  if ((res= db_repository->load_named_event(thd, event->dbname, event->name,
                                            &job_data)))
  {
    DBUG_PRINT("error", ("Got error from load_named_event"));
    goto end;
  }

  thd->enable_slow_log= TRUE;

  res= job_data.execute(thd, event->dropped);

  print_warnings(thd, &job_data);

  if (res)
    sql_print_information("Event Scheduler: "
                          "[%s].[%s.%s] event execution failed.",
                          job_data.definer.str,
                          job_data.dbname.str, job_data.name.str);
end:
  DBUG_PRINT("info", ("Done with Event %s.%s", event->dbname.str,
             event->name.str));

  delete event;
  deinit_event_thread(thd);

  DBUG_VOID_RETURN;
}


Event_scheduler::Event_scheduler(Event_queue *queue_arg)
  :state(INITIALIZED),
  scheduler_thd(NULL),
  queue(queue_arg),
  mutex_last_locked_at_line(0),
  mutex_last_unlocked_at_line(0),
  mutex_last_locked_in_func("n/a"),
  mutex_last_unlocked_in_func("n/a"),
  mutex_scheduler_data_locked(FALSE),
  waiting_on_cond(FALSE),
  started_events(0)
{
  mysql_mutex_init(key_event_scheduler_LOCK_scheduler_state,
                   &LOCK_scheduler_state, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_event_scheduler_COND_state, &COND_state, NULL);
}


Event_scheduler::~Event_scheduler()
{
  stop();                                    /* does nothing if not running */
  mysql_mutex_destroy(&LOCK_scheduler_state);
  mysql_cond_destroy(&COND_state);
}


/**
  Starts the scheduler (again). Creates a new THD and passes it to
  a forked thread. Does not wait for acknowledgement from the new
  thread that it has started. Asynchronous starting. Most of the
  needed initializations are done in the current thread to minimize
  the chance of failure in the spawned thread.

  @param[out] err_no - errno indicating type of error which caused
                       failure to start scheduler thread.

  @return
    @retval false Success.
    @retval true  Error.
*/

bool
Event_scheduler::start(int *err_no)
{
  THD *new_thd= NULL;
  bool ret= false;
  pthread_t th;
  struct scheduler_param *scheduler_param_value;
  DBUG_ENTER("Event_scheduler::start");

  LOCK_DATA();
  DBUG_PRINT("info", ("state before action %s", scheduler_states_names[state].str));
  if (state > INITIALIZED)
    goto end;

  DBUG_EXECUTE_IF("event_scheduler_thread_create_failure", {
                  *err_no= 11;
                  Events::opt_event_scheduler= Events::EVENTS_OFF;
                  ret= true;
                  goto end; });

  if (!(new_thd= new THD))
  {
    sql_print_error("Event Scheduler: Cannot initialize the scheduler thread");
    ret= true;
    goto end;
  }
  pre_init_event_thread(new_thd);
  new_thd->system_thread= SYSTEM_THREAD_EVENT_SCHEDULER;
  new_thd->set_command(COM_DAEMON);

  /*
    We should run the event scheduler thread under the super-user privileges.
    In particular, this is needed to be able to lock the mysql.event table
    for writing when the server is running in the read-only mode.

    Same goes for transaction access mode. Set it to read-write for this thd.
  */
  new_thd->security_ctx->master_access |= SUPER_ACL;
  new_thd->variables.tx_read_only= false;
  new_thd->tx_read_only= false;

  scheduler_param_value=
    (struct scheduler_param *)my_malloc(sizeof(struct scheduler_param), MYF(0));
  scheduler_param_value->thd= new_thd;
  scheduler_param_value->scheduler= this;

  scheduler_thd= new_thd;
  DBUG_PRINT("info", ("Setting state go RUNNING"));
  state= RUNNING;
  DBUG_PRINT("info", ("Forking new thread for scheduler. THD: 0x%lx", (long) new_thd));
  if ((*err_no= mysql_thread_create(key_thread_event_scheduler,
                                    &th, &connection_attrib,
                                    event_scheduler_thread,
                                    (void*)scheduler_param_value)))
  {
    DBUG_PRINT("error", ("cannot create a new thread"));
    sql_print_error("Event scheduler: Failed to start scheduler,"
                    " Can not create thread for event scheduler (errno=%d)",
                    *err_no);

    new_thd->proc_info= "Clearing";
    DBUG_ASSERT(new_thd->net.buff != 0);
    net_end(&new_thd->net);

    state= INITIALIZED;
    scheduler_thd= NULL;
    delete new_thd;

    delete scheduler_param_value;
    ret= true;
  }

end:
  UNLOCK_DATA();
  DBUG_RETURN(ret);
}


/*
  The main loop of the scheduler.

  SYNOPSIS
    Event_scheduler::run()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error (Serious error)
*/

bool
Event_scheduler::run(THD *thd)
{
  int res= FALSE;
  DBUG_ENTER("Event_scheduler::run");

  sql_print_information("Event Scheduler: scheduler thread started with id %lu",
                        thd->thread_id);
  /*
    Recalculate the values in the queue because there could have been stops
    in executions of the scheduler and some times could have passed by.
  */
  queue->recalculate_activation_times(thd);

  while (is_running())
  {
    Event_queue_element_for_exec *event_name;

    /* Gets a minimized version */
    if (queue->get_top_for_execution_if_time(thd, &event_name))
    {
      sql_print_information("Event Scheduler: "
                            "Serious error during getting next "
                            "event to execute. Stopping");
      break;
    }

    DBUG_PRINT("info", ("get_top_for_execution_if_time returned "
                        "event_name=0x%lx", (long) event_name));
    if (event_name)
    {
      if ((res= execute_top(event_name)))
        break;
    }
    else
    {
      DBUG_ASSERT(thd->killed);
      DBUG_PRINT("info", ("job_data is NULL, the thread was killed"));
    }
    DBUG_PRINT("info", ("state=%s", scheduler_states_names[state].str));
  }

  LOCK_DATA();
  deinit_event_thread(thd);
  scheduler_thd= NULL;
  state= INITIALIZED;
  DBUG_PRINT("info", ("Broadcasting COND_state back to the stoppers"));
  mysql_cond_broadcast(&COND_state);
  UNLOCK_DATA();

  DBUG_RETURN(res);
}


/*
  Creates a new THD instance and then forks a new thread, while passing
  the THD pointer and job_data to it.

  SYNOPSIS
    Event_scheduler::execute_top()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (Serious error)
*/

bool
Event_scheduler::execute_top(Event_queue_element_for_exec *event_name)
{
  THD *new_thd;
  pthread_t th;
  int res= 0;
  DBUG_ENTER("Event_scheduler::execute_top");
  if (!(new_thd= new THD()))
    goto error;

  pre_init_event_thread(new_thd);
  new_thd->system_thread= SYSTEM_THREAD_EVENT_WORKER;
  event_name->thd= new_thd;
  DBUG_PRINT("info", ("Event %s@%s ready for start",
             event_name->dbname.str, event_name->name.str));

  /*
    TODO: should use thread pool here, preferably with an upper limit
    on number of threads: if too many events are scheduled for the
    same time, starting all of them at once won't help them run truly
    in parallel (because of the great amount of synchronization), so
    we may as well execute them in sequence, keeping concurrency at a
    reasonable level.
  */
  /* Major failure */
  if ((res= mysql_thread_create(key_thread_event_worker,
                                &th, &connection_attrib, event_worker_thread,
                                event_name)))
  {
    mysql_mutex_lock(&LOCK_global_system_variables);
    Events::opt_event_scheduler= Events::EVENTS_OFF;
    mysql_mutex_unlock(&LOCK_global_system_variables);

    sql_print_error("Event_scheduler::execute_top: Can not create event worker"
                    " thread (errno=%d). Stopping event scheduler", res);

    new_thd->proc_info= "Clearing";
    DBUG_ASSERT(new_thd->net.buff != 0);
    net_end(&new_thd->net);

    goto error;
  }

  ++started_events;

  DBUG_PRINT("info", ("Event is in THD: 0x%lx", (long) new_thd));
  DBUG_RETURN(FALSE);

error:
  DBUG_PRINT("error", ("Event_scheduler::execute_top() res: %d", res));
  if (new_thd)
    delete new_thd;

  delete event_name;
  DBUG_RETURN(TRUE);
}


/*
  Checks whether the state of the scheduler is RUNNING

  SYNOPSIS
    Event_scheduler::is_running()

  RETURN VALUE
    TRUE   RUNNING
    FALSE  Not RUNNING
*/

bool
Event_scheduler::is_running()
{
  LOCK_DATA();
  bool ret= (state == RUNNING);
  UNLOCK_DATA();
  return ret;
}


/**
  Stops the scheduler (again). Waits for acknowledgement from the
  scheduler that it has stopped - synchronous stopping.

  Already running events will not be stopped. If the user needs
  them stopped manual intervention is needed.

  SYNOPSIS
    Event_scheduler::stop()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (not reported)
*/

bool
Event_scheduler::stop()
{
  THD *thd= current_thd;
  DBUG_ENTER("Event_scheduler::stop");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  LOCK_DATA();
  DBUG_PRINT("info", ("state before action %s", scheduler_states_names[state].str));
  if (state != RUNNING)
  {
    /* Synchronously wait until the scheduler stops. */
    while (state != INITIALIZED)
      COND_STATE_WAIT(thd, NULL, &stage_waiting_for_scheduler_to_stop);
    goto end;
  }

  /* Guarantee we don't catch spurious signals */
  do {
    DBUG_PRINT("info", ("Waiting for COND_started_or_stopped from "
                        "the scheduler thread.  Current value of state is %s . "
                        "workers count=%d", scheduler_states_names[state].str,
                        workers_count()));
    /*
      NOTE: We don't use kill_one_thread() because it can't kill COM_DEAMON
      threads. In addition, kill_one_thread() requires THD but during shutdown
      current_thd is NULL. Hence, if kill_one_thread should be used it has to
      be modified to kill also daemons, by adding a flag, and also we have to
      create artificial THD here. To save all this work, we just do what
      kill_one_thread() does to kill a thread. See also sql_repl.cc for similar
      usage.
    */

    state= STOPPING;
    DBUG_PRINT("info", ("Scheduler thread has id %lu",
                        scheduler_thd->thread_id));
    /* Lock from delete */
    mysql_mutex_lock(&scheduler_thd->LOCK_thd_data);
    /* This will wake up the thread if it waits on Queue's conditional */
    sql_print_information("Event Scheduler: Killing the scheduler thread, "
                          "thread id %lu",
                          scheduler_thd->thread_id);
    scheduler_thd->awake(THD::KILL_CONNECTION);
    mysql_mutex_unlock(&scheduler_thd->LOCK_thd_data);

    /* thd could be 0x0, when shutting down */
    sql_print_information("Event Scheduler: "
                          "Waiting for the scheduler thread to reply");
    COND_STATE_WAIT(thd, NULL, &stage_waiting_for_scheduler_to_stop);
  } while (state == STOPPING);
  DBUG_PRINT("info", ("Scheduler thread has cleaned up. Set state to INIT"));
  sql_print_information("Event Scheduler: Stopped");
end:
  UNLOCK_DATA();
  DBUG_RETURN(FALSE);
}


/*
  Returns the number of living event worker threads.

  SYNOPSIS
    Event_scheduler::workers_count()
*/

uint
Event_scheduler::workers_count()
{
  uint count= 0;

  DBUG_ENTER("Event_scheduler::workers_count");
  mysql_mutex_lock(&LOCK_thread_count);
  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
    if ((*it)->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      ++count;
  mysql_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("exit", ("%d", count));
  DBUG_RETURN(count);
}


/*
  Auxiliary function for locking LOCK_scheduler_state. Used
  by the LOCK_DATA macro.

  SYNOPSIS
    Event_scheduler::lock_data()
      func  Which function is requesting mutex lock
      line  On which line mutex lock is requested
*/

void
Event_scheduler::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::lock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mysql_mutex_lock(&LOCK_scheduler_state);
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_scheduler_data_locked= TRUE;
  DBUG_VOID_RETURN;
}


/*
  Auxiliary function for unlocking LOCK_scheduler_state. Used
  by the UNLOCK_DATA macro.

  SYNOPSIS
    Event_scheduler::unlock_data()
      func  Which function is requesting mutex unlock
      line  On which line mutex unlock is requested
*/

void
Event_scheduler::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::unlock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_unlocked_at_line= line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  mysql_mutex_unlock(&LOCK_scheduler_state);
  DBUG_VOID_RETURN;
}


/*
  Wrapper for mysql_cond_wait/timedwait

  SYNOPSIS
    Event_scheduler::cond_wait()
      thd     Thread (Could be NULL during shutdown procedure)
      abstime If not null then call mysql_cond_timedwait()
      msg     Message for thd->proc_info
      func    Which function is requesting cond_wait
      line    On which line cond_wait is requested
*/

void
Event_scheduler::cond_wait(THD *thd, struct timespec *abstime, const PSI_stage_info *stage,
                           const char *src_func, const char *src_file, uint src_line)
{
  DBUG_ENTER("Event_scheduler::cond_wait");
  waiting_on_cond= TRUE;
  mutex_last_unlocked_at_line= src_line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= src_func;
  if (thd)
    thd->enter_cond(&COND_state, &LOCK_scheduler_state, stage,
                    NULL, src_func, src_file, src_line);

  DBUG_PRINT("info", ("mysql_cond_%swait", abstime? "timed":""));
  if (!abstime)
    mysql_cond_wait(&COND_state, &LOCK_scheduler_state);
  else
    mysql_cond_timedwait(&COND_state, &LOCK_scheduler_state, abstime);
  if (thd)
  {
    /*
      This will free the lock so we need to relock. Not the best thing to
      do but we need to obey cond_wait()
    */
    thd->exit_cond(NULL, src_func, src_file, src_line);
    LOCK_DATA();
  }
  mutex_last_locked_in_func= src_func;
  mutex_last_locked_at_line= src_line;
  mutex_scheduler_data_locked= TRUE;
  waiting_on_cond= FALSE;
  DBUG_VOID_RETURN;
}


/*
  Dumps the internal status of the scheduler

  SYNOPSIS
    Event_scheduler::dump_internal_status()
*/

void
Event_scheduler::dump_internal_status()
{
  DBUG_ENTER("Event_scheduler::dump_internal_status");

  puts("");
  puts("Event scheduler status:");
  printf("State      : %s\n", scheduler_states_names[state].str);
  printf("Thread id  : %lu\n", scheduler_thd? scheduler_thd->thread_id : 0);
  printf("LLA        : %s:%u\n", mutex_last_locked_in_func,
                                 mutex_last_locked_at_line);
  printf("LUA        : %s:%u\n", mutex_last_unlocked_in_func,
                                 mutex_last_unlocked_at_line);
  printf("WOC        : %s\n", waiting_on_cond? "YES":"NO");
  printf("Workers    : %u\n", workers_count());
  printf("Executed   : %lu\n", (ulong) started_events);
  printf("Data locked: %s\n", mutex_scheduler_data_locked ? "YES":"NO");

  DBUG_VOID_RETURN;
}

/**
  @} (End of group Event_Scheduler)
*/
