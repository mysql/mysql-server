/* Copyright (C) 2004-2006 MySQL AB

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
#include "events_priv.h"
#include "events.h"
#include "event_timed.h"
#include "event_scheduler.h"
#include "sp_head.h"

/*
  ToDo:
  1. Talk to Alik to get a check for configure.in for my_time_t and time_t
  2. Look at guardian.h|cc to see its life cycle, has similarities.
*/


/*
  The scheduler is implemented as class Event_scheduler. Only one instance is
  kept during the runtime of the server, by implementing the Singleton DP.
  Object instance is always there because the memory is allocated statically
  and initialized when the OS loader loads mysqld. This initialization is
  bare. Extended initialization is done during the call to
  Event_scheduler::init() in Events::init(). The reason for that late initialization
  is that some subsystems needed to boot the Scheduler are not available at
  earlier stages of the mysqld boot procedure. Events::init() is called in
  mysqld.cc . If the mysqld is started with --event-scheduler=0 then
  no initialization takes place and the scheduler is unavailable during this
  server run. The server should be started with --event-scheduler=1 to have
  the scheduler initialized and able to execute jobs. This starting alwa
  s implies that the jobs execution will start immediately. If the server
  is started with --event-scheduler=2 then the scheduler is started in suspended
  state. Default state, if --event-scheduler is not specified is 2.

  The scheduler only manages execution of the events. Their creation,
  alteration and deletion is delegated to other routines found in event.cc .
  These routines interact with the scheduler :
  - CREATE EVENT -> Event_scheduler::create_event()
  - ALTER EVENT  -> Event_scheduler::update_event()
  - DROP EVENT   -> Event_scheduler::drop_event()

  There is one mutex in the single Event_scheduler object which controls
  the simultaneous access to the objects invariants. Using one lock makes
  it easy to follow the workflow. This mutex is LOCK_scheduler_data. It is
  initialized in Event_scheduler::init(). Which in turn is called by the
  Facade class Events in event.cc, coming from init_thread_environment() from
  mysqld.cc -> no concurrency at this point. It's destroyed in
  Events::destroy_mutexes() called from clean_up_mutexes() in mysqld.cc .

  The full initialization is done in Event_scheduler::init() called from 
  Events::init(). It's done before any requests coming in, so this is a
  guarantee for not having concurrency.

  The scheduler is started with Event_scheduler::start() and stopped with
  Event_scheduler::stop(). When the scheduler starts it loads all events
  from mysql.event table. Unfortunately, there is a race condition between
  the event disk management functions and the scheduler ones
  (add/replace/drop_event & load_events_from_db()), because the operations
  do not happen under one global lock but the disk operations are guarded
  by the MYISAM lock on mysql.event. In the same time, the queue operations
  are guarded by  LOCK_scheduler_data. If the scheduler is start()-ed during
  server startup and stopped()-ed during server shutdown (in Events::shutdown()
  called by kill_server() in mysqld.cc) these races does not exist.

  Since the user may want to temporarily inhibit execution of events the
  scheduler can be suspended and then it can be forced to resume its
  operations. The API call to perform these is
  Event_scheduler::suspend_or_resume(enum enum_suspend_or_resume) .
  When the scheduler is suspended the main scheduler thread, which ATM
  happens to have thread_id 1, locks on a condition COND_suspend_or_resume.
  When this is signal is sent for the reverse operation the main scheduler
  loops continues to roll and execute events.

  When the scheduler is suspended all add/replace/drop_event() operations
  work as expected and the modify the queue but no events execution takes
  place.

  In contrast to the previous scheduler implementation, found in
  event_executor.cc, the start, shutdown, suspend and resume are synchronous
  operations. As a whole all operations are synchronized and no busy waits
  are used except in stop_all_running_events(), which waits until all
  running event worker threads have finished. It would have been nice to
  use a conditional on which this method will wait and the last thread to
  finish would signal it but this implies subclassing THD.

  The scheduler does not keep a counter of how many event worker threads are
  running, at any specific moment, because this will copy functionality
  already existing in the server. Namely, all THDs are registered in the
  global `threads` array. THD has member variable system_thread which
  identifies the type of thread. Connection threads being NON_SYSTEM_THREAD,
  all other have their enum value. Important for the scheduler are 
  SYSTEM_THREAD_EVENT_SCHEDULER and SYSTEM_THREAD_EVENT_WORKER.

  Class THD subclasses class ilink, which is the linked list of all threads.
  When a THD instance is destroyed it's being removed from threads, thus
  no manual intervention is needed. On the contrary registering is manual
  with threads.append() . Traversing the threads array every time a subclass
  of THD, for instance if we would have had THD_scheduler_worker to see
  how many events we have and whether the scheduler is shutting down will
  take much time and lead to a deadlock. stop_all_running_events() is called
  under LOCK_scheduler_data. If the THD_scheduler_worker was aware of
  the single Event_scheduler instance it will try to check
  Event_scheduler::state but for this it would need to acquire 
  LOCK_scheduler_data => deadlock. Thus stop_all_running_events() uses a
  busy wait.

  DROP DATABASE DDL should drop all events defined in a specific schema.
  DROP USER also should drop all events who has as definer the user being
  dropped (this one is not addressed at the moment but a hook exists). For
  this specific needs Event_scheduler::drop_matching_events() is
  implemented. Which expects a callback to be applied on every object in
  the queue. Thus events that match specific schema or user, will be
  removed from the queue. The exposed interface is :
  - Event_scheduler::drop_schema_events()
  - Event_scheduler::drop_user_events()

  This bulk dropping happens under LOCK_scheduler_data, thus no two or
  more threads can execute it in parallel. However, DROP DATABASE is also
  synchronized, currently, in the server thus this does not impact the 
  overall performance. In addition, DROP DATABASE is not that often
  executed DDL.

  Though the interface to the scheduler is only through the public methods
  of class Event_scheduler, there are currently few functions which are
  used during its operations. Namely :
  - static evex_print_warnings()
    After every event execution all errors/warnings are dumped, so the user
    can see in case of a problem what the problem was.

  - static init_event_thread()
    This function is both used by event_scheduler_thread() and
    event_worker_thread(). It initializes the THD structure. The
    initialization looks pretty similar to the one in slave.cc done for the
    replication threads. However, though the similarities it cannot be
    factored out to have one routine.

  - static event_scheduler_thread()
    Because our way to register functions to be used by the threading library
    does not allow usage of static methods this function is used to start the
    scheduler in it. It does THD initialization and then calls
    Event_scheduler::run(). 

  - static event_worker_thread()
    With already stated the reason for not being able to use methods, this
    function executes the worker threads.

  The execution of events is, to some extent, synchronized to inhibit race
  conditions when Event_timed::thread_id is being updated with the thread_id of
  the THD in which the event is being executed. The thread_id is in the
  Event_timed object because we need to be able to kill quickly a specific
  event during ALTER/DROP EVENT without traversing the global `threads` array.
  However, this makes the scheduler's code more complicated. The event worker
  thread is started by Event_timed::spawn_now(), which in turn calls
  pthread_create(). The thread_id which will be associated in init_event_thread
  is not known in advance thus the registering takes place in
  event_worker_thread(). This registering has to be synchronized under
  LOCK_scheduler_data, so no kill_event() on a object in
  replace_event/drop_event/drop_matching_events() could take place.
  
  This synchronization is done through class Worker_thread_param that is
  local to this file. Event_scheduler::execute_top() is called under
  LOCK_scheduler_data. This method :
  1. Creates an instance of Worker_thread_param on the stack
  2. Locks Worker_thread_param::LOCK_started
  3. Calls Event_timed::spawn_now() which in turn creates a new thread.
  4. Locks on Worker_thread_param::COND_started_or_stopped and waits till the
     worker thread send signal. The code is spurious wake-up safe because
     Worker_thread_param::started is checked.
  5. The worker thread initializes its THD, then sets Event_timed::thread_id,
     sets Worker_thread_param::started to TRUE and sends back
     Worker_thread_param::COND_started. From this moment on, the event
     is being executed and could be killed by using Event_timed::thread_id.
     When Event_timed::spawn_thread_finish() is called in the worker thread,
     it sets thread_id to 0. From this moment on, the worker thread should not
     touch the Event_timed instance.


  The life-cycle of the server is a FSA.
  enum enum_state Event_scheduler::state keeps the state of the scheduler.

  The states are:

  |---UNINITIALIZED
  |                                         
  |                                      |------------------> IN_SHUTDOWN   
  --> INITIALIZED -> COMMENCING ---> RUNNING ----------|
       ^ ^               |            | ^              |
       | |- CANTSTART <--|            | |- SUSPENDED <-|
       |______________________________|

    - UNINITIALIZED :The object is created and only the mutex is initialized
    - INITIALIZED   :All member variables are initialized
    - COMMENCING    :The scheduler is starting, no other attempt to start 
                     should succeed before the state is back to INITIALIZED.
    - CANTSTART     :Set by the ::run() method in case it can't start for some
                     reason. In this case the connection thread that tries to
                     start the scheduler sees that some error has occurred and
                     returns an error to the user. Finally, the connection
                     thread sets the state to INITIALIZED, so further attempts
                     to start the scheduler could be made.
    - RUNNING       :The scheduler is running. New events could be added,
                     dropped, altered. The scheduler could be stopped.
    - SUSPENDED     :Like RUNNING but execution of events does not take place.
                     Operations on the memory queue are possible.
    - IN_SHUTDOWN   :The scheduler is shutting down, due to request by setting
                     the global event_scheduler to 0/FALSE, or because of a
                     KILL command sent by a user to the master thread.

  In every method the macros LOCK_SCHEDULER_DATA() and UNLOCK_SCHEDULER_DATA()
  are used for (un)locking purposes.  They are used to save the programmer
  from typing everytime
  lock_data(__FUNCTION__, __LINE__); 
  All locking goes through Event_scheduler::lock_data() and ::unlock_data().
  These two functions then record in variables where for last time
  LOCK_scheduler_data was locked and unlocked (two different variables). In
  multithreaded environment, in some cases they make no sense but are useful for
  inspecting deadlocks without having the server debug log turned on and the
  server is still running.

  The same strategy is used for conditional variables.
  Event_scheduler::cond_wait() is invoked from all places with parameter
  an enum enum_cond_vars. In this manner, it's possible to inspect the last
  on which condition the last call to cond_wait() was waiting. If the server
  was started with debug trace switched on, the trace file also holds information
  about conditional variables used.
*/

