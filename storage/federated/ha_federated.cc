/* Copyright (c) 2004, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*

  MySQL Federated Storage Engine

  ha_federated.cc - MySQL Federated Storage Engine
  Patrick Galbraith and Brian Aker, 2004

  This is a handler which uses a foreign database as the data file, as
  opposed to a handler like MyISAM, which uses .MYD files locally.

  How this handler works
  ----------------------------------
  Normal database files are local and as such: You create a table called
  'users', a file such as 'users.MYD' is created. A handler reads, inserts,
  deletes, updates data in this file. The data is stored in particular format,
  so to read, that data has to be parsed into fields, to write, fields have to
  be stored in this format to write to this data file.

  With MySQL Federated storage engine, there will be no local files
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

  As of 5.1 (See worklog #3031), federated now allows you to use a non-url
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

  CREATE TABLE federated.t1 (
    `id` int(20) NOT NULL,
    `name` varchar(64) NOT NULL default ''
    )
  ENGINE="FEDERATED" DEFAULT CHARSET=latin1
  CONNECTION='server_one';

  So, this will have been the equivalent of

  CONNECTION="mysql://root@127.0.0.1:3306/db1/t1"

  Then, we can also change the server to point to a new schema:

  ALTER SERVER 'server_one' options(DATABASE 'db2');

  All subsequent calls will now be against db2.t1! Guess what? You don't
  have to perform an alter table!

  This connection="connection string" is necessary for the handler to be
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
    federated storage engine type on the REMOTE end, MAKE SURE [ :) ] That
    the table you connect to IS NOT a table pointing BACK to your ORIGINAL
    table! You know  and have heard the screeching of audio feedback? You
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

   Method calls

   A two column table, with one record:

   (SELECT)

   "SELECT * FROM foo"
    ha_federated::info
    ha_federated::scan_time:
    ha_federated::rnd_init: share->select_query SELECT * FROM foo
    ha_federated::extra

    <for every row of data retrieved>
    ha_federated::rnd_next
    ha_federated::convert_row_to_internal_format
    ha_federated::rnd_next
    </for every row of data retrieved>

    ha_federated::rnd_end
    ha_federated::extra
    ha_federated::reset

    (INSERT)

    "INSERT INTO foo (id, ts) VALUES (2, now());"

    ha_federated::write_row

    ha_federated::reset

    (UPDATE)

    "UPDATE foo SET ts = now() WHERE id = 1;"

    ha_federated::index_init
    ha_federated::index_read
    ha_federated::index_read_idx
    ha_federated::rnd_next
    ha_federated::convert_row_to_internal_format
    ha_federated::update_row

    ha_federated::extra
    ha_federated::extra
    ha_federated::extra
    ha_federated::external_lock
    ha_federated::reset


    How do I use this handler?
    --------------------------
    First of all, you need to build this storage engine:

      ./configure --with-federated-storage-engine
      make

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
       ENGINE="FEDERATED"
       DEFAULT CHARSET=latin1
       CONNECTION='mysql://root@127.0.0.1:9306/federated/test_federated';

   Notice the "COMMENT" and "ENGINE" field? This is where you
   respectively set the engine type, "FEDERATED" and foreign
   host information, this being the database your 'client' database
   will connect to and use as the "data file". Obviously, the foreign
   database is running on port 9306, so you want to start up your other
   database so that it is indeed on port 9306, and your federated
   database on a port other than that. In my setup, I use port 5554
   for federated, and port 5555 for the foreign database.

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
    except that it is not using the federated handler and does
    not need the URL.


    How to see the handler in action
    --------------------------------

    When developing this handler, I compiled the federated database with
    debugging:

    ./configure --with-federated-storage-engine
    --prefix=/home/mysql/mysql-build/federated/ --with-debug

    Once compiled, I did a 'make install' (not for the purpose of installing
    the binary, but to install all the files the binary expects to see in the
    directory I specified in the build with --prefix,
    "/home/mysql/mysql-build/federated".

    Then, I started the foreign server:

    /usr/local/mysql/bin/mysqld_safe
    --user=mysql --log=/tmp/mysqld.5555.log -P 5555

    Then, I went back to the directory containing the newly compiled mysqld,
    <builddir>/sql/, started up gdb:

    gdb ./mysqld

    Then, withn the (gdb) prompt:
    (gdb) run --gdb --port=5554 --socket=/tmp/mysqld.5554 --debug

    Next, I open several windows for each:

    1. Tail the debug trace: tail -f /tmp/mysqld.trace|grep ha_fed
    2. Tail the SQL calls to the foreign database: tail -f /tmp/mysqld.5555.log
    3. A window with a client open to the federated server on port 5554
    4. A window with a client open to the federated server on port 5555

    I would create a table on the client to the foreign server on port
    5555, and then to the federated server on port 5554. At this point,
    I would run whatever queries I wanted to on the federated server,
    just always remembering that whatever changes I wanted to make on
    the table, or if I created new tables, that I would have to do that
    on the foreign server.

    Another thing to look for is 'show variables' to show you that you have
    support for federated handler support:

    show variables like '%federat%'

    and:

    show storage engines;

    Both should display the federated storage handler.


    Testing
    -------

    There is a test for MySQL Federated Storage Handler in ./mysql-test/t,
    federatedd.test It starts both a slave and master database using
    the same setup that the replication tests use, with the exception that
    it turns off replication, and sets replication to ignore the test tables.
    After ensuring that you actually do have support for the federated storage
    handler, numerous queries/inserts/updates/deletes are run, many derived
    from the MyISAM tests, plus some other tests which were meant to reveal
    any issues that would be most likely to affect this handler. All tests
    should work! ;)

    To run these tests, go into ./mysql-test (based in the directory you
    built the server in)

    ./mysql-test-run federated

    To run the test, or if you want to run the test and have debug info:

    ./mysql-test-run --debug federated

    This will run the test in debug mode, and you can view the trace and
    log files in the ./mysql-test/var/log directory

    ls -l mysql-test/var/log/
    -rw-r--r--  1 patg  patg        17  4 Dec 12:27 current_test
    -rw-r--r--  1 patg  patg       692  4 Dec 12:52 manager.log
    -rw-rw----  1 patg  patg     21246  4 Dec 12:51 master-bin.000001
    -rw-rw----  1 patg  patg        68  4 Dec 12:28 master-bin.index
    -rw-r--r--  1 patg  patg      1620  4 Dec 12:51 master.err
    -rw-rw----  1 patg  patg     23179  4 Dec 12:51 master.log
    -rw-rw----  1 patg  patg  16696550  4 Dec 12:51 master.trace
    -rw-r--r--  1 patg  patg         0  4 Dec 12:28 mysqltest-time
    -rw-r--r--  1 patg  patg   2024051  4 Dec 12:51 mysqltest.trace
    -rw-rw----  1 patg  patg     94992  4 Dec 12:51 slave-bin.000001
    -rw-rw----  1 patg  patg        67  4 Dec 12:28 slave-bin.index
    -rw-rw----  1 patg  patg       249  4 Dec 12:52 slave-relay-bin.000003
    -rw-rw----  1 patg  patg        73  4 Dec 12:28 slave-relay-bin.index
    -rw-r--r--  1 patg  patg      1349  4 Dec 12:51 slave.err
    -rw-rw----  1 patg  patg     96206  4 Dec 12:52 slave.log
    -rw-rw----  1 patg  patg  15706355  4 Dec 12:51 slave.trace
    -rw-r--r--  1 patg  patg         0  4 Dec 12:51 warnings

    Of course, again, you can tail the trace log:

    tail -f mysql-test/var/log/master.trace |grep ha_fed

    As well as the slave query log:

    tail -f mysql-test/var/log/slave.log

    Files that comprise the test suit
    ---------------------------------
    mysql-test/t/federated.test
    mysql-test/r/federated.result
    mysql-test/r/have_federated_db.require
    mysql-test/include/have_federated_db.inc


    Other tidbits
    -------------

    These were the files that were modified or created for this
    Federated handler to work, in 5.0:

    ./configure.in
    ./sql/Makefile.am
    ./config/ac_macros/ha_federated.m4
    ./sql/handler.cc
    ./sql/mysqld.cc
    ./sql/set_var.cc
    ./sql/field.h
    ./sql/sql_string.h
    ./mysql-test/mysql-test-run(.sh)
    ./mysql-test/t/federated.test
    ./mysql-test/r/federated.result
    ./mysql-test/r/have_federated_db.require
    ./mysql-test/include/have_federated_db.inc
    ./sql/ha_federated.cc
    ./sql/ha_federated.h

    In 5.1

    my:~/mysql-build/mysql-5.1-bkbits patg$ ls storage/federated/
    CMakeLists.txt                  Makefile.in
  ha_federated.h                  plug.in Makefile                        SCCS
  libfederated.a Makefile.am                     ha_federated.cc
  libfederated_a-ha_federated.o

*/

#define MYSQL_SERVER 1
#include "storage/federated/ha_federated.h"

#include <mysql/plugin.h>
#include <stdlib.h>
#include <algorithm>
#include <string>

#include "lex_string.h"
#include "map_helpers.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "myisam.h"  // TT_USEFRM
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/strings/my_strtoll10.h"
#include "sql/current_thd.h"
#include "sql/key.h"     // key_copy
#include "sql/mysqld.h"  // my_localhost
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_servers.h"  // FOREIGN_SERVER, get_server_by_name
#include "sql_common.h"
#include "string_with_len.h"
#include "strmake.h"
#include "template_utils.h"
#include "unsafe_string_append.h"

using std::max;
using std::min;
using std::string;

static PSI_memory_key fe_key_memory_federated_share;

/* Variables for federated share methods */
static malloc_unordered_map<string, FEDERATED_SHARE *> federated_open_tables{
    fe_key_memory_federated_share};   // To track open tables
mysql_mutex_t federated_mutex;        // To init the hash
static char ident_quote_char = '`';   // Character for quoting
                                      // identifiers
static char value_quote_char = '\'';  // Character for quoting
                                      // literals
static const int bulk_padding = 64;   // bytes "overhead" in packet

