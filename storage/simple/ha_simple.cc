/* Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file ha_simple.cc

  @brief
  The ha_simple engine is a stubbed storage engine for simple purposes only;
  it does nothing at this point. Its purpose is to provide a source
  code illustration of how to begin writing new storage engines; see also
  /storage/simple/ha_simple.h.

  @details
  ha_simple will let you create/open/delete tables, but
  nothing further (for simple, indexes are not supported nor can data
  be stored in the table). Use this simple as a template for
  implementing the same functionality in your own storage engine. You
  can enable the simple storage engine in your build by doing the
  following during your build process:<br> ./configure
  --with-simple-storage-engine

  Once this is done, MySQL will let you create tables with:<br>
  CREATE TABLE \<table name\> (...) ENGINE=SIMPLE;

  The simple storage engine is set up to use table locks. It
  implements an simple "SHARE" that is inserted into a hash by table
  name. You can use this to store information of state that any
  simple handler object will be able to see when it is using that
  table.

  Please read the object definition in ha_simple.h before reading the rest
  of this file.

  @note
  When you create an SIMPLE table, the MySQL Server creates a table .frm
  (format) file in the database directory, using the table name as the file
  name as is customary with MySQL. No other files are created. To get an idea
  of what occurs, here is an simple select that would do a scan of an entire
  table:

  @code
  ha_simple::store_lock
  ha_simple::external_lock
  ha_simple::info
  ha_simple::rnd_init
  ha_simple::extra
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::rnd_next
  ha_simple::extra
  ha_simple::external_lock
  ha_simple::extra
  ENUM HA_EXTRA_RESET        Reset database to after open
  @endcode

  Here you see that the simple storage engine has 9 rows called before
  rnd_next signals that it has reached the end of its data. Also note that
  the table in question was already opened; had it not been open, a call to
  ha_simple::open() would also have been necessary. Calls to
  ha_simple::extra() are hints as to what will be occuring to the request.

  A Longer Simple can be found called the "Skeleton Engine" which can be
  found on TangentOrg. It has both an engine and a full build environment
  for building a pluggable storage engine.

  Happy coding!<br>
    -Brian
*/

#include "storage/simple/ha_simple.h"

#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/sql_class.h"
#include "sql/sql_plugin.h"
#include "sql/table.h"
#include "sql/field.h"
#include "typelib.h"

static handler *simple_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool partitioned, MEM_ROOT *mem_root);

handlerton *simple_hton;

/* Interface to mysqld, to check system tables supported by SE */
static bool simple_is_supported_system_table(const char *db,
                                              const char *table_name,
                                              bool is_sql_layer_system_table);

Simple_share::Simple_share() { thr_lock_init(&lock); }

static int simple_init_func(void *p) {
  DBUG_ENTER("simple_init_func");

  simple_hton = (handlerton *)p;
  simple_hton->state = SHOW_OPTION_YES;
  simple_hton->create = simple_create_handler;
  simple_hton->flags = HTON_CAN_RECREATE;
  simple_hton->is_supported_system_table = simple_is_supported_system_table;

  DBUG_RETURN(0);
}

/**
  @brief
  Simple of simple lock controls. The "share" it creates is a
  structure we will pass to each simple handler. Do you have to have
  one of these? Well, you have pieces that are used for locking, and
  they are needed to function.
*/

Simple_share *ha_simple::get_share(const char *table_name) {
  Simple_share *tmp_share;

  DBUG_ENTER("ha_simple::get_share()");

  lock_shared_ha_data(); //Simple_share のロック
  if (!(tmp_share = static_cast<Simple_share *>(get_ha_share_ptr()))) { // すでにあればそれを取得、なければ作成
    tmp_share = new Simple_share;
    if (!tmp_share) goto err;
    tmp_share->data_file_name = table_name;
    tmp_share->write_opened = false;

    set_ha_share_ptr(static_cast<Handler_share *>(tmp_share));
  }
err:
  unlock_shared_ha_data();
  DBUG_RETURN(tmp_share);
}

static handler *simple_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_simple(hton, table);
}

