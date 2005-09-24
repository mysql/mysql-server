/* Copyright (C) 2005 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
#pragma implementation                         /* gcc class implementation */
#endif

#include "mysql_priv.h"
#include "sql_cursor.h"
#include "sql_select.h"

/****************************************************************************
  Declarations.
****************************************************************************/

/*
  Sensitive_cursor -- a sensitive non-materialized server side
  cursor An instance of this class cursor has its own runtime
  state -- list of used items and memory root for runtime memory,
  open and locked tables, change list for the changes of the
  parsed tree. This state is freed when the cursor is closed.
*/

class Sensitive_cursor: public Server_side_cursor
{
  MEM_ROOT main_mem_root;
  Query_arena *stmt_arena;
  JOIN *join;
  TABLE *open_tables;
  MYSQL_LOCK *lock;
  TABLE *derived_tables;
  /* List of items created during execution */
  query_id_t query_id;
  struct Engine_info
  {
    const handlerton *ht;
    void *read_view;
  };
  Engine_info ht_info[MAX_HA];
  Item_change_list change_list;
  my_bool close_at_commit;
  THR_LOCK_OWNER lock_id;
private:
  /* bzero cursor state in THD */
  void reset_thd(THD *thd);
public:
  Sensitive_cursor(THD *thd, select_result *result_arg);

  THR_LOCK_OWNER *get_lock_id() { return &lock_id; }
  /* Save THD state into cursor */
  void post_open(THD *thd);

  virtual bool is_open() const { return join != 0; }
  virtual int open(JOIN *join);
  virtual void fetch(ulong num_rows);
  virtual void close();
  virtual ~Sensitive_cursor();
};


/*
  Materialized_cursor -- an insensitive materialized server-side
  cursor. The result set of this cursor is saved in a temporary
  table at open. The cursor itself is simply an interface for the
  handler of the temporary table.
*/

class Materialized_cursor: public Server_side_cursor
{
  MEM_ROOT main_mem_root;
  /* A fake unit to supply to select_send when fetching */
  SELECT_LEX_UNIT fake_unit;
  TABLE *table;
  List<Item> item_list;
  ulong fetch_limit;
  ulong fetch_count;
public:
  Materialized_cursor(select_result *result, TABLE *table);

  virtual bool is_open() const { return table != 0; }
  virtual int open(JOIN *join __attribute__((unused)));
  virtual void fetch(ulong num_rows);
  virtual void close();
  virtual ~Materialized_cursor();
};


/*
  Select_materialize -- a mediator between a cursor query and the
  protocol. In case we were not able to open a non-materialzed
  cursor, it creates an internal temporary HEAP table, and insert
  all rows into it. When the table reaches max_heap_table_size,
  it's converted to a MyISAM table. Later this table is used to
  create a Materialized_cursor.
*/

class Select_materialize: public select_union
{
  select_result *result; /* the result object of the caller (PS or SP) */
public:
  Select_materialize(select_result *result_arg) :result(result_arg) {}
  virtual bool send_fields(List<Item> &list, uint flags);
};


/**************************************************************************/

/*
  Attempt to open a materialized or non-materialized cursor.

  SYNOPSIS
    mysql_open_cursor()
      thd                thread handle
      flags   [in]       create a materialized cursor or not
      result  [in]       result class of the caller used as a destination
                         for the rows fetched from the cursor
      pcursor [out]      a pointer to store a pointer to cursor in

  RETURN VALUE
    0                    the query has been successfully executed; in this
                         case pcursor may or may not contain
                         a pointer to an open cursor.
    non-zero             an error, 'pcursor' has been left intact.
*/

