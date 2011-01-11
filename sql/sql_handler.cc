/* Copyright (C) 2000-2004 MySQL AB
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include "sql_handler.h"

#define HANDLER_TABLES_HASH_SIZE 120

static enum enum_ha_read_modes rkey_to_rnext[]=
{ RNEXT_SAME, RNEXT, RPREV, RNEXT, RPREV, RNEXT, RPREV, RPREV };

/*
  Set handler to state after create, but keep base information about
  which table is used
*/

void SQL_HANDLER::reset()
{
  fields.empty();
  arena.free_items();
  free_root(&mem_root, MYF(0));
  my_free(lock, MYF(MY_ALLOW_ZERO_PTR));
  init();
}  
  
/* Free all allocated data */

SQL_HANDLER::~SQL_HANDLER()
{
  reset();
  my_free(base_data, MYF(MY_ALLOW_ZERO_PTR));
}

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

static char *mysql_ha_hash_get_key(SQL_HANDLER *table, size_t *key_len,
                                   my_bool first __attribute__((unused)))
{
  *key_len= table->handler_name.length + 1 ; /* include '\0' in comparisons */
  return table->handler_name.str;
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

static void mysql_ha_hash_free(SQL_HANDLER *table)
{
  delete table;
}

/**
  Close a HANDLER table.

  @param thd Thread identifier.
  @param tables A list of tables with the first entry to close.
  @param is_locked If LOCK_open is locked.

  @note Though this function takes a list of tables, only the first list entry
  will be closed.
  @mote handler_object is not deleted!
  @note Broadcasts refresh if it closed a table with old version.
*/

static void mysql_ha_close_table(SQL_HANDLER *handler,
                                 bool is_locked)
{
  THD *thd= handler->thd;
  TABLE *table= handler->table;
  TABLE **table_ptr;

  /* check if table was already closed */
  if (!table)
    return;

  /*
    Though we could take the table pointer from hash_tables->table,
    we must follow the thd->handler_tables chain anyway, as we need the
    address of the 'next' pointer referencing this table
    for close_thread_table().
  */
  for (table_ptr= &(thd->handler_tables);
       *table_ptr && (*table_ptr != table);
         table_ptr= &(*table_ptr)->next)
    ;

  if (*table_ptr)
  {
    if (handler->lock)
    {
      // Mark it unlocked, like in reset_lock_data()
      reset_lock_data(handler->lock, 1);
    }
    table->file->ha_index_or_rnd_end();
    if (! is_locked)
      VOID(pthread_mutex_lock(&LOCK_open));
    if (close_thread_table(thd, table_ptr))
    {
      /* Tell threads waiting for refresh that something has happened */
      broadcast_refresh();
    }
    if (! is_locked)
      VOID(pthread_mutex_unlock(&LOCK_open));
  }
  else
  {
    /* Must be a temporary table */
    table->file->ha_index_or_rnd_end();
    table->query_id= thd->query_id;
    table->open_by_handler= 0;
  }
  my_free(handler->lock, MYF(MY_ALLOW_ZERO_PTR));
  handler->init();
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
    'tables' is the pointer to the hashed SQL_HANDLER object which has been
    saved on the original open.
    'reopen' is also used to suppress the sending of an 'ok' message.

  RETURN
    FALSE OK
    TRUE  Error
*/

bool mysql_ha_open(THD *thd, TABLE_LIST *tables, SQL_HANDLER *reopen)
{
  SQL_HANDLER   *sql_handler= 0;
  uint          counter;
  int           error;
  TABLE         *table, *backup_open_tables, *write_lock_used;
  Query_arena backup_arena;
  DBUG_ENTER("mysql_ha_open");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'  reopen: %d",
                      tables->db, tables->table_name, tables->alias,
                      reopen != 0));

  if (tables->schema_table)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "HANDLER OPEN",
             INFORMATION_SCHEMA_NAME.str);
    DBUG_PRINT("exit",("ERROR"));
    DBUG_RETURN(TRUE);
  }

  if (! hash_inited(&thd->handler_tables_hash))
  {
    /*
      HASH entries are of type SQL_HANDLER
    */
    if (hash_init(&thd->handler_tables_hash, &my_charset_latin1,
                  HANDLER_TABLES_HASH_SIZE, 0, 0,
                  (hash_get_key) mysql_ha_hash_get_key,
                  (hash_free_key) mysql_ha_hash_free, 0))
      goto err;
  }
  else if (! reopen) /* Otherwise we have 'tables' already. */
  {
    if (hash_search(&thd->handler_tables_hash, (uchar*) tables->alias,
                    strlen(tables->alias) + 1))
    {
      DBUG_PRINT("info",("duplicate '%s'", tables->alias));
      my_error(ER_NONUNIQ_TABLE, MYF(0), tables->alias);
      goto err;
    }
  }

  /*
    Save and reset the open_tables list so that open_tables() won't
    be able to access (or know about) the previous list. And on return
    from open_tables(), thd->open_tables will contain only the opened
    table.

    The thd->handler_tables list is kept as-is to avoid deadlocks if
    open_table(), called by open_tables(), needs to back-off because
    of a pending name-lock on the table being opened.

    See open_table() back-off comments for more details.
  */
  backup_open_tables= thd->open_tables;
  thd->open_tables= NULL;

  /*
    open_tables() will set 'tables->table' if successful.
    It must be NULL for a real open when calling open_tables().
  */
  DBUG_ASSERT(! tables->table);

  /* for now HANDLER can be used only for real TABLES */
  tables->required_type= FRMTYPE_TABLE;
  /*
    We use open_tables() here, rather than, say,
    open_ltable() or open_table() because we would like to be able
    to open a temporary table.
  */
  error= open_tables(thd, &tables, &counter, 0);
  if (thd->open_tables)
  {
    if (thd->open_tables->next)
    {
      /*
        We opened something that is more than a single table.
        This happens with MERGE engine. Don't try to link
        this mess into thd->handler_tables list, close it
        and report an error. We must do it right away
        because mysql_ha_close_table(), called down the road,
        can close a single table only.
      */
      close_thread_tables(thd);
      my_error(ER_ILLEGAL_HA, MYF(0), tables->alias);
      error= 1;
    }
    else
    {
      /* Merge the opened table into handler_tables list. */
      thd->open_tables->next= thd->handler_tables;
      thd->handler_tables= thd->open_tables;
    }
  }

  /* Restore the state. */
  thd->open_tables= backup_open_tables;

  if (error)
    goto err;

  table= tables->table;

  /* There can be only one table in '*tables'. */
  if (! (table->file->ha_table_flags() & HA_CAN_SQL_HANDLER))
  {
    my_error(ER_ILLEGAL_HA, MYF(0), tables->alias);
    goto err;
  }

  if (! reopen)
  {
    /* copy data to sql_handler */
    if (!(sql_handler= new SQL_HANDLER(thd)))
      goto err;
    init_alloc_root(&sql_handler->mem_root, 1024, 0);

    sql_handler->table= table;
    sql_handler->db.length= strlen(tables->db);
    sql_handler->table_name.length= strlen(tables->table_name);
    sql_handler->handler_name.length= strlen(tables->alias);

    if (!(my_multi_malloc(MY_WME,
                          &sql_handler->db.str,
                          (uint) sql_handler->db.length + 1,
                          &sql_handler->table_name.str,
                          (uint) sql_handler->table_name.length + 1,
                          &sql_handler->handler_name.str,
                          (uint) sql_handler->handler_name.length + 1,
                          NullS)))
      goto err;
    sql_handler->base_data= sql_handler->db.str;  // Free this
    memcpy(sql_handler->db.str, tables->db, sql_handler->db.length +1);
    memcpy(sql_handler->table_name.str, tables->table_name,
           sql_handler->table_name.length+1);
    memcpy(sql_handler->handler_name.str, tables->alias,
           sql_handler->handler_name.length +1);

    /* add to hash */
    if (my_hash_insert(&thd->handler_tables_hash, (uchar*) sql_handler))
      goto err;
  }
  else
  {
    sql_handler= reopen;
    sql_handler->reset();
  }    
  sql_handler->table= table;

  if (!(sql_handler->lock= get_lock_data(thd, &sql_handler->table, 1,
                                         GET_LOCK_STORE_LOCKS,
                                         &write_lock_used)))
    goto err;

  /* Get a list of all fields for send_fields */
  thd->set_n_backup_active_arena(&sql_handler->arena, &backup_arena);
  error= table->fill_item_list(&sql_handler->fields);
  thd->restore_active_arena(&sql_handler->arena, &backup_arena);

  if (error)
  {
    if (reopen)
      sql_handler= 0;
    goto err;
  }

  /* Always read all columns */
  table->read_set= &table->s->all_set;
  table->vcol_set= &table->s->all_set;

  /*
    If it's a temp table, don't reset table->query_id as the table is
    being used by this handler. Otherwise, no meaning at all.
  */
  table->open_by_handler= 1;

  if (! reopen)
    my_ok(thd);
  DBUG_PRINT("exit",("OK"));
  DBUG_RETURN(FALSE);

