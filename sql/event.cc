/* Copyright (C) 2004-2005 MySQL AB

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

#include "event_priv.h"
#include "event.h"
#include "sp.h"

/*
 TODO list :
 - The default value of created/modified should not be 0000-00-00 because of
   STRICT mode restricions.

 - CREATE EVENT should not go into binary log! Does it now? The SQL statements
   issued by the EVENT are replicated.
   I have an idea how to solve the problem at failover. So the status field
   will be ENUM('DISABLED', 'ENABLED', 'SLAVESIDE_DISABLED').
   In this case when CREATE EVENT is replicated it should go into the binary
   as SLAVESIDE_DISABLED if it is ENABLED, when it's created as DISABLEd it
   should be replicated as disabled. If an event is ALTERed as DISABLED the
   query should go untouched into the binary log, when ALTERed as enable then
   it should go as SLAVESIDE_DISABLED. This is regarding the SQL interface.
   TT routines however modify mysql.event internally and this does not go the log
   so in this case queries has to be injected into the log...somehow... or
   maybe a solution is RBR for this case, because the event may go only from
   ENABLED to DISABLED status change and this is safe for replicating. As well
   an event may be deleted which is also safe for RBR.

 - Maybe move all allocations during parsing to evex_mem_root thus saving
    double parsing in evex_create_event!

 - If the server is killed (stopping) try to kill executing events?
 
 - What happens if one renames an event in the DB while it is in memory?
   Or even deleting it?
  
 - Consider using conditional variable when doing shutdown instead of
   waiting till all worker threads end.
 
 - Make event_timed::get_show_create_event() work

 - Add function documentation whenever needed.

 - Add logging to file

 - Move comparison code to class event_timed

Warning:
 - For now parallel execution is not possible because the same sp_head cannot be
   executed few times!!! There is still no lock attached to particular event.

*/


QUEUE EVEX_EQ_NAME;
MEM_ROOT evex_mem_root;


void
evex_queue_init(EVEX_QUEUE_TYPE *queue)
{
  if (init_queue_ex(queue, 100 /*num_el*/, 0 /*offset*/, 
                   0 /*smallest_on_top*/, event_timed_compare_q, NULL,
                   100 /*auto_extent*/))
    sql_print_error("Insufficient memory to initialize executing queue.");
}


static
int sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs)
{
 return cs->coll->strnncollsp(cs,
                              (unsigned char *) s.str,s.length,
                              (unsigned char *) t.str,t.length, 0);
}


int
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


  if (a->second_part > b->second_part)
    DBUG_RETURN(1);
  
  if (a->second_part < b->second_part)
    DBUG_RETURN(-1);


  DBUG_RETURN(0);
}


int
evex_time_diff(TIME *a, TIME *b)
{
  my_bool in_gap;
  DBUG_ENTER("my_time_diff");
  
  return sec_since_epoch_TIME(a) - sec_since_epoch_TIME(b);
}


inline int
event_timed_compare(event_timed **a, event_timed **b)
{
  my_ulonglong a_t, b_t;
  a_t= TIME_to_ulonglong_datetime(&(*a)->execute_at)*100L + 
       (*a)->execute_at.second_part;
  b_t= TIME_to_ulonglong_datetime(&(*b)->execute_at)*100L + 
       (*b)->execute_at.second_part;

  if (a_t > b_t)
    return 1;
  else if (a_t < b_t)
    return -1;
  else
    return 0;
  
}


int 
event_timed_compare_q(void *vptr, byte* a, byte *b)
{
  return event_timed_compare((event_timed **)&a, (event_timed **)&b);
}


/*
  Open mysql.event table for read

  SYNOPSIS
    evex_open_event_table_for_read()
      thd         Thread context
      lock_type   How to lock the table
      table       The table pointer
  RETURN
    1	Cannot lock table
    2   The table is corrupted - different number of fields
    0	OK
*/

int
evex_open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table)
{
  TABLE_LIST tables;
  bool not_used;
  DBUG_ENTER("open_proc_table");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "event";
  tables.lock_type= lock_type;

  if (simple_open_n_lock_tables(thd, &tables))
    DBUG_RETURN(1);
  
  if (tables.table->s->fields != EVEX_FIELD_COUNT)
  {
    my_error(ER_EVENT_COL_COUNT_DOESNT_MATCH, MYF(0), "mysql", "event");
    close_thread_tables(thd);
    DBUG_RETURN(2);
  }
  *table= tables.table;

  DBUG_RETURN(0);
}


