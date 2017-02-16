/* Copyright (c) 2004, 2013, Oracle and/or its affiliates.
   Copyright (c) 2010, 2014, SkySQL Ab.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file ha_example.cc

  @brief
  The ha_example engine is a stubbed storage engine for example purposes only;
  it does almost nothing at this point. Its purpose is to provide a source
  code illustration of how to begin writing new storage engines; see also
  storage/example/ha_example.h.

  Additionally, this file includes an example of a daemon plugin which does
  nothing at all - absolutely nothing, even less than example storage engine.
  But it shows that one dll/so can contain more than one plugin.

  @details
  ha_example will let you create/open/delete tables, but
  nothing further (for example, indexes are not supported nor can data
  be stored in the table). It also provides new status (example_func_example)
  and system (example_ulong_var and example_enum_var) variables.

  Use this example as a template for implementing the same functionality in
  your own storage engine. You can enable the example storage engine in your
  build by doing the following during your build process:<br> ./configure
  --with-example-storage-engine

  Once this is done, MySQL will let you create tables with:<br>
  CREATE TABLE <table name> (...) ENGINE=EXAMPLE;

  The example storage engine is set up to use table locks. It
  implements an example "SHARE" that is inserted into a hash by table
  name. You can use this to store information of state that any
  example handler object will be able to see when it is using that
  table.

  Please read the object definition in ha_example.h before reading the rest
  of this file.

  @note
  When you create an EXAMPLE table, the MySQL Server creates a table .frm
  (format) file in the database directory, using the table name as the file
  name as is customary with MySQL. No other files are created. To get an idea
  of what occurs, here is an example select that would do a scan of an entire
  table:

  @code
  ha_example::store_lock
  ha_example::external_lock
  ha_example::info
  ha_example::rnd_init
  ha_example::extra
  ENUM HA_EXTRA_CACHE        Cache record in HA_rrnd()
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
  ENUM HA_EXTRA_NO_CACHE     End caching of records (def)
  ha_example::external_lock
  ha_example::extra
  ENUM HA_EXTRA_RESET        Reset database to after open
  @endcode

  Here you see that the example storage engine has 9 rows called before
  rnd_next signals that it has reached the end of its data. Also note that
  the table in question was already opened; had it not been open, a call to
  ha_example::open() would also have been necessary. Calls to
  ha_example::extra() are hints as to what will be occuring to the request.

  A Longer Example can be found called the "Skeleton Engine" which can be 
  found on TangentOrg. It has both an engine and a full build environment
  for building a pluggable storage engine.

  Happy coding!<br>
    -Brian
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#include <my_config.h>
#include <mysql/plugin.h>
#include "ha_example.h"
#include "sql_class.h"

static handler *example_create_handler(handlerton *hton,
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root);

handlerton *example_hton;

/* Variables for example share methods */

/* 
   Hash used to track the number of open tables; variable for example share
   methods
*/
static HASH example_open_tables;

/* The mutex used to init the hash; variable for example share methods */
mysql_mutex_t example_mutex;


/**
  Structure for CREATE TABLE options (table options).
  It needs to be called ha_table_option_struct.

  The option values can be specified in the CREATE TABLE at the end:
  CREATE TABLE ( ... ) *here*
*/

struct ha_table_option_struct
{
  const char *strparam;
  ulonglong ullparam;
  uint enumparam;
  bool boolparam;
};


/**
  Structure for CREATE TABLE options (field options).
  It needs to be called ha_field_option_struct.

  The option values can be specified in the CREATE TABLE per field:
  CREATE TABLE ( field ... *here*, ... )
*/

struct ha_field_option_struct
{
  const char *complex_param_to_parse_it_in_engine;
};

/*
  no example here, but index options can be declared similarly
  using the ha_index_option_struct structure.

  Their values can be specified in the CREATE TABLE per index:
  CREATE TABLE ( field ..., .., INDEX .... *here*, ... )
*/

