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


/*****************************************************************************
**
** This file implements classes defined in sql_class.h
** Especially the classes to handle a result from a select
**
*****************************************************************************/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#include <sys/stat.h>
#include <thr_alarm.h>
#ifdef	__WIN__
#include <io.h>
#endif
#include <mysys_err.h>
#include <assert.h>


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
/* Used templates */
template class List<Key>;
template class List_iterator<Key>;
template class List<key_part_spec>;
template class List_iterator<key_part_spec>;
template class List<Alter_drop>;
template class List_iterator<Alter_drop>;
template class List<Alter_column>;
template class List_iterator<Alter_column>;
#endif

/****************************************************************************
** User variables
****************************************************************************/

extern "C" byte *get_var_key(user_var_entry *entry, uint *length,
			     my_bool not_used __attribute__((unused)))
{
  *length=(uint) entry->name.length;
  return (byte*) entry->name.str;
}

extern "C" void free_user_var(user_var_entry *entry)
{
  char *pos= (char*) entry+ALIGN_SIZE(sizeof(*entry));
  if (entry->value && entry->value != pos)
    my_free(entry->value, MYF(0));
  my_free((char*) entry,MYF(0));
}


/****************************************************************************
** Thread specific functions
****************************************************************************/

THD::THD():user_time(0),global_read_lock(0),fatal_error(0),
           last_insert_id_used(0),insert_id_used(0),rand_used(0),
           in_lock_tables(0),bootstrap(0)
{
  host=user=priv_user=db=query=ip=0;
  host_or_ip= "connecting host";
  locked=killed=count_cuted_fields=some_tables_deleted=no_errors=password=
    query_start_used=safe_to_cache_query=0;
  db_length=query_length=col_access=0;
  query_error=0;
  next_insert_id=last_insert_id=0;
  open_tables=temporary_tables=handler_tables=0;
  hash_clear(&handler_tables_hash);
  current_tablenr=0;
  handler_items=0;
  tmp_table=0;
  lock=locked_tables=0;
  used_tables=0;
  cuted_fields=sent_row_count=0L;
  start_time=(time_t) 0;
  current_linfo =  0;
  slave_thread = 0;
  slave_proxy_id = 0;
  file_id = 0;
  cond_count=0;
  mysys_var=0;
#ifndef DBUG_OFF
  dbug_sentry=THD_SENTRY_MAGIC;
#endif  
  net.vio=0;
  net.last_error[0]=0;				// If error on boot
  ull=0;
  system_thread=cleanup_done=0;
  peer_port= 0;					// For SHOW PROCESSLIST
  transaction.changed_tables = 0;
#ifdef	__WIN__
  real_id = 0;
#endif
#ifdef SIGNAL_WITH_VIO_CLOSE
  active_vio = 0;
#endif  
  pthread_mutex_init(&LOCK_delete, MY_MUTEX_INIT_FAST);

  /* Variables with default values */
  proc_info="login";
  where="field list";
  server_id = ::server_id;
  slave_net = 0;
  command=COM_CONNECT;
  set_query_id=1;
  db_access=NO_ACCESS;
  version=refresh_version;			// For boot

  init();
  /* Initialize sub structures */
  bzero((char*) &mem_root,sizeof(mem_root));
  bzero((char*) &transaction.mem_root,sizeof(transaction.mem_root));
  user_connect=(USER_CONN *)0;
  hash_init(&user_vars, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var,0);
#ifdef USING_TRANSACTIONS
  bzero((char*) &transaction,sizeof(transaction));
  if (opt_using_transactions)
  {
    if (open_cached_file(&transaction.trans_log,
			 mysql_tmpdir, LOG_PREFIX, binlog_cache_size,
			 MYF(MY_WME)))
      killed=1;
    transaction.trans_log.end_of_file= max_binlog_cache_size;
  }
#endif

  /*
    We need good random number initialization for new thread
    Just coping global one will not work
  */
  {
    pthread_mutex_lock(&LOCK_thread_count);
    ulong tmp=(ulong) (my_rnd(&sql_rand) * 0xffffffff); /* make all bits random */
    pthread_mutex_unlock(&LOCK_thread_count);
    randominit(&rand, tmp + (ulong) &rand, tmp + (ulong) ::query_id);
  }
}