/*
  Find row in open mysql.event table representing event

  SYNOPSIS
    evex_db_find_event_aux()
      thd    Thread context
      dbname Name of event's database
      rname  Name of the event inside the db  
      table  TABLE object for open mysql.event table.

  RETURN VALUE
    0                  - Routine found
    EVEX_KEY_NOT_FOUND - No routine with given name
*/

int
evex_db_find_event_aux(THD *thd, const LEX_STRING dbname,
                       const LEX_STRING ev_name, TABLE *table)
{
  byte key[MAX_KEY_LENGTH];
  DBUG_ENTER("evex_db_find_event_aux");
  DBUG_PRINT("enter", ("name: %.*s", ev_name.length, ev_name.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the two first fields in the table are
    'db' and 'name' and the first key is the primary key over the
    same fields.
  */
  if (dbname.length > table->field[EVEX_FIELD_DB]->field_length ||
      ev_name.length > table->field[EVEX_FIELD_NAME]->field_length)
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

  table->field[0]->store(dbname.str, dbname.length, &my_charset_bin);
  table->field[1]->store(ev_name.str, ev_name.length, &my_charset_bin);
  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->index_read_idx(table->record[0], 0, key,
                                 table->key_info->key_length,HA_READ_KEY_EXACT))
    DBUG_RETURN(EVEX_KEY_NOT_FOUND);

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

  if (table->s->fields != EVEX_FIELD_COUNT)
  {
    my_error(ER_EVENT_COL_COUNT_DOESNT_MATCH, MYF(0), "mysql", "event");
    DBUG_RETURN(EVEX_GET_FIELD_FAILED);
  }
  
  DBUG_PRINT("info", ("dbname.len=%d",et->dbname.length));  
  DBUG_PRINT("info", ("name.len=%d",et->name.length));  

  table->field[EVEX_FIELD_DB]->
      store(et->dbname.str, et->dbname.length, system_charset_info);
  table->field[EVEX_FIELD_NAME]->
      store(et->name.str, et->name.length, system_charset_info);

  table->field[EVEX_FIELD_ON_COMPLETION]->set_notnull();
  table->field[EVEX_FIELD_ON_COMPLETION]->store((longlong)et->on_completion);

  table->field[EVEX_FIELD_STATUS]->set_notnull();
  table->field[EVEX_FIELD_STATUS]->store((longlong)et->status);
//  et->status_changed= false;

  // ToDo: Andrey. How to use users current charset?
  if (et->body.str)
    table->field[EVEX_FIELD_BODY]->
      store(et->body.str, et->body.length, system_charset_info);

  if (et->starts.year)
  {
    table->field[EVEX_FIELD_STARTS]->set_notnull();// set NULL flag to OFF
    table->field[EVEX_FIELD_STARTS]->store_time(&et->starts, MYSQL_TIMESTAMP_DATETIME);
  }	   

  if (et->ends.year)
  {
    table->field[EVEX_FIELD_ENDS]->set_notnull();
    table->field[EVEX_FIELD_ENDS]->store_time(&et->ends, MYSQL_TIMESTAMP_DATETIME);
  }
   
  if (et->expression)
  {
    table->field[EVEX_FIELD_INTERVAL_EXPR]->set_notnull();
    table->field[EVEX_FIELD_INTERVAL_EXPR]->store((longlong)et->expression);

    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->set_notnull();
    /*
       In the enum (C) intervals start from 0 but in mysql enum valid values start
       from 1. Thus +1 offset is needed!
    */
    table->field[EVEX_FIELD_TRANSIENT_INTERVAL]->store((longlong)et->interval + 1);
  }
  else if (et->execute_at.year)
  {
    // fix_fields already called in init_execute_at
    table->field[EVEX_FIELD_EXECUTE_AT]->set_notnull();
    table->field[EVEX_FIELD_EXECUTE_AT]->store_time(&et->execute_at,
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

  if (et->comment.length)
    table->field[EVEX_FIELD_COMMENT]->
	store(et->comment.str, et->comment.length, system_charset_info);

  DBUG_RETURN(0);  
}


/*
   Creates an event in mysql.event

   SYNOPSIS
     db_create_event()
       thd  THD
       et   event_timed object containing information for the event 
   
     Return value
                        0 - OK
       EVEX_GENERAL_ERROR - Failure
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
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));


  DBUG_PRINT("info", ("open mysql.event for update"));
  if (evex_open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }

  DBUG_PRINT("info", ("check existance of an event with the same name"));
  if (!evex_db_find_event_aux(thd, et->dbname, et->name, table))
  {
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), et->name.str);
    goto err;    
  }

  DBUG_PRINT("info", ("non-existant, go forward"));
  if ((ret= sp_use_new_db(thd, et->dbname.str,olddb, sizeof(olddb),0, &dbchanged)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0));
    goto err;
  }
  
  restore_record(table, s->default_values); // Get default values for fields

  if (et->name.length > table->field[EVEX_FIELD_NAME]->field_length)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), et->name.str);
    goto err;
  }
  if (et->body.length > table->field[EVEX_FIELD_BODY]->field_length)
  {
    my_error(ER_TOO_LONG_BODY, MYF(0), et->name.str);
    goto err;
  }

  if (!(et->expression) && !(et->execute_at.year))
  {
    DBUG_PRINT("error", ("neither expression nor execute_at are set!"));
    my_error(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, MYF(0));    
    goto err;
  }

  strxmov(definer, et->definer_user.str, "@", et->definer_host.str, NullS);
  if ((ret=table->field[EVEX_FIELD_DEFINER]->
       store(definer, et->definer_user.length + 1 + et->definer_host.length,
             system_charset_info)))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
    goto err;
  }

  ((Field_timestamp *)table->field[EVEX_FIELD_CREATED])->set_time();

  // evex_fill_row() calls my_error() in case of error so no need to handle it here
  if ((ret= evex_fill_row(thd, table, et, false)))
    goto err; 

  if (table->file->write_row(table->record[0]))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
    goto err;
  }
  
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    /* Such a statement can always go directly to binlog, no trans cache */
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
    mysql_bin_log.write(&qinfo);
  }

  if (dbchanged)
    (void) mysql_change_db(thd, olddb, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_OK);

err:
  if (dbchanged)
    (void) mysql_change_db(thd, olddb, 1);
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
   Used to execute ALTER EVENT. Pendant to evex_update_event().

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
db_update_event(THD *thd, event_timed *et, sp_name *new_name)
{
  TABLE *table;
  int ret= EVEX_OPEN_TABLE_FAILED;
  DBUG_ENTER("db_update_event");
  DBUG_PRINT("enter", ("name: %.*s", et->name.length, et->name.str));
  if (new_name)
    DBUG_PRINT("enter", ("rename to: %.*s", new_name->m_name.length,
                                            new_name->m_name.str));

  if (evex_open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto err;
  }
  
  // first look whether we overwrite
  if (new_name)
  {
    if (!sortcmp_lex_string(et->name, new_name->m_name, system_charset_info) &&
        !sortcmp_lex_string(et->dbname, new_name->m_db, system_charset_info))
    {
      my_error(ER_EVENT_SAME_NAME, MYF(0), et->name.str);
      goto err;    
    }
  
    if (!evex_db_find_event_aux(thd, new_name->m_db, new_name->m_name, table))
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->m_name.str);
      goto err;
    }  
  }
  /*
    ...and then whether there is such an event. don't exchange the blocks
    because you will get error 120 from table handler because new_name will
    overwrite the key and SE will tell us that it cannot find the already found
    row (copied into record[1] later
  */
  if (EVEX_KEY_NOT_FOUND == evex_db_find_event_aux(thd, et->dbname, et->name,
                                                     table))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), et->name.str);
    goto err;    
  }

  
  store_record(table,record[1]);
  
  // Don't update create on row update.
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  // evex_fill_row() calls my_error() in case of error so no need to handle it here
  if ((ret= evex_fill_row(thd, table, et, true)))
    goto err;
   
  if (new_name)
  {    
    table->field[EVEX_FIELD_DB]->
      store(new_name->m_db.str, new_name->m_db.length, system_charset_info);
    table->field[EVEX_FIELD_NAME]->
      store(new_name->m_name.str, new_name->m_name.length, system_charset_info);
  }

  if ((ret= table->file->update_row(table->record[1], table->record[0])))
  {
    my_error(ER_EVENT_STORE_FAILED, MYF(0), et->name.str, ret);
    goto err;
  }

  // close mysql.event or we crash later when loading the event from disk
  close_thread_tables(thd);
  DBUG_RETURN(0);

