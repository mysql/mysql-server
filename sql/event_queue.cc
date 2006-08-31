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
#include "event_queue.h"
#include "event_data_objects.h"
#include "event_db_repository.h"


#define EVENT_QUEUE_INITIAL_SIZE 30
#define EVENT_QUEUE_EXTENT       30

#ifdef __GNUC__
#if __GNUC__ >= 2
#define SCHED_FUNC __FUNCTION__
#endif
#else
#define SCHED_FUNC "<unknown>"
#endif

#define LOCK_QUEUE_DATA()   lock_data(SCHED_FUNC, __LINE__)
#define UNLOCK_QUEUE_DATA() unlock_data(SCHED_FUNC, __LINE__)

struct event_queue_param
{
  THD *thd;
  Event_queue *queue;
  pthread_mutex_t LOCK_loaded;
  pthread_cond_t COND_loaded;
  bool loading_finished;
};


/*
  Compares the execute_at members of two Event_queue_element instances.
  Used as callback for the prioritized queue when shifting
  elements inside.

  SYNOPSIS
    event_queue_element_data_compare_q()
      vptr  Not used (set it to NULL)
      a     First Event_queue_element object
      b     Second Event_queue_element object

  RETURN VALUE
   -1   a->execute_at < b->execute_at
    0   a->execute_at == b->execute_at
    1   a->execute_at > b->execute_at

  NOTES
    execute_at.second_part is not considered during comparison
*/

static int 
event_queue_element_compare_q(void *vptr, byte* a, byte *b)
{
  return my_time_compare(&((Event_queue_element *)a)->execute_at,
                         &((Event_queue_element *)b)->execute_at);
}


/*
  Constructor of class Event_queue.

  SYNOPSIS
    Event_queue::Event_queue()
*/

Event_queue::Event_queue()
  :mutex_last_unlocked_at_line(0), mutex_last_locked_at_line(0),
   mutex_last_attempted_lock_at_line(0),
   mutex_queue_data_locked(FALSE), mutex_queue_data_attempting_lock(FALSE)
{
  mutex_last_unlocked_in_func= mutex_last_locked_in_func=
    mutex_last_attempted_lock_in_func= "";
  set_zero_time(&next_activation_at, MYSQL_TIMESTAMP_DATETIME);
}


/*
  Inits mutexes.

  SYNOPSIS
    Event_queue::init_mutexes()
*/

void
Event_queue::init_mutexes()
{
  pthread_mutex_init(&LOCK_event_queue, MY_MUTEX_INIT_FAST);
}


/*
  Destroys mutexes.

  SYNOPSIS
    Event_queue::deinit_mutexes()
*/

void
Event_queue::deinit_mutexes()
{
  pthread_mutex_destroy(&LOCK_event_queue);
}


/*
  This is a queue's constructor. Until this method is called, the
  queue is unusable.  We don't use a C++ constructor instead in
  order to be able to check the return value. The queue is
  initialized once at server startup.  Initialization can fail in
  case of a failure reading events from the database or out of
  memory.

  SYNOPSIS
    Event_queue::init()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_queue::init_queue(THD *thd, Event_db_repository *db_repo)
{
  pthread_t th;
  bool res;
  struct event_queue_param *event_queue_param_value= NULL;

  DBUG_ENTER("Event_queue::init_queue");
  DBUG_PRINT("enter", ("this=0x%lx", this));

  LOCK_QUEUE_DATA();
  db_repository= db_repo;

  if (init_queue_ex(&queue, EVENT_QUEUE_INITIAL_SIZE , 0 /*offset*/,
                    0 /*max_on_top*/, event_queue_element_compare_q,
                    NULL, EVENT_QUEUE_EXTENT))
  {
    sql_print_error("SCHEDULER: Can't initialize the execution queue");
    goto err;
  }

  if (sizeof(my_time_t) != sizeof(time_t))
  {
    sql_print_error("SCHEDULER: sizeof(my_time_t) != sizeof(time_t) ."
                    "The scheduler may not work correctly. Stopping");
    DBUG_ASSERT(0);
    goto err;
  }

  res= load_events_from_db(thd);
  UNLOCK_QUEUE_DATA();
  if (res)
    deinit_queue();

  DBUG_RETURN(res);

err:
  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(TRUE);
}


/*
  Deinits the queue. Remove all elements from it and destroys them
  too.

  SYNOPSIS
    Event_queue::deinit_queue()
*/