#ifdef __GNUC__
#if __GNUC__ >= 2
#define SCHED_FUNC __FUNCTION__
#endif
#else
#define SCHED_FUNC "<unknown>"
#endif

#define LOCK_SCHEDULER_DATA()   lock_data(SCHED_FUNC, __LINE__)
#define UNLOCK_SCHEDULER_DATA() unlock_data(SCHED_FUNC, __LINE__)


#ifndef DBUG_OFF
static
LEX_STRING states_names[] =
{
  {(char*) STRING_WITH_LEN("UNINITIALIZED")},
  {(char*) STRING_WITH_LEN("INITIALIZED")},
  {(char*) STRING_WITH_LEN("COMMENCING")},
  {(char*) STRING_WITH_LEN("CANTSTART")},
  {(char*) STRING_WITH_LEN("RUNNING")},
  {(char*) STRING_WITH_LEN("SUSPENDED")},
  {(char*) STRING_WITH_LEN("IN_SHUTDOWN")}
};
#endif


Event_scheduler
Event_scheduler::singleton;


const char * const
Event_scheduler::cond_vars_names[Event_scheduler::COND_LAST] =
{
  "new work",
  "started or stopped",
  "suspend or resume"
};


class Worker_thread_param
{
public:
  Event_timed *et;
  pthread_mutex_t LOCK_started;
  pthread_cond_t COND_started;
  bool started;

  Worker_thread_param(Event_timed *etn):et(etn), started(FALSE)
  {
    pthread_mutex_init(&LOCK_started, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&COND_started, NULL);  
  }

  ~Worker_thread_param()
  {
    pthread_mutex_destroy(&LOCK_started);
    pthread_cond_destroy(&COND_started);
  }
};


/*
  Compares the execute_at members of 2 Event_timed instances.
  Used as callback for the prioritized queue when shifting
  elements inside.

  SYNOPSIS
    event_timed_compare_q()
  
      vptr - not used (set it to NULL)
      a    - first Event_timed object
      b    - second Event_timed object

  RETURN VALUE
   -1   - a->execute_at < b->execute_at
    0   - a->execute_at == b->execute_at
    1   - a->execute_at > b->execute_at
    
  NOTES
    execute_at.second_part is not considered during comparison
*/

static int 
event_timed_compare_q(void *vptr, byte* a, byte *b)
{
  return my_time_compare(&((Event_timed *)a)->execute_at,
                         &((Event_timed *)b)->execute_at);
}


/*
  Prints the stack of infos, warnings, errors from thd to
  the console so it can be fetched by the logs-into-tables and
  checked later.

  SYNOPSIS
    evex_print_warnings
      thd    - thread used during the execution of the event
      et     - the event itself
*/

static void
evex_print_warnings(THD *thd, Event_timed *et)
{
  MYSQL_ERROR *err;
  DBUG_ENTER("evex_print_warnings");
  if (!thd->warn_list.elements)
    DBUG_VOID_RETURN;

  char msg_buf[10 * STRING_BUFFER_USUAL_SIZE];
  char prefix_buf[5 * STRING_BUFFER_USUAL_SIZE];
  String prefix(prefix_buf, sizeof(prefix_buf), system_charset_info);
  prefix.length(0);
  prefix.append("SCHEDULER: [");

  append_identifier(thd, &prefix, et->definer_user.str, et->definer_user.length);
  prefix.append('@');
  append_identifier(thd, &prefix, et->definer_host.str, et->definer_host.length);
  prefix.append("][", 2);
  append_identifier(thd,&prefix, et->dbname.str, et->dbname.length);
  prefix.append('.');
  append_identifier(thd,&prefix, et->name.str, et->name.length);
  prefix.append("] ", 2);

  List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
  while ((err= it++))
  {
    String err_msg(msg_buf, sizeof(msg_buf), system_charset_info);
    /* set it to 0 or we start adding at the end. That's the trick ;) */
    err_msg.length(0);
    err_msg.append(prefix);
    err_msg.append(err->msg, strlen(err->msg), system_charset_info);
    err_msg.append("]");
    DBUG_ASSERT(err->level < 3);
    (sql_print_message_handlers[err->level])("%*s", err_msg.length(),
                                              err_msg.c_ptr());
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

  RETURN VALUE
    0  OK
   -1  Error
*/

static int
init_event_thread(THD** t, enum enum_thread_type thread_type)
{
  THD *thd= *t;
  thd->thread_stack= (char*)t;                  // remember where our stack is
  DBUG_ENTER("init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->master_access= 0;
  thd->security_ctx->db_access= 0;
  thd->security_ctx->host_or_ip= (char*)my_localhost;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout= slave_net_timeout;
  thd->slave_thread= 0;
  thd->options|= OPTION_AUTO_IS_NULL;
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;
  thd->real_id=pthread_self();
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->thread_id= thread_id++;
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    DBUG_RETURN(-1);
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  /*
    Guarantees that we will see the thread in SHOW PROCESSLIST though its
    vio is NULL.
  */
  thd->system_thread= thread_type;

  thd->proc_info= "Initialized";
  thd->version= refresh_version;
  thd->set_time();

  DBUG_RETURN(0);
}


/*
  Inits the main scheduler thread and then calls Event_scheduler::run()
  of arg. 

  SYNOPSIS
    event_scheduler_thread()
      arg  void* ptr to Event_scheduler

  NOTES
    1. The host of the thead is my_localhost
    2. thd->net is initted with NULL - no communication.
    3. The reason to have a proxy function is that it's not possible to
       use a method as function to be executed in a spawned thread:
       - our pthread_hander_t macro uses extern "C"
       - separating thread setup from the real execution loop is also to be
         considered good.

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_scheduler_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd= NULL;                               
  Event_scheduler *scheduler= (Event_scheduler *) arg;

  DBUG_ENTER("event_scheduler_thread");

  my_thread_init();
  pthread_detach_this_thread();

  /* note that constructor of THD uses DBUG_ ! */
  if (!(thd= new THD) || init_event_thread(&thd, SYSTEM_THREAD_EVENT_SCHEDULER))
  {
    sql_print_error("SCHEDULER: Cannot init manager event thread.");
    scheduler->report_error_during_start();
  }
  else
  {
    thd->security_ctx->set_user((char*)"event_scheduler");

    sql_print_information("SCHEDULER: Manager thread booting");
    if (Event_scheduler::check_system_tables(thd))
      scheduler->report_error_during_start();
    else
      scheduler->run(thd);

    /*
      NOTE: Don't touch `scheduler` after this point because we have notified
            the
            thread which shuts us down that we have finished cleaning. In this
            very moment a new scheduler thread could be started and a crash is
            not welcome.
    */
  }

  /*
    If we cannot create THD then don't decrease because we haven't touched
    thread_count and thread_running in init_event_thread() which was never
    called. In init_event_thread() thread_count and thread_running are
    always increased even in the case the method returns an error.
  */
  if (thd)
  {
    thd->proc_info= "Clearing";
    DBUG_ASSERT(thd->net.buff != 0);
    net_end(&thd->net);
    pthread_mutex_lock(&LOCK_thread_count);
    thread_count--;
    thread_running--;
    delete thd;
    pthread_mutex_unlock(&LOCK_thread_count);
  }
  my_thread_end();
  DBUG_RETURN(0);                               // Can't return anything here
}


/*
  Function that executes an event in a child thread. Setups the 
  environment for the event execution and cleans after that.

  SYNOPSIS
    event_worker_thread()
      arg  The Event_timed object to be processed

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_worker_thread(void *arg)
{
  THD *thd; /* needs to be first for thread_stack */
  Worker_thread_param *param= (Worker_thread_param *) arg;
  Event_timed *event= param->et;
  int ret;
  bool startup_error= FALSE;
  Security_context *save_ctx;
  /* this one is local and not needed after exec */
  Security_context security_ctx;

  DBUG_ENTER("event_worker_thread");
  DBUG_PRINT("enter", ("event=[%s.%s]", event->dbname.str, event->name.str));

  my_thread_init();
  pthread_detach_this_thread();

  if (!(thd= new THD) || init_event_thread(&thd, SYSTEM_THREAD_EVENT_WORKER))
  {
    sql_print_error("SCHEDULER: Startup failure.");
    startup_error= TRUE;
    event->spawn_thread_finish(thd);
  }
  else
    event->set_thread_id(thd->thread_id);

  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));
  /*
    If we don't change it before we send the signal back, then an intermittent
    DROP EVENT will take LOCK_scheduler_data and try to kill this thread, because
    event->thread_id is already real. However, because thd->security_ctx->user
    is not initialized then a crash occurs in kill_one_thread(). Thus, we have
    to change the context before sending the signal. We are under
    LOCK_scheduler_data being held by Event_scheduler::run() -> ::execute_top().
  */
  change_security_context(thd, event->definer_user, event->definer_host,
                          event->dbname, &security_ctx, &save_ctx);
  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));

  /* Signal the scheduler thread that we have started successfully */
  pthread_mutex_lock(&param->LOCK_started);
  param->started= TRUE;
  pthread_cond_signal(&param->COND_started);
  pthread_mutex_unlock(&param->LOCK_started);

  if (!startup_error)
  {
    thd->init_for_queries();
    thd->enable_slow_log= TRUE;

    event->set_thread_id(thd->thread_id);
    sql_print_information("SCHEDULER: [%s.%s of %s] executing in thread %lu",
                          event->dbname.str, event->name.str,
                          event->definer.str, thd->thread_id);

    ret= event->execute(thd, thd->mem_root);
    evex_print_warnings(thd, event);
    sql_print_information("SCHEDULER: [%s.%s of %s] executed. RetCode=%d",
                          event->dbname.str, event->name.str,
                          event->definer.str, ret);
    if (ret == EVEX_COMPILE_ERROR)
      sql_print_information("SCHEDULER: COMPILE ERROR for event %s.%s of %s",
                            event->dbname.str, event->name.str,
                            event->definer.str);
    else if (ret == EVEX_MICROSECOND_UNSUP)
      sql_print_information("SCHEDULER: MICROSECOND is not supported");
    
    DBUG_PRINT("info", ("master_access=%d db_access=%d",
               thd->security_ctx->master_access, thd->security_ctx->db_access));

    /* If true is returned, we are expected to free it */
    if (event->spawn_thread_finish(thd))
    {
      DBUG_PRINT("info", ("Freeing object pointer"));
      delete event;
    }
  }

  if (thd)
  {
    thd->proc_info= "Clearing";
    DBUG_ASSERT(thd->net.buff != 0);
    /*
      Free it here because net.vio is NULL for us => THD::~THD will check it
      and won't call net_end(&net); See also replication code.
    */
    net_end(&thd->net);
    DBUG_PRINT("info", ("Worker thread %lu exiting", thd->thread_id));
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thread_count--;
    thread_running--;
    delete thd;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }

  my_thread_end();
  DBUG_RETURN(0);                               // Can't return anything here
}


