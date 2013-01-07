/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/*
  The servers are saved in the system table "servers"
  
  Currently, when the user performs an ALTER SERVER or a DROP SERVER
  operation, it will cause all open tables which refer to the named
  server connection to be flushed. This may cause some undesirable
  behaviour with regard to currently running transactions. It is 
  expected that the DBA knows what s/he is doing when s/he performs
  the ALTER SERVER or DROP SERVER operation.
  
  TODO:
  It is desirable for us to implement a callback mechanism instead where
  callbacks can be registered for specific server protocols. The callback
  will be fired when such a server name has been created/altered/dropped
  or when statistics are to be gathered such as how many actual connections.
  Storage engines etc will be able to make use of the callback so that
  currently running transactions etc will not be disrupted.
*/

#include "sql_priv.h"
#include "sql_servers.h"
#include "unireg.h"
#include "sql_base.h"                           // close_mysql_tables
#include "records.h"          // init_read_record, end_read_record
#include "hash_filo.h"
#include <m_ctype.h>
#include <stdarg.h>
#include "sp_head.h"
#include "sp.h"
#include "transaction.h"
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT

/*
  We only use 1 mutex to guard the data structures - THR_LOCK_servers.
  Read locked when only reading data and write-locked for all other access.
*/

static HASH servers_cache;
static MEM_ROOT mem;
static mysql_rwlock_t THR_LOCK_servers;

static bool get_server_from_table_to_cache(TABLE *table);

/* insert functions */
static bool insert_server(THD *thd, FOREIGN_SERVER *server_options);
static bool insert_server_record(TABLE *table, FOREIGN_SERVER *server);
static bool insert_server_record_into_cache(FOREIGN_SERVER *server);
static FOREIGN_SERVER *
prepare_server_struct_for_insert(LEX_SERVER_OPTIONS *server_options);
/* drop functions */ 
static bool delete_server_record(TABLE *table,
                                 char *server_name,
                                 size_t server_name_length,
                                 bool if_exists);
static bool delete_server_record_in_cache(LEX_SERVER_OPTIONS *server_options,
                                          bool if_exists);

/* update functions */
static void prepare_server_struct_for_update(LEX_SERVER_OPTIONS *server_options,
                                             FOREIGN_SERVER *existing,
                                             FOREIGN_SERVER *altered);
static bool update_server(THD *thd, FOREIGN_SERVER *existing,
                          FOREIGN_SERVER *altered);
static bool update_server_record(TABLE *table, FOREIGN_SERVER *server);
static bool update_server_record_in_cache(FOREIGN_SERVER *existing,
                                          FOREIGN_SERVER *altered);
/* utility functions */
static void merge_server_struct(FOREIGN_SERVER *from, FOREIGN_SERVER *to);



static uchar *servers_cache_get_key(FOREIGN_SERVER *server, size_t *length,
			       my_bool not_used __attribute__((unused)))
{
  DBUG_ENTER("servers_cache_get_key");
  DBUG_PRINT("info", ("server_name_length %d server_name %s",
                      server->server_name_length,
                      server->server_name));

  *length= (uint) server->server_name_length;
  DBUG_RETURN((uchar*) server->server_name);
}

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_key key_rwlock_THR_LOCK_servers;

static PSI_rwlock_info all_servers_cache_rwlocks[]=
{
  { &key_rwlock_THR_LOCK_servers, "THR_LOCK_servers", PSI_FLAG_GLOBAL}
};

static void init_servers_cache_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= array_elements(all_servers_cache_rwlocks);
  mysql_rwlock_register(category, all_servers_cache_rwlocks, count);
}
#endif /* HAVE_PSI_INTERFACE */

/*
  Initialize structures responsible for servers used in federated
  server scheme information for them from the server
  table in the 'mysql' database.

  SYNOPSIS
    servers_init()
      dont_read_server_table  TRUE if we want to skip loading data from
                            server table and disable privilege checking.

  NOTES
    This function is mostly responsible for preparatory steps, main work
    on initialization and grants loading is done in servers_reload().

  RETURN VALUES
    0	ok
    1	Could not initialize servers
*/

