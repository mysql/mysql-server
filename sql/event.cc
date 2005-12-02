#include "mysql_priv.h"
//#include "sql_acl.h"
#include "event.h"
#include "sp.h"

/*
 TODO list :
 1. Move event_timed class to event_timed.cc as well as the part of the header
 2. Do something aboute the events replication. SQL statements issued while
    executing an event should not be logged into the binary log.
 3. Add a lock and use it for guarding access to events_array dynamic array.
 4. Add checks everywhere where new instance of THD is created. NULL can be
    returned and this will crash the server. The server will crash probably
    later but should not be in this code! Add a global variable, and a lock
	to guard it, that will specify an error in a worker thread so preventing
	new threads from being spawned.
 5. Move executor related code to event_executor.cc and .h
 6. Maybe move all allocations during parsing to evex_mem_root thus saving
    double parsing in evex_create_event!
 7. If the server is killed (stopping) try to kill executing events..
 8. What happens if one renames an event in the DB while it is in memory?
     Or even deleting it?
 9. created & modified in the table should be UTC?
 10. Add a lock to event_timed to serialize execution of an event - do not
     allow parallel executions. Hmm, however how last_executed is marked
     then? The call to event_timed::mark_last_executed() must be moved to
     event_timed::execute()?
 11. Consider using conditional variable when doing shutdown instead of
     waiting some time (tries < 5).
 12. Fix event_timed::get_show_create_event.
 13. Add logging to file.
 14. Add function documentation whenever needed.
*/

enum
{
  EVEX_FIELD_DB = 0,
  EVEX_FIELD_NAME,
  EVEX_FIELD_BODY,
  EVEX_FIELD_DEFINER,
  EVEX_FIELD_EXECUTE_AT,  
  EVEX_FIELD_INTERVAL_EXPR,  
  EVEX_FIELD_TRANSIENT_INTERVAL,  
  EVEX_FIELD_CREATED,
  EVEX_FIELD_MODIFIED,
  EVEX_FIELD_LAST_EXECUTED,
  EVEX_FIELD_STARTS,
  EVEX_FIELD_ENDS,
  EVEX_FIELD_STATUS,
  EVEX_FIELD_ON_COMPLETION,
  EVEX_FIELD_COMMENT,
  EVEX_FIELD_COUNT /* a cool trick to count the number of fields :) */
};


#define EVEX_OPEN_TABLE_FOR_UPDATE() \
       open_proc_type_table_for_update(thd, "event", &mysql_event_table_exists)

static bool mysql_event_table_exists= 1;
static DYNAMIC_ARRAY events_array;
static DYNAMIC_ARRAY evex_executing_queue;
static MEM_ROOT evex_mem_root;
static uint workers_count;
static bool evex_is_running= false;
static pthread_mutex_t LOCK_event_arrays,
                       LOCK_workers_count,
                       LOCK_evex_running;

extern int yyparse(void *thd);

ulong opt_event_executor;
my_bool event_executor_running_global_var= false;

extern ulong thread_created;
//extern volatile uint thread_running;
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////     Static functions follow ///////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////



/* NOTE Andrey: Document better
  Compares two TIME structures.

  a > b ->  1
  a = b ->  0
  a < b -> -1
*/

static inline int
my_time_compare(TIME *a, TIME *b)
{
/*
 Or maybe it is faster to use TIME_to_ulonglong_datetime
 for "a" and "b"
*/

  DBUG_ENTER("my_time_compare");
  if (a->year > b->year)
    DBUG_RETURN(1);
  
  if (a->year < b->year)
    DBUG_RETURN(-1);

  if (a->month > b->month)
    DBUG_RETURN(1);
  
  if (a->month < b->month)
    DBUG_RETURN(-1);

  if (a->day > b->day)
    DBUG_RETURN(1);
  
  if (a->day < b->day)
    DBUG_RETURN(-1);

  if (a->hour > b->hour)
    DBUG_RETURN(1);
  
  if (a->hour < b->hour)
    DBUG_RETURN(-1);

  if (a->minute > b->minute)
    DBUG_RETURN(1);
  
  if (a->minute < b->minute)
    DBUG_RETURN(-1);

  if (a->second > b->second)
    DBUG_RETURN(1);
  
  if (a->second < b->second)
    DBUG_RETURN(-1);

 /*!!  second_part is not compared !*/

  DBUG_RETURN(0);
}


static int
event_timed_compare(event_timed **a, event_timed **b)
{
  return my_time_compare(&(*a)->m_execute_at, &(*b)->m_execute_at);
/*
  if (a->sort > b->sort)
    return -1;
  if (a->sort < b->sort)
    return 1;
  return 0;
*/
}


/*
   Puts some data common to CREATE and ALTER EVENT into a row.

   SYNOPSIS
     evex_fill_row()
       thd    THD
       table  the row to fill out
       et     Event's data
   
   DESCRIPTION 
     Used both when an event is created and when it is altered.
*/

static int
evex_fill_row(THD *thd, TABLE *table, event_timed *et)
{
  DBUG_ENTER("evex_fill_row");
  int ret=0;

  if (table->s->fields != EVEX_FIELD_COUNT)
    goto get_field_failed;

  DBUG_PRINT("info", ("m_db.len=%d",et->m_db.length));  
  DBUG_PRINT("info", ("m_name.len=%d",et->m_name.length));  

  table->field[EVEX_FIELD_DB]->
      store(et->m_db.str, et->m_db.length, system_charset_info);
  table->field[EVEX_FIELD_NAME]->
      store(et->m_name.str, et->m_name.length, system_charset_info);

  table->field[EVEX_FIELD_ON_COMPLETION]->set_notnull();
  table->field[EVEX_FIELD_ON_COMPLETION]->store((longlong)et->m_on_completion);

  table->field[EVEX_FIELD_STATUS]->set_notnull();
  table->field[EVEX_FIELD_STATUS]->store((longlong)et->m_status);
  et->m_status_changed= false;

  // ToDo: Andrey. How to use users current charset?
  if (et->m_body.str)
    table->field[EVEX_FIELD_BODY]->
      store(et->m_body.str, et->m_body.length, system_charset_info);

  if (et->m_starts.year)
  {
    table->field[EVEX_FIELD_STARTS]->set_notnull();// set NULL flag to OFF
    table->field[EVEX_FIELD_STARTS]->store_time(&et->m_starts,MYSQL_TIMESTAMP_DATETIME);    
  }	   

  if (et->m_ends.year)
  {
    table->field[EVEX_FIELD_ENDS]->set_notnull();
    table->field[EVEX_FIELD_ENDS]->store_time(&et->m_ends, MYSQL_TIMESTAMP_DATETIME);    
  }
   
  if (et->m_expr)
  {
    // m_expr was fixed in init_interval
//    et->m_expr->save_in_field(table->field[EVEX_FIELD_INTERVAL_EXPR], (my_bool)TRUE);
	  
    table->field[EVEX_FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[EVEX_FIELD_INTERVAL_EXPR]->store((longlong)et->m_expr);

    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    // in the enum (C) intervals start from 0 but in mysql enum valid values start
    // from 1. Thus +1 offset is needed!
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong)et->m_interval + 1);
  }
  else if (et->m_execute_at.year)
  {
    // fix_fields already called in init_execute_at
    table->field[EVEX_FIELD_EXECUTE_AT]->set_notnull();
    table->field[EVEX_FIELD_EXECUTE_AT]->store_time(&et->m_execute_at, MYSQL_TIMESTAMP_DATETIME);    
    
	//this will make it NULL because we don't call set_notnull
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong) 0);  
  }
  else
  {
    // it is normal to be here when the action is update
    // this is an error if the action is create. something is borked
  }
    
  ((Field_timestamp *)table->field[EVEX_FIELD_MODIFIED])->set_time();

  if ((et->m_comment).length)
    table->field[EVEX_FIELD_COMMENT]->
	store((et->m_comment).str, (et->m_comment).length, system_charset_info);

  DBUG_RETURN(0);  
