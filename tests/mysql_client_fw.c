/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <errmsg.h>
#include <my_compare.h>
#include <my_getopt.h>
#include <m_string.h>
#include <mysqld_error.h>
#include <sql_common.h>
#include <mysql/client_plugin.h>

/*
  If non_blocking_api_enabled is true, we will re-define all the blocking
  API functions as wrappers that call the corresponding non-blocking API
  and use poll()/select() to wait for them to complete. This way we can get
  a good coverage testing of the non-blocking API as well.
*/
#include <my_context.h>
static my_bool non_blocking_api_enabled= 0;
#if !defined(EMBEDDED_LIBRARY) && !defined(MY_CONTEXT_DISABLE)
#define WRAP_NONBLOCK_ENABLED non_blocking_api_enabled
#include "nonblock-wrappers.h"
#endif

#define VER "2.1"
#define MAX_TEST_QUERY_LENGTH 300 /* MAX QUERY BUFFER LENGTH */
#define MAX_KEY MAX_INDEXES
#define MAX_SERVER_ARGS 64

/* set default options */
static int   opt_testcase __attribute__((unused)) = 0;
static char *opt_db= 0;
static char *opt_user= 0;
static char *opt_password= 0;
static char *opt_host= 0;
static char *opt_unix_socket= 0;
#ifdef HAVE_SMEM
static char *shared_memory_base_name= 0;
#endif
static unsigned int  opt_port;
static my_bool tty_password= 0, opt_silent= 0;

static MYSQL *mysql= 0;
static char current_db[]= "client_test_db";
static unsigned int test_count= 0;
static unsigned int opt_count= 0;
static unsigned int iter_count= 0;
static my_bool have_innodb= FALSE;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static unsigned int opt_drop_db= 1;

static const char *opt_basedir= "./";
static const char *opt_vardir= "mysql-test/var";

static longlong opt_getopt_ll_test= 0;

static int embedded_server_arg_count= 0;
static char *embedded_server_args[MAX_SERVER_ARGS];

static const char *embedded_server_groups[]= {
  "server",
  "embedded",
  "mysql_client_test_SERVER",
  NullS
};

static time_t start_time, end_time;
static double total_time;

const char *default_dbug_option= "d:t:o,/tmp/mysql_client_test.trace";

struct my_tests_st
{
  const char *name;
  void       (*function)();
};

#define myheader(str)							\
DBUG_PRINT("test", ("name: %s", str));					\
 if (opt_silent < 2)							\
 {									\
   fprintf(stdout, "\n\n#####################################\n");	\
   fprintf(stdout, "%u of (%u/%u): %s", test_count++, iter_count,	\
   opt_count, str);							\
   fprintf(stdout, "  \n#####################################\n");	\
 }

#define myheader_r(str)							\
DBUG_PRINT("test", ("name: %s", str));					\
 if (!opt_silent)							\
 {									\
   fprintf(stdout, "\n\n#####################################\n");	\
   fprintf(stdout, "%s", str);						\
   fprintf(stdout, "  \n#####################################\n");	\
 }

static void print_error(const char *msg);
static void print_st_error(MYSQL_STMT *stmt, const char *msg);
static void client_disconnect(MYSQL* mysql);


/*
  Abort unless given experssion is non-zero.

  SYNOPSIS
    DIE_UNLESS(expr)

  DESCRIPTION
    We can't use any kind of system assert as we need to
    preserve tested invariants in release builds as well.
*/

