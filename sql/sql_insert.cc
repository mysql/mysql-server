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


/* Insert of records */

#include "mysql_priv.h"
#include "sql_acl.h"

static int check_null_fields(THD *thd,TABLE *entry);
static TABLE *delayed_get_table(THD *thd,TABLE_LIST *table_list);
static int write_delayed(THD *thd,TABLE *table, enum_duplicates dup,
			 char *query, uint query_length, bool log_on);
static void end_delayed_insert(THD *thd);
static pthread_handler_decl(handle_delayed_insert,arg);
static void unlink_blobs(register TABLE *table);

/* Define to force use of my_malloc() if the allocated memory block is big */

#ifndef HAVE_ALLOCA
#define my_safe_alloca(size, min_length) my_alloca(size)
#define my_safe_afree(ptr, size, min_length) my_afree(ptr)
#else
#define my_safe_alloca(size, min_length) ((size <= min_length) ? my_alloca(size) : my_malloc(size,MYF(0)))
#define my_safe_afree(ptr, size, min_length) if (size > min_length) my_free(ptr,MYF(0))
#endif


/*
  Check if insert fields are correct
  Resets form->time_stamp if a timestamp value is set
*/

static int
check_insert_fields(THD *thd,TABLE *table,List<Item> &fields,
		    List<Item> &values, ulong counter)
{
  if (fields.elements == 0 && values.elements != 0)
  {
    if (values.elements != table->fields)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
    if (grant_option &&
	check_grant_all_columns(thd,INSERT_ACL,table))
      return -1;
    table->time_stamp=0;			// This should be saved
  }
  else
  {						// Part field list
    if (fields.elements != values.elements)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.name=table->table_name;
    table_list.table=table;
    table_list.grant=table->grant;

    thd->dupp_field=0;
    if (setup_tables(&table_list) || setup_fields(thd,&table_list,fields,1,0))
      return -1;
    if (thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE,MYF(0), thd->dupp_field->field_name);
      return -1;
    }
    if (table->timestamp_field &&	// Don't set timestamp if used
	table->timestamp_field->query_id == thd->query_id)
      table->time_stamp=0;		// This should be saved
  }
 // For the values we need select_priv
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  return 0;
}


