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


/* locking functions for mysql */
/*
  Because of the new concurrent inserts, we must first get external locks
  before getting internal locks.  If we do it in the other order, the status
  information is not up to date when called from the lock handler.

TODO:
  Change to use my_malloc() ONLY when using LOCK TABLES command or when
  we are forced to use mysql_lock_merge.
*/

#include "mysql_priv.h"
#include <hash.h>

extern HASH open_cache;

static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table,uint count,
				 bool unlock, TABLE **write_locked);
static int lock_external(TABLE **table,uint count);
static int unlock_external(THD *thd, TABLE **table,uint count);
static void print_lock_error(int error);


MYSQL_LOCK *mysql_lock_tables(THD *thd,TABLE **tables,uint count)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  DBUG_ENTER("mysql_lock_tables");

  for (;;)
  {
    if (!(sql_lock = get_lock_data(thd,tables,count, 0,&write_lock_used)))
      break;

    if (global_read_lock && write_lock_used)
    {
      /*
	Someone has issued LOCK ALL TABLES FOR READ and we want a write lock
	Wait until the lock is gone
      */
      if (wait_if_global_read_lock(thd, 1))
      {
	my_free((gptr) sql_lock,MYF(0));
	sql_lock=0;
	break;
      }	
      if (thd->version != refresh_version)
      {
	my_free((gptr) sql_lock,MYF(0));
	goto retry;
      }
    }

    thd->proc_info="System lock";
    if (lock_external(tables,count))
    {
      my_free((gptr) sql_lock,MYF(0));
      sql_lock=0;
      thd->proc_info=0;
      break;
    }
    thd->proc_info=0;
    thd->locked=1;
    if (thr_multi_lock(sql_lock->locks,sql_lock->lock_count))
    {
      thd->some_tables_deleted=1;		// Try again
      sql_lock->lock_count=0;			// Locks are alread freed
    }
    else if (!thd->some_tables_deleted)
    {
      thd->locked=0;
      break;
    }

    /* some table was altered or deleted. reopen tables marked deleted */
    mysql_unlock_tables(thd,sql_lock);
    thd->locked=0;
retry:
    sql_lock=0;
    if (wait_for_tables(thd))
      break;					// Couldn't open tables
  }
  if (thd->killed)
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));
    if (sql_lock)
    {
      mysql_unlock_tables(thd,sql_lock);
      sql_lock=0;
    }
  }
  thd->lock_time();
  DBUG_RETURN (sql_lock);
}


static int lock_external(TABLE **tables,uint count)
{
  reg1 uint i;
  int lock_type,error;
  THD *thd=current_thd;
  DBUG_ENTER("lock_external");

  for (i=1 ; i <= count ; i++, tables++)
  {
    lock_type=F_WRLCK;				/* Lock exclusive */
    if ((*tables)->db_stat & HA_READ_ONLY ||
	((*tables)->reginfo.lock_type >= TL_READ &&
	 (*tables)->reginfo.lock_type <= TL_READ_NO_INSERT))
      lock_type=F_RDLCK;

    if ((error=(*tables)->file->external_lock(thd,lock_type)))
    {
      for ( ; i-- ; tables--)
      {
	(*tables)->file->external_lock(thd, F_UNLCK);
	(*tables)->current_lock=F_UNLCK;
      }
      print_lock_error(error);
      DBUG_RETURN(error);
    }
    else
    {
      (*tables)->db_stat &= ~ HA_BLOCK_LOCK;
      (*tables)->current_lock= lock_type;
    }
  }
  DBUG_RETURN(0);
}


void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  DBUG_ENTER("mysql_unlock_tables");
  thr_multi_unlock(sql_lock->locks,sql_lock->lock_count);
  VOID(unlock_external(thd,sql_lock->table,sql_lock->table_count));
  my_free((gptr) sql_lock,MYF(0));
  DBUG_VOID_RETURN;
}

/*
  Unlock some of the tables locked by mysql_lock_tables
  This will work even if get_lock_data fails (next unlock will free all)
  */

void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  if ((sql_lock = get_lock_data(thd, table, count, 1, &write_lock_used)))
    mysql_unlock_tables(thd, sql_lock);
}


/*
** unlock all tables locked for read.
*/