/* Variables used when chopping off trailing characters */
static const uint sizeof_trailing_comma = sizeof(", ") - 1;
static const uint sizeof_trailing_and = sizeof(" AND ") - 1;
static const uint sizeof_trailing_where = sizeof(" WHERE ") - 1;

/* Static declaration for handerton */
static handler *federated_create_handler(handlerton *hton, TABLE_SHARE *table,
                                         bool partitioned, MEM_ROOT *mem_root);
static int federated_commit(handlerton *hton, THD *thd, bool all);
static int federated_rollback(handlerton *hton, THD *thd, bool all);

/* Federated storage engine handlerton */

static handler *federated_create_handler(handlerton *hton, TABLE_SHARE *table,
                                         bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_federated(hton, table);
}

static PSI_mutex_key fe_key_mutex_federated, fe_key_mutex_FEDERATED_SHARE_mutex;

#ifdef HAVE_PSI_MUTEX_INTERFACE
/* clang-format off */
static PSI_mutex_info all_federated_mutexes[]=
{
  { &fe_key_mutex_federated, "federated", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &fe_key_mutex_FEDERATED_SHARE_mutex, "FEDERATED_SHARE::mutex", 0, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */
#endif /* HAVE_PSI_MUTEX_INTERFACE */

#ifdef HAVE_PSI_MEMORY_INTERFACE
static PSI_memory_info all_federated_memory[] = {
    {&fe_key_memory_federated_share, "FEDERATED_SHARE",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME}};
#endif /* HAVE_PSI_MEMORY_INTERFACE */

#ifdef HAVE_PSI_INTERFACE
static void init_federated_psi_keys(void) {
  const char *category [[maybe_unused]] = "federated";
  int count [[maybe_unused]];

#ifdef HAVE_PSI_MUTEX_INTERFACE
  count = static_cast<int>(array_elements(all_federated_mutexes));
  mysql_mutex_register(category, all_federated_mutexes, count);
#endif /* HAVE_PSI_MUTEX_INTERFACE */

#ifdef HAVE_PSI_MEMORY_INTERFACE
  count = static_cast<int>(array_elements(all_federated_memory));
  mysql_memory_register(category, all_federated_memory, count);
#endif /* HAVE_PSI_MEMORY_INTERFACE */
}
#endif /* HAVE_PSI_INTERFACE */

/*
  Initialize the federated handler.

  SYNOPSIS
    federated_db_init()
    p		Handlerton

  RETURN
    false       OK
    true        Error
*/

static int federated_db_init(void *p) {
  DBUG_TRACE;

#ifdef HAVE_PSI_INTERFACE
  init_federated_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

  handlerton *federated_hton = (handlerton *)p;
  federated_hton->state = SHOW_OPTION_YES;
  federated_hton->db_type = DB_TYPE_FEDERATED_DB;
  federated_hton->commit = federated_commit;
  federated_hton->rollback = federated_rollback;
  federated_hton->create = federated_create_handler;
  federated_hton->flags = HTON_ALTER_NOT_SUPPORTED | HTON_NO_PARTITION;

  /*
    Support for transactions disabled until WL#2952 fixes it.
        We do it like this to avoid "defined but not used" compiler warnings.
  */
  federated_hton->commit = nullptr;
  federated_hton->rollback = nullptr;

  if (mysql_mutex_init(fe_key_mutex_federated, &federated_mutex,
                       MY_MUTEX_INIT_FAST))
    goto error;

  return false;

  mysql_mutex_destroy(&federated_mutex);
error:
  return true;
}

/*
  Release the federated handler.

  SYNOPSIS
    federated_db_end()

  RETURN
    false       OK
*/

static int federated_done(void *) {
  mysql_mutex_destroy(&federated_mutex);

  return 0;
}

/**
  @brief Append identifiers to the string.

  @param[in,out] string	The target string.
  @param[in] name       Identifier name
  @param[in] length     Length of identifier name in bytes
  @param[in] quote_char Quote char to use for quoting identifier.

  @return Operation Status
  @retval false OK
  @retval true  There was an error appending to the string.

  @note This function is based upon the append_identifier() function
        in sql_show.cc except that quoting always occurs.
*/

static bool append_ident(String *string, const char *name, size_t length,
                         const char quote_char) {
  bool result = true;
  DBUG_TRACE;

  if (quote_char) {
    string->reserve(length * 2 + 2);

    if ((result = string->append(&quote_char, 1, system_charset_info)))
      goto err;

    uint clen = 0;

    for (const char *name_end = name + length; name < name_end; name += clen) {
      const char c = *name;

      if (!(clen = my_mbcharlen(system_charset_info, c))) goto err;

      if (clen == 1 && c == quote_char &&
          (result = string->append(&quote_char, 1, system_charset_info)))
        goto err;

      if ((result = string->append(name, clen, string->charset()))) goto err;
    }
    result = string->append(&quote_char, 1, system_charset_info);
  } else
    result = string->append(name, length, system_charset_info);

err:
  return result;
}

static int parse_url_error(TABLE *table, int error_num) {
  char buf[FEDERATED_QUERY_BUFFER_SIZE];
  size_t buf_len;
  DBUG_TRACE;

  buf_len = min<size_t>(table->s->connect_string.length,
                        FEDERATED_QUERY_BUFFER_SIZE - 1);
  strmake(buf, table->s->connect_string.str, buf_len);
  my_error(error_num, MYF(0), buf);
  return error_num;
}

/*
  retrieve server object which contains server meta-data
  from the system table given a server's name, set share
  connection parameter members
*/
static int get_connection(MEM_ROOT *mem_root, FEDERATED_SHARE *share) {
  int error_num = ER_FOREIGN_SERVER_DOESNT_EXIST;
  FOREIGN_SERVER *server, server_buffer;
  DBUG_TRACE;

  /*
    get_server_by_name() clones the server if exists and allocates
        copies of strings in the supplied mem_root
  */
  if (!(server = get_server_by_name(mem_root, share->connection_string,
                                    &server_buffer))) {
    DBUG_PRINT("info", ("get_server_by_name returned > 0 error condition!"));
    error_num = ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE;
    goto error;
  }
  DBUG_PRINT("info", ("get_server_by_name returned server at %p", server));

  /*
    Most of these should never be empty strings, error handling will
    need to be implemented. Also, is this the best way to set the share
    members? Is there some allocation needed? In running this code, it works
    except there are errors in the trace file of the share being overrun
    at the address of the share.
  */
  share->server_name_length = server->server_name_length;
  share->server_name = server->server_name;
  share->username = server->username;
  share->password = server->password;
  share->database = server->db;
  share->port = server->port > 0 && server->port < 65536 ? (ushort)server->port
                                                         : MYSQL_PORT;
  share->hostname = server->host;
  if (!(share->socket = server->socket) &&
      !strcmp(share->hostname, my_localhost))
    share->socket = MYSQL_UNIX_ADDR;
  share->scheme = server->scheme;

  DBUG_PRINT("info", ("share->username %s", share->username));
  DBUG_PRINT("info", ("share->password %s", share->password));
  DBUG_PRINT("info", ("share->hostname %s", share->hostname));
  DBUG_PRINT("info", ("share->database %s", share->database));
  DBUG_PRINT("info", ("share->port %d", share->port));
  DBUG_PRINT("info", ("share->socket %s", share->socket));
  return 0;

error:
  my_printf_error(error_num, "server name: '%s' doesn't exist!", MYF(0),
                  share->connection_string);
  return error_num;
}

/*
  Parse connection info from table->s->connect_string

  SYNOPSIS
    parse_url()
    mem_root            MEM_ROOT pointer for memory allocation
    share               pointer to FEDERATED share
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
    ENGINE="FEDERATED"
    CONNECTION="mysql://joe:joespass@192.168.1.111:9308/federated/testtable";

  CREATE TABLE t2 (
    id int(4) NOT NULL auto_increment,
    name varchar(32) NOT NULL,
    PRIMARY KEY(id)
    ) ENGINE="FEDERATED" CONNECTION="my_conn";

  ***IMPORTANT***
  Currently, the Federated Storage Engine only supports connecting to another
  MySQL Database ("scheme" of "mysql"). Connections using JDBC as well as
  other connectors are in the planning stage.


  'password' and 'port' are both optional.

  RETURN VALUE
    0           success
    error_num   particular error code

*/

static int parse_url(MEM_ROOT *mem_root, FEDERATED_SHARE *share, TABLE *table,
                     uint table_create_flag) {
  uint error_num =
      (table_create_flag ? ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE
                         : ER_FOREIGN_DATA_STRING_INVALID);
  DBUG_TRACE;

  share->port = 0;
  share->socket = nullptr;
  DBUG_PRINT("info", ("share at %p", share));
  DBUG_PRINT("info", ("Length: %u", (uint)table->s->connect_string.length));
  DBUG_PRINT("info", ("String: '%.*s'", (int)table->s->connect_string.length,
                      table->s->connect_string.str));
  share->connection_string = strmake_root(
      mem_root, table->s->connect_string.str, table->s->connect_string.length);

  DBUG_PRINT("info", ("parse_url alloced share->connection_string %p",
                      share->connection_string));

  DBUG_PRINT("info", ("share->connection_string %s", share->connection_string));
  /*
    No :// or @ in connection string. Must be a straight connection name of
    either "servername" or "servername/tablename"
  */
  if ((!strstr(share->connection_string, "://") &&
       (!strchr(share->connection_string, '@')))) {
    DBUG_PRINT("info", ("share->connection_string %s internal format \
                share->connection_string %p",
                        share->connection_string, share->connection_string));

    /* ok, so we do a little parsing, but not completely! */
    share->parsed = false;
    /*
      If there is a single '/' in the connection string, this means the user is
      specifying a table name
    */

    if ((share->table_name = strchr(share->connection_string, '/'))) {
      share->connection_string[share->table_name - share->connection_string] =
          '\0';
      share->table_name++;
      share->table_name_length = (uint)strlen(share->table_name);

      DBUG_PRINT("info",
                 ("internal format, parsed table_name share->connection_string \
                  %s share->table_name %s",
                  share->connection_string, share->table_name));

      /*
        there better not be any more '/'s !
      */
      if (strchr(share->table_name, '/')) goto error;

    }
    /*
      otherwise, straight server name, use tablename of federated table
      as remote table name
    */
    else {
      /*
        connection specifies everything but, resort to
        expecting remote and foreign table names to match
      */
      share->table_name = strmake_root(
          mem_root, table->s->table_name.str,
          (share->table_name_length = table->s->table_name.length));
      DBUG_PRINT(
          "info",
          ("internal format, default table_name share->connection_string \
                  %s share->table_name %s",
           share->connection_string, share->table_name));
    }

    if ((error_num = get_connection(mem_root, share))) goto error;
  } else {
    share->parsed = true;
    // Add a null for later termination of table name
    share->connection_string[table->s->connect_string.length] = 0;
    share->scheme = share->connection_string;
    DBUG_PRINT("info", ("parse_url alloced share->scheme %p", share->scheme));

    /*
      remove addition of null terminator and store length
      for each string  in share
    */
    if (!(share->username = strstr(share->scheme, "://"))) goto error;
    share->scheme[share->username - share->scheme] = '\0';

    if (strcmp(share->scheme, "mysql") != 0) goto error;

    share->username += 3;

    if (!(share->hostname = strchr(share->username, '@'))) goto error;

    share->username[share->hostname - share->username] = '\0';
    share->hostname++;

    if ((share->password = strchr(share->username, ':'))) {
      share->username[share->password - share->username] = '\0';
      share->password++;
      share->username = share->username;
      /* make sure there isn't an extra / or @ */
      if ((strchr(share->password, '/') || strchr(share->hostname, '@')))
        goto error;
      /*
        Found that if the string is:
        user:@hostname:port/db/table
        Then password is a null string, so set to NULL
      */
      if (share->password[0] == '\0') share->password = nullptr;
    } else
      share->username = share->username;

    /* make sure there isn't an extra / or @ */
    if ((strchr(share->username, '/')) || (strchr(share->hostname, '@')))
      goto error;

    if (!(share->database = strchr(share->hostname, '/'))) goto error;
    share->hostname[share->database - share->hostname] = '\0';
    share->database++;

    if ((share->sport = strchr(share->hostname, ':'))) {
      share->hostname[share->sport - share->hostname] = '\0';
      share->sport++;
      if (share->sport[0] == '\0')
        share->sport = nullptr;
      else
        share->port = atoi(share->sport);
    }

    if (!(share->table_name = strchr(share->database, '/'))) goto error;
    share->database[share->table_name - share->database] = '\0';
    share->table_name++;

    share->table_name_length = strlen(share->table_name);

    /* make sure there's not an extra / */
    if ((strchr(share->table_name, '/'))) goto error;

    /*
      If hostname is omitted, we set it to NULL. According to
      mysql_real_connect() manual:
      The value of host may be either a hostname or an IP address.
      If host is NULL or the string "localhost", a connection to the
      local host is assumed.
    */
    if (share->hostname[0] == '\0') share->hostname = nullptr;
  }

  if (!share->port) {
    if (!share->hostname || strcmp(share->hostname, my_localhost) == 0)
      share->socket = MYSQL_UNIX_ADDR;
    else
      share->port = MYSQL_PORT;
  }

  DBUG_PRINT("info",
             ("scheme: %s  username: %s  password: %s \
               hostname: %s  port: %d  db: %s  tablename: %s",
              share->scheme, share->username, share->password, share->hostname,
              share->port, share->database, share->table_name));

  return 0;

error:
  return parse_url_error(table, error_num);
}

/*****************************************************************************
** FEDERATED tables
*****************************************************************************/

ha_federated::ha_federated(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg),
      mysql(nullptr),
      stored_result(nullptr),
      results(fe_key_memory_federated_share) {
  trx_next = nullptr;
  memset(&bulk_insert, 0, sizeof(bulk_insert));
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

uint ha_federated::convert_row_to_internal_format(uchar *record, MYSQL_ROW row,
                                                  MYSQL_RES *result) {
  ulong *lengths;
  Field **field;
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_TRACE;

  lengths = mysql_fetch_lengths(result);

  // Clear BLOB data from the previous row.
  m_blob_root.ClearForReuse();

  for (field = table->field; *field; field++, row++, lengths++) {
    /*
      index variable to move us through the row at the
      same iterative step as the field
    */
    ptrdiff_t old_ptr;
    old_ptr = (ptrdiff_t)(record - table->record[0]);
    (*field)->move_field_offset(old_ptr);
    if (!*row) {
      (*field)->set_null();
      (*field)->reset();
    } else {
      if (bitmap_is_set(table->read_set, (*field)->field_index())) {
        (*field)->set_notnull();

        // Field_json::store expects the incoming data to be in utf8mb4_bin, so
        // we override the character set in those cases.
        if ((*field)->type() == MYSQL_TYPE_JSON) {
          (*field)->store(*row, *lengths, &my_charset_utf8mb4_bin);
        } else {
          (*field)->store(*row, *lengths, &my_charset_bin);
        }

        if ((*field)->is_flag_set(BLOB_FLAG)) {
          Field_blob *blob_field = down_cast<Field_blob *>(*field);
          const size_t length = blob_field->get_length();
          // BLOB data is not stored inside record. It only contains a
          // pointer to it. Copy the BLOB data into a separate memory
          // area so that it is not overwritten by subsequent calls to
          // Field::store() after moving the offset.
          if (length > 0) {
            unsigned char *new_blob = new (&m_blob_root) unsigned char[length];
            if (new_blob == nullptr) return HA_ERR_OUT_OF_MEM;
            memcpy(new_blob, blob_field->get_blob_data(), length);
            blob_field->set_ptr(length, new_blob);
          }
        }
      }
    }
    (*field)->move_field_offset(-old_ptr);
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return 0;
}

static bool emit_key_part_name(String *to, KEY_PART_INFO *part) {
  DBUG_TRACE;
  if (append_ident(to, part->field->field_name, strlen(part->field->field_name),
                   ident_quote_char))
    return true;  // Out of memory
  return false;
}

static bool emit_key_part_element(String *to, KEY_PART_INFO *part,
                                  bool needs_quotes, bool is_like, uchar *ptr,
                                  uint len) {
  Field *field = part->field;
  DBUG_TRACE;

  if (needs_quotes && to->append(STRING_WITH_LEN("'"))) return true;

  if (part->type == HA_KEYTYPE_BIT) {
    char buff[STRING_BUFFER_USUAL_SIZE], *buf = buff;

    *buf++ = '0';
    *buf++ = 'x';
    buf = octet2hex(buf, (char *)ptr, len);
    if (to->append((char *)buff, (uint)(buf - buff))) return true;
  } else if (part->key_part_flag & HA_BLOB_PART) {
    String blob;
    const uint blob_length = uint2korr(ptr);
    blob.set_quick((char *)ptr + HA_KEY_BLOB_LENGTH, blob_length,
                   &my_charset_bin);
    if (append_escaped(to, &blob)) return true;
  } else if (part->key_part_flag & HA_VAR_LENGTH_PART) {
    String varchar;
    const uint var_length = uint2korr(ptr);
    varchar.set_quick((char *)ptr + HA_KEY_BLOB_LENGTH, var_length,
                      &my_charset_bin);
    if (append_escaped(to, &varchar)) return true;
  } else {
    char strbuff[MAX_FIELD_WIDTH];
    String str(strbuff, sizeof(strbuff), part->field->charset()), *res;

    res = field->val_str(&str, ptr);

    if (field->result_type() == STRING_RESULT) {
      if (append_escaped(to, res)) return true;
    } else if (to->append(res->ptr(), res->length()))
      return true;
  }

  if (is_like && to->append(STRING_WITH_LEN("%"))) return true;

  if (needs_quotes && to->append(STRING_WITH_LEN("'"))) return true;

  return false;
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

1. Need logic to determine if a key is min or max when the flag is
HA_READ_AFTER_KEY, and handle appending correct operator accordingly

2. Need a boolean flag to pass to create_where_from_key, used in the
switch statement. Add 1 to the flag if:
  - start key flag is HA_READ_KEY_EXACT and the end key is NULL

*/

bool ha_federated::create_where_from_key(String *to, KEY *key_info,
                                         const key_range *start_key,
                                         const key_range *end_key,
                                         bool from_records_in_range,
                                         bool eq_range_arg) {
  const bool both_not_null =
      (start_key != nullptr && end_key != nullptr) ? true : false;
  uchar *ptr;
  uint remainder, length;
  char tmpbuff[FEDERATED_QUERY_BUFFER_SIZE];
  String tmp(tmpbuff, sizeof(tmpbuff), system_charset_info);
  const key_range *ranges[2] = {start_key, end_key};
  my_bitmap_map *old_map;
  DBUG_TRACE;

  tmp.length(0);
  if (start_key == nullptr && end_key == nullptr) return true;

  old_map = dbug_tmp_use_all_columns(table, table->write_set);
  for (uint i = 0; i <= 1; i++) {
    bool needs_quotes;
    KEY_PART_INFO *key_part;
    if (ranges[i] == nullptr) continue;

    if (both_not_null) {
      if (i > 0)
        tmp.append(STRING_WITH_LEN(") AND ("));
      else
        tmp.append(STRING_WITH_LEN(" ("));
    }

    for (key_part = key_info->key_part,
        remainder = key_info->user_defined_key_parts,
        length = ranges[i]->length, ptr = const_cast<uchar *>(ranges[i]->key);
         ; remainder--, key_part++) {
      Field *field = key_part->field;
      const uint store_length = key_part->store_length;
      const uint part_length = min(store_length, length);
      needs_quotes = field->str_needs_quotes();
      DBUG_DUMP("key, start of loop", ptr, length);

      if (key_part->null_bit) {
        if (*ptr++) {
          /*
            We got "IS [NOT] NULL" condition against nullable column. We
            distinguish between "IS NOT NULL" and "IS NULL" by flag. For
            "IS NULL", flag is set to HA_READ_KEY_EXACT.
          */
          if (emit_key_part_name(&tmp, key_part) ||
              (ranges[i]->flag == HA_READ_KEY_EXACT
                   ? tmp.append(STRING_WITH_LEN(" IS NULL "))
                   : tmp.append(STRING_WITH_LEN(" IS NOT NULL "))))
            goto err;
          /*
            We need to adjust pointer and length to be prepared for next
            key part. As well as check if this was last key part.
          */
          goto prepare_for_next_key_part;
        }
      }

      if (tmp.append(STRING_WITH_LEN(" ("))) goto err;

      switch (ranges[i]->flag) {
        case HA_READ_KEY_EXACT:
          DBUG_PRINT("info", ("federated HA_READ_KEY_EXACT %d", i));
          if (store_length >= length || !needs_quotes ||
              key_part->type == HA_KEYTYPE_BIT ||
              field->result_type() != STRING_RESULT) {
            if (emit_key_part_name(&tmp, key_part)) goto err;

            if (from_records_in_range) {
              if (tmp.append(STRING_WITH_LEN(" >= "))) goto err;
            } else {
              if (tmp.append(STRING_WITH_LEN(" = "))) goto err;
            }

            if (emit_key_part_element(&tmp, key_part, needs_quotes, false, ptr,
                                      part_length))
              goto err;
          } else {
            /* LIKE */
            if (emit_key_part_name(&tmp, key_part) ||
                tmp.append(STRING_WITH_LEN(" LIKE ")) ||
                emit_key_part_element(&tmp, key_part, needs_quotes, true, ptr,
                                      part_length))
              goto err;
          }
          break;
        case HA_READ_AFTER_KEY:
          if (eq_range_arg) {
            if (tmp.append("1=1"))  // Dummy
              goto err;
            break;
          }
          DBUG_PRINT("info", ("federated HA_READ_AFTER_KEY %d", i));
          if ((store_length >= length) || (i > 0)) /* for all parts of end key*/
          {
            if (emit_key_part_name(&tmp, key_part)) goto err;

            if (i > 0) /* end key */
            {
              if (tmp.append(STRING_WITH_LEN(" <= "))) goto err;
            } else /* start key */
            {
              if (tmp.append(STRING_WITH_LEN(" > "))) goto err;
            }

            if (emit_key_part_element(&tmp, key_part, needs_quotes, false, ptr,
                                      part_length)) {
              goto err;
            }
            break;
          }
          [[fallthrough]];
        case HA_READ_KEY_OR_NEXT:
          DBUG_PRINT("info", ("federated HA_READ_KEY_OR_NEXT %d", i));
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" >= ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, false, ptr,
                                    part_length))
            goto err;
          break;
        case HA_READ_BEFORE_KEY:
          DBUG_PRINT("info", ("federated HA_READ_BEFORE_KEY %d", i));
          if (store_length >= length) {
            if (emit_key_part_name(&tmp, key_part) ||
                tmp.append(STRING_WITH_LEN(" < ")) ||
                emit_key_part_element(&tmp, key_part, needs_quotes, false, ptr,
                                      part_length))
              goto err;
            break;
          }
          [[fallthrough]];
        case HA_READ_KEY_OR_PREV:
          DBUG_PRINT("info", ("federated HA_READ_KEY_OR_PREV %d", i));
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" <= ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, false, ptr,
                                    part_length))
            goto err;
          break;
        default:
          DBUG_PRINT("info", ("cannot handle flag %d", ranges[i]->flag));
          goto err;
      }
      if (tmp.append(STRING_WITH_LEN(") "))) goto err;

    prepare_for_next_key_part:
      if (store_length >= length) break;
      DBUG_PRINT("info", ("remainder %d", remainder));
      assert(remainder > 1);
      length -= store_length;
      /*
        For nullable columns, null-byte is already skipped before, that is
        ptr was incremented by 1. Since store_length still counts null-byte,
        we need to subtract 1 from store_length.
      */
      ptr += store_length - (key_part->null_bit ? 1 : 0);
      if (tmp.append(STRING_WITH_LEN(" AND "))) goto err;

      DBUG_PRINT("info",
                 ("create_where_from_key WHERE clause: %s", tmp.c_ptr_quick()));
    }
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);

  if (both_not_null)
    if (tmp.append(STRING_WITH_LEN(") "))) return true;

  if (to->append(STRING_WITH_LEN(" WHERE "))) return true;

  if (to->append(tmp)) return true;

  return false;

err:
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return true;
}

/*
  Example of simple lock controls. The "share" it creates is structure we will
  pass to each federated handler. Do you have to have one of these? Well, you
  have pieces that are used for locking, and they are needed to function.
*/

static FEDERATED_SHARE *get_share(const char *table_name, TABLE *table) {
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  Field **field;
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  FEDERATED_SHARE *share = nullptr, tmp_share;
  DBUG_TRACE;

  /*
    In order to use this string, we must first zero it's length,
    or it will contain garbage
  */
  query.length(0);

  MEM_ROOT mem_root(fe_key_memory_federated_share, 256);

  mysql_mutex_lock(&federated_mutex);

  tmp_share.share_key = table_name;
  tmp_share.share_key_length = (uint)strlen(table_name);
  if (parse_url(&mem_root, &tmp_share, table, 0)) goto error;

  {
    /* TODO: change tmp_share.scheme to LEX_STRING object */
    auto it = federated_open_tables.find(table_name);
    if (it == federated_open_tables.end()) {
      query.set_charset(system_charset_info);
      query.append(STRING_WITH_LEN("SELECT "));
      for (field = table->field; *field; field++) {
        append_ident(&query, (*field)->field_name, strlen((*field)->field_name),
                     ident_quote_char);
        query.append(STRING_WITH_LEN(", "));
      }
      /* chops off trailing comma */
      query.length(query.length() - sizeof_trailing_comma);

      query.append(STRING_WITH_LEN(" FROM "));

      append_ident(&query, tmp_share.table_name, tmp_share.table_name_length,
                   ident_quote_char);

      if (!(share = (FEDERATED_SHARE *)memdup_root(
                &mem_root, (char *)&tmp_share, sizeof(*share))) ||
          !(share->select_query = (char *)strmake_root(&mem_root, query.ptr(),
                                                       query.length() + 1)))
        goto error;

      share->use_count = 0;
      share->mem_root = std::move(mem_root);

      DBUG_PRINT("info", ("share->select_query %s", share->select_query));

      federated_open_tables.emplace(table_name, share);
      thr_lock_init(&share->lock);
      mysql_mutex_init(fe_key_mutex_FEDERATED_SHARE_mutex, &share->mutex,
                       MY_MUTEX_INIT_FAST);
    } else {
      share = it->second;
    }
  }

  share->use_count++;
  mysql_mutex_unlock(&federated_mutex);

  return share;

error:
  mysql_mutex_unlock(&federated_mutex);
  return nullptr;
}

/*
  Free lock controls. We call this whenever we close a table.
  If the table had the last reference to the share then we
  free memory associated with it.
*/

static int free_share(FEDERATED_SHARE *share) {
  DBUG_TRACE;

  mysql_mutex_lock(&federated_mutex);
  if (!--share->use_count) {
    federated_open_tables.erase(share->share_key);
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->mutex);
    MEM_ROOT mem_root = std::move(share->mem_root);
    mem_root.Clear();
  }
  mysql_mutex_unlock(&federated_mutex);

  return 0;
}

