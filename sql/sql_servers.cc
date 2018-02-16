/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_servers.h"
#include "sql_base.h"                           // close_mysql_tables
#include "records.h"          // init_read_record, end_read_record
#include "hash_filo.h"
#include <m_ctype.h>
#include <stdarg.h>
#include "log.h"
#include "auth_common.h"
#include "sql_parse.h"
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT
#include "transaction.h"      // trans_rollback_stmt, trans_commit_stmt
/*
  We only use 1 mutex to guard the data structures - THR_LOCK_servers.
  Read locked when only reading data and write-locked for all other access.
*/

static HASH servers_cache;
static MEM_ROOT mem;
static mysql_rwlock_t THR_LOCK_servers;

/**
   This enum describes the structure of the mysql.servers table.
*/
enum enum_servers_table_field
{
  SERVERS_FIELD_NAME= 0,
  SERVERS_FIELD_HOST,
  SERVERS_FIELD_DB,
  SERVERS_FIELD_USERNAME,
  SERVERS_FIELD_PASSWORD,
  SERVERS_FIELD_PORT,
  SERVERS_FIELD_SOCKET,
  SERVERS_FIELD_SCHEME,
  SERVERS_FIELD_OWNER
};

static bool get_server_from_table_to_cache(TABLE *table);

static uchar *servers_cache_get_key(FOREIGN_SERVER *server, size_t *length,
                                    my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= (uint) server->server_name_length;
  return (uchar*) server->server_name;
}

static PSI_memory_key key_memory_servers;

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_key key_rwlock_THR_LOCK_servers;

static PSI_rwlock_info all_servers_cache_rwlocks[]=
{
  { &key_rwlock_THR_LOCK_servers, "THR_LOCK_servers", PSI_FLAG_GLOBAL}
};

static PSI_memory_info all_servers_cache_memory[]=
{
  { &key_memory_servers, "servers_cache", PSI_FLAG_GLOBAL}
};

static void init_servers_cache_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= array_elements(all_servers_cache_rwlocks);
  mysql_rwlock_register(category, all_servers_cache_rwlocks, count);

  count= array_elements(all_servers_cache_memory);
  mysql_memory_register(category, all_servers_cache_memory, count);
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
                   (my_hash_get_key) servers_cache_get_key, 0, 0,
                   key_memory_servers))
  {
    return_val= TRUE; /* we failed, out of memory? */
    goto end;
  }

  /* Initialize the mem root for data */
  init_sql_alloc(key_memory_servers, &mem, ACL_ALLOC_BLOCK_SIZE, 0);

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

static bool servers_load(THD *thd, TABLE *table)
{
  READ_RECORD read_record_info;
  bool return_val= TRUE;
  DBUG_ENTER("servers_load");

  my_hash_reset(&servers_cache);
  free_root(&mem, MYF(0));
  init_sql_alloc(key_memory_servers, &mem, ACL_ALLOC_BLOCK_SIZE, 0);

  if (init_read_record(&read_record_info, thd, table,
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
  bool return_val= true;
  DBUG_ENTER("servers_reload");

  DBUG_PRINT("info", ("locking servers_cache"));
  mysql_rwlock_wrlock(&THR_LOCK_servers);

  tables[0].init_one_table("mysql", 5, "servers", 7, "servers", TL_READ);
  if (open_trans_system_tables_for_read(thd, tables))
  {
    /*
      Execution might have been interrupted; only print the error message
      if an error condition has been raised.
    */
    if (thd->get_stmt_da()->is_error())
      sql_print_error("Can't open and lock privilege tables: %s",
                      thd->get_stmt_da()->message_text());
    goto end;
  }

  if ((return_val= servers_load(thd, tables[0].table)))
  {					// Error. Revert to old list
    /* blast, for now, we have no servers, discuss later way to preserve */

    DBUG_PRINT("error",("Reverting to old privileges"));
    servers_free();
  }

  close_trans_system_tables(thd);
end:
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

static bool get_server_from_table_to_cache(TABLE *table)
{
  /* alloc a server struct */
  char *ptr;
  char * const blank= (char*)"";
  FOREIGN_SERVER *server= new (&mem) FOREIGN_SERVER();

  DBUG_ENTER("get_server_from_table_to_cache");
  table->use_all_columns();

  /* get each field into the server struct ptr */
  ptr= get_field(&mem, table->field[SERVERS_FIELD_NAME]);
  server->server_name= ptr ? ptr : blank;
  server->server_name_length= strlen(server->server_name);
  ptr= get_field(&mem, table->field[SERVERS_FIELD_HOST]);
  server->host= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_DB]);
  server->db= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_USERNAME]);
  server->username= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_PASSWORD]);
  server->password= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_PORT]);
  server->sport= ptr ? ptr : blank;

  server->port= server->sport ? atoi(server->sport) : 0;

  ptr= get_field(&mem, table->field[SERVERS_FIELD_SOCKET]);
  server->socket= ptr && strlen(ptr) ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_SCHEME]);
  server->scheme= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[SERVERS_FIELD_OWNER]);
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
  Close all tables which match specified connection string or
  if specified string is NULL, then any table with a connection string.
