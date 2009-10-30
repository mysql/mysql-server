/*
Copyright (c) 2008, Patrick Galbraith 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Neither the name of Patrick Galbraith nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*

  FederatedX Pluggable Storage Engine

  ha_federatedx.cc - FederatedX Pluggable Storage Engine
  Patrick Galbraith, 2008

  This is a handler which uses a foreign database as the data file, as
  opposed to a handler like MyISAM, which uses .MYD files locally.

  How this handler works
  ----------------------------------
  Normal database files are local and as such: You create a table called
  'users', a file such as 'users.MYD' is created. A handler reads, inserts,
  deletes, updates data in this file. The data is stored in particular format,
  so to read, that data has to be parsed into fields, to write, fields have to
  be stored in this format to write to this data file.

  With FederatedX storage engine, there will be no local files
  for each table's data (such as .MYD). A foreign database will store
  the data that would normally be in this file. This will necessitate
  the use of MySQL client API to read, delete, update, insert this
  data. The data will have to be retrieve via an SQL call "SELECT *
  FROM users". Then, to read this data, it will have to be retrieved
  via mysql_fetch_row one row at a time, then converted from the
  column in this select into the format that the handler expects.

  The create table will simply create the .frm file, and within the
  "CREATE TABLE" SQL, there SHALL be any of the following :

  connection=scheme://username:password@hostname:port/database/tablename
  connection=scheme://username@hostname/database/tablename
  connection=scheme://username:password@hostname/database/tablename
  connection=scheme://username:password@hostname/database/tablename

  - OR -

  As of 5.1 federatedx now allows you to use a non-url
  format, taking advantage of mysql.servers:

  connection="connection_one"
  connection="connection_one/table_foo"

  An example would be:

  connection=mysql://username:password@hostname:port/database/tablename

  or, if we had:

  create server 'server_one' foreign data wrapper 'mysql' options
  (HOST '127.0.0.1',
  DATABASE 'db1',
  USER 'root',
  PASSWORD '',
  PORT 3306,
  SOCKET '',
  OWNER 'root');

  CREATE TABLE federatedx.t1 (
    `id` int(20) NOT NULL,
    `name` varchar(64) NOT NULL default ''
    )
  ENGINE="FEDERATEDX" DEFAULT CHARSET=latin1
  CONNECTION='server_one';

  So, this will have been the equivalent of

  CONNECTION="mysql://root@127.0.0.1:3306/db1/t1"

  Then, we can also change the server to point to a new schema:

  ALTER SERVER 'server_one' options(DATABASE 'db2');

  All subsequent calls will now be against db2.t1! Guess what? You don't
  have to perform an alter table!

  This connecton="connection string" is necessary for the handler to be
  able to connect to the foreign server, either by URL, or by server
  name. 


  The basic flow is this:

  SQL calls issues locally ->
  mysql handler API (data in handler format) ->
  mysql client API (data converted to SQL calls) ->
  foreign database -> mysql client API ->
  convert result sets (if any) to handler format ->
  handler API -> results or rows affected to local

  What this handler does and doesn't support
  ------------------------------------------
  * Tables MUST be created on the foreign server prior to any action on those
    tables via the handler, first version. IMPORTANT: IF you MUST use the
    federatedx storage engine type on the REMOTE end, MAKE SURE [ :) ] That
    the table you connect to IS NOT a table pointing BACK to your ORIGNAL
    table! You know  and have heard the screaching of audio feedback? You
    know putting two mirror in front of each other how the reflection
    continues for eternity? Well, need I say more?!
  * There will not be support for transactions.
  * There is no way for the handler to know if the foreign database or table
    has changed. The reason for this is that this database has to work like a
    data file that would never be written to by anything other than the
    database. The integrity of the data in the local table could be breached
    if there was any change to the foreign database.
  * Support for SELECT, INSERT, UPDATE , DELETE, indexes.
  * No ALTER TABLE, DROP TABLE or any other Data Definition Language calls.
  * Prepared statements will not be used in the first implementation, it
    remains to to be seen whether the limited subset of the client API for the
    server supports this.
  * This uses SELECT, INSERT, UPDATE, DELETE and not HANDLER for its
    implementation.
  * This will not work with the query cache.

   Method calls

   A two column table, with one record:

   (SELECT)

   "SELECT * FROM foo"
    ha_federatedx::info
    ha_federatedx::scan_time:
    ha_federatedx::rnd_init: share->select_query SELECT * FROM foo
    ha_federatedx::extra

    <for every row of data retrieved>
    ha_federatedx::rnd_next
    ha_federatedx::convert_row_to_internal_format
    ha_federatedx::rnd_next
    </for every row of data retrieved>

    ha_federatedx::rnd_end
    ha_federatedx::extra
    ha_federatedx::reset

    (INSERT)

    "INSERT INTO foo (id, ts) VALUES (2, now());"

    ha_federatedx::write_row

    ha_federatedx::reset

    (UPDATE)

    "UPDATE foo SET ts = now() WHERE id = 1;"

    ha_federatedx::index_init
    ha_federatedx::index_read
    ha_federatedx::index_read_idx
    ha_federatedx::rnd_next
    ha_federatedx::convert_row_to_internal_format
    ha_federatedx::update_row

    ha_federatedx::extra
    ha_federatedx::extra
    ha_federatedx::extra
    ha_federatedx::external_lock
    ha_federatedx::reset


    How do I use this handler?
    --------------------------

    <insert text about plugin storage engine>

    Next, to use this handler, it's very simple. You must
    have two databases running, either both on the same host, or
    on different hosts.

    One the server that will be connecting to the foreign
    host (client), you create your table as such:

    CREATE TABLE test_table (
      id     int(20) NOT NULL auto_increment,
      name   varchar(32) NOT NULL default '',
      other  int(20) NOT NULL default '0',
      PRIMARY KEY  (id),
      KEY name (name),
      KEY other_key (other))
       ENGINE="FEDERATEDX"
       DEFAULT CHARSET=latin1
       CONNECTION='mysql://root@127.0.0.1:9306/federatedx/test_federatedx';

   Notice the "COMMENT" and "ENGINE" field? This is where you
   respectively set the engine type, "FEDERATEDX" and foreign
   host information, this being the database your 'client' database
   will connect to and use as the "data file". Obviously, the foreign
   database is running on port 9306, so you want to start up your other
   database so that it is indeed on port 9306, and your federatedx
   database on a port other than that. In my setup, I use port 5554
   for federatedx, and port 5555 for the foreign database.

   Then, on the foreign database:

   CREATE TABLE test_table (
     id     int(20) NOT NULL auto_increment,
     name   varchar(32) NOT NULL default '',
     other  int(20) NOT NULL default '0',
     PRIMARY KEY  (id),
     KEY name (name),
     KEY other_key (other))
     ENGINE="<NAME>" <-- whatever you want, or not specify
     DEFAULT CHARSET=latin1 ;

    This table is exactly the same (and must be exactly the same),
    except that it is not using the federatedx handler and does
    not need the URL.


    How to see the handler in action
    --------------------------------

    When developing this handler, I compiled the federatedx database with
    debugging:

    ./configure --with-federatedx-storage-engine
    --prefix=/home/mysql/mysql-build/federatedx/ --with-debug

    Once compiled, I did a 'make install' (not for the purpose of installing
    the binary, but to install all the files the binary expects to see in the
    diretory I specified in the build with --prefix,
    "/home/mysql/mysql-build/federatedx".

    Then, I started the foreign server:

    /usr/local/mysql/bin/mysqld_safe
    --user=mysql --log=/tmp/mysqld.5555.log -P 5555

    Then, I went back to the directory containing the newly compiled mysqld,
    <builddir>/sql/, started up gdb:

    gdb ./mysqld

    Then, withn the (gdb) prompt:
    (gdb) run --gdb --port=5554 --socket=/tmp/mysqld.5554 --skip-innodb --debug

    Next, I open several windows for each:

    1. Tail the debug trace: tail -f /tmp/mysqld.trace|grep ha_fed
    2. Tail the SQL calls to the foreign database: tail -f /tmp/mysqld.5555.log
    3. A window with a client open to the federatedx server on port 5554
    4. A window with a client open to the federatedx server on port 5555

    I would create a table on the client to the foreign server on port
    5555, and then to the federatedx server on port 5554. At this point,
    I would run whatever queries I wanted to on the federatedx server,
    just always remembering that whatever changes I wanted to make on
    the table, or if I created new tables, that I would have to do that
    on the foreign server.

    Another thing to look for is 'show variables' to show you that you have
    support for federatedx handler support:

    show variables like '%federat%'

    and:

    show storage engines;

    Both should display the federatedx storage handler.


    Testing
    -------

    Testing for FederatedX as a pluggable storage engine for
    now is a manual process that I intend to build a test
    suite that works for all pluggable storage engines.

    How to test

    1. cp fed.dat /tmp
    (make sure you have access to "test". Use a user that has
    super privileges for now)
    2. mysql -f -u root test < federated.test > federated.myresult 2>&1
    3. diff federated.result federated.myresult (there _should_ be no differences)


*/


#define MYSQL_SERVER 1q
#include "mysql_priv.h"
#include <mysql/plugin.h>

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation                          // gcc: Class implementation
#endif

#include "ha_federatedx.h"

#include "m_string.h"

#include <mysql/plugin.h>

/* Variables for federatedx share methods */
static HASH federatedx_open_tables;              // To track open tables
static HASH federatedx_open_servers;             // To track open servers
pthread_mutex_t federatedx_mutex;                // To init the hash
const char ident_quote_char= '`';               // Character for quoting
                                                // identifiers
const char value_quote_char= '\'';              // Character for quoting
                                                // literals
static const int bulk_padding= 64;              // bytes "overhead" in packet

/* Variables used when chopping off trailing characters */
static const uint sizeof_trailing_comma= sizeof(", ") - 1;
static const uint sizeof_trailing_closeparen= sizeof(") ") - 1;
static const uint sizeof_trailing_and= sizeof(" AND ") - 1;
static const uint sizeof_trailing_where= sizeof(" WHERE ") - 1;

/* Static declaration for handerton */
static handler *federatedx_create_handler(handlerton *hton,
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root);

/* FederatedX storage engine handlerton */

static handler *federatedx_create_handler(handlerton *hton, 
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_federatedx(hton, table);
}


/* Function we use in the creation of our hash to get key */

static uchar *
federatedx_share_get_key(FEDERATEDX_SHARE *share, size_t *length,
                         my_bool not_used __attribute__ ((unused)))
{
  *length= share->share_key_length;
  return (uchar*) share->share_key;
}


static uchar *
federatedx_server_get_key(FEDERATEDX_SERVER *server, size_t *length,
                          my_bool not_used __attribute__ ((unused)))
{
  *length= server->key_length;
  return server->key;
}


/*
  Initialize the federatedx handler.

  SYNOPSIS
    federatedx_db_init()
    p		Handlerton

  RETURN
    FALSE       OK
    TRUE        Error
*/

