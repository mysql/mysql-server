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
      if (thd->global_read_lock)	// This thread had the read locks
      {
	my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,MYF(0),
		 write_lock_used->table_name);
	my_free((gptr) sql_lock,MYF(0));
	sql_lock=0;
	break;
      }	

      pthread_mutex_lock(&LOCK_open);
      pthread_mutex_lock(&thd->mysys_var->mutex);
      thd->mysys_var->current_mutex= &LOCK_open;
      thd->mysys_var->current_cond= &COND_refresh;
      thd->proc_info="Waiting for table";
      pthread_mutex_unlock(&thd->mysys_var->mutex);

      while (global_read_lock && ! thd->killed ||
	     thd->version != refresh_version)
      {
	(void) pthread_cond_wait(&COND_refresh,&LOCK_open);
      }
      pthread_mutex_unlock(&LOCK_open);
      pthread_mutex_lock(&thd->mysys_var->mutex);
      thd->mysys_var->current_mutex= 0;
      thd->mysys_var->current_cond= 0;
      thd->proc_info= 0;
      pthread_mutex_unlock(&thd->mysys_var->mutex);

      if (thd->version != refresh_version || thd->killed)
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
      my_error(ER_CANT_LOCK,MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG),error);
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
    my_error(ER_CANT_LOCK,MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG),error_code);
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
    if (!table_ptr[i]->tmp_table)
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
    if ((table=table_ptr[i])->tmp_table)
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
  key_length=(uint) (strmov(strmov(key,table_list->db)+1,table_list->name)-key)+
    1;

  /* Only insert the table if we haven't insert it already */
  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table ;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
    if (table->in_use == thd)
      return 0;

  /* Create a table entry with the right key and with an old refresh version */
  /* Note that we must use my_malloc() here as this is freed by the table
     cache */

  if (!(table= (TABLE*) my_malloc(sizeof(*table)+key_length,
				  MYF(MY_WME | MY_ZEROFILL))))
    return -1;
  memcpy((table->table_cache_key= (char*) (table+1)), key, key_length);
  table->key_length=key_length;
  table->in_use=thd;
  table_list->table=table;

  if (hash_insert(&open_cache, (byte*) table))
    return -1;
  if (remove_table_from_cache(thd, table_list->db, table_list->name))
    return 1;					// Table is in use
  return 0;
}

void unlock_table_name(THD *thd, TABLE_LIST *table_list)
{
  if (table_list->table)
    hash_delete(&open_cache, (byte*) table_list->table);
}

static bool locked_named_table(THD *thd, TABLE_LIST *table_list)
{
  for ( ; table_list ; table_list=table_list->next)
  {
    if (table_list->table && table_is_used(table_list->table))
      return 1;
  }
  return 0;					// All tables are locked
}


bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool result=0;

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
  return result;
}