bool servers_init(bool dont_read_servers_table)
{
  THD  *thd;
  bool return_val= FALSE;
  DBUG_ENTER("servers_init");

#ifdef HAVE_PSI_INTERFACE
  init_servers_cache_psi_keys();
#endif

  /* init the mutex */
  if (mysql_rwlock_init(key_rwlock_THR_LOCK_servers, &THR_LOCK_servers))
    DBUG_RETURN(TRUE);

  /* initialise our servers cache */
  if (my_hash_init(&servers_cache, system_charset_info, 32, 0, 0,
                   (my_hash_get_key) servers_cache_get_key, 0, 0))
  {
    return_val= TRUE; /* we failed, out of memory? */
    goto end;
  }

  /* Initialize the mem root for data */
  init_sql_alloc(&mem, ACL_ALLOC_BLOCK_SIZE, 0);

  if (dont_read_servers_table)
    goto end;

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(TRUE);
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  /*
    It is safe to call servers_reload() since servers_* arrays and hashes which
    will be freed there are global static objects and thus are initialized
    by zeros at startup.
  */
  return_val= servers_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);

end:
  DBUG_RETURN(return_val);
}

/*
  Initialize server structures

  SYNOPSIS
    servers_load()
      thd     Current thread
      tables  List containing open "mysql.servers"

  RETURN VALUES
    FALSE  Success
    TRUE   Error

  TODO
    Revert back to old list if we failed to load new one.
*/

static bool servers_load(THD *thd, TABLE_LIST *tables)
{
  TABLE *table;
  READ_RECORD read_record_info;
  bool return_val= TRUE;
  DBUG_ENTER("servers_load");

  my_hash_reset(&servers_cache);
  free_root(&mem, MYF(0));
  init_sql_alloc(&mem, ACL_ALLOC_BLOCK_SIZE, 0);

  if (init_read_record(&read_record_info, thd, table=tables[0].table,
                       NULL, 1, 1, FALSE))
    DBUG_RETURN(TRUE);

  while (!(read_record_info.read_record(&read_record_info)))
  {
    /* return_val is already TRUE, so no need to set */
    if ((get_server_from_table_to_cache(table)))
      goto end;
  }

  return_val= FALSE;

end:
  end_read_record(&read_record_info);
  DBUG_RETURN(return_val);
}


/*
  Forget current servers cache and read new servers 
  from the conneciton table.

  SYNOPSIS
    servers_reload()
      thd  Current thread

  NOTE
    All tables of calling thread which were open and locked by LOCK TABLES
    statement will be unlocked and closed.
    This function is also used for initialization of structures responsible
    for user/db-level privilege checking.

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

bool servers_reload(THD *thd)
{
  TABLE_LIST tables[1];
  bool return_val= TRUE;
  DBUG_ENTER("servers_reload");

  DBUG_PRINT("info", ("locking servers_cache"));
  mysql_rwlock_wrlock(&THR_LOCK_servers);

  tables[0].init_one_table("mysql", 5, "servers", 7, "servers", TL_READ);

  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    /*
      Execution might have been interrupted; only print the error message
      if an error condition has been raised.
    */
    if (thd->get_stmt_da()->is_error())
      sql_print_error("Can't open and lock privilege tables: %s",
                      thd->get_stmt_da()->message_text());
    return_val= FALSE;
    goto end;
  }

  if ((return_val= servers_load(thd, tables)))
  {					// Error. Revert to old list
    /* blast, for now, we have no servers, discuss later way to preserve */

    DBUG_PRINT("error",("Reverting to old privileges"));
    servers_free();
  }

end:
  close_mysql_tables(thd);
  DBUG_PRINT("info", ("unlocking servers_cache"));
  mysql_rwlock_unlock(&THR_LOCK_servers);
  DBUG_RETURN(return_val);
}


/*
  Initialize structures responsible for servers used in federated
  server scheme information for them from the server
  table in the 'mysql' database.

  SYNOPSIS
    get_server_from_table_to_cache()
      TABLE *table         open table pointer


  NOTES
    This function takes a TABLE pointer (pointing to an opened
    table). With this open table, a FOREIGN_SERVER struct pointer
    is allocated into root memory, then each member of the FOREIGN_SERVER
    struct is populated. A char pointer takes the return value of get_field
    for each column we're interested in obtaining, and if that pointer
    isn't 0x0, the FOREIGN_SERVER member is set to that value, otherwise,
    is set to the value of an empty string, since get_field would set it to
    0x0 if the column's value is empty, even if the default value for that
    column is NOT NULL.

  RETURN VALUES
    0	ok
    1	could not insert server struct into global servers cache
*/