int federatedx_db_init(void *p)
{
  DBUG_ENTER("federatedx_db_init");
  handlerton *federatedx_hton= (handlerton *)p;
  federatedx_hton->state= SHOW_OPTION_YES;
  /* This is no longer needed for plugin storage engines */
  federatedx_hton->db_type= DB_TYPE_DEFAULT;
  federatedx_hton->savepoint_offset= sizeof(ulong);
  federatedx_hton->close_connection= ha_federatedx::disconnect;
  federatedx_hton->savepoint_set= ha_federatedx::savepoint_set;
  federatedx_hton->savepoint_rollback= ha_federatedx::savepoint_rollback;
  federatedx_hton->savepoint_release= ha_federatedx::savepoint_release;
  federatedx_hton->commit= ha_federatedx::commit;
  federatedx_hton->rollback= ha_federatedx::rollback;
  federatedx_hton->create= federatedx_create_handler;
  federatedx_hton->flags= HTON_ALTER_NOT_SUPPORTED | HTON_NO_PARTITION;

  if (pthread_mutex_init(&federatedx_mutex, MY_MUTEX_INIT_FAST))
    goto error;
  if (!hash_init(&federatedx_open_tables, &my_charset_bin, 32, 0, 0,
                 (hash_get_key) federatedx_share_get_key, 0, 0) &&
      !hash_init(&federatedx_open_servers, &my_charset_bin, 32, 0, 0,
                 (hash_get_key) federatedx_server_get_key, 0, 0))
  {
    DBUG_RETURN(FALSE);
  }

  VOID(pthread_mutex_destroy(&federatedx_mutex));
error:
  DBUG_RETURN(TRUE);
}


/*
  Release the federatedx handler.

  SYNOPSIS
    federatedx_db_end()

  RETURN
    FALSE       OK
*/

int federatedx_done(void *p)
{
  hash_free(&federatedx_open_tables);
  hash_free(&federatedx_open_servers);
  VOID(pthread_mutex_destroy(&federatedx_mutex));

  return 0;
}

/**
  @brief Append identifiers to the string.

  @param[in,out] string	The target string.
  @param[in] name 		Identifier name
  @param[in] length 	Length of identifier name in bytes
  @param[in] quote_char Quote char to use for quoting identifier.

  @return Operation Status
  @retval FALSE OK
  @retval TRUE  There was an error appending to the string.

  @note This function is based upon the append_identifier() function
        in sql_show.cc except that quoting always occurs.
*/

bool append_ident(String *string, const char *name, uint length,
                  const char quote_char)
{
  bool result;
  uint clen;
  const char *name_end;
  DBUG_ENTER("append_ident");

  if (quote_char)
  {
    string->reserve(length * 2 + 2);
    if ((result= string->append(&quote_char, 1, system_charset_info)))
      goto err;

    for (name_end= name+length; name < name_end; name+= clen)
    {
      uchar c= *(uchar *) name;
      if (!(clen= my_mbcharlen(system_charset_info, c)))
        clen= 1;
      if (clen == 1 && c == (uchar) quote_char &&
          (result= string->append(&quote_char, 1, system_charset_info)))
        goto err;
      if ((result= string->append(name, clen, string->charset())))
        goto err;
    }
    result= string->append(&quote_char, 1, system_charset_info);
  }
  else
    result= string->append(name, length, system_charset_info);

err:
  DBUG_RETURN(result);
}


static int parse_url_error(FEDERATEDX_SHARE *share, TABLE *table, int error_num)
{
  char buf[FEDERATEDX_QUERY_BUFFER_SIZE];
  int buf_len;
  DBUG_ENTER("ha_federatedx parse_url_error");

  buf_len= min(table->s->connect_string.length,
               FEDERATEDX_QUERY_BUFFER_SIZE-1);
  strmake(buf, table->s->connect_string.str, buf_len);
  my_error(error_num, MYF(0), buf);
  DBUG_RETURN(error_num);
}

/*
  retrieve server object which contains server meta-data 
  from the system table given a server's name, set share
  connection parameter members
*/
int get_connection(MEM_ROOT *mem_root, FEDERATEDX_SHARE *share)
{
  int error_num= ER_FOREIGN_SERVER_DOESNT_EXIST;
  char error_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  FOREIGN_SERVER *server, server_buffer;
  DBUG_ENTER("ha_federatedx::get_connection");

  /*
    get_server_by_name() clones the server if exists and allocates
	copies of strings in the supplied mem_root
  */
  if (!(server=
       get_server_by_name(mem_root, share->connection_string, &server_buffer)))
  {
    DBUG_PRINT("info", ("get_server_by_name returned > 0 error condition!"));
    /* need to come up with error handling */
    error_num=1;
    goto error;
  }
  DBUG_PRINT("info", ("get_server_by_name returned server at %lx",
                      (long unsigned int) server));

  /*
    Most of these should never be empty strings, error handling will
    need to be implemented. Also, is this the best way to set the share
    members? Is there some allocation needed? In running this code, it works
    except there are errors in the trace file of the share being overrun 
    at the address of the share.
  */
  share->server_name_length= server->server_name_length;
  share->server_name= server->server_name;
  share->username= server->username;
  share->password= server->password;
  share->database= server->db;
#ifndef I_AM_PARANOID
  share->port= server->port > 0 && server->port < 65536 ? 
#else
  share->port= server->port > 1023 && server->port < 65536 ? 
#endif
               (ushort) server->port : MYSQL_PORT;
  share->hostname= server->host;
  if (!(share->socket= server->socket) &&
      !strcmp(share->hostname, my_localhost))
    share->socket= (char *) MYSQL_UNIX_ADDR;
  share->scheme= server->scheme;

  DBUG_PRINT("info", ("share->username: %s", share->username));
  DBUG_PRINT("info", ("share->password: %s", share->password));
  DBUG_PRINT("info", ("share->hostname: %s", share->hostname));
  DBUG_PRINT("info", ("share->database: %s", share->database));
  DBUG_PRINT("info", ("share->port:     %d", share->port));
  DBUG_PRINT("info", ("share->socket:   %s", share->socket));
  DBUG_RETURN(0);

error:
  my_sprintf(error_buffer,
             (error_buffer, "server name: '%s' doesn't exist!",
              share->connection_string));
  my_error(error_num, MYF(0), error_buffer);
  DBUG_RETURN(error_num);
}

/*
  Parse connection info from table->s->connect_string

  SYNOPSIS
    parse_url()
    mem_root            MEM_ROOT pointer for memory allocation
    share               pointer to FEDERATEDX share
    table               pointer to current TABLE class
    table_create_flag   determines what error to throw

  DESCRIPTION
    Populates the share with information about the connection
    to the foreign database that will serve as the data source.
    This string must be specified (currently) in the "CONNECTION" field,
    listed in the CREATE TABLE statement.

    This string MUST be in the format of any of these:

    CONNECTION="scheme://username:password@hostname:port/database/table"
    CONNECTION="scheme://username@hostname/database/table"
    CONNECTION="scheme://username@hostname:port/database/table"
    CONNECTION="scheme://username:password@hostname/database/table"

    _OR_

    CONNECTION="connection name"

    

  An Example:

  CREATE TABLE t1 (id int(32))
    ENGINE="FEDERATEDX"
    CONNECTION="mysql://joe:joespass@192.168.1.111:9308/federatedx/testtable";

  CREATE TABLE t2 (
    id int(4) NOT NULL auto_increment,
    name varchar(32) NOT NULL,
    PRIMARY KEY(id)
    ) ENGINE="FEDERATEDX" CONNECTION="my_conn";

  ***IMPORTANT***
  Currently, the FederatedX Storage Engine only supports connecting to another
  Database ("scheme" of "mysql"). Connections using JDBC as well as 
  other connectors are in the planning stage.
  

  'password' and 'port' are both optional.

  RETURN VALUE
    0           success
    error_num   particular error code 

*/

static int parse_url(MEM_ROOT *mem_root, FEDERATEDX_SHARE *share, TABLE *table,
                     uint table_create_flag)
{
  uint error_num= (table_create_flag ?
                   ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE :
                   ER_FOREIGN_DATA_STRING_INVALID);
  DBUG_ENTER("ha_federatedx::parse_url");

  share->port= 0;
  share->socket= 0;
  DBUG_PRINT("info", ("share at %lx", (long unsigned int) share));
  DBUG_PRINT("info", ("Length: %u", (uint) table->s->connect_string.length));
  DBUG_PRINT("info", ("String: '%.*s'", (int) table->s->connect_string.length,
                      table->s->connect_string.str));
  share->connection_string= strmake_root(mem_root, table->s->connect_string.str,
                                       table->s->connect_string.length);

  DBUG_PRINT("info",("parse_url alloced share->connection_string %lx",
                     (long unsigned int) share->connection_string));

  DBUG_PRINT("info",("share->connection_string: %s",share->connection_string));
  /*
    No :// or @ in connection string. Must be a straight connection name of
    either "servername" or "servername/tablename"
  */
  if ((!strstr(share->connection_string, "://") &&
       (!strchr(share->connection_string, '@'))))
  {

    DBUG_PRINT("info",
               ("share->connection_string: %s  internal format "
                "share->connection_string: %lx",
                share->connection_string,
                (ulong) share->connection_string));

    /* ok, so we do a little parsing, but not completely! */
    share->parsed= FALSE;
    /*
      If there is a single '/' in the connection string, this means the user is
      specifying a table name
    */

    if ((share->table_name= strchr(share->connection_string, '/')))
    {
      *share->table_name++= '\0';
      share->table_name_length= strlen(share->table_name);

      DBUG_PRINT("info", 
                 ("internal format, parsed table_name "
                  "share->connection_string: %s  share->table_name: %s",
                  share->connection_string, share->table_name));

      /*
        there better not be any more '/'s !
      */
      if (strchr(share->table_name, '/'))
        goto error;
    }
    /*
      Otherwise, straight server name, use tablename of federatedx table
      as remote table name
    */
    else
    {
      /*
        Connection specifies everything but, resort to
        expecting remote and foreign table names to match
      */
      share->table_name= strmake_root(mem_root, table->s->table_name.str,
                                      (share->table_name_length=
                                       table->s->table_name.length));
      DBUG_PRINT("info", 
                 ("internal format, default table_name "
                  "share->connection_string: %s  share->table_name: %s",
                  share->connection_string, share->table_name));
    }

    if ((error_num= get_connection(mem_root, share)))
      goto error;
  }
  else
  {
    share->parsed= TRUE;
    // Add a null for later termination of table name
    share->connection_string[table->s->connect_string.length]= 0;
    share->scheme= share->connection_string;
    DBUG_PRINT("info",("parse_url alloced share->scheme: %lx",
                       (ulong) share->scheme));

    /*
      Remove addition of null terminator and store length
      for each string  in share
    */
    if (!(share->username= strstr(share->scheme, "://")))
      goto error;
    share->scheme[share->username - share->scheme]= '\0';

    if (!federatedx_io::handles_scheme(share->scheme))
      goto error;

    share->username+= 3;

    if (!(share->hostname= strchr(share->username, '@')))
      goto error;
    *share->hostname++= '\0';                   // End username

    if ((share->password= strchr(share->username, ':')))
    {
      *share->password++= '\0';                 // End username

      /* make sure there isn't an extra / or @ */
      if ((strchr(share->password, '/') || strchr(share->hostname, '@')))
        goto error;
      /*
        Found that if the string is:
        user:@hostname:port/db/table
        Then password is a null string, so set to NULL
      */
      if ((share->password[0] == '\0'))
        share->password= NULL;
    }

    /* make sure there isn't an extra / or @ */
    if ((strchr(share->username, '/')) || (strchr(share->hostname, '@')))
      goto error;

    if (!(share->database= strchr(share->hostname, '/')))
      goto error;
    *share->database++= '\0';

    if ((share->sport= strchr(share->hostname, ':')))
    {
      *share->sport++= '\0';
      if (share->sport[0] == '\0')
        share->sport= NULL;
      else
        share->port= atoi(share->sport);
    }

    if (!(share->table_name= strchr(share->database, '/')))
      goto error;
    *share->table_name++= '\0';

    share->table_name_length= strlen(share->table_name);

    /* make sure there's not an extra / */
    if ((strchr(share->table_name, '/')))
      goto error;

    if (share->hostname[0] == '\0')
      share->hostname= NULL;

  }
  if (!share->port)
  {
    if (!share->hostname || strcmp(share->hostname, my_localhost) == 0)
      share->socket= (char *) MYSQL_UNIX_ADDR;
    else
      share->port= MYSQL_PORT;
  }

  DBUG_PRINT("info",
             ("scheme: %s  username: %s  password: %s  hostname: %s  "
              "port: %d  db: %s  tablename: %s",
              share->scheme, share->username, share->password,
              share->hostname, share->port, share->database,
              share->table_name));

  DBUG_RETURN(0);

error:
  DBUG_RETURN(parse_url_error(share, table, error_num));
}

