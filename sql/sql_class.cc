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

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";


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

bool key_part_spec::operator==(const key_part_spec& other) const
{
  return length == other.length && !strcmp(field_name, other.field_name);
}


/*
  Test if a foreign key (= generated key) is a prefix of the given key
  (ignoring key name, key type and order of columns)

  NOTES:
    This is only used to test if an index for a FOREIGN KEY exists

  IMPLEMENTATION
    We only compare field names

  RETURN
    0	Generated key is a prefix of other key
    1	Not equal
*/

bool foreign_key_prefix(Key *a, Key *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.elements > b->columns.elements)
      swap_variables(Key*, a, b);               // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return TRUE;                              // No foreign key
    swap_variables(Key*, a, b);                 // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.elements > b->columns.elements)
    return TRUE;                                // Can't be prefix

  List_iterator<key_part_spec> col_it1(a->columns);
  List_iterator<key_part_spec> col_it2(b->columns);
  const key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= TRUE;
	break;
      }
    }
    if (!found)
      return TRUE;                              // Error
  }
  return FALSE;                                 // Is prefix
#else
  while ((col1= col_it1++))
  {
    col2= col_it2++;
    if (!(*col1 == *col2))
      return TRUE;
  }
  return FALSE;                                 // Is prefix
#endif
}


/****************************************************************************
** Thread specific functions
****************************************************************************/

THD::THD()
  :user_time(0), global_read_lock(0), is_fatal_error(0),
   last_insert_id_used(0),
   insert_id_used(0), rand_used(0), time_zone_used(0),
   in_lock_tables(0), bootstrap(0)
{
  current_arena= this;
  host= user= priv_user= db= ip=0;
  host_or_ip= "connecting host";
  locked=some_tables_deleted=no_errors=password= 0;
  killed=0;
  query_start_used= 0;
  count_cuted_fields= CHECK_FIELD_IGNORE;
  db_length= col_access= 0;
  query_error= tmp_table_used= 0;
  next_insert_id=last_insert_id=0;
  open_tables= temporary_tables= handler_tables= derived_tables= 0;
  tmp_table=0;
  lock=locked_tables=0;
  used_tables=0;
  cuted_fields= sent_row_count= 0L;
  statement_id_counter= 0UL;
  // Must be reset to handle error with THD's created for init of mysqld
  lex->current_select= 0;
  start_time=(time_t) 0;
  current_linfo =  0;
  slave_thread = 0;
  variables.pseudo_thread_id= 0;
  one_shot_set= 0;
  file_id = 0;
  warn_id= 0;
  db_charset= global_system_variables.collation_database;
  mysys_var=0;
#ifndef DBUG_OFF
  dbug_sentry=THD_SENTRY_MAGIC;
#endif
#ifndef EMBEDDED_LIBRARY  
  net.vio=0;
#endif
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
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  db_access=NO_ACCESS;
#endif
  version=refresh_version;			// For boot
  *scramble= '\0';

  init();
  /* Initialize sub structures */
  clear_alloc_root(&transaction.mem_root);
  init_alloc_root(&warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);
  user_connect=(USER_CONN *)0;
  hash_init(&user_vars, &my_charset_bin, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var,0);

  /* For user vars replication*/
  if (opt_bin_log)
    my_init_dynamic_array(&user_var_events,
			  sizeof(BINLOG_USER_VAR_EVENT *),
			  16,
			  16);
  else
    bzero((char*) &user_var_events, sizeof(user_var_events));

  /* Protocol */
  protocol= &protocol_simple;			// Default protocol
  protocol_simple.init(this);
  protocol_prep.init(this);

  tablespace_op=FALSE;
#ifdef USING_TRANSACTIONS
  bzero((char*) &transaction,sizeof(transaction));
  /*
    Binlog is always open (if needed) before a THD is created (including
    bootstrap).
  */
  if (opt_using_transactions && mysql_bin_log.is_open())
  {
    if (open_cached_file(&transaction.trans_log,
			 mysql_tmpdir, LOG_PREFIX, binlog_cache_size,
			 MYF(MY_WME)))
      killed=1;
    transaction.trans_log.end_of_file= max_binlog_cache_size;
  }
#endif
  {
    ulong tmp=sql_rnd_with_mutex();
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
  variables.time_format= date_time_format_copy((THD*) 0,
					       variables.time_format);
  variables.date_format= date_time_format_copy((THD*) 0,
					       variables.date_format);
  variables.datetime_format= date_time_format_copy((THD*) 0,
						   variables.datetime_format);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  server_status= SERVER_STATUS_AUTOCOMMIT;
  options= thd_startup_options;
  open_options=ha_open_options;
  update_lock_default= (variables.low_priority_updates ?
			TL_WRITE_LOW_PRIORITY :
			TL_WRITE);
  session_tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  warn_list.empty();
  bzero((char*) warn_count, sizeof(warn_count));
  total_warn_count= 0;
  update_charset();
}


/*
  Init THD for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in sql_class.h.
*/

void THD::init_for_queries()
{
  ha_enable_transaction(this,TRUE);
  init_sql_alloc(&mem_root,
                 variables.query_alloc_block_size,
                 variables.query_prealloc_size);
  init_sql_alloc(&transaction.mem_root,         
		 variables.trans_alloc_block_size, 
		 variables.trans_prealloc_size);
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
  cleanup_done= 0;
  init();
  hash_init(&user_vars, &my_charset_bin, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var, 0);
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
  if (handler_tables)
  {
    open_tables=handler_tables; handler_tables=0;
    close_thread_tables(this);
  }
  close_temporary_tables(this);
  my_free((char*) variables.time_format, MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) variables.date_format, MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) variables.datetime_format, MYF(MY_ALLOW_ZERO_PTR));
  delete_dynamic(&user_var_events);
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
#ifndef EMBEDDED_LIBRARY  
  if (net.vio)
  {
    vio_delete(net.vio);
    net_end(&net); 
  }
