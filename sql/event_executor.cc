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
#include "event.h"
#include "event_priv.h"
#include "sp.h"

#define DBUG_FAULTY_THR2

extern  ulong thread_created;


pthread_mutex_t LOCK_event_arrays,
                LOCK_workers_count,
                LOCK_evex_running;


bool evex_is_running= false;

ulong opt_event_executor;
my_bool event_executor_running_global_var= false;
static my_bool evex_mutexes_initted= false;
static uint workers_count;

static int
evex_load_events_from_db(THD *thd);



/*
  TODO Andrey: Check for command line option whether to start
               the main thread or not.
*/

pthread_handler_t
event_executor_worker(void *arg);

pthread_handler_t
event_executor_main(void *arg);

static
void evex_init_mutexes()
{
  if (evex_mutexes_initted)
  {
    evex_mutexes_initted= true;
    return;
  }
  pthread_mutex_init(&LOCK_event_arrays, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_workers_count, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_evex_running, MY_MUTEX_INIT_FAST);
}

int
init_events()
{
  pthread_t th;

  DBUG_ENTER("init_events");

  DBUG_PRINT("info",("Starting events main thread"));

  evex_init_mutexes();
  
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  event_executor_running_global_var= false;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

#ifndef DBUG_FAULTY_THR
  //TODO Andrey: Change the error code returned!
  if (pthread_create(&th, NULL, event_executor_main, (void*)NULL))
    DBUG_RETURN(ER_SLAVE_THREAD);
#else
  event_executor_main(NULL);
#endif

  DBUG_RETURN(0);
}


void
shutdown_events()
{
  DBUG_ENTER("shutdown_events");
  
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  pthread_mutex_destroy(&LOCK_event_arrays);
  pthread_mutex_destroy(&LOCK_workers_count);
  pthread_mutex_destroy(&LOCK_evex_running);
  
  DBUG_VOID_RETURN;
}