void
Event_queue::deinit_queue()
{
  DBUG_ENTER("Event_queue::deinit_queue");

  LOCK_QUEUE_DATA();
  empty_queue();
  delete_queue(&queue);
  UNLOCK_QUEUE_DATA();

  DBUG_VOID_RETURN;
}


/*
  Adds an event to the queue.

  SYNOPSIS
    Event_queue::create_event()
      dbname  The schema of the new event
      name    The name of the new event

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
*/

int
Event_queue::create_event(THD *thd, LEX_STRING dbname, LEX_STRING name)
{
  int res;
  Event_queue_element *new_element;
  DBUG_ENTER("Event_queue::create_event");
  DBUG_PRINT("enter", ("thd=0x%lx et=%s.%s",thd, dbname.str, name.str));

  new_element= new Event_queue_element();
  res= db_repository->load_named_event(thd, dbname, name, new_element);
  if (res || new_element->status == Event_queue_element::DISABLED)
    delete new_element;
  else
  {
    new_element->compute_next_execution_time();

    LOCK_QUEUE_DATA();
    DBUG_PRINT("info", ("new event in the queue 0x%lx", new_element));
    queue_insert_safe(&queue, (byte *) new_element);
    dbug_dump_queue(thd->query_start());
    pthread_cond_broadcast(&COND_queue_state);  
    UNLOCK_QUEUE_DATA();
  }

  DBUG_RETURN(res);
}


/*
  Updates an event from the scheduler queue

  SYNOPSIS
    Event_queue::update_event()
      thd        Thread
      dbname     Schema of the event
      name       Name of the event
      new_schema New schema, in case of RENAME TO, otherwise NULL
      new_name   New name, in case of RENAME TO, otherwise NULL

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
*/

int
Event_queue::update_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
                          LEX_STRING *new_schema, LEX_STRING *new_name)
{
  int res;
  Event_queue_element *new_element;

  DBUG_ENTER("Event_queue::update_event");
  DBUG_PRINT("enter", ("thd=0x%lx et=[%s.%s]", thd, dbname.str, name.str));

  new_element= new Event_queue_element();

  res= db_repository->load_named_event(thd, new_schema ? *new_schema:dbname,
                                       new_name ? *new_name:name, new_element);
  if (res)
  {
    delete new_element;
    goto end;
  }
  else if (new_element->status == Event_queue_element::DISABLED)
  {
    DBUG_PRINT("info", ("The event is disabled."));
    /*
      Destroy the object but don't skip to end: because we may have to remove
      object from the cache.
    */
    delete new_element;
    new_element= NULL;
  }
  else
    new_element->compute_next_execution_time();

  LOCK_QUEUE_DATA();
  find_n_remove_event(dbname, name);

  /* If not disabled event */
  if (new_element)
  {
    DBUG_PRINT("info", ("new event in the Q 0x%lx", new_element));
    queue_insert_safe(&queue, (byte *) new_element);
    pthread_cond_broadcast(&COND_queue_state);  
  }

  dbug_dump_queue(thd->query_start());
  UNLOCK_QUEUE_DATA();

end:
  DBUG_PRINT("info", ("res=%d", res));
  DBUG_RETURN(res);
}


/*
  Drops an event from the queue

  SYNOPSIS
    Event_queue::drop_event()
      thd     Thread
      dbname  Schema of the event to drop
      name    Name of the event to drop
*/

void
Event_queue::drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name)
{
  int res;
  DBUG_ENTER("Event_queue::drop_event");
  DBUG_PRINT("enter", ("thd=0x%lx name=0x%lx", thd, name));

  LOCK_QUEUE_DATA();
  find_n_remove_event(dbname, name);
  dbug_dump_queue(thd->query_start());
  UNLOCK_QUEUE_DATA();
  
  /*
    We don't signal here because the scheduler will catch the change
    next time it wakes up.
  */

  DBUG_VOID_RETURN;
}


/*
  Drops all events from the in-memory queue and disk that match
  certain pattern evaluated by a comparator function

  SYNOPSIS
    Event_queue::drop_matching_events()
      thd            THD
      pattern        A pattern string
      comparator     The function to use for comparing

  RETURN VALUE
    >=0  Number of dropped events
    
  NOTE
    Expected is the caller to acquire lock on LOCK_event_queue
*/