/*
  Constructor of class Event_scheduler.

  SYNOPSIS
    Event_scheduler::Event_scheduler()
*/

Event_scheduler::Event_scheduler()
  :state(UNINITIALIZED), start_scheduler_suspended(FALSE),
  thread_id(0), mutex_last_locked_at_line(0),
  mutex_last_unlocked_at_line(0), mutex_last_locked_in_func(""),
  mutex_last_unlocked_in_func(""), cond_waiting_on(COND_NONE),
  mutex_scheduler_data_locked(FALSE)
{
}


/*
  Returns the singleton instance of the class.

  SYNOPSIS
    Event_scheduler::get_instance()

  RETURN VALUE
    address
*/

Event_scheduler*
Event_scheduler::get_instance()
{
  DBUG_ENTER("Event_scheduler::get_instance");
  DBUG_RETURN(&singleton);
}


/*
  The implementation of full-fledged initialization.

  SYNOPSIS
    Event_scheduler::init()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_scheduler::init()
{
  int i= 0;
  bool ret= FALSE;
  DBUG_ENTER("Event_scheduler::init");
  DBUG_PRINT("enter", ("this=%p", this));

  LOCK_SCHEDULER_DATA();
  for (;i < COND_LAST; i++)
    if (pthread_cond_init(&cond_vars[i], NULL))
    {
      sql_print_error("SCHEDULER: Unable to initalize conditions");
      ret= TRUE;
      goto end;
    }

  /* init memory root */
  init_alloc_root(&scheduler_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  if (init_queue_ex(&queue, 30 /*num_el*/, 0 /*offset*/, 0 /*smallest_on_top*/,
                    event_timed_compare_q, NULL, 30 /*auto_extent*/))
  {
    sql_print_error("SCHEDULER: Can't initialize the execution queue");
    ret= TRUE;
    goto end;
  }

  if (sizeof(my_time_t) != sizeof(time_t))
  {
    sql_print_error("SCHEDULER: sizeof(my_time_t) != sizeof(time_t) ."
                    "The scheduler may not work correctly. Stopping.");
    DBUG_ASSERT(0);
    ret= TRUE;
    goto end;
  }

  state= INITIALIZED;
end:
  UNLOCK_SCHEDULER_DATA();
  DBUG_RETURN(ret);
}


/*
  Frees all memory allocated by the scheduler object.

  SYNOPSIS
    Event_scheduler::destroy()
  
  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

void
Event_scheduler::destroy()
{
  DBUG_ENTER("Event_scheduler");

  LOCK_SCHEDULER_DATA();
  switch (state) {
  case UNINITIALIZED:
    break;
  case INITIALIZED:
    delete_queue(&queue);
    free_root(&scheduler_root, MYF(0));
    int i;
    for (i= 0; i < COND_LAST; i++)
      pthread_cond_destroy(&cond_vars[i]);
    state= UNINITIALIZED;
    break;
  default:
    sql_print_error("SCHEDULER: Destroying while state is %d", state);
    /* I trust my code but ::safe() > ::sorry() */
    DBUG_ASSERT(0);
    break;
  }
  UNLOCK_SCHEDULER_DATA();

  DBUG_VOID_RETURN;
}


/*
  Creates an event in the scheduler queue

  SYNOPSIS
    Event_scheduler::create_event()
      et              The event to add
      check_existence Whether to check if already loaded.

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
*/

