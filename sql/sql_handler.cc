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

static TABLE *find_table_by_name(THD *thd, char *db, char *table_name);

int mysql_ha_open(THD *thd, TABLE_LIST *tables)
{
  int err=open_tables(thd,tables);
  if (!err)
  {
    thd->manual_open=1;
    send_ok(&thd->net);
  }
}

int mysql_ha_close(THD *thd, TABLE_LIST *tables)
{
  send_ok(&thd->net);
  return 0;
}

int mysql_ha_read(THD *thd, TABLE_LIST *tables,
    enum enum_ha_read_modes mode, char *keyname, List<Item> *key_expr,
    enum ha_rkey_function ha_rkey_mode)
{
  int err;
  TABLE *table=find_table_by_name(thd, tables->db, tables->name);
  if (!table)
  {
    my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),
	tables->name,"HANDLER");
    // send_error(&thd->net,ER_UNKNOWN_TABLE);
    // send_ok(&thd->net);
    return -1;
  }
  tables->table=table;
  
  int keyno=find_type(keyname, &table->keynames, 1+2)-1;
  if (keyno<0)
  {
    my_printf_error(ER_KEY_DOES_NOT_EXITS,ER(ER_KEY_DOES_NOT_EXITS),MYF(0),
	keyname,tables->name);
    return -1;
  }

  List<Item> list;
  list.push_front(new Item_field(NULL,NULL,"*"));
  List_iterator<Item> it(list);
  it++;

  insert_fields(thd,tables,tables->name,&it);
  
  table->file->index_init(keyno);
  
  switch(mode)
  {
    case RFIRST:
       err=table->file->index_first(table->record[0]);
       break;
    case RLAST:
       err=table->file->index_last(table->record[0]);
       break;
    case RNEXT:
       err=table->file->index_next(table->record[0]);
       break;
    case RPREV:
        err=table->file->index_prev(table->record[0]);
        break;
    case RKEY:
      {
        KEY *keyinfo=table->key_info+keyno;
	uint key_len=0, i;
	byte *key, *buf;
	for (i=0; i < key_expr->elements; i++)
	  key_len+=keyinfo->key_part[i].store_length;
	if (!(key=sql_calloc(ALIGN_SIZE(key_len))))
	{
	  send_error(&thd->net,ER_OUTOFMEMORY);
	  exit(-1); 
	}
        List_iterator<Item> it_ke(*key_expr);
      	for (i=0, buf=key; i < key_expr->elements; i++)
	{
          uint maybe_null= test(keyinfo->key_part[i].null_bit);
	  store_key_item ski=store_key_item(keyinfo->key_part[i].field,
	      (char*)buf+maybe_null,maybe_null ? (char*) buf : 0,
	      keyinfo->key_part[i].length, it_ke++);
	  ski.copy();
	  buf+=keyinfo->key_part[i].store_length;
	}
        err=table->file->index_read(table->record[0],
	    key,key_len,ha_rkey_mode);
        break;
      }
    default:
	send_error(&thd->net,ER_ILLEGAL_HA);
        exit(-1); 
  }
  
  if (err && err != HA_ERR_KEY_NOT_FOUND && err != HA_ERR_END_OF_FILE)
  {
    sql_print_error("mysql_ha_read: Got error %d when reading table",
                      err);
    table->file->print_error(err,MYF(0));
    return -1;
  }
  send_fields(thd,list,1);
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
        packet->free();				// Free used
        my_error(ER_OUT_OF_RESOURCES,MYF(0));
        return -1;
      }
    }
    my_net_write(&thd->net, (char*)packet->ptr(), packet->length());
  }
  send_eof(&thd->net);
  return 0;
}

/**************************************************************************
   2Monty: It could easily happen, that the following service functions are
   already defined somewhere in the code, but I failed to find them.
   If this is the case, just say a word and I'll use old functions here.
**************************************************************************/

/* Note: this function differs from find_locked_table() because we're looking
   here for alias, not real table name 
 */
static TABLE *find_table_by_name(THD *thd, char *db, char *table_name)
{
  int dblen;
  
  if (!db || ! *db) db=thd->db;
  if (!db || ! *db) db="";
  dblen=strlen(db);  
  
  for (TABLE *table=thd->open_tables; table ; table=table->next)
  {
    if (!memcmp(table->table_cache_key, db, dblen) &&
	!my_strcasecmp(table->table_name,table_name))
      return table;
  }
  return(0);
}