void
Event_queue::drop_matching_events(THD *thd, LEX_STRING pattern,
                           bool (*comparator)(LEX_STRING, Event_basic *))
{
  uint i= 0;
  DBUG_ENTER("Event_queue::drop_matching_events");
  DBUG_PRINT("enter", ("pattern=%s", pattern.str));

  while (i < queue.elements)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]?", et->dbname.str, et->name.str));
    if (comparator(pattern, et))
    {
      /*
        The queue is ordered. If we remove an element, then all elements
        after it will shift one position to the left, if we imagine it as
        an array from left to the right. In this case we should not
        increment the counter and the (i < queue.elements) condition is ok.
      */
      queue_remove(&queue, i);
      delete et;
    }
    else
      i++;
  }
  /*
    We don't call pthread_cond_broadcast(&COND_queue_state);  
    If we remove the top event:
    1. The queue is empty. The scheduler will wake up at some time and
       realize that the queue is empty. If create_event() comes inbetween
       it will signal the scheduler
    2. The queue is not empty, but the next event after the previous top,
       won't be executed any time sooner than the element we removed. Hence,
       we may not notify the scheduler and it will realize the change when it
       wakes up from timedwait.
  */

  DBUG_VOID_RETURN;
}


/*
  Drops all events from the in-memory queue and disk that are from
  certain schema.

  SYNOPSIS
    Event_queue::drop_schema_events()
      thd        HD
      schema    The schema name
*/

void
Event_queue::drop_schema_events(THD *thd, LEX_STRING schema)
{
  DBUG_ENTER("Event_queue::drop_schema_events");
  LOCK_QUEUE_DATA();
  drop_matching_events(thd, schema, event_basic_db_equal);
  UNLOCK_QUEUE_DATA();
  DBUG_VOID_RETURN;
}


/*
  Searches for an event in the queue

  SYNOPSIS
    Event_queue::find_n_remove_event()
      db    The schema of the event to find
      name  The event to find

  NOTE
    The caller should do the locking also the caller is responsible for
    actual signalling in case an event is removed from the queue.
*/

void
Event_queue::find_n_remove_event(LEX_STRING db, LEX_STRING name)
{
  uint i;
  DBUG_ENTER("Event_queue::find_n_remove_event");

  for (i= 0; i < queue.elements; ++i)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?", db.str, name.str,
                        et->dbname.str, et->name.str));
    if (event_basic_identifier_equal(db, name, et))
    {
      queue_remove(&queue, i);
      delete et;
      break;
    }
  }

  DBUG_VOID_RETURN;
}


/*
  Loads all ENABLED events from mysql.event into the prioritized
  queue. Called during scheduler main thread initialization. Compiles
  the events. Creates Event_queue_element instances for every ENABLED event
  from mysql.event.

  SYNOPSIS
    Event_queue::load_events_from_db()
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
Event_queue::load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  int ret= -1;
  uint count= 0;
  bool clean_the_queue= TRUE;
  /* Compile the events on this root but only for syntax check, then discard */
  MEM_ROOT boot_root;

  DBUG_ENTER("Event_queue::load_events_from_db");
  DBUG_PRINT("enter", ("thd=0x%lx", thd));

  if ((ret= db_repository->open_event_table(thd, TL_READ, &table)))
  {
    sql_print_error("SCHEDULER: Table mysql.event is damaged. Can not open");
    DBUG_RETURN(EVEX_OPEN_TABLE_FAILED);
  }

  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_queue_element *et;
    if (!(et= new Event_queue_element))
    {
      DBUG_PRINT("info", ("Out of memory"));
      break;
    }
    DBUG_PRINT("info", ("Loading event from row."));

    if ((ret= et->load_from_row(table)))
    {
      sql_print_error("SCHEDULER: Error while loading from mysql.event. "
                      "Table probably corrupted");
      break;
    }
    if (et->status != Event_queue_element::ENABLED)
    {
      DBUG_PRINT("info",("%s is disabled",et->name.str));
      delete et;
      continue;
    }

    /* let's find when to be executed */
    if (et->compute_next_execution_time())
    {
      sql_print_error("SCHEDULER: Error while computing execution time of %s.%s."
                      " Skipping", et->dbname.str, et->name.str);
      continue;
    }

    {
      Event_job_data temp_job_data;
      DBUG_PRINT("info", ("Event %s loaded from row. ", et->name.str));

      temp_job_data.load_from_row(table);

      /*
        We load only on scheduler root just to check whether the body
        compiles.
      */
      switch (ret= temp_job_data.compile(thd, thd->mem_root)) {
      case EVEX_MICROSECOND_UNSUP:
        sql_print_error("SCHEDULER: mysql.event is tampered. MICROSECOND is not "
                        "supported but found in mysql.event");
        break;
      case EVEX_COMPILE_ERROR:
        sql_print_error("SCHEDULER: Error while compiling %s.%s. Aborting load",
                        et->dbname.str, et->name.str);
        break;
      default:
        break;
      }
      thd->end_statement();
      thd->cleanup_after_query();
    }
    if (ret)
    {
      delete et;
      goto end;
    }

    DBUG_PRINT("load_events_from_db", ("Adding 0x%lx to the exec list."));
    queue_insert_safe(&queue,  (byte *) et);
    count++;
  }
  clean_the_queue= FALSE;