enum Event_scheduler::enum_error_code
Event_scheduler::create_event(THD *thd, Event_timed *et, bool check_existence)
{
  enum enum_error_code res;
  Event_timed *et_new;
  DBUG_ENTER("Event_scheduler::create_event");
  DBUG_PRINT("enter", ("thd=%p et=%p lock=%p",thd,et,&LOCK_scheduler_data));

  LOCK_SCHEDULER_DATA();
  if (!is_running_or_suspended())
  {
    DBUG_PRINT("info", ("scheduler not running but %d. doing nothing", state));
    UNLOCK_SCHEDULER_DATA();
    DBUG_RETURN(OP_OK);
  }
  if (check_existence && find_event(et, FALSE))
  {
    res= OP_ALREADY_EXISTS;
    goto end;
  }

  /* We need to load the event on scheduler_root */
  if (!(res= load_named_event(thd, et, &et_new)))
  {
    queue_insert_safe(&queue, (byte *) et_new);
    DBUG_PRINT("info", ("Sending COND_new_work"));
    pthread_cond_signal(&cond_vars[COND_new_work]);
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;
end:
  UNLOCK_SCHEDULER_DATA();
  DBUG_RETURN(res);
}


/*
  Drops an event from the scheduler queue

  SYNOPSIS
    Event_scheduler::drop_event()
      etn    The event to drop
      state  Wait the event or kill&drop

  RETURN VALUE
    FALSE OK (replaced or scheduler not working)
    TRUE  Failure
*/

bool
Event_scheduler::drop_event(THD *thd, Event_timed *et)
{
  int res;
  Event_timed *et_old;
  DBUG_ENTER("Event_scheduler::drop_event");
  DBUG_PRINT("enter", ("thd=%p et=%p lock=%p",thd,et,&LOCK_scheduler_data));

  LOCK_SCHEDULER_DATA();
  if (!is_running_or_suspended())
  {
    DBUG_PRINT("info", ("scheduler not running but %d. doing nothing", state));
    UNLOCK_SCHEDULER_DATA();
    DBUG_RETURN(OP_OK);
  }

  if (!(et_old= find_event(et, TRUE)))
    DBUG_PRINT("info", ("No such event found, probably DISABLED"));

  UNLOCK_SCHEDULER_DATA();

  /* See comments in ::replace_event() why this is split in two parts. */
  if (et_old)
  {
    switch ((res= et_old->kill_thread(thd))) {
    case EVEX_CANT_KILL:
      /* Don't delete but continue */
      et_old->flags |= EVENT_FREE_WHEN_FINISHED;
      break;
    case 0:
      /* 
        kill_thread() waits till the spawned thread finishes after it's
        killed. Hence, we delete here memory which is no more referenced from
        a running thread.
      */
      delete et_old;
      /*
        We don't signal COND_new_work here because:
        1. Even if the dropped event is on top of the queue this will not
          move another one to be executed before the time the one on the
          top (but could be at the same second as the dropped one)
        2. If this was the last event on the queue, then pthread_cond_timedwait
          in ::run() will finish and then see that the queue is empty and
          call cond_wait(). Hence, no need to interrupt the blocked
          ::run() thread.
      */
      break;
    default:
      sql_print_error("SCHEDULER: Got unexpected error %d", res);
      DBUG_ASSERT(0);
    }
  }

  DBUG_RETURN(FALSE);
}


/*
  Updates an event from the scheduler queue

  SYNOPSIS
    Event_scheduler::replace_event()
      et    The event to replace(add) into the queue
      state  Async or sync stopping

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
    OP_ALREADY_EXISTS Event already in the queue    
*/

enum Event_scheduler::enum_error_code
Event_scheduler::update_event(THD *thd, Event_timed *et,
                               LEX_STRING *new_schema,
                               LEX_STRING *new_name)
{
  enum enum_error_code res;
  Event_timed *et_old, *et_new= NULL;
  LEX_STRING old_schema, old_name;

  LINT_INIT(old_schema.str);
  LINT_INIT(old_schema.length);
  LINT_INIT(old_name.str);
  LINT_INIT(old_name.length);

  DBUG_ENTER("Event_scheduler::update_event");
  DBUG_PRINT("enter", ("thd=%p et=%p et=[%s.%s] lock=%p",
             thd, et, et->dbname.str, et->name.str, &LOCK_scheduler_data));

  LOCK_SCHEDULER_DATA();
  if (!is_running_or_suspended())
  {
    DBUG_PRINT("info", ("scheduler not running but %d. doing nothing", state));
    UNLOCK_SCHEDULER_DATA();
    DBUG_RETURN(OP_OK);
  }

  if (!(et_old= find_event(et, TRUE)))
    DBUG_PRINT("info", ("%s.%s not found cached, probably was DISABLED",
                        et->dbname.str, et->name.str));

  if (new_schema && new_name)
  {
    old_schema= et->dbname;
    old_name= et->name;
    et->dbname= *new_schema;
    et->name= *new_name;
  }
  /*
    We need to load the event (it's strings but on the object itself)
    on scheduler_root. et_new could be NULL :
    1. Error occured
    2. If the replace is DISABLED, we don't load it into the queue.
  */
  if (!(res= load_named_event(thd, et, &et_new)))
  {
    queue_insert_safe(&queue, (byte *) et_new);
    DBUG_PRINT("info", ("Sending COND_new_work"));
    pthread_cond_signal(&cond_vars[COND_new_work]);
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;

  if (new_schema && new_name)
  {
    et->dbname= old_schema;
    et->name= old_name;
  }

  UNLOCK_SCHEDULER_DATA();
  /*
    Andrey: Is this comment still truthful ???

    We don't move this code above because a potential kill_thread will call
    THD::awake(). Which in turn will try to acqure mysys_var->current_mutex,
    which is LOCK_scheduler_data on which the COND_new_work in ::run() locks.
    Hence, we try to acquire a lock which we have already acquired and we run
    into an assert. Holding LOCK_scheduler_data however is not needed because
    we don't touch any invariant of the scheduler anymore. ::drop_event() does
    the same.
  */
  if (et_old)
  {
    switch (et_old->kill_thread(thd)) {
    case EVEX_CANT_KILL:
      /* Don't delete but continue */
      et_old->flags |= EVENT_FREE_WHEN_FINISHED;
      break;
    case 0:
      /* 
        kill_thread() waits till the spawned thread finishes after it's
        killed. Hence, we delete here memory which is no more referenced from
        a running thread.
      */
      delete et_old;
      /*
        We don't signal COND_new_work here because:
        1. Even if the dropped event is on top of the queue this will not
          move another one to be executed before the time the one on the
          top (but could be at the same second as the dropped one)
        2. If this was the last event on the queue, then pthread_cond_timedwait
          in ::run() will finish and then see that the queue is empty and
          call cond_wait(). Hence, no need to interrupt the blocked
          ::run() thread.
      */
      break;
    default:
      DBUG_ASSERT(0);
    }
  }

  DBUG_RETURN(res);
}


/*
  Searches for an event in the scheduler queue

  SYNOPSIS
    Event_scheduler::find_event()
      etn            The event to find
      comparator     The function to use for comparing
      remove_from_q  If found whether to remove from the Q

  RETURN VALUE
    NULL       Not found
    otherwise  Address

  NOTE
    The caller should do the locking also the caller is responsible for
    actual signalling in case an event is removed from the queue 
    (signalling COND_new_work for instance).
*/

Event_timed *
Event_scheduler::find_event(Event_timed *etn, bool remove_from_q)
{
  uint i;
  DBUG_ENTER("Event_scheduler::find_event");

  for (i= 0; i < queue.elements; ++i)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?", etn->dbname.str, etn->name.str,
                        et->dbname.str, et->name.str));
    if (event_timed_identifier_equal(etn, et))
    {
      if (remove_from_q)
        queue_remove(&queue, i);
      DBUG_RETURN(et);
    }
  }

  DBUG_RETURN(NULL);
}


/*
  Drops all events from the in-memory queue and disk that match
  certain pattern evaluated by a comparator function

  SYNOPSIS
    Event_scheduler::drop_matching_events()
      thd            THD
      pattern        A pattern string
      comparator     The function to use for comparing

  RETURN VALUE
     -1  Scheduler not working
    >=0  Number of dropped events
    
  NOTE
    Expected is the caller to acquire lock on LOCK_scheduler_data
*/

void
Event_scheduler::drop_matching_events(THD *thd, LEX_STRING *pattern,
                           bool (*comparator)(Event_timed *,LEX_STRING *))
{
  DBUG_ENTER("Event_scheduler::drop_matching_events");
  DBUG_PRINT("enter", ("pattern=%*s state=%d", pattern->length, pattern->str,
             state));
  if (is_running_or_suspended())
  {
    uint i= 0, dropped= 0;   
    while (i < queue.elements)
    {
      Event_timed *et= (Event_timed *) queue_element(&queue, i);
      DBUG_PRINT("info", ("[%s.%s]?", et->dbname.str, et->name.str));
      if (comparator(et, pattern))
      {
        /*
          The queue is ordered. If we remove an element, then all elements after
          it will shift one position to the left, if we imagine it as an array
          from left to the right. In this case we should not increment the 
          counter and the (i < queue.elements) condition is ok.
        */
        queue_remove(&queue, i);

        /* See replace_event() */
        switch (et->kill_thread(thd)) {
        case EVEX_CANT_KILL:
          /* Don't delete but continue */
          et->flags |= EVENT_FREE_WHEN_FINISHED;
          ++dropped;
          break;
        case 0:
          delete et;
          ++dropped;
          break;
        default:
          DBUG_ASSERT(0);
        }
      }
      else
        i++;
    }
    DBUG_PRINT("info", ("Dropped %lu", dropped));
  }
  /*
    Don't send COND_new_work because no need to wake up the scheduler thread.
    When it wakes next time up it will recalculate how much more it should
    sleep if the top of the queue has been changed by this method.
  */

  DBUG_VOID_RETURN;
}


/*
  Drops all events from the in-memory queue and disk that are from
  certain schema.

  SYNOPSIS
    Event_scheduler::drop_schema_events()
      thd        THD
      db         The schema name

  RETURN VALUE
     -1  Scheduler not working
    >=0  Number of dropped events
*/

int
Event_scheduler::drop_schema_events(THD *thd, LEX_STRING *schema)
{
  int ret;
  DBUG_ENTER("Event_scheduler::drop_schema_events");
  LOCK_SCHEDULER_DATA();
  if (is_running_or_suspended())
    drop_matching_events(thd, schema, event_timed_db_equal);

  ret= db_drop_events_from_table(thd, schema);
  UNLOCK_SCHEDULER_DATA();

  DBUG_RETURN(ret);
}


extern pthread_attr_t connection_attrib;


/*
  Starts the event scheduler

  SYNOPSIS
    Event_scheduler::start()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_scheduler::start()
{
  bool ret= FALSE;
  pthread_t th;
  DBUG_ENTER("Event_scheduler::start");

  LOCK_SCHEDULER_DATA();
  /* If already working or starting don't make another attempt */
  DBUG_ASSERT(state == INITIALIZED);
  if (state > INITIALIZED)
  {
    DBUG_PRINT("info", ("scheduler is already running or starting"));
    ret= TRUE;
    goto end;
  }

  /*
    Now if another thread calls start it will bail-out because the branch
    above will be executed. Thus no two or more child threads will be forked.
    If the child thread cannot start for some reason then `state` is set
    to CANTSTART and COND_started is also signaled. In this case we
    set `state` back to INITIALIZED so another attempt to start the scheduler
    can be made.
  */
  state= COMMENCING;
  /* Fork */
  if (pthread_create(&th, &connection_attrib, event_scheduler_thread,
                    (void*)this))
  {
    DBUG_PRINT("error", ("cannot create a new thread"));
    state= INITIALIZED;
    ret= TRUE;
    goto end;
  }

  /*  Wait till the child thread has booted (w/ or wo success) */
  while (!is_running_or_suspended() && state != CANTSTART)
    cond_wait(COND_started_or_stopped, &LOCK_scheduler_data);

  /*
    If we cannot start for some reason then don't prohibit further attempts.
    Set back to INITIALIZED.
  */
  if (state == CANTSTART)
  {
    state= INITIALIZED;
    ret= TRUE;
    goto end;
  }

