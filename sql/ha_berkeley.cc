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


/*
  TODO:
  - Not compressed keys should use cmp_fix_length_key
  - Don't automaticly pack all string keys (To do this we need to modify
    CREATE TABLE so that one can use the pack_keys argument per key).
  - An argument to pack_key that we don't want compression.
  - DB_DBT_USERMEM should be used for fixed length tables
    We will need an updated Berkeley DB version for this.
  - Killing threads that has got a 'deadlock'
  - SHOW TABLE STATUS should give more information about the table.
  - Get a more accurate count of the number of rows (estimate_number_of_rows()).
    We could store the found number of rows when the table is scanned and
    then increment the counter for each attempted write.
  - We will need a manager thread that calls flush_logs, removes old
    logs and makes checkpoints at given intervals.
  - When not using UPDATE IGNORE, don't make a sub transaction but abort
    the main transaction on errors.
  - Handling of drop table during autocommit=0 ?
    (Should we just give an error in this case if there is a pending
    transaction ?)
  - When using ALTER TABLE IGNORE, we should not start an transaction, but do
    everything wthout transactions.
  - When we do rollback, we need to subtract the number of changed rows
    from the updated tables.

  Testing of:
  - Mark tables that participate in a transaction so that they are not
    closed during the transaction.  We need to test what happens if
    MySQL closes a table that is updated by a not commited transaction.
*/


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_BERKELEY_DB
#include <m_ctype.h>
#include <myisampack.h>
#include <assert.h>
#include <hash.h>
#include "ha_berkeley.h"
#include "sql_manager.h"
#include <stdarg.h>

#define HA_BERKELEY_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_BERKELEY_RANGE_COUNT   100
#define HA_BERKELEY_MAX_ROWS	  10000000 /* Max rows in table */
/* extra rows for estimate_number_of_rows() */
#define HA_BERKELEY_EXTRA_ROWS	  100

/* Bits for share->status */
#define STATUS_PRIMARY_KEY_INIT 1
#define STATUS_ROW_COUNT_INIT	2
#define STATUS_BDB_ANALYZE	4

const char *ha_berkeley_ext=".db";
bool berkeley_skip=0,berkeley_shared_data=0;
u_int32_t berkeley_init_flags= DB_PRIVATE | DB_RECOVER, berkeley_env_flags=0,
          berkeley_lock_type=DB_LOCK_DEFAULT;
ulong berkeley_cache_size, berkeley_log_buffer_size, berkeley_log_file_size=0;
char *berkeley_home, *berkeley_tmpdir, *berkeley_logdir;
long berkeley_lock_scan_time=0;
ulong berkeley_trans_retry=1;
ulong berkeley_max_lock;
pthread_mutex_t bdb_mutex;

static DB_ENV *db_env;
static HASH bdb_open_tables;

const char *berkeley_lock_names[] =
{ "DEFAULT", "OLDEST","RANDOM","YOUNGEST",0 };
u_int32_t berkeley_lock_types[]=
{ DB_LOCK_DEFAULT, DB_LOCK_OLDEST, DB_LOCK_RANDOM };
TYPELIB berkeley_lock_typelib= {array_elements(berkeley_lock_names),"",
				berkeley_lock_names};

static void berkeley_print_error(const char *db_errpfx, char *buffer);
static byte* bdb_get_key(BDB_SHARE *share,uint *length,
			 my_bool not_used __attribute__((unused)));
static BDB_SHARE *get_share(const char *table_name, TABLE *table);
static int free_share(BDB_SHARE *share, TABLE *table, uint hidden_primary_key,
		      bool mutex_is_locked);
static int write_status(DB *status_block, char *buff, uint length);
static void update_status(BDB_SHARE *share, TABLE *table);
static void berkeley_noticecall(DB_ENV *db_env, db_notices notice);



/* General functions */

bool berkeley_init(void)
{
  DBUG_ENTER("berkeley_init");

  if (!berkeley_tmpdir)
    berkeley_tmpdir=mysql_tmpdir;
  if (!berkeley_home)
    berkeley_home=mysql_real_data_home;
  DBUG_PRINT("bdb",("berkeley_home: %s",mysql_real_data_home));

  /*
    If we don't set set_lg_bsize() we will get into trouble when
    trying to use many open BDB tables.
    If log buffer is not set, assume that the we will need 512 byte per
    open table.  This is a number that we have reached by testing.
  */
  if (!berkeley_log_buffer_size)
  {
    berkeley_log_buffer_size= max(table_cache_size*512,32*1024);
  }
  /*
    Berkeley DB require that
    berkeley_log_file_size >= berkeley_log_buffer_size*4
  */
  berkeley_log_file_size= berkeley_log_buffer_size*4;
  berkeley_log_file_size= MY_ALIGN(berkeley_log_file_size,1024*1024L);
  berkeley_log_file_size= max(berkeley_log_file_size, 10*1024*1024L);

  if (db_env_create(&db_env,0))
    DBUG_RETURN(1); /* purecov: inspected */
  db_env->set_errcall(db_env,berkeley_print_error);
  db_env->set_errpfx(db_env,"bdb");
  db_env->set_noticecall(db_env, berkeley_noticecall);
  db_env->set_tmp_dir(db_env, berkeley_tmpdir);
  db_env->set_data_dir(db_env, mysql_data_home);
  db_env->set_flags(db_env, berkeley_env_flags, 1);
  if (berkeley_logdir)
    db_env->set_lg_dir(db_env, berkeley_logdir); /* purecov: tested */

  if (opt_endinfo)
    db_env->set_verbose(db_env,
			DB_VERB_CHKPOINT | DB_VERB_DEADLOCK | DB_VERB_RECOVERY,
			1);

  db_env->set_cachesize(db_env, 0, berkeley_cache_size, 0);
  db_env->set_lg_max(db_env, berkeley_log_file_size);
  db_env->set_lg_bsize(db_env, berkeley_log_buffer_size);
  db_env->set_lk_detect(db_env, berkeley_lock_type);
  if (berkeley_max_lock)
    db_env->set_lk_max(db_env, berkeley_max_lock);

  if (db_env->open(db_env,
		   berkeley_home,
		   berkeley_init_flags |  DB_INIT_LOCK |
		   DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN |
		   DB_CREATE | DB_THREAD, 0666))
  {
    db_env->close(db_env,0); /* purecov: inspected */
    db_env=0; /* purecov: inspected */
  }

  (void) hash_init(&bdb_open_tables,32,0,0,
		   (hash_get_key) bdb_get_key,0,0);
  pthread_mutex_init(&bdb_mutex,MY_MUTEX_INIT_FAST);
  DBUG_RETURN(db_env == 0);
}


bool berkeley_end(void)
{
  int error;
  DBUG_ENTER("berkeley_end");
  if (!db_env)
    return 1; /* purecov: tested */
  berkeley_cleanup_log_files();
  error=db_env->close(db_env,0);		// Error is logged
  db_env=0;
  hash_free(&bdb_open_tables);
  pthread_mutex_destroy(&bdb_mutex);
  DBUG_RETURN(error != 0);
}

bool berkeley_flush_logs()
{
  int error;
  bool result=0;
  DBUG_ENTER("berkeley_flush_logs");
  if ((error=log_flush(db_env,0)))
  {
    my_error(ER_ERROR_DURING_FLUSH_LOGS,MYF(0),error); /* purecov: inspected */
    result=1; /* purecov: inspected */
  }
  if ((error=txn_checkpoint(db_env,0,0,0)))
  {
    my_error(ER_ERROR_DURING_CHECKPOINT,MYF(0),error); /* purecov: inspected */
    result=1; /* purecov: inspected */
  }
  DBUG_RETURN(result);
}


int berkeley_commit(THD *thd, void *trans)
{
  DBUG_ENTER("berkeley_commit");
  DBUG_PRINT("trans",("ending transaction %s",
		      trans == thd->transaction.stmt.bdb_tid ? "stmt" : "all"));
  int error=txn_commit((DB_TXN*) trans,0);
#ifndef DBUG_OFF
  if (error)
    DBUG_PRINT("error",("error: %d",error)); /* purecov: inspected */
#endif
  DBUG_RETURN(error);
}

int berkeley_rollback(THD *thd, void *trans)
{
  DBUG_ENTER("berkeley_rollback");
  DBUG_PRINT("trans",("aborting transaction %s",
		      trans == thd->transaction.stmt.bdb_tid ? "stmt" : "all"));
  int error=txn_abort((DB_TXN*) trans);
  DBUG_RETURN(error);
}


