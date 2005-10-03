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


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "ha_myisammrg.h"
#ifndef MASTER
#include "../srclib/myisammrg/myrg_def.h"
#else
#include "../myisammrg/myrg_def.h"
#endif

/*****************************************************************************
** MyISAM MERGE tables
*****************************************************************************/

/* MyISAM MERGE handlerton */

handlerton myisammrg_hton= {
  "MRG_MYISAM",
  SHOW_OPTION_YES,
  "Collection of identical MyISAM tables", 
  DB_TYPE_MRG_MYISAM,
  NULL,
  0,       /* slot */
  0,       /* savepoint size. */
  NULL,    /* close_connection */
  NULL,    /* savepoint */
  NULL,    /* rollback to savepoint */
  NULL,    /* release savepoint */
  NULL,    /* commit */
  NULL,    /* rollback */
  NULL,    /* prepare */
  NULL,    /* recover */
  NULL,    /* commit_by_xid */
  NULL,    /* rollback_by_xid */
  NULL,    /* create_cursor_read_view */
  NULL,    /* set_cursor_read_view */
  NULL,    /* close_cursor_read_view */
  HTON_NO_FLAGS
};


ha_myisammrg::ha_myisammrg(TABLE *table_arg)
  :handler(&myisammrg_hton, table_arg), file(0)
{}

static const char *ha_myisammrg_exts[] = {
  ".MRG",
  NullS
};

const char **ha_myisammrg::bas_ext() const
{
  return ha_myisammrg_exts;
}


const char *ha_myisammrg::index_type(uint key_number)
{
  return ((table->key_info[key_number].flags & HA_FULLTEXT) ? 
	  "FULLTEXT" :
	  (table->key_info[key_number].flags & HA_SPATIAL) ?
	  "SPATIAL" :
	  (table->key_info[key_number].algorithm == HA_KEY_ALG_RTREE) ?
	  "RTREE" :
	  "BTREE");
}


int ha_myisammrg::open(const char *name, int mode, uint test_if_locked)
{
  char name_buff[FN_REFLEN];

  DBUG_PRINT("info", ("ha_myisammrg::open"));
  if (!(file=myrg_open(fn_format(name_buff,name,"","",2 | 4), mode,
		       test_if_locked)))
  {
    DBUG_PRINT("info", ("ha_myisammrg::open exit %d", my_errno));
    return (my_errno ? my_errno : -1);
  }
  DBUG_PRINT("info", ("ha_myisammrg::open myrg_extrafunc..."))
  myrg_extrafunc(file, query_cache_invalidate_by_MyISAM_filename_ref);
  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_NO_WAIT_LOCK,0);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_WAIT_LOCK,0);

  if (table->s->reclength != mean_rec_length && mean_rec_length)
  {
    DBUG_PRINT("error",("reclength: %d  mean_rec_length: %d",
			table->s->reclength, mean_rec_length));
    goto err;
  }
#if !defined(BIG_TABLES) || SIZEOF_OFF_T == 4
  /* Merge table has more than 2G rows */
  if (table->s->crashed)
    goto err;
#endif
  return (0);
err:
  myrg_close(file);
  file=0;
  return (my_errno= HA_ERR_WRONG_MRG_TABLE_DEF);
}

int ha_myisammrg::close(void)
{
  return myrg_close(file);
}

int ha_myisammrg::write_row(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_write_count,&LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  if (table->next_number_field && buf == table->record[0])
      update_auto_increment();
  return myrg_write(file,buf);
}

int ha_myisammrg::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(table->in_use->status_var.ha_update_count,&LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return myrg_update(file,old_data,new_data);
}

int ha_myisammrg::delete_row(const byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_delete_count,&LOCK_status);
  return myrg_delete(file,buf);
}