end:
  UNLOCK_SCHEDULER_DATA();
  DBUG_RETURN(ret);
}


/*
  Starts the event scheduler in suspended mode.

  SYNOPSIS
    Event_scheduler::start_suspended()

  RETURN VALUE
    TRUE   OK
    FALSE  Error
*/

bool
Event_scheduler::start_suspended()
{
  DBUG_ENTER("Event_scheduler::start_suspended");
  start_scheduler_suspended= TRUE;
  DBUG_RETURN(start());
}



/*
  Report back that we cannot start. Used for ocasions where
  we can't go into ::run() and have to report externally.

  SYNOPSIS
    Event_scheduler::report_error_during_start()
*/

inline void
Event_scheduler::report_error_during_start()
{
  DBUG_ENTER("Event_scheduler::report_error_during_start");

  LOCK_SCHEDULER_DATA();
  state= CANTSTART;
  DBUG_PRINT("info", ("Sending back COND_started_or_stopped"));
  pthread_cond_signal(&cond_vars[COND_started_or_stopped]);
  UNLOCK_SCHEDULER_DATA();

  DBUG_VOID_RETURN;
}


/*
  The internal loop of the event scheduler

  SYNOPSIS
    Event_scheduler::run()
      thd  Thread

  RETURN VALUE
    FALSE OK
    TRUE  Failure
*/

bool
Event_scheduler::run(THD *thd)
{
  int ret;
  struct timespec abstime;
  DBUG_ENTER("Event_scheduler::run");
  DBUG_PRINT("enter", ("thd=%p", thd));

  LOCK_SCHEDULER_DATA();
  ret= load_events_from_db(thd);

  if (!ret)
  {
    thread_id= thd->thread_id;
    state= start_scheduler_suspended? SUSPENDED:RUNNING;
    start_scheduler_suspended= FALSE;
  }
  else 
    state= CANTSTART;

  DBUG_PRINT("info", ("Sending back COND_started_or_stopped"));
  pthread_cond_signal(&cond_vars[COND_started_or_stopped]);
  if (ret)
  {
    UNLOCK_SCHEDULER_DATA();
    DBUG_RETURN(TRUE);
  }
  if (!check_n_suspend_if_needed(thd))
    UNLOCK_SCHEDULER_DATA();

  sql_print_information("SCHEDULER: Manager thread started with id %lu",
                        thd->thread_id);
  abstime.tv_nsec= 0;
  while (is_running_or_suspended())
  {
    Event_timed *et;

    LOCK_SCHEDULER_DATA();
    if (check_n_wait_for_non_empty_queue(thd))
      continue;

    /* On TRUE data is unlocked, go back to the beginning */
    if (check_n_suspend_if_needed(thd))
      continue;

    /* Guaranteed locked here */
    if (state == IN_SHUTDOWN || shutdown_in_progress)
    {
      UNLOCK_SCHEDULER_DATA();
      break;
    }
    DBUG_ASSERT(state == RUNNING);

    et= (Event_timed *)queue_top(&queue);
    
    /* Skip disabled events */
    if (et->status != Event_timed::ENABLED)
    {
      /*
        It could be a one-timer scheduled for a time, already in the past when the
        scheduler was suspended.
      */
      sql_print_information("SCHEDULER: Found a disabled event %*s.%*s in the queue",
                            et->dbname.length, et->dbname.str, et->name.length,
                            et->name.str);
      queue_remove(&queue, 0);
      /* ToDo: check this again */
      if (et->dropped)
        et->drop(thd);
      delete et;
      UNLOCK_SCHEDULER_DATA();
      continue;
    }
    thd->proc_info= (char *)"Computing";
    DBUG_PRINT("evex manager",("computing time to sleep till next exec"));
    /* Timestamp is in UTC */
    abstime.tv_sec= sec_since_epoch_TIME(&et->execute_at);

    thd->end_time();
    if (abstime.tv_sec > thd->query_start())
    {
      /* Event trigger time is in the future */
      thd->proc_info= (char *)"Sleep";
      DBUG_PRINT("info", ("Going to sleep. Should wakeup after approx %d secs",
                         abstime.tv_sec - thd->query_start()));
      DBUG_PRINT("info", ("Entering condition because waiting for activation"));
      /*
        Use THD::enter_cond()/exit_cond() or we won't be able to kill a
        sleeping thread. Though ::stop() can do it by sending COND_new_work
        an user can't by just issuing 'KILL x'; . In the latter case
        pthread_cond_timedwait() will wait till `abstime`.
        "Sleeping until next time"
      */
      thd->enter_cond(&cond_vars[COND_new_work],&LOCK_scheduler_data,"Sleeping");

      pthread_cond_timedwait(&cond_vars[COND_new_work], &LOCK_scheduler_data,
                             &abstime);

      DBUG_PRINT("info", ("Manager woke up. state is %d", state));
      /*
        If we get signal we should recalculate the whether it's the right time
        because there could be :
        1. Spurious wake-up
        2. The top of the queue was changed (new one becase of add/drop/replace)
      */
      /* This will do implicit UNLOCK_SCHEDULER_DATA() */
      thd->exit_cond("");
    }
    else
    {
      thd->proc_info= (char *)"Executing";
      /*
        Execute the event. An error may occur if a thread cannot be forked.
        In this case stop  the manager.
        We should enter ::execute_top() with locked LOCK_scheduler_data.
      */
      int ret= execute_top(thd);
      UNLOCK_SCHEDULER_DATA();
      if (ret)
        break;
    }
  }

  thd->proc_info= (char *)"Cleaning";

  LOCK_SCHEDULER_DATA();
  /*
    It's possible that a user has used (SQL)COM_KILL. Hence set the appropriate
    state because it is only set by ::stop().
  */
  if (state != IN_SHUTDOWN)
  {
    DBUG_PRINT("info", ("We got KILL but the but not from ::stop()"));
    state= IN_SHUTDOWN;
  }
  UNLOCK_SCHEDULER_DATA();

  sql_print_information("SCHEDULER: Shutting down");

  thd->proc_info= (char *)"Cleaning queue";
  clean_queue(thd);
  THD_CHECK_SENTRY(thd);

  /* free mamager_root memory but don't destroy the root */
  thd->proc_info= (char *)"Cleaning memory root";
  free_root(&scheduler_root, MYF(0));
  THD_CHECK_SENTRY(thd);

  /*
    We notify the waiting thread which shutdowns us that we have cleaned.
    There are few more instructions to be executed in this pthread but
    they don't affect manager structures thus it's safe to signal already
    at this point.
  */
  LOCK_SCHEDULER_DATA();
  thd->proc_info= (char *)"Sending shutdown signal";
  DBUG_PRINT("info", ("Sending COND_started_or_stopped"));
  if (state == IN_SHUTDOWN)
    pthread_cond_signal(&cond_vars[COND_started_or_stopped]);

  state= INITIALIZED;
  /*
    We set it here because ::run() can stop not only because of ::stop()
    call but also because of `KILL x`
  */
  thread_id= 0;
  sql_print_information("SCHEDULER: Stopped");
  UNLOCK_SCHEDULER_DATA();

  /* We have modified, we set back */
  thd->query= NULL;
  thd->query_length= 0;

  DBUG_RETURN(FALSE);
}


/*
  Executes the top element of the queue. Auxiliary method for ::run().

  SYNOPSIS
    Event_scheduler::execute_top()

  RETURN VALUE
    FALSE OK
    TRUE  Failure

  NOTE
    NO locking is done. EXPECTED is that the caller should have locked
    the queue (w/ LOCK_scheduler_data).
*/

