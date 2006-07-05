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
  Creates an event in the scheduler queue

  SYNOPSIS
    Event_queue::create_event()
      et              The event to add
      check_existence Whether to check if already loaded.

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
    OP_ALREADY_EXISTS Event already in the queue    
*/

int
Event_queue::create_event(THD *thd, Event_parse_data *et)
{
  int res;
  Event_timed *et_new;
  DBUG_ENTER("Event_queue::create_event");
  DBUG_PRINT("enter", ("thd=%p et=%p lock=%p",thd,et, &LOCK_event_queue));

  res= db_repository->load_named_event_timed(thd, et->dbname, et->name, &et_new);
  LOCK_QUEUE_DATA();
  if (!res)
  {
    DBUG_PRINT("info", ("new event in the queue %p", et_new));
    queue_insert_safe(&queue, (byte *) et_new);
    notify_observers();
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;
  UNLOCK_QUEUE_DATA();

  DBUG_RETURN(res);
}


/*
  Updates an event from the scheduler queue

  SYNOPSIS
    Event_queue::update_event()
      thd        Thread
      et         The event to replace(add) into the queue
      new_schema New schema, in case of RENAME TO
      new_name   New name, in case of RENAME TO

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
*/

int
Event_queue::update_event(THD *thd, Event_parse_data *et,
                          LEX_STRING *new_schema, LEX_STRING *new_name)
{
  int res;
  Event_timed *et_old= NULL, *et_new= NULL;

  DBUG_ENTER("Event_queue::update_event");
  DBUG_PRINT("enter", ("thd=%p et=%p et=[%s.%s] lock=%p",
             thd, et, et->dbname.str, et->name.str, &LOCK_event_queue));

  res= db_repository->
            load_named_event_timed(thd, new_schema?*new_schema:et->dbname,
                                   new_name? *new_name:et->name, &et_new);

  if (res && res != OP_DISABLED_EVENT)
    goto end;

  LOCK_QUEUE_DATA();
  if (!(et_old= find_event(et->dbname, et->name, TRUE)))
  {
    DBUG_PRINT("info", ("%s.%s not found cached, probably was DISABLED",
                        et->dbname.str, et->name.str));
  }

  if (!res)
  {
    DBUG_PRINT("info", ("new event in the queue %p old %p", et_new, et_old));
    queue_insert_safe(&queue, (byte *) et_new);
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;
  UNLOCK_QUEUE_DATA();

  notify_observers();

  if (et_old)
    delete et_old;
end:
  DBUG_PRINT("info", ("res=%d", res));
  DBUG_RETURN(res);
}


/*
  Drops an event from the queue

  SYNOPSIS
    Event_queue::drop_event()
      thd    Thread
      name   The event to drop

  RETURN VALUE
    FALSE OK (replaced or scheduler not working)
    TRUE  Failure
*/

bool
Event_queue::drop_event(THD *thd, sp_name *name)
{
  int res;
  Event_timed *et_old;
  DBUG_ENTER("Event_queue::drop_event");
  DBUG_PRINT("enter", ("thd=%p name=%p lock=%p", thd, name,
             &LOCK_event_queue));

  LOCK_QUEUE_DATA();
  if (!(et_old= find_event(name->m_db, name->m_name, TRUE)))
    DBUG_PRINT("info", ("No such event found, probably DISABLED"));
  UNLOCK_QUEUE_DATA();
  if (et_old)
    delete et_old;
  /*
    We don't signal here because the scheduler will catch the change
    next time it wakes up.
  */

  DBUG_RETURN(FALSE);
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

Event_timed *
Event_queue::find_event(LEX_STRING db, LEX_STRING name, bool remove_from_q)
{
  uint i;
  DBUG_ENTER("Event_queue::find_event");

  for (i= 0; i < queue.elements; ++i)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?", db.str, name.str,
                        et->dbname.str, et->name.str));
    if (event_timed_identifier_equal(db, name, et))
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
                           bool (*comparator)(Event_timed *,LEX_STRING *))
{
  DBUG_ENTER("Event_queue::drop_matching_events");
  DBUG_PRINT("enter", ("pattern=%*s state=%d", pattern.length, pattern.str));

  uint i= 0;   
  while (i < queue.elements)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    DBUG_PRINT("info", ("[%s.%s]?", et->dbname.str, et->name.str));
    if (comparator(et, &pattern))
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
  drop_matching_events(thd, schema, event_timed_db_equal);
  UNLOCK_QUEUE_DATA();
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_mutex_lock

  SYNOPSIS
    Event_queue::lock_data()
      mutex Mutex to lock
      line  The line number on which the lock is done

  RETURN VALUE
    Error code of pthread_mutex_lock()
*/

void
Event_queue::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_queue::lock_mutex");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  pthread_mutex_lock(&LOCK_event_queue);
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_queue_data_locked= TRUE;
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_mutex_unlock

  SYNOPSIS
    Event_queue::unlock_data()
      mutex Mutex to unlock
      line  The line number on which the unlock is done
*/

void
Event_queue::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_queue::unlock_mutex");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_unlocked_at_line= line;
  mutex_queue_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  pthread_mutex_unlock(&LOCK_event_queue);
  DBUG_VOID_RETURN;
}


/*
  Returns the number of elements in the queue

  SYNOPSIS
    Event_queue::events_count()

  RETURN VALUE
    0  Number of Event_timed objects in the queue
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
  Returns the number of elements in the queue

  SYNOPSIS
    Event_queue::events_count_no_lock()

  RETURN VALUE
    0  Number of Event_timed objects in the queue
*/

uint
Event_queue::events_count_no_lock()
{
  uint n;
  DBUG_ENTER("Event_queue::events_count_no_lock");

  n= queue.elements;

  DBUG_RETURN(n);
}


/*
  Loads all ENABLED events from mysql.event into the prioritized
  queue. Called during scheduler main thread initialization. Compiles
  the events. Creates Event_timed instances for every ENABLED event
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
  DBUG_PRINT("enter", ("thd=%p", thd));

  if ((ret= db_repository->open_event_table(thd, TL_READ, &table)))
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

    if ((ret= et->load_from_row(table)))
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
  The implementation of full-fledged initialization.

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
  DBUG_PRINT("enter", ("this=%p", this));

  LOCK_QUEUE_DATA();
  db_repository= db_repo;
  scheduler= sched;

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

end:
  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(ret);
}


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


void
Event_queue::recalculate_queue(THD *thd)
{
  uint i;
  for (i= 0; i < queue.elements; i++)
  {
    ((Event_timed*)queue_element(&queue, i))->compute_next_execution_time();
    ((Event_timed*)queue_element(&queue, i))->update_fields(thd);
  }
  queue_fix(&queue);
}


void
Event_queue::empty_queue()
{
  uint i;
  DBUG_ENTER("Event_queue::empty_queue");
  DBUG_PRINT("enter", ("Purging the queue. %d element(s)", queue.elements));
  /* empty the queue */
  for (i= 0; i < events_count_no_lock(); ++i)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    delete et;
  }
  resize_queue(&queue, 0);
  DBUG_VOID_RETURN;
}


