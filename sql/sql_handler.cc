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

#include <assert.h>
#include "mysql_priv.h"
#include "sql_select.h"

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
  
  But !!! do_cammand calls free_root at the end of every query and frees up
  all the sql_alloc'ed memory. It's harder to work around...
 */

#define HANDLER_TABLES_HACK(thd) {      \
  TABLE *tmp=thd->open_tables;          \
  thd->open_tables=thd->handler_tables; \
  thd->handler_tables=tmp; }

static TABLE **find_table_ptr_by_name(THD *thd, const char *db, 
				      const char *table_name);

int mysql_ha_open(THD *thd, TABLE_LIST *tables)
{
  HANDLER_TABLES_HACK(thd);
  int err=open_tables(thd,tables);
  HANDLER_TABLES_HACK(thd);
  if (err)
    return -1;
  
  send_ok(&thd->net);
  return 0;
}

int mysql_ha_close(THD *thd, TABLE_LIST *tables)
{
  TABLE **ptr=find_table_ptr_by_name(thd, tables->db, tables->name);

  if (*ptr)
    close_thread_table(thd, ptr);
  
  send_ok(&thd->net);
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
  TABLE *table=*find_table_ptr_by_name(thd, tables->db, tables->name);
  if (!table)
  {
    my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),
        tables->name,"HANDLER");
    return -1;
  }
  tables->table=table;

  if (cond && cond->fix_fields(thd,tables))
    return -1;
  
  if (keyname)
  {
    if ((keyno=find_type(keyname, &table->keynames, 1+2)-1)<0)
    {
      my_printf_error(ER_KEY_DOES_NOT_EXITS,ER(ER_KEY_DOES_NOT_EXITS),MYF(0),
          keyname,tables->name);
      return -1;
    }
  }

  List<Item> list;
  list.push_front(new Item_field(NULL,NULL,"*"));
  List_iterator<Item> it(list);
  it++;

  insert_fields(thd,tables,tables->db,tables->name,&it);

  table->file->index_init(keyno);

  select_limit+=offset_limit;
  send_fields(thd,list,1);
  
  MYSQL_LOCK *lock=mysql_lock_tables(thd,&tables->table,1);

  for (uint num_rows=0; num_rows < select_limit; )
  {
    switch(mode)
    {
      case RFIRST:
         err=keyname ?
             table->file->index_first(table->record[0]) :
             table->file->rnd_init(1) ||
             table->file->rnd_next(table->record[0]);
         mode=RNEXT;
         break;
      case RLAST:
         dbug_assert(keyname != 0);
         err=table->file->index_last(table->record[0]);
         mode=RPREV;
         break;
      case RNEXT:
         err=keyname ?
             table->file->index_next(table->record[0]) :
             table->file->rnd_next(table->record[0]);
         break;
      case RPREV:
         dbug_assert(keyname != 0);
         err=table->file->index_prev(table->record[0]);
         break;
      case RKEY:
        {
          dbug_assert(keyname != 0);
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
            item->save_in_field(key_part->field);
            key_len+=key_part->store_length;
          }
          if (!(key=sql_calloc(ALIGN_SIZE(key_len))))
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

    if (err)
    {
      if (err != HA_ERR_KEY_NOT_FOUND && err != HA_ERR_END_OF_FILE)
      {
        sql_print_error("mysql_ha_read: Got error %d when reading table",
                        err);
        table->file->print_error(err,MYF(0));
        goto err;
      }
      goto ok;
    }
    if (cond)
    { 
      err=err;
      if(!cond->val_int())
        continue;
    }
    if (num_rows>=offset_limit)
    {
      if (!err)
      {
        String *packet = &thd->packet;
        Item *item;
        packet->length(0);
        it.rewind();
        while ((item=it++))
        {
          if (item->send(packet))
          {
            packet->free();                             // Free used
            my_error(ER_OUT_OF_RESOURCES,MYF(0));
            goto err;
          }
        }
        my_net_write(&thd->net, (char*)packet->ptr(), packet->length());
      }
    }
    num_rows++;
  }
ok:
  mysql_unlock_tables(thd,lock);
  send_eof(&thd->net);
  return 0;
err:
  mysql_unlock_tables(thd,lock);
  return -1;
}

/**************************************************************************
   2Monty: It could easily happen, that the following service functions are
   already defined somewhere in the code, but I failed to find them.
   If this is the case, just say a word and I'll use old functions here.
**************************************************************************/

/* Note: this function differs from find_locked_table() because we're looking
   here for alias, not real table name 
 */
static TABLE **find_table_ptr_by_name(THD *thd, const char *db,
				      const char *table_name)
{
  int dblen;
  TABLE **ptr;
  
  if (!db || ! *db)
    db= thd->db ? thd->db : "";
  dblen=strlen(db)+1;  
  ptr=&(thd->handler_tables);
  
  for (TABLE *table=*ptr; table ; table=*ptr)
  {
    if (!memcmp(table->table_cache_key, db, dblen) &&
        !my_strcasecmp(table->table_name,table_name))
      break;
    ptr=&(table->next);
  }
  return ptr;
}