/*
  Init common variables that has to be reset on start and on change_user
*/

void THD::init(void)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  variables= global_system_variables;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  server_status= SERVER_STATUS_AUTOCOMMIT;
  options= thd_startup_options;
  sql_mode=(uint) opt_sql_mode;
  open_options=ha_open_options;
  update_lock_default= (variables.low_priority_updates ?
			TL_WRITE_LOW_PRIORITY :
			TL_WRITE);
  session_tx_isolation= (enum_tx_isolation) variables.tx_isolation;
}

/*
  Do what's needed when one invokes change user

  SYNOPSIS
    change_user()

  IMPLEMENTATION
    Reset all resources that are connection specific
*/


void THD::change_user(void)
{
  cleanup();
  cleanup_done=0;
  init();
  hash_init(&user_vars, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var,0);
}


/* Do operations that may take a long time */

void THD::cleanup(void)
{
  DBUG_ENTER("THD::cleanup");
  ha_rollback(this);
  if (locked_tables)
  {
    lock=locked_tables; locked_tables=0;
    close_thread_tables(this);
  }
  mysql_ha_flush(this, (TABLE_LIST*) 0,
                 MYSQL_HA_CLOSE_FINAL | MYSQL_HA_FLUSH_ALL);
  hash_free(&handler_tables_hash);
  close_temporary_tables(this);
  hash_free(&user_vars);
  if (global_read_lock)
    unlock_global_read_lock(this);
  if (ull)
  {
    pthread_mutex_lock(&LOCK_user_locks);
    item_user_lock_release(ull);
    pthread_mutex_unlock(&LOCK_user_locks);
    ull= 0;
  }
  cleanup_done=1;
  DBUG_VOID_RETURN;
}


THD::~THD()
{
  THD_CHECK_SENTRY(this);
  DBUG_ENTER("~THD()");
  /* Ensure that no one is using THD */
  pthread_mutex_lock(&LOCK_delete);
  pthread_mutex_unlock(&LOCK_delete);

  /* Close connection */
  if (net.vio)
  {
    vio_delete(net.vio);
    net_end(&net); 
  }
  if (!cleanup_done)
    cleanup();
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    close_cached_file(&transaction.trans_log);
    ha_close_connection(this);
  }
#endif

  DBUG_PRINT("info", ("freeing host"));
  if (host != localhost)			// If not pointer to constant
    safeFree(host);
  if (user != delayed_user)
    safeFree(user);
  safeFree(db);
  safeFree(ip);
  free_root(&mem_root,MYF(0));
  free_root(&transaction.mem_root,MYF(0));
  mysys_var=0;					// Safety (shouldn't be needed)
  pthread_mutex_destroy(&LOCK_delete);
#ifndef DBUG_OFF
  dbug_sentry = THD_SENTRY_GONE;
#endif  
  DBUG_VOID_RETURN;
}