err:
  delete sql_handler;
  if (tables->table)
  {
    SQL_HANDLER tmp_sql_handler(thd);
    tmp_sql_handler.table= tables->table;
    mysql_ha_close_table(&tmp_sql_handler, FALSE);
  }
  DBUG_PRINT("exit",("ERROR"));
  DBUG_RETURN(TRUE);
}


/*
  Close a HANDLER table by alias or table name

  SYNOPSIS
    mysql_ha_close()
    thd                         Thread identifier.
    tables                      A list of tables with the first entry to close.

  DESCRIPTION
    Closes the table that is associated (on the handler tables hash) with the
    name (table->alias) of the specified table.

  RETURN
    FALSE ok
    TRUE  error
*/

bool mysql_ha_close(THD *thd, TABLE_LIST *tables)
{
  SQL_HANDLER *handler;
  DBUG_ENTER("mysql_ha_close");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'",
                      tables->db, tables->table_name, tables->alias));

  if ((handler= (SQL_HANDLER*) hash_search(&thd->handler_tables_hash,
                                           (uchar*) tables->alias,
                                           strlen(tables->alias) + 1)))
  {
    mysql_ha_close_table(handler, FALSE);
    hash_delete(&thd->handler_tables_hash, (uchar*) handler);
  }
  else
  {
    my_error(ER_UNKNOWN_TABLE, MYF(0), tables->alias, "HANDLER");
    DBUG_PRINT("exit",("ERROR"));
    DBUG_RETURN(TRUE);
  }

  my_ok(thd);
  DBUG_PRINT("exit", ("OK"));
  DBUG_RETURN(FALSE);
}