int mysql_insert(THD *thd,TABLE_LIST *table_list, List<Item> &fields,
		 List<List_item> &values_list,enum_duplicates duplic,
		 thr_lock_type lock_type)
{
  int error;
  bool log_on= ((thd->options & OPTION_UPDATE_LOG) ||
		!(thd->master_access & PROCESS_ACL));
  bool using_transactions;
  uint value_count;
  uint save_time_stamp;
  ulong counter = 1;
  ulonglong id;
  COPY_INFO info;
  TABLE *table;
  List_iterator<List_item> its(values_list);
  List_item *values;
  char *query=thd->query;
  DBUG_ENTER("mysql_insert");

  /*
    in safe mode or with skip-new change delayed insert to be regular
    if we are told to replace duplicates, the insert cannot be concurrent
    delayed insert changed to regular in slave thread
   */
  if ((lock_type == TL_WRITE_DELAYED &&
       ((specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) ||
	thd->slave_thread)) ||
      (lock_type == TL_WRITE_CONCURRENT_INSERT && duplic == DUP_REPLACE))
    lock_type=TL_WRITE;

  if (lock_type == TL_WRITE_DELAYED)
  {
    if (thd->locked_tables)
    {
      if (find_locked_table(thd,
			    table_list->db ? table_list->db : thd->db,
			    table_list->real_name))
      {
	my_printf_error(ER_DELAYED_INSERT_TABLE_LOCKED,
			ER(ER_DELAYED_INSERT_TABLE_LOCKED),
			MYF(0), table_list->real_name);
	DBUG_RETURN(-1);
      }
    }
    if (!(table = delayed_get_table(thd,table_list)) && !thd->fatal_error)
      table = open_ltable(thd,table_list,lock_type=thd->update_lock_default);
  }
  else
    table = open_ltable(thd,table_list,lock_type);
  if (!table)
    DBUG_RETURN(-1);
  thd->proc_info="init";
  thd->used_tables=0;
  save_time_stamp=table->time_stamp;
  values= its++;
  if (check_insert_fields(thd,table,fields,*values,1) ||
      setup_tables(table_list) || setup_fields(thd,table_list,*values,0,0))
  {
    table->time_stamp=save_time_stamp;
    goto abort;
  }
  value_count= values->elements;
  while ((values = its++))
  {
    counter++;
    if (values->elements != value_count)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      table->time_stamp=save_time_stamp;
      goto abort;
    }
    if (setup_fields(thd,table_list,*values,0,0))
    {
      table->time_stamp=save_time_stamp;
      goto abort;
    }
  }
  its.rewind ();
  /*
  ** Fill in the given fields and dump it to the table file
  */

  info.records=info.deleted=info.copied=0;
  info.handle_duplicates=duplic;
  // Don't count warnings for simple inserts
  if (values_list.elements > 1 || (thd->options & OPTION_WARNINGS))
    thd->count_cuted_fields = 1;
  thd->cuted_fields = 0L;
  table->next_number_field=table->found_next_number_field;

  error=0;
  id=0;
  thd->proc_info="update";
  if (duplic == DUP_IGNORE || duplic == DUP_REPLACE)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  while ((values = its++))
  {
    if (fields.elements || !value_count)
    {
      restore_record(table,2);			// Get empty record
      if (fill_record(fields,*values) || check_null_fields(thd,table))
      {
	if (values_list.elements != 1)
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }
    else
    {
      if (thd->used_tables)			// Column used in values()
	restore_record(table,2);		// Get empty record
      else
	table->record[0][0]=table->record[2][0]; // Fix delete marker
      if (fill_record(table->field,*values))
      {
	if (values_list.elements != 1)
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }
    if (lock_type == TL_WRITE_DELAYED)
    {
      error=write_delayed(thd,table,duplic,query, thd->query_length, log_on);
      query=0;
    }
    else
      error=write_record(table,&info);
    if (error)
      break;
    /*
      If auto_increment values are used, save the first one
       for LAST_INSERT_ID() and for the update log.
       We can't use insert_id() as we don't want to touch the
       last_insert_id_used flag.
    */
    if (! id && thd->insert_id_used)
    {						// Get auto increment value
      id= thd->last_insert_id;
    }
  }
  if (lock_type == TL_WRITE_DELAYED)
  {
    if (!error)
    {
      id=0;					// No auto_increment id
      info.copied=values_list.elements;
      end_delayed_insert(thd);
    }
  }
  else
  {
    if (id && values_list.elements != 1)
      thd->insert_id(id);			// For update log
    else if (table->next_number_field)
      id=table->next_number_field->val_int();	// Return auto_increment value
    using_transactions=table->file->has_transactions();
    if ((info.copied || info.deleted) && (error <= 0 || !using_transactions))
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	Query_log_event qinfo(thd, thd->query, using_transactions);
	if (mysql_bin_log.write(&qinfo) && using_transactions)
	  error=1;
      }
      if (!using_transactions)
	thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
    }
    if (using_transactions)
      error=ha_autocommit_or_rollback(thd,error);
    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock=0;
    }
  }
  thd->proc_info="end";
  table->time_stamp=save_time_stamp;		// Restore auto timestamp ptr
  table->next_number_field=0;
  thd->count_cuted_fields=0;
  thd->next_insert_id=0;			// Reset this if wrongly used
  if (duplic == DUP_IGNORE || duplic == DUP_REPLACE)
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  if (error)
    goto abort;

  if (values_list.elements == 1 && (!(thd->options & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
    send_ok(&thd->net,info.copied+info.deleted,id);
  else
  {
    char buff[160];
    if (duplic == DUP_IGNORE)
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.records-info.copied,
	      thd->cuted_fields);
    else
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.deleted,
	      thd->cuted_fields);
    ::send_ok(&thd->net,info.copied+info.deleted,0L,buff);
  }
  DBUG_RETURN(0);

abort:
  if (lock_type == TL_WRITE_DELAYED)
    end_delayed_insert(thd);
  DBUG_RETURN(-1);
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(TABLE *table,uint keynr)
{
  while (++keynr < table->keys)
    if (table->key_info[keynr].flags & HA_NOSAME)
      return 0;
  return 1;
}


/*
** Write a record to table with optional deleting of conflicting records
*/


int write_record(TABLE *table,COPY_INFO *info)
{
  int error;
  char *key=0;
 
  info->records++;
  if (info->handle_duplicates == DUP_REPLACE)
  {
    while ((error=table->file->write_row(table->record[0])))
    {
      if (error != HA_WRITE_SKIPP)
	goto err;
      uint key_nr;
      if ((int) (key_nr = table->file->get_dup_key(error)) < 0)
      {
	error=HA_WRITE_SKIPP;			/* Database can't find key */
	goto err;
      }
      /*
	Don't allow REPLACE to replace a row when a auto_increment column
	was used.  This ensures that we don't get a problem when the
	whole range of the key has been used.
      */
      if (table->next_number_field && key_nr == table->next_number_index &&
	  table->file->auto_increment_column_changed)
	goto err;
      if (table->file->option_flag() & HA_DUPP_POS)
      {
	if (table->file->rnd_pos(table->record[1],table->file->dupp_ref))
	  goto err;
      }
      else
      {
	if (table->file->extra(HA_EXTRA_FLUSH_CACHE)) /* Not neaded with NISAM */
	{
	  error=my_errno;
	  goto err;
	}
	
	if (!key)
	{
	  if (!(key=(char*) my_safe_alloca(table->max_unique_length,
					   MAX_KEY_LENGTH)))
	  {
	    error=ENOMEM;
	    goto err;
	  }
	}
	key_copy((byte*) key,table,key_nr,0);
	if ((error=(table->file->index_read_idx(table->record[1],key_nr,
						(byte*) key,
						table->key_info[key_nr].key_length,
						HA_READ_KEY_EXACT))))
	  goto err;
      }
      if (last_uniq_key(table,key_nr))
      {
	if ((error=table->file->update_row(table->record[1],table->record[0])))
	  goto err;
	info->deleted++;
	break;					/* Update logfile and count */
      }
      else if ((error=table->file->delete_row(table->record[1])))
	goto err;
      info->deleted++;
    }
    info->copied++;
  }
  else if ((error=table->file->write_row(table->record[0])))
  {
    if (info->handle_duplicates != DUP_IGNORE ||
	(error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE))
      goto err;
  }
  else
    info->copied++;
  if (key)
    my_safe_afree(key,table->max_unique_length,MAX_KEY_LENGTH);
  return 0;

err:
  if (key)
    my_afree(key);
  table->file->print_error(error,MYF(0));
  return 1;
}


/******************************************************************************
	Check that all fields with arn't null_fields are used
	if DONT_USE_DEFAULT_FIELDS isn't defined use default value for not
	set fields.
******************************************************************************/

static int check_null_fields(THD *thd __attribute__((unused)),
			     TABLE *entry __attribute__((unused)))
{
#ifdef DONT_USE_DEFAULT_FIELDS
  for (Field **field=entry->field ; *field ; field++)
  {
    if ((*field)->query_id != thd->query_id && !(*field)->maybe_null() &&
	*field != entry->timestamp_field &&
	*field != entry->next_number_field)
    {
      my_printf_error(ER_BAD_NULL_ERROR, ER(ER_BAD_NULL_ERROR),MYF(0),
		      (*field)->field_name);
      return 1;
    }
  }
#endif
  return 0;
}

/*****************************************************************************
** Handling of delayed inserts
**
** A thread is created for each table that one uses with the DELAYED
** attribute.
*****************************************************************************/

class delayed_row :public ilink {
public:
  char *record,*query;
  enum_duplicates dup;
  time_t start_time;
  bool query_start_used,last_insert_id_used,insert_id_used,log_query;
  ulonglong last_insert_id;
  ulong time_stamp;
  uint query_length;

  delayed_row(enum_duplicates dup_arg, bool log_query_arg)
    :record(0),query(0),dup(dup_arg),log_query(log_query_arg) {}
  ~delayed_row()
  {
    x_free(record);
  }
};


class delayed_insert :public ilink {
  uint locks_in_memory;
public:
  THD thd;
  TABLE *table;
  pthread_mutex_t mutex;
  pthread_cond_t cond,cond_client;
  volatile uint tables_in_use,stacked_inserts;
  volatile bool status,dead;
  COPY_INFO info;
  I_List<delayed_row> rows;
  uint group_count;
  TABLE_LIST *table_list;			// Argument

  delayed_insert()
    :locks_in_memory(0),
     table(0),tables_in_use(0),stacked_inserts(0), status(0), dead(0),
     group_count(0)
  {
    thd.user=thd.priv_user=(char*) delayed_user;
    thd.host=(char*) localhost;
    thd.current_tablenr=0;
    thd.version=refresh_version;
    thd.command=COM_DELAYED_INSERT;

    bzero((char*) &thd.net,sizeof(thd.net));	// Safety
    thd.system_thread=1;
    bzero((char*) &info,sizeof(info));
    pthread_mutex_init(&mutex,MY_MUTEX_INIT_FAST);
    pthread_cond_init(&cond,NULL);
    pthread_cond_init(&cond_client,NULL);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    delayed_insert_threads++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  ~delayed_insert()
  {
    /* The following is not really neaded, but just for safety */
    delayed_row *row;
    while ((row=rows.get()))
      delete row;
    pthread_mutex_destroy(&mutex);
    if (table)
      close_thread_tables(&thd);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd.unlink();				// Must be unlinked under lock
    x_free(thd.query);
    thd.user=thd.host=0;
    thread_count--;
    delayed_insert_threads--;
    VOID(pthread_cond_broadcast(&COND_thread_count)); /* Tell main we are ready */
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }

  /* The following is for checking when we can delete ourselves */
  inline void lock()
  {
    locks_in_memory++;				// Assume LOCK_delay_insert
  }
  void unlock()
  {
    pthread_mutex_lock(&LOCK_delayed_insert);
    if (!--locks_in_memory)
    {
      pthread_mutex_lock(&mutex);
      if (thd.killed && ! stacked_inserts && ! tables_in_use)
      {
	pthread_cond_signal(&cond);
	status=1;
      }
      pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_unlock(&LOCK_delayed_insert);
  }
  inline uint lock_count() { return locks_in_memory; }

  TABLE* get_local_table(THD* client_thd);
  bool handle_inserts(void);
};


I_List<delayed_insert> delayed_threads;


delayed_insert *find_handler(THD *thd, TABLE_LIST *table_list)
{
  thd->proc_info="waiting for delay_list";
  pthread_mutex_lock(&LOCK_delayed_insert);	// Protect master list
  I_List_iterator<delayed_insert> it(delayed_threads);
  delayed_insert *tmp;
  while ((tmp=it++))
  {
    if (!strcmp(tmp->thd.db,table_list->db) &&
	!strcmp(table_list->real_name,tmp->table->real_name))
    {
      tmp->lock();
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_delayed_insert); // For unlink from list
  return tmp;
}


static TABLE *delayed_get_table(THD *thd,TABLE_LIST *table_list)
{
  int error;
  delayed_insert *tmp;
  DBUG_ENTER("delayed_get_table");

  if (!table_list->db)
    table_list->db=thd->db;

  /* no match; create a new thread to handle the table */
  if (!(tmp=find_handler(thd,table_list)))
  {
    thd->proc_info="Creating delayed handler";
    pthread_mutex_lock(&LOCK_delayed_create);
    if (!(tmp=find_handler(thd,table_list)))	// Was just created
    {
      if (!(tmp=new delayed_insert()))
      {
	thd->fatal_error=1;
	my_error(ER_OUTOFMEMORY,MYF(0),sizeof(delayed_insert));
	pthread_mutex_unlock(&LOCK_delayed_create);
	DBUG_RETURN(0);
      }
      pthread_mutex_lock(&LOCK_thread_count);
      thread_count++;
      pthread_mutex_unlock(&LOCK_thread_count);
      if (!(tmp->thd.db=my_strdup(table_list->db,MYF(MY_WME))) ||
	  !(tmp->thd.query=my_strdup(table_list->real_name,MYF(MY_WME))))
      {
	delete tmp;
	thd->fatal_error=1;
	my_error(ER_OUT_OF_RESOURCES,MYF(0));
	pthread_mutex_unlock(&LOCK_delayed_create);
	DBUG_RETURN(0);
      }
      tmp->table_list=table_list;			// Needed to open table
      tmp->lock();
      pthread_mutex_lock(&tmp->mutex);
      if ((error=pthread_create(&tmp->thd.real_id,&connection_attrib,
				handle_delayed_insert,(void*) tmp)))
      {
	DBUG_PRINT("error",
		   ("Can't create thread to handle delayed insert (error %d)",
		    error));
	pthread_mutex_unlock(&tmp->mutex);
	tmp->unlock();
	delete tmp;
	thd->fatal_error=1;
	pthread_mutex_unlock(&LOCK_delayed_create);
	net_printf(&thd->net,ER_CANT_CREATE_THREAD,error);
	DBUG_RETURN(0);
      }

      /* Wait until table is open */
      thd->proc_info="waiting for handler open";
      while (!tmp->thd.killed && !tmp->table && !thd->killed)
      {
	pthread_cond_wait(&tmp->cond_client,&tmp->mutex);
      }
      pthread_mutex_unlock(&tmp->mutex);
      thd->proc_info="got old table";
      if (tmp->thd.killed)
      {
	if (tmp->thd.fatal_error)
	{
	  /* Copy error message and abort */
	  thd->fatal_error=1;
	  strmov(thd->net.last_error,tmp->thd.net.last_error);
	  thd->net.last_errno=tmp->thd.net.last_errno;
	}
	tmp->unlock();
	pthread_mutex_unlock(&LOCK_delayed_create);
	DBUG_RETURN(0);				// Continue with normal insert
      }
      if (thd->killed)
      {
	tmp->unlock();
	pthread_mutex_unlock(&LOCK_delayed_create);
	DBUG_RETURN(0);
      }
    }
    pthread_mutex_unlock(&LOCK_delayed_create);
  }

  pthread_mutex_lock(&tmp->mutex);
  TABLE *table=tmp->get_local_table(thd);
  pthread_mutex_unlock(&tmp->mutex);
  tmp->unlock();
  if (table)
    thd->di=tmp;
  else if (tmp->thd.fatal_error)
    thd->fatal_error=1;
  DBUG_RETURN((table_list->table=table));
}


/*
  As we can't let many threads modify the same TABLE structure, we create
  an own structure for each tread.  This includes a row buffer to save the
  column values and new fields that points to the new row buffer.
  The memory is allocated in the client thread and is freed automaticly.
*/

TABLE *delayed_insert::get_local_table(THD* client_thd)
{
  my_ptrdiff_t adjust_ptrs;
  Field **field,**org_field, *found_next_number_field;
  TABLE *copy;

  /* First request insert thread to get a lock */
  status=1;
  tables_in_use++;
  if (!thd.lock)				// Table is not locked
  {
    client_thd->proc_info="waiting for handler lock";
    pthread_cond_signal(&cond);			// Tell handler to lock table
    while (!dead && !thd.lock && ! client_thd->killed)
    {
      pthread_cond_wait(&cond_client,&mutex);
    }
    client_thd->proc_info="got handler lock";
    if (client_thd->killed)
      goto error;
    if (dead)
    {
      strmov(client_thd->net.last_error,thd.net.last_error);
      client_thd->net.last_errno=thd.net.last_errno;
      goto error;
    }
  }

  client_thd->proc_info="allocating local table";
  copy= (TABLE*) sql_alloc(sizeof(*copy)+
			   (table->fields+1)*sizeof(Field**)+
			   table->reclength);
  if (!copy)
    goto error;
  *copy= *table;
  bzero((char*) &copy->name_hash,sizeof(copy->name_hash)); // No name hashing
  /* We don't need to change the file handler here */

  field=copy->field=(Field**) (copy+1);
  copy->record[0]=(byte*) (field+table->fields+1);
  memcpy((char*) copy->record[0],(char*) table->record[0],table->reclength);

  /* Make a copy of all fields */

  adjust_ptrs=PTR_BYTE_DIFF(copy->record[0],table->record[0]);

  found_next_number_field=table->found_next_number_field;
  for (org_field=table->field ; *org_field ; org_field++,field++)
  {
    if (!(*field= (*org_field)->new_field(copy)))
      return 0;
    (*field)->move_field(adjust_ptrs);		// Point at copy->record[0]
    if (*org_field == found_next_number_field)
      (*field)->table->found_next_number_field= *field;
  }
  *field=0;

  /* Adjust timestamp */
  if (table->timestamp_field)
  {
    /* Restore offset as this may have been reset in handle_inserts */
    copy->time_stamp=table->timestamp_field->offset()+1;
    copy->timestamp_field=
      (Field_timestamp*) copy->field[table->timestamp_field_offset];
  }

  /* _rowid is not used with delayed insert */
  copy->rowid_field=0;
  return copy;

  /* Got fatal error */
 error:
  tables_in_use--;
  status=1;
  pthread_cond_signal(&cond);			// Inform thread about abort
  return 0;
}


/* Put a question in queue */

static int write_delayed(THD *thd,TABLE *table,enum_duplicates duplic,
			 char *query, uint query_length, bool log_on)
{
  delayed_row *row=0;
  delayed_insert *di=thd->di;
  DBUG_ENTER("write_delayed");

  thd->proc_info="waiting for handler insert";
  pthread_mutex_lock(&di->mutex);
  while (di->stacked_inserts >= delayed_queue_size && !thd->killed)
    pthread_cond_wait(&di->cond_client,&di->mutex);
  thd->proc_info="storing row into queue";

  if (thd->killed || !(row= new delayed_row(duplic, log_on)))
    goto err;

  if (!query)
    query_length=0;
  if (!(row->record= (char*) my_malloc(table->reclength+query_length+1,
				       MYF(MY_WME))))
    goto err;
  memcpy(row->record,table->record[0],table->reclength);
  if (query_length)
  {
    row->query=row->record+table->reclength;
    memcpy(row->query,query,query_length+1);
  }
  row->query_length=		query_length;
  row->start_time=		thd->start_time;
  row->query_start_used=	thd->query_start_used;
  row->last_insert_id_used=	thd->last_insert_id_used;
  row->insert_id_used=		thd->insert_id_used;
  row->last_insert_id=		thd->last_insert_id;
  row->time_stamp=		table->time_stamp;

  di->rows.push_back(row);
  di->stacked_inserts++;
  di->status=1;
  if (table->blob_fields)
    unlink_blobs(table);
  pthread_cond_signal(&di->cond);

  thread_safe_increment(delayed_rows_in_use,&LOCK_delayed_status);
  pthread_mutex_unlock(&di->mutex);
  DBUG_RETURN(0);

 err:
  delete row;
  pthread_mutex_unlock(&di->mutex);
  DBUG_RETURN(1);
}


static void end_delayed_insert(THD *thd)
{
  DBUG_ENTER("end_delayed_insert");
  delayed_insert *di=thd->di;
  pthread_mutex_lock(&di->mutex);
  DBUG_PRINT("info",("tables in use: %d",di->tables_in_use));
  if (!--di->tables_in_use || di->thd.killed)
  {						// Unlock table
    di->status=1;
    pthread_cond_signal(&di->cond);
  }
  pthread_mutex_unlock(&di->mutex);
  DBUG_VOID_RETURN;
}


/* We kill all delayed threads when doing flush-tables */

void kill_delayed_threads(void)
{
  VOID(pthread_mutex_lock(&LOCK_delayed_insert)); // For unlink from list

  I_List_iterator<delayed_insert> it(delayed_threads);
  delayed_insert *tmp;
  while ((tmp=it++))
  {
    pthread_mutex_lock(&tmp->mutex);
    tmp->thd.killed=1;
    if (tmp->thd.mysys_var)
    {
      pthread_mutex_lock(&tmp->thd.mysys_var->mutex);
      if (tmp->thd.mysys_var->current_mutex)
      {
	if (&tmp->mutex != tmp->thd.mysys_var->current_mutex)
	  pthread_mutex_lock(tmp->thd.mysys_var->current_mutex);
	pthread_cond_broadcast(tmp->thd.mysys_var->current_cond);
	if (&tmp->mutex != tmp->thd.mysys_var->current_mutex)
	  pthread_mutex_unlock(tmp->thd.mysys_var->current_mutex);
      }
      pthread_mutex_unlock(&tmp->thd.mysys_var->mutex);
    }
    pthread_mutex_unlock(&tmp->mutex);
  }
  VOID(pthread_mutex_unlock(&LOCK_delayed_insert)); // For unlink from list
}


/*
 * Create a new delayed insert thread
*/

static pthread_handler_decl(handle_delayed_insert,arg)
{
  delayed_insert *di=(delayed_insert*) arg;
  THD *thd= &di->thd;

  pthread_detach_this_thread();
  /* Add thread to THD list so that's it's visible in 'show processlist' */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id=thread_id++;
  thd->end_time();
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);

  pthread_mutex_lock(&di->mutex);
#ifndef __WIN__	/* Win32 calls this in pthread_create */
  if (my_thread_init())
  {
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto end;
  }
#endif

  DBUG_ENTER("handle_delayed_insert");
  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    thd->fatal_error=1;
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto end;
  }
  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#ifndef __WIN__
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  /* open table */

  if (!(di->table=open_ltable(thd,di->table_list,TL_WRITE_DELAYED)))
  {
    thd->fatal_error=1;				// Abort waiting inserts
    goto end;
  }
  if (di->table->file->has_transactions())
  {
    thd->fatal_error=1;
    my_error(ER_ILLEGAL_HA, MYF(0), di->table_list->real_name);
    goto end;
  }
  di->table->copy_blobs=1;

  /* One can now use this */
  pthread_mutex_lock(&LOCK_delayed_insert);
  delayed_threads.append(di);
  pthread_mutex_unlock(&LOCK_delayed_insert);

  /* Tell client that the thread is initialized */
  pthread_cond_signal(&di->cond_client);

  /* Now wait until we get an insert or lock to handle */
  /* We will not abort as long as a client thread uses this thread */

  for (;;)
  {
    if (thd->killed)
    {
      uint lock_count;
      /*
	Remove this from delay insert list so that no one can request a
	table from this
      */
      pthread_mutex_unlock(&di->mutex);
      pthread_mutex_lock(&LOCK_delayed_insert);
      di->unlink();
      lock_count=di->lock_count();
      pthread_mutex_unlock(&LOCK_delayed_insert);
      pthread_mutex_lock(&di->mutex);
      if (!lock_count && !di->tables_in_use && !di->stacked_inserts)
	break;					// Time to die
    }

    if (!di->status && !di->stacked_inserts)
    {
      struct timespec abstime;
#if defined(HAVE_TIMESPEC_TS_SEC)
      abstime.ts_sec=time((time_t*) 0)+(time_t) delayed_insert_timeout;
      abstime.ts_nsec=0;
#elif defined(__WIN__)
      abstime.tv_sec=time((time_t*) 0)+(time_t) delayed_insert_timeout;
      abstime.tv_nsec=0;
#else
      struct timeval tv;
      gettimeofday(&tv,0);
      abstime.tv_sec=tv.tv_sec+(time_t) delayed_insert_timeout;
      abstime.tv_nsec=tv.tv_usec*1000;
#endif

      /* Information for pthread_kill */
      pthread_mutex_lock(&di->thd.mysys_var->mutex);
      di->thd.mysys_var->current_mutex= &di->mutex;
      di->thd.mysys_var->current_cond= &di->cond;
      pthread_mutex_unlock(&di->thd.mysys_var->mutex);
      di->thd.proc_info=0;

      DBUG_PRINT("info",("Waiting for someone to insert rows"));
      for ( ; ;)
      {
	int error;
#if (defined(HAVE_BROKEN_COND_TIMEDWAIT) || defined(HAVE_LINUXTHREADS))
	error=pthread_cond_wait(&di->cond,&di->mutex);
#else
	error=pthread_cond_timedwait(&di->cond,&di->mutex,&abstime);
#ifdef EXTRA_DEBUG
	if (error && error != EINTR)
	{
	  fprintf(stderr, "Got error %d from pthread_cond_timedwait\n",error);
	  DBUG_PRINT("error",("Got error %d from pthread_cond_timedwait",
			      error));
	}
#endif
#endif
	if (thd->killed || di->status)
	  break;
	if (error == ETIME || error == ETIMEDOUT)
	{
	  thd->killed=1;
	  break;
	}
      }
      pthread_mutex_lock(&di->thd.mysys_var->mutex);
      di->thd.mysys_var->current_mutex= 0;
      di->thd.mysys_var->current_cond= 0;
      pthread_mutex_unlock(&di->thd.mysys_var->mutex);
    }

    if (di->tables_in_use && ! thd->lock)
    {
      /* request for new delayed insert */
      if (!(thd->lock=mysql_lock_tables(thd,&di->table,1)))
      {
	di->dead=thd->killed=1;			// Fatal error
      }
      pthread_cond_broadcast(&di->cond_client);
    }
    if (di->stacked_inserts)
    {
      if (di->handle_inserts())
      {
	di->dead=thd->killed=1;			// Some fatal error
      }
    }
    di->status=0;
    if (!di->stacked_inserts && !di->tables_in_use && thd->lock)
    {
      /* No one is doing a insert delayed;
	 Unlock it so that other threads can use it */
      MYSQL_LOCK *lock=thd->lock;
      thd->lock=0;
      pthread_mutex_unlock(&di->mutex);
      mysql_unlock_tables(thd, lock);
      di->group_count=0;
      pthread_mutex_lock(&di->mutex);
    }
    if (di->tables_in_use)
      pthread_cond_broadcast(&di->cond_client); // If waiting clients
  }

end:
  /*
    di should be unlinked from the thread handler list and have no active
    clients
  */

  close_thread_tables(thd);			// Free the table
  di->table=0;
  di->dead=thd->killed=1;			// If error
  pthread_cond_broadcast(&di->cond_client);	// Safety
  pthread_mutex_unlock(&di->mutex);

  pthread_mutex_lock(&LOCK_delayed_create);	// Because of delayed_get_table
  pthread_mutex_lock(&LOCK_delayed_insert);	
  delete di;
  pthread_mutex_unlock(&LOCK_delayed_insert);
  pthread_mutex_unlock(&LOCK_delayed_create);  

  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);
}


/* Remove pointers from temporary fields to allocated values */

static void unlink_blobs(register TABLE *table)
{
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->clear_temporary();
  }
}