void THD::awake(bool prepare_to_die)
{
  THD_CHECK_SENTRY(this);
  safe_mutex_assert_owner(&LOCK_delete); 

  if (prepare_to_die)
    killed = 1;
  thr_alarm_kill(real_id);
#ifdef SIGNAL_WITH_VIO_CLOSE
  close_active_vio();
#endif    
  if (mysys_var)
  {
    pthread_mutex_lock(&mysys_var->mutex);
    if (!system_thread)		// Don't abort locks
      mysys_var->abort=1;
    /*
      This broadcast could be up in the air if the victim thread
      exits the cond in the time between read and broadcast, but that is
      ok since all we want to do is to make the victim thread get out
      of waiting on current_cond.
      If we see a non-zero current_cond: it cannot be an old value (because
      then exit_cond() should have run and it can't because we have mutex); so
      it is the true value but maybe current_mutex is not yet non-zero (we're
      in the middle of enter_cond() and there is a "memory order
      inversion"). So we test the mutex too to not lock 0.
      Note that there is a small chance we fail to kill. If victim has locked
      current_mutex, and hasn't entered enter_cond(), then we don't know it's
      going to wait on cond. Then victim goes into its cond "forever" (until
      we issue a second KILL). True we have set its thd->killed but it may not
      see it immediately and so may have time to reach the cond_wait().
    */
    if (mysys_var->current_cond && mysys_var->current_mutex)
    {
      pthread_mutex_lock(mysys_var->current_mutex);
      pthread_cond_broadcast(mysys_var->current_cond);
      pthread_mutex_unlock(mysys_var->current_mutex);
    }
    pthread_mutex_unlock(&mysys_var->mutex);
  }
}

/*
  Remember the location of thread info, the structure needed for
  sql_alloc() and the structure for the net buffer
*/

bool THD::store_globals()
{
  if (my_pthread_setspecific_ptr(THR_THD,  this) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &mem_root) ||
      my_pthread_setspecific_ptr(THR_NET,  &net))
    return 1;
  mysys_var=my_thread_var;
  dbug_thread_id=my_thread_id();
  /*
    By default 'slave_proxy_id' is 'thread_id'. They may later become different
    if this is the slave SQL thread.
  */
  slave_proxy_id= thread_id;
  return 0;
}


/* routings to adding tables to list of changed in transaction tables */

inline static void list_include(CHANGED_TABLE_LIST** prev,
				CHANGED_TABLE_LIST* curr,
				CHANGED_TABLE_LIST* new_table)
{
  if (new_table)
  {
    *prev = new_table;
    (*prev)->next = curr;
  }
}

/* add table to list of changed in transaction tables */

void THD::add_changed_table(TABLE *table)
{
  DBUG_ENTER("THD::add_changed_table(table)");

  DBUG_ASSERT((options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
	      table->file->has_transactions());
  add_changed_table(table->table_cache_key, table->key_length);
  DBUG_VOID_RETURN;
}


void THD::add_changed_table(const char *key, long key_length)
{
  DBUG_ENTER("THD::add_changed_table(key)");
  CHANGED_TABLE_LIST **prev_changed = &transaction.changed_tables;
  CHANGED_TABLE_LIST *curr = transaction.changed_tables;

  for (; curr; prev_changed = &(curr->next), curr = curr->next)
  {
    int cmp =  (long)curr->key_length - (long)key_length;
    if (cmp < 0)
    {
      list_include(prev_changed, curr, changed_table_dup(key, key_length));
      DBUG_PRINT("info", 
		 ("key_length %u %u", key_length, (*prev_changed)->key_length));
      DBUG_VOID_RETURN;
    }
    else if (cmp == 0)
    {
      cmp = memcmp(curr->key, key, curr->key_length);
      if (cmp < 0)
      {
	list_include(prev_changed, curr, changed_table_dup(key, key_length));
	DBUG_PRINT("info", 
		   ("key_length %u %u", key_length,
		    (*prev_changed)->key_length));
	DBUG_VOID_RETURN;
      }
      else if (cmp == 0)
      {
	DBUG_PRINT("info", ("already in list"));
	DBUG_VOID_RETURN;
      }
    }
  }
  *prev_changed = changed_table_dup(key, key_length);
  DBUG_PRINT("info", ("key_length %u %u", key_length,
		      (*prev_changed)->key_length));
  DBUG_VOID_RETURN;
}


CHANGED_TABLE_LIST* THD::changed_table_dup(const char *key, long key_length)
{
  CHANGED_TABLE_LIST* new_table = 
    (CHANGED_TABLE_LIST*) trans_alloc(ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST))+
				      key_length + 1);
  if (!new_table)
  {
    my_error(EE_OUTOFMEMORY, MYF(ME_BELL),
	     ALIGN_SIZE(sizeof(TABLE_LIST)) + key_length + 1);
    killed= 1;
    return 0;
  }

  new_table->key = (char *) (((byte*)new_table)+
			     ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST)));
  new_table->next = 0;
  new_table->key_length = key_length;
  ::memcpy(new_table->key, key, key_length);
  return new_table;
}


