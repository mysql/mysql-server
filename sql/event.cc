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

/*
 TODO list :
 - The default value of created/modified should not be 0000-00-00 because of
   STRICT mode restricions.
 - Remove m_ prefixes of member variables.

 - Use timestamps instead of datetime.

 - Don't use SP's functionality for opening and closing of tables

 - CREATE EVENT should not go into binary log! Does it now? The SQL statements
   issued by the EVENT are replicated.

 - Add a lock and use it for guarding access to events_array dynamic array.

 - Add checks everywhere where new instance of THD is created. NULL can be
    returned and this will crash the server. The server will crash probably
    later but should not be in this code! Add a global variable, and a lock
	to guard it, that will specify an error in a worker thread so preventing
	new threads from being spawned.

 - Maybe move all allocations during parsing to evex_mem_root thus saving
    double parsing in evex_create_event!

 - If the server is killed (stopping) try to kill executing events..
 
 - What happens if one renames an event in the DB while it is in memory?
     Or even deleting it?

 - created & modified in the table should be UTC?
 
 - Add a lock to event_timed to serialize execution of an event - do not
     allow parallel executions. Hmm, however how last_executed is marked
     then? The call to event_timed::mark_last_executed() must be moved to
     event_timed::execute()?
 
 - Consider using conditional variable when doing shutdown instead of
     waiting some time (tries < 5).
 - Make event_timed::get_show_create_event() work
 - Add function documentation whenever needed.
 - Add logging to file

Warning:
 - For now parallel execution is not possible because the same sp_head cannot be
   executed few times!!! There is still no lock attached to particular event.

 */




bool mysql_event_table_exists= 1;
DYNAMIC_ARRAY events_array;
DYNAMIC_ARRAY evex_executing_queue;
MEM_ROOT evex_mem_root;



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

inline int
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


inline int
event_timed_compare(event_timed **a, event_timed **b)
{
  return my_time_compare(&(*a)->m_execute_at, &(*b)->m_execute_at);
}



/*
  Open mysql.event table for read

  SYNOPSIS
    evex_open_event_table_for_read()
      thd         Thread context
      lock_type   How to lock the table
  RETURN
    0	Error
    #	Pointer to TABLE object
*/

TABLE *evex_open_event_table(THD *thd, enum thr_lock_type lock_type)
{
  TABLE_LIST tables;
  bool not_used;
  DBUG_ENTER("open_proc_table");

  /*
    Speed up things if the table doesn't exists. *table_exists
    is set when we create or read stored procedure or on flush privileges.
  */
  if (!mysql_event_table_exists)
    DBUG_RETURN(0);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "event";
  tables.lock_type= lock_type;

  if (simple_open_n_lock_tables(thd, &tables))
  {
    mysql_event_table_exists= 0;
    DBUG_RETURN(0);
  }

  DBUG_RETURN(tables.table);
}


/*
  Find row in open mysql.event table representing event

  SYNOPSIS
    evex_db_find_routine_aux()
      thd    Thread context
      dbname Name of event's database
      rname  Name of the event inside the db  
      table  TABLE object for open mysql.event table.

  RETURN VALUE
    SP_OK           - Routine found
    SP_KEY_NOT_FOUND- No routine with given name
*/

