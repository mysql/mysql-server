/* Copyright (c) 2001, 2011, Oracle and/or its affiliates. All rights reserved.

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
  The information about open HANDLER objects is stored in a HASH.
  It holds objects of type TABLE_LIST, which are indexed by table
  name/alias, and allows us to quickly find a HANDLER table for any
  operation at hand - be it HANDLER READ or HANDLER CLOSE.

  It also allows us to maintain an "open" HANDLER even in cases
  when there is no physically open cursor. E.g. a FLUSH TABLE
  statement in this or some other connection demands that all open
  HANDLERs against the flushed table are closed. In order to
  preserve the information about an open HANDLER, we don't perform
  a complete HANDLER CLOSE, but only close the TABLE object.  The
  corresponding TABLE_LIST is kept in the cache with 'table'
  pointer set to NULL. The table will be reopened on next access
  (this, however, leads to loss of cursor position, unless the
  cursor points at the first record).
*/

#include "sql_priv.h"
#include "sql_handler.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_base.h"                           // close_thread_tables
#include "lock.h"                               // mysql_unlock_tables
#include "key.h"                                // key_copy
#include "sql_base.h"                           // insert_fields
#include "sql_select.h"
#include "transaction.h"

#define HANDLER_TABLES_HASH_SIZE 120

static enum enum_ha_read_modes rkey_to_rnext[]=
{ RNEXT_SAME, RNEXT, RPREV, RNEXT, RPREV, RNEXT, RPREV, RPREV };

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

static char *mysql_ha_hash_get_key(TABLE_LIST *tables, size_t *key_len_p,
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
  my_free(tables);
}

/**
  Close a HANDLER table.

  @param thd Thread identifier.
  @param tables A list of tables with the first entry to close.

  @note Though this function takes a list of tables, only the first list entry
  will be closed.
  @note Broadcasts refresh if it closed a table with old version.
*/