#endif
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
  if (host != my_localhost)			// If not pointer to constant
    safeFree(host);
  if (user != delayed_user)
    safeFree(user);
  safeFree(ip);
  safeFree(db);
  free_root(&warn_root,MYF(0));
  free_root(&transaction.mem_root,MYF(0));
  mysys_var=0;					// Safety (shouldn't be needed)
  pthread_mutex_destroy(&LOCK_delete);
#ifndef DBUG_OFF
  dbug_sentry = THD_SENTRY_GONE;
#endif  
  /* Reset stmt_backup.mem_root to not double-free memory from thd.mem_root */
  clear_alloc_root(&stmt_backup.mem_root);
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
      current_mutex, but hasn't yet entered enter_cond() (which means that
      current_cond and current_mutex are 0), then the victim will not get
      a signal and it may wait "forever" on the cond (until
      we issue a second KILL or the status it's waiting for happens).
      It's true that we have set its thd->killed but it may not
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
      my_pthread_setspecific_ptr(THR_MALLOC, &mem_root))
    return 1;
  mysys_var=my_thread_var;
  dbug_thread_id=my_thread_id();
  /*
    By default 'slave_proxy_id' is 'thread_id'. They may later become different
    if this is the slave SQL thread.
  */
  variables.pseudo_thread_id= thread_id;
  return 0;
}


/*
  Convert a string to another character set

  SYNOPSIS
    convert_string()
    to				Store new allocated string here
    to_cs			New character set for allocated string
    from			String to convert
    from_length			Length of string to convert
    from_cs			Original character set

  NOTES
    to will be 0-terminated to make it easy to pass to system funcs

  RETURN
    0	ok
    1	End of memory.
        In this case to->str will point to 0 and to->length will be 0.
*/

bool THD::convert_string(LEX_STRING *to, CHARSET_INFO *to_cs,
			 const char *from, uint from_length,
			 CHARSET_INFO *from_cs)
{
  DBUG_ENTER("convert_string");
  size_s new_length= to_cs->mbmaxlen * from_length;
  if (!(to->str= alloc(new_length+1)))
  {
    to->length= 0;				// Safety fix
    DBUG_RETURN(1);				// EOM
  }
  to->length= copy_and_convert((char*) to->str, new_length, to_cs,
			       from, from_length, from_cs);
  to->str[to->length]=0;			// Safety
  DBUG_RETURN(0);
}


/*
  Convert string from source character set to target character set inplace.

  SYNOPSIS
    THD::convert_string

  DESCRIPTION
    Convert string using convert_buffer - buffer for character set 
    conversion shared between all protocols.

  RETURN
    0   ok
   !0   out of memory
*/