int berkeley_show_logs(THD *thd)
{
  char **all_logs, **free_logs, **a, **f;
  String *packet= &thd->packet;
  int error=1;
  MEM_ROOT show_logs_root;
  MEM_ROOT *old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
  DBUG_ENTER("berkeley_show_logs");

  init_alloc_root(&show_logs_root, 1024, 1024);
  my_pthread_setspecific_ptr(THR_MALLOC,&show_logs_root);

  if ((error= log_archive(db_env, &all_logs, DB_ARCH_ABS | DB_ARCH_LOG,
			  (void* (*)(size_t)) sql_alloc)) ||
      (error= log_archive(db_env, &free_logs, DB_ARCH_ABS, 
			  (void* (*)(size_t)) sql_alloc)))
  {
    DBUG_PRINT("error", ("log_archive failed (error %d)", error));
    db_env->err(db_env, error, "log_archive: DB_ARCH_ABS");
    if (error== DB_NOTFOUND)
      error=0;					// No log files
    goto err;
  }
  /* Error is 0 here */
  if (all_logs)
  {
    for (a = all_logs, f = free_logs; *a; ++a)
    {
      packet->length(0);
      net_store_data(packet,*a);
      net_store_data(packet,"BDB");
      if (f && *f && strcmp(*a, *f) == 0)
      {
	++f;
	net_store_data(packet, SHOW_LOG_STATUS_FREE);
      }
      else
	net_store_data(packet, SHOW_LOG_STATUS_INUSE);

      if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
      {
	error=1;
	goto err;
      }
    }
  }
err:
  free_root(&show_logs_root,MYF(0));
  my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  DBUG_RETURN(error);
}

static void berkeley_print_error(const char *db_errpfx, char *buffer)
{
  sql_print_error("%s:  %s",db_errpfx,buffer); /* purecov: tested */
}

static void berkeley_noticecall(DB_ENV *db_env, db_notices notice)
{
  switch (notice)
  {
  case DB_NOTICE_LOGFILE_CHANGED: /* purecov: tested */
    pthread_mutex_lock(&LOCK_manager);
    manager_status |= MANAGER_BERKELEY_LOG_CLEANUP;
    pthread_mutex_unlock(&LOCK_manager);
    pthread_cond_signal(&COND_manager);
    break;
  }
}

void berkeley_cleanup_log_files(void)
{
  DBUG_ENTER("berkeley_cleanup_log_files");
  char **names;
  int error;

  /* XXX: Probably this should be done somewhere else, and
   * should be tunable by the user. */
  if ((error = txn_checkpoint(db_env, 0, 0, 0)))
    my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error); /* purecov: inspected */

  if ((error = log_archive(db_env, &names, DB_ARCH_ABS, NULL)) != 0)
  {
    DBUG_PRINT("error", ("log_archive failed (error %d)", error)); /* purecov: inspected */
    db_env->err(db_env, error, "log_archive: DB_ARCH_ABS"); /* purecov: inspected */
    DBUG_VOID_RETURN; /* purecov: inspected */
  }

  if (names)
  {						/* purecov: tested */
    char **np;					/* purecov: tested */
    for (np = names; *np; ++np)			/* purecov: tested */
      my_delete(*np, MYF(MY_WME));		/* purecov: tested */

    free(names);				/* purecov: tested */
  }

  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Berkeley DB tables
*****************************************************************************/

const char **ha_berkeley::bas_ext() const
{ static const char *ext[]= { ha_berkeley_ext, NullS }; return ext; }


static int
berkeley_cmp_hidden_key(DB* file, const DBT *new_key, const DBT *saved_key)
{
  ulonglong a=uint5korr((char*) new_key->data);
  ulonglong b=uint5korr((char*) saved_key->data);
  return  a < b ? -1 : (a > b ? 1 : 0);
}

static int
berkeley_cmp_packed_key(DB *file, const DBT *new_key, const DBT *saved_key)
{
  KEY *key=	      (new_key->app_private ? (KEY*) new_key->app_private :
		       (KEY*) (file->app_private));
  char *new_key_ptr=  (char*) new_key->data;
  char *saved_key_ptr=(char*) saved_key->data;
  KEY_PART_INFO *key_part= key->key_part, *end=key_part+key->key_parts;
  uint key_length=new_key->size;

  for ( ; key_part != end && (int) key_length > 0; key_part++)
  {
    int cmp;
    if (key_part->null_bit)
    {
      if (*new_key_ptr != *saved_key_ptr++)
	return ((int) *new_key_ptr - (int) saved_key_ptr[-1]);
      key_length--;
      if (!*new_key_ptr++)
	continue;
    }
    if ((cmp=key_part->field->pack_cmp(new_key_ptr,saved_key_ptr,
				       key_part->length)))
      return cmp;
    uint length=key_part->field->packed_col_length(new_key_ptr);
    new_key_ptr+=length;
    key_length-=length;
    saved_key_ptr+=key_part->field->packed_col_length(saved_key_ptr);
  }
  return key->handler.bdb_return_if_eq;
}


/* The following is not yet used; Should be used for fixed length keys */

#ifdef NOT_YET
static int
berkeley_cmp_fix_length_key(DB *file, const DBT *new_key, const DBT *saved_key)
{
  KEY *key=	      (new_key->app_private ? (KEY*) new_key->app_private :
		       (KEY*) (file->app_private));
  char *new_key_ptr=  (char*) new_key->data;
  char *saved_key_ptr=(char*) saved_key->data;
  KEY_PART_INFO *key_part= key->key_part, *end=key_part+key->key_parts;
  uint key_length=new_key->size;

  for ( ; key_part != end && (int) key_length > 0 ; key_part++)
  {
    int cmp;
    if ((cmp=key_part->field->pack_cmp(new_key_ptr,saved_key_ptr,0)))
      return cmp;
    new_key_ptr+=key_part->length;
    key_length-= key_part->length;
    saved_key_ptr+=key_part->length;
  }
  return key->handler.bdb_return_if_eq;
}
#endif

/* Compare key against row */

static bool
berkeley_key_cmp(TABLE *table, KEY *key_info, const char *key, uint key_length)
{
  KEY_PART_INFO *key_part= key_info->key_part,
		*end=key_part+key_info->key_parts;

  for ( ; key_part != end && (int) key_length > 0; key_part++)
  {
    int cmp;
    if (key_part->null_bit)
    {
      key_length--;
      /*
	With the current usage, the following case will always be FALSE,
	because NULL keys are sorted before any other key
      */
      if (*key != (table->record[0][key_part->null_offset] &
		   key_part->null_bit) ? 0 : 1)
	return 1;
      if (!*key++)				// Null value
	continue;
    }
    if ((cmp=key_part->field->pack_cmp(key,key_part->length)))
      return cmp;
    uint length=key_part->field->packed_col_length(key);
    key+=length;
    key_length-=length;
  }
  return 0;					// Identical keys
}


