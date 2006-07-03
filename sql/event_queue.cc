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
#include "event_scheduler.h"
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


Event_scheduler*
Event_queue::singleton= NULL;


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
*/

int
Event_queue::create_event(THD *thd, Event_parse_data *et, bool check_existence)
{
  int res;
  Event_timed *et_new;
  DBUG_ENTER("Event_queue::create_event");
  DBUG_PRINT("enter", ("thd=%p et=%p lock=%p",thd,et, &LOCK_event_queue));

  LOCK_QUEUE_DATA();
  if (check_existence && find_event(et->dbname, et->name, FALSE))
  {
    res= OP_ALREADY_EXISTS;
    goto end;
  }

  /* We need to load the event on scheduler_root */
  if (!(res= db_repository->
                load_named_event(thd, et->dbname, et->name, &et_new)))
  {
    queue_insert_safe(&queue, (byte *) et_new);
    on_queue_change();
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;
end:
  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(res);
}


/*
  Updates an event from the scheduler queue

  SYNOPSIS
    Event_scheduler::update_event()
      thd        Thread
      et         The event to replace(add) into the queue
      new_schema New schema
      new_name   New name

  RETURN VALUE
    OP_OK             OK or scheduler not working
    OP_LOAD_ERROR     Error during loading from disk
    OP_ALREADY_EXISTS Event already in the queue    
*/

int
Event_queue::update_event(THD *thd, Event_parse_data *et,
                               LEX_STRING *new_schema,
                               LEX_STRING *new_name)
{
  int res= OP_OK;
  Event_timed *et_old, *et_new= NULL;
  LEX_STRING old_schema, old_name;

  LINT_INIT(old_schema.str);
  LINT_INIT(old_schema.length);
  LINT_INIT(old_name.str);
  LINT_INIT(old_name.length);

  DBUG_ENTER("Event_queue::update_event");
  DBUG_PRINT("enter", ("thd=%p et=%p et=[%s.%s] lock=%p",
             thd, et, et->dbname.str, et->name.str, &LOCK_event_queue));

  LOCK_QUEUE_DATA();
  if (!(et_old= find_event(et->dbname, et->name, TRUE)))
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
  if (!(res= db_repository->
            load_named_event(thd, et->dbname, et->name, &et_new)))
  {
    queue_insert_safe(&queue, (byte *) et_new);
    on_queue_change();
  }
  else if (res == OP_DISABLED_EVENT)
    res= OP_OK;

  if (new_schema && new_name)
  {
    et->dbname= old_schema;
    et->name= old_name;
  }
  DBUG_PRINT("info", ("res=%d", res));
  UNLOCK_QUEUE_DATA();
  /*
    Andrey: Is this comment still truthful ???

    We don't move this code above because a potential kill_thread will call
    THD::awake(). Which in turn will try to acqure mysys_var->current_mutex,
    which is LOCK_event_queue on which the COND_new_work in ::run() locks.
    Hence, we try to acquire a lock which we have already acquired and we run
    into an assert. Holding LOCK_event_queue however is not needed because
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
  Drops an event from the scheduler queue

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
  Searches for an event in the scheduler queue

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
     -1  Scheduler not working
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

  uint i= 0, dropped= 0;   
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
    Event_queue::drop_schema_events()
      thd        THD
      db         The schema name

  RETURN VALUE
     -1  Scheduler not working
    >=0  Number of dropped events
*/

int
Event_queue::drop_schema_events(THD *thd, LEX_STRING schema)
{
  int ret;
  DBUG_ENTER("Event_queue::drop_schema_events");
  LOCK_QUEUE_DATA();
  drop_matching_events(thd, schema, event_timed_db_equal);
  UNLOCK_QUEUE_DATA();

  DBUG_RETURN(ret);
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
  DBUG_PRINT("enter", ("mutex_lock=%p func=%s line=%u",
             &LOCK_event_queue, func, line));
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
  DBUG_ENTER("Event_queue::UNLOCK_mutex");
  DBUG_PRINT("enter", ("mutex_unlock=%p func=%s line=%u",
             &LOCK_event_queue, func, line));
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
  DBUG_ENTER("Event_scheduler::events_count_no_lock");

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
  pthread_mutex_init(&singleton->LOCK_event_queue, MY_MUTEX_INIT_FAST);
}


/*
  Destroys mutexes.

  SYNOPSIS
    Event_queue::destroy_mutexes()
*/

void
Event_queue::destroy_mutexes()
{
  pthread_mutex_destroy(&singleton->LOCK_event_queue);
}


/*
  Signals the main scheduler thread that the queue has changed
  its state.

  SYNOPSIS
    Event_queue::on_queue_change()
*/

void
Event_queue::on_queue_change()
{
  DBUG_ENTER("Event_queue::on_queue_change");
  DBUG_PRINT("info", ("Sending COND_new_work"));
  singleton->queue_changed();
  DBUG_VOID_RETURN;
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
Event_queue::init(Event_db_repository *db_repo)
{
  int i= 0;
  bool ret= FALSE;
  DBUG_ENTER("Event_scheduler::init");
  DBUG_PRINT("enter", ("this=%p", this));

  LOCK_QUEUE_DATA();
  db_repository= db_repo;
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

end:
  UNLOCK_QUEUE_DATA();
  DBUG_RETURN(ret);
}


void
Event_queue::deinit()
{
  DBUG_ENTER("Event_queue::deinit");

  LOCK_QUEUE_DATA();
  delete_queue(&queue);
  free_root(&scheduler_root, MYF(0));
  UNLOCK_QUEUE_DATA();

  DBUG_VOID_RETURN;
}


void
Event_queue::recalculate_queue(THD *thd)
{
  int i;
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
  int i;
  /* empty the queue */
  for (i= 0; i < events_count_no_lock(); ++i)
  {
    Event_timed *et= (Event_timed *) queue_element(&queue, i);
    et->free_sp();
    delete et;
  }
  resize_queue(&queue, 0);
}
