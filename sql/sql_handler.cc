/* Copyright (C) 2000-2004 MySQL AB
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

/*
  There are two containers holding information about open handler tables.
  The first is 'thd->handler_tables'. It is a linked list of TABLE objects.
  It is used like 'thd->open_tables' in the table cache. The trick is to
  exchange these two lists during open and lock of tables. Thus the normal
  table cache code can be used.
  The second container is a HASH. It holds objects of the type TABLE_LIST.
  Despite its name, no lists of tables but only single structs are hashed
  (the 'next' pointer is always NULL). The reason for theis second container
  is, that we want handler tables to survive FLUSH TABLE commands. A table
  affected by FLUSH TABLE must be closed so that other threads are not
  blocked by handler tables still in use. Since we use the normal table cache
  functions with 'thd->handler_tables', the closed tables are removed from
  this list. Hence we need the original open information for the handler
  table in the case that it is used again. This information is handed over
  to mysql_ha_open() as a TABLE_LIST. So we store this information in the
  second container, where it is not affected by FLUSH TABLE. The second
  container is implemented as a hash for performance reasons. Consequently,
  we use it not only for re-opening a handler table, but also for the
  HANDLER ... READ commands. For this purpose, we store a pointer to the
  TABLE structure (in the first container) in the TBALE_LIST object in the
  second container. When the table is flushed, the pointer is cleared.
*/

#include "mysql_priv.h"
#include "sql_select.h"
#include <assert.h>

#define HANDLER_TABLES_HASH_SIZE 120

static enum enum_ha_read_modes rkey_to_rnext[]=
    { RNEXT_SAME, RNEXT, RPREV, RNEXT, RPREV, RNEXT, RPREV, RPREV };

#define HANDLER_TABLES_HACK(thd) {      \
  TABLE *tmp=thd->open_tables;          \
  thd->open_tables=thd->handler_tables; \
  thd->handler_tables=tmp; }

static int mysql_ha_flush_table(THD *thd, TABLE **table_ptr, int mode_flags);


/*
  Get hash key and hash key length.

  SYNOPSIS
    mysql_ha_hash_get_key()
    tables                      Pointer to the hash object.
    key_len_p   (out)           Pointer to the result for key length.
    first                       Unused.

  DESCRIPTION
    The hash object is an TABLE_LIST struct.
    The hash key is the alias name.
    The hash key length is the alias name length plus one for the
    terminateing NUL character.

  RETURN
    Pointer to the TABLE_LIST struct.
*/

static char *mysql_ha_hash_get_key(TABLE_LIST *tables, uint *key_len_p,
                                   my_bool first __attribute__((unused)))
{
  *key_len_p= strlen(tables->alias) + 1 ; /* include '\0' in comparisons */
  return tables->alias;
}


/*
  Free an hash object.

  SYNOPSIS
    mysql_ha_hash_free()
    tables                      Pointer to the hash object.

  DESCRIPTION
    The hash object is an TABLE_LIST struct.

  RETURN
    Nothing
*/

static void mysql_ha_hash_free(TABLE_LIST *tables)
{
  my_free((char*) tables, MYF(0));
}


/*
  Open a HANDLER table.

  SYNOPSIS
    mysql_ha_open()
    thd                         Thread identifier.
    tables                      A list of tables with the first entry to open.
    reopen                      Re-open a previously opened handler table.

  DESCRIPTION
    Though this function takes a list of tables, only the first list entry
    will be opened.
    'reopen' is set when a handler table is to be re-opened. In this case,
    'tables' is the pointer to the hashed TABLE_LIST object which has been
    saved on the original open.
    'reopen' is also used to suppress the sending of an 'ok' message or
    error messages.

  RETURN
    0    ok
    != 0 error
*/