int
evex_db_find_routine_aux(THD *thd, const LEX_STRING dbname,
                       const LEX_STRING rname, TABLE *table)
{
  byte key[MAX_KEY_LENGTH];	// db, name, optional key length type
  DBUG_ENTER("evex_db_find_routine_aux");
  DBUG_PRINT("enter", ("name: %.*s", rname.length, rname.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the two first fields in the table are
    'db' and 'name' and the first key is the primary key over the
    same fields.
  */
  if (rname.length > table->field[1]->field_length)
    DBUG_RETURN(SP_KEY_NOT_FOUND);
  table->field[0]->store(dbname.str, dbname.length, &my_charset_bin);
  table->field[1]->store(rname.str, rname.length, &my_charset_bin);
  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->index_read_idx(table->record[0], 0,
				  key, table->key_info->key_length,
				  HA_READ_KEY_EXACT))
    DBUG_RETURN(SP_KEY_NOT_FOUND);

  DBUG_RETURN(0);
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
evex_fill_row(THD *thd, TABLE *table, event_timed *et, my_bool is_update)
{
  DBUG_ENTER("evex_fill_row");
  int ret=0;

  if (table->s->fields != EVEX_FIELD_COUNT)
    DBUG_RETURN(EVEX_GET_FIELD_FAILED);

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
    table->field[EVEX_FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[EVEX_FIELD_INTERVAL_EXPR]->store((longlong)et->m_expr);

    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
       In the enum (C) intervals start from 0 but in mysql enum valid values start
       from 1. Thus +1 offset is needed!
    */
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong)et->m_interval + 1);
  }
  else if (et->m_execute_at.year)
  {
    // fix_fields already called in init_execute_at
    table->field[EVEX_FIELD_EXECUTE_AT]->set_notnull();
    table->field[EVEX_FIELD_EXECUTE_AT]->store_time(&et->m_execute_at,
                                                    MYSQL_TIMESTAMP_DATETIME);    
    
	//this will make it NULL because we don't call set_notnull
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong) 0);  
  }
  else
  {
    DBUG_ASSERT(is_update);
    // it is normal to be here when the action is update
    // this is an error if the action is create. something is borked
  }
    
  ((Field_timestamp *)table->field[EVEX_FIELD_MODIFIED])->set_time();

  if ((et->m_comment).length)
    table->field[EVEX_FIELD_COMMENT]->
	store((et->m_comment).str, (et->m_comment).length, system_charset_info);

  DBUG_RETURN(0);  
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
  int ret= EVEX_OK;
  TABLE *table;
  char definer[HOSTNAME_LENGTH+USERNAME_LENGTH+2];
  char olddb[128];
  bool dbchanged= false;
  DBUG_ENTER("db_create_event");
  DBUG_PRINT("enter", ("name: %.*s", et->m_name.length, et->m_name.str));


  DBUG_PRINT("info", ("open mysql.event for update"));
  if (!(table= evex_open_event_table(thd, TL_WRITE)))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!evex_db_find_routine_aux(thd, et->m_db, et->m_name, table))
  {
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), et->m_name.str);
    goto done;    
  }

  DBUG_PRINT("info", ("non-existant, go forward"));
  if ((ret= sp_use_new_db(thd, et->m_db.str, olddb, sizeof(olddb),
			  0, &dbchanged)))
  {
    DBUG_PRINT("info", ("cannot use_new_db. code=%d", ret));
    my_error(ER_BAD_DB_ERROR, MYF(0));
    DBUG_RETURN(0);
  }
  
  restore_record(table, s->default_values); // Get default values for fields
  strxmov(definer, et->m_definer_user.str, "@", et->m_definer_host.str, NullS);

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
    DBUG_PRINT("error", ("neither m_expr nor m_execute_as are set!"));
    my_error(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, MYF(0));
    goto done;
  }

  if (table->field[EVEX_FIELD_DEFINER]->
       store(definer, (uint)strlen(definer), system_charset_info))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->m_name.str);
    goto done;
  }

  ((Field_timestamp *)table->field[EVEX_FIELD_CREATED])->set_time();
  if ((ret= evex_fill_row(thd, table, et, false)))
    goto done; 

  ret= EVEX_OK;
  if (table->file->write_row(table->record[0]))
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->m_name.str);
  else if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    /* Such a statement can always go directly to binlog, no trans cache */
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
    mysql_bin_log.write(&qinfo);
  }

