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


/* Basic functions neaded by many modules */

#include "mysql_priv.h"
#include "sql_acl.h"
#include <thr_alarm.h>
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <nisam.h>
#include <assert.h>
#ifdef	__WIN__
#include <io.h>
#endif

TABLE *unused_tables;				/* Used by mysql_test */
HASH open_cache;				/* Used by mysql_test */

static int open_unireg_entry(THD *thd,TABLE *entry,const char *db,
			     const char *name, const char *alias, bool locked);
static bool insert_fields(THD *thd,TABLE_LIST *tables, const char *table_name,
			  List_iterator<Item> *it);
static void free_cache_entry(TABLE *entry);
static void mysql_rm_tmp_tables(void);
static key_map get_key_map_from_key_list(TABLE *table,
					 List<String> *index_list);


static byte *cache_key(const byte *record,uint *length,
		       my_bool not_used __attribute__((unused)))
{
  TABLE *entry=(TABLE*) record;
  *length=entry->key_length;
  return (byte*) entry->table_cache_key;
}

void table_cache_init(void)
{
  VOID(hash_init(&open_cache,table_cache_size+16,0,0,cache_key,
		 (void (*)(void*)) free_cache_entry,0));
  mysql_rm_tmp_tables();
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

OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *wild)
{
  int result = 0;
  uint col_access=thd->col_access;
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

    if ((!entry->real_name))
      continue;					// Shouldn't happen
    if (wild)
    {
      strxmov(name,entry->table_cache_key,".",entry->real_name,NullS);
      if (wild_compare(name,wild))
	continue;
    }

    /* Check if user has SELECT privilege for any column in the table */
    table_list.db= (char*) entry->table_cache_key;
    table_list.real_name= entry->real_name;
    table_list.grant.privilege=0;
    if (check_table_access(thd,SELECT_ACL | EXTRA_ACL,&table_list))
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
	  sql_alloc(sizeof(OPEN_TABLE_LIST)+entry->key_length)))
    {
      open_list=0;				// Out of memory
      break;
    }
    (*start_list)->table=(strmov((*start_list)->db=(char*) ((*start_list)+1),
				 entry->table_cache_key)+1,
			  entry->real_name);
    (*start_list)->in_use= entry->in_use ? 1 : 0;
    (*start_list)->locked= entry->locked_by_name ? 1 : 0;
    start_list= &(*start_list)->next;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(open_list);
}


/******************************************************************************
** Send name and type of result to client.
** Sum fields has table name empty and field_name.
** flag is a bit mask with the following functions:
**   1 send number of rows
**   2 send default values
**   4 Don't convert field names
******************************************************************************/

