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

static int open_unireg_entry(THD *thd,TABLE *entry,const char *db,
			     const char *name, const char *alias);
static void free_cache_entry(TABLE *entry);
static void mysql_rm_tmp_tables(void);


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
    for (TABLE_LIST *table=tables ; table ; table=table->next)
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
    mysql_ha_flush(thd, tables, MYSQL_HA_REOPEN_ON_USAGE | MYSQL_HA_FLUSH_ALL);
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
    for (TABLE *table=thd->open_tables; table ; table=table->next)
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

  bool found_old_table=0;

  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  /* VOID(pthread_sigmask(SIG_SETMASK,&thd->block_signals,NULL)); */
  if (!lock_in_use)
    VOID(pthread_mutex_lock(&LOCK_open));
  safe_mutex_assert_owner(&LOCK_open);

  DBUG_PRINT("info", ("thd->open_tables=%p", thd->open_tables));

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
  Find first suitable table by alias in given list.

  SYNOPSIS
    find_table_in_list()
    table - pointer to table list
    db_name - data base name or 0 for any
    table_name - table name or 0 for any

  RETURN VALUES
    NULL	Table not found
    #		Pointer to found table.
*/

TABLE_LIST * find_table_in_list(TABLE_LIST *table,
				const char *db_name, const char *table_name)
{
  for (; table; table= table->next)
    if ((!db_name || !strcmp(table->db, db_name)) &&
	(!table_name || !my_strcasecmp(table_alias_charset,
				       table->alias, table_name)))
      break;
  return table;
}

/*
  Find real table in given list.

  SYNOPSIS
    find_real_table_in_list()
    table - pointer to table list
    db_name - data base name
    table_name - table name

  RETURN VALUES
    NULL	Table not found
    #		Pointer to found table.
*/