ha_create_table_option example_table_option_list[]=
{
  /*
    one numeric option, with the default of UINT_MAX32, valid
    range of values 0..UINT_MAX32, and a "block size" of 10
    (any value must be divisible by 10).
  */
  HA_TOPTION_NUMBER("ULL", ullparam, UINT_MAX32, 0, UINT_MAX32, 10),
  /*
    one option that takes an arbitrary string
  */
  HA_TOPTION_STRING("STR", strparam),
  /*
    one enum option. a valid values are strings ONE and TWO.
    A default value is 0, that is "one".
  */
  HA_TOPTION_ENUM("one_or_two", enumparam, "one,two", 0),
  /*
    one boolean option, the valid values are YES/NO, ON/OFF, 1/0.
    The default is 1, that is true, yes, on.
  */
  HA_TOPTION_BOOL("YESNO", boolparam, 1),
  HA_TOPTION_END
};

ha_create_table_option example_field_option_list[]=
{
  /*
    If the engine wants something more complex than a string, number, enum,
    or boolean - for example a list - it needs to specify the option
    as a string and parse it internally.
  */
  HA_FOPTION_STRING("COMPLEX", complex_param_to_parse_it_in_engine),
  HA_FOPTION_END
};


/**
  @brief
  Function we use in the creation of our hash to get key.
*/

static uchar* example_get_key(EXAMPLE_SHARE *share, size_t *length,
                             my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (uchar*) share->table_name;
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key ex_key_mutex_example, ex_key_mutex_EXAMPLE_SHARE_mutex;

static PSI_mutex_info all_example_mutexes[]=
{
  { &ex_key_mutex_example, "example", PSI_FLAG_GLOBAL},
  { &ex_key_mutex_EXAMPLE_SHARE_mutex, "EXAMPLE_SHARE::mutex", 0}
};

static void init_example_psi_keys()
{
  const char* category= "example";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_example_mutexes);
  PSI_server->register_mutex(category, all_example_mutexes, count);
}
#endif


static int example_init_func(void *p)
{
  DBUG_ENTER("example_init_func");

#ifdef HAVE_PSI_INTERFACE
  init_example_psi_keys();
#endif

  example_hton= (handlerton *)p;
  mysql_mutex_init(ex_key_mutex_example, &example_mutex, MY_MUTEX_INIT_FAST);
  (void) my_hash_init(&example_open_tables,system_charset_info,32,0,0,
                      (my_hash_get_key) example_get_key,0,0);

  example_hton->state=   SHOW_OPTION_YES;
  example_hton->create=  example_create_handler;
  example_hton->flags=   HTON_CAN_RECREATE;
  example_hton->table_options= example_table_option_list;
  example_hton->field_options= example_field_option_list;

  DBUG_RETURN(0);
}


static int example_done_func(void *p)
{
  int error= 0;
  DBUG_ENTER("example_done_func");

  if (example_open_tables.records)
    error= 1;
  my_hash_free(&example_open_tables);
  mysql_mutex_destroy(&example_mutex);

  DBUG_RETURN(error);
}


/**
  @brief
  Example of simple lock controls. The "share" it creates is a
  structure we will pass to each example handler. Do you have to have
  one of these? Well, you have pieces that are used for locking, and
  they are needed to function.
*/

static EXAMPLE_SHARE *get_share(const char *table_name, TABLE *table)
{
  EXAMPLE_SHARE *share;
  uint length;
  char *tmp_name;

  mysql_mutex_lock(&example_mutex);
  length=(uint) strlen(table_name);

  if (!(share=(EXAMPLE_SHARE*) my_hash_search(&example_open_tables,
                                              (uchar*) table_name,
                                              length)))
  {
    if (!(share=(EXAMPLE_SHARE *)
          my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share),
                          &tmp_name, length+1,
                          NullS)))
    {
      mysql_mutex_unlock(&example_mutex);
      return NULL;
    }

    share->use_count=0;
    share->table_name_length=length;
    share->table_name=tmp_name;
    strmov(share->table_name,table_name);
    if (my_hash_insert(&example_open_tables, (uchar*) share))
      goto error;
    thr_lock_init(&share->lock);
    mysql_mutex_init(ex_key_mutex_EXAMPLE_SHARE_mutex,
                     &share->mutex, MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  mysql_mutex_unlock(&example_mutex);

  return share;

error:
  mysql_mutex_destroy(&share->mutex);
  my_free(share);

  return NULL;
}