static bool 
get_server_from_table_to_cache(TABLE *table)
{
  /* alloc a server struct */
  char *ptr;
  char * const blank= (char*)"";
  FOREIGN_SERVER *server= (FOREIGN_SERVER *)alloc_root(&mem,
                                                       sizeof(FOREIGN_SERVER));
  DBUG_ENTER("get_server_from_table_to_cache");
  table->use_all_columns();

  /* get each field into the server struct ptr */
  ptr= get_field(&mem, table->field[0]);
  server->server_name= ptr ? ptr : blank;
  server->server_name_length= (uint) strlen(server->server_name);
  ptr= get_field(&mem, table->field[1]);
  server->host= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[2]);
  server->db= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[3]);
  server->username= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[4]);
  server->password= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[5]);
  server->sport= ptr ? ptr : blank;

  server->port= server->sport ? atoi(server->sport) : 0;

  ptr= get_field(&mem, table->field[6]);
  server->socket= ptr && strlen(ptr) ? ptr : blank;
  ptr= get_field(&mem, table->field[7]);
  server->scheme= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[8]);
  server->owner= ptr ? ptr : blank;
  DBUG_PRINT("info", ("server->server_name %s", server->server_name));
  DBUG_PRINT("info", ("server->host %s", server->host));
  DBUG_PRINT("info", ("server->db %s", server->db));
  DBUG_PRINT("info", ("server->username %s", server->username));
  DBUG_PRINT("info", ("server->password %s", server->password));
  DBUG_PRINT("info", ("server->socket %s", server->socket));
  if (my_hash_insert(&servers_cache, (uchar*) server))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        server->server_name, (long unsigned int) server));
    // error handling needed here
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/**
   This function takes a server object that is has all members properly
   prepared, ready to be inserted both into the mysql.servers table and
   the servers cache.

   @param thd     thread pointer
   @param server  pointer to prepared FOREIGN_SERVER struct

   @note THR_LOCK_servers must be write locked.

   @retval false OK
   @retval true  Error
*/

static bool insert_server(THD *thd, FOREIGN_SERVER *server)
{
  DBUG_ENTER("insert_server");

  TABLE_LIST tables;
  tables.init_one_table("mysql", 5, "servers", 7, "servers", TL_WRITE);

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  TABLE *table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT);
  if (!table)
    DBUG_RETURN(true);

  /* insert the server into the table and the cache */
  bool error= (insert_server_record(table, server) ||
               insert_server_record_into_cache(server));

  close_mysql_tables(thd);

  DBUG_RETURN(error);
}


/**
   This function takes a FOREIGN_SERVER pointer to an allocated (root mem)
   and inserts it into the global servers cache

   @param server  pointer to prepared FOREIGN_SERVER struct

   @note THR_LOCK_servers must be write locked.

   @retval false OK
   @retval true  Error
*/

static bool insert_server_record_into_cache(FOREIGN_SERVER *server)
{
  DBUG_ENTER("insert_server_record_into_cache");
  /*
    We succeded in insertion of the server to the table, now insert
    the server to the cache
  */
  DBUG_PRINT("info", ("inserting server %s at %lx, length %d",
                        server->server_name, (long unsigned int) server,
                        server->server_name_length));
  if (my_hash_insert(&servers_cache, (uchar*) server))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        server->server_name, (long unsigned int) server));
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/*
  SYNOPSIS
    store_server_fields()
      TABLE *table
      FOREIGN_SERVER *server

  NOTES
    This function takes an opened table object, and a pointer to an 
    allocated FOREIGN_SERVER struct, and then stores each member of
    the FOREIGN_SERVER to the appropriate fields in the table, in 
    advance of insertion into the mysql.servers table

  RETURN VALUE
    VOID

*/

static void 
store_server_fields(TABLE *table, FOREIGN_SERVER *server)
{

  table->use_all_columns();
  /*
    "server" has already been prepped by prepare_server_struct_for_<>
    so, all we need to do is check if the value is set (> -1 for port)

    If this happens to be an update, only the server members that 
    have changed will be set. If an insert, then all will be set,
    even if with empty strings
  */
  if (server->host)
    table->field[1]->store(server->host,
                           (uint) strlen(server->host), system_charset_info);
  if (server->db)
    table->field[2]->store(server->db,
                           (uint) strlen(server->db), system_charset_info);
  if (server->username)
    table->field[3]->store(server->username,
                           (uint) strlen(server->username), system_charset_info);
  if (server->password)
    table->field[4]->store(server->password,
                           (uint) strlen(server->password), system_charset_info);
  if (server->port > -1)
    table->field[5]->store(server->port);

  if (server->socket)
    table->field[6]->store(server->socket,
                           (uint) strlen(server->socket), system_charset_info);
  if (server->scheme)
    table->field[7]->store(server->scheme,
                           (uint) strlen(server->scheme), system_charset_info);
  if (server->owner)
    table->field[8]->store(server->owner,
                           (uint) strlen(server->owner), system_charset_info);
}