void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  uint i,found;
  DBUG_ENTER("mysql_unlock_read_tables");

  /* Move all write locks first */
  THR_LOCK_DATA **lock=sql_lock->locks;
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->locks[i]->type >= TL_WRITE_ALLOW_READ)
    {
      swap(THR_LOCK_DATA *,*lock,sql_lock->locks[i]);
      lock++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    thr_multi_unlock(lock,i-found);
    sql_lock->lock_count-=found;
  }

  /* Then to the same for the external locks */
  /* Move all write locked tables first */
  TABLE **table=sql_lock->table;
  for (i=found=0 ; i < sql_lock->table_count ; i++)
  {
    if ((uint) sql_lock->table[i]->reginfo.lock_type >= TL_WRITE_ALLOW_READ)
    {
      swap(TABLE *,*table,sql_lock->table[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    VOID(unlock_external(thd,table,i-found));
    sql_lock->table_count-=found;
  }
  DBUG_VOID_RETURN;
}



void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table)
{
  mysql_unlock_some_tables(thd, &table,1);
  if (locked)
  {
    reg1 uint i;
    for (i=0; i < locked->table_count; i++)
    {
      if (locked->table[i] == table)
      {
	locked->table_count--;
	bmove((char*) (locked->table+i),
	      (char*) (locked->table+i+1),
	      (locked->table_count-i)* sizeof(TABLE*));
	break;
      }
    }
    THR_LOCK_DATA **prev=locked->locks;
    for (i=0 ; i < locked->lock_count ; i++)
    {
      if (locked->locks[i]->type != TL_UNLOCK)
	*prev++ = locked->locks[i];
    }
    locked->lock_count=(uint) (prev - locked->locks);
  }
}

/* abort all other threads waiting to get lock in table */

void mysql_lock_abort(THD *thd, TABLE *table)
{
  MYSQL_LOCK *locked;
  TABLE *write_lock_used;
  if ((locked = get_lock_data(thd,&table,1,1,&write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_abort_locks(locked->locks[i]->lock);
    my_free((gptr) locked,MYF(0));
  }
}


MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b)
{
  MYSQL_LOCK *sql_lock;
  DBUG_ENTER("mysql_lock_merge");
  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*(a->lock_count+b->lock_count)+
		  sizeof(TABLE*)*(a->table_count+b->table_count),MYF(MY_WME))))
    DBUG_RETURN(0);				// Fatal error
  sql_lock->lock_count=a->lock_count+b->lock_count;
  sql_lock->table_count=a->table_count+b->table_count;
  sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (sql_lock->locks+sql_lock->lock_count);
  memcpy(sql_lock->locks,a->locks,a->lock_count*sizeof(*a->locks));
  memcpy(sql_lock->locks+a->lock_count,b->locks,
	 b->lock_count*sizeof(*b->locks));
  memcpy(sql_lock->table,a->table,a->table_count*sizeof(*a->table));
  memcpy(sql_lock->table+a->table_count,b->table,
	 b->table_count*sizeof(*b->table));
  my_free((gptr) a,MYF(0));
  my_free((gptr) b,MYF(0));
  DBUG_RETURN(sql_lock);
}


	/* unlock a set of external */

static int unlock_external(THD *thd, TABLE **table,uint count)
{
  int error,error_code;
  DBUG_ENTER("unlock_external");

  error_code=0;
  for (; count-- ; table++)
  {
    if ((*table)->current_lock != F_UNLCK)
    {
      (*table)->current_lock = F_UNLCK;
      if ((error=(*table)->file->external_lock(thd, F_UNLCK)))
	error_code=error;
    }
  }
  if (error_code)
    print_lock_error(error_code);
  DBUG_RETURN(error_code);
}


/*
** Get lock structures from table structs and initialize locks
*/


static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table_ptr, uint count,
				 bool get_old_locks, TABLE **write_lock_used)
{
  uint i,tables,lock_count;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks;
  TABLE **to;

  *write_lock_used=0;
  for (i=tables=lock_count=0 ; i < count ; i++)
  {
    if (table_ptr[i]->tmp_table != TMP_TABLE)
    {
      tables+=table_ptr[i]->file->lock_count();
      lock_count++;
    }
  }

  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*tables+sizeof(table_ptr)*lock_count,
		  MYF(0))))
    return 0;
  locks=sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  to=sql_lock->table=(TABLE**) (locks+tables);
  sql_lock->table_count=lock_count;
  sql_lock->lock_count=tables;

  for (i=0 ; i < count ; i++)
  {
    TABLE *table;
    if ((table=table_ptr[i])->tmp_table == TMP_TABLE)
      continue;
    *to++=table;
    enum thr_lock_type lock_type= table->reginfo.lock_type;
    if (lock_type >= TL_WRITE_ALLOW_WRITE)
    {
      *write_lock_used=table;
      if (table->db_stat & HA_READ_ONLY)
      {
	my_error(ER_OPEN_AS_READONLY,MYF(0),table->table_name);
	my_free((gptr) sql_lock,MYF(0));
	return 0;
      }
    }
    locks=table->file->store_lock(thd, locks, get_old_locks ? TL_IGNORE :
				  lock_type);
  }
  return sql_lock;
}

/*****************************************************************************
**  Lock table based on the name.
**  This is used when we need total access to a closed, not open table
*****************************************************************************/

/*
  Lock and wait for the named lock.
  Returns 0 on ok
*/

int lock_and_wait_for_table_name(THD *thd, TABLE_LIST *table_list)
{
  int lock_retcode;
  int error= -1;
  DBUG_ENTER("lock_and_wait_for_table_name");

  if (wait_if_global_read_lock(thd,0))
    DBUG_RETURN(1);
  VOID(pthread_mutex_lock(&LOCK_open));
  if ((lock_retcode = lock_table_name(thd, table_list)) < 0)
    goto end;
  if (lock_retcode && wait_for_locked_table_names(thd, table_list))
  {
    unlock_table_name(thd, table_list);
    goto end;
  }
  error=0;

end:
  start_waiting_global_read_lock(thd);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(error);
}


