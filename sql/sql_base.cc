/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/* Basic functions needed by many modules */

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <nisam.h>
#ifdef	__WIN__
#include <io.h>
#endif

TABLE *unused_tables;				/* Used by mysql_test */
HASH open_cache;				/* Used by mysql_test */
HASH assign_cache;

static int open_unireg_entry(THD *thd, TABLE *entry, const char *db,
			     const char *name, const char *alias,
			     TABLE_LIST *table_list, MEM_ROOT *mem_root);
static void free_cache_entry(TABLE *entry);
static void mysql_rm_tmp_tables(void);
static my_bool open_new_frm(const char *path, const char *alias,
                            const char *db, const char *table_name,
			    uint db_stat, uint prgflag,
			    uint ha_open_flags, TABLE *outparam,
			    TABLE_LIST *table_desc, MEM_ROOT *mem_root);

extern "C" byte *table_cache_key(const byte *record,uint *length,
				 my_bool not_used __attribute__((unused)))
{
  TABLE *entry=(TABLE*) record;
  *length=entry->key_length;
  return (byte*) entry->table_cache_key;
}

bool table_cache_init(void)
{
  mysql_rm_tmp_tables();
  return hash_init(&open_cache, &my_charset_bin, table_cache_size+16,
		   0, 0,table_cache_key,
		   (hash_free_key) free_cache_entry, 0) != 0;
}

void table_cache_free(void)
{
  DBUG_ENTER("table_cache_free");
  close_cached_tables((THD*) 0,0,(TABLE_LIST*) 0);
  if (!open_cache.records)			// Safety first
    hash_free(&open_cache);
  DBUG_VOID_RETURN;
}

uint cached_tables(void)
{
  return open_cache.records;
}

#ifdef EXTRA_DEBUG
static void check_unused(void)
{
  uint count=0,idx=0;
  TABLE *cur_link,*start_link;

  if ((start_link=cur_link=unused_tables))
  {
    do
    {
      if (cur_link != cur_link->next->prev || cur_link != cur_link->prev->next)
      {
	DBUG_PRINT("error",("Unused_links aren't linked properly")); /* purecov: inspected */
	return; /* purecov: inspected */
      }
    } while (count++ < open_cache.records &&
	     (cur_link=cur_link->next) != start_link);
    if (cur_link != start_link)
    {
      DBUG_PRINT("error",("Unused_links aren't connected")); /* purecov: inspected */
    }
  }
  for (idx=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    if (!entry->in_use)
      count--;
  }
  if (count != 0)
  {
    DBUG_PRINT("error",("Unused_links doesn't match open_cache: diff: %d", /* purecov: inspected */
			count)); /* purecov: inspected */
  }
}
#else
#define check_unused()
#endif

/*
  Create a list for all open tables matching SQL expression

  SYNOPSIS
    list_open_tables()
    thd			Thread THD
    wild		SQL like expression

  NOTES
    One gets only a list of tables for which one has any kind of privilege.
    db and table names are allocated in result struct, so one doesn't need
    a lock on LOCK_open when traversing the return list.

  RETURN VALUES
    NULL	Error (Probably OOM)
    #		Pointer to list of names of open tables.
*/

OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *wild)
{
  int result = 0;
  OPEN_TABLE_LIST **start_list, *open_list;
  TABLE_LIST table_list;
  char name[NAME_LEN*2];
  DBUG_ENTER("list_open_tables");

  VOID(pthread_mutex_lock(&LOCK_open));
  bzero((char*) &table_list,sizeof(table_list));
  start_list= &open_list;
  open_list=0;

  for (uint idx=0 ; result == 0 && idx < open_cache.records; idx++)
  {
    OPEN_TABLE_LIST *table;
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);

    DBUG_ASSERT(entry->real_name);
    if ((!entry->real_name))			// To be removed
      continue;					// Shouldn't happen
    if (wild)
    {
      strxmov(name,entry->table_cache_key,".",entry->real_name,NullS);
      if (wild_compare(name,wild,0))
	continue;
    }

    /* Check if user has SELECT privilege for any column in the table */
    table_list.db= (char*) entry->table_cache_key;
    table_list.real_name= entry->real_name;
    table_list.grant.privilege=0;

    if (check_table_access(thd,SELECT_ACL | EXTRA_ACL,&table_list,1))
      continue;
    /* need to check if we haven't already listed it */
    for (table= open_list  ; table ; table=table->next)
    {
      if (!strcmp(table->table,entry->real_name) &&
	  !strcmp(table->db,entry->table_cache_key))
      {
	if (entry->in_use)
	  table->in_use++;
	if (entry->locked_by_name)
	  table->locked++;
	break;
      }
    }
    if (table)
      continue;
    if (!(*start_list = (OPEN_TABLE_LIST *)
	  sql_alloc(sizeof(**start_list)+entry->key_length)))
    {
      open_list=0;				// Out of memory
      break;
    }
    strmov((*start_list)->table=
	   strmov(((*start_list)->db= (char*) ((*start_list)+1)),
		  entry->table_cache_key)+1,
	   entry->real_name);
    (*start_list)->in_use= entry->in_use ? 1 : 0;
    (*start_list)->locked= entry->locked_by_name ? 1 : 0;
    start_list= &(*start_list)->next;
    *start_list=0;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(open_list);
}

/*****************************************************************************
 *	 Functions to free open table cache
 ****************************************************************************/


void intern_close_table(TABLE *table)
{						// Free all structures
  free_io_cache(table);
  delete table->triggers;
  if (table->file)
    VOID(closefrm(table));			// close file
}

/*
  Remove table from the open table cache

  SYNOPSIS
    free_cache_entry()
    table		Table to remove

  NOTE
    We need to have a lock on LOCK_open when calling this
*/

static void free_cache_entry(TABLE *table)
{
  DBUG_ENTER("free_cache_entry");
  safe_mutex_assert_owner(&LOCK_open);

  intern_close_table(table);
  if (!table->in_use)
  {
    table->next->prev=table->prev;		/* remove from used chain */
    table->prev->next=table->next;
    if (table == unused_tables)
    {
      unused_tables=unused_tables->next;
      if (table == unused_tables)
	unused_tables=0;
    }
    check_unused();				// consisty check
  }
  my_free((gptr) table,MYF(0));
  DBUG_VOID_RETURN;
}

/* Free resources allocated by filesort() and read_record() */

void free_io_cache(TABLE *table)
{
  DBUG_ENTER("free_io_cache");
  if (table->sort.io_cache)
  {
    close_cached_file(table->sort.io_cache);
    my_free((gptr) table->sort.io_cache,MYF(0));
    table->sort.io_cache=0;
  }
  DBUG_VOID_RETURN;
}

	/* Close all tables which aren't in use by any thread */

bool close_cached_tables(THD *thd, bool if_wait_for_refresh,
			 TABLE_LIST *tables)
{
  bool result=0;
  DBUG_ENTER("close_cached_tables");

  VOID(pthread_mutex_lock(&LOCK_open));
  if (!tables)
  {
    while (unused_tables)
    {
#ifdef EXTRA_DEBUG
      if (hash_delete(&open_cache,(byte*) unused_tables))
	printf("Warning: Couldn't delete open table from hash\n");
#else
      VOID(hash_delete(&open_cache,(byte*) unused_tables));
#endif
    }
    refresh_version++;				// Force close of open tables
  }
  else
  {
    bool found=0;
    for (TABLE_LIST *table= tables; table; table= table->next_local)
    {
      if (remove_table_from_cache(thd, table->db, table->real_name, 1))
	found=1;
    }
    if (!found)
      if_wait_for_refresh=0;			// Nothing to wait for
  }
#ifndef EMBEDDED_LIBRARY
  if (!tables)
    kill_delayed_threads();
#endif
  if (if_wait_for_refresh)
  {
    /*
      If there is any table that has a lower refresh_version, wait until
      this is closed (or this thread is killed) before returning
    */
    thd->mysys_var->current_mutex= &LOCK_open;
    thd->mysys_var->current_cond= &COND_refresh;
    thd->proc_info="Flushing tables";

    close_old_data_files(thd,thd->open_tables,1,1);
    mysql_ha_close_list(thd, tables);
    bool found=1;
    /* Wait until all threads has closed all the tables we had locked */
    DBUG_PRINT("info",
	       ("Waiting for others threads to close their open tables"));
    while (found && ! thd->killed)
    {
      found=0;
      for (uint idx=0 ; idx < open_cache.records ; idx++)
      {
	TABLE *table=(TABLE*) hash_element(&open_cache,idx);
	if ((table->version) < refresh_version && table->db_stat)
	{
	  found=1;
	  pthread_cond_wait(&COND_refresh,&LOCK_open);
	  break;
	}
      }
    }
    /*
      No other thread has the locked tables open; reopen them and get the
      old locks. This should always succeed (unless some external process
      has removed the tables)
    */
    thd->in_lock_tables=1;
    result=reopen_tables(thd,1,1);
    thd->in_lock_tables=0;
    /* Set version for table */
    for (TABLE *table=thd->open_tables; table ; table= table->next)
      table->version=refresh_version;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  if (if_wait_for_refresh)
  {
    THD *thd=current_thd;
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= 0;
    thd->mysys_var->current_cond= 0;
    thd->proc_info=0;
    pthread_mutex_unlock(&thd->mysys_var->mutex);
  }
  DBUG_RETURN(result);
}


/*
  Close all tables used by thread

  SYNOPSIS
    close_thread_tables()
    thd			Thread handler
    lock_in_use		Set to 1 (0 = default) if caller has a lock on
			LOCK_open
    skip_derived	Set to 1 (0 = default) if we should not free derived
			tables.

  IMPLEMENTATION
    Unlocks tables and frees derived tables.
    Put all normal tables used by thread in free list.
*/

void close_thread_tables(THD *thd, bool lock_in_use, bool skip_derived)
{
  bool found_old_table=0;
  DBUG_ENTER("close_thread_tables");

  if (thd->derived_tables && !skip_derived)
  {
    TABLE *table, *next;
    /*
      Close all derived tables generated from questions like
      SELECT * from (select * from t1))
    */
    for (table= thd->derived_tables ; table ; table= next)
    {
      next= table->next;
      free_tmp_table(thd, table);
    }
    thd->derived_tables= 0;
  }
  if (thd->locked_tables)
  {
    ha_commit_stmt(thd);			// If select statement
    DBUG_VOID_RETURN;				// LOCK TABLES in use
  }

  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  /* VOID(pthread_sigmask(SIG_SETMASK,&thd->block_signals,NULL)); */
  if (!lock_in_use)
    VOID(pthread_mutex_lock(&LOCK_open));
  safe_mutex_assert_owner(&LOCK_open);

  DBUG_PRINT("info", ("thd->open_tables: %p", thd->open_tables));

  while (thd->open_tables)
    found_old_table|=close_thread_table(thd, &thd->open_tables);
  thd->some_tables_deleted=0;

  /* Free tables to hold down open files */
  while (open_cache.records > table_cache_size && unused_tables)
    VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */
  check_unused();
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    VOID(pthread_cond_broadcast(&COND_refresh));
  }
  if (!lock_in_use)
    VOID(pthread_mutex_unlock(&LOCK_open));
  /*  VOID(pthread_sigmask(SIG_SETMASK,&thd->signals,NULL)); */
  DBUG_VOID_RETURN;
}

/* move one table to free list */