bool
send_fields(THD *thd,List<Item> &list,uint flag)
{
  List_iterator<Item> it(list);
  Item *item;
  char buff[80];
  CONVERT *convert= (flag & 4) ? (CONVERT*) 0 : thd->convert_set;

  String tmp((char*) buff,sizeof(buff)),*res,*packet= &thd->packet;

  if (thd->fatal_error)		// We have got an error
    goto err;

  if (flag & 1)
  {				// Packet with number of elements
    char *pos=net_store_length(buff,(uint) list.elements);
    (void) my_net_write(&thd->net, buff,(uint) (pos-buff));
  }
  while ((item=it++))
  {
    char *pos;
    Send_field field;
    item->make_field(&field);
    packet->length(0);

    if (convert)
    {
      if (convert->store(packet,field.table_name,
			 (uint) strlen(field.table_name)) ||
	  convert->store(packet,field.col_name,
			 (uint) strlen(field.col_name)) ||
	  packet->realloc(packet->length()+10))
	goto err;
    }
    else if (net_store_data(packet,field.table_name) ||
	     net_store_data(packet,field.col_name) ||
	     packet->realloc(packet->length()+10))
      goto err; /* purecov: inspected */
    pos= (char*) packet->ptr()+packet->length();

    if (!(thd->client_capabilities & CLIENT_LONG_FLAG))
    {
      packet->length(packet->length()+9);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=2; pos[7]=(char) field.flags; pos[8]= (char) field.decimals;
    }
    else
    {
      packet->length(packet->length()+10);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=3; int2store(pos+7,field.flags); pos[9]= (char) field.decimals;
    }
    if (flag & 2)
    {						// Send default value
      if (!(res=item->val_str(&tmp)))
      {
	if (net_store_null(packet))
	  goto err;
      }
      else if (net_store_data(packet,res->ptr(),res->length()))
	goto err;
    }
    if (my_net_write(&thd->net, (char*) packet->ptr(),packet->length()))
      break;					/* purecov: inspected */
  }
  send_eof(&thd->net);
  return 0;
 err:
  send_error(&thd->net,ER_OUT_OF_RESOURCES);	/* purecov: inspected */
  return 1;					/* purecov: inspected */
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


static void free_cache_entry(TABLE *table)
{
  DBUG_ENTER("free_cache_entry");

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


void free_io_cache(TABLE *table)
{
  if (table->io_cache)
  {
    close_cached_file(table->io_cache);
    my_free((gptr) table->io_cache,MYF(0));
    table->io_cache=0;
  }
  if (table->record_pointers)
  {
    my_free((gptr) table->record_pointers,MYF(0));
    table->record_pointers=0;
  }
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
      if (remove_table_from_cache(thd, table->db, table->name, 1))
	found=1;
    }
    if (!found)
      if_wait_for_refresh=0;			// Nothing to wait for
  }
  if (if_wait_for_refresh)
  {
    /*
      If there is any table that has a lower refresh_version, wait until
      this is closed (or this thread is killed) before returning
    */
    if (!tables)
      kill_delayed_threads();
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= &LOCK_open;
    thd->mysys_var->current_cond= &COND_refresh;
    thd->proc_info="Flushing tables";
    pthread_mutex_unlock(&thd->mysys_var->mutex);

    close_old_data_files(thd,thd->open_tables,1,1);
    bool found=1;
    /* Wait until all threads has closed all the tables we had locked */
    DBUG_PRINT("info", ("Waiting for others threads to close their open tables"));
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


/* Put all tables used by thread in free list */

void close_thread_tables(THD *thd, bool locked)
{
  DBUG_ENTER("close_thread_tables");

  if (thd->locked_tables)
    DBUG_VOID_RETURN;				// LOCK TABLES in use

  TABLE *table,*next;
  bool found_old_table=0;

  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock); thd->lock=0;
  }
  /* VOID(pthread_sigmask(SIG_SETMASK,&thd->block_signals,NULL)); */
  if (!locked)
    VOID(pthread_mutex_lock(&LOCK_open));

  DBUG_PRINT("info", ("thd->open_tables=%p", thd->open_tables));

  for (table=thd->open_tables ; table ; table=next)
  {
    next=table->next;
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
	table->file->extra(HA_EXTRA_RESET);
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
  }
  thd->open_tables=0;
  /* Free tables to hold down open files */
  while (open_cache.records > table_cache_size && unused_tables)
    VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */
  check_unused();
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    VOID(pthread_cond_broadcast(&COND_refresh));
  }
  if (!locked)
    VOID(pthread_mutex_unlock(&LOCK_open));
  /*  VOID(pthread_sigmask(SIG_SETMASK,&thd->signals,NULL)); */
  DBUG_VOID_RETURN;
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
  uint init_query_buf_size = 11, query_buf_size; // "drop table "
  char* query, *p;
  LINT_INIT(p);
  query_buf_size = init_query_buf_size;

  for (table=thd->temporary_tables ; table ; table=table->next)
  {
    query_buf_size += table->key_length;

  }

  if(query_buf_size == init_query_buf_size)
    return; // no tables to close

  if((query = alloc_root(&thd->mem_root, query_buf_size)))
    {
      memcpy(query, "drop table ", init_query_buf_size);
      p = query + init_query_buf_size;
    }

  for (table=thd->temporary_tables ; table ; table=next)
  {
    if(query) // we might be out of memory, but this is not fatal
      {
	p = strxmov(p,table->table_cache_key,".",
		    table->table_name,",", NullS);
	// here we assume table_cache_key always starts
	// with \0 terminated db name
      }
    next=table->next;
    close_temporary(table);
  }
  if (query && mysql_bin_log.is_open())
  {
    uint save_query_len = thd->query_length;
    *--p = 0;
    thd->query_length = (uint)(p-query);
    Query_log_event qinfo(thd, query);
    mysql_bin_log.write(&qinfo);
    thd->query_length = save_query_len;
  }
  thd->temporary_tables=0;
}