bool THD::convert_string(String *s, CHARSET_INFO *from_cs, CHARSET_INFO *to_cs)
{
  if (convert_buffer.copy(s->ptr(), s->length(), from_cs, to_cs))
    return TRUE;
  /* If convert_buffer >> s copying is more efficient long term */
  if (convert_buffer.alloced_length() >= convert_buffer.length() * 2 ||
      !s->is_alloced())
  {
    return s->copy(convert_buffer);
  }
  s->swap(convert_buffer);
  return FALSE;
}


/*
  Update some cache variables when character set changes
*/

void THD::update_charset()
{
  uint32 not_used;
  charset_is_system_charset= !String::needs_conversion(0,charset(),
                                                       system_charset_info,
                                                       &not_used);
  charset_is_collation_connection= 
    !String::needs_conversion(0,charset(),variables.collation_connection,
                              &not_used);
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


int THD::send_explain_fields(select_result *result)
{
  List<Item> field_list;
  Item *item;
  CHARSET_INFO *cs= system_charset_info;
  field_list.push_back(new Item_return_int("id",3, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(new Item_empty_string("table", NAME_LEN, cs));
  field_list.push_back(new Item_empty_string("type", 10, cs));
  field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_LEN*MAX_KEY, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key", NAME_LEN, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_return_int("key_len",3,
						MYSQL_TYPE_LONGLONG));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("ref",
						  NAME_LEN*MAX_REF_PARTS, cs));
  item->maybe_null=1;
  field_list.push_back(new Item_return_int("rows", 10, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  return (result->send_fields(field_list,1));
}

#ifdef SIGNAL_WITH_VIO_CLOSE
void THD::close_active_vio()
{
  DBUG_ENTER("close_active_vio");
  safe_mutex_assert_owner(&LOCK_delete); 
#ifndef EMBEDDED_LIBRARY
  if (active_vio)
  {
    vio_close(active_vio);
    active_vio = 0;
  }
#endif
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

void select_result::send_error(uint errcode,const char *err)
{
  ::send_error(thd, errcode, err);
}


void select_result::cleanup()
{
  /* do nothing */
}

static String default_line_term("\n",default_charset_info);
static String default_escaped("\\",default_charset_info);
static String default_field_term("\t",default_charset_info);

sql_exchange::sql_exchange(char *name,bool flag)
  :file_name(name), opt_enclosed(0), dumpfile(flag), skip_lines(0)
{
  field_term= &default_field_term;
  enclosed=   line_start= &my_empty_string;
  line_term=  &default_line_term;
  escaped=    &default_escaped;
}

bool select_send::send_fields(List<Item> &list,uint flag)
{
  return thd->protocol->send_fields(&list,flag);
}

/* Send data to client. Returns 0 if ok */

bool select_send::send_data(List<Item> &items)
{
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }

#ifdef HAVE_INNOBASE_DB
  /*
    We may be passing the control from mysqld to the client: release the
    InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
    by thd
  */
  if (thd->transaction.all.innobase_tid)
    ha_release_temporary_latches(thd);
#endif

  List_iterator_fast<Item> li(items);
  Protocol *protocol= thd->protocol;
  char buff[MAX_FIELD_WIDTH];
  String buffer(buff, sizeof(buff), &my_charset_bin);
  DBUG_ENTER("send_data");

  protocol->prepare_for_resend();
  Item *item;
  while ((item=li++))
  {
    if (item->send(protocol, &buffer))
    {
      protocol->free();				// Free used buffer
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
      break;
    }
  }
  thd->sent_row_count++;
  if (!thd->vio_ok())
    DBUG_RETURN(0);
  if (!thd->net.report_error)
    DBUG_RETURN(protocol->write());
  DBUG_RETURN(1);
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
  if (!thd->net.report_error)
  {
    ::send_eof(thd);
    return 0;
  }
  else
    return 1;
}


/************************************************************************
  Handling writing to file
************************************************************************/

void select_to_file::send_error(uint errcode,const char *err)
{
  ::send_error(thd,errcode,err);
  if (file > 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    (void) my_delete(path,MYF(0));		// Delete file on error
    file= -1;
  }
}


bool select_to_file::send_eof()
{
  int error= test(end_io_cache(&cache));
  if (my_close(file,MYF(MY_WME)))
    error= 1;
  if (!error)
    ::send_ok(thd,row_count);
  file= -1;
  return error;
}


void select_to_file::cleanup()
{
  /* In case of error send_eof() may be not called: close the file here. */
  if (file >= 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
  path[0]= '\0';
  row_count= 0;
}


select_to_file::~select_to_file()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
}

/***************************************************************************
** Export of select to textfile
***************************************************************************/

select_export::~select_export()
{
  thd->sent_row_count=row_count;
}


/*
  Create file with IO cache

  SYNOPSIS
    create_file()
    thd			Thread handle
    path		File name
    exchange		Excange class
    cache		IO cache

  RETURN
    >= 0 	File handle
   -1		Error
*/


static File create_file(THD *thd, char *path, sql_exchange *exchange,
			IO_CACHE *cache)
{
  File file;
  uint option= MY_UNPACK_FILENAME;

#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|= MY_REPLACE_DIR;			// Force use of db directory
#endif

  if (!dirname_length(exchange->file_name))
  {
    strxnmov(path, FN_REFLEN, mysql_real_data_home, thd->db ? thd->db : "", NullS);
    (void) fn_format(path, exchange->file_name, path, "", option);
  }
  else
    (void) fn_format(path, exchange->file_name, mysql_real_data_home, "", option);
    
  if (!access(path, F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), exchange->file_name);
    return -1;
  }
  /* Create the file world readable */
  if ((file= my_create(path, 0666, O_WRONLY, MYF(MY_WME))) < 0)
    return file;
#ifdef HAVE_FCHMOD
  (void) fchmod(file, 0666);			// Because of umask()
#else
  (void) chmod(path, 0666);
#endif
  if (init_io_cache(cache, file, 0L, WRITE_CACHE, 0L, 1, MYF(MY_WME)))
  {
    my_close(file, MYF(0));
    my_delete(path, MYF(0));  // Delete file on error, it was just created 
    return -1;
  }
  return file;
}


int
select_export::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  bool blob_flag=0;
  unit= u;
  if ((uint) strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
    strmake(path,exchange->file_name,FN_REFLEN-1);

  if ((file= create_file(thd, path, exchange, &cache)) < 0)
    return 1;
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
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
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
          CHARSET_INFO *res_charset=res->charset();
	  if (use_mb(res_charset))
	  {
	    int l;
	    if ((l=my_ismbchar(res_charset, pos, end)))
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


/***************************************************************************
** Dump  of select to a binary file
***************************************************************************/


int
select_dump::prepare(List<Item> &list __attribute__((unused)),
		     SELECT_LEX_UNIT *u)
{
  unit= u;
  return (int) ((file= create_file(thd, path, exchange, &cache)) < 0);
}


bool select_dump::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);
  Item *item;
  DBUG_ENTER("send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  if (row_count++ > 1) 
  {
    my_error(ER_TOO_MANY_ROWS, MYF(0));
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


select_subselect::select_subselect(Item_subselect *item_arg)
{
  item= item_arg;
}


bool select_singlerow_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_singlerow_subselect::send_data");
  Item_singlerow_subselect *it= (Item_singlerow_subselect *)item;
  if (it->assigned())
  {
    my_message(ER_SUBQUERY_NO_1_ROW, ER(ER_SUBQUERY_NO_1_ROW), MYF(0));
    DBUG_RETURN(1);
  }
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  List_iterator_fast<Item> li(items);
  Item *val_item;
  for (uint i= 0; (val_item= li++); i++)
    it->store(i, val_item);
  it->assigned(1);
  DBUG_RETURN(0);
}


bool select_max_min_finder_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_max_min_finder_subselect::send_data");
  Item_singlerow_subselect *it= (Item_singlerow_subselect *)item;
  List_iterator_fast<Item> li(items);
  Item *val_item= li++;
  if (it->assigned())
  {
    cache->store(val_item);
    if ((this->*op)())
      it->store(0, cache);
  }
  else
  {
    if (!cache)
    {
      cache= Item_cache::get_cache(val_item->result_type());
      switch (val_item->result_type())
      {
      case REAL_RESULT:
	op= &select_max_min_finder_subselect::cmp_real;
	break;
      case INT_RESULT:
	op= &select_max_min_finder_subselect::cmp_int;
	break;
      case STRING_RESULT:
	op= &select_max_min_finder_subselect::cmp_str;
	break;
      case ROW_RESULT:
        // This case should never be choosen
	DBUG_ASSERT(0);
	op= 0;
      }
    }
    cache->store(val_item);
    it->store(0, cache);
  }
  it->assigned(1);
  DBUG_RETURN(0);
}

bool select_max_min_finder_subselect::cmp_real()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->el(0);
  double val1= cache->val(), val2= maxmin->val();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  else
    return (maxmin->null_value && !cache->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 < val2);
}

bool select_max_min_finder_subselect::cmp_int()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->el(0);
  longlong val1= cache->val_int(), val2= maxmin->val_int();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  else
    return (maxmin->null_value && !cache->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 < val2);
}

