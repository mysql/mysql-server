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


/* HANDLER ... commands - direct access to ISAM */

#include "mysql_priv.h"
#include "sql_select.h"
#include <assert.h>

/* TODO:
  HANDLER blabla OPEN [ AS foobar ] [ (column-list) ]

  the most natural (easiest, fastest) way to do it is to
  compute List<Item> field_list not in mysql_ha_read
  but in mysql_ha_open, and then store it in TABLE structure.

  The problem here is that mysql_parse calls free_item to free all the
  items allocated at the end of every query. The workaround would to
  keep two item lists per THD - normal free_list and handler_items.
  The second is to be freeed only on thread end. mysql_ha_open should
  then do { handler_items=concat(handler_items, free_list); free_list=0; }

  But !!! do_command calls free_root at the end of every query and frees up
  all the sql_alloc'ed memory. It's harder to work around...
*/

#define HANDLER_TABLES_HACK(thd) {      \
  TABLE *tmp=thd->open_tables;          \
  thd->open_tables=thd->handler_tables; \
  thd->handler_tables=tmp; }

static TABLE **find_table_ptr_by_name(THD *thd,const char *db,
				      const char *table_name, bool is_alias);

int mysql_ha_open(THD *thd, TABLE_LIST *tables)
{
  HANDLER_TABLES_HACK(thd);
  int err=open_tables(thd,tables);
  HANDLER_TABLES_HACK(thd);
  if (err)
    return -1;

  // there can be only one table in *tables
  if (!(tables->table->file->table_flags() & HA_CAN_SQL_HANDLER))
  {
    my_printf_error(ER_ILLEGAL_HA,ER(ER_ILLEGAL_HA),MYF(0), tables->alias);
    mysql_ha_close(thd, tables,1);
    return -1;
  }

  send_ok(&thd->net);
  return 0;
}

int mysql_ha_close(THD *thd, TABLE_LIST *tables, bool dont_send_ok)
{
  TABLE **ptr=find_table_ptr_by_name(thd, tables->db, tables->alias, 1);

  if (*ptr)
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    if (close_thread_table(thd, ptr))
    {
      /* Tell threads waiting for refresh that something has happened */
      VOID(pthread_cond_broadcast(&COND_refresh));
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  else
  {
    my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),
		    tables->alias, "HANDLER");
    return -1;
  }
  if (!dont_send_ok)
    send_ok(&thd->net);
  return 0;
}

int mysql_ha_closeall(THD *thd, TABLE_LIST *tables)
{
  TABLE **ptr=find_table_ptr_by_name(thd, tables->db, tables->real_name, 0);
  if (*ptr && close_thread_table(thd, ptr))
  {
    /* Tell threads waiting for refresh that something has happened */
    VOID(pthread_cond_broadcast(&COND_refresh));
  }
  return 0;
}

static enum enum_ha_read_modes rkey_to_rnext[]=
    { RNEXT, RNEXT, RPREV, RNEXT, RPREV, RNEXT, RPREV };