ha_rows ha_federated::records_in_range(uint, key_range *, key_range *) {
  /*

  We really want indexes to be used as often as possible, therefore
  we just need to hard-code the return value to a very low number to
  force the issue

*/
  DBUG_TRACE;
  return FEDERATED_RECORDS_IN_RANGE;
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

int ha_federated::open(const char *name, int, uint, const dd::Table *) {
  DBUG_TRACE;

  if (!(share = get_share(name, table))) return 1;
  thr_lock_data_init(&share->lock, &lock, nullptr);

  assert(mysql == nullptr);

  ref_length = sizeof(MYSQL_RES *) + sizeof(MYSQL_ROW_OFFSET);
  DBUG_PRINT("info", ("ref_length: %u", ref_length));

  reset();

  return 0;
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

int ha_federated::close(void) {
  THD *thd = current_thd;
  DBUG_TRACE;

  free_result();

  results.clear();

  /*
    Check to verify whether the connection is still alive or not.
    FLUSH TABLES will quit the connection and if connection is broken,
    it will reconnect again and quit silently.
  */
  if (mysql && (!mysql->net.vio || !vio_is_connected(mysql->net.vio)))
    mysql->net.error = 2;

  /* Disconnect from mysql */
  mysql_close(mysql);
  mysql = nullptr;

  /*
    mysql_close() might return an error if a remote server's gone
    for some reason. If that happens while removing a table from
    the table cache, the error will be propagated to a client even
    if the original query was not issued against the FEDERATED table.
    So, don't propagate errors from mysql_close().
  */
  if (table->in_use && thd != table->in_use) table->in_use->clear_error();

  /*
    Errors from mysql_close() are silently ignored for flush tables.
    Close the connection silently.
  */
  if (thd && thd->lex->sql_command == SQLCOM_FLUSH) thd->clear_error();

  return free_share(share);
}

/**
  @brief Construct the INSERT statement.

  @details This method will construct the INSERT statement and appends it to
  the supplied query string buffer.

  @retval false       No error
  @retval true        Failure
*/

bool ha_federated::append_stmt_insert(String *query) {
  char insert_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  Field **field;
  size_t tmp_length;
  bool added_field = false;

  /* The main insert query string */
  String insert_string(insert_buffer, sizeof(insert_buffer), &my_charset_bin);
  DBUG_TRACE;

  insert_string.length(0);

  if (replace_duplicates)
    insert_string.append(STRING_WITH_LEN("REPLACE INTO "));
  else if (ignore_duplicates && !insert_dup_update)
    insert_string.append(STRING_WITH_LEN("INSERT IGNORE INTO "));
  else
    insert_string.append(STRING_WITH_LEN("INSERT INTO "));
  append_ident(&insert_string, share->table_name, share->table_name_length,
               ident_quote_char);
  tmp_length = insert_string.length();
  insert_string.append(STRING_WITH_LEN(" ("));

  /*
    loop through the field pointer array, add any fields to both the values
    list and the fields list that match the current query id
  */
  for (field = table->field; *field; field++) {
    if (bitmap_is_set(table->write_set, (*field)->field_index())) {
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
      added_field = true;
    }
  }

  if (added_field) {
    /* Remove trailing comma. */
    insert_string.length(insert_string.length() - sizeof_trailing_comma);
    insert_string.append(STRING_WITH_LEN(") "));
  } else {
    /* If there were no fields, we don't want to add a closing paren. */
    insert_string.length(tmp_length);
  }

  insert_string.append(STRING_WITH_LEN(" VALUES "));

  return query->append(insert_string);
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

int ha_federated::write_row(uchar *) {
  char values_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char insert_field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  Field **field;
  size_t tmp_length;
  int error = 0;
  bool use_bulk_insert;
  bool auto_increment_update_required = (table->next_number_field != nullptr);

  /* The string containing the values to be added to the insert */
  String values_string(values_buffer, sizeof(values_buffer), &my_charset_bin);
  /* The actual value of the field, to be added to the values_string */
  String insert_field_value_string(insert_field_value_buffer,
                                   sizeof(insert_field_value_buffer),
                                   &my_charset_bin);
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
  DBUG_TRACE;

  values_string.length(0);
  insert_field_value_string.length(0);
  ha_statistic_increment(&System_status_var::ha_write_count);

  /*
    start both our field and field values strings
    We must disable multi-row insert for "INSERT...ON DUPLICATE KEY UPDATE"
    Ignore duplicates is always true when insert_dup_update is true.
    When replace_duplicates == true, we can safely enable multi-row insert.
    When performing multi-row insert, we only collect the columns values for
    the row. The start of the statement is only created when the first
    row is copied in to the bulk_insert string.
  */
  if (!(use_bulk_insert =
            bulk_insert.str && (!insert_dup_update || replace_duplicates)))
    append_stmt_insert(&values_string);

  values_string.append(STRING_WITH_LEN(" ("));
  tmp_length = values_string.length();

  /*
    loop through the field pointer array, add any fields to both the values
    list and the fields list that is part of the write set
  */
  for (field = table->field; *field; field++) {
    if (bitmap_is_set(table->write_set, (*field)->field_index())) {
      if ((*field)->is_null())
        values_string.append(STRING_WITH_LEN(" NULL "));
      else {
        const bool needs_quote = (*field)->str_needs_quotes();
        (*field)->val_str(&insert_field_value_string);
        if (needs_quote) values_string.append(value_quote_char);
        insert_field_value_string.print(&values_string);
        if (needs_quote) values_string.append(value_quote_char);

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
  if (values_string.length() > tmp_length) {
    /* chops off trailing comma */
    values_string.length(values_string.length() - sizeof_trailing_comma);
  }
  /* we always want to append this, even if there aren't any fields */
  values_string.append(STRING_WITH_LEN(") "));

  if (use_bulk_insert) {
    /*
      Send the current bulk insert out if appending the current row would
      cause the statement to overflow the packet size, otherwise set
      auto_increment_update_required to false as no query was executed.
    */
    if (bulk_insert.length + values_string.length() + bulk_padding >
            mysql->net.max_packet_size &&
        bulk_insert.length) {
      error = real_query(bulk_insert.str, bulk_insert.length);
      bulk_insert.length = 0;
    } else
      auto_increment_update_required = false;

    if (bulk_insert.length == 0) {
      char insert_buffer[FEDERATED_QUERY_BUFFER_SIZE];
      String insert_string(insert_buffer, sizeof(insert_buffer),
                           &my_charset_bin);
      insert_string.length(0);
      append_stmt_insert(&insert_string);
      dynstr_append_mem(&bulk_insert, insert_string.ptr(),
                        insert_string.length());
    } else
      dynstr_append_mem(&bulk_insert, ",", 1);

    dynstr_append_mem(&bulk_insert, values_string.ptr(),
                      values_string.length());
  } else {
    error = real_query(values_string.ptr(), values_string.length());
  }

  if (error) {
    return stash_remote_error();
  }
  /*
    If the table we've just written a record to contains an auto_increment
    field, then store the last_insert_id() value from the foreign server
  */
  if (auto_increment_update_required) {
    update_auto_increment();

    /* mysql_insert() uses this for protocol return value */
    table->next_number_field->store(stats.auto_increment_value, true);
  }

  return 0;
}

/**
  @brief Prepares the storage engine for bulk inserts.

  @param[in] rows       estimated number of rows in bulk insert
                        or 0 if unknown.

  @details Initializes memory structures required for bulk insert.
*/

void ha_federated::start_bulk_insert(ha_rows rows) {
  uint page_size;
  DBUG_TRACE;

  dynstr_free(&bulk_insert);

  /**
    We don't bother with bulk-insert semantics when the estimated rows == 1
    The rows value will be 0 if the server does not know how many rows
    would be inserted. This can occur when performing INSERT...SELECT
  */

  if (rows == 1) return;

  /*
    Make sure we have an open connection so that we know the
    maximum packet size.
  */
  if (!mysql && real_connect()) return;

  page_size = (uint)my_getpagesize();

  if (init_dynamic_string(&bulk_insert, nullptr, page_size)) return;

  bulk_insert.length = 0;
}

/**
  @brief End bulk insert.

  @details This method will send any remaining rows to the remote server.
  Finally, it will deinitialize the bulk insert data structure.

  @return Operation status
  @retval       0       No error
  @retval       != 0    Error occurred at remote server. Also sets my_errno.
*/

int ha_federated::end_bulk_insert() {
  int error = 0;
  DBUG_TRACE;

  if (bulk_insert.str && bulk_insert.length) {
    if (real_query(bulk_insert.str, bulk_insert.length))
      error = stash_remote_error();
    else if (table->next_number_field)
      update_auto_increment();
  }

  dynstr_free(&bulk_insert);

  set_my_errno(error);
  return error;
}

/*
  ha_federated::update_auto_increment

  This method ensures that last_insert_id() works properly. What it simply does
  is calls last_insert_id() on the foreign database immediately after insert
  (if the table has an auto_increment field) and sets the insert id via
  thd->insert_id(ID)).
*/
void ha_federated::update_auto_increment(void) {
  THD *thd = current_thd;
  DBUG_TRACE;

  ha_federated::info(HA_STATUS_AUTO);
  thd->first_successful_insert_id_in_cur_stmt = stats.auto_increment_value;
  DBUG_PRINT("info", ("last_insert_id: %ld", (long)stats.auto_increment_value));
}

int ha_federated::optimize(THD *, HA_CHECK_OPT *) {
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_TRACE;

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("OPTIMIZE TABLE "));
  append_ident(&query, share->table_name, share->table_name_length,
               ident_quote_char);

  if (real_query(query.ptr(), query.length())) {
    return stash_remote_error();
  }

  return 0;
}

int ha_federated::repair(THD *, HA_CHECK_OPT *check_opt) {
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_TRACE;

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("REPAIR TABLE "));
  append_ident(&query, share->table_name, share->table_name_length,
               ident_quote_char);
  if (check_opt->flags & T_QUICK) query.append(STRING_WITH_LEN(" QUICK"));
  if (check_opt->flags & T_EXTEND) query.append(STRING_WITH_LEN(" EXTENDED"));
  if (check_opt->sql_flags & TT_USEFRM)
    query.append(STRING_WITH_LEN(" USE_FRM"));

  if (real_query(query.ptr(), query.length())) {
    return stash_remote_error();
  }

  return 0;
}

/*
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in
  it.

  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.

  Currently new_data will not have an updated AUTO_INCREMENT record. You can
  do this for federated by doing the following:

    if (table->next_number_field && record == table->record[0])
      update_auto_increment();

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
*/

int ha_federated::update_row(const uchar *old_data, uchar *) {
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
  const bool has_a_primary_key = (table->s->primary_key != MAX_KEY);

  /*
    buffers for following strings
  */
  char field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  char update_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char where_buffer[FEDERATED_QUERY_BUFFER_SIZE];

  /* Work area for field values */
  String field_value(field_value_buffer, sizeof(field_value_buffer),
                     &my_charset_bin);
  /* stores the update query */
  String update_string(update_buffer, sizeof(update_buffer), &my_charset_bin);
  /* stores the WHERE clause */
  String where_string(where_buffer, sizeof(where_buffer), &my_charset_bin);
  uchar *record = table->record[0];
  DBUG_TRACE;
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
  append_ident(&update_string, share->table_name, share->table_name_length,
               ident_quote_char);
  update_string.append(STRING_WITH_LEN(" SET "));

  /*
    In this loop, we want to match column names to values being inserted
    (while building INSERT statement).

    Iterate through table->field (new data) and share->old_field (old_data)
    using the same index to create an SQL UPDATE statement. New data is
    used to create SET field=value and old data is used to create WHERE
    field=oldvalue
  */

  for (Field **field = table->field; *field; field++) {
    if (bitmap_is_set(table->write_set, (*field)->field_index())) {
      const size_t field_name_length = strlen((*field)->field_name);
      append_ident(&update_string, (*field)->field_name, field_name_length,
                   ident_quote_char);
      update_string.append(STRING_WITH_LEN(" = "));

      if ((*field)->is_null())
        update_string.append(STRING_WITH_LEN(" NULL "));
      else {
        /* otherwise = */
        my_bitmap_map *old_map = tmp_use_all_columns(table, table->read_set);
        const bool needs_quote = (*field)->str_needs_quotes();
        (*field)->val_str(&field_value);
        if (needs_quote) update_string.append(value_quote_char);
        field_value.print(&update_string);
        if (needs_quote) update_string.append(value_quote_char);
        field_value.length(0);
        tmp_restore_column_map(table->read_set, old_map);
      }
      update_string.append(STRING_WITH_LEN(", "));
    }

    if (bitmap_is_set(table->read_set, (*field)->field_index())) {
      const size_t field_name_length = strlen((*field)->field_name);
      append_ident(&where_string, (*field)->field_name, field_name_length,
                   ident_quote_char);
      if ((*field)->is_null_in_record(old_data))
        where_string.append(STRING_WITH_LEN(" IS NULL "));
      else {
        const bool needs_quote = (*field)->str_needs_quotes();
        where_string.append(STRING_WITH_LEN(" = "));

        const bool is_json = (*field)->type() == MYSQL_TYPE_JSON;
        if (is_json) {
          where_string.append("CAST(");
        }

        (*field)->val_str(
            &field_value,
            const_cast<uchar *>(old_data + (*field)->offset(record)));
        if (needs_quote) where_string.append(value_quote_char);
        field_value.print(&where_string);
        if (needs_quote) where_string.append(value_quote_char);

        if (is_json) {
          where_string.append(" AS JSON)");
        }
        field_value.length(0);
      }
      where_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  /* Remove last ', '. This works as there must be at least on updated field */
  update_string.length(update_string.length() - sizeof_trailing_comma);

  if (where_string.length()) {
    /* chop off trailing AND */
    where_string.length(where_string.length() - sizeof_trailing_and);
    update_string.append(STRING_WITH_LEN(" WHERE "));
    update_string.append(where_string);
  }

  /*
    If this table has not a primary key, then we could possibly
    update multiple rows. We want to make sure to only update one!
  */
  if (!has_a_primary_key) update_string.append(STRING_WITH_LEN(" LIMIT 1"));

  if (real_query(update_string.ptr(), update_string.length())) {
    return stash_remote_error();
  }
  return 0;
}

/*
  This will delete a row. 'buf' will contain a copy of the row to be =deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_next() or index call).
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier.
  Keep in mind that the server does not guarantee consecutive deletions.
  ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table information.
  Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select
  it is used for removing duplicates while in insert it is used for REPLACE
  calls.
*/

int ha_federated::delete_row(const uchar *) {
  char delete_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char data_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String delete_string(delete_buffer, sizeof(delete_buffer), &my_charset_bin);
  String data_string(data_buffer, sizeof(data_buffer), &my_charset_bin);
  uint found = 0;
  DBUG_TRACE;

  delete_string.length(0);
  if (ignore_duplicates)
    delete_string.append(STRING_WITH_LEN("DELETE IGNORE FROM "));
  else
    delete_string.append(STRING_WITH_LEN("DELETE FROM "));
  append_ident(&delete_string, share->table_name, share->table_name_length,
               ident_quote_char);
  delete_string.append(STRING_WITH_LEN(" WHERE "));

  for (Field **field = table->field; *field; field++) {
    Field *cur_field = *field;
    found++;
    if (bitmap_is_set(table->read_set, cur_field->field_index())) {
      append_ident(&delete_string, (*field)->field_name,
                   strlen((*field)->field_name), ident_quote_char);
      data_string.length(0);
      if (cur_field->is_null()) {
        delete_string.append(STRING_WITH_LEN(" IS NULL "));
      } else {
        const bool needs_quote = cur_field->str_needs_quotes();
        delete_string.append(STRING_WITH_LEN(" = "));

        const bool is_json = (*field)->type() == MYSQL_TYPE_JSON;
        if (is_json) {
          delete_string.append("CAST(");
        }

        cur_field->val_str(&data_string);
        if (needs_quote) delete_string.append(value_quote_char);
        data_string.print(&delete_string);
        if (needs_quote) delete_string.append(value_quote_char);

        if (is_json) {
          delete_string.append(" AS JSON)");
        }
      }
      delete_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  // Remove trailing AND
  delete_string.length(delete_string.length() - sizeof_trailing_and);
  if (!found)
    delete_string.length(delete_string.length() - sizeof_trailing_where);

  delete_string.append(STRING_WITH_LEN(" LIMIT 1"));
  DBUG_PRINT("info", ("Delete sql: %s", delete_string.c_ptr_quick()));
  if (real_query(delete_string.ptr(), delete_string.length())) {
    return stash_remote_error();
  }
  stats.deleted += (ha_rows)mysql->affected_rows;
  stats.records -= (ha_rows)mysql->affected_rows;
  DBUG_PRINT("info", ("rows deleted %ld  rows deleted for all time %ld",
                      (long)mysql->affected_rows, (long)stats.deleted));

  return 0;
}

int ha_federated::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                     key_part_map keypart_map,
                                     enum ha_rkey_function find_flag) {
  int error = index_init(index, false);
  if (error) return error;
  error = index_read_map(buf, key, keypart_map, find_flag);
  if (!error && stored_result) {
    uchar *dummy_arg = nullptr;
    position(dummy_arg);
  }
  const int error1 = index_end();
  return error ? error : error1;
}

/*
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index. This method, which is called in the case of an SQL statement having
  a WHERE clause on a non-primary key index, simply calls index_read_idx.
*/

int ha_federated::index_read(uchar *buf, const uchar *key, uint key_len,
                             ha_rkey_function find_flag) {
  int rc;
  DBUG_TRACE;

  free_result();
  rc = index_read_idx_with_result_set(buf, active_index, key, key_len,
                                      find_flag, &stored_result);
  return rc;
}

/*
  Positions an index cursor to the index specified in key. Fetches the
  row if any.  This is only used to read whole keys.

  This method is called via index_read in the case of a WHERE clause using
  a primary key index OR is called DIRECTLY when the WHERE clause
  uses a PRIMARY KEY index.

  NOTES
    This uses an internal result set that is deleted before function
    returns.  We need to be able to be calable from ha_rnd_pos()
*/

int ha_federated::index_read_idx(uchar *buf, uint index, const uchar *key,
                                 uint key_len,
                                 enum ha_rkey_function find_flag) {
  int retval;
  MYSQL_RES *mysql_result;
  DBUG_TRACE;

  if ((retval = index_read_idx_with_result_set(buf, index, key, key_len,
                                               find_flag, &mysql_result)))
    return retval;
  mysql_free_result(mysql_result);
  results.pop_back();
  return 0;
}

/*
  Create result set for rows matching query and return first row

  RESULT
    0	ok     In this case *result will contain the result set
    #   error  In this case *result will contain 0
*/

int ha_federated::index_read_idx_with_result_set(uchar *buf, uint index,
                                                 const uchar *key, uint key_len,
                                                 ha_rkey_function find_flag,
                                                 MYSQL_RES **result) {
  int retval;
  char error_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char index_value[STRING_BUFFER_USUAL_SIZE];
  char sql_query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String index_string(index_value, sizeof(index_value), &my_charset_bin);
  String sql_query(sql_query_buffer, sizeof(sql_query_buffer), &my_charset_bin);
  key_range range;
  DBUG_TRACE;

  *result = nullptr;  // In case of errors
  index_string.length(0);
  sql_query.length(0);
  ha_statistic_increment(&System_status_var::ha_read_key_count);

  sql_query.append(share->select_query);

  range.key = key;
  range.length = key_len;
  range.flag = find_flag;
  create_where_from_key(&index_string, &table->key_info[index], &range, nullptr,
                        false, false);
  sql_query.append(index_string);

  if (real_query(sql_query.ptr(), sql_query.length())) {
    sprintf(error_buffer, "error: %d '%s'", mysql_errno(mysql),
            mysql_error(mysql));
    retval = ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  if (!(*result = store_result(mysql))) {
    retval = HA_ERR_END_OF_FILE;
    goto error;
  }
  if ((retval = read_next(buf, *result))) {
    mysql_free_result(*result);
    results.pop_back();
    *result = nullptr;
    return retval;
  }
  return 0;

error:
  my_error(retval, MYF(0), error_buffer);
  return retval;
}

/*
  This method is used exlusevely by filesort() to check if we
  can create sorting buffers of necessary size.
  If the handler returns more records that it declares
  here server can just crash on filesort().
  We cannot guarantee that's not going to happen with
  the FEDERATED engine, as we have records==0 always if the
  client is a VIEW, and for the table the number of
  records can inpredictably change during execution.
  So we return maximum possible value here.
*/

ha_rows ha_federated::estimate_rows_upper_bound() { return HA_POS_ERROR; }

/* Initialized at each key walk (called multiple times unlike rnd_init()) */

int ha_federated::index_init(uint keynr, bool) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("table: '%s'  key: %u", table->s->table_name.str, keynr));
  active_index = keynr;
  return 0;
}

/*
  Read first range
*/

int ha_federated::read_range_first(const key_range *start_key,
                                   const key_range *end_key, bool eq_range_arg,
                                   bool) {
  char sql_query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  int retval;
  String sql_query(sql_query_buffer, sizeof(sql_query_buffer), &my_charset_bin);
  DBUG_TRACE;

  assert(!(start_key == nullptr && end_key == nullptr));

  sql_query.length(0);
  sql_query.append(share->select_query);
  create_where_from_key(&sql_query, &table->key_info[active_index], start_key,
                        end_key, false, eq_range_arg);
  if (real_query(sql_query.ptr(), sql_query.length())) {
    retval = ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  sql_query.length(0);

  if (!(stored_result = store_result(mysql))) {
    retval = HA_ERR_END_OF_FILE;
    goto error;
  }

  retval = read_next(table->record[0], stored_result);
  return retval;

error:
  return retval;
}

int ha_federated::read_range_next() {
  int retval;
  DBUG_TRACE;
  retval = rnd_next_int(table->record[0]);
  return retval;
}

/* Used to read forward through the index.  */
int ha_federated::index_next(uchar *buf) {
  int retval;
  DBUG_TRACE;
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  retval = read_next(buf, stored_result);
  return retval;
}

/*
  rnd_init() is called when the system wants the storage engine to do a table
  scan.

  This is the method that gets data for the SELECT calls.

  See the federated in the introduction at the top of this file to see when
  rnd_init() is called.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federated::rnd_init(bool scan) {
  DBUG_TRACE;
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
    unnecessarily call "select * from table" on the foreign table, then call
    mysql_store_result, which would wipe out the correct previous result set
    from the previous call of index_read_idx's that had the result set
    containing the correct record, hence update the wrong row!

  */

  if (scan) {
    if (real_query(share->select_query, strlen(share->select_query)) ||
        !(stored_result = store_result(mysql)))
      return stash_remote_error();
  }
  return 0;
}

int ha_federated::rnd_end() {
  DBUG_TRACE;
  return index_end();
}

int ha_federated::index_end(void) {
  DBUG_TRACE;
  free_result();
  active_index = MAX_KEY;
  m_blob_root.Clear();
  return 0;
}

/*
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federated::rnd_next(uchar *buf) {
  int rc;
  DBUG_TRACE;
  rc = rnd_next_int(buf);
  return rc;
}

int ha_federated::rnd_next_int(uchar *buf) {
  DBUG_TRACE;

  if (stored_result == nullptr) {
    /*
      Return value of rnd_init is not always checked (see records.cc),
      so we can get here _even_ if there is _no_ pre-fetched result-set!
      TODO: fix it. We can delete this in 5.1 when rnd_init() is checked.
    */
    return 1;
  }
  return read_next(buf, stored_result);
}

/*
  ha_federated::read_next

  reads from a result set and converts to mysql internal
  format

  SYNOPSIS
    ha_federated::read_next()
      buf       byte pointer to record
      result    mysql result set

    DESCRIPTION
     This method is a wrapper method that reads one record from a result
     set and converts it to the internal table format

    RETURN VALUE
      1    error
      0    no error
*/

int ha_federated::read_next(uchar *buf, MYSQL_RES *result) {
  int retval;
  MYSQL_ROW row;
  DBUG_TRACE;

  /* Save current data cursor position. */
  current_position = result->data_cursor;

  /* Fetch a row, insert it back in a row format. */
  if (!(row = mysql_fetch_row(result))) return HA_ERR_END_OF_FILE;

  retval = convert_row_to_internal_format(buf, row, result);

  return retval;
}

/**
  @brief      Store a reference to current row.

  @details    During a query execution we may have different result sets (RS),
              e.g. for different ranges. All the RS's used are stored in
              memory and placed in @c results dynamic array. At the end of
              execution all stored RS's are freed at once in the
              @c ha_federated::reset().
              So, in case of federated, a reference to current row is a
              stored result address and current data cursor position.
              As we keep all RS in memory during a query execution,
              we can get any record using the reference any time until
              @c ha_federated::reset() is called.
              TODO: we don't have to store all RS's rows but only those
              we call @c ha_federated::position() for, so we can free memory
              where we store other rows in the @c ha_federated::index_end().

  @param[in]  record  record data (unused)
*/

void ha_federated::position(const uchar *record [[maybe_unused]]) {
  DBUG_TRACE;

  assert(stored_result);

  position_called = true;
  /* Store result set address. */
  memcpy(ref, &stored_result, sizeof(MYSQL_RES *));
  /* Store data cursor position. */
  memcpy(ref + sizeof(MYSQL_RES *), &current_position,
         sizeof(MYSQL_ROW_OFFSET));
}

/*
  This is like rnd_next, but you are given a position to use to determine the
  row. The position will be of the type that you stored in ref.

  This method is required for an ORDER BY

  Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
*/

int ha_federated::rnd_pos(uchar *buf, uchar *pos) {
  MYSQL_RES *result;
  int ret_val;
  DBUG_TRACE;

  ha_statistic_increment(&System_status_var::ha_read_rnd_count);

  /* Get stored result set. */
  memcpy(&result, pos, sizeof(MYSQL_RES *));
  assert(result);
  /* Set data cursor position. */
  memcpy(&result->data_cursor, pos + sizeof(MYSQL_RES *),
         sizeof(MYSQL_ROW_OFFSET));
  /* Read a row. */
  ret_val = read_next(buf, result);
  return ret_val;
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

int ha_federated::info(uint flag) {
  char status_buf[FEDERATED_QUERY_BUFFER_SIZE];
  int error;
  uint error_code;
  MYSQL_RES *result = nullptr;
  MYSQL_ROW row;
  String status_query_string(status_buf, sizeof(status_buf), &my_charset_bin);
  DBUG_TRACE;

  error_code = ER_QUERY_ON_FOREIGN_DATA_SOURCE;
  /* we want not to show table status if not needed to do so */
  if (flag & (HA_STATUS_VARIABLE | HA_STATUS_CONST)) {
    status_query_string.length(0);
    status_query_string.append(STRING_WITH_LEN("SHOW TABLE STATUS LIKE "));
    append_ident(&status_query_string, share->table_name,
                 share->table_name_length, value_quote_char);

    if (real_query(status_query_string.ptr(), status_query_string.length()))
      goto error;

    status_query_string.length(0);

    result = mysql_store_result(mysql);

    /*
      We're going to use fields num. 4, 12 and 13 of the resultset,
      so make sure we have these fields.
    */
    if (!result || (mysql_num_fields(result) < 14)) goto error;

    if (!mysql_num_rows(result)) goto error;

    if (!(row = mysql_fetch_row(result))) goto error;

    /*
      deleted is set in ha_federated::info
    */
    /*
      need to figure out what this means as far as federated is concerned,
      since we don't have a "file"

      data_file_length = ?
      index_file_length = ?
      delete_length = ?
    */
    if (row[4] != nullptr)
      stats.records = (ha_rows)my_strtoll10(row[4], nullptr, &error);
    if (row[5] != nullptr)
      stats.mean_rec_length = (ulong)my_strtoll10(row[5], nullptr, &error);

    stats.data_file_length = stats.records * stats.mean_rec_length;

    if (row[12] != nullptr)
      stats.update_time = (ulong)my_strtoll10(row[12], nullptr, &error);
    if (row[13] != nullptr)
      stats.check_time = (ulong)my_strtoll10(row[13], nullptr, &error);

    /*
      size of IO operations (This is based on a good guess, no high science
      involved)
    */
    if (flag & HA_STATUS_CONST) stats.block_size = 4096;
  }

  if ((flag & HA_STATUS_AUTO) && mysql)
    stats.auto_increment_value = mysql->insert_id;

  mysql_free_result(result);

  return 0;

error:
  mysql_free_result(result);
  if (mysql) {
    my_printf_error(error_code, ": %d : %s", MYF(0), mysql_errno(mysql),
                    mysql_error(mysql));
  } else if (remote_error_number != -1 /* error already reported */) {
    error_code = remote_error_number;
    my_error(error_code, MYF(0));
  }
  return error_code;
}

/**
  @brief Handles extra signals from MySQL server

  @param[in] operation  Hint for storage engine

  @return Operation Status
  @retval 0     OK
 */
int ha_federated::extra(ha_extra_function operation) {
  DBUG_TRACE;
  switch (operation) {
    case HA_EXTRA_IGNORE_DUP_KEY:
      ignore_duplicates = true;
      break;
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
      insert_dup_update = false;
      ignore_duplicates = false;
      break;
    case HA_EXTRA_WRITE_CAN_REPLACE:
      replace_duplicates = true;
      break;
    case HA_EXTRA_WRITE_CANNOT_REPLACE:
      /*
        We use this flag to ensure that we do not create an "INSERT IGNORE"
        statement when inserting new rows into the remote table.
      */
      replace_duplicates = false;
      break;
    case HA_EXTRA_INSERT_WITH_UPDATE:
      insert_dup_update = true;
      break;
    default:
      /* do nothing */
      DBUG_PRINT("info", ("unhandled operation: %d", (uint)operation));
  }
  return 0;
}

/**
  @brief Reset state of file to after 'open'.

  @details This function is called after every statement for all tables
    used by that statement.

  @return Operation status
    @retval     0       OK
*/

int ha_federated::reset(void) {
  insert_dup_update = false;
  ignore_duplicates = false;
  replace_duplicates = false;

  /* Free stored result sets. */
  for (MYSQL_RES **result = results.begin(); result != results.end();
       ++result) {
    mysql_free_result(*result);
  }
  results.clear();

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
  Called from sql_union.cc by st_query_block_query_expression::exec().
*/

int ha_federated::delete_all_rows() {
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_TRACE;

  query.length(0);

  query.set_charset(system_charset_info);
  if (ignore_duplicates)
    query.append(STRING_WITH_LEN("DELETE IGNORE FROM "));
  else
    query.append(STRING_WITH_LEN("DELETE FROM "));
  append_ident(&query, share->table_name, share->table_name_length,
               ident_quote_char);

  if (real_query(query.ptr(), query.length())) {
    return stash_remote_error();
  }
  stats.deleted += stats.records;
  stats.records = 0;
  return 0;
}

/*
  Used to manually truncate the table.
*/

int ha_federated::truncate(dd::Table *) {
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_TRACE;

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("TRUNCATE TABLE "));
  append_ident(&query, share->table_name, share->table_name_length,
               ident_quote_char);

  /*
    TRUNCATE won't return anything in mysql_affected_rows
  */
  if (real_query(query.ptr(), query.length())) {
    return stash_remote_error();
  }
  stats.deleted += stats.records;
  stats.records = 0;
  return 0;
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

  Berkeley DB for federated  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
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

THR_LOCK_DATA **ha_federated::store_lock(THD *thd, THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type) {
  DBUG_TRACE;
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
        !thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type = TL_READ;

    lock.type = lock_type;
  }

  *to++ = &lock;

  return to;
}

/*
  create() does nothing, since we have no local setup of our own.
  FUTURE: We should potentially connect to the foreign database and
*/

int ha_federated::create(const char *, TABLE *table_arg, HA_CREATE_INFO *,
                         dd::Table *) {
  int retval;
  THD *thd = current_thd;
  FEDERATED_SHARE tmp_share;  // Only a temporary share, to test the url
  DBUG_TRACE;

  retval = parse_url(thd->mem_root, &tmp_share, table_arg, 1);

  return retval;
}

int ha_federated::real_connect() {
  char buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String sql_query(buffer, sizeof(buffer), &my_charset_bin);
  DBUG_TRACE;

  /*
    Bug#25679
    Ensure that we do not hold the LOCK_open mutex while attempting
    to establish Federated connection to guard against a trivial
    Denial of Service scenerio.
  */
  mysql_mutex_assert_not_owner(&LOCK_open);

  assert(mysql == nullptr);

  if (!(mysql = mysql_init(nullptr))) {
    remote_error_number = HA_ERR_OUT_OF_MEM;
    return -1;
  }

  /*
    BUG# 17044 Federated Storage Engine is not UTF8 clean
    Add set names to whatever charset the table is at open
    of table
  */
  /* this sets the csname like 'set names utf8' */
  mysql_options(mysql, MYSQL_SET_CHARSET_NAME,
                this->table->s->table_charset->csname);
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", "mysqld");
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "_client_role",
                 "federated_storage");
  sql_query.length(0);

  if (!mysql_real_connect(mysql, share->hostname, share->username,
                          share->password, share->database, share->port,
                          share->socket, 0)) {
    stash_remote_error();
    mysql_close(mysql);
    mysql = nullptr;
    my_error(ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), remote_error_buf);
    remote_error_number = -1;
    return -1;
  }

  /*
    We have established a connection, lets try a simple dummy query just
    to check that the table and expected columns are present.
  */
  sql_query.append(share->select_query);
  sql_query.append(STRING_WITH_LEN(" WHERE 1=0"));
  if (mysql_real_query(mysql, sql_query.ptr(),
                       static_cast<ulong>(sql_query.length()))) {
    sql_query.length(0);
    sql_query.append("error: ");
    qs_append(mysql_errno(mysql), &sql_query);
    sql_query.append("  '");
    sql_query.append(mysql_error(mysql));
    sql_query.append("'");
    mysql_close(mysql);
    mysql = nullptr;
    my_error(ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST, MYF(0), sql_query.ptr());
    remote_error_number = -1;
    return -1;
  }

  /* Just throw away the result, no rows anyways but need to keep in sync */
  mysql_free_result(mysql_store_result(mysql));

  return 0;
}

int ha_federated::real_query(const char *query, size_t length) {
  int rc = 0;
  DBUG_TRACE;
  if (!mysql && (rc = real_connect())) goto end;

  if (!query || !length) goto end;

  rc = mysql_real_query(mysql, query, static_cast<ulong>(length));

  // Simulate as errors happened within the previous query
  DBUG_EXECUTE_IF("bug33500956_simulate_out_of_order",
                  DBUG_SET("-d,bug33500956_simulate_out_of_order");
                  current_thd->get_stmt_da()->set_error_status(
                      current_thd, ER_NET_PACKETS_OUT_OF_ORDER);
                  mysql->net.last_errno = CR_SERVER_LOST; rc = 1;);
  DBUG_EXECUTE_IF("bug33500956_simulate_read_error",
                  DBUG_SET("-d,bug33500956_simulate_read_error");
                  current_thd->get_stmt_da()->set_error_status(
                      current_thd, ER_NET_READ_ERROR);
                  mysql->net.last_errno = CR_SERVER_LOST; rc = 1;);
  DBUG_EXECUTE_IF("bug33500956_simulate_read_interrupted",
                  DBUG_SET("-d,bug33500956_simulate_read_interrupted");
                  current_thd->get_stmt_da()->set_error_status(
                      current_thd, ER_NET_READ_INTERRUPTED);
                  mysql->net.last_errno = CR_SERVER_LOST; rc = 1;);
  DBUG_EXECUTE_IF("bug33500956_simulate_write_error",
                  DBUG_SET("-d,bug33500956_simulate_write_error");
                  current_thd->get_stmt_da()->set_error_status(
                      current_thd, ER_NET_ERROR_ON_WRITE);
                  mysql->net.last_errno = CR_SERVER_LOST; rc = 1;);
  DBUG_EXECUTE_IF("bug33500956_simulate_write_interrupted",
                  DBUG_SET("-d,bug33500956_simulate_write_interrupted");
                  current_thd->get_stmt_da()->set_error_status(
                      current_thd, ER_NET_WRITE_INTERRUPTED);
                  mysql->net.last_errno = CR_SERVER_LOST; rc = 1;);

  // reconnect and retry on timeout
  // (this fix will be obsoleted by WL#15232)
  if (rc) {
    Diagnostics_area *da = current_thd->get_stmt_da();
    if (da->is_set()) {
      const uint err = da->mysql_errno();
      if ((err == ER_NET_PACKETS_OUT_OF_ORDER || err == ER_NET_ERROR_ON_WRITE ||
           err == ER_NET_WRITE_INTERRUPTED || err == ER_NET_READ_ERROR ||
           err == ER_NET_READ_INTERRUPTED) &&
          mysql->net.last_errno == CR_SERVER_LOST) {
        mysql_free_result(mysql_store_result(mysql));
        da->reset_condition_info(current_thd);
        da->reset_diagnostics_area();
        mysql_reconnect(mysql);
        rc = mysql_real_query(mysql, query, static_cast<ulong>(length));
      }
    }
  }
end:
  return rc;
}

int ha_federated::stash_remote_error() {
  DBUG_TRACE;
  if (!mysql) return remote_error_number;
  remote_error_number = mysql_errno(mysql);
  strmake(remote_error_buf, mysql_error(mysql), sizeof(remote_error_buf) - 1);
  if (remote_error_number == ER_DUP_ENTRY || remote_error_number == ER_DUP_KEY)
    return HA_ERR_FOUND_DUPP_KEY;
  if (remote_error_number == ER_NO_REFERENCED_ROW ||
      remote_error_number == ER_NO_REFERENCED_ROW_2)
    return HA_ERR_NO_REFERENCED_ROW;
  return HA_FEDERATED_ERROR_WITH_REMOTE_SYSTEM;
}

bool ha_federated::get_error_message(int error, String *buf) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));
  if (error == HA_FEDERATED_ERROR_WITH_REMOTE_SYSTEM) {
    buf->append(STRING_WITH_LEN("Error on remote system: "));
    qs_append(remote_error_number, buf);
    buf->append(STRING_WITH_LEN(": "));
    buf->append(remote_error_buf);

    remote_error_number = 0;
    remote_error_buf[0] = '\0';
  }
  DBUG_PRINT("exit", ("message: %s", buf->ptr()));
  return false;
}