/*****************************************************************************
** FEDERATEDX tables
*****************************************************************************/

ha_federatedx::ha_federatedx(handlerton *hton,
                           TABLE_SHARE *table_arg)
  :handler(hton, table_arg),
   txn(0), io(0), stored_result(0)
{
  bzero(&bulk_insert, sizeof(bulk_insert));
}


/*
  Convert MySQL result set row to handler internal format

  SYNOPSIS
    convert_row_to_internal_format()
      record    Byte pointer to record
      row       MySQL result set row from fetchrow()
      result	Result set to use

  DESCRIPTION
    This method simply iterates through a row returned via fetchrow with
    values from a successful SELECT , and then stores each column's value
    in the field object via the field object pointer (pointing to the table's
    array of field object pointers). This is how the handler needs the data
    to be stored to then return results back to the user

  RETURN VALUE
    0   After fields have had field values stored from record
*/

uint ha_federatedx::convert_row_to_internal_format(uchar *record,
                                                  FEDERATEDX_IO_ROW *row,
                                                  FEDERATEDX_IO_RESULT *result)
{
  ulong *lengths;
  Field **field;
  int column= 0;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_ENTER("ha_federatedx::convert_row_to_internal_format");

  lengths= io->fetch_lengths(result);

  for (field= table->field; *field; field++, column++)
  {
    /*
      index variable to move us through the row at the
      same iterative step as the field
    */
    my_ptrdiff_t old_ptr;
    old_ptr= (my_ptrdiff_t) (record - table->record[0]);
    (*field)->move_field_offset(old_ptr);
    if (io->is_column_null(row, column))
      (*field)->set_null();
    else
    {
      if (bitmap_is_set(table->read_set, (*field)->field_index))
      {
        (*field)->set_notnull();
        (*field)->store(io->get_column_data(row, column), lengths[column], &my_charset_bin);
      }
    }
    (*field)->move_field_offset(-old_ptr);
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(0);
}

static bool emit_key_part_name(String *to, KEY_PART_INFO *part)
{
  DBUG_ENTER("emit_key_part_name");
  if (append_ident(to, part->field->field_name, 
                   strlen(part->field->field_name), ident_quote_char))
    DBUG_RETURN(1);                           // Out of memory
  DBUG_RETURN(0);
}

static bool emit_key_part_element(String *to, KEY_PART_INFO *part,
                                  bool needs_quotes, bool is_like,
                                  const uchar *ptr, uint len)
{
  Field *field= part->field;
  DBUG_ENTER("emit_key_part_element");

  if (needs_quotes && to->append(STRING_WITH_LEN("'")))
    DBUG_RETURN(1);

  if (part->type == HA_KEYTYPE_BIT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE], *buf= buff;

    *buf++= '0';
    *buf++= 'x';
    buf= octet2hex(buf, (char*) ptr, len);
    if (to->append((char*) buff, (uint)(buf - buff)))
      DBUG_RETURN(1);
  }
  else if (part->key_part_flag & HA_BLOB_PART)
  {
    String blob;
    uint blob_length= uint2korr(ptr);
    blob.set_quick((char*) ptr+HA_KEY_BLOB_LENGTH,
                   blob_length, &my_charset_bin);
    if (append_escaped(to, &blob))
      DBUG_RETURN(1);
  }
  else if (part->key_part_flag & HA_VAR_LENGTH_PART)
  {
    String varchar;
    uint var_length= uint2korr(ptr);
    varchar.set_quick((char*) ptr+HA_KEY_BLOB_LENGTH,
                      var_length, &my_charset_bin);
    if (append_escaped(to, &varchar))
      DBUG_RETURN(1);
  }
  else
  {
    char strbuff[MAX_FIELD_WIDTH];
    String str(strbuff, sizeof(strbuff), part->field->charset()), *res;

    res= field->val_str(&str, ptr);

    if (field->result_type() == STRING_RESULT)
    {
      if (append_escaped(to, res))
        DBUG_RETURN(1);
    }
    else if (to->append(res->ptr(), res->length()))
      DBUG_RETURN(1);
  }

  if (is_like && to->append(STRING_WITH_LEN("%")))
    DBUG_RETURN(1);

  if (needs_quotes && to->append(STRING_WITH_LEN("'")))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

/*
  Create a WHERE clause based off of values in keys
  Note: This code was inspired by key_copy from key.cc

  SYNOPSIS
    create_where_from_key ()
      to          String object to store WHERE clause
      key_info    KEY struct pointer
      key         byte pointer containing key
      key_length  length of key
      range_type  0 - no range, 1 - min range, 2 - max range
                  (see enum range_operation)

  DESCRIPTION
    Using iteration through all the keys via a KEY_PART_INFO pointer,
    This method 'extracts' the value of each key in the byte pointer
    *key, and for each key found, constructs an appropriate WHERE clause

  RETURN VALUE
    0   After all keys have been accounted for to create the WHERE clause
    1   No keys found

    Range flags Table per Timour:

   -----------------
   - start_key:
     * ">"  -> HA_READ_AFTER_KEY
     * ">=" -> HA_READ_KEY_OR_NEXT
     * "="  -> HA_READ_KEY_EXACT

   - end_key:
     * "<"  -> HA_READ_BEFORE_KEY
     * "<=" -> HA_READ_AFTER_KEY

   records_in_range:
   -----------------
   - start_key:
     * ">"  -> HA_READ_AFTER_KEY
     * ">=" -> HA_READ_KEY_EXACT
     * "="  -> HA_READ_KEY_EXACT

   - end_key:
     * "<"  -> HA_READ_BEFORE_KEY
     * "<=" -> HA_READ_AFTER_KEY
     * "="  -> HA_READ_AFTER_KEY

0 HA_READ_KEY_EXACT,              Find first record else error
1 HA_READ_KEY_OR_NEXT,            Record or next record
2 HA_READ_KEY_OR_PREV,            Record or previous
3 HA_READ_AFTER_KEY,              Find next rec. after key-record
4 HA_READ_BEFORE_KEY,             Find next rec. before key-record
5 HA_READ_PREFIX,                 Key which as same prefix
6 HA_READ_PREFIX_LAST,            Last key with the same prefix
7 HA_READ_PREFIX_LAST_OR_PREV,    Last or prev key with the same prefix

Flags that I've found:

id, primary key, varchar

id = 'ccccc'
records_in_range: start_key 0 end_key 3
read_range_first: start_key 0 end_key NULL

id > 'ccccc'
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id < 'ccccc'
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 'ccccc'
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id >= 'ccccc'
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id like 'cc%cc'
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 'aaaaa' and id < 'ccccc'
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 'aaaaa' and id < 'ccccc';
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 'aaaaa' and id <= 'ccccc';
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 'aaaaa' and id <= 'ccccc';
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

numeric keys:

id = 4
index_read_idx: start_key 0 end_key NULL 

id > 4
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id >= 4
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id < 4
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 4
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id like 4
full table scan, select * from

id > 2 and id < 8
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 2 and id < 8
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 2 and id <= 8
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 2 and id <= 8
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

multi keys (id int, name varchar, other varchar)

id = 1;
records_in_range: start_key 0 end_key 3
read_range_first: start_key 0 end_key NULL

id > 4;
id > 2 and name = '333'; remote: id > 2
id > 2 and name > '333'; remote: id > 2
id > 2 and name > '333' and other < 'ddd'; remote: id > 2 no results
id > 2 and name >= '333' and other < 'ddd'; remote: id > 2 1 result
id >= 4 and name = 'eric was here' and other > 'eeee';
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id >= 4;
id >= 2 and name = '333' and other < 'ddd';
remote: `id`  >= 2 AND `name`  >= '333';
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id < 4;
id < 3 and name = '222' and other <= 'ccc'; remote: id < 3
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 4;
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id like 4;
full table scan

id  > 2 and id < 4;
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 2 and id < 4;
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 2 and id <= 4;
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 2 and id <= 4;
id = 6 and name = 'eric was here' and other > 'eeee';
remote: (`id`  > 6 AND `name`  > 'eric was here' AND `other`  > 'eeee')
AND (`id`  <= 6) AND ( AND `name`  <= 'eric was here')
no results
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

Summary:

* If the start key flag is 0 the max key flag shouldn't even be set, 
  and if it is, the query produced would be invalid.
* Multipart keys, even if containing some or all numeric columns,
  are treated the same as non-numeric keys

  If the query is " = " (quotes or not):
  - records in range start key flag HA_READ_KEY_EXACT,
    end key flag HA_READ_AFTER_KEY (incorrect)
  - any other: start key flag HA_READ_KEY_OR_NEXT,
    end key flag HA_READ_AFTER_KEY (correct)

* 'like' queries (of key)
  - Numeric, full table scan
  - Non-numeric
      records_in_range: start_key 0 end_key 3
      other : start_key 1 end_key 3

* If the key flag is HA_READ_AFTER_KEY:
   if start_key, append >
   if end_key, append <=

* If create_where_key was called by records_in_range:

 - if the key is numeric:
    start key flag is 0 when end key is NULL, end key flag is 3 or 4
 - if create_where_key was called by any other function:
    start key flag is 1 when end key is NULL, end key flag is 3 or 4
 - if the key is non-numeric, or multipart
    When the query is an exact match, the start key flag is 0,
    end key flag is 3 for what should be a no-range condition where
    you should have 0 and max key NULL, which it is if called by
    read_range_first

Conclusion:

1. Need logic to determin if a key is min or max when the flag is
HA_READ_AFTER_KEY, and handle appending correct operator accordingly

2. Need a boolean flag to pass to create_where_from_key, used in the
switch statement. Add 1 to the flag if:
  - start key flag is HA_READ_KEY_EXACT and the end key is NULL

*/

bool ha_federatedx::create_where_from_key(String *to,
                                         KEY *key_info,
                                         const key_range *start_key,
                                         const key_range *end_key,
                                         bool from_records_in_range,
                                         bool eq_range)
{
  bool both_not_null=
    (start_key != NULL && end_key != NULL) ? TRUE : FALSE;
  const uchar *ptr;
  uint remainder, length;
  char tmpbuff[FEDERATEDX_QUERY_BUFFER_SIZE];
  String tmp(tmpbuff, sizeof(tmpbuff), system_charset_info);
  const key_range *ranges[2]= { start_key, end_key };
  my_bitmap_map *old_map;
  DBUG_ENTER("ha_federatedx::create_where_from_key");

  tmp.length(0); 
  if (start_key == NULL && end_key == NULL)
    DBUG_RETURN(1);

  old_map= dbug_tmp_use_all_columns(table, table->write_set);
  for (uint i= 0; i <= 1; i++)
  {
    bool needs_quotes;
    KEY_PART_INFO *key_part;
    if (ranges[i] == NULL)
      continue;

    if (both_not_null)
    {
      if (i > 0)
        tmp.append(STRING_WITH_LEN(") AND ("));
      else
        tmp.append(STRING_WITH_LEN(" ("));
    }

    for (key_part= key_info->key_part,
         remainder= key_info->key_parts,
         length= ranges[i]->length,
         ptr= ranges[i]->key; ;
         remainder--,
         key_part++)
    {
      Field *field= key_part->field;
      uint store_length= key_part->store_length;
      uint part_length= min(store_length, length);
      needs_quotes= field->str_needs_quotes();
      DBUG_DUMP("key, start of loop", ptr, length);

      if (key_part->null_bit)
      {
        if (*ptr++)
        {
          /*
            We got "IS [NOT] NULL" condition against nullable column. We
            distinguish between "IS NOT NULL" and "IS NULL" by flag. For
            "IS NULL", flag is set to HA_READ_KEY_EXACT.
          */
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(ranges[i]->flag == HA_READ_KEY_EXACT ?
                         " IS NULL " : " IS NOT NULL "))
            goto err;
          /*
            We need to adjust pointer and length to be prepared for next
            key part. As well as check if this was last key part.
          */
          goto prepare_for_next_key_part;
        }
      }

      if (tmp.append(STRING_WITH_LEN(" (")))
        goto err;

      switch (ranges[i]->flag) {
      case HA_READ_KEY_EXACT:
        DBUG_PRINT("info", ("federatedx HA_READ_KEY_EXACT %d", i));
        if (store_length >= length ||
            !needs_quotes ||
            key_part->type == HA_KEYTYPE_BIT ||
            field->result_type() != STRING_RESULT)
        {
          if (emit_key_part_name(&tmp, key_part))
            goto err;

          if (from_records_in_range)
          {
            if (tmp.append(STRING_WITH_LEN(" >= ")))
              goto err;
          }
          else
          {
            if (tmp.append(STRING_WITH_LEN(" = ")))
              goto err;
          }

          if (emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
            goto err;
        }
        else
        {
          /* LIKE */
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" LIKE ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, 1, ptr,
                                    part_length))
            goto err;
        }
        break;
      case HA_READ_AFTER_KEY:
        if (eq_range)
        {
          if (tmp.append("1=1"))                // Dummy
            goto err;
          break;
        }
        DBUG_PRINT("info", ("federatedx HA_READ_AFTER_KEY %d", i));
        if (store_length >= length) /* end key */
        {
          if (emit_key_part_name(&tmp, key_part))
            goto err;

          if (i > 0) /* end key */
          {
            if (tmp.append(STRING_WITH_LEN(" <= ")))
              goto err;
          }
          else /* start key */
          {
            if (tmp.append(STRING_WITH_LEN(" > ")))
              goto err;
          }

          if (emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
          {
            goto err;
          }
          break;
        }
      case HA_READ_KEY_OR_NEXT:
        DBUG_PRINT("info", ("federatedx HA_READ_KEY_OR_NEXT %d", i));
        if (emit_key_part_name(&tmp, key_part) ||
            tmp.append(STRING_WITH_LEN(" >= ")) ||
            emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
              part_length))
          goto err;
        break;
      case HA_READ_BEFORE_KEY:
        DBUG_PRINT("info", ("federatedx HA_READ_BEFORE_KEY %d", i));
        if (store_length >= length)
        {
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" < ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
            goto err;
          break;
        }
      case HA_READ_KEY_OR_PREV:
        DBUG_PRINT("info", ("federatedx HA_READ_KEY_OR_PREV %d", i));
        if (emit_key_part_name(&tmp, key_part) ||
            tmp.append(STRING_WITH_LEN(" <= ")) ||
            emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                  part_length))
          goto err;
        break;
      default:
        DBUG_PRINT("info",("cannot handle flag %d", ranges[i]->flag));
        goto err;
      }
      if (tmp.append(STRING_WITH_LEN(") ")))
        goto err;

prepare_for_next_key_part:
      if (store_length >= length)
        break;
      DBUG_PRINT("info", ("remainder %d", remainder));
      DBUG_ASSERT(remainder > 1);
      length-= store_length;
      /*
        For nullable columns, null-byte is already skipped before, that is
        ptr was incremented by 1. Since store_length still counts null-byte,
        we need to subtract 1 from store_length.
      */
      ptr+= store_length - test(key_part->null_bit);
      if (tmp.append(STRING_WITH_LEN(" AND ")))
        goto err;

      DBUG_PRINT("info",
                 ("create_where_from_key WHERE clause: %s",
                  tmp.c_ptr_quick()));
    }
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);

  if (both_not_null)
    if (tmp.append(STRING_WITH_LEN(") ")))
      DBUG_RETURN(1);

  if (to->append(STRING_WITH_LEN(" WHERE ")))
    DBUG_RETURN(1);

  if (to->append(tmp))
    DBUG_RETURN(1);

  DBUG_RETURN(0);