TABLE **find_temporary_table(THD *thd, const char *db, const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length= (uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  TABLE *table,**prev;

  int4store(key+key_length,thd->slave_proxy_id);
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
  if(thd->slave_thread)
    --slave_open_temp_tables;
  return 0;
}

bool rename_temporary_table(THD* thd, TABLE *table, const char *db,
			    const char *table_name)
{
  char *key;
  if (!(key=(char*) alloc_root(&table->mem_root,
			       (uint) strlen(db)+
			       (uint) strlen(table_name)+6)))
    return 1;				/* purecov: inspected */
  table->key_length=(uint)
    (strmov((table->real_name=strmov(table->table_cache_key=key,
				     db)+1),
	    table_name) - table->table_cache_key)+1;
  int4store(key+table->key_length,thd->slave_proxy_id);
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
   LOCK_OPEN ; This lock will be unlocked on return.
*/

void wait_for_refresh(THD *thd)
{
  /* Wait until the current table is up to date */
  const char *proc_info;
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  proc_info=thd->proc_info;
  thd->proc_info="Waiting for table";
  pthread_mutex_unlock(&thd->mysys_var->mutex);
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
  if(!(table = table_list->table))
    DBUG_RETURN(0);

  char* db = thd->db ? thd->db : table_list->db;
  char* table_name = table_list->name;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  pthread_mutex_lock(&LOCK_open);
  if (open_unireg_entry(thd, table, db, table_name, table_name, 1) ||
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
  table->outer_join=table->null_row=table->maybe_null=0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query=table->used_keys= table->keys_in_use;
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
  int4store(key + key_length, thd->slave_proxy_id);

  for (table=thd->temporary_tables; table ; table=table->next)
  {
    if (table->key_length == key_length+4 &&
	!memcmp(table->table_cache_key,key,key_length+4))
    {
      if (table->query_id == thd->query_id)
      {
	my_printf_error(ER_CANT_REOPEN_TABLE,
			ER(ER_CANT_REOPEN_TABLE),MYF(0),table->table_name);
	DBUG_RETURN(0);
      }
      table->query_id=thd->query_id;
      goto reset;
    }
  }

  if (thd->locked_tables)
  {						// Using table locks
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->key_length == key_length &&
	  !memcmp(table->table_cache_key,key,key_length) &&
	  !my_strcasecmp(table->table_name,alias))
	goto reset;
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
      DBUG_RETURN(NULL);
    if (open_unireg_entry(thd, table,db,table_name,alias,1) ||
	!(table->table_cache_key=memdup_root(&table->mem_root,(char*) key,
					     key_length)))
    {
      MEM_ROOT* glob_alloc;
      LINT_INIT(glob_alloc);

      if (errno == ENOENT &&
	 (glob_alloc = my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC)))
	// Sasha: needed for replication
	// remember the name of the non-existent table
	// so we can try to download it from the master
      {
	int table_name_len = (uint) strlen(table_name);
	int db_len = (uint) strlen(db);
	thd->last_nx_db = alloc_root(glob_alloc,db_len + table_name_len + 2);
	if(thd->last_nx_db)
	{
	  thd->last_nx_table = thd->last_nx_db + db_len + 1;
	  memcpy(thd->last_nx_table, table_name, table_name_len + 1);
	  memcpy(thd->last_nx_db, db, db_len + 1);
	}
      }
      table->next=table->prev=table;
      free_cache_entry(table);
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    table->key_length=key_length;
    table->version=refresh_version;
    table->flush_version=flush_version;
    DBUG_PRINT("info", ("inserting table %p into the cache", table));
    VOID(hash_insert(&open_cache,(byte*) table));
  }

  table->in_use=thd;
  check_unused();
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
  table->outer_join=table->null_row=table->maybe_null=0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query=table->used_keys= table->keys_in_use;
  dbug_assert(table->key_read == 0);
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

  if (open_unireg_entry(current_thd,&tmp,db,table_name,table->table_name,
			locked))
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
  tmp.keys_in_use_for_query=tmp.used_keys=tmp.keys_in_use;

  /* Get state */
  tmp.key_length=	table->key_length;
  tmp.in_use=    	table->in_use;
  tmp.reginfo.lock_type=table->reginfo.lock_type;
  tmp.version=		refresh_version;
  tmp.tmp_table=	table->tmp_table;
  tmp.grant=		table->grant;

  /* Replace table in open list */
  tmp.next=table->next;
  tmp.prev=table->prev;

  if (table->file)
    VOID(closefrm(table));		// close file, free everything

  *table=tmp;
  table->file->change_table_ptr(table);

  for (field=table->field ; *field ; field++)
  {
    (*field)->table=table;
    (*field)->table_name=table->table_name;
  }
  for (key=0 ; key < table->keys ; key++)
    for (part=0 ; part < table->key_info[key].usable_key_parts ; part++)
      table->key_info[key].key_part[part].field->table=table;
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


/* lock table to force abort of any threads trying to use table */

void abort_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
      mysql_lock_abort(thd,table);
  }
}

/****************************************************************************
**	open_unireg_entry
**	Purpose : Load a table definition from file and open unireg table
**	Args	: entry with DB and table given
**	Returns : 0 if ok
**	Note that the extra argument for open is taken from thd->open_options
*/

static int open_unireg_entry(THD *thd, TABLE *entry, const char *db,
			     const char *name, const char *alias, bool locked)
{
  char path[FN_REFLEN];
  int error;
  DBUG_ENTER("open_unireg_entry");

  (void) sprintf(path,"%s/%s/%s",mysql_data_home,db,name);
  if (openfrm(path,alias,
	       (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
		       HA_TRY_READ_ONLY),
	       READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      thd->open_options, entry))
  {
    if (!entry->crashed)
      goto err;					// Can't repair the table

    TABLE_LIST table_list;
    table_list.db=(char*) db;
    table_list.name=(char*) name;
    table_list.next=0;
    if (!locked)
      pthread_mutex_lock(&LOCK_open);
    if ((error=lock_table_name(thd,&table_list)))
    {
      if (error < 0)
      {
	if (!locked)
	  pthread_mutex_unlock(&LOCK_open);
	goto err;
      }
      if (wait_for_locked_table_names(thd,&table_list))
      {
	unlock_table_name(thd,&table_list);
	if (!locked)
	  pthread_mutex_unlock(&LOCK_open);
	goto err;
      }
    }
    pthread_mutex_unlock(&LOCK_open);
    thd->net.last_error[0]=0;				// Clear error message
    thd->net.last_errno=0;
    error=0;
    if (openfrm(path,alias,
		(uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
			 HA_TRY_READ_ONLY),
		READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
		ha_open_options | HA_OPEN_FOR_REPAIR,
		entry) || ! entry->file ||
	(entry->file->is_crashed() && entry->file->check_and_repair(thd)))
    {
      /* Give right error message */
      thd->net.last_error[0]=0;
      thd->net.last_errno=0;
      my_error(ER_NOT_KEYFILE, MYF(0), name, my_errno);
      sql_print_error("Error: Couldn't repair table: %s.%s",db,name);
      if (entry->file)
	closefrm(entry);
      error=1;
    }
    else
    {
      thd->net.last_error[0]=0;			// Clear error message
      thd->net.last_errno=0;
    }
    if (locked)
      pthread_mutex_lock(&LOCK_open);      // Get back original lock
    unlock_table_name(thd,&table_list);
    if (error)
      goto err;
  }
  (void) entry->file->extra(HA_EXTRA_NO_READCHECK);	// Not needed in SQL
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

/*****************************************************************************
** open all tables in list
*****************************************************************************/

int open_tables(THD *thd,TABLE_LIST *start)
{
  TABLE_LIST *tables;
  bool refresh;
  int result=0;
  DBUG_ENTER("open_tables");

 restart:
  thd->proc_info="Opening tables";
  for (tables=start ; tables ; tables=tables->next)
  {
    if (!tables->table &&
	!(tables->table=open_table(thd,
				   tables->db ? tables->db : thd->db,
				   tables->real_name,
				   tables->name, &refresh)))
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
	if (found)
	  VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
	pthread_mutex_unlock(&LOCK_open);
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


TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type lock_type)
{
  TABLE *table;
  bool refresh;
  DBUG_ENTER("open_ltable");

#ifdef __WIN__
  /* Win32 can't drop a file that is open */
  if (lock_type == TL_WRITE_ALLOW_READ)
    lock_type= TL_WRITE;
#endif
  thd->proc_info="Opening table";
  while (!(table=open_table(thd,table_list->db ? table_list->db : thd->db,
			    table_list->real_name,table_list->name,
			    &refresh)) && refresh) ;
  if (table)
  {
    table_list->table=table;
    table->grant= table_list->grant;
    if (thd->locked_tables)
    {
      thd->proc_info=0;
      if ((int) lock_type >= (int) TL_WRITE_ALLOW_READ &&
	  (int) table->reginfo.lock_type < (int) TL_WRITE_ALLOW_READ)
      {
	my_printf_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,
			ER(ER_TABLE_NOT_LOCKED_FOR_WRITE),
			MYF(0),table_list->name);
	DBUG_RETURN(0);
      }
      thd->proc_info=0;
      DBUG_RETURN(table);
    }
    if ((table->reginfo.lock_type=lock_type) != TL_UNLOCK)
      if (!(thd->lock=mysql_lock_tables(thd,&table_list->table,1)))
	  DBUG_RETURN(0);
  }
  thd->proc_info=0;
  DBUG_RETURN(table);
}

/*
** Open all tables in list and locks them for read.
** The lock will automaticly be freed by the close_thread_tables
*/

int open_and_lock_tables(THD *thd,TABLE_LIST *tables)
{
  if (open_tables(thd,tables) || lock_tables(thd,tables))
    return -1;					/* purecov: inspected */
  return 0;
}

int lock_tables(THD *thd,TABLE_LIST *tables)
{
  if (tables && !thd->locked_tables)
  {
    uint count=0;
    TABLE_LIST *table;
    for (table = tables ; table ; table=table->next)
      count++;
    TABLE **start,**ptr;
    if (!(ptr=start=(TABLE**) sql_alloc(sizeof(TABLE*)*count)))
      return -1;
    for (table = tables ; table ; table=table->next)
      *(ptr++)= table->table;
    if (!(thd->lock=mysql_lock_tables(thd,start,count)))
      return -1;				/* purecov: inspected */
  }
  return 0;
}

/*
** Open a single table without table caching and don't set it in open_list
** Used by alter_table to open a temporary table and when creating
** a temporary table with CREATE TEMPORARY ...
*/

TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list)
{
  TABLE *tmp_table;
  DBUG_ENTER("open_temporary_table");

  // the extra size in my_malloc() is for table_cache_key
  //  4 bytes for master thread id if we are in the slave
  //  1 byte to terminate db
  //  1 byte to terminate table_name
  // total of 6 extra bytes in my_malloc in addition to table/db stuff
  if (!(tmp_table=(TABLE*) my_malloc(sizeof(*tmp_table)+(uint) strlen(db)+
				     (uint) strlen(table_name)+6,
				     MYF(MY_WME))))
    DBUG_RETURN(0);				/* purecov: inspected */

  if (openfrm(path, table_name,
	      (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
		      HA_TRY_READ_ONLY),
	      READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      ha_open_options,
	      tmp_table))
  {
    DBUG_RETURN(0);
  }

  tmp_table->file->extra(HA_EXTRA_NO_READCHECK); // Not needed in SQL
  tmp_table->reginfo.lock_type=TL_WRITE;	// Simulate locked
  tmp_table->tmp_table = 1;
  tmp_table->table_cache_key=(char*) (tmp_table+1);
  tmp_table->key_length= (uint) (strmov(strmov(tmp_table->table_cache_key,db)
					+1, table_name)
				 - tmp_table->table_cache_key)+1;
  int4store(tmp_table->table_cache_key + tmp_table->key_length,
	    thd->slave_proxy_id);
  tmp_table->key_length += 4;

  if (link_in_list)
  {
    tmp_table->next=thd->temporary_tables;
    thd->temporary_tables=tmp_table;
    if(thd->slave_thread)
      ++slave_open_temp_tables;
  }
  DBUG_RETURN(tmp_table);
}


bool rm_temporary_table(enum db_type base, char *path)
{
  bool error=0;
  fn_format(path, path,"",reg_ext,4);
  unpack_filename(path,path);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  *fn_ext(path)='\0';				// remove extension
  handler *file=get_new_handler((TABLE*) 0, base);
  if (file && file->delete_table(path))
    error=1;
  delete file;
  return error;
}


/*****************************************************************************
** find field in list or tables. if field is unqualifed and unique,
** return unique field
******************************************************************************/

#define WRONG_GRANT (Field*) -1

Field *find_field_in_table(THD *thd,TABLE *table,const char *name,uint length,
			   bool check_grants, bool allow_rowid)
{
  Field *field;
  if (table->name_hash.records)
  {
    if ((field=(Field*) hash_search(&table->name_hash,(byte*) name,
				    length)))
      goto found;
  }
  else
  {
    Field **ptr=table->field;
    while ((field = *ptr++))
    {
      if (!my_strcasecmp(field->field_name, name))
	goto found;
    }
  }
  if (allow_rowid && !my_strcasecmp(name,"_rowid") &&
      (field=table->rowid_field))
    goto found;
  return (Field*) 0;

 found:
  if (thd->set_query_id)
  {
    if (field->query_id != thd->query_id)
    {
      field->query_id=thd->query_id;
      table->used_fields++;
      if (field->part_of_key)
      {
	if (!(field->part_of_key & table->ref_primary_key))
	  table->used_keys&=field->part_of_key;
      }
      else
	table->used_keys=0;
    }
    else
      thd->dupp_field=field;
  }
  if (check_grants && !thd->master_access && check_grant_column(thd,table,name,length))
    return WRONG_GRANT;
  return field;
}


Field *
find_field_in_tables(THD *thd,Item_field *item,TABLE_LIST *tables)
{
  Field *found=0;
  const char *db=item->db_name;
  const char *table_name=item->table_name;
  const char *name=item->field_name;
  uint length=(uint) strlen(name);

  if (table_name)
  {						/* Qualified field */
    bool found_table=0;
    for (; tables ; tables=tables->next)
    {
      if (!strcmp(tables->name,table_name) &&
	  (!db ||
	   (tables->db && !strcmp(db,tables->db)) ||
	   (!tables->db && !strcmp(db,thd->db))))
      {
	found_table=1;
	Field *find=find_field_in_table(thd,tables->table,name,length,
					grant_option && !thd->master_access,1);
	if (find)
	{
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
    if (!found_table)
    {
      char buff[NAME_LEN*2+1];
      if (db)
      {
	strxnmov(buff,sizeof(buff)-1,db,".",table_name,NullS);
	table_name=buff;
      }
      my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),table_name,
		      thd->where);
    }
    else
      my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
		      item->full_name(),thd->where);
    return (Field*) 0;
  }
  bool allow_rowid= tables && !tables->next;	// Only one table
  for (; tables ; tables=tables->next)
  {
    Field *field=find_field_in_table(thd,tables->table,name,length,
				     grant_option && !thd->master_access, allow_rowid);
    if (field)
    {
      if (field == WRONG_GRANT)
	return (Field*) 0;
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
  my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),
		  MYF(0),item->full_name(),thd->where);
  return (Field*) 0;
}

Item **
find_item_in_list(Item *find,List<Item> &items)
{
  List_iterator<Item> li(items);
  Item **found=0,*item;
  const char *field_name=0;
  const char *table_name=0;
  if (find->type() == Item::FIELD_ITEM	|| find->type() == Item::REF_ITEM)
  {
    field_name= ((Item_ident*) find)->field_name;
    table_name= ((Item_ident*) find)->table_name;
  }

  while ((item=li++))
  {
    if (field_name && item->type() == Item::FIELD_ITEM)
    {
      if (!my_strcasecmp(((Item_field*) item)->name,field_name))
      {
	if (!table_name)
	{
	  if (found)
	  {
	    if ((*found)->eq(item))
	      continue;				// Same field twice (Access?)
	    if (current_thd->where)
	      my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			      find->full_name(), current_thd->where);
	    return (Item**) 0;
	  }
	  found=li.ref();
	}
	else if (!strcmp(((Item_field*) item)->table_name,table_name))
	{
	  found=li.ref();
	  break;
	}
      }
    }
    else if (!table_name && (item->eq(find) ||
			     find->name &&
			     !my_strcasecmp(item->name,find->name)))
    {
      found=li.ref();
      break;
    }
  }
  if (!found && current_thd->where)
    my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
		    find->full_name(),current_thd->where);
  return found;
}