/**
   This function takes the arguments of an open table object and a pointer
   to an allocated FOREIGN_SERVER struct. It stores the server_name into
   the first field of the table (the primary key, server_name column). With
   this, index_read_idx is called, if the record is found, an error is set
   to ER_FOREIGN_SERVER_EXISTS (the server with that server name exists in the
   table), if not, then store_server_fields stores all fields of the
   FOREIGN_SERVER to the table, then ha_write_row is inserted. If an error
   is encountered in either index_read_idx or ha_write_row, then that error
   is returned

   @param table   table for storing server record
   @param server  pointer to prepared FOREIGN_SERVER struct

   @retval false OK
   @retval true  Error
*/

static bool insert_server_record(TABLE *table, FOREIGN_SERVER *server)
{
  int error;
  DBUG_ENTER("insert_server_record");
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();

  empty_record(table);

  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server->server_name,
                         server->server_name_length,
                         system_charset_info);

  /* read index until record is that specified in server_name */
  error= table->file->ha_index_read_idx_map(table->record[0], 0,
                                            (uchar *)table->field[0]->ptr,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);
  if (error)
  {
    /* if not found, err */
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->file->print_error(error, MYF(0));

    /* store each field to be inserted */
    store_server_fields(table, server);

    DBUG_PRINT("info",("record for server '%s' not found!",
                       server->server_name));
    /* write/insert the new server */
    if ((error=table->file->ha_write_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }
  else
  {
    my_error(ER_FOREIGN_SERVER_EXISTS, MYF(0), server->server_name);
    error= 1;
  }

  reenable_binlog(table->in_use);
  DBUG_RETURN(error != 0);
}


/**
   This function takes as its arguments a THD object pointer and a pointer
   to a LEX_SERVER_OPTIONS struct from the parser. The member 'server_name'
   of this LEX_SERVER_OPTIONS struct contains the value of the server to be
   deleted. The mysql.servers table is opened via open_ltable, a table object
   returned, the servers cache mutex locked, then delete_server_record is
   called with this table object and LEX_SERVER_OPTIONS server_name and
   server_name_length passed, containing the name of the server to be
   dropped/deleted, then delete_server_record_in_cache is called to delete
   the server from the servers cache.

   @param thd              thread pointer
   @param server_options   LEX_SERVER_OPTIONS from parser
   @param if_exists        true if DROP IF EXISTS

   @retval false OK
   @retval true  Error
*/

bool drop_server(THD *thd, LEX_SERVER_OPTIONS *server_options, bool if_exists)
{
  DBUG_ENTER("drop_server");
  DBUG_PRINT("info", ("server name server->server_name %s",
                      server_options->server_name));

  TABLE_LIST tables;
  tables.init_one_table("mysql", 5, "servers", 7, "servers", TL_WRITE);

  mysql_rwlock_wrlock(&THR_LOCK_servers);

  TABLE *table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT);
  if (!table)
  {
    mysql_rwlock_unlock(&THR_LOCK_servers);
    DBUG_RETURN(true);
  }

  LEX_STRING name= { server_options->server_name,
                     server_options->server_name_length };

  bool error= (delete_server_record_in_cache(server_options, if_exists) ||
               delete_server_record(table, name.str, name.length, if_exists));

  /* close the servers table before we call closed_cached_connection_tables */
  close_mysql_tables(thd);

  if (close_cached_connection_tables(thd, &name))
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_UNKNOWN_ERROR, "Server connection in use");
  }

  mysql_rwlock_unlock(&THR_LOCK_servers);
  DBUG_RETURN(error || thd->killed);
}


/**
   This function's  argument is a LEX_SERVER_OPTIONS struct pointer. This
   function uses the "server_name" and "server_name_length" members of the
   lex->server_options to search for the server in the servers_cache. Upon
   returned the server (pointer to a FOREIGN_SERVER struct), it then deletes
   that server from the servers_cache hash.

   @param server_options   LEX_SERVER_OPTIONS from parser
   @param if_exists        true if DROP IF EXISTS

   @retval false OK
   @retval true  Error
*/