err:
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(1);
}

static void fill_server(MEM_ROOT *mem_root, FEDERATEDX_SERVER *server,
                        FEDERATEDX_SHARE *share, CHARSET_INFO *table_charset)
{
  char buffer[STRING_BUFFER_USUAL_SIZE];
  String key(buffer, sizeof(buffer), &my_charset_bin);  
  String scheme(share->scheme, &my_charset_latin1);
  String hostname(share->hostname, &my_charset_latin1);
  String database(share->database, system_charset_info);
  String username(share->username, system_charset_info);
  String socket(share->socket ? share->socket : "", files_charset_info);
  String password(share->password ? share->password : "", &my_charset_bin);
  DBUG_ENTER("fill_server");

  /* Do some case conversions */
  scheme.reserve(scheme.length());
  scheme.length(my_casedn_str(&my_charset_latin1, scheme.c_ptr_safe()));
  
  hostname.reserve(hostname.length());
  hostname.length(my_casedn_str(&my_charset_latin1, hostname.c_ptr_safe()));
  
  if (lower_case_table_names)
  {
    database.reserve(database.length());
    database.length(my_casedn_str(system_charset_info, database.c_ptr_safe()));
  }
  
  if (lower_case_file_system && socket.length())
  {
    socket.reserve(socket.length());
    socket.length(my_casedn_str(files_charset_info, socket.c_ptr_safe()));
  }

  /* start with all bytes zeroed */  
  bzero(server, sizeof(*server));

  key.length(0);
  key.reserve(scheme.length() + hostname.length() + database.length() +
              socket.length() + username.length() + password.length() +
       sizeof(int) + 8);
  key.append(scheme);
  key.q_append('\0');
  server->hostname= (const char *) (intptr) key.length();
  key.append(hostname);
  key.q_append('\0');
  server->database= (const char *) (intptr) key.length();
  key.append(database);
  key.q_append('\0');
  key.q_append((uint32) share->port);
  server->socket= (const char *) (intptr) key.length();
  key.append(socket);
  key.q_append('\0');
  server->username= (const char *) (intptr) key.length();
  key.append(username);
  key.q_append('\0');
  server->password= (const char *) (intptr) key.length();
  key.append(password);
  
  server->key_length= key.length();
  server->key= (uchar *)  memdup_root(mem_root, key.ptr(), key.length()+1);

  /* pointer magic */
  server->scheme+= (intptr) server->key;
  server->hostname+= (intptr) server->key;
  server->database+= (intptr) server->key;
  server->username+= (intptr) server->key;
  server->password+= (intptr) server->key;
  server->socket+= (intptr) server->key;
  server->port= share->port;

  if (!share->socket)
    server->socket= NULL;
  if (!share->password)
    server->password= NULL;

  if (table_charset)
    server->csname= strdup_root(mem_root, table_charset->csname);

  DBUG_VOID_RETURN;
}


static FEDERATEDX_SERVER *get_server(FEDERATEDX_SHARE *share, TABLE *table)
{
  FEDERATEDX_SERVER *server= NULL, tmp_server;
  MEM_ROOT mem_root;
  char buffer[STRING_BUFFER_USUAL_SIZE];
  String key(buffer, sizeof(buffer), &my_charset_bin);  
  String scheme(share->scheme, &my_charset_latin1);
  String hostname(share->hostname, &my_charset_latin1);
  String database(share->database, system_charset_info);
  String username(share->username, system_charset_info);
  String socket(share->socket ? share->socket : "", files_charset_info);
  String password(share->password ? share->password : "", &my_charset_bin);
  DBUG_ENTER("ha_federated.cc::get_server");

  safe_mutex_assert_owner(&federatedx_mutex);

  init_alloc_root(&mem_root, 4096, 4096);

  fill_server(&mem_root, &tmp_server, share, table ? table->s->table_charset : 0);

  if (!(server= (FEDERATEDX_SERVER *) hash_search(&federatedx_open_servers,
                                                 tmp_server.key,
                                                 tmp_server.key_length)))
  {
    if (!table || !tmp_server.csname)
      goto error;
 
    if (!(server= (FEDERATEDX_SERVER *) memdup_root(&mem_root, 
                          (char *) &tmp_server,
                          sizeof(*server))))
      goto error;

    server->mem_root= mem_root;

    if (my_hash_insert(&federatedx_open_servers, (uchar*) server))
      goto error;

    pthread_mutex_init(&server->mutex, MY_MUTEX_INIT_FAST);
  }
  else
    free_root(&mem_root, MYF(0)); /* prevents memory leak */

  server->use_count++;
  
  DBUG_RETURN(server);
error:
  free_root(&mem_root, MYF(0));
  DBUG_RETURN(NULL);
}


/*
  Example of simple lock controls. The "share" it creates is structure we will
  pass to each federatedx handler. Do you have to have one of these? Well, you
  have pieces that are used for locking, and they are needed to function.
*/

static FEDERATEDX_SHARE *get_share(const char *table_name, TABLE *table)
{
  char query_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  Field **field;
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  FEDERATEDX_SHARE *share= NULL, tmp_share;
  MEM_ROOT mem_root;
  DBUG_ENTER("ha_federatedx.cc::get_share");

  /*
    In order to use this string, we must first zero it's length,
    or it will contain garbage
  */
  query.length(0);

  bzero(&tmp_share, sizeof(tmp_share));
  init_alloc_root(&mem_root, 256, 0);

  pthread_mutex_lock(&federatedx_mutex);

  tmp_share.share_key= table_name;
  tmp_share.share_key_length= strlen(table_name);
  if (parse_url(&mem_root, &tmp_share, table, 0))
    goto error;

  /* TODO: change tmp_share.scheme to LEX_STRING object */
  if (!(share= (FEDERATEDX_SHARE *) hash_search(&federatedx_open_tables,
                                               (uchar*) tmp_share.share_key,
                                               tmp_share.
                                               share_key_length)))
  {
    query.set_charset(system_charset_info);
    query.append(STRING_WITH_LEN("SELECT "));
    for (field= table->field; *field; field++)
    {
      append_ident(&query, (*field)->field_name, 
                   strlen((*field)->field_name), ident_quote_char);
      query.append(STRING_WITH_LEN(", "));
    }
    /* chops off trailing comma */
    query.length(query.length() - sizeof_trailing_comma);

    query.append(STRING_WITH_LEN(" FROM "));

    append_ident(&query, tmp_share.table_name, 
                 tmp_share.table_name_length, ident_quote_char);

    if (!(share= (FEDERATEDX_SHARE *) memdup_root(&mem_root, (char*)&tmp_share, sizeof(*share))) ||
        !(share->select_query= (char*) strmake_root(&mem_root, query.ptr(), query.length() + 1)))
      goto error;

    share->mem_root= mem_root;

    DBUG_PRINT("info",
               ("share->select_query %s", share->select_query));

    if (!(share->s= get_server(share, table)))
      goto error;
   
    if (my_hash_insert(&federatedx_open_tables, (uchar*) share))
      goto error;
    thr_lock_init(&share->lock);
  }
  else
    free_root(&mem_root, MYF(0)); /* prevents memory leak */

  share->use_count++;
  pthread_mutex_unlock(&federatedx_mutex);

  DBUG_RETURN(share);

error:
  pthread_mutex_unlock(&federatedx_mutex);
  free_root(&mem_root, MYF(0));
  DBUG_RETURN(NULL);
}