err:
  if (table)
    close_thread_tables(thd);
  DBUG_RETURN(EVEX_GENERAL_ERROR);
}


/*
   Looks for a named event in mysql.event and in case of success returns
   an object will data loaded from the table.

   SYNOPSIS
     db_find_event()
       thd      THD
       name     the name of the event to find
       ett      event's data if event is found
       tbl      TABLE object to use when not NULL
   
   NOTES
     1) Use sp_name for look up, return in **ett if found
     2) tbl is not closed at exit
*/

static int
db_find_event(THD *thd, sp_name *name, event_timed **ett, TABLE *tbl)
{
  TABLE *table;
  int ret;
  const char *definer;
  char *ptr;
  event_timed *et;  
  DBUG_ENTER("db_find_event");
  DBUG_PRINT("enter", ("name: %*s", name->m_name.length, name->m_name.str));

  if (tbl)
    table= tbl;
  else if (evex_open_event_table(thd, TL_READ, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    ret= EVEX_GENERAL_ERROR;
    goto done;
  }

  if ((ret= evex_db_find_event_aux(thd, name->m_db, name->m_name, table)))
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name->m_name.str);
    goto done;    
  }
  et= new event_timed;
  
  /*
    1)The table should not be closed beforehand.  ::load_from_row() only loads
      and does not compile

    2)::load_from_row() is silent on error therefore we emit error msg here
  */
  if ((ret= et->load_from_row(&evex_mem_root, table)))
  {
    my_error(ER_EVENT_CANNOT_LOAD_FROM_TABLE, MYF(0));
    goto done;
  }