#define DIE_UNLESS(expr) \
        ((void) ((expr) ? 0 : (die(__FILE__, __LINE__, #expr), 0)))
#define DIE_IF(expr) \
        ((void) ((expr) ? (die(__FILE__, __LINE__, #expr), 0) : 0))
#define DIE(expr) \
        die(__FILE__, __LINE__, #expr)

static void die(const char *file, int line, const char *expr)
{
  fflush(stdout);
  fprintf(stderr, "%s:%d: check failed: '%s'\n", file, line, expr);
  fprintf(stderr, "MySQL error %d: %s\n", mysql_errno(0), mysql_error(0));
  fflush(stderr);
  exit(1);
}


#define myerror(msg) print_error(msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myquery(RES)				\
{						\
 int r= (RES);					\
 if (r)						\
 myerror(NULL);					\
 DIE_UNLESS(r == 0);				\
}

#define myquery_r(r)				\
{						\
 if (r)						\
 myerror(NULL);					\
 DIE_UNLESS(r != 0);				\
}

#define check_execute(stmt, r)			\
{						\
 if (r)						\
 mysterror(stmt, NULL);				\
 DIE_UNLESS(r == 0);				\
}

#define check_execute_r(stmt, r)		\
{						\
 if (r)						\
 mysterror(stmt, NULL);				\
 DIE_UNLESS(r != 0);				\
}

#define check_stmt(stmt)			\
{						\
 if ( stmt == 0)				\
 myerror(NULL);					\
 DIE_UNLESS(stmt != 0);				\
}

#define check_stmt_r(stmt)			\
{						\
 if (stmt == 0)					\
 myerror(NULL);					\
 DIE_UNLESS(stmt == 0);				\
}

#define mytest(x) if (!(x)) {myerror(NULL);DIE_UNLESS(FALSE);}
#define mytest_r(x) if ((x)) {myerror(NULL);DIE_UNLESS(FALSE);}


/* A workaround for Sun Forte 5.6 on Solaris x86 */

static int cmp_double(double *a, double *b)
{
  return *a == *b;
}


/* Print the error message */

static void print_error(const char *msg)
{
  if (!opt_silent)
  {
    if (mysql && mysql_errno(mysql))
    {
      if (mysql->server_version)
        fprintf(stdout, "\n [MySQL-%s]", mysql->server_version);
      else
        fprintf(stdout, "\n [MySQL]");
      fprintf(stdout, "[%d] %s\n", mysql_errno(mysql), mysql_error(mysql));
    }
    else if (msg)
      fprintf(stderr, " [MySQL] %s\n", msg);
  }
}


static void print_st_error(MYSQL_STMT *stmt, const char *msg)
{
  if (!opt_silent)
  {
    if (stmt && mysql_stmt_errno(stmt))
    {
      if (stmt->mysql && stmt->mysql->server_version)
        fprintf(stdout, "\n [MySQL-%s]", stmt->mysql->server_version);
      else
        fprintf(stdout, "\n [MySQL]");

      fprintf(stdout, "[%d] %s\n", mysql_stmt_errno(stmt),
              mysql_stmt_error(stmt));
    }
    else if (msg)
      fprintf(stderr, " [MySQL] %s\n", msg);
  }
}

/*
  Enhanced version of mysql_client_init(), which may also set shared memory 
  base on Windows.
*/
static MYSQL *mysql_client_init(MYSQL* con)
{
  MYSQL* res = mysql_init(con);
#ifdef HAVE_SMEM
  if (res && shared_memory_base_name)
    mysql_options(res, MYSQL_SHARED_MEMORY_BASE_NAME, shared_memory_base_name);
#endif
  if (res && non_blocking_api_enabled)
    mysql_options(res, MYSQL_OPT_NONBLOCK, 0);
  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(res, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(res, MYSQL_DEFAULT_AUTH, opt_default_auth);
  return res;
}

/*
  Disable direct calls of mysql_init, as it disregards  shared memory base.
*/
#define mysql_init(A) Please use mysql_client_init instead of mysql_init


/* Check if the connection has InnoDB tables */

static my_bool check_have_innodb(MYSQL *conn)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  int rc;
  my_bool result;

  rc= mysql_query(conn, "show variables like 'have_innodb'");
  myquery(rc);
  res= mysql_use_result(conn);
  DIE_UNLESS(res);

  row= mysql_fetch_row(res);
  DIE_UNLESS(row);

  result= strcmp(row[1], "YES") == 0;
  mysql_free_result(res);
  return result;
}


/*
  This is to be what mysql_query() is for mysql_real_query(), for
  mysql_simple_prepare(): a variant without the 'length' parameter.
*/

static MYSQL_STMT *STDCALL
mysql_simple_prepare(MYSQL *mysql_arg, const char *query)
{
  MYSQL_STMT *stmt= mysql_stmt_init(mysql_arg);
  if (stmt && mysql_stmt_prepare(stmt, query, (uint) strlen(query)))
  {
    mysql_stmt_close(stmt);
    return 0;
  }
  return stmt;
}


/**
   Connect to the server with options given by arguments to this application,
   stored in global variables opt_host, opt_user, opt_password, opt_db, 
   opt_port and opt_unix_socket.

   @param flag[in]           client_flag passed on to mysql_real_connect
   @param protocol[in]       MYSQL_PROTOCOL_* to use for this connection
   @param auto_reconnect[in] set to 1 for auto reconnect
   
   @return pointer to initialized and connected MYSQL object
*/
static MYSQL* client_connect(ulong flag, uint protocol, my_bool auto_reconnect)
{
  MYSQL* mysql;
  int  rc;
  static char query[MAX_TEST_QUERY_LENGTH];
  myheader_r("client_connect");

  if (!opt_silent)
    fprintf(stdout, "\n Establishing a connection to '%s' ...",
            opt_host ? opt_host : "");

  if (!(mysql= mysql_client_init(NULL)))
  {
    opt_silent= 0;
    myerror("mysql_client_init() failed");
    exit(1);
  }
  /* enable local infile, in non-binary builds often disabled by default */
  mysql_options(mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(mysql, MYSQL_OPT_PROTOCOL, &protocol);
  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);

  if (!(mysql_real_connect(mysql, opt_host, opt_user,
                           opt_password, opt_db ? opt_db:"test", opt_port,
                           opt_unix_socket, flag)))
  {
    opt_silent= 0;
    myerror("connection failed");
    mysql_close(mysql);
    fprintf(stdout, "\n Check the connection options using --help or -?\n");
    exit(1);
  }
  mysql->reconnect= auto_reconnect;

  if (!opt_silent)
    fprintf(stdout, "OK");

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, TRUE);

  if (!opt_silent)
  {
    fprintf(stdout, "\nConnected to MySQL server version: %s (%lu)\n",
            mysql_get_server_info(mysql),
            (ulong) mysql_get_server_version(mysql));
    fprintf(stdout, "\n Creating a test database '%s' ...", current_db);
  }
  strxmov(query, "CREATE DATABASE IF NOT EXISTS ", current_db, NullS);

  rc= mysql_query(mysql, query);
  myquery(rc);

  strxmov(query, "USE ", current_db, NullS);
  rc= mysql_query(mysql, query);
  myquery(rc);
  have_innodb= check_have_innodb(mysql);

  if (!opt_silent)
    fprintf(stdout, "OK\n");

  return mysql;
}


/* Close the connection */

static void client_disconnect(MYSQL* mysql)
{
 static char query[MAX_TEST_QUERY_LENGTH];

 myheader_r("client_disconnect");

 if (mysql)
 {
   if (opt_drop_db)
   {
     if (!opt_silent)
     fprintf(stdout, "\n dropping the test database '%s' ...", current_db);
     strxmov(query, "DROP DATABASE IF EXISTS ", current_db, NullS);

     mysql_query(mysql, query);
     if (!opt_silent)
     fprintf(stdout, "OK");
   }

   if (!opt_silent)
   fprintf(stdout, "\n closing the connection ...");
   mysql_close(mysql);
   if (!opt_silent)
   fprintf(stdout, "OK\n");
 }
}


/* Print dashes */

static void my_print_dashes(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i, j;

  mysql_field_seek(result, 0);
  fputc('\t', stdout);
  fputc('+', stdout);

  for(i= 0; i< mysql_num_fields(result); i++)
  {
    field= mysql_fetch_field(result);
    for(j= 0; j < field->max_length+2; j++)
      fputc('-', stdout);
    fputc('+', stdout);
  }
  fputc('\n', stdout);
}


/* Print resultset metadata information */

static void my_print_result_metadata(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i, j;
  unsigned int field_count;

  mysql_field_seek(result, 0);
  if (!opt_silent)
  {
    fputc('\n', stdout);
    fputc('\n', stdout);
  }

  field_count= mysql_num_fields(result);
  for(i= 0; i< field_count; i++)
  {
    field= mysql_fetch_field(result);
    j= strlen(field->name);
    if (j < field->max_length)
      j= field->max_length;
    if (j < 4 && !IS_NOT_NULL(field->flags))
      j= 4;
    field->max_length= j;
  }
  if (!opt_silent)
  {
    my_print_dashes(result);
    fputc('\t', stdout);
    fputc('|', stdout);
  }

  mysql_field_seek(result, 0);
  for(i= 0; i< field_count; i++)
  {
    field= mysql_fetch_field(result);
    if (!opt_silent)
      fprintf(stdout, " %-*s |", (int) field->max_length, field->name);
  }
  if (!opt_silent)
  {
    fputc('\n', stdout);
    my_print_dashes(result);
  }
}


/* Process the result set */

static int my_process_result_set(MYSQL_RES *result)
{
  MYSQL_ROW    row;
  MYSQL_FIELD  *field;
  unsigned int i;
  unsigned int row_count= 0;

  if (!result)
    return 0;

  my_print_result_metadata(result);

  while ((row= mysql_fetch_row(result)) != NULL)
  {
    mysql_field_seek(result, 0);
    if (!opt_silent)
    {
      fputc('\t', stdout);
      fputc('|', stdout);
    }

    for(i= 0; i< mysql_num_fields(result); i++)
    {
      field= mysql_fetch_field(result);
      if (!opt_silent)
      {
        if (row[i] == NULL)
          fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
        else if (IS_NUM(field->type))
          fprintf(stdout, " %*s |", (int) field->max_length, row[i]);
        else
          fprintf(stdout, " %-*s |", (int) field->max_length, row[i]);
      }
    }
    if (!opt_silent)
    {
      fputc('\t', stdout);
      fputc('\n', stdout);
    }
    row_count++;
  }
  if (!opt_silent)
  {
    if (row_count)
      my_print_dashes(result);

    if (mysql_errno(mysql) != 0)
      fprintf(stderr, "\n\tmysql_fetch_row() failed\n");
    else
      fprintf(stdout, "\n\t%d %s returned\n", row_count,
              row_count == 1 ? "row" : "rows");
  }
  return row_count;
}


static int my_process_result(MYSQL *mysql_arg)
{
  MYSQL_RES *result;
  int       row_count;

  if (!(result= mysql_store_result(mysql_arg)))
    return 0;

  row_count= my_process_result_set(result);

  mysql_free_result(result);
  return row_count;
}


/* Process the statement result set */

#define MAX_RES_FIELDS 50
#define MAX_FIELD_DATA_SIZE 255

static int my_process_stmt_result(MYSQL_STMT *stmt)
{
  int         field_count;
  int         row_count= 0;
  MYSQL_BIND  buffer[MAX_RES_FIELDS];
  MYSQL_FIELD *field;
  MYSQL_RES   *result;
  char        data[MAX_RES_FIELDS][MAX_FIELD_DATA_SIZE];
  ulong       length[MAX_RES_FIELDS];
  my_bool     is_null[MAX_RES_FIELDS];
  int         rc, i;

  if (!(result= mysql_stmt_result_metadata(stmt))) /* No meta info */
  {
    while (!mysql_stmt_fetch(stmt))
      row_count++;
    return row_count;
  }

  field_count= min(mysql_num_fields(result), MAX_RES_FIELDS);

  bzero((char*) buffer, sizeof(buffer));
  bzero((char*) length, sizeof(length));
  bzero((char*) is_null, sizeof(is_null));

  for(i= 0; i < field_count; i++)
  {
    buffer[i].buffer_type= MYSQL_TYPE_STRING;
    buffer[i].buffer_length= MAX_FIELD_DATA_SIZE;
    buffer[i].length= &length[i];
    buffer[i].buffer= (void *) data[i];
    buffer[i].is_null= &is_null[i];
  }

  rc= mysql_stmt_bind_result(stmt, buffer);
  check_execute(stmt, rc);

  rc= 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*)&rc);
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);
  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  while ((rc= mysql_stmt_fetch(stmt)) == 0)
  {
    if (!opt_silent)
    {
      fputc('\t', stdout);
      fputc('|', stdout);
    }
    mysql_field_seek(result, 0);
    for (i= 0; i < field_count; i++)
    {
      field= mysql_fetch_field(result);
      if (!opt_silent)
      {
        if (is_null[i])
          fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
        else if (length[i] == 0)
        {
          data[i][0]= '\0';  /* unmodified buffer */
          fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
        }
        else if (IS_NUM(field->type))
          fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
        else
          fprintf(stdout, " %-*s |", (int) field->max_length, data[i]);
      }
    }
    if (!opt_silent)
    {
      fputc('\t', stdout);
      fputc('\n', stdout);
    }
    row_count++;
  }
  DIE_UNLESS(rc == MYSQL_NO_DATA);
  if (!opt_silent)
  {
    if (row_count)
      my_print_dashes(result);
    fprintf(stdout, "\n\t%d %s returned\n", row_count,
            row_count == 1 ? "row" : "rows");
  }
  mysql_free_result(result);
  return row_count;
}


/* Prepare statement, execute, and process result set for given query */

int my_stmt_result(const char *buff)
{
  MYSQL_STMT *stmt;
  int        row_count;
  int        rc;

  if (!opt_silent)
    fprintf(stdout, "\n\n %s", buff);
  stmt= mysql_simple_prepare(mysql, buff);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  row_count= my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);

  return row_count;
}

/* Print the total number of warnings and the warnings themselves.  */

void my_process_warnings(MYSQL *conn, unsigned expected_warning_count)
{
  MYSQL_RES *result;
  int rc;

  if (!opt_silent)
    fprintf(stdout, "\n total warnings: %u (expected: %u)\n",
            mysql_warning_count(conn), expected_warning_count);

  DIE_UNLESS(mysql_warning_count(mysql) == expected_warning_count);

  rc= mysql_query(conn, "SHOW WARNINGS");
  DIE_UNLESS(rc == 0);

  result= mysql_store_result(conn);
  mytest(result);

  rc= my_process_result_set(result);
  mysql_free_result(result);
}


/* Utility function to verify a particular column data */

static void verify_col_data(const char *table, const char *col,
                            const char *exp_data)
{
  static char query[MAX_TEST_QUERY_LENGTH];
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc, field= 1;

  if (table && col)
  {
    strxmov(query, "SELECT ", col, " FROM ", table, " LIMIT 1", NullS);
    if (!opt_silent)
      fprintf(stdout, "\n %s", query);
    rc= mysql_query(mysql, query);
    myquery(rc);

    field= 0;
  }

  result= mysql_use_result(mysql);
  mytest(result);

  if (!(row= mysql_fetch_row(result)) || !row[field])
  {
    fprintf(stdout, "\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  if (strcmp(row[field], exp_data))
  {
    fprintf(stdout, "\n obtained: `%s` (expected: `%s`)",
            row[field], exp_data);
    DIE_UNLESS(FALSE);
  }
  mysql_free_result(result);
}


/* Utility function to verify the field members */

#define verify_prepare_field(result,no,name,org_name,type,table,\
                             org_table,db,length,def) \
          do_verify_prepare_field((result),(no),(name),(org_name),(type), \
                                  (table),(org_table),(db),(length),(def), \
                                  __FILE__, __LINE__)

static void do_verify_prepare_field(MYSQL_RES *result,
                                   unsigned int no, const char *name,
                                   const char *org_name,
                                   enum enum_field_types type,
                                   const char *table,
                                   const char *org_table, const char *db,
                                   unsigned long length, const char *def,
                                   const char *file, int line)
{
  MYSQL_FIELD *field;
  CHARSET_INFO *cs;
  ulonglong expected_field_length;

  if (!(field= mysql_fetch_field_direct(result, no)))
  {
    fprintf(stdout, "\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  cs= get_charset(field->charsetnr, 0);
  DIE_UNLESS(cs);
  if ((expected_field_length= length * cs->mbmaxlen) > UINT_MAX32)
    expected_field_length= UINT_MAX32;
  if (!opt_silent)
  {
    fprintf(stdout, "\n field[%d]:", no);
    fprintf(stdout, "\n    name     :`%s`\t(expected: `%s`)", field->name, name);
    fprintf(stdout, "\n    org_name :`%s`\t(expected: `%s`)",
            field->org_name, org_name);
    fprintf(stdout, "\n    type     :`%d`\t(expected: `%d`)", field->type, type);
    if (table)
      fprintf(stdout, "\n    table    :`%s`\t(expected: `%s`)",
              field->table, table);
    if (org_table)	      
      fprintf(stdout, "\n    org_table:`%s`\t(expected: `%s`)",
              field->org_table, org_table);
    fprintf(stdout, "\n    database :`%s`\t(expected: `%s`)", field->db, db);
    fprintf(stdout, "\n    length   :`%lu`\t(expected: `%llu`)",
            field->length, expected_field_length);
    fprintf(stdout, "\n    maxlength:`%ld`", field->max_length);
    fprintf(stdout, "\n    charsetnr:`%d`", field->charsetnr);
    fprintf(stdout, "\n    default  :`%s`\t(expected: `%s`)",
            field->def ? field->def : "(null)", def ? def: "(null)");
    fprintf(stdout, "\n");
  }
  DIE_UNLESS(strcmp(field->name, name) == 0);
  DIE_UNLESS(strcmp(field->org_name, org_name) == 0);
  /*
    XXX: silent column specification change works based on number of
    bytes a column occupies. So CHAR -> VARCHAR upgrade is possible even
    for CHAR(2) column if its character set is multibyte.
    VARCHAR -> CHAR downgrade won't work for VARCHAR(3) as one would
    expect.
  */
  if (cs->mbmaxlen == 1)
  {
    if (field->type != type)
    {
      fprintf(stderr,
              "Expected field type: %d,  got type: %d in file %s, line %d\n",
              (int) type, (int) field->type, file, line);
      DIE_UNLESS(field->type == type);
    }
  }
  if (table)
    DIE_UNLESS(strcmp(field->table, table) == 0);
  if (org_table)
    DIE_UNLESS(strcmp(field->org_table, org_table) == 0);
  DIE_UNLESS(strcmp(field->db, db) == 0);
  /*
    Character set should be taken into account for multibyte encodings, such
    as utf8. Field length is calculated as number of characters * maximum
    number of bytes a character can occupy.
  */
  if (length && (field->length != expected_field_length))
  {
    fflush(stdout);
    fprintf(stderr, "Expected field length: %llu,  got length: %lu\n",
            expected_field_length, field->length);
    fflush(stderr);
    DIE_UNLESS(field->length == expected_field_length);
  }
  if (def)
    DIE_UNLESS(strcmp(field->def, def) == 0);
}


/* Utility function to verify the parameter count */

static void verify_param_count(MYSQL_STMT *stmt, long exp_count)
{
  long param_count= mysql_stmt_param_count(stmt);
  if (!opt_silent)
    fprintf(stdout, "\n total parameters in stmt: `%ld` (expected: `%ld`)",
            param_count, exp_count);
  DIE_UNLESS(param_count == exp_count);
}


/* Utility function to verify the total affected rows */

static void verify_st_affected_rows(MYSQL_STMT *stmt, ulonglong exp_count)
{
  ulonglong affected_rows= mysql_stmt_affected_rows(stmt);
  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: `%ld` (expected: `%ld`)",
            (long) affected_rows, (long) exp_count);
  DIE_UNLESS(affected_rows == exp_count);
}


/* Utility function to verify the total affected rows */

static void verify_affected_rows(ulonglong exp_count)
{
  ulonglong affected_rows= mysql_affected_rows(mysql);
  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: `%ld` (expected: `%ld`)",
            (long) affected_rows, (long) exp_count);
  DIE_UNLESS(affected_rows == exp_count);
}


/* Utility function to verify the total fields count */

static void verify_field_count(MYSQL_RES *result, uint exp_count)
{
  uint field_count= mysql_num_fields(result);
  if (!opt_silent)
    fprintf(stdout, "\n total fields in the result set: `%d` (expected: `%d`)",
            field_count, exp_count);
  DIE_UNLESS(field_count == exp_count);
}


/* Utility function to execute a query using prepare-execute */

#ifndef EMBEDDED_LIBRARY
static void execute_prepare_query(const char *query, ulonglong exp_count)
{
  MYSQL_STMT *stmt;
  ulonglong  affected_rows;
  int        rc;

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  myquery(rc);

  affected_rows= mysql_stmt_affected_rows(stmt);
  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: `%ld` (expected: `%ld`)",
            (long) affected_rows, (long) exp_count);

  DIE_UNLESS(affected_rows == exp_count);
  mysql_stmt_close(stmt);
}
#endif

/*
Accepts arbitrary number of queries and runs them against the database.
Used to fill tables for each test.
*/

void fill_tables(const char **query_list, unsigned query_count)
{
 int rc;
 const char **query;
 DBUG_ENTER("fill_tables");
 for (query= query_list; query < query_list + query_count;
      ++query)
 {
   rc= mysql_query(mysql, *query);
   myquery(rc);
 }
 DBUG_VOID_RETURN;
}

/*
All state of fetch from one statement: statement handle, out buffers,
fetch position.
See fetch_n for for the only use case.
*/

enum { MAX_COLUMN_LENGTH= 255 };

typedef struct st_stmt_fetch
{
const char *query;
unsigned stmt_no;
MYSQL_STMT *handle;
my_bool is_open;
MYSQL_BIND *bind_array;
char **out_data;
unsigned long *out_data_length;
unsigned column_count;
unsigned row_count;
} Stmt_fetch;


/*
Create statement handle, prepare it with statement, execute and allocate
fetch buffers.
*/

void stmt_fetch_init(Stmt_fetch *fetch, unsigned stmt_no_arg,
const char *query_arg)
{
 unsigned long type= CURSOR_TYPE_READ_ONLY;
 int rc;
 unsigned i;
 MYSQL_RES *metadata;
 DBUG_ENTER("stmt_fetch_init");

 /* Save query and statement number for error messages */
 fetch->stmt_no= stmt_no_arg;
 fetch->query= query_arg;

 fetch->handle= mysql_stmt_init(mysql);

 rc= mysql_stmt_prepare(fetch->handle, fetch->query, strlen(fetch->query));
 check_execute(fetch->handle, rc);

 /*
 The attribute is sent to server on execute and asks to open read-only
 for result set
 */
 mysql_stmt_attr_set(fetch->handle, STMT_ATTR_CURSOR_TYPE,
 (const void*) &type);

 rc= mysql_stmt_execute(fetch->handle);
 check_execute(fetch->handle, rc);

 /* Find out total number of columns in result set */
 metadata= mysql_stmt_result_metadata(fetch->handle);
 fetch->column_count= mysql_num_fields(metadata);
 mysql_free_result(metadata);

 /*
 Now allocate bind handles and buffers for output data:
 calloc memory to reduce number of MYSQL_BIND members we need to
 set up.
 */

 fetch->bind_array= (MYSQL_BIND *) calloc(1, sizeof(MYSQL_BIND) *
 fetch->column_count);
 fetch->out_data= (char**) calloc(1, sizeof(char*) * fetch->column_count);
 fetch->out_data_length= (ulong*) calloc(1, sizeof(ulong) *
 fetch->column_count);
 for (i= 0; i < fetch->column_count; ++i)
 {
   fetch->out_data[i]= (char*) calloc(1, MAX_COLUMN_LENGTH);
   fetch->bind_array[i].buffer_type= MYSQL_TYPE_STRING;
   fetch->bind_array[i].buffer= fetch->out_data[i];
   fetch->bind_array[i].buffer_length= MAX_COLUMN_LENGTH;
   fetch->bind_array[i].length= fetch->out_data_length + i;
 }

 mysql_stmt_bind_result(fetch->handle, fetch->bind_array);

 fetch->row_count= 0;
 fetch->is_open= TRUE;

 /* Ready for reading rows */
 DBUG_VOID_RETURN;
}


/* Fetch and print one row from cursor */

int stmt_fetch_fetch_row(Stmt_fetch *fetch)
{
 int rc;
 unsigned i;
 DBUG_ENTER("stmt_fetch_fetch_row");

 if ((rc= mysql_stmt_fetch(fetch->handle)) == 0)
 {
   ++fetch->row_count;
   if (!opt_silent)
   printf("Stmt %d fetched row %d:\n", fetch->stmt_no, fetch->row_count);
   for (i= 0; i < fetch->column_count; ++i)
   {
     fetch->out_data[i][fetch->out_data_length[i]]= '\0';
     if (!opt_silent)
     printf("column %d: %s\n", i+1, fetch->out_data[i]);
   }
 }
 else
 fetch->is_open= FALSE;
 DBUG_RETURN(rc);
}


void stmt_fetch_close(Stmt_fetch *fetch)
{
 unsigned i;
 DBUG_ENTER("stmt_fetch_close");

 for (i= 0; i < fetch->column_count; ++i)
 free(fetch->out_data[i]);
 free(fetch->out_data);
 free(fetch->out_data_length);
 free(fetch->bind_array);
 mysql_stmt_close(fetch->handle);
 DBUG_VOID_RETURN;
}

/*
For given array of queries, open query_count cursors and fetch
from them in simultaneous manner.
In case there was an error in one of the cursors, continue
reading from the rest.
*/

enum fetch_type { USE_ROW_BY_ROW_FETCH= 0, USE_STORE_RESULT= 1 };

my_bool fetch_n(const char **query_list, unsigned query_count,
enum fetch_type fetch_type)
{
 unsigned open_statements= query_count;
 int rc, error_count= 0;
 Stmt_fetch *fetch_array= (Stmt_fetch*) calloc(1, sizeof(Stmt_fetch) *
 query_count);
 Stmt_fetch *fetch;
 DBUG_ENTER("fetch_n");

 for (fetch= fetch_array; fetch < fetch_array + query_count; ++fetch)
 {
   /* Init will exit(1) in case of error */
   stmt_fetch_init(fetch, fetch - fetch_array,
   query_list[fetch - fetch_array]);
 }

 if (fetch_type == USE_STORE_RESULT)
 {
   for (fetch= fetch_array; fetch < fetch_array + query_count; ++fetch)
   {
     rc= mysql_stmt_store_result(fetch->handle);
     check_execute(fetch->handle, rc);
   }
 }

 while (open_statements)
 {
   for (fetch= fetch_array; fetch < fetch_array + query_count; ++fetch)
   {
     if (fetch->is_open && (rc= stmt_fetch_fetch_row(fetch)))
     {
       open_statements--;
       /*
       We try to fetch from the rest of the statements in case of
       error
       */
       if (rc != MYSQL_NO_DATA)
       {
	 fprintf(stderr,
	 "Got error reading rows from statement %d,\n"
	 "query is: %s,\n"
	 "error message: %s", (int) (fetch - fetch_array),
	 fetch->query,
	 mysql_stmt_error(fetch->handle));
	 error_count++;
       }
     }
   }
 }
 if (error_count)
 fprintf(stderr, "Fetch FAILED");
 else
 {
   unsigned total_row_count= 0;
   for (fetch= fetch_array; fetch < fetch_array + query_count; ++fetch)
   total_row_count+= fetch->row_count;
   if (!opt_silent)
   printf("Success, total rows fetched: %d\n", total_row_count);
 }
 for (fetch= fetch_array; fetch < fetch_array + query_count; ++fetch)
 stmt_fetch_close(fetch);
 free(fetch_array);
 DBUG_RETURN(error_count != 0);
}

/* Separate thread query to test some cases */

static my_bool thread_query(const char *query)
{
 MYSQL *l_mysql;
 my_bool error;

 error= 0;
 if (!opt_silent)
 fprintf(stdout, "\n in thread_query(%s)", query);
 if (!(l_mysql= mysql_client_init(NULL)))
 {
   myerror("mysql_client_init() failed");
   return 1;
 }
 if (!(mysql_real_connect(l_mysql, opt_host, opt_user,
 opt_password, current_db, opt_port,
 opt_unix_socket, 0)))
 {
   myerror("connection failed");
   error= 1;
   goto end;
 }
 l_mysql->reconnect= 1;
 if (mysql_query(l_mysql, query))
 {
   fprintf(stderr, "Query failed (%s)\n", mysql_error(l_mysql));
   error= 1;
   goto end;
 }
 mysql_commit(l_mysql);
 end:
 mysql_close(l_mysql);
 return error;
}


/*
  Read and parse arguments and MySQL options from my.cnf
*/

static const char *client_test_load_default_groups[]=
{ "client", "client-server", "client-mariadb", 0 };
static char **defaults_argv;

static struct my_option client_test_long_options[] =
{
  {"basedir", 'b', "Basedir for tests.", &opt_basedir,
   &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 't', "Number of times test to be executed", &opt_count,
   &opt_count, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", &opt_db, &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"do-not-drop-database", 'd', "Do not drop database while disconnecting",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", &opt_host, &opt_host,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_port, &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", 'A', "Send embedded server this as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-tests", 'T', "Show all tests' names", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0,
   0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", 'm', "Base name of shared memory.", 
  &shared_memory_base_name, (uchar**)&shared_memory_base_name, 0, 
  GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "Socket file to use for connection",
   &opt_unix_socket, &opt_unix_socket, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"testcase", 'c',
   "May disable some code when runs as mysql-test-run testcase.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user", &opt_user,
   &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"vardir", 'v', "Data dir for tests.", &opt_vardir,
   &opt_vardir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"non-blocking-api", 'n',
   "Use the non-blocking client API for communication.",
   &non_blocking_api_enabled, &non_blocking_api_enabled, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"getopt-ll-test", 'g', "Option for testing bug in getopt library",
   &opt_getopt_ll_test, &opt_getopt_ll_test, 0,
   GET_LL, REQUIRED_ARG, 0, 0, LONGLONG_MAX, 0, 0, 0},
  {"plugin_dir", 0, "Directory for client-side plugins.",
   &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", 0, "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(void)
{
  /* show the usage string when the user asks for this */
  putc('\n', stdout);
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",
	 my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  puts("By Monty, Venu, Kent and others\n");
  printf("\
Copyright (C) 2002-2004 MySQL AB\n\
This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n");
  printf("Usage: %s [OPTIONS] [TESTNAME1 TESTNAME2...]\n", my_progname);
  my_print_help(client_test_long_options);
  print_defaults("my", client_test_load_default_groups);
  my_print_variables(client_test_long_options);
}

static struct my_tests_st *get_my_tests();  /* To be defined in main .c file */

static struct my_tests_st *my_testlist= 0;

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
  case 'c':
    opt_testcase = 1;
    break;
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password);
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';               /* Destroy argument */
      if (*start)
        start[1]=0;
    }
    else
      tty_password= 1;
    break;
  case 's':
    if (argument == disabled_my_option)
      opt_silent= 0;
    else
      opt_silent++;
    break;
  case 'd':
    opt_drop_db= 0;
    break;
  case 'A':
    /*
      When the embedded server is being tested, the test suite needs to be
      able to pass command-line arguments to the embedded server so it can
      locate the language files and data directory. The test suite
      (mysql-test-run) never uses config files, just command-line options.
    */
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count= 1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(argument, MYF(MY_FAE))))
    {
      DIE("Can't use server argument");
    }
    break;
  case 'T':
    {
      struct my_tests_st *fptr;
      
      printf("All possible test names:\n\n");
      for (fptr= my_testlist; fptr->name; fptr++)
	printf("%s\n", fptr->name);
      exit(0);
      break;
    }
  case '?':
  case 'I':                                     /* Info */
    usage();
    exit(0);
    break;
  }
  return 0;
}