int mysql_ha_open(THD *thd, TABLE_LIST *tables, bool reopen)
{
  TABLE_LIST    *hash_tables;
  char          *db, *name, *alias;
  uint          dblen, namelen, aliaslen, counter;
  int           err;
  DBUG_ENTER("mysql_ha_open");
  DBUG_PRINT("enter",("mysql_ha_open: '%s'.'%s' as '%s' reopen %d",
                      tables->db, tables->real_name, tables->alias, reopen));

  if (! hash_inited(&thd->handler_tables_hash))
  {
    /*
      HASH entries are of type TABLE_LIST.
    */
    if (hash_init(&thd->handler_tables_hash, &my_charset_latin1,
                  HANDLER_TABLES_HASH_SIZE, 0, 0,
                  (hash_get_key) mysql_ha_hash_get_key,
                  (hash_free_key) mysql_ha_hash_free, 0))
      goto err;
  }
  else if (! reopen) /* Otherwise we have 'tables' already. */
  {
    if (hash_search(&thd->handler_tables_hash, (byte*) tables->alias,
                    strlen(tables->alias) + 1))
    {
      DBUG_PRINT("info",("mysql_ha_open:  duplicate '%s'", tables->alias));
      if (! reopen)
        my_printf_error(ER_NONUNIQ_TABLE, ER(ER_NONUNIQ_TABLE),
                        MYF(0), tables->alias);
      goto err;
    }
  }

  /*
    open_tables() will set 'tables->table' if successful.
    It must be NULL for a real open when calling open_tables().
  */
  DBUG_ASSERT(! tables->table);
  HANDLER_TABLES_HACK(thd);
  err=open_tables(thd, tables, &counter);
  HANDLER_TABLES_HACK(thd);
  if (err)
    goto err;

  /* There can be only one table in '*tables'. */
  if (! (tables->table->file->table_flags() & HA_CAN_SQL_HANDLER))
  {
    if (! reopen)
      my_printf_error(ER_ILLEGAL_HA,ER(ER_ILLEGAL_HA),MYF(0), tables->alias);
    mysql_ha_close(thd, tables);
    goto err;
  }

  if (! reopen)
  {
    /* copy the TABLE_LIST struct */
    dblen= strlen(tables->db) + 1;
    namelen= strlen(tables->real_name) + 1;
    aliaslen= strlen(tables->alias) + 1;
    if (!(my_multi_malloc(MYF(MY_WME),
                          &hash_tables, sizeof(*hash_tables),
                          &db, dblen,
                          &name, namelen,
                          &alias, aliaslen,
                          NullS)))
    {
      DBUG_PRINT("exit",("mysql_ha_open: malloc ERROR"));
      goto err;
    }
    /* structure copy */
    *hash_tables= *tables;
    hash_tables->db= db;
    hash_tables->real_name= name;
    hash_tables->alias= alias;
    memcpy(hash_tables->db, tables->db, dblen);
    memcpy(hash_tables->real_name, tables->real_name, namelen);
    memcpy(hash_tables->alias, tables->alias, aliaslen);

    /* add to hash */
    if (my_hash_insert(&thd->handler_tables_hash, (byte*) hash_tables))
    {
      mysql_ha_close(thd, tables);
      goto err;
    }
  }

  if (! reopen)
    send_ok(thd);
  DBUG_PRINT("exit",("mysql_ha_open: OK"));
  DBUG_RETURN(0);

err:
  DBUG_PRINT("exit",("mysql_ha_open: ERROR"));
  DBUG_RETURN(-1);
}


/*
  Close a HANDLER table.

  SYNOPSIS
    mysql_ha_close()
    thd                         Thread identifier.
    tables                      A list of tables with the first entry to close.

  DESCRIPTION
    Though this function takes a list of tables, only the first list entry
    will be closed. Broadcasts a COND_refresh condition.

  RETURN
    0    ok
    != 0 error
*/

int mysql_ha_close(THD *thd, TABLE_LIST *tables)
{
  TABLE_LIST    *hash_tables;
  TABLE         **table_ptr;
  bool          was_flushed= FALSE;
  bool          not_opened;
  DBUG_ENTER("mysql_ha_close");
  DBUG_PRINT("enter",("mysql_ha_close: '%s'.'%s' as '%s'",
                      tables->db, tables->real_name, tables->alias));

  if ((hash_tables= (TABLE_LIST*) hash_search(&thd->handler_tables_hash,
                                              (byte*) tables->alias,
                                              strlen(tables->alias) + 1)))
  {
    /*
      Though we could take the table pointer from hash_tables->table,
      we must follow the thd->handler_tables chain anyway, as we need the
      address of the 'next' pointer referencing this table
      for close_thread_table().
    */
    for (table_ptr= &(thd->handler_tables);
         *table_ptr && (*table_ptr != hash_tables->table);
           table_ptr= &(*table_ptr)->next);

#if MYSQL_VERSION_ID < 40100
    if (*tables->db && strcmp(hash_tables->db, tables->db))
    {
      DBUG_PRINT("info",("mysql_ha_close: wrong db"));
      hash_tables= NULL;
    }
    else
#endif
    {
      if (*table_ptr)
      {
        (*table_ptr)->file->ha_index_or_rnd_end();
        VOID(pthread_mutex_lock(&LOCK_open));
        if (close_thread_table(thd, table_ptr))
        {
          /* Tell threads waiting for refresh that something has happened */
          VOID(pthread_cond_broadcast(&COND_refresh));
        }
        VOID(pthread_mutex_unlock(&LOCK_open));
      }

      hash_delete(&thd->handler_tables_hash, (byte*) hash_tables);
    }
  }

  if (! hash_tables)
  {
#if MYSQL_VERSION_ID < 40100
    char buff[MAX_DBKEY_LENGTH];
    if (*tables->db)
      strxnmov(buff, sizeof(buff), tables->db, ".", tables->real_name, NullS);
    else
      strncpy(buff, tables->alias, sizeof(buff));
    my_printf_error(ER_UNKNOWN_TABLE, ER(ER_UNKNOWN_TABLE), MYF(0),
                    buff, "HANDLER");
#else
    my_printf_error(ER_UNKNOWN_TABLE, ER(ER_UNKNOWN_TABLE), MYF(0),
                    tables->alias, "HANDLER");
#endif
    DBUG_PRINT("exit",("mysql_ha_close: ERROR"));
    DBUG_RETURN(-1);
  }

  send_ok(thd);
  DBUG_PRINT("exit",("mysql_ha_close: OK"));
  DBUG_RETURN(0);
}