bool
Event_scheduler::execute_top(THD *thd)
{
  int spawn_ret_code;
  bool ret= FALSE;
  DBUG_ENTER("Event_scheduler::execute_top");
  DBUG_PRINT("enter", ("thd=%p", thd));

  Event_timed *et= (Event_timed *)queue_top(&queue);

  /* Is it good idea to pass a stack address ?*/
  Worker_thread_param param(et);

  pthread_mutex_lock(&param.LOCK_started);
  /* 
    We don't lock LOCK_scheduler_data fpr workers_increment() because it's a
    pre-requisite for calling the current_method.
  */
  switch ((spawn_ret_code= et->spawn_now(event_worker_thread, &param))) {
  case EVENT_EXEC_CANT_FORK:
    /* 
      We don't lock LOCK_scheduler_data here because it's a pre-requisite
      for calling the current_method.
    */
    sql_print_error("SCHEDULER: Problem while trying to create a thread");
    ret= TRUE;
    break;
  case EVENT_EXEC_ALREADY_EXEC:
    /* 
      We don't lock LOCK_scheduler_data here because it's a pre-requisite
      for calling the current_method.
    */
    sql_print_information("SCHEDULER: %s.%s in execution. Skip this time.",
                          et->dbname.str, et->name.str);
    if ((et->flags & EVENT_EXEC_NO_MORE) || et->status == Event_timed::DISABLED)
      queue_remove(&queue, 0);// 0 is top, internally 1
    else
      queue_replaced(&queue);
    break;
  default:
    DBUG_ASSERT(!spawn_ret_code);
    if ((et->flags & EVENT_EXEC_NO_MORE) || et->status == Event_timed::DISABLED)
      queue_remove(&queue, 0);// 0 is top, internally 1
    else
      queue_replaced(&queue);
    /* 
      We don't lock LOCK_scheduler_data here because it's a pre-requisite
      for calling the current_method.
    */
    if (likely(!spawn_ret_code))
    {
      /* Wait the forked thread to start */
      do {
        pthread_cond_wait(&param.COND_started, &param.LOCK_started);
      } while (!param.started);
    }
    /*
      param was allocated on the stack so no explicit delete as well as
      in this moment it's no more used in the spawned thread so it's safe
      to be deleted.
    */
    break;
  }
  pthread_mutex_unlock(&param.LOCK_started);
  /* `param` is on the stack and will be destructed by the compiler */

  DBUG_RETURN(ret);
}


/*
  Cleans the scheduler's queue. Auxiliary method for ::run().

  SYNOPSIS
    Event_scheduler::clean_queue()
      thd  Thread
*/

void
Event_scheduler::clean_queue(THD *thd)
{
  CHARSET_INFO *scs= system_charset_info;
  uint i;
  DBUG_ENTER("Event_scheduler::clean_queue");
  DBUG_PRINT("enter", ("thd=%p", thd));

  LOCK_SCHEDULER_DATA();
  stop_all_running_events(thd);
  UNLOCK_SCHEDULER_DATA();

  sql_print_information("SCHEDULER: Emptying the queue");

  /* empty the queue */
  for (i= 0; i < queue.elements; ++i)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    et->free_sp();
    delete et;
  }
  resize_queue(&queue, 0);

  DBUG_VOID_RETURN;
}


/*
  Stops all running events

  SYNOPSIS
    Event_scheduler::stop_all_running_events()
      thd  Thread
  
  NOTE
    LOCK_scheduler data must be acquired prior to call to this method
*/

void
Event_scheduler::stop_all_running_events(THD *thd)
{
  CHARSET_INFO *scs= system_charset_info;
  uint i;
  DYNAMIC_ARRAY running_threads;
  THD *tmp;
  DBUG_ENTER("Event_scheduler::stop_all_running_events");
  DBUG_PRINT("enter", ("workers_count=%d", workers_count()));

  my_init_dynamic_array(&running_threads, sizeof(ulong), 10, 10);

  bool had_super= FALSE;
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      push_dynamic(&running_threads, (gptr) &tmp->thread_id);
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  /* We need temporarily SUPER_ACL to be able to kill our offsprings */
  if (!(thd->security_ctx->master_access & SUPER_ACL))
    thd->security_ctx->master_access|= SUPER_ACL;
  else
    had_super= TRUE;

  char tmp_buff[10*STRING_BUFFER_USUAL_SIZE];
  char int_buff[STRING_BUFFER_USUAL_SIZE];
  String tmp_string(tmp_buff, sizeof(tmp_buff), scs);
  String int_string(int_buff, sizeof(int_buff), scs);
  tmp_string.length(0);

  for (i= 0; i < running_threads.elements; ++i)
  {
    int ret;
    ulong thd_id= *dynamic_element(&running_threads, i, ulong*);

    int_string.set((longlong) thd_id,scs);
    tmp_string.append(int_string);
    if (i < running_threads.elements - 1)
      tmp_string.append(' ');

    if ((ret= kill_one_thread(thd, thd_id, FALSE)))
    {
      sql_print_error("SCHEDULER: Error killing %lu code=%d", thd_id, ret);
      break;
    }
  }
  if (running_threads.elements)
    sql_print_information("SCHEDULER: Killing workers :%s", tmp_string.c_ptr());

  if (!had_super)
    thd->security_ctx->master_access &= ~SUPER_ACL;

  delete_dynamic(&running_threads);

  sql_print_information("SCHEDULER: Waiting for worker threads to finish");

  while (workers_count())
    my_sleep(100000);

  DBUG_VOID_RETURN;
}


/*
  Stops the event scheduler

  SYNOPSIS
    Event_scheduler::stop()

  RETURN VALUE
    OP_OK           OK
    OP_CANT_KILL    Error during stopping of manager thread
    OP_NOT_RUNNING  Manager not working

  NOTE
    The caller must have acquited LOCK_scheduler_data.
*/

enum Event_scheduler::enum_error_code
Event_scheduler::stop()
{
  THD *thd= current_thd;
  DBUG_ENTER("Event_scheduler::stop");
  DBUG_PRINT("enter", ("thd=%p", current_thd));

  LOCK_SCHEDULER_DATA();
  if (!is_running_or_suspended())
  {
    /*
      One situation to be here is if there was a start that forked a new
      thread but the new thread did not acquire yet LOCK_scheduler_data.
      Hence, in this case return an error.    
    */
    DBUG_PRINT("info", ("manager not running but %d. doing nothing", state));
    UNLOCK_SCHEDULER_DATA();
    DBUG_RETURN(OP_NOT_RUNNING);
  }
  state= IN_SHUTDOWN;

  DBUG_PRINT("info", ("Manager thread has id %d", thread_id));
  sql_print_information("SCHEDULER: Killing manager thread %lu", thread_id);
  
  /* 
    Sending the COND_new_work to ::run() is a way to get this working without
    race conditions. If we use kill_one_thread() it will call THD::awake() and
    because in ::run() both THD::enter_cond()/::exit_cond() are used,
    THD::awake() will try to lock LOCK_scheduler_data. If we UNLOCK it before,
    then the pthread_cond_signal(COND_started_or_stopped) could be signaled in
    ::run() and we can miss the signal before we relock. A way is to use
    another mutex for this shutdown procedure but better not.
  */
  pthread_cond_signal(&cond_vars[COND_new_work]);
  /* Or we are suspended - then we should wake up */
  pthread_cond_signal(&cond_vars[COND_suspend_or_resume]);

  /* Guarantee we don't catch spurious signals */
  sql_print_information("SCHEDULER: Waiting the manager thread to reply");
  while (state != INITIALIZED)
  {
    DBUG_PRINT("info", ("Waiting for COND_started_or_stopped from the manager "
                        "thread.  Current value of state is %d . "
                        "workers count=%d", state, workers_count()));
    cond_wait(COND_started_or_stopped, &LOCK_scheduler_data);
  }
  DBUG_PRINT("info", ("Manager thread has cleaned up. Set state to INIT"));
  UNLOCK_SCHEDULER_DATA();

  DBUG_RETURN(OP_OK);
}


/*
  Suspends or resumes the scheduler.
  SUSPEND - it won't execute any event till resumed.
  RESUME - it will resume if suspended.

  SYNOPSIS
    Event_scheduler::suspend_or_resume()

  RETURN VALUE
    OP_OK  OK
*/

enum Event_scheduler::enum_error_code
Event_scheduler::suspend_or_resume(
              enum Event_scheduler::enum_suspend_or_resume action)
{
  DBUG_ENTER("Event_scheduler::suspend_or_resume");
  DBUG_PRINT("enter", ("action=%d", action));

  LOCK_SCHEDULER_DATA();

  if ((action == SUSPEND && state == SUSPENDED) ||
      (action == RESUME  && state == RUNNING))
  {
    DBUG_PRINT("info", ("Either trying to suspend suspended or resume "
               "running scheduler. Doing nothing."));
  }
  else
  {
    /* Wake the main thread up if he is asleep */
    DBUG_PRINT("info", ("Sending signal"));
    if (action==SUSPEND)
    {
      state= SUSPENDED;
      pthread_cond_signal(&cond_vars[COND_new_work]);
    }
    else
    {
      state= RUNNING;
      pthread_cond_signal(&cond_vars[COND_suspend_or_resume]);
    }
    DBUG_PRINT("info", ("Waiting on COND_suspend_or_resume"));
    cond_wait(COND_suspend_or_resume, &LOCK_scheduler_data);
    DBUG_PRINT("info", ("Got response"));
  }
  UNLOCK_SCHEDULER_DATA();
  DBUG_RETURN(OP_OK);
}


/*
  Returns the number of executing events.

  SYNOPSIS
    Event_scheduler::workers_count()
*/

uint
Event_scheduler::workers_count()
{
  THD *tmp;
  uint count= 0;
  
  DBUG_ENTER("Event_scheduler::workers_count");
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      ++count;
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  DBUG_PRINT("exit", ("%d", count));
  DBUG_RETURN(count);  
}