static void mysql_ha_close_table(THD *thd, TABLE_LIST *tables)
{

  if (tables->table && !tables->table->s->tmp_table)
  {
    /* Non temporary table. */
    tables->table->file->ha_index_or_rnd_end();
    tables->table->open_by_handler= 0;
    (void) close_thread_table(thd, &tables->table);
    thd->mdl_context.release_lock(tables->mdl_request.ticket);
  }
  else if (tables->table)
  {
    /* Must be a temporary table */
    TABLE *table= tables->table;
    table->file->ha_index_or_rnd_end();
    table->query_id= thd->query_id;
    table->open_by_handler= 0;
    mark_tmp_table_for_reuse(table);
  }

  /* Mark table as closed, ready for re-open if necessary. */
  tables->table= NULL;
  /* Safety, cleanup the pointer to satisfy MDL assertions. */
  tables->mdl_request.ticket= NULL;
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
    'reopen' is also used to suppress the sending of an 'ok' message.

  RETURN
    FALSE OK
    TRUE  Error
*/

bool mysql_ha_open(THD *thd, TABLE_LIST *tables, bool reopen)
{
  TABLE_LIST    *hash_tables = NULL;
  char          *db, *name, *alias;
  uint          dblen, namelen, aliaslen, counter;
  bool          error;
  TABLE         *backup_open_tables;
  MDL_savepoint mdl_savepoint;
  DBUG_ENTER("mysql_ha_open");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'  reopen: %d",
                      tables->db, tables->table_name, tables->alias,
                      (int) reopen));

  if (thd->locked_tables_mode)
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (tables->schema_table)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "HANDLER OPEN",
             INFORMATION_SCHEMA_NAME.str);
    DBUG_PRINT("exit",("ERROR"));
    DBUG_RETURN(TRUE);
  }

  if (! my_hash_inited(&thd->handler_tables_hash))
  {
    /*
      HASH entries are of type TABLE_LIST.
    */
    if (my_hash_init(&thd->handler_tables_hash, &my_charset_latin1,
                     HANDLER_TABLES_HASH_SIZE, 0, 0,
                     (my_hash_get_key) mysql_ha_hash_get_key,
                     (my_hash_free_key) mysql_ha_hash_free, 0))
    {
      DBUG_PRINT("exit",("ERROR"));
      DBUG_RETURN(TRUE);
    }
  }
  else if (! reopen) /* Otherwise we have 'tables' already. */
  {
    if (my_hash_search(&thd->handler_tables_hash, (uchar*) tables->alias,
                       strlen(tables->alias) + 1))
    {
      DBUG_PRINT("info",("duplicate '%s'", tables->alias));
      DBUG_PRINT("exit",("ERROR"));
      my_error(ER_NONUNIQ_TABLE, MYF(0), tables->alias);
      DBUG_RETURN(TRUE);
    }
  }

  if (! reopen)
  {
    /* copy the TABLE_LIST struct */
    dblen= strlen(tables->db) + 1;
    namelen= strlen(tables->table_name) + 1;
    aliaslen= strlen(tables->alias) + 1;
    if (!(my_multi_malloc(MYF(MY_WME),
                          &hash_tables, (uint) sizeof(*hash_tables),
                          &db, (uint) dblen,
                          &name, (uint) namelen,
                          &alias, (uint) aliaslen,
                          NullS)))
    {
      DBUG_PRINT("exit",("ERROR"));
      DBUG_RETURN(TRUE);
    }
    /* structure copy */
    *hash_tables= *tables;
    hash_tables->db= db;
    hash_tables->table_name= name;
    hash_tables->alias= alias;
    memcpy(hash_tables->db, tables->db, dblen);
    memcpy(hash_tables->table_name, tables->table_name, namelen);
    memcpy(hash_tables->alias, tables->alias, aliaslen);
    /*
      We can't request lock with explicit duration for this table
      right from the start as open_tables() can't handle properly
      back-off for such locks.
    */
    hash_tables->mdl_request.init(MDL_key::TABLE, db, name, MDL_SHARED,
                                  MDL_TRANSACTION);
    /* for now HANDLER can be used only for real TABLES */
    hash_tables->required_type= FRMTYPE_TABLE;

    /* add to hash */
    if (my_hash_insert(&thd->handler_tables_hash, (uchar*) hash_tables))
    {
      my_free(hash_tables);
      DBUG_PRINT("exit",("ERROR"));
      DBUG_RETURN(TRUE);
    }
  }
  else
    hash_tables= tables;

  /*
    Save and reset the open_tables list so that open_tables() won't
    be able to access (or know about) the previous list. And on return
    from open_tables(), thd->open_tables will contain only the opened
    table.

    See open_table() back-off comments for more details.
  */
  backup_open_tables= thd->open_tables;
  thd->set_open_tables(NULL);
  mdl_savepoint= thd->mdl_context.mdl_savepoint();

  /*
    open_tables() will set 'hash_tables->table' if successful.
    It must be NULL for a real open when calling open_tables().
  */
  DBUG_ASSERT(! hash_tables->table);

  /*
    We use open_tables() here, rather than, say,
    open_ltable() or open_table() because we would like to be able
    to open a temporary table.
  */
  error= open_tables(thd, &hash_tables, &counter, 0);

  if (! error &&
      ! (hash_tables->table->file->ha_table_flags() & HA_CAN_SQL_HANDLER))
  {
    my_error(ER_ILLEGAL_HA, MYF(0), tables->alias);
    error= TRUE;
  }
  if (!error &&
      hash_tables->mdl_request.ticket &&
      thd->mdl_context.has_lock(mdl_savepoint,
                                hash_tables->mdl_request.ticket))
  {
    /* The ticket returned is within a savepoint. Make a copy.  */
    error= thd->mdl_context.clone_ticket(&hash_tables->mdl_request);
    hash_tables->table->mdl_ticket= hash_tables->mdl_request.ticket;
  }
  if (error)
  {
    /*
      No need to rollback statement transaction, it's not started.
      If called with reopen flag, no need to rollback either,
      it will be done at statement end.
    */
    DBUG_ASSERT(thd->transaction.stmt.is_empty());
    close_thread_tables(thd);
    thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
    thd->set_open_tables(backup_open_tables);
    if (!reopen)
      my_hash_delete(&thd->handler_tables_hash, (uchar*) hash_tables);
    else
    {
      hash_tables->table= NULL;
      /* Safety, cleanup the pointer to satisfy MDL assertions. */
      hash_tables->mdl_request.ticket= NULL;
    }
    DBUG_PRINT("exit",("ERROR"));
    DBUG_RETURN(TRUE);
  }
  thd->set_open_tables(backup_open_tables);
  if (hash_tables->mdl_request.ticket)
  {
    thd->mdl_context.set_lock_duration(hash_tables->mdl_request.ticket,
                                       MDL_EXPLICIT);
    thd->mdl_context.set_needs_thr_lock_abort(TRUE);
  }

  /*
    Assert that the above check prevents opening of views and merge tables.
    For temporary tables, TABLE::next can be set even if only one table
    was opened for HANDLER as it is used to link them together
    (see thd->temporary_tables).
  */
  DBUG_ASSERT(hash_tables->table->next == NULL ||
              hash_tables->table->s->tmp_table);
  /*
    If it's a temp table, don't reset table->query_id as the table is
    being used by this handler. For non-temp tables we use this flag
    in asserts.
  */
  hash_tables->table->open_by_handler= 1;

  if (! reopen)
    my_ok(thd);
  DBUG_PRINT("exit",("OK"));
  DBUG_RETURN(FALSE);
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
  TABLE_LIST    *hash_tables;
  DBUG_ENTER("mysql_ha_close");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'",
                      tables->db, tables->table_name, tables->alias));

  if (thd->locked_tables_mode)
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(TRUE);
  }
  if ((hash_tables= (TABLE_LIST*) my_hash_search(&thd->handler_tables_hash,
                                                 (uchar*) tables->alias,
                                                 strlen(tables->alias) + 1)))
  {
    mysql_ha_close_table(thd, hash_tables);
    my_hash_delete(&thd->handler_tables_hash, (uchar*) hash_tables);
  }
  else
  {
    my_error(ER_UNKNOWN_TABLE, MYF(0), tables->alias, "HANDLER");
    DBUG_PRINT("exit",("ERROR"));
    DBUG_RETURN(TRUE);
  }

  /*
    Mark MDL_context as no longer breaking protocol if we have
    closed last HANDLER.
  */
  if (! thd->handler_tables_hash.records)
    thd->mdl_context.set_needs_thr_lock_abort(FALSE);

  my_ok(thd);
  DBUG_PRINT("exit", ("OK"));
  DBUG_RETURN(FALSE);
}