*/

static bool close_cached_connection_tables(THD *thd,
                                           const char *connection_string,
                                           size_t connection_length)
{
  uint idx;
  TABLE_LIST tmp, *tables= NULL;
  bool result= FALSE;
  DBUG_ENTER("close_cached_connection_tables");
  DBUG_ASSERT(thd);

  mysql_mutex_lock(&LOCK_open);

  for (idx= 0; idx < table_def_cache.records; idx++)
  {
    TABLE_SHARE *share= (TABLE_SHARE *) my_hash_element(&table_def_cache, idx);

    /*
      Skip table shares being opened to avoid comparison reading into
      uninitialized memory further below.

      Thus, in theory, there is a risk that shares are left in the
      cache that should really be closed (matching the submitted
      connection string), and this risk is already present since
      LOCK_open is unlocked before calling this function. However,
      this function is called as the final step of DROP/ALTER SERVER,
      so its goal is to flush all tables which were open before
      DROP/ALTER SERVER started. Thus, if a share gets opened after
      this function is called, the information about the server has
      already been updated, so the new table will use the new
      definition of the server.

      It might have been an issue, however if one thread started
      opening a federated table, read the old server definition into a
      share, and then a switch to another thread doing ALTER SERVER
      happened right before setting m_open_in_progress to false for
      the share. Because in this case ALTER SERVER would not flush
      the share opened by the first thread as it should have been. But
      luckily, server definitions affected by * SERVER statements are
      not read into TABLE_SHARE structures, but are read when we
      create the TABLE object in ha_federated::open().

      This means that ignoring shares that are in the process of being
      opened is safe, because such shares don't have TABLE objects
      associated with them yet.
    */
    if (share->m_open_in_progress)
      continue;

    /* Ignore if table is not open or does not have a connect_string */
    if (!share->connect_string.length || !share->ref_count)
      continue;

    /* Compare the connection string */
    if (connection_string &&
        (connection_length > share->connect_string.length ||
         (connection_length < share->connect_string.length &&
          (share->connect_string.str[connection_length] != '/' &&
           share->connect_string.str[connection_length] != '\\')) ||
         native_strncasecmp(connection_string, share->connect_string.str,
                     connection_length)))
      continue;

    /* close_cached_tables() only uses these elements */
    tmp.db= share->db.str;
    tmp.table_name= share->table_name.str;
    tmp.next_local= tables;

    tables= (TABLE_LIST *) memdup_root(thd->mem_root, (char*)&tmp,
                                       sizeof(TABLE_LIST));
  }
  mysql_mutex_unlock(&LOCK_open);

  if (tables)
    result= close_cached_tables(thd, tables, FALSE, LONG_TIMEOUT);

  DBUG_RETURN(result);
}


void Server_options::reset()
{
  m_server_name.str= NULL;
  m_server_name.length= 0;
  m_port= PORT_NOT_SET;
  m_host.str= NULL;
  m_host.length= 0;
  m_db.str= NULL;
  m_db.length= 0;
  m_username.str= NULL;
  m_db.length= 0;
  m_password.str= NULL;
  m_password.length= 0;
  m_scheme.str= NULL;
  m_scheme.length= 0;
  m_socket.str= NULL;
  m_socket.length= 0;
  m_owner.str= NULL;
  m_owner.length= 0;
}