bool close_thread_table(THD *thd, TABLE **table_ptr)
{
  DBUG_ENTER("close_thread_table");

  bool found_old_table= 0;
  TABLE *table= *table_ptr;
  DBUG_ASSERT(table->key_read == 0);
  DBUG_ASSERT(table->file->inited == handler::NONE);

  *table_ptr=table->next;
  if (table->version != refresh_version ||
      thd->version != refresh_version || !table->db_stat)
  {
    VOID(hash_delete(&open_cache,(byte*) table));
    found_old_table=1;
  }
  else
  {
    if (table->flush_version != flush_version)
    {
      table->flush_version=flush_version;
      table->file->extra(HA_EXTRA_FLUSH);
    }
    else
    {
      // Free memory and reset for next loop
      table->file->reset();
    }
    table->in_use=0;
    if (unused_tables)
    {
      table->next=unused_tables;		/* Link in last */
      table->prev=unused_tables->prev;
      unused_tables->prev=table;
      table->prev->next=table;
    }
    else
      unused_tables=table->next=table->prev=table;
  }
  DBUG_RETURN(found_old_table);
}

	/* Close and delete temporary tables */

void close_temporary(TABLE *table,bool delete_table)
{
  DBUG_ENTER("close_temporary");
  char path[FN_REFLEN];
  db_type table_type=table->db_type;
  strmov(path,table->path);
  free_io_cache(table);
  closefrm(table);
  my_free((char*) table,MYF(0));
  if (delete_table)
    rm_temporary_table(table_type, path);
  DBUG_VOID_RETURN;
}


void close_temporary_tables(THD *thd)
{
  TABLE *table,*next;
  char *query, *end;
  uint query_buf_size; 
  bool found_user_tables = 0;

  if (!thd->temporary_tables)
    return;
  
  LINT_INIT(end);
  query_buf_size= 50;   // Enough for DROP ... TABLE IF EXISTS

  for (table=thd->temporary_tables ; table ; table=table->next)
    /*
      We are going to add 4 ` around the db/table names, so 1 does not look
      enough; indeed it is enough, because table->key_length is greater (by 8,
      because of server_id and thread_id) than db||table.
    */
    query_buf_size+= table->key_length+1;

  if ((query = alloc_root(&thd->mem_root, query_buf_size)))
    // Better add "if exists", in case a RESET MASTER has been done
    end=strmov(query, "DROP /*!40005 TEMPORARY */ TABLE IF EXISTS ");

  for (table=thd->temporary_tables ; table ; table=next)
  {
    if (query) // we might be out of memory, but this is not fatal
    {
      // skip temporary tables not created directly by the user
      if (table->real_name[0] != '#')
	found_user_tables = 1;
      /*
        Here we assume table_cache_key always starts
        with \0 terminated db name
      */
      end = strxmov(end,"`",table->table_cache_key,"`.`",
                    table->real_name,"`,", NullS);
    }
    next=table->next;
    close_temporary(table);
  }
  if (query && found_user_tables && mysql_bin_log.is_open())
  {
    /* The -1 is to remove last ',' */
    thd->clear_error();
    Query_log_event qinfo(thd, query, (ulong)(end-query)-1, 0);
    /*
      Imagine the thread had created a temp table, then was doing a SELECT, and
      the SELECT was killed. Then it's not clever to mark the statement above as
      "killed", because it's not really a statement updating data, and there
      are 99.99% chances it will succeed on slave.
      If a real update (one updating a persistent table) was killed on the
      master, then this real update will be logged with error_code=killed,
      rightfully causing the slave to stop.
    */
    qinfo.error_code= 0;
    mysql_bin_log.write(&qinfo);
  }
  thd->temporary_tables=0;
}


/*
  Find table in list.

  SYNOPSIS
    find_table_in_list()
    table 		Pointer to table list
    offset		Offset to which list in table structure to use
    db_name 		Data base name
    table_name 		Table name

  NOTES:
    This is called by find_table_in_local_list() and
    find_table_in_global_list().

  RETURN VALUES
    NULL	Table not found
    #		Pointer to found table.
*/

TABLE_LIST *find_table_in_list(TABLE_LIST *table,
                               uint offset,
                               const char *db_name,
                               const char *table_name)
{
  if (lower_case_table_names)
  {
    for (; table; table= *(TABLE_LIST **) ((char*) table + offset))
    {
      if ((!strcmp(table->db, db_name) &&
           !strcmp(table->real_name, table_name)) ||
          (table->view &&
           !my_strcasecmp(table_alias_charset,
                          table->table->table_cache_key, db_name) &&
           !my_strcasecmp(table_alias_charset,
                          table->table->table_name, table_name)))
        break;
    }
  }
  else
  {
    for (; table; table= *(TABLE_LIST **) ((char*) table + offset))
    {
      if ((!strcmp(table->db, db_name) &&
           !strcmp(table->real_name, table_name)) ||
          (table->view &&
           !strcmp(table->table->table_cache_key, db_name) &&
           !strcmp(table->table->table_name, table_name)))
        break;
    }
  }
  return table;
}


/*
  Test that table is unique

  SYNOPSIS
    unique_table()
    table       table which should be chaked
    table_list  list of tables

  RETURN
    found duplicate
    0 if table is unique
*/

TABLE_LIST* unique_table(TABLE_LIST *table, TABLE_LIST *table_list)
{
  TABLE_LIST *res;
  const char *d_name= table->db, *t_name= table->real_name;
  char d_name_buff[MAX_ALIAS_NAME], t_name_buff[MAX_ALIAS_NAME];
  if (table->view)
  {
    /* it is view and table opened */
    if (lower_case_table_names)
    {
      strmov(t_name_buff, table->table->table_name);
      my_casedn_str(files_charset_info, t_name_buff);
      t_name= t_name_buff;
      strmov(d_name_buff, table->table->table_cache_key);
      my_casedn_str(files_charset_info, d_name_buff);
      d_name= d_name_buff;
    }
    else
    {
      d_name= table->table->table_cache_key;
      t_name= table->table->table_name;
    }
    if (d_name == 0)
    {
      /* it's temporary table => always unique */
      return 0;
    }
  }
  if ((res= find_table_in_global_list(table_list, d_name, t_name)) &&
      res->table && res->table == table->table)
  {
    // we found entry of this table try again.
    return find_table_in_global_list(res->next_global, d_name, t_name);
  }
  return res;
}


TABLE **find_temporary_table(THD *thd, const char *db, const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length= (uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  TABLE *table,**prev;

  int4store(key+key_length,thd->server_id);
  key_length += 4;
  int4store(key+key_length,thd->variables.pseudo_thread_id);
  key_length += 4;

  prev= &thd->temporary_tables;
  for (table=thd->temporary_tables ; table ; table=table->next)
  {
    if (table->key_length == key_length &&
	!memcmp(table->table_cache_key,key,key_length))
      return prev;
    prev= &table->next;
  }
  return 0;					// Not a temporary table
}

bool close_temporary_table(THD *thd, const char *db, const char *table_name)
{
  TABLE *table,**prev;

  if (!(prev=find_temporary_table(thd,db,table_name)))
    return 1;
  table= *prev;
  *prev= table->next;
  close_temporary(table);
  if (thd->slave_thread)
    --slave_open_temp_tables;
  return 0;
}

/*
  Used by ALTER TABLE when the table is a temporary one. It changes something
  only if the ALTER contained a RENAME clause (otherwise, table_name is the old
  name).
  Prepares a table cache key, which is the concatenation of db, table_name and
  thd->slave_proxy_id, separated by '\0'.
*/
bool rename_temporary_table(THD* thd, TABLE *table, const char *db,
			    const char *table_name)
{
  char *key;
  if (!(key=(char*) alloc_root(&table->mem_root,
			       (uint) strlen(db)+
			       (uint) strlen(table_name)+6+4)))
    return 1;				/* purecov: inspected */
  table->key_length=(uint)
    (strmov((table->real_name=strmov(table->table_cache_key=key,
				     db)+1),
	    table_name) - table->table_cache_key)+1;
  int4store(key+table->key_length,thd->server_id);
  table->key_length += 4;
  int4store(key+table->key_length,thd->variables.pseudo_thread_id);
  table->key_length += 4;
  return 0;
}


	/* move table first in unused links */

static void relink_unused(TABLE *table)
{
  if (table != unused_tables)
  {
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    table->next=unused_tables;			/* Link in unused tables */
    table->prev=unused_tables->prev;
    unused_tables->prev->next=table;
    unused_tables->prev=table;
    unused_tables=table;
    check_unused();
  }
}


/*
  Remove all instances of table from the current open list
  Free all locks on tables that are done with LOCK TABLES
 */

TABLE *unlink_open_table(THD *thd, TABLE *list, TABLE *find)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length=find->key_length;
  TABLE *start=list,**prev,*next;
  prev= &start;
  memcpy(key,find->table_cache_key,key_length);
  for (; list ; list=next)
  {
    next=list->next;
    if (list->key_length == key_length &&
	!memcmp(list->table_cache_key,key,key_length))
    {
      if (thd->locked_tables)
	mysql_lock_remove(thd, thd->locked_tables,list);
      VOID(hash_delete(&open_cache,(byte*) list)); // Close table
    }
    else
    {
      *prev=list;				// put in use list
      prev= &list->next;
    }
  }
  *prev=0;
  // Notify any 'refresh' threads
  pthread_cond_broadcast(&COND_refresh);
  return start;
}


/*
   When we call the following function we must have a lock on
   LOCK_open ; This lock will be unlocked on return.
*/

void wait_for_refresh(THD *thd)
{
  safe_mutex_assert_owner(&LOCK_open);

  /* Wait until the current table is up to date */
  const char *proc_info;
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  proc_info=thd->proc_info;
  thd->proc_info="Waiting for table";
  if (!thd->killed)
    (void) pthread_cond_wait(&COND_refresh,&LOCK_open);

  pthread_mutex_unlock(&LOCK_open);	// Must be unlocked first
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  thd->proc_info= proc_info;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
}


TABLE *reopen_name_locked_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("reopen_name_locked_table");
  if (thd->killed)
    DBUG_RETURN(0);
  TABLE* table;
  if (!(table = table_list->table))
    DBUG_RETURN(0);

  char* db = thd->db ? thd->db : table_list->db;
  char* table_name = table_list->real_name;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  pthread_mutex_lock(&LOCK_open);
  if (open_unireg_entry(thd, table, db, table_name, table_name, 0,
                        &thd->mem_root) ||
      !(table->table_cache_key =memdup_root(&table->mem_root,(char*) key,
					    key_length)))
  {
    delete table->triggers;
    closefrm(table);
    pthread_mutex_unlock(&LOCK_open);
    DBUG_RETURN(0);
  }

  table->key_length=key_length;
  table->version=0;
  table->flush_version=0;
  table->in_use = thd;
  check_unused();
  pthread_mutex_unlock(&LOCK_open);
  table->next = thd->open_tables;
  thd->open_tables = table;
  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->outer_join= table->null_row= table->maybe_null= table->force_index= 0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query= table->keys_in_use;
  table->used_keys= table->keys_for_keyread;
  DBUG_RETURN(table);
}


/******************************************************************************
** open a table
** Uses a cache of open tables to find a table not in use.
** If refresh is a NULL pointer, then the is no version number checking and
** the table is not put in the thread-open-list
** If the return value is NULL and refresh is set then one must close
** all tables and retry the open
******************************************************************************/