static int
init_event_thread(THD* thd)
{
  DBUG_ENTER("init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->skip_grants();
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->slave_thread= 0;
  thd->options= OPTION_AUTO_IS_NULL;
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

pthread_handler_t
event_executor_main(void *arg)
{
  THD *thd;			/* needs to be first for thread_stack */
  ulonglong iter_num= 0;
  uint i=0, j=0;

  DBUG_ENTER("event_executor_main");
  DBUG_PRINT("event_executor_main", ("EVEX thread started"));    


  // init memory root
  init_alloc_root(&evex_mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  
  //TODO Andrey: Check for NULL
  if (!(thd = new THD)) // note that contructor of THD uses DBUG_ !
  {
    sql_print_error("Cannot create THD for event_executor_main");
    goto err_no_thd;
  }    
  thd->thread_stack = (char*)&thd; // remember where our stack is
  
  pthread_detach_this_thread();

  if (init_event_thread(thd))
    goto err;

  // make this thread invisible it has no vio -> show processlist won't see
  thd->system_thread= 1;

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  DBUG_PRINT("EVEX main thread", ("Initing events_array"));

  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  /*
    my_malloc is used as underlying allocator which does not use a mem_root
    thus data should be freed at later stage.
  */
  VOID(my_init_dynamic_array(&events_array, sizeof(event_timed), 50, 100));
  VOID(my_init_dynamic_array(&evex_executing_queue, sizeof(event_timed *), 50, 100));
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));  

  /*
    eventually manifest that we are running, not to crashe because of
    usage of non-initialized memory structures.
  */
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= true;  
  event_executor_running_global_var= opt_event_executor;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  if (evex_load_events_from_db(thd))
   goto err;

  THD_CHECK_SENTRY(thd);
  /* Read queries from the IO/THREAD until this thread is killed */
  while (!thd->killed)
  {
    TIME time_now;
    my_time_t now;
    my_ulonglong cnt;
    
    DBUG_PRINT("info", ("EVEX External Loop %d", ++cnt));
    thd->proc_info = "Sleeping";
    my_sleep(1000000);// sleep 1s
    if (!event_executor_running_global_var)
      continue;
    time(&now);
    my_tz_UTC->gmt_sec_to_TIME(&time_now, now);

    VOID(pthread_mutex_lock(&LOCK_event_arrays));
    for (i= 0; (i < evex_executing_queue.elements) && !thd->killed; ++i)
    {
      event_timed *et= *dynamic_element(&evex_executing_queue,i,event_timed**);
//      printf("%llu\n", TIME_to_ulonglong_datetime(&et->m_execute_at));
      if (!event_executor_running_global_var)
        break;

      thd->proc_info = "Iterating";
      THD_CHECK_SENTRY(thd);
      /*
        if this is the first event which is after time_now then no
        more need to iterate over more elements since the array is sorted.
      */ 
      if (et->m_execute_at.year > 1969 &&
          my_time_compare(&time_now, &et->m_execute_at) == -1)
        break;
      
      if (et->m_status == MYSQL_EVENT_ENABLED && 
          !check_access(thd, EVENT_ACL, et->m_db.str, 0, 0, 0,
                        is_schema_db(et->m_db.str)))
      {
        pthread_t th;

        DBUG_PRINT("info", ("  Spawning a thread %d", ++iter_num));
        thd->proc_info = "Starting new thread";
        sql_print_information("  Spawning a thread %d", ++iter_num);
#ifndef DBUG_FAULTY_THR
        if (pthread_create(&th, NULL, event_executor_worker, (void*)et))
        {
          sql_print_error("Problem while trying to create a thread");
          UNLOCK_MUTEX_AND_BAIL_OUT(LOCK_event_arrays, err);
        }
#else
        event_executor_worker((void *) et);
#endif
        et->mark_last_executed();
        thd->proc_info = "Computing next time";
        et->compute_next_execution_time();
        et->update_fields(thd);
        if ((et->m_execute_at.year && !et->m_expr)
            || TIME_to_ulonglong_datetime(&et->m_execute_at) == 0L)
          et->m_flags |= EVENT_EXEC_NO_MORE;
      }
    }
    /*
      Let's remove elements which won't be executed any more
      The number is "i" and it is <= up to evex_executing_queue.elements
    */
    j= 0;
    while (j < i && j < evex_executing_queue.elements)
    {
      event_timed *et= *dynamic_element(&evex_executing_queue, j, event_timed**);
      if (et->m_flags & EVENT_EXEC_NO_MORE || et->m_status == MYSQL_EVENT_DISABLED)
      {
        delete_dynamic_element(&evex_executing_queue, j);
        DBUG_PRINT("EVEX main thread", ("DELETING FROM EXECUTION QUEUE [%s.%s]", 
                                         et->m_db.str, et->m_name.str));
        // nulling the position, will delete later
        if (et->m_dropped)
        {
          // we have to drop the event
          int idx;
          et->drop(thd);
          idx= get_index_dynamic(&events_array, (gptr) et);
          DBUG_ASSERT(idx != -1);
          delete_dynamic_element(&events_array, idx);
        }
        continue;
      }
      ++j;
    }
    if (evex_executing_queue.elements)
      //ToDo Andrey : put a lock here
      qsort((gptr) dynamic_element(&evex_executing_queue, 0, event_timed**),
                                   evex_executing_queue.elements,
                                   sizeof(event_timed **),
                                   (qsort_cmp) event_timed_compare
                                 );

    VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  }

err:
  // First manifest that this thread does not work and then destroy
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  sql_print_information("Event executor stopping");

  /*
    TODO: A better will be with a conditional variable
  */
  /* 
    Read workers_count without lock, no need for locking.
    In the worst case we have to wait 1sec more.
  */
  while (workers_count)
    my_sleep(1000000);// 1s

  /*
    LEX_STRINGs reside in the memory root and will be destroyed with it.
    Hence no need of delete but only freeing of SP
  */
  for (i= 0; i < events_array.elements; ++i)
    dynamic_element(&events_array, i, event_timed*)->free_sp();

  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  // No need to use lock here if EVEX is not running but anyway
  delete_dynamic(&evex_executing_queue);
  delete_dynamic(&events_array);
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  
  thd->proc_info = "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  THD_CHECK_SENTRY(thd);

  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  thread_running--;
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);


err_no_thd:
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  free_root(&evex_mem_root, MYF(0));
  sql_print_information("Event executor stopped");

  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);// Can't return anything here
}