end:
  end_read_record(&read_record_info);

  if (clean_the_queue)
  {
    empty_queue();
    ret= -1;
  }
  else
  {
    ret= 0;
    sql_print_information("SCHEDULER: Loaded %d event%s", count,
                          (count == 1)?"":"s");
  }

  close_thread_tables(thd);

  DBUG_PRINT("info", ("Status code %d. Loaded %d event(s)", ret, count));
  DBUG_RETURN(ret);
}


/*
  Recalculates activation times in the queue. There is one reason for
  that. Because the values (execute_at) by which the queue is ordered are
  changed by calls to compute_next_execution_time() on a request from the
  scheduler thread, if it is not running then the values won't be updated.
  Once the scheduler is started again the values has to be recalculated
  so they are right for the current time.

  SYNOPSIS
    Event_queue::recalculate_activation_times()
      thd  Thread
*/

void
Event_queue::recalculate_activation_times(THD *thd)
{
  uint i;
  DBUG_ENTER("Event_queue::recalculate_activation_times");

  LOCK_QUEUE_DATA();
  DBUG_PRINT("info", ("%u loaded events to be recalculated", queue.elements));
  for (i= 0; i < queue.elements; i++)
  {
    ((Event_queue_element*)queue_element(&queue, i))->compute_next_execution_time();
    ((Event_queue_element*)queue_element(&queue, i))->update_timing_fields(thd);
  }
  queue_fix(&queue);
  UNLOCK_QUEUE_DATA();

  DBUG_VOID_RETURN;
}


/*
  Empties the queue and destroys the Event_queue_element objects in the
  queue.

  SYNOPSIS
    Event_queue::empty_queue()

  NOTE
    Should be called with LOCK_event_queue locked
*/

void
Event_queue::empty_queue()
{
  uint i;
  DBUG_ENTER("Event_queue::empty_queue");
  DBUG_PRINT("enter", ("Purging the queue. %d element(s)", queue.elements));
  sql_print_information("SCHEDULER: Purging queue. %u events", queue.elements);
  /* empty the queue */
  for (i= 0; i < queue.elements; ++i)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    delete et;
  }
  resize_queue(&queue, 0);
  DBUG_VOID_RETURN;
}


/*
  Dumps the queue to the trace log.

  SYNOPSIS
    Event_queue::dbug_dump_queue()
      now  Current timestamp
*/

void
Event_queue::dbug_dump_queue(time_t now)
{
#ifndef DBUG_OFF
  Event_queue_element *et;
  uint i;
  DBUG_ENTER("Event_queue::dbug_dump_queue");
  DBUG_PRINT("info", ("Dumping queue . Elements=%u", queue.elements));
  for (i = 0; i < queue.elements; i++)
  {
    et= ((Event_queue_element*)queue_element(&queue, i));
    DBUG_PRINT("info",("et=0x%lx db=%s name=%s",et, et->dbname.str, et->name.str));
    DBUG_PRINT("info", ("exec_at=%llu starts=%llu ends=%llu execs_so_far=%u"
               " expr=%lld et.exec_at=%d now=%d (et.exec_at - now)=%d if=%d",
               TIME_to_ulonglong_datetime(&et->execute_at),
               TIME_to_ulonglong_datetime(&et->starts),
               TIME_to_ulonglong_datetime(&et->ends),
               et->execution_count,
               et->expression, sec_since_epoch_TIME(&et->execute_at), now,
               (int)(sec_since_epoch_TIME(&et->execute_at) - now),
               sec_since_epoch_TIME(&et->execute_at) <= now));
  }
  DBUG_VOID_RETURN;
#endif
}

static const char *queue_empty_msg= "Waiting on empty queue";
static const char *queue_wait_msg= "Waiting for next activation";