int ha_berkeley::open(const char *name, int mode, uint test_if_locked)
{
  char name_buff[FN_REFLEN];
  uint open_mode=(mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
  int error;
  DBUG_ENTER("ha_berkeley::open");

  /* Open primary key */
  hidden_primary_key=0;
  if ((primary_key=table->primary_key) >= MAX_KEY)
  {						// No primary key
    primary_key=table->keys;
    key_used_on_scan=MAX_KEY;
    ref_length=hidden_primary_key=BDB_HIDDEN_PRIMARY_KEY_LENGTH;
  }
  else
    key_used_on_scan=primary_key;

  /* Need some extra memory in case of packed keys */
  uint max_key_length= table->max_key_length + MAX_REF_PARTS*3;
  if (!(alloc_ptr=
	my_multi_malloc(MYF(MY_WME),
			&key_buff,  max_key_length,
			&key_buff2, max_key_length,
			&primary_key_buff,
			(hidden_primary_key ? 0 :
			 table->key_info[table->primary_key].key_length),
			NullS)))
    DBUG_RETURN(1); /* purecov: inspected */
  if (!(rec_buff= (byte*) my_malloc((alloced_rec_buff_length=
				     table->rec_buff_length),
				    MYF(MY_WME))))
  {
    my_free(alloc_ptr,MYF(0)); /* purecov: inspected */
    DBUG_RETURN(1); /* purecov: inspected */
  }

  /* Init shared structure */
  if (!(share=get_share(name,table)))
  {
    my_free((char*) rec_buff,MYF(0)); /* purecov: inspected */
    my_free(alloc_ptr,MYF(0)); /* purecov: inspected */
    DBUG_RETURN(1); /* purecov: inspected */
  }
  thr_lock_data_init(&share->lock,&lock,(void*) 0);
  key_file = share->key_file;
  key_type = share->key_type;
  bzero((char*) &current_row,sizeof(current_row));

  /* Fill in shared structure, if needed */
  pthread_mutex_lock(&share->mutex);
  file = share->file;
  if (!share->use_count++)
  {
    if ((error=db_create(&file, db_env, 0)))
    {
      free_share(share,table, hidden_primary_key,1); /* purecov: inspected */
      my_free((char*) rec_buff,MYF(0)); /* purecov: inspected */
      my_free(alloc_ptr,MYF(0)); /* purecov: inspected */
      my_errno=error; /* purecov: inspected */
      DBUG_RETURN(1); /* purecov: inspected */
    }
    share->file = file;

    file->set_bt_compare(file,
			 (hidden_primary_key ? berkeley_cmp_hidden_key :
			  berkeley_cmp_packed_key));
    if (!hidden_primary_key)
      file->app_private= (void*) (table->key_info+table->primary_key);
    if ((error=(file->open(file, fn_format(name_buff,name,"", ha_berkeley_ext,
					   2 | 4),
			   "main", DB_BTREE, open_mode,0))))
    {
      free_share(share,table, hidden_primary_key,1); /* purecov: inspected */
      my_free((char*) rec_buff,MYF(0)); /* purecov: inspected */
      my_free(alloc_ptr,MYF(0)); /* purecov: inspected */
      my_errno=error; /* purecov: inspected */
      DBUG_RETURN(1); /* purecov: inspected */
    }

    /* Open other keys;  These are part of the share structure */
    key_file[primary_key]=file;
    key_type[primary_key]=DB_NOOVERWRITE;

    DB **ptr=key_file;
    for (uint i=0, used_keys=0; i < table->keys ; i++, ptr++)
    {
      char part[7];
      if (i != primary_key)
      {
	if ((error=db_create(ptr, db_env, 0)))
	{
	  close();				/* purecov: inspected */
	  my_errno=error;			/* purecov: inspected */
	  DBUG_RETURN(1);			/* purecov: inspected */
	}
	sprintf(part,"key%02d",++used_keys);
	key_type[i]=table->key_info[i].flags & HA_NOSAME ? DB_NOOVERWRITE : 0;
	(*ptr)->set_bt_compare(*ptr, berkeley_cmp_packed_key);
	(*ptr)->app_private= (void*) (table->key_info+i);
	if (!(table->key_info[i].flags & HA_NOSAME))
	  (*ptr)->set_flags(*ptr, DB_DUP);
	if ((error=((*ptr)->open(*ptr, name_buff, part, DB_BTREE,
				 open_mode, 0))))
	{
	  close();				/* purecov: inspected */
	  my_errno=error;			/* purecov: inspected */
	  DBUG_RETURN(1);			/* purecov: inspected */
	}
      }
    }
    /* Calculate pack_length of primary key */
    share->fixed_length_primary_key=1;
    if (!hidden_primary_key)
    {
      ref_length=0;
      KEY_PART_INFO *key_part= table->key_info[primary_key].key_part;
      KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;
      for ( ; key_part != end ; key_part++)
	ref_length+= key_part->field->max_packed_col_length(key_part->length);
      share->fixed_length_primary_key=
	(ref_length == table->key_info[primary_key].key_length);
      share->status|=STATUS_PRIMARY_KEY_INIT;
    }    
    share->ref_length=ref_length;
  }
  ref_length=share->ref_length;			// If second open
  pthread_mutex_unlock(&share->mutex);

  transaction=0;
  cursor=0;
  key_read=0;
  block_size=8192;				// Berkeley DB block size
  share->fixed_length_row=!(table->db_create_options & HA_OPTION_PACK_RECORD);

  get_status();
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  DBUG_RETURN(0);
}


int ha_berkeley::close(void)
{
  DBUG_ENTER("ha_berkeley::close");

  my_free((char*) rec_buff,MYF(MY_ALLOW_ZERO_PTR));
  my_free(alloc_ptr,MYF(MY_ALLOW_ZERO_PTR));
  ha_berkeley::extra(HA_EXTRA_RESET);		// current_row buffer
  DBUG_RETURN(free_share(share,table, hidden_primary_key,0));
}


/* Reallocate buffer if needed */

bool ha_berkeley::fix_rec_buff_for_blob(ulong length)
{
  if (! rec_buff || length > alloced_rec_buff_length)
  {
    byte *newptr;
    if (!(newptr=(byte*) my_realloc((gptr) rec_buff, length,
				    MYF(MY_ALLOW_ZERO_PTR))))
      return 1; /* purecov: inspected */
    rec_buff=newptr;
    alloced_rec_buff_length=length;
  }
  return 0;
}


/* Calculate max length needed for row */

ulong ha_berkeley::max_row_length(const byte *buf)
{
  ulong length=table->reclength + table->fields*2;
  for (Field_blob **ptr=table->blob_field ; *ptr ; ptr++)
    length+= (*ptr)->get_length((char*) buf+(*ptr)->offset())+2;
  return length;
}


/*
  Pack a row for storage.  If the row is of fixed length, just store the
  row 'as is'.
  If not, we will generate a packed row suitable for storage.
  This will only fail if we don't have enough memory to pack the row, which;
  may only happen in rows with blobs,  as the default row length is
  pre-allocated.
*/

int ha_berkeley::pack_row(DBT *row, const byte *record, bool new_row)
{
  bzero((char*) row,sizeof(*row));
  if (share->fixed_length_row)
  {
    row->data=(void*) record;
    row->size=table->reclength+hidden_primary_key;
    if (hidden_primary_key)
    {
      if (new_row)
	get_auto_primary_key(current_ident);
      memcpy_fixed((char*) record+table->reclength, (char*) current_ident,
		   BDB_HIDDEN_PRIMARY_KEY_LENGTH);
    }
    return 0;
  }
  if (table->blob_fields)
  {
    if (fix_rec_buff_for_blob(max_row_length(record)))
      return HA_ERR_OUT_OF_MEM; /* purecov: inspected */
  }

  /* Copy null bits */
  memcpy(rec_buff, record, table->null_bytes);
  byte *ptr=rec_buff + table->null_bytes;

  for (Field **field=table->field ; *field ; field++)
    ptr=(byte*) (*field)->pack((char*) ptr,
			       (char*) record + (*field)->offset());

  if (hidden_primary_key)
  {
    if (new_row)
      get_auto_primary_key(current_ident);
    memcpy_fixed((char*) ptr, (char*) current_ident,
		 BDB_HIDDEN_PRIMARY_KEY_LENGTH);
    ptr+=BDB_HIDDEN_PRIMARY_KEY_LENGTH;
  }
  row->data=rec_buff;
  row->size= (size_t) (ptr - rec_buff);
  return 0;
}


void ha_berkeley::unpack_row(char *record, DBT *row)
{
  if (share->fixed_length_row)
    memcpy(record,(char*) row->data,table->reclength+hidden_primary_key);
  else
  {
    /* Copy null bits */
    const char *ptr= (const char*) row->data;
    memcpy(record, ptr, table->null_bytes);
    ptr+=table->null_bytes;
    for (Field **field=table->field ; *field ; field++)
      ptr= (*field)->unpack(record + (*field)->offset(), ptr);
  }
}


/* Store the key and the primary key into the row */

void ha_berkeley::unpack_key(char *record, DBT *key, uint index)
{
  KEY *key_info=table->key_info+index;
  KEY_PART_INFO *key_part= key_info->key_part,
		*end=key_part+key_info->key_parts;

  char *pos=(char*) key->data;
  for ( ; key_part != end; key_part++)
  {
    if (key_part->null_bit)
    {
      if (!*pos++)				// Null value
      {
	/*
	  We don't need to reset the record data as we will not access it
	  if the null data is set
	*/

	record[key_part->null_offset]|=key_part->null_bit;
	continue;
      }
      record[key_part->null_offset]&= ~key_part->null_bit;
    }
    pos= (char*) key_part->field->unpack(record + key_part->field->offset(),
					 pos);
  }
}


/*
  Create a packed key from from a row
  This will never fail as the key buffer is pre allocated.
*/

DBT *ha_berkeley::create_key(DBT *key, uint keynr, char *buff,
			     const byte *record, int key_length)
{
  bzero((char*) key,sizeof(*key));
  if (hidden_primary_key && keynr == primary_key)
  {
    /* We don't need to set app_private here */
    key->data=current_ident;
    key->size=BDB_HIDDEN_PRIMARY_KEY_LENGTH;
    return key;
  }

  KEY *key_info=table->key_info+keynr;
  KEY_PART_INFO *key_part=key_info->key_part;
  KEY_PART_INFO *end=key_part+key_info->key_parts;
  DBUG_ENTER("create_key");

  key->data=buff;
  key->app_private= key_info;
  for ( ; key_part != end && key_length > 0; key_part++)
  {
    if (key_part->null_bit)
    {
      /* Store 0 if the key part is a NULL part */
      if (record[key_part->null_offset] & key_part->null_bit)
      {
	*buff++ =0;
	key->flags|=DB_DBT_DUPOK;
	continue;
      }
      *buff++ = 1;				// Store NOT NULL marker
    }
    buff=key_part->field->pack_key(buff,(char*) (record + key_part->offset),
				   key_part->length);
    key_length-=key_part->length;
  }
  key->size= (buff  - (char*) key->data);
  DBUG_DUMP("key",(char*) key->data, key->size);
  DBUG_RETURN(key);
}


/*
  Create a packed key from from a MySQL unpacked key
*/

DBT *ha_berkeley::pack_key(DBT *key, uint keynr, char *buff,
			   const byte *key_ptr, uint key_length)
{
  KEY *key_info=table->key_info+keynr;
  KEY_PART_INFO *key_part=key_info->key_part;
  KEY_PART_INFO *end=key_part+key_info->key_parts;
  DBUG_ENTER("bdb:pack_key");

  bzero((char*) key,sizeof(*key));
  key->data=buff;
  key->app_private= (void*) key_info;

  for (; key_part != end && (int) key_length > 0 ; key_part++)
  {
    uint offset=0;
    if (key_part->null_bit)
    {
      if (!(*buff++ = (*key_ptr == 0)))		// Store 0 if NULL
      {
	key_length-= key_part->store_length;
	key_ptr+=   key_part->store_length;
	key->flags|=DB_DBT_DUPOK;
	continue;
      }
      offset=1;					// Data is at key_ptr+1
    }
    buff=key_part->field->pack_key_from_key_image(buff,(char*) key_ptr+offset,
						  key_part->length);
    key_ptr+=key_part->store_length;
    key_length-=key_part->store_length;
  }
  key->size= (buff  - (char*) key->data);
  DBUG_DUMP("key",(char*) key->data, key->size);
  DBUG_RETURN(key);
}


int ha_berkeley::write_row(byte * record)
{
  DBT row,prim_key,key;
  int error;
  DBUG_ENTER("write_row");

  statistic_increment(ha_write_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(record+table->time_stamp-1);
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();
  if ((error=pack_row(&row, record,1)))
    DBUG_RETURN(error); /* purecov: inspected */

  if (table->keys + test(hidden_primary_key) == 1)
  {
    error=file->put(file, transaction, create_key(&prim_key, primary_key,
						  key_buff, record),
		    &row, key_type[primary_key]);
    last_dup_key=primary_key;
  }
  else
  {
    DB_TXN *sub_trans = transaction;
    /* Don't use sub transactions in temporary tables (in_use == 0) */
    ulong thd_options = table->in_use ? table->in_use->options : 0;
    for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
    {
      key_map changed_keys = 0;
      if (using_ignore && (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
      {
	if ((error=txn_begin(db_env, transaction, &sub_trans, 0))) /* purecov: deadcode */
	  break; /* purecov: deadcode */
	DBUG_PRINT("trans",("starting subtransaction")); /* purecov: deadcode */
      }
      if (!(error=file->put(file, sub_trans, create_key(&prim_key, primary_key,
							key_buff, record),
			    &row, key_type[primary_key])))
      {
	changed_keys |= (key_map) 1 << primary_key;
	for (uint keynr=0 ; keynr < table->keys ; keynr++)
	{
	  if (keynr == primary_key)
	    continue;
	  if ((error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  create_key(&key, keynr, key_buff2,
						     record),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	  changed_keys |= (key_map) 1 << keynr;
	}
      }
      else
	last_dup_key=primary_key;
      if (error)
      {
	/* Remove inserted row */
	DBUG_PRINT("error",("Got error %d",error));
	if (using_ignore)
	{
	  int new_error = 0;
	  if (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS)
	  {
	    DBUG_PRINT("trans",("aborting subtransaction")); /* purecov: deadcode */
	    new_error=txn_abort(sub_trans); /* purecov: deadcode */
	  }
	  else if (changed_keys)
	  {
	    new_error = 0;
	    for (uint keynr=0; changed_keys; keynr++, changed_keys >>= 1)
	    {
	      if (changed_keys & 1)
	      {
		if ((new_error = remove_key(sub_trans, keynr, record,
					    &prim_key)))
		  break; /* purecov: inspected */
	      }
	    }
	  }
	  if (new_error)
	  {
	    error=new_error;			// This shouldn't happen /* purecov: inspected */
	    break; /* purecov: inspected */
	  }
	}
      }
      else if (using_ignore && (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
      {
	DBUG_PRINT("trans",("committing subtransaction")); /* purecov: deadcode */
	error=txn_commit(sub_trans, 0); /* purecov: deadcode */
      }
      if (error != DB_LOCK_DEADLOCK)
	break;
    }
  }
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  else if (!error)
    changed_rows++;
  DBUG_RETURN(error);
}


/* Compare if a key in a row has changed */

int ha_berkeley::key_cmp(uint keynr, const byte * old_row,
			 const byte * new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for ( ; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
	  (new_row[key_part->null_offset] & key_part->null_bit))
	return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH))
    {

      if (key_part->field->cmp_binary((char*) (old_row + key_part->offset),
				      (char*) (new_row + key_part->offset),
				      (ulong) key_part->length))
	return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
		 key_part->length))
	return 1;
    }
  }
  return 0;
}


/*
  Update a row from one value to another.
  Clobbers key_buff2
*/

int ha_berkeley::update_primary_key(DB_TXN *trans, bool primary_key_changed,
				    const byte * old_row, DBT *old_key,
				    const byte * new_row, DBT *new_key,
				    ulong thd_options, bool local_using_ignore)
{
  DBT row;
  int error;
  DBUG_ENTER("update_primary_key");

  if (primary_key_changed)
  {
    // Primary key changed or we are updating a key that can have duplicates.
    // Delete the old row and add a new one
    if (!(error=remove_key(trans, primary_key, old_row, old_key)))
    {
      if (!(error=pack_row(&row, new_row, 0)))
      {
	if ((error=file->put(file, trans, new_key, &row,
			     key_type[primary_key])))
	{
	  // Probably a duplicated key; restore old key and row if needed
	  last_dup_key=primary_key;
	  if (local_using_ignore &&
	      !(thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
	  {
	    int new_error;
	    if ((new_error=pack_row(&row, old_row, 0)) ||
		(new_error=file->put(file, trans, old_key, &row,
				     key_type[primary_key])))
	      error=new_error;                  // fatal error /* purecov: inspected */
	  }
	}
      }
    }
  }
  else
  {
    // Primary key didn't change;  just update the row data
    if (!(error=pack_row(&row, new_row, 0)))
      error=file->put(file, trans, new_key, &row, 0);
  }
  DBUG_RETURN(error);
}

/*
  Restore changed keys, when a non-fatal error aborts the insert/update
  of one row.
  Clobbers keybuff2
*/

int ha_berkeley::restore_keys(DB_TXN *trans, key_map changed_keys,
			      uint primary_key,
			      const byte *old_row, DBT *old_key,
			      const byte *new_row, DBT *new_key,
			      ulong thd_options)
{
  int error;
  DBT tmp_key;
  uint keynr;
  DBUG_ENTER("restore_keys");

  /* Restore the old primary key, and the old row, but don't ignore
     duplicate key failure */
  if ((error=update_primary_key(trans, TRUE, new_row, new_key,
				old_row, old_key, thd_options, FALSE)))
    goto err; /* purecov: inspected */

  /* Remove the new key, and put back the old key
     changed_keys is a map of all non-primary keys that need to be
     rolled back.  The last key set in changed_keys is the one that
     triggered the duplicate key error (it wasn't inserted), so for
     that one just put back the old value. */
  for (keynr=0; changed_keys; keynr++, changed_keys >>= 1)
  {
    if (changed_keys & 1)
    {
      if (changed_keys != 1 &&
	  (error = remove_key(trans, keynr, new_row, new_key)))
	break; /* purecov: inspected */
      if ((error = key_file[keynr]->put(key_file[keynr], trans,
					create_key(&tmp_key, keynr, key_buff2,
						   old_row),
					old_key, key_type[keynr])))
	break; /* purecov: inspected */
    }
  }
  
err:
  dbug_assert(error != DB_KEYEXIST);
  DBUG_RETURN(error);
}


int ha_berkeley::update_row(const byte * old_row, byte * new_row)
{
  DBT prim_key, key, old_prim_key;
  int error;
  DB_TXN *sub_trans;
  ulong thd_options = table->in_use ? table->in_use->options : 0;
  bool primary_key_changed;
  DBUG_ENTER("update_row");
  LINT_INIT(error);

  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_row+table->time_stamp-1);

  if (hidden_primary_key)
  {
    primary_key_changed=0;
    bzero((char*) &prim_key,sizeof(prim_key));
    prim_key.data= (void*) current_ident;
    prim_key.size=BDB_HIDDEN_PRIMARY_KEY_LENGTH;
    old_prim_key=prim_key;
  }
  else
  {
    create_key(&prim_key, primary_key, key_buff, new_row);

    if ((primary_key_changed=key_cmp(primary_key, old_row, new_row)))
      create_key(&old_prim_key, primary_key, primary_key_buff, old_row);
    else
      old_prim_key=prim_key;
  }

  sub_trans = transaction;
  for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
  {
    key_map changed_keys = 0;
    if (using_ignore &&	(thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
    {
      if ((error=txn_begin(db_env, transaction, &sub_trans, 0))) /* purecov: deadcode */
	break; /* purecov: deadcode */
      DBUG_PRINT("trans",("starting subtransaction")); /* purecov: deadcode */
    }
    /* Start by updating the primary key */
    if (!(error=update_primary_key(sub_trans, primary_key_changed,
				   old_row, &old_prim_key,
				   new_row, &prim_key,
				   thd_options, using_ignore)))
    {
      // Update all other keys
      for (uint keynr=0 ; keynr < table->keys ; keynr++)
      {
	if (keynr == primary_key)
	  continue;
	if (key_cmp(keynr, old_row, new_row) || primary_key_changed)
	{
	  if ((error=remove_key(sub_trans, keynr, old_row, &old_prim_key)))
	  {
	    if (using_ignore && /* purecov: inspected */
		(thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
            {
	      int new_error;
	      DBUG_PRINT("trans",("aborting subtransaction"));
	      new_error=txn_abort(sub_trans);
	      if (new_error)
		error = new_error;
	    }
	    DBUG_RETURN(error);			// Fatal error /* purecov: inspected */
	  }
	  changed_keys |= (key_map)1 << keynr;
	  if ((error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  create_key(&key, keynr, key_buff2,
						     new_row),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	}
      }
    }
    if (error)
    {
      /* Remove inserted row */
      DBUG_PRINT("error",("Got error %d",error));
      if (using_ignore)
      {
	int new_error = 0;
	if (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS)
	{
	  DBUG_PRINT("trans",("aborting subtransaction")); /* purecov: deadcode */
	  new_error=txn_abort(sub_trans); /* purecov: deadcode */
	}
	else if (changed_keys)
	  new_error=restore_keys(transaction, changed_keys, primary_key,
				 old_row, &old_prim_key, new_row, &prim_key,
				 thd_options);
	if (new_error)
	{
	  error=new_error;			// This shouldn't happen /* purecov: inspected */
	  break; /* purecov: inspected */
	}
      }
    }
    else if (using_ignore && (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
    {
      DBUG_PRINT("trans",("committing subtransaction")); /* purecov: deadcode */
      error=txn_commit(sub_trans, 0); /* purecov: deadcode */
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  DBUG_RETURN(error);
}


/*
  Delete one key
  This uses key_buff2, when keynr != primary key, so it's important that
  a function that calls this doesn't use this buffer for anything else.
*/

int ha_berkeley::remove_key(DB_TXN *trans, uint keynr, const byte *record,
			    DBT *prim_key)
{
  int error;
  DBT key;
  DBUG_ENTER("remove_key");
  DBUG_PRINT("enter",("index: %d",keynr));

  if (keynr == active_index && cursor)
    error=cursor->c_del(cursor,0);
  else if (keynr == primary_key ||
	   ((table->key_info[keynr].flags & (HA_NOSAME | HA_NULL_PART_KEY)) ==
	    HA_NOSAME))
  {						// Unique key
    dbug_assert(keynr == primary_key || prim_key->data != key_buff2);
    error=key_file[keynr]->del(key_file[keynr], trans,
			       keynr == primary_key ?
			       prim_key :
			       create_key(&key, keynr, key_buff2, record),
			       0);
  }
  else
  {
    /*
      To delete the not duplicated key, we need to open an cursor on the
      row to find the key to be delete and delete it.
      We will never come here with keynr = primary_key
    */
    dbug_assert(keynr != primary_key && prim_key->data != key_buff2);
    DBC *tmp_cursor;
    if (!(error=key_file[keynr]->cursor(key_file[keynr], trans,
					&tmp_cursor, 0)))
    {
      if (!(error=tmp_cursor->c_get(tmp_cursor,
                                    create_key(&key, keynr, key_buff2, record),
                                    prim_key, DB_GET_BOTH | DB_RMW)))
      {					// This shouldn't happen
	error=tmp_cursor->c_del(tmp_cursor,0);
      }
      int result=tmp_cursor->c_close(tmp_cursor);
      if (!error)
	error=result;
    }
  }
  DBUG_RETURN(error);
}


/* Delete all keys for new_record */

int ha_berkeley::remove_keys(DB_TXN *trans, const byte *record,
			     DBT *new_record, DBT *prim_key, key_map keys)
{
  int result = 0;
  for (uint keynr=0; keys; keynr++, keys>>=1)
  {
    if (keys & 1)
    {
      int new_error=remove_key(trans, keynr, record, prim_key);
      if (new_error)
      {
	result=new_error;			// Return last error /* purecov: inspected */
	break;					// Let rollback correct things /* purecov: inspected */
      }
    }
  }
  return result;
}


int ha_berkeley::delete_row(const byte * record)
{
  int error;
  DBT row, prim_key;
  key_map keys=table->keys_in_use;
  ulong thd_options = table->in_use ? table->in_use->options : 0;
  DBUG_ENTER("delete_row");
  statistic_increment(ha_delete_count,&LOCK_status);

  if ((error=pack_row(&row, record, 0)))
    DBUG_RETURN((error)); /* purecov: inspected */
  create_key(&prim_key, primary_key, key_buff, record);
  if (hidden_primary_key)
    keys|= (key_map) 1 << primary_key;

  /* Subtransactions may be used in order to retry the delete in
     case we get a DB_LOCK_DEADLOCK error. */
  DB_TXN *sub_trans = transaction;
  for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
  {
    if (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS)
    {
      if ((error=txn_begin(db_env, transaction, &sub_trans, 0))) /* purecov: deadcode */
	break; /* purecov: deadcode */
      DBUG_PRINT("trans",("starting sub transaction")); /* purecov: deadcode */
    }
    error=remove_keys(sub_trans, record, &row, &prim_key, keys);
    if (!error && (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS))
    {
      DBUG_PRINT("trans",("ending sub transaction")); /* purecov: deadcode */
      error=txn_commit(sub_trans, 0); /* purecov: deadcode */
    }
    if (error)
    { /* purecov: inspected */
      DBUG_PRINT("error",("Got error %d",error));
      if (thd_options & OPTION_INTERNAL_SUBTRANSACTIONS)
      {
	/* retry */
	int new_error;
	DBUG_PRINT("trans",("aborting subtransaction"));
	if ((new_error=txn_abort(sub_trans)))
	{
	  error=new_error;			// This shouldn't happen
	  break;
	}
      }
      else
	break;					// No retry - return error
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
#ifdef CANT_COUNT_DELETED_ROWS
  if (!error)
    changed_rows--;
#endif
  DBUG_RETURN(error);
}


int ha_berkeley::index_init(uint keynr)
{
  int error;
  DBUG_ENTER("ha_berkeley::index_init");
  DBUG_PRINT("enter",("table: '%s'  key: %d", table->real_name, keynr));

  /*
    Under some very rare conditions (like full joins) we may already have
    an active cursor at this point
  */
  if (cursor)
  {
    DBUG_PRINT("note",("Closing active cursor"));
    cursor->c_close(cursor);
  }
  active_index=keynr;
  if ((error=key_file[keynr]->cursor(key_file[keynr], transaction, &cursor,
				     table->reginfo.lock_type >
				     TL_WRITE_ALLOW_READ ?
				     0 : 0)))
    cursor=0;				// Safety /* purecov: inspected */
  bzero((char*) &last_key,sizeof(last_key));
  DBUG_RETURN(error);
}

int ha_berkeley::index_end()
{
  int error=0;
  DBUG_ENTER("index_end");
  if (cursor)
  {
    DBUG_PRINT("enter",("table: '%s'", table->real_name));
    error=cursor->c_close(cursor);
    cursor=0;
  }
  DBUG_RETURN(error);
}


/* What to do after we have read a row based on an index */

int ha_berkeley::read_row(int error, char *buf, uint keynr, DBT *row,
			  DBT *found_key, bool read_next)
{
  DBUG_ENTER("ha_berkeley::read_row");
  if (error)
  {
    if (error == DB_NOTFOUND || error == DB_KEYEMPTY)
      error=read_next ? HA_ERR_END_OF_FILE : HA_ERR_KEY_NOT_FOUND;
    table->status=STATUS_NOT_FOUND;
    DBUG_RETURN(error);
  }
  if (hidden_primary_key)
    memcpy_fixed(current_ident,
		 (char*) row->data+row->size-BDB_HIDDEN_PRIMARY_KEY_LENGTH,
		 BDB_HIDDEN_PRIMARY_KEY_LENGTH);
  table->status=0;
  if (keynr != primary_key)
  {
    /* We only found the primary key.  Now we have to use this to find
       the row data */
    if (key_read && found_key)
    {
      unpack_key(buf,found_key,keynr);
      if (!hidden_primary_key)
	unpack_key(buf,row,primary_key);
      DBUG_RETURN(0);
    }
    DBT key;
    bzero((char*) &key,sizeof(key));
    key.data=key_buff;
    key.size=row->size;
    key.app_private= (void*) (table->key_info+primary_key);
    memcpy(key_buff,row->data,row->size);
    /* Read the data into current_row */
    current_row.flags=DB_DBT_REALLOC;
    if ((error=file->get(file, transaction, &key, &current_row, 0)))
    {
      table->status=STATUS_NOT_FOUND; /* purecov: inspected */
      DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error); /* purecov: inspected */
    }
    row= &current_row;
  }
  unpack_row(buf,row);
  DBUG_RETURN(0);
}


/* This is only used to read whole keys */

int ha_berkeley::index_read_idx(byte * buf, uint keynr, const byte * key,
				uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  DBUG_ENTER("index_read_idx");
  current_row.flags=DB_DBT_REALLOC;
  active_index= (uint) -1;
  DBUG_RETURN(read_row(key_file[keynr]->get(key_file[keynr], transaction,
				 pack_key(&last_key, keynr, key_buff, key,
					  key_len),
				 &current_row,0),
		       (char*) buf, keynr, &current_row, &last_key, 0));
}


int ha_berkeley::index_read(byte * buf, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  DBT row;
  int error;
  KEY *key_info= &table->key_info[active_index];
  DBUG_ENTER("ha_berkeley::index_read");

  statistic_increment(ha_read_key_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  if (key_len == key_info->key_length)
  {
    error=read_row(cursor->c_get(cursor, pack_key(&last_key,
						  active_index,
						  key_buff,
						  key, key_len),
				 &row,
				 (find_flag == HA_READ_KEY_EXACT ?
				  DB_SET : DB_SET_RANGE)),
		   (char*) buf, active_index, &row, (DBT*) 0, 0);
  }
  else
  {
    /* read of partial key */
    pack_key(&last_key, active_index, key_buff, key, key_len);
    /* Store for compare */
    memcpy(key_buff2, key_buff, (key_len=last_key.size));
    /*
      If HA_READ_AFTER_KEY is set, return next key, else return first
      matching key.
    */
    key_info->handler.bdb_return_if_eq= (find_flag == HA_READ_AFTER_KEY ?
					 1 : -1);
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE),
		   (char*) buf, active_index, &row, (DBT*) 0, 0);
    key_info->handler.bdb_return_if_eq= 0;
    if (!error && find_flag == HA_READ_KEY_EXACT)
    {
      /* Ensure that we found a key that is equal to the current one */
      if (!error && berkeley_key_cmp(table, key_info, key_buff2, key_len))
	error=HA_ERR_KEY_NOT_FOUND;
    }
  }
  DBUG_RETURN(error);
}


int ha_berkeley::index_next(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_next");
  statistic_increment(ha_read_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       (char*) buf, active_index, &row, &last_key, 1));
}

int ha_berkeley::index_next_same(byte * buf, const byte *key, uint keylen)
{
  DBT row;
  int error;
  DBUG_ENTER("index_next_same");
  statistic_increment(ha_read_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  if (keylen == table->key_info[active_index].key_length)
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT_DUP),
		   (char*) buf, active_index, &row, &last_key, 1);
  else
  {
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		   (char*) buf, active_index, &row, &last_key, 1);
    if (!error && ::key_cmp(table, key, active_index, keylen))
      error=HA_ERR_END_OF_FILE;
  }
  DBUG_RETURN(error);
}


int ha_berkeley::index_prev(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_prev");
  statistic_increment(ha_read_prev_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV),
		       (char*) buf, active_index, &row, &last_key, 1));
}


int ha_berkeley::index_first(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_first");
  statistic_increment(ha_read_first_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_FIRST),
		       (char*) buf, active_index, &row, &last_key, 1));
}

int ha_berkeley::index_last(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_last");
  statistic_increment(ha_read_last_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_LAST),
		       (char*) buf, active_index, &row, &last_key, 0));
}

int ha_berkeley::rnd_init(bool scan)
{
  DBUG_ENTER("rnd_init");
  current_row.flags=DB_DBT_REALLOC;
  DBUG_RETURN(index_init(primary_key));
}

int ha_berkeley::rnd_end()
{
  return index_end();
}

int ha_berkeley::rnd_next(byte *buf)
{
  DBT row;
  DBUG_ENTER("rnd_next");
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       (char*) buf, primary_key, &row, &last_key, 1));
}


DBT *ha_berkeley::get_pos(DBT *to, byte *pos)
{
  /* We don't need to set app_private here */
  bzero((char*) to,sizeof(*to));

  to->data=pos;
  if (share->fixed_length_primary_key)
    to->size=ref_length;
  else
  {
    KEY_PART_INFO *key_part=table->key_info[primary_key].key_part;
    KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;

    for ( ; key_part != end ; key_part++)
      pos+=key_part->field->packed_col_length((char*) pos);
    to->size= (uint) (pos- (byte*) to->data);
  }
  return to;
}


int ha_berkeley::rnd_pos(byte * buf, byte *pos)
{
  DBT db_pos;
  statistic_increment(ha_read_rnd_count,&LOCK_status);

  active_index= (uint) -1;			// Don't delete via cursor
  return read_row(file->get(file, transaction,
			    get_pos(&db_pos, pos),
			    &current_row, 0),
		 (char*) buf, primary_key, &current_row, (DBT*) 0, 0);
}

void ha_berkeley::position(const byte *record)
{
  DBT key;
  if (hidden_primary_key)
    memcpy_fixed(ref, (char*) current_ident, BDB_HIDDEN_PRIMARY_KEY_LENGTH);
  else
    create_key(&key, primary_key, (char*) ref, record);
}


void ha_berkeley::info(uint flag)
{
  DBUG_ENTER("ha_berkeley::info");
  if (flag & HA_STATUS_VARIABLE)
  {
    records = share->rows + changed_rows; // Just to get optimisations right
    deleted = 0;
  }
  if ((flag & HA_STATUS_CONST) || version != share->version)
  {
    version=share->version;
    for (uint i=0 ; i < table->keys ; i++)
    {
      table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]=
	share->rec_per_key[i];
    }
  }
  else if (flag & HA_STATUS_ERRKEY)
    errkey=last_dup_key;
  DBUG_VOID_RETURN;
}


int ha_berkeley::extra(enum ha_extra_function operation)
{
  switch (operation) {
  case HA_EXTRA_RESET:
  case HA_EXTRA_RESET_STATE:
    key_read=0;
    using_ignore=0;
    if (current_row.flags & (DB_DBT_MALLOC | DB_DBT_REALLOC))
    {
      current_row.flags=0;
      if (current_row.data)
      {
	free(current_row.data);
	current_row.data=0;
      }
    }
    break;
  case HA_EXTRA_KEYREAD:
    key_read=1;					// Query satisfied with key
    break;
  case HA_EXTRA_NO_KEYREAD:
    key_read=0;
    break;
  case HA_EXTRA_IGNORE_DUP_KEY:
    using_ignore=1;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    using_ignore=0;
    break;
  default:
    break;
  }
  return 0;
}


int ha_berkeley::reset(void)
{
  key_read=0;					// Reset to state after open
  return 0;
}


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement to be able to rollback the statement.
  If not, we have to start a master transaction if there doesn't exist
  one from before.
*/

int ha_berkeley::external_lock(THD *thd, int lock_type)
{
  int error=0;
  DBUG_ENTER("ha_berkeley::external_lock");
  if (lock_type != F_UNLCK)
  {
    if (!thd->transaction.bdb_lock_count++)
    {
      DBUG_ASSERT(thd->transaction.stmt.bdb_tid == 0);
      transaction=0;				// Safety
      /* First table lock, start transaction */
      if ((thd->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN |
			   OPTION_TABLE_LOCK)) &&
	  !thd->transaction.all.bdb_tid)
      {
	/* We have to start a master transaction */
	DBUG_PRINT("trans",("starting transaction all"));
	if ((error=txn_begin(db_env, 0,
			     (DB_TXN**) &thd->transaction.all.bdb_tid,
			     0)))
	{
	  thd->transaction.bdb_lock_count--;	// We didn't get the lock /* purecov: inspected */
	  DBUG_RETURN(error); /* purecov: inspected */
	}
	if (thd->in_lock_tables)
	  DBUG_RETURN(0);			// Don't create stmt trans
      }
      DBUG_PRINT("trans",("starting transaction stmt"));
      if ((error=txn_begin(db_env,
			   (DB_TXN*) thd->transaction.all.bdb_tid,
			   (DB_TXN**) &thd->transaction.stmt.bdb_tid,
			   0)))
      {
	/* We leave the possible master transaction open */
	thd->transaction.bdb_lock_count--;	// We didn't get the lock /* purecov: inspected */
	DBUG_RETURN(error); /* purecov: inspected */
      }
    }
    transaction= (DB_TXN*) thd->transaction.stmt.bdb_tid;
  }
  else
  {
    lock.type=TL_UNLOCK;			// Unlocked
    thread_safe_add(share->rows, changed_rows, &share->mutex);
    changed_rows=0;
    if (!--thd->transaction.bdb_lock_count)
    {
      if (thd->transaction.stmt.bdb_tid)
      {
	/*
	   F_UNLOCK is done without a transaction commit / rollback.
	   This happens if the thread didn't update any rows
	   We must in this case commit the work to keep the row locks
	*/
	DBUG_PRINT("trans",("commiting non-updating transaction"));
	error=txn_commit((DB_TXN*) thd->transaction.stmt.bdb_tid,0);
	thd->transaction.stmt.bdb_tid=0;
	transaction=0;
      }
    }
  }
  DBUG_RETURN(error);
}


/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_berkeley::start_stmt(THD *thd)
{
  int error=0;
  DBUG_ENTER("ha_berkeley::start_stmt");
  if (!thd->transaction.stmt.bdb_tid)
  {
    DBUG_PRINT("trans",("starting transaction stmt"));
    error=txn_begin(db_env, (DB_TXN*) thd->transaction.all.bdb_tid,
		    (DB_TXN**) &thd->transaction.stmt.bdb_tid,
		    0);
  }
  transaction= (DB_TXN*) thd->transaction.stmt.bdb_tid;
  DBUG_RETURN(error);
}


/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Berkeley DB changes all WRITE locks to TL_WRITE_ALLOW_WRITE (which
  signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.
*/


THR_LOCK_DATA **ha_berkeley::store_lock(THD *thd, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /* If we are not doing a LOCK TABLE, then allow multiple writers */
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
	 lock_type <= TL_WRITE) &&
	!thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;
    lock.type=lock_type;
    lock_on_read= ((table->reginfo.lock_type > TL_WRITE_ALLOW_READ) ? DB_RMW :
		   0);
  }
  *to++= &lock;
  return to;
}