int mysql_open_cursor(THD *thd, uint flags, select_result *result,
                      Server_side_cursor **pcursor)
{
  Sensitive_cursor *sensitive_cursor;
  select_result *save_result;
  Select_materialize *result_materialize;
  LEX *lex= thd->lex;
  int rc;

  /*
    The lifetime of the sensitive cursor is the same or less as the
    lifetime of the runtime memory of the statement it's opened for.
  */
  if (! (result_materialize= new (thd->mem_root) Select_materialize(result)))
    return 1;

  if (! (sensitive_cursor= new (thd->mem_root) Sensitive_cursor(thd, result)))
  {
    delete result;
    return 1;
  }

  save_result= lex->result;

  lex->result= result_materialize;
  if (! (flags & (uint) ALWAYS_MATERIALIZED_CURSOR))
  {
    thd->lock_id= sensitive_cursor->get_lock_id();
    thd->cursor= sensitive_cursor;
  }

  rc= mysql_execute_command(thd);

  lex->result= save_result;
  thd->lock_id= &thd->main_lock_id;
  thd->cursor= 0;

  /*
    Possible options here:
    - a sensitive cursor is open. In this case rc is 0 and
      result_materialize->table is NULL, or
    - a materialized cursor is open. In this case rc is 0 and
      result_materialize->table is not NULL
    - an error occured during materializaton.
      result_materialize->table is not NULL, but rc != 0
    - successful completion of mysql_execute_command without
      a cursor: rc is 0, result_materialize->table is NULL,
      sensitive_cursor is not open.
      This is possible if some command writes directly to the
      network, bypassing select_result mechanism. An example of
      such command is SHOW VARIABLES or SHOW STATUS.
  */
  if (rc)
    goto err_open;

  if (sensitive_cursor->is_open())
  {
    DBUG_ASSERT(!result_materialize->table);
    /*
      It's safer if we grab THD state after mysql_execute_command
      is finished and not in Sensitive_cursor::open(), because
      currently the call to Sensitive_cursor::open is buried deep
      in JOIN::exec of the top level join.
    */
    sensitive_cursor->post_open(thd);
    *pcursor= sensitive_cursor;
    goto end;
  }
  else if (result_materialize->table)
  {
    Materialized_cursor *materialized_cursor;
    TABLE *table= result_materialize->table;
    MEM_ROOT *mem_root= &table->mem_root;

    if (!(materialized_cursor= new (mem_root)
                               Materialized_cursor(result, table)))
    {
      rc= 1;
      goto err_open;
    }

    if ((rc= materialized_cursor->open(0)))
    {
      delete materialized_cursor;
      goto err_open;
    }

    *pcursor= materialized_cursor;
    thd->stmt_arena->cleanup_stmt();
    goto end;
  }

err_open:
  DBUG_ASSERT(! (sensitive_cursor && sensitive_cursor->is_open()));
  delete sensitive_cursor;
  if (result_materialize->table)
    free_tmp_table(thd, result_materialize->table);
end:
  delete result_materialize;
  return rc;
}

/****************************************************************************
  Server_side_cursor
****************************************************************************/

Server_side_cursor::~Server_side_cursor()
{
}


void Server_side_cursor::operator delete(void *ptr, size_t size)
{
  Server_side_cursor *cursor= (Server_side_cursor*) ptr;
  MEM_ROOT own_root= *cursor->mem_root;

  DBUG_ENTER("Server_side_cursor::operator delete");
  TRASH(ptr, size);
  /*
    If this cursor has never been opened mem_root is empty. Otherwise
    mem_root points to the memory the cursor object was allocated in.
    In this case it's important to call free_root last, and free a copy
    instead of *mem_root to avoid writing into freed memory.
  */
  free_root(&own_root, MYF(0));
  DBUG_VOID_RETURN;
}

/****************************************************************************
  Sensitive_cursor
****************************************************************************/