/****************************************************************************
** Check that all given fields exists and fill struct with current data
****************************************************************************/

int setup_fields(THD *thd, TABLE_LIST *tables, List<Item> &fields,
		 bool set_query_id, List<Item> *sum_func_list)
{
  reg2 Item *item;
  List_iterator<Item> it(fields);
  DBUG_ENTER("setup_fields");

  thd->set_query_id=set_query_id;
  thd->allow_sum_func= test(sum_func_list);
  thd->where="field list";

  while ((item=it++))
  {
    if (item->type() == Item::FIELD_ITEM &&
	((Item_field*) item)->field_name[0] == '*')
    {
      if (insert_fields(thd,tables,((Item_field*) item)->table_name,&it))
	DBUG_RETURN(-1); /* purecov: inspected */
    }
    else
    {
      if (item->fix_fields(thd,tables))
	DBUG_RETURN(-1); /* purecov: inspected */
      if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
	item->split_sum_func(*sum_func_list);
      thd->used_tables|=item->used_tables();
    }
  }
  DBUG_RETURN(test(thd->fatal_error));
}


/*
  Remap table numbers if INSERT ... SELECT
  Check also that the 'used keys' and 'ignored keys' exists and set up the
  table structure accordingly
*/

bool setup_tables(TABLE_LIST *tables)
{
  DBUG_ENTER("setup_tables");
  uint tablenr=0;
  for (TABLE_LIST *table=tables ; table ; table=table->next,tablenr++)
  {
    table->table->tablenr=tablenr;
    table->table->map= (table_map) 1 << tablenr;
    if ((table->table->outer_join=table->outer_join))
      table->table->maybe_null=1;		// LEFT OUTER JOIN ...
    if (table->use_index)
    {
      key_map map= get_key_map_from_key_list(table->table,
					     table->use_index);
      if (map == ~(key_map) 0)
	DBUG_RETURN(1);
      table->table->keys_in_use_for_query=map;
    }
    if (table->ignore_index)
    {
      key_map map= get_key_map_from_key_list(table->table,
					     table->ignore_index);
      if (map == ~(key_map) 0)
	DBUG_RETURN(1);
      table->table->keys_in_use_for_query &= ~map;
    }
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0),MAX_TABLES);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static key_map get_key_map_from_key_list(TABLE *table, 
					 List<String> *index_list)
{
  key_map map=0;
  List_iterator<String> it(*index_list);
  String *name;
  uint pos;
  while ((name=it++))
  {
    if ((pos=find_type(name->c_ptr(), &table->keynames, 1+2)) <= 0)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), name->c_ptr(),
	       table->real_name);
      return (~ (key_map) 0);
    }
    map|= ((key_map) 1) << (pos-1);
  }
  return map;
}