static void get_options(int *argc, char ***argv)
{
  int ho_error;

  if ((ho_error= handle_options(argc, argv, client_test_long_options,
                                get_one_option)))
    exit(ho_error);

  if (tty_password)
    opt_password= get_tty_password(NullS);
  return;
}

/*
  Print the test output on successful execution before exiting
*/

static void print_test_output()
{
  if (opt_silent < 3)
  {
    fprintf(stdout, "\n\n");
    fprintf(stdout, "All '%d' tests were successful (in '%d' iterations)",
            test_count-1, opt_count);
    fprintf(stdout, "\n  Total execution time: %g SECS", total_time);
    if (opt_count > 1)
      fprintf(stdout, " (Avg: %g SECS)", total_time/opt_count);

    fprintf(stdout, "\n\n!!! SUCCESS !!!\n");
  }
}

/***************************************************************************
  main routine
***************************************************************************/


int main(int argc, char **argv)
{
  struct my_tests_st *fptr;
  my_testlist= get_my_tests();

  MY_INIT(argv[0]);

  if (load_defaults("my", client_test_load_default_groups, &argc, &argv))
    exit(1);

  defaults_argv= argv;
  get_options(&argc, &argv);

  if (mysql_server_init(embedded_server_arg_count,
                        embedded_server_args,
                        (char**) embedded_server_groups))
    DIE("Can't initialize MySQL server");

  /* connect to server with no flags, default protocol, auto reconnect true */
  mysql= client_connect(0, MYSQL_PROTOCOL_DEFAULT, 1);

  total_time= 0;
  for (iter_count= 1; iter_count <= opt_count; iter_count++)
  {
    /* Start of tests */
    test_count= 1;
    start_time= time((time_t *)0);
    if (!argc)
    {
      for (fptr= my_testlist; fptr->name; fptr++)
        (*fptr->function)();	
    }
    else
    {
      for ( ; *argv ; argv++)
      {
        for (fptr= my_testlist; fptr->name; fptr++)
        {
          if (!strcmp(fptr->name, *argv))
          {
            (*fptr->function)();
            break;
          }
        }
        if (!fptr->name)
        {
          fprintf(stderr, "\n\nGiven test not found: '%s'\n", *argv);
          fprintf(stderr, "See legal test names with %s -T\n\nAborting!\n",
                  my_progname);
          client_disconnect(mysql);
          free_defaults(defaults_argv);
          exit(1);
        }
      }
    }

    end_time= time((time_t *)0);
    total_time+= difftime(end_time, start_time);

    /* End of tests */
  }

  client_disconnect(mysql);    /* disconnect from server */

  free_defaults(defaults_argv);
  print_test_output();

  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count]);

  mysql_server_end();

  my_end(0);

  exit(0);
}