bool select_max_min_finder_subselect::cmp_str()
{
  String *val1, *val2, buf1, buf2;
  Item *maxmin= ((Item_singlerow_subselect *)item)->el(0);
  /*
    as far as both operand is Item_cache buf1 & buf2 will not be used,
    but added for safety
  */
  val1= cache->val_str(&buf1);
  val2= maxmin->val_str(&buf1);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       sortcmp(val1, val2, cache->collation.collation) > 0) ;
  else
    return (maxmin->null_value && !cache->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       sortcmp(val1, val2, cache->collation.collation) < 0);
}

bool select_exists_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_exists_subselect::send_data");
  Item_exists_subselect *it= (Item_exists_subselect *)item;
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  it->value= 1;
  it->assigned(1);
  DBUG_RETURN(0);
}


/***************************************************************************
  Dump of select to variables
***************************************************************************/

int select_dumpvar::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  List_iterator_fast<Item> li(list);
  List_iterator_fast<LEX_STRING> gl(var_list);
  Item *item;
  LEX_STRING *ls;
  if (var_list.elements != list.elements)
  {
    my_error(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, MYF(0));
    return 1;
  }
  unit=u;
  while ((item=li++))
  {
    ls= gl++;
    Item_func_set_user_var *xx = new Item_func_set_user_var(*ls,item);
    /*
      Item_func_set_user_var can't substitute something else on its place =>
      0 can be passed as last argument (reference on item)
    */
    xx->fix_fields(thd,(TABLE_LIST*) thd->lex->select_lex.table_list.first,
		   0);
    xx->fix_length_and_dec();
    vars.push_back(xx);
  }
  return 0;
}