bool Server_options::insert_into_cache() const
{
  char *unset_ptr= (char*)"";
  DBUG_ENTER("Server_options::insert_into_cache");

  FOREIGN_SERVER *server= new (&mem) FOREIGN_SERVER();
  if (!server)
    DBUG_RETURN(true);

  /* these two MUST be set */
  if (!(server->server_name= strdup_root(&mem, m_server_name.str)))
    DBUG_RETURN(true);
  server->server_name_length= m_server_name.length;

  if (!(server->host= m_host.str ? strdup_root(&mem, m_host.str) : unset_ptr))
    DBUG_RETURN(true);

  if (!(server->db= m_db.str ? strdup_root(&mem, m_db.str) : unset_ptr))
    DBUG_RETURN(true);

  if (!(server->username= m_username.str ?
        strdup_root(&mem, m_username.str) : unset_ptr))
    DBUG_RETURN(true);

  if (!(server->password= m_password.str ?
        strdup_root(&mem, m_password.str) : unset_ptr))
    DBUG_RETURN(true);

  /* set to 0 if not specified */
  server->port= m_port != PORT_NOT_SET ? m_port : 0;

  if (!(server->socket= m_socket.str ?
        strdup_root(&mem, m_socket.str) : unset_ptr))
    DBUG_RETURN(true);

  if (!(server->scheme= m_scheme.str ?
        strdup_root(&mem, m_scheme.str) : unset_ptr))
    DBUG_RETURN(true);

  if (!(server->owner= m_owner.str ?
        strdup_root(&mem, m_owner.str) : unset_ptr))
    DBUG_RETURN(true);

  DBUG_RETURN(my_hash_insert(&servers_cache, (uchar*) server));
}


bool Server_options::update_cache(FOREIGN_SERVER *existing) const
{
  DBUG_ENTER("Server_options::update_cache");

  /*
    Note: Since the name can't change, we don't need to set it.
    This also means we can just update the existing cache entry.
  */

  /*
    The logic here is this: is this value set AND is it different
    than the existing value?
  */
  if (m_host.str && strcmp(m_host.str, existing->host) &&
      !(existing->host= strdup_root(&mem, m_host.str)))
    DBUG_RETURN(true);

  if (m_db.str && strcmp(m_db.str, existing->db) &&
      !(existing->db= strdup_root(&mem, m_db.str)))
    DBUG_RETURN(true);

  if (m_username.str && strcmp(m_username.str, existing->username) &&
      !(existing->username= strdup_root(&mem, m_username.str)))
    DBUG_RETURN(true);

  if (m_password.str && strcmp(m_password.str, existing->password) &&
      !(existing->password= strdup_root(&mem, m_password.str)))
    DBUG_RETURN(true);

  /*
    port is initialised to PORT_NOT_SET, so if unset, it will be -1
  */
  if (m_port != PORT_NOT_SET && m_port != existing->port)
    existing->port= m_port;

  if (m_socket.str && strcmp(m_socket.str, existing->socket) &&
      !(existing->socket= strdup_root(&mem, m_socket.str)))
    DBUG_RETURN(true);

  if (m_scheme.str && strcmp(m_scheme.str, existing->scheme) &&
      !(existing->scheme= strdup_root(&mem, m_scheme.str)))
    DBUG_RETURN(true);

  if (m_owner.str && strcmp(m_owner.str, existing->owner) &&
      !(existing->owner= strdup_root(&mem, m_owner.str)))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/**
   Helper function for creating a record for inserting
   a new server into the mysql.servers table.

   Set a field to the given parser string. If the parser
   string is empty, set the field to "" instead.
*/

static inline void store_new_field(TABLE *table,
                                   enum_servers_table_field field,
                                   const LEX_STRING *val)
{
  if (val->str)
    table->field[field]->store(val->str, val->length,
                                  system_charset_info);
  else
    table->field[field]->store("", 0U, system_charset_info);
}


void Server_options::store_new_server(TABLE *table) const
{
  store_new_field(table, SERVERS_FIELD_HOST, &m_host);
  store_new_field(table, SERVERS_FIELD_DB, &m_db);
  store_new_field(table, SERVERS_FIELD_USERNAME, &m_username);
  store_new_field(table, SERVERS_FIELD_PASSWORD, &m_password);

  if (m_port != PORT_NOT_SET)
    table->field[SERVERS_FIELD_PORT]->store(m_port);
  else
    table->field[SERVERS_FIELD_PORT]->store(0);

  store_new_field(table, SERVERS_FIELD_SOCKET, &m_socket);
  store_new_field(table, SERVERS_FIELD_SCHEME, &m_scheme);
  store_new_field(table, SERVERS_FIELD_OWNER, &m_owner);
}


/**
   Helper function for creating a record for updating
   an existing server in the mysql.servers table.

   Set a field to the given parser string unless
   the parser string is empty or equal to the existing value.
*/

static inline void store_updated_field(TABLE *table,
                                       enum_servers_table_field field,
                                       const char *existing_val,
                                       const LEX_STRING *new_val)
{
  if (new_val->str && strcmp(new_val->str, existing_val))
    table->field[field]->store(new_val->str, new_val->length,
                               system_charset_info);
}


void Server_options::store_altered_server(TABLE *table,
                                          FOREIGN_SERVER *existing) const
{
  store_updated_field(table, SERVERS_FIELD_HOST, existing->host, &m_host);
  store_updated_field(table, SERVERS_FIELD_DB, existing->db, &m_db);
  store_updated_field(table, SERVERS_FIELD_USERNAME,
                      existing->username, &m_username);
  store_updated_field(table, SERVERS_FIELD_PASSWORD,
                      existing->password, &m_password);

  if (m_port != PORT_NOT_SET && m_port != existing->port)
    table->field[SERVERS_FIELD_PORT]->store(m_port);

  store_updated_field(table, SERVERS_FIELD_SOCKET, existing->socket, &m_socket);
  store_updated_field(table, SERVERS_FIELD_SCHEME, existing->scheme, &m_scheme);
  store_updated_field(table, SERVERS_FIELD_OWNER, existing->owner, &m_owner);
}


bool Sql_cmd_common_server::check_and_open_table(THD *thd)
{
  if (check_global_access(thd, SUPER_ACL))
    return true;

  TABLE_LIST tables;
  tables.init_one_table("mysql", 5, "servers", 7, "servers", TL_WRITE);

  table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT);
  return (table == NULL);
}