/**
   Finds an open HANDLER table.

   @params name		Name of handler to open

   @return 0 failure
   @return handler
*/  

SQL_HANDLER *mysql_ha_find_handler(THD *thd, const char *name)
{
  SQL_HANDLER *handler;
  if ((handler= (SQL_HANDLER*) hash_search(&thd->handler_tables_hash,
                                           (uchar*) name,
                                           strlen(name) + 1)))
  {
    DBUG_PRINT("info-in-hash",("'%s'.'%s' as '%s' table: %p",
                               handler->db.str,
                               handler->table_name.str,
                               handler->handler_name.str, handler->table));
    if (!handler->table)
    {
      /* The handler table has been closed. Re-open it. */
      TABLE_LIST tmp;
      tmp.init_one_table(handler->db.str, handler->table_name.str,
                         TL_READ);
      tmp.alias= handler->handler_name.str;

      if (mysql_ha_open(thd, &tmp, handler))
      {
        DBUG_PRINT("exit",("reopen failed"));
        return 0;
      }
    }
  }
  else
  {
    my_error(ER_UNKNOWN_TABLE, MYF(0), name, "HANDLER");
    return 0;
  }
  return handler;
}


/**
   Check that condition and key name are ok

   @param handler
   @param mode		Read mode (RFIRST, RNEXT etc...)
   @param keyname	Key to use.
   @param key_expr      List of key column values
   @param cond		Where clause
   @param in_prepare	If we are in prepare phase (we can't evalute items yet)

   @return 0 ok
   @return 1 error

   In ok, then values of used key and mode is stored in sql_handler
*/