int mysql_ha_read(THD *thd, TABLE_LIST *tables,
    enum enum_ha_read_modes mode, char *keyname, List<Item> *key_expr,
    enum ha_rkey_function ha_rkey_mode, Item *cond,
    ha_rows select_limit,ha_rows offset_limit)
{
  int err, keyno=-1;
  TABLE *table=*find_table_ptr_by_name(thd, tables->db, tables->alias, 1);
  if (!table)
  {
    my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),
		    tables->alias,"HANDLER");
    return -1;
  }
  tables->table=table;

  if (cond && cond->fix_fields(thd,tables))
    return -1;

  table->file->init_table_handle_for_HANDLER(); // Only InnoDB requires it

  if (keyname)
  {
    if ((keyno=find_type(keyname, &table->keynames, 1+2)-1)<0)
    {
      my_printf_error(ER_KEY_DOES_NOT_EXITS,ER(ER_KEY_DOES_NOT_EXITS),MYF(0),
          keyname,tables->alias);
      return -1;
    }
    table->file->index_init(keyno);
  }

  List<Item> list;
  list.push_front(new Item_field(NULL,NULL,"*"));
  List_iterator<Item> it(list);
  uint num_rows;
  it++;

  insert_fields(thd,tables,tables->db,tables->alias,&it);

  select_limit+=offset_limit;
  send_fields(thd,list,1);

  HANDLER_TABLES_HACK(thd);
  MYSQL_LOCK *lock=mysql_lock_tables(thd,&tables->table,1);
  HANDLER_TABLES_HACK(thd);
  if (!lock)
     goto err0; // mysql_lock_tables() printed error message already

  table->file->init_table_handle_for_HANDLER(); // Only InnoDB requires it

  for (num_rows=0; num_rows < select_limit; )
  {
    switch(mode) {
    case RFIRST:
      if (keyname)
        err=table->file->index_first(table->record[0]);
      else
      {
	if (!(err=table->file->rnd_init(1)))
          err=table->file->rnd_next(table->record[0]);
      }
      mode=RNEXT;
      break;
    case RLAST:
      DBUG_ASSERT(keyname != 0);
      err=table->file->index_last(table->record[0]);
      mode=RPREV;
      break;
    case RNEXT:
      err=keyname ?
	table->file->index_next(table->record[0]) :
	table->file->rnd_next(table->record[0]);
      break;
    case RPREV:
      DBUG_ASSERT(keyname != 0);
      err=table->file->index_prev(table->record[0]);
      break;
    case RKEY:
    {
      DBUG_ASSERT(keyname != 0);
      KEY *keyinfo=table->key_info+keyno;
      KEY_PART_INFO *key_part=keyinfo->key_part;
      uint key_len;
      byte *key;
      if (key_expr->elements > keyinfo->key_parts)
      {
	my_printf_error(ER_TOO_MANY_KEY_PARTS,ER(ER_TOO_MANY_KEY_PARTS),
			MYF(0),keyinfo->key_parts);
	goto err;
      }
      List_iterator_fast<Item> it_ke(*key_expr);
      Item *item;
      for (key_len=0 ; (item=it_ke++) ; key_part++)
      {
	if (item->fix_fields(thd, tables))
	  goto err;
	if (item->used_tables() & ~RAND_TABLE_BIT)
        {
          my_error(ER_WRONG_ARGUMENTS,MYF(0),"HANDLER ... READ");
	  goto err;
        }
	item->save_in_field(key_part->field, 1);
	key_len+=key_part->store_length;
      }
      if (!(key= (byte*) thd->calloc(ALIGN_SIZE(key_len))))
      {
	send_error(&thd->net,ER_OUTOFMEMORY);
	goto err;
      }
      key_copy(key, table, keyno, key_len);
      err=table->file->index_read(table->record[0],
				  key,key_len,ha_rkey_mode);
      mode=rkey_to_rnext[(int)ha_rkey_mode];
      break;
    }
    default:
      send_error(&thd->net,ER_ILLEGAL_HA);
      goto err;
    }

    if (err == HA_ERR_RECORD_DELETED)
      continue;
    if (err)
    {
      if (err != HA_ERR_KEY_NOT_FOUND && err != HA_ERR_END_OF_FILE)
      {
        sql_print_error("mysql_ha_read: Got error %d when reading table '%s'",
                        err, tables->real_name);
        table->file->print_error(err,MYF(0));
        goto err;
      }
      goto ok;
    }
    if (cond && !cond->val_int())
      continue;
    if (num_rows >= offset_limit)
    {
      String *packet = &thd->packet;
      Item *item;
      packet->length(0);
      it.rewind();
      while ((item=it++))
      {
	if (item->send(thd,packet))
	{
	  packet->free();                             // Free used
	  my_error(ER_OUT_OF_RESOURCES,MYF(0));
	  goto err;
	}
      }
      my_net_write(&thd->net, (char*)packet->ptr(), packet->length());
    }
    num_rows++;
  }
ok:
  mysql_unlock_tables(thd,lock);
  send_eof(&thd->net);
  return 0;
err:
  mysql_unlock_tables(thd,lock);
err0:
  return -1;
}

static TABLE **find_table_ptr_by_name(THD *thd, const char *db,
				      const char *table_name, bool is_alias)
{
  int dblen;
  TABLE **ptr;

  DBUG_ASSERT(db);
  dblen=*db ? strlen(db)+1 : 0;
  ptr=&(thd->handler_tables);

  for (TABLE *table=*ptr; table ; table=*ptr)
  {
    if ((!dblen || !memcmp(table->table_cache_key, db, dblen)) &&
        !my_strcasecmp((is_alias ? table->table_name : table->real_name),table_name))
    {
      if (table->version != refresh_version)
      {
        VOID(pthread_mutex_lock(&LOCK_open));
        if (close_thread_table(thd, ptr))
        {
          /* Tell threads waiting for refresh that something has happened */
          VOID(pthread_cond_broadcast(&COND_refresh));
        }
        VOID(pthread_mutex_unlock(&LOCK_open));
        continue;
      }
      break;
    }
    ptr=&(table->next);
  }
  return ptr;
}