bool Sql_cmd_create_server::execute(THD *thd)
{
  DBUG_ENTER("Sql_cmd_create_server::execute");

  if (Sql_cmd_common_server::check_and_open_table(thd))
    DBUG_RETURN(true);

  // Check for existing cache entries with same name
  mysql_rwlock_wrlock(&THR_LOCK_servers);
  if (my_hash_search(&servers_cache,
                     (uchar*) m_server_options->m_server_name.str,
                     m_server_options->m_server_name.length))
  {
    mysql_rwlock_unlock(&THR_LOCK_servers);
    my_error(ER_FOREIGN_SERVER_EXISTS, MYF(0),
             m_server_options->m_server_name.str);
    trans_rollback_stmt(thd);
    close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  int error;
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();
  empty_record(table);

  /* set the field that's the PK to the value we're looking for */
  table->field[SERVERS_FIELD_NAME]->store(
    m_server_options->m_server_name.str,
    m_server_options->m_server_name.length,
    system_charset_info);

  /* read index until record is that specified in server_name */
  error= table->file->ha_index_read_idx_map(
    table->record[0], 0,
    table->field[SERVERS_FIELD_NAME]->ptr,
    HA_WHOLE_KEY,
    HA_READ_KEY_EXACT);

  if (!error)
  {
    my_error(ER_FOREIGN_SERVER_EXISTS, MYF(0),
             m_server_options->m_server_name.str);
    error= 1;
  }
  else if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
  {
    /* if not found, err */
    table->file->print_error(error, MYF(0));
  }
  else
  {
    /* store each field to be inserted */
    m_server_options->store_new_server(table);

    /* write/insert the new server */
    if ((error= table->file->ha_write_row(table->record[0])))
      table->file->print_error(error, MYF(0));
    else
    {
      /* insert the server into the cache */
      if ((error= m_server_options->insert_into_cache()))
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
    }
  }

  reenable_binlog(table->in_use);
  mysql_rwlock_unlock(&THR_LOCK_servers);

  if (error)
    trans_rollback_stmt(thd);
  else
    trans_commit_stmt(thd);
  close_mysql_tables(thd);

  if (error == 0 && !thd->killed)
    my_ok(thd, 1);
  DBUG_RETURN(error != 0 || thd->killed);
}


bool Sql_cmd_alter_server::execute(THD *thd)
{
  DBUG_ENTER("Sql_cmd_alter_server::execute");

  if (Sql_cmd_common_server::check_and_open_table(thd))
    DBUG_RETURN(true);

  // Find existing cache entry to update
  mysql_rwlock_wrlock(&THR_LOCK_servers);
  FOREIGN_SERVER *existing=
    (FOREIGN_SERVER *) my_hash_search(&servers_cache,
                                  (uchar*) m_server_options->m_server_name.str,
                                  m_server_options->m_server_name.length);
  if (!existing)
  {
    my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0),
             m_server_options->m_server_name.str);
    mysql_rwlock_unlock(&THR_LOCK_servers);
    trans_rollback_stmt(thd);
    close_mysql_tables(thd);
    DBUG_RETURN(true);
  }

  int error;
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();

  /* set the field that's the PK to the value we're looking for */
  table->field[SERVERS_FIELD_NAME]->store(
    m_server_options->m_server_name.str,
    m_server_options->m_server_name.length,
    system_charset_info);

  error= table->file->ha_index_read_idx_map(
    table->record[0], 0,
    table->field[SERVERS_FIELD_NAME]->ptr,
    ~(longlong)0,
    HA_READ_KEY_EXACT);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->file->print_error(error, MYF(0));
    else
      my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0),
               m_server_options->m_server_name.str);
  }
  else
  {
    /* ok, so we can update since the record exists in the table */
    store_record(table, record[1]);
    m_server_options->store_altered_server(table, existing);
    if ((error=table->file->ha_update_row(table->record[1],
                                          table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
      table->file->print_error(error, MYF(0));
    else
    {
      // Update cache entry
      if ((error= m_server_options->update_cache(existing)))
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
    }
  }

  reenable_binlog(table->in_use);

  /* Perform a reload so we don't have a 'hole' in our mem_root */
  servers_load(thd, table);

  // NOTE: servers_load() must be called under acquired THR_LOCK_servers.
  mysql_rwlock_unlock(&THR_LOCK_servers);

  if (error)
    trans_rollback_stmt(thd);
  else
    trans_commit_stmt(thd);
  close_mysql_tables(thd);

  if (close_cached_connection_tables(thd, m_server_options->m_server_name.str,
                                     m_server_options->m_server_name.length))
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_UNKNOWN_ERROR, "Server connection in use");
  }

  if (error == 0 && !thd->killed)
    my_ok(thd, 1);
  DBUG_RETURN(error != 0 || thd->killed);
}