/**
  A helper class to process an error from mysql_lock_tables().
  HANDLER READ statement's attempt to lock the subject table
  may get aborted if there is a pending DDL. In that case
  we close the table, reopen it, and try to read again.
  This is implicit and obscure, since HANDLER position
  is lost in the process, but it's the legacy server
  behaviour we should preserve.
*/

class Sql_handler_lock_error_handler: public Internal_error_handler
{
public:
  virtual
  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char *sqlstate,
                        MYSQL_ERROR::enum_warning_level level,
                        const char* msg,
                        MYSQL_ERROR **cond_hdl);

  bool need_reopen() const { return m_need_reopen; };
  void init() { m_need_reopen= FALSE; };
private:
  bool m_need_reopen;
};


/**
  Handle an error from mysql_lock_tables().
  Ignore ER_LOCK_ABORTED errors.
*/

bool
Sql_handler_lock_error_handler::
handle_condition(THD *thd,
                 uint sql_errno,
                 const char *sqlstate,
                 MYSQL_ERROR::enum_warning_level level,
                 const char* msg,
                 MYSQL_ERROR **cond_hdl)
{
  *cond_hdl= NULL;
  if (sql_errno == ER_LOCK_ABORTED)
    m_need_reopen= TRUE;

  return m_need_reopen;
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
  TABLE_LIST    *hash_tables;
  TABLE         *table, *backup_open_tables;
  MYSQL_LOCK    *lock;
  List<Item>	list;
  Protocol	*protocol= thd->protocol;
  char		buff[MAX_FIELD_WIDTH];
  String	buffer(buff, sizeof(buff), system_charset_info);
  int           error, keyno= -1;
  uint          num_rows;
  uchar		*UNINIT_VAR(key);
  uint		UNINIT_VAR(key_len);
  Sql_handler_lock_error_handler sql_handler_lock_error;
  DBUG_ENTER("mysql_ha_read");
  DBUG_PRINT("enter",("'%s'.'%s' as '%s'",
                      tables->db, tables->table_name, tables->alias));

  if (thd->locked_tables_mode)
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(TRUE);
  }

  thd->lex->select_lex.context.resolve_in_table_list_only(tables);
  list.push_front(new Item_field(&thd->lex->select_lex.context,
                                 NULL, NULL, "*"));
  List_iterator<Item> it(list);
  it++;