#ifdef SIGNAL_WITH_VIO_CLOSE
void THD::close_active_vio()
{
  DBUG_ENTER("close_active_vio");
  safe_mutex_assert_owner(&LOCK_delete); 
  if (active_vio)
  {
    vio_close(active_vio);
    active_vio = 0;
  }
  DBUG_VOID_RETURN;
}
#endif

/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

select_result::select_result()
{
  thd=current_thd;
}

static String default_line_term("\n"),default_escaped("\\"),
	      default_field_term("\t");

sql_exchange::sql_exchange(char *name,bool flag)
  :file_name(name), opt_enclosed(0), dumpfile(flag), skip_lines(0)
{
  field_term= &default_field_term;
  enclosed=   line_start= &empty_string;
  line_term=  &default_line_term;
  escaped=    &default_escaped;
}

bool select_send::send_fields(List<Item> &list,uint flag)
{
  return ::send_fields(thd,list,flag);
}


/* Send data to client. Returns 0 if ok */

bool select_send::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  String *packet= &thd->packet;
  DBUG_ENTER("send_data");

#ifdef HAVE_INNOBASE_DB
  /* We may be passing the control from mysqld to the client: release the
     InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
     by thd */
  if (thd->transaction.all.innobase_tid)
    ha_release_temporary_latches(thd);
#endif

  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    DBUG_RETURN(0);
  }
  packet->length(0);				// Reset packet
  Item *item;
  while ((item=li++))
  {
    if (item->send(thd, packet))
    {
      packet->free();				// Free used
      my_error(ER_OUT_OF_RESOURCES,MYF(0));
      DBUG_RETURN(1);
    }
  }
  thd->sent_row_count++;
  if (!thd->net.vio)
    DBUG_RETURN(0);
  bool error=my_net_write(&thd->net,(char*) packet->ptr(),packet->length());
  DBUG_RETURN(error);
}

bool select_send::send_eof()
{
#ifdef HAVE_INNOBASE_DB
  /* We may be passing the control from mysqld to the client: release the
     InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
     by thd */
  if (thd->transaction.all.innobase_tid)
    ha_release_temporary_latches(thd);
#endif

  /* Unlock tables before sending packet to gain some speed */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock); thd->lock=0;
  }
  ::send_eof(&thd->net);
  return 0;
}


/***************************************************************************
** Export of select to textfile
***************************************************************************/


select_export::~select_export()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
  thd->sent_row_count=row_count;
}