void select_dumpvar::cleanup()
{
  vars.empty();
  row_count=0;
}


Item_arena::Item_arena(THD* thd)
  :free_list(0),
  state(INITIALIZED)
{
  init_sql_alloc(&mem_root,
                 thd->variables.query_alloc_block_size,
                 thd->variables.query_prealloc_size);
}


/* This constructor is called when Item_arena is a subobject of THD */

Item_arena::Item_arena()
  :free_list(0),
  state(CONVENTIONAL_EXECUTION)
{
  clear_alloc_root(&mem_root);
}


Item_arena::Item_arena(bool init_mem_root)
  :free_list(0),
  state(INITIALIZED)
{
  if (init_mem_root)
    clear_alloc_root(&mem_root);
}

Item_arena::Type Item_arena::type() const
{
  DBUG_ASSERT("Item_arena::type()" == "abstract");
  return STATEMENT;
}


Item_arena::~Item_arena()
{}


/*
  Statement functions 
*/

Statement::Statement(THD *thd)
  :Item_arena(thd),
  id(++thd->statement_id_counter),
  set_query_id(1),
  allow_sum_func(0),
  lex(&main_lex),
  query(0),
  query_length(0)
{
  name.str= NULL;
}

/*
  This constructor is called when statement is a subobject of THD:
  Some variables are initialized in THD::init due to locking problems
  This statement object will be used to 
*/

Statement::Statement()
  :id(0),
  set_query_id(1),
  allow_sum_func(0),                            /* initialized later */
  lex(&main_lex),
  query(0),                                     /* these two are set */ 
  query_length(0)                               /* in alloc_query() */
{
}


Item_arena::Type Statement::type() const
{
  return STATEMENT;
}


void Statement::set_statement(Statement *stmt)
{
  id=             stmt->id;
  set_query_id=   stmt->set_query_id;
  allow_sum_func= stmt->allow_sum_func;
  lex=            stmt->lex;
  query=          stmt->query;
  query_length=   stmt->query_length;
}