/****************************************************************************
**	This just drops in all fields instead of current '*' field
**	Returns pointer to last inserted field if ok
****************************************************************************/

static bool
insert_fields(THD *thd,TABLE_LIST *tables, const char *table_name,
	      List_iterator<Item> *it)
{
  uint found;
  DBUG_ENTER("insert_fields");

  found=0;
  for (; tables ; tables=tables->next)
  {
    TABLE *table=tables->table;
    if (grant_option && !thd->master_access &&
	check_grant_all_columns(thd,SELECT_ACL,table) )
      DBUG_RETURN(-1);
    if (!table_name || !strcmp(table_name,tables->name))
    {
      Field **ptr=table->field,*field;
      thd->used_tables|=table->map;
      while ((field = *ptr++))
      {
	Item_field *item= new Item_field(field);
	if (!found++)
	  (void) it->replace(item);
	else
	  it->after(item);
	if (field->query_id == thd->query_id)
	  thd->dupp_field=field;
	field->query_id=thd->query_id;

	if (field->part_of_key)
	{
	  if (!(field->part_of_key & table->ref_primary_key))
	    table->used_keys&=field->part_of_key;
	}
	else
	  table->used_keys=0;
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
  DBUG_ENTER("setup_conds");
  thd->set_query_id=1;
  thd->cond_count=0;
  thd->allow_sum_func=0;
  if (*conds)
  {
    thd->where="where clause";
    if ((*conds)->fix_fields(thd,tables))
      DBUG_RETURN(1);
  }

  /* Check if we are using outer joins */
  for (TABLE_LIST *table=tables ; table ; table=table->next)
  {
    if (table->on_expr)
    {
      /* Make a join an a expression */
      thd->where="on clause";
      if (table->on_expr->fix_fields(thd,tables))
	DBUG_RETURN(1);
      thd->cond_count++;

      /* If it's a normal join, add the ON/USING expression to the WHERE */
      if (!table->outer_join)
      {
	if (!(*conds=and_conds(*conds, table->on_expr)))
	  DBUG_RETURN(1);
	table->on_expr=0;
      }
    }
    if (table->natural_join)
    {
      /* Make a join of all fields with have the same name */
      TABLE *t1=table->table;
      TABLE *t2=table->natural_join->table;
      Item_cond_and *cond_and=new Item_cond_and();
      if (!cond_and)				// If not out of memory
	DBUG_RETURN(1);

      uint i,j;
      for (i=0 ; i < t1->fields ; i++)
      {
	// TODO: This could be optimized to use hashed names if t2 had a hash
	for (j=0 ; j < t2->fields ; j++)
	{
	  key_map tmp_map;
	  if (!my_strcasecmp(t1->field[i]->field_name,
			     t2->field[j]->field_name))
	  {
	    Item_func_eq *tmp=new Item_func_eq(new Item_field(t1->field[i]),
					       new Item_field(t2->field[j]));
	    if (!tmp)
	      DBUG_RETURN(1);
	    tmp->fix_length_and_dec();	// Update cmp_type
	    tmp->const_item_cache=0;
	    /* Mark field used for table cache */
	    t1->field[i]->query_id=t2->field[j]->query_id=thd->query_id;
	    cond_and->list.push_back(tmp);
	    if ((tmp_map=t1->field[i]->part_of_key))
	    {
	      if (!(tmp_map & t1->ref_primary_key))
		t1->used_keys&=tmp_map;
	    }
	    else
	      t1->used_keys=0;
	    if ((tmp_map=t2->field[j]->part_of_key))
	    {
	      if (!(tmp_map & t2->ref_primary_key))
		t2->used_keys&=tmp_map;
	    }
	    else
	      t2->used_keys=0;
	    break;
	  }
	}
      }
      cond_and->used_tables_cache= t1->map | t2->map;
      thd->cond_count+=cond_and->list.elements;
      if (!table->outer_join)			// Not left join
      {
	if (!(*conds=and_conds(*conds, cond_and)))
	  DBUG_RETURN(1);
      }
      else
	table->on_expr=and_conds(table->on_expr,cond_and);
    }
  }
  DBUG_RETURN(test(thd->fatal_error));
}


/******************************************************************************
** Fill a record with data (for INSERT or UPDATE)
** Returns : 1 if some field has wrong type
******************************************************************************/

int
fill_record(List<Item> &fields,List<Item> &values)
{
  List_iterator<Item> f(fields),v(values);
  Item *value;
  Item_field *field;
  DBUG_ENTER("fill_record");

  while ((field=(Item_field*) f++))
  {
    value=v++;
    if (value->save_in_field(field->field))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int
fill_record(Field **ptr,List<Item> &values)
{
  List_iterator<Item> v(values);
  Item *value;
  DBUG_ENTER("fill_record");

  Field *field;
  while ((field = *ptr++))
  {
    value=v++;
    if (value->save_in_field(field))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static void mysql_rm_tmp_tables(void)
{
  uint idx;
  char	filePath[FN_REFLEN];
  MY_DIR *dirp;
  FILEINFO *file;
  DBUG_ENTER("mysql_rm_tmp_tables");

  /* See if the directory exists */
  if (!(dirp = my_dir(mysql_tmpdir,MYF(MY_WME | MY_DONT_SORT))))
    DBUG_VOID_RETURN;				/* purecov: inspected */

  /*
  ** Remove all SQLxxx tables from directory
  */

  for (idx=2 ; idx < (uint) dirp->number_off_files ; idx++)
  {
    file=dirp->dir_entry+idx;
    if (!bcmp(file->name,tmp_file_prefix,tmp_file_prefix_length))
    {
      sprintf(filePath,"%s%s",mysql_tmpdir,file->name); /* purecov: inspected */
      VOID(my_delete(filePath,MYF(MY_WME)));	/* purecov: inspected */
    }
  }
  my_dirend(dirp);
  DBUG_VOID_RETURN;
}


/*
** CREATE INDEX and DROP INDEX are implemented by calling ALTER TABLE with
** the proper arguments.  This isn't very fast but it should work for most
** cases.
** One should normally create all indexes with CREATE TABLE or ALTER TABLE.
*/

int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys)
{
  List<create_field> fields;
  List<Alter_drop> drop;
  List<Alter_column> alter;
  HA_CREATE_INFO create_info;
  DBUG_ENTER("mysql_create_index");
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				&create_info, table_list,
				fields, keys, drop, alter, (ORDER*)0, FALSE, DUP_ERROR));
}


int mysql_drop_index(THD *thd, TABLE_LIST *table_list, List<Alter_drop> &drop)
{
  List<create_field> fields;
  List<Key> keys;
  List<Alter_column> alter;
  HA_CREATE_INFO create_info;
  DBUG_ENTER("mysql_drop_index");
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				&create_info, table_list,
				fields, keys, drop, alter, (ORDER*)0, FALSE, DUP_ERROR));
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
      relink_unused(table);
    else if (in_use != thd)
    {
      in_use->some_tables_deleted=1;
      if (table->db_stat)
	result=1;
      /* Kill delayed insert threads */
      if (in_use->system_thread && ! in_use->killed)
      {
	in_use->killed=1;
	pthread_mutex_lock(&in_use->mysys_var->mutex);
	if (in_use->mysys_var->current_mutex)
	{
	  pthread_mutex_lock(in_use->mysys_var->current_mutex);
	  pthread_cond_broadcast(in_use->mysys_var->current_cond);
	  pthread_mutex_unlock(in_use->mysys_var->current_mutex);
	}
	pthread_mutex_unlock(&in_use->mysys_var->mutex);
      }
    }
    else
      result= result || return_if_owned_by_thd;
  }
  while (unused_tables && !unused_tables->version)
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
  DBUG_RETURN(result);
}

int setup_ftfuncs(THD *thd,TABLE_LIST *tables, List<Item_func_match> &ftfuncs)
{
  List_iterator<Item_func_match> li(ftfuncs), li2(ftfuncs);
  Item_func_match *ftf, *ftf2;

  while ((ftf=li++))
  {
    if (ftf->fix_index())
      return 1;
    li2.rewind();
    while ((ftf2=li2++) != ftf)
    {
      if (ftf->eq(ftf2) && !ftf2->master)
        ftf2->master=ftf;
    }
  }

  return 0;
}