TABLE_LIST * find_real_table_in_list(TABLE_LIST *table,
				     const char *db_name,
				     const char *table_name)
{
  for (; table; table= table->next)
    if (!strcmp(table->db, db_name) &&
	!strcmp(table->real_name, table_name))
      break;
  return table;
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
  if (open_unireg_entry(thd, table, db, table_name, table_name) ||
      !(table->table_cache_key =memdup_root(&table->mem_root,(char*) key,
					    key_length)))
  {
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


TABLE *open_table(THD *thd,const char *db,const char *table_name,
		  const char *alias,bool *refresh)
{
  reg1	TABLE *table;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  DBUG_ENTER("open_table");

  /* find a unused table in the open table cache */
  if (refresh)
    *refresh=0;
  if (thd->killed)
    DBUG_RETURN(0);
  key_length= (uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  int4store(key + key_length, thd->server_id);
  int4store(key + key_length + 4, thd->variables.pseudo_thread_id);

  for (table=thd->temporary_tables; table ; table=table->next)
  {
    if (table->key_length == key_length+8 &&
	!memcmp(table->table_cache_key,key,key_length+8))
    {
      if (table->query_id == thd->query_id)
      {
	my_printf_error(ER_CANT_REOPEN_TABLE,
			ER(ER_CANT_REOPEN_TABLE),MYF(0),table->table_name);
	DBUG_RETURN(0);
      }
      table->query_id=thd->query_id;
      table->clear_query_id=1;
      thd->tmp_table_used= 1;
      DBUG_PRINT("info",("Using temporary table"));
      goto reset;
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
        DBUG_PRINT("info",("Using locked table"));
	goto reset;
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
  mysql_ha_flush(thd, (TABLE_LIST*) NULL, MYSQL_HA_REOPEN_ON_USAGE);

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
	VOID(pthread_mutex_unlock(&LOCK_open));
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
    if (open_unireg_entry(thd, table,db,table_name,alias) ||
	!(table->table_cache_key=memdup_root(&table->mem_root,(char*) key,
					     key_length)))
    {
      table->next=table->prev=table;
      free_cache_entry(table);
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    table->key_length=key_length;
    table->version=refresh_version;
    table->flush_version=flush_version;
    DBUG_PRINT("info", ("inserting table %p into the cache", table));
    VOID(my_hash_insert(&open_cache,(byte*) table));
  }

  table->in_use=thd;
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
#if MYSQL_VERSION_ID < 40100
  /*
    If per-connection "new" variable (represented by variables.new_mode)
    is set then we should pretend that the length of TIMESTAMP field is 19.
    The cheapest (from perfomance viewpoint) way to achieve that is to set
    field_length of all Field_timestamp objects in a table after opening
    it (to 19 if new_mode is true or to original field length otherwise).
    We save value of new_mode variable in TABLE::timestamp_mode to
    not perform this setup if new_mode value is the same between sequential
    table opens.
  */
  my_bool new_mode= thd->variables.new_mode;
  if (table->timestamp_mode != new_mode)
  {
    for (uint i=0 ; i < table->fields ; i++)
    {
      Field *field= table->field[i];

      if (field->type() == FIELD_TYPE_TIMESTAMP)
        field->field_length= new_mode ? 19 :
                             ((Field_timestamp *)(field))->orig_field_length;
    }
    table->timestamp_mode= new_mode;
  }
#endif
  /* These variables are also set in reopen_table() */
  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->outer_join= table->null_row= table->maybe_null= table->force_index= 0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query= table->keys_in_use;
  table->used_keys= table->keys_for_keyread;
  if (table->timestamp_field)
    table->timestamp_field_type= table->timestamp_field->get_auto_set_type();
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

  if (open_unireg_entry(current_thd,&tmp,db,table_name,table->table_name))
    goto end;
  free_io_cache(table);

  if (!(tmp.table_cache_key= memdup_root(&tmp.mem_root,db,
					 table->key_length)))
  {
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
    mysql_ha_flush(thd, (TABLE_LIST*) NULL, MYSQL_HA_REOPEN_ON_USAGE);
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

  NOTES
   Extra argument for open is taken from thd->open_options

  RETURN
    0	ok
    #	Error
*/

static int open_unireg_entry(THD *thd, TABLE *entry, const char *db,
			     const char *name, const char *alias)
{
  char path[FN_REFLEN];
  int error;
  uint discover_retry_count= 0;
  DBUG_ENTER("open_unireg_entry");

  strxmov(path, mysql_data_home, "/", db, "/", name, NullS);
  while (openfrm(path,alias,
	       (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
		       HA_TRY_READ_ONLY),
	       READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      thd->open_options, entry))
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
      if (ha_create_table_from_engine(thd, db, name, TRUE) != 0)
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
    if (openfrm(path,alias,
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
      sql_print_error("Couldn't repair table: %s.%s",db,name);
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
        sql_print_error("When opening HEAP table, could not allocate \
memory to write 'DELETE FROM `%s`.`%s`' to the binary log",db,name);
        if (entry->file)
          closefrm(entry);
        goto err;
      }
    }
  }
  DBUG_RETURN(0);
err:
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

  thd->current_tablenr= 0;
 restart:
  *counter= 0;
  thd->proc_info="Opening tables";
  for (tables=start ; tables ; tables=tables->next)
  {
    /*
      Ignore placeholders for derived tables. After derived tables
      processing, link to created temporary table will be put here.
     */
    if (tables->derived)
      continue;
    (*counter)++;
    if (!tables->table &&
	!(tables->table= open_table(thd,
				    tables->db,
				    tables->real_name,
				    tables->alias, &refresh)))
    {
      if (refresh)				// Refresh in progress
      {
	/* close all 'old' tables used by this thread */
	pthread_mutex_lock(&LOCK_open);
	// if query_id is not reset, we will get an error
	// re-opening a temp table
	thd->version=refresh_version;
	TABLE **prev_table= &thd->open_tables;
	bool found=0;
	for (TABLE_LIST *tmp=start ; tmp ; tmp=tmp->next)
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
    if (tables->lock_type != TL_UNLOCK && ! thd->locked_tables)
      tables->table->reginfo.lock_type=tables->lock_type;
    tables->table->grant= tables->grant;
  }
  thd->proc_info=0;
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
  while (!(table=open_table(thd,table_list->db,
			    table_list->real_name,table_list->alias,
			    &refresh)) && refresh) ;

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

  NOTE
    The lock will automaticly be freed by close_thread_tables()
*/

int open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("open_and_lock_tables");
  uint counter;
  if (open_tables(thd, tables, &counter) || lock_tables(thd, tables, counter))
    DBUG_RETURN(-1);				/* purecov: inspected */
  relink_tables_for_derived(thd);
  DBUG_RETURN(mysql_handle_derived(thd->lex));
}


/*
  Let us propagate pointers to open tables from global table list
  to table lists in particular selects if needed.
*/

void relink_tables_for_derived(THD *thd)
{
  if (thd->lex->all_selects_list->next_select_in_list() ||
      thd->lex->time_zone_tables_used)
  {
    for (SELECT_LEX *sl= thd->lex->all_selects_list;
	 sl;
	 sl= sl->next_select_in_list())
      for (TABLE_LIST *cursor= (TABLE_LIST *) sl->table_list.first;
           cursor;
           cursor=cursor->next)
        if (cursor->table_list)
          cursor->table= cursor->table_list->table;
  }
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
    for (table = tables ; table ; table=table->next)
    {
      if (!table->derived)
	*(ptr++)= table->table;
    }
    if (!(thd->lock=mysql_lock_tables(thd,start, (uint) (ptr - start))))
      return -1;				/* purecov: inspected */
  }
  else
  {
    for (table = tables ; table ; table=table->next)
    {
      if (!table->derived && 
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

  if (openfrm(path, table_name,
	      (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX),
	      READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      ha_open_options,
	      tmp_table))
  {
    my_free((char*) tmp_table,MYF(0));
    DBUG_RETURN(0);
  }

  tmp_table->reginfo.lock_type=TL_WRITE;	 // Simulate locked
  tmp_table->in_use= thd;
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
    sql_print_warning("Could not remove tmp table: '%s', error: %d",
                      path, my_errno);
  }
  delete file;
  DBUG_RETURN(error);
}


/*****************************************************************************
** find field in list or tables. if field is unqualifed and unique,
** return unique field
******************************************************************************/

#define WRONG_GRANT (Field*) -1

Field *find_field_in_table(THD *thd,TABLE *table,const char *name,uint length,
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
  if (check_grants && check_grant_column(thd,table,name,length))
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
    where		Table where field found will be returned via
			this parameter
    report_error	If FALSE then do not report error if item not found
			and return not_found_field

  RETURN VALUES
    0			Field is not found or field is not unique- error
			message is reported
    not_found_field	Function was called with report_error == FALSE and
			field was not found. no error message reported.
    found field
*/

// Special Field pointer for find_field_in_tables returning
const Field *not_found_field= (Field*) 0x1;

Field *
find_field_in_tables(THD *thd, Item_ident *item, TABLE_LIST *tables,
		     TABLE_LIST **where, bool report_error)
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
    found= find_field_in_table(thd, item->cached_table->table, name, length,
                               test(item->cached_table->
				    table->grant.want_privilege),
                               1, &(item->cached_field_index));

    if (found)
    {
      (*where)= tables;
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
    for (; tables ; tables=tables->next)
    {
      if (!my_strcasecmp(table_alias_charset, tables->alias, table_name) &&
	  (!db || !tables->db ||  !tables->db[0] || !strcmp(db,tables->db)))
      {
	found_table=1;
	Field *find=find_field_in_table(thd,tables->table,name,length,
					test(tables->table->grant.
					     want_privilege),
					1, &(item->cached_field_index));
	if (find)
	{
	  (*where)= item->cached_table= tables;
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
  bool allow_rowid= tables && !tables->next;	// Only one table
  for (; tables ; tables=tables->next)
  {
    if (!tables->table)
    {
      if (report_error)
	my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
			item->full_name(),thd->where);
      return (Field*) not_found_field;
    }

    Field *field=find_field_in_table(thd,tables->table,name,length,
				     test(tables->table->grant.want_privilege),
				     allow_rowid, &(item->cached_field_index));
    if (field)
    {
      if (field == WRONG_GRANT)
	return (Field*) 0;
      (*where)= item->cached_table= tables;
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
    unaliased                   Set to true if item is field which was found
                                by original field name and not by its alias
                                in item list. Set to false otherwise.

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
                  find_item_error_report_type report_error, bool *unaliased)
{
  List_iterator<Item> li(items);
  Item **found=0, **found_unaliased= 0, *item;
  const char *db_name=0;
  const char *field_name=0;
  const char *table_name=0;
  bool found_unaliased_non_uniq= 0;
  uint unaliased_counter;

  *unaliased= FALSE;

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
          aliases in this case.

          Since we should NOT prefer fields from the select list over
          other fields from the tables participating in this select in
          case of ambiguity we have to do extra check outside this function.

          We use strcmp for table names and database names as these may be
          case sensitive. In cases where they are not case sensitive, they
          are always in lower case.

	  item_field->field_name and item_field->table_name can be 0x0 if
	  item is not fix_field()'ed yet.
        */
        if (item_field->field_name && item_field->table_name &&
	    !my_strcasecmp(system_charset_info, item_field->field_name,
                           field_name) &&
            !strcmp(item_field->table_name, table_name) &&
            (!db_name || (item_field->db_name &&
                          !strcmp(item_field->db_name, db_name))))
        {
          if (found_unaliased)
          {
            if ((*found_unaliased)->eq(item, 0))
              continue;
            /*
              Two matching fields in select list.
              We already can bail out because we are searching through
              unaliased names only and will have duplicate error anyway.
            */
            if (report_error != IGNORE_ERRORS)
              my_printf_error(ER_NON_UNIQ_ERROR, ER(ER_NON_UNIQ_ERROR),
                              MYF(0), find->full_name(), current_thd->where);
            return (Item**) 0;
          }
          found_unaliased= li.ref();
          unaliased_counter= i;
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
      *unaliased= TRUE;
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
  if (!wild_num)
    return 0;
  Item_arena *arena= thd->current_arena, backup;

  /*
    If we are in preparing prepared statement phase then we have change
    temporary mem_root to statement mem root to save changes of SELECT list
  */
  if (arena->is_stmt_prepare())
    thd->set_n_backup_item_arena(arena, &backup);

  reg2 Item *item;
  List_iterator<Item> it(fields);
  while ( wild_num && (item= it++))
  {    
    if (item->type() == Item::FIELD_ITEM && ((Item_field*) item)->field_name &&
	((Item_field*) item)->field_name[0] == '*' &&
	!((Item_field*) item)->field)
    {
      uint elem= fields.elements;
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
                             ((Item_field*) item)->table_name, &it))
      {
        if (arena->is_stmt_prepare())
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
  if (arena->is_stmt_prepare())
      thd->restore_backup_item_arena(arena, &backup);
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
  DBUG_ENTER("setup_fields");

  thd->set_query_id=set_query_id;
  thd->allow_sum_func= allow_sum_func;
  thd->where="field list";

  Item **ref= ref_pointer_array;
  while ((item= it++))
  {
    if (!item->fixed && item->fix_fields(thd, tables, it.ref()) ||
	(item= *(it.ref()))->check_cols(1))
      DBUG_RETURN(-1); /* purecov: inspected */
    if (ref)
      *(ref++)= item;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM &&
	sum_func_list)
      item->split_sum_func(thd, ref_pointer_array, *sum_func_list);
    thd->used_tables|=item->used_tables();
  }
  DBUG_RETURN(test(thd->net.report_error));
}


/*
  prepare tables

  SYNOPSIS
    setup_tables()
    tables - tables list

   RETURN
     0	ok;  In this case *map will includes the choosed index
     1	error

   NOTE
     Remap table numbers if INSERT ... SELECT
     Check also that the 'used keys' and 'ignored keys' exists and set up the
     table structure accordingly

     This has to be called for all tables that are used by items, as otherwise
     table->map is not set and all Item_field will be regarded as const items.
*/

bool setup_tables(TABLE_LIST *tables)
{
  DBUG_ENTER("setup_tables");
  uint tablenr=0;
  for (TABLE_LIST *table_list=tables ; table_list ;
       table_list=table_list->next,tablenr++)
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
    if ((pos= find_type(&table->keynames, name->ptr(), name->length(), 1)) <=
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


/****************************************************************************
  This just drops in all fields instead of current '*' field
  Returns pointer to last inserted field if ok
****************************************************************************/

bool
insert_fields(THD *thd,TABLE_LIST *tables, const char *db_name,
	      const char *table_name, List_iterator<Item> *it)
{
  char name_buff[NAME_LEN+1];
  uint found;
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


  found=0;
  for (; tables ; tables=tables->next)
  {
    TABLE *table=tables->table;
    if (!table_name || (!my_strcasecmp(table_alias_charset, table_name,
				       tables->alias) &&
			(!db_name || !strcmp(tables->db,db_name))))
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* Ensure that we have access right to all columns */
      if (!(table->grant.privilege & SELECT_ACL) &&
	  check_grant_all_columns(thd,SELECT_ACL,table))
	DBUG_RETURN(-1);
#endif
      Field **ptr=table->field,*field;
      TABLE *natural_join_table= 0;

      thd->used_tables|=table->map;
      if (!table->outer_join &&
          tables->natural_join &&
          !tables->natural_join->table->outer_join)
        natural_join_table= tables->natural_join->table;

      while ((field = *ptr++))
      {
        uint not_used_field_index= NO_CACHED_FIELD_INDEX;
        /* Skip duplicate field names if NATURAL JOIN is used */
        if (!natural_join_table ||
            !find_field_in_table(thd, natural_join_table, field->field_name, 
                                 strlen(field->field_name), 0, 0,
                                 &not_used_field_index))
        {
          Item_field *item= new Item_field(thd, field);
          if (!found++)
            (void) it->replace(item);		// Replace '*'
          else
            it->after(item);
        }
	/*
	  Mark if field used before in this select.
	  Used by 'insert' to verify if a field name is used twice
	*/
	if (field->query_id == thd->query_id)
	  thd->dupp_field=field;
	field->query_id=thd->query_id;
	table->used_keys.intersect(field->part_of_key);
      }
      /* All fields are used */
      table->used_fields=table->fields;
    }
  }
  if (!found)
  {
    if (!table_name)
      my_error(ER_NO_TABLES_USED,MYF(0));
    else
      my_error(ER_BAD_TABLE_ERROR,MYF(0),table_name);
  }
  DBUG_RETURN(!found);
}


/*
** Fix all conditions and outer join expressions
*/

int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds)
{
  table_map not_null_tables= 0;
  Item_arena *arena= thd->current_arena, backup;

  DBUG_ENTER("setup_conds");
  thd->set_query_id=1;
  
  thd->lex->current_select->cond_count= 0;
  if (*conds)
  {
    thd->where="where clause";
    if (!(*conds)->fixed && (*conds)->fix_fields(thd, tables, conds) ||
	(*conds)->check_cols(1))
      DBUG_RETURN(1);
    not_null_tables= (*conds)->not_null_tables();
  }


  /* Check if we are using outer joins */
  for (TABLE_LIST *table=tables ; table ; table=table->next)
  {
    if (table->on_expr)
    {
      /* Make a join an a expression */
      thd->where="on clause";
      
      if (!table->on_expr->fixed &&
	  table->on_expr->fix_fields(thd, tables, &table->on_expr) ||
	  table->on_expr->check_cols(1))
	DBUG_RETURN(1);
      thd->lex->current_select->cond_count++;

      /*
	If it's a normal join or a LEFT JOIN which can be optimized away
	add the ON/USING expression to the WHERE
      */
      if (!table->outer_join ||
	  ((table->table->map & not_null_tables) &&
	   !(specialflag & SPECIAL_NO_NEW_FUNC)))
      {
	table->outer_join= 0;
	if (arena->is_stmt_prepare())
	  thd->set_n_backup_item_arena(arena, &backup);
	*conds= and_conds(*conds, table->on_expr);
	table->on_expr=0;
	if (arena->is_stmt_prepare())
	  thd->restore_backup_item_arena(arena, &backup);
	if ((*conds) && !(*conds)->fixed &&
	    (*conds)->fix_fields(thd, tables, conds))
	  DBUG_RETURN(1);
      }
    }
    if (table->natural_join)
    {
      if (arena->is_stmt_prepare())
	thd->set_n_backup_item_arena(arena, &backup);
      /* Make a join of all fields with have the same name */
      TABLE *t1= table->table;
      TABLE *t2= table->natural_join->table;
      Item_cond_and *cond_and= new Item_cond_and();
      if (!cond_and)				// If not out of memory
	goto err;
      cond_and->top_level_item();

      Field **t1_field, *t2_field;
      for (t1_field= t1->field; (*t1_field); t1_field++)
      {
        const char *t1_field_name= (*t1_field)->field_name;
        uint not_used_field_index= NO_CACHED_FIELD_INDEX;

        if ((t2_field= find_field_in_table(thd, t2, t1_field_name,
                                           strlen(t1_field_name), 0, 0,
                                           &not_used_field_index)))
        {
          Item_func_eq *tmp=new Item_func_eq(new Item_field(*t1_field),
                                             new Item_field(t2_field));
          if (!tmp)
            goto err;
          /* Mark field used for table cache */
          (*t1_field)->query_id= t2_field->query_id= thd->query_id;
          cond_and->list.push_back(tmp);
          t1->used_keys.intersect((*t1_field)->part_of_key);
          t2->used_keys.intersect(t2_field->part_of_key);
        }
      }
      thd->lex->current_select->cond_count+= cond_and->list.elements;

      // to prevent natural join processing during PS re-execution
      table->natural_join= 0;

      if (cond_and->list.elements)
      {
        if (!table->outer_join)			// Not left join
        {
          *conds= and_conds(*conds, cond_and);
          // fix_fields() should be made with temporary memory pool
          if (arena->is_stmt_prepare())
            thd->restore_backup_item_arena(arena, &backup);
          if (*conds && !(*conds)->fixed)
          {
            if ((*conds)->fix_fields(thd, tables, conds))
              DBUG_RETURN(1);
          }
        }
        else
        {
          table->on_expr= and_conds(table->on_expr, cond_and);
          // fix_fields() should be made with temporary memory pool
          if (arena->is_stmt_prepare())
            thd->restore_backup_item_arena(arena, &backup);
          if (table->on_expr && !table->on_expr->fixed)
          {
            if (table->on_expr->fix_fields(thd, tables, &table->on_expr))
             DBUG_RETURN(1);
          }
        }
      }
    }
  }

  if (arena->is_stmt_prepare())
  {
    /*
      We are in prepared statement preparation code => we should store
      WHERE clause changing for next executions.

      We do this ON -> WHERE transformation only once per PS statement.
    */
    thd->lex->current_select->where= *conds;
  }
  DBUG_RETURN(test(thd->net.report_error));

err:
  if (arena->is_stmt_prepare())
      thd->restore_backup_item_arena(arena, &backup);
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
  Mark all entries with the table as deleted to force an reopen of the table

  The table will be closed (not stored in cache) by the current thread when
  close_thread_tables() is called.

  RETURN
    0  This thread now have exclusive access to this table and no other thread
       can access the table until close_thread_tables() is called.
    1  Table is in use by another thread
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
	in_use->killed=1;
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