/* Free blobs stored in current row */

static void free_delayed_insert_blobs(register TABLE *table)
{
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
    {
      char *str;
      ((Field_blob *) (*ptr))->get_ptr(&str);
      my_free(str,MYF(MY_ALLOW_ZERO_PTR));
      ((Field_blob *) (*ptr))->reset();
    }
  }
}


bool delayed_insert::handle_inserts(void)
{
  int error;
  uint max_rows;
  bool using_ignore=0;
  delayed_row *row;
  DBUG_ENTER("handle_inserts");

  /* Allow client to insert new rows */
  pthread_mutex_unlock(&mutex);

  table->next_number_field=table->found_next_number_field;

  thd.proc_info="upgrading lock";
  if (thr_upgrade_write_delay_lock(*thd.lock->locks))
  {
    /* This can only happen if thread is killed by shutdown */
    sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),table->real_name);
    goto err;
  }

  thd.proc_info="insert";
  max_rows=delayed_insert_limit;
  if (thd.killed || table->version != refresh_version)
  {
    thd.killed=1;
    max_rows= ~0;				// Do as much as possible
  }

  table->file->extra(HA_EXTRA_WRITE_CACHE);
  pthread_mutex_lock(&mutex);
  while ((row=rows.get()))
  {
    stacked_inserts--;
    pthread_mutex_unlock(&mutex);
    memcpy(table->record[0],row->record,table->reclength);

    thd.start_time=row->start_time;
    thd.query_start_used=row->query_start_used;
    thd.last_insert_id=row->last_insert_id;
    thd.last_insert_id_used=row->last_insert_id_used;
    thd.insert_id_used=row->insert_id_used;
    table->time_stamp=row->time_stamp;

    info.handle_duplicates= row->dup;
    if (info.handle_duplicates == DUP_IGNORE ||
	info.handle_duplicates == DUP_REPLACE)
    {
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
      using_ignore=1;
    }
    thd.net.last_errno = 0; // reset error for binlog
    if (write_record(table,&info))
    {
      info.error++;				// Ignore errors
      thread_safe_increment(delayed_insert_errors,&LOCK_delayed_status);
      row->log_query = 0;
    }
    if (using_ignore)
    {
      using_ignore=0;
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    }
    if (row->query && row->log_query)
    {
      mysql_update_log.write(&thd,row->query, row->query_length);
      if (mysql_bin_log.is_open())
      {
	thd.query_length = row->query_length;
	Query_log_event qinfo(&thd, row->query);
	mysql_bin_log.write(&qinfo);
      }
    }
    if (table->blob_fields)
      free_delayed_insert_blobs(table);
    thread_safe_sub(delayed_rows_in_use,1,&LOCK_delayed_status);
    thread_safe_increment(delayed_insert_writes,&LOCK_delayed_status);
    pthread_mutex_lock(&mutex);

    delete row;
    /* Let READ clients do something once in a while */
    if (group_count++ == max_rows)
    {
      group_count=0;
      if (stacked_inserts || tables_in_use)	// Let these wait a while
      {
	if (tables_in_use)
	  pthread_cond_broadcast(&cond_client); // If waiting clients
	thd.proc_info="reschedule";
	pthread_mutex_unlock(&mutex);
	if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
	{
	  /* This should never happen */
	  table->file->print_error(error,MYF(0));
	  sql_print_error("%s",thd.net.last_error);
	  goto err;
	}
	if (thr_reschedule_write_lock(*thd.lock->locks))
	{
	  /* This should never happen */
	  sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),table->real_name);
	}
	table->file->extra(HA_EXTRA_WRITE_CACHE);
	pthread_mutex_lock(&mutex);
	thd.proc_info="insert";
      }
      if (tables_in_use)
	pthread_cond_broadcast(&cond_client);	// If waiting clients
    }
  }

  thd.proc_info=0;
  table->next_number_field=0;
  pthread_mutex_unlock(&mutex);
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {						// This shouldn't happen
    table->file->print_error(error,MYF(0));
    sql_print_error("%s",thd.net.last_error);
    goto err;
  }
  pthread_mutex_lock(&mutex);
  DBUG_RETURN(0);

 err:
  /* Remove all not used rows */
  while ((row=rows.get()))
  {
    delete row;
    thread_safe_increment(delayed_insert_errors,&LOCK_delayed_status);
    stacked_inserts--;
  }
  thread_safe_increment(delayed_insert_errors, &LOCK_delayed_status);
  pthread_mutex_lock(&mutex);
  DBUG_RETURN(1);
}