/*
  Checks whether the top of the queue is elligible for execution and
  returns an Event_job_data instance in case it should be executed.
  `now` is compared against `execute_at` of the top element in the queue.

  SYNOPSIS
    Event_queue::get_top_for_execution_if_time()
      thd      [in]  Thread
      now      [in]  Current timestamp
      job_data [out] The object to execute
      abstime  [out] Time to sleep

  RETURN VALUE
    FALSE  No error. If *job_data==NULL then top not elligible for execution.
           Could be that there is no top. If abstime->tv_sec is set to value
           greater than zero then use abstime with pthread_cond_timedwait().
           If abstime->tv_sec is zero then sleep with pthread_cond_wait().
           abstime->tv_nsec is always zero.
    TRUE   Error
    
*/

bool
Event_queue::get_top_for_execution_if_time(THD *thd, Event_job_data **job_data)
{
  bool ret= FALSE;
  struct timespec top_time;
  struct timespec *abstime;
  *job_data= NULL;
  DBUG_ENTER("Event_queue::get_top_for_execution_if_time");

  top_time.tv_nsec= 0;
  LOCK_QUEUE_DATA();
  for (;;)
  {
    int res;
    Event_queue_element *top= NULL;

    thd->end_time();
    time_t now= thd->query_start();
    abstime= NULL;

    if (queue.elements)
    {
      top= ((Event_queue_element*) queue_element(&queue, 0));
      top_time.tv_sec= sec_since_epoch_TIME(&top->execute_at);

      abstime= &top_time;
    }

    if (!abstime || abstime->tv_sec > now)
    {
      const char *msg;
      if (abstime)
      {
        next_activation_at= top->execute_at;
        msg= queue_wait_msg;
      }
      else
      {
        set_zero_time(&next_activation_at, MYSQL_TIMESTAMP_DATETIME);
        msg= queue_wait_msg;
      }

      cond_wait(thd, abstime, msg, SCHED_FUNC, __LINE__);
      if (thd->killed)
      {
        DBUG_PRINT("info", ("thd->killed=%d", thd->killed));
        goto end;
      }
      /*
        The queue could have been emptied. Therefore it's safe to start from
        the beginning. Moreover, this way we will get also the new top, if
        the element at the top has been changed.
      */
      continue;
    }

    DBUG_PRINT("info", ("Ready for execution"));
    if (!(*job_data= new Event_job_data()))
    {
      ret= TRUE;
      break;
    }
    if ((res= db_repository->load_named_event(thd, top->dbname, top->name,
                                              *job_data)))
    {
      DBUG_PRINT("error", ("Got %d from load_named_event", res));
      delete *job_data;
      *job_data= NULL;
      ret= TRUE;
      break;
    }

    top->mark_last_executed(thd);
    if (top->compute_next_execution_time())
      top->status= Event_queue_element::DISABLED;
    DBUG_PRINT("info", ("event %s status is %d", top->name.str, top->status));

    (*job_data)->execution_count= top->execution_count;

    top->update_timing_fields(thd);
    if (((top->execute_at.year && !top->expression) || top->execute_at_null) ||
        (top->status == Event_queue_element::DISABLED))
    {
      DBUG_PRINT("info", ("removing from the queue"));
      sql_print_information("SCHEDULER: Last execution of %s.%s. %s",
                            top->dbname.str, top->name.str,
                            top->dropped? "Dropping.":"");
      if (top->dropped)
        top->drop(thd);
      delete top;
      queue_remove(&queue, 0);
    }
    else
      queue_replaced(&queue);

    dbug_dump_queue(now);
    break;
  }
end:
  UNLOCK_QUEUE_DATA();
  
  DBUG_PRINT("info", ("returning %d. et_new=0x%lx abstime.tv_sec=%d ",
             ret, *job_data, abstime? abstime->tv_sec:0));

  if (*job_data)
    DBUG_PRINT("info", ("db=%s  name=%s definer=%s", (*job_data)->dbname.str,
               (*job_data)->name.str, (*job_data)->definer.str));

  DBUG_RETURN(ret);
}


/*
  Auxiliary function for locking LOCK_event_queue. Used by the
  LOCK_QUEUE_DATA macro

  SYNOPSIS
    Event_queue::lock_data()
      func  Which function is requesting mutex lock
      line  On which line mutex lock is requested
*/

