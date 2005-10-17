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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <sys/stat.h>
#include <thr_alarm.h>
#ifdef	__WIN__
#include <io.h>
#endif
#include <mysys_err.h>

#include "sp_rcontext.h"
#include "sp_cache.h"

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
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

Open_tables_state::Open_tables_state(ulong version_arg)
  :version(version_arg)
{
  reset_open_tables_state();
}


/*
  Pass nominal parameters to Statement constructor only to ensure that
  the destructor works OK in case of error. The main_mem_root will be
  re-initialized in init().
*/

THD::THD()
  :Statement(CONVENTIONAL_EXECUTION, 0, ALLOC_ROOT_MIN_BLOCK_SIZE, 0),
   Open_tables_state(refresh_version),
   lock_id(&main_lock_id),
   user_time(0), in_sub_stmt(0), global_read_lock(0), is_fatal_error(0),
   rand_used(0), time_zone_used(0),
   last_insert_id_used(0), insert_id_used(0), clear_next_insert_id(0),
   in_lock_tables(0), bootstrap(0), derived_tables_processing(FALSE),
   spcont(NULL)
{
  stmt_arena= this;
  db= 0;
  catalog= (char*)"std"; // the only catalog we have for now
  main_security_ctx.init();
  security_ctx= &main_security_ctx;
  locked=some_tables_deleted=no_errors=password= 0;
  query_start_used= 0;
  count_cuted_fields= CHECK_FIELD_IGNORE;
  killed= NOT_KILLED;
  db_length= col_access=0;
  query_error= tmp_table_used= 0;
  next_insert_id=last_insert_id=0;
  hash_clear(&handler_tables_hash);
  tmp_table=0;
  used_tables=0;
  cuted_fields= sent_row_count= 0L;
  limit_found_rows= 0;
  statement_id_counter= 0UL;
  // Must be reset to handle error with THD's created for init of mysqld
  lex->current_select= 0;
  start_time=(time_t) 0;
  current_linfo =  0;
  slave_thread = 0;
  variables.pseudo_thread_id= 0;
  one_shot_set= 0;
  file_id = 0;
  query_id= 0;
  warn_id= 0;
  db_charset= global_system_variables.collation_database;
  bzero(ha_data, sizeof(ha_data));
  mysys_var=0;
  binlog_evt_union.do_union= FALSE;
#ifndef DBUG_OFF
  dbug_sentry=THD_SENTRY_MAGIC;
#endif
#ifndef EMBEDDED_LIBRARY
  net.vio=0;
#endif
  net.last_error[0]=0;                          // If error on boot
  net.query_cache_query=0;                      // If error on boot
  ull=0;
  system_thread= cleanup_done= abort_on_warning= no_warnings_for_error= 0;
  peer_port= 0;					// For SHOW PROCESSLIST
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
  *scramble= '\0';

  init();
  /* Initialize sub structures */
  init_sql_alloc(&warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);
  user_connect=(USER_CONN *)0;
  hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var, 0);

  sp_proc_cache= NULL;
  sp_func_cache= NULL;

  /* For user vars replication*/
  if (opt_bin_log)
    my_init_dynamic_array(&user_var_events,
			  sizeof(BINLOG_USER_VAR_EVENT *), 16, 16);
  else
    bzero((char*) &user_var_events, sizeof(user_var_events));

  /* Protocol */
  protocol= &protocol_simple;			// Default protocol
  protocol_simple.init(this);
  protocol_prep.init(this);

  tablespace_op=FALSE;
  ulong tmp=sql_rnd_with_mutex();
  randominit(&rand, tmp + (ulong) &rand, tmp + (ulong) ::query_id);
  thr_lock_info_init(&lock_info); /* safety: will be reset after start */
  thr_lock_owner_init(&main_lock_id, &lock_info);
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
#ifdef HAVE_NDBCLUSTER_DB
  variables.ndb_use_transactions= 1;