static bool delete_server_record_in_cache(LEX_SERVER_OPTIONS *server_options,
                                          bool if_exists)
{
  DBUG_ENTER("delete_server_record_in_cache");
  DBUG_PRINT("info",("trying to obtain server name %s length %d",
                     server_options->server_name,
                     server_options->server_name_length));

  FOREIGN_SERVER *server=
    (FOREIGN_SERVER *)my_hash_search(&servers_cache,
                                     (uchar*) server_options->server_name,
                                     server_options->server_name_length);
  if (!server)
  {
    DBUG_PRINT("info", ("server_name %s length %d not found!",
                        server_options->server_name,
                        server_options->server_name_length));
    if (!if_exists)
      my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0),
               server_options->server_name);
    DBUG_RETURN(true);
  }
  /*
    We succeded in deletion of the server to the table, now delete
    the server from the cache
  */
  DBUG_PRINT("info",("deleting server %s length %d",
                     server->server_name,
                     server->server_name_length));

  my_hash_delete(&servers_cache, (uchar*) server);

  DBUG_RETURN(false);
}


/**
   This function takes as arguments a THD object pointer, and two pointers,
   one pointing to the existing FOREIGN_SERVER struct "existing" (which is
   the current record as it is) and another pointer pointing to the
   FOREIGN_SERVER struct with the members containing the modified/altered
   values that need to be updated in both the mysql.servers table and the
   servers_cache. It opens a table, passes the table and the altered
   FOREIGN_SERVER pointer, which will be used to update the mysql.servers
   table for the particular server via the call to update_server_record,
   and in the servers_cache via update_server_record_in_cache.

   @param thd       thread pointer
   @param existing  pointer to FOREIGN_SERVER struct for current server values
   @param altered   pointer to FOREIGN_SERVER struct for modified server values

   @note THR_LOCK_servers must be write locked.

   @retval false OK
   @retval true  Error
*/

bool update_server(THD *thd, FOREIGN_SERVER *existing, FOREIGN_SERVER *altered)
{
  DBUG_ENTER("update_server");

  TABLE_LIST tables;
  tables.init_one_table("mysql", 5, "servers", 7, "servers", TL_WRITE);

  TABLE *table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT);
  if (!table)
    DBUG_RETURN(true);

  bool error= (update_server_record(table, altered) ||
               update_server_record_in_cache(existing, altered));

  /* Perform a reload so we don't have a 'hole' in our mem_root */
  servers_load(thd, &tables);

  close_mysql_tables(thd);

  DBUG_RETURN(error);
}


/**
   This function takes as an argument the FOREIGN_SERVER structi pointer
   for the existing server and the FOREIGN_SERVER struct populated with only
   the members which have been updated. It then "merges" the "altered" struct
   members to the existing server, the existing server then represents an
   updated server. Then, the existing record is deleted from the servers_cache
   HASH, then the updated record inserted, in essence replacing the old
   record.

   @param existing  pointer to FOREIGN_SERVER struct for current server values
   @param altered   pointer to FOREIGN_SERVER struct for modified server values

   @note THR_LOCK_servers must be write locked.

   @retval false OK
   @retval true  Error
*/

bool update_server_record_in_cache(FOREIGN_SERVER *existing,
                                   FOREIGN_SERVER *altered)
{
  DBUG_ENTER("update_server_record_in_cache");

  /*
    update the members that haven't been change in the altered server struct
    with the values of the existing server struct
  */
  merge_server_struct(existing, altered);

  /* delete the existing server struct from the server cache */
  my_hash_delete(&servers_cache, (uchar*)existing);

  /* Insert the altered server struct into the server cache */
  if (my_hash_insert(&servers_cache, (uchar*)altered))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        altered->server_name, (long unsigned int) altered));
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/*

  SYNOPSIS
    merge_server_struct()
      FOREIGN_SERVER *from
      FOREIGN_SERVER *to

  NOTES
    This function takes as its arguments two pointers each to an allocated
    FOREIGN_SERVER struct. The first FOREIGN_SERVER struct represents the struct
    that we will obtain values from (hence the name "from"), the second
    FOREIGN_SERVER struct represents which FOREIGN_SERVER struct we will be
    "copying" any members that have a value to (hence the name "to")

  RETURN VALUE
    VOID

*/