done:
  if (ret && et)
  {
    delete et;
    et= 0;
  }
  // don't close the table if we haven't opened it ourselves
  if (!tbl && table)
    close_thread_tables(thd);
  *ett= et;
  DBUG_RETURN(ret);
}


/*
   Looks for a named event in mysql.event and then loads it from 
   the table, compiles it and insert it into the cache.

   SYNOPSIS
     evex_load_and_compile_event()
       thd       THD
       spn       the name of the event to alter
       use_lock  whether to obtain a lock on LOCK_event_arrays or not
       
   RETURN VALUE
       0   - OK
       < 0 - error (in this case underlying functions call my_error()).

*/

static int
evex_load_and_compile_event(THD * thd, sp_name *spn, bool use_lock)
{
  int ret= 0;
  MEM_ROOT *tmp_mem_root;
  event_timed *ett;
  Open_tables_state backup;

  DBUG_ENTER("db_load_and_compile_event");
  DBUG_PRINT("enter", ("name: %*s", spn->m_name.length, spn->m_name.str));

  tmp_mem_root= thd->mem_root;
  thd->mem_root= &evex_mem_root;

  thd->reset_n_backup_open_tables_state(&backup);
  // no need to use my_error() here because db_find_event() has done it
  if ((ret= db_find_event(thd, spn, &ett, NULL)))
    goto done;

  thd->restore_backup_open_tables_state(&backup);
  /*
    allocate on evex_mem_root. if you call without evex_mem_root
    then sphead will not be cleared!
  */
  if ((ret= ett->compile(thd, &evex_mem_root)))
    goto done;
  
  ett->compute_next_execution_time();
  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));

  evex_queue_insert(&EVEX_EQ_NAME, (EVEX_PTOQEL) ett);

  /*
    There is a copy in the array which we don't need. sphead won't be
    destroyed.
  */

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

  DBUG_ENTER("evex_remove_from_cache");
  /*
    It is possible that 2 (or 1) pass(es) won't find the event in memory.
    The reason is that DISABLED events are not cached.
  */

  if (use_lock)
    VOID(pthread_mutex_lock(&LOCK_event_arrays));

  for (i= 0; i < evex_queue_num_elements(EVEX_EQ_NAME); ++i)
  {
    event_timed *et= evex_queue_element(&EVEX_EQ_NAME, i, event_timed*);
    DBUG_PRINT("info", ("[%s.%s]==[%s.%s]?",db->str,name->str, et->dbname.str, 
                        et->name.str));
    if (!sortcmp_lex_string(*name, et->name, system_charset_info) &&
        !sortcmp_lex_string(*db, et->dbname, system_charset_info))
    {
      et->free_sp();
      delete et;
      evex_queue_delete_element(&EVEX_EQ_NAME, i);
      // ok, we have cleaned
      goto done;
    }
  }