/***************************************************************************
** store records in INSERT ... SELECT *
***************************************************************************/

int
select_insert::prepare(List<Item> &values)
{
  DBUG_ENTER("select_insert::prepare");

  save_time_stamp=table->time_stamp;
  if (check_insert_fields(thd,table,*fields,values,1))
    DBUG_RETURN(1);

  restore_record(table,2);			// Get empty record
  table->next_number_field=table->found_next_number_field;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0;
	if (info.handle_duplicates != DUP_REPLACE)
		table->file->extra(HA_EXTRA_WRITE_CACHE);
	if (info.handle_duplicates == DUP_IGNORE ||
			info.handle_duplicates == DUP_REPLACE)
		table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
	table->file->deactivate_non_unique_index((ha_rows) 0);
  DBUG_RETURN(0);
}

select_insert::~select_insert()
{
  if (table)
  {
    if (save_time_stamp)
      table->time_stamp=save_time_stamp;
    table->next_number_field=0;
    table->file->extra(HA_EXTRA_RESET);
  }
  thd->count_cuted_fields=0;
}


bool select_insert::send_data(List<Item> &values)
{
  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    return 0;
  }
  if (fields->elements)
    fill_record(*fields,values);
  else
    fill_record(table->field,values);
  if (write_record(table,&info))
    return 1;
  if (table->next_number_field)		// Clear for next record
  {
    table->next_number_field->reset();
    if (! last_insert_id && thd->insert_id_used)
      last_insert_id=thd->insert_id();
  }
  return 0;
}