static bool
mysql_ha_fix_cond_and_key(SQL_HANDLER *handler, 
                          enum enum_ha_read_modes mode, char *keyname,
                          List<Item> *key_expr,
                          Item *cond, bool in_prepare)
{
  THD *thd= handler->thd;
  TABLE *table= handler->table;
  if (cond)
  {
    /* This can only be true for temp tables */
    if (table->query_id != thd->query_id)
      cond->cleanup();                          // File was reopened
    if ((!cond->fixed &&
	 cond->fix_fields(thd, &cond)) || cond->check_cols(1))
      return 1;
  }

  if (keyname)
  {
    /* Check if same as last keyname. If not, do a full lookup */
    if (handler->keyno < 0 ||
        my_strcasecmp(&my_charset_latin1,
                      keyname,
                      table->s->key_info[handler->keyno].name))
    {
      if ((handler->keyno= find_type(keyname, &table->s->keynames, 1+2)-1)<0)
      {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), keyname,
                 handler->handler_name);
        return 1;
      }
    }

    /* Check key parts */
    if (mode == RKEY)
    {
      TABLE *table= handler->table;
      KEY *keyinfo= table->key_info + handler->keyno;
      KEY_PART_INFO *key_part= keyinfo->key_part;
      List_iterator<Item> it_ke(*key_expr);
      Item *item;
      key_part_map keypart_map;
      uint key_len;

      if (key_expr->elements > keyinfo->key_parts)
      {
        my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), keyinfo->key_parts);
        return 1;
      }
      for (keypart_map= key_len=0 ; (item=it_ke++) ; key_part++)
      {
        my_bitmap_map *old_map;
	/* note that 'item' can be changed by fix_fields() call */
        if ((!item->fixed &&
             item->fix_fields(thd, it_ke.ref())) ||
	    (item= *it_ke.ref())->check_cols(1))
          return 1;
	if (item->used_tables() & ~(RAND_TABLE_BIT | PARAM_TABLE_BIT))
        {
          my_error(ER_WRONG_ARGUMENTS,MYF(0),"HANDLER ... READ");
	  return 1;
        }
        if (!in_prepare)
        {
          old_map= dbug_tmp_use_all_columns(table, table->write_set);
          (void) item->save_in_field(key_part->field, 1);
          dbug_tmp_restore_column_map(table->write_set, old_map);
        }
        key_len+= key_part->store_length;
        keypart_map= (keypart_map << 1) | 1;
      }
      handler->keypart_map= keypart_map;
      handler->key_len= key_len;
    }
    else
    {
      /*
        Check if the same index involved.
        We need to always do this check because we may not have yet
        called the handler since the last keyno change.
      */
      if ((uint) handler->keyno != table->file->get_index())
      {
        if (mode == RNEXT)
          mode= RFIRST;
        else if (mode == RPREV)
          mode= RLAST;
      }
    }
  }
  else if (table->file->inited != handler::RND)
  {
    /* Convert RNEXT to RFIRST if we haven't started row scan */
    if (mode == RNEXT)
      mode= RFIRST;
  }
  handler->mode= mode;                          // Store adjusted mode
  return 0;
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
    select_limit_cnt
    offset_limit_cnt

  RETURN
    FALSE ok
    TRUE  error
*/
 
bool mysql_ha_read(THD *thd, TABLE_LIST *tables,
                   enum enum_ha_read_modes mode, char *keyname,
                   List<Item> *key_expr,
                   enum ha_rkey_function ha_rkey_mode, Item *cond,
                   ha_rows select_limit_cnt, ha_rows offset_limit_cnt)
{
  SQL_HANDLER   *handler;
  TABLE         *table;
  List<Item>	list;
  Protocol	*protocol= thd->protocol;
  char		buff[MAX_FIELD_WIDTH];
  String	buffer(buff, sizeof(buff), system_charset_info);
  int           error, keyno;
  uint          num_rows;
  uchar		*UNINIT_VAR(key);
  uint		UNINIT_VAR(key_len);
  bool          need_reopen;
  List_iterator<Item> it;
  DBUG_ENTER("mysql_ha_read");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'",
                      tables->db, tables->table_name, tables->alias));