#endif
  pthread_mutex_unlock(&LOCK_global_system_variables);
  server_status= SERVER_STATUS_AUTOCOMMIT;
  if (variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
    server_status|= SERVER_STATUS_NO_BACKSLASH_ESCAPES;
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
  bzero((char *) &status_var, sizeof(status_var));
}


/*
  Init THD for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in sql_class.h.
*/

void THD::init_for_queries()
{
  ha_enable_transaction(this,TRUE);

  reset_root_defaults(mem_root, variables.query_alloc_block_size,
                      variables.query_prealloc_size);
#ifdef USING_TRANSACTIONS
  reset_root_defaults(&transaction.mem_root,
                      variables.trans_alloc_block_size,
                      variables.trans_prealloc_size);
#endif
  transaction.xid_state.xid.null();
  transaction.xid_state.in_thd=1;
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
  stmt_map.reset();
  hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var, 0);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);
}


/* Do operations that may take a long time */

void THD::cleanup(void)
{
  DBUG_ENTER("THD::cleanup");
#ifdef ENABLE_WHEN_BINLOG_WILL_BE_ABLE_TO_PREPARE
  if (transaction.xid_state.xa_state == XA_PREPARED)
  {
#error xid_state in the cache should be replaced by the allocated value
  }
#endif
  {
    ha_rollback(this);
    xid_cache_delete(&transaction.xid_state);
  }
  if (locked_tables)
  {
    lock=locked_tables; locked_tables=0;
    close_thread_tables(this);
  }
  mysql_ha_flush(this, (TABLE_LIST*) 0,
                 MYSQL_HA_CLOSE_FINAL | MYSQL_HA_FLUSH_ALL);
  hash_free(&handler_tables_hash);
  delete_dynamic(&user_var_events);
  hash_free(&user_vars);
  close_temporary_tables(this);
  my_free((char*) variables.time_format, MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) variables.date_format, MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) variables.datetime_format, MYF(MY_ALLOW_ZERO_PTR));
  
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

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
  add_to_status(&global_status_var, &status_var);

  /* Close connection */
#ifndef EMBEDDED_LIBRARY
  if (net.vio)
  {
    vio_delete(net.vio);
    net_end(&net);
  }
#endif
  stmt_map.destroy();                     /* close all prepared statements */
  DBUG_ASSERT(lock_info.n_cursors == 0);
  if (!cleanup_done)
    cleanup();

  ha_close_connection(this);

  DBUG_PRINT("info", ("freeing security context"));
  main_security_ctx.destroy();
  safeFree(db);
  free_root(&warn_root,MYF(0));
#ifdef USING_TRANSACTIONS
  free_root(&transaction.mem_root,MYF(0));
#endif
  mysys_var=0;					// Safety (shouldn't be needed)
  pthread_mutex_destroy(&LOCK_delete);
#ifndef DBUG_OFF
  dbug_sentry= THD_SENTRY_GONE;
#endif  
  DBUG_VOID_RETURN;
}


/*
  Add to one status variable another status variable

  NOTES
    This function assumes that all variables are long/ulong.
    If this assumption will change, then we have to explictely add
    the other variables after the while loop
*/

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var)
{
  ulong *end= (ulong*) ((byte*) to_var + offsetof(STATUS_VAR,
						  last_system_status_var) +
			sizeof(ulong));
  ulong *to= (ulong*) to_var, *from= (ulong*) from_var;

  while (to != end)
    *(to++)+= *(from++);
  /* it doesn't make sense to add last_query_cost values */
}


void THD::awake(THD::killed_state state_to_set)
{
  THD_CHECK_SENTRY(this);
  safe_mutex_assert_owner(&LOCK_delete); 

  killed= state_to_set;
  if (state_to_set != THD::KILL_QUERY)
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
  /*
    We have to call thr_lock_info_init() again here as THD may have been
    created in another thread
  */
  thr_lock_info_init(&lock_info);
  return 0;
}