void
Event_queue::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_queue::lock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_attempted_lock_in_func= func;
  mutex_last_attempted_lock_at_line= line;
  mutex_queue_data_attempting_lock= TRUE;
  pthread_mutex_lock(&LOCK_event_queue);
  mutex_last_attempted_lock_in_func= "";
  mutex_last_attempted_lock_at_line= 0;
  mutex_queue_data_attempting_lock= FALSE;

  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_queue_data_locked= TRUE;

  DBUG_VOID_RETURN;
}


/*
  Auxiliary function for unlocking LOCK_event_queue. Used by the
  UNLOCK_QUEUE_DATA macro

  SYNOPSIS
    Event_queue::unlock_data()
      func  Which function is requesting mutex unlock
      line  On which line mutex unlock is requested
*/

void
Event_queue::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_queue::unlock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_unlocked_at_line= line;
  mutex_queue_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  pthread_mutex_unlock(&LOCK_event_queue);
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_cond_wait/timedwait

  SYNOPSIS
    Event_queue::cond_wait()
      thd     Thread (Could be NULL during shutdown procedure)
      msg     Message for thd->proc_info
      abstime If not null then call pthread_cond_timedwait()
      func    Which function is requesting cond_wait
      line    On which line cond_wait is requested
*/

void
Event_queue::cond_wait(THD *thd, struct timespec *abstime, const char* msg,
                       const char *func, uint line)
{
  DBUG_ENTER("Event_queue::cond_wait");
  waiting_on_cond= TRUE;
  mutex_last_unlocked_at_line= line;
  mutex_queue_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;

  thd->enter_cond(&COND_queue_state, &LOCK_event_queue, msg);

  DBUG_PRINT("info", ("pthread_cond_%swait", abstime? "timed":""));
  if (!abstime)
    pthread_cond_wait(&COND_queue_state, &LOCK_event_queue);
  else
    pthread_cond_timedwait(&COND_queue_state, &LOCK_event_queue, abstime);

  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_queue_data_locked= TRUE;
  waiting_on_cond= FALSE;

  /*
    This will free the lock so we need to relock. Not the best thing to
    do but we need to obey cond_wait()
  */
  thd->exit_cond("");
  LOCK_QUEUE_DATA();

  DBUG_VOID_RETURN;
}


/*
  Dumps the internal status of the queue

  SYNOPSIS
    Event_queue::dump_internal_status()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_queue::dump_internal_status(THD *thd)
{
  DBUG_ENTER("Event_queue::dump_internal_status");
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

  /* workers_count */
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue element count"), scs);
  int_string.set((longlong) queue.elements, scs);
  protocol->store(&int_string);
  ret= protocol->write();

  /* queue_data_locked */
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue data locked"), scs);
  int_string.set((longlong) mutex_queue_data_locked, scs);
  protocol->store(&int_string);
  ret= protocol->write();

  /* queue_data_attempting_lock */
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue data attempting lock"), scs);
  int_string.set((longlong) mutex_queue_data_attempting_lock, scs);
  protocol->store(&int_string);
  ret= protocol->write();

  /* last locked at*/
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue last locked at"), scs);
  tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                        tmp_string.alloced_length(), "%s::%d",
                                        mutex_last_locked_in_func,
                                        mutex_last_locked_at_line));
  protocol->store(&tmp_string);
  ret= protocol->write();

  /* last unlocked at*/
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue last unlocked at"), scs);
  tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                        tmp_string.alloced_length(), "%s::%d",
                                        mutex_last_unlocked_in_func,
                                        mutex_last_unlocked_at_line));
  protocol->store(&tmp_string);
  ret= protocol->write();

  /* last attempted lock  at*/
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue last attempted lock at"), scs);
  tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                        tmp_string.alloced_length(), "%s::%d",
                                        mutex_last_attempted_lock_in_func,
                                        mutex_last_attempted_lock_at_line));
  protocol->store(&tmp_string);
  ret= protocol->write();

  /* waiting on */
  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("queue waiting on condition"), scs);
  int_string.set((longlong) waiting_on_cond, scs);
  protocol->store(&int_string);
  ret= protocol->write();

  protocol->prepare_for_resend();
  protocol->store(STRING_WITH_LEN("next activation at"), scs);
  tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                        tmp_string.alloced_length(),
                                        "%4d-%02d-%02d %02d:%02d:%02d",
                                        next_activation_at.year,
                                        next_activation_at.month,
                                        next_activation_at.day,
                                        next_activation_at.hour,
                                        next_activation_at.minute,
                                        next_activation_at.second
                                        ));
  protocol->store(&tmp_string);
  ret= protocol->write();

#endif
  DBUG_RETURN(FALSE);
}
