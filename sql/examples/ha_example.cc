/* Copyright (C) 2003 MySQL AB

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
  ha_example is a stubbed storage engine. It does nothing at this point. It
  will let you create/open/delete tables but that is all. You can enable it
  in your buld by doing the following during your build process:
  ./configure --with-example-storage-engine

  Once this is done mysql will let you create tables with:
  CREATE TABLE A (...) ENGINE=EXAMPLE;

  The example is setup to use table locks. It implements an example "SHARE"
  that is inserted into a hash by table name. You can use this to store
  information of state that any example handler object will be able to see
  if it is using the same table.

  Please read the object definition in ha_example.h before reading the rest
  if this file.

  To get an idea of what occurs here is an example select that would do a
  scan of an entire table:
  ha_example::store_lock
  ha_example::external_lock
  ha_example::info
  ha_example::rnd_init
  ha_example::extra
  ENUM HA_EXTRA_CACHE   Cash record in HA_rrnd()
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::rnd_next
  ha_example::extra
  ENUM HA_EXTRA_NO_CACHE   End cacheing of records (def)
  ha_example::external_lock
  ha_example::extra
  ENUM HA_EXTRA_RESET   Reset database to after open

  In the above example has 9 row called before rnd_next signalled that it was
  at the end of its data. In the above example the table was already opened
  (or you would have seen a call to ha_example::open(). Calls to
  ha_example::extra() are hints as to what will be occuring to the request.

  Happy coding!
    -Brian
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#include "../mysql_priv.h"

#ifdef HAVE_EXAMPLE_DB
#include "ha_example.h"


handlerton example_hton= {
  "CSV",
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
  HTON_CAN_RECREATE
};

/* Variables for example share methods */
static HASH example_open_tables; // Hash used to track open tables
pthread_mutex_t example_mutex;   // This is the mutex we use to init the hash
static int example_init= 0;      // Variable for checking the init state of hash


/*
  Function we use in the creation of our hash to get key.
*/
static byte* example_get_key(EXAMPLE_SHARE *share,uint *length,
                             my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}