retry:
  if ((hash_tables= (TABLE_LIST*) my_hash_search(&thd->handler_tables_hash,
                                                 (uchar*) tables->alias,
                                                 strlen(tables->alias) + 1)))
  {
    table= hash_tables->table;
    DBUG_PRINT("info-in-hash",("'%s'.'%s' as '%s' table: 0x%lx",
                               hash_tables->db, hash_tables->table_name,
                               hash_tables->alias, (long) table));
    if (!table)
    {
      /*
        The handler table has been closed. Re-open it.
      */
      if (mysql_ha_open(thd, hash_tables, 1))
      {
        DBUG_PRINT("exit",("reopen failed"));
        goto err0;
      }

      table= hash_tables->table;
      DBUG_PRINT("info",("re-opened '%s'.'%s' as '%s' tab %p",
                         hash_tables->db, hash_tables->table_name,
                         hash_tables->alias, table));
    }
  }
  else
    table= NULL;

  if (!table)
  {
    my_error(ER_UNKNOWN_TABLE, MYF(0), tables->alias, "HANDLER");
    goto err0;
  }

  /* save open_tables state */
  backup_open_tables= thd->open_tables;
  /* Always a one-element list, see mysql_ha_open(). */
  DBUG_ASSERT(hash_tables->table->next == NULL ||
              hash_tables->table->s->tmp_table);
  /*
    mysql_lock_tables() needs thd->open_tables to be set correctly to
    be able to handle aborts properly.
  */
  thd->set_open_tables(hash_tables->table);


  sql_handler_lock_error.init();
  thd->push_internal_handler(&sql_handler_lock_error);

  lock= mysql_lock_tables(thd, &thd->open_tables, 1, 0);

  thd->pop_internal_handler();
  /*
    In 5.1 and earlier, mysql_lock_tables() could replace the TABLE
    object with another one (reopen it). This is no longer the case
    with new MDL.
  */
  DBUG_ASSERT(hash_tables->table == thd->open_tables);
  /* Restore previous context. */
  thd->set_open_tables(backup_open_tables);

  if (sql_handler_lock_error.need_reopen())
  {
    DBUG_ASSERT(!lock && !thd->is_error());
    /*
      Always close statement transaction explicitly,
      so that the engine doesn't have to count locks.
    */
    trans_rollback_stmt(thd);
    mysql_ha_close_table(thd, hash_tables);
    goto retry;
  }

  if (!lock)
    goto err0; // mysql_lock_tables() printed error message already

  // Always read all columns
  hash_tables->table->read_set= &hash_tables->table->s->all_set;
  tables->table= hash_tables->table;

  if (cond)
  {
    if (table->query_id != thd->query_id)
      cond->cleanup();                          // File was reopened
    if ((!cond->fixed &&
	 cond->fix_fields(thd, &cond)) || cond->check_cols(1))
      goto err;
  }

  if (keyname)
  {
    if ((keyno= find_type(keyname, &table->s->keynames,
                          FIND_TYPE_NO_PREFIX) - 1) < 0)
    {
      my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), keyname, tables->alias);
      goto err;
    }
    /* Check if the same index involved. */
    if ((uint) keyno != table->file->get_index())
    {
      if (mode == RNEXT)
        mode= RFIRST;
      else if (mode == RPREV)
        mode= RLAST;
    }
  }

  if (insert_fields(thd, &thd->lex->select_lex.context,
                    tables->db, tables->alias, &it, 0))
    goto err;

  protocol->send_result_set_metadata(&list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

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
        if (keyname)
        {
          /* Check if we read from the same index. */
          DBUG_ASSERT((uint) keyno == table->file->get_index());
          error= table->file->index_next(table->record[0]);
        }
        else
        {
          error= table->file->rnd_next(table->record[0]);
        }
        break;
      }
      /* else fall through */
    case RFIRST:
      if (keyname)
      {
        table->file->ha_index_or_rnd_end();
        table->file->ha_index_init(keyno, 1);
        error= table->file->index_first(table->record[0]);
      }
      else
      {
        table->file->ha_index_or_rnd_end();
	if (!(error= table->file->ha_rnd_init(1)))
          error= table->file->rnd_next(table->record[0]);
      }
      mode=RNEXT;
      break;
    case RPREV:
      DBUG_ASSERT(keyname != 0);
      /* Check if we read from the same index. */
      DBUG_ASSERT((uint) keyno == table->file->get_index());
      if (table->file->inited != handler::NONE)
      {
        error=table->file->index_prev(table->record[0]);
        break;
      }
      /* else fall through */
    case RLAST:
      DBUG_ASSERT(keyname != 0);
      table->file->ha_index_or_rnd_end();
      table->file->ha_index_init(keyno, 1);
      error= table->file->index_last(table->record[0]);
      mode=RPREV;
      break;
    case RNEXT_SAME:
      /* Continue scan on "(keypart1,keypart2,...)=(c1, c2, ...)  */
      DBUG_ASSERT(keyname != 0);
      error= table->file->index_next_same(table->record[0], key, key_len);
      break;
    case RKEY:
    {
      DBUG_ASSERT(keyname != 0);
      KEY *keyinfo=table->key_info+keyno;
      KEY_PART_INFO *key_part=keyinfo->key_part;
      if (key_expr->elements > keyinfo->key_parts)
      {
	my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), keyinfo->key_parts);
	goto err;
      }
      List_iterator<Item> it_ke(*key_expr);
      Item *item;
      key_part_map keypart_map;
      for (keypart_map= key_len=0 ; (item=it_ke++) ; key_part++)
      {
        my_bitmap_map *old_map;
	// 'item' can be changed by fix_fields() call
        if ((!item->fixed &&
             item->fix_fields(thd, it_ke.ref())) ||
	    (item= *it_ke.ref())->check_cols(1))
	  goto err;
	if (item->used_tables() & ~RAND_TABLE_BIT)
        {
          my_error(ER_WRONG_ARGUMENTS,MYF(0),"HANDLER ... READ");
	  goto err;
        }
        old_map= dbug_tmp_use_all_columns(table, table->write_set);
	(void) item->save_in_field(key_part->field, 1);
        dbug_tmp_restore_column_map(table->write_set, old_map);
	key_len+=key_part->store_length;
        keypart_map= (keypart_map << 1) | 1;
      }

      if (!(key= (uchar*) thd->calloc(ALIGN_SIZE(key_len))))
	goto err;
      table->file->ha_index_or_rnd_end();
      table->file->ha_index_init(keyno, 1);
      key_copy(key, table->record[0], table->key_info + keyno, key_len);
      error= table->file->index_read_map(table->record[0],
                                         key, keypart_map, ha_rkey_mode);
      mode=rkey_to_rnext[(int)ha_rkey_mode];
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
        sql_print_error("mysql_ha_read: Got error %d when reading table '%s'",
                        error, tables->table_name);
        table->file->print_error(error,MYF(0));
        goto err;
      }
      goto ok;
    }
    if (cond && !cond->val_int())
    {
      if (thd->is_error())
        goto err;
      continue;
    }
    if (num_rows >= offset_limit_cnt)
    {
      protocol->prepare_for_resend();

      if (protocol->send_result_set_row(&list))
        goto err;

      protocol->write();
    }
    num_rows++;
  }