/* Cleanup after a query */

void THD::cleanup_after_query()
{
  if (clear_next_insert_id)
  {
    clear_next_insert_id= 0;
    next_insert_id= 0;
  }
  /* Free Items that were created during this execution */
  free_items();
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
  uint dummy_errors;
  if (!(to->str= alloc(new_length+1)))
  {
    to->length= 0;				// Safety fix
    DBUG_RETURN(1);				// EOM
  }
  to->length= copy_and_convert((char*) to->str, new_length, to_cs,
			       from, from_length, from_cs, &dummy_errors);
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
  uint dummy_errors;
  if (convert_buffer.copy(s->ptr(), s->length(), from_cs, to_cs, &dummy_errors))
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
  add_changed_table(table->s->table_cache_key, table->s->key_length);
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
    killed= KILL_CONNECTION;
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
  field_list.push_back(item= new Item_empty_string("table", NAME_LEN, cs));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_empty_string("type", 10, cs));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_LEN*MAX_KEY, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key", NAME_LEN, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key_len",
						  NAME_LEN*MAX_KEY));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("ref",
						  NAME_LEN*MAX_REF_PARTS, cs));
  item->maybe_null=1;
  field_list.push_back(item= new Item_return_int("rows", 10,
                                                 MYSQL_TYPE_LONGLONG));
  item->maybe_null= 1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  return (result->send_fields(field_list,
                              Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF));
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


struct Item_change_record: public ilink
{
  Item **place;
  Item *old_value;
  /* Placement new was hidden by `new' in ilink (TODO: check): */
  static void *operator new(size_t size, void *mem) { return mem; }
  static void operator delete(void *ptr, size_t size) {}
  static void operator delete(void *ptr, void *mem) { /* never called */ }
};


/*
  Register an item tree tree transformation, performed by the query
  optimizer. We need a pointer to runtime_memroot because it may be !=
  thd->mem_root (due to possible set_n_backup_active_arena called for thd).
*/

void THD::nocheck_register_item_tree_change(Item **place, Item *old_value,
                                            MEM_ROOT *runtime_memroot)
{
  Item_change_record *change;
  /*
    Now we use one node per change, which adds some memory overhead,
    but still is rather fast as we use alloc_root for allocations.
    A list of item tree changes of an average query should be short.
  */
  void *change_mem= alloc_root(runtime_memroot, sizeof(*change));
  if (change_mem == 0)
  {
    /*
      OOM, thd->fatal_error() is called by the error handler of the
      memroot. Just return.
    */
    return;
  }
  change= new (change_mem) Item_change_record;
  change->place= place;
  change->old_value= old_value;
  change_list.append(change);
}