static int free_server(federatedx_txn *txn, FEDERATEDX_SERVER *server)
{
  bool destroy;
  DBUG_ENTER("free_server");

  pthread_mutex_lock(&federatedx_mutex);
  if ((destroy= !--server->use_count))
    hash_delete(&federatedx_open_servers, (uchar*) server);
  pthread_mutex_unlock(&federatedx_mutex);

  if (destroy)
  {
    MEM_ROOT mem_root;

    txn->close(server);

    DBUG_ASSERT(server->io_count == 0);

    pthread_mutex_destroy(&server->mutex);
    mem_root= server->mem_root;
    free_root(&mem_root, MYF(0));
  }

  DBUG_RETURN(0);
}


/*
  Free lock controls. We call this whenever we close a table.
  If the table had the last reference to the share then we
  free memory associated with it.
*/

static int free_share(federatedx_txn *txn, FEDERATEDX_SHARE *share)
{
  bool destroy;
  DBUG_ENTER("free_share");

  pthread_mutex_lock(&federatedx_mutex);
  if ((destroy= !--share->use_count))
    hash_delete(&federatedx_open_tables, (uchar*) share);
  pthread_mutex_unlock(&federatedx_mutex);

  if (destroy)
  {
    MEM_ROOT mem_root;
    FEDERATEDX_SERVER *server= share->s;

    thr_lock_delete(&share->lock);

    mem_root= share->mem_root;
    free_root(&mem_root, MYF(0));

    free_server(txn, server);
  }

  DBUG_RETURN(0);
}


ha_rows ha_federatedx::records_in_range(uint inx, key_range *start_key,
                                       key_range *end_key)
{
  /*

  We really want indexes to be used as often as possible, therefore
  we just need to hard-code the return value to a very low number to
  force the issue

*/
  DBUG_ENTER("ha_federatedx::records_in_range");
  DBUG_RETURN(FEDERATEDX_RECORDS_IN_RANGE);
}
/*
  If frm_error() is called then we will use this to to find out
  what file extentions exist for the storage engine. This is
  also used by the default rename_table and delete_table method
  in handler.cc.
*/

const char **ha_federatedx::bas_ext() const
{
  static const char *ext[]=
  {
    NullS
  };
  return ext;
}


federatedx_txn *ha_federatedx::get_txn(THD *thd, bool no_create)
{
  federatedx_txn **txnp= (federatedx_txn **) ha_data(thd);
  if (!*txnp && !no_create)
    *txnp= new federatedx_txn();
  return *txnp;
}

  
int ha_federatedx::disconnect(handlerton *hton, MYSQL_THD thd)
{
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  delete txn;
  return 0;
}
 

/*
  Used for opening tables. The name will be the name of the file.
  A table is opened when it needs to be opened. For instance
  when a request comes in for a select on the table (tables are not
  open and closed for each request, they are cached).

  Called from handler.cc by handler::ha_open(). The server opens
  all tables by calling ha_open() which then calls the handler
  specific open().
*/