/*
  Put a not open table with an old refresh version in the table cache.
  This will force any other threads that uses the table to release it
  as soon as possible.
  One must have a lock on LOCK_open !
  Return values:
   < 0 error
   == 0 table locked
   > 0  table locked, but someone is using it
*/

int lock_table_name(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  char  key[MAX_DBKEY_LENGTH];
  uint  key_length;
  DBUG_ENTER("lock_table_name");

  key_length=(uint) (strmov(strmov(key,table_list->db)+1,table_list->name)
		     -key)+ 1;

  /* Only insert the table if we haven't insert it already */
  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table ;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
    if (table->in_use == thd)
      DBUG_RETURN(0);

  /* Create a table entry with the right key and with an old refresh version */
  /* Note that we must use my_malloc() here as this is freed by the table
     cache */

  if (!(table= (TABLE*) my_malloc(sizeof(*table)+key_length,
				  MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(-1);
  memcpy((table->table_cache_key= (char*) (table+1)), key, key_length);
  table->key_length=key_length;
  table->in_use=thd;
  table->locked_by_name=1;
  table_list->table=table;

  if (hash_insert(&open_cache, (byte*) table))
  {
    my_free((gptr) table,MYF(0));
    DBUG_RETURN(-1);
  }
  if (remove_table_from_cache(thd, table_list->db, table_list->name))
    DBUG_RETURN(1);					// Table is in use
  DBUG_RETURN(0);
}

void unlock_table_name(THD *thd, TABLE_LIST *table_list)
{
  if (table_list->table)
  {
    hash_delete(&open_cache, (byte*) table_list->table);
    (void) pthread_cond_broadcast(&COND_refresh);
  }
}

static bool locked_named_table(THD *thd, TABLE_LIST *table_list)
{
  for ( ; table_list ; table_list=table_list->next)
  {
    if (table_list->table && table_is_used(table_list->table,0))
      return 1;
  }
  return 0;					// All tables are locked
}


bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool result=0;
  DBUG_ENTER("wait_for_locked_table_names");

  while (locked_named_table(thd,table_list))
  {
    if (thd->killed)
    {
      result=1;
      break;
    }
    wait_for_refresh(thd);
    pthread_mutex_lock(&LOCK_open);
  }
  DBUG_RETURN(result);
}

static void print_lock_error(int error)
{
  int textno;
  DBUG_ENTER("print_lock_error");

  switch (error) {
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  default:
    textno=ER_CANT_LOCK;
    break;
  }
  my_error(textno,MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG),error);
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Handling of global read locks

  The global locks are handled through the global variables:
  global_read_lock
  waiting_for_read_lock 
  protect_against_global_read_lock
****************************************************************************/

volatile uint global_read_lock=0;
static volatile uint protect_against_global_read_lock=0;
static volatile uint waiting_for_read_lock=0;

bool lock_global_read_lock(THD *thd)
{
  DBUG_ENTER("lock_global_read_lock");

  if (!thd->global_read_lock)
  {
    (void) pthread_mutex_lock(&LOCK_open);
    const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
					    "Waiting to get readlock");
    waiting_for_read_lock++;
    while (protect_against_global_read_lock && !thd->killed)
      pthread_cond_wait(&COND_refresh, &LOCK_open);
    waiting_for_read_lock--;
    if (thd->killed)
    {
      (void) pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(1);
    }
    thd->global_read_lock=1;
    global_read_lock++;
    (void) pthread_mutex_unlock(&LOCK_open);
  }
  DBUG_RETURN(0);
}

void unlock_global_read_lock(THD *thd)
{
  uint tmp;
  thd->global_read_lock=0;
  pthread_mutex_lock(&LOCK_open);
  tmp= --global_read_lock;
  pthread_mutex_unlock(&LOCK_open);
  /* Send the signal outside the mutex to avoid a context switch */
  if (!tmp)
    pthread_cond_broadcast(&COND_refresh);
}


bool wait_if_global_read_lock(THD *thd, bool abort_on_refresh)
{
  const char *old_message;
  bool result=0;
  DBUG_ENTER("wait_if_global_read_lock");

  (void) pthread_mutex_lock(&LOCK_open);
  if (global_read_lock)
  {
    if (thd->global_read_lock)		// This thread had the read locks
    {
      my_error(ER_CANT_UPDATE_WITH_READLOCK,MYF(0));
      DBUG_RETURN(1);
    }	
    old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
				"Waiting for release of readlock");
    while (global_read_lock && ! thd->killed &&
	   (!abort_on_refresh || thd->version == refresh_version))
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    if (thd->killed)
      result=1;
    thd->exit_cond(old_message);
  }
  if (!abort_on_refresh && !result)
    protect_against_global_read_lock++;
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(result);
}


void start_waiting_global_read_lock(THD *thd)
{
  bool tmp;
  (void) pthread_mutex_lock(&LOCK_open);
  tmp= (!--protect_against_global_read_lock && waiting_for_read_lock);
  (void) pthread_mutex_unlock(&LOCK_open);
  if (tmp)
    pthread_cond_broadcast(&COND_refresh);
}