int
select_export::prepare(List<Item> &list)
{
  uint option=4;
  bool blob_flag=0;
#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|=1;					// Force use of db directory
#endif
  if ((uint) strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
    strmake(path,exchange->file_name,FN_REFLEN-1);
  (void) fn_format(path,exchange->file_name, thd->db ? thd->db : "", "",
		   option);
  if (!access(path,F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR,MYF(0),exchange->file_name);
    return 1;
  }
  /* Create the file world readable */
  if ((file=my_create(path, 0666, O_WRONLY, MYF(MY_WME))) < 0)
    return 1;
#ifdef HAVE_FCHMOD
  (void) fchmod(file,0666);			// Because of umask()
#else
  (void) chmod(path,0666);
#endif
  if (init_io_cache(&cache,file,0L,WRITE_CACHE,0L,1,MYF(MY_WME)))
  {
    my_close(file,MYF(0));
    file= -1;
    return 1;
  }
  /* Check if there is any blobs in data */
  {
    List_iterator_fast<Item> li(list);
    Item *item;
    while ((item=li++))
    {
      if (item->max_length >= MAX_BLOB_WIDTH)
      {
	blob_flag=1;
	break;
      }
    }
  }
  field_term_length=exchange->field_term->length();
  if (!exchange->line_term->length())
    exchange->line_term=exchange->field_term;	// Use this if it exists
  field_sep_char= (exchange->enclosed->length() ? (*exchange->enclosed)[0] :
		   field_term_length ? (*exchange->field_term)[0] : INT_MAX);
  escape_char=	(exchange->escaped->length() ? (*exchange->escaped)[0] : -1);
  line_sep_char= (exchange->line_term->length() ?
		  (*exchange->line_term)[0] : INT_MAX);
  if (!field_term_length)
    exchange->opt_enclosed=0;
  if (!exchange->enclosed->length())
    exchange->opt_enclosed=1;			// A little quicker loop
  fixed_row_size= (!field_term_length && !exchange->enclosed->length() &&
		   !blob_flag);
  return 0;
}


bool select_export::send_data(List<Item> &items)
{

  DBUG_ENTER("send_data");
  char buff[MAX_FIELD_WIDTH],null_buff[2],space[MAX_FIELD_WIDTH];
  bool space_inited=0;
  String tmp(buff,sizeof(buff)),*res;
  tmp.length(0);

  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    DBUG_RETURN(0);
  }
  row_count++;
  Item *item;
  char *buff_ptr=buff;
  uint used_length=0,items_left=items.elements;
  List_iterator_fast<Item> li(items);

  if (my_b_write(&cache,(byte*) exchange->line_start->ptr(),
		 exchange->line_start->length()))
    goto err;
  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    res=item->str_result(&tmp);
    if (res && (!exchange->opt_enclosed || result_type == STRING_RESULT))
    {
      if (my_b_write(&cache,(byte*) exchange->enclosed->ptr(),
		     exchange->enclosed->length()))
	goto err;
    }
    if (!res)
    {						// NULL
      if (!fixed_row_size)
      {
	if (escape_char != -1)			// Use \N syntax
	{
	  null_buff[0]=escape_char;
	  null_buff[1]='N';
	  if (my_b_write(&cache,(byte*) null_buff,2))
	    goto err;
	}
	else if (my_b_write(&cache,(byte*) "NULL",4))
	  goto err;
      }
      else
      {
	used_length=0;				// Fill with space
      }
    }
    else
    {
      if (fixed_row_size)
	used_length=min(res->length(),item->max_length);
      else
	used_length=res->length();
      if (result_type == STRING_RESULT && escape_char != -1)
      {
	char *pos,*start,*end;

	for (start=pos=(char*) res->ptr(),end=pos+used_length ;
	     pos != end ;
	     pos++)
	{
#ifdef USE_MB
	  if (use_mb(default_charset_info))
	  {
	    int l;
	    if ((l=my_ismbchar(default_charset_info, pos, end)))
	    {
	      pos += l-1;
	      continue;
	    }
	  }
#endif
	  if ((int) *pos == escape_char || (int) *pos == field_sep_char ||
	      (int) *pos == line_sep_char || !*pos)
	  {
	    char tmp_buff[2];
	    tmp_buff[0]= escape_char;
	    tmp_buff[1]= *pos ? *pos : '0';
	    if (my_b_write(&cache,(byte*) start,(uint) (pos-start)) ||
		my_b_write(&cache,(byte*) tmp_buff,2))
	      goto err;
	    start=pos+1;
	  }
	}
	if (my_b_write(&cache,(byte*) start,(uint) (pos-start)))
	  goto err;
      }
      else if (my_b_write(&cache,(byte*) res->ptr(),used_length))
	goto err;
    }
    if (fixed_row_size)
    {						// Fill with space
      if (item->max_length > used_length)
      {
	/* QQ:  Fix by adding a my_b_fill() function */
	if (!space_inited)
	{
	  space_inited=1;
	  bfill(space,sizeof(space),' ');
	}
	uint length=item->max_length-used_length;
	for (; length > sizeof(space) ; length-=sizeof(space))
	{
	  if (my_b_write(&cache,(byte*) space,sizeof(space)))
	    goto err;
	}
	if (my_b_write(&cache,(byte*) space,length))
	  goto err;
      }
    }
    buff_ptr=buff;				// Place separators here
    if (res && (!exchange->opt_enclosed || result_type == STRING_RESULT))
    {
      memcpy(buff_ptr,exchange->enclosed->ptr(),exchange->enclosed->length());
      buff_ptr+=exchange->enclosed->length();
    }
    if (--items_left)
    {
      memcpy(buff_ptr,exchange->field_term->ptr(),field_term_length);
      buff_ptr+=field_term_length;
    }
    if (my_b_write(&cache,(byte*) buff,(uint) (buff_ptr-buff)))
      goto err;
  }
  if (my_b_write(&cache,(byte*) exchange->line_term->ptr(),
		 exchange->line_term->length()))
    goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