/*
  Example of simple lock controls. The "share" it creates is structure we will
  pass to each example handler. Do you have to have one of these? Well, you have
  pieces that are used for locking, and they are needed to function.
*/
static EXAMPLE_SHARE *get_share(const char *table_name, TABLE *table)
{
  EXAMPLE_SHARE *share;
  uint length;
  char *tmp_name;

  /*
    So why does this exist? There is no way currently to init a storage engine.
    Innodb and BDB both have modifications to the server to allow them to
    do this. Since you will not want to do this, this is probably the next
    best method.
  */
  if (!example_init)
  {
    /* Hijack a mutex for init'ing the storage engine */
    pthread_mutex_lock(&LOCK_mysql_create_db);
    if (!example_init)
    {
      example_init++;
      VOID(pthread_mutex_init(&example_mutex,MY_MUTEX_INIT_FAST));
      (void) hash_init(&example_open_tables,system_charset_info,32,0,0,
                       (hash_get_key) example_get_key,0,0);
    }
    pthread_mutex_unlock(&LOCK_mysql_create_db);
  }
  pthread_mutex_lock(&example_mutex);
  length=(uint) strlen(table_name);

  if (!(share=(EXAMPLE_SHARE*) hash_search(&example_open_tables,
                                           (byte*) table_name,
                                           length)))
  {
    if (!(share=(EXAMPLE_SHARE *)
          my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share),
                          &tmp_name, length+1,
                          NullS)))
    {
      pthread_mutex_unlock(&example_mutex);
      return NULL;
    }

    share->use_count=0;
    share->table_name_length=length;
    share->table_name=tmp_name;
    strmov(share->table_name,table_name);
    if (my_hash_insert(&example_open_tables, (byte*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&example_mutex);

  return share;

error:
  pthread_mutex_destroy(&share->mutex);
  pthread_mutex_unlock(&example_mutex);
  my_free((gptr) share, MYF(0));

  return NULL;
}


/*
  Free lock controls. We call this whenever we close a table. If the table had
  the last reference to the share then we free memory associated with it.
*/
static int free_share(EXAMPLE_SHARE *share)
{
  pthread_mutex_lock(&example_mutex);
  if (!--share->use_count)
  {
    hash_delete(&example_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&example_mutex);

  return 0;
}


ha_example::ha_example(TABLE *table_arg)
  :handler(&example_hton, table_arg)
{}

/*
  If frm_error() is called then we will use this to to find out what file extentions
  exist for the storage engine. This is also used by the default rename_table and
  delete_table method in handler.cc.
*/
static const char *ha_example_exts[] = {
  NullS
};

const char **ha_example::bas_ext() const
{
  return ha_example_exts;
}


/*
  Used for opening tables. The name will be the name of the file.
  A table is opened when it needs to be opened. For instance
  when a request comes in for a select on the table (tables are not
  open and closed for each request, they are cached).

  Called from handler.cc by handler::ha_open(). The server opens all tables by
  calling ha_open() which then calls the handler specific open().
*/
int ha_example::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_example::open");

  if (!(share = get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock,&lock,NULL);

  DBUG_RETURN(0);
}


/*
  Closes a table. We call the free_share() function to free any resources
  that we have allocated in the "shared" structure.

  Called from sql_base.cc, sql_select.cc, and table.cc.
  In sql_select.cc it is only used to close up temporary tables or during
  the process where a temporary table is converted over to being a
  myisam table.
  For sql_base.cc look at close_data_tables().
*/
int ha_example::close(void)
{
  DBUG_ENTER("ha_example::close");
  DBUG_RETURN(free_share(share));
}


/*
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happeneding. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.
  Example of this would be:
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }

  See ha_tina.cc for an example of extracting all of the data as strings.
  ha_berekly.cc has an example of how to store it intact by "packing" it
  for ha_berkeley's own native storage type.

  See the note for update_row() on auto_increments and timestamps. This
  case also applied to write_row().

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.
*/
int ha_example::write_row(byte * buf)
{
  DBUG_ENTER("ha_example::write_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in
  it.
  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guarenteed.
  Currently new_data will not have an updated auto_increament record, or
  and updated timestamp field. You can do these for example by doing these:
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
*/
int ha_example::update_row(const byte * old_data, byte * new_data)
{

  DBUG_ENTER("ha_example::update_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  This will delete a row. buf will contain a copy of the row to be deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_nexT() or index call).
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier.
  Keep in mind that the server does no guarentee consecutive deletions. ORDER BY
  clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table information.
  Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select it is
  used for removing duplicates while in insert it is used for REPLACE calls.
*/
int ha_example::delete_row(const byte * buf)
{
  DBUG_ENTER("ha_example::delete_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/
int ha_example::index_read(byte * buf, const byte * key,
                           uint key_len __attribute__((unused)),
                           enum ha_rkey_function find_flag
                           __attribute__((unused)))
{
  DBUG_ENTER("ha_example::index_read");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  Positions an index cursor to the index specified in key. Fetches the
  row if any.  This is only used to read whole keys.
*/
int ha_example::index_read_idx(byte * buf, uint index, const byte * key,
                               uint key_len __attribute__((unused)),
                               enum ha_rkey_function find_flag
                               __attribute__((unused)))
{
  DBUG_ENTER("ha_example::index_read_idx");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  Used to read forward through the index.
*/
int ha_example::index_next(byte * buf)
{
  DBUG_ENTER("ha_example::index_next");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  Used to read backwards through the index.
*/
int ha_example::index_prev(byte * buf)
{
  DBUG_ENTER("ha_example::index_prev");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  index_first() asks for the first key in the index.

  Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
  and sql_select.cc.
*/
int ha_example::index_first(byte * buf)
{
  DBUG_ENTER("ha_example::index_first");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  index_last() asks for the last key in the index.

  Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
  and sql_select.cc.
*/
int ha_example::index_last(byte * buf)
{
  DBUG_ENTER("ha_example::index_last");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  rnd_init() is called when the system wants the storage engine to do a table
  scan.
  See the example in the introduction at the top of this file to see when
  rnd_init() is called.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
  and sql_update.cc.
*/
int ha_example::rnd_init(bool scan)
{
  DBUG_ENTER("ha_example::rnd_init");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_example::rnd_end()
{
  DBUG_ENTER("ha_example::rnd_end");
  DBUG_RETURN(0);
}

/*
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
  and sql_update.cc.
*/
int ha_example::rnd_next(byte *buf)
{
  DBUG_ENTER("ha_example::rnd_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


/*
  position() is called after each call to rnd_next() if the data needs
  to be ordered. You can do something like the following to store
  the position:
  my_store_ptr(ref, ref_length, current_position);

  The server uses ref to store data. ref_length in the above case is
  the size needed to store current_position. ref is just a byte array
  that the server will maintain. If you are using offsets to mark rows, then
  current_position should be the offset. If it is a primary key like in
  BDB, then it needs to be a primary key.

  Called from filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc.
*/
void ha_example::position(const byte *record)
{
  DBUG_ENTER("ha_example::position");
  DBUG_VOID_RETURN;
}


/*
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.
  Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
*/
int ha_example::rnd_pos(byte * buf, byte *pos)
{
  DBUG_ENTER("ha_example::rnd_pos");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
  Another note, you will probably want to have the following in your
  code:
  if (records < 2)
    records = 2;
  The reason is that the server will optimize for cases of only a single
  record. If in a table scan you don't know the number of records
  it will probably be better to set records to two so you can return
  as many records as you need.
  Along with records a few more variables you may wish to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in:
    filesort.cc
    ha_heap.cc
    item_sum.cc
    opt_sum.cc
    sql_delete.cc
    sql_delete.cc
    sql_derived.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_table.cc
    sql_union.cc
    sql_update.cc

*/
void ha_example::info(uint flag)
{
  DBUG_ENTER("ha_example::info");
  DBUG_VOID_RETURN;
}


/*
  extra() is called whenever the server wishes to send a hint to
  the storage engine. The myisam engine implements the most hints.
  ha_innodb.cc has the most exhaustive list of these hints.
*/
int ha_example::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_example::extra");
  DBUG_RETURN(0);
}


/*
  Deprecated and likely to be removed in the future. Storage engines normally
  just make a call like:
  ha_example::extra(HA_EXTRA_RESET);
  to handle it.
*/
int ha_example::reset(void)
{
  DBUG_ENTER("ha_example::reset");
  DBUG_RETURN(0);
}


/*
  Used to delete all rows in a table. Both for cases of truncate and
  for cases where the optimizer realizes that all rows will be
  removed as a result of a SQL statement.

  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_select_lex_unit::exec().
*/
int ha_example::delete_all_rows()
{
  DBUG_ENTER("ha_example::delete_all_rows");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/*
  First you should go read the section "locking functions for mysql" in
  lock.cc to understand this.
  This create a lock on the table. If you are implementing a storage engine
  that can handle transacations look at ha_berkely.cc to see how you will
  want to goo about doing this. Otherwise you should consider calling flock()
  here.

  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().
*/
int ha_example::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_example::external_lock");
  DBUG_RETURN(0);
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

  Berkeley DB for example  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
  (which signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.

  Called from lock.cc by get_lock_data().
*/
THR_LOCK_DATA **ha_example::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

/*
  Used to delete a table. By the time delete_table() has been called all
  opened references to this table will have been closed (and your globally
  shared references released. The variable name will just be the name of
  the table. You will need to remove any files you have created at this point.

  If you do not implement this, the default delete_table() is called from
  handler.cc and it will delete all files with the file extentions returned
  by bas_ext().

  Called from handler.cc by delete_table and  ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.
*/
int ha_example::delete_table(const char *name)
{
  DBUG_ENTER("ha_example::delete_table");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
}

/*
  Renames a table from one name to another from alter table call.

  If you do not implement this, the default rename_table() is called from
  handler.cc and it will delete all files with the file extentions returned
  by bas_ext().

  Called from sql_table.cc by mysql_rename_table().
*/
int ha_example::rename_table(const char * from, const char * to)
{
  DBUG_ENTER("ha_example::rename_table ");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/*
  Given a starting key, and an ending key estimate the number of rows that
  will exist between the two. end_key may be empty which in case determine
  if start_key matches any rows.

  Called from opt_range.cc by check_quick_keys().
*/
ha_rows ha_example::records_in_range(uint inx, key_range *min_key,
                                     key_range *max_key)
{
  DBUG_ENTER("ha_example::records_in_range");
  DBUG_RETURN(10);                         // low number to force index usage
}


/*
  create() is called to create a database. The variable name will have the name
  of the table. When create() is called you do not need to worry about opening
  the table. Also, the FRM file will have already been created so adjusting
  create_info will not do you any good. You can overwrite the frm file at this
  point if you wish to change the table definition, but there are no methods
  currently provided for doing that.

  Called from handle.cc by ha_create_table().
*/
int ha_example::create(const char *name, TABLE *table_arg,
                       HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_example::create");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
}
#endif /* HAVE_EXAMPLE_DB */