void select_insert::send_error(uint errcode,const char *err)
{
  ::send_error(&thd->net,errcode,err);
  table->file->extra(HA_EXTRA_NO_CACHE);
  table->file->activate_all_index(thd);
  ha_rollback(thd);
}


bool select_insert::send_eof()
{
  int error,error2;
  if (!(error=table->file->extra(HA_EXTRA_NO_CACHE)))
    error=table->file->activate_all_index(thd);
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  if ((error2=ha_autocommit_or_rollback(thd,error)) && ! error)
    error=error2;

  if (error)
  {
    table->file->print_error(error,MYF(0));
    ::send_error(&thd->net);
    return 1;
  }
  else
  {
    char buff[160];
    if (info.handle_duplicates == DUP_IGNORE)
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.records-info.copied,
	      thd->cuted_fields);
    else
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.deleted,
	      thd->cuted_fields);
    if (last_insert_id)
      thd->insert_id(last_insert_id);		// For update log
		if (!unions)
			::send_ok(&thd->net,info.copied,last_insert_id,buff);
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query,
			    table->file->has_transactions());
      mysql_bin_log.write(&qinfo);
    }
    return 0;
  }
}


/***************************************************************************
** CREATE TABLE (SELECT) ...
***************************************************************************/

int
select_create::prepare(List<Item> &values)
{
  DBUG_ENTER("select_create::prepare");

  table=create_table_from_items(thd, create_info, db, name,
				extra_fields, keys, &values, &lock);
  if (!table)
    DBUG_RETURN(-1);				// abort() deletes table

  /* First field to copy */
  field=table->field+table->fields - values.elements;

  save_time_stamp=table->time_stamp;
  if (table->timestamp_field)			// Don't set timestamp if used
  {
    table->timestamp_field->set_time();
    table->time_stamp=0;			// This should be saved
  }
  table->next_number_field=table->found_next_number_field;

  restore_record(table,2);			// Get empty record
  thd->count_cuted_fields=1;			// count warnings
  thd->cuted_fields=0;
  if (info.handle_duplicates == DUP_IGNORE ||
      info.handle_duplicates == DUP_REPLACE)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  table->file->deactivate_non_unique_index((ha_rows) 0);
  DBUG_RETURN(0);
}