TABLE *open_table(THD *thd, TABLE_LIST *table_list, MEM_ROOT *mem_root,
		  bool *refresh)
{
  reg1	TABLE *table;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  char	*alias= table_list->alias;
  DBUG_ENTER("open_table");

  /* find a unused table in the open table cache */
  if (refresh)
    *refresh=0;
  if (thd->killed)
    DBUG_RETURN(0);
  key_length= (uint) (strmov(strmov(key, table_list->db)+1,
			     table_list->real_name)-key)+1;
  int4store(key + key_length, thd->server_id);
  int4store(key + key_length + 4, thd->variables.pseudo_thread_id);

  if (!table_list->skip_temporary)
  {
    for (table= thd->temporary_tables; table ; table=table->next)
    {
      if (table->key_length == key_length + 8 &&
	  !memcmp(table->table_cache_key, key, key_length + 8))
      {
	if (table->query_id == thd->query_id)
	{
	  my_printf_error(ER_CANT_REOPEN_TABLE,
			  ER(ER_CANT_REOPEN_TABLE), MYF(0), table->table_name);
	  DBUG_RETURN(0);
	}
	table->query_id= thd->query_id;
	table->clear_query_id= 1;
	thd->tmp_table_used= 1;
	goto reset;
      }
    }
  }

  if (thd->locked_tables)
  {						// Using table locks
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->key_length == key_length &&
	  !memcmp(table->table_cache_key,key,key_length) &&
	  !my_strcasecmp(system_charset_info, table->table_name, alias) &&
	  table->query_id != thd->query_id)
      {
	table->query_id=thd->query_id;
	goto reset;
      }
    }
    /*
      is it view?
      (it is work around to allow to open view with locked tables,
      real fix will be made after definition cache will be made)
    */
    {
      char path[FN_REFLEN];
      strxnmov(path, FN_REFLEN, mysql_data_home, "/", table_list->db, "/",
               table_list->real_name, reg_ext, NullS);
      (void) unpack_filename(path, path);
      if (mysql_frm_type(path) == FRMTYPE_VIEW)
      {
        TABLE tab;// will not be used (because it's VIEW) but have to be passed
        table= &tab;
        VOID(pthread_mutex_lock(&LOCK_open));
        if (open_unireg_entry(thd, table, table_list->db,
                              table_list->real_name,
                              alias, table_list, mem_root))
        {
          table->next=table->prev=table;
          free_cache_entry(table);
        }
        else
        {
          DBUG_ASSERT(table_list->view);
          VOID(pthread_mutex_unlock(&LOCK_open));
          DBUG_RETURN(0); // VIEW
        }
        VOID(pthread_mutex_unlock(&LOCK_open));
      }
    }
    my_printf_error(ER_TABLE_NOT_LOCKED,ER(ER_TABLE_NOT_LOCKED),MYF(0),alias);
    DBUG_RETURN(0);
  }

  VOID(pthread_mutex_lock(&LOCK_open));

  if (!thd->open_tables)
    thd->version=refresh_version;
  else if (thd->version != refresh_version && refresh)
  {
    /* Someone did a refresh while thread was opening tables */
    *refresh=1;
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(0);
  }

  /* close handler tables which are marked for flush */
  mysql_ha_close_list(thd, (TABLE_LIST*) NULL, /*flushed*/ 1);

  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table && table->in_use ;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
  {
    if (table->version != refresh_version)
    {
      /*
      ** There is a refresh in progress for this table
      ** Wait until the table is freed or the thread is killed.
      */
      close_old_data_files(thd,thd->open_tables,0,0);
      if (table->in_use != thd)
	wait_for_refresh(thd);
      else
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
      }
      if (refresh)
	*refresh=1;
      DBUG_RETURN(0);
    }
  }
  if (table)
  {
    if (table == unused_tables)
    {						// First unused
      unused_tables=unused_tables->next;	// Remove from link
      if (table == unused_tables)
	unused_tables=0;
    }
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    table->in_use= thd;
  }
  else
  {
    /* Free cache if too big */
    while (open_cache.records > table_cache_size && unused_tables)
      VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */

    /* make a new table */
    if (!(table=(TABLE*) my_malloc(sizeof(*table),MYF(MY_WME))))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    if (open_unireg_entry(thd, table, table_list->db, table_list->real_name,
			  alias, table_list, mem_root) ||
	(!table_list->view && 
	 !(table->table_cache_key= memdup_root(&table->mem_root, (char*) key,
					       key_length))))
    {
      table->next=table->prev=table;
      free_cache_entry(table);
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    if (table_list->view)
    {
      my_free((gptr)table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(0); // VIEW
    }
    table->key_length=key_length;
    table->version=refresh_version;
    table->flush_version=flush_version;
    DBUG_PRINT("info", ("inserting table %p into the cache", table));
    VOID(my_hash_insert(&open_cache,(byte*) table));
  }

  check_unused();				// Debugging call

  VOID(pthread_mutex_unlock(&LOCK_open));
  if (refresh)
  {
    table->next=thd->open_tables;		/* Link into simple list */
    thd->open_tables=table;
  }
  table->reginfo.lock_type=TL_READ;		/* Assume read */

 reset:
  /* Fix alias if table name changes */
  if (strcmp(table->table_name,alias))
  {
    uint length=(uint) strlen(alias)+1;
    table->table_name= (char*) my_realloc(table->table_name,length,
					  MYF(MY_WME));
    memcpy(table->table_name,alias,length);
    for (uint i=0 ; i < table->fields ; i++)
      table->field[i]->table_name=table->table_name;
  }
  /* These variables are also set in reopen_table() */
  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->outer_join= table->null_row= table->maybe_null= table->force_index= 0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query= table->keys_in_use;
  table->used_keys= table->keys_for_keyread;
  if (table->timestamp_field)
    table->timestamp_field->set_timestamp_offsets();
  table_list->updatable= 1; // It is not derived table nor non-updatable VIEW
  DBUG_ASSERT(table->key_read == 0);
  DBUG_RETURN(table);
}


TABLE *find_locked_table(THD *thd, const char *db,const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  for (TABLE *table=thd->open_tables; table ; table=table->next)
  {
    if (table->key_length == key_length &&
	!memcmp(table->table_cache_key,key,key_length))
      return table;
  }
  return(0);
}


/****************************************************************************
** Reopen an table because the definition has changed. The date file for the
** table is already closed.
** Returns 0 if ok.
** If table can't be reopened, the entry is unchanged.
****************************************************************************/

bool reopen_table(TABLE *table,bool locked)
{
  TABLE tmp;
  char *db=table->table_cache_key;
  char *table_name=table->real_name;
  bool error=1;
  Field **field;
  uint key,part;
  DBUG_ENTER("reopen_table");

#ifdef EXTRA_DEBUG
  if (table->db_stat)
    sql_print_error("Table %s had a open data handler in reopen_table",
		    table->table_name);
#endif
  if (!locked)
    VOID(pthread_mutex_lock(&LOCK_open));
  safe_mutex_assert_owner(&LOCK_open);

  if (open_unireg_entry(table->in_use, &tmp, db, table_name,
			table->table_name, 0, &table->in_use->mem_root))
    goto end;
  free_io_cache(table);

  if (!(tmp.table_cache_key= memdup_root(&tmp.mem_root,db,
					 table->key_length)))
  {
    delete tmp.triggers;
    closefrm(&tmp);				// End of memory
    goto end;
  }

  /* This list copies variables set by open_table */
  tmp.tablenr=		table->tablenr;
  tmp.used_fields=	table->used_fields;
  tmp.const_table=	table->const_table;
  tmp.outer_join=	table->outer_join;
  tmp.null_row=		table->null_row;
  tmp.maybe_null=	table->maybe_null;
  tmp.status=		table->status;
  tmp.keys_in_use_for_query= tmp.keys_in_use;
  tmp.used_keys= 	tmp.keys_for_keyread;
  tmp.force_index=	tmp.force_index;

  /* Get state */
  tmp.key_length=	table->key_length;
  tmp.in_use=    	table->in_use;
  tmp.reginfo.lock_type=table->reginfo.lock_type;
  tmp.version=		refresh_version;
  tmp.tmp_table=	table->tmp_table;
  tmp.grant=		table->grant;

  /* Replace table in open list */
  tmp.next=		table->next;
  tmp.prev=		table->prev;

  delete table->triggers;
  if (table->file)
    VOID(closefrm(table));		// close file, free everything

  *table=tmp;
  table->file->change_table_ptr(table);

  DBUG_ASSERT(table->table_name);
  for (field=table->field ; *field ; field++)
  {
    (*field)->table= (*field)->orig_table= table;
    (*field)->table_name=table->table_name;
  }
  for (key=0 ; key < table->keys ; key++)
    for (part=0 ; part < table->key_info[key].usable_key_parts ; part++)
      table->key_info[key].key_part[part].field->table= table;
  VOID(pthread_cond_broadcast(&COND_refresh));
  error=0;

 end:
  if (!locked)
    VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(error);
}


/*
  Used with ALTER TABLE:
  Close all instanses of table when LOCK TABLES is in used;
  Close first all instances of table and then reopen them
 */

bool close_data_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
    {
      mysql_lock_remove(thd, thd->locked_tables,table);
      table->file->close();
      table->db_stat=0;
    }
  }
  return 0;					// For the future
}


/*
  Reopen all tables with closed data files
  One should have lock on LOCK_open when calling this
*/

bool reopen_tables(THD *thd,bool get_locks,bool in_refresh)
{
  DBUG_ENTER("reopen_tables");
  safe_mutex_assert_owner(&LOCK_open);

  if (!thd->open_tables)
    DBUG_RETURN(0);

  TABLE *table,*next,**prev;
  TABLE **tables,**tables_ptr;			// For locks
  bool error=0;
  if (get_locks)
  {
    /* The ptr is checked later */
    uint opens=0;
    for (table=thd->open_tables; table ; table=table->next) opens++;
    tables= (TABLE**) my_alloca(sizeof(TABLE*)*opens);
  }
  else
    tables= &thd->open_tables;
  tables_ptr =tables;

  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    uint db_stat=table->db_stat;
    next=table->next;
    if (!tables || (!db_stat && reopen_table(table,1)))
    {
      my_error(ER_CANT_REOPEN_TABLE,MYF(0),table->table_name);
      VOID(hash_delete(&open_cache,(byte*) table));
      error=1;
    }
    else
    {
      *prev= table;
      prev= &table->next;
      if (get_locks && !db_stat)
	*tables_ptr++= table;			// need new lock on this
      if (in_refresh)
      {
	table->version=0;
	table->locked_by_flush=0;
      }
    }
  }
  if (tables != tables_ptr)			// Should we get back old locks
  {
    MYSQL_LOCK *lock;
    /* We should always get these locks */
    thd->some_tables_deleted=0;
    if ((lock=mysql_lock_tables(thd,tables,(uint) (tables_ptr-tables))))
    {
      thd->locked_tables=mysql_lock_merge(thd->locked_tables,lock);
    }
    else
      error=1;
  }
  if (get_locks && tables)
  {
    my_afree((gptr) tables);
  }
  VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
  *prev=0;
  DBUG_RETURN(error);
}

/*
  Close handlers for tables in list, but leave the TABLE structure
  intact so that we can re-open these quickly
  abort_locks is set if called from flush_tables.
*/

void close_old_data_files(THD *thd, TABLE *table, bool abort_locks,
			  bool send_refresh)
{
  DBUG_ENTER("close_old_data_files");
  bool found=send_refresh;
  for (; table ; table=table->next)
  {
    if (table->version != refresh_version)
    {
      found=1;
      if (!abort_locks)				// If not from flush tables
	table->version = refresh_version;	// Let other threads use table
      if (table->db_stat)
      {
	if (abort_locks)
	{
	  mysql_lock_abort(thd,table);		// Close waiting threads
	  mysql_lock_remove(thd, thd->locked_tables,table);
	  table->locked_by_flush=1;		// Will be reopened with locks
	}
	table->file->close();
	table->db_stat=0;
      }
    }
  }
  if (found)
    VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
  DBUG_VOID_RETURN;
}


/*
  Wait until all threads has closed the tables in the list
  We have also to wait if there is thread that has a lock on this table even
  if the table is closed
*/