/*
  Checks and suspends if needed

  SYNOPSIS
    Event_scheduler::check_n_suspend_if_needed()
      thd  Thread
  
  RETURN VALUE
    FALSE  Not suspended, we haven't slept
    TRUE   We were suspended. LOCK_scheduler_data is unlocked.

  NOTE
    The caller should have locked LOCK_scheduler_data!
    The mutex will be unlocked in case this function returns TRUE
*/

bool
Event_scheduler::check_n_suspend_if_needed(THD *thd)
{
  bool was_suspended= FALSE;
  DBUG_ENTER("Event_scheduler::check_n_suspend_if_needed");
  if (thd->killed && !shutdown_in_progress)
  {
    state= SUSPENDED;
    thd->killed= THD::NOT_KILLED;
  }
  if (state == SUSPENDED)
  {
    thd->enter_cond(&cond_vars[COND_suspend_or_resume], &LOCK_scheduler_data,
                    "Suspended");
    /* Send back signal to the thread that asked us to suspend operations */
    pthread_cond_signal(&cond_vars[COND_suspend_or_resume]);
    sql_print_information("SCHEDULER: Suspending operations");
    was_suspended= TRUE;
  }
  while (state == SUSPENDED)
  {
    cond_wait(COND_suspend_or_resume, &LOCK_scheduler_data);
    DBUG_PRINT("info", ("Woke up after waiting on COND_suspend_or_resume"));
    if (state != SUSPENDED)
    {
      pthread_cond_signal(&cond_vars[COND_suspend_or_resume]);
      sql_print_information("SCHEDULER: Resuming operations");
    }
  }
  if (was_suspended)
  {
    if (queue.elements)
    {
      uint i;
      DBUG_PRINT("info", ("We have to recompute the execution times"));

      for (i= 0; i < queue.elements; i++)
      {
        ((Event_timed*)queue_element(&queue, i))->compute_next_execution_time();
        ((Event_timed*)queue_element(&queue, i))->update_fields(thd);
      }
      queue_fix(&queue);
    }
    /* This will implicitly unlock LOCK_scheduler_data */
    thd->exit_cond("");
  }
  DBUG_RETURN(was_suspended);
}


/*
  Checks for empty queue and waits till new element gets in

  SYNOPSIS
    Event_scheduler::check_n_wait_for_non_empty_queue()
      thd  Thread

  RETURN VALUE
    FALSE  Did not wait - LOCK_scheduler_data still locked.
    TRUE   Waited - LOCK_scheduler_data unlocked.

  NOTE
    The caller should have locked LOCK_scheduler_data!
*/

bool
Event_scheduler::check_n_wait_for_non_empty_queue(THD *thd)
{
  bool slept= FALSE;
  DBUG_ENTER("Event_scheduler::check_n_wait_for_non_empty_queue");
  DBUG_PRINT("enter", ("q.elements=%lu state=%s",
             queue.elements, states_names[state]));
  
  if (!queue.elements)
    thd->enter_cond(&cond_vars[COND_new_work], &LOCK_scheduler_data,
                    "Empty queue, sleeping");  

  /* Wait in a loop protecting against catching spurious signals */
  while (!queue.elements && state == RUNNING)
  {
    slept= TRUE;
    DBUG_PRINT("info", ("Entering condition because of empty queue"));
    cond_wait(COND_new_work, &LOCK_scheduler_data);
    DBUG_PRINT("info", ("Manager woke up. Hope we have events now. state=%d",
               state));
    /*
      exit_cond does implicit mutex_UNLOCK, we needed it locked if
      1. we loop again
      2. end the current loop and start doing calculations
    */
  }
  if (slept)
    thd->exit_cond("");

  DBUG_PRINT("exit", ("q.elements=%lu state=%s thd->killed=%d",
             queue.elements, states_names[state], thd->killed));

  DBUG_RETURN(slept);
}


/*
  Wrapper for pthread_mutex_lock

  SYNOPSIS
    Event_scheduler::lock_data()
      mutex Mutex to lock
      line  The line number on which the lock is done

  RETURN VALUE
    Error code of pthread_mutex_lock()
*/

inline void
Event_scheduler::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::lock_mutex");
  DBUG_PRINT("enter", ("mutex_lock=%p func=%s line=%u",
             &LOCK_scheduler_data, func, line));
  pthread_mutex_lock(&LOCK_scheduler_data);
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_scheduler_data_locked= TRUE;
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_mutex_unlock

  SYNOPSIS
    Event_scheduler::unlock_data()
      mutex Mutex to unlock
      line  The line number on which the unlock is done
*/

inline void
Event_scheduler::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::UNLOCK_mutex");
  DBUG_PRINT("enter", ("mutex_unlock=%p func=%s line=%u",
             &LOCK_scheduler_data, func, line));
  mutex_last_unlocked_at_line= line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  pthread_mutex_unlock(&LOCK_scheduler_data);
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_cond_wait

  SYNOPSIS
    Event_scheduler::cond_wait()
      cond   Conditional to wait for
      mutex  Mutex of the conditional

  RETURN VALUE
    Error code of pthread_cond_wait()
*/

inline int
Event_scheduler::cond_wait(enum Event_scheduler::enum_cond_vars cond,
                           pthread_mutex_t *mutex)
{
  int ret;
  DBUG_ENTER("Event_scheduler::cond_wait");
  DBUG_PRINT("enter", ("cond=%s mutex=%p", cond_vars_names[cond], mutex));
  ret= pthread_cond_wait(&cond_vars[cond_waiting_on=cond], mutex);
  cond_waiting_on= COND_NONE;
  DBUG_RETURN(ret);
}


/*
  Checks whether the scheduler is in a running or suspended state.

  SYNOPSIS
    Event_scheduler::is_running_or_suspended()

  RETURN VALUE
    TRUE   Either running or suspended
    FALSE  IN_SHUTDOWN, not started, etc.
*/

inline bool
Event_scheduler::is_running_or_suspended()
{
  return (state == SUSPENDED || state == RUNNING);
}


/*
  Returns the current state of the scheduler

  SYNOPSIS
    Event_scheduler::get_state()
*/

enum Event_scheduler::enum_state
Event_scheduler::get_state()
{
  enum Event_scheduler::enum_state ret;
  DBUG_ENTER("Event_scheduler::get_state");
  /* lock_data & unlock_data are not static */
  pthread_mutex_lock(&singleton.LOCK_scheduler_data);
  ret= singleton.state;
  pthread_mutex_unlock(&singleton.LOCK_scheduler_data);
  DBUG_RETURN(ret);
}


/*
  Returns whether the scheduler was initialized.

  SYNOPSIS
    Event_scheduler::initialized()
  
  RETURN VALUE
    FALSE  Was not initialized so far
    TRUE   Was initialized
*/

bool
Event_scheduler::initialized()
{
  DBUG_ENTER("Event_scheduler::initialized");
  DBUG_RETURN(Event_scheduler::get_state() != UNINITIALIZED);
}


/*
  Returns the number of elements in the queue

  SYNOPSIS
    Event_scheduler::events_count()

  RETURN VALUE
    0  Number of Event_timed objects in the queue
*/

uint
Event_scheduler::events_count()
{
  uint n;
  DBUG_ENTER("Event_scheduler::events_count");
  LOCK_SCHEDULER_DATA();
  n= queue.elements;
  UNLOCK_SCHEDULER_DATA();

  DBUG_RETURN(n);
}


/*
  Looks for a named event in mysql.event and then loads it from
  the table, compiles and inserts it into the cache.

  SYNOPSIS
    Event_scheduler::load_named_event()
      thd      THD
      etn      The name of the event to load and compile on scheduler's root
      etn_new  The loaded event

  RETURN VALUE
    NULL       Error during compile or the event is non-enabled.
    otherwise  Address
*/

enum Event_scheduler::enum_error_code
Event_scheduler::load_named_event(THD *thd, Event_timed *etn, Event_timed **etn_new)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root;
  Event_timed *et_loaded= NULL;
  Open_tables_state backup;

  DBUG_ENTER("Event_scheduler::load_and_compile_event");
  DBUG_PRINT("enter",("thd=%p name:%*s",thd, etn->name.length, etn->name.str));

  thd->reset_n_backup_open_tables_state(&backup);
  /* No need to use my_error() here because db_find_event() has done it */
  {
    sp_name spn(etn->dbname, etn->name);
    ret= db_find_event(thd, &spn, &et_loaded, NULL, &scheduler_root);
  }
  thd->restore_backup_open_tables_state(&backup);
  /* In this case no memory was allocated so we don't need to clean */
  if (ret)
    DBUG_RETURN(OP_LOAD_ERROR);

  if (et_loaded->status != Event_timed::ENABLED)
  {
    /*
      We don't load non-enabled events.
      In db_find_event() `et_new` was allocated on the heap and not on
      scheduler_root therefore we delete it here.
    */
    delete et_loaded;
    DBUG_RETURN(OP_DISABLED_EVENT);
  }

  et_loaded->compute_next_execution_time();
  *etn_new= et_loaded;

  DBUG_RETURN(OP_OK);
}