ok:
  /*
    Always close statement transaction explicitly,
    so that the engine doesn't have to count locks.
  */
  trans_commit_stmt(thd);
  mysql_unlock_tables(thd,lock);
  my_eof(thd);
  DBUG_PRINT("exit",("OK"));
  DBUG_RETURN(FALSE);

err:
  trans_rollback_stmt(thd);
  mysql_unlock_tables(thd,lock);
err0:
  DBUG_PRINT("exit",("ERROR"));
  DBUG_RETURN(TRUE);
}


/**
  Scan the handler tables hash for matching tables.

  @param thd Thread identifier.
  @param tables The list of tables to remove.

  @return Pointer to head of linked list (TABLE_LIST::next_local) of matching
          TABLE_LIST elements from handler_tables_hash. Otherwise, NULL if no
          table was matched.
*/

static TABLE_LIST *mysql_ha_find(THD *thd, TABLE_LIST *tables)
{
  TABLE_LIST *hash_tables, *head= NULL, *first= tables;
  DBUG_ENTER("mysql_ha_find");

  /* search for all handlers with matching table names */
  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TABLE_LIST*) my_hash_element(&thd->handler_tables_hash, i);
    for (tables= first; tables; tables= tables->next_local)
    {
      if ((! *tables->db ||
          ! my_strcasecmp(&my_charset_latin1, hash_tables->db, tables->db)) &&
          ! my_strcasecmp(&my_charset_latin1, hash_tables->table_name,
                          tables->table_name))
        break;
    }
    if (tables)
    {
      hash_tables->next_local= head;
      head= hash_tables;
    }
  }

  DBUG_RETURN(head);
}