parse_error:
  DBUG_RETURN(EVEX_PARSE_ERROR);
general_error:
  DBUG_RETURN(EVEX_GENERAL_ERROR);
get_field_failed:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


/*
   Creates an event in mysql.event

   SYNOPSIS
     db_create_event()
       thd  THD
       et   event_timed object containing information for the event 
   
   DESCRIPTION 
     Creates an event. Relies on evex_fill_row which is shared with 
     db_update_event. The name of the event is inside "et".
*/

static int
db_create_event(THD *thd, event_timed *et)
{
  int ret;
  TABLE *table;
  TABLE_LIST tables;
  char definer[HOSTNAME_LENGTH+USERNAME_LENGTH+2];
  char olddb[128];
  bool dbchanged;
  DBUG_ENTER("db_create_event");
  DBUG_PRINT("enter", ("name: %.*s", et->m_name.length, et->m_name.str));

  dbchanged= false;
  if ((ret= sp_use_new_db(thd, et->m_db.str, olddb, sizeof(olddb),
			  0, &dbchanged)))
  {
    DBUG_PRINT("info", ("cannot use_new_db. code=%d", ret));
    DBUG_RETURN(EVEX_NO_DB_ERROR);
  }

  bzero(&tables, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.table_name= tables.alias= (char*)"event";

  if (!(table= EVEX_OPEN_TABLE_FOR_UPDATE()))
  {
    if (dbchanged)
      (void)mysql_change_db(thd, olddb, 1);
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  }

  restore_record(table, s->default_values); // Get default values for fields
  strxmov(definer, et->m_definer_user.str, "@", et->m_definer_host.str, NullS);

  if (table->s->fields != EVEX_FIELD_COUNT)
  {
    ret= EVEX_GET_FIELD_FAILED;
    goto done;
  }
/* TODO : Uncomment these and add handling in sql_parse.cc or here

  if (sp->m_name.length > table->field[MYSQL_PROC_FIELD_NAME]->field_length)
  {
    ret= SP_BAD_IDENTIFIER;
    goto done;
  }
  if (sp->m_body.length > table->field[MYSQL_PROC_FIELD_BODY]->field_length)
  {
    ret= SP_BODY_TOO_LONG;
    goto done;
  }
*/
  if (!(et->m_expr) && !(et->m_execute_at.year))
  {
    DBUG_PRINT("error", ("neither m_expr nor m_execute_as is set!"));
    ret= EVEX_WRITE_ROW_FAILED;
    goto done;
  }
  ret= table->field[EVEX_FIELD_DEFINER]->
       store(definer, (uint)strlen(definer), system_charset_info);
  if (ret)
  {
    ret= EVEX_PARSE_ERROR;
    goto done;
  }
    
  ((Field_timestamp *)table->field[EVEX_FIELD_CREATED])->set_time();
  if ((ret= evex_fill_row(thd, table, et)))
    goto done; 

  ret= EVEX_OK;
  if (table->file->write_row(table->record[0]))
    ret= EVEX_WRITE_ROW_FAILED;
  else if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    /* Such a statement can always go directly to binlog, no trans cache */
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
    mysql_bin_log.write(&qinfo);
  }

done:
  close_thread_tables(thd);
  if (dbchanged)
    (void)mysql_change_db(thd, olddb, 1);
  DBUG_RETURN(ret);
}


/*
   Used to execute ALTER EVENT

   SYNOPSIS
     db_update_event()
       thd      THD
       sp_name  the name of the event to alter
       et       event's data
   
   NOTES
     sp_name is passed since this is the name of the event to
     alter in case of RENAME TO.
*/

static int
db_update_event(THD *thd, sp_name *name, event_timed *et)
{
  TABLE *table;
  int ret;
  DBUG_ENTER("db_update_event");
  DBUG_PRINT("enter", ("name: %.*s", et->m_name.length, et->m_name.str));
  if (name)
    DBUG_PRINT("enter", ("rename to: %.*s", name->m_name.length, name->m_name.str));

  // Todo: Handle in sql_prepare.cc SP_OPEN_TABLE_FAILED
  if (!(table= EVEX_OPEN_TABLE_FOR_UPDATE()))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  ret= sp_db_find_routine_aux(thd, 0/*notype*/, et->m_db, et->m_name, table);
  if (ret == EVEX_OK)
  {
    store_record(table,record[1]);
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET; // Don't update create on row update.
    ret= evex_fill_row(thd, table, et);
    if (ret)
      goto done;
    
    if (name)
    {    
      table->field[EVEX_FIELD_DB]->
        store(name->m_db.str, name->m_db.length, system_charset_info);
      table->field[EVEX_FIELD_NAME]->
        store(name->m_name.str, name->m_name.length, system_charset_info);
    }

    if ((table->file->update_row(table->record[1],table->record[0])))
      ret= EVEX_WRITE_ROW_FAILED;
  }
done:
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}

/*
    Use sp_name for look up, return in **ett if found
*/
static int
db_find_event(THD *thd, sp_name *name, event_timed **ett)
{
  TABLE *table;
  int ret;
  const char *definer;
  char *ptr;
  event_timed *et;  
  Open_tables_state open_tables_state_backup;
  DBUG_ENTER("db_find_event");
  DBUG_PRINT("enter", ("name: %*s",
		       name->m_name.length, name->m_name.str));


  if (!(table= open_proc_type_table_for_read(thd, &open_tables_state_backup,
                                             "event", &mysql_event_table_exists)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if ((ret= sp_db_find_routine_aux(thd, 0/*notype*/, name->m_db, name->m_name,
                                   table)) != SP_OK)
    goto done;

  et= new event_timed;
  
  /*
    The table should not be closed beforehand. 
    ::load_from_row only loads and does not compile
  */
  if ((ret= et->load_from_row(&evex_mem_root, table)))
    goto done;

done:
  if (ret && et)
  {
    delete et;
    et= 0;
  }
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&open_tables_state_backup);
  *ett= et;
  DBUG_RETURN(ret);
}


static int
evex_load_and_compile_event(THD * thd, sp_name *spn, bool use_lock)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root;
  event_timed *ett, *ett_copy;

  DBUG_ENTER("db_load_and_compile_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  tmp_mem_root= thd->mem_root;
  thd->mem_root= &evex_mem_root;

  if (db_find_event(thd, spn, &ett))
  {
    ret= EVEX_GENERAL_ERROR;
    goto done;
  }

  /*
    allocate on evex_mem_root. call without evex_mem_root and
    and m_sphead will not be cleared!
  */
  if ((ret= ett->compile(thd, &evex_mem_root)))
  {
    thd->mem_root= tmp_mem_root;
    goto done;
  }
  
  ett->compute_next_execution_time();
  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));

  VOID(push_dynamic(&events_array,(gptr) ett));
  ett_copy= dynamic_element(&events_array, events_array.elements - 1,
                            event_timed*);
  VOID(push_dynamic(&evex_executing_queue, (gptr) &ett_copy));

  /*
    There is a copy in the array which we don't need. m_sphead won't be
    destroyed.
  */
  ett->m_free_sphead_on_delete= false;
  delete ett;

  /*
    We find where the first element resides in the arraay. And then do a
    qsort of events_array.elements (the current number of elements).
    We know that the elements are stored in a contiguous block w/o holes.
  */
  qsort((gptr) dynamic_element(&evex_executing_queue, 0, event_timed**),
                               evex_executing_queue.elements,
                               sizeof(event_timed **),
                               (qsort_cmp) event_timed_compare);

  if (use_lock)
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));

done:
  if (thd->mem_root != tmp_mem_root)
    thd->mem_root= tmp_mem_root;  

  if (spn) 
    delete spn;
  DBUG_RETURN(ret);
}