static int create_sub_table(const char *table_name, const char *sub_name,
			    DBTYPE type, int flags)
{
  int error;
  DB *file;
  DBUG_ENTER("create_sub_table");
  DBUG_PRINT("enter",("sub_name: %s",sub_name));

  if (!(error=db_create(&file, db_env, 0)))
  {
    file->set_flags(file, flags);
    error=(file->open(file, table_name, sub_name, type,
		      DB_THREAD | DB_CREATE, my_umask));
    if (error)
    {
      DBUG_PRINT("error",("Got error: %d when opening table '%s'",error, /* purecov: inspected */
			  table_name)); /* purecov: inspected */
      (void) file->remove(file,table_name,NULL,0); /* purecov: inspected */
    }
    else
      (void) file->close(file,0);
  }
  else
  {
    DBUG_PRINT("error",("Got error: %d when creting table",error)); /* purecov: inspected */
  }
  if (error)
    my_errno=error; /* purecov: inspected */
  DBUG_RETURN(error);
}


int ha_berkeley::create(const char *name, register TABLE *form,
			HA_CREATE_INFO *create_info)
{
  char name_buff[FN_REFLEN];
  char part[7];
  uint index=1;
  int error=1;
  DBUG_ENTER("ha_berkeley::create");

  fn_format(name_buff,name,"", ha_berkeley_ext,2 | 4);

  /* Create the main table that will hold the real rows */
  if (create_sub_table(name_buff,"main",DB_BTREE,0))
    DBUG_RETURN(1); /* purecov: inspected */

  primary_key=table->primary_key;
  /* Create the keys */
  for (uint i=0; i < form->keys; i++)
  {
    if (i != primary_key)
    {
      sprintf(part,"key%02d",index++);
      if (create_sub_table(name_buff, part, DB_BTREE,
			   (table->key_info[i].flags & HA_NOSAME) ? 0 :
			   DB_DUP))
	DBUG_RETURN(1); /* purecov: inspected */
    }
  }

  /* Create the status block to save information from last status command */
  /* Is DB_BTREE the best option here ? (QUEUE can't be used in sub tables) */

  DB *status_block;
  if (!db_create(&status_block, db_env, 0))
  {
    if (!status_block->open(status_block, name_buff,
			    "status", DB_BTREE, DB_CREATE, 0))
    {
      char rec_buff[4+MAX_KEY*4];
      uint length= 4+ table->keys*4;
      bzero(rec_buff, length);
      if (!write_status(status_block, rec_buff, length))
	error=0;
      status_block->close(status_block,0);
    }
  }
  DBUG_RETURN(error);
}


