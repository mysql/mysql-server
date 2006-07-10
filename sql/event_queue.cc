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
#include "events.h"
#include "event_scheduler_ng.h"
#include "event_queue.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "sp_head.h"


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
  Compares the execute_at members of 2 Event_queue_element instances.
  Used as callback for the prioritized queue when shifting
  elements inside.

  SYNOPSIS
    event_queue_element_data_compare_q()

      vptr - not used (set it to NULL)
      a    - first Event_queue_element object
      b    - second Event_queue_element object

  RETURN VALUE
   -1   - a->execute_at < b->execute_at
    0   - a->execute_at == b->execute_at
    1   - a->execute_at > b->execute_at

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
{
  mutex_last_unlocked_at_line= mutex_last_locked_at_line= 0;
  mutex_last_unlocked_in_func= mutex_last_locked_in_func= "";
  mutex_queue_data_locked= FALSE;
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
  Signals the main scheduler thread that the queue has changed
  its state.

  SYNOPSIS
    Event_queue::notify_observers()
*/

void
Event_queue::notify_observers()
{
  DBUG_ENTER("Event_queue::notify_observers");
  DBUG_PRINT("info", ("Signalling change of the queue"));
  scheduler->queue_changed();
  DBUG_VOID_RETURN;
}


/*
  Inits the queue

  SYNOPSIS
    Event_queue::init()

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_queue::init_queue(Event_db_repository *db_repo, Event_scheduler_ng *sched)
{
  int i= 0;
  bool ret= FALSE;
  DBUG_ENTER("Event_queue::init_queue");
  DBUG_PRINT("enter", ("this=0x%lx", this));

  LOCK_QUEUE_DATA();
  db_repository= db_repo;
  scheduler= sched;

  if (init_queue_ex(&queue, 30 /*num_el*/, 0 /*offset*/, 0 /*smallest_on_top*/,
                    event_queue_element_compare_q, NULL, 30 /*auto_extent*/))
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

end:
  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(ret);
}


/*
  Deinits the queue

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
  Creates an event in the scheduler queue

  SYNOPSIS
    Event_queue::create_event()
      et              The event to add
      check_existence Whether to check if already loaded.

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
*/