bool select_create::send_data(List<Item> &values)
{
  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    return 0;
  }
  fill_record(field,values);
  if (write_record(table,&info))
    return 1;
  if (table->next_number_field)		// Clear for next record
  {
    table->next_number_field->reset();
    if (! last_insert_id && thd->insert_id_used)
      last_insert_id=thd->insert_id();
  }
  return 0;
}

extern HASH open_cache;

bool select_create::send_eof()
{
  bool tmp=select_insert::send_eof();
  if (tmp)
    abort();
  else
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    VOID(pthread_mutex_lock(&LOCK_open));
    mysql_unlock_tables(thd, lock);
    if (!table->tmp_table)
      hash_delete(&open_cache,(byte*) table);
    lock=0; 
		if (!unions)
			table=0;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  return tmp;
}

void select_create::abort()
{
  VOID(pthread_mutex_lock(&LOCK_open));
  if (lock)
  {
    mysql_unlock_tables(thd, lock);
    lock=0;
  }
  if (table)
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    enum db_type table_type=table->db_type;
    if (!table->tmp_table)
      hash_delete(&open_cache,(byte*) table);
    quick_rm_table(table_type,db,name);
    table=0;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List_iterator<List_item>;
template class I_List<delayed_insert>;
template class I_List_iterator<delayed_insert>;
template class I_List<delayed_row>;
#endif