void THD::rollback_item_tree_changes()
{
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  DBUG_ENTER("rollback_item_tree_changes");

  while ((change= it++))
    *change->place= change->old_value;
  /* We can forget about changes memory: it's allocated in runtime memroot */
  change_list.empty();
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

select_result::select_result()
{
  thd=current_thd;
}

void select_result::send_error(uint errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
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

bool select_send::send_fields(List<Item> &list, uint flags)
{
  bool res;
  if (!(res= thd->protocol->send_fields(&list, flags)))
    status= 1;
  return res;
}

void select_send::abort()
{
  DBUG_ENTER("select_send::abort");
  if (status && thd->spcont &&
      thd->spcont->find_handler(thd->net.last_errno,
                                MYSQL_ERROR::WARN_LEVEL_ERROR))
  {
    /*
      Executing stored procedure without a handler.
      Here we should actually send an error to the client,
      but as an error will break a multiple result set, the only thing we
      can do for now is to nicely end the current data set and remembering
      the error so that the calling routine will abort
    */
    thd->net.report_error= 0;
    send_eof();
    thd->net.report_error= 1; // Abort SP
  }
  DBUG_VOID_RETURN;
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
    ha_release_temporary_latches(thd);
#endif

  /* Unlock tables before sending packet to gain some speed */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  if (!thd->net.report_error)
  {
    ::send_eof(thd);
    status= 0;
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
  my_message(errcode, err, MYF(0));
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
  if ((file= my_create(path, 0666, O_WRONLY|O_EXCL, MYF(MY_WME))) < 0)
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
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
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
      my_error(ER_ERROR_ON_WRITE, MYF(0), path, my_errno);
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


void select_max_min_finder_subselect::cleanup()
{
  DBUG_ENTER("select_max_min_finder_subselect::cleanup");
  cache= 0;
  DBUG_VOID_RETURN;
}


bool select_max_min_finder_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_max_min_finder_subselect::send_data");
  Item_maxmin_subselect *it= (Item_maxmin_subselect *)item;
  List_iterator_fast<Item> li(items);
  Item *val_item= li++;
  it->register_value();
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
      case DECIMAL_RESULT:
        op= &select_max_min_finder_subselect::cmp_decimal;
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
  double val1= cache->val_real(), val2= maxmin->val_real();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
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
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     val1 < val2);
}

bool select_max_min_finder_subselect::cmp_decimal()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->el(0);
  my_decimal cval, *cvalue= cache->val_decimal(&cval);
  my_decimal mval, *mvalue= maxmin->val_decimal(&mval);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       my_decimal_cmp(cvalue, mvalue) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     my_decimal_cmp(cvalue,mvalue) < 0);
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
  List_iterator_fast<my_var> gl(var_list);
  Item *item;

  local_vars.empty();				// Clear list if SP
  unit= u;
  row_count= 0;

  if (var_list.elements != list.elements)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
               ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT), MYF(0));
    return 1;
  }
  while ((item=li++))
  {
    my_var *mv= gl++;
    if (mv->local)
      (void)local_vars.push_back(new Item_splocal(mv->s, mv->offset));
    else
    {
      Item_func_set_user_var *var= new Item_func_set_user_var(mv->s, item);
      /*
        Item_func_set_user_var can't substitute something else on its place =>
        0 can be passed as last argument (reference on item)
        Item_func_set_user_var can't be fixed after creation, so we do not
        check var->fixed
      */
      var->fix_fields(thd, 0);
      var->fix_length_and_dec();
      vars.push_back(var);
    }
  }
  return 0;
}


void select_dumpvar::cleanup()
{
  vars.empty();
  row_count=0;
}


Query_arena::Type Query_arena::type() const
{
  DBUG_ASSERT(0); /* Should never be called */
  return STATEMENT;
}


void Query_arena::free_items()
{
  Item *next;
  DBUG_ENTER("Query_arena::free_items");
  /* This works because items are allocated with sql_alloc() */
  for (; free_list; free_list= next)
  {
    next= free_list->next;
    free_list->delete_self();
  }
  /* Postcondition: free_list is 0 */
  DBUG_VOID_RETURN;
}


void Query_arena::set_query_arena(Query_arena *set)
{
  mem_root=  set->mem_root;
  free_list= set->free_list;
  state= set->state;
}


void Query_arena::cleanup_stmt()
{
  DBUG_ASSERT("Query_arena::cleanup_stmt()" == "not implemented");
}

/*
  Statement functions 
*/

Statement::Statement(enum enum_state state_arg, ulong id_arg,
                     ulong alloc_block_size, ulong prealloc_size)
  :Query_arena(&main_mem_root, state_arg),
  id(id_arg),
  set_query_id(1),
  lex(&main_lex),
  query(0),
  query_length(0),
  cursor(0)
{
  name.str= NULL;
  init_sql_alloc(&main_mem_root, alloc_block_size, prealloc_size);
}


Query_arena::Type Statement::type() const
{
  return STATEMENT;
}