retry:
  if (!(handler= mysql_ha_find_handler(thd, tables->alias)))
    goto err0;

  table= handler->table;
  tables->table= table;                         // This is used by fix_fields

  /* save open_tables state */
  if (handler->lock->lock_count > 0)
  {
    bool lock_error;

    handler->lock->locks[0]->type= handler->lock->locks[0]->org_type;
    lock_error= mysql_lock_tables(thd, handler->lock, 0, 
                                  (MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN |
                                   (handler->table->s->tmp_table ==
                                    NO_TMP_TABLE ?
                                    MYSQL_LOCK_NOT_TEMPORARY : 0)),
                                  &need_reopen);
    if (need_reopen)
    {
      mysql_ha_close_table(handler, FALSE);
      if (thd->stmt_arena->is_stmt_execute())
      {
        /*
          As we have already sent field list and types to the client, we can't
          handle any changes in the table format for prepared statements.
          Better to force a reprepare.
        */
        my_error(ER_NEED_REPREPARE, MYF(0));
        goto err0;
      }

      /*
        The lock might have been aborted, we need to manually reset
        thd->some_tables_deleted because handler's tables are closed
        in a non-standard way. Otherwise we might loop indefinitely.
      */
      thd->some_tables_deleted= 0;
      goto retry;
    }

    if (lock_error)
      goto err0; // mysql_lock_tables() printed error message already
  }

  if (mysql_ha_fix_cond_and_key(handler, mode, keyname, key_expr, cond, 0))
    goto err;
  mode= handler->mode;
  keyno= handler->keyno;

  it.init(handler->fields);
  protocol->send_fields(&handler->fields,
                        Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  /*
    In ::external_lock InnoDB resets the fields which tell it that
    the handle is used in the HANDLER interface. Tell it again that
    we are using it for HANDLER.
  */

  table->file->init_table_handle_for_HANDLER();

  for (num_rows=0; num_rows < select_limit_cnt; )
  {
    switch (mode) {
    case RNEXT:
      if (table->file->inited != handler::NONE)
      {
        if ((error= table->file->can_continue_handler_scan()))
          break;
        if (keyname)
        {
          /* Check if we read from the same index. */
          DBUG_ASSERT((uint) keyno == table->file->get_index());
          error= table->file->ha_index_next(table->record[0]);
        }
        else
          error= table->file->ha_rnd_next(table->record[0]);
        break;
      }
      /* else fall through */
    case RFIRST:
      if (keyname)
      {
        table->file->ha_index_or_rnd_end();
        table->file->ha_index_init(keyno, 1);
        error= table->file->ha_index_first(table->record[0]);
      }
      else
      {
        table->file->ha_index_or_rnd_end();
	if (!(error= table->file->ha_rnd_init(1)))
          error= table->file->ha_rnd_next(table->record[0]);
      }
      mode= RNEXT;
      break;
    case RPREV:
      DBUG_ASSERT(keyname != 0);
      /* Check if we read from the same index. */
      DBUG_ASSERT((uint) keyno == table->file->get_index());
      if (table->file->inited != handler::NONE)
      {
        if ((error= table->file->can_continue_handler_scan()))
          break;
        error= table->file->ha_index_prev(table->record[0]);
        break;
      }
      /* else fall through */
    case RLAST:
      DBUG_ASSERT(keyname != 0);
      table->file->ha_index_or_rnd_end();
      table->file->ha_index_init(keyno, 1);
      error= table->file->ha_index_last(table->record[0]);
      mode= RPREV;
      break;
    case RNEXT_SAME:
      /* Continue scan on "(keypart1,keypart2,...)=(c1, c2, ...)  */
      DBUG_ASSERT(keyname != 0);
      error= table->file->ha_index_next_same(table->record[0], key, key_len);
      break;
    case RKEY:
    {
      DBUG_ASSERT(keyname != 0);

      if (!(key= (uchar*) thd->calloc(ALIGN_SIZE(handler->key_len))))
	goto err;
      table->file->ha_index_or_rnd_end();
      table->file->ha_index_init(keyno, 1);
      key_copy(key, table->record[0], table->key_info + keyno,
               handler->key_len);
      error= table->file->ha_index_read_map(table->record[0],
                                            key, handler->keypart_map,
                                            ha_rkey_mode);
      mode= rkey_to_rnext[(int)ha_rkey_mode];
      break;
    }
    default:
      my_message(ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA), MYF(0));
      goto err;
    }

    if (error)
    {
      if (error == HA_ERR_RECORD_DELETED)
        continue;
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        /* Don't give error in the log file for some expected problems */
        if (error != HA_ERR_RECORD_CHANGED && error != HA_ERR_WRONG_COMMAND)
          sql_print_error("mysql_ha_read: Got error %d when reading "
                          "table '%s'",
                          error, tables->table_name);
        table->file->print_error(error,MYF(0));
        goto err;
      }
      goto ok;
    }
    /* Generate values for virtual fields */
    update_virtual_fields(thd, table);
    if (cond && !cond->val_int())
      continue;
    if (num_rows >= offset_limit_cnt)
    {
      Item *item;
      protocol->prepare_for_resend();
      it.rewind();
      while ((item=it++))
      {
	if (item->send(thd->protocol, &buffer))
	{
	  protocol->free();                             // Free used
	  my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
	  goto err;
	}
      }
      protocol->write();
    }
    num_rows++;
  }