static int
evex_remove_from_cache(LEX_STRING *db, LEX_STRING *name, bool use_lock)
{
  uint i;
  int ret= 0;
  bool need_second_pass= true;

  DBUG_ENTER("evex_remove_from_cache");
  /*
    It is possible that 2 (or 1) pass(es) won't find the event in memory.
    The reason is that DISABLED events are not cached.
  */

  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));
 
  for (i= 0; i < evex_executing_queue.elements; ++i)
  {
    event_timed **p_et= dynamic_element(&evex_executing_queue, i, event_timed**);
    event_timed *ett= *p_et;
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?",db->str,name->str,
                ett->m_db.str, ett->m_name.str));
    if (name->length == ett->m_name.length &&
        db->length == ett->m_db.length &&
        0 == strncmp(db->str, ett->m_db.str, db->length) &&
        0 == strncmp(name->str, ett->m_name.str, name->length)
       )
    {
      int idx;
      //we are lucky the event is in the executing queue, no need of second pass
      need_second_pass= false;
      idx= get_index_dynamic(&events_array, (gptr) ett);
      if (idx != -1)
      {
        //destruct first and then remove. the destructor will delete sp_head
        ett->free_sp();
        delete_dynamic_element(&events_array, idx);
        delete_dynamic_element(&evex_executing_queue, i);
      }
      else
      {
        //this should never happen
        DBUG_PRINT("error", ("Sth weird with get_index_dynamic. %d."
               "i=%d idx=%d evex_ex_queue.buf=%p evex_ex_queue.elements=%d ett=%p\n"
               "events_array=%p events_array.elements=%d events_array.buf=%p\n"
               "p_et=%p ett=%p",
               __LINE__, i, idx, &evex_executing_queue.buffer,
               evex_executing_queue.elements, ett, &events_array,
               events_array.elements, events_array.buffer, p_et, ett));
        ret= EVEX_GENERAL_ERROR;
        goto done;
      }
      //ok we have cleaned
    }
  }

  /*
    ToDo Andrey : Think about whether second pass is needed. All events
                  that are in memory are enabled. If an event is being
                  disabled (by a SQL stmt) it will be uncached. Hmm...
                  However is this true for events that has been 
                  disabled because of another reason like - no need
                  to be executed because ENDS is in the past?
                  For instance, second_pass is needed when an event
                  was created as DISABLED but then altered as ENABLED.
  */
  if (need_second_pass)
    //we haven't found the event in the executing queue. This is nice! :)
    //Look for it in the events_array.
    for (i= 0; i < events_array.elements; ++i)
    {
      event_timed *ett= dynamic_element(&events_array, i, event_timed*);

      if (name->length == ett->m_name.length &&
          db->length == ett->m_db.length &&
          0 == strncmp(db->str, ett->m_db.str, db->length) &&
          0 == strncmp(name->str, ett->m_name.str, name->length)
         )
        delete_dynamic_element(&events_array, i);
    } 

done:
  if (use_lock)
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));

  DBUG_RETURN(ret);
}


/*
                      Exported functions follow 
*/

/*
   The function exported to the world for creating of events.

   SYNOPSIS
     evex_create_event()
       thd            THD
       et             event's data
       create_options Options specified when in the query. We are
                      interested whether there is IF NOT EXISTS
          
   NOTES
     - in case there is an event with the same name (db) and 
       IF NOT EXISTS is specified, an warning is put into the W stack.
   TODO
     - Add code for in-memory structures - caching & uncaching.
*/

int
evex_create_event(THD *thd, event_timed *et, uint create_options)
{
  int ret = 0;
  sp_name *spn= 0;

  DBUG_ENTER("evex_create_event");
  DBUG_PRINT("enter", ("name: %*s options:%d", et->m_name.length,
                et->m_name.str, create_options));

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // TODO: put an warning to the user here.
  //       Is it needed? (Andrey, 051129)
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));


  if ((ret = db_create_event(thd, et)) == EVEX_WRITE_ROW_FAILED && 
        (create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		      ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS),
		      "EVENT", thd->lex->et->m_name.str);
    ret= 0;
    goto done;
  }
  /*
    A warning is thrown only when create_options is set to 
    HA_LEX_CREATE_IF_NOT_EXISTS. In this case if EVEX_WRITE_ROW_FAILED,
    which means that we have duplicated key -> warning. In all
    other cases -> error.
  */
  if (ret)
    goto done;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  {
    VOID(pthread_mutex_unlock(&LOCK_evex_running));
    goto done;
  }  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  
  //cache only if the event is ENABLED
  if (et->m_status == MYSQL_EVENT_ENABLED)
  {
    spn= new sp_name(et->m_db, et->m_name);
    if ((ret= evex_load_and_compile_event(thd, spn, true)))
      goto done;
  }

done:
  if (spn) 
    delete spn;
  DBUG_RETURN(ret);
}


/*
   The function exported to the world for alteration of events.

   SYNOPSIS
     evex_update_event()
       thd     THD
       name    the real name of the event.    
       et      event's data
          
   NOTES
     et contains data about dbname and event name. 
     name is the new name of the event. if not null this means
     that RENAME TO was specified in the query.
   TODO
     - Add code for in-memory structures - caching & uncaching.
*/

int
evex_update_event(THD *thd, sp_name *name, event_timed *et)
{
  int ret, i;
  bool need_second_pass= true;
  sp_name *spn= 0;

  DBUG_ENTER("evex_update_event");
  DBUG_PRINT("enter", ("name: %*s", et->m_name.length, et->m_name.str));

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // put an warning to the user here
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  
  if ((ret= db_update_event(thd, name, et)))
    goto done_no_evex;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // not running - therefore no memory structures
    goto done_no_evex;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  /*
    It is possible that 2 (or 1) pass(es) won't find the event in memory.
    The reason is that DISABLED events are not cached.
  */
  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  if (name)
  {
    evex_remove_from_cache(&name->m_db, &name->m_name, false);
    if (et->m_status == MYSQL_EVENT_ENABLED &&
        (ret= evex_load_and_compile_event(thd, name, false))
       )
      goto done;
  }
  else
  {
    evex_remove_from_cache(&et->m_db, &et->m_name, false);
    spn= new sp_name(et->m_db, et->m_name);
    if (et->m_status == MYSQL_EVENT_ENABLED &&
        (ret= evex_load_and_compile_event(thd, spn, false))
        )
    {
      delete spn;
      goto done;
    } 
  }

done:
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));

done_no_evex:
  DBUG_RETURN(ret);
}


/*
   Checks for existance of a specified event

   SYNOPSIS
     evex_event_exists()
       thd     THD
       et      event's name
          
   NOTES
     Currently unused
*/