void Statement::set_statement(Statement *stmt)
{
  id=             stmt->id;
  set_query_id=   stmt->set_query_id;
  lex=            stmt->lex;
  query=          stmt->query;
  query_length=   stmt->query_length;
  cursor=         stmt->cursor;
}


void
Statement::set_n_backup_statement(Statement *stmt, Statement *backup)
{
  DBUG_ENTER("Statement::set_n_backup_statement");
  backup->set_statement(this);
  set_statement(stmt);
  DBUG_VOID_RETURN;
}


void Statement::restore_backup_statement(Statement *stmt, Statement *backup)
{
  DBUG_ENTER("Statement::restore_backup_statement");
  stmt->set_statement(this);
  set_statement(backup);
  DBUG_VOID_RETURN;
}


void THD::end_statement()
{
  /* Cleanup SQL processing state to resuse this statement in next query. */
  lex_end(lex);
  delete lex->result;
  lex->result= 0;
  /* Note that free_list is freed in cleanup_after_query() */

  /*
    Don't free mem_root, as mem_root is freed in the end of dispatch_command
    (once for any command).
  */
}


void THD::set_n_backup_active_arena(Query_arena *set, Query_arena *backup)
{
  DBUG_ENTER("THD::set_n_backup_active_arena");
  DBUG_ASSERT(backup->is_backup_arena == FALSE);

  backup->set_query_arena(this);
  set_query_arena(set);
#ifndef DBUG_OFF
  backup->is_backup_arena= TRUE;
#endif
  DBUG_VOID_RETURN;
}


void THD::restore_active_arena(Query_arena *set, Query_arena *backup)
{
  DBUG_ENTER("THD::restore_active_arena");
  DBUG_ASSERT(backup->is_backup_arena);
  set->set_query_arena(this);
  set_query_arena(backup);
#ifndef DBUG_OFF
  backup->is_backup_arena= FALSE;
#endif
  DBUG_VOID_RETURN;
}

Statement::~Statement()
{
  /*
    We must free `main_mem_root', not `mem_root' (pointer), to work
    correctly if this statement is used as a backup statement,
    for which `mem_root' may point to some other statement.
  */
  free_root(&main_mem_root, MYF(0));
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
  int res= my_hash_insert(&st_hash, (byte *) statement);
  if (res)
    return res;
  if (statement->name.str)
  {
    if ((res= my_hash_insert(&names_hash, (byte*)statement)))
    {
      hash_delete(&st_hash, (byte*)statement);
      return res;
    }
  }
  last_found_statement= statement;
  return res;
}


void Statement_map::close_transient_cursors()
{
#ifdef TO_BE_IMPLEMENTED
  Statement *stmt;
  while ((stmt= transient_cursor_list.head()))
    stmt->close_cursor();                 /* deletes itself from the list */
#endif
}


bool select_dumpvar::send_data(List<Item> &items)
{
  List_iterator_fast<Item_func_set_user_var> li(vars);
  List_iterator_fast<Item_splocal> var_li(local_vars);
  List_iterator_fast<my_var> my_li(var_list);
  List_iterator<Item> it(items);
  Item_func_set_user_var *xx;
  Item_splocal *yy;
  my_var *zz;
  DBUG_ENTER("send_data");
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }

  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  if (row_count++) 
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    DBUG_RETURN(1);
  }
  while ((zz=my_li++) && (it++))
  {
    if (zz->local)
    {
      if ((yy=var_li++)) 
      {
	if (thd->spcont->set_item_eval(current_thd,
				       yy->get_offset(), it.ref(), zz->type))
	  DBUG_RETURN(1);
      }
    }
    else
    {
      if ((xx=li++))
      {
        xx->check();
	xx->update();
      }
    }
  }
  DBUG_RETURN(0);
}

bool select_dumpvar::send_eof()
{
  if (! row_count)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA));
  ::send_ok(thd,row_count);
  return 0;
}

/****************************************************************************
  TMP_TABLE_PARAM
****************************************************************************/