bool table_is_used(TABLE *table, bool wait_for_name_lock)
{
  do
  {
    char *key= table->table_cache_key;
    uint key_length=table->key_length;
    for (TABLE *search=(TABLE*) hash_search(&open_cache,
					    (byte*) key,key_length) ;
	 search ;
	 search = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
    {
      if (search->locked_by_flush ||
	  search->locked_by_name && wait_for_name_lock ||
	  search->db_stat && search->version < refresh_version)
	return 1;				// Table is used
    }
  } while ((table=table->next));
  return 0;
}


/* Wait until all used tables are refreshed */

bool wait_for_tables(THD *thd)
{
  bool result;
  DBUG_ENTER("wait_for_tables");

  thd->proc_info="Waiting for tables";
  pthread_mutex_lock(&LOCK_open);
  while (!thd->killed)
  {
    thd->some_tables_deleted=0;
    close_old_data_files(thd,thd->open_tables,0,dropping_tables != 0);
    mysql_ha_close_list(thd, (TABLE_LIST*) NULL, /*flushed*/ 1);
    if (!table_is_used(thd->open_tables,1))
      break;
    (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
  }
  if (thd->killed)
    result= 1;					// aborted
  else
  {
    /* Now we can open all tables without any interference */
    thd->proc_info="Reopen tables";
    result=reopen_tables(thd,0,0);
  }
  pthread_mutex_unlock(&LOCK_open);
  thd->proc_info=0;
  DBUG_RETURN(result);
}


/* drop tables from locked list */

bool drop_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table,*next,**prev;
  bool found=0;
  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    next=table->next;
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
    {
      mysql_lock_remove(thd, thd->locked_tables,table);
      VOID(hash_delete(&open_cache,(byte*) table));
      found=1;
    }
    else
    {
      *prev=table;
      prev= &table->next;
    }
  }
  *prev=0;
  if (found)
    VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
  if (thd->locked_tables && thd->locked_tables->table_count == 0)
  {
    my_free((gptr) thd->locked_tables,MYF(0));
    thd->locked_tables=0;
  }
  return found;
}


/*
  If we have the table open, which only happens when a LOCK TABLE has been
  done on the table, change the lock type to a lock that will abort all
  other threads trying to get the lock.
*/

void abort_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table= thd->open_tables; table ; table= table->next)
  {
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
    {
      mysql_lock_abort(thd,table);
      break;
    }
  }
}


/*
  Load a table definition from file and open unireg table

  SYNOPSIS
    open_unireg_entry()
    thd			Thread handle
    entry		Store open table definition here
    db			Database name
    name		Table name
    alias		Alias name
    table_desc		TABLE_LIST descriptor (used with views)
    mem_root		temporary mem_root for parsing

  NOTES
   Extra argument for open is taken from thd->open_options

  RETURN
    0	ok
    #	Error
*/
static int open_unireg_entry(THD *thd, TABLE *entry, const char *db,
			     const char *name, const char *alias,
			     TABLE_LIST *table_desc, MEM_ROOT *mem_root)
{
  char path[FN_REFLEN];
  int error;
  uint discover_retry_count= 0;
  DBUG_ENTER("open_unireg_entry");

  strxmov(path, mysql_data_home, "/", db, "/", name, NullS);
  while ((error= openfrm(thd, path, alias,
		         (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
			         HA_GET_INDEX | HA_TRY_READ_ONLY |
                                 NO_ERR_ON_NEW_FRM),
		      READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
		      thd->open_options, entry)) &&
      (error != 5 ||
       (fn_format(path, path, 0, reg_ext, MY_UNPACK_FILENAME),
        open_new_frm(path, alias, db, name,
                     (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                             HA_GET_INDEX | HA_TRY_READ_ONLY),
                     READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
                     thd->open_options, entry, table_desc, mem_root))))

  {
    if (!entry->crashed)
    {
      /*
        Frm file could not be found on disk
        Since it does not exist, no one can be using it
        LOCK_open has been locked to protect from someone else
        trying to discover the table at the same time.
      */
      if (discover_retry_count++ != 0)
       goto err;
      if (create_table_from_handler(db, name, true) != 0)
       goto err;

      thd->clear_error(); // Clear error message
      continue;
    }

    // Code below is for repairing a crashed file
    TABLE_LIST table_list;
    bzero((char*) &table_list, sizeof(table_list)); // just for safe
    table_list.db=(char*) db;
    table_list.real_name=(char*) name;

    safe_mutex_assert_owner(&LOCK_open);

    if ((error=lock_table_name(thd,&table_list)))
    {
      if (error < 0)
      {
	goto err;
      }
      if (wait_for_locked_table_names(thd,&table_list))
      {
	unlock_table_name(thd,&table_list);
	goto err;
      }
    }
    pthread_mutex_unlock(&LOCK_open);
    thd->clear_error();				// Clear error message
    error= 0;
    if (openfrm(thd, path, alias,
		(uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
			 HA_TRY_READ_ONLY),
		READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
		ha_open_options | HA_OPEN_FOR_REPAIR,
		entry) || ! entry->file ||
	(entry->file->is_crashed() && entry->file->check_and_repair(thd)))
    {
      /* Give right error message */
      thd->clear_error();
      my_error(ER_NOT_KEYFILE, MYF(0), name, my_errno);
      sql_print_error("Error: Couldn't repair table: %s.%s",db,name);
      if (entry->file)
	closefrm(entry);
      error=1;
    }
    else
      thd->clear_error();			// Clear error message
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd,&table_list);

    if (error)
      goto err;
    break;
  }

  if (error == 5)
    DBUG_RETURN(0);	// we have just opened VIEW

  if (Table_triggers_list::check_n_load(thd, db, name, entry))
    goto err;

  /*
    If we are here, there was no fatal error (but error may be still
    unitialized).
  */
  if (unlikely(entry->file->implicit_emptied))
  {
    entry->file->implicit_emptied= 0;
    if (mysql_bin_log.is_open())
    {
      char *query, *end;
      uint query_buf_size= 20 + 2*NAME_LEN + 1;
      if ((query= (char*)my_malloc(query_buf_size,MYF(MY_WME))))
      {
        end = strxmov(strmov(query, "DELETE FROM `"),
                      db,"`.`",name,"`", NullS);
        Query_log_event qinfo(thd, query, (ulong)(end-query), 0);
        mysql_bin_log.write(&qinfo);
        my_free(query, MYF(0));
      }
      else
      {
        /*
          As replication is maybe going to be corrupted, we need to warn the
          DBA on top of warning the client (which will automatically be done
          because of MYF(MY_WME) in my_malloc() above).
        */
        sql_print_error("Error: when opening HEAP table, could not allocate \
memory to write 'DELETE FROM `%s`.`%s`' to the binary log",db,name);
        delete entry->triggers;
        if (entry->file)
          closefrm(entry);
        goto err;
      }
    }
  }
  DBUG_RETURN(0);
err:
  /* Hide "Table doesn't exist" errors if table belong to view */
  if (thd->net.last_errno == ER_NO_SUCH_TABLE &&
      table_desc && table_desc->belong_to_view)
  {
    TABLE_LIST * view= table_desc->belong_to_view;
    thd->clear_error();
    my_error(ER_VIEW_INVALID, MYF(0), view->view_db.str, view->view_name.str);
  }
  DBUG_RETURN(1);
}

/*
  Open all tables in list

  SYNOPSIS
    open_tables()
    thd - thread handler
    start - list of tables
    counter - number of opened tables will be return using this parameter

  RETURN
    0  - OK
    -1 - error
*/

int open_tables(THD *thd, TABLE_LIST *start, uint *counter)
{
  TABLE_LIST *tables;
  bool refresh;
  int result=0;
  DBUG_ENTER("open_tables");
  MEM_ROOT new_frm_mem;
  /*
    temporary mem_root for new .frm parsing.
    TODO: variables for size
  */
  init_alloc_root(&new_frm_mem, 8024, 8024);

  thd->current_tablenr= 0;
 restart:
  *counter= 0;
  thd->proc_info="Opening tables";
  for (tables= start; tables ;tables= tables->next_global)
  {
    /*
      Ignore placeholders for derived tables. After derived tables
      processing, link to created temporary table will be put here.
     */
    if (tables->derived)
      continue;
    (*counter)++;
    if (!tables->table &&
	!(tables->table= open_table(thd, tables, &new_frm_mem, &refresh)))
    {
      free_root(&new_frm_mem, MYF(MY_KEEP_PREALLOC));
      if (tables->view)
      {
	(*counter)--;
	continue; //VIEW placeholder
      }

      if (refresh)				// Refresh in progress
      {
	/* close all 'old' tables used by this thread */
	pthread_mutex_lock(&LOCK_open);
	// if query_id is not reset, we will get an error
	// re-opening a temp table
	thd->version=refresh_version;
	TABLE **prev_table= &thd->open_tables;
	bool found=0;
	for (TABLE_LIST *tmp= start; tmp; tmp= tmp->next_global)
	{
	  /* Close normal (not temporary) changed tables */
	  if (tmp->table && ! tmp->table->tmp_table)
	  {
	    if (tmp->table->version != refresh_version ||
		! tmp->table->db_stat)
	    {
	      VOID(hash_delete(&open_cache,(byte*) tmp->table));
	      tmp->table=0;
	      found=1;
	    }
	    else
	    {
	      *prev_table= tmp->table;		// Relink open list
	      prev_table= &tmp->table->next;
	    }
	  }
	}
	*prev_table=0;
	pthread_mutex_unlock(&LOCK_open);
	if (found)
	  VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
	goto restart;
      }
      result= -1;				// Fatal error
      break;
    }
    else
      free_root(&new_frm_mem, MYF(MY_KEEP_PREALLOC));

    if (tables->lock_type != TL_UNLOCK && ! thd->locked_tables)
      tables->table->reginfo.lock_type=tables->lock_type;
    tables->table->grant= tables->grant;
  }
  thd->proc_info=0;
  free_root(&new_frm_mem, MYF(0));              // Free pre-alloced block
  DBUG_RETURN(result);
}


/*
  Check that lock is ok for tables; Call start stmt if ok

  SYNOPSIS
    check_lock_and_start_stmt()
    thd			Thread handle
    table_list		Table to check
    lock_type		Lock used for table

  RETURN VALUES
  0	ok
  1	error
*/

static bool check_lock_and_start_stmt(THD *thd, TABLE *table,
				      thr_lock_type lock_type)
{
  int error;
  DBUG_ENTER("check_lock_and_start_stmt");

  if ((int) lock_type >= (int) TL_WRITE_ALLOW_READ &&
      (int) table->reginfo.lock_type < (int) TL_WRITE_ALLOW_READ)
  {
    my_printf_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,
		    ER(ER_TABLE_NOT_LOCKED_FOR_WRITE),
		    MYF(0),table->table_name);
    DBUG_RETURN(1);
  }
  if ((error=table->file->start_stmt(thd)))
  {
    table->file->print_error(error,MYF(0));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Open and lock one table

  SYNOPSIS
    open_ltable()
    thd			Thread handler
    table_list		Table to open is first table in this list
    lock_type		Lock to use for open

  RETURN VALUES
    table		Opened table
    0			Error
  
    If ok, the following are also set:
      table_list->lock_type 	lock_type
      table_list->table		table
*/

TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type lock_type)
{
  TABLE *table;
  bool refresh;
  DBUG_ENTER("open_ltable");

  thd->proc_info="Opening table";
  thd->current_tablenr= 0;
  /* open_ltable can be used only for BASIC TABLEs */
  table_list->required_type= FRMTYPE_TABLE;
  while (!(table= open_table(thd, table_list, &thd->mem_root, &refresh)) &&
         refresh)
    ;

  if (table)
  {
#if defined( __WIN__) || defined(OS2)
    /* Win32 can't drop a file that is open */
    if (lock_type == TL_WRITE_ALLOW_READ)
    {
      lock_type= TL_WRITE;
    }
#endif /* __WIN__ || OS2 */
    table_list->lock_type= lock_type;
    table_list->table=	   table;
    table->grant= table_list->grant;
    if (thd->locked_tables)
    {
      if (check_lock_and_start_stmt(thd, table, lock_type))
	table= 0;
    }
    else
    {
      DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
      if ((table->reginfo.lock_type= lock_type) != TL_UNLOCK)
	if (!(thd->lock=mysql_lock_tables(thd,&table_list->table,1)))
	  table= 0;
    }
  }
  thd->proc_info=0;
  DBUG_RETURN(table);
}