int ha_berkeley::delete_table(const char *name)
{
  int error;
  char name_buff[FN_REFLEN];
  if ((error=db_create(&file, db_env, 0)))
    my_errno=error; /* purecov: inspected */
  else
    error=file->remove(file,fn_format(name_buff,name,"",ha_berkeley_ext,2 | 4),
		       NULL,0);
  file=0;					// Safety
  return error;
}

/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_berkeley::scan_time()
{
  return records/3;
}

ha_rows ha_berkeley::records_in_range(int keynr,
				      const byte *start_key,uint start_key_len,
				      enum ha_rkey_function start_search_flag,
				      const byte *end_key,uint end_key_len,
				      enum ha_rkey_function end_search_flag)
{
  DBT key;
  DB_KEY_RANGE start_range, end_range;
  DB *kfile=key_file[keynr];
  double start_pos,end_pos,rows;
  DBUG_ENTER("records_in_range");

  if ((start_key && kfile->key_range(kfile,transaction,
				     pack_key(&key, keynr, key_buff, start_key,
					      start_key_len),
				     &start_range,0)) ||
      (end_key && kfile->key_range(kfile,transaction,
				   pack_key(&key, keynr, key_buff, end_key,
					    end_key_len),
				   &end_range,0)))
    DBUG_RETURN(HA_BERKELEY_RANGE_COUNT); // Better than returning an error /* purecov: inspected */