void merge_server_struct(FOREIGN_SERVER *from, FOREIGN_SERVER *to)
{
  DBUG_ENTER("merge_server_struct");
  if (!to->host)
    to->host= strdup_root(&mem, from->host);
  if (!to->db)
    to->db= strdup_root(&mem, from->db);
  if (!to->username)
    to->username= strdup_root(&mem, from->username);
  if (!to->password)
    to->password= strdup_root(&mem, from->password);
  if (to->port == -1)
    to->port= from->port;
  if (!to->socket && from->socket)
    to->socket= strdup_root(&mem, from->socket);
  if (!to->scheme && from->scheme)
    to->scheme= strdup_root(&mem, from->scheme);
  if (!to->owner)
    to->owner= strdup_root(&mem, from->owner);

  DBUG_VOID_RETURN;
}


/**
   This function takes as its arguments an open TABLE pointer, and a pointer
   to an allocated FOREIGN_SERVER structure representing an updated record
   which needs to be inserted. The primary key, server_name is stored to field
   0, then index_read_idx is called to read the index to that record, the
   record then being ready to be updated, if found. If not found an error is
   set and error message printed. If the record is found, store_record is
   called, then store_server_fields stores each field from the the members of
   the updated FOREIGN_SERVER struct.

   @param table   table for storing server record
   @param server  pointer to prepared FOREIGN_SERVER struct

   @retval false OK
   @retval true  Error
*/


static bool update_server_record(TABLE *table, FOREIGN_SERVER *server)
{
  int error;
  DBUG_ENTER("update_server_record");
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();
  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server->server_name,
                         server->server_name_length,
                         system_charset_info);

  error= table->file->ha_index_read_idx_map(table->record[0], 0,
                                            (uchar *)table->field[0]->ptr,
                                            ~(longlong)0,
                                            HA_READ_KEY_EXACT);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->file->print_error(error, MYF(0));
    DBUG_PRINT("info",("server not found!"));
    my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0), server->server_name);
  }
  else
  {
    /* ok, so we can update since the record exists in the table */
    store_record(table,record[1]);
    store_server_fields(table, server);
    if ((error=table->file->ha_update_row(table->record[1],
                                          table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
    {
      table->file->print_error(error, MYF(0));
      DBUG_PRINT("info",("problems with ha_update_row %d", error));
    }
    else
      error= 0;
  }

  reenable_binlog(table->in_use);
  DBUG_RETURN(error != 0);
}


/**
   @param table               table where to delete server record
   @param server_name         name of server info to delete
   @param server_name_length  length of name
   @param if_exists           true if DROP IF EXISTS

   @retval false OK
   @retval true  Error
*/

static bool delete_server_record(TABLE *table, char *server_name,
                                 size_t server_name_length, bool if_exists)
{
  int error;
  DBUG_ENTER("delete_server_record");
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();

  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server_name, server_name_length, system_charset_info);

  error= table->file->ha_index_read_idx_map(table->record[0], 0,
                                            (uchar *)table->field[0]->ptr,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->file->print_error(error, MYF(0));
    DBUG_PRINT("info",("server not found!"));
    if (!if_exists)
      my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0), server_name);
  }
  else
  {
    if ((error= table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }

  reenable_binlog(table->in_use);
  DBUG_RETURN(error != 0);
}


/**
   @param thd             thread pointer
   @param server_options  LEX_SERVER_OPTIONS from parser

   @retval false OK
   @retval true  Error
*/

bool create_server(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  bool error= true;
  FOREIGN_SERVER *server;

  DBUG_ENTER("create_server");
  DBUG_PRINT("info", ("server_options->server_name %s",
                      server_options->server_name));

  mysql_rwlock_wrlock(&THR_LOCK_servers);

  /* hit the memory first */
  if (my_hash_search(&servers_cache, (uchar*) server_options->server_name,
                     server_options->server_name_length))
  {
    my_error(ER_FOREIGN_SERVER_EXISTS, MYF(0), server_options->server_name);
    goto end;
  }

  server= prepare_server_struct_for_insert(server_options);
  if (!server)
  {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    goto end;
  }

  error= insert_server(thd, server);

end:
  mysql_rwlock_unlock(&THR_LOCK_servers);
  DBUG_RETURN(error || thd->killed);
}


/**
   @param thd             thread pointer
   @param server_options  LEX_SERVER_OPTIONS from parser

  @retval false OK
  @retval true  Error
*/

bool alter_server(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  bool error= true;
  FOREIGN_SERVER *altered, *existing;
  LEX_STRING name= { server_options->server_name, 
                     server_options->server_name_length };

  DBUG_ENTER("alter_server");
  DBUG_PRINT("info", ("server_options->server_name %s",
                      server_options->server_name));

  mysql_rwlock_wrlock(&THR_LOCK_servers);

  existing= (FOREIGN_SERVER *) my_hash_search(&servers_cache,
                                              (uchar*) name.str,
                                              name.length);
  if (!existing)
  {
    my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0), name.str);
    goto end;
  }

  altered= (FOREIGN_SERVER *)alloc_root(&mem, sizeof(FOREIGN_SERVER));

  prepare_server_struct_for_update(server_options, existing, altered);

  error= update_server(thd, existing, altered);

  if (close_cached_connection_tables(thd, &name))
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_UNKNOWN_ERROR, "Server connection in use");
  }

