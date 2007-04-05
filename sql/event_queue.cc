/* Copyright (C) 2004-2006 MySQL AB

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

#include "mysql_priv.h"
#include "event_queue.h"
#include "event_data_objects.h"


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
  my_time_t lhs = ((Event_queue_element *)a)->execute_at;
  my_time_t rhs = ((Event_queue_element *)b)->execute_at;

  return (lhs < rhs ? -1 : (lhs > rhs ? 1 : 0));
}


/*
  Constructor of class Event_queue.

  SYNOPSIS
    Event_queue::Event_queue()
*/

Event_queue::Event_queue()
  :mutex_last_unlocked_at_line(0), mutex_last_locked_at_line(0),
   mutex_last_attempted_lock_at_line(0),
   mutex_queue_data_locked(FALSE),
   mutex_queue_data_attempting_lock(FALSE),
   next_activation_at(0)
{
  mutex_last_unlocked_in_func= mutex_last_locked_in_func=
    mutex_last_attempted_lock_in_func= "";

  pthread_mutex_init(&LOCK_event_queue, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_queue_state, NULL);
}


Event_queue::~Event_queue()
{
  deinit_queue();
  pthread_mutex_destroy(&LOCK_event_queue);
  pthread_cond_destroy(&COND_queue_state);
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
Event_queue::init_queue(THD *thd)
{
  DBUG_ENTER("Event_queue::init_queue");
  DBUG_PRINT("enter", ("this: 0x%lx", (long) this));

  LOCK_QUEUE_DATA();

  if (init_queue_ex(&queue, EVENT_QUEUE_INITIAL_SIZE , 0 /*offset*/,
                    0 /*max_on_top*/, event_queue_element_compare_q,
                    NULL, EVENT_QUEUE_EXTENT))
  {
    sql_print_error("Event Scheduler: Can't initialize the execution queue");
    goto err;
  }

  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(FALSE);

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


/**
  Adds an event to the queue.

  Compute the next execution time for an event, and if it is still
  active, add it to the queue. Otherwise delete it.
  The object is left intact in case of an error. Otherwise
  the queue container assumes ownership of it.

  @param[in]  thd      thread handle
  @param[in]  new_element a new element to add to the queue
  @param[out] created  set to TRUE if no error and the element is
                       added to the queue, FALSE otherwise

  @retval TRUE  an error occured. The value of created is undefined,
                the element was not deleted.
  @retval FALSE success
*/

bool
Event_queue::create_event(THD *thd, Event_queue_element *new_element,
                          bool *created)
{
  DBUG_ENTER("Event_queue::create_event");
  DBUG_PRINT("enter", ("thd: 0x%lx et=%s.%s", (long) thd,
             new_element->dbname.str, new_element->name.str));

  /* Will do nothing if the event is disabled */
  new_element->compute_next_execution_time();
  if (new_element->status == Event_queue_element::DISABLED)
  {
    delete new_element;
    *created= FALSE;
    DBUG_RETURN(FALSE);
  }

  DBUG_PRINT("info", ("new event in the queue: 0x%lx", (long) new_element));

  LOCK_QUEUE_DATA();
  *created= (queue_insert_safe(&queue, (byte *) new_element) == FALSE);
  dbug_dump_queue(thd->query_start());
  pthread_cond_broadcast(&COND_queue_state);
  UNLOCK_QUEUE_DATA();

  DBUG_RETURN(!*created);
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
*/

void
Event_queue::update_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
                          Event_queue_element *new_element)
{
  DBUG_ENTER("Event_queue::update_event");
  DBUG_PRINT("enter", ("thd: 0x%lx  et=[%s.%s]", (long) thd, dbname.str, name.str));

  if (new_element->status == Event_queue_element::DISABLED)
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
    DBUG_PRINT("info", ("new event in the queue: 0x%lx", (long) new_element));
    queue_insert_safe(&queue, (byte *) new_element);
    pthread_cond_broadcast(&COND_queue_state);
  }

  dbug_dump_queue(thd->query_start());
  UNLOCK_QUEUE_DATA();

  DBUG_VOID_RETURN;
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
  DBUG_ENTER("Event_queue::drop_event");
  DBUG_PRINT("enter", ("thd: 0x%lx  db :%s  name: %s", (long) thd,
                       dbname.str, name.str));

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
  DBUG_PRINT("enter", ("Purging the queue. %u element(s)", queue.elements));
  sql_print_information("Event Scheduler: Purging the queue. %u events",
                        queue.elements);
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
    DBUG_PRINT("info", ("et: 0x%lx  name: %s.%s", (long) et,
                        et->dbname.str, et->name.str));
    DBUG_PRINT("info", ("exec_at: %lu  starts: %lu  ends: %lu  execs_so_far: %u  "
                        "expr: %ld  et.exec_at: %ld  now: %ld  "
                        "(et.exec_at - now): %d  if: %d",
                        (long) et->execute_at, (long) et->starts,
                        (long) et->ends, et->execution_count,
                        (long) et->expression, (long) et->execute_at,
                        (long) now, (int) (et->execute_at - now),
                        et->execute_at <= now));
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
      thd        [in]  Thread
      event_name [out] The object to execute

  RETURN VALUE
    FALSE  No error. event_name != NULL
    TRUE   Serious error
*/

bool
Event_queue::get_top_for_execution_if_time(THD *thd,
                Event_queue_element_for_exec **event_name)
{
  bool ret= FALSE;
  *event_name= NULL;
  DBUG_ENTER("Event_queue::get_top_for_execution_if_time");

  LOCK_QUEUE_DATA();
  for (;;)
  {
    Event_queue_element *top= NULL;

    /* Break loop if thd has been killed */
    if (thd->killed)
    {
      DBUG_PRINT("info", ("thd->killed=%d", thd->killed));
      goto end;
    }

    if (!queue.elements)
    {
      /* There are no events in the queue */
      next_activation_at= 0;

      /* Wait on condition until signaled. Release LOCK_queue while waiting. */
      cond_wait(thd, NULL, queue_empty_msg, SCHED_FUNC, __LINE__);

      continue;
    }

    top= ((Event_queue_element*) queue_element(&queue, 0));

    thd->end_time(); /* Get current time */

    next_activation_at= top->execute_at;
    if (next_activation_at > thd->query_start())
    {
      /*
        Not yet time for top event, wait on condition with
        time or until signaled. Release LOCK_queue while waiting.
      */
      struct timespec top_time;
      set_timespec(top_time, next_activation_at - thd->query_start());
      cond_wait(thd, &top_time, queue_wait_msg, SCHED_FUNC, __LINE__);

      continue;
    }

    if (!(*event_name= new Event_queue_element_for_exec()) ||
        (*event_name)->init(top->dbname, top->name))
    {
      ret= TRUE;
      break;
    }

    DBUG_PRINT("info", ("Ready for execution"));
    top->mark_last_executed(thd);
    if (top->compute_next_execution_time())
      top->status= Event_queue_element::DISABLED;
    DBUG_PRINT("info", ("event %s status is %d", top->name.str, top->status));

    top->execution_count++;
    (*event_name)->dropped= top->dropped;

    top->update_timing_fields(thd);
    if (top->status == Event_queue_element::DISABLED)
    {
      DBUG_PRINT("info", ("removing from the queue"));
      sql_print_information("Event Scheduler: Last execution of %s.%s. %s",
                            top->dbname.str, top->name.str,
                            top->dropped? "Dropping.":"");
      delete top;
      queue_remove(&queue, 0);
    }
    else
      queue_replaced(&queue);

    dbug_dump_queue(thd->query_start());
    break;
  }
end:
  UNLOCK_QUEUE_DATA();

  DBUG_PRINT("info", ("returning %d  et_new: 0x%lx ",
                      ret, (long) *event_name));

  if (*event_name)
    DBUG_PRINT("info", ("db: %s  name: %s",
                        (*event_name)->dbname.str, (*event_name)->name.str));

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
  lock_data(func, line);

  DBUG_VOID_RETURN;
}


/*
  Dumps the internal status of the queue

  SYNOPSIS
    Event_queue::dump_internal_status()
*/

void
Event_queue::dump_internal_status()
{
  DBUG_ENTER("Event_queue::dump_internal_status");

  /* element count */
  puts("");
  puts("Event queue status:");
  printf("Element count   : %u\n", queue.elements);
  printf("Data locked     : %s\n", mutex_queue_data_locked? "YES":"NO");
  printf("Attempting lock : %s\n", mutex_queue_data_attempting_lock? "YES":"NO");
  printf("LLA             : %s:%u\n", mutex_last_locked_in_func,
                                        mutex_last_locked_at_line);
  printf("LUA             : %s:%u\n", mutex_last_unlocked_in_func,
                                        mutex_last_unlocked_at_line);
  if (mutex_last_attempted_lock_at_line)
    printf("Last lock attempt at: %s:%u\n", mutex_last_attempted_lock_in_func,
                                            mutex_last_attempted_lock_at_line);
  printf("WOC             : %s\n", waiting_on_cond? "YES":"NO");

  TIME time;
  my_tz_UTC->gmt_sec_to_TIME(&time, next_activation_at);
  printf("Next activation : %04d-%02d-%02d %02d:%02d:%02d\n",
         time.year, time.month, time.day, time.hour, time.minute, time.second);

  DBUG_VOID_RETURN;
}