void select_export::send_error(uint errcode,const char *err)
{
  ::send_error(&thd->net,errcode,err);
  if (file > 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    (void) my_delete(path,MYF(0));		// Delete file on error
    file= -1;
  }
}


bool select_export::send_eof()
{
  int error=test(end_io_cache(&cache));
  if (my_close(file,MYF(MY_WME)))
    error=1;
  if (error)
    ::send_error(&thd->net);
  else
    ::send_ok(&thd->net,row_count);
  file= -1;
  return error;
}


/***************************************************************************
** Dump  of select to a binary file
***************************************************************************/


select_dump::~select_dump()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
}

int
select_dump::prepare(List<Item> &list __attribute__((unused)))
{
  uint option=4;
#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|=1;					// Force use of db directory
#endif
  (void) fn_format(path,exchange->file_name, thd->db ? thd->db : "", "",
		   option);
  if (!access(path,F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR,MYF(0),exchange->file_name);
    return 1;
  }
  /* Create the file world readable */
  if ((file=my_create(path, 0666, O_WRONLY, MYF(MY_WME))) < 0)
    return 1;
#ifdef HAVE_FCHMOD
  (void) fchmod(file,0666);			// Because of umask()
#else
  (void) chmod(path,0666);
#endif
  if (init_io_cache(&cache,file,0L,WRITE_CACHE,0L,1,MYF(MY_WME)))
  {
    my_close(file,MYF(0));
    my_delete(path,MYF(0));
    file= -1;
    return 1;
  }
  return 0;
}


bool select_dump::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff)),*res;
  tmp.length(0);
  Item *item;
  DBUG_ENTER("send_data");

  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    DBUG_RETURN(0);
  }
  if (row_count++ > 1) 
  {
    my_error(ER_TOO_MANY_ROWS,MYF(0));
    goto err;
  }
  while ((item=li++))
  {
    res=item->str_result(&tmp);
    if (!res)					// If NULL
    {
      if (my_b_write(&cache,(byte*) "",1))
	goto err;
    }
    else if (my_b_write(&cache,(byte*) res->ptr(),res->length()))
    {
      my_error(ER_ERROR_ON_WRITE,MYF(0), path, my_errno);
      goto err;
    }
  }
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


void select_dump::send_error(uint errcode,const char *err)
{
  ::send_error(&thd->net,errcode,err);
  if (file > 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    (void) my_delete(path,MYF(0));		// Delete file on error
    file= -1;
  }
}


bool select_dump::send_eof()
{
  int error=test(end_io_cache(&cache));
  if (my_close(file,MYF(MY_WME)))
    error=1;
  if (error)
    ::send_error(&thd->net);
  else
    ::send_ok(&thd->net,row_count);
  file= -1;
  return error;
}