/*
  Open all tables in list and locks them for read without derived
  tables processing.

  SYNOPSIS
    simple_open_n_lock_tables()
    thd		- thread handler
    tables	- list of tables for open&locking

  RETURN
    0  - ok
    -1 - error

  NOTE
    The lock will automaticly be freed by close_thread_tables()
*/

int simple_open_n_lock_tables(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("simple_open_n_lock_tables");
  uint counter;
  if (open_tables(thd, tables, &counter) || lock_tables(thd, tables, counter))
    DBUG_RETURN(-1);				/* purecov: inspected */
  DBUG_RETURN(0);
}


/*
  Open all tables in list, locks them and process derived tables
  tables processing.

  SYNOPSIS
    open_and_lock_tables()
    thd		- thread handler
    tables	- list of tables for open&locking

  RETURN
    0  - ok
    -1 - error
    1  - error reported to user

  NOTE
    The lock will automaticly be freed by close_thread_tables()
*/

int open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("open_and_lock_tables");
  uint counter;
  if (open_tables(thd, tables, &counter) || lock_tables(thd, tables, counter)
      || mysql_handle_derived(thd->lex))
    DBUG_RETURN(thd->net.report_error ? -1 : 1); /* purecov: inspected */
  /*
    Let us propagate pointers to open tables from global table list
    to table lists in particular selects if needed.
  */
  if (thd->lex->all_selects_list->next_select_in_list() ||
      thd->lex->time_zone_tables_used)
  {
    for (SELECT_LEX *sl= thd->lex->all_selects_list;
	 sl;
	 sl= sl->next_select_in_list())
    {
      for (TABLE_LIST *cursor= (TABLE_LIST *) sl->table_list.first;
           cursor;
           cursor=cursor->next_local)
      {
        if (cursor->correspondent_table)
          cursor->table= cursor->correspondent_table->table;
      }
    }
  }
  DBUG_RETURN(0);
}


/*
  Lock all tables in list

  SYNOPSIS
    lock_tables()
    thd			Thread handler
    tables		Tables to lock
    count		umber of opened tables

  NOTES
    You can't call lock_tables twice, as this would break the dead-lock-free
    handling thr_lock gives us.  You most always get all needed locks at
    once.

  RETURN VALUES
   0	ok
   -1	Error
*/

int lock_tables(THD *thd, TABLE_LIST *tables, uint count)
{
  TABLE_LIST *table;
  if (!tables)
    return 0;

  if (!thd->locked_tables)
  {
    DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
    TABLE **start,**ptr;
    if (!(ptr=start=(TABLE**) sql_alloc(sizeof(TABLE*)*count)))
      return -1;
    for (table= tables; table; table= table->next_global)
    {
      if (!table->placeholder())
	*(ptr++)= table->table;
    }
    if (!(thd->lock=mysql_lock_tables(thd,start,count)))
      return -1;				/* purecov: inspected */
  }
  else
  {
    for (table= tables; table; table= table->next_global)
    {
      if (!table->placeholder() && 
	  check_lock_and_start_stmt(thd, table->table, table->lock_type))
      {
	ha_rollback_stmt(thd);
	return -1;
      }
    }
  }
  return 0;
}


/*
  Open a single table without table caching and don't set it in open_list
  Used by alter_table to open a temporary table and when creating
  a temporary table with CREATE TEMPORARY ...
*/

TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list)
{
  TABLE *tmp_table;
  DBUG_ENTER("open_temporary_table");

  /*
    The extra size in my_malloc() is for table_cache_key
    4 bytes for master thread id if we are in the slave
    1 byte to terminate db
    1 byte to terminate table_name
    total of 6 extra bytes in my_malloc in addition to table/db stuff
  */
  if (!(tmp_table=(TABLE*) my_malloc(sizeof(*tmp_table)+(uint) strlen(db)+
				     (uint) strlen(table_name)+6+4,
				     MYF(MY_WME))))
    DBUG_RETURN(0);				/* purecov: inspected */

  if (openfrm(thd, path, table_name,
	      (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX),
	      READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      ha_open_options,
	      tmp_table))
  {
    my_free((char*) tmp_table,MYF(0));
    DBUG_RETURN(0);
  }

  tmp_table->reginfo.lock_type=TL_WRITE;	 // Simulate locked
  tmp_table->tmp_table = (tmp_table->file->has_transactions() ? 
			  TRANSACTIONAL_TMP_TABLE : TMP_TABLE);
  tmp_table->table_cache_key=(char*) (tmp_table+1);
  tmp_table->key_length= (uint) (strmov((tmp_table->real_name=
					 strmov(tmp_table->table_cache_key,db)
					 +1), table_name)
				 - tmp_table->table_cache_key)+1;
  int4store(tmp_table->table_cache_key + tmp_table->key_length,
	    thd->server_id);
  tmp_table->key_length += 4;
  int4store(tmp_table->table_cache_key + tmp_table->key_length,
	    thd->variables.pseudo_thread_id);
  tmp_table->key_length += 4;

  if (link_in_list)
  {
    tmp_table->next=thd->temporary_tables;
    thd->temporary_tables=tmp_table;
    if (thd->slave_thread)
      slave_open_temp_tables++;
  }
  DBUG_RETURN(tmp_table);
}


bool rm_temporary_table(enum db_type base, char *path)
{
  bool error=0;
  DBUG_ENTER("rm_temporary_table");

  fn_format(path, path,"",reg_ext,4);
  unpack_filename(path,path);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  *fn_ext(path)='\0';				// remove extension
  handler *file=get_new_handler((TABLE*) 0, base);
  if (file && file->delete_table(path))
  {
    error=1;
    sql_print_error("Warning: Could not remove tmp table: '%s', error: %d",
		    path, my_errno);
  }
  delete file;
  DBUG_RETURN(error);
}


/*****************************************************************************
** find field in list or tables. if field is unqualifed and unique,
** return unique field
******************************************************************************/

/* Special Field pointers for find_field_in_tables returning */
const Field *not_found_field= (Field*) 0x1;
const Field *view_ref_found= (Field*) 0x2; 

#define WRONG_GRANT (Field*) -1


/*
  Find field in table or view

  SYNOPSIS
    find_field_in_table()
    thd				thread handler
    table_list			table where to find
    name			name of field
    item_name                   name of item if it will be created (VIEW)
    length			length of name
    ref				expression substituted in VIEW should be
				  passed using this reference (return
				  view_ref_found)
    check_grants_table		do check columns grants for table?
    check_grants_view		do check columns grants for view?
    allow_rowid			do allow finding of "_rowid" field?
    cached_field_index_ptr	cached position in field list (used to
				  speedup prepared tables field finding)

  RETURN
    0			field is not found
    view_ref_found	found value in VIEW (real result is in *ref)
    #			pointer to field
*/

Field *
find_field_in_table(THD *thd, TABLE_LIST *table_list,
                    const char *name, const char *item_name,
                    uint length, Item **ref,
                    bool check_grants_table, bool check_grants_view,
                    bool allow_rowid,
                    uint *cached_field_index_ptr)
{
  Field *fld;
  if (table_list->field_translation)
  {
    DBUG_ASSERT(ref != 0 && table_list->view != 0);
    uint num= table_list->view->select_lex.item_list.elements;
    Item **trans= table_list->field_translation;
    for (uint i= 0; i < num; i ++)
    {
      if (strcmp(trans[i]->name, name) == 0)
      {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
	if (check_grants_view &&
	    check_grant_column(thd, &table_list->grant,
			       table_list->view_db.str,
			       table_list->view_name.str,
			       name, length))
	  return WRONG_GRANT;
#endif
        if (thd->lex->current_select->no_wrap_view_item)
          *ref= trans[i];
        else
        {
          *ref= new Item_ref(trans + i, ref, table_list->view_name.str,
                             item_name);
          /* as far as Item_ref have defined refernce it do not need tables */
          if (*ref)
            (*ref)->fix_fields(thd, 0, ref);
        }
	return (Field*) view_ref_found;
      }
    }
    return 0;
  }
  fld= find_field_in_real_table(thd, table_list->table, name, length,
				check_grants_table, allow_rowid,
				cached_field_index_ptr);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* check for views with temporary table algorithm */
  if (check_grants_view && table_list->view &&
      fld && fld != WRONG_GRANT &&
      check_grant_column(thd, &table_list->grant,
			 table_list->view_db.str,
			 table_list->view_name.str,
			 name, length))
  {
    return WRONG_GRANT;
  }
#endif
  return fld;
}


/*
  Find field in table

  SYNOPSIS
    find_field_in_real_table()
    thd				thread handler
    table_list			table where to find
    name			name of field
    length			length of name
    check_grants		do check columns grants?
    allow_rowid			do allow finding of "_rowid" field?
    cached_field_index_ptr	cached position in field list (used to
				  speedup prepared tables field finding)

  RETURN
    0			field is not found
    #			pointer to field
*/

Field *find_field_in_real_table(THD *thd, TABLE *table,
                                const char *name, uint length,
                                bool check_grants, bool allow_rowid,
                                uint *cached_field_index_ptr)
{
  Field **field_ptr, *field;
  uint cached_field_index= *cached_field_index_ptr;

  /* We assume here that table->field < NO_CACHED_FIELD_INDEX = UINT_MAX */
  if (cached_field_index < table->fields &&
      !my_strcasecmp(system_charset_info, 
                     table->field[cached_field_index]->field_name, name))
    field_ptr= table->field + cached_field_index;
  else if (table->name_hash.records)
    field_ptr= (Field**)hash_search(&table->name_hash,(byte*) name,
                                    length);
  else
  {
    if (!(field_ptr= table->field))
      return (Field *)0;
    for (; *field_ptr; ++field_ptr)
      if (!my_strcasecmp(system_charset_info, (*field_ptr)->field_name, name))
        break;
  }

  if (field_ptr && *field_ptr)
  {
    *cached_field_index_ptr= field_ptr - table->field;
    field= *field_ptr;
  }
  else
  {
    if (!allow_rowid ||
        my_strcasecmp(system_charset_info, name, "_rowid") ||
        !(field=table->rowid_field))
      return (Field*) 0;
  }

  if (thd->set_query_id)
  {
    if (field->query_id != thd->query_id)
    {
      field->query_id=thd->query_id;
      table->used_fields++;
      table->used_keys.intersect(field->part_of_key);
    }
    else
      thd->dupp_field=field;
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_grants && check_grant_column(thd, &table->grant,
					 table->table_cache_key,
					 table->real_name, name, length))
    return WRONG_GRANT;
#endif
  return field;
}


/*
  Find field in table list.

  SYNOPSIS
    find_field_in_tables()
    thd			Pointer to current thread structure
    item		Field item that should be found
    tables		Tables for scanning
    ref			if view field is found, pointer to view item will
			be returned via this parameter
    report_error	If FALSE then do not report error if item not found
			and return not_found_field
    check_privileges    need to check privileges

  RETURN VALUES
    0			Field is not found or field is not unique- error
			message is reported
    not_found_field	Function was called with report_error == FALSE and
			field was not found. no error message reported.
    view_ref_found	view field is found, item passed through ref parameter
    found field
*/