int ha_federatedx::open(const char *name, int mode, uint test_if_locked)
{
  int error;
  THD *thd= current_thd;
  DBUG_ENTER("ha_federatedx::open");

  if (!(share= get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock, &lock, NULL);

  DBUG_ASSERT(io == NULL);

  txn= get_txn(thd);

  if ((error= txn->acquire(share, TRUE, &io)))
  {
    free_share(txn, share);
    DBUG_RETURN(error);
  }
 
  txn->release(&io);
 
  ref_length= (table->s->primary_key != MAX_KEY ?
               table->key_info[table->s->primary_key].key_length :
               table->s->reclength);
  DBUG_PRINT("info", ("ref_length: %u", ref_length));

  reset();

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

int ha_federatedx::close(void)
{
  int retval, error;
  THD *thd= current_thd;
  DBUG_ENTER("ha_federatedx::close");

  /* free the result set */
  if (stored_result)
    retval= free_result();

  /* Disconnect from mysql */
  if ((txn= get_txn(thd, true)))
    txn->release(&io);

  DBUG_ASSERT(io == NULL);

  if ((error= free_share(txn, share)))
    retval= error;
  DBUG_RETURN(retval);
}

/*

  Checks if a field in a record is SQL NULL.

  SYNOPSIS
    field_in_record_is_null()
      table     TABLE pointer, MySQL table object
      field     Field pointer, MySQL field object
      record    char pointer, contains record

    DESCRIPTION
      This method uses the record format information in table to track
      the null bit in record.

    RETURN VALUE
      1    if NULL
      0    otherwise
*/

static inline uint field_in_record_is_null(TABLE *table,
                                    Field *field,
                                    char *record)
{
  int null_offset;
  DBUG_ENTER("ha_federatedx::field_in_record_is_null");

  if (!field->null_ptr)
    DBUG_RETURN(0);

  null_offset= (uint) ((char*)field->null_ptr - (char*)table->record[0]);

  if (record[null_offset] & field->null_bit)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/**
  @brief Construct the INSERT statement.
  
  @details This method will construct the INSERT statement and appends it to
  the supplied query string buffer.
  
  @return
    @retval FALSE       No error
    @retval TRUE        Failure
*/

bool ha_federatedx::append_stmt_insert(String *query)
{
  char insert_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  Field **field;
  uint tmp_length;
  bool added_field= FALSE;

  /* The main insert query string */
  String insert_string(insert_buffer, sizeof(insert_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federatedx::append_stmt_insert");

  insert_string.length(0);

  if (replace_duplicates)
    insert_string.append(STRING_WITH_LEN("REPLACE INTO "));
  else if (ignore_duplicates && !insert_dup_update)
    insert_string.append(STRING_WITH_LEN("INSERT IGNORE INTO "));
  else
    insert_string.append(STRING_WITH_LEN("INSERT INTO "));
  append_ident(&insert_string, share->table_name, share->table_name_length, 
               ident_quote_char);
  tmp_length= insert_string.length();
  insert_string.append(STRING_WITH_LEN(" ("));

  /*
    loop through the field pointer array, add any fields to both the values
    list and the fields list that match the current query id
  */
  for (field= table->field; *field; field++)
  {
    if (bitmap_is_set(table->write_set, (*field)->field_index))
    {
      /* append the field name */
      append_ident(&insert_string, (*field)->field_name, 
                   strlen((*field)->field_name), ident_quote_char);

      /* append commas between both fields and fieldnames */
      /*
        unfortunately, we can't use the logic if *(fields + 1) to
        make the following appends conditional as we don't know if the
        next field is in the write set
      */
      insert_string.append(STRING_WITH_LEN(", "));
      added_field= TRUE;
    }
  }

  if (added_field)
  {
    /* Remove trailing comma. */
    insert_string.length(insert_string.length() - sizeof_trailing_comma);
    insert_string.append(STRING_WITH_LEN(") "));
  }
  else
  {
    /* If there were no fields, we don't want to add a closing paren. */
    insert_string.length(tmp_length);
  }

  insert_string.append(STRING_WITH_LEN(" VALUES "));

  DBUG_RETURN(query->append(insert_string));
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

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.
*/

int ha_federatedx::write_row(uchar *buf)
{
  char values_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  char insert_field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  Field **field;
  uint tmp_length;
  int error= 0;
  bool use_bulk_insert;
  bool auto_increment_update_required= (table->next_number_field != NULL);

  /* The string containing the values to be added to the insert */
  String values_string(values_buffer, sizeof(values_buffer), &my_charset_bin);
  /* The actual value of the field, to be added to the values_string */
  String insert_field_value_string(insert_field_value_buffer,
                                   sizeof(insert_field_value_buffer),
                                   &my_charset_bin);
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  DBUG_ENTER("ha_federatedx::write_row");

  values_string.length(0);
  insert_field_value_string.length(0);
  ha_statistic_increment(&SSV::ha_write_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  /*
    start both our field and field values strings
    We must disable multi-row insert for "INSERT...ON DUPLICATE KEY UPDATE"
    Ignore duplicates is always true when insert_dup_update is true.
    When replace_duplicates == TRUE, we can safely enable multi-row insert.
    When performing multi-row insert, we only collect the columns values for
    the row. The start of the statement is only created when the first
    row is copied in to the bulk_insert string.
  */
  if (!(use_bulk_insert= bulk_insert.str && 
        (!insert_dup_update || replace_duplicates)))
    append_stmt_insert(&values_string);

  values_string.append(STRING_WITH_LEN(" ("));
  tmp_length= values_string.length();

  /*
    loop through the field pointer array, add any fields to both the values
    list and the fields list that is part of the write set
  */
  for (field= table->field; *field; field++)
  {
    if (bitmap_is_set(table->write_set, (*field)->field_index))
    {
      if ((*field)->is_null())
        values_string.append(STRING_WITH_LEN(" NULL "));
      else
      {
        bool needs_quote= (*field)->str_needs_quotes();
        (*field)->val_str(&insert_field_value_string);
        if (needs_quote)
          values_string.append(value_quote_char);
        insert_field_value_string.print(&values_string);
        if (needs_quote)
          values_string.append(value_quote_char);

        insert_field_value_string.length(0);
      }

      /* append commas between both fields and fieldnames */
      /*
        unfortunately, we can't use the logic if *(fields + 1) to
        make the following appends conditional as we don't know if the
        next field is in the write set
      */
      values_string.append(STRING_WITH_LEN(", "));
    }
  }
  dbug_tmp_restore_column_map(table->read_set, old_map);

  /*
    if there were no fields, we don't want to add a closing paren
    AND, we don't want to chop off the last char '('
    insert will be "INSERT INTO t1 VALUES ();"
  */
  if (values_string.length() > tmp_length)
  {
    /* chops off trailing comma */
    values_string.length(values_string.length() - sizeof_trailing_comma);
  }
  /* we always want to append this, even if there aren't any fields */
  values_string.append(STRING_WITH_LEN(") "));

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (use_bulk_insert)
  {
    /*
      Send the current bulk insert out if appending the current row would
      cause the statement to overflow the packet size, otherwise set
      auto_increment_update_required to FALSE as no query was executed.
    */
    if (bulk_insert.length + values_string.length() + bulk_padding >
        io->max_query_size() && bulk_insert.length)
    {
      error= io->query(bulk_insert.str, bulk_insert.length);
      bulk_insert.length= 0;
    }
    else
      auto_increment_update_required= FALSE;
      
    if (bulk_insert.length == 0)
    {
      char insert_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
      String insert_string(insert_buffer, sizeof(insert_buffer), 
                           &my_charset_bin);
      insert_string.length(0);
      append_stmt_insert(&insert_string);
      dynstr_append_mem(&bulk_insert, insert_string.ptr(), 
                        insert_string.length());
    }
    else
      dynstr_append_mem(&bulk_insert, ",", 1);

    dynstr_append_mem(&bulk_insert, values_string.ptr(), 
                      values_string.length());
  }  
  else
  {
    error= io->query(values_string.ptr(), values_string.length());
  }
  
  if (error)
  {
    DBUG_RETURN(stash_remote_error());
  }
  /*
    If the table we've just written a record to contains an auto_increment
    field, then store the last_insert_id() value from the foreign server
  */
  if (auto_increment_update_required)
  {
    update_auto_increment();

    /* mysql_insert() uses this for protocol return value */
    table->next_number_field->store(stats.auto_increment_value, 1);
  }

  DBUG_RETURN(0);
}


/**
  @brief Prepares the storage engine for bulk inserts.
  
  @param[in] rows       estimated number of rows in bulk insert 
                        or 0 if unknown.
  
  @details Initializes memory structures required for bulk insert.
*/

void ha_federatedx::start_bulk_insert(ha_rows rows)
{
  uint page_size;
  DBUG_ENTER("ha_federatedx::start_bulk_insert");

  dynstr_free(&bulk_insert);
  
  /**
    We don't bother with bulk-insert semantics when the estimated rows == 1
    The rows value will be 0 if the server does not know how many rows
    would be inserted. This can occur when performing INSERT...SELECT
  */
  
  if (rows == 1)
    DBUG_VOID_RETURN;

  /*
    Make sure we have an open connection so that we know the 
    maximum packet size.
  */
  if (txn->acquire(share, FALSE, &io))
    DBUG_VOID_RETURN;

  page_size= (uint) my_getpagesize();

  if (init_dynamic_string(&bulk_insert, NULL, page_size, page_size))
    DBUG_VOID_RETURN;
  
  bulk_insert.length= 0;
  DBUG_VOID_RETURN;
}


/**
  @brief End bulk insert.
  
  @details This method will send any remaining rows to the remote server.
  Finally, it will deinitialize the bulk insert data structure.
  
  @return Operation status
  @retval       0       No error
  @retval       != 0    Error occured at remote server. Also sets my_errno.
*/

int ha_federatedx::end_bulk_insert(bool abort)
{
  int error= 0;
  DBUG_ENTER("ha_federatedx::end_bulk_insert");
  
  if (bulk_insert.str && bulk_insert.length && !abort)
  {
    if ((error= txn->acquire(share, FALSE, &io)))
      DBUG_RETURN(error);
    if (io->query(bulk_insert.str, bulk_insert.length))
      error= stash_remote_error();
    else
    if (table->next_number_field)
      update_auto_increment();
  }

  dynstr_free(&bulk_insert);
  
  DBUG_RETURN(my_errno= error);
}


/*
  ha_federatedx::update_auto_increment

  This method ensures that last_insert_id() works properly. What it simply does
  is calls last_insert_id() on the foreign database immediately after insert
  (if the table has an auto_increment field) and sets the insert id via
  thd->insert_id(ID)).
*/
void ha_federatedx::update_auto_increment(void)
{
  THD *thd= current_thd;
  DBUG_ENTER("ha_federatedx::update_auto_increment");

  ha_federatedx::info(HA_STATUS_AUTO);
  thd->first_successful_insert_id_in_cur_stmt= 
    stats.auto_increment_value;
  DBUG_PRINT("info",("last_insert_id: %ld", (long) stats.auto_increment_value));

  DBUG_VOID_RETURN;
}

int ha_federatedx::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  int error= 0;
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federatedx::optimize");
  
  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("OPTIMIZE TABLE "));
  append_ident(&query, share->table_name, share->table_name_length, 
               ident_quote_char);

  DBUG_ASSERT(txn == get_txn(thd));

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (io->query(query.ptr(), query.length()))
    error= stash_remote_error();

  DBUG_RETURN(error);
}


int ha_federatedx::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  int error= 0;
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federatedx::repair");

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("REPAIR TABLE "));
  append_ident(&query, share->table_name, share->table_name_length, 
               ident_quote_char);
  if (check_opt->flags & T_QUICK)
    query.append(STRING_WITH_LEN(" QUICK"));
  if (check_opt->flags & T_EXTEND)
    query.append(STRING_WITH_LEN(" EXTENDED"));
  if (check_opt->sql_flags & TT_USEFRM)
    query.append(STRING_WITH_LEN(" USE_FRM"));

  DBUG_ASSERT(txn == get_txn(thd));

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (io->query(query.ptr(), query.length()))
    error= stash_remote_error();

  DBUG_RETURN(error);
}


/*
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in
  it.

  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.
  Currently new_data will not have an updated auto_increament record, or
  and updated timestamp field. You can do these for federatedx by doing these:
  if (table->timestamp_on_update_now)
    update_timestamp(new_row+table->timestamp_on_update_now-1);
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
*/

int ha_federatedx::update_row(const uchar *old_data, uchar *new_data)
{
  /*
    This used to control how the query was built. If there was a
    primary key, the query would be built such that there was a where
    clause with only that column as the condition. This is flawed,
    because if we have a multi-part primary key, it would only use the
    first part! We don't need to do this anyway, because
    read_range_first will retrieve the correct record, which is what
    is used to build the WHERE clause. We can however use this to
    append a LIMIT to the end if there is NOT a primary key. Why do
    this? Because we only are updating one record, and LIMIT enforces
    this.
  */
  bool has_a_primary_key= test(table->s->primary_key != MAX_KEY);
  
  /*
    buffers for following strings
  */
  char field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  char update_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  char where_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];

  /* Work area for field values */
  String field_value(field_value_buffer, sizeof(field_value_buffer),
                     &my_charset_bin);
  /* stores the update query */
  String update_string(update_buffer,
                       sizeof(update_buffer),
                       &my_charset_bin);
  /* stores the WHERE clause */
  String where_string(where_buffer,
                      sizeof(where_buffer),
                      &my_charset_bin);
  uchar *record= table->record[0];
  int error;
  DBUG_ENTER("ha_federatedx::update_row");
  /*
    set string lengths to 0 to avoid misc chars in string
  */
  field_value.length(0);
  update_string.length(0);
  where_string.length(0);

  if (ignore_duplicates)
    update_string.append(STRING_WITH_LEN("UPDATE IGNORE "));
  else
    update_string.append(STRING_WITH_LEN("UPDATE "));
  append_ident(&update_string, share->table_name,
               share->table_name_length, ident_quote_char);
  update_string.append(STRING_WITH_LEN(" SET "));

  /*
    In this loop, we want to match column names to values being inserted
    (while building INSERT statement).

    Iterate through table->field (new data) and share->old_field (old_data)
    using the same index to create an SQL UPDATE statement. New data is
    used to create SET field=value and old data is used to create WHERE
    field=oldvalue
  */

  for (Field **field= table->field; *field; field++)
  {
    if (bitmap_is_set(table->write_set, (*field)->field_index))
    {
      uint field_name_length= strlen((*field)->field_name);
      append_ident(&update_string, (*field)->field_name, field_name_length,
                   ident_quote_char);
      update_string.append(STRING_WITH_LEN(" = "));

      if ((*field)->is_null())
        update_string.append(STRING_WITH_LEN(" NULL "));
      else
      {
        /* otherwise = */
        my_bitmap_map *old_map= tmp_use_all_columns(table, table->read_set);
        bool needs_quote= (*field)->str_needs_quotes();
	(*field)->val_str(&field_value);
        if (needs_quote)
          update_string.append(value_quote_char);
        field_value.print(&update_string);
        if (needs_quote)
          update_string.append(value_quote_char);
        field_value.length(0);
        tmp_restore_column_map(table->read_set, old_map);
      }
      update_string.append(STRING_WITH_LEN(", "));
    }

    if (bitmap_is_set(table->read_set, (*field)->field_index))
    {
      uint field_name_length= strlen((*field)->field_name);
      append_ident(&where_string, (*field)->field_name, field_name_length,
                   ident_quote_char);
      if (field_in_record_is_null(table, *field, (char*) old_data))
        where_string.append(STRING_WITH_LEN(" IS NULL "));
      else
      {
        bool needs_quote= (*field)->str_needs_quotes();
        where_string.append(STRING_WITH_LEN(" = "));
        (*field)->val_str(&field_value,
                          (old_data + (*field)->offset(record)));
        if (needs_quote)
          where_string.append(value_quote_char);
        field_value.print(&where_string);
        if (needs_quote)
          where_string.append(value_quote_char);
        field_value.length(0);
      }
      where_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  /* Remove last ', '. This works as there must be at least on updated field */
  update_string.length(update_string.length() - sizeof_trailing_comma);

  if (where_string.length())
  {
    /* chop off trailing AND */
    where_string.length(where_string.length() - sizeof_trailing_and);
    update_string.append(STRING_WITH_LEN(" WHERE "));
    update_string.append(where_string);
  }

  /*
    If this table has not a primary key, then we could possibly
    update multiple rows. We want to make sure to only update one!
  */
  if (!has_a_primary_key)
    update_string.append(STRING_WITH_LEN(" LIMIT 1"));

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (io->query(update_string.ptr(), update_string.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  DBUG_RETURN(0);
}

/*
  This will delete a row. 'buf' will contain a copy of the row to be =deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_next() or index call).
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier.
  Keep in mind that the server does no guarentee consecutive deletions.
  ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table information.
  Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select
  it is used for removing duplicates while in insert it is used for REPLACE
  calls.
*/

int ha_federatedx::delete_row(const uchar *buf)
{
  char delete_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  char data_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  String delete_string(delete_buffer, sizeof(delete_buffer), &my_charset_bin);
  String data_string(data_buffer, sizeof(data_buffer), &my_charset_bin);
  uint found= 0;
  int error;
  DBUG_ENTER("ha_federatedx::delete_row");

  delete_string.length(0);
  delete_string.append(STRING_WITH_LEN("DELETE FROM "));
  append_ident(&delete_string, share->table_name,
               share->table_name_length, ident_quote_char);
  delete_string.append(STRING_WITH_LEN(" WHERE "));

  for (Field **field= table->field; *field; field++)
  {
    Field *cur_field= *field;
    found++;
    if (bitmap_is_set(table->read_set, cur_field->field_index))
    {
      append_ident(&delete_string, (*field)->field_name,
                   strlen((*field)->field_name), ident_quote_char);
      data_string.length(0);
      if (cur_field->is_null())
      {
        delete_string.append(STRING_WITH_LEN(" IS NULL "));
      }
      else
      {
        bool needs_quote= cur_field->str_needs_quotes();
        delete_string.append(STRING_WITH_LEN(" = "));
        cur_field->val_str(&data_string);
        if (needs_quote)
          delete_string.append(value_quote_char);
        data_string.print(&delete_string);
        if (needs_quote)
          delete_string.append(value_quote_char);
      }
      delete_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  // Remove trailing AND
  delete_string.length(delete_string.length() - sizeof_trailing_and);
  if (!found)
    delete_string.length(delete_string.length() - sizeof_trailing_where);

  delete_string.append(STRING_WITH_LEN(" LIMIT 1"));
  DBUG_PRINT("info",
             ("Delete sql: %s", delete_string.c_ptr_quick()));

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (io->query(delete_string.ptr(), delete_string.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  stats.deleted+= (ha_rows) io->affected_rows();
  stats.records-= (ha_rows) io->affected_rows();
  DBUG_PRINT("info",
             ("rows deleted %ld  rows deleted for all time %ld",
              (long) io->affected_rows(), (long) stats.deleted));

  DBUG_RETURN(0);
}


/*
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index. This method, which is called in the case of an SQL statement having
  a WHERE clause on a non-primary key index, simply calls index_read_idx.
*/

int ha_federatedx::index_read(uchar *buf, const uchar *key,
                             uint key_len, ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_federatedx::index_read");

  if (stored_result)
    (void) free_result();
  DBUG_RETURN(index_read_idx_with_result_set(buf, active_index, key,
                                             key_len, find_flag,
                                             &stored_result));
}


/*
  Positions an index cursor to the index specified in key. Fetches the
  row if any.  This is only used to read whole keys.

  This method is called via index_read in the case of a WHERE clause using
  a primary key index OR is called DIRECTLY when the WHERE clause
  uses a PRIMARY KEY index.

  NOTES
    This uses an internal result set that is deleted before function
    returns.  We need to be able to be callable from ha_rnd_pos()
*/

int ha_federatedx::index_read_idx(uchar *buf, uint index, const uchar *key,
                                 uint key_len, enum ha_rkey_function find_flag)
{
  int retval;
  FEDERATEDX_IO_RESULT *io_result;
  DBUG_ENTER("ha_federatedx::index_read_idx");

  if ((retval= index_read_idx_with_result_set(buf, index, key,
                                              key_len, find_flag,
                                              &io_result)))
    DBUG_RETURN(retval);
  /* io is correct, as index_read_idx_with_result_set was ok */
  io->free_result(io_result);
  DBUG_RETURN(retval);
}


/*
  Create result set for rows matching query and return first row

  RESULT
    0	ok     In this case *result will contain the result set
	       table->status == 0 
    #   error  In this case *result will contain 0
               table->status == STATUS_NOT_FOUND
*/

int ha_federatedx::index_read_idx_with_result_set(uchar *buf, uint index,
                                                 const uchar *key,
                                                 uint key_len,
                                                 ha_rkey_function find_flag,
                                                 FEDERATEDX_IO_RESULT **result)
{
  int retval;
  char error_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  char index_value[STRING_BUFFER_USUAL_SIZE];
  char sql_query_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  String index_string(index_value,
                      sizeof(index_value),
                      &my_charset_bin);
  String sql_query(sql_query_buffer,
                   sizeof(sql_query_buffer),
                   &my_charset_bin);
  key_range range;
  DBUG_ENTER("ha_federatedx::index_read_idx_with_result_set");

  *result= 0;                                   // In case of errors
  index_string.length(0);
  sql_query.length(0);
  ha_statistic_increment(&SSV::ha_read_key_count);

  sql_query.append(share->select_query);

  range.key= key;
  range.length= key_len;
  range.flag= find_flag;
  create_where_from_key(&index_string,
                        &table->key_info[index],
                        &range,
                        NULL, 0, 0);
  sql_query.append(index_string);

  if ((retval= txn->acquire(share, TRUE, &io)))
    DBUG_RETURN(retval);

  if (io->query(sql_query.ptr(), sql_query.length()))
  {
    my_sprintf(error_buffer, (error_buffer, "error: %d '%s'",
                              io->error_code(), io->error_str()));
    retval= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  if (!(*result= io->store_result()))
  {
    retval= HA_ERR_END_OF_FILE;
    goto error;
  }
  if (!(retval= read_next(buf, *result)))
    DBUG_RETURN(retval);

  io->free_result(*result);
  *result= 0;
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(retval);

error:
  table->status= STATUS_NOT_FOUND;
  my_error(retval, MYF(0), error_buffer);
  DBUG_RETURN(retval);
}


/*
  This method is used exlusevely by filesort() to check if we
  can create sorting buffers of necessary size.
  If the handler returns more records that it declares
  here server can just crash on filesort().
  We cannot guarantee that's not going to happen with
  the FEDERATEDX engine, as we have records==0 always if the
  client is a VIEW, and for the table the number of
  records can inpredictably change during execution.
  So we return maximum possible value here.
*/

ha_rows ha_federatedx::estimate_rows_upper_bound()
{
  return HA_POS_ERROR;
}


/* Initialized at each key walk (called multiple times unlike rnd_init()) */

int ha_federatedx::index_init(uint keynr, bool sorted)
{
  DBUG_ENTER("ha_federatedx::index_init");
  DBUG_PRINT("info", ("table: '%s'  key: %u", table->s->table_name.str, keynr));
  active_index= keynr;
  DBUG_RETURN(0);
}


/*
  Read first range
*/

int ha_federatedx::read_range_first(const key_range *start_key,
                                   const key_range *end_key,
                                   bool eq_range_arg, bool sorted)
{
  char sql_query_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  int retval;
  String sql_query(sql_query_buffer,
                   sizeof(sql_query_buffer),
                   &my_charset_bin);
  DBUG_ENTER("ha_federatedx::read_range_first");

  DBUG_ASSERT(!(start_key == NULL && end_key == NULL));

  sql_query.length(0);
  sql_query.append(share->select_query);
  create_where_from_key(&sql_query,
                        &table->key_info[active_index],
                        start_key, end_key, 0, eq_range_arg);

  if ((retval= txn->acquire(share, TRUE, &io)))
    DBUG_RETURN(retval);

  if (stored_result)
  {
    io->free_result(stored_result);
    stored_result= 0;
  }

  if (io->query(sql_query.ptr(), sql_query.length()))
  {
    retval= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  sql_query.length(0);

  if (!(stored_result= io->store_result()))
  {
    retval= HA_ERR_END_OF_FILE;
    goto error;
  }

  retval= read_next(table->record[0], stored_result);
  DBUG_RETURN(retval);

error:
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(retval);
}


int ha_federatedx::read_range_next()
{
  int retval;
  DBUG_ENTER("ha_federatedx::read_range_next");
  retval= rnd_next(table->record[0]);
  DBUG_RETURN(retval);
}


/* Used to read forward through the index.  */
int ha_federatedx::index_next(uchar *buf)
{
  DBUG_ENTER("ha_federatedx::index_next");
  ha_statistic_increment(&SSV::ha_read_next_count);
  DBUG_RETURN(read_next(buf, stored_result));
}


/*
  rnd_init() is called when the system wants the storage engine to do a table
  scan.

  This is the method that gets data for the SELECT calls.

  See the federatedx in the introduction at the top of this file to see when
  rnd_init() is called.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federatedx::rnd_init(bool scan)
{
  DBUG_ENTER("ha_federatedx::rnd_init");
  /*
    The use of the 'scan' flag is incredibly important for this handler
    to work properly, especially with updates containing WHERE clauses
    using indexed columns.

    When the initial query contains a WHERE clause of the query using an
    indexed column, it's index_read_idx that selects the exact record from
    the foreign database.

    When there is NO index in the query, either due to not having a WHERE
    clause, or the WHERE clause is using columns that are not indexed, a
    'full table scan' done by rnd_init, which in this situation simply means
    a 'select * from ...' on the foreign table.

    In other words, this 'scan' flag gives us the means to ensure that if
    there is an index involved in the query, we want index_read_idx to
    retrieve the exact record (scan flag is 0), and do not  want rnd_init
    to do a 'full table scan' and wipe out that result set.

    Prior to using this flag, the problem was most apparent with updates.

    An initial query like 'UPDATE tablename SET anything = whatever WHERE
    indexedcol = someval', index_read_idx would get called, using a query
    constructed with a WHERE clause built from the values of index ('indexcol'
    in this case, having a value of 'someval').  mysql_store_result would
    then get called (this would be the result set we want to use).

    After this rnd_init (from sql_update.cc) would be called, it would then
    unecessarily call "select * from table" on the foreign table, then call
    mysql_store_result, which would wipe out the correct previous result set
    from the previous call of index_read_idx's that had the result set
    containing the correct record, hence update the wrong row!

  */

  if (scan)
  {
    int error;

    if ((error= txn->acquire(share, TRUE, &io)))
      DBUG_RETURN(error);

    if (stored_result)
    {
      io->free_result(stored_result);
      stored_result= 0;
    }

    if (io->query(share->select_query,
                  strlen(share->select_query)))
      goto error;

    stored_result= io->store_result();
    if (!stored_result)
      goto error;
  }
  DBUG_RETURN(0);

error:
  DBUG_RETURN(stash_remote_error());
}


int ha_federatedx::rnd_end()
{
  DBUG_ENTER("ha_federatedx::rnd_end");
  DBUG_RETURN(index_end());
}


int ha_federatedx::free_result()
{
  int error;
  DBUG_ASSERT(stored_result);
  if ((error= txn->acquire(share, FALSE, &io)))
  {
    DBUG_ASSERT(0);                             // Fail when testing
    return error;
  }
  io->free_result(stored_result);
  stored_result= 0;
  return 0;
}

int ha_federatedx::index_end(void)
{
  int error= 0;
  DBUG_ENTER("ha_federatedx::index_end");
  if (stored_result)
    error= free_result();
  active_index= MAX_KEY;
  DBUG_RETURN(error);
}


/*
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federatedx::rnd_next(uchar *buf)
{
  DBUG_ENTER("ha_federatedx::rnd_next");

  if (stored_result == 0)
  {
    /*
      Return value of rnd_init is not always checked (see records.cc),
      so we can get here _even_ if there is _no_ pre-fetched result-set!
      TODO: fix it. We can delete this in 5.1 when rnd_init() is checked.
    */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(read_next(buf, stored_result));
}


/*
  ha_federatedx::read_next

  reads from a result set and converts to mysql internal
  format

  SYNOPSIS
    field_in_record_is_null()
      buf       byte pointer to record 
      result    mysql result set 

    DESCRIPTION
     This method is a wrapper method that reads one record from a result
     set and converts it to the internal table format

    RETURN VALUE
      1    error
      0    no error 
*/

int ha_federatedx::read_next(uchar *buf, FEDERATEDX_IO_RESULT *result)
{
  int retval;
  FEDERATEDX_IO_ROW *row;
  DBUG_ENTER("ha_federatedx::read_next");

  table->status= STATUS_NOT_FOUND;              // For easier return

  if ((retval= txn->acquire(share, TRUE, &io)))
    DBUG_RETURN(retval);

  /* Fetch a row, insert it back in a row format. */
  if (!(row= io->fetch_row(result)))
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  if (!(retval= convert_row_to_internal_format(buf, row, result)))
    table->status= 0;

  DBUG_RETURN(retval);
}


/*
  store reference to current row so that we can later find it for
  a re-read, update or delete.

  In case of federatedx, a reference is either a primary key or
  the whole record.

  Called from filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc.
*/

void ha_federatedx::position(const uchar *record)
{
  DBUG_ENTER("ha_federatedx::position");
  if (table->s->primary_key != MAX_KEY)
    key_copy(ref, (uchar *)record, table->key_info + table->s->primary_key,
             ref_length);
  else
    memcpy(ref, record, ref_length);
  DBUG_VOID_RETURN;
}


/*
  This is like rnd_next, but you are given a position to use to determine the
  row. The position will be of the type that you stored in ref.

  This method is required for an ORDER BY

  Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
*/

int ha_federatedx::rnd_pos(uchar *buf, uchar *pos)
{
  int result;
  DBUG_ENTER("ha_federatedx::rnd_pos");
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  if (table->s->primary_key != MAX_KEY)
  {
    /* We have a primary key, so use index_read_idx to find row */
    result= index_read_idx(buf, table->s->primary_key, pos,
                           ref_length, HA_READ_KEY_EXACT);
  }
  else
  {
    /* otherwise, get the old record ref as obtained in ::position */
    memcpy(buf, pos, ref_length);
    result= 0;
  }
  table->status= result ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(result);
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

int ha_federatedx::info(uint flag)
{
  char error_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  uint error_code;
  federatedx_io *tmp_io= 0;
  DBUG_ENTER("ha_federatedx::info");

  error_code= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
  
  /* we want not to show table status if not needed to do so */
  if (flag & (HA_STATUS_VARIABLE | HA_STATUS_CONST | HA_STATUS_AUTO))
  {
    if ((error_code= txn->acquire(share, TRUE, &tmp_io)))
      goto fail;
  }

  if (flag & (HA_STATUS_VARIABLE | HA_STATUS_CONST))
  {
    /*
      size of IO operations (This is based on a good guess, no high science
      involved)
    */
    if (flag & HA_STATUS_CONST)
      stats.block_size= 4096;

    if (tmp_io->table_metadata(&stats, share->table_name,
                               share->table_name_length, flag))
      goto error;
  }

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= tmp_io->last_insert_id();

  /*
    If ::info created it's own transaction, close it. This happens in case
    of show table status;
  */
  txn->release(&tmp_io);

  DBUG_RETURN(0);

error:
  if (tmp_io)
  {
    my_sprintf(error_buffer, (error_buffer, ": %d : %s",
                              tmp_io->error_code(), tmp_io->error_str()));
    my_error(error_code, MYF(0), error_buffer);
  }
  else
  if (remote_error_number != -1 /* error already reported */)
  {
    error_code= remote_error_number;
    my_error(error_code, MYF(0), ER(error_code));
  }
fail:
  txn->release(&tmp_io);
  DBUG_RETURN(error_code);
}


/**
  @brief Handles extra signals from MySQL server

  @param[in] operation  Hint for storage engine

  @return Operation Status
  @retval 0     OK
 */
int ha_federatedx::extra(ha_extra_function operation)
{
  DBUG_ENTER("ha_federatedx::extra");
  switch (operation) {
  case HA_EXTRA_IGNORE_DUP_KEY:
    ignore_duplicates= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    insert_dup_update= FALSE;
    ignore_duplicates= FALSE;
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    replace_duplicates= TRUE;
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    /*
      We use this flag to ensure that we do not create an "INSERT IGNORE"
      statement when inserting new rows into the remote table.
    */
    replace_duplicates= FALSE;
    break;
  case HA_EXTRA_INSERT_WITH_UPDATE:
    insert_dup_update= TRUE;
    break;
  default:
    /* do nothing */
    DBUG_PRINT("info",("unhandled operation: %d", (uint) operation));
  }
  DBUG_RETURN(0);
}


/**
  @brief Reset state of file to after 'open'.

  @detail This function is called after every statement for all tables
    used by that statement.

  @return Operation status
    @retval     0       OK
*/

int ha_federatedx::reset(void)
{
  insert_dup_update= FALSE;
  ignore_duplicates= FALSE;
  replace_duplicates= FALSE;
  return 0;
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

int ha_federatedx::delete_all_rows()
{
  char query_buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  int error;
  DBUG_ENTER("ha_federatedx::delete_all_rows");

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("TRUNCATE "));
  append_ident(&query, share->table_name, share->table_name_length,
               ident_quote_char);

  /* no need for savepoint in autocommit mode */
  if (!(ha_thd()->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
    txn->stmt_autocommit();

  /*
    TRUNCATE won't return anything in mysql_affected_rows
  */

  if ((error= txn->acquire(share, FALSE, &io)))
    DBUG_RETURN(error);

  if (io->query(query.ptr(), query.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  stats.deleted+= stats.records;
  stats.records= 0;
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

  Berkeley DB for federatedx  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
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

THR_LOCK_DATA **ha_federatedx::store_lock(THD *thd,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  DBUG_ENTER("ha_federatedx::store_lock");
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)
      lock_type= TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;

    lock.type= lock_type;
  }

  *to++= &lock;

  DBUG_RETURN(to);
}


static int test_connection(MYSQL_THD thd, federatedx_io *io,
                           FEDERATEDX_SHARE *share)
{
  char buffer[FEDERATEDX_QUERY_BUFFER_SIZE];
  String str(buffer, sizeof(buffer), &my_charset_bin);
  FEDERATEDX_IO_RESULT *resultset= NULL;
  int retval;

  str.length(0);
  str.append(STRING_WITH_LEN("SELECT * FROM "));
  append_identifier(thd, &str, share->table_name, 
                    share->table_name_length);
  str.append(STRING_WITH_LEN(" WHERE 1=0"));

  if ((retval= io->query(str.ptr(), str.length())))
  {
    my_sprintf(buffer, (buffer,
               "database: '%s'  username: '%s'  hostname: '%s'",
               share->database, share->username, share->hostname));
    DBUG_PRINT("info", ("error-code: %d", io->error_code()));
    my_error(ER_CANT_CREATE_FEDERATED_TABLE, MYF(0), buffer);
  }
  else
    resultset= io->store_result();

  io->free_result(resultset);

  return retval;
}

/*
  create() does nothing, since we have no local setup of our own.
  FUTURE: We should potentially connect to the foreign database and
*/

int ha_federatedx::create(const char *name, TABLE *table_arg,
                         HA_CREATE_INFO *create_info)
{
  int retval;
  THD *thd= current_thd;
  FEDERATEDX_SHARE tmp_share; // Only a temporary share, to test the url
  federatedx_txn *tmp_txn;
  federatedx_io *tmp_io= NULL;
  DBUG_ENTER("ha_federatedx::create");

  if ((retval= parse_url(thd->mem_root, &tmp_share, table_arg, 1)))
    goto error;

  /* loopback socket connections hang due to LOCK_open mutex */
  if ((!tmp_share.hostname || !strcmp(tmp_share.hostname,my_localhost)) &&
      !tmp_share.port)
    goto error;

  /*
    If possible, we try to use an existing network connection to
    the remote server. To ensure that no new FEDERATEDX_SERVER
    instance is created, we pass NULL in get_server() TABLE arg.
  */
  pthread_mutex_lock(&federatedx_mutex);
  tmp_share.s= get_server(&tmp_share, NULL);
  pthread_mutex_unlock(&federatedx_mutex);
    
  if (tmp_share.s)
  {
    tmp_txn= get_txn(thd);
    if (!(retval= tmp_txn->acquire(&tmp_share, TRUE, &tmp_io)))
    {
      retval= test_connection(thd, tmp_io, &tmp_share);
      tmp_txn->release(&tmp_io);    
    }
    free_server(tmp_txn, tmp_share.s);
  }
  else
  {
    FEDERATEDX_SERVER server;

#ifdef NOT_YET
    /* 
      Bug#25679
      Ensure that we do not hold the LOCK_open mutex while attempting
      to establish FederatedX connection to guard against a trivial
      Denial of Service scenerio.
    */
    safe_mutex_assert_not_owner(&LOCK_open);
#endif

    fill_server(thd->mem_root, &server, &tmp_share, create_info->table_charset);

#ifndef DBUG_OFF
    pthread_mutex_init(&server.mutex, MY_MUTEX_INIT_FAST);
    pthread_mutex_lock(&server.mutex);
#endif

    tmp_io= federatedx_io::construct(thd->mem_root, &server);

    retval= test_connection(thd, tmp_io, &tmp_share);

#ifndef DBUG_OFF
    pthread_mutex_unlock(&server.mutex);
    pthread_mutex_destroy(&server.mutex);
#endif

    delete tmp_io;
  }

error:
  DBUG_RETURN(retval);

}


int ha_federatedx::stash_remote_error()
{
  DBUG_ENTER("ha_federatedx::stash_remote_error()");
  if (!io)
    DBUG_RETURN(remote_error_number);
  remote_error_number= io->error_code();
  strmake(remote_error_buf, io->error_str(), sizeof(remote_error_buf)-1);
  if (remote_error_number == ER_DUP_ENTRY ||
      remote_error_number == ER_DUP_KEY)
    DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
  DBUG_RETURN(HA_FEDERATEDX_ERROR_WITH_REMOTE_SYSTEM);
}


bool ha_federatedx::get_error_message(int error, String* buf)
{
  DBUG_ENTER("ha_federatedx::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));
  if (error == HA_FEDERATEDX_ERROR_WITH_REMOTE_SYSTEM)
  {
    buf->append(STRING_WITH_LEN("Error on remote system: "));
    buf->qs_append(remote_error_number);
    buf->append(STRING_WITH_LEN(": "));
    buf->append(remote_error_buf);

    remote_error_number= 0;
    remote_error_buf[0]= '\0';
  }
  DBUG_PRINT("exit", ("message: %s", buf->ptr()));
  DBUG_RETURN(FALSE);
}


int ha_federatedx::start_stmt(MYSQL_THD thd, thr_lock_type lock_type)
{
  DBUG_ENTER("ha_federatedx::start_stmt");
  DBUG_ASSERT(txn == get_txn(thd));
  
  if (!txn->in_transaction())
  {
    txn->stmt_begin();
    trans_register_ha(thd, FALSE, ht);
  }
  DBUG_RETURN(0);
}


int ha_federatedx::external_lock(MYSQL_THD thd, int lock_type)
{
  int error= 0;
  DBUG_ENTER("ha_federatedx::external_lock");

  if (lock_type == F_UNLCK)
    txn->release(&io);
  else
  {
    txn= get_txn(thd);  
    if (!(error= txn->acquire(share, lock_type == F_RDLCK, &io)) &&
        (lock_type == F_WRLCK || !io->is_autocommit()))
    {
      if (!thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
      {
        txn->stmt_begin();
        trans_register_ha(thd, FALSE, ht);
      }
      else
      {
        txn->txn_begin();
        trans_register_ha(thd, TRUE, ht);
      }
    }
  }

  DBUG_RETURN(error);
}


int ha_federatedx::savepoint_set(handlerton *hton, MYSQL_THD thd, void *sv)
{
  int error= 0;
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  DBUG_ENTER("ha_federatedx::savepoint_set");

  if (txn && txn->has_connections())
  {
    if (txn->txn_begin())
      trans_register_ha(thd, TRUE, hton);
    
    txn->sp_acquire((ulong *) sv);

    DBUG_ASSERT(1 < *(ulong *) sv);
  }

  DBUG_RETURN(error);
}


int ha_federatedx::savepoint_rollback(handlerton *hton, MYSQL_THD thd, void *sv)
 {
  int error= 0;
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  DBUG_ENTER("ha_federatedx::savepoint_rollback");
  
  if (txn)
    error= txn->sp_rollback((ulong *) sv);

  DBUG_RETURN(error);
}


int ha_federatedx::savepoint_release(handlerton *hton, MYSQL_THD thd, void *sv)
{
  int error= 0;
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  DBUG_ENTER("ha_federatedx::savepoint_release");
  
  if (txn)
    error= txn->sp_release((ulong *) sv);

  DBUG_RETURN(error);
}


int ha_federatedx::commit(handlerton *hton, MYSQL_THD thd, bool all)
{
  int return_val;
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  DBUG_ENTER("ha_federatedx::commit");

  if (all)
    return_val= txn->txn_commit();
  else
    return_val= txn->stmt_commit();    
  
  DBUG_PRINT("info", ("error val: %d", return_val));
  DBUG_RETURN(return_val);
}


int ha_federatedx::rollback(handlerton *hton, MYSQL_THD thd, bool all)
{
  int return_val;
  federatedx_txn *txn= (federatedx_txn *) thd_get_ha_data(thd, hton);
  DBUG_ENTER("ha_federatedx::rollback");

  if (all)
    return_val= txn->txn_rollback();
  else
    return_val= txn->stmt_rollback();

  DBUG_PRINT("info", ("error val: %d", return_val));
  DBUG_RETURN(return_val);
}

struct st_mysql_storage_engine federatedx_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(federated)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &federatedx_storage_engine,
  "FEDERATED",
  "Patrick Galbraith",
  "FederatedX pluggable storage engine",
  PLUGIN_LICENSE_GPL,
  federatedx_db_init, /* Plugin Init */
  federatedx_done, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