/**
  @brief
  Free lock controls. We call this whenever we close a table. If the table had
  the last reference to the share, then we free memory associated with it.
*/

static int free_share(EXAMPLE_SHARE *share)
{
  mysql_mutex_lock(&example_mutex);
  if (!--share->use_count)
  {
    my_hash_delete(&example_open_tables, (uchar*) share);
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->mutex);
    my_free(share);
  }
  mysql_mutex_unlock(&example_mutex);

  return 0;
}

static handler* example_create_handler(handlerton *hton,
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root)
{
  return new (mem_root) ha_example(hton, table);
}

ha_example::ha_example(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg)
{}


/**
  @brief
  If frm_error() is called then we will use this to determine
  the file extensions that exist for the storage engine. This is also
  used by the default rename_table and delete_table method in
  handler.cc.

  For engines that have two file name extentions (separate meta/index file
  and data file), the order of elements is relevant. First element of engine
  file name extentions array should be meta/index file extention. Second
  element - data file extention. This order is assumed by
  prepare_for_repair() when REPAIR TABLE ... USE_FRM is issued.

  @see
  rename_table method in handler.cc and
  delete_table method in handler.cc
*/

static const char *ha_example_exts[] = {
  NullS
};

const char **ha_example::bas_ext() const
{
  return ha_example_exts;
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

int ha_example::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_example::open");

  if (!(share = get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock,&lock,NULL);

#ifndef DBUG_OFF
  ha_table_option_struct *options= table->s->option_struct;

  DBUG_ASSERT(options);
  DBUG_PRINT("info", ("strparam: '%-.64s'  ullparam: %llu  enumparam: %u  "\
                      "boolparam: %u",
                      (options->strparam ? options->strparam : "<NULL>"),
                      options->ullparam, options->enumparam, options->boolparam));
#endif

  DBUG_RETURN(0);
}


/**
  @brief
  Closes a table. We call the free_share() function to free any resources
  that we have allocated in the "shared" structure.

  @details
  Called from sql_base.cc, sql_select.cc, and table.cc. In sql_select.cc it is
  only used to close up temporary tables or during the process where a
  temporary table is converted over to being a myisam table.

  For sql_base.cc look at close_data_tables().

  @see
  sql_base.cc, sql_select.cc and table.cc
*/

int ha_example::close(void)
{
  DBUG_ENTER("ha_example::close");
  DBUG_RETURN(free_share(share));
}


/**
  @brief
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happening. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.

  @details
  Example of this would be:
  @code
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }
  @endcode

  See ha_tina.cc for an example of extracting all of the data as strings.
  ha_berekly.cc has an example of how to store it intact by "packing" it
  for ha_berkeley's own native storage type.

  See the note for update_row() on auto_increments and timestamps. This
  case also applies to write_row().

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.

  @see
  item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc and sql_update.cc
*/

int ha_example::write_row(uchar *buf)
{
  DBUG_ENTER("ha_example::write_row");
  /*
    Example of a successful write_row. We don't store the data
    anywhere; they are thrown away. A real implementation will
    probably need to do something with 'buf'. We report a success
    here, to pretend that the insert was successful.
  */
  DBUG_RETURN(0);
}


/**
  @brief
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in it.
  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.

  @details
  Currently new_data will not have an updated auto_increament record, or
  and updated timestamp field. You can do these for example by doing:
  @code
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();
  @endcode

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.

  @see
  sql_select.cc, sql_acl.cc, sql_update.cc and sql_insert.cc
*/
int ha_example::update_row(const uchar *old_data, uchar *new_data)
{

  DBUG_ENTER("ha_example::update_row");
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

int ha_example::delete_row(const uchar *buf)
{
  DBUG_ENTER("ha_example::delete_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/**
  @brief
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/

int ha_example::index_read_map(uchar *buf, const uchar *key,
                               key_part_map keypart_map __attribute__((unused)),
                               enum ha_rkey_function find_flag
                               __attribute__((unused)))
{
  int rc;
  DBUG_ENTER("ha_example::index_read");
  rc= HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}


/**
  @brief
  Used to read forward through the index.
*/

int ha_example::index_next(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_example::index_next");
  rc= HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}


/**
  @brief
  Used to read backwards through the index.
*/

int ha_example::index_prev(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_example::index_prev");
  rc= HA_ERR_WRONG_COMMAND;
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
int ha_example::index_first(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_example::index_first");
  rc= HA_ERR_WRONG_COMMAND;
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
int ha_example::index_last(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_example::index_last");
  rc= HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}


/**
  @brief
  rnd_init() is called when the system wants the storage engine to do a table
  scan. See the example in the introduction at the top of this file to see when
  rnd_init() is called.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
  and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and sql_update.cc
*/
int ha_example::rnd_init(bool scan)
{
  DBUG_ENTER("ha_example::rnd_init");
  DBUG_RETURN(0);
}

int ha_example::rnd_end()
{
  DBUG_ENTER("ha_example::rnd_end");
  DBUG_RETURN(0);
}


/**
  @brief
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
  and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and sql_update.cc
*/
int ha_example::rnd_next(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_example::rnd_next");
  rc= HA_ERR_END_OF_FILE;
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
void ha_example::position(const uchar *record)
{
  DBUG_ENTER("ha_example::position");
  DBUG_VOID_RETURN;
}


/**
  @brief
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.

  @details
  Called from filesort.cc, records.cc, sql_insert.cc, sql_select.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_insert.cc, sql_select.cc and sql_update.cc
*/
int ha_example::rnd_pos(uchar *buf, uchar *pos)
{
  int rc;
  DBUG_ENTER("ha_example::rnd_pos");
  rc= HA_ERR_WRONG_COMMAND;
  DBUG_RETURN(rc);
}


/**
  @brief
  ::info() is used to return information to the optimizer. See my_base.h for
  the complete description.

  @details
  Currently this table handler doesn't implement most of the fields really needed.
  SHOW also makes use of this data.

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
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_table.cc, sql_union.cc, and sql_update.cc.

  @see
  filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc, sql_delete.cc,
  sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc, sql_show.cc, sql_table.cc,
  sql_union.cc and sql_update.cc
*/
int ha_example::info(uint flag)
{
  DBUG_ENTER("ha_example::info");
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
int ha_example::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_example::extra");
  DBUG_RETURN(0);
}


/**
  @brief
  Used to delete all rows in a table, including cases of truncate and cases where
  the optimizer realizes that all rows will be removed as a result of an SQL statement.

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
int ha_example::delete_all_rows()
{
  DBUG_ENTER("ha_example::delete_all_rows");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


/**
  @brief
  This create a lock on the table. If you are implementing a storage engine
  that can handle transacations look at ha_berkely.cc to see how you will
  want to go about doing this. Otherwise you should consider calling flock()
  here. Hint: Read the section "locking functions for mysql" in lock.cc to understand
  this.

  @details
  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().

  @see
  lock.cc by lock_external() and unlock_external() in lock.cc;
  the section "locking functions for mysql" in lock.cc;
  copy_data_between_tables() in sql_table.cc.
*/
int ha_example::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_example::external_lock");
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

  Berkeley DB, for example, changes all WRITE locks to TL_WRITE_ALLOW_WRITE
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
THR_LOCK_DATA **ha_example::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
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
  handler.cc and it will delete all files with the file extensions returned
  by bas_ext().

  Called from handler.cc by delete_table and ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.

  @see
  delete_table and ha_create_table() in handler.cc
*/
int ha_example::delete_table(const char *name)
{
  DBUG_ENTER("ha_example::delete_table");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
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
ha_rows ha_example::records_in_range(uint inx, key_range *min_key,
                                     key_range *max_key)
{
  DBUG_ENTER("ha_example::records_in_range");
  DBUG_RETURN(10);                         // low number to force index usage
}


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

int ha_example::create(const char *name, TABLE *table_arg,
                       HA_CREATE_INFO *create_info)
{
#ifndef DBUG_OFF
  ha_table_option_struct *options= table_arg->s->option_struct;
  DBUG_ENTER("ha_example::create");
  /*
    This example shows how to support custom engine specific table and field
    options.
  */
  DBUG_ASSERT(options);
  DBUG_PRINT("info", ("strparam: '%-.64s'  ullparam: %llu  enumparam: %u  "\
                      "boolparam: %u",
                      (options->strparam ? options->strparam : "<NULL>"),
                      options->ullparam, options->enumparam, options->boolparam));
  for (Field **field= table_arg->s->field; *field; field++)
  {
    ha_field_option_struct *field_options= (*field)->option_struct;
    DBUG_ASSERT(field_options);
    DBUG_PRINT("info", ("field: %s  complex: '%-.64s'",
                         (*field)->field_name,
                         (field_options->complex_param_to_parse_it_in_engine ?
                          field_options->complex_param_to_parse_it_in_engine :
                          "<NULL>")));
  }
#endif
  DBUG_RETURN(0);
}


/**
  check_if_incompatible_data() called if ALTER TABLE can't detect otherwise
  if new and old definition are compatible

  @details If there are no other explicit signs like changed number of
  fields this function will be called by compare_tables()
  (sql/sql_tables.cc) to decide should we rewrite whole table or only .frm
  file.

*/

bool ha_example::check_if_incompatible_data(HA_CREATE_INFO *info,
                                            uint table_changes)
{
  ha_table_option_struct *param_old, *param_new;
  DBUG_ENTER("ha_example::check_if_incompatible_data");
  /*
    This example shows how custom engine specific table and field
    options can be accessed from this function to be compared.
  */
  param_new= info->option_struct;
  DBUG_PRINT("info", ("new strparam: '%-.64s'  ullparam: %llu  enumparam: %u  "
                      "boolparam: %u",
                      (param_new->strparam ? param_new->strparam : "<NULL>"),
                      param_new->ullparam, param_new->enumparam,
                      param_new->boolparam));

  param_old= table->s->option_struct;
  DBUG_PRINT("info", ("old strparam: '%-.64s'  ullparam: %llu  enumparam: %u  "
                      "boolparam: %u",
                      (param_old->strparam ? param_old->strparam : "<NULL>"),
                      param_old->ullparam, param_old->enumparam,
                      param_old->boolparam));

  /*
    check important parameters:
    for this example engine, we'll assume that changing ullparam or
    boolparam requires a table to be rebuilt, while changing strparam
    or enumparam - does not.
  */
  if (param_new->ullparam != param_old->ullparam ||
      param_new->boolparam != param_old->boolparam)
    DBUG_RETURN(COMPATIBLE_DATA_NO);

#ifndef DBUG_OFF
  for (uint i= 0; i < table->s->fields; i++)
  {
    ha_field_option_struct *f_old, *f_new;
    f_old= table->s->field[i]->option_struct;
    DBUG_ASSERT(f_old);
    DBUG_PRINT("info", ("old field: %u old complex: '%-.64s'", i,
                         (f_old->complex_param_to_parse_it_in_engine ?
                          f_old->complex_param_to_parse_it_in_engine :
                          "<NULL>")));
    if (info->fields_option_struct[i])
    {
      f_new= info->fields_option_struct[i];
      DBUG_PRINT("info", ("old field: %u  new complex: '%-.64s'", i,
                          (f_new->complex_param_to_parse_it_in_engine ?
                           f_new->complex_param_to_parse_it_in_engine :
                           "<NULL>")));
    }
    else
      DBUG_PRINT("info", ("old field %i did not changed", i));
  }
#endif

  DBUG_RETURN(COMPATIBLE_DATA_YES);
}


struct st_mysql_storage_engine example_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

static ulong srv_enum_var= 0;
static ulong srv_ulong_var= 0;
static double srv_double_var= 0;

const char *enum_var_names[]=
{
  "e1", "e2", NullS
};

TYPELIB enum_var_typelib=
{
  array_elements(enum_var_names) - 1, "enum_var_typelib",
  enum_var_names, NULL
};

static MYSQL_SYSVAR_ENUM(
  enum_var,                       // name
  srv_enum_var,                   // varname
  PLUGIN_VAR_RQCMDARG,            // opt
  "Sample ENUM system variable.", // comment
  NULL,                           // check
  NULL,                           // update
  0,                              // def
  &enum_var_typelib);             // typelib

static MYSQL_THDVAR_INT(int_var, PLUGIN_VAR_RQCMDARG, "-1..1",
  NULL, NULL, 0, -1, 1, 0);

static MYSQL_SYSVAR_ULONG(
  ulong_var,
  srv_ulong_var,
  PLUGIN_VAR_RQCMDARG,
  "0..1000",
  NULL,
  NULL,
  8,
  0,
  1000,
  0);

static MYSQL_SYSVAR_DOUBLE(
  double_var,
  srv_double_var,
  PLUGIN_VAR_RQCMDARG,
  "0.500000..1000.500000",
  NULL,
  NULL,
  8.5,
  0.5,
  1000.5,
  0);                             // reserved always 0

static MYSQL_THDVAR_DOUBLE(
  double_thdvar,
  PLUGIN_VAR_RQCMDARG,
  "0.500000..1000.500000",
  NULL,
  NULL,
  8.5,
  0.5,
  1000.5,
  0);

static struct st_mysql_sys_var* example_system_variables[]= {
  MYSQL_SYSVAR(enum_var),
  MYSQL_SYSVAR(ulong_var),
  MYSQL_SYSVAR(int_var),
  MYSQL_SYSVAR(double_var),
  MYSQL_SYSVAR(double_thdvar),
  NULL
};

// this is an example of SHOW_FUNC and of my_snprintf() service
static int show_func_example(MYSQL_THD thd, struct st_mysql_show_var *var,
                             char *buf)
{
  var->type= SHOW_CHAR;
  var->value= buf; // it's of SHOW_VAR_FUNC_BUFF_SIZE bytes
  my_snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE,
              "enum_var is %lu, ulong_var is %lu, int_var is %d, "
              "double_var is %f, %.6b", // %b is a MySQL extension
              srv_enum_var, srv_ulong_var, THDVAR(thd, int_var),
              srv_double_var, "really");
  return 0;
}

static struct st_mysql_show_var func_status[]=
{
  {"example_func_example",  (char *)show_func_example, SHOW_FUNC},
  {0,0,SHOW_UNDEF}
};

struct st_mysql_daemon unusable_example=
{ MYSQL_DAEMON_INTERFACE_VERSION };

mysql_declare_plugin(example)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &example_storage_engine,
  "EXAMPLE",
  "Brian Aker, MySQL AB",
  "Example storage engine",
  PLUGIN_LICENSE_GPL,
  example_init_func,                            /* Plugin Init */
  example_done_func,                            /* Plugin Deinit */
  0x0001 /* 0.1 */,
  func_status,                                  /* status variables */
  example_system_variables,                     /* system variables */
  NULL,                                         /* config options */
  0,                                            /* flags */
}
mysql_declare_plugin_end;
maria_declare_plugin(example)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &example_storage_engine,
  "EXAMPLE",
  "Brian Aker, MySQL AB",
  "Example storage engine",
  PLUGIN_LICENSE_GPL,
  example_init_func,                            /* Plugin Init */
  example_done_func,                            /* Plugin Deinit */
  0x0001,                                       /* version number (0.1) */
  func_status,                                  /* status variables */
  example_system_variables,                     /* system variables */
  "0.1",                                        /* string version */
  MariaDB_PLUGIN_MATURITY_EXPERIMENTAL          /* maturity */
},
{
  MYSQL_DAEMON_PLUGIN,
  &unusable_example,
  "UNUSABLE",
  "Sergei Golubchik",
  "Unusable Daemon",
  PLUGIN_LICENSE_GPL,
  NULL,                                         /* Plugin Init */
  NULL,                                         /* Plugin Deinit */
  0x030E,                                       /* version number (3.14) */
  NULL,                                         /* status variables */
  NULL,                                         /* system variables */
  "3.14.15.926" ,                               /* version, as a string */
  MariaDB_PLUGIN_MATURITY_EXPERIMENTAL          /* maturity */
}
maria_declare_plugin_end;