Field *
find_field_in_tables(THD *thd, Item_ident *item, TABLE_LIST *tables,
		     Item **ref, bool report_error,
                     bool check_privileges)
{
  Field *found=0;
  const char *db=item->db_name;
  const char *table_name=item->table_name;
  const char *name=item->field_name;
  uint length=(uint) strlen(name);
  char name_buff[NAME_LEN+1];


  if (item->cached_table)
  {
    /*
      This shortcut is used by prepared statements. We assuming that 
      TABLE_LIST *tables is not changed during query execution (which 
      is true for all queries except RENAME but luckily RENAME doesn't 
      use fields...) so we can rely on reusing pointer to its member.
      With this optimisation we also miss case when addition of one more
      field makes some prepared query ambiguous and so erronous, but we 
      accept this trade off.
    */
    found= find_field_in_real_table(thd, item->cached_table->table,
				    name, length,
				    test(item->cached_table->
					 table->grant.want_privilege) &&
                                    check_privileges,
				    1, &(item->cached_field_index));

    if (found)
    {
      if (found == WRONG_GRANT)
        return (Field*) 0;
      return found;
    }
  }

  if (db && lower_case_table_names)
  {
    /*
      convert database to lower case for comparision.
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strmake(name_buff, db, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db= name_buff;
  }

  if (table_name && table_name[0])
  {						/* Qualified field */
    bool found_table=0;
    for (; tables; tables= tables->next_local)
    {
      if (!my_strcasecmp(table_alias_charset, tables->alias, table_name) &&
	  (!db || !tables->db ||  !tables->db[0] || !strcmp(db,tables->db)))
      {
	found_table=1;
	Field *find= find_field_in_table(thd, tables, name, item->name,
                                         length, ref,
					 (test(tables->table->grant.
                                               want_privilege) &&
                                          check_privileges),
					 (test(tables->grant.want_privilege) &&
                                          check_privileges),
					 1, &(item->cached_field_index));
	if (find)
	{
	  item->cached_table= tables;
	  if (!tables->cacheable_table)
	    item->cached_table= 0;
	  if (find == WRONG_GRANT)
	    return (Field*) 0;
	  if (db || !thd->where)
	    return find;
	  if (found)
	  {
	    my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			    item->full_name(),thd->where);
	    return (Field*) 0;
	  }
	  found=find;
	}
      }
    }
    if (found)
      return found;
    if (!found_table && report_error)
    {
      char buff[NAME_LEN*2+1];
      if (db && db[0])
      {
	strxnmov(buff,sizeof(buff)-1,db,".",table_name,NullS);
	table_name=buff;
      }
      if (report_error)
      {
	my_printf_error(ER_UNKNOWN_TABLE, ER(ER_UNKNOWN_TABLE), MYF(0),
			table_name, thd->where);
      }
      else
	return (Field*) not_found_field;
    }
    else
      if (report_error)
	my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
			item->full_name(),thd->where);
      else
	return (Field*) not_found_field;
    return (Field*) 0;
  }
  bool allow_rowid= tables && !tables->next_local;	// Only one table
  for (; tables ; tables= tables->next_local)
  {
    if (!tables->table)
    {
      if (report_error)
	my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
			item->full_name(),thd->where);
      return (Field*) not_found_field;
    }

    Field *field= find_field_in_table(thd, tables, name, item->name,
                                      length, ref,
				      (test(tables->table->grant.
                                            want_privilege) &&
                                       check_privileges),
				      (test(tables->grant.want_privilege) &&
                                       check_privileges),
				      allow_rowid, &(item->cached_field_index));
    if (field)
    {
      if (field == WRONG_GRANT)
	return (Field*) 0;
      item->cached_table= tables;
      if (!tables->cacheable_table)
	item->cached_table= 0;
      if (found)
      {
	if (!thd->where)			// Returns first found
	  break;
	my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			name,thd->where);
	return (Field*) 0;
      }
      found=field;
    }
  }
  if (found)
    return found;
  if (report_error)
    my_printf_error(ER_BAD_FIELD_ERROR, ER(ER_BAD_FIELD_ERROR),
		    MYF(0), item->full_name(), thd->where);
  else
    return (Field*) not_found_field;
  return (Field*) 0;
}


/*
  Find Item in list of items (find_field_in_tables analog)

  TODO
    is it better return only counter?

  SYNOPSIS
    find_item_in_list()
    find			Item to find
    items			List of items
    counter			To return number of found item
    report_error
      REPORT_ALL_ERRORS		report errors, return 0 if error
      REPORT_EXCEPT_NOT_FOUND	Do not report 'not found' error and
				return not_found_item, report other errors,
				return 0
      IGNORE_ERRORS		Do not report errors, return 0 if error
      
  RETURN VALUES
    0			Item is not found or item is not unique,
			error message is reported
    not_found_item	Function was called with
			report_error == REPORT_EXCEPT_NOT_FOUND and
			item was not found. No error message was reported
    found field 
*/

// Special Item pointer for find_item_in_list returning
const Item **not_found_item= (const Item**) 0x1;


Item **
find_item_in_list(Item *find, List<Item> &items, uint *counter,
		  find_item_error_report_type report_error)
{
  List_iterator<Item> li(items);
  Item **found=0, **found_unaliased= 0, *item;
  const char *db_name=0;
  const char *field_name=0;
  const char *table_name=0;
  bool found_unaliased_non_uniq= 0;
  uint unaliased_counter;
  if (find->type() == Item::FIELD_ITEM	|| find->type() == Item::REF_ITEM)
  {
    field_name= ((Item_ident*) find)->field_name;
    table_name= ((Item_ident*) find)->table_name;
    db_name=    ((Item_ident*) find)->db_name;
  }

  for (uint i= 0; (item=li++); i++)
  {
    if (field_name && item->type() == Item::FIELD_ITEM)
    {
      Item_field *item_field= (Item_field*) item;

      /*
	In case of group_concat() with ORDER BY condition in the QUERY
	item_field can be field of temporary table without item name 
	(if this field created from expression argument of group_concat()),
	=> we have to check presence of name before compare
      */ 
      if (!item_field->name)
        continue;

      if (table_name)
      {
        /*
          If table name is specified we should find field 'field_name' in
          table 'table_name'. According to SQL-standard we should ignore
          aliases in this case. Note that we should prefer fields from the
          select list over other fields from the tables participating in
          this select in case of ambiguity.

          We use strcmp for table names and database names as these may be
          case sensitive.
          In cases where they are not case sensitive, they are always in lower
          case.
        */
        if (!my_strcasecmp(system_charset_info, item_field->field_name,
                           field_name) &&
            !strcmp(item_field->table_name, table_name) &&
            (!db_name || (item_field->db_name &&
                          !strcmp(item_field->db_name, db_name))))
        {
          if (found)
          {
            if ((*found)->eq(item, 0))
              continue;                         // Same field twice
            if (report_error != IGNORE_ERRORS)
              my_printf_error(ER_NON_UNIQ_ERROR, ER(ER_NON_UNIQ_ERROR),
                              MYF(0), find->full_name(), current_thd->where);
            return (Item**) 0;
          }
          found= li.ref();
          *counter= i;
          if (db_name)
            break;                              // Perfect match
        }
      }
      else if (!my_strcasecmp(system_charset_info, item_field->name,
                              field_name))
      {
        /*
          If table name was not given we should scan through aliases
          (or non-aliased fields) first. We are also checking unaliased
          name of the field in then next else-if, to be able to find
          instantly field (hidden by alias) if no suitable alias (or
          non-aliased field) was found.
        */
        if (found)
        {
          if ((*found)->eq(item, 0))
            continue;                           // Same field twice
          if (report_error != IGNORE_ERRORS)
            my_printf_error(ER_NON_UNIQ_ERROR, ER(ER_NON_UNIQ_ERROR),
                            MYF(0), find->full_name(), current_thd->where);
          return (Item**) 0;
        }
        found= li.ref();
        *counter= i;
      }
      else if (!my_strcasecmp(system_charset_info, item_field->field_name,
                              field_name))
      {
        /*
          We will use un-aliased field or react on such ambiguities only if
          we won't be able to find aliased field.
          Again if we have ambiguity with field outside of select list
          we should prefer fields from select list.
        */
        if (found_unaliased)
        {
          if ((*found_unaliased)->eq(item, 0))
            continue;                           // Same field twice
          found_unaliased_non_uniq= 1;
        }
        else
        {
          found_unaliased= li.ref();
          unaliased_counter= i;
        }
      }
    }
    else if (!table_name && (item->eq(find,0) ||
			     find->name && item->name &&
			     !my_strcasecmp(system_charset_info, 
					    item->name,find->name)))
    {
      found= li.ref();
      *counter= i;
      break;
    }
  }
  if (!found)
  {
    if (found_unaliased_non_uniq)
    {
      if (report_error != IGNORE_ERRORS)
        my_printf_error(ER_NON_UNIQ_ERROR, ER(ER_NON_UNIQ_ERROR), MYF(0),
                        find->full_name(), current_thd->where);
      return (Item **) 0;
    }
    if (found_unaliased)
    {
      found= found_unaliased;
      *counter= unaliased_counter;
    }
  }
  if (found)
    return found;
  if (report_error != REPORT_EXCEPT_NOT_FOUND)
  {
    if (report_error == REPORT_ALL_ERRORS)
      my_printf_error(ER_BAD_FIELD_ERROR, ER(ER_BAD_FIELD_ERROR), MYF(0),
		      find->full_name(), current_thd->where);
    return (Item **) 0;
  }
  else
    return (Item **) not_found_item;
}

/****************************************************************************
** Expand all '*' in given fields
****************************************************************************/

int setup_wild(THD *thd, TABLE_LIST *tables, List<Item> &fields,
	       List<Item> *sum_func_list,
	       uint wild_num)
{
  Item *item;
  if (!wild_num)
    return 0;
  Item_arena *arena= thd->current_arena, backup;

  /*
    Don't use arena if we are not in prepared statements or stored procedures
    For PS/SP we have to use arena to remember the changes
  */
  if (arena->is_conventional())
    arena= 0;                                   // For easier test later one
  else
    thd->set_n_backup_item_arena(arena, &backup);

  List_iterator<Item> it(fields);
  while (wild_num && (item= it++))
  {
    if (item->type() == Item::FIELD_ITEM && ((Item_field*) item)->field_name &&
	((Item_field*) item)->field_name[0] == '*' &&
	!((Item_field*) item)->field)
    {
      uint elem= fields.elements;
      bool any_privileges= ((Item_field *) item)->any_privileges;
      Item_subselect *subsel= thd->lex->current_select->master_unit()->item;
      if (subsel &&
          subsel->substype() == Item_subselect::EXISTS_SUBS)
      {
        /*
          It is EXISTS(SELECT * ...) and we can replace * by any constant.

          Item_int do not need fix_fields() because it is basic constant.
        */
        it.replace(new Item_int("Not_used", (longlong) 1, 21));
      }
      else if (insert_fields(thd,tables,((Item_field*) item)->db_name,
                             ((Item_field*) item)->table_name, &it,
                             any_privileges, arena != 0))
      {
	if (arena)
	  thd->restore_backup_item_arena(arena, &backup);
	return (-1);
      }
      if (sum_func_list)
      {
	/*
	  sum_func_list is a list that has the fields list as a tail.
	  Because of this we have to update the element count also for this
	  list after expanding the '*' entry.
	*/
	sum_func_list->elements+= fields.elements - elem;
      }
      wild_num--;
    }
  }
  if (arena)
  {
    /* make * substituting permanent */
    SELECT_LEX *select_lex= thd->lex->current_select;
    select_lex->with_wild= 0;
    select_lex->item_list= fields;

    thd->restore_backup_item_arena(arena, &backup);
  }
  return 0;
}