/*
  Read from a HANDLER table.

  SYNOPSIS
    mysql_ha_read()
    thd                         Thread identifier.
    tables                      A list of tables with the first entry to read.
    mode
    keyname
    key_expr
    ha_rkey_mode
    cond
    select_limit
    offset_limit

  RETURN
    0    ok
    != 0 error
*/
 
int mysql_ha_read(THD *thd, TABLE_LIST *tables,
    enum enum_ha_read_modes mode, char *keyname, List<Item> *key_expr,
    enum ha_rkey_function ha_rkey_mode, Item *cond,
    ha_rows select_limit,ha_rows offset_limit)
{
  TABLE_LIST    *hash_tables;
  TABLE         *table;
  MYSQL_LOCK    *lock;
  List<Item>	list;
  Protocol	*protocol= thd->protocol;
  char		buff[MAX_FIELD_WIDTH];
  String	buffer(buff, sizeof(buff), system_charset_info);
  int           err, keyno= -1;
  uint          num_rows;
  byte		*key;
  uint		key_len;
  bool          was_flushed;
  DBUG_ENTER("mysql_ha_read");
  DBUG_PRINT("enter",("mysql_ha_read: '%s'.'%s' as '%s'",
                      tables->db, tables->real_name, tables->alias));

  LINT_INIT(key);
  LINT_INIT(key_len);

  list.push_front(new Item_field(NULL,NULL,"*"));
  List_iterator<Item> it(list);
  it++;

  if ((hash_tables= (TABLE_LIST*) hash_search(&thd->handler_tables_hash,
                                              (byte*) tables->alias,
                                              strlen(tables->alias) + 1)))
  {
    table= hash_tables->table;
    DBUG_PRINT("info",("mysql_ha_read: found in hash '%s'.'%s' as '%s' tab %p",
                       hash_tables->db, hash_tables->real_name,
                       hash_tables->alias, table));
    if (!table)
    {
      /*
        The handler table has been closed. Re-open it.
      */
      if (mysql_ha_open(thd, hash_tables, 1))
      {
        DBUG_PRINT("exit",("mysql_ha_read: reopen failed"));
        goto err0;
      }

      table= hash_tables->table;
      DBUG_PRINT("info",("mysql_ha_read: re-opened '%s'.'%s' as '%s' tab %p",
                         hash_tables->db, hash_tables->real_name,
                         hash_tables->alias, table));
    }

#if MYSQL_VERSION_ID < 40100
    if (*tables->db && strcmp(table->table_cache_key, tables->db))
    {
      DBUG_PRINT("info",("mysql_ha_read: wrong db"));
      table= NULL;
    }
#endif
  }
  else
    table= NULL;

  if (!table)
  {
#if MYSQL_VERSION_ID < 40100
    char buff[MAX_DBKEY_LENGTH];
    if (*tables->db)
      strxnmov(buff, sizeof(buff), tables->db, ".", tables->real_name, NullS);
    else
      strncpy(buff, tables->alias, sizeof(buff));
    my_printf_error(ER_UNKNOWN_TABLE, ER(ER_UNKNOWN_TABLE), MYF(0),
                    buff, "HANDLER");
#else
    my_printf_error(ER_UNKNOWN_TABLE, ER(ER_UNKNOWN_TABLE), MYF(0),
                    tables->alias, "HANDLER");
#endif
    goto err0;
  }
  tables->table=table;

  if (cond && (cond->fix_fields(thd, tables, &cond) || cond->check_cols(1)))
    goto err0;

  table->file->init_table_handle_for_HANDLER(); // Only InnoDB requires it

  if (keyname)
  {
    if ((keyno=find_type(keyname, &table->keynames, 1+2)-1)<0)
    {
      my_printf_error(ER_KEY_DOES_NOT_EXITS,ER(ER_KEY_DOES_NOT_EXITS),MYF(0),
          keyname,tables->alias);
      goto err0;
    }
    table->file->ha_index_or_rnd_end();
    table->file->ha_index_init(keyno);
  }

  if (insert_fields(thd,tables,tables->db,tables->alias,&it))
    goto err0;

  select_limit+=offset_limit;
  protocol->send_fields(&list,1);

  HANDLER_TABLES_HACK(thd);
  lock= mysql_lock_tables(thd, &tables->table, 1);
  HANDLER_TABLES_HACK(thd);

  if (!lock)
     goto err0; // mysql_lock_tables() printed error message already

  /*
    In ::external_lock InnoDB resets the fields which tell it that
    the handle is used in the HANDLER interface. Tell it again that
    we are using it for HANDLER.
  */

  table->file->init_table_handle_for_HANDLER();

  for (num_rows=0; num_rows < select_limit; )
  {
    switch (mode) {
    case RFIRST:
      if (keyname)
        err=table->file->index_first(table->record[0]);
      else
      {
        table->file->ha_index_or_rnd_end();
	if (!(err=table->file->ha_rnd_init(1)))
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
    case RNEXT_SAME:
      /* Continue scan on "(keypart1,keypart2,...)=(c1, c2, ...)  */
      DBUG_ASSERT(keyname != 0);
      err= table->file->index_next_same(table->record[0], key, key_len);
      break;
    case RKEY:
    {
      DBUG_ASSERT(keyname != 0);
      KEY *keyinfo=table->key_info+keyno;
      KEY_PART_INFO *key_part=keyinfo->key_part;
      if (key_expr->elements > keyinfo->key_parts)
      {
	my_printf_error(ER_TOO_MANY_KEY_PARTS,ER(ER_TOO_MANY_KEY_PARTS),
			MYF(0),keyinfo->key_parts);
	goto err;
      }
      List_iterator<Item> it_ke(*key_expr);
      Item *item;
      for (key_len=0 ; (item=it_ke++) ; key_part++)
      {
	// 'item' can be changed by fix_fields() call
	if (item->fix_fields(thd, tables, it_ke.ref()) ||
	    (item= *it_ke.ref())->check_cols(1))
	  goto err;
	if (item->used_tables() & ~RAND_TABLE_BIT)
        {
          my_error(ER_WRONG_ARGUMENTS,MYF(0),"HANDLER ... READ");
	  goto err;
        }
	(void) item->save_in_field(key_part->field, 1);
	key_len+=key_part->store_length;
      }
      if (!(key= (byte*) thd->calloc(ALIGN_SIZE(key_len))))
      {
	send_error(thd,ER_OUTOFMEMORY);
	goto err;
      }
      key_copy(key, table, keyno, key_len);
      err=table->file->index_read(table->record[0],
				  key,key_len,ha_rkey_mode);
      mode=rkey_to_rnext[(int)ha_rkey_mode];
      break;
    }
    default:
      send_error(thd,ER_ILLEGAL_HA);
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
      Item *item;
      protocol->prepare_for_resend();
      it.rewind();
      while ((item=it++))
      {
	if (item->send(thd->protocol, &buffer))
	{
	  protocol->free();                             // Free used
	  my_error(ER_OUT_OF_RESOURCES,MYF(0));
	  goto err;
	}
      }
      protocol->write();
    }
    num_rows++;
  }
ok:
  mysql_unlock_tables(thd,lock);
  send_eof(thd);
  DBUG_PRINT("exit",("mysql_ha_read: OK"));
  DBUG_RETURN(0);
err:
  mysql_unlock_tables(thd,lock);
err0:
  DBUG_PRINT("exit",("mysql_ha_read: ERROR"));
  DBUG_RETURN(-1);
}


/*
  Flush (close) a list of HANDLER tables.

  SYNOPSIS
    mysql_ha_flush()
    thd                         Thread identifier.
    tables                      The list of tables to close. If NULL,
                                close all HANDLER tables [marked as flushed].
    mode_flags                  MYSQL_HA_CLOSE_FINAL finally close the table.
                                MYSQL_HA_REOPEN_ON_USAGE mark for reopen.
                                MYSQL_HA_FLUSH_ALL flush all tables, not only
                                those marked for flush.

  DESCRIPTION
    The list of HANDLER tables may be NULL, in which case all HANDLER
    tables are closed (if MYSQL_HA_FLUSH_ALL) is set.
    If 'tables' is NULL and MYSQL_HA_FLUSH_ALL is not set,
    all HANDLER tables marked for flush are closed.
    Broadcasts a COND_refresh condition, for every table closed.
    The caller must lock LOCK_open.

  NOTE
    Since mysql_ha_flush() is called when the base table has to be closed,
    we compare real table names, not aliases. Hence, database names matter.

  RETURN
    0  ok
*/

int mysql_ha_flush(THD *thd, TABLE_LIST *tables, int mode_flags)
{
  TABLE_LIST    **tmp_tables_p;
  TABLE_LIST    *tmp_tables;
  TABLE         **table_ptr;
  bool          was_flushed;
  DBUG_ENTER("mysql_ha_flush");
  DBUG_PRINT("enter",("mysql_ha_flush: tables %p mode_flags 0x%02x",
                      tables, mode_flags));

  if (tables)
  {
    /* Close all tables in the list. */
    for (tmp_tables= tables ; tmp_tables; tmp_tables= tmp_tables->next)
    {
      DBUG_PRINT("info",("mysql_ha_flush: in tables list '%s'.'%s' as '%s'",
                         tmp_tables->db, tmp_tables->real_name,
                         tmp_tables->alias));
      /* Close all currently open handler tables with the same base table. */
      table_ptr= &(thd->handler_tables);
      while (*table_ptr)
      {
        if ((! *tmp_tables->db ||
             ! my_strcasecmp(&my_charset_latin1, (*table_ptr)->table_cache_key,
                             tmp_tables->db)) &&
            ! my_strcasecmp(&my_charset_latin1, (*table_ptr)->real_name,
                            tmp_tables->real_name))
        {
          DBUG_PRINT("info",("mysql_ha_flush: *table_ptr '%s'.'%s' as '%s'",
                             (*table_ptr)->table_cache_key,
                             (*table_ptr)->real_name,
                             (*table_ptr)->table_name));
          mysql_ha_flush_table(thd, table_ptr, mode_flags);
          continue;
        }
        table_ptr= &(*table_ptr)->next;
      }
      /* end of handler_tables list */
    }
    /* end of flush tables list */
  }
  else
  {
    /* Close all currently open tables [which are marked for flush]. */
    table_ptr= &(thd->handler_tables);
    while (*table_ptr)
    {
      if ((mode_flags & MYSQL_HA_FLUSH_ALL) ||
          ((*table_ptr)->version != refresh_version))
      {
        mysql_ha_flush_table(thd, table_ptr, mode_flags);
        continue;
      }
      table_ptr= &(*table_ptr)->next;
    }
  }

  DBUG_PRINT("exit",("mysql_ha_flush: OK"));
  DBUG_RETURN(0);
}

/*
  Flush (close) a table.

  SYNOPSIS
    mysql_ha_flush_table()
    thd                         Thread identifier.
    table                       The table to close.
    mode_flags                  MYSQL_HA_CLOSE_FINAL finally close the table.
                                MYSQL_HA_REOPEN_ON_USAGE mark for reopen.

  DESCRIPTION
    Broadcasts a COND_refresh condition, for every table closed.
    The caller must lock LOCK_open.

  RETURN
    0  ok
*/

static int mysql_ha_flush_table(THD *thd, TABLE **table_ptr, int mode_flags)
{
  TABLE_LIST    *hash_tables;
  TABLE         *table= *table_ptr;
  bool          was_flushed;
  DBUG_ENTER("mysql_ha_flush_table");
  DBUG_PRINT("info",("mysql_ha_flush_table: '%s'.'%s' as '%s' flags 0x%02x",
                     table->table_cache_key, table->real_name,
                     table->table_name, mode_flags));

  if ((hash_tables= (TABLE_LIST*) hash_search(&thd->handler_tables_hash,
                                        (*table_ptr)->table_name,
                                        strlen((*table_ptr)->table_name) + 1)))
  {
    if (! (mode_flags & MYSQL_HA_REOPEN_ON_USAGE))
    {
      /* This is a final close. Remove from hash. */
      hash_delete(&thd->handler_tables_hash, (byte*) hash_tables);
    }
    else
    {
      /* Mark table as closed, ready for re-open. */
      hash_tables->table= NULL;
    }
  }    

  (*table_ptr)->file->ha_index_or_rnd_end();
  if (close_thread_table(thd, table_ptr))
  {
    /* Tell threads waiting for refresh that something has happened */
    VOID(pthread_cond_broadcast(&COND_refresh));
  }

  DBUG_PRINT("exit",("mysql_ha_flush_table: OK"));
  DBUG_RETURN(0);
}