done:
  if (use_lock)
    VOID(pthread_mutex_unlock(&LOCK_event_arrays));

  DBUG_RETURN(0);
}




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
*/

int
evex_create_event(THD *thd, event_timed *et, uint create_options)
{
  int ret = 0;

  DBUG_ENTER("evex_create_event");
  DBUG_PRINT("enter", ("name: %*s options:%d", et->name.length,
                et->name.str, create_options));

  if ((ret = db_create_event(thd, et)) == EVEX_WRITE_ROW_FAILED && 
        (create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		      ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS),
		      "EVENT", et->name.str);
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
  if (evex_is_running && et->status == MYSQL_EVENT_ENABLED)
  {
    sp_name spn(et->dbname, et->name);
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
       thd        THD
       et         event's data
       new_name   set in case of RENAME TO.    
          
   NOTES
     et contains data about dbname and event name. 
     name is the new name of the event, if not null this means
     that RENAME TO was specified in the query.
*/

int
evex_update_event(THD *thd, event_timed *et, sp_name *new_name)
{
  int ret, i;
  bool need_second_pass= true;

  DBUG_ENTER("evex_update_event");
  DBUG_PRINT("enter", ("name: %*s", et->name.length, et->name.str));

  /*
    db_update_event() opens & closes the table to prevent
    crash later in the code when loading and compiling the new definition.
    Also on error conditions my_error() is called so no need to handle here
  */
  if ((ret= db_update_event(thd, et, new_name)))
    goto done;

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (!evex_is_running)
    UNLOCK_MUTEX_AND_BAIL_OUT(LOCK_evex_running, done);

  VOID(pthread_mutex_lock(&LOCK_event_arrays));
  evex_remove_from_cache(&et->dbname, &et->name, false);
  if (et->status == MYSQL_EVENT_ENABLED)
  {
    if (new_name)
      ret= evex_load_and_compile_event(thd, new_name, false);
    else
    {
      sp_name spn(et->dbname, et->name);
      ret= evex_load_and_compile_event(thd, &spn, false);
    }
    if (ret == EVEX_COMPILE_ERROR)
      my_error(ER_EVENT_COMPILE_ERROR, MYF(0));
  }
  VOID(pthread_mutex_unlock(&LOCK_event_arrays));
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:
  DBUG_RETURN(ret);
}


/*
 Drops an event

 SYNOPSIS
   evex_drop_event()
     thd             THD
     et              event's name
     drop_if_exists  if set and the event not existing => warning onto the stack
          
*/

int
evex_drop_event(THD *thd, event_timed *et, bool drop_if_exists)
{
  TABLE *table;
  int ret= EVEX_OPEN_TABLE_FAILED;
  DBUG_ENTER("evex_drop_event");

  if (evex_open_event_table(thd, TL_WRITE, &table))
  {
    my_error(ER_EVENT_OPEN_TABLE_FAILED, MYF(0));
    goto done;
  }

  if (!(ret= evex_db_find_event_aux(thd, et->dbname, et->name, table)))
  {
    if ((ret= table->file->delete_row(table->record[0])))
    { 	
      my_error(ER_EVENT_CANNOT_DELETE, MYF(0));
      goto done;
    }
  }
  else if (ret == EVEX_KEY_NOT_FOUND)
  { 
    if (drop_if_exists)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		    ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
		    "Event", et->name.str);
      ret= 0;
    } else
      my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), et->name.str);
    goto done;
  }

  VOID(pthread_mutex_lock(&LOCK_evex_running));
  if (evex_is_running)
    ret= evex_remove_from_cache(&et->dbname, &et->name, true);
  VOID(pthread_mutex_unlock(&LOCK_evex_running));

done:
  /*
    evex_drop_event() is used by event_timed::drop therefore
    we have to close our thread tables.
  */
  close_thread_tables(thd);

  DBUG_RETURN(ret);
}