int
Event_queue::create_event(THD *thd, LEX_STRING dbname, LEX_STRING name)
{
  int res;
  Event_queue_element *element_new;
  DBUG_ENTER("Event_queue::create_event");
  DBUG_PRINT("enter", ("thd=0x%lx et=%s.%s",thd, dbname.str, name.str));

  element_new= new Event_queue_element();
  res= db_repository->load_named_event(thd, dbname, name, element_new);
  if (res || element_new->status == Event_queue_element::DISABLED)
    delete element_new;
  else
  {
    element_new->compute_next_execution_time();

    LOCK_QUEUE_DATA();
    DBUG_PRINT("info", ("new event in the queue 0x%lx", element_new));
    queue_insert_safe(&queue, (byte *) element_new);
    UNLOCK_QUEUE_DATA();

    notify_observers();
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
  Event_queue_element *element_old= NULL,
                      *element_new;

  DBUG_ENTER("Event_queue::update_event");
  DBUG_PRINT("enter", ("thd=0x%lx et=[%s.%s]", thd, dbname.str, name.str));

  element_new= new Event_queue_element();

  res= db_repository->load_named_event(thd, new_schema? *new_schema:dbname,
                                       new_name? *new_name:name, element_new);
  if (res)
  {
    delete element_new;
    goto end;
  }
  else if (element_new->status == Event_queue_element::DISABLED)
  {
    DBUG_PRINT("info", ("The event is disabled."));
    /*
      Destroy the object but don't skip to end: because we may have to remove
      object from the cache.
    */
    delete element_new;
    element_new= NULL;
  }
  else
    element_new->compute_next_execution_time();

  LOCK_QUEUE_DATA();
  if (!(element_old= find_event(dbname, name, TRUE)))
  {
    DBUG_PRINT("info", ("%s.%s not cached, probably was DISABLED",
                        dbname.str, name.str));
  }
  /* If not disabled event */
  if (element_new)
  {
    DBUG_PRINT("info", ("new event in the Q 0x%lx old 0x%lx",
                        element_new, element_old));
    queue_insert_safe(&queue, (byte *) element_new);
  }
  UNLOCK_QUEUE_DATA();

  notify_observers();

  if (element_old)
    delete element_old;
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
  Event_queue_element *element;
  DBUG_ENTER("Event_queue::drop_event");
  DBUG_PRINT("enter", ("thd=0x%lx name=0x%lx", thd, name));

  LOCK_QUEUE_DATA();
  element= find_event(dbname, name, TRUE);
  UNLOCK_QUEUE_DATA();

  if (element)
    delete element;
  else
    DBUG_PRINT("info", ("No such event found, probably DISABLED"));
  
  /*
    We don't signal here because the scheduler will catch the change
    next time it wakes up.
  */

  DBUG_VOID_RETURN;
}


/*
  Searches for an event in the queue

  SYNOPSIS
    Event_queue::find_event()
      db             The schema of the event to find
      name           The event to find
      remove_from_q  If found whether to remove from the Q

  RETURN VALUE
    NULL       Not found
    otherwise  Address

  NOTE
    The caller should do the locking also the caller is responsible for
    actual signalling in case an event is removed from the queue 
    (signalling COND_new_work for instance).
*/

Event_queue_element *
Event_queue::find_event(LEX_STRING db, LEX_STRING name, bool remove_from_q)
{
  uint i;
  DBUG_ENTER("Event_queue::find_event");

  for (i= 0; i < queue.elements; ++i)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?", db.str, name.str,
                        et->dbname.str, et->name.str));
    if (event_basic_identifier_equal(db, name, et))
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
                           bool (*comparator)(LEX_STRING *, Event_basic *))
{
  DBUG_ENTER("Event_queue::drop_matching_events");
  DBUG_PRINT("enter", ("pattern=%*s state=%d", pattern.length, pattern.str));

  uint i= 0;
  while (i < queue.elements)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]?", et->dbname.str, et->name.str));
    if (comparator(&pattern, et))
    {
      /*
        The queue is ordered. If we remove an element, then all elements after
        it will shift one position to the left, if we imagine it as an array
        from left to the right. In this case we should not increment the 
        counter and the (i < queue.elements) condition is ok.
      */
      queue_remove(&queue, i);
      delete et;
    }
    else
      i++;
  }
  /*
    We don't call notify_observers() . If we remove the top event:
    1. The queue is empty. The scheduler will wake up at some time and realize
       that the queue is empty. If create_event() comes inbetween it will
       signal the scheduler
    2. The queue is not empty, but the next event after the previous top, won't
       be executed any time sooner than the element we removed. Hence, we may
       not notify the scheduler and it will realize the change when it
       wakes up from timedwait.
  */

  DBUG_VOID_RETURN;
}


/*
  Drops all events from the in-memory queue and disk that are from
  certain schema.

  SYNOPSIS
    Event_queue::drop_schema_events()
      thd        THD
      db         The schema name

  RETURN VALUE
    >=0  Number of dropped events
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
  Returns the number of elements in the queue

  SYNOPSIS
    Event_queue::events_count()

  RETURN VALUE
    Number of Event_queue_element objects in the queue
*/