void TMP_TABLE_PARAM::init()
{
  field_count= sum_func_count= func_count= hidden_field_count= 0;
  group_parts= group_length= group_null_parts= 0;
  quick_group= 1;
  table_charset= 0;
}


void thd_increment_bytes_sent(ulong length)
{
  THD *thd=current_thd;
  if (likely(thd != 0))
  { /* current_thd==0 when close_connection() calls net_send_error() */
    thd->status_var.bytes_sent+= length;
  }
}


void thd_increment_bytes_received(ulong length)
{
  current_thd->status_var.bytes_received+= length;
}


void thd_increment_net_big_packet_count(ulong length)
{
  current_thd->status_var.net_big_packet_count+= length;
}


void THD::set_status_var_init()
{
  bzero((char*) &status_var, sizeof(status_var));
}


void Security_context::init()
{
  host= user= priv_user= ip= 0;
  host_or_ip= "connecting host";
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  db_access= NO_ACCESS;
#endif
}


void Security_context::destroy()
{
  // If not pointer to constant
  if (host != my_localhost)
    safeFree(host);
  if (user != delayed_user)
    safeFree(user);
  safeFree(ip);
}


void Security_context::skip_grants()
{
  /* privileges for the user are unknown everything is allowed */
  host_or_ip= (char *)"";
  master_access= ~NO_ACCESS;
  priv_user= (char *)"";
  *priv_host= '\0';
}


/****************************************************************************
  Handling of open and locked tables states.

  This is used when we want to open/lock (and then close) some tables when
  we already have a set of tables open and locked. We use these methods for
  access to mysql.proc table to find definitions of stored routines.
****************************************************************************/

void THD::reset_n_backup_open_tables_state(Open_tables_state *backup)
{
  DBUG_ENTER("reset_n_backup_open_tables_state");
  backup->set_open_tables_state(this);
  reset_open_tables_state();
  DBUG_VOID_RETURN;
}


void THD::restore_backup_open_tables_state(Open_tables_state *backup)
{
  DBUG_ENTER("restore_backup_open_tables_state");
  /*
    Before we will throw away current open tables state we want
    to be sure that it was properly cleaned up.
  */
  DBUG_ASSERT(open_tables == 0 && temporary_tables == 0 &&
              handler_tables == 0 && derived_tables == 0 &&
              lock == 0 && locked_tables == 0 &&
              prelocked_mode == NON_PRELOCKED);
  set_open_tables_state(backup);
  DBUG_VOID_RETURN;
}



/****************************************************************************
  Handling of statement states in functions and triggers.

  This is used to ensure that the function/trigger gets a clean state
  to work with and does not cause any side effects of the calling statement.

  It also allows most stored functions and triggers to replicate even
  if they are used items that would normally be stored in the binary
  replication (like last_insert_id() etc...)

  The following things is done
  - Disable binary logging for the duration of the statement
  - Disable multi-result-sets for the duration of the statement
  - Value of last_insert_id() is reset and restored
  - Value set by 'SET INSERT_ID=#' is reset and restored
  - Value for found_rows() is reset and restored
  - examined_row_count is added to the total
  - cuted_fields is added to the total

  NOTES:
    Seed for random() is saved for the first! usage of RAND()
    We reset examined_row_count and cuted_fields and add these to the
    result to ensure that if we have a bug that would reset these within
    a function, we are not loosing any rows from the main statement.
****************************************************************************/