/****************************************************************************
** Check that all given fields exists and fill struct with current data
****************************************************************************/

int setup_fields(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables, 
		 List<Item> &fields, bool set_query_id,
		 List<Item> *sum_func_list, bool allow_sum_func)
{
  reg2 Item *item;
  List_iterator<Item> it(fields);
  SELECT_LEX *select_lex= thd->lex->current_select;
  DBUG_ENTER("setup_fields");

  thd->set_query_id=set_query_id;
  thd->allow_sum_func= allow_sum_func;
  thd->where="field list";

  Item **ref= ref_pointer_array;
  while ((item= it++))
  {
    if (!item->fixed && item->fix_fields(thd, tables, it.ref()) ||
	(item= *(it.ref()))->check_cols(1))
    {
      select_lex->no_wrap_view_item= 0;
      DBUG_RETURN(-1); /* purecov: inspected */
    }
    if (ref)
      *(ref++)= item;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM &&
	sum_func_list)
      item->split_sum_func(ref_pointer_array, *sum_func_list);
    thd->used_tables|=item->used_tables();
  }
  DBUG_RETURN(test(thd->net.report_error));
}


/*
  prepare tables

  SYNOPSIS
    setup_tables()
    thd    - thread handler
    tables - tables list
    conds  - condition of current SELECT (can be changed by VIEW)

   RETURN
     0	ok;  In this case *map will includes the choosed index
     1	error

   NOTE
     Remap table numbers if INSERT ... SELECT
     Check also that the 'used keys' and 'ignored keys' exists and set up the
     table structure accordingly

     This has to be called for all tables that are used by items, as otherwise
     table->map is not set and all Item_field will be regarded as const items.

     if tables do not contain VIEWs it is OK to pass 0 as conds
*/

bool setup_tables(THD *thd, TABLE_LIST *tables, Item **conds)
{
  DBUG_ENTER("setup_tables");
  if (!tables || tables->setup_is_done)
    DBUG_RETURN(0);
  tables->setup_is_done= 1;
  uint tablenr=0;
  for (TABLE_LIST *table_list= tables;
       table_list;
       table_list= table_list->next_local, tablenr++)
  {
    TABLE *table= table_list->table;
    setup_table_map(table, table_list, tablenr);
    table->used_keys= table->keys_for_keyread;
    if (table_list->use_index)
    {
      key_map map;
      get_key_map_from_key_list(&map, table, table_list->use_index);
      if (map.is_set_all())
	DBUG_RETURN(1);
      table->keys_in_use_for_query=map;
    }
    if (table_list->ignore_index)
    {
      key_map map;
      get_key_map_from_key_list(&map, table, table_list->ignore_index);
      if (map.is_set_all())
	DBUG_RETURN(1);
      table->keys_in_use_for_query.subtract(map);
    }
    table->used_keys.intersect(table->keys_in_use_for_query);
    if (table_list->ancestor && table_list->setup_ancestor(thd, conds))
      DBUG_RETURN(1);
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0),MAX_TABLES);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
   Create a key_map from a list of index names

   SYNOPSIS
     get_key_map_from_key_list()
     map		key_map to fill in
     table		Table
     index_list		List of index names

   RETURN
     0	ok;  In this case *map will includes the choosed index
     1	error
*/

bool get_key_map_from_key_list(key_map *map, TABLE *table,
                               List<String> *index_list)
{
  List_iterator_fast<String> it(*index_list);
  String *name;
  uint pos;

  map->clear_all();
  while ((name=it++))
  {
    if (table->keynames.type_names == 0 ||
        (pos= find_type(&table->keynames, name->ptr(), name->length(), 1)) <=
        0)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), name->c_ptr(),
	       table->real_name);
      map->set_all();
      return 1;
    }
    map->set_bit(pos-1);
  }
  return 0;
}


/*
  Drops in all fields instead of current '*' field

  SYNOPSIS
    insert_fields()
    thd			Thread handler
    tables		List of tables
    db_name		Database name in case of 'database_name.table_name.*'
    table_name		Table name in case of 'table_name.*'
    it			Pointer to '*'
    any_privileges	0 If we should ensure that we have SELECT privileges
		          for all columns
                        1 If any privilege is ok
    allocate_view_names if true view names will be copied to current Item_arena                         memory (made for SP/PS)
  RETURN
    0	ok
        'it' is updated to point at last inserted
    1	error.  The error message is sent to client
*/

bool
insert_fields(THD *thd, TABLE_LIST *tables, const char *db_name,
	      const char *table_name, List_iterator<Item> *it,
              bool any_privileges, bool allocate_view_names)
{
  /* allocate variables on stack to avoid pool alloaction */
  Field_iterator_table table_iter;
  Field_iterator_view view_iter;
  uint found;
  char name_buff[NAME_LEN+1];
  DBUG_ENTER("insert_fields");

  if (db_name && lower_case_table_names)
  {
    /*
      convert database to lower case for comparison
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strmake(name_buff, db_name, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db_name= name_buff;
  }

  found= 0;
  for (; tables; tables= tables->next_local)
  {
    Field_iterator *iterator;
    TABLE_LIST *natural_join_table;
    Field *field;
    TABLE_LIST *embedded;
    TABLE_LIST *last;
    TABLE_LIST *embedding;
    TABLE *table= tables->table;

    if (!table_name || (!my_strcasecmp(table_alias_charset, table_name,
				       tables->alias) &&
			(!db_name || !strcmp(tables->db,db_name))))
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* Ensure that we have access right to all columns */
      if (!(table->grant.privilege & SELECT_ACL) && !any_privileges)
      {
        if (tables->view)
        {
          view_iter.set(tables);
          if (check_grant_all_columns(thd, SELECT_ACL, &tables->grant,
                                      tables->view_db.str,
                                      tables->view_name.str,
                                      &view_iter))
            goto err;
        }
        else
        {
          table_iter.set(tables);
          if (check_grant_all_columns(thd, SELECT_ACL, &table->grant,
                                      table->table_cache_key, table->real_name,
                                      &table_iter))
            goto err;
        }
      }
#endif
      natural_join_table= 0;
      thd->used_tables|= table->map;
      last= embedded= tables;

      while ((embedding= embedded->embedding) &&
              embedding->join_list->elements != 1)
      {
        TABLE_LIST *next;
        List_iterator_fast<TABLE_LIST> it(embedding->nested_join->join_list);
        last= it++;
        while ((next= it++))
          last= next;
        if (last != tables)
          break;
        embedded= embedding;
      }

      if (tables == last &&
          !embedded->outer_join &&
          embedded->natural_join &&
          !embedded->natural_join->outer_join)
      {
        embedding= embedded->natural_join;
        while (embedding->nested_join)
          embedding= embedding->nested_join->join_list.head();
        natural_join_table= embedding;
      }
      if (tables->field_translation)
        iterator= &view_iter;
      else
        iterator= &table_iter;
      iterator->set(tables);

      for (; !iterator->end_of_fields(); iterator->next())
      {
        Item *not_used_item;
        uint not_used_field_index= NO_CACHED_FIELD_INDEX;
        const char *field_name= iterator->name();
        /* Skip duplicate field names if NATURAL JOIN is used */
        if (!natural_join_table ||
            !find_field_in_table(thd, natural_join_table, field_name,
                                 field_name,
                                 strlen(field_name), &not_used_item, 0, 0, 0,
                                 &not_used_field_index))
        {
          Item *item= iterator->item(thd);
          if (!found++)
            (void) it->replace(item);		// Replace '*'
          else
            it->after(item);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
          if (any_privileges)
          {
            /*
              In time of view creation MEGRGE algorithm for underlying
              VIEWs can't be used => it should be Item_field
            */
            DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
            Item_field *fld= (Item_field*)item;
            char *db, *tab;
            if (tables->view)
            {
              db= tables->view_db.str;
              tab= tables->view_name.str;
            }
            else
            {
              db= tables->db;
              tab= tables->real_name;
            }
            if (!(fld->have_privileges= (get_column_grant(thd,
                                                          &table->grant,
                                                          db,
                                                          tab,
                                                          fld->field_name) &
                                         VIEW_ANY_ACL)))
            {
              my_printf_error(ER_COLUMNACCESS_DENIED_ERROR,
                              ER(ER_COLUMNACCESS_DENIED_ERROR),
                              MYF(0),
                              "ANY",
                              thd->priv_user,
                              thd->host_or_ip,
                              fld->field_name,
                              tab);
              goto err;
            }
          }
#endif
        }
        if ((field= iterator->field()))
        {
          /*
            Mark if field used before in this select.
            Used by 'insert' to verify if a field name is used twice
          */
          if (field->query_id == thd->query_id)
            thd->dupp_field=field;
          field->query_id=thd->query_id;
          table->used_keys.intersect(field->part_of_key);
        }
        else if (allocate_view_names &&
                 thd->lex->current_select->first_execution)
        {
          Item_field *item= new Item_field(thd->strdup(tables->view_db.str),
                                           thd->strdup(tables->view_name.str),
                                           thd->strdup(field_name));
          /*
            during cleunup() this item will be put in list to replace
            expression from VIEW
          */
          item->changed_during_fix_field= it->ref();
        }

      }
      /* All fields are used */
      table->used_fields=table->fields;
    }
  }
  if (found)
    DBUG_RETURN(0);

  if (!table_name)
    my_error(ER_NO_TABLES_USED, MYF(0));
  else
    my_error(ER_BAD_TABLE_ERROR, MYF(0), table_name);

err:
  send_error(thd);
  DBUG_RETURN(1);
}


/*
** Fix all conditions and outer join expressions
*/