/**
  Remove matching tables from the HANDLER's hash table.

  @param thd Thread identifier.
  @param tables The list of tables to remove.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables)
{
  TABLE_LIST *hash_tables, *next;
  DBUG_ENTER("mysql_ha_rm_tables");

  DBUG_ASSERT(tables);

  hash_tables= mysql_ha_find(thd, tables);

  while (hash_tables)
  {
    next= hash_tables->next_local;
    if (hash_tables->table)
      mysql_ha_close_table(thd, hash_tables);
    my_hash_delete(&thd->handler_tables_hash, (uchar*) hash_tables);
    hash_tables= next;
  }

  /*
    Mark MDL_context as no longer breaking protocol if we have
    closed last HANDLER.
  */
  if (! thd->handler_tables_hash.records)
    thd->mdl_context.set_needs_thr_lock_abort(FALSE);

  DBUG_VOID_RETURN;
}


/**
  Close cursors of matching tables from the HANDLER's hash table.

  @param thd Thread identifier.
  @param tables The list of tables to flush.
*/

void mysql_ha_flush_tables(THD *thd, TABLE_LIST *all_tables)
{
  DBUG_ENTER("mysql_ha_flush_tables");

  for (TABLE_LIST *table_list= all_tables; table_list;
       table_list= table_list->next_global)
  {
    TABLE_LIST *hash_tables= mysql_ha_find(thd, table_list);
    /* Close all aliases of the same table. */
    while (hash_tables)
    {
      TABLE_LIST *next_local= hash_tables->next_local;
      if (hash_tables->table)
        mysql_ha_close_table(thd, hash_tables);
      hash_tables= next_local;
    }
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
  TABLE_LIST *hash_tables;
  DBUG_ENTER("mysql_ha_flush");

  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    Don't try to flush open HANDLERs when we're working with
    system tables. The main MDL context is backed up and we can't
    properly release HANDLER locks stored there.
  */
  if (thd->state_flags & Open_tables_state::BACKUPS_AVAIL)
    DBUG_VOID_RETURN;

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TABLE_LIST*) my_hash_element(&thd->handler_tables_hash, i);
    /*
      TABLE::mdl_ticket is 0 for temporary tables so we need extra check.
    */
    if (hash_tables->table &&
        ((hash_tables->table->mdl_ticket &&
         hash_tables->table->mdl_ticket->has_pending_conflicting_lock()) ||
         (!hash_tables->table->s->tmp_table &&
          hash_tables->table->s->has_old_version())))
      mysql_ha_close_table(thd, hash_tables);
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
  TABLE_LIST *hash_tables;
  DBUG_ENTER("mysql_ha_cleanup");

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TABLE_LIST*) my_hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table)
      mysql_ha_close_table(thd, hash_tables);
  }

  my_hash_free(&thd->handler_tables_hash);

  DBUG_VOID_RETURN;
}


/**
  Set explicit duration for metadata locks corresponding to open HANDLERs
  to protect them from being released at the end of transaction.

  @param thd Thread identifier.
*/

void mysql_ha_set_explicit_lock_duration(THD *thd)
{
  TABLE_LIST *hash_tables;
  DBUG_ENTER("mysql_ha_set_explicit_lock_duration");

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TABLE_LIST*) my_hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table && hash_tables->table->mdl_ticket)
      thd->mdl_context.set_lock_duration(hash_tables->table->mdl_ticket,
                                         MDL_EXPLICIT);
  }
  DBUG_VOID_RETURN;
}