/**
  @brief      Store a result set.

  @details    Call @c mysql_store_result() to save a result set then
              append it to the stored results array.

  @param[in]  mysql_arg  MySLQ connection structure.

  @return     Stored result set (MYSQL_RES object).
*/

MYSQL_RES *ha_federated::store_result(MYSQL *mysql_arg) {
  MYSQL_RES *result = mysql_store_result(mysql_arg);
  DBUG_TRACE;
  if (result) {
    results.push_back(result);
  }
  position_called = false;
  return result;
}

void ha_federated::free_result() {
  DBUG_TRACE;
  if (stored_result && !position_called) {
    mysql_free_result(stored_result);
    stored_result = nullptr;
    if (!results.empty()) results.pop_back();
  }
}

int ha_federated::external_lock(THD *, int) {
  int error = 0;
  DBUG_TRACE;

  /*
    Support for transactions disabled until WL#2952 fixes it.
  */
  return error;
}

static int federated_commit(handlerton *hton, THD *thd, bool all) {
  int return_val = 0;
  ha_federated *trx = (ha_federated *)thd_get_ha_data(thd, hton);
  DBUG_TRACE;

  if (all) {
    int error = 0;
    ha_federated *ptr, *old = nullptr;
    for (ptr = trx; ptr; old = ptr, ptr = ptr->trx_next) {
      if (old) old->trx_next = nullptr;
      error = ptr->connection_commit();
      if (error && !return_val) return_val = error;
    }
    thd_set_ha_data(thd, hton, nullptr);
  }

  DBUG_PRINT("info", ("error val: %d", return_val));
  return return_val;
}