Sensitive_cursor::Sensitive_cursor(THD *thd, select_result *result_arg)
   :Server_side_cursor(&main_mem_root, result_arg),
   stmt_arena(0),
   join(0),
   close_at_commit(FALSE)
{
  /* We will overwrite it at open anyway. */
  init_sql_alloc(&main_mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
  thr_lock_owner_init(&lock_id, &thd->lock_info);
  bzero((void*) ht_info, sizeof(ht_info));
}


void
Sensitive_cursor::post_open(THD *thd)
{
  Engine_info *info;
  /*
    We need to save and reset thd->mem_root, otherwise it'll be
    freed later in mysql_parse.

    We can't just change thd->mem_root here as we want to keep the
    things that are already allocated in thd->mem_root for
    Sensitive_cursor::fetch()
  */
  *mem_root=  *thd->mem_root;
  stmt_arena= thd->stmt_arena;
  state= stmt_arena->state;
  /* Allocate a new memory root for thd */
  init_sql_alloc(thd->mem_root,
                 thd->variables.query_alloc_block_size,
                 thd->variables.query_prealloc_size);

  /*
    Save tables and zero THD pointers to prevent table close in
    close_thread_tables.
  */
  derived_tables= thd->derived_tables;
  open_tables=    thd->open_tables;
  lock=           thd->lock;
  query_id=       thd->query_id;
  free_list=      thd->free_list;
  change_list=    thd->change_list;
  reset_thd(thd);
  /* Now we have an active cursor and can cause a deadlock */
  thd->lock_info.n_cursors++;

  close_at_commit= FALSE; /* reset in case we're reusing the cursor */
  info= &ht_info[0];
  for (handlerton **pht= thd->transaction.stmt.ht; *pht; pht++)
  {
    const handlerton *ht= *pht;
    close_at_commit|= test(ht->flags & HTON_CLOSE_CURSORS_AT_COMMIT);
    if (ht->create_cursor_read_view)
    {
      info->ht= ht;
      info->read_view= (ht->create_cursor_read_view)();
      ++info;
    }
  }
  /*
    XXX: thd->locked_tables is not changed.
    What problems can we have with it if cursor is open?
    TODO: must be fixed because of the prelocked mode.
  */
}


void
Sensitive_cursor::reset_thd(THD *thd)
{
  thd->derived_tables= 0;
  thd->open_tables= 0;
  thd->lock= 0;
  thd->free_list= 0;
  thd->change_list.empty();
}


int
Sensitive_cursor::open(JOIN *join_arg)
{
  join= join_arg;
  THD *thd= join->thd;
  /* First non-constant table */
  JOIN_TAB *join_tab= join->join_tab + join->const_tables;
  DBUG_ENTER("Sensitive_cursor::open");

  join->change_result(result);
  /*
    Send fields description to the client; server_status is sent
    in 'EOF' packet, which follows send_fields().
    We don't simply use SEND_EOF flag of send_fields because we also
    want to flush the network buffer, which is done only in a standalone
    send_eof().
  */
  result->send_fields(*join->fields, Protocol::SEND_NUM_ROWS);
  thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
  result->send_eof();
  thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;

  /* Prepare JOIN for reading rows. */
  join->tmp_table= 0;
  join->join_tab[join->tables-1].next_select= setup_end_select_func(join);
  join->send_records= 0;
  join->fetch_limit= join->unit->offset_limit_cnt;

  /* Disable JOIN CACHE as it is not working with cursors yet */
  for (JOIN_TAB *tab= join_tab;
       tab != join->join_tab + join->tables - 1;
       tab++)
  {
    if (tab->next_select == sub_select_cache)
      tab->next_select= sub_select;
  }

  DBUG_ASSERT(join_tab->table->reginfo.not_exists_optimize == 0);
  DBUG_ASSERT(join_tab->not_used_in_distinct == 0);
  /*
    null_row is set only if row not found and it's outer join: should never
    happen for the first table in join_tab list
  */
  DBUG_ASSERT(join_tab->table->null_row == 0);
  DBUG_RETURN(0);
}


/*
  SYNOPSIS
    Sensitive_cursor::fetch()
      num_rows           fetch up to this number of rows (maybe less)

  DESCRIPTION
    Fetch next num_rows rows from the cursor and send them to the client

    Precondition:
    Sensitive_cursor is open

  RETURN VALUES:
    none, this function will send OK to the clinet or set an error
    message in THD
*/

void
Sensitive_cursor::fetch(ulong num_rows)
{
  THD *thd= join->thd;
  JOIN_TAB *join_tab= join->join_tab + join->const_tables;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  Query_arena backup_arena;
  Engine_info *info;
  DBUG_ENTER("Sensitive_cursor::fetch");
  DBUG_PRINT("enter",("rows: %lu", num_rows));

  DBUG_ASSERT(thd->derived_tables == 0 && thd->open_tables == 0 &&
              thd->lock == 0);

  thd->derived_tables= derived_tables;
  thd->open_tables= open_tables;
  thd->lock= lock;
  thd->query_id= query_id;
  thd->change_list= change_list;
  /* save references to memory allocated during fetch */
  thd->set_n_backup_active_arena(this, &backup_arena);

  for (info= ht_info; info->read_view ; info++)
    (info->ht->set_cursor_read_view)(info->read_view);

  join->fetch_limit+= num_rows;

  error= sub_select(join, join_tab, 0);
  if (error == NESTED_LOOP_OK || error == NESTED_LOOP_NO_MORE_ROWS)
    error= sub_select(join,join_tab,1);
  if (error == NESTED_LOOP_QUERY_LIMIT)
    error= NESTED_LOOP_OK;                    /* select_limit used */
  if (error == NESTED_LOOP_CURSOR_LIMIT)
    join->resume_nested_loop= TRUE;

#ifdef USING_TRANSACTIONS
    ha_release_temporary_latches(thd);
#endif
  /* Grab free_list here to correctly free it in close */
  thd->restore_active_arena(this, &backup_arena);

  change_list= thd->change_list;
  reset_thd(thd);

  for (info= ht_info; info->read_view; info++)
    (info->ht->set_cursor_read_view)(0);

  if (error == NESTED_LOOP_CURSOR_LIMIT)
  {
    /* Fetch limit worked, possibly more rows are there */
    thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
    result->send_eof();
    thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;
  }
  else
  {
    close();
    if (error == NESTED_LOOP_OK)
    {
      thd->server_status|= SERVER_STATUS_LAST_ROW_SENT;
      result->send_eof();
      thd->server_status&= ~SERVER_STATUS_LAST_ROW_SENT;
    }
    else if (error != NESTED_LOOP_KILLED)
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
  }
  DBUG_VOID_RETURN;
}


void
Sensitive_cursor::close()
{
  THD *thd= join->thd;
  DBUG_ENTER("Sensitive_cursor::close");

  for (Engine_info *info= ht_info; info->read_view; info++)
  {
    (info->ht->close_cursor_read_view)(info->read_view);
    info->read_view= 0;
    info->ht= 0;
  }

  thd->change_list= change_list;
  {
    /*
      XXX: Another hack: we need to set THD state as if in a fetch to be
      able to call stmt close.
    */
    DBUG_ASSERT(lock || open_tables || derived_tables);

    TABLE *tmp_derived_tables= thd->derived_tables;
    MYSQL_LOCK *tmp_lock= thd->lock;

    thd->open_tables= open_tables;
    thd->derived_tables= derived_tables;
    thd->lock= lock;

    /* Is expected to at least close tables and empty thd->change_list */
    stmt_arena->cleanup_stmt();

    thd->open_tables= tmp_derived_tables;
    thd->derived_tables= tmp_derived_tables;
    thd->lock= tmp_lock;
  }
  thd->lock_info.n_cursors--; /* Decrease the number of active cursors */
  join= 0;
  stmt_arena= 0;
  free_items();
  change_list.empty();
  DBUG_VOID_RETURN;
}


Sensitive_cursor::~Sensitive_cursor()
{
  if (is_open())
    close();
}

/***************************************************************************
 Materialized_cursor
****************************************************************************/

Materialized_cursor::Materialized_cursor(select_result *result_arg,
                                         TABLE *table_arg)
  :Server_side_cursor(&table_arg->mem_root, result_arg),
  table(table_arg),
  fetch_limit(0),
  fetch_count(0)
{
  fake_unit.init_query();
  fake_unit.thd= table->in_use;
}


int Materialized_cursor::open(JOIN *join __attribute__((unused)))
{
  THD *thd= fake_unit.thd;
  int rc;
  Query_arena backup_arena;

  thd->set_n_backup_active_arena(this, &backup_arena);
  /* Create a list of fields and start sequential scan */
  rc= (table->fill_item_list(&item_list) ||
       result->prepare(item_list, &fake_unit) ||
       table->file->ha_rnd_init(TRUE));
  thd->restore_active_arena(this, &backup_arena);
  return rc;
}


/*
  Fetch up to the given number of rows from a materialized cursor.

  DESCRIPTION
    Precondition: the cursor is open.

    If the cursor points after the last row, the fetch will automatically
    close the cursor and not send any data (except the 'EOF' packet
    with SERVER_STATUS_LAST_ROW_SENT). This is an extra round trip
    and probably should be improved to return
    SERVER_STATUS_LAST_ROW_SENT along with the last row.

  RETURN VALUE
    none, in case of success the row is sent to the client, otherwise
    an error message is set in THD
*/

void Materialized_cursor::fetch(ulong num_rows)
{
  THD *thd= table->in_use;

  int res= 0;
  for (fetch_limit+= num_rows; fetch_count < fetch_limit; fetch_count++)
  {
    if ((res= table->file->rnd_next(table->record[0])))
      break;
    /* Send data only if the read was successful. */
    result->send_data(item_list);
  }

  switch (res) {
  case 0:
    thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
    result->send_eof();
    thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;
    break;
  case HA_ERR_END_OF_FILE:
    thd->server_status|= SERVER_STATUS_LAST_ROW_SENT;
    result->send_eof();
    thd->server_status&= ~SERVER_STATUS_LAST_ROW_SENT;
    close();
    break;
  default:
    table->file->print_error(res, MYF(0));
    close();
    break;
  }
}


void Materialized_cursor::close()
{
  /* Free item_list items */
  free_items();
  (void) table->file->ha_rnd_end();
  /*
    We need to grab table->mem_root to prevent free_tmp_table from freeing:
    the cursor object was allocated in this memory.
  */
  main_mem_root= table->mem_root;
  mem_root= &main_mem_root;
  clear_alloc_root(&table->mem_root);
  free_tmp_table(table->in_use, table);
  table= 0;
}


Materialized_cursor::~Materialized_cursor()
{
  if (is_open())
    close();
}


/***************************************************************************
 Select_materialize
****************************************************************************/

bool Select_materialize::send_fields(List<Item> &list, uint flags)
{
  bool rc;
  DBUG_ASSERT(table == 0);
  if (create_result_table(unit->thd, unit->get_unit_column_types(),
                          FALSE, thd->options | TMP_TABLE_ALL_COLUMNS, ""))
    return TRUE;
  /*
    We can't simply supply SEND_EOF flag to send_fields, because send_fields
    doesn't flush the network buffer.
  */
  rc= result->send_fields(list, Protocol::SEND_NUM_ROWS);
  thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
  result->send_eof();
  thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;
  return rc;
}