ok:
  mysql_unlock_tables(thd, handler->lock, 0);
  my_eof(thd);
  DBUG_PRINT("exit",("OK"));
  DBUG_RETURN(FALSE);

err:
  mysql_unlock_tables(thd, handler->lock, 0);
err0:
  DBUG_PRINT("exit",("ERROR"));
  DBUG_RETURN(TRUE);
}


/**
   Prepare for handler read

   For parameters, see mysql_ha_read()
*/

SQL_HANDLER *mysql_ha_read_prepare(THD *thd, TABLE_LIST *tables,
                                   enum enum_ha_read_modes mode, char *keyname,
                                   List<Item> *key_expr, Item *cond)
{
  SQL_HANDLER *handler;
  DBUG_ENTER("mysql_ha_read_prepare");
  if (!(handler= mysql_ha_find_handler(thd, tables->alias)))
    DBUG_RETURN(0);
  tables->table= handler->table;         // This is used by fix_fields
  if (mysql_ha_fix_cond_and_key(handler, mode, keyname, key_expr, cond, 1))
    DBUG_RETURN(0);
  DBUG_RETURN(handler);
}

  

/**
  Scan the handler tables hash for matching tables.

  @param thd Thread identifier.
  @param tables The list of tables to remove.

  @return Pointer to head of linked list (TABLE_LIST::next_local) of matching
          TABLE_LIST elements from handler_tables_hash. Otherwise, NULL if no
          table was matched.
*/

static SQL_HANDLER *mysql_ha_find_match(THD *thd, TABLE_LIST *tables)
{
  SQL_HANDLER *hash_tables, *head= NULL;
  TABLE_LIST *first= tables;
  DBUG_ENTER("mysql_ha_find_match");

  /* search for all handlers with matching table names */
  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (SQL_HANDLER*) hash_element(&thd->handler_tables_hash, i);

    for (tables= first; tables; tables= tables->next_local)
    {
      if ((! *tables->db ||
          ! my_strcasecmp(&my_charset_latin1, hash_tables->db.str,
                          tables->db)) &&
          ! my_strcasecmp(&my_charset_latin1, hash_tables->table_name.str,
                          tables->table_name))
      {
        /* Link into hash_tables list */
        hash_tables->next= head;
        head= hash_tables;
        break;
      }
    }
  }
  DBUG_RETURN(head);
}


/**
  Remove matching tables from the HANDLER's hash table.

  @param thd Thread identifier.
  @param tables The list of tables to remove.
  @param is_locked If LOCK_open is locked.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables, bool is_locked)
{
  SQL_HANDLER *hash_tables, *next;
  DBUG_ENTER("mysql_ha_rm_tables");

  DBUG_ASSERT(tables);

  hash_tables= mysql_ha_find_match(thd, tables);

  while (hash_tables)
  {
    next= hash_tables->next;
    if (hash_tables->table)
      mysql_ha_close_table(hash_tables, is_locked);
    hash_delete(&thd->handler_tables_hash, (uchar*) hash_tables);
    hash_tables= next;
  }

  DBUG_VOID_RETURN;
}


/**
  Flush (close and mark for re-open) all tables that should be should
  be reopen.

  @param thd Thread identifier.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_flush(THD *thd)
{
  SQL_HANDLER *hash_tables;
  DBUG_ENTER("mysql_ha_flush");

  safe_mutex_assert_owner(&LOCK_open);

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (SQL_HANDLER*) hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table && hash_tables->table->needs_reopen_or_name_lock())
      mysql_ha_close_table(hash_tables, TRUE);
  }

  DBUG_VOID_RETURN;
}


/**
  Close all HANDLER's tables.

  @param thd Thread identifier.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_cleanup(THD *thd)
{
  SQL_HANDLER *hash_tables;
  DBUG_ENTER("mysql_ha_cleanup");

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (SQL_HANDLER*) hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table)
      mysql_ha_close_table(hash_tables, FALSE);
   }

  hash_free(&thd->handler_tables_hash);

  DBUG_VOID_RETURN;
}