uint
Event_queue::events_count()
{
  uint n;
  DBUG_ENTER("Event_scheduler::events_count");
  LOCK_QUEUE_DATA();
  n= queue.elements;
  UNLOCK_QUEUE_DATA();
  DBUG_PRINT("info", ("n=%u", n));
  DBUG_RETURN(n);
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
  bool clean_the_queue= FALSE;
  /* Compile the events on this root but only for syntax check, then discard */
  MEM_ROOT boot_root;

  DBUG_ENTER("Event_queue::load_events_from_db");
  DBUG_PRINT("enter", ("thd=0x%lx", thd));

  if ((ret= db_repository->open_event_table(thd, TL_READ, &table)))
  {
    sql_print_error("SCHEDULER: Table mysql.event is damaged. Can not open.");
    DBUG_RETURN(EVEX_OPEN_TABLE_FAILED);
  }

  init_read_record(&read_record_info, thd, table ,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    Event_queue_element *et;
    if (!(et= new Event_queue_element))
    {
      DBUG_PRINT("info", ("Out of memory"));
      clean_the_queue= TRUE;
      break;
    }
    DBUG_PRINT("info", ("Loading event from row."));

    if ((ret= et->load_from_row(table)))
    {
      clean_the_queue= TRUE;
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
#if 0
    init_alloc_root(&boot_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
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
    free_root(&boot_root, MYF(0));

    /* let's find when to be executed */
    if (et->compute_next_execution_time())
    {
      sql_print_error("SCHEDULER: Error while computing execution time of %s.%s."
                      " Skipping", et->dbname.str, et->name.str);
      continue;
    }
#endif
    DBUG_PRINT("load_events_from_db", ("Adding 0x%lx to the exec list."));
    queue_insert_safe(&queue,  (byte *) et);
    count++;
  }
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
    Event_queue::check_system_tables()
*/

bool
Event_queue::check_system_tables(THD *thd)
{
  TABLE_LIST tables;
  bool not_used;
  Open_tables_state backup;
  bool ret;

  DBUG_ENTER("Event_queue::check_system_tables");
  DBUG_PRINT("enter", ("thd=0x%lx", thd));

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
  /* empty the queue */
  for (i= 0; i < queue.elements; ++i)
  {
    Event_queue_element *et= (Event_queue_element *) queue_element(&queue, i);
    delete et;
  }
  resize_queue(&queue, 0);
  DBUG_VOID_RETURN;
}


inline void
Event_queue::dbug_dump_queue(time_t now)
{
#ifndef DBUG_OFF
  Event_queue_element *et;
  uint i;
  DBUG_PRINT("info", ("Dumping queue . Elements=%u", queue.elements));
  for (i = 0; i < queue.elements; i++)
  {
    et= ((Event_queue_element*)queue_element(&queue, i));
    DBUG_PRINT("info",("et=0x%lx db=%s name=%s",et, et->dbname.str, et->name.str));
    DBUG_PRINT("info", ("exec_at=%llu starts=%llu ends=%llu "
               " expr=%lld et.exec_at=%d now=%d (et.exec_at - now)=%d if=%d",
               TIME_to_ulonglong_datetime(&et->execute_at),
               TIME_to_ulonglong_datetime(&et->starts),
               TIME_to_ulonglong_datetime(&et->ends),
               et->expression, sec_since_epoch_TIME(&et->execute_at), now,
               (int)(sec_since_epoch_TIME(&et->execute_at) - now),
               sec_since_epoch_TIME(&et->execute_at) <= now));
  }
#endif
}

Event_job_data *
Event_queue::get_top_for_execution_if_time(THD *thd, time_t now,
                                           struct timespec *abstime)
{
  struct timespec top_time;
  Event_job_data *et_new= NULL;
  DBUG_ENTER("Event_queue::get_top_for_execution_if_time");
  DBUG_PRINT("enter", ("thd=0x%lx now=%d", thd, now));
  abstime->tv_nsec= 0;
  LOCK_QUEUE_DATA();
  do {
    int res;
    if (!queue.elements)
    {
      abstime->tv_sec= 0;
      break;
    }
    dbug_dump_queue(now);

    Event_queue_element *et= ((Event_queue_element*) queue_element(&queue, 0));
    top_time.tv_sec= sec_since_epoch_TIME(&et->execute_at);

    if (top_time.tv_sec <= now)
    {
      DBUG_PRINT("info", ("Ready for execution"));
      abstime->tv_sec= 0;
      et_new= new Event_job_data();
      if ((res= db_repository->load_named_event(thd, et->dbname, et->name,
                                                et_new)))
      {
        delete et_new;
        et_new= NULL;
        DBUG_ASSERT(0);
        break;
      }

      et->mark_last_executed(thd);
      if (et->compute_next_execution_time())
        et->status= Event_queue_element::DISABLED;
      DBUG_PRINT("info", ("event's status is %d", et->status));

      et->update_timing_fields(thd);
      if (((et->execute_at.year && !et->expression) || et->execute_at_null) ||
          (et->status == Event_queue_element::DISABLED))
      {
        DBUG_PRINT("info", ("removing from the queue"));
        if (et->dropped)
          et->drop(thd);
        delete et;
        queue_remove(&queue, 0);
      }
      else
        queue_replaced(&queue);
    }
    else
    {
      abstime->tv_sec= top_time.tv_sec;
      DBUG_PRINT("info", ("Have to wait %d till %d", abstime->tv_sec - now,
                 abstime->tv_sec));
    }
  } while (0);
  UNLOCK_QUEUE_DATA();
  
  DBUG_PRINT("info", ("returning. et_new=0x%lx abstime.tv_sec=%d ", et_new,
             abstime->tv_sec));
  if (et_new)
    DBUG_PRINT("info", ("db=%s  name=%s definer=%s",
               et_new->dbname.str, et_new->name.str, et_new->definer.str));
  DBUG_RETURN(et_new);
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
  pthread_mutex_lock(&LOCK_event_queue);
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
#endif
  DBUG_RETURN(FALSE);
}