void
Statement::set_n_backup_statement(Statement *stmt, Statement *backup)
{
  backup->set_statement(this);
  set_statement(stmt);
}


void Statement::restore_backup_statement(Statement *stmt, Statement *backup)
{
  stmt->set_statement(this);
  set_statement(backup);
}


void Statement::end_statement()
{
  /* Cleanup SQL processing state to resuse this statement in next query. */
  lex_end(lex);
  delete lex->result;
  lex->result= 0;
  free_items(free_list);
  free_list= 0;
  /*
    Don't free mem_root, as mem_root is freed in the end of dispatch_command
    (once for any command).
  */
}


void Item_arena::set_n_backup_item_arena(Item_arena *set, Item_arena *backup)
{
  backup->set_item_arena(this);
  set_item_arena(set);
}


void Item_arena::restore_backup_item_arena(Item_arena *set, Item_arena *backup)
{
  set->set_item_arena(this);
  set_item_arena(backup);
  // reset backup mem_root to avoid its freeing
  init_alloc_root(&backup->mem_root, 0, 0);
}

void Item_arena::set_item_arena(Item_arena *set)
{
  mem_root= set->mem_root;
  free_list= set->free_list;
  state= set->state;
}

Statement::~Statement()
{
  free_root(&mem_root, MYF(0));
}

C_MODE_START

static byte *
get_statement_id_as_hash_key(const byte *record, uint *key_length,
                             my_bool not_used __attribute__((unused)))
{
  const Statement *statement= (const Statement *) record; 
  *key_length= sizeof(statement->id);
  return (byte *) &((const Statement *) statement)->id;
}

static void delete_statement_as_hash_key(void *key)
{
  delete (Statement *) key;
}

static byte *get_stmt_name_hash_key(Statement *entry, uint *length,
                                    my_bool not_used __attribute__((unused)))
{
  *length=(uint) entry->name.length;
  return (byte*) entry->name.str;
}

C_MODE_END

Statement_map::Statement_map() :
  last_found_statement(0)
{
  enum
  {
    START_STMT_HASH_SIZE = 16,
    START_NAME_HASH_SIZE = 16
  };
  hash_init(&st_hash, &my_charset_bin, START_STMT_HASH_SIZE, 0, 0,
            get_statement_id_as_hash_key,
            delete_statement_as_hash_key, MYF(0));
  hash_init(&names_hash, system_charset_info, START_NAME_HASH_SIZE, 0, 0,
            (hash_get_key) get_stmt_name_hash_key,
            NULL,MYF(0));
}

int Statement_map::insert(Statement *statement)
{
  int rc= my_hash_insert(&st_hash, (byte *) statement);
  if (rc == 0)
    last_found_statement= statement;
  if (statement->name.str)
  {
    /*
      If there is a statement with the same name, remove it. It is ok to 
      remove old and fail to insert new one at the same time.
    */
    Statement *old_stmt;
    if ((old_stmt= find_by_name(&statement->name)))
      erase(old_stmt); 
    if ((rc= my_hash_insert(&names_hash, (byte*)statement)))
      hash_delete(&st_hash, (byte*)statement);
  }
  return rc;
}


bool select_dumpvar::send_data(List<Item> &items)
{
  List_iterator_fast<Item_func_set_user_var> li(vars);
  Item_func_set_user_var *xx;
  DBUG_ENTER("send_data");

  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  if (row_count++) 
  {
    my_error(ER_TOO_MANY_ROWS, MYF(0));
    DBUG_RETURN(1);
  }
  while ((xx=li++))
  {
    xx->check();
    xx->update();
  }
  DBUG_RETURN(0);
}

bool select_dumpvar::send_eof()
{
  if (row_count)
  {
    ::send_ok(thd,row_count);
    return 0;
  }
  else
  {
    my_error(ER_EMPTY_QUERY,MYF(0));
    return 1;
  }
}

/****************************************************************************
  TMP_TABLE_PARAM
****************************************************************************/

void TMP_TABLE_PARAM::init()
{
  field_count= sum_func_count= func_count= hidden_field_count= 0;
  group_parts= group_length= group_null_parts= 0;
  quick_group= 1;
}