bool
evex_event_exists(THD *thd, event_timed *et)
{
  TABLE *table;
  int ret;
  bool opened= FALSE;
  Open_tables_state open_tables_state_backup;
  DBUG_ENTER("evex_event_exists");

  if (!(table= open_proc_type_table_for_read(thd, &open_tables_state_backup,
                                             "event", &mysql_event_table_exists)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  ret= sp_db_find_routine_aux(thd, 0/*notype*/, et->m_db, et->m_name, table);

  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(&open_tables_state_backup);
  thd->clear_error();

  DBUG_RETURN(ret == SP_OK);
}


/*
 Drops an event

 SYNOPSIS
   evex_drop_event()
     thd             THD
     et              event's name
     drop_if_exists  if set and the event not existing => warning onto the stack
          
 TODO
   Update in-memory structures
*/

int
evex_drop_event(THD *thd, event_timed *et, bool drop_if_exists)
{
  TABLE *table;
  int ret;
  bool opened;
  DBUG_ENTER("evex_drop_event");

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // put an warning to the user here
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  if (!(table= EVEX_OPEN_TABLE_FOR_UPDATE()))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  ret= sp_db_find_routine_aux(thd, 0/*notype*/, et->m_db, et->m_name, table);

  if (ret == EVEX_OK)
  {
    if (table->file->delete_row(table->record[0]))
    { 	
      ret= EVEX_DELETE_ROW_FAILED;
      goto done;
    }
  }
  else if (ret == SP_KEY_NOT_FOUND && drop_if_exists)
  {
     push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		    ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
		    "EVENT", thd->lex->et->m_name.str);
     ret= 0;
     goto done;
  } else
    goto done;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (evex_is_running)
    ret= evex_remove_from_cache(&et->m_db, &et->m_name, true);
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:  
  /* 
    "opened" is switched to TRUE when we open mysql.event for checking.
    In this case we have to close the table after finishing working with it.
  */
  close_thread_tables(thd);

  DBUG_RETURN(ret);
}


/*
  !!! This one is executor related so maybe moving it to
  event_executor.cc is a good idea or ?
*/
static int
evex_load_events_from_db(THD *thd)
{
  TABLE *table;
  READ_RECORD read_record_info;
  MYSQL_LOCK *lock;
  Open_tables_state open_tables_state_backup;
  int ret= -1;
  
  DBUG_ENTER("evex_load_events_from_db");  

  if (!(table= open_proc_type_table_for_read(thd, &open_tables_state_backup,
                                             "event", &mysql_event_table_exists)))
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
    
    if (et->load_from_row(&evex_mem_root, table))
      //error loading!
      continue;
    
    DBUG_PRINT("evex_load_events_from_db",
            ("Event %s loaded from row. Time to compile", et->m_name.str));
    
    if (et->compile(thd, &evex_mem_root))
      //problem during compile
      continue;
    // let's find when to be executed  
    et->compute_next_execution_time();
    
    DBUG_PRINT("evex_load_events_from_db",
                ("Adding %s to the executor list.", et->m_name.str));
    VOID(push_dynamic(&events_array,(gptr) et));
    // we always add at the end so the number of elements - 1 is the place
    // in the buffer
    et_copy= dynamic_element(&events_array, events_array.elements - 1,
                    event_timed*);
    VOID(push_dynamic(&evex_executing_queue,(gptr) &et_copy));
    et->m_free_sphead_on_delete= false;
    DBUG_PRINT("info", (""));
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
  thd->restore_backup_open_tables_state(&open_tables_state_backup);

  DBUG_PRINT("evex_load_events_from_db",
                    ("Events loaded from DB. Status code %d", ret));
  DBUG_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////     EVENT_TIMED class /////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


/*
 Init all member variables

 SYNOPSIS
   event_timed::init()
*/

void
event_timed::init()
{
  DBUG_ENTER("event_timed::init");

  m_qname.str= m_db.str= m_name.str= m_body.str= m_comment.str= 0;
  m_qname.length= m_db.length= m_name.length= m_body.length= m_comment.length= 0;
  
  set_zero_time(&m_starts, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&m_ends, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&m_execute_at, MYSQL_TIMESTAMP_DATETIME);
  set_zero_time(&m_last_executed, MYSQL_TIMESTAMP_DATETIME);

  m_definer_user.str= m_definer_host.str= 0;
  m_definer_user.length= m_definer_host.length= 0;
    
  DBUG_VOID_RETURN;
}


/*
 Set a name of the event

 SYNOPSIS
   event_timed::init_name()
     thd   THD
     name  the name extracted in the parser
*/

void
event_timed::init_name(THD *thd, sp_name *name)
{
  DBUG_ENTER("event_timed::init_name");
  uint n;			/* Counter for nul trimming */ 
  /* During parsing, we must use thd->mem_root */
  MEM_ROOT *root= thd->mem_root;

  /* We have to copy strings to get them into the right memroot */
  if (name)
  {
    m_db.length= name->m_db.length;
    if (name->m_db.length == 0)
      m_db.str= NULL;
    else
      m_db.str= strmake_root(root, name->m_db.str, name->m_db.length);
    m_name.length= name->m_name.length;
    m_name.str= strmake_root(root, name->m_name.str, name->m_name.length);

    if (name->m_qname.length == 0)
      name->init_qname(thd);
    m_qname.length= name->m_qname.length;
    m_qname.str= strmake_root(root, name->m_qname.str, m_qname.length);
  }
  else if (thd->db)
  {
    m_db.length= thd->db_length;
    m_db.str= strmake_root(root, thd->db, m_db.length);
  }
  
  DBUG_PRINT("m_db", ("len=%d db=%s",m_db.length, m_db.str));  
  DBUG_PRINT("m_name", ("len=%d name=%s",m_name.length, m_name.str));  

  DBUG_VOID_RETURN;
}


/*
 Set body of the event - what should be executed.

 SYNOPSIS
   event_timed::init_body()
     thd   THD

  NOTE
    The body is extracted by copying all data between the
    start of the body set by another method and the current pointer in Lex.
*/

void
event_timed::init_body(THD *thd)
{
  DBUG_ENTER("event_timed::init_body");
  MEM_ROOT *root= thd->mem_root;

  m_body.length= thd->lex->ptr - m_body_begin;
  // Trim nuls at the end 
  while (m_body.length && m_body_begin[m_body.length-1] == '\0')
    m_body.length--;

  m_body.str= strmake_root(root, (char *)m_body_begin, m_body.length);

  DBUG_VOID_RETURN;
}


/*
 Set time for execution for one time events.

 SYNOPSIS
   event_timed::init_execute_at()
     expr   when (datetime)

 RETURNS
   0 - OK
   EVEX_PARSE_ERROR - fix_fields failed
   EVEX_BAD_PARAMS  - datetime is in the past
*/

int
event_timed::init_execute_at(THD *thd, Item *expr)
{
  my_bool not_used;
  TIME ltime;
  my_time_t my_time_tmp;

  TIME time_tmp;
  DBUG_ENTER("event_timed::init_execute_at");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  if (expr->val_int() == MYSQL_TIMESTAMP_ERROR)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  // let's check whether time is in the past
  thd->variables.time_zone->gmt_sec_to_TIME(&time_tmp, 
                              (my_time_t) thd->query_start()); 

  if (expr->val_int() < TIME_to_ulonglong_datetime(&time_tmp))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  if ((not_used= expr->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
      This may result in a 1970-01-01 date if ltime is > 2037-xx-xx
      CONVERT_TZ has similar problem
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd,&ltime, &not_used));


  m_execute_at= ltime;
  DBUG_RETURN(0);
}


/*
 Set time for execution for transient events.

 SYNOPSIS
   event_timed::init_interval()
     expr      how much?
     interval  what is the interval

 RETURNS
   0 - OK
   EVEX_PARSE_ERROR - fix_fields failed
   EVEX_BAD_PARAMS  - Interval is not positive
*/

int
event_timed::init_interval(THD *thd, Item *expr, interval_type interval)
{
  longlong tmp;
  DBUG_ENTER("event_timed::init_interval");

  if (expr->fix_fields(thd, &expr))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  if ((tmp= expr->val_int()) <= 0)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  m_expr= tmp;
  m_interval= interval;
  DBUG_RETURN(0);
}


/*
 Set activation time.

 SYNOPSIS
   event_timed::init_starts()
     expr      how much?
     interval  what is the interval

 NOTES
  Note that activation time is not execution time.
  EVERY 5 MINUTE STARTS "2004-12-12 10:00:00" means that
  the event will be executed every 5 minutes but this will
  start at the date shown above. Expressions are possible :
  DATE_ADD(NOW(), INTERVAL 1 DAY)  -- start tommorow at
  same time.

 RETURNS
   0 - OK
   EVEX_PARSE_ERROR - fix_fields failed
*/

int
event_timed::init_starts(THD *thd, Item *starts)
{
  my_bool not_used;
  TIME ltime;
  my_time_t my_time_tmp;

  DBUG_ENTER("event_timed::init_starts");

  if (starts->fix_fields(thd, &starts))
    DBUG_RETURN(EVEX_PARSE_ERROR);

  if (starts->val_int() == MYSQL_TIMESTAMP_ERROR)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  if ((not_used= starts->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
      This may result in a 1970-01-01 date if ltime is > 2037-xx-xx
      CONVERT_TZ has similar problem
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd,&ltime, &not_used));

  m_starts= ltime;
  DBUG_RETURN(0);
}


/*
 Set deactivation time.

 SYNOPSIS
   event_timed::init_ends()
     thd      THD
     ends  when?

 NOTES
  Note that activation time is not execution time.
  EVERY 5 MINUTE ENDS "2004-12-12 10:00:00" means that
  the event will be executed every 5 minutes but this will
  end at the date shown above. Expressions are possible :
  DATE_ADD(NOW(), INTERVAL 1 DAY)  -- end tommorow at
  same time.

 RETURNS
   0 - OK
   EVEX_PARSE_ERROR - fix_fields failed
   EVEX_BAD_PARAMS  - ENDS before STARTS
*/

int 
event_timed::init_ends(THD *thd, Item *ends)
{
  TIME ltime;
  my_time_t my_time_tmp;
  my_bool not_used;

  DBUG_ENTER("event_timed::init_ends");

  if (ends->fix_fields(thd, &ends))
    DBUG_RETURN(EVEX_PARSE_ERROR);

    // the field was already fixed in init_ends
  if ((not_used= ends->get_date(&ltime, TIME_NO_ZERO_DATE)))
    DBUG_RETURN(EVEX_BAD_PARAMS);

  /*
    This may result in a 1970-01-01 date if ltime is > 2037-xx-xx
    CONVERT_TZ has similar problem
  */
  my_tz_UTC->gmt_sec_to_TIME(&ltime, TIME_to_timestamp(thd, &ltime, &not_used));
 
  if (m_starts.year && my_time_compare(&m_starts, &ltime) != -1)
    DBUG_RETURN(EVEX_BAD_PARAMS);

  m_ends= ltime;
  DBUG_RETURN(0);
}


/*
 Set behaviour when ENDS has been set and passed by.

 SYNOPSIS
   event_timed::init_interval()
     drop  if set then drop the event otherwise preserve it.
*/

void
event_timed::set_on_completion_drop(bool drop)
{
  DBUG_ENTER("event_timed::set_on_completion");
  if (drop) 
    m_on_completion= MYSQL_EVENT_ON_COMPLETION_DROP;
  else 
    m_on_completion= MYSQL_EVENT_ON_COMPLETION_PRESERVE;

  DBUG_VOID_RETURN;
}


/*
 Sets event's status. DISABLED - not executable even if
 everything else is ok (STARTS, ENDS, INTERVAL and so on).

 SYNOPSIS
   event_timed::set_event_status()
     enabled  set whether enabled or not.
*/

void
event_timed::set_event_status(bool enabled)
{
  DBUG_ENTER("event_timed::set_on_completion");

  m_status_changed= true;
  if (enabled) 
    m_status= MYSQL_EVENT_ENABLED;
  else 
    m_status= MYSQL_EVENT_DISABLED;

  DBUG_VOID_RETURN;
}


/*
 Sets comment.

 SYNOPSIS
   event_timed::init_comment()
     thd      THD - used for memory allocation
     comment  the string.
*/

void
event_timed::init_comment(THD *thd, LEX_STRING *comment)
{
  DBUG_ENTER("event_timed::init_comment");

  MEM_ROOT *root= thd->mem_root;
  m_comment.length= comment->length;
  m_comment.str= strmake_root(root, comment->str, comment->length);
  DBUG_PRINT("m_comment", ("len=%d",m_comment.length));

  DBUG_VOID_RETURN;
}


/*
 Inits definer (m_definer_user and m_definer_host) during
 parsing.

 SYNOPSIS
   event_timed::init_definer()
*/

int
event_timed::init_definer(THD *thd)
{
  DBUG_ENTER("event_timed::init_definer");

  m_definer_user.str= strdup_root(thd->mem_root, thd->security_ctx->priv_user);
  m_definer_user.length= strlen(thd->security_ctx->priv_user);

  m_definer_host.str= strdup_root(thd->mem_root, thd->security_ctx->priv_host);
  m_definer_host.length= strlen(thd->security_ctx->priv_host);

  DBUG_RETURN(0);
}


/*
 Loads an event from a row from mysql.event
 
 SYNOPSIS
   event_timed::load_from_row()
*/

int
event_timed::load_from_row(MEM_ROOT *mem_root, TABLE *table)
{
  longlong created;
  longlong modified;
  char *ptr;
  event_timed *et;
  uint len;
  bool res1, res2;

  DBUG_ENTER("event_timed::load_from_row");

  if (!table)
    goto error;

  et= this;
  
  if (table->s->fields != EVEX_FIELD_COUNT)
    goto error;

  if ((et->m_db.str= get_field(mem_root,
                          table->field[EVEX_FIELD_DB])) == NULL)
    goto error;

  et->m_db.length= strlen(et->m_db.str);

  if ((et->m_name.str= get_field(mem_root,
                          table->field[EVEX_FIELD_NAME])) == NULL)
    goto error;

  et->m_name.length= strlen(et->m_name.str);

  if ((et->m_body.str= get_field(mem_root,
                          table->field[EVEX_FIELD_BODY])) == NULL)
    goto error;

  et->m_body.length= strlen(et->m_body.str);

  if ((et->m_definer.str= get_field(mem_root,
                          table->field[EVEX_FIELD_DEFINER])) == NullS)
    goto error;
  et->m_definer.length= strlen(et->m_definer.str);

  ptr= strchr(et->m_definer.str, '@');

  if (! ptr)
    ptr= et->m_definer.str;		// Weird, isn't it?

  len= ptr - et->m_definer.str;

  et->m_definer_user.str= strmake_root(mem_root, et->m_definer.str, len);
  et->m_definer_user.length= len;
  len= et->m_definer.length - len - 1; //1 is because of @
  et->m_definer_host.str= strmake_root(mem_root, ptr + 1, len);//1: because of @
  et->m_definer_host.length= len;
  
  
  res1= table->field[EVEX_FIELD_STARTS]->
                     get_date(&et->m_starts, TIME_NO_ZERO_DATE);

  res2= table->field[EVEX_FIELD_ENDS]->
                     get_date(&et->m_ends, TIME_NO_ZERO_DATE);
  
  et->m_expr= table->field[EVEX_FIELD_INTERVAL_EXPR]->val_int();

  /*
    If res1 and res2 are true then both fields are empty.
	Hence if EVEX_FIELD_EXECUTE_AT is empty there is an error.
  */
  if (res1 && res2 && !et->m_expr && table->field[EVEX_FIELD_EXECUTE_AT]->
                get_date(&et->m_execute_at, TIME_NO_ZERO_DATE))
    goto error;

  /*
    In DB the values start from 1 but enum interval_type starts
    from 0
  */
  et->m_interval= (interval_type)
       ((ulonglong) table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->val_int() - 1);

  et->m_modified= table->field[EVEX_FIELD_CREATED]->val_int();
  et->m_created= table->field[EVEX_FIELD_MODIFIED]->val_int();

  /*
    ToDo Andrey : Ask PeterG & Serg what to do in this case.
                  Whether on load last_executed_at should be loaded
                  or it must be 0ed. If last_executed_at is loaded
                  then an event can be scheduled for execution
                  instantly. Let's say an event has to be executed
                  every 15 mins. The server has been stopped for
                  more than this time and then started. If L_E_AT
                  is loaded from DB, execution at L_E_AT+15min
                  will be scheduled. However this time is in the past.
                  Hence immediate execution. Due to patch of
                  ::mark_last_executed() m_last_executed gets time_now
                  and not m_execute_at. If not like this a big
                  queue can be scheduled for times which are still in
                  the past (2, 3 and more executions which will be
                  consequent).
  */
  set_zero_time(&m_last_executed, MYSQL_TIMESTAMP_DATETIME);
#ifdef ANDREY_0
  table->field[EVEX_FIELD_LAST_EXECUTED]->
                     get_date(&et->m_last_executed, TIME_NO_ZERO_DATE);
#endif
  m_last_executed_changed= false;

  // ToDo : Andrey . Find a way not to allocate ptr on event_mem_root
  if ((ptr= get_field(mem_root, table->field[EVEX_FIELD_STATUS])) == NullS)
    goto error;
  
  DBUG_PRINT("load_from_row", ("Event [%s] is [%s]", et->m_name.str, ptr));
  et->m_status= (ptr[0]=='E'? MYSQL_EVENT_ENABLED:
                                     MYSQL_EVENT_DISABLED);

  // ToDo : Andrey . Find a way not to allocate ptr on event_mem_root
  if ((ptr= get_field(mem_root,
                  table->field[EVEX_FIELD_ON_COMPLETION])) == NullS)
    goto error;

  et->m_on_completion= (ptr[0]=='D'? MYSQL_EVENT_ON_COMPLETION_DROP:
                                     MYSQL_EVENT_ON_COMPLETION_PRESERVE);

  et->m_comment.str= get_field(mem_root, table->field[EVEX_FIELD_COMMENT]);
  if (et->m_comment.str != NullS)
    et->m_comment.length= strlen(et->m_comment.str);
  else
    et->m_comment.length= 0;
    
  DBUG_RETURN(0);
error:
  DBUG_RETURN(EVEX_GET_FIELD_FAILED);
}


bool
event_timed::compute_next_execution_time()
{
  TIME time_now;
  my_time_t now;
  int tmp;

  DBUG_ENTER("event_timed::compute_next_execution_time");

  if (m_status == MYSQL_EVENT_DISABLED)
  {
    DBUG_PRINT("compute_next_execution_time",
                  ("Event %s is DISABLED", m_name.str));
    goto ret;
  }
  //if one-time no need to do computation
  if (!m_expr)
  {
    //let's check whether it was executed
    if (m_last_executed.year)
    {
      DBUG_PRINT("compute_next_execution_time",
                ("One-time event %s was already executed", m_name.str));
      if (m_on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
      {
        DBUG_PRINT("compute_next_execution_time",
                          ("One-time event will be dropped."));
        m_dropped= true;
      }
      m_status= MYSQL_EVENT_DISABLED;
      m_status_changed= true;
    }
    goto ret;
  }
  time(&now);
  my_tz_UTC->gmt_sec_to_TIME(&time_now, now);
/*
  sql_print_information("[%s.%s]", m_db.str, m_name.str);
  sql_print_information("time_now : [%d-%d-%d %d:%d:%d ]", time_now.year, time_now.month, time_now.day, time_now.hour, time_now.minute, time_now.second);
  sql_print_information("m_starts : [%d-%d-%d %d:%d:%d ]", m_starts.year, m_starts.month, m_starts.day, m_starts.hour, m_starts.minute, m_starts.second);
  sql_print_information("m_ends   : [%d-%d-%d %d:%d:%d ]", m_ends.year, m_ends.month, m_ends.day, m_ends.hour, m_ends.minute, m_ends.second);
  sql_print_information("m_last_ex: [%d-%d-%d %d:%d:%d ]", m_last_executed.year, m_last_executed.month, m_last_executed.day, m_last_executed.hour, m_last_executed.minute, m_last_executed.second);
*/
  //if time_now is after m_ends don't execute anymore
  if (m_ends.year && (tmp= my_time_compare(&m_ends, &time_now)) == -1)
  {
    // time_now is after m_ends. don't execute anymore
    set_zero_time(&m_execute_at, MYSQL_TIMESTAMP_DATETIME);
    if (m_on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
      m_dropped= true;
    m_status= MYSQL_EVENT_DISABLED;
    m_status_changed= true;

    goto ret;
  }
  
  /* 
     Here time_now is before or equals m_ends if the latter is set.
     Let's check whether time_now is before m_starts.
     If so schedule for m_starts
  */
  if (m_starts.year && (tmp= my_time_compare(&time_now, &m_starts)) < 1)
  {
    if (tmp == 0 && my_time_compare(&m_starts, &m_last_executed) == 0)
    {
       /*
        time_now = m_starts = m_last_executed
        do nothing or we will schedule for second time execution at m_starts.
      */
    }
    else
    {
      //m_starts is in the future
      //time_now before m_starts. Scheduling for m_starts
      m_execute_at= m_starts;
      goto ret;
    }
  }
  
  if (m_starts.year && m_ends.year)
  {
    /* 
      Both m_starts and m_ends are set and time_now is between them (incl.)
      If m_last_executed is set then increase with m_expr. The new TIME is
      after m_ends set m_execute_at to 0. And check for m_on_completion
      If not set then schedule for now.
    */
    if (!m_last_executed.year)
      m_execute_at= time_now;
    else
    {
      my_time_t last, ll_ends;

      // There was previous execution     
      last= sec_since_epoch_TIME(&m_last_executed) + m_expr;
      ll_ends= sec_since_epoch_TIME(&m_ends);
      //now convert back to TIME
      //ToDo Andrey: maybe check for error here?
      if (ll_ends < last)
      {
        // Next execution after ends. No more executions
        set_zero_time(&m_execute_at, MYSQL_TIMESTAMP_DATETIME);
        if (m_on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
          m_dropped= true;
      }
      else
        my_tz_UTC->gmt_sec_to_TIME(&m_execute_at, last);
    }
    goto ret;
  }
  else if (!m_starts.year && !m_ends.year)
  {
    // both m_starts and m_ends are not set, se we schedule for the next
    // based on m_last_executed
    if (!m_last_executed.year)
       //m_last_executed not set. Schedule the event for now
      m_execute_at= time_now;
    else
      //ToDo Andrey: maybe check for error here?
      my_tz_UTC->gmt_sec_to_TIME(&m_execute_at, 
                   sec_since_epoch_TIME(&m_last_executed) + m_expr);
    goto ret;
  }
  else
  {
    //either m_starts or m_ends is set
    if (m_starts.year)
    {
      /*
        - m_starts is set.
        - m_starts is not in the future according to check made before
        Hence schedule for m_starts + m_expr in case m_last_executed
        is not set, otherwise to m_last_executed + m_expr
      */
      my_time_t last;

      //convert either m_last_executed or m_starts to seconds
      if (m_last_executed.year)
        last= sec_since_epoch_TIME(&m_last_executed) + m_expr;
      else
        last= sec_since_epoch_TIME(&m_starts);

      //now convert back to TIME
      //ToDo Andrey: maybe check for error here?
      my_tz_UTC->gmt_sec_to_TIME(&m_execute_at, last);
    }
    else
    {
      /*
        - m_ends is set
        - m_ends is after time_now or is equal
        Hence check for m_last_execute and increment with m_expr.
        If m_last_executed is not set then schedule for now
      */
      my_time_t last, ll_ends;

      if (!m_last_executed.year)
        m_execute_at= time_now;
      else
      {
        last= sec_since_epoch_TIME(&m_last_executed);
        ll_ends= sec_since_epoch_TIME(&m_ends);
        last+= m_expr;
        //now convert back to TIME
        //ToDo Andrey: maybe check for error here?
        if (ll_ends < last)
        {
          set_zero_time(&m_execute_at, MYSQL_TIMESTAMP_DATETIME);
          if (m_on_completion == MYSQL_EVENT_ON_COMPLETION_DROP)
            m_dropped= true;
        }
        else
          my_tz_UTC->gmt_sec_to_TIME(&m_execute_at, last);
      }
    }
    goto ret;
  }
ret:

  DBUG_RETURN(false);
}


void
event_timed::mark_last_executed()
{
  TIME time_now;
  my_time_t now;

  time(&now);
  my_tz_UTC->gmt_sec_to_TIME(&time_now, now);

  m_last_executed= time_now; // was m_execute_at
#ifdef ANDREY_0
  m_last_executed= m_execute_at;
#endif
  m_last_executed_changed= true;
}


bool
event_timed::drop(THD *thd)
{
  DBUG_ENTER("event_timed::drop");

  if (evex_drop_event(thd, this, false))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


bool
event_timed::update_fields(THD *thd)
{
  TABLE *table;
  int ret= 0;
  bool opened;

  DBUG_ENTER("event_timed::update_time_fields");

  DBUG_PRINT("enter", ("name: %*s", m_name.length, m_name.str));
 
  //no need to update if nothing has changed
  if (!(m_status_changed || m_last_executed_changed))
    goto done;
  
  if (!(table= EVEX_OPEN_TABLE_FOR_UPDATE()))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if ((ret= sp_db_find_routine_aux(thd, 0/*notype*/, m_db, m_name, table)))
    goto done;

  store_record(table,record[1]);
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET; // Don't update create on row update.

  if (m_last_executed_changed)
  {
    table->field[EVEX_FIELD_LAST_EXECUTED]->set_notnull();
    table->field[EVEX_FIELD_LAST_EXECUTED]->store_time(&m_last_executed,
                           MYSQL_TIMESTAMP_DATETIME);
    m_last_executed_changed= false;
  }
  if (m_status_changed)
  {
    table->field[EVEX_FIELD_STATUS]->set_notnull();
    table->field[EVEX_FIELD_STATUS]->store((longlong)m_status);
    m_status_changed= false;
  }
    
  if ((table->file->update_row(table->record[1],table->record[0])))
    ret= EVEX_WRITE_ROW_FAILED;

done:
  close_thread_tables(thd);

  DBUG_RETURN(ret);
}


char *
event_timed::get_show_create_event(THD *thd, uint *length)
{
  char *dst, *ret;
  uint len, tmp_len;

  len = strlen("CREATE EVENT ") + m_db.length + strlen(".") + m_name.length +
        strlen(" ON SCHEDULE ") + strlen("EVERY 5 MINUTE ")
/*
	+ strlen("ON COMPLETION ")
	+ (m_on_completion==MYSQL_EVENT_ON_COMPLETION_DROP?
		         strlen("NOT PRESERVE "):strlen("PRESERVE "))
	+ (m_status==MYSQL_EVENT_ENABLED?
		         strlen("ENABLE "):strlen("DISABLE "))
	+ strlen("COMMENT \"") + m_comment.length + strlen("\" ")
*/
    + strlen("DO ") +
	+ m_body.length + strlen(";");
  
  ret= dst= (char*) alloc_root(thd->mem_root, len);
  memcpy(dst, "CREATE EVENT ", tmp_len= strlen("CREATE EVENT "));
  dst+= tmp_len;
  memcpy(dst, m_db.str, tmp_len=m_db.length);
  dst+= tmp_len;
  memcpy(dst, ".", tmp_len= strlen("."));
  dst+= tmp_len;
  memcpy(dst, m_name.str, tmp_len= m_name.length);
  dst+= tmp_len;
  memcpy(dst, " ON SCHEDULE ", tmp_len= strlen(" ON SCHEDULE "));
  dst+= tmp_len;
  memcpy(dst, "EVERY 5 MINUTE ", tmp_len= strlen("EVERY 5 MINUTE "));
  dst+= tmp_len;
/*
  memcpy(dst, "ON COMPLETION ", tmp_len =strlen("ON COMPLETION "));
  dst+= tmp_len;
  memcpy(dst, (m_on_completion==MYSQL_EVENT_ON_COMPLETION_DROP?
		         "NOT PRESERVE ":"PRESERVE "),
			 tmp_len =(m_on_completion==MYSQL_EVENT_ON_COMPLETION_DROP? 13:9));
  dst+= tmp_len;

  memcpy(dst, (m_status==MYSQL_EVENT_ENABLED?
		         "ENABLE  ":"DISABLE  "),
			 tmp_len= (m_status==MYSQL_EVENT_ENABLED? 8:9));
  dst+=tmp_len;

  memcpy(dst, "COMMENT \"", tmp_len= strlen("COMMENT \""));
  dst+= tmp_len;
  memcpy(dst, m_comment.str, tmp_len= m_comment.length);
  dst+= tmp_len;
  memcpy(dst, "\" ", tmp_len=2);
  dst+= tmp_len;
*/
  memcpy(dst, "DO ", tmp_len=3);
  dst+= tmp_len;

  memcpy(dst, m_body.str, tmp_len= m_body.length);
  dst+= tmp_len;
  memcpy(dst, ";", 1);
  ++dst;
  *dst= '\0';

  *length= len;
  
  return ret;
}


int
event_timed::execute(THD *thd, MEM_ROOT *mem_root= NULL)
{
  List<Item> empty_item_list;
  int ret= 0;
   
  DBUG_ENTER("event_timed::execute");

  // TODO Andrey : make this as member variable and delete in destructor
  empty_item_list.empty();
  
  if (!m_sphead && (ret= compile(thd, mem_root)))
    goto done;
  
  ret= m_sphead->execute_procedure(thd, &empty_item_list);

done:
  // Don't cache m_sphead if allocated on another mem_root
  if (mem_root && m_sphead)
  {
    delete m_sphead;
    m_sphead= 0;
  }

  DBUG_RETURN(ret);
}


int
event_timed::compile(THD *thd, MEM_ROOT *mem_root= NULL)
{
  MEM_ROOT *tmp_mem_root= 0;
  LEX *old_lex= thd->lex, lex;
  char *old_db;
  event_timed *ett;
  sp_name *spn;
  char *old_query;
  uint old_query_len;
  st_sp_chistics *p;
  
  DBUG_ENTER("event_timed::compile");
  // change the memory root for the execution time
  if (mem_root)
  {
    tmp_mem_root= thd->mem_root;
    thd->mem_root= mem_root;
  }
  old_query_len= thd->query_length;
  old_query= thd->query;
  old_db= thd->db;
  thd->db= m_db.str;
  thd->query= get_show_create_event(thd, &thd->query_length);
  DBUG_PRINT("event_timed::compile", ("query:%s",thd->query));

  thd->lex= &lex;
  lex_start(thd, (uchar*)thd->query, thd->query_length);
  lex.et_compile_phase= TRUE;
  if (yyparse((void *)thd) || thd->is_fatal_error)
  {
    //  Free lex associated resources
    //  QQ: Do we really need all this stuff here ?
    if (lex.sphead)
    {
      if (&lex != thd->lex)
        thd->lex->sphead->restore_lex(thd);
      delete lex.sphead;
      lex.sphead= 0;
    }
    // QQ: anything else ?
    lex_end(&lex);
    thd->lex= old_lex;
    DBUG_RETURN(-1);
  }
  
  m_sphead= lex.sphead;
  m_sphead->m_db= m_db;
  //copy also chistics since they will vanish otherwise we get 0x0 pointer
  // Todo : Handle sql_mode !!
  m_sphead->set_definer(m_definer.str,  m_definer.length);
  m_sphead->set_info(0, 0, &lex.sp_chistics, 0/*sql_mode*/);
  m_sphead->optimize();
  lex_end(&lex);
  thd->lex= old_lex;
  thd->query= old_query;
  thd->query_length= old_query_len;
  thd->db= old_db;
  /*
    Change the memory root for the execution time.
  */
  if (mem_root)
    thd->mem_root= tmp_mem_root;

  DBUG_RETURN(0);
}


/******************************** EXECUTOR ************************************/


//extern "C" pthread_handler_decl(event_executor_main, arg);
//extern "C" pthread_handler_decl(event_executor_worker, arg);

/*
  TODO Andrey: Check for command line option whether to start
               the main thread or not.
*/

pthread_handler_t event_executor_worker(void *arg);
pthread_handler_t event_executor_main(void *arg);

int
init_events()
{
  pthread_t th;

  DBUG_ENTER("init_events");

  DBUG_PRINT("info",("Starting events main thread"));

  pthread_mutex_init(&LOCK_event_arrays, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_workers_count, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_evex_running, MY_MUTEX_INIT_FAST);

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  event_executor_running_global_var= false;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  //TODO Andrey: Change the error code returned!
  if (pthread_create(&th, NULL, event_executor_main, (void*)NULL))
    DBUG_RETURN(ER_SLAVE_THREAD);

  DBUG_RETURN(0);
}


void
shutdown_events()
{
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
  pthread_mutex_destroy(&LOCK_event_arrays);
  pthread_mutex_destroy(&LOCK_workers_count);
  pthread_mutex_destroy(&LOCK_evex_running);
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


pthread_handler_t event_executor_main(void *arg)
{
  THD *thd;			/* needs to be first for thread_stack */
  ulonglong iter_num= 0;
  uint i=0, j=0;

  DBUG_ENTER("event_executor_main");
  DBUG_PRINT("event_executor_main", ("EVEX thread started"));    

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= true;  
  event_executor_running_global_var= opt_event_executor;
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

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

  thd->init_for_queries();

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
//    sql_print_information("[EVEX] External Loop!");
    my_sleep(500000);// sleep 0.5s
    if (!event_executor_running_global_var)
      continue;
    time(&now);
    my_tz_UTC->gmt_sec_to_TIME(&time_now, now);

	
    VOID(pthread_mutex_lock(&LOCK_event_arrays));
    for (i= 0; (i < evex_executing_queue.elements) && !thd->killed; ++i)
    {
      event_timed **p_et=dynamic_element(&evex_executing_queue,i,event_timed**);
      event_timed *et= *p_et;
//      sql_print_information("[EVEX] External Loop 2!");
      
      if (!event_executor_running_global_var)
        break;// soon we will do only continue (see the code a bit above)

      thd->proc_info = "Iterating";
      THD_CHECK_SENTRY(thd);
      /*
        if this is the first event which is after time_now then no
        more need to iterate over more elements since the array is sorted.
      */ 
      if (et->m_execute_at.year &&
          my_time_compare(&time_now, &et->m_execute_at) == -1)
        break;
      
      if (et->m_status == MYSQL_EVENT_ENABLED) 
      {
        pthread_t th;

        DBUG_PRINT("info", ("  Spawning a thread %d", ++iter_num));
//        sql_print_information("  Spawning a thread %d", ++iter_num);

        if (pthread_create(&th, NULL, event_executor_worker, (void*)et))
        {
          sql_print_error("Problem while trying to create a thread");
          VOID(pthread_mutex_unlock(&LOCK_event_arrays));
          goto err; // for now finish execution of the Executor
        }

        et->mark_last_executed();
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
      event_timed **p_et= dynamic_element(&evex_executing_queue, j, event_timed**);
      event_timed *et= *p_et;
      if (et->m_flags & EVENT_EXEC_NO_MORE || et->m_status == MYSQL_EVENT_DISABLED)
      {
        delete_dynamic_element(&evex_executing_queue, j);
        DBUG_PRINT("", ("DELETING FROM EXECUTION QUEUE [%s.%s]",et->m_db.str, et->m_name.str));
        // nulling the position, will delete later
        if (et->m_dropped)
        {
          // we have to drop the event
          int idx;
          et->drop(thd);
          idx= get_index_dynamic(&events_array, (gptr) et);
          if (idx != -1)
            delete_dynamic_element(&events_array, idx);
          else
            sql_print_error("Something weird happened with events. %d", __LINE__);
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
  }// while (!thd->killed)

err:
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  sql_print_information("Event executor stopping");
  // LEX_STRINGs reside in the memory root and will be destroyed with it.
  // Hence no need of delete but only freeing of SP
  for (i=0; i < events_array.elements; ++i)
  {
    event_timed *et= dynamic_element(&events_array, i, event_timed*);
    et->free_sp();
  }
  // TODO Andrey: USE lock here!
  delete_dynamic(&evex_executing_queue);
  delete_dynamic(&events_array);
  
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

  /*
    sleeping some time may help not crash the server. sleeping
    is done to wait for spawned threads to finish.
    
    TODO: A better will be with a conditional variable
  */
  {
    uint tries= 0;
    while (tries++ < 5)
    {
      VOID(pthread_mutex_lock(&LOCK_workers_count));
      if (!workers_count)
      {
        VOID(pthread_mutex_unlock(&LOCK_workers_count));
        break;
      }  
      VOID(pthread_mutex_unlock(&LOCK_workers_count));
      DBUG_PRINT("info", ("Sleep %d", tries));
      my_sleep(1000000 * tries);// 1s
    }
    DBUG_PRINT("info", ("Maybe now it is ok to kill the thread and evex MRoot"));
  }

err_no_thd:
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  evex_is_running= false;  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  free_root(&evex_mem_root, MYF(0));
  sql_print_information("Event executor stopped");

  shutdown_events();

  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


pthread_handler_t event_executor_worker(void *event_void)
{
  THD *thd; /* needs to be first for thread_stack */
  List<Item> empty_item_list;
  event_timed *event = (event_timed *) event_void;
  MEM_ROOT mem_root;
  ulong save_options;


  DBUG_ENTER("event_executor_worker");
  VOID(pthread_mutex_lock(&LOCK_workers_count));
  ++workers_count;  
  VOID(pthread_mutex_unlock(&LOCK_workers_count));

  init_alloc_root(&mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  //we pass this empty list as parameter to the SP_HEAD of the event
  empty_item_list.empty();

  my_thread_init();

  //TODO Andrey: Check for NULL
  if (!(thd = new THD)) // note that contructor of THD uses DBUG_ !
  {
    sql_print_error("Cannot create a THD structure in worker thread");
    goto err_no_thd;
  }
  thd->thread_stack = (char*)&thd; // remember where our stack is
  thd->mem_root= &mem_root;
//  pthread_detach_this_thread();
  pthread_detach(pthread_self());
  if (init_event_thread(thd))
    goto err;

  thd->init_for_queries();
  save_options= thd->options;
  thd->options&= ~OPTION_BIN_LOG;  

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  //thd->security_ctx->priv_host is char[MAX_HOSTNAME]
  
  strxnmov(thd->security_ctx->priv_host, sizeof(thd->security_ctx->priv_host),
                event->m_definer_host.str, NullS);  

  thd->security_ctx->priv_user= event->m_definer_user.str;

  thd->db= event->m_db.str;
  if (!check_global_access(thd, EVENT_ACL))
  {
    char exec_time[200];
    int ret;
    my_TIME_to_str(&event->m_execute_at, exec_time);
    DBUG_PRINT("info", ("    EVEX EXECUTING event for event %s.%s [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time));
//    sql_print_information("    EVEX EXECUTING event for event %s.%s [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time);
    ret= event->execute(thd);
//    sql_print_information("    EVEX EXECUTED event for event %s.%s  [EXPR:%d][EXECUTE_AT:%s]. RetCode=%d", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time, ret); 
    DBUG_PRINT("info", ("    EVEX EXECUTED event for event %s.%s  [EXPR:%d][EXECUTE_AT:%s]", event->m_db.str, event->m_name.str,(int) event->m_expr, exec_time)); 
  }
  thd->db= 0;
  //reenable (is it needed?)
  thd->options= save_options;
err:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thread_count--;
  thread_running--;
  /*
    Some extra safety, which should not been needed (normally, event deletion
    should already have done these assignments (each event which sets these
    variables is supposed to set them to 0 before terminating)).
  */
  //thd->query= thd->db= thd->catalog= 0; 
  //thd->query_length= thd->db_length= 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  //thd->temporary_tables = 0; // remove tempation from destructor to close them
  thd->proc_info = "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  THD_CHECK_SENTRY(thd);
  
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  THD_CHECK_SENTRY(thd);
  delete thd;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

err_no_thd:

  free_root(&mem_root, MYF(0));
//  sql_print_information("    Worker thread exiting");    
  
  VOID(pthread_mutex_lock(&LOCK_workers_count));
  --workers_count;  
  VOID(pthread_mutex_unlock(&LOCK_workers_count));
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0); // Can't return anything here
}