/*
   Loads all ENABLED events from mysql.event into the prioritized
   queue. Called during scheduler main thread initialization. Compiles
   the events. Creates Event_timed instances for every ENABLED event
   from mysql.event.

   SYNOPSIS
     Event_scheduler::load_events_from_db()
       thd - Thread context. Used for memory allocation in some cases.
     
   RETURN VALUE
     0  OK
    !0  Error (EVEX_OPEN_TABLE_FAILED, EVEX_MICROSECOND_UNSUP, 
               EVEX_COMPILE_ERROR) - in all these cases mysql.event was
               tampered.

   NOTES
     Reports the error to the console
*/

int
Event_scheduler::load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  int ret= -1;
  uint count= 0;
  bool clean_the_queue= FALSE;
  /* Compile the events on this root but only for syntax check, then discard */
  MEM_ROOT boot_root;

  DBUG_ENTER("Event_scheduler::load_events_from_db");
  DBUG_PRINT("enter", ("thd=%p", thd));

  if (state > COMMENCING)
  {
    DBUG_ASSERT(0);
    sql_print_error("SCHEDULER: Trying to load events while already running.");
    DBUG_RETURN(EVEX_GENERAL_ERROR);
  }

  if ((ret= Events::open_event_table(thd, TL_READ, &table)))
  {
    sql_print_error("SCHEDULER: Table mysql.event is damaged. Can not open.");
    DBUG_RETURN(EVEX_OPEN_TABLE_FAILED);
  }

  init_alloc_root(&boot_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_timed *et;
    if (!(et= new Event_timed))
    {
      DBUG_PRINT("info", ("Out of memory"));
      clean_the_queue= TRUE;
      break;
    }
    DBUG_PRINT("info", ("Loading event from row."));

    if ((ret= et->load_from_row(&scheduler_root, table)))
    {
      clean_the_queue= TRUE;
      sql_print_error("SCHEDULER: Error while loading from mysql.event. "
                      "Table probably corrupted");
      break;
    }
    if (et->status != Event_timed::ENABLED)
    {
      DBUG_PRINT("info",("%s is disabled",et->name.str));
      delete et;
      continue;
    }

    DBUG_PRINT("info", ("Event %s loaded from row. ", et->name.str));

    /* We load only on scheduler root just to check whether the body compiles */
    switch (ret= et->compile(thd, &boot_root)) {
    case EVEX_MICROSECOND_UNSUP:
      et->free_sp();
      sql_print_error("SCHEDULER: mysql.event is tampered. MICROSECOND is not "
                      "supported but found in mysql.event");
      goto end;
    case EVEX_COMPILE_ERROR:
      sql_print_error("SCHEDULER: Error while compiling %s.%s. Aborting load.",
                      et->dbname.str, et->name.str);
      goto end;
    default:
      /* Free it, it will be compiled again on the worker thread */
      et->free_sp();
      break;
    }

    /* let's find when to be executed */
    if (et->compute_next_execution_time())
    {
      sql_print_error("SCHEDULER: Error while computing execution time of %s.%s."
                      " Skipping", et->dbname.str, et->name.str);
      continue;
    }

    DBUG_PRINT("load_events_from_db", ("Adding %p to the exec list."));
    queue_insert_safe(&queue,  (byte *) et);
    count++;
  }
end:
  end_read_record(&read_record_info);
  free_root(&boot_root, MYF(0));

  if (clean_the_queue)
  {
    for (count= 0; count < queue.elements; ++count)
      queue_remove(&queue, 0);
    ret= -1;
  }
  else
  {
    ret= 0;
    sql_print_information("SCHEDULER: Loaded %d event%s", count, (count == 1)?"":"s");
  }

  /* Force close to free memory */
  thd->version--;  

  close_thread_tables(thd);

  DBUG_PRINT("info", ("Status code %d. Loaded %d event(s)", ret, count));
  DBUG_RETURN(ret);
}


/*
  Opens mysql.db and mysql.user and checks whether:
    1. mysql.db has column Event_priv at column 20 (0 based);
    2. mysql.user has column Event_priv at column 29 (0 based);

  SYNOPSIS
    Event_scheduler::check_system_tables()
*/

bool
Event_scheduler::check_system_tables(THD *thd)
{
  TABLE_LIST tables;
  bool not_used;
  Open_tables_state backup;
  bool ret;

  DBUG_ENTER("Event_scheduler::check_system_tables");
  DBUG_PRINT("enter", ("thd=%p", thd));

  thd->reset_n_backup_open_tables_state(&backup);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "db";
  tables.lock_type= TL_READ;

  if ((ret= simple_open_n_lock_tables(thd, &tables)))
    sql_print_error("Cannot open mysql.db");
  else
  {
    ret= table_check_intact(tables.table, MYSQL_DB_FIELD_COUNT,
                           mysql_db_table_fields, &mysql_db_table_last_check,
                           ER_CANNOT_LOAD_FROM_TABLE);
    close_thread_tables(thd);
  }
  if (ret)
    DBUG_RETURN(TRUE);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "user";
  tables.lock_type= TL_READ;

  if ((ret= simple_open_n_lock_tables(thd, &tables)))
    sql_print_error("Cannot open mysql.db");
  else
  {
    if (tables.table->s->fields < 29 ||
        strncmp(tables.table->field[29]->field_name,
                STRING_WITH_LEN("Event_priv")))
    {
      sql_print_error("mysql.user has no `Event_priv` column at position 29");
      ret= TRUE;
    }
    close_thread_tables(thd);
  }

  thd->restore_backup_open_tables_state(&backup);

  DBUG_RETURN(ret);
}


/*
  Inits mutexes.

  SYNOPSIS
    Event_scheduler::init_mutexes()
*/

void
Event_scheduler::init_mutexes()
{
  pthread_mutex_init(&singleton.LOCK_scheduler_data, MY_MUTEX_INIT_FAST);
}


/*
  Destroys mutexes.

  SYNOPSIS
    Event_scheduler::destroy_mutexes()
*/

void
Event_scheduler::destroy_mutexes()
{
  pthread_mutex_destroy(&singleton.LOCK_scheduler_data);
}


/*
  Dumps some data about the internal status of the scheduler.

  SYNOPSIS
    Event_scheduler::dump_internal_status()
      thd      THD

  RETURN VALUE
    0  OK
    1  Error
*/

int
Event_scheduler::dump_internal_status(THD *thd)
{
  DBUG_ENTER("dump_internal_status");
#ifndef DBUG_OFF
  CHARSET_INFO *scs= system_charset_info;
  Protocol *protocol= thd->protocol;
  List<Item> field_list;
  int ret;
  char tmp_buff[5*STRING_BUFFER_USUAL_SIZE];
  char int_buff[STRING_BUFFER_USUAL_SIZE];
  String tmp_string(tmp_buff, sizeof(tmp_buff), scs);
  String int_string(int_buff, sizeof(int_buff), scs);
  tmp_string.length(0);
  int_string.length(0);

  field_list.push_back(new Item_empty_string("Name", 20));
  field_list.push_back(new Item_empty_string("Value",20));
  if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
    DBUG_RETURN(1);

  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("state"), scs);
  protocol->store(states_names[singleton.state].str,
                  states_names[singleton.state].length,
                  scs);

  ret= protocol->write();
  /*
    If not initialized - don't show anything else. get_instance()
    will otherwise implicitly initialize it. We don't want that.
  */
  if (singleton.state >= INITIALIZED)
  {
    /* last locked at*/
    /* 
      The first thing to do, or get_instance() will overwrite the values.
      mutex_last_locked_at_line / mutex_last_unlocked_at_line
    */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("last locked at"), scs);
    tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                       tmp_string.alloced_length(), "%s::%d",
                                       singleton.mutex_last_locked_in_func,
                                       singleton.mutex_last_locked_at_line));
    protocol->store(&tmp_string);
    ret= protocol->write();

    /* last unlocked at*/
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("last unlocked at"), scs);
    tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                       tmp_string.alloced_length(), "%s::%d",
                                       singleton.mutex_last_unlocked_in_func,
                                       singleton.mutex_last_unlocked_at_line));
    protocol->store(&tmp_string);
    ret= protocol->write();

    /* waiting on */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("waiting on condition"), scs);
    tmp_string.length(scs->cset->
                    snprintf(scs, (char*) tmp_string.ptr(),
                             tmp_string.alloced_length(), "%s",
                             (singleton.cond_waiting_on != COND_NONE) ?
                               cond_vars_names[singleton.cond_waiting_on]:
                               "NONE"));
    protocol->store(&tmp_string);
    ret= protocol->write();

    Event_scheduler *scheduler= get_instance();

    /* workers_count */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("workers_count"), scs);
    int_string.set((longlong) scheduler->workers_count(), scs);
    protocol->store(&int_string);
    ret= protocol->write();

    /* queue.elements */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("queue.elements"), scs);
    int_string.set((longlong) scheduler->queue.elements, scs);
    protocol->store(&int_string);
    ret= protocol->write();

    /* scheduler_data_locked */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler data locked"), scs);
    int_string.set((longlong) scheduler->mutex_scheduler_data_locked, scs);
    protocol->store(&int_string);
    ret= protocol->write();
  }
  send_eof(thd);
#endif
  DBUG_RETURN(0);
}