int ha_myisammrg::index_read(byte * buf, const byte * key,
			  uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=myrg_rkey(file,buf,active_index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_read_idx(byte * buf, uint index, const byte * key,
				 uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=myrg_rkey(file,buf,index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_read_last(byte * buf, const byte * key, uint key_len)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=myrg_rkey(file,buf,active_index, key, key_len,
		      HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_next(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  int error=myrg_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_prev(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_prev_count,
		      &LOCK_status);
  int error=myrg_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_first(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_first_count,
		      &LOCK_status);
  int error=myrg_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_last(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_last_count,
		      &LOCK_status);
  int error=myrg_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_next_same(byte * buf,
                                  const byte *key __attribute__((unused)),
                                  uint length __attribute__((unused)))
{
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  int error=myrg_rnext_same(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::rnd_init(bool scan)
{
  return myrg_extra(file,HA_EXTRA_RESET,0);
}

int ha_myisammrg::rnd_next(byte *buf)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
  int error=myrg_rrnd(file, buf, HA_OFFSET_ERROR);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::rnd_pos(byte * buf, byte *pos)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_count,
		      &LOCK_status);
  int error=myrg_rrnd(file, buf, my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_myisammrg::position(const byte *record)
{
  ulonglong position= myrg_position(file);
  my_store_ptr(ref, ref_length, (my_off_t) position);
}


ha_rows ha_myisammrg::records_in_range(uint inx, key_range *min_key,
                                       key_range *max_key)
{
  return (ha_rows) myrg_records_in_range(file, (int) inx, min_key, max_key);
}


void ha_myisammrg::info(uint flag)
{
  MYMERGE_INFO info;
  (void) myrg_status(file,&info,flag);
  /*
    The following fails if one has not compiled MySQL with -DBIG_TABLES
    and one has more than 2^32 rows in the merge tables.
  */
  records = (ha_rows) info.records;
  deleted = (ha_rows) info.deleted;
#if !defined(BIG_TABLES) || SIZEOF_OFF_T == 4
  if ((info.records >= (ulonglong) 1 << 32) ||
      (info.deleted >= (ulonglong) 1 << 32))
    table->s->crashed= 1;
#endif
  data_file_length=info.data_file_length;
  errkey  = info.errkey;
  table->s->keys_in_use.set_prefix(table->s->keys);
  table->s->db_options_in_use= info.options;
  table->s->is_view= 1;
  mean_rec_length= info.reclength;
  block_size=0;
  update_time=0;
#if SIZEOF_OFF_T > 4
  ref_length=6;					// Should be big enough
#else
  ref_length=4;					// Can't be > than my_off_t
#endif
  if (flag & HA_STATUS_CONST)
  {
    if (table->s->key_parts && info.rec_per_key)
      memcpy((char*) table->key_info[0].rec_per_key,
	     (char*) info.rec_per_key,
	     sizeof(table->key_info[0].rec_per_key)*table->s->key_parts);
  }
}


int ha_myisammrg::extra(enum ha_extra_function operation)
{
  /* As this is just a mapping, we don't have to force the underlying
     tables to be closed */
  if (operation == HA_EXTRA_FORCE_REOPEN ||
      operation == HA_EXTRA_PREPARE_FOR_DELETE)
    return 0;
  return myrg_extra(file,operation,0);
}


/* To be used with WRITE_CACHE, EXTRA_CACHE and BULK_INSERT_BEGIN */

int ha_myisammrg::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_WRITE_CACHE)
    return 0;
  return myrg_extra(file, operation, (void*) &cache_size);
}

int ha_myisammrg::external_lock(THD *thd, int lock_type)
{
  return myrg_lock_database(file,lock_type);
}

uint ha_myisammrg::lock_count(void) const
{
  return file->tables;
}


THR_LOCK_DATA **ha_myisammrg::store_lock(THD *thd,
					 THR_LOCK_DATA **to,
					 enum thr_lock_type lock_type)
{
  MYRG_TABLE *open_table;

  for (open_table=file->open_tables ;
       open_table != file->end_table ;
       open_table++)
  {
    *(to++)= &open_table->table->lock;
    if (lock_type != TL_IGNORE && open_table->table->lock.type == TL_UNLOCK)
      open_table->table->lock.type=lock_type;
  }
  return to;
}


/* Find out database name and table name from a filename */

static void split_file_name(const char *file_name,
			    LEX_STRING *db, LEX_STRING *name)
{
  uint dir_length, prefix_length;
  char buff[FN_REFLEN];

  db->length= 0;
  strmake(buff, file_name, sizeof(buff)-1);
  dir_length= dirname_length(buff);
  if (dir_length > 1)
  {
    /* Get database */
    buff[dir_length-1]= 0;			// Remove end '/'
    prefix_length= dirname_length(buff);
    db->str= (char*) file_name+ prefix_length;
    db->length= dir_length - prefix_length -1;
  }
  name->str= (char*) file_name+ dir_length;
  name->length= (uint) (fn_ext(name->str) - name->str);
}


void ha_myisammrg::update_create_info(HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_myisammrg::update_create_info");

  if (!(create_info->used_fields & HA_CREATE_USED_UNION))
  {
    MYRG_TABLE *open_table;
    THD *thd=current_thd;

    create_info->merge_list.next= &create_info->merge_list.first;
    create_info->merge_list.elements=0;

    for (open_table=file->open_tables ;
	 open_table != file->end_table ;
	 open_table++)
    {
      TABLE_LIST *ptr;
      LEX_STRING db, name;

      if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
	goto err;
      split_file_name(open_table->table->filename, &db, &name);
      if (!(ptr->table_name= thd->strmake(name.str, name.length)))
	goto err;
      if (db.length && !(ptr->db= thd->strmake(db.str, db.length)))
	goto err;

      create_info->merge_list.elements++;
      (*create_info->merge_list.next) = (byte*) ptr;
      create_info->merge_list.next= (byte**) &ptr->next_local;
    }
    *create_info->merge_list.next=0;
  }
  if (!(create_info->used_fields & HA_CREATE_USED_INSERT_METHOD))
  {
    create_info->merge_insert_method = file->merge_insert_method;
  }
  DBUG_VOID_RETURN;

err:
  create_info->merge_list.elements=0;
  create_info->merge_list.first=0;
  DBUG_VOID_RETURN;
}


int ha_myisammrg::create(const char *name, register TABLE *form,
			 HA_CREATE_INFO *create_info)
{
  char buff[FN_REFLEN];
  const char **table_names, **pos;
  TABLE_LIST *tables= (TABLE_LIST*) create_info->merge_list.first;
  THD *thd= current_thd;
  uint dirlgt= dirname_length(name);
  DBUG_ENTER("ha_myisammrg::create");

  if (!(table_names= (const char**)
        thd->alloc((create_info->merge_list.elements+1) * sizeof(char*))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  for (pos= table_names; tables; tables= tables->next_local)
  {
    const char *table_name;
    TABLE **tbl= 0;
    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
      tbl= find_temporary_table(thd, tables->db, tables->table_name);
    if (!tbl)
    {
      /*
        Construct the path to the MyISAM table. Try to meet two conditions:
        1.) Allow to include MyISAM tables from different databases, and
        2.) allow for moving DATADIR around in the file system.
        The first means that we need paths in the .MRG file. The second
        means that we should not have absolute paths in the .MRG file.
        The best, we can do, is to use 'mysql_data_home', which is '.'
        in mysqld and may be an absolute path in an embedded server.
        This means that it might not be possible to move the DATADIR of
        an embedded server without changing the paths in the .MRG file.
      */
      uint length= my_snprintf(buff, FN_REFLEN, "%s/%s/%s", mysql_data_home,
			       tables->db, tables->table_name);
      /*
        If a MyISAM table is in the same directory as the MERGE table,
        we use the table name without a path. This means that the
        DATADIR can easily be moved even for an embedded server as long
        as the MyISAM tables are from the same database as the MERGE table.
      */
      if ((dirname_length(buff) == dirlgt) && ! memcmp(buff, name, dirlgt))
        table_name= tables->table_name;
      else
        if (! (table_name= thd->strmake(buff, length)))
          DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
    else
      table_name= (*tbl)->s->path;
    *pos++= table_name;
  }
  *pos=0;
  DBUG_RETURN(myrg_create(fn_format(buff,name,"","",2+4+16),
			  table_names,
                          create_info->merge_insert_method,
                          (my_bool) 0));
}


void ha_myisammrg::append_create_info(String *packet)
{
  const char *current_db;
  uint db_length;
  THD *thd= current_thd;

  if (file->merge_insert_method != MERGE_INSERT_DISABLED)
  {
    packet->append(" INSERT_METHOD=",15);
    packet->append(get_type(&merge_insert_method,file->merge_insert_method-1));
  }
  packet->append(" UNION=(",8);
  MYRG_TABLE *open_table,*first;

  current_db= table->s->db;
  db_length= (uint) strlen(current_db);

  for (first=open_table=file->open_tables ;
       open_table != file->end_table ;
       open_table++)
  {
    LEX_STRING db, name;
    split_file_name(open_table->table->filename, &db, &name);
    if (open_table != first)
      packet->append(',');
    /* Report database for mapped table if it isn't in current database */
    if (db.length &&
	(db_length != db.length ||
	 strncmp(current_db, db.str, db.length)))
    {
      append_identifier(thd, packet, db.str, db.length);
      packet->append('.');
    }
    append_identifier(thd, packet, name.str, name.length);
  }
  packet->append(')');
}