  if (!start_key)
    start_pos=0.0;
  else if (start_search_flag == HA_READ_KEY_EXACT)
    start_pos=start_range.less;
  else
    start_pos=start_range.less+start_range.equal;

  if (!end_key)
    end_pos=1.0;
  else if (end_search_flag == HA_READ_BEFORE_KEY)
    end_pos=end_range.less;
  else
    end_pos=end_range.less+end_range.equal;
  rows=(end_pos-start_pos)*records;
  DBUG_PRINT("exit",("rows: %g",rows));
  DBUG_RETURN(rows <= 1.0 ? (ha_rows) 1 : (ha_rows) rows);
}


longlong ha_berkeley::get_auto_increment()
{
  longlong nr=1;				// Default if error or new key
  int error;
  (void) ha_berkeley::extra(HA_EXTRA_KEYREAD);

  /* Set 'active_index' */
  ha_berkeley::index_init(table->next_number_index);

  if (!table->next_number_key_offset)
  {						// Autoincrement at key-start
    error=ha_berkeley::index_last(table->record[1]);
  }
  else
  {
    DBT row,old_key;
    bzero((char*) &row,sizeof(row));
    KEY *key_info= &table->key_info[active_index];

    /* Reading next available number for a sub key */
    ha_berkeley::create_key(&last_key, active_index,
			    key_buff, table->record[0],
			    table->next_number_key_offset);
    /* Store for compare */
    memcpy(old_key.data=key_buff2, key_buff, (old_key.size=last_key.size));
    old_key.app_private=(void*) key_info;
    error=1;
    {
      /* Modify the compare so that we will find the next key */
      key_info->handler.bdb_return_if_eq= 1;
      /* We lock the next key as the new key will probl. be on the same page */
      error=cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE | DB_RMW);
      key_info->handler.bdb_return_if_eq= 0;
      if (!error || error == DB_NOTFOUND)
      {
	/*
	  Now search go one step back and then we should have found the
	  biggest key with the given prefix
	  */
	error=1;
	if (!cursor->c_get(cursor, &last_key, &row, DB_PREV | DB_RMW) &&
	    !berkeley_cmp_packed_key(key_file[active_index], &old_key,
				     &last_key))
	{
	  error=0;				// Found value
	  unpack_key((char*) table->record[1], &last_key, active_index);
	}
      }
    }
  }
  if (!error)
    nr=(longlong)
      table->next_number_field->val_int_offset(table->rec_buff_length)+1;
  ha_berkeley::index_end();
  (void) ha_berkeley::extra(HA_EXTRA_NO_KEYREAD);
  return nr;
}