ha_simple::ha_simple(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {}

/*
  List of all system tables specific to the SE.
  Array element would look like below,
     { "<database_name>", "<system table name>" },
  The last element MUST be,
     { (const char*)NULL, (const char*)NULL }

  This array is optional, so every SE need not implement it.
*/
static st_handler_tablename ha_simple_system_tables[] = {
    {(const char *)NULL, (const char *)NULL}};

/**
  @brief Check if the given db.tablename is a system table for this SE.

  @param db                         Database name to check.
  @param table_name                 table name to check.
  @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                    layer system table.

  @return
    @retval true   Given db.table_name is supported system table.
    @retval false  Given db.table_name is not a supported system table.
*/
static bool simple_is_supported_system_table(const char *db,
                                              const char *table_name,
                                              bool is_sql_layer_system_table) {
  st_handler_tablename *systab;

  // Does this SE support "ALL" SQL layer system tables ?
  if (is_sql_layer_system_table) return false;

  // Check if this is SE layer system tables
  systab = ha_simple_system_tables;
  while (systab && systab->db) {
    if (systab->db == db && strcmp(systab->tablename, table_name) == 0)
      return true;
    systab++;
  }

  return false;
}

/**
  @brief
  Used for opening tables. The name will be the name of the file.

  @details
  A table is opened when it needs to be opened; e.g. when a request comes in
  for a SELECT on the table (tables are not open and closed for each request,
  they are cached).

  Called from handler.cc by handler::ha_open(). The server opens all tables by
  calling ha_open() which then calls the handler specific open().

  @see
  handler::ha_open() in handler.cc
*/

int ha_simple::open(const char *name, int, uint, const dd::Table *) {
  DBUG_ENTER("ha_simple::open");

  if (!(share = get_share(name))) DBUG_RETURN(1); // Simple_shareの取得
  thr_lock_data_init(&share->lock, &lock, NULL); // ロックオブジェクトの初期化

  if ((data_file = my_open(share->data_file_name, O_RDONLY, MYF(0))) == -1) {
      close();
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/**
  @brief
  Closes a table.

  @details
  Called from sql_base.cc, sql_select.cc, and table.cc. In sql_select.cc it is
  only used to close up temporary tables or during the process where a
  temporary table is converted over to being a myisam table.

  For sql_base.cc look at close_data_tables().

  @see
  sql_base.cc, sql_select.cc and table.cc
*/

int ha_simple::close(void) {
  DBUG_ENTER("ha_simple::close");
  DBUG_RETURN(0);
}

/**
  @brief
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happening. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.

  @details
  Simple of this would be:
  @code
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }
  @endcode

  See ha_tina.cc for an simple of extracting all of the data as strings.
  ha_berekly.cc has an simple of how to store it intact by "packing" it
  for ha_berkeley's own native storage type.

  See the note for update_row() on auto_increments. This case also applies to
  write_row().

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.

  @see
  item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc and sql_update.cc
*/

int ha_simple::write_row(uchar *) {
  DBUG_ENTER("ha_simple::write_row");
  /*
    Simple of a successful write_row. We don't store the data
    anywhere; they are thrown away. A real implementation will
    probably need to do something with 'buf'. We report a success
    here, to pretend that the insert was successful.
  */
  int size;

  ha_statistic_increment(&System_status_var::ha_write_count);

  if(!share->write_opened)
      if(init_writer()) DBUG_RETURN(-1);

  size = encode_quote();
  if((my_write(share->write_fd, (uchar *)buffer.ptr(), size, MYF(0))) < 0)
      DBUG_RETURN(-1);

  stats.records++;
  DBUG_RETURN(0);
}

/**
 *  Added to write_row
 */
int ha_simple::init_writer() {
    DBUG_ENTER("ha_simple::init_writer");

    if((share->write_fd = my_open(share->data_file_name, O_RDWR | O_APPEND, MYF(0))) == -1) {
        DBUG_RETURN(-1);
    }
    share->write_opened = true;

    DBUG_RETURN(0);
}

/**
 * Added to write_row
 */
int ha_simple::encode_quote() {
    char attribute_buffer[1024];
    String attribute(attribute_buffer, sizeof(attribute_buffer), &my_charset_bin); //??

    my_bitmap_map *org_bitmap = tmp_use_all_columns(table, table->read_set);
    buffer.length(0);

    for(Field **field = table->field; *field; field++) {
        const char *p;
        const char *end;

        (*field)->val_str(&attribute, &attribute); //?? クエリ文字列の実際の長さをattributeにセット
        p = attribute.ptr(); // 書き込む文字列の先頭にポイインタをセット
        end = attribute.length() + p;  // 書き込む文字列の終端にポインタをセット

        buffer.append('"');
        for (; p < end; p++) {
            buffer.append(*p);
        }
        buffer.append('"');
        buffer.append(',');
    }

    buffer.length(buffer.length() - 1);
    buffer.append('\n');

    tmp_restore_column_map(table->read_set, org_bitmap); //?? 読み取りフラグを寝かせる
    return (buffer.length());
}

/**
  @brief
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in it.
  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.

  @details
  Currently new_data will not have an updated auto_increament record. You can
  do this for simple by doing:

  @code

  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  @endcode

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.

  @see
  sql_select.cc, sql_acl.cc, sql_update.cc and sql_insert.cc
*/
int ha_simple::update_row(const uchar *, uchar *) {
  DBUG_ENTER("ha_simple::update_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/**
  @brief
  This will delete a row. buf will contain a copy of the row to be deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_nexT() or index call).

  @details
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier. Keep in mind that the server does
  not guarantee consecutive deletions. ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table
  information.  Called in sql_delete.cc, sql_insert.cc, and
  sql_select.cc. In sql_select it is used for removing duplicates
  while in insert it is used for REPLACE calls.

  @see
  sql_acl.cc, sql_udf.cc, sql_delete.cc, sql_insert.cc and sql_select.cc
*/

int ha_simple::delete_row(const uchar *) {
  DBUG_ENTER("ha_simple::delete_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/**
  @brief
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/

int ha_simple::index_read_map(uchar *, const uchar *, key_part_map,
                               enum ha_rkey_function) {
  int rc;
  DBUG_ENTER("ha_simple::index_read");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  Used to read forward through the index.
*/

int ha_simple::index_next(uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::index_next");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  Used to read backwards through the index.
*/

int ha_simple::index_prev(uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::index_prev");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  index_first() asks for the first key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_simple::index_first(uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::index_first");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  index_last() asks for the last key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_simple::index_last(uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::index_last");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  rnd_init() is called when the system wants the storage engine to do a table
  scan. See the simple in the introduction at the top of this file to see when
  rnd_init() is called.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_simple::rnd_init(bool) {
  DBUG_ENTER("ha_simple::rnd_init");
  DBUG_RETURN(0);
}

int ha_simple::rnd_end() {
  DBUG_ENTER("ha_simple::rnd_end");
  DBUG_RETURN(0);
}

/**
  @brief
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_simple::rnd_next(uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::rnd_next");
  rc = HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}

/**
  @brief
  position() is called after each call to rnd_next() if the data needs
  to be ordered. You can do something like the following to store
  the position:
  @code
  my_store_ptr(ref, ref_length, current_position);
  @endcode

  @details
  The server uses ref to store data. ref_length in the above case is
  the size needed to store current_position. ref is just a byte array
  that the server will maintain. If you are using offsets to mark rows, then
  current_position should be the offset. If it is a primary key like in
  BDB, then it needs to be a primary key.

  Called from filesort.cc, sql_select.cc, sql_delete.cc, and sql_update.cc.

  @see
  filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc
*/
void ha_simple::position(const uchar *) {
  DBUG_ENTER("ha_simple::position");
  DBUG_VOID_RETURN;
}

/**
  @brief
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.

  @details
  Called from filesort.cc, records.cc, sql_insert.cc, sql_select.cc, and
  sql_update.cc.

  @see
  filesort.cc, records.cc, sql_insert.cc, sql_select.cc and sql_update.cc
*/
int ha_simple::rnd_pos(uchar *, uchar *) {
  int rc;
  DBUG_ENTER("ha_simple::rnd_pos");
  rc = HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}

/**
  @brief
  ::info() is used to return information to the optimizer. See my_base.h for
  the complete description.

  @details
  Currently this table handler doesn't implement most of the fields really
  needed. SHOW also makes use of this data.

  You will probably want to have the following in your code:
  @code
  if (records < 2)
    records = 2;
  @endcode
  The reason is that the server will optimize for cases of only a single
  record. If, in a table scan, you don't know the number of records, it
  will probably be better to set records to two so you can return as many
  records as you need. Along with records, a few more variables you may wish
  to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc, and sql_update.cc.

  @see
  filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc and sql_update.cc
*/
int ha_simple::info(uint) {
  DBUG_ENTER("ha_simple::info");
  DBUG_RETURN(0);
}

/**
  @brief
  extra() is called whenever the server wishes to send a hint to
  the storage engine. The myisam engine implements the most hints.
  ha_innodb.cc has the most exhaustive list of these hints.

    @see
  ha_innodb.cc
*/
int ha_simple::extra(enum ha_extra_function) {
  DBUG_ENTER("ha_simple::extra");
  DBUG_RETURN(0);
}

/**
  @brief
  Used to delete all rows in a table, including cases of truncate and cases
  where the optimizer realizes that all rows will be removed as a result of an
  SQL statement.

  @details
  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_select_lex_unit::exec().

  @see
  Item_func_group_concat::clear(), Item_sum_count_distinct::clear() and
  Item_func_group_concat::clear() in item_sum.cc;
  mysql_delete() in sql_delete.cc;
  JOIN::reinit() in sql_select.cc and
  st_select_lex_unit::exec() in sql_union.cc.
*/
int ha_simple::delete_all_rows() {
  DBUG_ENTER("ha_simple::delete_all_rows");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/**
  @brief
  This create a lock on the table. If you are implementing a storage engine
  that can handle transacations look at ha_berkely.cc to see how you will
  want to go about doing this. Otherwise you should consider calling flock()
  here. Hint: Read the section "locking functions for mysql" in lock.cc to
  understand this.

  @details
  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().

  @see
  lock.cc by lock_external() and unlock_external() in lock.cc;
  the section "locking functions for mysql" in lock.cc;
  copy_data_between_tables() in sql_table.cc.
*/
int ha_simple::external_lock(THD *, int) {
  DBUG_ENTER("ha_simple::external_lock");
  DBUG_RETURN(0);
}

/**
  @brief
  The idea with handler::store_lock() is: The statement decides which locks
  should be needed for the table. For updates/deletes/inserts we get WRITE
  locks, for SELECT... we get read locks.

  @details
  Before adding the lock into the table lock handler (see thr_lock.c),
  mysqld calls store lock with the requested locks. Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all), or add locks
  for many tables (like we do when we are using a MERGE handler).

  Berkeley DB, for simple, changes all WRITE locks to TL_WRITE_ALLOW_WRITE
  (which signals that we are doing WRITES, but are still allowing other
  readers and writers).

  When releasing locks, store_lock() is also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time). In the future we will probably try to remove this.

  Called from lock.cc by get_lock_data().

  @note
  In this method one should NEVER rely on table->in_use, it may, in fact,
  refer to a different thread! (this happens if get_lock_data() is called
  from mysql_lock_abort_for_thread() function)

  @see
  get_lock_data() in lock.cc
*/
THR_LOCK_DATA **ha_simple::store_lock(THD *, THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type) {
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) lock.type = lock_type;
  *to++ = &lock;
  return to;
}

/**
  @brief
  Used to delete a table. By the time delete_table() has been called all
  opened references to this table will have been closed (and your globally
  shared references released). The variable name will just be the name of
  the table. You will need to remove any files you have created at this point.

  @details
  If you do not implement this, the default delete_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from handler.cc by delete_table and ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.

  @see
  delete_table and ha_create_table() in handler.cc
*/
int ha_simple::delete_table(const char *, const dd::Table *) {
  DBUG_ENTER("ha_simple::delete_table");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
}

/**
  @brief
  Renames a table from one name to another via an alter table call.

  @details
  If you do not implement this, the default rename_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from sql_table.cc by mysql_rename_table().

  @see
  mysql_rename_table() in sql_table.cc
*/
int ha_simple::rename_table(const char *, const char *, const dd::Table *,
                             dd::Table *) {
  DBUG_ENTER("ha_simple::rename_table ");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/**
  @brief
  Given a starting key and an ending key, estimate the number of rows that
  will exist between the two keys.

  @details
  end_key may be empty, in which case determine if start_key matches any rows.

  Called from opt_range.cc by check_quick_keys().

  @see
  check_quick_keys() in opt_range.cc
*/
ha_rows ha_simple::records_in_range(uint, key_range *, key_range *) {
  DBUG_ENTER("ha_simple::records_in_range");
  DBUG_RETURN(10);  // low number to force index usage
}

static MYSQL_THDVAR_STR(last_create_thdvar, PLUGIN_VAR_MEMALLOC, NULL, NULL,
                        NULL, NULL);

static MYSQL_THDVAR_UINT(create_count_thdvar, 0, NULL, NULL, NULL, 0, 0, 1000,
                         0);

/**
  @brief
  create() is called to create a database. The variable name will have the name
  of the table.

  @details
  When create() is called you do not need to worry about
  opening the table. Also, the .frm file will have already been
  created so adjusting create_info is not necessary. You can overwrite
  the .frm file at this point if you wish to change the table
  definition, but there are no methods currently provided for doing
  so.

  Called from handle.cc by ha_create_table().

  @see
  ha_create_table() in handle.cc
*/

int ha_simple::create(const char *name, TABLE *, HA_CREATE_INFO *,
                       dd::Table *) {
  DBUG_ENTER("ha_simple::create");
  /*
    This is not implemented but we want someone to be able to see that it
    works.
  */
  File table_file;
  if((table_file = my_create(name, 0, O_RDWR, MYF(0))) < 0)
      DBUG_RETURN(-1);
  if((my_close(table_file, MYF(0))) < 0)
      DBUG_RETURN(-1);

  /*
    It's just an exammple of THDVAR_SET() usage below.
  */
  THD *thd = ha_thd();
  char *buf = (char *)my_malloc(PSI_NOT_INSTRUMENTED, SHOW_VAR_FUNC_BUFF_SIZE,
                                MYF(MY_FAE));
  snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "Last creation '%s'", name);
  THDVAR_SET(thd, last_create_thdvar, buf);
  my_free(buf);

  uint count = THDVAR(thd, create_count_thdvar) + 1;
  THDVAR_SET(thd, create_count_thdvar, &count);

  DBUG_RETURN(0);
}

struct st_mysql_storage_engine simple_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static ulong srv_enum_var = 0;
static ulong srv_ulong_var = 0;
static double srv_double_var = 0;
static int srv_signed_int_var = 0;
static long srv_signed_long_var = 0;
static longlong srv_signed_longlong_var = 0;

const char *enum_var_names[] = {"e1", "e2", NullS};

TYPELIB enum_var_typelib = {array_elements(enum_var_names) - 1,
                            "enum_var_typelib", enum_var_names, NULL};

static MYSQL_SYSVAR_ENUM(enum_var,                        // name
                         srv_enum_var,                    // varname
                         PLUGIN_VAR_RQCMDARG,             // opt
                         "Sample ENUM system variable.",  // comment
                         NULL,                            // check
                         NULL,                            // update
                         0,                               // def
                         &enum_var_typelib);              // typelib

static MYSQL_SYSVAR_ULONG(ulong_var, srv_ulong_var, PLUGIN_VAR_RQCMDARG,
                          "0..1000", NULL, NULL, 8, 0, 1000, 0);

static MYSQL_SYSVAR_DOUBLE(double_var, srv_double_var, PLUGIN_VAR_RQCMDARG,
                           "0.500000..1000.500000", NULL, NULL, 8.5, 0.5,
                           1000.5,
                           0);  // reserved always 0

static MYSQL_THDVAR_DOUBLE(double_thdvar, PLUGIN_VAR_RQCMDARG,
                           "0.500000..1000.500000", NULL, NULL, 8.5, 0.5,
                           1000.5, 0);

static MYSQL_SYSVAR_INT(signed_int_var, srv_signed_int_var, PLUGIN_VAR_RQCMDARG,
                        "INT_MIN..INT_MAX", NULL, NULL, -10, INT_MIN, INT_MAX,
                        0);

static MYSQL_THDVAR_INT(signed_int_thdvar, PLUGIN_VAR_RQCMDARG,
                        "INT_MIN..INT_MAX", NULL, NULL, -10, INT_MIN, INT_MAX,
                        0);

static MYSQL_SYSVAR_LONG(signed_long_var, srv_signed_long_var,
                         PLUGIN_VAR_RQCMDARG, "LONG_MIN..LONG_MAX", NULL, NULL,
                         -10, LONG_MIN, LONG_MAX, 0);

static MYSQL_THDVAR_LONG(signed_long_thdvar, PLUGIN_VAR_RQCMDARG,
                         "LONG_MIN..LONG_MAX", NULL, NULL, -10, LONG_MIN,
                         LONG_MAX, 0);

static MYSQL_SYSVAR_LONGLONG(signed_longlong_var, srv_signed_longlong_var,
                             PLUGIN_VAR_RQCMDARG, "LLONG_MIN..LLONG_MAX", NULL,
                             NULL, -10, LLONG_MIN, LLONG_MAX, 0);

static MYSQL_THDVAR_LONGLONG(signed_longlong_thdvar, PLUGIN_VAR_RQCMDARG,
                             "LLONG_MIN..LLONG_MAX", NULL, NULL, -10, LLONG_MIN,
                             LLONG_MAX, 0);

static SYS_VAR *simple_system_variables[] = {
    MYSQL_SYSVAR(enum_var),
    MYSQL_SYSVAR(ulong_var),
    MYSQL_SYSVAR(double_var),
    MYSQL_SYSVAR(double_thdvar),
    MYSQL_SYSVAR(last_create_thdvar),
    MYSQL_SYSVAR(create_count_thdvar),
    MYSQL_SYSVAR(signed_int_var),
    MYSQL_SYSVAR(signed_int_thdvar),
    MYSQL_SYSVAR(signed_long_var),
    MYSQL_SYSVAR(signed_long_thdvar),
    MYSQL_SYSVAR(signed_longlong_var),
    MYSQL_SYSVAR(signed_longlong_thdvar),
    NULL};

// this is an simple of SHOW_FUNC
static int show_func_simple(MYSQL_THD, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;  // it's of SHOW_VAR_FUNC_BUFF_SIZE bytes
  snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE,
           "enum_var is %lu, ulong_var is %lu, "
           "double_var is %f, signed_int_var is %d, "
           "signed_long_var is %ld, signed_longlong_var is %lld",
           srv_enum_var, srv_ulong_var, srv_double_var, srv_signed_int_var,
           srv_signed_long_var, srv_signed_longlong_var);
  return 0;
}

struct simple_vars_t {
  ulong var1;
  double var2;
  char var3[64];
  bool var4;
  bool var5;
  ulong var6;
};

simple_vars_t simple_vars = {100, 20.01, "three hundred", true, 0, 8250};

static SHOW_VAR show_status_simple[] = {
    {"var1", (char *)&simple_vars.var1, SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"var2", (char *)&simple_vars.var2, SHOW_DOUBLE, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};

static SHOW_VAR show_array_simple[] = {
    {"array", (char *)show_status_simple, SHOW_ARRAY, SHOW_SCOPE_GLOBAL},
    {"var3", (char *)&simple_vars.var3, SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {"var4", (char *)&simple_vars.var4, SHOW_BOOL, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}};

static SHOW_VAR func_status[] = {
    {"simple_func_simple", (char *)show_func_simple, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"simple_status_var5", (char *)&simple_vars.var5, SHOW_BOOL,
     SHOW_SCOPE_GLOBAL},
    {"simple_status_var6", (char *)&simple_vars.var6, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"simple_status", (char *)show_array_simple, SHOW_ARRAY,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}};

mysql_declare_plugin(simple){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &simple_storage_engine,
    "SIMPLE",
    "tom--bo",
    "Simple storage engine",
    PLUGIN_LICENSE_GPL,
    simple_init_func, /* Plugin Init */
    NULL,              /* Plugin check uninstall */
    NULL,              /* Plugin Deinit */
    0x0001 /* 0.1 */,
    func_status,              /* status variables */
    simple_system_variables, /* system variables */
    NULL,                     /* config options */
    0,                        /* flags */
} mysql_declare_plugin_end;