int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds)
{
  table_map not_null_tables= 0;
  SELECT_LEX *select_lex= thd->lex->current_select;
  Item_arena *arena= thd->current_arena;
  Item_arena backup;
  bool save_wrapper= thd->lex->current_select->no_wrap_view_item;
  DBUG_ENTER("setup_conds");

  if (select_lex->conds_processed_with_permanent_arena ||
      arena->is_conventional())
    arena= 0;                                   // For easier test

  thd->set_query_id=1;

  thd->lex->current_select->no_wrap_view_item= 1;
  select_lex->cond_count= 0;
  if (*conds)
  {
    thd->where="where clause";
    if (!(*conds)->fixed && (*conds)->fix_fields(thd, tables, conds) ||
	(*conds)->check_cols(1))
      goto err_no_arena;
  }

  /* Check if we are using outer joins */
  for (TABLE_LIST *table= tables; table; table= table->next_local)
  {
    TABLE_LIST *embedded;
    TABLE_LIST *embedding= table;
    do
    {
      embedded= embedding;
      if (embedded->on_expr)
      {
        /* Make a join an a expression */
        thd->where="on clause";
        if (!embedded->on_expr->fixed &&
            embedded->on_expr->fix_fields(thd, tables, &embedded->on_expr) ||
	    embedded->on_expr->check_cols(1))
	  goto err_no_arena;
        select_lex->cond_count++;
      }
      if (embedded->natural_join)
      {
        /* Make a join of all fields wich have the same name */
        TABLE_LIST *tab1= embedded;
        TABLE_LIST *tab2= embedded->natural_join;
        if (!(embedded->outer_join & JOIN_TYPE_RIGHT))
        {
          while (tab1->nested_join)
          {
            TABLE_LIST *next;
            List_iterator_fast<TABLE_LIST> it(tab1->nested_join->join_list);
            tab1= it++;
            while ((next= it++))
              tab1= next;
          }
        }
        else
        {
          while (tab1->nested_join)
            tab1= tab1->nested_join->join_list.head();
        }
        if (embedded->outer_join & JOIN_TYPE_RIGHT)
        {
          while (tab2->nested_join)
          {
            TABLE_LIST *next;
            List_iterator_fast<TABLE_LIST> it(tab2->nested_join->join_list);
            tab2= it++;
            while ((next= it++))
              tab2= next;
          }
        }
        else
        {
          while (tab2->nested_join)
            tab2= tab2->nested_join->join_list.head();
        }

        if (arena)
	  thd->set_n_backup_item_arena(arena, &backup);

        TABLE *t1=tab1->table;
        TABLE *t2=tab2->table;
        Field_iterator_table table_iter;
        Field_iterator_view view_iter;
        Field_iterator *iterator;
        Field *t1_field, *t2_field;
        Item *item_t2;
        Item_cond_and *cond_and=new Item_cond_and();

        if (!cond_and)				// If not out of memory
	  goto err_no_arena;
        cond_and->top_level_item();

        if (table->field_translation)
        {
          iterator= &view_iter;
          view_iter.set(tab1);
        }
        else
        {
          iterator= &table_iter;
          table_iter.set(tab1);
        }

        for (; !iterator->end_of_fields(); iterator->next())
        {
          const char *t1_field_name= iterator->name();
          uint not_used_field_index= NO_CACHED_FIELD_INDEX;

          if ((t2_field= find_field_in_table(thd, tab2, t1_field_name,
                                             t1_field_name,
                                             strlen(t1_field_name), &item_t2,
                                             0, 0, 0,
                                             &not_used_field_index)))
          {
            if (t2_field != view_ref_found)
            {
              if (!(item_t2= new Item_field(t2_field)))
                goto err;
              /* Mark field used for table cache */
              t2_field->query_id= thd->query_id;
              t2->used_keys.intersect(t2_field->part_of_key);
            }
            if ((t1_field= iterator->field()))
            {
              /* Mark field used for table cache */
              t1_field->query_id= thd->query_id;
              t1->used_keys.intersect(t1_field->part_of_key);
            }
            Item_func_eq *tmp= new Item_func_eq(iterator->item(thd),
                                                item_t2);
            if (!tmp)
              goto err;
            cond_and->list.push_back(tmp);
          }
        }
        select_lex->cond_count+= cond_and->list.elements;

        // to prevent natural join processing during PS re-execution
        embedding->natural_join= 0;

        if (cond_and->list.elements)
        {
          COND *on_expr= cond_and;
          on_expr->fix_fields(thd, 0, &on_expr);
          if (!embedded->outer_join)			// Not left join
          {
            *conds= and_conds(*conds, cond_and);
            // fix_fields() should be made with temporary memory pool
            if (arena)
              thd->restore_backup_item_arena(arena, &backup);
            if (*conds && !(*conds)->fixed)
            {
              if ((*conds)->fix_fields(thd, tables, conds))
                goto err_no_arena;
            }
          }
          else
          {
            embedded->on_expr= and_conds(embedded->on_expr, cond_and);
            // fix_fields() should be made with temporary memory pool
            if (arena)
              thd->restore_backup_item_arena(arena, &backup);
            if (embedded->on_expr && !embedded->on_expr->fixed)
            {
              if (embedded->on_expr->fix_fields(thd, tables, &table->on_expr))
                goto err_no_arena;
            }
          }
        }
      }
      embedding= embedded->embedding;
    }
    while (embedding &&
           embedding->nested_join->join_list.head() == embedded);
  }

  if (arena)
  {
    /*
      We are in prepared statement preparation code => we should store
      WHERE clause changing for next executions.

      We do this ON -> WHERE transformation only once per PS/SP statement.
    */
    select_lex->where= *conds;
    select_lex->conds_processed_with_permanent_arena= 1;
  }
  thd->lex->current_select->no_wrap_view_item= save_wrapper;
  DBUG_RETURN(test(thd->net.report_error));

err:
  if (arena)
    thd->restore_backup_item_arena(arena, &backup);
err_no_arena:
  thd->lex->current_select->no_wrap_view_item= save_wrapper;
  DBUG_RETURN(1);
}


/******************************************************************************
** Fill a record with data (for INSERT or UPDATE)
** Returns : 1 if some field has wrong type
******************************************************************************/

int
fill_record(List<Item> &fields,List<Item> &values, bool ignore_errors)
{
  List_iterator_fast<Item> f(fields),v(values);
  Item *value;
  Item_field *field;
  DBUG_ENTER("fill_record");

  while ((field=(Item_field*) f++))
  {
    value=v++;
    Field *rfield= field->field;
    TABLE *table= rfield->table;
    if (rfield == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if ((value->save_in_field(rfield, 0) < 0) && !ignore_errors)
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int
fill_record(Field **ptr,List<Item> &values, bool ignore_errors)
{
  List_iterator_fast<Item> v(values);
  Item *value;
  DBUG_ENTER("fill_record");

  Field *field;
  while ((field = *ptr++))
  {
    value=v++;
    TABLE *table= field->table;
    if (field == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if ((value->save_in_field(field, 0) < 0) && !ignore_errors)
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static void mysql_rm_tmp_tables(void)
{
  uint i, idx;
  char	filePath[FN_REFLEN], *tmpdir;
  MY_DIR *dirp;
  FILEINFO *file;
  DBUG_ENTER("mysql_rm_tmp_tables");

  for (i=0; i<=mysql_tmpdir_list.max; i++)
  {
    tmpdir=mysql_tmpdir_list.list[i];
  /* See if the directory exists */
    if (!(dirp = my_dir(tmpdir,MYF(MY_WME | MY_DONT_SORT))))
      continue;

    /* Remove all SQLxxx tables from directory */

  for (idx=0 ; idx < (uint) dirp->number_off_files ; idx++)
  {
    file=dirp->dir_entry+idx;

    /* skiping . and .. */
    if (file->name[0] == '.' && (!file->name[1] ||
       (file->name[1] == '.' &&  !file->name[2])))
      continue;

    if (!bcmp(file->name,tmp_file_prefix,tmp_file_prefix_length))
    {
        sprintf(filePath,"%s%s",tmpdir,file->name);
        VOID(my_delete(filePath,MYF(MY_WME)));
    }
  }
  my_dirend(dirp);
  }
  DBUG_VOID_RETURN;
}



/*****************************************************************************
	unireg support functions
*****************************************************************************/

/*
** Invalidate any cache entries that are for some DB
** We can't use hash_delete when looping hash_elements. We mark them first
** and afterwards delete those marked unused.
*/

void remove_db_from_cache(const my_string db)
{
  for (uint idx=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *table=(TABLE*) hash_element(&open_cache,idx);
    if (!strcmp(table->table_cache_key,db))
    {
      table->version=0L;			/* Free when thread is ready */
      if (!table->in_use)
	relink_unused(table);
    }
  }
  while (unused_tables && !unused_tables->version)
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
}


/*
** free all unused tables
*/

void flush_tables()
{
  (void) pthread_mutex_lock(&LOCK_open);
  while (unused_tables)
    hash_delete(&open_cache,(byte*) unused_tables);
  (void) pthread_mutex_unlock(&LOCK_open);
}


/*
** Mark all entries with the table as deleted to force an reopen of the table
** Returns true if the table is in use by another thread
*/

bool remove_table_from_cache(THD *thd, const char *db, const char *table_name,
			     bool return_if_owned_by_thd)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE *table;
  bool result=0;
  DBUG_ENTER("remove_table_from_cache");

  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
  {
    THD *in_use;
    table->version=0L;			/* Free when thread is ready */
    if (!(in_use=table->in_use))
    {
      DBUG_PRINT("info",("Table was not in use"));
      relink_unused(table);
    }
    else if (in_use != thd)
    {
      in_use->some_tables_deleted=1;
      if (table->db_stat)
	result=1;
      /* Kill delayed insert threads */
      if ((in_use->system_thread & SYSTEM_THREAD_DELAYED_INSERT) &&
          ! in_use->killed)
      {
	in_use->killed= THD::KILL_CONNECTION;
	pthread_mutex_lock(&in_use->mysys_var->mutex);
	if (in_use->mysys_var->current_cond)
	{
	  pthread_mutex_lock(in_use->mysys_var->current_mutex);
	  pthread_cond_broadcast(in_use->mysys_var->current_cond);
	  pthread_mutex_unlock(in_use->mysys_var->current_mutex);
	}
	pthread_mutex_unlock(&in_use->mysys_var->mutex);
      }
      /*
	Now we must abort all tables locks used by this thread
	as the thread may be waiting to get a lock for another table
      */
      for (TABLE *thd_table= in_use->open_tables;
	   thd_table ;
	   thd_table= thd_table->next)
      {
	if (thd_table->db_stat)			// If table is open
	  mysql_lock_abort_for_thread(thd, thd_table);
      }
    }
    else
      result= result || return_if_owned_by_thd;
  }
  while (unused_tables && !unused_tables->version)
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
  DBUG_RETURN(result);
}

int setup_ftfuncs(SELECT_LEX *select_lex)
{
  List_iterator<Item_func_match> li(*(select_lex->ftfunc_list)),
                                 lj(*(select_lex->ftfunc_list));
  Item_func_match *ftf, *ftf2;

  while ((ftf=li++))
  {
    if (ftf->fix_index())
      return 1;
    lj.rewind();
    while ((ftf2=lj++) != ftf)
    {
      if (ftf->eq(ftf2,1) && !ftf2->master)
        ftf2->master=ftf;
    }
  }

  return 0;
}


int init_ftfuncs(THD *thd, SELECT_LEX *select_lex, bool no_order)
{
  if (select_lex->ftfunc_list->elements)
  {
    List_iterator<Item_func_match> li(*(select_lex->ftfunc_list));
    Item_func_match *ifm;
    DBUG_PRINT("info",("Performing FULLTEXT search"));
    thd->proc_info="FULLTEXT initialization";

    while ((ifm=li++))
      ifm->init_search(no_order);
  }
  return 0;
}


/*
  open new .frm format table

  SYNOPSIS
    open_new_frm()
    path	  path to .frm
    alias	  alias for table
    db            database
    table_name    name of table
    db_stat	  open flags (for example HA_OPEN_KEYFILE|HA_OPEN_RNDFILE..)
		  can be 0 (example in ha_example_table)
    prgflag	  READ_ALL etc..
    ha_open_flags HA_OPEN_ABORT_IF_LOCKED etc..
    outparam	  result table
    table_desc	  TABLE_LIST descriptor
    mem_root	  temporary MEM_ROOT for parsing
*/

static my_bool
open_new_frm(const char *path, const char *alias,
             const char *db, const char *table_name,
             uint db_stat, uint prgflag,
	     uint ha_open_flags, TABLE *outparam, TABLE_LIST *table_desc,
	     MEM_ROOT *mem_root)
{
  LEX_STRING pathstr;
  File_parser *parser;
  DBUG_ENTER("open_new_frm");

  pathstr.str=    (char*) path;
  pathstr.length= strlen(path);

  if ((parser= sql_parse_prepare(&pathstr, mem_root, 1)))
  {
    if (!strncmp("VIEW", parser->type()->str, parser->type()->length))
    {
      if (table_desc == 0 || table_desc->required_type == FRMTYPE_TABLE)
      {
        my_error(ER_WRONG_OBJECT, MYF(0), db, table_name, "BASE TABLE");
        goto err;
      }
      if (mysql_make_view(parser, table_desc))
        goto err;
    }
    else
    {
      /* only VIEWs are supported now */
      my_error(ER_FRM_UNKNOWN_TYPE, MYF(0), path,  parser->type()->str);
      goto err;
    }
    DBUG_RETURN(0);
  }
 
err:
  bzero(outparam, sizeof(TABLE));	// do not run repair
  DBUG_RETURN(1);
}