/****************************************************************************
	 Analyzing, checking, and optimizing tables
****************************************************************************/

#ifdef NOT_YET
static void print_msg(THD *thd, const char *table_name, const char *op_name,
		      const char *msg_type, const char *fmt, ...)
{
  String* packet = &thd->packet;
  packet->length(0);
  char msgbuf[256];
  msgbuf[0] = 0;
  va_list args;
  va_start(args,fmt);

  my_vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  DBUG_PRINT(msg_type,("message: %s",msgbuf));

  net_store_data(packet, table_name);
  net_store_data(packet, op_name);
  net_store_data(packet, msg_type);
  net_store_data(packet, msgbuf);
  if (my_net_write(&thd->net, (char*)thd->packet.ptr(),
		   thd->packet.length()))
    thd->killed=1;
}
#endif

int ha_berkeley::analyze(THD* thd, HA_CHECK_OPT* check_opt)
{
  DB_BTREE_STAT *stat=0;
  uint i;

  for (i=0 ; i < table->keys ; i++)
  {
    if (stat)
    {
      free(stat);
      stat=0;
    }
    if (key_file[i]->stat(key_file[i], (void*) &stat, 0, 0))
      goto err; /* purecov: inspected */
    share->rec_per_key[i]= (stat->bt_ndata /
			    (stat->bt_nkeys ? stat->bt_nkeys : 1));
  }
  /* A hidden primary key is not in key_file[] */
  if (hidden_primary_key)
  {
    if (stat)
    {
      free(stat);
      stat=0;
    }
    if (file->stat(file, (void*) &stat, 0, 0))
      goto err; /* purecov: inspected */
  }
  pthread_mutex_lock(&share->mutex);
  share->rows=stat->bt_ndata;
  share->status|=STATUS_BDB_ANALYZE;		// Save status on close
  share->version++;				// Update stat in table
  pthread_mutex_unlock(&share->mutex);
  update_status(share,table);			// Write status to file
  if (stat)
    free(stat);
  return ((share->status & STATUS_BDB_ANALYZE) ? HA_ADMIN_FAILED :
	  HA_ADMIN_OK);

err:
  if (stat) /* purecov: inspected */
    free(stat); /* purecov: inspected */
  return HA_ADMIN_FAILED; /* purecov: inspected */
}

int ha_berkeley::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  return ha_berkeley::analyze(thd,check_opt);
}


int ha_berkeley::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_berkeley::check");

  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);