static int federated_rollback(handlerton *hton, THD *thd, bool all) {
  int return_val = 0;
  ha_federated *trx = (ha_federated *)thd_get_ha_data(thd, hton);
  DBUG_TRACE;

  if (all) {
    int error = 0;
    ha_federated *ptr, *old = nullptr;
    for (ptr = trx; ptr; old = ptr, ptr = ptr->trx_next) {
      if (old) old->trx_next = nullptr;
      error = ptr->connection_rollback();
      if (error && !return_val) return_val = error;
    }
    thd_set_ha_data(thd, hton, nullptr);
  }

  DBUG_PRINT("info", ("error val: %d", return_val));
  return return_val;
}

int ha_federated::connection_commit() {
  DBUG_TRACE;
  return execute_simple_query("COMMIT", 6);
}

int ha_federated::connection_rollback() {
  DBUG_TRACE;
  return execute_simple_query("ROLLBACK", 8);
}

int ha_federated::connection_autocommit(bool state) {
  const char *text;
  DBUG_TRACE;
  text = (state == true) ? "SET AUTOCOMMIT=1" : "SET AUTOCOMMIT=0";
  return execute_simple_query(text, 16);
}

int ha_federated::execute_simple_query(const char *query, int len) {
  DBUG_TRACE;

  if (mysql_real_query(mysql, query, (ulong)len)) {
    return stash_remote_error();
  }
  return 0;
}

int ha_federated::rnd_pos_by_record(uchar *record [[maybe_unused]]) {
  int error;
  assert(table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION);

  error = ha_rnd_init(false);
  if (error != 0) return error;

  if (stored_result) {
    position(record);
    error = ha_rnd_pos(record, ref);
  }

  ha_rnd_end();
  return error;
}

struct st_mysql_storage_engine federated_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(federated){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &federated_storage_engine,
    "FEDERATED",
    PLUGIN_AUTHOR_ORACLE,
    "Federated MySQL storage engine",
    PLUGIN_LICENSE_GPL,
    federated_db_init, /* Plugin Init */
    nullptr,           /* Plugin check uninstall */
    federated_done,    /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr,                /* status variables                */
    nullptr,                /* system variables                */
    nullptr,                /* config options                  */
    PLUGIN_OPT_DEFAULT_OFF, /* flags            */
} mysql_declare_plugin_end;