bool Sql_cmd_drop_server::execute(THD *thd)
{
  DBUG_ENTER("Sql_cmd_drop_server::execute");

  if (Sql_cmd_common_server::check_and_open_table(thd))
    DBUG_RETURN(true);

  int error;
  mysql_rwlock_wrlock(&THR_LOCK_servers);
  tmp_disable_binlog(table->in_use);
  table->use_all_columns();

  /* set the field that's the PK to the value we're looking for */
  table->field[SERVERS_FIELD_NAME]->store(m_server_name.str,
                                          m_server_name.length,
                                          system_charset_info);

  error= table->file->ha_index_read_idx_map(
    table->record[0], 0,
    table->field[SERVERS_FIELD_NAME]->ptr,
    HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->file->print_error(error, MYF(0));
    else if (!m_if_exists)
      my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0), m_server_name.str);
    else
      error= 0; // Reset error - we will report my_ok() in this case.
  }
  else
  {
    // Delete from table
    if ((error= table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
    else
    {
      // Remove from cache
      FOREIGN_SERVER *server=
        (FOREIGN_SERVER *)my_hash_search(&servers_cache,
                                         (uchar*) m_server_name.str,
                                         m_server_name.length);
      if (server)
        my_hash_delete(&servers_cache, (uchar*) server);
      else if (!m_if_exists)
      {
        my_error(ER_FOREIGN_SERVER_DOESNT_EXIST, MYF(0),  m_server_name.str);
        error= 1;
      }
    }
  }

  reenable_binlog(table->in_use);
  mysql_rwlock_unlock(&THR_LOCK_servers);

  if (error)
    trans_rollback_stmt(thd);
  else
    trans_commit_stmt(thd);
  close_mysql_tables(thd);

  if (close_cached_connection_tables(thd, m_server_name.str,
                                     m_server_name.length))
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_UNKNOWN_ERROR, "Server connection in use");
  }

  if (error == 0 && !thd->killed)
    my_ok(thd, 1);
  DBUG_RETURN(error != 0 || thd->killed);
}


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
    buffer= new (mem) FOREIGN_SERVER();

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