#ifdef NOT_YET
  char name_buff[FN_REFLEN];
  int error;
  DB *tmp_file;
  /*
    To get this to work we need to ensure that no running transaction is
    using the table. We also need to create a new environment without
    locking for this.
  */

  /* We must open the file again to be able to check it! */
  if ((error=db_create(&tmp_file, db_env, 0)))
  {
    print_msg(thd, table->real_name, "check", "error",
	      "Got error %d creating environment",error);
    DBUG_RETURN(HA_ADMIN_FAILED);
  }

  /* Compare the overall structure */
  tmp_file->set_bt_compare(tmp_file,
			   (hidden_primary_key ? berkeley_cmp_hidden_key :
			    berkeley_cmp_packed_key));
  tmp_file->app_private= (void*) (table->key_info+table->primary_key);
  fn_format(name_buff,share->table_name,"", ha_berkeley_ext, 2 | 4);
  if ((error=tmp_file->verify(tmp_file, name_buff, NullS, (FILE*) 0,
			      hidden_primary_key ? 0 : DB_NOORDERCHK)))
  {
    print_msg(thd, table->real_name, "check", "error",
	      "Got error %d checking file structure",error);
    tmp_file->close(tmp_file,0);
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  }

  /* Check each index */
  tmp_file->set_bt_compare(tmp_file, berkeley_cmp_packed_key);
  for (uint index=0,i=0 ; i < table->keys ; i++)
  {
    char part[7];
    if (i == primary_key)
      strmov(part,"main");
    else
      sprintf(part,"key%02d",++index);
    tmp_file->app_private= (void*) (table->key_info+i);
    if ((error=tmp_file->verify(tmp_file, name_buff, part, (FILE*) 0,
				DB_ORDERCHKONLY)))
    {
      print_msg(thd, table->real_name, "check", "error",
		"Key %d was not in order (Error: %d)",
		index+ test(i >= primary_key),
		error);
      tmp_file->close(tmp_file,0);
      DBUG_RETURN(HA_ADMIN_CORRUPT);
    }
  }
  tmp_file->close(tmp_file,0);
  DBUG_RETURN(HA_ADMIN_OK);
#endif
}

/****************************************************************************
 Handling the shared BDB_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static byte* bdb_get_key(BDB_SHARE *share,uint *length,
			 my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static BDB_SHARE *get_share(const char *table_name, TABLE *table)
{
  BDB_SHARE *share;
  pthread_mutex_lock(&bdb_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(BDB_SHARE*) hash_search(&bdb_open_tables, (byte*) table_name,
				       length)))
  {
    ha_rows *rec_per_key;
    char *tmp_name;
    DB **key_file;
    u_int32_t *key_type;
    
    if ((share=(BDB_SHARE *)
	 my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
			 &share, sizeof(*share),
			 &rec_per_key, table->keys * sizeof(ha_rows),
			 &tmp_name, length+1,
			 &key_file, (table->keys+1) * sizeof(*key_file),
			 &key_type, (table->keys+1) * sizeof(u_int32_t),
			 NullS)))
    {
      share->rec_per_key = rec_per_key;
      share->table_name = tmp_name;
      share->table_name_length=length;
      strmov(share->table_name,table_name);
      share->key_file = key_file;
      share->key_type = key_type;
      if (hash_insert(&bdb_open_tables, (byte*) share))
      {
	pthread_mutex_unlock(&bdb_mutex); /* purecov: inspected */
	my_free((gptr) share,0); /* purecov: inspected */
	return 0; /* purecov: inspected */
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
    }
  }
  pthread_mutex_unlock(&bdb_mutex);
  return share;
}

static int free_share(BDB_SHARE *share, TABLE *table, uint hidden_primary_key,
		      bool mutex_is_locked)
{
  int error, result = 0;
  uint keys=table->keys + test(hidden_primary_key);
  pthread_mutex_lock(&bdb_mutex);
  if (mutex_is_locked)
    pthread_mutex_unlock(&share->mutex); /* purecov: inspected */
  if (!--share->use_count)
  {
    DB **key_file = share->key_file;
    update_status(share,table);
    /* this does share->file->close() implicitly */
    for (uint i=0; i < keys; i++)
    {
      if (key_file[i] && (error=key_file[i]->close(key_file[i],0)))
	result=error; /* purecov: inspected */
    }
    if (share->status_block &&
	(error = share->status_block->close(share->status_block,0)))
      result = error; /* purecov: inspected */
    hash_delete(&bdb_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&bdb_mutex);
  return result;
}

/*
  Get status information that is stored in the 'status' sub database
  and the max used value for the hidden primary key.
*/

void ha_berkeley::get_status()
{
  if (!test_all_bits(share->status,(STATUS_PRIMARY_KEY_INIT |
				    STATUS_ROW_COUNT_INIT)))
  {
    pthread_mutex_lock(&share->mutex);
    if (!(share->status & STATUS_PRIMARY_KEY_INIT))
    {
      (void) extra(HA_EXTRA_KEYREAD);
      index_init(primary_key);
      if (!index_last(table->record[1]))
	share->auto_ident=uint5korr(current_ident);
      index_end();
      (void) extra(HA_EXTRA_NO_KEYREAD);
    }
    if (! share->status_block)
    {
      char name_buff[FN_REFLEN];
      uint open_mode= (((table->db_stat & HA_READ_ONLY) ? DB_RDONLY : 0)
		       | DB_THREAD);
      fn_format(name_buff, share->table_name,"", ha_berkeley_ext, 2 | 4);
      if (!db_create(&share->status_block, db_env, 0))
      {
	if (share->status_block->open(share->status_block, name_buff,
				      "status", DB_BTREE, open_mode, 0))
	{
	  share->status_block->close(share->status_block, 0); /* purecov: inspected */
	  share->status_block=0; /* purecov: inspected */
	}
      }
    }
    if (!(share->status & STATUS_ROW_COUNT_INIT) && share->status_block)
    {
      share->org_rows=share->rows=
	table->max_rows ? table->max_rows : HA_BERKELEY_MAX_ROWS;
      if (!share->status_block->cursor(share->status_block, 0, &cursor, 0))
      {
	DBT row;
	char rec_buff[64];
	bzero((char*) &row,sizeof(row));
	bzero((char*) &last_key,sizeof(last_key));
	row.data=rec_buff;
	row.ulen=sizeof(rec_buff);
	row.flags=DB_DBT_USERMEM;
	if (!cursor->c_get(cursor, &last_key, &row, DB_FIRST))
	{
	  uint i;
	  uchar *pos=(uchar*) row.data;
	  share->org_rows=share->rows=uint4korr(pos); pos+=4;
	  for (i=0 ; i < table->keys ; i++)
	  {
	    share->rec_per_key[i]=uint4korr(pos); pos+=4;
	  }
	}
	cursor->c_close(cursor);
      }
      cursor=0;					// Safety
    }
    share->status|= STATUS_PRIMARY_KEY_INIT | STATUS_ROW_COUNT_INIT;
    pthread_mutex_unlock(&share->mutex);
  }
}


static int write_status(DB *status_block, char *buff, uint length)
{
  DBT row,key;
  int error;
  const char *key_buff="status";

  bzero((char*) &row,sizeof(row));
  bzero((char*) &key,sizeof(key));
  row.data=buff;
  key.data=(void*) key_buff;
  key.size=sizeof(key_buff);
  row.size=length;
  error=status_block->put(status_block, 0, &key, &row, 0);
  return error;
}


static void update_status(BDB_SHARE *share, TABLE *table)
{
  DBUG_ENTER("update_status");
  if (share->rows != share->org_rows ||
      (share->status & STATUS_BDB_ANALYZE))
  {
    pthread_mutex_lock(&share->mutex);
    if (!share->status_block)
    {
      /*
	Create sub database 'status' if it doesn't exist from before
	(This '*should*' always exist for table created with MySQL)
      */

      char name_buff[FN_REFLEN]; /* purecov: inspected */
      if (db_create(&share->status_block, db_env, 0)) /* purecov: inspected */
	goto end; /* purecov: inspected */
      share->status_block->set_flags(share->status_block,0); /* purecov: inspected */
      if (share->status_block->open(share->status_block,
				    fn_format(name_buff,share->table_name,"",
					      ha_berkeley_ext,2 | 4),
				    "status", DB_BTREE,
				    DB_THREAD | DB_CREATE, my_umask)) /* purecov: inspected */
	goto end; /* purecov: inspected */
    }
    {
      char rec_buff[4+MAX_KEY*4], *pos=rec_buff;
      int4store(pos,share->rows); pos+=4;
      for (uint i=0 ; i < table->keys ; i++)
      {
	int4store(pos,share->rec_per_key[i]); pos+=4;
      }
      DBUG_PRINT("info",("updating status for %s",share->table_name));
      (void) write_status(share->status_block, rec_buff,
			  (uint) (pos-rec_buff));
      share->status&= ~STATUS_BDB_ANALYZE;
      share->org_rows=share->rows;
    }
end:
    pthread_mutex_unlock(&share->mutex);
  }
  DBUG_VOID_RETURN;
}

/*
  Return an estimated of the number of rows in the table.
  Used when sorting to allocate buffers and by the optimizer.
*/

ha_rows ha_berkeley::estimate_number_of_rows()
{
  return share->rows + HA_BERKELEY_EXTRA_ROWS;
}

#endif /* HAVE_BERKELEY_DB */