void THD::reset_sub_statement_state(Sub_statement_state *backup,
                                    uint new_state)
{
  backup->options=         options;
  backup->in_sub_stmt=     in_sub_stmt;
  backup->no_send_ok=      net.no_send_ok;
  backup->enable_slow_log= enable_slow_log;
  backup->last_insert_id=  last_insert_id;
  backup->next_insert_id=  next_insert_id;
  backup->insert_id_used=  insert_id_used;
  backup->limit_found_rows= limit_found_rows;
  backup->examined_row_count= examined_row_count;
  backup->sent_row_count=   sent_row_count;
  backup->cuted_fields=     cuted_fields;
  backup->client_capabilities= client_capabilities;

  if (!lex->requires_prelocking() || is_update_query(lex->sql_command))
    options&= ~OPTION_BIN_LOG;
  /* Disable result sets */
  client_capabilities &= ~CLIENT_MULTI_RESULTS;
  in_sub_stmt|= new_state;
  last_insert_id= 0;
  next_insert_id= 0;
  insert_id_used= 0;
  examined_row_count= 0;
  sent_row_count= 0;
  cuted_fields= 0;

#ifndef EMBEDDED_LIBRARY
  /* Surpress OK packets in case if we will execute statements */
  net.no_send_ok= TRUE;
#endif
}


void THD::restore_sub_statement_state(Sub_statement_state *backup)
{
  options=          backup->options;
  in_sub_stmt=      backup->in_sub_stmt;
  net.no_send_ok=   backup->no_send_ok;
  enable_slow_log=  backup->enable_slow_log;
  last_insert_id=   backup->last_insert_id;
  next_insert_id=   backup->next_insert_id;
  insert_id_used=   backup->insert_id_used;
  limit_found_rows= backup->limit_found_rows;
  sent_row_count=   backup->sent_row_count;
  client_capabilities= backup->client_capabilities;

  /*
    The following is added to the old values as we are interested in the
    total complexity of the query
  */
  examined_row_count+= backup->examined_row_count;
  cuted_fields+=       backup->cuted_fields;
}


/***************************************************************************
  Handling of XA id cacheing
***************************************************************************/

pthread_mutex_t LOCK_xid_cache;
HASH xid_cache;

static byte *xid_get_hash_key(const byte *ptr,uint *length,
                                  my_bool not_used __attribute__((unused)))
{
  *length=((XID_STATE*)ptr)->xid.key_length();
  return ((XID_STATE*)ptr)->xid.key();
}

static void xid_free_hash (void *ptr)
{
  if (!((XID_STATE*)ptr)->in_thd)
    my_free((gptr)ptr, MYF(0));
}

bool xid_cache_init()
{
  pthread_mutex_init(&LOCK_xid_cache, MY_MUTEX_INIT_FAST);
  return hash_init(&xid_cache, &my_charset_bin, 100, 0, 0,
                   xid_get_hash_key, xid_free_hash, 0) != 0;
}

void xid_cache_free()
{
  if (hash_inited(&xid_cache))
  {
    hash_free(&xid_cache);
    pthread_mutex_destroy(&LOCK_xid_cache);
  }
}

XID_STATE *xid_cache_search(XID *xid)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  XID_STATE *res=(XID_STATE *)hash_search(&xid_cache, xid->key(), xid->key_length());
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID *xid, enum xa_states xa_state)
{
  XID_STATE *xs;
  my_bool res;
  pthread_mutex_lock(&LOCK_xid_cache);
  if (hash_search(&xid_cache, xid->key(), xid->key_length()))
    res=0;
  else if (!(xs=(XID_STATE *)my_malloc(sizeof(*xs), MYF(MY_WME))))
    res=1;
  else
  {
    xs->xa_state=xa_state;
    xs->xid.set(xid);
    xs->in_thd=0;
    res=my_hash_insert(&xid_cache, (byte*)xs);
  }
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID_STATE *xid_state)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  DBUG_ASSERT(hash_search(&xid_cache, xid_state->xid.key(),
                          xid_state->xid.key_length())==0);
  my_bool res=my_hash_insert(&xid_cache, (byte*)xid_state);
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


void xid_cache_delete(XID_STATE *xid_state)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  hash_delete(&xid_cache, (byte *)xid_state);
  pthread_mutex_unlock(&LOCK_xid_cache);
}