Event_timed*
Event_queue::get_top()
{
  return (Event_timed *)queue_top(&queue); 
}


void
Event_queue::remove_top()
{
  queue_remove(&queue, 0);// 0 is top, internally 1
}


void
Event_queue::top_changed()
{
  queue_replaced(&queue);
}


inline void
Event_queue::dbug_dump_queue(time_t now)
{
#ifndef DBUG_OFF
  Event_timed *et;
  uint i;
  DBUG_PRINT("info", ("Dumping queue . Elements=%u", queue.elements));
  for (i = 0; i < queue.elements; i++)
  {
    et= ((Event_timed*)queue_element(&queue, i));
    DBUG_PRINT("info",("et=%p db=%s name=%s",et, et->dbname.str, et->name.str));
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

Event_timed *
Event_queue::get_top_for_execution_if_time(THD *thd, time_t now,
                                           struct timespec *abstime)
{
  struct timespec top_time;
  Event_timed *et_new= NULL;
  DBUG_ENTER("Event_queue::get_top_for_execution_if_time");
  DBUG_PRINT("enter", ("thd=%p now=%d", thd, now));
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

    Event_timed *et= ((Event_timed*)queue_element(&queue, 0));
    top_time.tv_sec= sec_since_epoch_TIME(&et->execute_at);

    if (top_time.tv_sec <= now)
    {
      DBUG_PRINT("info", ("Ready for execution"));
      abstime->tv_sec= 0;
      if ((res= db_repository->load_named_event_timed(thd, et->dbname, et->name,
                                                      &et_new)))
      {
        DBUG_ASSERT(0);
        break;
      }

      et->mark_last_executed(thd);
      if (et->compute_next_execution_time())
        et->status= Event_timed::DISABLED;
      DBUG_PRINT("info", ("event's status is %d", et->status));

      et->update_fields(thd);
      if (((et->execute_at.year && !et->expression) || et->execute_at_null) ||
          (et->status == Event_timed::DISABLED))
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
  
  DBUG_PRINT("info", ("returning. et_new=%p abstime.tv_sec=%d ", et_new,
             abstime->tv_sec));
  if (et_new)
    DBUG_PRINT("info", ("db=%s  name=%s definer=%s "
               "et_new.execute_at=%lld", et_new->dbname.str, et_new->name.str,
               et_new->definer.str,
               TIME_to_ulonglong_datetime(&et_new->execute_at)));
  DBUG_RETURN(et_new);
}