done:
  // No need to close the table, it will be closed in sql_parse::do_command

  if (dbchanged)
    (void) mysql_change_db(thd, olddb, 1);
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
  if (!(table= evex_open_event_table(thd, TL_WRITE)))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if (evex_db_find_routine_aux(thd, et->m_db, et->m_name, table) == SP_KEY_NOT_FOUND)
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), et->m_name.str);
    goto done;    
  }
  
  store_record(table,record[1]);
  
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET; // Don't update create on row update.
  if ((ret= evex_fill_row(thd, table, et, true)))
    goto done;
   
  if (name)
  {    
    table->field[EVEX_FIELD_DB]->
      store(name->m_db.str, name->m_db.length, system_charset_info);
    table->field[EVEX_FIELD_NAME]->
      store(name->m_name.str, name->m_name.length, system_charset_info);
  }

  if ((ret= table->file->update_row(table->record[1],table->record[0])))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->m_name.str);
    goto done;
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
  DBUG_ENTER("db_find_event");
  DBUG_PRINT("enter", ("name: %*s",
		       name->m_name.length, name->m_name.str));


  if (!(table= evex_open_event_table(thd, TL_READ)))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if ((ret= evex_db_find_routine_aux(thd, name->m_db, name->m_name, table)))
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
      if (idx == -1)
      {
        //this should never happen
        DBUG_PRINT("error", (" get_index_dynamic problem. %d."
               "i=%d idx=%d evex_ex_queue.buf=%p evex_ex_queue.elements=%d ett=%p\n"
               "events_array=%p events_array.elements=%d events_array.buf=%p\n"
               "p_et=%p ett=%p",
               __LINE__, i, idx, &evex_executing_queue.buffer,
               evex_executing_queue.elements, ett, &events_array,
               events_array.elements, events_array.buffer, p_et, ett));
        DBUG_ASSERT(0);
      }
      //destruct first and then remove. the destructor will delete sp_head
      ett->free_sp();
      delete_dynamic_element(&events_array, idx);
      delete_dynamic_element(&evex_executing_queue, i);
      // ok, we have cleaned
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

  DBUG_ENTER("evex_create_event");
  DBUG_PRINT("enter", ("name: %*s options:%d", et->m_name.length,
                et->m_name.str, create_options));

/*
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // TODO: put an warning to the user here.
  //       Is it needed? (Andrey, 051129)
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
*/

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
  if (evex_is_running && et->m_status == MYSQL_EVENT_ENABLED)
  {
    sp_name spn(et->m_db, et->m_name);
    ret= evex_load_and_compile_event(thd, &spn, true);
  }
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:
  // No need to close the table, it will be closed in sql_parse::do_command

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
     name is the new name of the event, if not null this means
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

/*
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // put an warning to the user here
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
*/

  if ((ret= db_update_event(thd, name, et)))
    goto done_no_evex;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  {
    // not running - therefore no memory structures
    VOID(pthread_mutex_unlock(&LOCK_evex_running));
    goto done_no_evex;
  }
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

  /*
    It is possible that 2 (or 1) pass(es) won't find the event in memory.
    The reason is that DISABLED events are not cached.
  */
  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  evex_remove_from_cache(&et->m_db, &et->m_name, false);
  if (et->m_status == MYSQL_EVENT_ENABLED)
    if (name)
      ret= evex_load_and_compile_event(thd, name, false);
    else
    {
      spn= new sp_name(et->m_db, et->m_name);
      ret= evex_load_and_compile_event(thd, spn, false);
      delete spn;
    }

done:
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));

done_no_evex:
  // No need to close the table, it will be closed in sql_parse::do_command

  DBUG_RETURN(ret);
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

/*
  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
  // put an warning to the user here
  {}  
  VOID(pthread_mutex_unlock(&LOCK_evex_running));
*/
////
  if (!(table= evex_open_event_table(thd, TL_WRITE)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  ret= evex_db_find_routine_aux(thd, et->m_db, et->m_name, table);

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
  // No need to close the table, it will be closed in sql_parse::do_command

  DBUG_RETURN(ret);
}