end:
  mysql_rwlock_unlock(&THR_LOCK_servers);
  DBUG_RETURN(error || thd->killed);
}


/*

  SYNOPSIS
    prepare_server_struct_for_insert()
      LEX_SERVER_OPTIONS *server_options

  NOTES
    As FOREIGN_SERVER members are allocated on mem_root, we do not need to
    free them in case of error.

  RETURN VALUE
    On success filled FOREIGN_SERVER, or NULL in case out of memory.

*/

static FOREIGN_SERVER *
prepare_server_struct_for_insert(LEX_SERVER_OPTIONS *server_options)
{
  char *unset_ptr= (char*)"";
  FOREIGN_SERVER *server;
  DBUG_ENTER("prepare_server_struct");

  if (!(server= (FOREIGN_SERVER *)alloc_root(&mem, sizeof(FOREIGN_SERVER))))
    DBUG_RETURN(NULL); /* purecov: inspected */

  /* these two MUST be set */
  if (!(server->server_name= strdup_root(&mem, server_options->server_name)))
    DBUG_RETURN(NULL); /* purecov: inspected */
  server->server_name_length= server_options->server_name_length;

  if (!(server->host= server_options->host ?
          strdup_root(&mem, server_options->host) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  if (!(server->db= server_options->db ?
          strdup_root(&mem, server_options->db) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  if (!(server->username= server_options->username ?
          strdup_root(&mem, server_options->username) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  if (!(server->password= server_options->password ?
          strdup_root(&mem, server_options->password) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  /* set to 0 if not specified */
  server->port= server_options->port > -1 ?
    server_options->port : 0;

  if (!(server->socket= server_options->socket ?
          strdup_root(&mem, server_options->socket) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  if (!(server->scheme= server_options->scheme ?
          strdup_root(&mem, server_options->scheme) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  if (!(server->owner= server_options->owner ?
          strdup_root(&mem, server_options->owner) : unset_ptr))
    DBUG_RETURN(NULL); /* purecov: inspected */

  DBUG_RETURN(server);
}

/*

  SYNOPSIS
    prepare_server_struct_for_update()
      LEX_SERVER_OPTIONS *server_options

  NOTES

  RETURN VALUE
    0 - no error

*/

static void
prepare_server_struct_for_update(LEX_SERVER_OPTIONS *server_options,
                                 FOREIGN_SERVER *existing,
                                 FOREIGN_SERVER *altered)
{
  DBUG_ENTER("prepare_server_struct_for_update");

  altered->server_name= strdup_root(&mem, server_options->server_name);
  altered->server_name_length= server_options->server_name_length;
  DBUG_PRINT("info", ("existing name %s altered name %s",
                      existing->server_name, altered->server_name));

  /*
    The logic here is this: is this value set AND is it different
    than the existing value?
  */
  altered->host=
    (server_options->host && (strcmp(server_options->host, existing->host))) ?
     strdup_root(&mem, server_options->host) : 0;

  altered->db=
      (server_options->db && (strcmp(server_options->db, existing->db))) ?
        strdup_root(&mem, server_options->db) : 0;

  altered->username=
      (server_options->username &&
      (strcmp(server_options->username, existing->username))) ?
        strdup_root(&mem, server_options->username) : 0;

  altered->password=
      (server_options->password &&
      (strcmp(server_options->password, existing->password))) ?
        strdup_root(&mem, server_options->password) : 0;

  /*
    port is initialised to -1, so if unset, it will be -1
  */
  altered->port= (server_options->port > -1 &&
                 server_options->port != existing->port) ?
    server_options->port : -1;

  altered->socket=
    (server_options->socket &&
    (strcmp(server_options->socket, existing->socket))) ?
      strdup_root(&mem, server_options->socket) : 0;

  altered->scheme=
    (server_options->scheme &&
    (strcmp(server_options->scheme, existing->scheme))) ?
      strdup_root(&mem, server_options->scheme) : 0;

  altered->owner=
    (server_options->owner &&
    (strcmp(server_options->owner, existing->owner))) ?
      strdup_root(&mem, server_options->owner) : 0;

  DBUG_VOID_RETURN;
}

/*

  SYNOPSIS
    servers_free()
      bool end

  NOTES

  RETURN VALUE
    void

*/

void servers_free(bool end)
{
  DBUG_ENTER("servers_free");
  if (!my_hash_inited(&servers_cache))
    DBUG_VOID_RETURN;
  if (!end)
  {
    free_root(&mem, MYF(MY_MARK_BLOCKS_FREE));
	my_hash_reset(&servers_cache);
    DBUG_VOID_RETURN;
  }
  mysql_rwlock_destroy(&THR_LOCK_servers);
  free_root(&mem,MYF(0));
  my_hash_free(&servers_cache);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS

  clone_server(MEM_ROOT *mem_root, FOREIGN_SERVER *orig, FOREIGN_SERVER *buff)

  Create a clone of FOREIGN_SERVER. If the supplied mem_root is of
  thd->mem_root then the copy is automatically disposed at end of statement.

  NOTES

  ARGS
   MEM_ROOT pointer (strings are copied into this mem root) 
   FOREIGN_SERVER pointer (made a copy of)
   FOREIGN_SERVER buffer (if not-NULL, this pointer is returned)

  RETURN VALUE
   FOREIGN_SEVER pointer (copy of one supplied FOREIGN_SERVER)
*/

static FOREIGN_SERVER *clone_server(MEM_ROOT *mem, const FOREIGN_SERVER *server,
                                    FOREIGN_SERVER *buffer)
{
  DBUG_ENTER("sql_server.cc:clone_server");

  if (!buffer)
    buffer= (FOREIGN_SERVER *) alloc_root(mem, sizeof(FOREIGN_SERVER));

  buffer->server_name= strmake_root(mem, server->server_name,
                                    server->server_name_length);
  buffer->port= server->port;
  buffer->server_name_length= server->server_name_length;
  
  /* TODO: We need to examine which of these can really be NULL */
  buffer->db= server->db ? strdup_root(mem, server->db) : NULL;
  buffer->scheme= server->scheme ? strdup_root(mem, server->scheme) : NULL;
  buffer->username= server->username? strdup_root(mem, server->username): NULL;
  buffer->password= server->password? strdup_root(mem, server->password): NULL;
  buffer->socket= server->socket ? strdup_root(mem, server->socket) : NULL;
  buffer->owner= server->owner ? strdup_root(mem, server->owner) : NULL;
  buffer->host= server->host ? strdup_root(mem, server->host) : NULL;

 DBUG_RETURN(buffer);
}


/*

  SYNOPSIS
    get_server_by_name()
      const char *server_name

  NOTES

  RETURN VALUE
   FOREIGN_SERVER *

*/

FOREIGN_SERVER *get_server_by_name(MEM_ROOT *mem, const char *server_name,
                                   FOREIGN_SERVER *buff)
{
  size_t server_name_length;
  FOREIGN_SERVER *server;
  DBUG_ENTER("get_server_by_name");
  DBUG_PRINT("info", ("server_name %s", server_name));

  server_name_length= strlen(server_name);

  if (! server_name || !strlen(server_name))
  {
    DBUG_PRINT("info", ("server_name not defined!"));
    DBUG_RETURN((FOREIGN_SERVER *)NULL);
  }

  DBUG_PRINT("info", ("locking servers_cache"));
  mysql_rwlock_rdlock(&THR_LOCK_servers);
  if (!(server= (FOREIGN_SERVER *) my_hash_search(&servers_cache,
                                                  (uchar*) server_name,
                                                  server_name_length)))
  {
    DBUG_PRINT("info", ("server_name %s length %u not found!",
                        server_name, (unsigned) server_name_length));
    server= (FOREIGN_SERVER *) NULL;
  }
  /* otherwise, make copy of server */
  else
    server= clone_server(mem, server, buff);

  DBUG_PRINT("info", ("unlocking servers_cache"));
  mysql_rwlock_unlock(&THR_LOCK_servers);
  DBUG_RETURN(server);

}