pthread_handler_t
event_executor_worker(void *event_void)
{
  THD *thd; /* needs to be first for thread_stack */
  event_timed *event = (event_timed *) event_void;
  MEM_ROOT worker_mem_root;

  DBUG_ENTER("event_executor_worker");
  VOID(pthread_mutex_lock(&LOCK_workers_count));
  ++workers_count;  
  VOID(pthread_mutex_unlock(&LOCK_workers_count));

  init_alloc_root(&worker_mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  my_thread_init();

  if (!(thd = new THD)) // note that contructor of THD uses DBUG_ !
  {
    sql_print_error("Cannot create a THD structure in worker thread");
    goto err_no_thd;
  }
  thd->thread_stack = (char*)&thd; // remember where our stack is
  thd->mem_root= &worker_mem_root;

  pthread_detach(pthread_self());

  if (init_event_thread(thd))
    goto err;

  thd->init_for_queries();

  // make this thread visible it has no vio -> show processlist needs this flag
  thd->system_thread= 1;

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  // thd->security_ctx->priv_host is char[MAX_HOSTNAME]
  
  strxnmov(thd->security_ctx->priv_host, sizeof(thd->security_ctx->priv_host),
                event->m_definer_host.str, NullS);  

  thd->security_ctx->priv_user= event->m_definer_user.str;

  thd->db= event->m_db.str;
  {
    char exec_time[200];
    int ret;
    my_TIME_to_str(&event->m_execute_at, exec_time);
    DBUG_PRINT("info", ("    EVEX EXECUTING event for event %s.%s [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time));
    sql_print_information("    EVEX EXECUTING event for event %s.%s [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time);
    ret= event->execute(thd, &worker_mem_root);
    sql_print_information("    EVEX EXECUTED event for event %s.%s  [EXPR:%d][EXECUTE_AT:%s]. RetCode=%d", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time, ret); 
    DBUG_PRINT("info", ("    EVEX EXECUTED event for event %s.%s  [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time)); 
  }
  thd->db= 0;

err:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
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
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

err_no_thd:

  free_root(&worker_mem_root, MYF(0));
  
  VOID(pthread_mutex_lock(&LOCK_workers_count));
  --workers_count;  
  VOID(pthread_mutex_unlock(&LOCK_workers_count));

#ifndef DBUG_FAULTY_THR
  my_thread_end();
  pthread_exit(0);
#endif
  DBUG_RETURN(0); // Can't return anything here
}


static int
evex_load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  MYSQL_LOCK *lock;
  int ret= -1;
  
  DBUG_ENTER("evex_load_events_from_db");  

  if (!(table= evex_open_event_table(thd, TL_READ)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  VOID(pthread_mutex_lock(&LOCK_event_arrays));

  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    event_timed *et, *et_copy;
    if (!(et= new event_timed()))
    {
      DBUG_PRINT("evex_load_events_from_db", ("Out of memory"));
      ret= -1;
      goto end;
    }
    DBUG_PRINT("evex_load_events_from_db", ("Loading event from row."));
    
    if ((ret= et->load_from_row(&evex_mem_root, table)))
    {
      sql_print_error("Error while loading from mysql.event. "
                      "Table probably corrupted");
      goto end;
    }
    
    DBUG_PRINT("evex_load_events_from_db",
            ("Event %s loaded from row. Time to compile", et->m_name.str));
    
    if ((ret= et->compile(thd, &evex_mem_root)))
    {
      sql_print_error("Error while compiling %s.%s. Aborting load.",
                      et->m_db.str, et->m_name.str);
      goto end;
    }
    // let's find when to be executed  
    et->compute_next_execution_time();
    
    DBUG_PRINT("evex_load_events_from_db",
                ("Adding %s to the executor list.", et->m_name.str));
    VOID(push_dynamic(&events_array,(gptr) et));
    /*
      We always add at the end so the number of elements - 1 is the place
      in the buffer.
      DYNAMIC_ARRAY copies the object bit by bit so we have a hollow copy
      in event_array. We don't need the original therefore we delete it.
    */
    et_copy= dynamic_element(&events_array, events_array.elements - 1,
                             event_timed*);
    VOID(push_dynamic(&evex_executing_queue,(gptr) &et_copy));
    et->m_free_sphead_on_delete= false;
    delete et; 
  }
  end_read_record(&read_record_info);

  qsort((gptr) dynamic_element(&evex_executing_queue, 0, event_timed**),
                               evex_executing_queue.elements,
                               sizeof(event_timed **),
                               (qsort_cmp) event_timed_compare
                              );
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));

  thd->version--;  // Force close to free memory
  ret= 0;

end:
  close_thread_tables(thd);

  DBUG_PRINT("info", ("Finishing with status code %d", ret));
  DBUG_RETURN(ret);
}



bool sys_var_event_executor::update(THD *thd, set_var *var)
{
  // here start the thread if not running.
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if ((my_bool) var->save_result.ulong_value && !evex_is_running)
  {
    VOID(pthread_mutex_unlock(&LOCK_evex_running));
    init_events();
  } else 
    VOID(pthread_mutex_unlock(&LOCK_evex_running));

  return sys_var_bool_ptr::update(thd, var);
}

